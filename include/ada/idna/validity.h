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
 * Same check for a label of a Bidi domain name: with bidi_domain set, the
 * RFC 5893 conditions apply even when the label has no RTL code point.
 */
bool is_label_valid(std::u32string_view label, bool bidi_domain);

/**
 * True when the domain contains a code point of Bidi class R, AL or AN, which
 * makes it a Bidi domain name.
 * @see https://www.rfc-editor.org/rfc/rfc5893#section-2
 */
bool is_bidi_domain(std::u32string_view domain);

}  // namespace ada::idna

#endif  // ADA_IDNA_VALIDITY_H
