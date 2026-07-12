// Runtime store for compressed Unicode/IDNA tables.
// Large tables are DEFLATE-compressed (read-only) and expanded once on first
// use into a heap buffer so the working set does not bloat the on-disk binary.
// Hot-path lookups use the same O(1) multi-stage layout as the pre-compression
// code; compression only affects on-disk size and one-time init.
//
// Thread-safe without mutex: atomic CAS + spin-wait. ensure_tables() returns
// false if initialization fails (OOM, corrupt blob); callers must treat that
// as a hard error and not touch table pointers.
#pragma once

#include "raw_inflate.hpp"
#include "table_blob.inc"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <new>

namespace ada::idna {

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
static_assert(table_blob::compressed_size == sizeof(table_blob::compressed));

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

// ISO HDLC / zlib CRC-32 of the uncompressed table payload.
[[nodiscard]] inline uint32_t crc32_ieee(const uint8_t* data,
                                         size_t len) noexcept {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    c ^= data[i];
    for (int k = 0; k < 8; ++k) {
      const uint32_t mask = 0u - (c & 1u);
      c = (c >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~c;
}

[[nodiscard]] inline bool tables_are_ready() noexcept {
  return tables_init_state.load(std::memory_order_acquire) == kTablesReady;
}

// Returns true only when all table pointers are safe to use.
[[nodiscard]] inline bool ensure_tables() noexcept {
  uint8_t state = tables_init_state.load(std::memory_order_acquire);
  if (state == kTablesReady) {
    return true;
  }
  if (state == kTablesFailed) {
    return false;
  }

  uint8_t expected = kTablesUninit;
  if (tables_init_state.compare_exchange_strong(expected, kTablesInProgress,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
    uint8_t* buffer = new (std::nothrow) uint8_t[table_blob::uncompressed_size];
    if (buffer == nullptr) {
      tables_init_state.store(kTablesFailed, std::memory_order_release);
      return false;
    }

    const size_t n = deflate::inflate_raw(table_blob::compressed,
                                          table_blob::compressed_size, buffer,
                                          table_blob::uncompressed_size);
    if (n != table_blob::uncompressed_size ||
        crc32_ieee(buffer, n) != table_blob::uncompressed_crc32) {
      delete[] buffer;
      tables_init_state.store(kTablesFailed, std::memory_order_release);
      return false;
    }

    auto at = [&](size_t off) noexcept -> const uint8_t* {
      return buffer + off;
    };

    // Publish all non-atomic pointers before READY. Waiters synchronize via
    // acquire on tables_init_state and then observe these writes.
    idna_stage1 =
        reinterpret_cast<const uint16_t*>(at(table_blob::off_idna_stage1));
    idna_stage2 =
        reinterpret_cast<const uint16_t*>(at(table_blob::off_idna_stage2));
    idna_bool_blocks =
        reinterpret_cast<const uint64_t*>(at(table_blob::off_idna_bool_blocks));
    idna_utf8_mappings = at(table_blob::off_idna_utf8_mappings);

    decomposition_index = at(table_blob::off_decomposition_index);
    decomposition_block_flat = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_decomposition_block));
    decomposition_data = reinterpret_cast<const char32_t*>(
        at(table_blob::off_decomposition_data));
    ccc_index = at(table_blob::off_ccc_index);
    ccc_block_flat = at(table_blob::off_ccc_block);
    composition_index = at(table_blob::off_composition_index);
    composition_block_flat = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_composition_block));
    composition_data =
        reinterpret_cast<const char32_t*>(at(table_blob::off_composition_data));

    id_continue =
        reinterpret_cast<range_pair_ptr>(at(table_blob::off_id_continue_flat));
    id_start =
        reinterpret_cast<range_pair_ptr>(at(table_blob::off_id_start_flat));

    dir_start =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_dir_start));
    dir_final =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_dir_final));
    dir_value = at(table_blob::off_dir_value);
    combining_ranges =
        reinterpret_cast<range_pair_ptr>(at(table_blob::off_combining_flat));

    tables_buffer = buffer;
    tables_init_state.store(kTablesReady, std::memory_order_release);
    return true;
  }

  // Another thread owns init (or finished between our loads).
  for (uint64_t spins = 0; spins < kTablesSpinLimit; ++spins) {
    state = tables_init_state.load(std::memory_order_acquire);
    if (state == kTablesReady) {
      return true;
    }
    if (state == kTablesFailed) {
      return false;
    }
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
    __builtin_ia32_pause();
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("yield" ::: "memory");
#endif
#endif
  }
  // Timed out waiting for a peer - treat as failure rather than hang.
  return false;
}

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
