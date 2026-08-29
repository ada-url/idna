#pragma once
// Compact expand of filter_version 2 dense payload → multi-stage working
// buffer. Matches scripts/dense_pack.py expand_dense() (CRC-checked).
#include <cstdint>
#include <cstring>
#include <new>

#include "table_blob.inc"

namespace ada::idna::detail {

namespace dense_exp {

[[nodiscard]] inline bool read_varint(const uint8_t*& p, const uint8_t* end,
                                      uint32_t& v) noexcept {
  v = 0;
  int s = 0;
  while (p < end) {
    const uint8_t b = *p++;
    v |= static_cast<uint32_t>(b & 0x7fu) << s;
    if ((b & 0x80u) == 0) {
      return true;
    }
    s += 7;
    if (s > 35) {
      return false;
    }
  }
  return false;
}

[[nodiscard]] inline bool read_u24(const uint8_t*& p, const uint8_t* end,
                                   uint32_t& v) noexcept {
  if (p + 3 > end) {
    return false;
  }
  v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
      (static_cast<uint32_t>(p[2]) << 16);
  p += 3;
  return true;
}

inline void store_u16(uint8_t* p, uint16_t v) noexcept {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
inline void store_u32(uint8_t* p, uint32_t v) noexcept {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16);
  p[3] = static_cast<uint8_t>(v >> 24);
}
inline void store_u64(uint8_t* p, uint64_t v) noexcept {
  for (int i = 0; i < 8; ++i) {
    p[i] = static_cast<uint8_t>(v >> (8 * i));
  }
}

// Find identical NUL-terminated utf8 string already stored; return offset or
// -1.
inline int find_utf8(const uint8_t* utf8, size_t utf8_n, const uint8_t* s,
                     size_t sn) noexcept {
  size_t i = 0;
  while (i < utf8_n) {
    size_t j = 0;
    while (j < sn && i + j < utf8_n && utf8[i + j] == s[j]) {
      ++j;
    }
    if (j == sn && i + j < utf8_n && utf8[i + j] == 0) {
      return static_cast<int>(i);
    }
    while (i < utf8_n && utf8[i] != 0) {
      ++i;
    }
    if (i < utf8_n) {
      ++i;  // skip NUL, next string start
    }
  }
  return -1;
}

// Compare kBlock uint16 values at a and b.
inline bool eq_u16n(const uint16_t* a, const uint16_t* b, size_t n) noexcept {
  for (size_t i = 0; i < n; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool expand(const uint8_t* dense, size_t dense_n,
                                 uint8_t* working) noexcept {
  if (dense_n < 16) {
    return false;
  }
  const uint8_t* p = dense;
  const uint8_t* end = dense + dense_n;
  const uint8_t* parts[4];
  size_t plen[4];
  for (int i = 0; i < 4; ++i) {
    if (p + 4 > end) {
      return false;
    }
    const uint32_t n = static_cast<uint32_t>(p[0]) |
                       (static_cast<uint32_t>(p[1]) << 8) |
                       (static_cast<uint32_t>(p[2]) << 16) |
                       (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
    if (p + n > end) {
      return false;
    }
    parts[i] = p;
    plen[i] = n;
    p += n;
  }

  constexpr uint16_t kValid = 0xFFFF;
  constexpr uint16_t kDis = 0xFFFE;
  constexpr uint16_t kIgn = 0;
  constexpr uint16_t kBool = 0x8000;
  constexpr uint32_t kBlock = 64;
  constexpr uint32_t kNPages = 0x1100;
  // low_range_end is emitted by pack_tables for both v1 (optional) and v2.
  const uint32_t low = static_cast<uint32_t>(table_blob::low_range_end);

  // Allocate scratch: flat[low] + stage tables written directly into working.
  uint16_t* flat = new (std::nothrow) uint16_t[low];
  if (!flat) {
    return false;
  }
  for (uint32_t i = 0; i < low; ++i) {
    flat[i] = kDis;
  }

  // utf8 built in working region later; temp buffer first.
  uint8_t* utf8 =
      new (std::nothrow) uint8_t[table_blob::count_idna_utf8_mappings];
  if (!utf8) {
    delete[] flat;
    return false;
  }
  size_t utf8_n = 1;
  utf8[0] = 0;

  const uint8_t* mp = parts[0];
  const uint8_t* mend = mp + plen[0];
  uint32_t cp = 0;
  while (mp < mend) {
    uint32_t d = 0, ln = 0;
    if (!read_varint(mp, mend, d) || !read_varint(mp, mend, ln) || mp >= mend) {
      delete[] flat;
      delete[] utf8;
      return false;
    }
    const uint8_t kind = *mp++;
    cp += d;
    if (kind == 3) {
      if (mp >= mend) {
        delete[] flat;
        delete[] utf8;
        return false;
      }
      const uint8_t n = *mp++;
      if (mp + n > mend) {
        delete[] flat;
        delete[] utf8;
        return false;
      }
      int idx = find_utf8(utf8, utf8_n, mp, n);
      if (idx < 0) {
        if (utf8_n + n + 1 > table_blob::count_idna_utf8_mappings) {
          delete[] flat;
          delete[] utf8;
          return false;
        }
        idx = static_cast<int>(utf8_n);
        std::memcpy(utf8 + utf8_n, mp, n);
        utf8_n += n;
        utf8[utf8_n++] = 0;
      }
      mp += n;
      if (cp < low) {
        flat[cp] = static_cast<uint16_t>(idx);
      }
      cp += 1;
    } else {
      uint16_t val = kDis;
      if (kind == 0) {
        val = kValid;
      } else if (kind == 2) {
        val = kIgn;
      } else if (kind != 1) {
        delete[] flat;
        delete[] utf8;
        return false;
      }
      for (uint32_t c = cp; c < cp + ln + 1 && c < low; ++c) {
        flat[c] = val;
      }
      cp += ln + 1;
    }
  }
  if (utf8_n != table_blob::count_idna_utf8_mappings) {
    delete[] flat;
    delete[] utf8;
    return false;
  }

  // two-level into temp then copy
  uint16_t* stage1 = new (std::nothrow) uint16_t[table_blob::count_idna_stage1];
  uint16_t* mixed = new (std::nothrow) uint16_t[table_blob::count_idna_stage2];
  uint64_t* bools =
      new (std::nothrow) uint64_t[table_blob::count_idna_bool_blocks];
  if (!stage1 || !mixed || !bools) {
    delete[] flat;
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }
  size_t mixed_n = 0;
  size_t bool_n = 0;
  const uint32_t n_blocks = (low + kBlock - 1) / kBlock;
  if (n_blocks != table_blob::count_idna_stage1) {
    delete[] flat;
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }
  for (uint32_t bi = 0; bi < n_blocks; ++bi) {
    uint16_t block[kBlock];
    bool is_bool = true;
    for (uint32_t j = 0; j < kBlock; ++j) {
      const uint32_t c = bi * kBlock + j;
      block[j] = (c < low) ? flat[c] : kDis;
      if (block[j] != kValid && block[j] != kDis) {
        is_bool = false;
      }
    }
    if (is_bool) {
      uint64_t bits = 0;
      for (uint32_t j = 0; j < kBlock; ++j) {
        if (block[j] == kValid) {
          bits |= (uint64_t{1} << j);
        }
      }
      size_t idx = 0;
      while (idx < bool_n && bools[idx] != bits) {
        ++idx;
      }
      if (idx == bool_n) {
        if (bool_n >= table_blob::count_idna_bool_blocks) {
          delete[] flat;
          delete[] utf8;
          delete[] stage1;
          delete[] mixed;
          delete[] bools;
          return false;
        }
        bools[bool_n++] = bits;
      }
      stage1[bi] = static_cast<uint16_t>(kBool | idx);
    } else {
      size_t base = 0;
      bool found = false;
      for (; base + kBlock <= mixed_n; base += kBlock) {
        if (eq_u16n(mixed + base, block, kBlock)) {
          found = true;
          break;
        }
      }
      if (!found) {
        if (mixed_n + kBlock > table_blob::count_idna_stage2) {
          delete[] flat;
          delete[] utf8;
          delete[] stage1;
          delete[] mixed;
          delete[] bools;
          return false;
        }
        base = mixed_n;
        for (uint32_t j = 0; j < kBlock; ++j) {
          mixed[mixed_n++] = block[j];
        }
      }
      stage1[bi] = static_cast<uint16_t>(base);
    }
  }
  delete[] flat;
  if (mixed_n != table_blob::count_idna_stage2 ||
      bool_n != table_blob::count_idna_bool_blocks) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }

  // --- decomp sparse ---
  // Max entries bounded by dense; use parallel arrays.
  constexpr size_t kMaxDecomp = 4096;
  uint32_t d_cp[kMaxDecomp];
  uint8_t d_len[kMaxDecomp];
  uint32_t d_off[kMaxDecomp];
  uint32_t decomp_data[2048];
  size_t n_decomp = 0;
  size_t decomp_data_n = 0;
  const uint8_t* dp = parts[1];
  const uint8_t* dend = dp + plen[1];
  uint32_t prev = 0;
  while (dp < dend) {
    uint32_t d = 0;
    if (!read_varint(dp, dend, d) || dp >= dend) {
      delete[] utf8;
      delete[] stage1;
      delete[] mixed;
      delete[] bools;
      return false;
    }
    prev += d;
    const uint8_t n = *dp++;
    if (n_decomp >= kMaxDecomp || decomp_data_n + n > 2048) {
      delete[] utf8;
      delete[] stage1;
      delete[] mixed;
      delete[] bools;
      return false;
    }
    d_cp[n_decomp] = prev;
    d_len[n_decomp] = n;
    d_off[n_decomp] = static_cast<uint32_t>(decomp_data_n);
    for (uint8_t k = 0; k < n; ++k) {
      uint32_t v = 0;
      if (!read_u24(dp, dend, v)) {
        delete[] utf8;
        delete[] stage1;
        delete[] mixed;
        delete[] bools;
        return false;
      }
      decomp_data[decomp_data_n++] = v;
    }
    ++n_decomp;
  }
  if (decomp_data_n != table_blob::count_decomposition_data) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }

  // ccc sparse
  constexpr size_t kMaxCcc = 2048;
  uint32_t c_cp[kMaxCcc];
  uint8_t c_val[kMaxCcc];
  size_t n_ccc = 0;
  const uint8_t* cp_p = parts[2];
  const uint8_t* cend = cp_p + plen[2];
  prev = 0;
  while (cp_p < cend) {
    uint32_t d = 0;
    if (!read_varint(cp_p, cend, d) || cp_p >= cend) {
      delete[] utf8;
      delete[] stage1;
      delete[] mixed;
      delete[] bools;
      return false;
    }
    prev += d;
    if (n_ccc >= kMaxCcc) {
      delete[] utf8;
      delete[] stage1;
      delete[] mixed;
      delete[] bools;
      return false;
    }
    c_cp[n_ccc] = prev;
    c_val[n_ccc] = *cp_p++;
    ++n_ccc;
  }

  auto find_decomp = [&](uint32_t c, uint32_t& off, uint8_t& len) -> bool {
    for (size_t i = 0; i < n_decomp; ++i) {
      if (d_cp[i] == c) {
        off = d_off[i];
        len = d_len[i];
        return true;
      }
    }
    return false;
  };
  auto find_ccc = [&](uint32_t c) -> uint8_t {
    for (size_t i = 0; i < n_ccc; ++i) {
      if (c_cp[i] == c) {
        return c_val[i];
      }
    }
    return 0;
  };

  // decomp multi-stage blocks
  constexpr size_t kMaxDBlocks = 64;
  uint16_t dblocks[kMaxDBlocks][257];
  size_t n_dblocks = 1;
  std::memset(dblocks[0], 0, sizeof(dblocks[0]));
  uint8_t dindex[kNPages];
  std::memset(dindex, 0, sizeof(dindex));

  for (uint32_t page = 0; page < kNPages; ++page) {
    uint16_t m[257];
    uint32_t off_a[256];
    uint8_t len_a[256];
    bool has_a[256];
    bool any = false;
    uint32_t last_end = 0;
    for (uint32_t j = 0; j < 256; ++j) {
      has_a[j] = find_decomp(page * 256 + j, off_a[j], len_a[j]);
      if (has_a[j]) {
        any = true;
        last_end = off_a[j] + len_a[j];
      }
    }
    if (!any) {
      dindex[page] = 0;
      continue;
    }
    m[256] = static_cast<uint16_t>(last_end << 2);
    for (int j = 255; j >= 0; --j) {
      if (has_a[j]) {
        m[j] = static_cast<uint16_t>(off_a[j] << 2);
        m[j + 1] = static_cast<uint16_t>((off_a[j] + len_a[j]) << 2);
      } else {
        m[j] = m[j + 1];
      }
    }
    size_t bi = 0;
    for (; bi < n_dblocks; ++bi) {
      if (eq_u16n(dblocks[bi], m, 257)) {
        break;
      }
    }
    if (bi == n_dblocks) {
      if (n_dblocks >= kMaxDBlocks) {
        delete[] utf8;
        delete[] stage1;
        delete[] mixed;
        delete[] bools;
        return false;
      }
      std::memcpy(dblocks[n_dblocks], m, sizeof(m));
      bi = n_dblocks++;
    }
    dindex[page] = static_cast<uint8_t>(bi);
  }
  if (n_dblocks != table_blob::decomposition_block_rows) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }

  // ccc multi-stage
  constexpr size_t kMaxCBlocks = 128;
  uint8_t cblocks[kMaxCBlocks][256];
  size_t n_cblocks = 1;
  std::memset(cblocks[0], 0, 256);
  uint8_t cindex[kNPages];
  std::memset(cindex, 0, sizeof(cindex));
  for (uint32_t page = 0; page < kNPages; ++page) {
    uint8_t row[256];
    bool anyv = false;
    for (uint32_t j = 0; j < 256; ++j) {
      row[j] = find_ccc(page * 256 + j);
      if (row[j]) {
        anyv = true;
      }
    }
    if (!anyv) {
      cindex[page] = 0;
      continue;
    }
    size_t bi = 0;
    for (; bi < n_cblocks; ++bi) {
      if (std::memcmp(cblocks[bi], row, 256) == 0) {
        break;
      }
    }
    if (bi == n_cblocks) {
      if (n_cblocks >= kMaxCBlocks) {
        delete[] utf8;
        delete[] stage1;
        delete[] mixed;
        delete[] bools;
        return false;
      }
      std::memcpy(cblocks[n_cblocks], row, 256);
      bi = n_cblocks++;
    }
    cindex[page] = static_cast<uint8_t>(bi);
  }
  if (n_cblocks != table_blob::ccc_block_rows) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }

