#include <iostream>

#include "idna.h"

std::u32string to_utf32(std::string_view ut8_string) {
  size_t utf32_length =
      ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  std::u32string utf32(utf32_length, '\0');
  ada::idna::utf8_to_utf32(ut8_string.data(), ut8_string.size(), utf32.data());
  return utf32;
}

void verify(std::string_view input, bool first, bool expected) {
  std::u32string first_code_point = to_utf32(input);
  if (first_code_point.empty()) {
    std::cerr << "bug" << input << std::endl;
    exit(-1);
  }
  if (ada::idna::valid_name_code_point(first_code_point[0], first) !=
      expected) {
    std::cerr << "bug" << input << std::endl;
    exit(-1);
  }
}

int main(int argc, char **argv) {
  verify("a", true, true);
  verify("é", true, true);
  verify("A", true, true);
  verify("0", true, false);
  verify("a", false, true);
  verify("A", false, true);
  verify("À", false, true);
  verify("0", false, true);
  verify(" ", false, false);

  std::cout << "SUCCESS" << std::endl;
  return EXIT_SUCCESS;
}