// Runtime store for compressed Unicode/IDNA tables.
// Large tables are prefiltered (delta + byte-plane split) then raw-DEFLATE
// compressed (read-only) and expanded once on first use into a heap buffer so
// the working set does not bloat the on-disk binary. Hot-path lookups use the
// same O(1) multi-stage layout as the pre-compression code; compression only
// affects on-disk size and one-time init.
//
// Thread-safe without mutex: atomic CAS + spin-wait. ensure_tables() returns
// false if initialization fails (OOM, corrupt blob); callers must treat that
// as a hard error and not touch table pointers.
#pragma once

#include "table_blob.inc"

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstddef>
#include <new>

namespace ada::idna {

// table_blob.inc is packed little-endian (see scripts/pack_tables.py). After
// inflate the multi-byte sections must be converted to host endianness so
// big-endian platforms (s390x) can load uint16_t/uint32_t/uint64_t natively.
// CRC is checked on the raw little-endian payload before any conversion.
namespace detail {

template <typename T>
inline void bswap_inplace(T* data, size_t count) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    (void)data;
    (void)count;
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    if constexpr (sizeof(T) == 2) {
#if defined(__GNUC__) || defined(__clang__)
      data[i] =
          static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(data[i])));
#else
      const auto v = static_cast<uint16_t>(data[i]);
      data[i] = static_cast<T>(static_cast<uint16_t>((v >> 8) | (v << 8)));
#endif
    } else if constexpr (sizeof(T) == 4) {
#if defined(__GNUC__) || defined(__clang__)
      data[i] =
          static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(data[i])));
#else
      auto v = static_cast<uint32_t>(data[i]);
      v = ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
          ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
      data[i] = static_cast<T>(v);
#endif
    } else if constexpr (sizeof(T) == 8) {
#if defined(__GNUC__) || defined(__clang__)
      data[i] =
          static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(data[i])));
#else
      auto v = static_cast<uint64_t>(data[i]);
      v = ((v & 0x00000000000000FFull) << 56) |
          ((v & 0x000000000000FF00ull) << 40) |
          ((v & 0x0000000000FF0000ull) << 24) |
          ((v & 0x00000000FF000000ull) << 8) |
          ((v & 0x000000FF00000000ull) >> 8) |
          ((v & 0x0000FF0000000000ull) >> 24) |
          ((v & 0x00FF000000000000ull) >> 40) |
          ((v & 0xFF00000000000000ull) >> 56);
      data[i] = static_cast<T>(v);
#endif
    } else {
      static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
    }
  }
}

// Host-endian conversion and unfilter live in tables_init.cpp (cold TU).
// Declared here for tests that exercise bswap helpers directly.
inline void convert_table_blob_to_host_endian(uint8_t* buffer) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    (void)buffer;
    return;
  }
  auto u16 = [&](size_t off, size_t count) {
    bswap_inplace(reinterpret_cast<uint16_t*>(buffer + off), count);
  };
  auto u32 = [&](size_t off, size_t count) {
    bswap_inplace(reinterpret_cast<uint32_t*>(buffer + off), count);
  };
  auto u64 = [&](size_t off, size_t count) {
    bswap_inplace(reinterpret_cast<uint64_t*>(buffer + off), count);
  };

  u16(table_blob::off_idna_stage1, table_blob::count_idna_stage1);
  u16(table_blob::off_idna_stage2, table_blob::count_idna_stage2);
  u64(table_blob::off_idna_bool_blocks, table_blob::count_idna_bool_blocks);
  u16(table_blob::off_decomposition_block,
      table_blob::count_decomposition_block);
  u32(table_blob::off_decomposition_data, table_blob::count_decomposition_data);
  u16(table_blob::off_composition_block, table_blob::count_composition_block);
  u32(table_blob::off_composition_data, table_blob::count_composition_data);
  u32(table_blob::off_id_continue_flat, table_blob::count_id_continue_flat);
  u32(table_blob::off_id_start_flat, table_blob::count_id_start_flat);
  u32(table_blob::off_dir_start, table_blob::count_dir_start);
  u32(table_blob::off_dir_final, table_blob::count_dir_final);
  u32(table_blob::off_combining_flat, table_blob::count_combining_flat);
}

