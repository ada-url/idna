#ifndef ADA_IDNA_LIMITS_H
#define ADA_IDNA_LIMITS_H

#include <cstddef>

namespace ada::idna {

// Maximum accepted UTF-8 domain length for to_ascii / to_unicode.
// Bounds heap growth under untrusted input (DoS resistance). DNS wire limits
// are smaller; this allows long Unicode labels used in URL tests/fixtures.
inline constexpr size_t max_domain_input_bytes = 16384;

}  // namespace ada::idna

#endif  // ADA_IDNA_LIMITS_H
