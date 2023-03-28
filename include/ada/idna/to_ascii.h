#ifndef ADA_IDNA_TO_ASCII_H
#define ADA_IDNA_TO_ASCII_H

#include <string>
#include <string_view>

namespace ada::idna {
// Converts a domain (e.g., www.google.com) possibly containing international
// characters to an ascii domain (with punycode). It will not do percent
// decoding: percent decoding should be done prior to calling this function. We
// do not remove tabs and spaces, they should have been removed prior to calling
// this function. We also do not trim control characters. We also assume that
// the input is not empty. We return "" on error. For now.
std::string to_ascii(std::string_view ut8_string);

bool constexpr begins_with(std::u32string_view view,
                           std::u32string_view prefix);
bool constexpr begins_with(std::string_view view, std::string_view prefix);

bool constexpr is_ascii(std::u32string_view view);
bool constexpr is_ascii(std::string_view view);

std::string from_ascii_to_ascii(std::string_view ut8_string);

}  // namespace ada::idna

#endif  // ADA_IDNA_TO_ASCII_H
