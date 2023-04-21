#include "ada/idna/mapping.h"

#include <algorithm>
#include <array>
#include <string>

#include "mapping_tables.cpp"

namespace ada::idna {

// This can be greatly accelerated. For now we just use a simply
// binary search. In practice, you should *not* do that.
uint32_t find_range_index(uint32_t key) {
  ////////////////
  // This could be implemented with std::lower_bound, but we roll our own
  // because we want to allow further optimizations in the future.
  ////////////////
  uint32_t len = std::size(table);
  uint32_t low = 0;
  uint32_t high = len - 1;
  while (low <= high) {
    uint32_t middle_index = (low + high) >> 1;  // cannot overflow
    uint32_t middle_value = table[middle_index][0];
    if (middle_value < key) {
      low = middle_index + 1;
    } else if (middle_value > key) {
      high = middle_index - 1;
    } else {
      return middle_index;  // perfect match
    }
  }
  return low == 0 ? 0 : low - 1;
}

bool ascii_has_upper_case(char* input, size_t length) {
  auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101ull * v; };
  uint64_t broadcast_80 = broadcast(0x80);
  uint64_t broadcast_Ap = broadcast(128 - 'A');
  uint64_t broadcast_Zp = broadcast(128 - 'Z' - 1);
  size_t i = 0;

  uint64_t runner{0};

  for (; i + 7 < length; i += 8) {
    uint64_t word{};
    memcpy(&word, input + i, sizeof(word));
    runner |= (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80);
  }
  if (i < length) {
    uint64_t word{};
    memcpy(&word, input + i, length - i);
    runner |= (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80);
  }
  return runner != 0;
}

void ascii_map(char* input, size_t length) {
  auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101ull * v; };
  uint64_t broadcast_80 = broadcast(0x80);
  uint64_t broadcast_Ap = broadcast(128 - 'A');
  uint64_t broadcast_Zp = broadcast(128 - 'Z' - 1);
  size_t i = 0;

  for (; i + 7 < length; i += 8) {
    uint64_t word{};
    memcpy(&word, input + i, sizeof(word));
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    memcpy(input + i, &word, sizeof(word));
  }
  if (i < length) {
    uint64_t word{};
    memcpy(&word, input + i, length - i);
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    memcpy(input + i, &word, length - i);
  }
}

// Map the characters according to IDNA, returning the empty string on error.
std::u32string map(std::u32string_view input) {
  //  [Map](https://www.unicode.org/reports/tr46/#ProcessingStepMap).
  //  For each code point in the domain_name string, look up the status
  //  value in Section 5, [IDNA Mapping
  //  Table](https://www.unicode.org/reports/tr46/#IDNA_Mapping_Table),
  //  and take the following actions:
  //    * disallowed: Leave the code point unchanged in the string, and
  //    record that there was an error.
  //    * ignored: Remove the code point from the string. This is
  //    equivalent to mapping the code point to an empty string.
  //    * mapped: Replace the code point in the string by the value for
  //    the mapping in Section 5, [IDNA Mapping
  //    Table](https://www.unicode.org/reports/tr46/#IDNA_Mapping_Table).
  //    * valid: Leave the code point unchanged in the string.
  static std::u32string error = U"";
  std::u32string answer;
  answer.reserve(input.size());
  for (char32_t x : input) {
    size_t index = find_range_index(x);
    uint32_t descriptor = table[index][1];
    uint8_t code = uint8_t(descriptor);
    switch (code) {
      case 0:
        break;  // nothing to do, ignored
      case 1:
        answer.push_back(x);  // valid, we just copy it to output
        break;
      case 2:
        return error;  // disallowed
        break;

      // case 3 :
      default:
        // We have a mapping
        {
          size_t char_count = (descriptor >> 24);
          uint16_t char_index = uint16_t(descriptor >> 8);
          for (size_t idx = char_index; idx < char_index + char_count; idx++) {
            answer.push_back(mappings[idx]);
          }
        }
    }
  }
  return answer;
}
}  // namespace ada::idna
