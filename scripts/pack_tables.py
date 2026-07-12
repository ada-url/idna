#!/usr/bin/env python3
"""Pack IDNA/Unicode tables into a raw-DEFLATE blob (src/table_blob.inc).

This script is the source-of-truth generator for the compressed table payload
used at runtime (see src/table_store.hpp).

It expects the *expanded* table arrays to be present in:
  - src/mapping_tables.cpp
  - src/normalization_tables.cpp  (structural form with named arrays)
  - src/id_tables.cpp
  - src/validity.cpp              (dir_table + combining_ranges)

After a Unicode version bump, restore/regenerate those expanded arrays, then:
  python3 scripts/pack_tables.py

Runtime decompression is one-shot via ensure_tables() (heap buffer).
"""
from __future__ import annotations

import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse_array(content: str, name: str):
    m = re.search(rf"\b{re.escape(name)}\[(\d+)\](?:\[(\d+)\])?\s*=\s*\{{", content)
    if not m:
        raise SystemExit(f"missing array {name}")
    dims = [int(x) for x in m.groups() if x]
    start = m.end() - 1
    depth = 0
    end = None
    for i in range(start, len(content)):
        if content[i] == "{":
            depth += 1
        elif content[i] == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
    body = content[start + 1 : end]
    vals = [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", body)]
    expected = 1
    for d in dims:
        expected *= d
    if len(vals) < expected:
        vals += [0] * (expected - len(vals))
    elif len(vals) > expected:
        vals = vals[:expected]
    return vals, dims


def parse_pairs(content: str, name: str):
    m = re.search(rf"{name}\[(\d+)\]\[2\]\s*=\s*\{{(.*?)\}};", content, re.S)
    if not m:
        raise SystemExit(f"missing pairs {name}")
    pairs = [(int(a), int(b)) for a, b in re.findall(r"\{(\d+),\s*(\d+)\}", m.group(2))]
    n = int(m.group(1))
    if len(pairs) != n:
        raise SystemExit(f"{name}: got {len(pairs)} pairs, declared {n}")
    flat: list[int] = []
    for a, b in pairs:
        flat.extend([a, b])
    return flat, n


def main() -> int:
    mc = (ROOT / "src/mapping_tables.cpp").read_text()
    nc = (ROOT / "src/normalization_tables.cpp").read_text()
    ic = (ROOT / "src/id_tables.cpp").read_text()
    vc = (ROOT / "src/validity.cpp").read_text()

    # Fail fast if tables were already packed away.
    if "idna_stage1[" not in mc or "const uint16_t idna_stage1" not in mc:
        print(
            "error: expanded mapping tables not found in src/mapping_tables.cpp\n"
            "Restore the pre-pack array form (or regenerate from scripts/idna_table.py)\n"
            "before running this packer.",
            file=sys.stderr,
        )
        return 1

    layout: list[tuple[str, int, int, str, int]] = []
    blob = bytearray()

    def align(a: int) -> None:
        while len(blob) % a:
            blob.append(0)

    def add_u16(name: str, vals: list[int]) -> None:
        align(2)
        off = len(blob)
        blob.extend(struct.pack(f"<{len(vals)}H", *vals))
        layout.append((name, off, len(vals) * 2, "uint16_t", len(vals)))

    def add_u32(name: str, vals: list[int]) -> None:
        align(4)
        off = len(blob)
        blob.extend(struct.pack(f"<{len(vals)}I", *vals))
        layout.append((name, off, len(vals) * 4, "uint32_t", len(vals)))

    def add_u8(name: str, vals: list[int]) -> None:
        off = len(blob)
        blob.extend(bytes(vals))
        layout.append((name, off, len(vals), "uint8_t", len(vals)))

    def add_u64(name: str, vals: list[int]) -> None:
        align(8)
        off = len(blob)
        blob.extend(struct.pack(f"<{len(vals)}Q", *vals))
        layout.append((name, off, len(vals) * 8, "uint64_t", len(vals)))

    s1, _ = parse_array(mc, "idna_stage1")
    s2, _ = parse_array(mc, "idna_stage2")
    bb, _ = parse_array(mc, "idna_bool_blocks")
    u8, _ = parse_array(mc, "idna_utf8_mappings")
    add_u16("idna_stage1", s1)
    add_u16("idna_stage2", s2)
    add_u64("idna_bool_blocks", bb)
    add_u8("idna_utf8_mappings", u8)

    add_u32("decomposition_cp", parse_array(nc, "decomposition_cp")[0])
    add_u16("decomposition_offset", parse_array(nc, "decomposition_offset")[0])
    add_u8("decomposition_length", parse_array(nc, "decomposition_length")[0])
    add_u16("decomposition_data16", parse_array(nc, "decomposition_data16")[0])
    add_u16("decomposition_high_index", parse_array(nc, "decomposition_high_index")[0])
    add_u32("decomposition_high_cp", parse_array(nc, "decomposition_high_cp")[0])
    add_u32("ccc_range_start", parse_array(nc, "ccc_range_start")[0])
    add_u8("ccc_range_length", parse_array(nc, "ccc_range_length")[0])
    add_u8("ccc_range_value", parse_array(nc, "ccc_range_value")[0])
    add_u16("composition_sparse_page", parse_array(nc, "composition_sparse_page")[0])
    add_u8("composition_sparse_block", parse_array(nc, "composition_sparse_block")[0])
    cpb, dims = parse_array(nc, "composition_block")
    add_u16("composition_block_flat", cpb)
    add_u16("composition_data16", parse_array(nc, "composition_data16")[0])
    add_u16("composition_high_index", parse_array(nc, "composition_high_index")[0])
    add_u32("composition_high_cp", parse_array(nc, "composition_high_cp")[0])

    icont, n_ic = parse_pairs(ic, "id_continue")
    istart, n_is = parse_pairs(ic, "id_start")
    add_u32("id_continue_flat", icont)
    add_u32("id_start_flat", istart)

    entries = re.findall(
        r"\{(0x[0-9a-fA-F]+|\d+),\s*(0x[0-9a-fA-F]+|\d+),\s*direction::(\w+)\}",
        vc,
    )
    dir_names = [
        "NONE", "BN", "CS", "ES", "ON", "EN", "L", "R", "NSM", "AL", "AN", "ET",
        "WS", "RLO", "LRO", "PDF", "RLE", "RLI", "FSI", "PDI", "LRI", "B", "S", "LRE",
    ]
    dir_map = {n: i for i, n in enumerate(dir_names)}
    add_u32("dir_start", [int(a, 0) for a, _, _ in entries])
    add_u32("dir_final", [int(b, 0) for _, b, _ in entries])
    add_u8("dir_value", [dir_map[d] for *_, d in entries])

    m = re.search(r"combining_ranges\[\]\[2\] = \{(.*?)\};", vc, re.S)
    if not m:
        raise SystemExit("missing combining_ranges")
    cranges = [(int(a), int(b)) for a, b in re.findall(r"\{(\d+),\s*(\d+)\}", m.group(1))]
    cflat: list[int] = []
    for a, b in cranges:
        cflat.extend([a, b])
    add_u32("combining_flat", cflat)

    uncompressed = bytes(blob)
    co = zlib.compressobj(9, zlib.DEFLATED, -15)
    compressed = co.compress(uncompressed) + co.flush()
    assert zlib.decompress(compressed, -15) == uncompressed

    counts = {name: count for name, _, _, _, count in layout}
    meta = {
        "decomposition_count": counts["decomposition_cp"],
        "decomposition_high_count": counts["decomposition_high_index"],
        "ccc_range_count": counts["ccc_range_start"],
        "composition_sparse_count": counts["composition_sparse_page"],
        "composition_block_count": dims[0] if dims else 24,
        "composition_high_count": counts["composition_high_index"],
        "id_continue_count": n_ic,
        "id_start_count": n_is,
        "dir_table_count": len(entries),
        "combining_range_count": len(cranges),
    }

    def c_bytes(data: bytes, per: int = 16) -> str:
        lines = []
        for i in range(0, len(data), per):
            chunk = data[i : i + per]
            lines.append(",".join(f"0x{b:02x}" for b in chunk) + ",")
        return "\n".join(lines)

    out: list[str] = [
        "// Auto-generated by scripts/pack_tables.py — do not edit.",
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
    for name, off, nbytes, ctype, count in layout:
        out.append(f"constexpr size_t off_{name} = {off};")
        out.append(f"constexpr size_t count_{name} = {count};")
    out.append("alignas(8) inline constexpr uint8_t compressed[] = {")
    out.append(c_bytes(compressed))
    out.append("};")
    out.append("}  // namespace ada::idna::table_blob")
    out.append("#endif")

    path = ROOT / "src/table_blob.inc"
    path.write_text("\n".join(out) + "\n")
    print(f"Wrote {path}")
    print(f"  uncompressed: {len(uncompressed)} bytes")
    print(f"  compressed:   {len(compressed)} bytes "
          f"({100 * len(compressed) / len(uncompressed):.1f}%)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
