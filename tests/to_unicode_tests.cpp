#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ada/idna/to_ascii.h"
#include "ada/idna/to_unicode.h"

// Global variables for file paths that can be set via environment variables
std::string GetUnicodeFilename() {
  const char* env_file = std::getenv("IDNA_UNICODE_FILE");
  return env_file ? std::string(env_file)
                  : "fixtures/to_unicode_alternating.txt";
}

class IdnaToUnicodeTest : public ::testing::Test {
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

// Test for Unicode conversions from file
TEST_F(IdnaToUnicodeTest, AlternatingFileConversions) {
  std::string filename = GetUnicodeFilename();

  // Skip test if file doesn't exist
  if (!std::filesystem::exists(filename)) {
    GTEST_SKIP() << "Test file not found: " << filename;
  }

  std::string buffer = ReadFile(filename);
  std::vector<std::string> lines = SplitString(buffer);

  ASSERT_GE(lines.size(), 2) << "File must contain at least 2 lines";

  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    const std::string& input = lines[i];
    const std::string& output = lines[i + 1];

    SCOPED_TRACE("Line " + std::to_string(i) + ": " + input);
    ASSERT_EQ(ada::idna::to_unicode(input), output);
  }
}
