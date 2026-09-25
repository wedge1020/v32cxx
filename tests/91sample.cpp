// Deliberately INVALID: every by-value use of a `native` type that sema
// must reject (docs/NATIVE_PASSTHROUGH.md, "The pointer-only rule").
// A native's layout lives in a C header v32c++ never parses, so any
// by-value use would silently produce a wrong-size stack slot or field.
// Expected: one semantic error per numbered line below, each naming the
// type, and a nonzero exit. Nothing here should be a SYNTAX error --
// every line is well-formed C++; the rejections are all sema's.

#include "time.h"

native date_info;

class Holder
{
    public:
        date_info field;                        // 1: by-value member
};

date_info make_one();                           // 2: by-value return (prototype)
void take_one( date_info d );                   // 3: by-value param (prototype)

void use_one( const date_info d )               // 4: const by-value param
{
}

int size_of_it()
{
    return sizeof( date_info );                 // 5: sizeof of the type itself
}

void peek( date_info* p )
{
    int y = p->year;                            // 6: member access through a native
}

typedef int counter;
native counter;                                 // 7: conflicts with a typedef

void main()
{
    date_info local;                            // 8: by-value local
}
