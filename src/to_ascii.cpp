#include "ada/idna/to_ascii.h"

#include <algorithm>
#include <cstdint>
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
// regardless of the outcome of Unicode ToASCII. This is a deliberate
// web-compatibility carve-out: a label may decode from its ACE ("xn--") form
// yet still fail IDNA validity criteria (ContextJ/Bidi rules, code points with
// "mapped" status, empty labels, ...) and must nevertheless be accepted as-is.
//
// See https://url.spec.whatwg.org/#concept-domain-to-ascii
static std::string from_ascii_to_ascii(std::string_view ut8_string) {
  std::string out(ut8_string);
  ascii_map(out.data(), out.size());
  return out;
}

[[nodiscard]] bool to_ascii(std::string_view ut8_string, std::string& out) {
  out.clear();
  if (ut8_string.size() > max_domain_input_bytes) {
    return false;
  }
  if (is_ascii(ut8_string)) {
    out = from_ascii_to_ascii(ut8_string);
    return true;
  }

#ifdef ADA_USE_SIMDUTF
  size_t utf32_length =
      simdutf::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  if (utf32_length == 0 && !ut8_string.empty()) {
    return false;
  }
  std::u32string utf32(utf32_length, '\0');
  size_t actual_utf32_length = simdutf::convert_utf8_to_utf32(
      ut8_string.data(), ut8_string.size(), utf32.data());
#else
  size_t utf32_length =
      ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  if (utf32_length == 0 && !ut8_string.empty()) {
    return false;
  }
  std::u32string utf32(utf32_length, '\0');
  size_t actual_utf32_length = ada::idna::utf8_to_utf32(
      ut8_string.data(), ut8_string.size(), utf32.data());
#endif
  // Require a complete conversion: reject invalid UTF-8 (actual < expected)
  // and the empty conversion of non-empty input.
  if (actual_utf32_length == 0 || actual_utf32_length != utf32_length) {
    return false;
  }
  utf32.resize(actual_utf32_length);

  std::u32string mapped;
  std::u32string scratch;
  std::u32string post_map;
  if (!ada::idna::map(utf32, mapped)) {
    return false;
  }
  // ASCII is already NFC; skip the (expensive) normalize pass when mapping
  // produced only ASCII (common: e.g. U+00DF LATIN SMALL LETTER SHARP S ->
  // "ss").
  if (!is_ascii(mapped)) {
    if (!normalize(mapped)) {
      return false;
    }
  }
  out.reserve(ut8_string.size());
  size_t label_start = 0;

  while (label_start != mapped.size()) {
    size_t loc_dot = mapped.find('.', label_start);
    bool is_last_label = (loc_dot == std::string_view::npos);
    size_t label_size =
        is_last_label ? mapped.size() - label_start : loc_dot - label_start;
    size_t label_size_with_dot = is_last_label ? label_size : label_size + 1;
    std::u32string_view label_view(mapped.data() + label_start, label_size);
    label_start += label_size_with_dot;
    if (label_size == 0) {
      // empty label? Nothing to do.
    } else if (label_view.starts_with(U"xn--")) {
      for (char32_t c : label_view) {
        if (c >= 0x80) {
          out.clear();
          return false;
        }
      }
      size_t label_out_start = out.size();
      out.resize(label_out_start + label_size);
      char* dest = out.data() + label_out_start;
      for (char32_t c : label_view) {
        *dest++ = static_cast<char>(c);
      }
      std::string_view puny_segment_ascii(out.data() + label_out_start + 4,
                                          label_size - 4);
      scratch.clear();
      bool is_ok = ada::idna::punycode_to_utf32(puny_segment_ascii, scratch);
      if (!is_ok) {
        out.clear();
        return false;
      }
      if (is_ascii(scratch)) {
        out.clear();
        return false;
      }
      if (!ada::idna::map(scratch, post_map)) {
        out.clear();
        return false;
      }
      if (scratch != post_map) {
        out.clear();
        return false;
      }
      if (!is_ascii(post_map)) {
        if (!normalize(post_map) || post_map != scratch) {
          out.clear();
          return false;
        }
      }
      if (post_map.empty()) {
        out.clear();
        return false;
      }
      if (!is_label_valid(post_map)) {
        out.clear();
        return false;
      }
    } else {
      if (is_ascii(label_view)) {
        size_t old_size = out.size();
        out.resize(old_size + label_size);
        char* dest = out.data() + old_size;
        for (char32_t c : label_view) {
          *dest++ = static_cast<char>(c);
        }
      } else {
        if (!is_label_valid(label_view)) {
          out.clear();
          return false;
        }
        out.append("xn--");
        bool is_ok = ada::idna::utf32_to_punycode(label_view, out);
        if (!is_ok) {
          out.clear();
          return false;
        }
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
