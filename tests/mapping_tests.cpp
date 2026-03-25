#include "gtest/gtest.h"
#include "ada/idna/mapping.h"

// ── original smoke tests ───────────────────────────────────────────────────
TEST(mapping_tests, verify) {
  ASSERT_EQ(ada::idna::map(U"asciitwontchange"), U"asciitwontchange");
  ASSERT_EQ(ada::idna::map(U"hasomit\u00adted"), U"hasomitted");
  ASSERT_EQ(ada::idna::map(U"\u00aalla"), U"alla");
}

// ── ASCII fast-path ────────────────────────────────────────────────────────
// All 26 capital letters must map to their lowercase equivalents.
TEST(mapping_tests, ascii_uppercase_to_lowercase) {
  for (char32_t cp = U'A'; cp <= U'Z'; ++cp) {
    std::u32string input(1, cp);
    std::u32string expected(1, cp + 32);
    EXPECT_EQ(ada::idna::map(input), expected)
        << "Failed for U+" << std::hex << (unsigned)cp;
  }
}

// Digits and hyphen must be valid.
TEST(mapping_tests, ascii_digits_and_hyphen_valid) {
  for (char32_t cp = U'0'; cp <= U'9'; ++cp) {
    std::u32string input(1, cp);
    EXPECT_EQ(ada::idna::map(input), input)
        << "Digit U+" << std::hex << (unsigned)cp << " should be valid";
  }
  EXPECT_EQ(ada::idna::map(U"-"), U"-"); // hyphen-minus
}

// ── Ignored code points → empty output ────────────────────────────────────
TEST(mapping_tests, ignored_code_points) {
  // U+00AD SOFT HYPHEN
  EXPECT_EQ(ada::idna::map(U"\U000000AD"), U"");
  // U+034F COMBINING GRAPHEME JOINER
  EXPECT_EQ(ada::idna::map(U"\U0000034F"), U"");
  // U+200B ZERO WIDTH SPACE
  EXPECT_EQ(ada::idna::map(U"\U0000200B"), U"");
  // U+2060 WORD JOINER
  EXPECT_EQ(ada::idna::map(U"\U00002060"), U"");
  // U+FEFF ZERO WIDTH NO-BREAK SPACE (BOM)
  EXPECT_EQ(ada::idna::map(U"\U0000FEFF"), U"");
}

// Ignored code points embedded in a valid string vanish.
TEST(mapping_tests, ignored_embedded_in_valid) {
  // "ab\u034Fcd" → "abcd"  (CGJ between valid chars)
  EXPECT_EQ(ada::idna::map(U"ab\U0000034Fcd"), U"abcd");
  // Soft-hyphen inside a word
  EXPECT_EQ(ada::idna::map(U"foo\U000000ADbar"), U"foobar");
}

// ── Variation selector supplement U+E0100–U+E01EF (all ignored) ───────────
TEST(mapping_tests, variation_selector_supplement_ignored) {
  // First in range
  EXPECT_EQ(ada::idna::map(U"\U000E0100"), U"");
  // Middle of range
  EXPECT_EQ(ada::idna::map(U"\U000E0150"), U"");
  // Last in range
  EXPECT_EQ(ada::idna::map(U"\U000E01EF"), U"");
}

