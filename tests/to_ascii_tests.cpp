#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ada/idna/punycode.h"
#include "ada/idna/to_ascii.h"
#include "ada/idna/unicode_transcoding.h"
#include "gtest/gtest.h"

class IdnaFileTest : public ::testing::Test {
 protected:
  // Helper function to read a file
  static std::string ReadFile(const std::string& filename) {
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(filename.c_str());
    stream.exceptions(std::ios_base::badbit);
    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size)) {
      out.append(buf, 0, size_t(stream.gcount()));
    }
    out.append(buf, 0, size_t(stream.gcount()));
    return out;
  }

  // Helper function to split a string by newlines
  static std::vector<std::string> SplitString(const std::string& str) {
    auto result = std::vector<std::string>{};
    auto ss = std::stringstream{str};
    for (std::string line; std::getline(ss, line, '\n');) {
      result.push_back(line);
    }
    return result;
  }

  // Set default filenames that can be overridden in tests
  std::string alternating_filename = "fixtures/to_ascii_alternating.txt";
  std::string invalid_filename = "fixtures/to_ascii_invalid.txt";
};

// Test for valid ASCII conversions from file
TEST_F(IdnaFileTest, AlternatingFileConversions) {
  // Skip test if file doesn't exist
  if (!std::filesystem::exists(alternating_filename)) {
    GTEST_SKIP() << "Test file not found: " << alternating_filename;
  }

  std::string buffer = ReadFile(alternating_filename);
  std::vector<std::string> lines = SplitString(buffer);

  ASSERT_GT(lines.size(), 1) << "File must contain at least 2 lines";

  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    const std::string& utf8_string = lines[i];
    const std::string& puny_string = lines[i + 1];

    SCOPED_TRACE("Line " + std::to_string(i) + ": " + utf8_string);

    std::string processed = ada::idna::to_ascii(utf8_string);
    EXPECT_EQ(processed, puny_string)
        << "Conversion failed for '" << utf8_string << "'"
        << "\nExpected: " << puny_string << "\nGot: " << processed;
  }
}

// Test for invalid conversions from file
TEST_F(IdnaFileTest, InvalidFileConversions) {
  // Skip test if file doesn't exist
  if (!std::filesystem::exists(invalid_filename)) {
    GTEST_SKIP() << "Test file not found: " << invalid_filename;
  }

  std::string buffer = ReadFile(invalid_filename);
  std::vector<std::string> lines = SplitString(buffer);

  for (size_t i = 0; i < lines.size(); i++) {
    const std::string& utf8_string = lines[i];

    SCOPED_TRACE("Line " + std::to_string(i) + ": " + utf8_string);

    EXPECT_TRUE(ada::idna::to_ascii(utf8_string).empty())
        << "Should have failed for invalid input: " << utf8_string;
  }
}
#define UNICODE16 0
TEST(to_ascii_tests, special_cases) {
  // Test case for Tangut ideograph U+17E68 (𗹨)
#if UNICODE16
  // This test needs to be reviewed for Unicode 16.
  ASSERT_TRUE(ada::idna::to_ascii("\xf0\xaf\xa1\xa8").empty())
      << "Tangut ideograph should result in empty string";
#endif
  // Test case for German capital sharp S (ẞ)
  // We would prefer "\u1E9E" but Visual Studio complains.
  ASSERT_EQ(ada::idna::to_ascii("\xe1\xba\x9e"), "xn--zca")
      << "German capital sharp S should convert to expected Punycode";

  // Test case for soft hyphen (U+00AD)
  ASSERT_TRUE(ada::idna::to_ascii("\u00AD").empty())
      << "Soft hyphen should result in empty string";

  // Test case for replacement character (U+FFFD)
  ASSERT_TRUE(ada::idna::to_ascii("\xef\xbf\xbd.com").empty())
      << "Replacement character in domain should result in empty string";
}

