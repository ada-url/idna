import os.path
import requests
import re
import sys
if sys.version_info[0] < 3:
    print('You need to run this with Python 3')
    sys.exit(1)

# ─── Configuration ────────────────────────────────────────────────────────────
BLOCK_BITS   = 6          # block size = 2^BLOCK_BITS = 64 code points
BLOCK_SIZE   = 1 << BLOCK_BITS
BLOCK_MASK   = BLOCK_SIZE - 1
BOOL_FLAG    = 0x8000     # set in stage1 when entry points to a bool-block
# stage2 sentinel values (must not overlap with valid mapping-table indices)
SENTINEL_VALID      = 0xFFFF
SENTINEL_DISALLOWED = 0xFFFE
# stage2 value 0 is reserved for "ignored" (empty mapping, index 0 in utf8 table)
IGNORED_IDX  = 0

# ─── Download / cache IDNA mapping table ─────────────────────────────────────
url      = "https://www.unicode.org/Public/idna/latest/IdnaMappingTable.txt"
filename = "IdnaMappingTable.txt"

def get_table():
    if not os.path.exists(filename):
        tablefile = requests.get(url)
        with open(filename, 'wb') as f:
            f.write(tablefile.content)
    with open(filename, 'r') as f:
        return f.read()

def get_version(table_data):
    return re.search(r"# Version: (.*)", table_data).group(1)

# ─── UTF-8 helpers ────────────────────────────────────────────────────────────
def encode_utf8(cp):
    """Return UTF-8 encoding of a Unicode code point as bytes."""
    if cp <= 0x7F:
        return bytes([cp])
    elif cp <= 0x7FF:
        return bytes([0xC0 | (cp >> 6),
                      0x80 | (cp & 0x3F)])
    elif cp <= 0xFFFF:
        return bytes([0xE0 | (cp >> 12),
                      0x80 | ((cp >> 6) & 0x3F),
                      0x80 | (cp & 0x3F)])
    else:
        return bytes([0xF0 | (cp >> 18),
                      0x80 | ((cp >> 12) & 0x3F),
                      0x80 | ((cp >> 6) & 0x3F),
                      0x80 | (cp & 0x3F)])

# ─── Parse raw IDNA table ─────────────────────────────────────────────────────
def parse_idna(table_data):
    """
    Returns a list of (cp_start, cp_end_inclusive, code, mapped_seq) where
      code = 0 ignored | 1 valid | 2 disallowed | 3 mapped
      mapped_seq = tuple of uint32 code points (only when code==3)
    """
    mapping = {"ignored": 0, "valid": 1, "disallowed": 2, "mapped": 3}
    entries = []
    for line in table_data.split('\n'):
        if line.startswith('#') or ';' not in line:
            continue
        line = line[:line.index('#')]
        parts = [p.strip() for p in line.split(';') if p.strip()]
        if not parts:
            continue
        status = parts[1]
        # IDNA2003 compatibility: treat STD3 as non-STD3, deviation as valid
        if status.startswith('disallowed_STD3_'):
            status = status[16:]
        if status == 'deviation':
            status = 'valid'
        code = mapping.get(status)
        if code is None:
            continue
        cp_range = [int(x, 16) for x in parts[0].split('..')]
        cp_start = cp_range[0]
        cp_end   = cp_range[-1]
        mapped_seq = ()
        if code == 3:
            mapped_seq = tuple(int(x, 16) for x in parts[2].split())
        entries.append((cp_start, cp_end, code, mapped_seq))
    return entries

# ─── Determine table layout from actual data ──────────────────────────────────
def find_high_ignored_range(entries):
    """
    Find the ignored code point range above U+E0000.
    Returns (start, end_exclusive) or (None, None) if not found.
    """
    high_ignored = [(s, e) for s, e, c, m in entries if c == 0 and s >= 0xE0000]
    if not high_ignored:
        return None, None
    start = min(s for s, e in high_ignored)
    end   = max(e for s, e in high_ignored) + 1  # exclusive
    return start, end

