#!/usr/bin/env python3
"""Pack IDNA/Unicode tables into a raw-DEFLATE blob (src/table_blob.inc).

Runtime tables are stored compressed and expanded once by ensure_tables()
(see src/table_store.hpp). This script is the only writer of table_blob.inc.

Typical workflows
-----------------
Regenerate mapping tables from Unicode (IdnaMappingTable.txt) and repack:

  python3 scripts/idna_table.py --write

Regenerate identifier tables from DerivedCoreProperties.txt and repack:

  python3 scripts/derived_table.py --write

Repack using whatever is already in the blob (no-op rebuild / verify):

  python3 scripts/pack_tables.py

Update only some sections programmatically:

  from pack_tables import load_blob_sections, write_blob, write_mapping_constants
  sections = load_blob_sections()
  sections['idna_stage1'] = (...)  # list of ints
  ...
  write_blob(sections)
  write_mapping_constants(...)

Section layout (all present in the blob)
----------------------------------------
  Mapping:     idna_stage1, idna_stage2, idna_bool_blocks, idna_utf8_mappings
  Norm:        decomposition_*, ccc_range_*, composition_*
  Identifier:  id_continue_flat, id_start_flat
  Validity:    dir_start, dir_final, dir_value, combining_flat

Normalization and bidi/combining data have no generator in this repo; they are
preserved across regenerations by reading the existing table_blob.inc.
"""
from __future__ import annotations

import re
import struct
import sys
import zlib
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
BLOB_PATH = ROOT / "src" / "table_blob.inc"
MAPPING_CPP = ROOT / "src" / "mapping_tables.cpp"
ID_CPP = ROOT / "src" / "id_tables.cpp"

# Fixed section order for a stable on-disk layout.
SECTION_ORDER = [
    ("idna_stage1", "u16"),
    ("idna_stage2", "u16"),
    ("idna_bool_blocks", "u64"),
    ("idna_utf8_mappings", "u8"),
    ("decomposition_cp", "u32"),
    ("decomposition_offset", "u16"),
    ("decomposition_length", "u8"),
    ("decomposition_data16", "u16"),
    ("decomposition_high_index", "u16"),
    ("decomposition_high_cp", "u32"),
    ("ccc_range_start", "u32"),
    ("ccc_range_length", "u8"),
    ("ccc_range_value", "u8"),
    ("composition_sparse_page", "u16"),
    ("composition_sparse_block", "u8"),
    ("composition_block_flat", "u16"),
    ("composition_data16", "u16"),
    ("composition_high_index", "u16"),
    ("composition_high_cp", "u32"),
    ("id_continue_flat", "u32"),
    ("id_start_flat", "u32"),
    ("dir_start", "u32"),
    ("dir_final", "u32"),
    ("dir_value", "u8"),
    ("combining_flat", "u32"),
]

ALIGN = {"u8": 1, "u16": 2, "u32": 4, "u64": 8}
PACK = {"u8": "B", "u16": "H", "u32": "I", "u64": "Q"}
WIDTH = {"u8": 1, "u16": 2, "u32": 4, "u64": 8}


def _parse_blob_meta(text: str) -> dict[str, Any]:
    comp_m = re.search(r"compressed\[\] = \{(.*?)\};", text, re.S)
    if not comp_m:
        raise SystemExit(f"no compressed[] array in {BLOB_PATH}")
    compressed = bytes(
        int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", comp_m.group(1))
    )
    us = int(re.search(r"uncompressed_size = (\d+)", text).group(1))
    offs = {
        m.group(1): int(m.group(2))
        for m in re.finditer(r"constexpr size_t off_(\w+) = (\d+);", text)
    }
    counts = {
        m.group(1): int(m.group(2))
        for m in re.finditer(r"constexpr size_t count_(\w+) = (\d+);", text)
    }
    meta = {
        m.group(1): int(m.group(2))
        for m in re.finditer(
            r"constexpr size_t (decomposition_count|decomposition_high_count|"
            r"ccc_range_count|composition_sparse_count|composition_block_count|"
            r"composition_high_count|id_continue_count|id_start_count|"
            r"dir_table_count|combining_range_count) = (\d+);",
            text,
        )
    }
    plain = zlib.decompress(compressed, -15)
    if len(plain) != us:
        raise SystemExit(
            f"blob size mismatch: decompressed {len(plain)} != declared {us}"
        )
    return {
        "plain": plain,
        "offs": offs,
        "counts": counts,
        "meta": meta,
    }


