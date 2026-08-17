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
  // One pass: SIMD rejects C0 / space / non-ASCII (signed byte < 0x21), then
  // an unrolled table covers the remaining forbidden ASCII punctuation.
  const char* data = view.data();
  const size_t n = view.size();
  size_t i = 0;
  uint8_t bits = 0;
#if defined(ADA_IDNA_SSE2)
  const __m128i lim = _mm_set1_epi8(0x21);
  for (; i + 16 <= n; i += 16) {
    const __m128i word =
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
    if (_mm_movemask_epi8(_mm_cmplt_epi8(word, lim)) != 0) {
      return true;
    }
    bits = static_cast<uint8_t>(
        bits | is_forbidden_domain_code_point_table[uint8_t(data[i])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 1])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 2])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 3])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 4])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 5])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 6])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 7])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 8])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 9])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 10])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 11])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 12])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 13])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 14])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 15])]);
  }
#elif defined(ADA_IDNA_NEON)
  for (; i + 16 <= n; i += 16) {
    const uint8x16_t word =
        vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
    if (vmaxvq_u8(vcltq_u8(word, vdupq_n_u8(0x21))) != 0) {
      return true;
    }
    bits = static_cast<uint8_t>(
        bits | is_forbidden_domain_code_point_table[uint8_t(data[i])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 1])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 2])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 3])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 4])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 5])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 6])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 7])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 8])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 9])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 10])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 11])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 12])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 13])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 14])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 15])]);
  }
#endif
  for (; i + 8 <= n; i += 8) {
    bits = static_cast<uint8_t>(
        bits | is_forbidden_domain_code_point_table[uint8_t(data[i])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 1])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 2])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 3])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 4])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 5])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 6])] |
        is_forbidden_domain_code_point_table[uint8_t(data[i + 7])]);
  }
  for (; i < n; ++i) {
    bits = static_cast<uint8_t>(
        bits | is_forbidden_domain_code_point_table[uint8_t(data[i])]);
  }
  return bits != 0;
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
  const uint32_t* src = reinterpret_cast<const uint32_t*>(label.data());
  size_t i = 0;
  for (; i + 4 <= label.size(); i += 4) {
    simd::pack4_ascii_utf32(src + i, dest + i);
  }
  for (; i < label.size(); ++i) {
    dest[i] = static_cast<char>(src[i]);
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
    const size_t remaining = static_cast<size_t>(end - p);
    p += simd::find_char32(p, remaining, U'.');
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