def compute_low_range_end(entries, high_ignored_start):
    """
    Find the first block boundary that covers all non-disallowed code points
    below the high-ignored range.  This dynamically captures the full active
    range (including CJK Extension I/J and similar future additions) without
    requiring any hardcoded boundaries.
    """
    max_active_cp = 0
    for cp_start, cp_end, code, seq in entries:
        # Exclude code points in/above the high ignored range and pure
        # disallowed entries.
        if cp_start >= high_ignored_start:
            continue
        if code != 2:  # non-disallowed (valid, ignored, mapped)
            max_active_cp = max(max_active_cp, cp_end)

    # Round up to the next block boundary so that max_active_cp is
    # fully contained within the table.
    return ((max_active_cp >> BLOCK_BITS) + 1) << BLOCK_BITS

# ─── Build flat per-code-point array + UTF-8 mapping table ───────────────────
def build_flat_and_mappings(entries, low_range_end):
    """
    Returns (flat, utf8_table, seq_to_idx) where
      flat[cp] = SENTINEL_VALID | SENTINEL_DISALLOWED | utf8_byte_offset
      utf8_table = bytearray of null-terminated UTF-8 mapping strings
      seq_to_idx = dict mapping tuple of codepoints → byte offset in utf8_table
    """
    flat = [SENTINEL_DISALLOWED] * low_range_end

    # Build UTF-8 mapping table.
    # Index 0 is always the empty string (for "ignored" code points).
    utf8_table  = bytearray([0])   # byte 0: null terminator (empty mapping)
    seq_to_idx  = {(): IGNORED_IDX}

    # Pre-populate all unique mapping sequences
    for cp_start, cp_end, code, seq in entries:
        if code == 3 and seq not in seq_to_idx:
            offset = len(utf8_table)
            seq_to_idx[seq] = offset
            for cp in seq:
                utf8_table.extend(encode_utf8(cp))
            utf8_table.append(0)  # null terminator

    # Fill flat array
    for cp_start, cp_end, code, seq in entries:
        lo = max(cp_start, 0)
        hi = min(cp_end + 1, low_range_end)
        if lo >= hi:
            continue
        if code == 0:   # ignored → empty mapping
            val = IGNORED_IDX
        elif code == 1: # valid
            val = SENTINEL_VALID
        elif code == 2: # disallowed
            val = SENTINEL_DISALLOWED
        else:           # mapped
            val = seq_to_idx[seq]
        for cp in range(lo, hi):
            flat[cp] = val

    return flat, utf8_table, seq_to_idx

# ─── Build two-level compressed table ────────────────────────────────────────
def build_two_level(flat):
    """
    Returns (stage1, mixed_data, bool_words) where
      stage1[i] = index for block i
        bit 15 set  → bool block: lower 15 bits = bool_block_idx
        bit 15 clear → mixed block: value = base offset into mixed_data[]
      mixed_data = flat uint16_t array (all mixed blocks concatenated)
      bool_words = list of uint64_t bitwords; bit k=1 ↔ code point is VALID
    """
    block_size  = BLOCK_SIZE
    n_blocks    = (len(flat) + block_size - 1) // block_size
    stage1      = []
    mixed_data  = []
    bool_words  = []
    mixed_map   = {}   # tuple(block) → base offset in mixed_data
    bool_map    = {}   # int (bitword) → index in bool_words

    for bi in range(n_blocks):
        start = bi * block_size
        end   = start + block_size
        block = flat[start:end]
        if len(block) < block_size:
            block = block + [SENTINEL_DISALLOWED] * (block_size - len(block))

        # Check if block only contains VALID / DISALLOWED (boolean block)
        is_bool = all(v in (SENTINEL_VALID, SENTINEL_DISALLOWED) for v in block)

        if is_bool:
            bits = 0
            for i, v in enumerate(block):
                if v == SENTINEL_VALID:
                    bits |= (1 << i)
            if bits not in bool_map:
                bool_map[bits] = len(bool_words)
                bool_words.append(bits)
            stage1.append(BOOL_FLAG | bool_map[bits])
        else:
            key = tuple(block)
            if key not in mixed_map:
                base = len(mixed_data)
                mixed_map[key] = base
                mixed_data.extend(block)
            stage1.append(mixed_map[key])

    return stage1, mixed_data, bool_words

