#include "unicode_transcoding.cpp"
#include "mapping.cpp"
#include "normalization.cpp"
#include "punycode.cpp"
#include "validity.cpp"
#include "to_ascii.cpp"
#include "to_unicode.cpp"
#include "identifier.cpp"
// tables_init.cpp is a separate TU (compiled -Os) so the compressed blob and
// inflater stay out of the hot -O3 object.
