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

TEST(Safety, IsAlreadyNfcCompositionExclusions) {
  ASSERT_TRUE(ada::idna::ensure_tables());
  // A canonical decomposition longer than one code point does not make a
  // character NFC: composition exclusions and non-starter decompositions are
  // NFC_Quick_Check=No and must normalize.
  //   U+0958 DEVANAGARI LETTER QA -> U+0915 U+093C (excluded, no recompose)
  std::u32string qa = {0x0958};
  EXPECT_FALSE(ada::idna::is_already_nfc(qa));
  ASSERT_TRUE(ada::idna::normalize(qa));
  EXPECT_EQ(qa, std::u32string({0x0915, 0x093C}));

  //   U+1F71 GREEK SMALL LETTER ALPHA WITH OXIA -> U+03AC: its decomposition
  //   recomposes to a different primary composite, so U+1F71 is not NFC.
  std::u32string oxia = {0x1F71};
  EXPECT_FALSE(ada::idna::is_already_nfc(oxia));
  ASSERT_TRUE(ada::idna::normalize(oxia));
  EXPECT_EQ(oxia, std::u32string({0x03AC}));

  //   U+0344 COMBINING GREEK DIALYTIKA TONOS -> U+0308 U+0301 (non-starter
  //   decomposition, stays decomposed).
  std::u32string dialytika = {0x0344};
  EXPECT_FALSE(ada::idna::is_already_nfc(dialytika));
  ASSERT_TRUE(ada::idna::normalize(dialytika));
  EXPECT_EQ(dialytika, std::u32string({0x0308, 0x0301}));

  // Genuine primary composites stay on the already-NFC fast path.
  EXPECT_TRUE(ada::idna::is_already_nfc(std::u32string({0x00E9})));
  EXPECT_TRUE(ada::idna::is_already_nfc(std::u32string({0x03AC})));
}

TEST(Safety, ComposesHangulLvPlusTrailingJamo) {
  ASSERT_TRUE(ada::idna::ensure_tables());
  // A precomposed LV syllable (SIndex % TCount == 0) followed by a trailing
  // jamo composes to the LVT syllable under NFC.
  //   U+AC00 (가, LV) + U+11A8 (ᆨ, T) -> U+AC01 (각)
  std::u32string lv_plus_t = {0xAC00, 0x11A8};
  EXPECT_FALSE(ada::idna::is_already_nfc(lv_plus_t));
  ASSERT_TRUE(ada::idna::normalize(lv_plus_t));
  EXPECT_EQ(lv_plus_t, std::u32string({0xAC01}));

  //   U+C4D4 (LV) + U+11B6 (T) -> U+C4E3
  std::u32string lv_plus_t2 = {0xC4D4, 0x11B6};
  ASSERT_TRUE(ada::idna::normalize(lv_plus_t2));
  EXPECT_EQ(lv_plus_t2, std::u32string({0xC4E3}));
}
