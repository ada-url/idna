#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ada/idna/punycode.h"
#include "ada/idna/to_ascii.h"
#include "ada/idna/unicode_transcoding.h"

bool file_exists(std::string_view filename) {
  namespace fs = std::filesystem;
  std::filesystem::path f{filename};
  if (std::filesystem::exists(filename)) {
    std::cout << "  file found: " << filename << std::endl;
    return true;
  } else {
    std::cout << "  file missing: " << filename << std::endl;
    return false;
  }
}

std::string read_file(std::string filename) {
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

std::vector<std::string> split_string(const std::string& str) {
  auto result = std::vector<std::string>{};
  auto ss = std::stringstream{str};
  for (std::string line; std::getline(ss, line, '\n');) {
    result.push_back(line);
  }
  return result;
}

bool test(std::string ut8_string, std::string puny_string) {
  std::cout << "processing " << puny_string << std::endl;
  auto processed = ada::idna::to_ascii(ut8_string);
  if (processed != puny_string) {
    std::cout << "got " << processed << std::endl;
    std::cout << "expected " << puny_string << std::endl;
    return false;
  }
  return true;
}

bool special_cases() {
  if (!ada::idna::to_ascii("\u00AD").empty()) {
    return false;
  }
  if (!ada::idna::to_ascii("\xef\xbf\xbd.com").empty()) {
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  if (!special_cases()) {
    return EXIT_FAILURE;
  }
  std::string filename = "fixtures/to_ascii_alternating.txt";
  if (argc > 1) {
    filename = argv[1];
  }

  if (!file_exists(filename)) {
    return EXIT_FAILURE;
  }
  std::string buffer = read_file(filename);
  std::vector<std::string> lines = split_string(buffer);
  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    std::string ut8_string = lines[i];
    std::string puny_string = lines[i + 1];
    if (!test(ut8_string, puny_string)) {
      return EXIT_FAILURE;
    }
  }
  filename = "fixtures/to_ascii_invalid.txt";
  if (argc > 2) {
    filename = argv[2];
  }

  if (!file_exists(filename)) {
    return EXIT_FAILURE;
  }
  buffer = read_file(filename);
  lines = split_string(buffer);
  for (size_t i = 0; i < lines.size(); i++) {
    std::string ut8_string = lines[i];
    if (!ada::idna::to_ascii(ut8_string).empty()) {
      std::cout << "Should have failed: " << ut8_string << std::endl;
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}