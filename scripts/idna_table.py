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

# Unicode code-point ranges that need table lookup
# Low range: 0x00000 – LOW_RANGE_END (exclusive)
LOW_RANGE_END = 0x30000    # 196 608 code points

# Mid range handled with simple branches in C++ code (no table):
#   0x30000 – 0x3134A : valid
#   0x3134B – 0x3134F : disallowed
#   0x31350 – 0x33479 : valid
#   0x3347A – 0xE00FF : disallowed

# Ignored range: 0xE0100 – 0xE01EF (variation selectors supplement)
# Everything else: disallowed

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

# ─── Build flat per-code-point array + UTF-8 mapping table ───────────────────
def build_flat_and_mappings(entries):
    """
    Returns (flat, utf8_table, seq_to_idx) where
      flat[cp] = SENTINEL_VALID | SENTINEL_DISALLOWED | utf8_byte_offset
      utf8_table = bytearray of null-terminated UTF-8 mapping strings
      seq_to_idx = dict mapping tuple of codepoints → byte offset in utf8_table
    """
    flat = [SENTINEL_DISALLOWED] * LOW_RANGE_END

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
        hi = min(cp_end + 1, LOW_RANGE_END)
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
    n_blocks   = (LOW_RANGE_END + BLOCK_SIZE - 1) // BLOCK_SIZE
    stage1     = []
    mixed_data = []
    bool_words = []
    mixed_map  = {}   # tuple(block) → base offset in mixed_data
    bool_map   = {}   # int (bitword) → index in bool_words

    for bi in range(n_blocks):
        start = bi * BLOCK_SIZE
        end   = start + BLOCK_SIZE
        block = flat[start:end]
        if len(block) < BLOCK_SIZE:
            block = block + [SENTINEL_DISALLOWED] * (BLOCK_SIZE - len(block))

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
def hex16(v): return f'0x{v:016X}'

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

    flat, utf8_table, seq_to_idx = build_flat_and_mappings(entries)
    stage1, mixed_data, bool_words = build_two_level(flat)

    n_stage1    = len(stage1)
    n_mixed     = len(mixed_data)
    n_bool      = len(bool_words)
    n_utf8      = len(utf8_table)

    total_bytes = (n_stage1 * 2) + (n_mixed * 2) + (n_bool * 8) + n_utf8

    # Validate utf8 table entries will fit in uint16_t
    max_offset = max(v for v in flat if v not in (SENTINEL_VALID, SENTINEL_DISALLOWED))
    assert max_offset < 0xFFFD, f"UTF-8 table too large: max offset {max_offset:#x}"
    assert len(utf8_table) < 0xFFFD, f"UTF-8 table won't fit in uint16_t: {len(utf8_table)}"

    print(f"// IDNA {version}")
    print(f"// Two-level compressed mapping table.")
    print(f"// Total binary size: {total_bytes} bytes ({total_bytes/1024:.1f} KB)")
    print(f"//   stage1:     {n_stage1*2:6} bytes")
    print(f"//   stage2:     {n_mixed*2:6} bytes")
    print(f"//   bool_blocks:{n_bool*8:6} bytes")
    print(f"//   utf8 maps:  {n_utf8:6} bytes")
    print()
    print("// clang-format off")
    print("#ifndef ADA_IDNA_MAPPING_TABLE_H")
    print("#define ADA_IDNA_MAPPING_TABLE_H")
    print("#include <cstdint>")
    print()
    print("namespace ada::idna {")
    print()

    # ── Constants ──────────────────────────────────────────────────────────────
    print(f"// Block size for two-level table (2^{BLOCK_BITS} = {BLOCK_SIZE} code points per block).")
    print(f"constexpr uint32_t IDNA_BLOCK_BITS = {BLOCK_BITS};")
    print(f"constexpr uint32_t IDNA_BLOCK_SIZE = {BLOCK_SIZE};")
    print(f"constexpr uint32_t IDNA_BLOCK_MASK = {BLOCK_MASK};")
    print()
    print(f"// Sentinel values stored in stage2 / returned by lookup.")
    print(f"constexpr uint16_t IDNA_VALID      = {hex4(SENTINEL_VALID)};  // code point is valid as-is")
    print(f"constexpr uint16_t IDNA_DISALLOWED = {hex4(SENTINEL_DISALLOWED)};  // code point is disallowed")
    print(f"constexpr uint16_t IDNA_IGNORED    = {hex4(IGNORED_IDX)};      // code point is ignored (maps to empty)")
    print()
    print(f"// Bit 15 of stage1 entry: set = boolean block, clear = mixed block.")
    print(f"constexpr uint16_t IDNA_BOOL_FLAG  = {hex4(BOOL_FLAG)};")
    print()
    print(f"// Upper code-point boundary of stage1/stage2 tables.")
    print(f"constexpr uint32_t IDNA_LOW_RANGE_END = {hex8(LOW_RANGE_END)};")
    print()
    print(f"// Mid range (0x30000–0x3347A): handled with branch logic.")
    print(f"// High ignored range (0xE0100–0xE01EF): variation selectors supplement.")
    print(f"constexpr uint32_t IDNA_MID_VALID1_END   = {hex8(0x3134B)};  // exclusive")
    print(f"constexpr uint32_t IDNA_MID_DISALLOW_END = {hex8(0x31350)};  // exclusive")
    print(f"constexpr uint32_t IDNA_MID_VALID2_END   = {hex8(0x3347A)};  // exclusive")
    print(f"constexpr uint32_t IDNA_HIGH_IGNORED_START = {hex8(0xE0100)};")
    print(f"constexpr uint32_t IDNA_HIGH_IGNORED_END   = {hex8(0xE01F0)};  // exclusive")
    print()

    # ── stage1 ─────────────────────────────────────────────────────────────────
    print(f"// stage1[cp >> {BLOCK_BITS}]: index for each {BLOCK_SIZE}-code-point block.")
    print(f"// If bit 15 is set, the lower 15 bits index into idna_bool_blocks[].")
    print(f"// Otherwise the value is the base offset into idna_stage2[].")
    print(emit_array_uint16(f"idna_stage1", stage1))
    print()

    # ── stage2 (mixed blocks) ──────────────────────────────────────────────────
    print(f"// idna_stage2[]: mixed blocks. Each entry is either IDNA_VALID,")
    print(f"// IDNA_DISALLOWED, IDNA_IGNORED, or a byte offset into idna_utf8_mappings[].")
    print(emit_array_uint16(f"idna_stage2", mixed_data))
    print()

    # ── boolean blocks ──────────────────────────────────────────────────────────
    print(f"// idna_bool_blocks[]: one uint64_t per boolean block.")
    print(f"// Bit k (0-indexed from LSB) = 1 means code point (block_start + k) is VALID.")
    print(f"// Bit k = 0 means DISALLOWED.")
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
