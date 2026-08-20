#include "gtest/gtest.h"

#include <string>

#include "idna.h"
#include "ada/idna/limits.h"
#include "../src/table_store.hpp"

TEST(Safety, RejectsOverlongDomainInput) {
  std::string huge(ada::idna::max_domain_input_bytes + 1, 'a');
  huge[huge.size() / 2] = '.';
  EXPECT_TRUE(ada::idna::to_ascii(huge).empty());

  std::string out;
  EXPECT_FALSE(ada::idna::to_ascii(huge, out));
  EXPECT_TRUE(out.empty());

  std::string uni_out;
  EXPECT_FALSE(ada::idna::to_unicode(huge, uni_out));
  EXPECT_TRUE(uni_out.empty());
}

TEST(Safety, RejectsInvalidUtf8) {
  // Overlong / truncated multi-byte sequences.
  const char bad1[] = {'\xc3', '\x00'};  // incomplete 2-byte
  std::string_view s1(bad1, 1);
  EXPECT_TRUE(ada::idna::to_ascii(s1).empty());

  const char bad2[] = {'\xe2', '\x82'};  // incomplete 3-byte
  std::string_view s2(bad2, 2);
  EXPECT_TRUE(ada::idna::to_ascii(s2).empty());

  const char bad3[] = {'\xf0', '\x9f', '\x98'};  // incomplete 4-byte
  std::string_view s3(bad3, 3);
  EXPECT_TRUE(ada::idna::to_ascii(s3).empty());
}

TEST(Safety, ToAsciiBoolOverload) {
  std::string out;
  ASSERT_TRUE(ada::idna::to_ascii("Example.COM", out));
  EXPECT_EQ(out, "example.com");

  ASSERT_TRUE(
      ada::idna::to_ascii("me\xc3\x9f"
                          "agefactory.ca",
                          out));
  EXPECT_EQ(out, "xn--meagefactory-m9a.ca");
}

TEST(Safety, ToAsciiToUnicodeRoundTripAscii) {
  const char* cases[] = {
      "example.com",
      "www.google.com",
      "a.b.c.example.org",
      "xn--mgba3gch31f060k",
  };
  for (const char* c : cases) {
    std::string ascii = ada::idna::to_ascii(c);
    ASSERT_FALSE(ascii.empty()) << c;
    std::string uni = ada::idna::to_unicode(ascii);
    std::string again = ada::idna::to_ascii(uni);
    EXPECT_EQ(again, ascii) << "round-trip failed for " << c;
  }
}

TEST(Safety, ToAsciiToUnicodeRoundTripUnicode) {
  // UTF-8 meßagefactory.ca / münchen.de / faß.de
  const char* cases[] = {
      "me\xc3\x9f"
      "agefactory.ca",
      "m\xc3\xbc"
      "nchen.de",
      "fa\xc3\x9f.de",
  };
  for (const char* c : cases) {
    std::string ascii = ada::idna::to_ascii(c);
    ASSERT_FALSE(ascii.empty()) << c;
    std::string uni = ada::idna::to_unicode(ascii);
    std::string again = ada::idna::to_ascii(uni);
    EXPECT_EQ(again, ascii) << "round-trip failed for " << c;
  }
}

TEST(Safety, TablesReadyAfterUse) {
  (void)ada::idna::to_ascii("a.com");
  // After a successful call that needs tables, init should be ready.
  // (ASCII-only path may skip tables; force a non-ASCII domain.)
  ASSERT_FALSE(ada::idna::to_ascii("me\xc3\x9f"
                                   "agefactory.ca")
                   .empty());
  EXPECT_TRUE(ada::idna::tables_are_ready());
}

TEST(Safety, IsAlreadyNfc) {
  ASSERT_TRUE(ada::idna::ensure_tables());
  std::u32string ascii = U"example.com";
  EXPECT_TRUE(ada::idna::is_already_nfc(ascii));
  // Precomposed e-acute is NFC; e + combining acute is not.
  std::u32string precomposed = {0xE9};
  std::u32string decomposed = {0x65, 0x301};
  EXPECT_TRUE(ada::idna::is_already_nfc(precomposed));
  EXPECT_FALSE(ada::idna::is_already_nfc(decomposed));
}

