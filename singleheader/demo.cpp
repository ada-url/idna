#include <iostream>

#include "ada_idna.cpp"
#include "ada_idna.h"

int main(int, char *[]) {
  auto url = ada::idna::to_ascii("www.google.com");
  return EXIT_SUCCESS;
}
