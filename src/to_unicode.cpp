#include "ada/idna/to_unicode.h"

#include <algorithm>
#include <string>

#include "ada/idna/mapping.h"
#include "ada/idna/normalization.h"
#include "ada/idna/punycode.h"
#include "ada/idna/to_ascii.h"
#include "ada/idna/unicode_transcoding.h"
#include "ada/idna/validity.h"
#include "idna.h"

#ifdef ADA_USE_SIMDUTF
#include "simdutf.h"
#endif

namespace ada::idna {
std::string to_unicode(std::string_view input) {
  std::string output;
  output.reserve(input.size());

  size_t label_start = 0;
  std::u32string tmp_buffer;
  std::u32string post_map;
  while (label_start < input.size()) {
    size_t loc_dot = input.find('.', label_start);
    bool is_last_label = (loc_dot == std::string_view::npos);
    size_t label_size =
        is_last_label ? input.size() - label_start : loc_dot - label_start;
    auto label_view = std::string_view(input.data() + label_start, label_size);

    if (label_view.starts_with("xn--") && ada::idna::is_ascii(label_view)) {
      label_view.remove_prefix(4);
      tmp_buffer.clear();
      if (ada::idna::punycode_to_utf32(label_view, tmp_buffer)) {
        // Per UTS #46, the decoded label must be re-validated. Reject decodings
        // that are pure ASCII (xn-- encoding of an ASCII-only label), or whose
        // mapping/normalization is not stable, or that fail label validity.
        bool accept_decoded = true;
        if (ada::idna::is_ascii(tmp_buffer)) {
          accept_decoded = false;
        } else {
          post_map.clear();
          if (!ada::idna::map(tmp_buffer, post_map) || post_map != tmp_buffer) {
            accept_decoded = false;
          } else {
            ada::idna::normalize(post_map);
            if (post_map != tmp_buffer || post_map.empty() ||
                !ada::idna::is_label_valid(post_map)) {
              accept_decoded = false;
            }
          }
        }

        if (accept_decoded) {
#ifdef ADA_USE_SIMDUTF
          auto utf8_size = simdutf::utf8_length_from_utf32(tmp_buffer.data(),
                                                           tmp_buffer.size());
          size_t old_size = output.size();
          output.resize(old_size + utf8_size);
          simdutf::convert_utf32_to_utf8(tmp_buffer.data(), tmp_buffer.size(),
                                         output.data() + old_size);
#else
          auto utf8_size = ada::idna::utf8_length_from_utf32(tmp_buffer.data(),
                                                             tmp_buffer.size());
          size_t old_size = output.size();
          output.resize(old_size + utf8_size);
          ada::idna::utf32_to_utf8(tmp_buffer.data(), tmp_buffer.size(),
                                   output.data() + old_size);
#endif
        } else {
          // ToUnicode never fails. If any step fails, return the original
          // input sequence for the label.
          output.append(
              std::string_view(input.data() + label_start, label_size));
        }
      } else {
        // ToUnicode never fails.  If any step fails, then the original input
        // sequence is returned immediately in that step.
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
