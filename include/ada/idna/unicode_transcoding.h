#ifndef ADA_IDNA_UNICODE_TRANSCODING_H
#define ADA_IDNA_UNICODE_TRANSCODING_H

#include <string>
#include <string_view>

namespace ada::idna {

size_t utf8_to_utf32(const char* buf, size_t len, char32_t* utf32_output);

size_t utf8_length_from_utf32(const char32_t* buf, size_t len);

size_t utf32_length_from_utf8(const char* buf, size_t len);

size_t utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output);

}

#endif // ADA_IDNA_UNICODE_TRANSCODING_H