// Exercises `native Name;` -- the opaque pass-through type declaration
// (docs/NATIVE_PASSTHROUGH.md). date_info/time_info are real structs
// from Vircon32's own "time.h", which v32c++ passes through unparsed;
// `native` is what lets this source NAME them at all.
//
// Every legal shape is used here: pointer locals, pointer params,
// pointer returns, pointer class members, reference params (lowered to
// pointers), a const pointer, sizeof of a POINTER to a native, a
// repeated `native` of the same name (idempotent -- two headers may both
// declare it), a native declared inside a namespace, and -- the point of
// the whole feature -- a call to the REAL C API function
// (translate_date/translate_time) with a native pointer argument.
//
// The by-value rejections live in sample91 (deliberately invalid).
// Expected: transpiles cleanly, zero errors, zero warnings; the
// --target=standard output compiles with gcc against a stub time.h.

#include "time.h"

native date_info;
native time_info;
native date_info;          // repeat: must be accepted, not a syntax error

namespace clock
{
    native game_signature; // namespace-scoped native
}

class Stamp
{
    public:
        date_info* date;   // pointer member: allowed
        time_info* time;

        Stamp( date_info* d, time_info* t ) : date( d ), time( t ) {}

        void refresh()
        {
            translate_date( get_date(), date );   // the REAL C function
            translate_time( get_time(), time );
        }
};

date_info* pick( date_info* a, date_info* b, bool first )
{
    if( first ) return a;
    return b;
}

// Reference params to a native: legal (lowered to pointers). Kept to
// reference-to-reference forwarding plus a deref-of-pointer argument --
// `&d` on a reference param is deliberately NOT used here, because
// reference lowering (lower.c phase 5) doesn't yet rewrite `&ref` for
// ANY type, native or not (see docs/DESIGN_NOTES.md).
void touch( date_info& d, time_info& t )
{
}

void forward( date_info& d, time_info& t, date_info* p )
{
    touch( d, t );
    touch( *p, t );
}

int pointer_size()
{
    return sizeof( date_info* );
}

void main()
{
    date_info* none = nullptr;
    const time_info* ctime = nullptr;
    date_info* chosen = pick( none, none, true );
    int words = pointer_size();
}
