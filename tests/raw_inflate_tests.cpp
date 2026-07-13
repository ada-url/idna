// Regression tests for the in-tree raw DEFLATE inflater (RFC 1951).
// These TUs are built under the same pedantic flags as ada's g++-12 CI
// (-Werror -Wextra -Wno-unused-parameter -Wimplicit-fallthrough) so that
// unused-but-set locals in Huffman table construction cannot reappear.
#include "gtest/gtest.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "idna.h"
#include "../src/raw_inflate.hpp"
#include "../src/table_store.hpp"

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
  std::string ascii = ada::idna::to_ascii("me\xc3\x9f"
                                          "agefactory.ca");
  ASSERT_FALSE(ascii.empty());
  EXPECT_EQ(ascii, "xn--meagefactory-m9a.ca");
}