// Unfilter is implemented in tables_init.cpp (cold -Os TU).
[[nodiscard]] bool unfilter_table_blob(uint8_t* buffer) noexcept;

}  // namespace detail

// --- Blob layout invariants --------------------------------------------------
static_assert(table_blob::count_decomposition_index == 4352);
static_assert(table_blob::count_ccc_index == 4352);
static_assert(table_blob::count_composition_index == 4352);
static_assert(table_blob::count_decomposition_block ==
              table_blob::decomposition_block_rows *
                  table_blob::decomposition_block_cols);
static_assert(table_blob::count_ccc_block ==
              table_blob::ccc_block_rows * table_blob::ccc_block_cols);
static_assert(table_blob::count_composition_block ==
              table_blob::composition_block_rows *
                  table_blob::composition_block_cols);
static_assert(table_blob::count_dir_start == table_blob::dir_table_count &&
              table_blob::count_dir_final == table_blob::dir_table_count &&
              table_blob::count_dir_value == table_blob::dir_table_count);
static_assert(table_blob::count_id_continue_flat ==
              table_blob::id_continue_count * 2);
static_assert(table_blob::count_id_start_flat ==
              table_blob::id_start_count * 2);
static_assert(table_blob::count_combining_flat ==
              table_blob::combining_range_count * 2);
static_assert(table_blob::off_idna_stage1 % alignof(uint16_t) == 0);
static_assert(table_blob::off_idna_stage2 % alignof(uint16_t) == 0);
static_assert(table_blob::off_idna_bool_blocks % alignof(uint64_t) == 0);
static_assert(table_blob::off_decomposition_block % alignof(uint16_t) == 0);
static_assert(table_blob::off_decomposition_data % alignof(char32_t) == 0);
static_assert(table_blob::off_composition_block % alignof(uint16_t) == 0);
static_assert(table_blob::off_composition_data % alignof(char32_t) == 0);
static_assert(table_blob::off_id_continue_flat % alignof(uint32_t) == 0);
static_assert(table_blob::off_id_start_flat % alignof(uint32_t) == 0);
static_assert(table_blob::off_dir_start % alignof(uint32_t) == 0);
static_assert(table_blob::off_dir_final % alignof(uint32_t) == 0);
static_assert(table_blob::off_combining_flat % alignof(uint32_t) == 0);

// Every section must lie entirely inside the uncompressed buffer.
#define ADA_IDNA_SECTION_IN_BOUNDS(off, count, width)                       \
  static_assert((off) + (count) * (width) <= table_blob::uncompressed_size, \
                #off " overflows uncompressed buffer")
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_idna_stage1,
                           table_blob::count_idna_stage1, 2);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_idna_stage2,
                           table_blob::count_idna_stage2, 2);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_idna_bool_blocks,
                           table_blob::count_idna_bool_blocks, 8);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_idna_utf8_mappings,
                           table_blob::count_idna_utf8_mappings, 1);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_decomposition_index,
                           table_blob::count_decomposition_index, 1);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_decomposition_block,
                           table_blob::count_decomposition_block, 2);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_decomposition_data,
                           table_blob::count_decomposition_data, 4);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_ccc_index,
                           table_blob::count_ccc_index, 1);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_ccc_block,
                           table_blob::count_ccc_block, 1);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_composition_index,
                           table_blob::count_composition_index, 1);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_composition_block,
                           table_blob::count_composition_block, 2);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_composition_data,
                           table_blob::count_composition_data, 4);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_id_continue_flat,
                           table_blob::count_id_continue_flat, 4);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_id_start_flat,
                           table_blob::count_id_start_flat, 4);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_dir_start,
                           table_blob::count_dir_start, 4);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_dir_final,
                           table_blob::count_dir_final, 4);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_dir_value,
                           table_blob::count_dir_value, 1);
ADA_IDNA_SECTION_IN_BOUNDS(table_blob::off_combining_flat,
                           table_blob::count_combining_flat, 4);
#undef ADA_IDNA_SECTION_IN_BOUNDS

// --- Mapping -----------------------------------------------------------------
inline const uint16_t* idna_stage1 = nullptr;
inline const uint16_t* idna_stage2 = nullptr;
inline const uint64_t* idna_bool_blocks = nullptr;
inline const uint8_t* idna_utf8_mappings = nullptr;

