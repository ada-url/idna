#include <iostream>

#include "ada/idna/identifier.h"

void verify(std::string_view input, bool first, bool expected) {
  if (ada::idna::valid_name_code_point(input, first) != expected) {
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