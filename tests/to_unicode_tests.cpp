#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ada/idna/to_ascii.h"
#include "ada/idna/to_unicode.h"

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
    out.append(buf, 0, stream.gcount());
  }
  out.append(buf, 0, stream.gcount());
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

bool test(std::string utf8_string, std::string puny_string) {
  std::cout << "processing " << puny_string << std::endl;
  auto processed = ada::idna::to_unicode(puny_string);
  if (processed != puny_string) {
    std::cout << "got " << processed << std::endl;
    std::cout << "expected " << utf8_string << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  std::string filename = "to_unicode_alternating.txt";
  if (argc > 1) {
    filename = argv[1];
  }

  if (!file_exists(filename)) {
    return EXIT_FAILURE;
  }
  std::string buffer = read_file(filename);
  std::vector<std::string> lines = split_string(buffer);
  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    std::string utf8_string = lines[i];
    std::string puny_string{};
    puny_string.resize(63);
    puny_string.append(lines[i + 1]);

    if (!test(utf8_string, puny_string)) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}