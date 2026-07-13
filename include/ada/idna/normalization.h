#ifndef ADA_IDNA_NORMALIZATION_H
#define ADA_IDNA_NORMALIZATION_H

#include <string>
#include <string_view>

namespace ada::idna {

// Returns true if `input` is already in Unicode Normalization Form C.
// Requires that internal tables have been loaded (call ensure via normalize
// or map first, or this returns false if tables are unavailable).
[[nodiscard]] bool is_already_nfc(std::u32string_view input) noexcept;

// Normalize the characters according to IDNA (Unicode Normalization Form C).
// Returns false if the internal Unicode tables could not be loaded; in that
// case `input` is left unchanged. Skips work when the string is already NFC.
[[nodiscard]] bool normalize(std::u32string& input);

}  // namespace ada::idna
#endif