TEST(to_ascii_tests, comma_test) {
  ASSERT_FALSE(ada::idna::to_ascii("128.0,0.1").empty());
}

// Unicode 17 test cases - CJK Extension J code points (U+323B0..U+33479)
TEST(to_ascii_tests, unicode17_cjk_extension_j) {
  // Test case: U+32931 (𲤱) - CJK Extension J character
  // Input: "𲤱20.音.ꡦ1." (U+32931, ASCII, U+97F3, U+A866)
  ASSERT_EQ(ada::idna::to_ascii("\xf0\xb2\xa4\xb1"
                                "20.\xe9\x9f\xb3.\xea\xa1\xa6"
                                "1."),
            "xn--20-9802c.xn--0w5a.xn--1-eg4e.")
      << "CJK Extension J character should be valid in Unicode 17";

  // Test case: Already punycode-encoded version should pass through
  ASSERT_EQ(ada::idna::to_ascii("xn--20-9802c.xn--0w5a.xn--1-eg4e."),
            "xn--20-9802c.xn--0w5a.xn--1-eg4e.")
      << "Punycode version should remain unchanged";

  // Test case: U+32B9A (𲮚) - CJK Extension J with combining marks
  // Input: "𲮚9ꍩ៓.ss" (U+32B9A, ASCII 9, U+A369, U+17D3)
  ASSERT_EQ(ada::idna::to_ascii("\xf0\xb2\xae\x9a"
                                "9\xea\x8d\xa9\xe1\x9f\x93.ss"),
            "xn--9-i0j5967eg3qz.ss")
      << "CJK Extension J with combining marks should convert correctly";

  // Test case: Same as above but with uppercase SS - should normalize to
  // lowercase
  ASSERT_EQ(ada::idna::to_ascii("\xf0\xb2\xae\x9a"
                                "9\xea\x8d\xa9\xe1\x9f\x93.SS"),
            "xn--9-i0j5967eg3qz.ss")
      << "Uppercase SS should normalize to lowercase ss";
}

// Regression test for https://github.com/whatwg/url/issues/803
// A label like U+33FF U+33FD followed by ASCII "xn--" encodes to a punycode
// payload starting with "xn--", but the decoded form begins with non-ASCII
// characters -- it is NOT a double-encoded ACE label and must be accepted.
TEST(to_ascii_tests, mixed_label_xn_prefix_regression) {
  // U+33FF = \xe3\x8f\xbf, U+33FD = \xe3\x8f\xbd in UTF-8
  const std::string input = "\xe3\x8f\xbf\xe3\x8f\xbdxn--.example";
  std::string first = ada::idna::to_ascii(input);
  ASSERT_FALSE(first.empty()) << "Mixed IDNA label whose punycode starts with "
                                 "'xn--' must not be rejected";
  // Idempotency: to_ascii applied twice must give the same result.
  std::string second = ada::idna::to_ascii(first);
  ASSERT_EQ(first, second) << "to_ascii must be idempotent";
}

TEST(to_ascii_tests, bidi_regression) {
  // LTR label ending in RTL/AL/AN should fail (Rule 6)
  EXPECT_TRUE(ada::idna::to_ascii("a\u05D0").empty())
      << "LTR + RTL should fail";
  EXPECT_TRUE(ada::idna::to_ascii("a\u0627").empty()) << "LTR + AL should fail";
  EXPECT_TRUE(ada::idna::to_ascii("a\u0661").empty()) << "LTR + AN should fail";

  // RTL + trailing NSM
  EXPECT_TRUE(ada::idna::to_ascii("a\u05D0\u0301").empty())
      << "RTL + trailing NSM should fail";

  // NSM before RTL
  EXPECT_TRUE(ada::idna::to_ascii("a\u0301\u05D0").empty())
      << "NSM before RTL should fail";

  // multiple RTL
  EXPECT_TRUE(ada::idna::to_ascii("a\u05D0\u05D1").empty())
      << "multiple RTL should fail";

  // multi-label
  EXPECT_TRUE(ada::idna::to_ascii("a\u05D0.b").empty())
      << "multi-label should fail";
}

