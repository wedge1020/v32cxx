// The arena runs out of space (built with --alloc=arena --heap-words=64):
// the program must stop with a message, not carry on with a bad pointer.
// tests/arena_full.sh builds it as standard C and checks the exit status
// (1) and the message; "survived" must never print.
#include <v32/memory.hpp>

int test_errors = -1;

class Blob {
public:
    int words[8];
    Blob() { words[0] = 1; }
};

int main(void)
{
    for (int i = 0; i < 100; i++) {
        Blob *b = new Blob;
        b->words[1] = i;
    }
    test_errors = 0;
    return 0;
}
