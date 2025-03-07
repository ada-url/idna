#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ada/idna/punycode.h"
#include "ada/idna/to_ascii.h"
#include "ada/idna/unicode_transcoding.h"

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