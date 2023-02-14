#include "ada_idna.cpp"
#include "ada_idna.h"
#include <iostream>

int main(int , char *[]) {
  auto url = ada::idna::to_ascii("www.google.com");
  return EXIT_SUCCESS;
}
