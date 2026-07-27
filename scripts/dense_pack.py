"""Dense on-disk table encoding (filter_version 2).

Encoding (little-endian lengths):
  [u32 map_len][map_bytes]
  [u32 decomp_len][decomp_bytes]
  [u32 ccc_len][ccc_bytes]
  [u32 misc_len][misc_bytes]

map: runs of (varint start_delta, varint length_or_0, u8 kind, optional payload)
  kind 0=VALID run, 1=DIS run, 2=IGN run, 3=MAP single with u8 strlen + utf8
decomp: (varint cp_delta, u8 n, n * u24 le) for IDNA alphabet NFC-closure only
ccc: (varint cp_delta, u8 ccc) for the same closure
misc: id_continue / id_start / combining range pairs, then dir SoA as varints

At pack and at runtime, expand_dense() rebuilds multi-stage working tables
(lookup-equivalent for IDNA processing). Composition is derived from
canonical length-2 decompositions (Hangul stays algorithmic).
"""
from __future__ import annotations

import struct
from collections import defaultdict, deque
from pathlib import Path
from typing import Any

VALID = 0xFFFF
DIS = 0xFFFE
IGN = 0
BOOL = 0x8000
BLOCK = 64
NPAGES = 0x1100  # Unicode scalar pages


def put_varint(v: int) -> bytes:
    out = bytearray()
    v &= 0xFFFFFFFF
    while v > 0x7F:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    out.append(v & 0x7F)
    return bytes(out)


def put_u24(v: int) -> bytes:
    return bytes((v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF))


def get_varint(buf: bytes, i: int) -> tuple[int, int]:
    v = 0
    s = 0
    while True:
        if i >= len(buf):
            raise ValueError("varint EOF")
        b = buf[i]
        i += 1
        v |= (b & 0x7F) << s
        if not (b & 0x80):
            return v, i
        s += 7
        if s > 35:
            raise ValueError("varint overflow")


def get_u24(buf: bytes, i: int) -> tuple[int, int]:
    if i + 3 > len(buf):
        raise ValueError("u24 EOF")
    return buf[i] | (buf[i + 1] << 8) | (buf[i + 2] << 16), i + 3


def build_dense(sections: dict, mapping_cpp: Path) -> tuple[bytes, dict]:
    import re

    mt = mapping_cpp.read_text()
    low = int(re.search(r"IDNA_LOW_RANGE_END\s*=\s*0x([0-9A-F]+)", mt).group(1), 16)
    s1, s2, bools = (
        sections["idna_stage1"],
        sections["idna_stage2"],
        sections["idna_bool_blocks"],
    )
    utf8 = bytes(sections["idna_utf8_mappings"])

    def status(cp: int):
        ref = s1[cp >> 6]
        if ref & BOOL:
            bit_idx = (ref & ~BOOL) * 64 + (cp & 63)
            return ("V" if (bools[bit_idx >> 6] >> (bit_idx & 63)) & 1 else "D", None)
        v = s2[ref + (cp & 63)]
        if v == VALID:
            return ("V", None)
        if v == DIS:
            return ("D", None)
        if v == IGN:
            return ("I", None)
        return ("M", v)

    enc = bytearray()
    prev = 0
    cp = 0
    while cp < low:
        st, off = status(cp)
        if st == "M":
            end = utf8.find(0, off)
            s = bytes(utf8[off:end])
            enc += put_varint(cp - prev)
            enc += put_varint(0)
            enc.append(3)
            enc.append(len(s))
            enc += s
            prev = cp + 1
            cp += 1
            continue
        start = cp
        while cp + 1 < low and status(cp + 1)[0] == st:
            cp += 1
        enc += put_varint(start - prev)
        enc += put_varint(cp - start)
        enc.append({"V": 0, "D": 1, "I": 2}[st])
        prev = cp + 1
        cp += 1

    alphabet: set[int] = set()
    for c in range(low):
        st, off = status(c)
        if st == "V":
            alphabet.add(c)
        elif st == "M":
            end = utf8.find(0, off)
            for ch in utf8[off:end].decode("utf-8"):
                alphabet.add(ord(ch))

    meta = sections["_meta"]
    rows, cols = meta["decomposition_block_rows"], meta["decomposition_block_cols"]
    di, db, dd = (
        sections["decomposition_index"],
        sections["decomposition_block"],
        sections["decomposition_data"],
    )

    def decomp(cp: int) -> list[int]:
        if cp >= 0x110000:
            return []
        base = di[cp >> 8] * cols + (cp % 256)
        d0, d1 = db[base], db[base + 1]
        ln = (d1 >> 2) - (d0 >> 2)
        if ln == 0 or (d0 & 1):
            return []
        start = d0 >> 2
        return list(dd[start : start + ln])

    need = set(alphabet)
    q: deque[int] = deque(alphabet)
    while q:
        c = q.popleft()
        for p in decomp(c):
            if p not in need:
                need.add(p)
                q.append(p)

    enc2 = bytearray()
    prev = 0
    for c in sorted(need):
        seq = decomp(c)
        if not seq:
            continue
        enc2 += put_varint(c - prev)
        prev = c
        enc2.append(len(seq))
        for v in seq:
            enc2 += put_u24(v)

    ci, cb = sections["ccc_index"], sections["ccc_block"]
    ccc_cols = meta["ccc_block_cols"]
    enc3 = bytearray()
    prev = 0
    for c in sorted(need):
        v = cb[ci[c >> 8] * ccc_cols + (c % 256)]
        if not v:
            continue
        enc3 += put_varint(c - prev)
        prev = c
        enc3.append(v)

    misc = bytearray()
    for name in ("id_continue_flat", "id_start_flat", "combining_flat"):
        vals = sections[name]
        pairs = list(zip(vals[0::2], vals[1::2]))
        misc += put_varint(len(pairs))
        prev = 0
        for a, b in pairs:
            misc += put_varint(a - prev)
            misc += put_varint(b - a)
            prev = b + 1
    ds, df, dv = sections["dir_start"], sections["dir_final"], sections["dir_value"]
    misc += put_varint(len(ds))
    prev = 0
    for i in range(len(ds)):
        misc += put_varint(ds[i] - prev)
        misc += put_varint(df[i] - ds[i])
        misc.append(dv[i])
        prev = df[i] + 1

    parts = [bytes(enc), bytes(enc2), bytes(enc3), bytes(misc)]
    out = bytearray()
    for p in parts:
        out += struct.pack("<I", len(p))
        out += p
    meta_out = {
        "low_range_end": low,
        "dense_raw": len(out),
        "map_len": len(enc),
        "decomp_len": len(enc2),
        "ccc_len": len(enc3),
        "misc_len": len(misc),
    }
    return bytes(out), meta_out


