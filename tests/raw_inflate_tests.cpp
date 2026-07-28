// Regression tests for the in-tree raw DEFLATE inflater (RFC 1951).
// These TUs are built under the same pedantic flags as ada's g++-12 CI
// (-Werror -Wextra -Wno-unused-parameter -Wimplicit-fallthrough) so that
// unused-but-set locals in Huffman table construction cannot reappear.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../src/raw_inflate.hpp"
#include "../src/table_store.hpp"
#include "gtest/gtest.h"
#include "idna.h"
// detail::bswap_inplace is exercised for BE host conversion regression.

namespace {

// Raw DEFLATE of "hello" (zlib.compressobj(wbits=-15)).
constexpr uint8_t kHelloRaw[] = {203, 72, 205, 201, 201, 7, 0};

}  // namespace

TEST(RawInflate, FixedHuffmanHello) {
  uint8_t out[16]{};
  size_t n = ada::idna::deflate::inflate_raw(kHelloRaw, sizeof(kHelloRaw), out,
                                             sizeof(out));
  ASSERT_EQ(n, 5u);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(out), n), "hello");
}

TEST(RawInflate, RejectsTruncatedInput) {
  uint8_t out[16]{};
  // Only the first byte of the hello stream.
  EXPECT_EQ(ada::idna::deflate::inflate_raw(kHelloRaw, 1, out, sizeof(out)),
            0u);
}

TEST(RawInflate, RejectsTinyOutputBuffer) {
  uint8_t out[2]{};
  EXPECT_EQ(
      ada::idna::deflate::inflate_raw(kHelloRaw, sizeof(kHelloRaw), out, 2),
      0u);
}

TEST(RawInflate, HuffBuildFixedLitLen) {
  ada::idna::deflate::Huff h;
  uint8_t lens[288];
  for (int i = 0; i <= 143; i++) lens[i] = 8;
  for (int i = 144; i <= 255; i++) lens[i] = 9;
  for (int i = 256; i <= 279; i++) lens[i] = 7;
  for (int i = 280; i <= 287; i++) lens[i] = 8;
  ASSERT_TRUE(h.build(lens, 288));
  EXPECT_GT(h.max_bits, 0);
  EXPECT_EQ(h.counts[8], 144 + 8);  // 0-143 and 280-287
  EXPECT_EQ(h.counts[9], 112);      // 144-255
  EXPECT_EQ(h.counts[7], 24);       // 256-279
}

TEST(RawInflate, TableBlobInflatesViaEnsureTables) {
  ASSERT_TRUE(ada::idna::ensure_tables());
  EXPECT_TRUE(ada::idna::tables_are_ready());
  // Non-ASCII path exercises mapping tables produced by inflate.
  std::string ascii = ada::idna::to_ascii(
      "me\xc3\x9f"
      "agefactory.ca");
  ASSERT_FALSE(ascii.empty());
  EXPECT_EQ(ascii, "xn--meagefactory-m9a.ca");
}

// Regression: table_blob is little-endian. On big-endian hosts (s390x) we must
// bswap multi-byte sections after inflate or IDNA lookups fail.
TEST(RawInflate, LittleEndianWordsConvertToHost) {
  // LE memory image of 0x1234 and 0xABCD.
  uint8_t le_bytes[4] = {0x34, 0x12, 0xCD, 0xAB};
  uint16_t words[2];
  std::memcpy(words, le_bytes, sizeof(words));
  ada::idna::detail::bswap_inplace(words, 2);
  EXPECT_EQ(words[0], 0x1234);
  EXPECT_EQ(words[1], 0xABCD);

  uint8_t le32[4] = {0x78, 0x56, 0x34, 0x12};
  uint32_t w32;
  std::memcpy(&w32, le32, 4);
  ada::idna::detail::bswap_inplace(&w32, 1);
  EXPECT_EQ(w32, 0x12345678u);
}

TEST(RawInflate, HostEndianTablesServeUnicodeIdna) {
  // Multi-byte table-backed mapping / normalization / identifier ranges.
  // (UTS #46 non-transitional: U+00DF is not mapped to "ss" alone as a label.)
  EXPECT_EQ(ada::idna::to_ascii("\xc3\x9f.com"), "xn--zca.com");
  EXPECT_EQ(ada::idna::to_ascii("m\xc3\xbc"
                                "nchen.de"),
            "xn--mnchen-3ya.de");
  EXPECT_EQ(ada::idna::to_ascii("me\xc3\x9f"
                                "agefactory.ca"),
            "xn--meagefactory-m9a.ca");
  EXPECT_TRUE(ada::idna::valid_name_code_point(U'a', true));
  EXPECT_TRUE(ada::idna::valid_name_code_point(U'\u03b1', true));  // alpha
}

