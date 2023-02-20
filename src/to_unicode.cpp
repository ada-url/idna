#include "ada/idna/to_unicode.h"

#include <algorithm>
#include <string>

#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"

namespace ada::idna {
std::string to_unicode(std::string_view input) {
  std::string output;
  output.reserve(input.size());

  const char* label_start = input.data();
  const char* input_end = input.data() + input.size();

  while (label_start < input_end) {
    const char* loc_dot = std::find(label_start, input_end, '.');
    bool is_last_label = (loc_dot == input_end);
    size_t label_size =
        is_last_label ? (input_end - label_start) : (loc_dot - label_start);

    auto label_view = std::string_view(label_start, label_size);

    if (label_view.find("xn--") == 0) {
      label_view.remove_prefix(4);
      if (ada::idna::verify_punycode(label_view)) {
        std::u32string tmp_buffer;
        if (ada::idna::punycode_to_utf32(label_view, tmp_buffer)) {
          auto utf8_size = ada::idna::utf8_length_from_utf32(tmp_buffer.data(),
                                                             tmp_buffer.size());
          std::string finalutf8(utf8_size, '\0');
          ada::idna::utf32_to_utf8(tmp_buffer.data(), tmp_buffer.size(),
                                   finalutf8.data());
          output.append(finalutf8);
        } else {
          // ToUnicode never fails.  If any step fails, then the original input
          // sequence is returned immediately in that step.
          output.append(label_view);
        }
      }
    } else {
      output.append(label_view);
    }

    if (!is_last_label) {
      output.push_back('.');
    }

    label_start = loc_dot + 1;
  }

  return output;
}
}  // namespace ada::idna