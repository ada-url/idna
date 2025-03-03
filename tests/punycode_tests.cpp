#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"

// Global function to get filename from environment or use default
std::string GetPunycodeFilename() {
  const char* env_file = std::getenv("IDNA_PUNYCODE_FILE");
  return env_file ? std::string(env_file)
                  : "fixtures/utf8_punycode_alternating.txt";
}

class PunycodeTest : public ::testing::Test {
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
};

// Test for Punycode transcoding from file
TEST_F(PunycodeTest, AlternatingFileTranscoding) {
  std::string filename = GetPunycodeFilename();

  // Skip test if file doesn't exist
  if (!std::filesystem::exists(filename)) {
    GTEST_SKIP() << "Test file not found: " << filename;
    return;
  }

  std::string buffer = ReadFile(filename);
  std::vector<std::string> lines = SplitString(buffer);

  ASSERT_GE(lines.size(), 2) << "File must contain at least 2 lines";

  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    std::string utf8_string = lines[i];
    std::string puny_string = lines[i + 1];

    SCOPED_TRACE("Line " + std::to_string(i) + ": UTF-8 = '" + utf8_string +
                 "', Punycode = '" + puny_string + "'");

    std::cout << "processing " << puny_string << std::endl;

    // Test UTF-8 <=> UTF-32 roundtrip
    size_t utf32_length = ada::idna::utf32_length_from_utf8(utf8_string.data(),
                                                            utf8_string.size());
    std::u32string utf32(utf32_length, '\0');
    ada::idna::utf8_to_utf32(utf8_string.data(), utf8_string.size(),
                             utf32.data());

    std::string tmp(utf8_string.size(), '\0');
    ada::idna::utf32_to_utf8(utf32.data(), utf32_length, tmp.data());

    EXPECT_EQ(tmp, utf8_string) << "bad utf-8 <==> utf-32 transcoding";

    // Test UTF-32 => Punycode
    std::string puny;
    bool is_ok1 = ada::idna::utf32_to_punycode(utf32, puny);

    EXPECT_TRUE(is_ok1) << "bad utf-32 => punycode transcoding";
    EXPECT_EQ(puny, puny_string) << "punycode mismatch";

    // Test Punycode => UTF-32
    std::u32string utf32back;
    bool is_ok2 = ada::idna::punycode_to_utf32(puny, utf32back);

    EXPECT_TRUE(is_ok2) << "bad punycode => utf-32 transcoding";

    // Test roundtrip Punycode
    std::string punyback;
    bool is_ok3 = ada::idna::utf32_to_punycode(utf32back, punyback);

    EXPECT_TRUE(is_ok3) << "bad utf-32 => punycode transcoding (second time)";
    EXPECT_EQ(punyback, puny)
        << "bad punycode => utf-32 => punycode transcoding";

    // Test full roundtrip back to UTF-8
    size_t utf8_length =
        ada::idna::utf8_length_from_utf32(utf32back.data(), utf32back.size());
    std::string finalutf8(utf8_length, '\0');
    ada::idna::utf32_to_utf8(utf32back.data(), utf32back.size(),
                             finalutf8.data());

    EXPECT_EQ(finalutf8, utf8_string)
        << "bad roundtrip utf8 => utf8 transcoding";
  }
}