// Regression tests for the WHATWG URL "domain to ASCII" web-compatibility
// carve-out: when the input domain is an ASCII string (beStrict = false), the
// result is the input lowercased, regardless of Unicode ToASCII's outcome. An
// ACE ("xn--") label may decode yet still fail IDNA validity criteria and must
// nevertheless be accepted as-is.
// See https://url.spec.whatwg.org/#concept-domain-to-ascii
TEST(to_ascii_tests, ascii_xn_carveout) {
  // Bare ACE prefix: the Punycode payload is empty. Previously rejected.
  EXPECT_EQ(ada::idna::to_ascii("xn--"), "xn--");

  // Invalid Punycode payload (WPT toascii.json "Invalid Punycode"): the ACE
  // segment does not decode. Previously rejected; under the carve-out the
  // ASCII input is returned lowercased. This is the case that broke Ada's
  // URLPattern hostname unit test after the 0.6.0 bump
  // (hostname "xn--a" must be accepted as "xn--a").
  EXPECT_EQ(ada::idna::to_ascii("xn--a"), "xn--a");
  EXPECT_EQ(ada::idna::to_ascii("XN--A"), "xn--a");
  // Same invalid ACE as a label of a larger domain (IdnaTestV2 V7).
  EXPECT_EQ(ada::idna::to_ascii("xn--a.pt"), "xn--a.pt");

  // ContextJ (C1): xn--ab-j1t decodes to "a\u200Cb" (ZWNJ not preceded by a
  // virama). Previously rejected; now accepted as-is.
  EXPECT_EQ(ada::idna::to_ascii("xn--ab-j1t"), "xn--ab-j1t");

  // Decoded label has code points with "mapped" status (enclosed CJK), so the
  // ACE form is non-canonical. Previously rejected; now accepted as-is.
  EXPECT_EQ(ada::idna::to_ascii("a.b.c.xn--pokxncvks"), "a.b.c.xn--pokxncvks");

  // The URL spec's own example: xn--8i7caa decodes to fullwidth "www", whose
  // code points have "mapped" status.
  EXPECT_EQ(ada::idna::to_ascii("xn--8i7caa"), "xn--8i7caa");

  // Mixed/upper-case ACE prefixes must be lowercased.
  EXPECT_EQ(ada::idna::to_ascii("a.b.c.XN--pokxncvks"), "a.b.c.xn--pokxncvks");
  EXPECT_EQ(ada::idna::to_ascii("a.b.c.Xn--pokxncvks"), "a.b.c.xn--pokxncvks");

  // A plain already-ASCII domain is returned lowercased.
  EXPECT_EQ(ada::idna::to_ascii("EXAMPLE.COM"), "example.com");

  // Idempotency: to_ascii of an ASCII result is stable.
  const std::string once = ada::idna::to_ascii("xn--ab-j1t");
  EXPECT_EQ(ada::idna::to_ascii(once), once);
  EXPECT_EQ(ada::idna::to_ascii("xn--a"), "xn--a");
}

// The ASCII carve-out must NOT relax validation for non-ASCII inputs: those go
// through full UTS#46 validation. Contrast xn--ab-j1t (ASCII, accepted above)
// with its decoded form "a\u200Cb" supplied directly (non-ASCII, rejected).
TEST(to_ascii_tests, non_ascii_inputs_still_validated) {
  // ZWNJ (U+200C) without a preceding virama: ContextJ (C1) violation.
  EXPECT_TRUE(ada::idna::to_ascii("a\u200Cb").empty())
      << "non-ASCII ContextJ violation should still fail";

  // LTR label containing an RTL code point: Bidi rule violation.
  EXPECT_TRUE(ada::idna::to_ascii("a\u05D0").empty())
      << "non-ASCII Bidi violation should still fail";
}