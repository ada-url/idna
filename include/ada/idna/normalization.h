#ifndef ADA_IDNA_NORMALIZATION_H
#define ADA_IDNA_NORMALIZATION_H

#include <string>
#include <string_view>

namespace ada::idna {

// Normalize the characters according to IDNA (Unicode Normalization Form C).
// Returns false if the internal Unicode tables could not be loaded; in that
// case `input` is left unchanged.
[[nodiscard]] bool normalize(std::u32string& input);

}  // namespace ada::idna
#endif
