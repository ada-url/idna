#include "gtest/gtest.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ada/idna/to_ascii.h"
#include "simdjson.h"

using namespace simdjson;

class IdnaTestV2 : public ::testing::Test {
 protected:
  // Global function to get filename from environment or use default
  static std::string GetTestV2Filename() {
    const char* env_file = std::getenv("IDNA_TEST_V2_FILE");
    return env_file ? std::string(env_file) : "fixtures/IdnaTestV2.json";
  }
};

// Test for IdnaTestV2 JSON file
TEST_F(IdnaTestV2, ToAscii) {
  std::string filename = GetTestV2Filename();

  // Skip test if file doesn't exist
  if (!std::filesystem::exists(filename)) {
    GTEST_SKIP() << "Test file not found: " << filename;
  }

  ondemand::parser parser;

  // Load the JSON file
  padded_string json;
  try {
    json = padded_string::load(filename);
    std::cout << "  loaded " << filename << " (" << json.size() << " kB)"
              << std::endl;
  } catch (const simdjson_error& e) {
    FAIL() << "Failed to load JSON file: " << e.what();
  }

  ondemand::document doc;
  try {
    doc = parser.iterate(json);
  } catch (const simdjson_error& e) {
    FAIL() << "Failed to parse JSON: " << e.what();
  }

  // Process each test case
  for (auto element : doc.get_array()) {
    if (element.type() == ondemand::json_type::string) {
      continue;  // Skip string elements (comments)
    }

    ondemand::object object;
    try {
      object = element.get_object();
    } catch (const simdjson_error& e) {
      ADD_FAILURE() << "Failed to get object from array element: " << e.what();
      continue;
    }

    auto json_string =
        std::string(std::string_view(simdjson::to_json_string(object)));

    // Process comment if available (for debugging)
    ondemand::value comment;
    if (object["comment"].get(comment) == simdjson::SUCCESS) {
      std::string_view comment_string;
      if (comment.get_string().get(comment_string) == simdjson::SUCCESS) {
        // Doesn't see useful to print all comments:
        // std::cout << "  comment: " << comment_string << std::endl;
      }
    }

    // Get input and process with IDNA
    std::string_view input;
    try {
      input = object["input"].get_string();
    } catch (const simdjson_error& e) {
      // We assume that the input is valid Unicode.
      // However, the test cases may contain invalid Unicode strings.
      // We skip the test case if the input is invalid.
#if UNICODE16
      ADD_FAILURE()
#else
      std::cout
#endif
          << "Failed to get input string: " << e.what()
          << " for test case: " << json_string;
      continue;
    }

    // Create scoped trace for better test failures
    SCOPED_TRACE("Test case: " + json_string);

    // Process input with IDNA
    std::string output = ada::idna::to_ascii(input);
    if (ada::idna::contains_forbidden_domain_code_point(output)) {
      output = "";
    }

    // Check expected output
    auto expected_output = object["output"];

    if (expected_output.is_null()) {
      // For null expected output, we expect empty string result
      if (!output.empty()) {
        std::cout << "\n  Test case: " + json_string +
                         "\n  Got output: " + output
                  << std::endl;
        EXPECT_TRUE(output.empty())
            << "Expected null output but got: " << output;
      }
    } else if (expected_output.type() == ondemand::json_type::string) {
      std::string_view str_expected_output;
      try {
        str_expected_output = expected_output.get_string();
      } catch (const simdjson_error& e) {
        ADD_FAILURE() << "Failed to get expected output string: " << e.what();
        continue;
      }

      if (str_expected_output != output) {
        std::cout << "\n  Test case: " + json_string +
                         "\n  Got output: " + output
                  << std::endl;
        EXPECT_EQ(str_expected_output, output)
            << "Output doesn't match expected value for input: " << input;
      }
    }
  }
}

// WPT toascii.json — assorted IDNA / domain-to-ASCII cases that IdnaTestV2 may
// not cover. Vendored from web-platform-tests (also used by Ada).
// Mirrors Ada's unicode::to_ascii: to_ascii + forbidden-domain-code-point
// filter. See
// https://github.com/web-platform-tests/wpt/blob/master/url/resources/toascii.json
TEST(ToAsciiWpt, ToAscii) {
  const char* filename = "fixtures/toascii.json";
  if (!std::filesystem::exists(filename)) {
    GTEST_SKIP() << "Test file not found: " << filename;
  }

  ondemand::parser parser;
  padded_string json;
  try {
    json = padded_string::load(filename);
    std::cout << "  loaded " << filename << " (" << json.size() << " B)"
              << std::endl;
  } catch (const simdjson_error& e) {
    FAIL() << "Failed to load JSON file: " << e.what();
  }

  ondemand::document doc;
  try {
    doc = parser.iterate(json);
  } catch (const simdjson_error& e) {
    FAIL() << "Failed to parse JSON: " << e.what();
  }

  size_t cases = 0;
  for (auto element : doc.get_array()) {
    if (element.type() == ondemand::json_type::string) {
      continue;  // comments
    }

    ondemand::object object;
    try {
      object = element.get_object();
    } catch (const simdjson_error& e) {
      ADD_FAILURE() << "Failed to get object: " << e.what();
      continue;
    }

    auto json_string =
        std::string(std::string_view(simdjson::to_json_string(object)));

    std::string_view input;
    try {
      input = object["input"].get_string();
    } catch (const simdjson_error& e) {
      // Invalid Unicode in the JSON string — skip (Ada does the same).
      std::cout << "  skip invalid input encoding: " << e.what()
                << " for: " << json_string << std::endl;
      continue;
    }

    SCOPED_TRACE("Test case: " + json_string);

    std::string output = ada::idna::to_ascii(input);
    if (ada::idna::contains_forbidden_domain_code_point(output)) {
      output = "";
    }

    auto expected_output = object["output"];
    ++cases;

    if (expected_output.is_null()) {
      EXPECT_TRUE(output.empty())
          << "Expected failure (null) for input but got: " << output
          << " case: " << json_string;
    } else if (expected_output.type() == ondemand::json_type::string) {
      std::string_view str_expected_output;
      try {
        str_expected_output = expected_output.get_string();
      } catch (const simdjson_error& e) {
        ADD_FAILURE() << "Failed to get expected output string: " << e.what();
        continue;
      }
      EXPECT_EQ(output, str_expected_output)
          << "toascii mismatch for case: " << json_string;

      // Idempotency on successful results: applying to_ascii again is stable.
      if (!output.empty()) {
        std::string again = ada::idna::to_ascii(output);
        if (ada::idna::contains_forbidden_domain_code_point(again)) {
          again = "";
        }
        EXPECT_EQ(again, output) << "to_ascii not idempotent for: " << output;
      }
    }
  }
  EXPECT_GT(cases, 50u) << "expected toascii.json to contribute many cases";
  std::cout << "  ran " << cases << " toascii.json cases" << std::endl;
}