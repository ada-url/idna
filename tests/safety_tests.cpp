#include "gtest/gtest.h"

#include <string>

#include "idna.h"
#include "ada/idna/limits.h"
#include "../src/table_store.hpp"

TEST(Safety, RejectsOverlongDomainInput) {
  // Non-ASCII input over the limit is rejected by to_ascii.
  std::string huge;
  while (huge.size() <= ada::idna::max_domain_input_bytes) {
    huge += "\xC3\xA0.";  // "\u00E0."
  }
  EXPECT_TRUE(ada::idna::to_ascii(huge).empty());

  std::string out;
  EXPECT_FALSE(ada::idna::to_ascii(huge, out));
  EXPECT_TRUE(out.empty());

  // to_unicode bounds its input regardless (punycode decoding can expand it).
  std::string huge_ascii(ada::idna::max_domain_input_bytes + 1, 'a');
  huge_ascii[huge_ascii.size() / 2] = '.';
  std::string uni_out;
  EXPECT_FALSE(ada::idna::to_unicode(huge_ascii, uni_out));
  EXPECT_TRUE(uni_out.empty());
}

TEST(Safety, ToAsciiAcceptsLongAsciiInput) {
  std::string huge(ada::idna::max_domain_input_bytes + 1, 'A');
  huge[huge.size() / 2] = '.';
  std::string out;
  ASSERT_TRUE(ada::idna::to_ascii(huge, out));
  EXPECT_EQ(out.size(), huge.size());
  EXPECT_EQ(out[0], 'a');
}

TEST(Safety, ToAsciiIdempotentAcrossLimit) {
  // 2049 x "\u00E0." is 6147 bytes in, 16392 bytes out (over the limit).
  std::string host;
  for (int i = 0; i < 2049; i++) host += "\xC3\xA0.";
  std::string ascii;
  ASSERT_TRUE(ada::idna::to_ascii(host, ascii));
  ASSERT_GT(ascii.size(), ada::idna::max_domain_input_bytes);
  std::string again;
  ASSERT_TRUE(ada::idna::to_ascii(ascii, again));
  EXPECT_EQ(again, ascii);
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

TEST(Safety, ComposesUnicode16Compositions) {
  ASSERT_TRUE(ada::idna::ensure_tables());
  // U+1611E U+1611E -> U+16121 GURUNG KHEMA VOWEL SIGN U.
  std::u32string gurung_khema = {0x1611E, 0x1611E};
  EXPECT_FALSE(ada::idna::is_already_nfc(gurung_khema));
  ASSERT_TRUE(ada::idna::normalize(gurung_khema));
  EXPECT_EQ(gurung_khema, std::u32string({0x16121}));
  EXPECT_TRUE(ada::idna::is_already_nfc(std::u32string({0x16121})));

  // Chained: U+16D63 U+16D67 -> U+16D69, then U+16D69 U+16D67 -> U+16D6A
  // KIRAT RAI VOWEL SIGN AU.
  std::u32string kirat_rai = {0x16D63, 0x16D67, 0x16D67};
  ASSERT_TRUE(ada::idna::normalize(kirat_rai));
  EXPECT_EQ(kirat_rai, std::u32string({0x16D6A}));

  // A starter and a combining mark: U+105D2 U+0307 -> U+105C9 TODHRI LETTER EI.
  std::u32string todhri = {0x105D2, 0x0307};
  ASSERT_TRUE(ada::idna::normalize(todhri));
  EXPECT_EQ(todhri, std::u32string({0x105C9}));

  // A decomposition that begins with a code point composing with the starter
  // before it: U+16121 is U+1611E U+1611E, so U+1611E U+16121 is not NFC.
  std::u32string recomposed = {0x1611E, 0x16121};
  EXPECT_FALSE(ada::idna::is_already_nfc(recomposed));
  ASSERT_TRUE(ada::idna::normalize(recomposed));
  EXPECT_EQ(recomposed, std::u32string({0x16121, 0x1611E}));
  EXPECT_TRUE(ada::idna::is_already_nfc(recomposed));

  // U+16D68 is U+16D67 U+16D67: U+16D63 U+16D68 -> U+16D69 U+16D67 -> U+16D6A.
  std::u32string kirat_rai_ai = {0x16D63, 0x16D68};
  EXPECT_FALSE(ada::idna::is_already_nfc(kirat_rai_ai));
  ASSERT_TRUE(ada::idna::normalize(kirat_rai_ai));
  EXPECT_EQ(kirat_rai_ai, std::u32string({0x16D6A}));
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
