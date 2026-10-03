/* =============================================================================
 * v32c++ KNOWN-GAP PROBE HARNESS
 * -----------------------------------------------------------------------------
 * Every construct below is one the v32c++ docs/reports list as NOT currently
 * supported when targeting Vircon32 C. Each is quarantined inside its own
 * `#ifdef GAP_xxx` so a failure (or a silent bad-codegen) can be attributed
 * to exactly one missing feature.
 *
 * BUILD / RUN:
 *   v32c++ -c -DGAP_STATIC       gap_probes.cpp   # one gap at a time
 *   v32c++ -c -DGAP_BITFIELD     gap_probes.cpp
 *   ... etc. (see the table at the bottom)
 *
 * RULES:
 *   - With NO -D flags this file must transpile CLEANLY (it then contains
 *     only the stub sinks). If it does not, the harness itself is broken.
 *   - With exactly one GAP_xxx defined, the expected outcomes are:
 *       * PARSE ERROR  -> good: the gap is still a hard error, as documented.
 *       * clean output -> then inspect the generated C by hand:
 *           - if the construct survived verbatim into the output, Vircon32 C
 *             will reject it downstream (a silent-leak bug worth filing).
 *           - if it was rewritten correctly, the gap is CLOSED: update the
 *             docs and move the construct into spyroad.cpp as [COV].
 *   - Probes marked (LEAK RISK) are the ones the report says currently parse
 *     but are NOT rewritten; for those expect outcome 2a, not 1.
 * ========================================================================== */

#include "video.h"
#include "input.h"
#include "time.h"
#include "misc.h"

/* ------------------------------------------------------------------------ *
 * Stub sinks so probe code has something to call / assign through.
 * This section alone must transpile cleanly with no -D flags.
 * ------------------------------------------------------------------------ */

int  sink_int( int x )              { return x + 1; }
void sink_void( int x )             { x = x; }   /* no-op use; no (void) cast */

struct Point2
{
    int x;
    int y;
};

/* ------------------------------------------------------------------------ *
 * GAP_STATIC : storage-class `static` on file-scope variables and functions.
 *              Vircon32 C has no linker visibility model for it, and the
 *              parser historically rejects the keyword outside class bodies.
 * ------------------------------------------------------------------------ */
#ifdef GAP_STATIC
static int s_counter = 0;

static int s_helper( int x )
{
    return x * 2;
}

