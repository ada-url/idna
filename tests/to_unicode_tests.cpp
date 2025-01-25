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

bool test(std::string input, const std::string& output) {
  std::cout << "processing " << input << std::endl;
  auto processed = ada::idna::to_unicode(input);
  if (processed != output) {
    std::cout << "got " << processed << processed.size() << std::endl;
    std::cout << "expected " << output << output.size() << std::endl;
    return false;
  }
  std::cout << "processed " << processed << std::endl;
  return true;
}

int main(int argc, char** argv) {
  std::string filename = "fixtures/to_unicode_alternating.txt";
  if (argc > 1) {
    filename = argv[1];
  }

  if (!file_exists(filename)) {
    return EXIT_FAILURE;
  }
  std::string buffer = read_file(filename);
  std::vector<std::string> lines = split_string(buffer);
  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    std::string input = lines[i];
    std::string output = lines[i + 1];
    if (!test(input, output)) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}