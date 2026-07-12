// IDNA  17.0.0
// Normalization table constants and helpers.
// Large table payloads are DEFLATE-compressed; see table_store.hpp.

// clang-format off
#ifndef ADA_IDNA_NORMALIZATION_TABLES_H
#define ADA_IDNA_NORMALIZATION_TABLES_H
#include <cstdint>
#include <cstddef>

/**
 * Unicode Standard Annex #15 - UNICODE NORMALIZATION FORMS
 * https://www.unicode.org/reports/tr15/
 *
 * Table data is loaded from a compressed blob on first use.
 */

namespace ada::idna {

// Sentinel in 16-bit data arrays: actual value is in the high-plane table.
constexpr uint16_t NORM_CP_HIGH = 0xFFFF;

// Binary search a sorted uint16 index table; returns position or npos.
inline size_t find_u16(const uint16_t* arr, size_t n, uint16_t key) noexcept {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (arr[mid] < key) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  if (lo < n && arr[lo] == key) {
    return lo;
  }
  return static_cast<size_t>(-1);
}

inline char32_t decomposition_data_at(size_t i) noexcept {
  uint16_t v = decomposition_data16[i];
  if (v != NORM_CP_HIGH) {
    return static_cast<char32_t>(v);
  }
  size_t h = find_u16(decomposition_high_index, decomposition_high_count,
                      static_cast<uint16_t>(i));
  return h != static_cast<size_t>(-1) ? decomposition_high_cp[h] : 0;
}

inline char32_t composition_data_at(size_t i) noexcept {
  uint16_t v = composition_data16[i];
  if (v != NORM_CP_HIGH) {
    return static_cast<char32_t>(v);
  }
  size_t h = find_u16(composition_high_index, composition_high_count,
                      static_cast<uint16_t>(i));
  return h != static_cast<size_t>(-1) ? composition_high_cp[h] : 0;
}

// Look up composition block index for a Unicode page (cp >> 8).
inline uint8_t composition_block_for_page(uint32_t page) noexcept {
  size_t lo = 0, hi = composition_sparse_count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (composition_sparse_page[mid] < page) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  if (lo < composition_sparse_count && composition_sparse_page[lo] == page) {
    return composition_sparse_block[lo];
  }
  return composition_default_block;
}

}  // namespace ada::idna
#endif  // ADA_IDNA_NORMALIZATION_TABLES_H
