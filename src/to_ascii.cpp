#include "ada/idna/to_ascii.h"

#include <cstdint>

#include "ada/idna/mapping.h"
#include "ada/idna/punycode.h"
#include "ada/idna/unicode_transcoding.h"

namespace ada::idna {

bool begins_with(std::u32string_view view, std::u32string_view prefix) {
  if (view.size() < prefix.size()) {
    return false;
  }
  return view.substr(0, prefix.size()) == prefix;
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
  // To do: utf8_to_utf32 will return zero if the input is invalid
  // UTF-8, we should check.
  ada::idna::utf8_to_utf32(ut8_string.data(), ut8_string.size(), utf32.data());
  // mapping
  utf32 = ada::idna::map(utf32);
  //  * [Normalize](https://www.unicode.org/reports/tr46/#ProcessingStepNormalize). Normalize
  //     the domain_name string to Unicode Normalization Form C. See
  //     https://dev.w3.org/cvsweb/charlint/charlint.pl?rev=1.28;content-type=text%2Fplain
  //     for a Perl script that does it.
  ////////////////////////////////////////////////////
  // TODO: Implement normalization.
  ////////////////////////////////////////////////////
  std::string out;
  size_t label_start = 0;
  //  * [Break](https://www.unicode.org/reports/tr46/#ProcessingStepBreak).
  //  Break the string into labels at U+002E ( . ) FULL STOP.

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
      //    * [If the label starts with
      //    “xn--”](https://www.unicode.org/reports/tr46/#ProcessingStepPunycode):
      //      * Attempt to convert the rest of the label
      //      to Unicode according to Punycode [[RFC3492
      //      (https://www.unicode.org/reports/tr46/#RFC3492)].
      //      If that conversion fails, record that
      //      there was an error, and continue with the
      //      next label. Otherwise replace the original
      //      label in the string by the results of the
      //      conversion.
      //      * Verify that the label meets the validity
      //      criteria in Section 4.1, [Validity
      //      Criteria](https://www.unicode.org/reports/tr46/#Validity_Criteria)
      //      for Nontransitional Processing. If any of
      //      the validity criteria are not satisfied,
      //      record that there was an error.
      ////////////////////////////////////////////////////
      // TODO: current code merely verifies that we have
      // proper punycode. But we should decode and
      // verify the cotent.
      ////////////////////////////////////////////////////
      for (char32_t c : label_view) {
        if (c >= 0x80) {
          return error;
        }
        out += (unsigned char)(c);
      }
      std::string_view puny_segment_ascii(out.data() - label_view.size() + 4,
                                          label_view.size() - 4);
      if (!ada::idna::verify_punycode(puny_segment_ascii)) {
        return error;
      }
    } else {
      //    * [If the label does not start with
      //    “xn--”](https://www.unicode.org/reports/tr46/#ProcessingStepNonPunycode):
      //  Verify that the label meets the validity
      //  criteria in Section 4.1, [Validity
      //  Criteria](https://www.unicode.org/reports/tr46/#Validity_Criteria)
      //  for the input Processing choice (Transitional
      //  or Nontransitional). If any of the validity
      //  criteria are not satisfied, record that there
      //  was an error.
      ////////////////////////////////////////////////////
      // TODO: current code merely encodes to punycode
      // but we should also check the validity criteria.
      ////////////////////////////////////////////////////
      ada::idna::utf32_to_punycode(label_view, out);
    }
    if (!is_last_label) {
      out.push_back('.');
    }
  }
  return out;
}
}  // namespace ada::idna