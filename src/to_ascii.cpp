#include "ada/idna/to_ascii.h"

#include <cstdint>

#include "ada/idna/mapping.h"
#include "ada/idna/normalization.h"
#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"
#include "ada/idna/validity.h"
#include "simd.hpp"

#ifdef ADA_USE_SIMDUTF
#include "simdutf.h"
#endif

namespace ada::idna {

bool is_ascii(std::u32string_view view) noexcept {
  return simd::is_ascii_32(view.data(), view.size());
}

bool is_ascii(std::string_view view) noexcept {
  return simd::is_ascii_8(view.data(), view.size());
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

bool contains_forbidden_domain_code_point(std::string_view view) {
  const auto* p = reinterpret_cast<const uint8_t*>(view.data());
  const size_t n = view.size();
  size_t i = 0;
  uint8_t bits = 0;
  for (; i + 4 <= n; i += 4) {
    bits =
        static_cast<uint8_t>(bits | is_forbidden_domain_code_point_table[p[i]] |
                             is_forbidden_domain_code_point_table[p[i + 1]] |
                             is_forbidden_domain_code_point_table[p[i + 2]] |
                             is_forbidden_domain_code_point_table[p[i + 3]]);
  }
  for (; i < n; ++i) {
    bits =
        static_cast<uint8_t>(bits | is_forbidden_domain_code_point_table[p[i]]);
  }
  return bits != 0;
}

// Append ASCII code units from a UTF-32 label (all values < 0x80).
static void append_ascii_label(std::string& out, std::u32string_view label) {
  const size_t old = out.size();
  out.resize(old + label.size());
  char* dest = out.data() + old;
  const char32_t* src = label.data();
  for (size_t i = 0; i < label.size(); ++i) {
    dest[i] = static_cast<char>(src[i]);
  }
}

// True if label begins with "xn--" (already lowercased by mapping).
static bool is_ace_prefix(std::u32string_view label) noexcept {
  return label.size() >= 4 && label[0] == U'x' && label[1] == U'n' &&
         label[2] == U'-' && label[3] == U'-';
}

[[nodiscard]] bool to_ascii(std::string_view ut8_string, std::string& out) {
  if (ut8_string.size() > max_domain_input_bytes) {
    out.clear();
    return false;
  }
  // WHATWG beStrict=false: ASCII input is just copied and lowercased.
  // One pass: lowercase in place and test the high bit (no extra is_ascii
  // scan). See https://url.spec.whatwg.org/#concept-domain-to-ascii
  out.assign(ut8_string);
  if (simd::ascii_lowercase_is_ascii(out.data(), out.size())) {
    return true;
  }
  out.clear();

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
      if (!is_ascii(label_view)) {
        out.clear();
        return false;
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
