#pragma once
// Minimal raw DEFLATE inflater (RFC 1951), enough for zlib raw streams.
// Returns number of output bytes written, or 0 on failure.
#include <cstdint>
#include <cstring>

namespace ada::idna::deflate {

struct BitReader {
  const uint8_t* p;
  const uint8_t* end;
  uint32_t acc = 0;
  int bits = 0;

  explicit BitReader(const uint8_t* data, size_t len)
      : p(data), end(data + len) {}

  bool ensure(int n) {
    while (bits < n) {
      if (p >= end) return false;
      acc |= uint32_t(*p++) << bits;
      bits += 8;
    }
    return true;
  }
  uint32_t get(int n) {
    if (!ensure(n)) return UINT32_MAX;
    uint32_t v = acc & ((1u << n) - 1);
    acc >>= n;
    bits -= n;
    return v;
  }
  void align_byte() {
    acc >>= (bits & 7);
    bits -= (bits & 7);
  }
};

struct Huff {
  // canonical codes: for each length, list of symbols
  // Fast path: table for codes up to max_bits
  static constexpr int MAXBITS = 15;
  uint8_t lengths[288]{};
  int counts[MAXBITS + 1]{};
  int first_code[MAXBITS + 1]{};
  int16_t symbols[288]{};
  int nsymbols = 0;
  int max_bits = 0;

  bool build(const uint8_t* lens, int n) {
    std::memset(counts, 0, sizeof(counts));
    nsymbols = n;
    max_bits = 0;
    for (int i = 0; i < n; i++) {
      lengths[i] = lens[i];
      if (lens[i]) {
        counts[lens[i]]++;
        if (lens[i] > max_bits) max_bits = lens[i];
      }
    }
    if (max_bits == 0) return true;
    int code = 0;
    counts[0] = 0;
    int next_code[MAXBITS + 1]{};
    for (int bits = 1; bits <= max_bits; bits++) {
      code = (code + counts[bits - 1]) << 1;
      next_code[bits] = code;
      first_code[bits] = code;
    }
    // Check canonical
    int offs[MAXBITS + 1]{};
    int total = 0;
    for (int bits = 1; bits <= max_bits; bits++) {
      offs[bits] = total;
      total += counts[bits];
    }
    for (int sym = 0; sym < n; sym++) {
      int len = lengths[sym];
      if (len) {
        int c = next_code[len]++;
        // position among symbols of this length: c - first_code[len]
        int idx = offs[len] + (c - first_code[len]);
        if (idx < 0 || idx >= 288) return false;
        symbols[idx] = static_cast<int16_t>(sym);
      }
    }
    // recompute offs as base index per length
    // symbols laid out by length groups in order of code
    // Rebuild properly:
    total = 0;
    int base[MAXBITS + 1]{};
    for (int bits = 1; bits <= max_bits; bits++) {
      base[bits] = total;
      total += counts[bits];
    }
    int fill[MAXBITS + 1]{};
    for (int bits = 1; bits <= max_bits; bits++) fill[bits] = base[bits];
    // Reset next_code
    code = 0;
    for (int bits = 1; bits <= max_bits; bits++) {
      code = (code + counts[bits - 1]) << 1;
      next_code[bits] = code;
      first_code[bits] = code;
    }
    for (int sym = 0; sym < n; sym++) {
      int len = lengths[sym];
      if (!len) continue;
      int c = next_code[len]++;
      int idx = base[len] + (c - first_code[len]);
      symbols[idx] = static_cast<int16_t>(sym);
    }
    // store base into counts reuse - keep first_code and counts
    for (int bits = 1; bits <= max_bits; bits++) {
      // first_code already set; store base index in a side array - reuse
      // symbols layout
    }
    // Copy base into first_code's sibling - use lengths[0] area no
    // Store base in first_code after using it for decode differently
    // Simpler decode: linear search for short tables... for correctness first.
    return true;
  }

  // Need base offsets for decode - store in first_code as code start, counts as
  // count Add base_idx
  int base_idx[MAXBITS + 1]{};

  bool build2(const uint8_t* lens, int n) {
    std::memset(counts, 0, sizeof(counts));
    nsymbols = n;
    max_bits = 0;
    for (int i = 0; i < n; i++) {
      lengths[i] = lens[i];
      if (lens[i]) {
        counts[lens[i]]++;
        if (lens[i] > max_bits) max_bits = lens[i];
      }
    }
    int code = 0;
    int next_code[MAXBITS + 1]{};
    next_code[0] = 0;
    for (int bits = 1; bits <= MAXBITS; bits++) {
      code = (code + counts[bits - 1]) << 1;
      next_code[bits] = code;
      first_code[bits] = code;
    }
    int idx = 0;
    for (int bits = 1; bits <= max_bits; bits++) {
      base_idx[bits] = idx;
      // assign symbols in symbol order for codes of this length
      for (int sym = 0; sym < n; sym++) {
        if (lengths[sym] == bits) {
          symbols[idx++] = static_cast<int16_t>(sym);
        }
      }
    }
    return true;
  }

