
#include "ada/idna/unicode_transcoding.h"

#include <cstdint>
#include <cstring>
namespace ada::idna {

size_t utf8_to_utf32(const char* buf, size_t len, char32_t* utf32_output) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buf);
  size_t pos = 0;
  char32_t* start{utf32_output};
  while (pos < len) {
    // try to convert the next block of 16 ASCII bytes
    if (pos + 16 <= len) {  // if it is safe to read 16 more
                            // bytes, check that they are ascii
      uint64_t v1;
      std::memcpy(&v1, data + pos, sizeof(uint64_t));
      uint64_t v2;
      std::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
      uint64_t v{v1 | v2};
      if ((v & 0x8080808080808080) == 0) {
        size_t final_pos = pos + 16;
        while (pos < final_pos) {
          *utf32_output++ = char32_t(buf[pos]);
          pos++;
        }
        continue;
      }
    }
    uint8_t leading_byte = data[pos];  // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *utf32_output++ = char32_t(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) {
      // We have a two-byte UTF-8
      if (pos + 1 >= len) {
        return 0;
      }  // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      }
      // range check
      uint32_t code_point =
          (leading_byte & 0b00011111) << 6 | (data[pos + 1] & 0b00111111);
      if (code_point < 0x80 || 0x7ff < code_point) {
        return 0;
      }
      *utf32_output++ = char32_t(code_point);
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8
      if (pos + 2 >= len) {
        return 0;
      }  // minimal bound checking

      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return 0;
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00001111) << 12 |
                            (data[pos + 1] & 0b00111111) << 6 |
                            (data[pos + 2] & 0b00111111);
      if (code_point < 0x800 || 0xffff < code_point ||
          (0xd7ff < code_point && code_point < 0xe000)) {
        return 0;
      }
      *utf32_output++ = char32_t(code_point);
      pos += 3;
    } else if ((leading_byte & 0b11111000) == 0b11110000) {  // 0b11110000
      // we have a 4-byte UTF-8 word.
      if (pos + 3 >= len) {
        return 0;
      }  // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((data[pos + 3] & 0b11000000) != 0b10000000) {
        return 0;
      }

      // range check
      uint32_t code_point = (leading_byte & 0b00000111) << 18 |
                            (data[pos + 1] & 0b00111111) << 12 |
                            (data[pos + 2] & 0b00111111) << 6 |
                            (data[pos + 3] & 0b00111111);
      if (code_point <= 0xffff || 0x10ffff < code_point) {
        return 0;
      }
      *utf32_output++ = char32_t(code_point);
      pos += 4;
    } else {
      return 0;
    }
  }
  return utf32_output - start;
}

size_t utf8_length_from_utf32(const char32_t* buf, size_t len) {
  // We are not BOM aware.
  const uint32_t* p = reinterpret_cast<const uint32_t*>(buf);
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    /** ASCII **/
    if (p[i] <= 0x7F) {
      counter++;
    }
    /** two-byte **/
    else if (p[i] <= 0x7FF) {
      counter += 2;
    }
    /** three-byte **/
    else if (p[i] <= 0xFFFF) {
      counter += 3;
    }
    /** four-bytes **/
    else {
      counter += 4;
    }
  }
  return counter;
}

size_t utf32_length_from_utf8(const char* buf, size_t len) {
  const int8_t* p = reinterpret_cast<const int8_t*>(buf);
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    // -65 is 0b10111111, anything larger in two-complement's
    // should start a new code point.
    if (p[i] > -65) {
      counter++;
    }
  }
  return counter;
}

size_t utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output) {
  const uint32_t* data = reinterpret_cast<const uint32_t*>(buf);
  size_t pos = 0;
  char* start{utf8_output};
  while (pos < len) {
    // try to convert the next block of 2 ASCII characters
    if (pos + 2 <= len) {  // if it is safe to read 8 more
                           // bytes, check that they are ascii
      uint64_t v;
      std::memcpy(&v, data + pos, sizeof(uint64_t));
      if ((v & 0xFFFFFF80FFFFFF80) == 0) {
        *utf8_output++ = char(buf[pos]);
        *utf8_output++ = char(buf[pos + 1]);
        pos += 2;
        continue;
      }
    }
    uint32_t word = data[pos];
    if ((word & 0xFFFFFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xFFFFF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xFFFF0000) == 0) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      if (word >= 0xD800 && word <= 0xDFFF) {
        return 0;
      }
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX
      // 0b10XXXXXX
      if (word > 0x10FFFF) {
        return 0;
      }
      *utf8_output++ = char((word >> 18) | 0b11110000);
      *utf8_output++ = char(((word >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    }
  }
  return utf8_output - start;
}
}  // namespace ada::idna