  // composition pairs from length-2 decomps
  constexpr size_t kMaxPairs = 512;
  uint32_t pair_starter[kMaxPairs];
  uint32_t pair_trail[kMaxPairs];
  uint32_t pair_result[kMaxPairs];
  size_t n_pairs = 0;
  for (size_t i = 0; i < n_decomp; ++i) {
    if (d_len[i] != 2) {
      continue;
    }
    const uint32_t a = decomp_data[d_off[i]];
    const uint32_t b = decomp_data[d_off[i] + 1];
    if (find_ccc(a) != 0) {
      continue;
    }
    if (n_pairs >= kMaxPairs) {
      delete[] utf8;
      delete[] stage1;
      delete[] mixed;
      delete[] bools;
      return false;
    }
    pair_starter[n_pairs] = a;
    pair_trail[n_pairs] = b;
    pair_result[n_pairs] = d_cp[i];
    ++n_pairs;
  }
  // sort pairs by starter then trail (insertion sort; n small)
  for (size_t i = 1; i < n_pairs; ++i) {
    size_t j = i;
    while (j > 0 && (pair_starter[j - 1] > pair_starter[j] ||
                     (pair_starter[j - 1] == pair_starter[j] &&
                      pair_trail[j - 1] > pair_trail[j]))) {
      // swap j-1, j
      uint32_t t;
      t = pair_starter[j - 1];
      pair_starter[j - 1] = pair_starter[j];
      pair_starter[j] = t;
      t = pair_trail[j - 1];
      pair_trail[j - 1] = pair_trail[j];
      pair_trail[j] = t;
      t = pair_result[j - 1];
      pair_result[j - 1] = pair_result[j];
      pair_result[j] = t;
      --j;
    }
  }

