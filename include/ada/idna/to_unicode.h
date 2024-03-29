
#ifndef ADA_IDNA_TO_UNICODE_H
#define ADA_IDNA_TO_UNICODE_H

#include <string_view>

namespace ada::idna {

std::string to_unicode(std::string_view input);

}  // namespace ada::idna

#endif  // ADA_IDNA_TO_UNICODE_H