#include "ada/idna/identifier.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>

#include "table_store.hpp"
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
  ensure_tables();

  // Fast paths
  if (first && (code_point == U'$' || code_point == U'_' ||
                is_ascii_letter(code_point))) {
    return true;
  }
  if (!first && (code_point == U'$' || is_ascii_letter_or_digit(code_point))) {
    return true;
  }

  // Minimal error handling for invalid code point
  if (code_point > 0x10FFFF) {
    return false;
  }
  if (code_point >= 0xD800 && code_point <= 0xDFFF) {
    return false;
  }

  // Slow path: binary search through the appropriate Unicode range table.
  // Each entry is a [low, high] inclusive range.
  const std::span<const uint32_t[2]> ranges =
      first ? std::span<const uint32_t[2]>{ada::idna::id_start,
                                           ada::idna::id_start_count}
            : std::span<const uint32_t[2]>{ada::idna::id_continue,
                                           ada::idna::id_continue_count};

  const auto iter = std::ranges::lower_bound(
      ranges, code_point, {},
      [](const auto& range) { return range[1]; });  // project to range-high

  return iter != ranges.end() && code_point >= (*iter)[0];
}
}  // namespace ada::idna