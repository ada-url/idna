#include "table_store.hpp"

#include <algorithm>
#include <span>
#include <string_view>

namespace ada::idna {

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

// dir_table data lives in the compressed blob (table_store.hpp).

// CheckJoiners and CheckBidi are true for URL specification.

inline static direction find_direction(uint32_t code_point) noexcept {
  ensure_tables();
  // Binary search on dir_final (sorted upper bounds).
  size_t lo = 0, hi = dir_table_count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (dir_final[mid] < code_point) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  if (lo == dir_table_count) {
    return direction::NONE;
  }
  return code_point >= dir_start[lo] ? static_cast<direction>(dir_value[lo])
                                     : direction::NONE;
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

bool is_label_valid(const std::u32string_view label) {
  if (label.empty()) {
    return true;
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
  ensure_tables();
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
  constexpr static uint32_t virama[] = {
      0x094D,  0x09CD,  0x0A4D,  0x0ACD,  0x0B4D,  0x0BCD,  0x0C4D,  0x0CCD,
      0x0D3B,  0x0D3C,  0x0D4D,  0x0DCA,  0x0E3A,  0x0EBA,  0x0F84,  0x1039,
      0x103A,  0x1714,  0x1734,  0x17D2,  0x1A60,  0x1B44,  0x1BAA,  0x1BAB,
      0x1BF2,  0x1BF3,  0x2D7F,  0xA806,  0xA82C,  0xA8C4,  0xA953,  0xA9C0,
      0xAAF6,  0xABED,  0x10A3F, 0x11046, 0x1107F, 0x110B9, 0x11133, 0x11134,
      0x111C0, 0x11235, 0x112EA, 0x1134D, 0x11442, 0x114C2, 0x115BF, 0x1163F,
      0x116B6, 0x1172B, 0x11839, 0x1193D, 0x1193E, 0x119E0, 0x11A34, 0x11A47,
      0x11A99, 0x11C3F, 0x11D44, 0x11D45, 0x11D97};
  constexpr static uint32_t R[] = {
      0x622, 0x623, 0x624, 0x625, 0x627, 0x629, 0x62f, 0x630, 0x631,
      0x632, 0x648, 0x671, 0x672, 0x673, 0x675, 0x676, 0x677, 0x688,
      0x689, 0x68a, 0x68b, 0x68c, 0x68d, 0x68e, 0x68f, 0x690, 0x691,
      0x692, 0x693, 0x694, 0x695, 0x696, 0x697, 0x698, 0x699, 0x6c0,
      0x6c3, 0x6c4, 0x6c5, 0x6c6, 0x6c7, 0x6c8, 0x6c9, 0x6ca, 0x6cb,
      0x6cd, 0x6cf, 0x6d2, 0x6d3, 0x6d5, 0x6ee, 0x6ef, 0x710, 0x715,
      0x716, 0x717, 0x718, 0x719, 0x71e, 0x728, 0x72a, 0x72c, 0x72f,
      0x74d, 0x759, 0x75a, 0x75b, 0x854, 0x8aa, 0x8ab, 0x8ac};
  constexpr static uint32_t L[] = {0xa872};
  constexpr static uint32_t D[] = {
      0x620,  0x626,  0x628,  0x62a,  0x62b,  0x62c,  0x62d,  0x62e,  0x633,
      0x634,  0x635,  0x636,  0x637,  0x638,  0x639,  0x63a,  0x63b,  0x63c,
      0x63d,  0x63e,  0x63f,  0x641,  0x642,  0x643,  0x644,  0x645,  0x646,
      0x647,  0x649,  0x64a,  0x66e,  0x66f,  0x678,  0x679,  0x67a,  0x67b,
      0x67c,  0x67d,  0x67e,  0x67f,  0x680,  0x681,  0x682,  0x683,  0x684,
      0x685,  0x686,  0x687,  0x69a,  0x69b,  0x69c,  0x69d,  0x69e,  0x69f,
      0x6a0,  0x6a1,  0x6a2,  0x6a3,  0x6a4,  0x6a5,  0x6a6,  0x6a7,  0x6a8,
      0x6a9,  0x6aa,  0x6ab,  0x6ac,  0x6ad,  0x6ae,  0x6af,  0x6b0,  0x6b1,
      0x6b2,  0x6b3,  0x6b4,  0x6b5,  0x6b6,  0x6b7,  0x6b8,  0x6b9,  0x6ba,
      0x6bb,  0x6bc,  0x6bd,  0x6be,  0x6bf,  0x6c1,  0x6c2,  0x6cc,  0x6ce,
      0x6d0,  0x6d1,  0x6fa,  0x6fb,  0x6fc,  0x6ff,  0x712,  0x713,  0x714,
      0x71a,  0x71b,  0x71c,  0x71d,  0x71f,  0x720,  0x721,  0x722,  0x723,
      0x724,  0x725,  0x726,  0x727,  0x729,  0x72b,  0x72d,  0x72e,  0x74e,
      0x74f,  0x750,  0x751,  0x752,  0x753,  0x754,  0x755,  0x756,  0x757,
      0x758,  0x75c,  0x75d,  0x75e,  0x75f,  0x760,  0x761,  0x762,  0x763,
      0x764,  0x765,  0x766,  0x850,  0x851,  0x852,  0x853,  0x855,  0x8a0,
      0x8a2,  0x8a3,  0x8a4,  0x8a5,  0x8a6,  0x8a7,  0x8a8,  0x8a9,  0x1807,
      0x1820, 0x1821, 0x1822, 0x1823, 0x1824, 0x1825, 0x1826, 0x1827, 0x1828,
      0x1829, 0x182a, 0x182b, 0x182c, 0x182d, 0x182e, 0x182f, 0x1830, 0x1831,
      0x1832, 0x1833, 0x1834, 0x1835, 0x1836, 0x1837, 0x1838, 0x1839, 0x183a,
      0x183b, 0x183c, 0x183d, 0x183e, 0x183f, 0x1840, 0x1841, 0x1842, 0x1843,
      0x1844, 0x1845, 0x1846, 0x1847, 0x1848, 0x1849, 0x184a, 0x184b, 0x184c,
      0x184d, 0x184e, 0x184f, 0x1850, 0x1851, 0x1852, 0x1853, 0x1854, 0x1855,
      0x1856, 0x1857, 0x1858, 0x1859, 0x185a, 0x185b, 0x185c, 0x185d, 0x185e,
      0x185f, 0x1860, 0x1861, 0x1862, 0x1863, 0x1864, 0x1865, 0x1866, 0x1867,
      0x1868, 0x1869, 0x186a, 0x186b, 0x186c, 0x186d, 0x186e, 0x186f, 0x1870,
      0x1871, 0x1872, 0x1873, 0x1874, 0x1875, 0x1876, 0x1877, 0x1887, 0x1888,
      0x1889, 0x188a, 0x188b, 0x188c, 0x188d, 0x188e, 0x188f, 0x1890, 0x1891,
      0x1892, 0x1893, 0x1894, 0x1895, 0x1896, 0x1897, 0x1898, 0x1899, 0x189a,
      0x189b, 0x189c, 0x189d, 0x189e, 0x189f, 0x18a0, 0x18a1, 0x18a2, 0x18a3,
      0x18a4, 0x18a5, 0x18a6, 0x18a7, 0x18a8, 0x18aa, 0xa840, 0xa841, 0xa842,
      0xa843, 0xa844, 0xa845, 0xa846, 0xa847, 0xa848, 0xa849, 0xa84a, 0xa84b,
      0xa84c, 0xa84d, 0xa84e, 0xa84f, 0xa850, 0xa851, 0xa852, 0xa853, 0xa854,
      0xa855, 0xa856, 0xa857, 0xa858, 0xa859, 0xa85a, 0xa85b, 0xa85c, 0xa85d,
      0xa85e, 0xa85f, 0xa860, 0xa861, 0xa862, 0xa863, 0xa864, 0xa865, 0xa866,
      0xa867, 0xa868, 0xa869, 0xa86a, 0xa86b, 0xa86c, 0xa86d, 0xa86e, 0xa86f,
      0xa870, 0xa871};

  for (size_t i = 0; i < label.size(); i++) {
    uint32_t c = label[i];
    if (c == 0x200c) {
      if (i > 0) {
        if (std::ranges::binary_search(virama, label[i - 1])) {
          return true;
        }
      }
      if ((i == 0) || (i + 1 >= label.size())) {
        return false;
      }
      // we go backward looking for L or D
      auto is_l_or_d = [](uint32_t code) {
        return std::ranges::binary_search(L, code) ||
               std::ranges::binary_search(D, code);
      };
      auto is_r_or_d = [](uint32_t code) {
        return std::ranges::binary_search(R, code) ||
               std::ranges::binary_search(D, code);
      };
      std::u32string_view before = label.substr(0, i);
      std::u32string_view after = label.substr(i + 1);
      return (std::find_if(before.begin(), before.end(), is_l_or_d) !=
              before.end()) &&
             (std::find_if(after.begin(), after.end(), is_r_or_d) !=
              after.end());
    } else if (c == 0x200d) {
      if (i > 0) {
        if (std::ranges::binary_search(virama, label[i - 1])) {
          return true;
        }
      }
      return false;
    }
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
  // The following rule, consisting of six conditions, applies to labels in Bidi
  // domain names.
  if (is_rtl_label(label)) {
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

}  // namespace ada::idna
