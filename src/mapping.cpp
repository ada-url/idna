#include "ada/idna/mapping.h"

#include <array>
#include <cstring>
#include <cstdint>
#include <string>

#include "table_store.hpp"
#include "mapping_tables.cpp"

namespace ada::idna {

// --- O(1) two-level table lookup ---------------------------------------------
//
// Returns one of:
//   IDNA_VALID      - keep code point in output unchanged
//   IDNA_DISALLOWED - code point is not allowed (map() returns error)
//   IDNA_IGNORED    - code point is ignored (index 0 = empty UTF-8 entry)
//   other           - byte offset into idna_utf8_mappings[] (null-terminated)
//
// Assumes ensure_tables() has already been called by the public entry point.
static uint16_t idna_lookup(uint32_t cp) noexcept {
  if (cp < IDNA_LOW_RANGE_END) {
    uint16_t ref = idna_stage1[cp >> IDNA_BLOCK_BITS];
    if (ref & IDNA_BOOL_FLAG) {
      uint32_t bit_idx =
          static_cast<uint32_t>(ref & ~IDNA_BOOL_FLAG) * IDNA_BLOCK_SIZE +
          (cp & IDNA_BLOCK_MASK);
      bool is_valid = (idna_bool_blocks[bit_idx >> 6] >> (bit_idx & 63u)) & 1u;
      return is_valid ? IDNA_VALID : IDNA_DISALLOWED;
    }
    return idna_stage2[ref + (cp & IDNA_BLOCK_MASK)];
  }

  if (cp >= IDNA_HIGH_IGNORED_START && cp < IDNA_HIGH_IGNORED_END) {
    return IDNA_IGNORED;
  }

  return IDNA_DISALLOWED;
}

// Advances *ptr past one well-formed UTF-8 code point from the mapping table.
static char32_t utf8_next(const uint8_t*& ptr) noexcept {
  uint8_t b0 = *ptr++;
  if (b0 < 0x80u) return static_cast<char32_t>(b0);
  if (b0 < 0xE0u) {
    uint32_t cp = (b0 & 0x1Fu) << 6;
    cp |= (*ptr++ & 0x3Fu);
    return static_cast<char32_t>(cp);
  }
  if (b0 < 0xF0u) {
    uint32_t cp = (b0 & 0x0Fu) << 12;
    cp |= ((*ptr++ & 0x3Fu) << 6);
    cp |= (*ptr++ & 0x3Fu);
    return static_cast<char32_t>(cp);
  }
  uint32_t cp = (b0 & 0x07u) << 18;
  cp |= ((*ptr++ & 0x3Fu) << 12);
  cp |= ((*ptr++ & 0x3Fu) << 6);
  cp |= (*ptr++ & 0x3Fu);
  return static_cast<char32_t>(cp);
}

// Count UTF-8 code points in a null-terminated mapping string (trusted table).
static size_t utf8_count_codepoints(const uint8_t* ptr) noexcept {
  size_t n = 0;
  while (*ptr != 0) {
    ++n;
    if (*ptr < 0x80u) {
      ++ptr;
    } else if (*ptr < 0xE0u) {
      ptr += 2;
    } else if (*ptr < 0xF0u) {
      ptr += 3;
    } else {
      ptr += 4;
    }
  }
  return n;
}

// --- ASCII fast path ---------------------------------------------------------
void ascii_map(char* input, size_t length) {
  auto broadcast = [](uint8_t v) -> uint64_t {
    return 0x101010101010101ull * v;
  };
  uint64_t broadcast_80 = broadcast(0x80);
  uint64_t broadcast_Ap = broadcast(128 - 'A');
  uint64_t broadcast_Zp = broadcast(128 - 'Z' - 1);
  size_t i = 0;

  for (; i + 7 < length; i += 8) {
    uint64_t word{};
    std::memcpy(&word, input + i, sizeof(word));
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    std::memcpy(input + i, &word, sizeof(word));
  }
  if (i < length) {
    uint64_t word{};
    std::memcpy(&word, input + i, length - i);
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    std::memcpy(input + i, &word, length - i);
  }
}

// Two-pass map: first validate + exact size, then write once (no growth
// realloc).
bool map(std::u32string_view input, std::u32string& out) {
  //  [Map](https://www.unicode.org/reports/tr46/#ProcessingStepMap).
  out.clear();
  if (!ensure_tables()) {
    return false;
  }

  // Pass 1: validate and compute exact output length.
  size_t out_size = 0;
  for (char32_t x : input) {
    uint16_t status = idna_lookup(static_cast<uint32_t>(x));
    if (status == IDNA_DISALLOWED) {
      return false;
    }
    if (status == IDNA_VALID) {
      ++out_size;
      continue;
    }
    if (static_cast<size_t>(status) >= idna_utf8_mappings_size) {
      return false;
    }
    out_size += utf8_count_codepoints(idna_utf8_mappings + status);
  }

  // Pass 2: write into a single allocation.
  out.resize(out_size);
  size_t w = 0;
  for (char32_t x : input) {
    uint16_t status = idna_lookup(static_cast<uint32_t>(x));
    if (status == IDNA_VALID) {
      out[w++] = x;
      continue;
    }
    // IGNORED or mapped (status already validated in pass 1).
    const uint8_t* ptr = idna_utf8_mappings + status;
    while (*ptr != 0) {
      out[w++] = utf8_next(ptr);
    }
  }
  return true;
}

std::u32string map(std::u32string_view input) {
  std::u32string answer;
  if (!map(input, answer)) {
    return {};
  }
  return answer;
}

}  // namespace ada::idna
