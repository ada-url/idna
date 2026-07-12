#ifndef ADA_IDNA_TO_UNICODE_H
#define ADA_IDNA_TO_UNICODE_H

#include <string>
#include <string_view>

#include "ada/idna/limits.h"

namespace ada::idna {

// UTS #46 ToUnicode. Never fails per the standard: on step failure the original
// label is kept. Inputs longer than max_domain_input_bytes are returned
// unchanged as a safety measure under untrusted input.
std::string to_unicode(std::string_view input);

// Writes into `out`. Returns false only if the input exceeds
// max_domain_input_bytes (out is left empty). Otherwise always returns true
// (ToUnicode does not fail).
[[nodiscard]] bool to_unicode(std::string_view input, std::string& out);

}  // namespace ada::idna

#endif  // ADA_IDNA_TO_UNICODE_H
