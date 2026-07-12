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