void probe_static()
{
    s_counter += s_helper( 3 );
    sink_int( s_counter );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_EXTERN : file-scope `extern` declaration of a symbol defined in
 *              another translation unit (Vircon32 carts are single-TU).
 * ------------------------------------------------------------------------ */
#ifdef GAP_EXTERN
extern int e_shared;

void probe_extern()
{
    sink_int( e_shared + 1 );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_VOLATILE : `volatile` qualifier (both as a type-test and as a hint
 *                that reads must not be cached across end_frame()).
 * ------------------------------------------------------------------------ */
#ifdef GAP_VOLATILE
volatile int v_ticks = 0;

void probe_volatile()
{
    v_ticks = v_ticks + 1;
    sink_int( v_ticks );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_UNSIGNED : the full C integer-type keyword set:
 *                unsigned / signed / short / long (+ combinations).
 * ------------------------------------------------------------------------ */
#ifdef GAP_UNSIGNED
unsigned int  u_a = 4000000000u;      /* [COV-adjacent] u suffix */
signed int    s_a = -5;
short         h_a = 300;
long          l_a = 100000;
unsigned char uc   = 200;              /* + GAP_CHAR territory */
signed short  ss_a = -300;

void probe_unsigned()
{
    sink_int( (int)( u_a + (unsigned int)s_a + (unsigned int)h_a +
                    (unsigned int)l_a + (unsigned int)uc + (unsigned int)ss_a ) );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_BITFIELD : bit-fields inside a struct.
 * ------------------------------------------------------------------------ */
#ifdef GAP_BITFIELD
struct Packed
{
    int flag  : 1;
    int level : 6;
    int spare : 9;
};

void probe_bitfield()
{
    Packed p;
    p.flag  = 1;
    p.level = 33;
    sink_int( p.flag + p.level );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_CONST_PTR : `int* const` -- a CONST POINTER (not pointer-to-const,
 *                 which IS supported). The constant lives on the pointer
 *                 itself, which the declarator rewriter does not model.
 * ------------------------------------------------------------------------ */
#ifdef GAP_CONST_PTR
int cp_storage = 7;

void probe_const_ptr()
{
    int* const locked = &cp_storage;
    sink_int( *locked );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_VOID_PARAM : the `(void)` parameter-list spelling for a no-arg
 *                  function. Supported spelling is `()`.
 * ------------------------------------------------------------------------ */
#ifdef GAP_VOID_PARAM
int probe_void_param( void )
{
    return sink_int( 1 );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_ARRAY_PARAM : array-typed parameter `int f( int a[] )` (unsized
 *                   array in a parameter list; also try int a[4]).
 * ------------------------------------------------------------------------ */
#ifdef GAP_ARRAY_PARAM
int probe_array_param( int a[] )
{
    return a[ 0 ] + sink_int( a[ 1 ] );
}

int probe_array_param_sized( int a[ 4 ] )
{
    return a[ 0 ] + a[ 3 ];
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_TERNARY_ARG : (LEAK RISK) a conditional expression as a CALL ARGUMENT.
 *                  Parses fine, but the ternary rewrite only fires in
 *                  variable-init / bare-identifier-assign / return positions,
 *                  so it would leak into the output C verbatim.
 * ------------------------------------------------------------------------ */
#ifdef GAP_TERNARY_ARG
void probe_ternary_arg( int a, int b )
{
    sink_int( a > b ? a : b );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_TERNARY_MEMBER : (LEAK RISK) a ternary on the RHS of a MEMBER or
 *                      ARRAY-SUBSCRIPT assignment (same leak class as
 *                      GAP_TERNARY_ARG -- the rewrite only looks at bare
 *                      identifiers).
 * ------------------------------------------------------------------------ */
#ifdef GAP_TERNARY_MEMBER
struct Holder
{
    int slot;
    int arr[ 4 ];
};

void probe_ternary_member( int c, int x, int y )
{
    Holder h;
    h.slot = c ? x : y;                 /* member assign      */
    h.arr[ 0 ] = c ? y : x;             /* array-sub assign   */
    sink_int( h.slot + h.arr[ 0 ] );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_COMMA_ARG : (LEAK RISK) the comma OPERATOR inside a call argument
 *                 (it is only rewritten in expression statements and
 *                 for clauses).
 * ------------------------------------------------------------------------ */
#ifdef GAP_COMMA_ARG
void probe_comma_arg( int a, int b )
{
    sink_int( ( a += 1, b += 1, a + b ) );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_COMMA_RETURN : (LEAK RISK) the comma operator in a return expression.
 * ------------------------------------------------------------------------ */
#ifdef GAP_COMMA_RETURN
int probe_comma_return( int a, int b )
{
    return a += 1, a + b;
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_MULTIDECL_ARRAY : one declaration statement mixing plain and ARRAY
 *                       declarators: `int a, b[ 4 ];`. Plain+plain and
 *                       pointer mixes ARE supported; the array flavor is not.
 * ------------------------------------------------------------------------ */
#ifdef GAP_MULTIDECL_ARRAY
void probe_multidecl_array()
{
    int a, b[ 4 ];                      /* plain mixed with array */
    a = 1;
    b[ 0 ] = 2;
    sink_int( a + b[ 0 ] );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_STRUCT_INIT : braced STRUCT initializer: `struct P p = { 1, 2 };`
 *                   (scalar array init lists ARE supported; struct ones
 *                   are not).
 * ------------------------------------------------------------------------ */
#ifdef GAP_STRUCT_INIT
void probe_struct_init()
{
    Point2 p = { 1, 2 };
    sink_int( p.x + p.y );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_STRUCT_ASSIGN : struct by-value ASSIGNMENT `p = q;`. Vircon32 C has
 *                     no struct assignment (one-word ABI), so the
 *                     transpiler must either rewrite it member-wise or
 *                     reject it -- currently it does neither reliably.
 * ------------------------------------------------------------------------ */
#ifdef GAP_STRUCT_ASSIGN
void probe_struct_assign( int x, int y )
{
    Point2 p;
    Point2 q;
    p.x = x;  p.y = y;
    q = p;                              /* by-value struct copy */
    sink_int( q.x + q.y );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_STRUCT_RETURN : returning a MULTI-WORD struct by value through a
 *                     ternary-wrapped or directly-typed return. Returning
 *                     a struct at all is the risky part: the ABI can only
 *                     carry one word, and the report lists struct returns
 *                     as unsupported despite single-word structs being
 *                     common in real headers.
 * ------------------------------------------------------------------------ */
#ifdef GAP_STRUCT_RETURN
Point2 make_point( int x, int y )
{
    Point2 p;
    p.x = x;
    p.y = y;
    return p;                          /* by-value struct return: Point2 is
                                           TWO words, and Vircon32 C only
                                           returns one ("functions cannot
                                           return values of size > 1").
                                           v32c++ warns about it. This
                                           lived in the always-on stub
                                           section, where it made every
                                           probe fail downstream. */
}

Point2 probe_struct_return( int x, int y )
{
    return make_point( x, y );          /* RVO? there is none in Vircon32 C */
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_CHAR : `char` as a standalone declared type (as opposed to char
 *            LITERALS, which ARE supported).
 * ------------------------------------------------------------------------ */
#ifdef GAP_CHAR
void probe_char()
{
    char letter = 'A';                  /* the type is the gap, not the literal */
    char text[ 8 ] = "SPY";
    sink_int( letter + text[ 0 ] );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_UNSAVED_ARRAY : unsized array with an init list: `int t[] = { ... };`
 *                     (the length must be inferred from the list).
 * ------------------------------------------------------------------------ */
#ifdef GAP_UNSAVED_ARRAY
void probe_unsized_array()
{
    int t[] = { 10, 20, 30 };
    sink_int( t[ 0 ] + t[ 1 ] + t[ 2 ] );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_NESTED_INIT : nested braced initializer for a 2D array (and a
 *                   struct-inside-struct flavor for good measure).
 * ------------------------------------------------------------------------ */
#ifdef GAP_NESTED_INIT
void probe_nested_init()
{
    int m[ 2 ][ 2 ] = { { 1, 2 }, { 3, 4 } };
    sink_int( m[ 0 ][ 0 ] + m[ 1 ][ 1 ] );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_BITNOT : the unary bitwise-NOT operator `~`.
 *              Workaround used in spyroad.cpp: `x &= MASK_ALL ^ MASK_BIT;`
 * ------------------------------------------------------------------------ */
#ifdef GAP_BITNOT
void probe_bitnot( int x )
{
    int y = ~x;
    sink_int( y );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_MODASSIGN : the compound `%=` operator. (All other compound ops are
 *                 supported; use `a = a % b;` meanwhile.)
 * ------------------------------------------------------------------------ */
#ifdef GAP_MODASSIGN
void probe_modassign( int a )
{
    a %= 7;
    sink_int( a );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_ELABORATED : [CONFIRMED GAP #2, found by spyvsspy.c] elaborated type
 *                  specifiers -- `typedef struct X Y;` and `struct X v;`
 *                  as a declaration. typedef_decl and var_decl only accept
 *                  a type_spec after their first keyword, and the STRUCT/
 *                  UNION/ENUM keywords are not in type_spec's first set,
 *                  so both die with "syntax error, unexpected STRUCT"
 *                  (or UNION/ENUM). Workaround used in spyvsspy.c: rely on
 *                  the tag being registered (SYM_CLASS/SYM_UNION) and the
 *                  lexer hack classifying it TYPE_NAME -- the bare tag name
 *                  works everywhere the elaborated form doesn't.
 * ------------------------------------------------------------------------ */
#ifdef GAP_ELABORATED
typedef struct Point2 Point2Alias;      /* typedef struct ... ...; */

void probe_elaborated()
{
    struct Point2 q;                   /* elaborated local declaration */
    Point2Alias r;                     /* the typedef name itself, once made */
    q.x = 1;
    q.y = 2;
    r.x = 3;
    sink_int( q.x + q.y + r.x );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_FP_PARAM_NAME : [CONFIRMED GAP #3, found by spyvsspy.c] a NAMED
 *                     parameter inside a function-pointer TYPE's parameter
 *                     list: `typedef void (*Fn)( int* p );`. func_ptr_
 *                     param_type only accepts bare types, so the name dies
 *                     with "syntax error, unexpected IDENTIFIER, expecting
 *                     ')'". The grammar comment says bare-types-only
 *                     "matches real C++ exactly", but real C/C++ allows
 *                     (and discards) a name in any function declarator,
 *                     including fp typedefs -- a true coverage gap, not a
 *                     scope boundary. Both declarator spellings below go
 *                     through the same opt_func_ptr_param_list.
 *                     Workaround in spyvsspy.c: drop the name.
 * ------------------------------------------------------------------------ */
#ifdef GAP_FP_PARAM_NAME
typedef void (*NamedFn)( int* p, float );   /* standard-C spelling, mixed
                                               named/unnamed params */
int fpn_dummy = 7;

void probe_fp_param_name()
{
    void ( int* q, float )* g_fp;           /* Vircon32-style spelling,
                                               same named-param list */
    NamedFn f;
    f = 0;                                  /* 0-vs-pointer assign path */
    g_fp = 0;
    sink_int( fpn_dummy + ( f != 0 ) + ( g_fp != 0 ) );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_PTR_ARITH : [CONFIRMED GAP #4, found by spyvsspy.c] pointer ARITHMETIC
 *                 leaking through to the generated C. v32c++ parses
 *                 `p + i`, `i + p`, `p - i` and `p - q` and emits them
 *                 verbatim; Vircon32 C accepts `++p`, `p != q` and
 *                 `p->m` but rejects + and - on pointers outright
 *                 ("invalid operands for addition/subtraction"), so valid
 *                 C transpiles clean and dies in the DOWNSTREAM compile --
 *                 the silent-leak class. Needs a Vircon32-mode lowering
 *                 pass in lower.c (same shape as the ternary phase), not
 *                 a parser change. spyvsspy.c keeps its pointer iteration
 *                 as-is pending that fix, per the no-workarounds policy.
 * ------------------------------------------------------------------------ */
#ifdef GAP_PTR_ARITH
int pa_arr[ 8 ];

int probe_ptr_arith()
{
    int* p = pa_arr + 4;        /* ptr + int (array decay first) */
    int* q = 4 + pa_arr;        /* int + ptr, commuted form */
    int* r = p - 2;             /* ptr - int */
    return p - q + ( r != 0 );  /* ptr - ptr difference */
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_CONST_TERNARY : [CONFIRMED GAP #5, found by spyvsspy.c] a ternary with
 *                     CONST-QUALIFIED parameters as branch operands, in a
 *                     position the lowering hoists a temp for: the rewrite
 *                     assigns `const int` lvalues into a plain
 *                     `int __v32_tern_tmpN`, and Vircon32 C -- stricter
 *                     than real C, where a by-value scalar copy may
 *                     discard const -- rejects it ("cannot assign const
 *                     int to int: discards const qualifier"). The NESTED
 *                     ternary below forces the hoist path even in the
 *                     return position. Transpiler-side fix: wrap const
 *                     scalar lvalues in an unqualify cast (`(int)lo`)
 *                     inside the ternary machinery's synthesized
 *                     assignments (lower.c).
 * ------------------------------------------------------------------------ */
#ifdef GAP_CONST_TERNARY
int ct_min( const int v, const int lo )
{
    return v < lo ? lo : ( v > lo ? v : lo );
}
#endif

/* ------------------------------------------------------------------------ *
 * GAP_NULL_INIT : initializing a POINTER with NULL / nullptr at DECLARATION
 *                 (compares against 0 are auto-rewritten, but declarator
 *                 initialization with a pointer constant is a different
 *                 code path). nullptr is claimed supported; NULL via the
 *                 SDK headers is the interesting one.
 * ------------------------------------------------------------------------ */
#ifdef GAP_NULL_INIT
#ifndef NULL
#define NULL 0        /* [COV-adjacent] #ifndef fallback macro definition */
#endif

int cp_dummy = 3;

void probe_null_init()
{
    int* p = NULL;                      /* NULL: fallback macro -> 0 */
    int* q = nullptr;                   /* C++ spelling, claimed supported  */
    p = &cp_dummy;
    q = &cp_dummy;
    sink_int( *p + *q );
}
#endif

/* =============================================================================
 * RESULT TABLE -- as of 20261003, each probe transpiled and then compiled
 * with the real Vircon32 C compiler (v26.04.24)
 * -----------------------------------------------------------------------------
 *   CLOSED (transpiles, compiles; run-time checked in tests/100sample.cpp):
 *     GAP_STATIC  GAP_EXTERN  GAP_VOLATILE  GAP_UNSIGNED  GAP_CONST_PTR
 *     GAP_COMMA_RETURN  GAP_MULTIDECL_ARRAY  GAP_STRUCT_INIT
 *     GAP_UNSAVED_ARRAY  GAP_NESTED_INIT  GAP_MODASSIGN  GAP_NULL_INIT
 *     GAP_ELABORATED  GAP_FP_PARAM_NAME  GAP_PTR_ARITH  GAP_CONST_TERNARY
 *   WAS NEVER A GAP (this table used to expect a failure):
 *     GAP_VOID_PARAM  GAP_ARRAY_PARAM  GAP_TERNARY_ARG  GAP_TERNARY_MEMBER
 *     GAP_COMMA_ARG  GAP_STRUCT_ASSIGN  GAP_CHAR  GAP_BITNOT
 *   STILL OPEN:
 *     GAP_BITFIELD      hard error, by choice: "bit-fields are not supported"
 *     GAP_STRUCT_RETURN v32c++ warns; Vircon32 C rejects ("functions cannot
 *                       return values of size > 1"). Needs a hidden
 *                       out-pointer rewrite to close.
 *   Notes: GAP_UNSIGNED prints one warning (unsigned is treated as int).
 *   What each fix does: demos/c/spyvsspy/README.md and
 *   docs/VIRCON32_QUIRKS.md #22-#24.
 * ========================================================================== */
