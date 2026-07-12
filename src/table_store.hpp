// Runtime store for compressed Unicode/IDNA tables.
// Large tables are stored DEFLATE-compressed and expanded once on first use
// into a BSS buffer (does not increase binary size).
#pragma once

#include "raw_inflate.hpp"
#include "table_blob.inc"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <mutex>

namespace ada::idna {

// --- Mapping tables ----------------------------------------------------------
inline const uint16_t* idna_stage1 = nullptr;
inline const uint16_t* idna_stage2 = nullptr;
inline const uint64_t* idna_bool_blocks = nullptr;
inline const uint8_t* idna_utf8_mappings = nullptr;

// --- Normalization tables ----------------------------------------------------
inline const uint32_t* decomposition_cp = nullptr;
inline const uint16_t* decomposition_offset = nullptr;
inline const uint8_t* decomposition_length = nullptr;
inline const uint16_t* decomposition_data16 = nullptr;
inline const uint16_t* decomposition_high_index = nullptr;
inline const char32_t* decomposition_high_cp = nullptr;
inline const uint32_t* ccc_range_start = nullptr;
inline const uint8_t* ccc_range_length = nullptr;
inline const uint8_t* ccc_range_value = nullptr;
inline const uint16_t* composition_sparse_page = nullptr;
inline const uint8_t* composition_sparse_block = nullptr;
// composition_block is [block][257] laid out row-major in a flat array.
inline const uint16_t* composition_block_flat = nullptr;
inline const uint16_t* composition_data16 = nullptr;
inline const uint16_t* composition_high_index = nullptr;
inline const char32_t* composition_high_cp = nullptr;

// --- Identifier tables -------------------------------------------------------
// Each entry is a [low, high] inclusive range; count is number of ranges.
inline const uint32_t (*id_continue)[2] = nullptr;
inline const uint32_t (*id_start)[2] = nullptr;

// --- Validity tables ---------------------------------------------------------
inline const uint32_t* dir_start = nullptr;
inline const uint32_t* dir_final = nullptr;
inline const uint8_t* dir_value = nullptr;
inline const uint32_t (*combining_ranges)[2] = nullptr;

// Counts (also available as table_blob::*)
inline constexpr size_t decomposition_count = table_blob::decomposition_count;
inline constexpr size_t decomposition_high_count =
    table_blob::decomposition_high_count;
inline constexpr size_t ccc_range_count = table_blob::ccc_range_count;
inline constexpr size_t composition_sparse_count =
    table_blob::composition_sparse_count;
inline constexpr size_t composition_block_count =
    table_blob::composition_block_count;
inline constexpr size_t composition_high_count =
    table_blob::composition_high_count;
inline constexpr size_t id_continue_count = table_blob::id_continue_count;
inline constexpr size_t id_start_count = table_blob::id_start_count;
inline constexpr size_t dir_table_count = table_blob::dir_table_count;
inline constexpr size_t combining_range_count =
    table_blob::combining_range_count;

// composition_default_block is a small constant (not in the blob).
inline constexpr uint8_t composition_default_block = 5;

inline void ensure_tables() {
  static std::once_flag once;
  std::call_once(once, [] {
    // Heap storage so the ~134 KB working tables never bloat the on-disk
    // binary (a static buffer can end up in __data on some toolchains).
    static std::unique_ptr<uint8_t[]> holder(
        new uint8_t[table_blob::uncompressed_size]);
    uint8_t* buffer = holder.get();
    const size_t n = deflate::inflate_raw(table_blob::compressed,
                                          table_blob::compressed_size, buffer,
                                          table_blob::uncompressed_size);
    if (n != table_blob::uncompressed_size) {
      // Tables are trusted build artifacts; a mismatch means a corrupt build.
      // Leave pointers null so subsequent use fails loudly rather than
      // reading garbage.
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

    decomposition_cp =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_decomposition_cp));
    decomposition_offset = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_decomposition_offset));
    decomposition_length = at(table_blob::off_decomposition_length);
    decomposition_data16 = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_decomposition_data16));
    decomposition_high_index = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_decomposition_high_index));
    decomposition_high_cp = reinterpret_cast<const char32_t*>(
        at(table_blob::off_decomposition_high_cp));

    ccc_range_start =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_ccc_range_start));
    ccc_range_length = at(table_blob::off_ccc_range_length);
    ccc_range_value = at(table_blob::off_ccc_range_value);

    composition_sparse_page = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_composition_sparse_page));
    composition_sparse_block = at(table_blob::off_composition_sparse_block);
    composition_block_flat = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_composition_block_flat));
    composition_data16 = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_composition_data16));
    composition_high_index = reinterpret_cast<const uint16_t*>(
        at(table_blob::off_composition_high_index));
    composition_high_cp = reinterpret_cast<const char32_t*>(
        at(table_blob::off_composition_high_cp));

    id_continue = reinterpret_cast<const uint32_t (*)[2]>(
        at(table_blob::off_id_continue_flat));
    id_start = reinterpret_cast<const uint32_t (*)[2]>(
        at(table_blob::off_id_start_flat));

    dir_start =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_dir_start));
    dir_final =
        reinterpret_cast<const uint32_t*>(at(table_blob::off_dir_final));
    dir_value = at(table_blob::off_dir_value);
    combining_ranges = reinterpret_cast<const uint32_t (*)[2]>(
        at(table_blob::off_combining_flat));
  });
}

// Row accessor for the flat composition block table.
inline const uint16_t* composition_block_row(uint8_t block_index) noexcept {
  return composition_block_flat + static_cast<size_t>(block_index) * 257u;
}

}  // namespace ada::idna
