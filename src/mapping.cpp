#include "ada/idna/mapping.h"

#include <array>
#include <cstdint>
#include <string>

#include "mapping_tables.cpp"

namespace ada::idna {

// ─── O(1) two-level table lookup ─────────────────────────────────────────────
//
// Returns one of:
//   IDNA_VALID      – keep code point in output unchanged
//   IDNA_DISALLOWED – code point is not allowed (map() returns error)
//   IDNA_IGNORED    – code point is ignored (index 0 = empty UTF-8 entry)
//   other           – byte offset into idna_utf8_mappings[] (null-terminated)
//
// The two-level table covers [0, IDNA_LOW_RANGE_END).  All constants
// (LOW_RANGE_END, HIGH_IGNORED_*) are generated from the IDNA table itself;
// no Unicode version-specific values are hardcoded here.
//
static uint16_t idna_lookup(uint32_t cp) noexcept {
  // ── Two-level table covers the full active code-point range ───────────────
  if (cp < IDNA_LOW_RANGE_END) {
    uint16_t ref = idna_stage1[cp >> IDNA_BLOCK_BITS];
    if (ref & IDNA_BOOL_FLAG) {
      // Boolean block: one bit per code point, 1 = VALID, 0 = DISALLOWED.
      uint32_t bit_idx = static_cast<uint32_t>(ref & ~IDNA_BOOL_FLAG) * IDNA_BLOCK_SIZE
                         + (cp & IDNA_BLOCK_MASK);
      bool is_valid = (idna_bool_blocks[bit_idx >> 6] >> (bit_idx & 63u)) & 1u;
      return is_valid ? IDNA_VALID : IDNA_DISALLOWED;
    }
    return idna_stage2[ref + (cp & IDNA_BLOCK_MASK)];
  }

  // ── Variation selectors supplement (U+E0100–U+E01EF): all ignored ─────────
  // Everything else above IDNA_LOW_RANGE_END is disallowed.
  if (cp >= IDNA_HIGH_IGNORED_START && cp < IDNA_HIGH_IGNORED_END) {
    return IDNA_IGNORED;
  }

  return IDNA_DISALLOWED;
}

// ─── Decode one UTF-8 code point ─────────────────────────────────────────────
// Advances *ptr past the bytes consumed.  The mapping table is trusted to be
// well-formed UTF-8, so no validity checking is performed.
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
  // 4-byte sequence
  uint32_t cp = (b0 & 0x07u) << 18;
  cp |= ((*ptr++ & 0x3Fu) << 12);
  cp |= ((*ptr++ & 0x3Fu) << 6);
  cp |= (*ptr++ & 0x3Fu);
  return static_cast<char32_t>(cp);
}

// ─── ASCII fast path ──────────────────────────────────────────────────────────
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
    __builtin_memcpy(&word, input + i, sizeof(word));
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    __builtin_memcpy(input + i, &word, sizeof(word));
  }
  if (i < length) {
    uint64_t word{};
    __builtin_memcpy(&word, input + i, length - i);
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    __builtin_memcpy(input + i, &word, length - i);
  }
}

// ─── IDNA map ─────────────────────────────────────────────────────────────────
// Maps each code point according to IDNA processing.
// Returns an empty string on error (disallowed code point encountered).
std::u32string map(std::u32string_view input) {
  //  [Map](https://www.unicode.org/reports/tr46/#ProcessingStepMap).
  //  For each code point in the domain_name string, look up the status
  //  value in Section 5, [IDNA Mapping
  //  Table](https://www.unicode.org/reports/tr46/#IDNA_Mapping_Table),
  //  and take the following actions:
  //    * disallowed: Leave the code point unchanged in the string, and
  //    record that there was an error.
  //    * ignored: Remove the code point from the string.
  //    * mapped: Replace the code point in the string by the value for
  //    the mapping in Section 5, [IDNA Mapping
  //    Table](https://www.unicode.org/reports/tr46/#IDNA_Mapping_Table).
  //    * valid: Leave the code point unchanged in the string.
  static std::u32string error = U"";
  std::u32string answer;
  answer.reserve(input.size());

  for (char32_t x : input) {
    uint16_t status = idna_lookup(static_cast<uint32_t>(x));

    if (status == IDNA_DISALLOWED) {
      return error;
    }
    if (status == IDNA_VALID) {
      answer.push_back(x);
      continue;
    }
    // IDNA_IGNORED (status==0) falls through: idna_utf8_mappings[0] == 0x00
    // (null terminator), so the decode loop below produces nothing.

    // Mapped (or ignored): decode null-terminated UTF-8 from the mapping table.
    const uint8_t* ptr = idna_utf8_mappings + status;
    while (*ptr != 0) {
      answer.push_back(utf8_next(ptr));
    }
  }
  return answer;
}

}  // namespace ada::idna
