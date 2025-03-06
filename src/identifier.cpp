#include "ada/idna/identifier.h"

#include <algorithm>
#include <array>
#include <string>

#include "id_tables.cpp"

namespace ada::idna {
constexpr bool is_ascii_letter(char32_t c) noexcept {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

constexpr bool is_ascii_letter_or_digit(char32_t c) noexcept {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9');
}

bool valid_name_code_point(char32_t code_point, bool first) {
  // https://tc39.es/ecma262/#prod-IdentifierStart
  // Fast paths:
  if (first &&
      (code_point == '$' || code_point == '_' || is_ascii_letter(code_point))) {
    return true;
  }
  if (!first && (code_point == '$' || is_ascii_letter_or_digit(code_point))) {
    return true;
  }
  // Slow path...
  if (code_point == 0xffffffff) {
    return false;  // minimal error handling
  }
  if (first) {
    auto iter = std::lower_bound(
        std::begin(ada::idna::id_start), std::end(ada::idna::id_start),
        code_point,
        [](const uint32_t* range, uint32_t cp) { return range[1] < cp; });
    return iter != std::end(id_start) && code_point >= (*iter)[0];
  } else {
    auto iter = std::lower_bound(
        std::begin(id_continue), std::end(id_continue), code_point,
        [](const uint32_t* range, uint32_t cp) { return range[1] < cp; });
    return iter != std::end(id_start) && code_point >= (*iter)[0];
  }
}
}  // namespace ada::idna