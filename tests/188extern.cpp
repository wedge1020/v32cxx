// *****************************************************************************
//  tests/188extern.cpp -- what keeps its own name when namespace variables are
//  renamed (`a::x` -> `a__x`): an `extern` declaration inside a namespace names
//  something defined elsewhere (the SDK, a C file), a file-scope variable is not
//  in a namespace at all. Checked by grep in `make test`.
// *****************************************************************************

int test_errors = -1;

int plain = 1;                 // file scope: `plain`

namespace hw {
    extern int c_side;         // defined in C: stays `c_side`
    extern int *c_table;
    int mine = 2;              // hw__mine
    int use() { return c_side + mine; }
}

namespace other {
    int mine = 3;              // other__mine: no clash with hw's
}

int main(void)
{
    test_errors = (hw::c_table == 0 && other::mine + hw::mine + plain == 6) ? 0 : 1;
    return 0;
}
