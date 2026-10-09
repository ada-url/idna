#ifndef ADA_IDNA_VALIDITY_H
#define ADA_IDNA_VALIDITY_H

#include <string>
#include <string_view>

namespace ada::idna {

/**
 * @see https://www.unicode.org/reports/tr46/#Validity_Criteria
 */
bool is_label_valid(std::u32string_view label);

/**
 * Same as above, but with the CheckBidi decision taken from the whole domain:
 * once the domain contains at least one RTL label (a "Bidi domain name"),
 * RFC 5893 Section 2 applies to every label, including all-ASCII ones.
 */
bool is_label_valid(std::u32string_view label, bool bidi_domain);

/**
 * True if the label contains a code point of Bidi class R, AL or AN, i.e. it
 * is an RTL label per RFC 5893 Section 2.
 */
bool has_rtl_characters(std::u32string_view label);

}  // namespace ada::idna

#endif  // ADA_IDNA_VALIDITY_H
