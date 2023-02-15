#ifndef ADA_IDNA_NORMALIZATION_H
#define ADA_IDNA_NORMALIZATION_H

#include <string>
#include <string_view>

namespace ada::idna {

// Normalize the characters according to IDNA (Unicode Normalization Form C).
void normalize(std::u32string& input);

}  // namespace ada::idna
#endif