def load_blob_sections(path: Path = BLOB_PATH) -> dict[str, list[int]]:
    """Load every table section from the current compressed blob."""
    text = path.read_text()
    info = _parse_blob_meta(text)
    plain = info["plain"]
    offs = info["offs"]
    counts = info["counts"]
    sections: dict[str, list[int]] = {}
    for name, kind in SECTION_ORDER:
        if name not in offs:
            raise SystemExit(f"blob missing section {name}")
        off = offs[name]
        count = counts[name]
        w = WIDTH[kind]
        fmt = "<" + PACK[kind] * count
        raw = plain[off : off + count * w]
        if len(raw) != count * w:
            raise SystemExit(f"truncated read of {name}")
        vals = list(struct.unpack(fmt, raw))
        sections[name] = vals
    # Stash meta for write_blob
    sections["_meta"] = info["meta"]  # type: ignore[assignment]
    return sections


def write_blob(sections: dict[str, Any], path: Path = BLOB_PATH) -> None:
    """Write sections (name -> list[int]) as src/table_blob.inc."""
    meta_in = sections.get("_meta") or {}
    blob = bytearray()
    layout: list[tuple[str, int, int, str, int]] = []

    def align(a: int) -> None:
        while len(blob) % a:
            blob.append(0)

    for name, kind in SECTION_ORDER:
        if name not in sections:
            raise SystemExit(f"write_blob missing section {name}")
        vals = sections[name]
        a = ALIGN[kind]
        align(a)
        off = len(blob)
        fmt = "<" + PACK[kind] * len(vals)
        blob.extend(struct.pack(fmt, *vals))
        layout.append((name, off, len(vals) * WIDTH[kind], kind, len(vals)))

    uncompressed = bytes(blob)
    co = zlib.compressobj(9, zlib.DEFLATED, -15)
    compressed = co.compress(uncompressed) + co.flush()
    assert zlib.decompress(compressed, -15) == uncompressed

    # Derive high-level counts used by table_store.hpp
    meta = {
        "decomposition_count": len(sections["decomposition_cp"]),
        "decomposition_high_count": len(sections["decomposition_high_index"]),
        "ccc_range_count": len(sections["ccc_range_start"]),
        "composition_sparse_count": len(sections["composition_sparse_page"]),
        "composition_block_count": meta_in.get(
            "composition_block_count",
            len(sections["composition_block_flat"]) // 257,
        ),
        "composition_high_count": len(sections["composition_high_index"]),
        # id_*_flat stores pairs, so range count = len/2
        "id_continue_count": len(sections["id_continue_flat"]) // 2,
        "id_start_count": len(sections["id_start_flat"]) // 2,
        "dir_table_count": len(sections["dir_start"]),
        "combining_range_count": len(sections["combining_flat"]) // 2,
    }

    def c_bytes(data: bytes, per: int = 16) -> str:
        lines = []
        for i in range(0, len(data), per):
            chunk = data[i : i + per]
            lines.append(",".join(f"0x{b:02x}" for b in chunk) + ",")
        return "\n".join(lines)

    out: list[str] = [
        "// Auto-generated by scripts/pack_tables.py - do not edit.",
        "// Compressed Unicode/IDNA tables (raw DEFLATE).",
        "#ifndef ADA_IDNA_TABLE_BLOB_H",
        "#define ADA_IDNA_TABLE_BLOB_H",
        "#include <cstdint>",
        "#include <cstddef>",
        "namespace ada::idna::table_blob {",
        f"constexpr size_t uncompressed_size = {len(uncompressed)};",
        f"constexpr size_t compressed_size = {len(compressed)};",
    ]
    for k, v in meta.items():
        out.append(f"constexpr size_t {k} = {v};")
    out.append("// Offsets into the decompressed buffer:")
    for name, off, _nbytes, _kind, count in layout:
        out.append(f"constexpr size_t off_{name} = {off};")
        out.append(f"constexpr size_t count_{name} = {count};")
    out.append("alignas(8) inline constexpr uint8_t compressed[] = {")
    out.append(c_bytes(compressed))
    out.append("};")
    out.append("}  // namespace ada::idna::table_blob")
    out.append("#endif")

    path.write_text("\n".join(out) + "\n")
    print(
        f"Wrote {path.relative_to(ROOT)}: "
        f"{len(uncompressed)} raw -> {len(compressed)} compressed "
        f"({100 * len(compressed) / len(uncompressed):.1f}%)"
    )


