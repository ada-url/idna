#ifndef ADA_IDNA_MAPPING_H
#define ADA_IDNA_MAPPING_H

#include <string>
#include <string_view>
namespace ada::idna {

uint32_t constexpr find_range_index(uint32_t key) noexcept;

// If the input is ascii, then the mapping is just -> lower case.
void constexpr ascii_map(char* input, size_t length) noexcept;
// check whether an ascii string needs mapping
bool constexpr ascii_has_upper_case(char* input, size_t length) noexcept;
// Map the characters according to IDNA, returning the empty string on error.
std::u32string map(std::u32string_view input);

}  // namespace ada::idna

#endif