def expand_dense(dense: bytes, low_range_end: int) -> dict[str, Any]:
    """Expand dense payload into multi-stage section lists (same keys as pack_tables)."""
    off = 0
    parts: list[bytes] = []
    for _ in range(4):
        if off + 4 > len(dense):
            raise ValueError("dense truncated")
        n = struct.unpack_from("<I", dense, off)[0]
        off += 4
        if off + n > len(dense):
            raise ValueError("dense section OOB")
        parts.append(dense[off : off + n])
        off += n
    mapb, decompb, cccb, miscb = parts

    # --- map → flat → two-level ---
    flat = [DIS] * low_range_end
    utf8 = bytearray([0])
    seq_to_idx: dict[bytes, int] = {b"": 0}
    i = 0
    cp = 0
    while i < len(mapb):
        d, i = get_varint(mapb, i)
        ln, i = get_varint(mapb, i)
        kind = mapb[i]
        i += 1
        cp += d
        if kind == 3:
            n = mapb[i]
            i += 1
            s = bytes(mapb[i : i + n])
            i += n
            if s not in seq_to_idx:
                seq_to_idx[s] = len(utf8)
                utf8 += s + b"\x00"
            if cp < low_range_end:
                flat[cp] = seq_to_idx[s]
            cp += 1
        else:
            val = {0: VALID, 1: DIS, 2: IGN}[kind]
            for c in range(cp, cp + ln + 1):
                if c < low_range_end:
                    flat[c] = val
            cp += ln + 1

    stage1: list[int] = []
    mixed: list[int] = []
    bool_words: list[int] = []
    mixed_map: dict[tuple[int, ...], int] = {}
    bool_map: dict[int, int] = {}
    n_blocks = (low_range_end + BLOCK - 1) // BLOCK
    for bi in range(n_blocks):
        block = flat[bi * BLOCK : (bi + 1) * BLOCK]
        if len(block) < BLOCK:
            block = block + [DIS] * (BLOCK - len(block))
        if all(v in (VALID, DIS) for v in block):
            bits = 0
            for j, v in enumerate(block):
                if v == VALID:
                    bits |= 1 << j
            if bits not in bool_map:
                bool_map[bits] = len(bool_words)
                bool_words.append(bits)
            stage1.append(BOOL | bool_map[bits])
        else:
            key = tuple(block)
            if key not in mixed_map:
                mixed_map[key] = len(mixed)
                mixed.extend(block)
            stage1.append(mixed_map[key])

    # --- sparse decomp ---
    decomps: dict[int, list[int]] = {}
    i = 0
    prev = 0
    while i < len(decompb):
        d, i = get_varint(decompb, i)
        prev += d
        n = decompb[i]
        i += 1
        seq: list[int] = []
        for _ in range(n):
            v, i = get_u24(decompb, i)
            seq.append(v)
        decomps[prev] = seq

    cccs: dict[int, int] = {}
    i = 0
    prev = 0
    while i < len(cccb):
        d, i = get_varint(cccb, i)
        prev += d
        cccs[prev] = cccb[i]
        i += 1

    # multi-stage decomp
    decomp_data: list[int] = []
    cp_info: dict[int, tuple[int, int]] = {}
    for c in sorted(decomps):
        seq = decomps[c]
        start = len(decomp_data)
        decomp_data.extend(seq)
        cp_info[c] = (start, len(seq))

    blocks: list[list[int]] = []
    block_map: dict[tuple[int, ...], int] = {}
    index = [0] * NPAGES
    empty = tuple([0] * 257)
    block_map[empty] = 0
    blocks.append(list(empty))

    for page in range(NPAGES):
        entries: list[tuple[int, int] | None] = []
        for j in range(256):
            c = page * 256 + j
            if c in cp_info:
                st, ln = cp_info[c]
                entries.append((st, ln))
            else:
                entries.append(None)
        if not any(e is not None for e in entries):
            index[page] = 0
            continue
        # Boundary style from the right so empty cells have length 0 even when
        # the next real entry starts at a non-zero global data offset.
        m = [0] * 257
        last_end = 0
        for e in entries:
            if e is not None:
                last_end = e[0] + e[1]
        m[256] = last_end << 2
        for j in range(255, -1, -1):
            e = entries[j]
            if e is not None:
                st, ln = e
                m[j] = st << 2
                # length must be ln: M[j+1] >> 2 - st == ln
                m[j + 1] = (st + ln) << 2
            else:
                m[j] = m[j + 1]
        key = tuple(m)
        if key not in block_map:
            block_map[key] = len(blocks)
            blocks.append(list(m))
        index[page] = block_map[key]

    decomp_block: list[int] = []
    for b in blocks:
        decomp_block.extend(b)
    decomp_rows = len(blocks)

    # multi-stage ccc
    ccc_blocks: list[bytearray] = []
    ccc_bmap: dict[bytes, int] = {}
    ccc_index = [0] * NPAGES
    empty_c = bytes(256)
    ccc_bmap[empty_c] = 0
    ccc_blocks.append(bytearray(256))
    for page in range(NPAGES):
        row = bytearray(256)
        anyv = False
        for j in range(256):
            c = page * 256 + j
            if c in cccs:
                row[j] = cccs[c]
                anyv = True
        if not anyv:
            ccc_index[page] = 0
            continue
        keyb = bytes(row)
        if keyb not in ccc_bmap:
            ccc_bmap[keyb] = len(ccc_blocks)
            ccc_blocks.append(row)
        ccc_index[page] = ccc_bmap[keyb]
    ccc_block = bytearray()
    for b in ccc_blocks:
        ccc_block += b
    ccc_rows = len(ccc_blocks)

    # composition from length-2 canonical decomps with starter ccc==0
    pairs_by_starter: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for c, seq in decomps.items():
        if len(seq) != 2:
            continue
        a, b = seq
        if cccs.get(a, 0) != 0:
            continue
        pairs_by_starter[a].append((b, c))
    for a in pairs_by_starter:
        pairs_by_starter[a].sort()

    composition_data = [0]
    starter_range: dict[int, tuple[int, int]] = {}
    for page in range(NPAGES):
        for c in range(page * 256, (page + 1) * 256):
            if c not in pairs_by_starter:
                continue
            left = len(composition_data)
            for t, r in pairs_by_starter[c]:
                composition_data.append(t)
                composition_data.append(r)
            starter_range[c] = (left, len(composition_data))

    comp_blocks: list[list[int]] = []
    comp_bmap: dict[tuple[int, ...], int] = {}
    comp_index = [0] * NPAGES
    empty_comp = tuple([1] * 257)
    comp_bmap[empty_comp] = 0
    comp_blocks.append(list(empty_comp))
    for page in range(NPAGES):
        items: list[tuple[int, int, int]] = []
        for j in range(256):
            c = page * 256 + j
            if c in starter_range:
                left, right = starter_range[c]
                items.append((j, left, right))
        if not items:
            comp_index[page] = 0
            continue
        m = [1] * 257
        first_left = items[0][1]
        for j in range(items[0][0] + 1):
            m[j] = first_left
        for idx, (j, left, right) in enumerate(items):
            m[j] = left
            m[j + 1] = right
            next_j = items[idx + 1][0] if idx + 1 < len(items) else 256
            for k in range(j + 1, next_j + 1):
                m[k] = right
        key = tuple(m)
        if key not in comp_bmap:
            comp_bmap[key] = len(comp_blocks)
            comp_blocks.append(list(m))
        comp_index[page] = comp_bmap[key]

    comp_block: list[int] = []
    for b in comp_blocks:
        comp_block.extend(b)
    comp_rows = len(comp_blocks)

    def read_pairs(buf: bytes, i: int) -> tuple[list[tuple[int, int]], int]:
        n, i = get_varint(buf, i)
        pairs: list[tuple[int, int]] = []
        prev = 0
        for _ in range(n):
            da, i = get_varint(buf, i)
            a = prev + da
            db, i = get_varint(buf, i)
            b = a + db
            pairs.append((a, b))
            prev = b + 1
        return pairs, i

    i = 0
    id_cont, i = read_pairs(miscb, i)
    id_start, i = read_pairs(miscb, i)
    comb, i = read_pairs(miscb, i)
    n, i = get_varint(miscb, i)
    dir_s: list[int] = []
    dir_f: list[int] = []
    dir_v: list[int] = []
    prev = 0
    for _ in range(n):
        ds, i = get_varint(miscb, i)
        a = prev + ds
        df, i = get_varint(miscb, i)
        b = a + df
        v = miscb[i]
        i += 1
        dir_s.append(a)
        dir_f.append(b)
        dir_v.append(v)
        prev = b + 1

    return {
        "idna_stage1": stage1,
        "idna_stage2": mixed,
        "idna_bool_blocks": bool_words,
        "idna_utf8_mappings": list(utf8),
        "decomposition_index": index,
        "decomposition_block": decomp_block,
        "decomposition_data": decomp_data,
        "ccc_index": list(ccc_index),
        "ccc_block": list(ccc_block),
        "composition_index": list(comp_index),
        "composition_block": comp_block,
        "composition_data": composition_data,
        "id_continue_flat": [x for p in id_cont for x in p],
        "id_start_flat": [x for p in id_start for x in p],
        "dir_start": dir_s,
        "dir_final": dir_f,
        "dir_value": dir_v,
        "combining_flat": [x for p in comb for x in p],
        "_meta": {
            "decomposition_block_rows": decomp_rows,
            "decomposition_block_cols": 257,
            "ccc_block_rows": ccc_rows,
            "ccc_block_cols": 256,
            "composition_block_rows": comp_rows,
            "composition_block_cols": 257,
            "id_continue_count": len(id_cont),
            "id_start_count": len(id_start),
            "dir_table_count": len(dir_s),
            "combining_range_count": len(comb),
            "low_range_end": low_range_end,
        },
    }


def sections_to_plain(sections: dict[str, Any], section_order, align_map, pack_map, width_map) -> tuple[bytes, list]:
    """Pack sections into aligned little-endian plain working buffer."""
    blob = bytearray()
    layout = []

    def align(a: int) -> None:
        while len(blob) % a:
            blob.append(0)

    for name, kind in section_order:
        vals = sections[name]
        a = align_map[kind]
        align(a)
        off = len(blob)
        fmt = "<" + pack_map[kind] * len(vals)
        blob.extend(struct.pack(fmt, *vals))
        layout.append((name, off, len(vals) * width_map[kind], kind, len(vals)))
    return bytes(blob), layout


if __name__ == "__main__":
    import zlib
    from pathlib import Path

    import pack_tables as pt

    sections = pt.load_blob_sections()
    dense, meta = build_dense(sections, Path("src/mapping_tables.cpp"))
    print(meta)
    co = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    z = co.compress(dense) + co.flush()
    print("zlib", len(z))
    try:
        import zopfli.zlib as zz

        print("zopfli", len(zz.compress(dense, numiterations=15)[2:-4]))
    except Exception as e:
        print("zopfli", e)
    exp = expand_dense(dense, meta["low_range_end"])
    print("expanded meta", exp["_meta"])
