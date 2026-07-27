// Cold path amalgamation (built -Os): support algorithms + table inflate/expand.
// Combined into one TU so the static archive does not pay per-object overhead
// three times for aux + tables_init + large blob.
#include "idna_aux.cpp"
#include "tables_init.cpp"
