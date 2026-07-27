// Cold table init: inflate, unfilter, CRC, publish pointers.
// Compiled with -Os so the large blob and inflater do not bloat the hot -O3 TU.
#include "table_store.hpp"
#include "raw_inflate.hpp"
#include "table_blob_data.inc"

#include <atomic>
#include <cstdint>
#include <new>

namespace ada::idna {
namespace detail {

[[nodiscard]] bool unfilter_table_blob(uint8_t* buffer) noexcept {
  if constexpr (table_blob::filter_version == 0) {
    (void)buffer;
    return true;
  }

  struct Sec {
    uint32_t off;
    uint32_t count;
    uint8_t width;  // 2, 4, or 8
    uint8_t delta;  // 1 = prefix-sum after unsplit
  };
  static constexpr Sec kSecs[] = {
      {static_cast<uint32_t>(table_blob::off_idna_stage1),
       static_cast<uint32_t>(table_blob::count_idna_stage1), 2, 1},
      {static_cast<uint32_t>(table_blob::off_idna_stage2),
       static_cast<uint32_t>(table_blob::count_idna_stage2), 2, 1},
      {static_cast<uint32_t>(table_blob::off_idna_bool_blocks),
       static_cast<uint32_t>(table_blob::count_idna_bool_blocks), 8, 0},
      {static_cast<uint32_t>(table_blob::off_decomposition_block),
       static_cast<uint32_t>(table_blob::count_decomposition_block), 2, 1},
      {static_cast<uint32_t>(table_blob::off_decomposition_data),
       static_cast<uint32_t>(table_blob::count_decomposition_data), 4, 1},
      {static_cast<uint32_t>(table_blob::off_composition_block),
       static_cast<uint32_t>(table_blob::count_composition_block), 2, 1},
      {static_cast<uint32_t>(table_blob::off_composition_data),
       static_cast<uint32_t>(table_blob::count_composition_data), 4, 1},
      {static_cast<uint32_t>(table_blob::off_id_continue_flat),
       static_cast<uint32_t>(table_blob::count_id_continue_flat), 4, 1},
      {static_cast<uint32_t>(table_blob::off_id_start_flat),
       static_cast<uint32_t>(table_blob::count_id_start_flat), 4, 1},
      {static_cast<uint32_t>(table_blob::off_dir_start),
       static_cast<uint32_t>(table_blob::count_dir_start), 4, 1},
      {static_cast<uint32_t>(table_blob::off_dir_final),
       static_cast<uint32_t>(table_blob::count_dir_final), 4, 1},
      {static_cast<uint32_t>(table_blob::off_combining_flat),
       static_cast<uint32_t>(table_blob::count_combining_flat), 4, 1},
  };

  size_t scratch_need = 0;
  for (const Sec& s : kSecs) {
    const size_t bytes = static_cast<size_t>(s.count) * s.width;
    if (bytes > scratch_need) {
      scratch_need = bytes;
    }
  }
  uint8_t* scratch = new (std::nothrow) uint8_t[scratch_need];
  if (scratch == nullptr) {
    return false;
  }

  for (const Sec& s : kSecs) {
    uint8_t* data = buffer + s.off;
    const size_t count = s.count;
    const size_t width = s.width;
    for (size_t i = 0; i < count; ++i) {
      for (size_t b = 0; b < width; ++b) {
        scratch[i * width + b] = data[b * count + i];
      }
    }
    const size_t nbytes = count * width;
    for (size_t i = 0; i < nbytes; ++i) {
      data[i] = scratch[i];
    }
    if (s.delta) {
      if (width == 2) {
        auto* p = reinterpret_cast<uint16_t*>(data);
        uint16_t prev = 0;
        for (size_t i = 0; i < count; ++i) {
          prev = static_cast<uint16_t>(prev + p[i]);
          p[i] = prev;
        }
      } else {
        auto* p = reinterpret_cast<uint32_t*>(data);
        uint32_t prev = 0;
        for (size_t i = 0; i < count; ++i) {
          prev += p[i];
          p[i] = prev;
        }
      }
    }
  }

  delete[] scratch;
  return true;
}

}  // namespace detail

namespace {

[[nodiscard]] uint32_t crc32_ieee(const uint8_t* data, size_t len) noexcept {
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

}  // namespace

[[nodiscard]] bool ensure_tables() noexcept {
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
    if (n != table_blob::uncompressed_size) {
      delete[] buffer;
      tables_init_state.store(kTablesFailed, std::memory_order_release);
      return false;
    }

    if (!detail::unfilter_table_blob(buffer)) {
      delete[] buffer;
      tables_init_state.store(kTablesFailed, std::memory_order_release);
      return false;
    }

    if (crc32_ieee(buffer, n) != table_blob::uncompressed_crc32) {
      delete[] buffer;
      tables_init_state.store(kTablesFailed, std::memory_order_release);
      return false;
    }

    detail::convert_table_blob_to_host_endian(buffer);

    auto at = [&](size_t off) noexcept -> const uint8_t* {
      return buffer + off;
    };

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
  return false;
}

}  // namespace ada::idna