  uint32_t composition_data[1024];
  composition_data[0] = 0;
  size_t comp_data_n = 1;
  // starter_range via parallel arrays of starters that have pairs
  uint32_t sr_cp[512];
  uint16_t sr_left[512];
  uint16_t sr_right[512];
  size_t n_sr = 0;
  for (uint32_t page = 0; page < kNPages; ++page) {
    for (uint32_t c = page * 256; c < (page + 1) * 256; ++c) {
      // count pairs for c
      size_t first = n_pairs;
      size_t last = n_pairs;
      for (size_t i = 0; i < n_pairs; ++i) {
        if (pair_starter[i] == c) {
          if (first == n_pairs) {
            first = i;
          }
          last = i + 1;
        }
      }
      if (first == n_pairs) {
        continue;
      }
      if (n_sr >= 512 || comp_data_n + 2 * (last - first) > 1024) {
        delete[] utf8;
        delete[] stage1;
        delete[] mixed;
        delete[] bools;
        return false;
      }
      sr_cp[n_sr] = c;
      sr_left[n_sr] = static_cast<uint16_t>(comp_data_n);
      for (size_t i = first; i < last; ++i) {
        composition_data[comp_data_n++] = pair_trail[i];
        composition_data[comp_data_n++] = pair_result[i];
      }
      sr_right[n_sr] = static_cast<uint16_t>(comp_data_n);
      ++n_sr;
    }
  }
  if (comp_data_n != table_blob::count_composition_data) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }

