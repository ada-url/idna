#include "ada/idna/to_unicode.h"

#include <string>
#include <algorithm>

#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"

namespace ada::idna {
std::string to_unicode(const std::string_view& input) {
  std::string output;
  output.reserve(input.size());

  std::u32string label_buffer;
  label_buffer.reserve(63);  // Maximum length of a Punycode-encoded label.

  const char* label_start = input.data();
  const char* input_end = input.data() + input.size();

  while (label_start < input_end) {
    const char* loc_dot = std::find(label_start, input_end, '.');
    bool is_last_label = (loc_dot == input_end);
    size_t label_size =
        is_last_label ? (input_end - label_start) : (loc_dot - label_start);

    if (label_size == 0) {
      // Empty label - nothing to do.
    } else if (ada::idna::verify_punycode(
                   std::string_view(label_start, label_size))) {
      label_buffer.clear();
      if (ada::idna::punycode_to_utf32(
              std::string_view(label_start, label_size), label_buffer)) {
        output.append(reinterpret_cast<const char*>(label_buffer.data()),
                      label_buffer.size() * sizeof(char32_t));
      } else {
        // ToUnicode never fails.  If any step fails, then the original input
        // sequence is returned immediately in that step.
        output.append(label_start, label_start + label_size);
      }
    } else {
      output.append(label_start, label_start + label_size);
    }

    if (!is_last_label) {
      output.push_back('.');
    }

    label_start = loc_dot + 1;
  }

  return output;
}
}  // namespace ada::idna