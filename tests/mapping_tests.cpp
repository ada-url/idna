#include "gtest/gtest.h"
#include "ada/idna/mapping.h"

TEST(mapping_tests, verify) {
  ASSERT_EQ(ada::idna::map(U"asciitwontchange"), U"asciitwontchange");
  ASSERT_EQ(ada::idna::map(U"hasomit\u00adted"), U"hasomitted");
  ASSERT_EQ(ada::idna::map(U"\u00aalla"), U"alla");
}
