#!/usr/bin/env python3
"""Pack IDNA/Unicode tables into a filtered raw-DEFLATE blob (src/table_blob.inc).

Runtime tables are stored compressed and expanded once by ensure_tables()
(see src/table_store.hpp). This script is the only writer of table_blob.inc.

On-disk layout (filter_version >= 1)
------------------------------------
Before DEFLATE, multi-byte sections are prefiltered so zlib/zopfli see more
redundant byte streams (same idea as PNG filters):

  * u16 / u32 sections: delta-encode LE values, then split into byte planes
  * u64 sections: byte-plane split only
  * u8 sections: unchanged

Runtime inflates into a same-sized buffer, reverses the filter in place, checks
CRC-32 of the logical LE payload, then applies host endian conversion.

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
BLOB_DATA_PATH = ROOT / "src" / "table_blob_data.inc"
MAPPING_CPP = ROOT / "src" / "mapping_tables.cpp"
ID_CPP = ROOT / "src" / "id_tables.cpp"

# Fixed section order for a stable on-disk layout.
SECTION_ORDER = [
    ("idna_stage1", "u16"),
    ("idna_stage2", "u16"),
    ("idna_bool_blocks", "u64"),
    ("idna_utf8_mappings", "u8"),
    ("decomposition_index", "u8"),
    ("decomposition_block", "u16"),
    ("decomposition_data", "u32"),
    ("ccc_index", "u8"),
    ("ccc_block", "u8"),
    ("composition_index", "u8"),
    ("composition_block", "u16"),
    ("composition_data", "u32"),
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

# On-disk prefilter version.
#   v0 = raw multi-stage (legacy)
#   v1 = per-section delta (u16/u32) + byte-plane split before raw DEFLATE
#   v2 = dense run/sparse encoding; inflate then expand_dense() to multi-stage
#
# Default is v1 of the *IDNA-closure* multi-stage tables (built via dense_pack
# expand at pack time). That is smaller on disk than full-Unicode multi-stage
# and avoids a large runtime expander (v2).
FILTER_VERSION = 1
# When True with FILTER_VERSION==1, rewrite sections through dense build/expand
# so NFC tables only cover the IDNA alphabet closure (much smaller blob).
USE_IDNA_CLOSURE_TABLES = True


def _delta_encode(data: bytes, width: int) -> bytes:
    """In-place-style delta of little-endian integers (first sample absolute)."""
    out = bytearray(len(data))
    prev = 0
    if width == 2:
        for i in range(0, len(data), 2):
            v = struct.unpack_from("<H", data, i)[0]
            struct.pack_into("<H", out, i, (v - prev) & 0xFFFF)
            prev = v
    elif width == 4:
        for i in range(0, len(data), 4):
            v = struct.unpack_from("<I", data, i)[0]
            struct.pack_into("<I", out, i, (v - prev) & 0xFFFFFFFF)
            prev = v
    else:
        raise ValueError(f"delta width {width}")
    return bytes(out)


def _delta_decode(data: bytes, width: int) -> bytes:
    out = bytearray(len(data))
    prev = 0
    if width == 2:
        for i in range(0, len(data), 2):
            d = struct.unpack_from("<H", data, i)[0]
            prev = (prev + d) & 0xFFFF
            struct.pack_into("<H", out, i, prev)
    elif width == 4:
        for i in range(0, len(data), 4):
            d = struct.unpack_from("<I", data, i)[0]
            prev = (prev + d) & 0xFFFFFFFF
            struct.pack_into("<I", out, i, prev)
    else:
        raise ValueError(f"delta width {width}")
    return bytes(out)


def _byte_split(data: bytes, width: int) -> bytes:
    """Transpose LE multi-byte values into width successive byte planes."""
    if width == 1:
        return data
    n = len(data) // width
    planes = [bytearray(n) for _ in range(width)]
    for i in range(n):
        base = i * width
        for b in range(width):
            planes[b][i] = data[base + b]
    return b"".join(bytes(p) for p in planes)


def _byte_unsplit(data: bytes, width: int) -> bytes:
    if width == 1:
        return data
    n = len(data) // width
    out = bytearray(len(data))
    for i in range(n):
        for b in range(width):
            out[i * width + b] = data[b * n + i]
    return bytes(out)


def _filter_section(data: bytes, kind: str) -> bytes:
    w = WIDTH[kind]
    if kind in ("u16", "u32"):
        return _byte_split(_delta_encode(data, w), w)
    if kind == "u64":
        return _byte_split(data, w)
    return data


def _unfilter_section(data: bytes, kind: str) -> bytes:
    w = WIDTH[kind]
    if kind in ("u16", "u32"):
        return _delta_decode(_byte_unsplit(data, w), w)
    if kind == "u64":
        return _byte_unsplit(data, w)
    return data


def _filter_blob(
    plain: bytes, layout: list[tuple[str, int, int, str, int]]
) -> bytes:
    """Apply per-section prefilters; padding between sections is unchanged."""
    out = bytearray(plain)
    for _name, off, nbytes, kind, _count in layout:
        chunk = bytes(out[off : off + nbytes])
        out[off : off + nbytes] = _filter_section(chunk, kind)
    return bytes(out)


def _unfilter_blob(
    filtered: bytes, layout: list[tuple[str, int, int, str, int]]
) -> bytes:
    out = bytearray(filtered)
    for _name, off, nbytes, kind, _count in layout:
        chunk = bytes(out[off : off + nbytes])
        out[off : off + nbytes] = _unfilter_section(chunk, kind)
    return bytes(out)


def _layout_from_meta(
    offs: dict[str, int], counts: dict[str, int]
) -> list[tuple[str, int, int, str, int]]:
    layout: list[tuple[str, int, int, str, int]] = []
    for name, kind in SECTION_ORDER:
        if name not in offs:
            raise SystemExit(f"blob missing section {name}")
        count = counts[name]
        layout.append((name, offs[name], count * WIDTH[kind], kind, count))
    return layout


def _best_raw_deflate(data: bytes) -> bytes:
    """Pick the smallest raw-DEFLATE stream among zlib strategies."""
    best: bytes | None = None
    for strategy in (
        zlib.Z_DEFAULT_STRATEGY,
        zlib.Z_FILTERED,
        zlib.Z_HUFFMAN_ONLY,
        zlib.Z_RLE,
    ):
        co = zlib.compressobj(9, zlib.DEFLATED, -15, 9, strategy)
        out = co.compress(data) + co.flush()
        if best is None or len(out) < len(best):
            best = out
    assert best is not None
    # Optional zopfli if installed (pack-time only; runtime is raw DEFLATE).
    try:
        import zopfli.zlib as zopfli_zlib  # type: ignore

        zc = zopfli_zlib.compress(data, numiterations=15)
        raw = zc[2:-4]  # strip zlib header + adler32
        if zlib.decompress(raw, -15) == data and len(raw) < len(best):
            best = raw
    except Exception:
        pass
    assert zlib.decompress(best, -15) == data
    return best


def _low_range_end() -> int:
    mt = MAPPING_CPP.read_text()
    m = re.search(r"IDNA_LOW_RANGE_END\s*=\s*0x([0-9A-Fa-f]+)", mt)
    if not m:
        raise SystemExit(f"IDNA_LOW_RANGE_END not found in {MAPPING_CPP}")
    return int(m.group(1), 16)


def _pack_sections_plain(
    sections: dict[str, Any],
) -> tuple[bytes, list[tuple[str, int, int, str, int]], dict[str, int]]:
    """Pack multi-stage sections into the aligned working buffer."""
    meta_in = sections.get("_meta") or {}
    blob = bytearray()
    layout: list[tuple[str, int, int, str, int]] = []

    def align(a: int) -> None:
        while len(blob) % a:
            blob.append(0)

    for name, kind in SECTION_ORDER:
        if name not in sections:
            raise SystemExit(f"pack missing section {name}")
        vals = sections[name]
        a = ALIGN[kind]
        align(a)
        off = len(blob)
        fmt = "<" + PACK[kind] * len(vals)
        blob.extend(struct.pack(fmt, *vals))
        layout.append((name, off, len(vals) * WIDTH[kind], kind, len(vals)))

    meta = {
        "decomposition_block_rows": meta_in.get(
            "decomposition_block_rows",
            len(sections["decomposition_block"]) // 257,
        ),
        "decomposition_block_cols": 257,
        "ccc_block_rows": meta_in.get(
            "ccc_block_rows", len(sections["ccc_block"]) // 256
        ),
        "ccc_block_cols": 256,
        "composition_block_rows": meta_in.get(
            "composition_block_rows",
            len(sections["composition_block"]) // 257,
        ),
        "composition_block_cols": 257,
        "id_continue_count": len(sections["id_continue_flat"]) // 2,
        "id_start_count": len(sections["id_start_flat"]) // 2,
        "dir_table_count": len(sections["dir_start"]),
        "combining_range_count": len(sections["combining_flat"]) // 2,
    }
    return bytes(blob), layout, meta


def _parse_blob_meta(text: str, data_text: str | None = None) -> dict[str, Any]:
    # Payload may live in table_blob.inc (legacy/combined) or table_blob_data.inc.
    search_text = text
    comp_m = re.search(
        r"compressed(?:\[[^\]]*\])?\s*=\s*\{(.*?)\};", search_text, re.S
    )
    if not comp_m and data_text is not None:
        search_text = data_text
        comp_m = re.search(
            r"compressed(?:\[[^\]]*\])?\s*=\s*\{(.*?)\};", search_text, re.S
        )
    if not comp_m and BLOB_DATA_PATH.exists():
        data_text = BLOB_DATA_PATH.read_text()
        search_text = data_text
        comp_m = re.search(
            r"compressed(?:\[[^\]]*\])?\s*=\s*\{(.*?)\};", search_text, re.S
        )
    if not comp_m:
        raise SystemExit(
            f"no compressed[] array in {BLOB_PATH} or {BLOB_DATA_PATH}"
        )
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
            r"constexpr size_t (decomposition_block_rows|decomposition_block_cols|"
            r"ccc_block_rows|ccc_block_cols|composition_block_rows|"
            r"composition_block_cols|id_continue_count|id_start_count|"
            r"dir_table_count|combining_range_count|dense_uncompressed_size|"
            r"low_range_end) = (\d+);",
            text,
        )
    }
    # filter_version may be absent in legacy blobs (treated as 0 = no filter).
    # Accept optional C++ unsigned suffix (1u).
    fv_m = re.search(r"filter_version = (\d+)u?;", text)
    filter_version = int(fv_m.group(1)) if fv_m else 0

    payload = zlib.decompress(compressed, -15)
    layout = _layout_from_meta(offs, counts)
    if filter_version >= 2:
        from dense_pack import expand_dense

        dense_us = meta.get("dense_uncompressed_size", len(payload))
        if len(payload) != dense_us:
            raise SystemExit(
                f"dense size mismatch: decompressed {len(payload)} != declared {dense_us}"
            )
        low = meta.get("low_range_end", _low_range_end())
        expanded = expand_dense(payload, low)
        plain, layout, exp_meta = _pack_sections_plain(expanded)
        # Prefer expanded meta counts (should match header).
        meta = {**meta, **exp_meta}
        if len(plain) != us:
            raise SystemExit(
                f"expanded working size {len(plain)} != declared uncompressed_size {us}"
            )
    elif filter_version >= 1:
        if len(payload) != us:
            raise SystemExit(
                f"blob size mismatch: decompressed {len(payload)} != declared {us}"
            )
        plain = _unfilter_blob(payload, layout)
    else:
        if len(payload) != us:
            raise SystemExit(
                f"blob size mismatch: decompressed {len(payload)} != declared {us}"
            )
        plain = payload
    return {
        "plain": plain,
        "offs": {name: off for name, off, _n, _k, _c in layout}
        if filter_version >= 2
        else offs,
        "counts": {name: count for name, _o, _n, _k, count in layout}
        if filter_version >= 2
        else counts,
        "meta": meta,
        "filter_version": filter_version,
        "layout": layout,
    }


def load_blob_sections(path: Path = BLOB_PATH) -> dict[str, list[int]]:
    """Load every table section from the current compressed blob."""
    text = path.read_text()
    data_text = BLOB_DATA_PATH.read_text() if BLOB_DATA_PATH.exists() else None
    info = _parse_blob_meta(text, data_text)
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
    dense_uncompressed_size = 0
    low_range_end = _low_range_end()
    write_sections = sections
    dense_payload: bytes | None = None

    if FILTER_VERSION >= 2 or USE_IDNA_CLOSURE_TABLES:
        from dense_pack import build_dense, expand_dense

        # Rebuild multi-stage from dense (IDNA alphabet NFC-closure).
        dense_payload, dmeta = build_dense(sections, MAPPING_CPP)
        low_range_end = dmeta["low_range_end"]
        dense_uncompressed_size = len(dense_payload)
        write_sections = expand_dense(dense_payload, low_range_end)

    if FILTER_VERSION >= 2:
        assert dense_payload is not None
        plain, layout, meta = _pack_sections_plain(write_sections)
        uncompressed_crc32 = zlib.crc32(plain) & 0xFFFFFFFF
        compressed = _best_raw_deflate(dense_payload)
        assert zlib.decompress(compressed, -15) == dense_payload
        meta["dense_uncompressed_size"] = dense_uncompressed_size
        meta["low_range_end"] = low_range_end
    else:
        plain, layout, meta = _pack_sections_plain(write_sections)
        uncompressed_crc32 = zlib.crc32(plain) & 0xFFFFFFFF
        filtered = _filter_blob(plain, layout)
        assert _unfilter_blob(filtered, layout) == plain
        compressed = _best_raw_deflate(filtered)
        assert _unfilter_blob(zlib.decompress(compressed, -15), layout) == plain
        # Optional metadata for tools / future dense expand paths.
        meta["low_range_end"] = low_range_end

    def c_bytes(data: bytes, per: int = 16) -> str:
        lines = []
        for i in range(0, len(data), per):
            chunk = data[i : i + per]
            lines.append(",".join(f"0x{b:02x}" for b in chunk) + ",")
        return "\n".join(lines)

    # Meta header (sizes/offsets only) — safe to include from hot TUs.
    meta_out: list[str] = [
        "// Auto-generated by scripts/pack_tables.py - do not edit.",
        "// Compressed Unicode/IDNA tables (dense/filter + raw DEFLATE) — metadata.",
        "// Payload bytes live in table_blob_data.inc (cold init TU only).",
        "// clang-format off",
        "#ifndef ADA_IDNA_TABLE_BLOB_H",
        "#define ADA_IDNA_TABLE_BLOB_H",
        "#include <cstdint>",
        "#include <cstddef>",
        "namespace ada::idna::table_blob {",
        f"constexpr size_t uncompressed_size = {len(plain)};",
        f"constexpr size_t compressed_size = {len(compressed)};",
        f"constexpr uint32_t uncompressed_crc32 = 0x{uncompressed_crc32:08X}u;",
        f"constexpr uint32_t filter_version = {FILTER_VERSION}u;",
    ]
    for k, v in meta.items():
        meta_out.append(f"constexpr size_t {k} = {v};")
    meta_out.append("// Offsets into the expanded working buffer:")
    for name, off, _nbytes, _kind, count in layout:
        meta_out.append(f"constexpr size_t off_{name} = {off};")
        meta_out.append(f"constexpr size_t count_{name} = {count};")
    meta_out.append("extern const uint8_t compressed[compressed_size];")
    meta_out.append("}  // namespace ada::idna::table_blob")
    meta_out.append("#endif")

    data_out: list[str] = [
        "// Auto-generated by scripts/pack_tables.py - do not edit.",
        "// Compressed table payload (dense/filter + raw DEFLATE).",
        "// clang-format off",
        "#ifndef ADA_IDNA_TABLE_BLOB_DATA_H",
        "#define ADA_IDNA_TABLE_BLOB_DATA_H",
        '#include "table_blob.inc"',
        "namespace ada::idna::table_blob {",
        "alignas(8) const uint8_t compressed[compressed_size] = {",
        c_bytes(compressed),
        "};",
        "static_assert(sizeof(compressed) == compressed_size);",
        "}  // namespace ada::idna::table_blob",
        "#endif",
    ]

    path.write_text("\n".join(meta_out) + "\n")
    BLOB_DATA_PATH.write_text("\n".join(data_out) + "\n")
    kind = "dense" if FILTER_VERSION >= 2 else "filter"
    print(
        f"Wrote {path.relative_to(ROOT)} + {BLOB_DATA_PATH.relative_to(ROOT)}: "
        f"working {len(plain)} <- {kind} "
        f"{dense_uncompressed_size if FILTER_VERSION >= 2 else len(plain)} "
        f"-> {len(compressed)} compressed "
        f"({100 * len(compressed) / max(len(plain), 1):.1f}% of working)"
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
