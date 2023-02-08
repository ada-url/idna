#ifndef ADA_IDNA_MAPPING_H
#define ADA_IDNA_MAPPING_H

#include <string>
#include <string_view>
namespace ada::idna {

// Map the characters according to IDNA, returning the empty string on error.
std::u32string map(std::u32string_view input);

}  // namespace ada::idna

#endif
