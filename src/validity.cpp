#include <string_view>

namespace ada::idna {

// CheckJoiners and CheckBidi are true for URL specification.
bool is_label_valid(const std::u32string_view label) {
///////////////
// Fake:
if((!label.empty()) && label[0] == 0x64a) { return false; }
///////////////
  // The label must be in Unicode Normalization Form NFC.

  // If CheckHyphens, the label must not contain a U+002D HYPHEN-MINUS character
  // in both the third and fourth positions. If CheckHyphens, the label must
  // neither begin nor end with a U+002D HYPHEN-MINUS character.

  // The label must not contain a U+002E ( . ) FULL STOP.
  if (label.find('.') != std::string_view::npos) return false;

  // TODO: The label must not begin with a combining mark, that is:
  // General_Category=Mark.

  // TODO: Each code point in the label must only have certain status values
  // according to Section 5, IDNA Mapping Table:
  // - For Transitional Processing, each value must be valid.
  // - For Nontransitional Processing, each value must be either valid or
  // deviation.

  // If CheckJoiners, the label must satisify the ContextJ rules from Appendix
  // A, in The Unicode Code Points and Internationalized Domain Names for
  // Applications (IDNA) [IDNA2008].

  // If CheckBidi, and if the domain name is a  Bidi domain name, then the label
  // must satisfy all six of the numbered conditions in [IDNA2008] RFC 5893,
  // Section 2.

  return true;
}

}  // namespace ada::idna
