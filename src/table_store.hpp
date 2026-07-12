// Runtime store for compressed Unicode/IDNA tables.
// Large tables are DEFLATE-compressed (read-only) and expanded once on first
// use into a heap buffer so the working set does not bloat the on-disk binary.
// Hot-path lookups use the same O(1) multi-stage layout as the pre-compression
// code; compression only affects on-disk size and one-time init.
#pragma once

#include "raw_inflate.hpp"
#include "table_blob.inc"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <mutex>

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
static_assert(table_blob::off_idna_bool_blocks % alignof(uint64_t) == 0);
static_assert(table_blob::off_decomposition_data % alignof(char32_t) == 0);
static_assert(table_blob::off_dir_start % alignof(uint32_t) == 0);

// --- Mapping -----------------------------------------------------------------
inline const uint16_t* idna_stage1 = nullptr;
inline const uint16_t* idna_stage2 = nullptr;
inline const uint64_t* idna_bool_blocks = nullptr;
inline const uint8_t* idna_utf8_mappings = nullptr;

// --- Normalization (O(1) multi-stage, same layout as uni-algo style tables) --
inline const uint8_t* decomposition_index = nullptr;
// Row-major [rows][cols]; access via helpers below.
inline const uint16_t* decomposition_block_flat = nullptr;
inline const char32_t* decomposition_data = nullptr;
inline const uint8_t* ccc_index = nullptr;
inline const uint8_t* ccc_block_flat = nullptr;
inline const uint8_t* composition_index = nullptr;
inline const uint16_t* composition_block_flat = nullptr;
inline const char32_t* composition_data = nullptr;

// --- Identifier --------------------------------------------------------------
inline const uint32_t (*id_continue)[2] = nullptr;
inline const uint32_t (*id_start)[2] = nullptr;

// --- Validity (const SoA) ----------------------------------------------------
inline const uint32_t* dir_start = nullptr;
inline const uint32_t* dir_final = nullptr;
inline const uint8_t* dir_value = nullptr;
inline const uint32_t (*combining_ranges)[2] = nullptr;

inline constexpr size_t id_continue_count = table_blob::id_continue_count;
inline constexpr size_t id_start_count = table_blob::id_start_count;
inline constexpr size_t dir_table_count = table_blob::dir_table_count;
inline constexpr size_t combining_range_count =
    table_blob::combining_range_count;

inline constexpr size_t kDecompBlockCols =
    table_blob::decomposition_block_cols;                            // 257
inline constexpr size_t kCccBlockCols = table_blob::ccc_block_cols;  // 256
inline constexpr size_t kCompBlockCols =
    table_blob::composition_block_cols;  // 257

// Fast path: avoid call_once synchronization after first init.
inline std::atomic<bool> tables_ready{false};

inline void ensure_tables() {
  if (tables_ready.load(std::memory_order_acquire)) {
    return;
  }
  static std::once_flag once;
  std::call_once(once, [] {
    static std::unique_ptr<uint8_t[]> holder(
        new uint8_t[table_blob::uncompressed_size]);
    uint8_t* buffer = holder.get();
    const size_t n = deflate::inflate_raw(table_blob::compressed,
                                          table_blob::compressed_size, buffer,
                                          table_blob::uncompressed_size);
    if (n != table_blob::uncompressed_size) {
      holder.reset();
      return;
    }

    auto at = [&](size_t off) -> const uint8_t* { return buffer + off; };

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

    id_continue = reinterpret_cast<const uint32_t(*)[2]>(
        at(table_blob::off_id_continue_flat));
    id_start =
        reinterpret_cast<const uint32_t(*)[2]>(at(table_blob::off_id_start_flat));

    dir_start =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_dir_start));
    dir_final =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_dir_final));
    dir_value = at(table_blob::off_dir_value);
    combining_ranges = reinterpret_cast<const uint32_t(*)[2]>(
        at(table_blob::off_combining_flat));

    tables_ready.store(true, std::memory_order_release);
  });
}

// O(1) multi-stage accessors matching the original uni-algo layout.
inline const uint16_t* decomposition_block_row(uint8_t bi) noexcept {
  return decomposition_block_flat + static_cast<size_t>(bi) * kDecompBlockCols;
}

inline const uint8_t* ccc_block_row(uint8_t bi) noexcept {
  return ccc_block_flat + static_cast<size_t>(bi) * kCccBlockCols;
}

inline const uint16_t* composition_block_row(uint8_t bi) noexcept {
  return composition_block_flat + static_cast<size_t>(bi) * kCompBlockCols;
}

}  // namespace ada::idna
