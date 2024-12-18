#ifndef ADA_IDNA_IDENTIFIER_H
#define ADA_IDNA_IDENTIFIER_H

#include <string>
#include <string_view>

namespace ada::idna {

// Access the first code point of the input string.
// Verify if it is valid name code point given a Unicode code point and a
// boolean first: If first is true return the result of checking if code point
// is contained in the IdentifierStart set of code points. Otherwise return the
// result of checking if code point is contained in the IdentifierPart set of
// code points. Returns false if the input is empty or the code point is not
// valid. There is minimal Unicode error handling: the input should be valid
// UTF-8. https://urlpattern.spec.whatwg.org/#is-a-valid-name-code-point
bool valid_name_code_point(std::string_view input, bool first);

}  // namespace ada::idna

#endif
