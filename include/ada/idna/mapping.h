#ifndef ADA_IDNA_MAPPING_H
#define ADA_IDNA_MAPPING_H

#include <string>
#include <string_view>

namespace ada::idna {

// If the input is ascii, then the mapping is just -> lower case.
void ascii_map(char* input, size_t length);
// Map the characters according to IDNA, returning the empty string on error.
std::u32string map(std::u32string_view input);
// Map into an existing buffer (cleared on entry). Returns false if any code
// point is disallowed. Reusing the buffer avoids repeated heap allocations
// when called in a loop over multiple labels.
bool map(std::u32string_view input, std::u32string& out);

}  // namespace ada::idna

#endif
