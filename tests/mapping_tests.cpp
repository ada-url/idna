#include "ada/idna/mapping.h"
#include <iostream>

void verify(std::u32string_view input, std::u32string_view expected) {
    if(input != expected) { std::cerr << "bug" << std::endl; exit(-1); }
}

int main(int argc, char **argv) {
    verify(ada::idna::map(U"asciitwontchange"), U"asciitwontchange");
    verify(ada::idna::map(U"hasomit\u00adted"), U"hasomitted");
    verify(ada::idna::map(U"\u00aalla"), U"alla");
    std::cout << "SUCCESS" << std::endl;
    return EXIT_SUCCESS;
}