TEST(Safety, IsAsciiUtf8LengthsAndAlignment) {
  EXPECT_TRUE(ada::idna::is_ascii(std::string_view{}));
  EXPECT_TRUE(ada::idna::is_ascii(""));
  EXPECT_TRUE(ada::idna::is_ascii("a"));
  EXPECT_TRUE(ada::idna::is_ascii("example.com"));
  // 15 / 16 / 17 / 32 / 33 cover SWAR tail, SIMD block, and overlap.
  const std::string a15(15, 'a');
  const std::string a16(16, 'b');
  const std::string a17(17, 'c');
  const std::string a32(32, 'd');
  const std::string a33(33, 'e');
  EXPECT_TRUE(ada::idna::is_ascii(a15));
  EXPECT_TRUE(ada::idna::is_ascii(a16));
  EXPECT_TRUE(ada::idna::is_ascii(a17));
  EXPECT_TRUE(ada::idna::is_ascii(a32));
  EXPECT_TRUE(ada::idna::is_ascii(a33));

  std::string high_first = a16;
  high_first[0] = static_cast<char>(0x80);
  EXPECT_FALSE(ada::idna::is_ascii(high_first));

  std::string high_last16 = a16;
  high_last16[15] = static_cast<char>(0xFF);
  EXPECT_FALSE(ada::idna::is_ascii(high_last16));

  std::string high_mid32 = a32;
  high_mid32[16] = static_cast<char>(0xC3);
  EXPECT_FALSE(ada::idna::is_ascii(high_mid32));

  std::string high_tail = a33;
  high_tail[32] = static_cast<char>(0x80);
  EXPECT_FALSE(ada::idna::is_ascii(high_tail));

  // Unaligned view (offset 1) still uses unaligned SIMD loads.
  std::string padded = std::string(1, 'z') + a32;
  EXPECT_TRUE(ada::idna::is_ascii(std::string_view(padded).substr(1)));
  padded[1 + 20] = static_cast<char>(0x80);
  EXPECT_FALSE(ada::idna::is_ascii(std::string_view(padded).substr(1)));
}

TEST(Safety, IsAsciiUtf32Lengths) {
  EXPECT_TRUE(ada::idna::is_ascii(std::u32string_view{}));
  EXPECT_TRUE(ada::idna::is_ascii(std::u32string_view(U"abc")));
  EXPECT_TRUE(ada::idna::is_ascii(std::u32string(3, U'x')));
  EXPECT_TRUE(ada::idna::is_ascii(std::u32string(4, U'x')));
  EXPECT_TRUE(ada::idna::is_ascii(std::u32string(5, U'x')));
  EXPECT_TRUE(ada::idna::is_ascii(std::u32string(8, U'x')));
  EXPECT_TRUE(ada::idna::is_ascii(std::u32string(9, U'x')));

  std::u32string mixed(8, U'a');
  mixed[0] = 0xE9;
  EXPECT_FALSE(ada::idna::is_ascii(std::u32string_view(mixed)));
  mixed[0] = U'a';
  mixed[3] = 0x80;
  EXPECT_FALSE(ada::idna::is_ascii(std::u32string_view(mixed)));
  mixed[3] = U'a';
  mixed[7] = 0x10FFFF;
  EXPECT_FALSE(ada::idna::is_ascii(std::u32string_view(mixed)));
}

TEST(Safety, ForbiddenDomainCodePointsSimdWidths) {
  EXPECT_FALSE(ada::idna::contains_forbidden_domain_code_point("example.com"));
  EXPECT_FALSE(ada::idna::contains_forbidden_domain_code_point(""));
  const std::string ok16(16, 'a');
  const std::string ok32(32, 'b');
  EXPECT_FALSE(ada::idna::contains_forbidden_domain_code_point(ok16));
  EXPECT_FALSE(ada::idna::contains_forbidden_domain_code_point(ok32));

  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point("foo bar"));
  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point("foo#bar"));
  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point("foo/bar"));
  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point("foo:bar"));
  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point("foo?bar"));
  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point("foo@bar"));

  std::string high = ok16;
  high[10] = static_cast<char>(0x80);
  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point(high));

  std::string hash_at_16 = ok16 + "#tail";
  EXPECT_TRUE(ada::idna::contains_forbidden_domain_code_point(hash_at_16));
}

TEST(Safety, UtfTranscodingAsciiBlocks) {
  const std::string ascii32(32, 'A');
  const size_t n32 =
      ada::idna::utf32_length_from_utf8(ascii32.data(), ascii32.size());
  EXPECT_EQ(n32, 32u);
  std::u32string u32(n32, U'\0');
  EXPECT_EQ(
      ada::idna::utf8_to_utf32(ascii32.data(), ascii32.size(), u32.data()),
      32u);
  EXPECT_EQ(u32, std::u32string(32, U'A'));
  EXPECT_EQ(ada::idna::utf8_length_from_utf32(u32.data(), u32.size()), 32u);
  std::string back(32, '\0');
  EXPECT_EQ(ada::idna::utf32_to_utf8(u32.data(), u32.size(), back.data()), 32u);
  EXPECT_EQ(back, ascii32);

  // Mixed: 16 ASCII + 2-byte UTF-8 + 16 ASCII (hits SIMD then scalar).
  std::string mixed = std::string(16, 'x') + "\xc3\xa9" + std::string(16, 'y');
  const size_t n_mixed =
      ada::idna::utf32_length_from_utf8(mixed.data(), mixed.size());
  EXPECT_EQ(n_mixed, 33u);
  std::u32string u_mixed(n_mixed, U'\0');
  EXPECT_EQ(
      ada::idna::utf8_to_utf32(mixed.data(), mixed.size(), u_mixed.data()),
      33u);
  EXPECT_EQ(u_mixed[16], char32_t(0xE9));
  EXPECT_EQ(ada::idna::utf8_length_from_utf32(u_mixed.data(), u_mixed.size()),
            mixed.size());
  std::string mixed_back(mixed.size(), '\0');
  EXPECT_EQ(ada::idna::utf32_to_utf8(u_mixed.data(), u_mixed.size(),
                                     mixed_back.data()),
            mixed.size());
  EXPECT_EQ(mixed_back, mixed);
}
