#ifndef ADA_IDNA_PUNYCODE_H
#define ADA_IDNA_PUNYCODE_H

#include <string>
#include <string_view>

namespace ada::idna {

static constexpr int32_t char_to_digit_value(char value);
static constexpr char digit_to_char(int32_t digit);
static constexpr int32_t adapt(int32_t d, int32_t n, bool firsttime);

bool punycode_to_utf32(std::string_view input, std::u32string& out);
bool verify_punycode(std::string_view input);
bool utf32_to_punycode(std::u32string_view input, std::string& out);

}  // namespace ada::idna

#endif  // ADA_IDNA_PUNYCODE_H