// --- Normalization (O(1) multi-stage) ----------------------------------------
inline const uint8_t* decomposition_index = nullptr;
inline const uint16_t* decomposition_block_flat = nullptr;
inline const char32_t* decomposition_data = nullptr;
inline const uint8_t* ccc_index = nullptr;
inline const uint8_t* ccc_block_flat = nullptr;
inline const uint8_t* composition_index = nullptr;
inline const uint16_t* composition_block_flat = nullptr;
inline const char32_t* composition_data = nullptr;

// --- Identifier --------------------------------------------------------------
// Pointer-to-array alias. CF17 vs CF22 disagree on spacing inside (*)[2].
// clang-format off
using range_pair_ptr = const uint32_t(*)[2];
// clang-format on
inline range_pair_ptr id_continue = nullptr;
inline range_pair_ptr id_start = nullptr;

// --- Validity (const SoA) ----------------------------------------------------
inline const uint32_t* dir_start = nullptr;
inline const uint32_t* dir_final = nullptr;
inline const uint8_t* dir_value = nullptr;
inline range_pair_ptr combining_ranges = nullptr;

inline constexpr size_t id_continue_count = table_blob::id_continue_count;
inline constexpr size_t id_start_count = table_blob::id_start_count;
inline constexpr size_t dir_table_count = table_blob::dir_table_count;
inline constexpr size_t combining_range_count =
    table_blob::combining_range_count;
inline constexpr size_t idna_utf8_mappings_size =
    table_blob::count_idna_utf8_mappings;
inline constexpr size_t decomposition_data_size =
    table_blob::count_decomposition_data;
inline constexpr size_t composition_data_size =
    table_blob::count_composition_data;

inline constexpr size_t kDecompBlockCols =
    table_blob::decomposition_block_cols;                            // 257
inline constexpr size_t kCccBlockCols = table_blob::ccc_block_cols;  // 256
inline constexpr size_t kCompBlockCols =
    table_blob::composition_block_cols;  // 257
inline constexpr size_t kDecompBlockRows = table_blob::decomposition_block_rows;
inline constexpr size_t kCccBlockRows = table_blob::ccc_block_rows;
inline constexpr size_t kCompBlockRows = table_blob::composition_block_rows;

// Init state: 0=uninit, 1=in progress, 2=ready, 3=failed.
inline constexpr uint8_t kTablesUninit = 0;
inline constexpr uint8_t kTablesInProgress = 1;
inline constexpr uint8_t kTablesReady = 2;
inline constexpr uint8_t kTablesFailed = 3;

inline std::atomic<uint8_t> tables_init_state{kTablesUninit};
// Process-lifetime allocation published only by the winning init thread.
// Never freed: shared read-only data for the process. Do not dlclose a DSO
// that owns this buffer while other code may still call into ada::idna.
inline uint8_t* tables_buffer = nullptr;

// Cap spin-wait so a stuck peer cannot hang the process forever.
inline constexpr uint64_t kTablesSpinLimit = 1'000'000'000ull;

[[nodiscard]] inline bool tables_are_ready() noexcept {
  return tables_init_state.load(std::memory_order_acquire) == kTablesReady;
}

// Defined in tables_init.cpp (cold -Os TU: inflate + blob + unfilter).
// Returns true only when all table pointers are safe to use.
[[nodiscard]] bool ensure_tables() noexcept;

// O(1) multi-stage accessors. Block indices from the tables are uint8_t and
// theoretically can be out of range if the blob is corrupt; clamp to a valid
// empty/default row (index 0) instead of reading OOB.
inline const uint16_t* decomposition_block_row(uint8_t bi) noexcept {
  if (bi >= kDecompBlockRows) {
    bi = 0;
  }
  return decomposition_block_flat + static_cast<size_t>(bi) * kDecompBlockCols;
}

inline const uint8_t* ccc_block_row(uint8_t bi) noexcept {
  if (bi >= kCccBlockRows) {
    bi = 0;
  }
  return ccc_block_flat + static_cast<size_t>(bi) * kCccBlockCols;
}

inline const uint16_t* composition_block_row(uint8_t bi) noexcept {
  if (bi >= kCompBlockRows) {
    bi = 0;
  }
  return composition_block_flat + static_cast<size_t>(bi) * kCompBlockCols;
}

}  // namespace ada::idna