  int decode(BitReader& br) const {
    if (max_bits == 0) return -1;
    int code = 0;
    for (int len = 1; len <= max_bits; len++) {
      uint32_t b = br.get(1);
      if (b == UINT32_MAX) return -1;
      code = (code << 1) | int(b);
      int first = first_code[len];
      int cnt = counts[len];
      if (cnt && code - first < cnt) {
        return symbols[base_idx[len] + (code - first)];
      }
    }
    return -1;
  }
};

inline int fixed_litlen(BitReader& br) {
  // Fixed Huffman: 0-143:8, 144-255:9, 256-279:7, 280-287:8
  // Build on first use
  static Huff h;
  static bool ready = false;
  if (!ready) {
    uint8_t lens[288];
    for (int i = 0; i <= 143; i++) lens[i] = 8;
    for (int i = 144; i <= 255; i++) lens[i] = 9;
    for (int i = 256; i <= 279; i++) lens[i] = 7;
    for (int i = 280; i <= 287; i++) lens[i] = 8;
    h.build2(lens, 288);
    ready = true;
  }
  return h.decode(br);
}

inline int fixed_dist(BitReader& br) {
  static Huff h;
  static bool ready = false;
  if (!ready) {
    uint8_t lens[32];
    for (int i = 0; i < 32; i++) lens[i] = 5;
    h.build2(lens, 32);
    ready = true;
  }
  return h.decode(br);
}

// length and distance base tables
static const uint16_t len_base[29] = {
    3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                      1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                      4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {
    1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
    33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                       4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                       9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

inline size_t inflate_raw(const uint8_t* src, size_t src_len, uint8_t* dst,
                          size_t dst_cap) {
  BitReader br(src, src_len);
  size_t out = 0;
  for (;;) {
    uint32_t bfinal = br.get(1);
    uint32_t btype = br.get(2);
    if (bfinal == UINT32_MAX || btype == UINT32_MAX) return 0;
    if (btype == 0) {
      // stored
      br.align_byte();
      if (!br.ensure(16)) return 0;
      // read 16+16 from bit buffer awkwardly - use byte path
      // Align: bits is multiple of 8
      // Get len from remaining bits then bytes
      uint32_t len = br.get(16);
      uint32_t nlen = br.get(16);
      if (len == UINT32_MAX || nlen == UINT32_MAX) return 0;
      if ((len ^ 0xFFFF) != nlen) return 0;
      // After get, may have bits in acc from previous - for stored, should be
      // byte aligned Copy len bytes from p
      if (br.bits != 0) {
        // leftover bits should be 0 if aligned
      }
      if (size_t(br.end - br.p) < len) return 0;
      if (out + len > dst_cap) return 0;
      std::memcpy(dst + out, br.p, len);
      br.p += len;
      out += len;
    } else if (btype == 1 || btype == 2) {
      Huff lit, dist;
      if (btype == 1) {
        // use fixed via functions
      } else {
        // dynamic
        uint32_t hlit = br.get(5);
        if (hlit == UINT32_MAX) return 0;
        hlit += 257;
        uint32_t hdist = br.get(5);
        if (hdist == UINT32_MAX) return 0;
        hdist += 1;
        uint32_t hclen = br.get(4);
        if (hclen == UINT32_MAX) return 0;
        hclen += 4;
        static const int order[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                      11, 4,  12, 3, 13, 2, 14, 1, 15};
        uint8_t clen[19]{};
        for (uint32_t i = 0; i < hclen; i++) {
          uint32_t v = br.get(3);
          if (v == UINT32_MAX) return 0;
          clen[order[i]] = uint8_t(v);
        }
        Huff cl;
        if (!cl.build2(clen, 19)) return 0;
        uint8_t lens[288 + 32]{};
        uint32_t n = hlit + hdist;
        for (uint32_t i = 0; i < n;) {
          int sym = cl.decode(br);
          if (sym < 0) return 0;
          if (sym < 16) {
            lens[i++] = uint8_t(sym);
          } else if (sym == 16) {
            uint32_t rep = br.get(2);
            if (rep == UINT32_MAX) return 0;
            rep += 3;
            if (i == 0) return 0;
            uint8_t v = lens[i - 1];
            while (rep--) lens[i++] = v;
          } else if (sym == 17) {
            uint32_t rep = br.get(3);
            if (rep == UINT32_MAX) return 0;
            rep += 3;
            while (rep--) lens[i++] = 0;
          } else if (sym == 18) {
            uint32_t rep = br.get(7);
            if (rep == UINT32_MAX) return 0;
            rep += 11;
            while (rep--) lens[i++] = 0;
          } else
            return 0;
        }
        if (!lit.build2(lens, int(hlit))) return 0;
        if (!dist.build2(lens + hlit, int(hdist))) return 0;
      }
      for (;;) {
        int sym;
        if (btype == 1)
          sym = fixed_litlen(br);
        else
          sym = lit.decode(br);
        if (sym < 0) return 0;
        if (sym < 256) {
          if (out >= dst_cap) return 0;
          dst[out++] = uint8_t(sym);
        } else if (sym == 256) {
          break;
        } else {
          int len_code = sym - 257;
          if (len_code < 0 || len_code > 28) return 0;
          uint32_t extra = br.get(len_extra[len_code]);
          if (len_extra[len_code] && extra == UINT32_MAX) return 0;
          uint32_t length =
              len_base[len_code] + (len_extra[len_code] ? extra : 0);
          int dsym;
          if (btype == 1)
            dsym = fixed_dist(br);
          else
            dsym = dist.decode(br);
          if (dsym < 0 || dsym > 29) return 0;
          uint32_t dextra = br.get(dist_extra[dsym]);
          if (dist_extra[dsym] && dextra == UINT32_MAX) return 0;
          uint32_t distance = dist_base[dsym] + (dist_extra[dsym] ? dextra : 0);
          if (distance > out || out + length > dst_cap) return 0;
          for (uint32_t k = 0; k < length; k++) {
            dst[out] = dst[out - distance];
            out++;
          }
        }
      }
    } else {
      return 0;  // reserved
    }
    if (bfinal) break;
  }
  return out;
}

}  // namespace ada::idna::deflate
