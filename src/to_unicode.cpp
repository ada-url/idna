#include "ada/idna/to_unicode.h"

#include <algorithm>
#include <string>

#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"
#include "idna.h"

namespace ada::idna {
std::string to_unicode(std::string_view input) {
  std::string output;
  output.reserve(input.size());

  size_t label_start = 0;
  while (label_start < input.size()) {
    size_t loc_dot = input.find('.', label_start);
    bool is_last_label = (loc_dot == std::string_view::npos);
    size_t label_size =
        is_last_label ? input.size() - label_start : loc_dot - label_start;
    auto label_view = std::string_view(input.data() + label_start, label_size);

    if (ada::idna::begins_with(label_view, "xn--") &&
        ada::idna::is_ascii(label_view)) {
      label_view.remove_prefix(4);
      if (ada::idna::verify_punycode(label_view)) {
        std::u32string tmp_buffer;
        if (ada::idna::punycode_to_utf32(label_view, tmp_buffer)) {
          auto utf8_size = ada::idna::utf8_length_from_utf32(tmp_buffer.data(),
                                                             tmp_buffer.size());
          std::string final_utf8(utf8_size, '\0');
          ada::idna::utf32_to_utf8(tmp_buffer.data(), tmp_buffer.size(),
                                   final_utf8.data());
          output.append(final_utf8);
        } else {
          // ToUnicode never fails.  If any step fails, then the original input
          // sequence is returned immediately in that step.
          output.append(
              std::string_view(input.data() + label_start, label_size));
        }
      } else {
        output.append(std::string_view(input.data() + label_start, label_size));
      }
    } else {
      output.append(label_view);
    }

    if (!is_last_label) {
      output.push_back('.');
    }

    label_start += label_size + 1;
  }

  return output;
}
}  // namespace ada::idna