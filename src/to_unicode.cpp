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

// True when an all-ASCII label is valid on its own but violates RFC 5893
// because the domain contains an RTL label (for example a label starting
// with a digit). ToUnicode then returns the original input (UTS #46).
static bool ascii_label_fails_bidi_domain(std::string_view label) {
  std::u32string utf32(label.size(), 0);
  for (size_t i = 0; i < label.size(); i++) {
    utf32[i] = static_cast<unsigned char>(label[i]);
  }
  return check_label(utf32, false) && !check_label(utf32, true);
}

static bool utf8_domain_is_bidi(std::string_view input) {
  std::u32string decoded;
  size_t label_start = 0;
  while (label_start < input.size()) {
    const size_t loc_dot = input.find('.', label_start);
    const bool is_last_label = (loc_dot == std::string_view::npos);
    const size_t label_size =
        is_last_label ? input.size() - label_start : loc_dot - label_start;
    const std::string_view label = input.substr(label_start, label_size);
    if (label.starts_with("xn--") && is_ascii(label)) {
      decoded.clear();
      if (label.size() > 4 && punycode_to_utf32(label.substr(4), decoded) &&
          label_contains_rtl(decoded)) {
        return true;
      }
    } else if (!is_ascii(label)) {
      const size_t utf32_length =
          utf32_length_from_utf8(label.data(), label.size());
      if (utf32_length != 0) {
        decoded.assign(utf32_length, 0);
        const size_t written =
            utf8_to_utf32(label.data(), label.size(), decoded.data());
        if (written == utf32_length && label_contains_rtl(decoded)) {
          return true;
        }
      }
    }
    if (is_last_label) {
      break;
    }
    label_start += label_size + 1;
  }
  return false;
}

[[nodiscard]] bool to_unicode(std::string_view input, std::string& output) {
  output.clear();
  if (input.size() > max_domain_input_bytes) {
    return false;
  }
  // An ASCII label that is only illegal because the domain is a Bidi domain
  // fails ToUnicode for the whole input (UTS #46), not just that label.
  const bool bidi_domain = utf8_domain_is_bidi(input);
  if (bidi_domain) {
    size_t label_start = 0;
    while (label_start < input.size()) {
      const size_t loc_dot = input.find('.', label_start);
      const bool is_last_label = (loc_dot == std::string_view::npos);
      const size_t label_size =
          is_last_label ? input.size() - label_start : loc_dot - label_start;
      const std::string_view label = input.substr(label_start, label_size);
      if (!label.empty() && is_ascii(label) && !label.starts_with("xn--") &&
          ascii_label_fails_bidi_domain(label)) {
        output.assign(input);
        return true;
      }
      if (is_last_label) {
        break;
      }
      label_start += label_size + 1;
    }
  }
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
          } else if (!ada::idna::normalize(post_map) ||
                     post_map != tmp_buffer || post_map.empty() ||
                     !check_label(post_map, bidi_domain)) {
            accept_decoded = false;
          }
        }

        if (accept_decoded) {
#ifdef ADA_USE_SIMDUTF
          auto utf8_size = simdutf::utf8_length_from_utf32(tmp_buffer.data(),
                                                           tmp_buffer.size());
          size_t old_size = output.size();
          output.resize(old_size + utf8_size);
          size_t written = simdutf::convert_utf32_to_utf8(
              tmp_buffer.data(), tmp_buffer.size(), output.data() + old_size);
          if (written != utf8_size) {
            output.resize(old_size);
            output.append(
                std::string_view(input.data() + label_start, label_size));
          }
#else
          auto utf8_size = ada::idna::utf8_length_from_utf32(tmp_buffer.data(),
                                                             tmp_buffer.size());
          size_t old_size = output.size();
          output.resize(old_size + utf8_size);
          size_t written = ada::idna::utf32_to_utf8(
              tmp_buffer.data(), tmp_buffer.size(), output.data() + old_size);
          if (written != utf8_size) {
            output.resize(old_size);
            output.append(
                std::string_view(input.data() + label_start, label_size));
          }
#endif
        } else {
          // ToUnicode never fails. If any step fails, return the original
          // input sequence for the label.
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

  return true;
}

std::string to_unicode(std::string_view input) {
  std::string output;
  if (!to_unicode(input, output)) {
    // Over-limit input: return original (ToUnicode never fails in the sense of
    // producing no answer for valid-length inputs).
    return std::string(input);
  }
  return output;
}
}  // namespace ada::idna
