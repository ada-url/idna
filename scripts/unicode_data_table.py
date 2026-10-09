#!/usr/bin/env python3
"""Generate the normalization and label validity tables from UnicodeData.txt
and CompositionExclusions.txt, and pack them into src/table_blob.inc.

Use the same Unicode version as the mapping table (scripts/idna_table.py).

  python3 scripts/unicode_data_table.py          # print a summary
  python3 scripts/unicode_data_table.py --write  # update src/table_blob.inc

Sections
--------
Normalization (src/normalization.cpp). Each table is indexed by cp >> 8 into
rows of 256 entries; blocks without data share one all-zero row.

  decomposition_index/block/data
      Row entry = (offset << 2) | flags. The decomposition of cp is
      data[entry(cp) >> 2 .. entry(cp + 1) >> 2), so rows have 257 columns.
      The data holds the full (recursive) canonical decomposition, or the full
      compatibility decomposition when only that exists. Flags: 1 = the
      decomposition is compatibility-only; 2 = canonical, but the full
      compatibility decomposition differs. data[0] is unused. Hangul syllables
      are decomposed algorithmically and have no entries.
  ccc_index/block
      Canonical_Combining_Class.
  composition_index/block/data
      Row entry = index into data, which holds (second, composite) pairs
      sorted by second for each first code point; [entry(cp), entry(cp + 1))
      are the pairs starting with cp. Only primary composites (canonical
      decompositions of length two, not Full_Composition_Exclusion) appear.
      data[0] is unused.

Validity (src/validity.cpp).

  combining_flat
      General_Category Mn, Mc or Me ranges: a label must not begin with a mark.
  dir_start/dir_final/dir_value
      Bidi_Class ranges of assigned code points for the RFC 5893 Bidi rule.
      Unassigned code points are rejected by the mapping table first and look
      up as NONE.
"""
from __future__ import annotations

import os
import re
import sys
import urllib.request
from collections import defaultdict

UNICODE_VERSION = "17.0.0"
UCD_URL = "https://www.unicode.org/Public/{version}/ucd/{name}"

MAX_CODE_POINT = 0x10FFFF
BLOCK_COUNT = (MAX_CODE_POINT + 1) >> 8

HANGUL_SBASE = 0xAC00
HANGUL_SCOUNT = 11172

COMBINING_CATEGORIES = {"Mn", "Mc", "Me"}

# Must match `enum direction` in src/validity.cpp (0 is NONE).
BIDI_CLASSES = [
    "NONE", "BN", "CS", "ES", "ON", "EN", "L", "R", "NSM", "AL", "AN", "ET",
    "WS", "RLO", "LRO", "PDF", "RLE", "RLI", "FSI", "PDI", "LRI", "B", "S",
    "LRE",
]


def get_ucd_file(name: str, version: str = UNICODE_VERSION) -> str:
    """Return a UCD file, downloading it once into the working directory."""
    stem, ext = os.path.splitext(name)
    cached = f"{stem}-{version}{ext}"
    if not os.path.exists(cached):
        url = UCD_URL.format(version=version, name=name)
        with urllib.request.urlopen(url) as response:
            data = response.read()
        with open(cached, "wb") as f:
            f.write(data)
    with open(cached, encoding="utf-8") as f:
        return f.read()


class UnicodeData:
    """The UnicodeData.txt fields used here, keyed by code point."""

    def __init__(self, text: str):
        self.category: dict[int, str] = {}
        self.ccc: dict[int, int] = {}
        self.bidi: dict[int, str] = {}
        self.canonical: dict[int, list[int]] = {}
        self.compatibility: dict[int, list[int]] = {}
        range_first = None
        for line in text.splitlines():
            if not line:
                continue
            fields = line.split(";")
            cp = int(fields[0], 16)
            name = fields[1]
            if name.endswith(", First>"):
                range_first = cp
                continue
            first = cp
            if name.endswith(", Last>"):
                first, range_first = range_first, None
            for c in range(first, cp + 1):
                self.category[c] = fields[2]
                self.ccc[c] = int(fields[3])
                self.bidi[c] = fields[4]
            decomposition = fields[5].split()
            if decomposition and decomposition[0].startswith("<"):
                mapping = [int(x, 16) for x in decomposition[1:]]
                self.compatibility[cp] = mapping
            elif decomposition:
                self.canonical[cp] = [int(x, 16) for x in decomposition]

    def full_decomposition(self, cp: int, compatibility: bool) -> list[int]:
        mapping = self.canonical.get(cp)
        if mapping is None and compatibility:
            mapping = self.compatibility.get(cp)
        if mapping is None:
            return [cp]
        return [
            x
            for c in mapping
            for x in self.full_decomposition(c, compatibility)
        ]


