#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ada/idna/to_ascii.h"
#include "simdjson.h"

using namespace simdjson;

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

bool idna_test_v2_to_ascii(std::string_view filename) {
  ondemand::parser parser;

  padded_string json = padded_string::load(filename);
  std::cout << "  loaded " << filename << " (" << json.size() << " kB)"
            << std::endl;

  ondemand::document doc = parser.iterate(json);
  for (auto element : doc.get_array()) {
    if (element.type() == ondemand::json_type::string) {
      continue;
    }

    ondemand::object object = element.get_object();
    auto json_string =
        std::string(std::string_view(simdjson::to_json_string(object)));
    ondemand::value comment;
    if (!object["comment"].get(comment)) {
      std::string_view comment_string;
      if (!comment.get_string().get(comment_string)) {
        std::cout << "  comment: " << comment.get_string() << std::endl;
      }
    }

    std::string_view input = object["input"].get_string();
    std::string output = ada::idna::to_ascii(input);
    if (ada::idna::contains_forbidden_domain_code_point(output)) {
      output = "";
    }
    auto expected_output = object["output"];

    if (expected_output.is_null() && output.size()) {
      std::cout << "\n  Test case: " + json_string + "\n  Got output: " + output
                << std::endl;
      return false;
    } else if (expected_output.type() == ondemand::json_type::string) {
      std::string_view str_expected_output = expected_output.get_string();
      if (str_expected_output != output) {
        std::cout << "\n  Test case: " + json_string +
                         "\n  Got output: " + output
                  << std::endl;
        return false;
      }
    }
  }

  return true;
}

int main(int argc, char** argv) {
  std::string filename = "fixtures/IdnaTestV2.json";
  if (argc > 1) {
    filename = argv[1];
  }

  if (!file_exists(filename)) {
    return EXIT_FAILURE;
  }

  bool result = idna_test_v2_to_ascii(filename);
  if (!result) return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
