// String-literal array initializers -- both declarator spellings,
// int (Vircon32's native "char") and char element types, escapes,
// and the exact-fit / shorter-than-N cases.

int msg_std[8] = "Hello";      // standard-C declarator, int elements
int [7] msg_v32 = "v32c++";    // Vircon32-native declarator (exact fit:
                               // 5 chars + terminator = 6)
char name[16] = "Vircon32";    // char elements -- valid real C++ too
int esc[8] = "a\tb\\c\"d";     // escape decoding round-trip

void main(void)
{
    // locals, not just globals
    int local[4] = "Hi";

    // terminator + zero-fill visible through reads
    if (msg_std[5] != 0) return;
    if (msg_std[6] != 0) return;  // zero-filled remainder
    if (msg_std[0] != 72) return; // 'H'
    if (esc[1] != 9) return;      // '\t'
    if (local[2] != 0) return;
}