# ─── C++ output helpers ───────────────────────────────────────────────────────
def hex2(v):  return f'0x{v:02X}'
def hex4(v):  return f'0x{v:04X}'
def hex8(v):  return f'0x{v:08X}'
def hex16(v): return f'0x{v:016X}ULL'

def emit_array_uint8(name, data, cols=16):
    lines  = [f'const uint8_t {name}[{len(data)}] = {{']
    row    = []
    for i, v in enumerate(data):
        row.append(hex2(v))
        if len(row) == cols:
            lines.append('\t' + ', '.join(row) + ',')
            row = []
    if row:
        lines.append('\t' + ', '.join(row))
    lines.append('};')
    return '\n'.join(lines)

def emit_array_uint16(name, data, cols=12):
    lines = [f'const uint16_t {name}[{len(data)}] = {{']
    row   = []
    for i, v in enumerate(data):
        row.append(hex4(v))
        if len(row) == cols:
            lines.append('\t' + ', '.join(row) + ',')
            row = []
    if row:
        lines.append('\t' + ', '.join(row))
    lines.append('};')
    return '\n'.join(lines)

def emit_array_uint64(name, data, cols=4):
    lines = [f'const uint64_t {name}[{len(data)}] = {{']
    row   = []
    for i, v in enumerate(data):
        row.append(hex16(v))
        if len(row) == cols:
            lines.append('\t' + ', '.join(row) + ',')
            row = []
    if row:
        lines.append('\t' + ', '.join(row))
    lines.append('};')
    return '\n'.join(lines)

