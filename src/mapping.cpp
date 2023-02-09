#include "ada/idna/mapping.h"

#include <array>

#include "mapping_tables.cpp"

namespace ada::idna {

// This can be greatly accelerated. For now we just use a simply
// binary search. In practice, you should *not* do that.
uint32_t find_range_index(uint32_t key) {
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
          for (size_t idx = char_index; idx < char_index + char_count;
               idx++) {
            answer.push_back(mappings[idx]);
          }
        }
    }
  }
  return answer;
}
}  // namespace ada::idna