// ── Disallowed code points → empty string returned from map() ─────────────
// Note: U+007F (DEL) is actually VALID in IDNA (range 007B..007F valid NV8).
// The first disallowed code point after ASCII printables is U+0080.
TEST(mapping_tests, disallowed_returns_empty) {
  // U+0080 first C1 control (UCN banned for < 0xA0; use char32_t cast)
  EXPECT_TRUE(ada::idna::map(std::u32string(1, char32_t(0x80))).empty());
  // U+0085 NEXT LINE (another C1 control)
  EXPECT_TRUE(ada::idna::map(std::u32string(1, char32_t(0x85))).empty());
  // Private-use area (U+E000)
  EXPECT_TRUE(ada::idna::map(U"\U0000E000").empty());
  // Surrogates (UCN not allowed for D800-DFFF; use char32_t cast)
  EXPECT_TRUE(ada::idna::map(std::u32string(1, char32_t(0xD800))).empty());
  EXPECT_TRUE(ada::idna::map(std::u32string(1, char32_t(0xDFFF))).empty());
  // Noncharacters
  EXPECT_TRUE(ada::idna::map(U"\U0000FFFE").empty());
  EXPECT_TRUE(ada::idna::map(U"\U0000FFFF").empty());
  // Just after the mid-range valid area
  EXPECT_TRUE(ada::idna::map(U"\U0003347A").empty());
  // Just after the ignored high range
  EXPECT_TRUE(ada::idna::map(U"\U000E01F0").empty());
}

// ── Single-code-point mappings ─────────────────────────────────────────────
TEST(mapping_tests, single_codepoint_mappings) {
  // U+00AA FEMININE ORDINAL INDICATOR → 'a'
  EXPECT_EQ(ada::idna::map(U"\U000000AA"), U"a");
  // U+00BA MASCULINE ORDINAL INDICATOR → 'o'
  EXPECT_EQ(ada::idna::map(U"\U000000BA"), U"o");
  // U+00B5 MICRO SIGN → U+03BC (Greek small mu)
  EXPECT_EQ(ada::idna::map(U"\U000000B5"), U"\U000003BC");
  // U+03A3 GREEK CAPITAL SIGMA → U+03C3
  EXPECT_EQ(ada::idna::map(U"\U000003A3"), U"\U000003C3");
  // U+0410 CYRILLIC CAPITAL A → U+0430
  EXPECT_EQ(ada::idna::map(U"\U00000410"), U"\U00000430");
  // U+FF21 FULLWIDTH LATIN CAPITAL LETTER A → 'a'
  EXPECT_EQ(ada::idna::map(U"\U0000FF21"), U"a");
  // U+FF3A FULLWIDTH LATIN CAPITAL LETTER Z → 'z'
  EXPECT_EQ(ada::idna::map(U"\U0000FF3A"), U"z");
}

// ── Multi-code-point mappings ──────────────────────────────────────────────
TEST(mapping_tests, multi_codepoint_mappings) {
  // U+00B2 SUPERSCRIPT TWO → "2"
  EXPECT_EQ(ada::idna::map(U"\U000000B2"), U"2");
  // U+00B3 SUPERSCRIPT THREE → "3"
  EXPECT_EQ(ada::idna::map(U"\U000000B3"), U"3");
  // U+00BC VULGAR FRACTION ONE QUARTER → "1/4" (U+0031 U+2044 U+0034)
  EXPECT_EQ(ada::idna::map(U"\U000000BC"), (std::u32string{0x31, 0x2044, 0x34}));
  // U+FB00 LATIN SMALL LIGATURE FF → "ff"
  EXPECT_EQ(ada::idna::map(U"\U0000FB00"), U"ff");
  // U+FB01 LATIN SMALL LIGATURE FI → "fi"
  EXPECT_EQ(ada::idna::map(U"\U0000FB01"), U"fi");
  // U+FB03 LATIN SMALL LIGATURE FFI → "ffi"
  EXPECT_EQ(ada::idna::map(U"\U0000FB03"), U"ffi");
  // U+0132 LATIN CAPITAL LIGATURE IJ → "ij"
  EXPECT_EQ(ada::idna::map(U"\U00000132"), U"ij");
  // U+01F1 LATIN CAPITAL LETTER DZ → "dz"
  EXPECT_EQ(ada::idna::map(U"\U000001F1"), U"dz");
  // U+1E9E LATIN CAPITAL LETTER SHARP S → U+00DF
  EXPECT_EQ(ada::idna::map(U"\U00001E9E"), U"\U000000DF");
}

