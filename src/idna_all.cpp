// Single-TU amalgamation for the static library: one object, smaller .a.
// Hot sources first (still benefit from inlining across the TU under -Os).
#include "idna_hot.cpp"
#include "idna_cold.cpp"
