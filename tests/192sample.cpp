// *****************************************************************************
//  tests/192sample.cpp -- `extern` is read per declaration, and an `extern`
//  declaration is one object with the definition of the same variable.
//
//  The C++ lexer drops `extern`, so the prescan reads it from the text, after
//  macro expansion, and notes each declarator name that follows it (see
//  prescan.c). Vircon32 C has no `extern` and no linker: an `extern` that
//  nothing defines is printed as the variable (once); one that something
//  defines disappears into the definition, wherever that is.
//
//  Self-checking: test_errors must end at 0. The Makefile checks the output.
// *****************************************************************************

int test_errors = -1;

#define EXT extern

extern int g1; int g2 = 2;             // g1: extern, g2: not
int g3 = 3; extern int g4;             // g3: not, g4: extern
EXT int g5;                            // extern through a macro
extern int ml1,
           ml2;                        // one declaration over two lines
int (*fp)(int);  extern int (*efp)(int);   // a function pointer, then an extern one

namespace hw {
    extern int c_side;                 // defined in C: stays `c_side`
    extern int c_def; int mine = 4;    // c_def is defined at file scope below
    int sub = 5; extern int c_two;     // hw__sub, but `c_two`
}

namespace n { extern int v; }          // the same variable as ...
namespace n { int v = 6; }             // ... this one: n__v

namespace p { namespace q { int w = 9; extern int z; } }

int c_def = 8;                         // what hw::c_def means

int twice(int a) { return a * 2; }

int main(void)
{
    int e = 0;
    g1 = 1; g4 = 4; g5 = 5; ml1 = 10; ml2 = 11; hw::c_side = 12; hw::c_two = 13;
    p::q::z = 14;
    efp = fp = twice;

    if (g1 + g2 + g3 + g4 + g5 != 15) e += 1;
    if (ml1 + ml2 + hw::c_side + hw::c_two != 46) e += 2;
    if (hw::c_def != 8 || c_def != 8) e += 4;
    if (hw::mine + hw::sub != 9) e += 8;
    if (n::v != 6) e += 16;
    if (p::q::w + p::q::z != 23) e += 32;
    if (fp(4) != 8 || efp(5) != 10) e += 64;
    test_errors = e;
    return 0;
}
