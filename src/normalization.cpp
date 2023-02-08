#include "ada/idna/normalization.h"

#include "unilib/uninorms.h"
#include "unilib/uninorms.cpp"

void normalize(std::u32string& input) {
    //    [Normalize](https://www.unicode.org/reports/tr46/#ProcessingStepNormalize). Normalize
    //     the domain_name string to Unicode Normalization Form C.
    ufal::unilib::uninorms::nfc(input);
}
