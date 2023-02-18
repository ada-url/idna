#ifndef ADA_IDNA_UTILS_H
#define ADA_IDNA_UTILS_H

namespace ada::idna::utils {

bool constexpr begins_with(std::u32string_view view,
                           std::u32string_view prefix);
bool constexpr begins_with(std::string_view view, std::string_view prefix);

}  // namespace ada::idna::utils

#endif  // ADA_IDNA_UTILS_H