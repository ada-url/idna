#ifndef ADA_IDNA_VALIDITY_H
#define ADA_IDNA_VALIDITY_H

#include <string>
#include <string_view>

namespace ada::idna {

/**
 * @see https://www.unicode.org/reports/tr46/#Validity_Criteria
 */
bool is_label_valid(const std::string_view label);

}  // namespace ada::idna

#endif  // ADA_IDNA_VALIDITY_H