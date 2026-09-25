// Namespace-aware free-function mangling and lookup (see sema.c,
// "namespace-aware free-function lookup", and docs/DESIGN_NOTES.md).
//
// Before this round, mangling and the free-function registry ignored
// namespaces entirely: v32::draw(int,int,int) and a user's own file-scope
// draw(int,int,int) both became draw__int_int_int, were deduplicated into
// ONE registry entry, and the generated C defined that symbol twice. This
// is exactly the hazard for every v32:: veneer header whose wrapper name
// a user might reuse.
//
// Expected: transpiles cleanly; every call below binds to the function
// named in its trailing comment, and each namespaced function's C name
// carries its namespace path (v32__draw__..., outer__inner__twice__...).

namespace v32
{
    int draw( int region, int x, int y ) { return 1; }

    int helper( int v ) { return v + 100; }

    // unqualified call inside the namespace: v32::helper, not ::helper
    int uses_helper( int v ) { return helper( v ); }       // -> v32__helper

    namespace detail
    {
        int helper( int v ) { return v + 200; }

        // innermost scope wins over the enclosing v32::helper
        int inner_uses_helper( int v ) { return helper( v ); }   // -> v32__detail__helper

        // a name only the ENCLOSING namespace has is still found
        int reaches_out( int v ) { return uses_helper( v ); }    // -> v32__uses_helper
    }

    // relative qualification: detail:: from inside v32 means v32::detail::
    int relative( int v ) { return detail::helper( v ); }    // -> v32__detail__helper
}

namespace outer
{
    namespace inner
    {
        int twice( int v ) { return v * 2; }
    }
}

// the user's own function with v32::draw's exact signature
int draw( int region, int x, int y ) { return 2; }

int helper( int v ) { return v + 300; }

// a friend function of a class in a namespace belongs to that namespace
namespace shapes
{
    class Box
    {
        int secret;
        public:
            Box() { secret = 7; }
            friend int peek( Box* b );
    };

    int peek( Box* b ) { return b->secret; }
}

typedef int (*Unary)( int );

void main()
{
    int a = draw( 0, 1, 2 );                    // -> draw (file scope)
    int b = v32::draw( 0, 1, 2 );               // -> v32__draw
    int c = helper( 1 );                        // -> helper (file scope)
    int d = v32::helper( 1 );                   // -> v32__helper
    int e = v32::detail::inner_uses_helper( 1 );
    int f = v32::detail::reaches_out( 1 );
    int g = v32::relative( 1 );
    int h = outer::inner::twice( 21 );          // -> outer__inner__twice

    Unary fp1 = helper;                         // -> &helper
    Unary fp2 = v32::helper;                    // -> &v32__helper

    shapes::Box box;
    int i = shapes::peek( &box );               // -> shapes__peek
}