# ─── Main ─────────────────────────────────────────────────────────────────────
def print_idna():
    table_data = get_table()
    version    = get_version(table_data)
    entries    = parse_idna(table_data)

    # Derive layout constants from the actual table data.
    high_ignored_start, high_ignored_end = find_high_ignored_range(entries)
    if high_ignored_start is None:
        # No isolated high-ignored range; nothing special needed above LOW_RANGE_END.
        high_ignored_start = 0x110000
        high_ignored_end   = 0x110000

    low_range_end = compute_low_range_end(entries, high_ignored_start)

    flat, utf8_table, seq_to_idx = build_flat_and_mappings(entries, low_range_end)
    stage1, mixed_data, bool_words = build_two_level(flat)

    n_stage1    = len(stage1)
    n_mixed     = len(mixed_data)
    n_bool      = len(bool_words)
    n_utf8      = len(utf8_table)

    total_bytes = (n_stage1 * 2) + (n_mixed * 2) + (n_bool * 8) + n_utf8

    # Validate utf8 table entries will fit in uint16_t sentinels
    assert len(utf8_table) < 0xFFFD, \
        f"UTF-8 table too large: {len(utf8_table)} bytes, max 0xFFFD"
    assert n_stage1 <= 0x8000, \
        f"stage1 too large: {n_stage1} entries"

    print(f"// IDNA {version}")
    print(f"// Two-level compressed mapping table.")
    print(f"// All constants are derived from the table data; no hardcoded boundaries.")
    print(f"// Total binary size: {total_bytes} bytes ({total_bytes/1024:.1f} KB)")
    print(f"//   stage1:      {n_stage1*2:6} bytes  ({n_stage1} uint16_t entries)")
    print(f"//   stage2:      {n_mixed*2:6} bytes  ({len(mixed_data)//BLOCK_SIZE} mixed blocks x {BLOCK_SIZE})")
    print(f"//   bool_blocks: {n_bool*8:6} bytes  ({n_bool} uint64_t words)")
    print(f"//   utf8 maps:   {n_utf8:6} bytes")
    print()
    print("// clang-format off")
    print("#ifndef ADA_IDNA_MAPPING_TABLE_H")
    print("#define ADA_IDNA_MAPPING_TABLE_H")
    print("#include <cstdint>")
    print()
    print("namespace ada::idna {")
    print()

    # ── Constants (all derived from the actual table, not hardcoded) ───────────
    print(f"// Block size for two-level table (2^{BLOCK_BITS} = {BLOCK_SIZE} code points per block).")
    print(f"constexpr uint32_t IDNA_BLOCK_BITS = {BLOCK_BITS}u;")
    print(f"constexpr uint32_t IDNA_BLOCK_SIZE = {BLOCK_SIZE}u;")
    print(f"constexpr uint32_t IDNA_BLOCK_MASK = {BLOCK_MASK}u;")
    print()
    print(f"// Sentinel values stored in stage2 / returned by lookup.")
    print(f"constexpr uint16_t IDNA_VALID      = {hex4(SENTINEL_VALID)};  // code point is valid as-is")
    print(f"constexpr uint16_t IDNA_DISALLOWED = {hex4(SENTINEL_DISALLOWED)};  // code point is disallowed")
    print(f"constexpr uint16_t IDNA_IGNORED    = {hex4(IGNORED_IDX)};    // ignored (index into empty UTF-8 entry)")
    print()
    print(f"// Bit 15 of a stage1 entry: set = boolean block, clear = mixed block.")
    print(f"constexpr uint16_t IDNA_BOOL_FLAG  = {hex4(BOOL_FLAG)};")
    print()
    print(f"// Two-level table covers code points [0, IDNA_LOW_RANGE_END).")
    print(f"// Derived from the highest non-disallowed code point below the high-ignored range,")
    print(f"// rounded up to the next {BLOCK_SIZE}-code-point block boundary.")
    print(f"constexpr uint32_t IDNA_LOW_RANGE_END    = {hex8(low_range_end)};")
    print()
    print(f"// Variation selectors supplement: U+{high_ignored_start:04X}..U+{high_ignored_end-1:04X} are all ignored.")
    print(f"// These are handled with a simple range check; everything else above")
    print(f"// IDNA_LOW_RANGE_END is disallowed.")
    print(f"constexpr uint32_t IDNA_HIGH_IGNORED_START = {hex8(high_ignored_start)};")
    print(f"constexpr uint32_t IDNA_HIGH_IGNORED_END   = {hex8(high_ignored_end)};  // exclusive")
    print()

    # ── stage1 ─────────────────────────────────────────────────────────────────
    print(f"// idna_stage1[cp >> {BLOCK_BITS}]: one entry per {BLOCK_SIZE}-code-point block.")
    print(f"// Bit 15 set  → lower 15 bits = index into idna_bool_blocks[].")
    print(f"// Bit 15 clear → value = base offset into idna_stage2[] for this block.")
    print(emit_array_uint16(f"idna_stage1", stage1))
    print()

    # ── stage2 (mixed blocks) ──────────────────────────────────────────────────
    print(f"// idna_stage2[]: mixed blocks. Each entry is IDNA_VALID, IDNA_DISALLOWED,")
    print(f"// IDNA_IGNORED, or a byte offset into idna_utf8_mappings[].")
    print(emit_array_uint16(f"idna_stage2", mixed_data))
    print()

    # ── boolean blocks ──────────────────────────────────────────────────────────
    print(f"// idna_bool_blocks[]: one uint64_t per boolean block.")
    print(f"// Bit k (0 = LSB) = 1 → (block_start + k) is VALID; 0 → DISALLOWED.")
    print(emit_array_uint64(f"idna_bool_blocks", bool_words))
    print()

    # ── UTF-8 mapping table ────────────────────────────────────────────────────
    print(f"// idna_utf8_mappings[]: null-terminated UTF-8 mapping strings.")
    print(f"// Byte offset 0 is the empty string (for ignored code points).")
    print(emit_array_uint8(f"idna_utf8_mappings", utf8_table))
    print()

    print("} // namespace ada::idna")
    print("#endif // ADA_IDNA_MAPPING_TABLE_H")

if __name__ == "__main__":
    print_idna()