// ── Valid non-ASCII code points pass through unchanged ─────────────────────
TEST(mapping_tests, valid_non_ascii_unchanged) {
  // U+00DF LATIN SMALL LETTER SHARP S (valid in IDNA, not mapped to "ss")
  EXPECT_EQ(ada::idna::map(U"\U000000DF"), U"\U000000DF");
  // U+00E0 LATIN SMALL LETTER A WITH GRAVE
  EXPECT_EQ(ada::idna::map(U"\U000000E0"), U"\U000000E0");
  // U+03B1 GREEK SMALL LETTER ALPHA
  EXPECT_EQ(ada::idna::map(U"\U000003B1"), U"\U000003B1");
  // U+4E2D CJK UNIFIED IDEOGRAPH
  EXPECT_EQ(ada::idna::map(U"\U00004E2D"), U"\U00004E2D");
}

// ── Mid-range boundaries (0x30000–0x3347A) ────────────────────────────────
// These code points are inside the two-level table (LOW_RANGE_END = 0x33480).
// U+3134B–U+3134F is a small disallowed gap within an otherwise-valid range.
TEST(mapping_tests, mid_range_boundaries) {
  // U+2FFFF: last code point inside the low-range two-level table
  // (It is disallowed – just verify we don't crash or return wrong result)
  EXPECT_TRUE(ada::idna::map(U"\U0002FFFF").empty()); // disallowed

  // U+30000: first valid code point of mid range (CJK Extension I start)
  EXPECT_EQ(ada::idna::map(U"\U00030000"), (std::u32string{0x30000}));

  // U+3134A: last valid of mid-range part 1
  EXPECT_EQ(ada::idna::map(U"\U0003134A"), (std::u32string{0x3134A}));

  // U+3134B: first disallowed in the small mid-range gap
  EXPECT_TRUE(ada::idna::map(U"\U0003134B").empty());

  // U+3134F: last disallowed in the small mid-range gap
  EXPECT_TRUE(ada::idna::map(U"\U0003134F").empty());

  // U+31350: first valid of mid-range part 2 (CJK Extension J start)
  EXPECT_EQ(ada::idna::map(U"\U00031350"), (std::u32string{0x31350}));

  // U+33479: last valid of mid range
  EXPECT_EQ(ada::idna::map(U"\U00033479"), (std::u32string{0x33479}));

  // U+3347A: first disallowed after the mid range
  EXPECT_TRUE(ada::idna::map(U"\U0003347A").empty());
}

// ── High ignored range boundaries (0xE0100–0xE01EF) ───────────────────────
TEST(mapping_tests, high_ignored_range_boundaries) {
  // U+E00FF: just before the ignored range → disallowed
  EXPECT_TRUE(ada::idna::map(U"\U000E00FF").empty());

  // U+E0100: first in the ignored range
  EXPECT_EQ(ada::idna::map(U"\U000E0100"), U"");

  // U+E01EF: last in the ignored range
  EXPECT_EQ(ada::idna::map(U"\U000E01EF"), U"");

  // U+E01F0: just after the ignored range → disallowed
  EXPECT_TRUE(ada::idna::map(U"\U000E01F0").empty());
}