def parse_composition_exclusions(text: str) -> set[int]:
    excluded: set[int] = set()
    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        first, _, last = line.partition("..")
        excluded.update(range(int(first, 16), int(last or first, 16) + 1))
    return excluded


def build_multistage(
    entries: list[int], cols: int, block_is_empty: list[bool]
) -> tuple[list[int], list[int]]:
    """Split per-code-point entries into an index and deduplicated rows.

    Row b covers entries[b * 256 : b * 256 + cols]. Empty blocks get a zero
    row.
    """
    index: list[int] = []
    rows: list[int] = []
    row_ids: dict[tuple[int, ...], int] = {}
    for block in range(BLOCK_COUNT):
        if block_is_empty[block]:
            row = (0,) * cols
        else:
            row = tuple(entries[block << 8 : (block << 8) + cols])
        if row not in row_ids:
            row_ids[row] = len(row_ids)
            rows.extend(row)
        index.append(row_ids[row])
    if len(row_ids) > 256:
        raise SystemExit(f"{len(row_ids)} rows do not fit a uint8_t index")
    return index, rows


def build_decomposition(ucd: UnicodeData) -> dict[str, list[int]]:
    data = [0]
    entries = [0] * (MAX_CODE_POINT + 2)
    has_data = [False] * BLOCK_COUNT
    for cp in range(MAX_CODE_POINT + 1):
        flags = 0
        decomposition: list[int] = []
        if HANGUL_SBASE <= cp < HANGUL_SBASE + HANGUL_SCOUNT:
            pass
        elif cp in ucd.canonical:
            decomposition = ucd.full_decomposition(cp, compatibility=False)
            if ucd.full_decomposition(cp, compatibility=True) != decomposition:
                flags = 2
        elif cp in ucd.compatibility:
            decomposition = ucd.full_decomposition(cp, compatibility=True)
            flags = 1
        entries[cp] = (len(data) << 2) | flags
        if decomposition:
            data.extend(decomposition)
            has_data[cp >> 8] = True
    entries[MAX_CODE_POINT + 1] = len(data) << 2
    if len(data) >= 1 << 14:
        raise SystemExit("decomposition data does not fit 14-bit offsets")
    index, block = build_multistage(entries, 257, [not x for x in has_data])
    return {
        "decomposition_index": index,
        "decomposition_block": block,
        "decomposition_data": data,
    }


def build_ccc(ucd: UnicodeData) -> dict[str, list[int]]:
    entries = [ucd.ccc.get(cp, 0) for cp in range(MAX_CODE_POINT + 1)]
    is_empty = [
        not any(entries[block << 8 : (block + 1) << 8])
        for block in range(BLOCK_COUNT)
    ]
    index, block = build_multistage(entries, 256, is_empty)
    return {"ccc_index": index, "ccc_block": block}


