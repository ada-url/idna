#include "ada/idna/identifier.h"

#include <algorithm>
#include <array>
#include <string>

#include "id_tables.cpp"

namespace ada::idna {
// return 0xffffffff in case of error
// We do not fully validate the input
uint32_t get_first_code_point(std::string_view input) {
  constexpr uint32_t error = 0xffffffff;
  // Check if the input is empty
  if (input.empty()) {
    return error;
  }

  uint32_t code_point = 0;
  size_t number_bytes = 0;
  unsigned char first_byte = input[0];

  if ((first_byte & 0x80) == 0) {
    // 1-byte character (ASCII)
    return first_byte;
  } else if ((first_byte & 0xE0) == 0xC0) {
    // 2-byte character
    code_point = first_byte & 0x1F;
    number_bytes = 2;
  } else if ((first_byte & 0xF0) == 0xE0) {
    // 3-byte character
    code_point = first_byte & 0x0F;
    number_bytes = 3;
  } else if ((first_byte & 0xF8) == 0xF0) {
    // 4-byte character
    code_point = first_byte & 0x07;
    number_bytes = 4;
  } else {
    return error;
  }

  // Decode the remaining bytes
  for (size_t i = 1; i < number_bytes; ++i) {
    if (i >= input.size()) {
      return error;
    }
    unsigned char byte = input[i];
    if ((byte & 0xC0) != 0x80) {
      return error;
    }
    code_point = (code_point << 6) | (byte & 0x3F);
  }
  return code_point;
}

bool is_ascii_letter(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_ascii_letter_or_digit(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9');
}

bool valid_name_code_point(std::string_view input, bool first) {
  // https://urlpattern.spec.whatwg.org/#is-a-valid-name-code-point
  if (input.empty()) {
    return false;
  }
  // https://tc39.es/ecma262/#prod-IdentifierStart
  // Fast paths:
  if (first &&
      (input[0] == '$' || input[0] == '_' || is_ascii_letter(input[0]))) {
    return true;
  }
  if (!first && (input[0] == '$' || is_ascii_letter_or_digit(input[0]))) {
    return true;
  }
  // Slow path...
  uint32_t code_point = get_first_code_point(input);
  if (code_point == 0xffffffff) {
    return false;  // minimal error handling
  }
  if (first) {
    auto iter = std::lower_bound(
        std::begin(ada::idna::id_start), std::end(ada::idna::id_start),
        code_point, [](const uint32_t* range, uint32_t code_point) {
          return range[1] < code_point;
        });
    return iter != std::end(id_start) && code_point >= (*iter)[0];
  } else {
    auto iter = std::lower_bound(
        std::begin(id_continue), std::end(id_continue), code_point,
        [](const uint32_t* range, uint32_t code_point) {
          return range[1] < code_point;
        });
    return iter != std::end(id_start) && code_point >= (*iter)[0];
  }
}
}  // namespace ada::idna