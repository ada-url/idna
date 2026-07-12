// IDNA 17.0.0
// Two-level compressed mapping table.
// All constants are derived from the table data; no hardcoded boundaries.
// Total binary size: 48659 bytes (47.5 KB)
//   stage1:        6564 bytes  (3282 uint16_t entries)
//   stage2:       22912 bytes  (179 mixed blocks x 64)
//   bool_blocks:   1800 bytes  (225 uint64_t words)
//   utf8 maps:    17383 bytes

// clang-format off
#ifndef ADA_IDNA_MAPPING_TABLE_H
#define ADA_IDNA_MAPPING_TABLE_H
#include <cstdint>

namespace ada::idna {

// Block size for two-level table (2^6 = 64 code points per block).
constexpr uint32_t IDNA_BLOCK_BITS = 6u;
constexpr uint32_t IDNA_BLOCK_SIZE = 64u;
constexpr uint32_t IDNA_BLOCK_MASK = 63u;

// Sentinel values stored in stage2 / returned by lookup.
constexpr uint16_t IDNA_VALID      = 0xFFFF;  // code point is valid as-is
constexpr uint16_t IDNA_DISALLOWED = 0xFFFE;  // code point is disallowed
constexpr uint16_t IDNA_IGNORED    = 0x0000;    // ignored (index into empty UTF-8 entry)

// Bit 15 of a stage1 entry: set = boolean block, clear = mixed block.
constexpr uint16_t IDNA_BOOL_FLAG  = 0x8000;

// Two-level table covers code points [0, IDNA_LOW_RANGE_END).
// Derived from the highest non-disallowed code point below the high-ignored range,
// rounded up to the next 64-code-point block boundary.
constexpr uint32_t IDNA_LOW_RANGE_END    = 0x00033480;

// Variation selectors supplement: U+E0100..U+E01EF are all ignored.
// These are handled with a simple range check; everything else above
// IDNA_LOW_RANGE_END is disallowed.
constexpr uint32_t IDNA_HIGH_IGNORED_START = 0x000E0100;
constexpr uint32_t IDNA_HIGH_IGNORED_END   = 0x000E01F0;  // exclusive

// idna_stage1[cp >> 6]: one entry per 64-code-point block.
// Bit 15 set  -> lower 15 bits = index into idna_bool_blocks[].
// Bit 15 clear -> value = base offset into idna_stage2[] for this block.

}  // namespace ada::idna
#endif  // ADA_IDNA_MAPPING_TABLE_H
