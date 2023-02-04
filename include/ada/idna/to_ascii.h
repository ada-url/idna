
#ifndef ADA_IDNA_PUNYCODE_H
#define ADA_IDNA_PUNYCODE_H

#include <string>
#include <string_view>

namespace ada::idna {
    // Converts a domain (e.g., www.google.com) possibly containing international characters
    // to an ascii domain (with punycode).
    // We return "" on error. For now.
    std::string to_ascii(std::string_view ut8_string)
}

#endif // ADA_IDNA_PUNYCODE_H