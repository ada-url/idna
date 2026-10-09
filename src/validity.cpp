#include "table_store.hpp"

#include <algorithm>
#include <span>
#include <string_view>

namespace ada::idna {

// Values stored in dir_value; keep in sync with BIDI_CLASSES in
// scripts/unicode_data_table.py.
enum direction : uint8_t {
  NONE,
  BN,
  CS,
  ES,
  ON,
  EN,
  L,
  R,
  NSM,
  AL,
  AN,
  ET,
  WS,
  RLO,
  LRO,
  PDF,
  RLE,
  RLI,
  FSI,
  PDI,
  LRI,
  B,
  S,
  LRE
};

// Values stored in joining_value; keep in sync with JOINING_TYPES in
// scripts/unicode_data_table.py. Code points without a range are U.
enum class joining_type : uint8_t { U, C, D, L, R, T };

// Bidi direction and Joining_Type ranges live in the compressed blob as const
// SoA arrays (dir_* and joining_*). See table_store.hpp layout asserts.
// Regenerate with: python3 scripts/unicode_data_table.py --write

// Defined in normalization.cpp.
uint8_t get_ccc(char32_t c) noexcept;

// CheckJoiners and CheckBidi are true for URL specification.

// Returns the index of the range [start[i], last[i]] that contains code_point,
// or count if there is none. `last` must be sorted.
inline static size_t find_range(const uint32_t* start, const uint32_t* last,
                                size_t count, uint32_t code_point) noexcept {
  size_t lo = 0, hi = count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (last[mid] < code_point) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo < count && code_point >= start[lo] ? lo : count;
}

// Assumes ensure_tables() has already been called by is_label_valid().
inline static direction find_direction(uint32_t code_point) noexcept {
  size_t i = find_range(dir_start, dir_final, dir_table_count, code_point);
  return i == dir_table_count ? direction::NONE
                              : static_cast<direction>(dir_value[i]);
}

// Assumes ensure_tables() has already been called by is_label_valid().
inline static joining_type find_joining_type(uint32_t code_point) noexcept {
  size_t i =
      find_range(joining_start, joining_final, joining_table_count, code_point);
  return i == joining_table_count ? joining_type::U
                                  : static_cast<joining_type>(joining_value[i]);
}

// The ContextJ rules for U+200C and U+200D from RFC 5892 Appendix A.
inline static bool is_contextj_valid(std::u32string_view label) noexcept {
  constexpr uint8_t virama_ccc = 9;
  for (size_t i = 0; i < label.size(); i++) {
    if (label[i] != 0x200C && label[i] != 0x200D) {
      continue;
    }
    // If Canonical_Combining_Class(Before(cp)) .eq. Virama Then True;
    if (i > 0 && get_ccc(label[i - 1]) == virama_ccc) {
      continue;
    }
    if (label[i] == 0x200D) {
      return false;
    }
    // If RegExpMatch((Joining_Type:{L,D})(Joining_Type:T)*\u200C
    //    (Joining_Type:T)*(Joining_Type:{R,D})) Then True;
    joining_type before = joining_type::U;
    for (size_t j = i; j > 0;) {
      before = find_joining_type(label[--j]);
      if (before != joining_type::T) {
        break;
      }
    }
    if (before != joining_type::L && before != joining_type::D) {
      return false;
    }
    joining_type after = joining_type::U;
    for (size_t j = i + 1; j < label.size(); j++) {
      after = find_joining_type(label[j]);
      if (after != joining_type::T) {
        break;
      }
    }
    if (after != joining_type::R && after != joining_type::D) {
      return false;
    }
  }
  return true;
}

inline static size_t find_last_not_of_nsm(
    const std::u32string_view label) noexcept {
  for (int i = static_cast<int>(label.size() - 1); i >= 0; i--)
    if (find_direction(label[i]) != direction::NSM) return i;

  return std::u32string_view::npos;
}

// An RTL label is a label that contains at least one character of type R, AL,
// or AN. https://www.rfc-editor.org/rfc/rfc5893#section-2
inline static bool is_rtl_label(const std::u32string_view label) noexcept {
  const size_t mask =
      (1u << direction::R) | (1u << direction::AL) | (1u << direction::AN);

  size_t directions = 0;
  for (size_t i = 0; i < label.size(); i++) {
    directions |= 1u << find_direction(label[i]);
  }
  return (directions & mask) != 0;
}

bool is_label_valid(const std::u32string_view label, bool bidi_domain) {
  if (label.empty()) {
    return true;
  }
  if (!ensure_tables() || combining_ranges == nullptr || dir_start == nullptr ||
      dir_final == nullptr || dir_value == nullptr ||
      joining_start == nullptr || joining_final == nullptr ||
      joining_value == nullptr) {
    return false;
  }

  ///////////////
  // We have a normalization step which ensures that we are in NFC.
  // If we receive punycode, we normalize and check that the normalized
  // version matches the original.
  // --------------------------------------
  // The label must be in Unicode Normalization Form NFC.

  // Current URL standard indicatest that CheckHyphens is set to false.
  // ---------------------------------------
  // If CheckHyphens, the label must not contain a U+002D HYPHEN-MINUS character
  // in both the third and fourth positions. If CheckHyphens, the label must
  // neither begin nor end with a U+002D HYPHEN-MINUS character.

  // This is not necessary because we segment the
  // labels by '.'.
  // ---------------------------------------
  // The label must not contain a U+002E ( . ) FULL STOP.
  // if (label.find('.') != std::string_view::npos) return false;

  // The label must not begin with a combining mark.
  // Range membership via lower_bound on range end.
  const std::span<const uint32_t[2]> comb_span{combining_ranges,
                                               combining_range_count};
  const auto comb_it = std::ranges::lower_bound(
      comb_span, label.front(), {}, [](const auto& range) { return range[1]; });
  if (comb_it != comb_span.end() && label.front() >= (*comb_it)[0]) {
    return false;
  }
  // We verify this next step as part of the mapping:
  // ---------------------------------------------
  // Each code point in the label must only have certain status values
  // according to Section 5, IDNA Mapping Table:
  // - For Transitional Processing, each value must be valid.
  // - For Nontransitional Processing, each value must be either valid or
  // deviation.

  // If CheckJoiners, the label must satisfy the ContextJ rules from Appendix
  // A, in The Unicode Code Points and Internationalized Domain Names for
  // Applications (IDNA) [IDNA2008].
  if (!is_contextj_valid(label)) {
    return false;
  }

  // If CheckBidi, and if the domain name is a  Bidi domain name, then the label
  // must satisfy all six of the numbered conditions in [IDNA2008] RFC 5893,
  // Section 2.

  // The following rule, consisting of six conditions, applies to labels
  // in Bidi domain names.  The requirements that this rule satisfies are
  // described in Section 3.  All of the conditions must be satisfied for
  // the rule to be satisfied.
  //
  //  1.  The first character must be a character with Bidi property L, R,
  //     or AL.  If it has the R or AL property, it is an RTL label; if it
  //     has the L property, it is an LTR label.
  //
  //  2.  In an RTL label, only characters with the Bidi properties R, AL,
  //      AN, EN, ES, CS, ET, ON, BN, or NSM are allowed.
  //
  //   3.  In an RTL label, the end of the label must be a character with
  //       Bidi property R, AL, EN, or AN, followed by zero or more
  //       characters with Bidi property NSM.
  //
  //   4.  In an RTL label, if an EN is present, no AN may be present, and
  //       vice versa.
  //
  //  5.  In an LTR label, only characters with the Bidi properties L, EN,
  //       ES, CS, ET, ON, BN, or NSM are allowed.
  //
  //   6.  In an LTR label, the end of the label must be a character with
  //       Bidi property L or EN, followed by zero or more characters with
  //       Bidi property NSM.

  size_t last_non_nsm_char = find_last_not_of_nsm(label);
  if (last_non_nsm_char == std::u32string_view::npos) {
    return false;
  }

  // A "Bidi domain name" is a domain name that contains at least one RTL label.
  // RFC 5893 Section 2 then applies all six conditions to *every* label of the
  // name, including labels with no RTL character (e.g. an all-ASCII "1" fails
  // condition 1). The caller decides `bidi_domain` from the whole domain; see
  // to_ascii(). The one-argument overload below keeps the old per-label
  // behaviour for callers without domain context.
  if (bidi_domain) {
    // The first character must be a character with Bidi property L, R,
    // or AL. If it has the R or AL property, it is an RTL label; if it
    // has the L property, it is an LTR label.

    if (find_direction(label[0]) == direction::L) {
      // Eval as LTR

      // In an LTR label, only characters with the Bidi properties L, EN,
      // ES, CS, ET, ON, BN, or NSM are allowed.
      for (size_t i = 0; i <= last_non_nsm_char; i++) {
        const direction d = find_direction(label[i]);
        if (!(d == direction::L || d == direction::EN || d == direction::ES ||
              d == direction::CS || d == direction::ET || d == direction::ON ||
              d == direction::BN || d == direction::NSM)) {
          return false;
        }
      }

      const direction last_dir = find_direction(label[last_non_nsm_char]);
      if (!(last_dir == direction::L || last_dir == direction::EN)) {
        return false;
      }

      return true;

    } else {
      // Eval as RTL

      // The first character must be R or AL; a leading AN (or any other
      // direction) does not start a valid RTL label.
      const direction first_dir = find_direction(label[0]);
      if (first_dir != direction::R && first_dir != direction::AL) {
        return false;
      }

      bool has_an = false;
      bool has_en = false;
      for (size_t i = 0; i <= last_non_nsm_char; i++) {
        const direction d = find_direction(label[i]);

        // In an RTL label, if an EN is present, no AN may be present, and vice
        // versa.
        if ((d == direction::EN && ((has_en = true) && has_an)) ||
            (d == direction::AN && ((has_an = true) && has_en))) {
          return false;
        }

        if (!(d == direction::R || d == direction::AL || d == direction::AN ||
              d == direction::EN || d == direction::ES || d == direction::CS ||
              d == direction::ET || d == direction::ON || d == direction::BN ||
              d == direction::NSM)) {
          return false;
        }

        if (i == last_non_nsm_char &&
            !(d == direction::R || d == direction::AL || d == direction::AN ||
              d == direction::EN)) {
          return false;
        }
      }

      return true;
    }
  }

  return true;
}

bool has_rtl_characters(const std::u32string_view label) {
  if (!ensure_tables() || dir_start == nullptr || dir_final == nullptr ||
      dir_value == nullptr) {
    return false;
  }
  return is_rtl_label(label);
}

bool is_label_valid(const std::u32string_view label) {
  // Without domain context, apply the Bidi rule only when the label itself is
  // an RTL label (its previous behaviour).
  return is_label_valid(label, has_rtl_characters(label));
}

}  // namespace ada::idna