def build_composition(
    ucd: UnicodeData, exclusions: set[int]
) -> dict[str, list[int]]:
    pairs: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for cp, mapping in ucd.canonical.items():
        if (
            len(mapping) != 2
            or cp in exclusions
            or ucd.ccc.get(cp, 0) != 0
            or ucd.ccc.get(mapping[0], 0) != 0
        ):
            continue
        pairs[mapping[0]].append((mapping[1], cp))
    data = [0]
    entries = [0] * (MAX_CODE_POINT + 2)
    has_data = [False] * BLOCK_COUNT
    for cp in range(MAX_CODE_POINT + 1):
        entries[cp] = len(data)
        for second, composite in sorted(pairs.get(cp, ())):
            data.extend((second, composite))
            has_data[cp >> 8] = True
    entries[MAX_CODE_POINT + 1] = len(data)
    if len(data) > 0xFFFF:
        raise SystemExit("composition data does not fit 16-bit indexes")
    index, block = build_multistage(entries, 257, [not x for x in has_data])
    return {
        "composition_index": index,
        "composition_block": block,
        "composition_data": data,
    }


def build_ranges(values: dict[int, int]) -> list[tuple[int, int, int]]:
    """Merge code points with equal values into (first, last, value) ranges."""
    ranges: list[tuple[int, int, int]] = []
    for cp in sorted(values):
        value = values[cp]
        if ranges and ranges[-1][2] == value and ranges[-1][1] + 1 == cp:
            ranges[-1] = (ranges[-1][0], cp, value)
        else:
            ranges.append((cp, cp, value))
    return ranges


def build_validity(ucd: UnicodeData) -> dict[str, list[int]]:
    marks = {
        cp: 1 for cp, gc in ucd.category.items() if gc in COMBINING_CATEGORIES
    }
    combining = build_ranges(marks)
    unknown = set(ucd.bidi.values()) - set(BIDI_CLASSES[1:])
    if unknown:
        raise SystemExit(f"unknown Bidi_Class values {sorted(unknown)}")
    directions = build_ranges(
        {cp: BIDI_CLASSES.index(bidi) for cp, bidi in ucd.bidi.items()}
    )
    return {
        "combining_flat": [cp for r in combining for cp in r[:2]],
        "dir_start": [first for first, _, _ in directions],
        "dir_final": [last for _, last, _ in directions],
        "dir_value": [value for _, _, value in directions],
    }


def build_sections(
    unicode_data_text: str, composition_exclusions_text: str
) -> dict[str, list[int]]:
    ucd = UnicodeData(unicode_data_text)
    exclusions = parse_composition_exclusions(composition_exclusions_text)
    sections: dict[str, list[int]] = {}
    sections.update(build_decomposition(ucd))
    sections.update(build_ccc(ucd))
    sections.update(build_composition(ucd, exclusions))
    sections.update(build_validity(ucd))
    return sections


def generate(version: str = UNICODE_VERSION) -> dict[str, list[int]]:
    exclusions = get_ucd_file("CompositionExclusions.txt", version)
    m = re.search(r"# CompositionExclusions-(\S+)\.txt", exclusions)
    if not m or m.group(1) != version:
        raise SystemExit(f"CompositionExclusions.txt is not version {version}")
    return build_sections(get_ucd_file("UnicodeData.txt", version), exclusions)


def update_in_blob(generated: dict[str, list[int]]) -> None:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if script_dir not in sys.path:
        sys.path.insert(0, script_dir)
    import pack_tables

    sections = pack_tables.load_blob_sections()
    sections.update(generated)
    pack_tables.write_blob(sections)


def main(argv: list[str]) -> int:
    if argv in (["-h"], ["--help"]):
        print(__doc__)
        return 0
    if argv not in ([], ["--write"], ["-w"]):
        print(__doc__)
        return 1
    generated = generate()
    g = generated
    print(f"Unicode {UNICODE_VERSION}")
    print(f"  decomposition: {len(g['decomposition_block']) // 257} rows, "
          f"{len(g['decomposition_data'])} code points")
    print(f"  ccc:           {len(g['ccc_block']) // 256} rows")
    print(f"  composition:   {len(g['composition_block']) // 257} rows, "
          f"{(len(g['composition_data']) - 1) // 2} pairs")
    print(f"  combining:     {len(g['combining_flat']) // 2} ranges")
    print(f"  directions:    {len(g['dir_start'])} ranges")
    if argv:
        update_in_blob(generated)
        print("Normalization and validity tables packed into table_blob.inc")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
