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

// We return "" on error.
static std::string from_ascii_to_ascii(std::string_view ut8_string) {
  static const std::string error = "";
  // copy and map
  // we could be more efficient by avoiding the copy when unnecessary.
  std::string mapped_string = std::string(ut8_string);
  ascii_map(mapped_string.data(), mapped_string.size());
  std::string out;
  size_t label_start = 0;

  while (label_start != mapped_string.size()) {
    size_t loc_dot = mapped_string.find('.', label_start);
    bool is_last_label = (loc_dot == std::string_view::npos);
    size_t label_size = is_last_label ? mapped_string.size() - label_start
                                      : loc_dot - label_start;
    size_t label_size_with_dot = is_last_label ? label_size : label_size + 1;
    std::string_view label_view(mapped_string.data() + label_start, label_size);
    label_start += label_size_with_dot;
    if (label_size == 0) {
      // empty label? Nothing to do.
    } else if (label_view.starts_with("xn--")) {
      // The xn-- part is the expensive game.
      out.append(label_view);
      std::string_view puny_segment_ascii(
          out.data() + out.size() - label_view.size() + 4,
          label_view.size() - 4);
      std::u32string tmp_buffer;
      bool is_ok = ada::idna::punycode_to_utf32(puny_segment_ascii, tmp_buffer);
      if (!is_ok) {
        return error;
      }
      // If the input is just ASCII, it should not have been encoded
      // as punycode.
      // https://github.com/whatwg/url/issues/760
      if (is_ascii(tmp_buffer)) {
        return error;
      }
      std::u32string post_map = ada::idna::map(tmp_buffer);
      if (tmp_buffer != post_map) {
        return error;
      }
      std::u32string pre_normal = post_map;
      normalize(post_map);
      if (post_map != pre_normal) {
        return error;
      }
      if (post_map.empty()) {
        return error;
      }
      if (!is_label_valid(post_map)) {
        return error;
      }
    } else {
      out.append(label_view);
    }
    if (!is_last_label) {
      out.push_back('.');
    }
  }
  return out;
}

// We return "" on error.
std::string to_ascii(std::string_view ut8_string) {
  if (is_ascii(ut8_string)) {
    return from_ascii_to_ascii(ut8_string);
  }
  static const std::string error = "";
  // We convert to UTF-32

#ifdef ADA_USE_SIMDUTF
  size_t utf32_length =
      simdutf::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  std::u32string utf32(utf32_length, '\0');
  size_t actual_utf32_length = simdutf::convert_utf8_to_utf32(
      ut8_string.data(), ut8_string.size(), utf32.data());
#else
  size_t utf32_length =
      ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  std::u32string utf32(utf32_length, '\0');
  size_t actual_utf32_length = ada::idna::utf8_to_utf32(
      ut8_string.data(), ut8_string.size(), utf32.data());
#endif
  if (actual_utf32_length == 0) {
    return error;
  }
  // mapping
  utf32 = ada::idna::map(utf32);
  normalize(utf32);
  std::string out;
  size_t label_start = 0;

  while (label_start != utf32.size()) {
    size_t loc_dot = utf32.find('.', label_start);
    bool is_last_label = (loc_dot == std::string_view::npos);
    size_t label_size =
        is_last_label ? utf32.size() - label_start : loc_dot - label_start;
    size_t label_size_with_dot = is_last_label ? label_size : label_size + 1;
    std::u32string_view label_view(utf32.data() + label_start, label_size);
    label_start += label_size_with_dot;
    if (label_size == 0) {
      // empty label? Nothing to do.
    } else if (label_view.starts_with(U"xn--")) {
      // we do not need to check, e.g., Xn-- because mapping goes to lower case
      for (char32_t c : label_view) {
        if (c >= 0x80) {
          return error;
        }
        out += (unsigned char)(c);
      }
      std::string_view puny_segment_ascii(
          out.data() + out.size() - label_view.size() + 4,
          label_view.size() - 4);
      std::u32string tmp_buffer;
      bool is_ok = ada::idna::punycode_to_utf32(puny_segment_ascii, tmp_buffer);
      if (!is_ok) {
        return error;
      }
      // If the input is just ASCII, it should not have been encoded
      // as punycode.
      // https://github.com/whatwg/url/issues/760
      if (is_ascii(tmp_buffer)) {
        return error;
      }
      std::u32string post_map = ada::idna::map(tmp_buffer);
      if (tmp_buffer != post_map) {
        return error;
      }
      std::u32string pre_normal = post_map;
      normalize(post_map);
      if (post_map != pre_normal) {
        return error;
      }
      if (post_map.empty()) {
        return error;
      }
      if (!is_label_valid(post_map)) {
        return error;
      }
    } else {
      // The fast path here is an ascii label.
      if (is_ascii(label_view)) {
        // no validation needed.
        for (char32_t c : label_view) {
          out += (unsigned char)(c);
        }
      } else {
        // slow path.
        // first check validity.
        if (!is_label_valid(label_view)) {
          return error;
        }
        // It is valid! So now we must encode it as punycode...
        out.append("xn--");
        bool is_ok = ada::idna::utf32_to_punycode(label_view, out);
        if (!is_ok) {
          return error;
        }
      }
    }
    if (!is_last_label) {
      out.push_back('.');
    }
  }
  return out;
}
}  // namespace ada::idna