def write_mapping_constants(
    *,
    version: str,
    block_bits: int,
    block_size: int,
    block_mask: int,
    sentinel_valid: int,
    sentinel_disallowed: int,
    ignored_idx: int,
    bool_flag: int,
    low_range_end: int,
    high_ignored_start: int,
    high_ignored_end: int,
    stage1: list[int],
    mixed_data: list[int],
    bool_words: list[int],
    utf8_table: list[int],
    path: Path = MAPPING_CPP,
) -> None:
    """Write constants-only src/mapping_tables.cpp (arrays live in the blob)."""
    total = len(stage1) * 2 + len(mixed_data) * 2 + len(bool_words) * 8 + len(
        utf8_table
    )
    text = f"""// IDNA {version}
// Two-level compressed mapping table (constants only).
// Array payloads are stored in the DEFLATE blob (see table_store.hpp /
// scripts/pack_tables.py). Regenerate with: python3 scripts/idna_table.py --write
// Logical table size: {total} bytes ({total / 1024:.1f} KB)
//   stage1:      {len(stage1) * 2:6} bytes  ({len(stage1)} uint16_t entries)
//   stage2:      {len(mixed_data) * 2:6} bytes  ({len(mixed_data) // block_size} mixed blocks x {block_size})
//   bool_blocks: {len(bool_words) * 8:6} bytes  ({len(bool_words)} uint64_t words)
//   utf8 maps:   {len(utf8_table):6} bytes

// clang-format off
#ifndef ADA_IDNA_MAPPING_TABLE_H
#define ADA_IDNA_MAPPING_TABLE_H
#include <cstdint>

namespace ada::idna {{

// Block size for two-level table (2^{block_bits} = {block_size} code points per block).
constexpr uint32_t IDNA_BLOCK_BITS = {block_bits}u;
constexpr uint32_t IDNA_BLOCK_SIZE = {block_size}u;
constexpr uint32_t IDNA_BLOCK_MASK = {block_mask}u;

// Sentinel values stored in stage2 / returned by lookup.
constexpr uint16_t IDNA_VALID      = 0x{sentinel_valid:04X};  // code point is valid as-is
constexpr uint16_t IDNA_DISALLOWED = 0x{sentinel_disallowed:04X};  // code point is disallowed
constexpr uint16_t IDNA_IGNORED    = 0x{ignored_idx:04X};    // ignored (index into empty UTF-8 entry)

// Bit 15 of a stage1 entry: set = boolean block, clear = mixed block.
constexpr uint16_t IDNA_BOOL_FLAG  = 0x{bool_flag:04X};

// Two-level table covers code points [0, IDNA_LOW_RANGE_END).
// Derived from the highest non-disallowed code point below the high-ignored range,
// rounded up to the next {block_size}-code-point block boundary.
constexpr uint32_t IDNA_LOW_RANGE_END    = 0x{low_range_end:08X};

// Variation selectors supplement: U+{high_ignored_start:04X}..U+{high_ignored_end - 1:04X} are all ignored.
// These are handled with a simple range check; everything else above
// IDNA_LOW_RANGE_END is disallowed.
constexpr uint32_t IDNA_HIGH_IGNORED_START = 0x{high_ignored_start:08X};
constexpr uint32_t IDNA_HIGH_IGNORED_END   = 0x{high_ignored_end:08X};  // exclusive

// Large arrays (idna_stage1/stage2/bool_blocks/utf8_mappings) live in table_blob.

}}  // namespace ada::idna
#endif  // ADA_IDNA_MAPPING_TABLE_H
"""
    path.write_text(text)
    print(f"Wrote {path.relative_to(ROOT)} (constants only)")


def write_id_tables_stub(
    *,
    version: str,
    id_continue: list[list[int]],
    id_start: list[list[int]],
    path: Path = ID_CPP,
) -> None:
    """Write slim src/id_tables.cpp (arrays live in the blob)."""
    text = f"""// IDNA  {version}
// Identifier range tables are stored in the compressed blob (table_store.hpp).
// Regenerate with: python3 scripts/derived_table.py --write
//   id_continue: {len(id_continue)} ranges
//   id_start:    {len(id_start)} ranges

// clang-format off
#ifndef ADA_IDNA_IDENTIFIER_TABLES_H
#define ADA_IDNA_IDENTIFIER_TABLES_H
#include <cstdint>

namespace ada::idna {{

// id_continue / id_start pointers and counts are provided by table_store.hpp.

}}  // namespace ada::idna
#endif  // ADA_IDNA_IDENTIFIER_TABLES_H
"""
    path.write_text(text)
    print(f"Wrote {path.relative_to(ROOT)} (stub; data in blob)")


def update_mapping_in_blob(
    stage1: list[int],
    mixed_data: list[int],
    bool_words: list[int],
    utf8_table: list[int],
) -> None:
    sections = load_blob_sections()
    sections["idna_stage1"] = list(stage1)
    sections["idna_stage2"] = list(mixed_data)
    sections["idna_bool_blocks"] = list(bool_words)
    sections["idna_utf8_mappings"] = list(utf8_table)
    write_blob(sections)


def update_id_in_blob(
    id_continue: list[list[int]], id_start: list[list[int]]
) -> None:
    sections = load_blob_sections()
    cont_flat: list[int] = []
    for a, b in id_continue:
        cont_flat.extend([a, b])
    start_flat: list[int] = []
    for a, b in id_start:
        start_flat.extend([a, b])
    sections["id_continue_flat"] = cont_flat
    sections["id_start_flat"] = start_flat
    write_blob(sections)


def main(argv: list[str] | None = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    if argv in (["-h"], ["--help"]):
        print(__doc__)
        return 0
    # Default: load and rewrite the blob (verifies round-trip / refreshes layout).
    sections = load_blob_sections()
    write_blob(sections)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
