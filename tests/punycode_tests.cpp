#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ada/idna/punycode.h"
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

std::string read_file(const std::string& filename) {
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

bool test(const std::string& ut8_string, const std::string& puny_string) {
  std::cout << "processing " << puny_string << std::endl;
  // first check that we can go to UTF-32 and and back.
  size_t utf32_length =
      ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  std::u32string utf32(utf32_length, '\0');
  ada::idna::utf8_to_utf32(ut8_string.data(), ut8_string.size(), utf32.data());
  std::string tmp(ut8_string.size(), '\0');
  ada::idna::utf32_to_utf8(utf32.data(), utf32_length, tmp.data());
  if (tmp != ut8_string) {
    std::cout << "bad utf-8 <==> utf-32 transcoding" << std::endl;
    return false;
  }
  std::string puny;
  bool is_ok1 = ada::idna::utf32_to_punycode(utf32, puny);
  if (!is_ok1) {
    std::cout << "bad utf-32 => punycode transcoding" << std::endl;
    return false;
  }
  if (puny != puny_string) {
    std::cout << "punycode mismatch" << std::endl;
    return false;
  }
  std::u32string utf32back;
  bool is_ok2 = ada::idna::punycode_to_utf32(puny, utf32back);
  if (!is_ok2) {
    std::cout << "bad punycode => utf-32 transcoding" << std::endl;
    return false;
  }
  // see if we can go back
  std::string punyback;
  bool is_ok3 = ada::idna::utf32_to_punycode(utf32back, punyback);
  if ((!is_ok3) || (punyback != puny)) {
    std::cout << "bad punycode => utf-32 => punycode transcoding" << std::endl;
    return false;
  }
  size_t utf8_length =
      ada::idna::utf8_length_from_utf32(utf32back.data(), utf32back.size());
  std::string finalutf8(utf8_length, '\0');
  ada::idna::utf32_to_utf8(utf32back.data(), utf32back.size(),
                           finalutf8.data());
  if (finalutf8 != ut8_string) {
    std::cout << "bad roundtrip utf8 => utf8 transcoding" << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  std::string filename = "fixtures/utf8_punycode_alternating.txt";
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
  return EXIT_SUCCESS;
}