#include "ada/idna/to_ascii.h"

#include <cstdint>

#include "ada/idna/mapping.h"
#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"
#include "ada/idna/normalization.h"
#include "ada/idna/validity.h"


namespace ada::idna {

bool begins_with(std::u32string_view view, std::u32string_view prefix) {
  if (view.size() < prefix.size()) {
    return false;
  }
  return view.substr(0, prefix.size()) == prefix;
}


bool is_ascii(std::u32string_view view) {
  for(uint32_t c : view) { if(c>=0x80) { return false; } }
  return true;
}

// We return "" on error. For now.
std::string to_ascii(std::string_view ut8_string) {
  // If the string is pure ascii, then **we do not need** to convert to
  // UTF-32 and could use a faster path where we only do verify_punycode
  // where needed. Though we may need to do mapping and check for
  // forbidden characters.
  static const std::string error = "";
  // We convert to UTF-32
  size_t utf32_length =
      ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  std::u32string utf32(utf32_length, '\0');
  size_t actual_utf32_length = ada::idna::utf8_to_utf32(ut8_string.data(), ut8_string.size(), utf32.data());
  // mapping
  utf32 = ada::idna::map(utf32);
  normalize(utf32);
  std::string out;
  size_t label_start = 0;

  while (label_start != utf32.size()) {
    size_t loc_dot = utf32.find('.', label_start);
    bool is_last_label = (loc_dot == std::string_view::npos);
    size_t label_size =
        is_last_label ? utf32.size() - label_start : loc_dot - label_start;
    size_t label_size_with_dot = is_last_label ? label_size : label_size + 1;
    std::u32string_view label_view(utf32.data() + label_start, label_size);
    label_start += label_size_with_dot;
    if (label_size == 0) {
      // empty label? Nothing to do.
    } else if (begins_with(label_view, U"xn--") ||
               begins_with(label_view, U"XN--") ||
               begins_with(label_view, U"Xn--") ||
               begins_with(label_view, U"xN--")) {
      for (char32_t c : label_view) {
        if (c >= 0x80) {
          return error;
        }
        out += (unsigned char)(c);
      }
      std::string_view puny_segment_ascii(out.data() + out.size() - label_view.size() + 4,
                                          label_view.size() - 4);
      std::u32string tmp_buffer;
      ada::idna::punycode_to_utf32(puny_segment_ascii, tmp_buffer);
      tmp_buffer = ada::idna::map(tmp_buffer);
      normalize(tmp_buffer);
      if(tmp_buffer.empty()) { return error; }
      if(!is_label_valid(tmp_buffer)) { return error; }
    } else {
      if(!is_label_valid(label_view)) { return error; }
      if(is_ascii(label_view)) {
        for (char32_t c : label_view) {
          out += (unsigned char)(c);
        }

      } else {
        out.append("xn--");
        ada::idna::utf32_to_punycode(label_view, out);

      }
    }
    if (!is_last_label) {
      out.push_back('.');
    }
  }
  return out;
}
}  // namespace ada::idna