#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"

#include <cstdint>

namespace ada::idna {

bool begins_with(std::u32string_view view, std::u32string_view prefix) {
  if (view.size() < prefix.size()) {
    return false;
  }
  return view.substr(0, prefix.size()) == prefix;
}

// We return "" on error. For now.
std::string to_ascii(std::string_view ut8_string) {
  // We convert to UTF-32
  size_t utf32_length =
      ada::idna::utf32_length_from_utf8(ut8_string.data(), ut8_string.size());
  std::u32string utf32(utf32_length, '\0');
  ada::idna::utf8_to_utf32(ut8_string.data(), ut8_string.size(), utf32.data());

  // Here we would do extra work such as mapping and so forth. We do not have to
  // do it yet.

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

      // hopefully, this will do UTF-32 to ASCII conversion.
      for (char32_t c : label_view) {
        if (c >= 0x80) {
          return "";
        }
        out += (unsigned char)(c);
      }
      std::string_view puny_segment_ascii(out.data() - label_view.size() + 4,
                                          label_view.size() - 4);
      if (!ada::idna::verify_punycode(puny_segment_ascii)) {
        return "";
      }
    } else {
      // convert the label to punycode and write it out
      ada::idna::utf32_to_punycode(label_view, out);
    }
    if (!is_last_label) {
      out.push_back('.');
    }
  }
  return out;
}
} // namespace ada::idna