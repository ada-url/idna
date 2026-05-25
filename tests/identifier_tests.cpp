#include "gtest/gtest.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <random>

#include "idna.h"

// Pull in the raw Unicode range tables so the exhaustive test below can use
// them as the ground truth against valid_name_code_point().
#include "../src/id_tables.cpp"

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

// Disabled by default: exhaustive sweep over all 1.1M code points is slow.
// Run explicitly with `--gtest_also_run_disabled_tests`.
TEST_F(IdnaTest, DISABLED_ExhaustiveAgainstIdTables) {
  // Walk the entire Unicode code-point space [0, 0x10FFFF] and confirm that
  // valid_name_code_point() agrees with a direct scan of id_start /
  // id_continue.
  //
  // The function adds two JS-identifier extensions on top of the Unicode
  // ID_Start / ID_Continue properties (per
  // https://tc39.es/ecma262/#prod-IdentifierStart):
  //   - '$' and '_' are valid in IdentifierStart
  //   - '$' is valid in IdentifierPart
  // Surrogates (U+D800..U+DFFF) are rejected outright.
  //
  // The tables are sorted by range, so we walk them with rolling indices
  // (si, ci) that only ever advance — O(N + table_size) total work.
  constexpr std::size_t id_start_n = std::size(ada::idna::id_start);
  constexpr std::size_t id_continue_n = std::size(ada::idna::id_continue);

  std::size_t si = 0, ci = 0;
  for (uint32_t cp = 0; cp <= 0x10FFFF; ++cp) {
    while (si < id_start_n && ada::idna::id_start[si][1] < cp) ++si;
    while (ci < id_continue_n && ada::idna::id_continue[ci][1] < cp) ++ci;

    const bool is_surrogate = (cp >= 0xD800 && cp <= 0xDFFF);

    const bool expected_first =
        !is_surrogate &&
        (cp == U'$' || cp == U'_' ||
         (si < id_start_n && cp >= ada::idna::id_start[si][0]));

    const bool expected_continue =
        !is_surrogate && (cp == U'$' || (ci < id_continue_n &&
                                         cp >= ada::idna::id_continue[ci][0]));

    ASSERT_EQ(ada::idna::valid_name_code_point(static_cast<char32_t>(cp), true),
              expected_first)
        << "Mismatch at U+" << std::hex << cp << " (first=true)";
    ASSERT_EQ(
        ada::idna::valid_name_code_point(static_cast<char32_t>(cp), false),
        expected_continue)
        << "Mismatch at U+" << std::hex << cp << " (first=false)";
  }
}

TEST_F(IdnaTest, SampledAgainstIdTables) {
  // Same check as DISABLED_ExhaustiveAgainstIdTables, but samples roughly
  // 1 in 100 code points using a fixed-seed RNG so the run is deterministic
  // and fast enough for the default test suite. The rolling table indices
  // still advance for every code point so they remain consistent at the
  // sampled positions.
  constexpr std::size_t id_start_n = std::size(ada::idna::id_start);
  constexpr std::size_t id_continue_n = std::size(ada::idna::id_continue);

  std::mt19937 rng(0xC0DE);
  std::uniform_int_distribution<int> pick(0, 99);

  std::size_t si = 0, ci = 0;
  for (uint32_t cp = 0; cp <= 0x10FFFF; ++cp) {
    while (si < id_start_n && ada::idna::id_start[si][1] < cp) ++si;
    while (ci < id_continue_n && ada::idna::id_continue[ci][1] < cp) ++ci;

    if (pick(rng) != 0) continue;

    const bool is_surrogate = (cp >= 0xD800 && cp <= 0xDFFF);

    const bool expected_first =
        !is_surrogate &&
        (cp == U'$' || cp == U'_' ||
         (si < id_start_n && cp >= ada::idna::id_start[si][0]));

    const bool expected_continue =
        !is_surrogate && (cp == U'$' || (ci < id_continue_n &&
                                         cp >= ada::idna::id_continue[ci][0]));

    ASSERT_EQ(ada::idna::valid_name_code_point(static_cast<char32_t>(cp), true),
              expected_first)
        << "Mismatch at U+" << std::hex << cp << " (first=true)";
    ASSERT_EQ(
        ada::idna::valid_name_code_point(static_cast<char32_t>(cp), false),
        expected_continue)
        << "Mismatch at U+" << std::hex << cp << " (first=false)";
  }
}