  auto find_sr = [&](uint32_t c, uint16_t& left, uint16_t& right) -> bool {
    for (size_t i = 0; i < n_sr; ++i) {
      if (sr_cp[i] == c) {
        left = sr_left[i];
        right = sr_right[i];
        return true;
      }
    }
    return false;
  };

  constexpr size_t kMaxPBlocks = 64;
  uint16_t pblocks[kMaxPBlocks][257];
  size_t n_pblocks = 1;
  for (int i = 0; i < 257; ++i) {
    pblocks[0][i] = 1;
  }
  uint8_t pindex[kNPages];
  std::memset(pindex, 0, sizeof(pindex));
  for (uint32_t page = 0; page < kNPages; ++page) {
    uint32_t items_j[64];
    uint16_t items_l[64];
    uint16_t items_r[64];
    size_t ni = 0;
    for (uint32_t j = 0; j < 256; ++j) {
      uint16_t left = 0, right = 0;
      if (find_sr(page * 256 + j, left, right)) {
        if (ni >= 64) {
          delete[] utf8;
          delete[] stage1;
          delete[] mixed;
          delete[] bools;
          return false;
        }
        items_j[ni] = j;
        items_l[ni] = left;
        items_r[ni] = right;
        ++ni;
      }
    }
    if (ni == 0) {
      pindex[page] = 0;
      continue;
    }
    uint16_t m[257];
    for (int i = 0; i < 257; ++i) {
      m[i] = 1;
    }
    const uint16_t first_left = items_l[0];
    for (uint32_t j = 0; j <= items_j[0]; ++j) {
      m[j] = first_left;
    }
    for (size_t idx = 0; idx < ni; ++idx) {
      const uint32_t j = items_j[idx];
      m[j] = items_l[idx];
      m[j + 1] = items_r[idx];
      const uint32_t next_j = (idx + 1 < ni) ? items_j[idx + 1] : 256;
      for (uint32_t k = j + 1; k <= next_j; ++k) {
        m[k] = items_r[idx];
      }
    }
    size_t bi = 0;
    for (; bi < n_pblocks; ++bi) {
      if (eq_u16n(pblocks[bi], m, 257)) {
        break;
      }
    }
    if (bi == n_pblocks) {
      if (n_pblocks >= kMaxPBlocks) {
        delete[] utf8;
        delete[] stage1;
        delete[] mixed;
        delete[] bools;
        return false;
      }
      std::memcpy(pblocks[n_pblocks], m, sizeof(m));
      bi = n_pblocks++;
    }
    pindex[page] = static_cast<uint8_t>(bi);
  }
  if (n_pblocks != table_blob::composition_block_rows) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }

  // misc ranges
  const uint8_t* rp = parts[3];
  const uint8_t* rend = rp + plen[3];
  auto read_pairs_flat = [&](uint32_t* out, size_t expect) -> bool {
    uint32_t n = 0;
    if (!read_varint(rp, rend, n)) {
      return false;
    }
    if (n * 2 != expect) {
      return false;
    }
    uint32_t prevb = 0;
    for (uint32_t i = 0; i < n; ++i) {
      uint32_t da = 0, db = 0;
      if (!read_varint(rp, rend, da) || !read_varint(rp, rend, db)) {
        return false;
      }
      const uint32_t a = prevb + da;
      const uint32_t b = a + db;
      out[i * 2] = a;
      out[i * 2 + 1] = b;
      prevb = b + 1;
    }
    return true;
  };
  uint32_t id_cont[2836];
  uint32_t id_start[1552];
  uint32_t comb[580];
  if (!read_pairs_flat(id_cont, table_blob::count_id_continue_flat) ||
      !read_pairs_flat(id_start, table_blob::count_id_start_flat) ||
      !read_pairs_flat(comb, table_blob::count_combining_flat)) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }
  uint32_t n_dir = 0;
  if (!read_varint(rp, rend, n_dir) || n_dir != table_blob::count_dir_start) {
    delete[] utf8;
    delete[] stage1;
    delete[] mixed;
    delete[] bools;
    return false;
  }
  uint32_t dir_s[1449];
  uint32_t dir_f[1449];
  uint8_t dir_v[1449];
  prev = 0;
  for (uint32_t i = 0; i < n_dir; ++i) {
    uint32_t ds = 0, df = 0;
    if (!read_varint(rp, rend, ds) || !read_varint(rp, rend, df) ||
        rp >= rend) {
      delete[] utf8;
      delete[] stage1;
      delete[] mixed;
      delete[] bools;
      return false;
    }
    const uint32_t a = prev + ds;
    const uint32_t b = a + df;
    dir_s[i] = a;
    dir_f[i] = b;
    dir_v[i] = *rp++;
    prev = b + 1;
  }

  // write working buffer LE
  std::memset(working, 0, table_blob::uncompressed_size);
  for (size_t i = 0; i < table_blob::count_idna_stage1; ++i) {
    store_u16(working + table_blob::off_idna_stage1 + i * 2, stage1[i]);
  }
  for (size_t i = 0; i < table_blob::count_idna_stage2; ++i) {
    store_u16(working + table_blob::off_idna_stage2 + i * 2, mixed[i]);
  }
  for (size_t i = 0; i < table_blob::count_idna_bool_blocks; ++i) {
    store_u64(working + table_blob::off_idna_bool_blocks + i * 8, bools[i]);
  }
  std::memcpy(working + table_blob::off_idna_utf8_mappings, utf8, utf8_n);
  std::memcpy(working + table_blob::off_decomposition_index, dindex, kNPages);
  for (size_t bi = 0; bi < n_dblocks; ++bi) {
    for (size_t j = 0; j < 257; ++j) {
      store_u16(
          working + table_blob::off_decomposition_block + (bi * 257 + j) * 2,
          dblocks[bi][j]);
    }
  }
  for (size_t i = 0; i < decomp_data_n; ++i) {
    store_u32(working + table_blob::off_decomposition_data + i * 4,
              decomp_data[i]);
  }
  std::memcpy(working + table_blob::off_ccc_index, cindex, kNPages);
  for (size_t bi = 0; bi < n_cblocks; ++bi) {
    std::memcpy(working + table_blob::off_ccc_block + bi * 256, cblocks[bi],
                256);
  }
  std::memcpy(working + table_blob::off_composition_index, pindex, kNPages);
  for (size_t bi = 0; bi < n_pblocks; ++bi) {
    for (size_t j = 0; j < 257; ++j) {
      store_u16(
          working + table_blob::off_composition_block + (bi * 257 + j) * 2,
          pblocks[bi][j]);
    }
  }
  for (size_t i = 0; i < comp_data_n; ++i) {
    store_u32(working + table_blob::off_composition_data + i * 4,
              composition_data[i]);
  }
  for (size_t i = 0; i < table_blob::count_id_continue_flat; ++i) {
    store_u32(working + table_blob::off_id_continue_flat + i * 4, id_cont[i]);
  }
  for (size_t i = 0; i < table_blob::count_id_start_flat; ++i) {
    store_u32(working + table_blob::off_id_start_flat + i * 4, id_start[i]);
  }
  for (size_t i = 0; i < table_blob::count_dir_start; ++i) {
    store_u32(working + table_blob::off_dir_start + i * 4, dir_s[i]);
    store_u32(working + table_blob::off_dir_final + i * 4, dir_f[i]);
  }
  std::memcpy(working + table_blob::off_dir_value, dir_v,
              table_blob::count_dir_value);
  for (size_t i = 0; i < table_blob::count_combining_flat; ++i) {
    store_u32(working + table_blob::off_combining_flat + i * 4, comb[i]);
  }

  delete[] utf8;
  delete[] stage1;
  delete[] mixed;
  delete[] bools;
  return true;
}

}  // namespace dense_exp

[[nodiscard]] inline bool expand_dense_to_working(const uint8_t* dense,
                                                  size_t dense_n,
                                                  uint8_t* working) noexcept {
  return dense_exp::expand(dense, dense_n, working);
}

}  // namespace ada::idna::detail