namespace {

// Mirror of scripts/pack_tables.py _filter_section for multi-byte kinds:
// little-endian delta, then byte-plane split. Used only to *build* the
// filtered fixture; the code under test is detail::unfilter_table_blob.
void filter_section_le(uint8_t* data, size_t count, size_t width, bool delta) {
  std::vector<uint8_t> tmp(count * width);
  if (delta && width == 2) {
    uint16_t prev = 0;
    for (size_t i = 0; i < count; ++i) {
      const uint16_t v = static_cast<uint16_t>(
          data[i * 2] | (static_cast<uint16_t>(data[i * 2 + 1]) << 8));
      const uint16_t d = static_cast<uint16_t>(v - prev);
      prev = v;
      data[i * 2] = static_cast<uint8_t>(d & 0xffu);
      data[i * 2 + 1] = static_cast<uint8_t>((d >> 8) & 0xffu);
    }
  } else if (delta && width == 4) {
    uint32_t prev = 0;
    for (size_t i = 0; i < count; ++i) {
      const uint32_t v = static_cast<uint32_t>(data[i * 4]) |
                         (static_cast<uint32_t>(data[i * 4 + 1]) << 8) |
                         (static_cast<uint32_t>(data[i * 4 + 2]) << 16) |
                         (static_cast<uint32_t>(data[i * 4 + 3]) << 24);
      const uint32_t d = v - prev;
      prev = v;
      data[i * 4] = static_cast<uint8_t>(d & 0xffu);
      data[i * 4 + 1] = static_cast<uint8_t>((d >> 8) & 0xffu);
      data[i * 4 + 2] = static_cast<uint8_t>((d >> 16) & 0xffu);
      data[i * 4 + 3] = static_cast<uint8_t>((d >> 24) & 0xffu);
    }
  }
  // byte-plane split: out[b*count + i] = data[i*width + b]
  for (size_t i = 0; i < count; ++i) {
    for (size_t b = 0; b < width; ++b) {
      tmp[b * count + i] = data[i * width + b];
    }
  }
  std::memcpy(data, tmp.data(), tmp.size());
}

void filter_blob_like_pack_tables(uint8_t* buffer) {
  namespace tb = ada::idna::table_blob;
  struct Sec {
    size_t off;
    size_t count;
    size_t width;
    bool delta;
  };
  const Sec secs[] = {
      {tb::off_idna_stage1, tb::count_idna_stage1, 2, true},
      {tb::off_idna_stage2, tb::count_idna_stage2, 2, true},
      {tb::off_idna_bool_blocks, tb::count_idna_bool_blocks, 8, false},
      {tb::off_decomposition_block, tb::count_decomposition_block, 2, true},
      {tb::off_decomposition_data, tb::count_decomposition_data, 4, true},
      {tb::off_composition_block, tb::count_composition_block, 2, true},
      {tb::off_composition_data, tb::count_composition_data, 4, true},
      {tb::off_id_continue_flat, tb::count_id_continue_flat, 4, true},
      {tb::off_id_start_flat, tb::count_id_start_flat, 4, true},
      {tb::off_dir_start, tb::count_dir_start, 4, true},
      {tb::off_dir_final, tb::count_dir_final, 4, true},
      {tb::off_combining_flat, tb::count_combining_flat, 4, true},
  };
  for (const Sec& s : secs) {
    filter_section_le(buffer + s.off, s.count, s.width, s.delta);
  }
}

}  // namespace

// Drives shipped detail::unfilter_table_blob on a full-size buffer: fill
// multi-byte sections with distinctive LE patterns, filter like pack_tables.py,
// unfilter, and require byte-identical recovery (locks BE-safe LE delta
// decode).
TEST(RawInflate, UnfilterTableBlobRoundTrip) {
  namespace tb = ada::idna::table_blob;
  const size_t n = tb::uncompressed_size;
  std::vector<uint8_t> plain(n, 0);

  // Distinctive LE patterns in u16 stage1 and u32 decomp_data.
  for (size_t i = 0; i < tb::count_idna_stage1; ++i) {
    const uint16_t v = static_cast<uint16_t>((i * 17u + 3u) & 0xffffu);
    plain[tb::off_idna_stage1 + i * 2] = static_cast<uint8_t>(v & 0xffu);
    plain[tb::off_idna_stage1 + i * 2 + 1] =
        static_cast<uint8_t>((v >> 8) & 0xffu);
  }
  for (size_t i = 0; i < tb::count_decomposition_data; ++i) {
    const uint32_t v = static_cast<uint32_t>(i * 65537u + 0x00ABCDEFu);
    const size_t o = tb::off_decomposition_data + i * 4;
    plain[o] = static_cast<uint8_t>(v & 0xffu);
    plain[o + 1] = static_cast<uint8_t>((v >> 8) & 0xffu);
    plain[o + 2] = static_cast<uint8_t>((v >> 16) & 0xffu);
    plain[o + 3] = static_cast<uint8_t>((v >> 24) & 0xffu);
  }
  // Non-zero u64 bool plane so split-only path is covered.
  for (size_t i = 0; i < tb::count_idna_bool_blocks; ++i) {
    const size_t o = tb::off_idna_bool_blocks + i * 8;
    for (size_t b = 0; b < 8; ++b) {
      plain[o + b] = static_cast<uint8_t>(0xA0u + b + static_cast<uint8_t>(i));
    }
  }

  std::vector<uint8_t> filtered = plain;
  filter_blob_like_pack_tables(filtered.data());
  ASSERT_NE(filtered, plain);  // filter must actually change multi-byte data

  ASSERT_TRUE(ada::idna::detail::unfilter_table_blob(filtered.data()));
  EXPECT_EQ(filtered, plain);
}
