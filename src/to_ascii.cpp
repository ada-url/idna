#include "ada/idna/to_ascii.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <vector>

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
  // The ASCII path is a single same-size copy, so it needs no length bound.
  // Checking first would reject the (longer) ASCII output of a non-ASCII input
  // that was accepted, breaking to_ascii(to_ascii(x)) == to_ascii(x).
  if (is_ascii(ut8_string)) {
    from_ascii_to_ascii(ut8_string, out);
    return true;
  }
  if (ut8_string.size() > max_domain_input_bytes) {
    return false;
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

  // Validation is deferred until every label is known: the Bidi rule applies
  // to all labels once any label of the domain is RTL (RFC 5893 Section 2).
  // Only decoded ACE labels need storage; others are re-read from `mapped`.
  std::vector<std::u32string> decoded_labels;

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
      if (post_map.empty()) {
        out.clear();
        return false;
      }
      decoded_labels.push_back(post_map);
    } else if (is_ascii(label_view)) {
      append_ascii_label(out, label_view);
    } else {
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

  // Validity criteria (UTS #46 Section 4.1). CheckBidi is decided for the whole
  // domain: with one RTL label anywhere, every label must satisfy it. Walk the
  // labels of `mapped` again (ACE labels in decoded form); `fn` returns false
  // to stop.
  const auto for_each_label = [&](auto&& fn) -> bool {
    size_t ace_index = 0;
    const char32_t* q = mapped.data();
    for (;;) {
      const char32_t* label_begin = q;
      while (q < end && *q != U'.') {
        ++q;
      }
      std::u32string_view v(label_begin, static_cast<size_t>(q - label_begin));
      if (!v.empty() && is_ace_prefix(v)) {
        v = decoded_labels[ace_index++];
      }
      if (!fn(v)) {
        return false;
      }
      if (q == end) {
        return true;
      }
      ++q;  // skip dot
    }
  };
  bool bidi_domain = false;
  for_each_label([&](std::u32string_view v) {
    if (!is_ascii(v) && has_rtl_characters(v)) {
      bidi_domain = true;
      return false;
    }
    return true;
  });
  const bool labels_valid = for_each_label([&](std::u32string_view v) {
    if (v.empty() || (!bidi_domain && is_ascii(v))) {
      return true;
    }
    return is_label_valid(v, bidi_domain);
  });
  if (!labels_valid) {
    out.clear();
    return false;
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