// ── Block (64-cp) boundary conditions ─────────────────────────────────────
// Test the last code point of a block and the first of the next block
// to guard against off-by-one errors in the stage1/stage2 indexing.
TEST(mapping_tests, block_boundary_conditions) {
  // Block 0 ends at cp 63 (U+003F '?') – valid
  EXPECT_EQ(ada::idna::map(U"\U0000003F"), U"\U0000003F");
  // Block 1 starts at cp 64 (U+0040 '@') – valid
  EXPECT_EQ(ada::idna::map(U"\U00000040"), U"\U00000040");

  // Block 1 ends at cp 127 (U+007F DEL) – valid per IDNA (007B..007F valid NV8)
  EXPECT_EQ(ada::idna::map(std::u32string(1, char32_t(0x7F))),
            std::u32string(1, char32_t(0x7F)));
  // Block 2 starts at cp 128 (U+0080 first C1 control) – disallowed
  EXPECT_TRUE(ada::idna::map(std::u32string(1, char32_t(0x80))).empty());

  // Block 3: cp 192 (U+00C0 'À') → mapped to 'à' (U+00E0)
  EXPECT_EQ(ada::idna::map(U"\U000000C0"), U"\U000000E0");
  // Block 3: cp 223 (U+00DF 'ß') → valid
  EXPECT_EQ(ada::idna::map(U"\U000000DF"), U"\U000000DF");
  // Block 4: cp 224 (U+00E0 'à') → valid
  EXPECT_EQ(ada::idna::map(U"\U000000E0"), U"\U000000E0");

  // Block at 0x640 (cp 1600 = Arabic tatweel, U+0640) → valid
  EXPECT_EQ(ada::idna::map(U"\U00000640"), U"\U00000640");

  // cp 0x3FC (1020 = U+03FC GREEK SMALL LETTER RHO WITH STROKE) → valid
  EXPECT_EQ(ada::idna::map(U"\U000003FC"), U"\U000003FC");
}

// ── String-level tests ──────────────────────────────────────────────────────
// Mapping operates on strings, not just individual code points.

TEST(mapping_tests, string_with_all_valid) {
  EXPECT_EQ(ada::idna::map(U"hello"), U"hello");
  EXPECT_EQ(ada::idna::map(U"hello123"), U"hello123");
}

TEST(mapping_tests, string_uppercase_folded) {
  EXPECT_EQ(ada::idna::map(U"Hello"), U"hello");
  EXPECT_EQ(ada::idna::map(U"EXAMPLE"), U"example");
  EXPECT_EQ(ada::idna::map(U"MiXeD"), U"mixed");
}

TEST(mapping_tests, string_ignored_chars_removed) {
  // Multiple soft hyphens removed
  EXPECT_EQ(ada::idna::map(U"a\U000000AD\U000000ADb"), U"ab");
  // Ignored variation selector removed, leaving base char
  EXPECT_EQ(ada::idna::map(U"\U00004E2D\U000E0100"), U"\U00004E2D");
}

TEST(mapping_tests, string_disallowed_anywhere_fails) {
  // Disallowed at the start
  EXPECT_TRUE(ada::idna::map(U"\U0000FFFEhello").empty());
  // Disallowed in the middle (U+0080, first C1 control)
  EXPECT_TRUE(ada::idna::map(std::u32string(U"hel") +
                             std::u32string(1, char32_t(0x80)) +
                             std::u32string(U"lo")).empty());
  // Disallowed at the end
  EXPECT_TRUE(ada::idna::map(std::u32string(U"hello") +
                             std::u32string(1, char32_t(0x80))).empty());
}

TEST(mapping_tests, string_multi_cp_mappings_in_context) {
  // "ﬁle" (fi ligature + "le") → "file"
  EXPECT_EQ(ada::idna::map(U"\U0000FB01le"), U"file");
  // Mix of uppercase, ligature, and valid
  EXPECT_EQ(ada::idna::map(U"A\U0000FB00B"), U"affb");
}

TEST(mapping_tests, empty_string) {
  EXPECT_EQ(ada::idna::map(U""), U"");
}

// ── CJK compatibility ideographs in the mapping range ─────────────────────
// These are in the high area of the two-level table (0x2F800 range).
TEST(mapping_tests, cjk_compatibility_ideographs) {
  // U+2F800 maps to U+4E3D
  EXPECT_EQ(ada::idna::map(U"\U0002F800"), (std::u32string{0x4E3D}));
  // U+2F801 maps to U+4E38
  EXPECT_EQ(ada::idna::map(U"\U0002F801"), (std::u32string{0x4E38}));
}
