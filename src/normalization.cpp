#include "ada/idna/normalization.h"

#include "unilib/uninorms.h"
#include "unilib/uninorms.cpp"

namespace ada::idna {

void normalize(std::u32string& input) {
  /**
   * Normalize the domain_name string to Unicode Normalization Form C.
   * @see https://www.unicode.org/reports/tr46/#ProcessingStepNormalize
   */
  ufal::unilib::uninorms::nfc(input);
}

}  // namespace ada::idna