#include "gtest/gtest.h"
#include <iostream>

#include "idna.h"

class IdnaTest : public testing::Test {
 protected:
  static std::u32string to_utf32(std::string_view ut8_string) {
    size_t utf32_length =
        ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
    std::u32string utf32(utf32_length, '\0');
    ada::idna::utf8_to_utf32(ut8_string.data(), ut8_string.size(),
                             utf32.data());
    return utf32;
  }

  // Helper method to verify code points
  static void VerifyCodePoint(std::string_view input, bool first,
                              bool expected) {
    std::u32string code_points = to_utf32(input);
    ASSERT_FALSE(code_points.empty()) << "Failed to convert: " << input;
    ASSERT_EQ(ada::idna::valid_name_code_point(code_points[0], first), expected)
        << "Test failed for input: " << input << ", first: " << std::boolalpha
        << first;
  }
};

TEST_F(IdnaTest, FirstPositionCodePoints) {
  VerifyCodePoint("a", true, true);
  VerifyCodePoint("é", true, true);
  VerifyCodePoint("A", true, true);
  VerifyCodePoint("0", true, false);
}

TEST_F(IdnaTest, OtherPositionCodePoints) {
  VerifyCodePoint("a", false, true);
  VerifyCodePoint("A", false, true);
  VerifyCodePoint("À", false, true);
  VerifyCodePoint("0", false, true);
  VerifyCodePoint(" ", false, false);
}
