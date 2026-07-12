#ifndef ADA_IDNA_TO_ASCII_H
#define ADA_IDNA_TO_ASCII_H

#include <string>
#include <string_view>

#include "ada/idna/limits.h"

namespace ada::idna {

// Converts a domain (e.g., www.google.com) possibly containing international
// characters to an ascii domain (with punycode). It will not do percent
// decoding: percent decoding should be done prior to calling this function. We
// do not remove tabs and spaces, they should have been removed prior to calling
// this function. We also do not trim control characters. We also assume that
// the input is not empty. We return "" on error. Inputs longer than
// max_domain_input_bytes are rejected.
//
// This function may accept or even produce invalid domains (WHATWG carve-outs).
std::string to_ascii(std::string_view ut8_string);

// Same as to_ascii, but writes into `out` and returns false on error without
// relying on empty-string ambiguity.
[[nodiscard]] bool to_ascii(std::string_view ut8_string, std::string& out);

// Returns true if the string contains a forbidden code point according to the
// WHATGL URL specification:
// https://url.spec.whatwg.org/#forbidden-domain-code-point
bool contains_forbidden_domain_code_point(std::string_view ascii_string);

bool constexpr is_ascii(std::u32string_view view);
bool constexpr is_ascii(std::string_view view);

}  // namespace ada::idna

#endif  // ADA_IDNA_TO_ASCII_H
