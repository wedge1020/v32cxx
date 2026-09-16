// Exercises bitwise operators: &, |, ^, <<, >>, unary ~ (already
// supported before this round), and their compound-assignment forms.
// Also exercises the classic C precedence gotcha directly, not just as
// a claim: equality binds TIGHTER than bitwise-and in real C, so
// "a & b == b" parses as "a & (b == b)", NOT "(a & b) == b" -- a
// famous footgun. gotcha's own value proves which reading this
// project's build actually used: "8 & (8 == 8)" is "8 & 1" = 0, while
// the WRONG reading "(8 & 8) == 8" would be "8 == 8" = 1 -- the two
// readings genuinely diverge here, so the generated value itself
// confirms the precedence, not just the parenthesization.
//
// Expected values, computed by hand:
//   masked   = 12 & 10 = 0b1100 & 0b1010 = 0b1000 = 8
//   combined = 12 | 3  = 0b1100 | 0b0011 = 0b1111 = 15
//   toggled  = 15 ^ 5  = 0b1111 ^ 0b0101 = 0b1010 = 10
//   shifted  = (1 << 4) = 16, then >> 2 = 4
//   inverted = ~0 = -1 (all bits set, two's complement)
//   gotcha   = 8 & 8 == 8  -->  8 & (8 == 8)  -->  8 & 1  =  0

void main() {
    int masked = 12 & 10;
    int combined = 12 | 3;
    int toggled = 15 ^ 5;
    int shifted = 1 << 4;
    shifted = shifted >> 2;
    int inverted = ~0;

    int gotcha = 8 & 8 == 8;

    int accum = 5;
    accum &= 6;
    accum |= 1;
    accum ^= 3;
    accum <<= 2;
    accum >>= 1;
}
