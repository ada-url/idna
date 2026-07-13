#include "ada/idna/to_ascii.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ranges>

#include "ada/idna/mapping.h"
#include "ada/idna/normalization.h"
#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"
#include "ada/idna/validity.h"

#ifdef ADA_USE_SIMDUTF
#include "simdutf.h"
#endif

namespace ada::idna {

bool constexpr is_ascii(std::u32string_view view) {
  for (uint32_t c : view) {
    if (c >= 0x80) {
      return false;
    }
  }
  return true;
}

bool constexpr is_ascii(std::string_view view) {
  for (uint8_t c : view) {
    if (c >= 0x80) {
      return false;
    }
  }
  return true;
}

constexpr static uint8_t is_forbidden_domain_code_point_table[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

static_assert(sizeof(is_forbidden_domain_code_point_table) == 256);

inline bool is_forbidden_domain_code_point(const char c) noexcept {
  return is_forbidden_domain_code_point_table[uint8_t(c)];
}

bool contains_forbidden_domain_code_point(std::string_view view) {
  return std::ranges::any_of(view, is_forbidden_domain_code_point);
}

// Per the WHATWG URL "domain to ASCII" algorithm, when beStrict is false and
// the input domain is an ASCII string, the result is the input lowercased,
// regardless of the outcome of Unicode ToASCII.
//
// See https://url.spec.whatwg.org/#concept-domain-to-ascii
static void from_ascii_to_ascii(std::string_view ut8_string, std::string& out) {
  out.assign(ut8_string);
  ascii_map(out.data(), out.size());
}

// Append ASCII code units from a UTF-32 label (all values < 0x80).
static void append_ascii_label(std::string& out, std::u32string_view label) {
  const size_t old = out.size();
  out.resize(old + label.size());
  char* dest = out.data() + old;
  for (char32_t c : label) {
    *dest++ = static_cast<char>(c);
  }
}

// True if label begins with "xn--" (already lowercased by mapping).
static bool is_ace_prefix(std::u32string_view label) noexcept {
  return label.size() >= 4 && label[0] == U'x' && label[1] == U'n' &&
         label[2] == U'-' && label[3] == U'-';
}

[[nodiscard]] bool to_ascii(std::string_view ut8_string, std::string& out) {
  out.clear();
  if (ut8_string.size() > max_domain_input_bytes) {
    return false;
  }
  if (is_ascii(ut8_string)) {
    from_ascii_to_ascii(ut8_string, out);
    return true;
  }

#ifdef ADA_USE_SIMDUTF
  size_t utf32_length =
      simdutf::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  if (utf32_length == 0 && !ut8_string.empty()) {
    return false;
  }
  std::u32string working(utf32_length, U'\0');
  size_t actual_utf32_length = simdutf::convert_utf8_to_utf32(
      ut8_string.data(), ut8_string.size(), working.data());
#else
  size_t utf32_length =
      ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  if (utf32_length == 0 && !ut8_string.empty()) {
    return false;
  }
  std::u32string working(utf32_length, U'\0');
  size_t actual_utf32_length = ada::idna::utf8_to_utf32(
      ut8_string.data(), ut8_string.size(), working.data());
#endif
  if (actual_utf32_length == 0 || actual_utf32_length != utf32_length) {
    return false;
  }
  working.resize(actual_utf32_length);

  // Map into a second buffer with exact sizing (no growth reallocs).
  std::u32string mapped;
  if (!ada::idna::map(working, mapped)) {
    return false;
  }
  // Drop UTF-32 input; reuse `working` as scratch for ACE validation below.
  working.clear();

  // Skip NFC when already normalized (ASCII is a fast subset of this check).
  if (!is_ascii(mapped) && !is_already_nfc(mapped)) {
    if (!normalize(mapped)) {
      return false;
    }
  }

  // Estimate ASCII output size (punycode may expand non-ASCII labels).
  out.reserve(mapped.size() + 8);

  // Walk labels with a single pointer scan (no repeated string::find).
  const char32_t* p = mapped.data();
  const char32_t* const end = p + mapped.size();
  std::u32string post_map;

  while (p < end) {
    const char32_t* label_begin = p;
    while (p < end && *p != U'.') {
      ++p;
    }
    const size_t label_size = static_cast<size_t>(p - label_begin);
    std::u32string_view label_view(label_begin, label_size);
    const bool is_last_label = (p == end);
    if (p < end) {
      ++p;  // skip dot
    }

    if (label_size == 0) {
      // empty label
    } else if (is_ace_prefix(label_view)) {
      for (char32_t c : label_view) {
        if (c >= 0x80) {
          out.clear();
          return false;
        }
      }
      append_ascii_label(out, label_view);
      std::string_view puny_segment_ascii(
          out.data() + (out.size() - label_size) + 4, label_size - 4);
      working.clear();
      if (!ada::idna::punycode_to_utf32(puny_segment_ascii, working)) {
        out.clear();
        return false;
      }
      if (is_ascii(working)) {
        out.clear();
        return false;
      }
      if (!ada::idna::map(working, post_map) || working != post_map) {
        out.clear();
        return false;
      }
      // Mapping must be stable; NFC must not change the mapped form either.
      if (!is_ascii(post_map) && !is_already_nfc(post_map)) {
        if (!normalize(post_map) || post_map != working) {
          out.clear();
          return false;
        }
      }
      if (post_map.empty() || !is_label_valid(post_map)) {
        out.clear();
        return false;
      }
    } else if (is_ascii(label_view)) {
      append_ascii_label(out, label_view);
    } else {
      if (!is_label_valid(label_view)) {
        out.clear();
        return false;
      }
      out.append("xn--");
      if (!ada::idna::utf32_to_punycode(label_view, out)) {
        out.clear();
        return false;
      }
    }
    if (!is_last_label) {
      out.push_back('.');
    }
  }
  return true;
}

std::string to_ascii(std::string_view ut8_string) {
  std::string out;
  if (!to_ascii(ut8_string, out)) {
    return {};
  }
  return out;
}
}  // namespace ada::idna
