// Exercises hex, octal, and binary integer literals, plus integer
// literal suffixes (u/U, l/L, and combinations) -- none of which
// existed in this project's own lexer before this round. Binary
// literals (0b...) are a C++14 addition, not part of every C++
// standard -- accepted here as a convenience regardless (see the
// README/man page for the same note).
//
// Expected values, computed by hand:
//   hexVal      = 0x1F  = 31
//   octVal      = 013   = 11  (1*8 + 3)
//   binVal      = 0b1010 = 10
//   suffixedU   = 42u   -> 42  (suffix ignored, one integer type here)
//   suffixedL   = 100L  -> 100
//   suffixedUL  = 7ul   -> 7
//   hexSuffixed = 0xFFu -> 255

void main() {
    int hexVal = 0x1F;
    int octVal = 013;
    int binVal = 0b1010;
    int suffixedU = 42u;
    int suffixedL = 100L;
    int suffixedUL = 7ul;
    int hexSuffixed = 0xFFu;
}
