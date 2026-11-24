#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "sema.h"
#include "lower.h"
#include "codegen.h"
#include "cmode.h"
#include "driver.h" /* g_preprocessor_lines -- see its own doc comment there */
#include "debugmap.h"
#include "prescan.h" /* prescan_is_header_struct */
#include "macro.h"     /* macro_name_is_stable */

/* ---- output-line tracking, for -g's debug map ---------------------------
 *
 * g_codegen_out_line always reflects the line of `out` about to be
 * written NEXT (starting at 1) -- kept accurate by counting '\n'
 * characters in everything actually written through fprintf, via the
 * `#define fprintf tracked_fprintf` immediately below. Every one of
 * this file's ~155 `fprintf(out, ...)` call sites already targets `out`
 * specifically (confirmed by grep before choosing this approach --
 * nothing in this file ever fprintf's to stderr or anywhere else), so
 * redefining the name itself, once, is equivalent to -- and far less
 * error-prone than -- threading a counter through every individual call
 * site by hand. Scoped to this translation unit only: the #define's
 * effect ends at this file's own end (no #undef needed, since it's
 * never #included elsewhere and no other file sees this definition).
 *
 * print_stmt/emit_function_definition (further down) are what actually
 * DECIDE when a new debug_map_record() entry is worth making -- this
 * section only keeps the line counter itself accurate for them to read.
 */
static int g_codegen_out_line = 1;

/* -vv support: sprinkles explanatory comments into the generated C at
 * the points where this project's own C++-to-C transformation is least
 * obvious to someone reading the output -- see codegen.h's own doc
 * comment on codegen_run's verbose_comments parameter for the full
 * picture. `explain` is the one function every such comment goes
 * through: a plain, single-line C block comment, at the given
 * indentation, emitted ONLY when g_verbose_comments is set -- a no-op
 * call everywhere else, so call sites stay unconditional and readable
 * rather than wrapped in `if (g_verbose_comments)` at every one. Uses
 * the SAME tracked_fprintf every other emission in this file goes
 * through (via the `#define fprintf` below), so -g's own line-count
 * tracking sees these lines too -- an explanatory comment shifts every
 * SUBSEQUENT line's own number, and the debug map needs to reflect
 * where things actually ended up, not pretend the comments aren't
 * there. */
static int g_verbose_comments = 0;

static void indent_spaces(FILE *out, int indent); /* forward decl -- defined below, near print_stmt; explain() needs it earlier */

static void explain(FILE *out, int indent, const char *comment) {
    if (!g_verbose_comments) return;
    indent_spaces(out, indent);
    fprintf(out, "/* %s */\n", comment);
}


static int tracked_fprintf(FILE *out, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char stack_buf[1024];
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(stack_buf, sizeof(stack_buf), fmt, args_copy);
    va_end(args_copy);

    char *buf = stack_buf;
    char *heap_buf = NULL;
    if (needed >= (int)sizeof(stack_buf)) {
        /* Practically never happens -- every format string in this file
         * produces a single short line or fragment, nowhere close to
         * 1KB -- but handled correctly rather than silently truncated
         * (and miscounting newlines) on the off chance it ever does. */
        heap_buf = malloc((size_t)needed + 1);
        vsnprintf(heap_buf, (size_t)needed + 1, fmt, args);
        buf = heap_buf;
    }
    va_end(args);

    for (int i = 0; buf[i] != '\0'; i++) {
        if (buf[i] == '\n') g_codegen_out_line++;
    }
    fputs(buf, out);
    free(heap_buf);
    return needed; /* matches fprintf's own contract (chars that would be
                       written, excluding the null terminator) -- fputs's
                       own return value does NOT mean the same thing, so
                       returning it directly here would be a silent
                       behavior change for any caller that checks it. */
}
#define fprintf tracked_fprintf

/* ---- type printing ------------------------------------------------------
 *
 * Prints a type in valid Vircon32 C declarator syntax to `out`. This is
 * the one function every other part of this module funnels type output
 * through, specifically so the "no `struct` keyword on a reference"
 * quirk (see codegen.h) only has to be gotten right in one place.
 *
 *  - AST_IDENT: printed bare, whatever the name actually is -- a
 *    primitive keyword (int/float/bool/char/void), a class name, or a
 *    typedef name are all just bare identifiers in valid C, and none of
 *    them should ever be prefixed with `struct` here (see codegen.h).
 *  - AST_QUALIFIED_ID (a namespace-qualified name, e.g. `v32::Timer`):
 *    flattened to its LAST component only, matching type_to_class's own
 *    convention in sema.c. C (and Vircon32 C) has no namespace concept,
 *    and the rest of this compiler already treats classes as a flat,
 *    namespace-oblivious registry -- this is an existing, documented
 *    limitation (see DESIGN_NOTES.md's "known gaps" list), not something
 *    this module invents. Two DIFFERENT namespaces declaring same-named
 *    classes would already collide in mangled function names before
 *    codegen existed at all.
 *  - AST_POINTER_TYPE: `<inner> *`.
 *  - AST_REFERENCE_TYPE: SHOULD NEVER reach here -- lower.c's phase 5
 *    (reference-to-pointer) converts every one of these into
 *    AST_POINTER_TYPE before lower_run() even returns, and codegen only
 *    ever runs after that. Printed as a pointer anyway, with a loud
 *    comment, matching this project's best-effort philosophy rather than
 *    crashing -- but seeing this case actually fire means either phase 5
 *    has a bug or codegen is somehow running against a not-fully-lowered
 *    AST, and is worth investigating immediately, not silently ignoring.
 *  - NULL: "void" (a function/method with no declared return type).
 *  - anything else unrecognized: "void" as a best-effort fallback,
 *    rather than emitting nothing and leaving a syntax error with no
 *    explanation.
 */
/* Standard mode's own counterpart to type_to_class -- answers "does
 * `name` refer to a top-level (or namespace-level) enum or union
 * declaration", the same question type_to_class already answers for
 * classes, but for the two OTHER kinds of tagged type this project
 * accepts. No registry exists for these the way sema.c's own class
 * registry does (enums/unions are never looked up for any semantic
 * reason -- they're passed through to codegen as literal, unmodified
 * declarations, per AST_ENUM_DECL/AST_UNION_DECL's own doc comments in
 * ast.h), so this walks the top-level declaration list directly,
 * mirroring program_has_any_class's own walk (further down this file)
 * rather than adding a whole new sema.c-side registry for a lookup
 * this project only ever needs from codegen, in standard mode, for
 * this one purpose. Returns AST_ENUM_DECL, AST_UNION_DECL, or NULL (not
 * found -- a primitive keyword, a class, or a typedef name, any of
 * which print_type's own AST_IDENT case already handles correctly
 * without this). `g_program` (driver.h) is this project's single,
 * already-parsed AST root -- the same global sema.c's own registries
 * are built from and lower.c's phases walk, so reusing it here needs
 * no new plumbing into print_type's own single-type-in signature. */
static AstNode *find_enum_or_union_decl(const AstList *decls, const char *name) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if ((n->kind == AST_ENUM_DECL || n->kind == AST_UNION_DECL) && strcmp(n->str1, name) == 0) {
            return n;
        }
        if (n->kind == AST_NAMESPACE_DECL) {
            AstNode *found = find_enum_or_union_decl(&n->list, name);
            if (found != NULL) return found;
        }
    }
    return NULL;
}

static void print_expr(FILE *out, const AstNode *e);

/* Spells a float literal for the generated C: fixed-point with the
 * FEWEST decimals that read back as exactly the same double (0.1, not
 * 0.10000000000000001), and never in exponent form. Two real bugs this
 * fixes, both silent at transpile time:
 *   - a plain %.17g turned 34.0 into "34" -- an INT literal -- so
 *     `t / 34.0` came out as the integer division `t / 34`, and
 *     `1.0 - t / 34.0` as `1 - t / 34` (found in the TEMPEST 32K demo:
 *     compiled cleanly, produced wrong values);
 *   - %g switches to exponent notation for small/large magnitudes
 *     (1.0000000000000001e-05), which the Vircon32 C lexer rejects as a
 *     "bad floating point literal" -- confirmed against the real compiler,
 *     which accepts neither 1e8 nor 1.5e-3. */
static void format_float_literal(char *buf, size_t size, double v) {
    if (v != v || v - v != 0) {          /* nan / inf: not representable */
        snprintf(buf, size, "0.0");
        return;
    }
    for (int places = 1; places <= 340; places++) {
        snprintf(buf, size, "%.*f", places, v);
        if (strtod(buf, NULL) == v) break;
    }
}

/* A literal that came from `#define NAME value` prints as NAME when the
 * name means the same thing throughout the run (the #define itself is
 * passed through to the top of the generated C). Returns 1 if printed. */
static int print_macro_name(FILE *out, const AstNode *lit) {
    if (lit->macro_name == NULL || !macro_name_is_stable(lit->macro_name)) return 0;
    fprintf(out, "%s", lit->macro_name);
    return 1;
}

/* One array dimension: as written when that's safe to print back
 * (`[MAX]`, `[ROWS * 2]` -- see ast_dim_expr_printable), else its value. */
static void print_array_dim(FILE *out, const AstNode *array_type) {
    fprintf(out, "[");
    if (array_type->b != NULL && ast_dim_expr_printable(array_type->b))
        print_expr(out, array_type->b);
    else
        fprintf(out, "%d", array_type->ival);
    fprintf(out, "]");
}

static void print_type(FILE *out, const AstNode *type) {
    if (type == NULL) {
        fprintf(out, "void");
        return;
    }
    switch (type->kind) {
        case AST_IDENT:
            /* Vircon32 mode: bare, always -- a primitive keyword, a
             * class name, a typedef name, an enum name, or a union
             * name are all just bare identifiers in valid Vircon32 C,
             * which specifically rejects `struct`/`enum`/`union Name`
             * as a type REFERENCE (see the quirks doc). Standard mode
             * needs the matching tag keyword for whichever of the
             * three kinds this name actually is: a CLASS name
             * (confirmed via type_to_class, the same lookup sema.c
             * itself already uses for this -- find_class is static to
             * sema.c, so this reuses the already-public wrapper rather
             * than exposing a second entry point for the same lookup)
             * needs `struct`; an ENUM or UNION name (confirmed via
             * find_enum_or_union_decl, just above -- no sema.c
             * registry exists for either, unlike classes, so this
             * walks the top-level declaration list directly) needs
             * `enum`/`union` respectively -- a real, previously-
             * undiscovered gap, confirmed directly against
             * tests/60sample.cpp/61sample.cpp (an enum parameter and a
             * union variable), both of which failed standard-mode gcc
             * compilation ("unknown type name 'Color'"/"'Value'; use
             * 'union' keyword") before this fix, since this case only
             * ever checked the class registry. A primitive or a
             * typedef name (none of the three lookups match) stays
             * bare either way, in both targets. */
            if (g_target == TARGET_STANDARD) {
                /* The name must BE the class's, not a typedef that leads
                 * to one: type_to_class follows typedefs, and `typedef
                 * Enemy *EP;` used as `EP p;` was printed `struct EP p;`. */
                AstNode *named_class = type_to_class(type);
                if (named_class != NULL && named_class->str1 != NULL &&
                    strcmp(named_class->str1, type->str1) == 0) {
                    fprintf(out, "struct %s", type->str1);
                    break;
                }
                AstNode *tag = find_enum_or_union_decl(&g_program->list, type->str1);
                if (tag != NULL) {
                    fprintf(out, "%s %s", tag->kind == AST_ENUM_DECL ? "enum" : "union", type->str1);
                    break;
                }
            }
            fprintf(out, "%s", type->str1);
            break;
        case AST_QUALIFIED_ID: {
            /* A namespace-qualified TYPE reference (e.g. `si::Game` used
             * as a variable's declared type, as opposed to an unqualified
             * `Game` used from inside `namespace si` itself, which parses
             * as a plain AST_IDENT and already goes through the AST_IDENT
             * case above). A real, previously-undiscovered gap: this case
             * used to just print the flattened final component's bare
             * name with NO struct/enum/union keyword handling at all,
             * unlike the AST_IDENT case just above it -- meaning
             * `si::Game game;` at file scope (main(), outside the
             * namespace) printed as `Game game;` under --target=standard
             * (a hard "unknown type name" compile error), even though
             * every OTHER, unqualified use of the exact same class
             * throughout the rest of the generated file correctly got
             * `struct Game`. Found via a real user's Space Invaders
             * program, whose main() declares `si::Game game;`,
             * `si::Input input;`, `si::Video video;`. Fixed by giving
             * this case the identical struct/enum/union lookup the
             * AST_IDENT case already does -- type_to_class and
             * find_enum_or_union_decl both already accept a flattened
             * final-component name string, so the same two lookups work
             * here unchanged, just fed this node's own last component
             * instead of an AST_IDENT's str1. Vircon32 mode still wants
             * the bare name either way, exactly as AST_IDENT's own case
             * does, since Vircon32 C rejects the tag keyword as a type
             * reference. */
            if (type->list.count == 0) {
                fprintf(out, "void" /* malformed -- shouldn't happen */);
                break;
            }
            const char *name = type->list.items[type->list.count - 1]->str1;
            if (g_target == TARGET_STANDARD) {
                if (type_to_class(type) != NULL) {
                    fprintf(out, "struct %s", name);
                    break;
                }
                AstNode *tag = find_enum_or_union_decl(&g_program->list, name);
                if (tag != NULL) {
                    fprintf(out, "%s %s", tag->kind == AST_ENUM_DECL ? "enum" : "union", name);
                    break;
                }
            }
            fprintf(out, "%s", name);
            break;
        }
        case AST_POINTER_TYPE:
            /* (C input drops `const` everywhere -- see AST_CONST_TYPE.) */
            print_type(out, type->a);
            fprintf(out, " *");
            break;
        case AST_REFERENCE_TYPE:
            print_type(out, type->a);
            fprintf(out, " /* WARNING: unlowered reference type */ *");
            break;
        case AST_CONST_TYPE:
            /* Prefixes rather than suffixes when printed -- "const int",
             * never "int const" -- unlike every other wrap this
             * function handles (pointer, reference, array all recurse
             * THEN append their own marker after). Matches real C's
             * own conventional placement, and is what real Vircon32 C
             * itself expects too, being ordinary C in this respect. */
            if (!(g_c_mode && g_target == TARGET_VIRCON32)) fprintf(out, "const ");
            print_type(out, type->a);
            break;
        case AST_ARRAY_TYPE: {
            /* Vircon32's own reversed array-declarator quirk: length in
             * brackets BEFORE the name, not after (`int [8] scores;`,
             * not standard C's `int scores[8];`). Every call site in
             * this file already does "print_type(type); then print the
             * name" -- so emitting "ElementType [N]" here, with the
             * name appended separately by the (unmodified) caller
             * exactly as it already does for every other type, produces
             * the correct Vircon32 form with no caller-side changes at
             * all. Confirmed by tracing every print_type call site in
             * this file before implementing: all of them already follow
             * this identical pattern. Emitted this way regardless of
             * which of parser.y's two accepted C++-side declarator
             * forms (standard-C length-after-name, or Vircon32-style
             * length-before-name, offered as an alternate input
             * spelling) produced the AST_ARRAY_TYPE node -- the AST
             * itself carries no memory of which spelling the source
             * used, and output is always this one form regardless.
             *
             * Multi-dimensional arrays are nested AST_ARRAY_TYPE nodes
             * (see its own doc comment in ast.h) -- printed here as
             * "BaseType [D1][D2]...", outermost dimension first,
             * matching both real C's own multi-dimensional array
             * syntax and how the source itself was written. This is
             * NOT the naive "print_type(type->a) then append this
             * node's own bracket" the single-dimension case above might
             * suggest generalizes on its own -- traced through a
             * concrete `int grid[8][4]` by hand before trusting that:
             * recursing first and appending after, unwound normally,
             * would print the INNERMOST node's own bracket first
             * ("int [4] [8]"), backwards from what real C requires
             * ("int [8][4]") and a real change of meaning, not just
             * cosmetics -- a caught-before-shipping bug, not a
             * hypothetical one. Instead, walk down through however many
             * AST_ARRAY_TYPE layers exist, collecting each one's own
             * length in that same outermost-first order, until reaching
             * the true (non-array) element type; print that base type
             * once, then every collected bracket immediately after it,
             * adjacent to each other (no space between brackets,
             * matching real C's own convention -- only the first
             * bracket gets a leading space, separating it from the
             * base type's own name). */
            const AstNode *base = type;
            const AstNode *dims[64]; /* generous fixed cap, not a dynamically-sized
                structure -- this is a purely local, transient printing
                operation, not part of the persistent AST, and a
                64-dimensional array is so far beyond anything remotely
                realistic that a fixed stack array is the simpler,
                completely sufficient choice here, matching how other
                bounded, small-scale bookkeeping already works elsewhere
                in this file (e.g. indent tracking). */
            int dim_count = 0;
            while (base->kind == AST_ARRAY_TYPE && dim_count < 64) {
                dims[dim_count++] = base;
                base = base->a;
            }
            print_type(out, base);
            if (g_target == TARGET_STANDARD) {
                /* Standard C wants the bracket(s) AFTER the name
                 * (`int scores[8];`), which this function's own
                 * "prefix, then caller appends the name" architecture
                 * can't produce here -- print the base type ONLY,
                 * exactly as just above, and stop; the caller is
                 * responsible for calling print_array_suffix (this
                 * function's own standard-mode counterpart, defined
                 * further down) AFTER printing the name, at the one
                 * or two call sites where an array-typed declaration
                 * can actually appear. See print_array_suffix's own
                 * doc comment for the full reasoning and exactly
                 * which call sites those are. */
                break;
            }
            for (int i = 0; i < dim_count; i++) {
                if (i == 0) fprintf(out, " ");
                print_array_dim(out, dims[i]);
            }
            break;
        }
        case AST_FUNC_PTR_TYPE:
            /* Vircon32's own reversed function-pointer-declarator
             * quirk (see docs/VIRCON32_QUIRKS.md's own "Function-
             * pointer declarator syntax reversed" entry, confirmed
             * against the real compiler for vtable-slot emission --
             * `int(Shape *)* Shape__area__void;`, never the standard-C
             * `int (*Shape__area__void)(Shape *);`) -- emitted here
             * regardless of which of parser.y's two accepted C++-side
             * declarator forms produced this node, same "AST carries
             * no memory of which spelling was used" treatment
             * AST_ARRAY_TYPE just above already established. No name
             * embedded inside the parens at all (unlike the standard-C
             * form's own "(*name)") -- the caller appends " name"
             * after this function returns, the exact same pattern
             * every other type in this function already follows,
             * which is also what makes composing with AST_ARRAY_TYPE
             * (an array of function pointers) work with no extra code
             * at all: that case's own recursive print_type(type->a)
             * call lands here first, producing "ReturnType(Params)*",
             * then its own " [%d]" is appended, then the ordinary
             * caller-appends-the-name step happens exactly as always.
             *
             * KNOWN, DELIBERATE GAP that's since been closed: this
             * function itself still ALWAYS emits Vircon32's own form
             * here, unconditionally, regardless of g_target -- unlike
             * AST_ARRAY_TYPE just above, standard C's own function-
             * pointer declarator (`ReturnType (*name)(ParamTypes)`)
             * needs the NAME embedded INSIDE the parens, in the
             * middle of the type, not appended afterward the way
             * every other case in this function (arrays included)
             * works, so the same "print a suffix after the name" fix
             * that solved arrays doesn't apply to this function
             * itself at all. Solved differently instead: a new
             * print_type_and_name (below) builds the entire standard-
             * mode function-pointer declarator (return type, name,
             * any array dimensions, parameter list) as ONE self-
             * contained unit at each of the two call sites that can
             * ever reach a function-pointer-typed declaration
             * (confirmed directly: only var_decl's own grammar ever
             * produces one), rather than trying to force this shape
             * through this function's own single-type-in, single-
             * string-out, caller-appends-the-name contract. This
             * function's own AST_FUNC_PTR_TYPE case (right here)
             * therefore still only ever needs to know Vircon32's own
             * form -- print_type_and_name's own standard-mode branch
             * never calls back into this case at all for the OUTER
             * function-pointer type, only for the return type and
             * each parameter's own (non-function-pointer) type. See
             * print_type_and_name's own doc comment for the full
             * standard-mode shape, and emit_vtable_struct/emit_
             * vtable_instance for the identical fix applied to their
             * own separate, inline function-pointer declarator/cast
             * text (vtable slots are function-pointer-typed by
             * construction, never routed through print_type at all). */
            print_type(out, type->type);
            fprintf(out, "(");
            for (int i = 0; i < type->list.count; i++) {
                if (i > 0) fprintf(out, ", ");
                print_type(out, type->list.items[i]);
            }
            fprintf(out, ")*");
            break;
        default:
            fprintf(out, "void" /* unrecognized type node -- best-effort */);
            break;
    }
}

/* Prints `name` as a type reference, applying the same Vircon32-vs-
 * standard "struct" keyword treatment print_type's own AST_IDENT case
 * does -- but for the handful of runtime-generation functions further
 * down (emit_new_delete_runtime, emit_array_new_runtime, emit_v32_delete,
 * emit_delete_runtime) that build a class's own v32_new_ and v32_delete
 * wrapper functions directly via class_decl->str1 (always genuinely a
 * class name by construction here -- these functions only ever exist
 * for an actual class) rather than through print_type/an AST node at
 * all, and so never picked up print_type's own g_target handling
 * automatically. A real, separate gap from the one print_type itself
 * needed fixing for arrays -- these functions build their own
 * declarator text by hand, entirely bypassing print_type, discovered
 * only by actually running --target=standard against a real class-
 * having test and reading the output, not by re-deriving every call
 * site from first principles. `is_known_primitive` exists because
 * emit_array_new_runtime's own `type_name` parameter is the one
 * exception in this group that ISN'T always a class name -- it's also
 * called with the four hardcoded primitive element types
 * (emit_primitive_array_new_runtime), which must never get `struct`
 * prefixed. */
static int is_known_primitive_type_name(const char *name) {
    return strcmp(name, "int") == 0 || strcmp(name, "float") == 0
        || strcmp(name, "char") == 0 || strcmp(name, "bool") == 0
        || strcmp(name, "void") == 0;
}

static void print_class_type_name(FILE *out, const char *name) {
    if (g_target == TARGET_STANDARD && !is_known_primitive_type_name(name)) {
        fprintf(out, "struct %s", name);
    } else {
        fprintf(out, "%s", name);
    }
}

/* Standard-C mode's own counterpart to AST_ARRAY_TYPE's printing above:
 * Vircon32 puts the bracket(s) BEFORE the name (print_type handles that
 * entirely on its own, the caller just appends the name afterward, no
 * special-casing needed anywhere else) -- but standard C puts them
 * AFTER the name (`int scores[8];`, never `int [8] scores;`), which
 * print_type's own "prefix, then caller appends the name" architecture
 * can't produce on its own. So in standard mode, print_type's own
 * AST_ARRAY_TYPE case (just above) skips printing any bracket at all --
 * see its own dim_count==0 short-circuit -- and every CALLER that might
 * be printing an array-typed declaration calls this function AFTER
 * printing the name instead. Only two call sites actually need this:
 * print_var_decl_inline (covers both local variables and, through it,
 * globals) and emit_struct's own data-field loop (struct/class
 * members) -- confirmed directly by finding every print_type call site
 * in this file and checking which ones can ever see an array type at
 * all: a function's own return type and every parameter's own type
 * never can (illegal to return an array by value in C/C++; `param`'s
 * own grammar decays an array parameter straight to a pointer at parse
 * time, so no AST_ARRAY_TYPE node is ever built for one in the first
 * place -- see var_decl's own grammar comments in parser.y). A no-op
 * for a non-array type (nothing printed at all) and for Vircon32 mode
 * (where print_type already handled the brackets itself, before the
 * name) -- safe to call unconditionally from either call site without
 * its own target/array-type guard at each one. */
static void print_array_suffix(FILE *out, const AstNode *type) {
    if (g_target != TARGET_STANDARD || type == NULL) return;
    while (type->kind == AST_ARRAY_TYPE) {
        print_array_dim(out, type);
        type = type->a;
    }
}

/* Prints "Type name" (or, for a function-pointer type in standard
 * mode, the fully different declarator shape that requires) for the
 * two call sites that can ever need it -- print_var_decl_inline
 * (local and global variables) and emit_struct's own data-field loop
 * (class members). Confirmed directly, not assumed, that these are
 * the ONLY two: ast_wrap_func_ptr (ast.c) is only ever called from
 * var_decl's own grammar productions (parser.y), and var_decl is
 * what BOTH a local/global variable declaration AND a class member
 * (`member: ... | var_decl ';'`) reduce to -- no parameter and no
 * return type can ever be function-pointer-typed in this grammar (no
 * grammar production wraps a func-ptr type anywhere else), so those
 * callers correctly never needed this and still don't.
 *
 * Vircon32 mode, and standard mode for anything that ISN'T a function
 * pointer (or an array of them): behaves exactly like the "print_type
 * then the name then print_array_suffix" pattern this replaces --
 * print_type already handles the ENTIRE Vircon32-mode declarator
 * (including function pointers, prefix-style, name appended after by
 * the caller same as everything else), so nothing about that path
 * changes at all.
 *
 * Standard mode, function-pointer base type (walking through any
 * wrapping AST_ARRAY_TYPE layers first, the same dimension-collecting
 * walk print_type's own AST_ARRAY_TYPE case already does, to find out
 * whether the ELEMENT type -- not the outermost node -- is a function
 * pointer): standard C needs the name INSIDE the parens
 * (`ReturnType (*name)(Params);`), with any array dimensions ALSO
 * inside those same parens, right after the name
 * (`ReturnType (*name[N])(Params);`, standard C's own syntax for an
 * array of function pointers -- confirmed against ordinary C
 * declarator rules, not the real Vircon32 compiler, since this shape
 * only ever applies to standard-mode output in the first place) --
 * this function's own "prefix, then caller appends the name" pattern
 * genuinely can't produce that on its own, since print_type has
 * already returned (and the name printed) long before the closing
 * `)(Params)` suffix would need to appear. Built directly here
 * instead, as one self-contained declarator, rather than trying to
 * force this shape through print_type's own single-type-in,
 * single-string-out contract. */
static void print_type_and_name(FILE *out, const AstNode *type, const char *name) {
    if (g_target == TARGET_STANDARD) {
        const AstNode *base = type;
        int dims[64]; /* see print_type's own identical cap and reasoning
            in its AST_ARRAY_TYPE case -- same purely local, transient
            printing operation, same "far beyond anything realistic"
            justification */
        int dim_count = 0;
        while (base != NULL && base->kind == AST_ARRAY_TYPE && dim_count < 64) {
            dims[dim_count++] = base->ival;
            base = base->a;
        }
        if (base != NULL && base->kind == AST_FUNC_PTR_TYPE) {
            print_type(out, base->type); /* the function pointer's own return type */
            fprintf(out, " (*%s", name);
            for (int i = 0; i < dim_count; i++) {
                fprintf(out, "[%d]", dims[i]);
            }
            fprintf(out, ")(");
            if (base->list.count == 0) {
                /* Standard C: empty parens mean UNSPECIFIED parameters,
                 * not NO parameters -- a real semantic difference this
                 * project already takes seriously for ordinary function
                 * signatures (see emit_function_header's own identical
                 * "func->list.count == 0" check). In practice this
                 * grammar's own accepted "(void)" spelling already
                 * produces a one-entry list (a literal `void` type,
                 * falling through type_spec's own VOID_KW alternative,
                 * not a dedicated empty-list case) rather than a truly
                 * empty one, so this branch is a safety net for the
                 * bare "()" spelling specifically, not the common case. */
                fprintf(out, "void");
            }
            for (int i = 0; i < base->list.count; i++) {
                if (i > 0) fprintf(out, ", ");
                print_type(out, base->list.items[i]);
            }
            fprintf(out, ")");
            return;
        }
    }
    print_type(out, type);
    fprintf(out, " %s", name);
    print_array_suffix(out, type);
}

/* ---- typedefs -------------------------------------------------------- */

/* NOTE: AST_NATIVE_DECL is deliberately NOT emitted by any walk in
 * this file -- a `native X;` is a compile-time-only declaration
 * (sema registers X as an opaque type; the real X is defined by a
 * pass-through #include, resolved by the downstream C compiler).
 * Uses of X in generated code print as the bare name via print_type. */

static void emit_typedefs(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_TYPEDEF_DECL) {
            /* Routed through print_type_and_name (not plain print_type)
             * specifically so a function-pointer typedef gets the
             * correct declarator shape for whichever target is active --
             * plain print_type's own AST_FUNC_PTR_TYPE case always
             * emits Vircon32's own "name appended after" form (see its
             * own doc comment), which is wrong for standard mode's
             * "name INSIDE the parens" requirement. print_type_and_name
             * already handles exactly this distinction (originally
             * built for var_decl/emit_struct's own function-pointer
             * declarations -- a typedef is simply the third place one
             * can appear). A no-op behavior change for every other
             * typedef kind: print_type_and_name falls back to plain
             * "print_type, then the name" for anything that isn't a
             * function-pointer type in standard mode, identical to what
             * this loop did directly before this routing existed. */
            fprintf(out, "typedef ");
            print_type_and_name(out, n->type, n->str1);
            fprintf(out, ";\n");
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_typedefs(out, &n->list);
        }
    }
}

static void print_expr(FILE *out, const AstNode *e); /* forward decl -- defined below; emit_enums needs it earlier */

/* ---- enums ------------------------------------------------------------
 *
 * Literal, unmodified C enum syntax -- no lowering transformation
 * happened for this node at all (see AST_ENUM_DECL's own doc comment
 * in ast.h), since Vircon32 C already has this natively, the same
 * "pass it straight through" treatment AST_SWITCH already gets.
 * Called from codegen_run() alongside emit_typedefs, before
 * emit_classes -- a class field could plausibly be typed as an
 * already-declared enum, so the enum's own declaration needs to exist
 * first in the generated output, same reasoning as typedefs.
 */
static void print_var_decl_inline(FILE *out, const AstNode *n); /* forward decl -- defined below; emit_unions (and, further down, emit_globals) needs it earlier */

static void emit_enums(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_ENUM_DECL) {
            fprintf(out, "enum %s {\n", n->str1);
            for (int j = 0; j < n->list.count; j++) {
                const AstNode *ev = n->list.items[j];
                fprintf(out, "    %s", ev->str1);
                if (ev->a != NULL) {
                    fprintf(out, " = ");
                    print_expr(out, ev->a);
                }
                fprintf(out, "%s\n", (j < n->list.count - 1) ? "," : "");
            }
            fprintf(out, "};\n\n");
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_enums(out, &n->list);
        }
    }
}

/* ---- unions -------------------------------------------------------------
 *
 * Literal, unmodified C union syntax -- no lowering transformation
 * happened for this node at all (see AST_UNION_DECL's own doc comment
 * in ast.h), the same "pass it straight through" treatment
 * AST_SWITCH/AST_ENUM_DECL already get. Each member reuses
 * print_var_decl_inline directly -- a union member's own C syntax
 * ("type name;") is identical to a local or global variable's, so
 * there's no need for a second, parallel printing function here either
 * (same reasoning as emit_globals's own reuse of it).
 */
static void emit_union(FILE *out, const AstNode *n) {
    fprintf(out, "union %s {\n", n->str1);
    for (int j = 0; j < n->list.count; j++) {
        fprintf(out, "    ");
        print_var_decl_inline(out, n->list.items[j]);
        fprintf(out, ";\n");
    }
    fprintf(out, "};\n\n");
}



/* ---- global (file-scope) variables --------------------------------------
 *
 * A real, previously-existing gap, not a deliberate scope boundary: a
 * top-level `int x = 0;` was already syntactically accepted by the
 * grammar (var_decl is a valid top_decl alternative) and already walked
 * by sema.c (see check_globals, sema.c) once that gap was closed
 * alongside this one -- but codegen_run itself never had any function
 * that actually EMITTED one. The generated C simply dropped it
 * entirely, silently: any function referencing that "global" produced
 * output that referenced an undeclared identifier, a real, confirmed
 * compile failure downstream, not merely an unsupported-but-harmless
 * gap. Reuses print_var_decl_inline directly -- the C syntax for a
 * global declaration is identical to a local one (`type name [=
 * initializer];`), just written at file scope instead of inside a
 * function body, so there was no need for a second, parallel printing
 * function. Placed in codegen_run's own emission order AFTER
 * emit_classes (below), not before -- a global whose own type is a
 * class (rare, and not fully supported yet regardless -- see this
 * function's own scope note below) would need that class's own struct
 * definition to already exist; ordinary primitive globals don't care
 * either way, so putting class-typed ones on the safe side costs
 * nothing.
 *
 * SCOPE: only ever prints the declaration and, if present, the
 * initializer expression exactly as written -- a class-typed global's
 * own constructor is never invoked here (this project has no
 * "construct a global" mechanism at all, the same gap class-typed
 * MEMBER fields still have -- see resolve_member_init_list's own doc
 * comment in sema.c for the closest existing analogue). A class-typed
 * global with no explicit initializer would compile as plain,
 * uninitialized memory in the generated C, not a real C++ default-
 * constructed object -- silently different from what real C++ would
 * do, in the same direction this project's other class-typed-value
 * gaps already are, not a new kind of imprecision.
 */
static void emit_globals(FILE *out, const AstList *decls) {
    int any = 0;
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_VAR_DECL) {
            print_var_decl_inline(out, n);
            fprintf(out, ";\n");
            any = 1;
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_globals(out, &n->list);
        }
    }
    if (any) fprintf(out, "\n");
}

/* ---- vtable struct types ----------------------------------------------
 *
 * find_declaring_class (needed here to know a vtable slot's
 * canonical_method's OWN declaring class, since it can be an ancestor of
 * whichever class's vtable is currently being emitted) now lives in
 * sema.c/sema.h -- lower.c's finalize_call needs the exact same
 * "which class actually declares this method" lookup for its own,
 * separate reason (inserting a cast on an inherited method's receiver
 * argument -- see AST_CAST in ast.h and finalize_call's own doc comment
 * in lower.c), so it moved to a shared location rather than existing
 * twice with the two copies inevitably drifting apart eventually.
 */

/* Emits `class_decl`'s vtable struct TYPE -- one field per virtual slot,
 * each a function pointer. Does nothing if the class has no vtable at
 * all (layout->vtable == NULL).
 *
 * FUNCTION-POINTER DECLARATOR SYNTAX: Vircon32 C does NOT use standard
 * C's `ReturnType (*name)(ParamTypes);` form for a function-pointer
 * field -- confirmed against the real compiler (Matthew hand-converted
 * this exact struct while testing tests/sample14.cpp's generated
 * output, and it's what let compilation get past this point at all) --
 * it wants `ReturnType(ParamTypes)* name;` instead: the parenthesized
 * parameter list sits directly after the return type, with NO `*` or
 * name inside it at all, and the `*` plus the field name come after the
 * closing paren. Matches the pattern in Vircon32's own documented
 * function-pointer example, `void()* Action = &DoSomething;`.
 *
 * Each field's name is the slot's canonical_method's OWN mangled name
 * (matching lower.c's phase 3 exactly -- `obj->vtable->FIELD(...)`
 * already uses that same name at every call site, so the struct
 * definition has to match it verbatim or the generated code simply
 * wouldn't refer to the same field).
 *
 * Each field's function-pointer signature needs the receiver
 * (`ClassName *`) as its first parameter -- but canonical_method's OWN
 * parameter list might or might not already include an injected `this`,
 * depending on whether it's ever been through this-injection at all
 * (lower.c's phase 2 only touches AST_FUNC_DEF -- a method with a body
 * -- never a prototype-only AST_FUNC_DECL). Rather than assume either
 * shape, this reconstructs the receiver parameter explicitly every time
 * (via find_declaring_class, above) and skips canonical_method's own
 * param[0] ONLY when it's an AST_FUNC_DEF (meaning that slot IS the
 * injected `this`, now redundant with the explicitly-printed receiver).
 * This is deliberately the same kind of care the operator-arity bug a
 * few rounds back should have gotten from the start: a count/shape that
 * differs depending on whether this-injection has touched it needs to be
 * checked for that difference explicitly, not assumed uniform.
 *
 * Function-pointer declarator output for --target=standard: now
 * IMPLEMENTED, branching internally on g_target -- see the standard-
 * mode branch's own comment, just below, for the full reasoning.
 */
static void emit_vtable_struct(FILE *out, const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL || layout->vtable == NULL) return;

    explain(out, 0, "a vtable type: one function-pointer field per virtual method, in a fixed order every class in the hierarchy agrees on");
    fprintf(out, "struct %s_VTable {\n", class_decl->str1);
    for (int i = 0; i < layout->vtable->count; i++) {
        VtableEntry *entry = &layout->vtable->entries[i];
        AstNode *canonical = entry->canonical_method;
        FuncSemaInfo *info = (FuncSemaInfo *)canonical->sema_info;
        const char *field_name = (info != NULL) ? info->mangled_name : canonical->str1;
        const AstNode *canonical_class = find_declaring_class(class_decl, canonical);

        fprintf(out, "    ");
        int already_this_injected = (canonical->kind == AST_FUNC_DEF);
        int start = already_this_injected ? 1 : 0;
        /* A const method (`int spriteForFrame(int) const`) needs its
         * vtable slot's OWN receiver type to say so too -- `canonical`'s
         * own `str2` ("const" or NULL) is set directly by the grammar
         * (func_header/out_of_line_def, parser.y) BEFORE this-injection
         * ever runs, and is never touched afterward, so it's a reliable
         * check regardless of whether `canonical` is an AST_FUNC_DEF
         * (already this-injected -- its own list.items[0] is already a
         * `const ClassName *` receiver) or a still-uninjected
         * AST_FUNC_DECL prototype. A real, previously-undiscovered gap
         * found by actually gcc-compiling a real-world program: without
         * this, every override's own generated function (whose receiver
         * IS correctly emitted as const, from emit_method_prototype's own
         * unrelated, already-correct logic) gets assigned into a vtable
         * slot declared as a plain, non-const pointer -- an incompatible
         * function-pointer-type mismatch gcc only warns about
         * (`-Wincompatible-pointer-types`) but which this project's own
         * established pattern (see strip_const_member_read's and
         * cast_receiver_if_needed's own doc comments in lower.c) says to
         * assume the real, stricter Vircon32 compiler rejects outright,
         * the same shape of gap as every other "Vircon32 is pickier than
         * gcc" bug this project has already found and fixed. */
        int is_const_method = (canonical->str2 != NULL && strcmp(canonical->str2, "const") == 0);
        if (g_target == TARGET_STANDARD) {
            /* Standard C's own function-pointer field declarator needs
             * the name INSIDE the parens -- same reasoning as
             * print_type_and_name's own doc comment above, applied
             * here directly since a vtable slot is a function pointer
             * BY CONSTRUCTION, not something print_type's own general
             * "prefix, caller appends name" pattern could route
             * through even with that helper's own array/func-ptr
             * handling (a struct FIELD name is available here, same
             * as print_type_and_name's own two call sites, but this
             * function builds its own declarator text directly rather
             * than working from an AST_FUNC_PTR_TYPE node at all, so
             * reuses the same SHAPE of fix inline instead of trying to
             * force this through that helper's own single-node
             * contract). */
            print_type(out, canonical->type);
            fprintf(out, " (*%s)(", field_name);
            if (is_const_method) fprintf(out, "const ");
            print_class_type_name(out, canonical_class->str1);
            fprintf(out, " *");
            for (int p = start; p < canonical->list.count; p++) {
                fprintf(out, ", ");
                print_type(out, canonical->list.items[p]->type);
            }
            fprintf(out, ");\n");
        } else {
            print_type(out, canonical->type);
            fprintf(out, "(%s%s *", is_const_method ? "const " : "", canonical_class->str1);
            for (int p = start; p < canonical->list.count; p++) {
                fprintf(out, ", ");
                print_type(out, canonical->list.items[p]->type);
            }
            fprintf(out, ")* %s;\n", field_name);
        }
    }
    fprintf(out, "};\n\n");
}

/* ---- struct definitions -------------------------------------------------
 *
 * Emits `class_decl`'s own struct definition from its already-computed
 * StructLayout (lower.c, phase 1) -- field order, the vtable pointer's
 * position, and which ancestor a data member was inherited from are all
 * already decided there; this function is a near-mechanical
 * pretty-printer over that decision, not a place new layout decisions
 * get made.
 */
static void emit_struct(FILE *out, const AstNode *class_decl) {
    StructLayout *layout = (StructLayout *)class_decl->lower_info;
    if (layout == NULL) return; /* shouldn't happen once lower_run() has
        run over every class -- best-effort skip rather than crash */

    int explained_inherited = 0;
    fprintf(out, "struct %s {\n", class_decl->str1);
    if (layout->count == 0) {
        /* A class with no data members and no vtable (every method,
         * nothing else -- tests/79sample.cpp's own BoxPrinter is
         * exactly this shape: a `friend`-granted accessor class with
         * only a method body, holding no state of its own) would
         * otherwise emit a genuinely empty `struct BoxPrinter {\n};\n`.
         * gcc accepts that (a well-known extension, silent even under
         * `-Wall -Wextra`), which is exactly why this went unnoticed by
         * every `--target=standard`-based check this project's own
         * verification sweeps run -- but the real Vircon32 compiler
         * rejects it outright: "structures must have at least 1
         * member". Confirmed directly against a real compile of
         * tests/79sample.cpp's own generated output, not assumed from
         * a spec reading. Fixed the same way this project already
         * fixes every other "Vircon32 is stricter than either gcc or
         * the C standard requires" case (see cast_receiver_if_needed's
         * and strip_const_member_read's own doc comments in lower.c
         * for two others): insert a single unused placeholder byte
         * field so the struct is never empty, in BOTH target dialects
         * -- not gated to Vircon32-mode only, since a struct that's
         * valid in one dialect and not the other for no reason a user
         * wrote themselves would be a confusing, purely accidental
         * difference between this project's own two output modes. */
        explain(out, 1, "Vircon32 rejects an empty struct outright (\"structures must have at least 1 member\") -- this class has no data members and no vtable of its own, so this unused placeholder byte is here purely to give the struct one");
        fprintf(out, "    char __v32_empty_struct_pad;\n");
    }
    for (int i = 0; i < layout->count; i++) {
        StructField *f = &layout->fields[i];
        if (f->kind == FIELD_VTABLE_PTR) {
            /* explain() BEFORE the field's own "    " prefix below,
             * deliberately -- explain() prints a complete, self-
             * contained line of its own (its own indent, the comment,
             * and a trailing newline), so the field's own "    " prefix
             * has to come AFTER it, not be replaced by it. Getting this
             * backwards was a real, confirmed bug caught by actually
             * reading a -vv run's output, not just by this reasoning:
             * an explain() call sitting where the field's own "    "
             * used to be left the FIELD line itself with no indentation
             * at all, since explain()'s own trailing newline already
             * started a fresh line the old "    " fprintf below was no
             * longer positioned to indent. */
            explain(out, 1, "C has no built-in dynamic dispatch -- this pointer to a table of function pointers is how a virtual call finds the right override at runtime");
            fprintf(out, "    ");
            /* Vircon32 mode: bare, no `struct` keyword -- this is a
             * REFERENCE to the vtable struct type emitted just above
             * by emit_vtable_struct(), not a definition. Standard
             * mode: needs `struct` for the same reason print_type's
             * own AST_IDENT case does for an ordinary class reference
             * -- this is also a struct-tag reference with no typedef
             * of its own. Inlined directly (not via
             * print_class_type_name, which expects a bare name, not
             * one with "_VTable" already appended) since this is the
             * only spot needing exactly this suffixed shape. */
            fprintf(out, "%s%s_VTable *vtable;\n",
                    (g_target == TARGET_STANDARD) ? "struct " : "", class_decl->str1);
        } else {
            if (f->declaring_class != class_decl && !explained_inherited) {
                /* Only explain once, at the FIRST inherited field --
                 * layout->fields is ordered base-first (see
                 * StructLayout's own construction in lower.c), so every
                 * inherited field from here up to the first field this
                 * class itself declares is one contiguous run; one
                 * comment covers the whole run, not one per field.
                 * Tracked with its own flag rather than an index check
                 * (i == 1, say) -- the vtable pointer, if this class has
                 * one at all, isn't always field 0 (a class with no
                 * virtual methods has none), so the first DATA field's
                 * own index isn't fixed either. */
                explain(out, 1, "fields above this point come from a base class -- C structs have no inheritance, so lower.c flattens a base class's own fields directly into every derived class's struct");
                explained_inherited = 1;
            }
            fprintf(out, "    ");
            print_type_and_name(out, f->type, f->name); /* handles the
                function-pointer-in-standard-mode special case
                internally; see its own doc comment */
            fprintf(out, ";\n");
        }
    }
    fprintf(out, "};\n\n");
}

/* ---- vtable static instances --------------------------------------------
 *
 * Emits a static, populated instance of `class_decl`'s own vtable struct
 * TYPE (emit_vtable_struct, above) -- something has existed for a
 * class's `vtable` FIELD to point at since that phase; this is where an
 * actual object gets a value TO put there.
 *
 * The key distinction that makes a vtable work at all: a slot's FIELD
 * NAME always comes from `canonical_method` (stable across the whole
 * hierarchy, matching lower.c's finalize_call, which already dispatches
 * through that same stable name) -- but the VALUE stored in that field,
 * for THIS class's own instance, comes from `entry.method`, whichever
 * implementation actually applies at this level (an override, if one
 * exists here or was inherited from a closer ancestor than whoever
 * declared the slot). Confusing the two would mean every class's
 * instance pointing at the same implementation regardless of overrides,
 * defeating the entire point of having a vtable.
 *
 * Whenever `entry.method`'s own declaring class (found via
 * find_declaring_class, same as finalize_call's receiver-cast logic)
 * differs from `canonical_method`'s (the field's own declared receiver
 * type), the function pointer needs an explicit cast -- assigning
 * `&Circle__draw__void` (a function taking `Circle *`) into a field
 * declared `void(Shape *)*` is the same category of mismatch
 * finalize_call already casts for at call sites, just encountered here
 * at initialization time instead. UNTESTED: Vircon32's cast syntax for
 * ITS reversed function-pointer declarator form is genuinely unknown
 * from here -- `(ReturnType(ParamTypes)*)expr`, matching the declarator
 * pattern with no name inside, is this module's best-reasoned attempt,
 * not a confirmed-working one. Needs a real compile to settle, the same
 * as every other Vircon32-specific syntax choice in this file.
 *
 * A slot whose CURRENT implementation (`entry.method`) has no body at
 * all (a virtual method declared but never defined) gets a literal `0`
 * for that field instead of a function pointer -- there is no C
 * function to point at. Calling that slot at runtime would call through
 * a null pointer; this module doesn't try to prevent that, only avoids
 * emitting a reference to something that doesn't exist. A real,
 * documented limitation, not silently papered over.
 *
 * Uses positional (not C99 designated) struct initialization -- same
 * reasoning as print_type's function-pointer choices elsewhere in this
 * file: safer, more likely to be supported without needing to confirm
 * a second, independent piece of Vircon32-specific syntax.
 *
 * Function-pointer CAST syntax for --target=standard: now IMPLEMENTED,
 * branching internally on g_target in the cast-insertion branch below
 * -- see its own comment there for the full reasoning.
 */
static void emit_vtable_instance(FILE *out, const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL || layout->vtable == NULL) return;

    /* No "struct" keyword here, deliberately -- this is a type
     * REFERENCE (declaring a variable of type ClassName_VTable), not a
     * definition, and Vircon32 requires struct references to be bare
     * (confirmed against the real compiler several rounds back; see
     * print_type's own doc comment for the general rule). This
     * function's FIRST version got this wrong -- hand-wrote "struct
     * %s_VTable" directly instead of following the rule this file
     * already centralizes everywhere else, confirmed as a real,
     * fatal-to-compile mistake against the actual toolchain
     * ("expected '{'"). Fixed here rather than left as a trap for
     * whoever next hand-writes a struct-typed declaration in this file
     * without routing it through print_type or this same reasoning. */
    explain(out, 0, "every object of this class points its own vtable field at THIS single, shared instance -- one copy per class, not one per object");
    fprintf(out, "%s%s_VTable %s_vtable_instance = {\n",
            (g_target == TARGET_STANDARD) ? "struct " : "", class_decl->str1, class_decl->str1);
    for (int i = 0; i < layout->vtable->count; i++) {
        VtableEntry *entry = &layout->vtable->entries[i];
        AstNode *impl = entry->method;
        FuncSemaInfo *impl_info = (FuncSemaInfo *)impl->sema_info;
        const char *impl_mangled = (impl_info != NULL) ? impl_info->mangled_name : impl->str1;

        fprintf(out, "    ");
        if (impl->kind != AST_FUNC_DEF) {
            /* No body exists anywhere for this slot's current
             * implementation -- see this function's own doc comment
             * above for why this can't be a function pointer at all. */
            fprintf(out, "0");
        } else {
            const AstNode *canonical_class = find_declaring_class(class_decl, entry->canonical_method);
            const AstNode *impl_class = find_declaring_class(class_decl, impl);
            if (impl_class != canonical_class) {
                int start = (entry->canonical_method->kind == AST_FUNC_DEF) ? 1 : 0;
                /* Same const-receiver check emit_vtable_struct's own
                 * doc comment explains -- this cast's own target type
                 * has to match the vtable slot's own declared type
                 * exactly (including constness), or this is right back
                 * to the identical incompatible-function-pointer-type
                 * mismatch fixing the slot's own declaration alone
                 * would only have moved here instead of removing. */
                int is_const_method = (entry->canonical_method->str2 != NULL
                    && strcmp(entry->canonical_method->str2, "const") == 0);
                fprintf(out, "(");
                print_type(out, entry->canonical_method->type);
                if (g_target == TARGET_STANDARD) {
                    /* Standard C's own function-pointer CAST syntax
                     * has an EMPTY "()" where a declarator's own name
                     * would go (there's no name to give a cast at
                     * all) -- `(ReturnType (*)(Params))expr`, not
                     * Vircon32's `(ReturnType(Params)*)expr`. Same
                     * "name sits inside the parens, not appended
                     * after" shape as every other function-pointer
                     * fix this round, just with the name slot left
                     * empty since a cast has none. */
                    fprintf(out, " (*)(");
                    if (is_const_method) fprintf(out, "const ");
                    print_class_type_name(out, canonical_class->str1);
                    fprintf(out, " *");
                    for (int p = start; p < entry->canonical_method->list.count; p++) {
                        fprintf(out, ", ");
                        print_type(out, entry->canonical_method->list.items[p]->type);
                    }
                    fprintf(out, "))&%s", impl_mangled);
                } else {
                    fprintf(out, "(%s%s *", is_const_method ? "const " : "", canonical_class->str1);
                    for (int p = start; p < entry->canonical_method->list.count; p++) {
                        fprintf(out, ", ");
                        print_type(out, entry->canonical_method->list.items[p]->type);
                    }
                    fprintf(out, ")*)&%s", impl_mangled);
                }
            } else {
                fprintf(out, "&%s", impl_mangled);
            }
        }
        if (i < layout->vtable->count - 1) fprintf(out, ",");
        fprintf(out, "\n");
    }
    fprintf(out, "};\n\n\n");
}

static void emit_vtable_instances_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            emit_vtable_instance(out, n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_vtable_instances_classes(out, &n->list);
        }
    }
}

/* ---- aggregate types, in dependency order ------------------------------
 *
 * Every struct, class and union is written after the aggregates it holds BY VALUE: the stock compiler
 * rejects `union V { int a; B b; };` printed before `struct B` ("not fully defined", QUIRKS #35), and
 * a struct holding a union or a struct declared later fails the same way. A pointer member needs no
 * order (the forward declarations above cover it). Source order is kept wherever it already works:
 * a depth-first walk in source order that writes a type's by-value members first. Namespaces are
 * flattened (class and union C names are bare). A by-value cycle (A holds B holds A) cannot exist in C:
 * codegen_check_aggregates reports it before anything is written.
 * What counts as by value: a struct's data fields, inherited ones included (the layout is flat), a
 * union's members; through arrays, `const`, and typedef names; not through pointers or references. */
typedef struct { const AstNode *decl; int state; } AggEntry;     /* state: 0 new, 1 on the stack, 2 written */
typedef struct { AggEntry *v; int n, cap; const AstList *roots; const AstNode *err_from, *err_to; int errors; int emit; FILE *out; } AggSet;

static void agg_collect(AggSet *a, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_CLASS_DECL || n->kind == AST_UNION_DECL) {
            if (a->n == a->cap) { a->cap = a->cap ? a->cap * 2 : 64; a->v = realloc(a->v, sizeof(AggEntry) * (size_t)a->cap); }
            a->v[a->n].decl = n;
            a->v[a->n].state = 0;
            a->n++;
        } else if (n->kind == AST_NAMESPACE_DECL) {
            agg_collect(a, &n->list);
        }
    }
}

static const AstNode *agg_find_typedef(const AstList *decls, const char *name) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_TYPEDEF_DECL && n->str1 != NULL && strcmp(n->str1, name) == 0) return n;
        if (n->kind == AST_NAMESPACE_DECL) {
            const AstNode *f = agg_find_typedef(&n->list, name);
            if (f != NULL) return f;
        }
    }
    return NULL;
}

static void agg_write(AggSet *a, int idx);

/* the aggregate a by-value use of `type` needs written first, or -1 */
static int agg_type_dep(AggSet *a, const AstNode *type, int depth) {
    while (type != NULL && depth < 16) {
        if (type->kind == AST_CONST_TYPE || type->kind == AST_ARRAY_TYPE) { type = type->a; continue; }
        const char *name = NULL;
        if (type->kind == AST_IDENT) name = type->str1;
        else if (type->kind == AST_QUALIFIED_ID && type->list.count > 0) name = type->list.items[type->list.count - 1]->str1;
        else return -1;                 /* pointer, reference, function pointer, primitive */
        if (name == NULL) return -1;
        for (int i = 0; i < a->n; i++)
            if (a->v[i].decl->str1 != NULL && strcmp(a->v[i].decl->str1, name) == 0) return i;
        const AstNode *td = agg_find_typedef(a->roots, name);
        if (td == NULL) return -1;
        type = td->type;
        depth++;
    }
    return -1;
}

static void agg_visit_dep(AggSet *a, int from, const AstNode *type) {
    int d = agg_type_dep(a, type, 0);
    if (d < 0) return;
    if (a->v[d].state == 1) {
        if (a->errors == 0) { a->err_from = a->v[from].decl; a->err_to = a->v[d].decl; }
        a->errors++;
        return;
    }
    if (a->v[d].state == 0) agg_write(a, d);
}

static void agg_write(AggSet *a, int idx) {
    const AstNode *n = a->v[idx].decl;
    a->v[idx].state = 1;
    if (n->kind == AST_UNION_DECL) {
        for (int j = 0; j < n->list.count; j++) {
            const AstNode *m = n->list.items[j];
            if (m != NULL && m->kind == AST_VAR_DECL) agg_visit_dep(a, idx, m->type);
        }
    } else {
        StructLayout *layout = (StructLayout *)n->lower_info;
        if (layout != NULL)
            for (int j = 0; j < layout->count; j++)
                if (layout->fields[j].kind != FIELD_VTABLE_PTR) agg_visit_dep(a, idx, layout->fields[j].type);
    }
    a->v[idx].state = 2;
    if (!a->emit) return;
    if (n->kind == AST_UNION_DECL) {
        emit_union(a->out, n);
    } else {
        emit_vtable_struct(a->out, n);
        emit_struct(a->out, n);
    }
}

static int agg_run(const AstList *decls, int emit, FILE *out, const AstNode **from, const AstNode **to) {
    AggSet a = {0};
    a.roots = decls;
    a.emit = emit;
    a.out = out;
    agg_collect(&a, decls);
    for (int i = 0; i < a.n; i++)
        if (a.v[i].state == 0) agg_write(&a, i);
    if (from) *from = a.err_from;
    if (to) *to = a.err_to;
    int errors = a.errors;
    free(a.v);
    return errors;
}

static void emit_classes(FILE *out, const AstList *decls) {
    agg_run(decls, 1, out, NULL, NULL);
}

/* A by-value cycle between structs/unions: an error, said before any output is written. */
int codegen_check_aggregates(const AstNode *program) {
    const AstNode *from = NULL, *to = NULL;
    int errors = agg_run(&program->list, 0, NULL, &from, &to);
    if (errors > 0 && from != NULL && to != NULL) {
        fprintf(stderr, "%s:%d: error: '%s' and '%s' hold each other by value (a type cannot contain itself): "
                        "make one of the members a pointer\n",
                from->file ? from->file : "?", from->line, from->str1, to->str1);
    }
    return errors > 0 ? 1 : 0;
}

/* ---- forward declarations ----------------------------------------------
 *
 * Emits `struct Name;` for every class in the program, before anything
 * else. Confirmed against the real Vircon32 compiler (see
 * docs/DESIGN_NOTES.md's "forward-reference ordering" section for the
 * full test progression) that this is both valid syntax on its own and
 * sufficient to make the BARE name usable as a pointer type from that
 * point forward, even before the class's own full struct definition
 * appears -- which is exactly what resolves the general case this
 * module's struct-emission order alone couldn't: two classes holding
 * pointers to each other, or simply one class textually preceding
 * another it points to, no longer depends on source order being
 * "lucky" the way every existing test file's happened to be so far.
 *
 * Confirmed NOT to redefine/conflict with the later full definition --
 * forward-declaring then fully defining the same tag in the same file is
 * fine, standard C's own rule and Vircon32's alike.
 */
static void emit_forward_declarations(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            fprintf(out, "struct %s;\n", n->str1);
        } else if (n->kind == AST_UNION_DECL) {
            /* a pointer to a union may come before the union (the stock compiler takes `union U;`) */
            fprintf(out, "union %s;\n", n->str1);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_forward_declarations(out, &n->list);
        }
    }
}

/* ---- expression printing ------------------------------------------------
 *
 * Prints an expression in valid Vircon32 C syntax. Every AST kind this
 * function handles is exactly the shape lower.c's phases leave behind in
 * a FULLY LOWERED body (this-injected, calls finalized, operators
 * resolved, references converted, new/delete already rewritten to
 * placeholder calls) -- AST_THIS, AST_NEW, and AST_DELETE should never
 * actually reach here (phase 2 rewrites every AST_THIS into
 * AST_IDENT("this"); phase 6 rewrites every AST_NEW/AST_DELETE into an
 * AST_CALL). They're handled anyway, with a loud comment instead of
 * silently producing nothing, on the same best-effort principle
 * print_type() already follows for AST_REFERENCE_TYPE -- seeing one of
 * these actually fire means investigate immediately, not "huh, weird."
 *
 * PARENTHESIZATION STRATEGY: every binary/assignment/prefix-unary
 * expression is wrapped in its own parentheses, unconditionally. This is
 * deliberately the simplest possible correct strategy for a first pass --
 * it trades slightly noisier output for a total absence of precedence
 * bugs, rather than trying to reconstruct C's precedence table and risk
 * getting one operator's binding wrong. A real language's compiler earns
 * the right to skip redundant parens by construction; a from-scratch
 * code generator hasn't earned that yet.
 */
static void print_expr(FILE *out, const AstNode *e);

/* Prints a string literal's raw body (escapes undecoded) in the escape
 * forms the Vircon32 C lexer actually reads: it knows only \n \r \t \\
 * \' \" and \x with EXACTLY two hex digits -- \0 and every octal escape
 * draw an "unknown escape character" warning and decode as the digit
 * itself ('\0' becomes '0'), and a one-digit \x7 would swallow the next
 * character. Everything else is rewritten as \xHH. */
static void print_vircon32_string(FILE *out, const char *raw) {
    fputc('"', out);
    for (const char *p = raw; *p; ) {
        if (*p != '\\' || p[1] == '\0') { fputc(*p++, out); continue; }
        char c = p[1];
        if (c == 'n' || c == 'r' || c == 't' || c == '\\' || c == '\'' || c == '"') {
            fputc('\\', out);
            fputc(c, out);
            p += 2;
            continue;
        }
        p++;
        int v = ast_decode_escape(&p);
        fprintf(out, "\\x%02x", (unsigned)(v & 0xFF));
    }
    fputc('"', out);
}

static void print_char_literal(FILE *out, int code) {
    /* Basic, standard C escaping -- not yet exercised by any test (no
     * current sample has a char literal reach codegen), implemented
     * defensively rather than left to crash or silently misprint
     * whenever one eventually does. */
    switch (code) {
        case '\n': fprintf(out, "'\\n'"); return;
        case '\t': fprintf(out, "'\\t'"); return;
        case '\r': fprintf(out, "'\\r'"); return;
        case '\\': fprintf(out, "'\\\\'"); return;
        case '\'': fprintf(out, "'\\''"); return;
        case '\0':
            /* Vircon32 C has no \0 escape: it warns and reads '0' (48). */
            fprintf(out, g_target == TARGET_VIRCON32 ? "'\\x00'" : "'\\0'");
            return;
        default:
            if (code >= 32 && code < 127) {
                fprintf(out, "'%c'", (char)code);
            } else {
                fprintf(out, "'\\x%02x'", (unsigned)(code & 0xFF));
            }
            return;
    }
}

static void print_unop(FILE *out, const AstNode *e) {
    const char *op = e->str1;
    if (strcmp(op, "post++") == 0) {
        fprintf(out, "(");
        print_expr(out, e->a);
        fprintf(out, "++)");
    } else if (strcmp(op, "post--") == 0) {
        fprintf(out, "(");
        print_expr(out, e->a);
        fprintf(out, "--)");
    } else if (strcmp(op, "pre++") == 0) {
        fprintf(out, "(++");
        print_expr(out, e->a);
        fprintf(out, ")");
    } else if (strcmp(op, "pre--") == 0) {
        fprintf(out, "(--");
        print_expr(out, e->a);
        fprintf(out, ")");
    } else if (strcmp(op, "neg") == 0) {
        fprintf(out, "(-");
        print_expr(out, e->a);
        fprintf(out, ")");
    } else if (strcmp(op, "addr") == 0) {
        fprintf(out, "(&");
        print_expr(out, e->a);
        fprintf(out, ")");
    } else if (strcmp(op, "deref") == 0) {
        fprintf(out, "(*");
        print_expr(out, e->a);
        fprintf(out, ")");
    } else {
        /* "!" and "~" are already the literal C operator text. */
        fprintf(out, "(%s", op);
        print_expr(out, e->a);
        fprintf(out, ")");
    }
}

static void print_expr(FILE *out, const AstNode *e) {
    if (e == NULL) return;
    switch (e->kind) {
        case AST_INT_LIT:
            /* A negative value only arises from a negative named constant
             * (`#define LOW -1`) whose name can't be printed; parenthesized
             * so `x - LOW` can never come out as the decrement `x --1`. */
            if (!print_macro_name(out, e)) {
                /* INT_MIN has no positive twin: Vircon32 C reads
                 * "2147483648" first and rejects it ("number out of range
                 * for a 32-bit integer"), so 0x80000000 and -2147483648
                 * are spelled as a subtraction. */
                if (e->ival == -2147483647 - 1) fprintf(out, "(-2147483647 - 1)");
                else fprintf(out, e->ival < 0 ? "(%d)" : "%d", e->ival);
            }
            break;
        case AST_INIT_LIST:
            /* `{1, 2, 3}` -- positional, same reasoning as
             * emit_vtable_instance's own struct-initializer choice:
             * simpler and more likely to be accepted without needing to
             * confirm a second, independent piece of Vircon32-specific
             * syntax than C99 designated initializers would be. Only
             * ever appears as a var_decl's own initializer (`a`), for an
             * array-typed one -- not a general expression position. */
            fprintf(out, "{");
            for (int i = 0; i < e->list.count; i++) {
                if (i > 0) fprintf(out, ", ");
                print_expr(out, e->list.items[i]);
            }
            fprintf(out, "}");
            break;
        case AST_FLOAT_LIT:
            /* print_float_literal: shortest exact spelling, and ALWAYS
             * with a '.' or exponent -- see its own comment for the bug
             * that requirement fixes. */
            if (!print_macro_name(out, e)) {
                char text[400];
                format_float_literal(text, sizeof text, e->fval);
                fprintf(out, e->fval < 0 ? "(%s)" : "%s", text);
            }
            break;
        case AST_BOOL_LIT:
            /* Assumes Vircon32 C has `true`/`false` keywords, consistent
             * with it having a native `bool` type (used throughout this
             * project's own test suite already) -- not yet independently
             * confirmed against the real compiler the way the struct/
             * forward-declaration quirks were. Worth confirming if this
             * is ever the line that fails to compile. */
            fprintf(out, "%s", e->ival ? "true" : "false");
            break;
        case AST_CHAR_LIT:
            if (!print_macro_name(out, e)) print_char_literal(out, e->ival);
            break;
        case AST_NULL_LIT:
            if (g_c_mode && g_target == TARGET_VIRCON32) {
                /* C input: the null pointer is 0, as C says -- not
                 * Vircon32 C's NULL, which is -1 (zeroed memory and
                 * `if (p)` both depend on it; see lower.c, phase 12). */
                fprintf(out, "((void *)0)");
                break;
            }
            /* `nullptr` always prints as the literal word `NULL`, in
             * BOTH target dialects -- Vircon32's own compiler rejects a
             * bare `0` in pointer context outright (docs/
             * VIRCON32_QUIRKS.md), so this is the one literal this
             * project cannot afford to print naively even though gcc
             * itself wouldn't mind a bare 0 in --target=standard output.
             * `NULL` is already in scope everywhere a program can
             * possibly reach here: unconditionally via misc.h's own
             * `#include` for TARGET_VIRCON32 (codegen_run, near the top
             * of this file), and via <stdlib.h>'s own unconditional
             * `#include` for TARGET_STANDARD (this file's own comment on
             * that `#include`, added for `new`/`delete`'s own malloc/
             * free calls, which already guarantees NULL is declared
             * there too). */
            fprintf(out, "NULL");
            break;
        case AST_IDENT:
            fprintf(out, "%s", e->str1);
            break;
        case AST_THIS:
            /* Shouldn't happen -- this-injection (lower.c phase 2)
             * rewrites every AST_THIS into AST_IDENT("this") well before
             * codegen ever runs. */
            fprintf(out, "this" /* WARNING: unlowered AST_THIS reached codegen */);
            break;
        case AST_MEMBER:
            /* `(*p).m` is printed `(p)->m`: the same in C, and Vircon32 C
             * evaluates p TWICE in the first form when p is a call
             * (`(*get()).n` calls get() twice; `get()->n` once) -- the
             * shape every member access on a reference-returning call
             * takes. See VIRCON32_QUIRKS.md #27. */
            if (e->str1 != NULL && strcmp(e->str1, ".") == 0 && e->a != NULL &&
                e->a->kind == AST_UNOP && e->a->str1 != NULL &&
                strcmp(e->a->str1, "deref") == 0) {
                fprintf(out, "(");
                print_expr(out, e->a->a);
                fprintf(out, ")->%s", e->str2);
                break;
            }
            print_expr(out, e->a);
            fprintf(out, "%s%s", e->str1, e->str2);
            break;
        case AST_CALL:
            print_expr(out, e->a);
            fprintf(out, "(");
            for (int i = 0; i < e->list.count; i++) {
                if (i > 0) fprintf(out, ", ");
                print_expr(out, e->list.items[i]);
            }
            fprintf(out, ")");
            break;
        case AST_BINOP:
            fprintf(out, "(");
            print_expr(out, e->a);
            fprintf(out, " %s ", e->str1);
            print_expr(out, e->b);
            fprintf(out, ")");
            break;
        case AST_ASSIGN:
            fprintf(out, "(");
            print_expr(out, e->a);
            fprintf(out, " %s ", e->str1);
            print_expr(out, e->b);
            fprintf(out, ")");
            break;
        case AST_TERNARY:
            fprintf(out, "(");
            print_expr(out, e->a);
            fprintf(out, " ? ");
            print_expr(out, e->b);
            fprintf(out, " : ");
            print_expr(out, e->c);
            fprintf(out, ")");
            break;
        case AST_UNOP:
            print_unop(out, e);
            break;
        case AST_SUBSCRIPT:
            if (e->a != NULL && (e->a->kind == AST_CALL || e->a->kind == AST_CAST ||
                                 e->a->kind == AST_BINOP || e->a->kind == AST_UNOP)) {
                /* `f()[i]`, `(p + 1)[i]`, `(&a[0])[i]` -- subscripting a
                 * computed pointer rather than a named one. The
                 * real Vircon32 C compiler dies on it when a member
                 * access follows (`f()[3].x`: "cannot emit memory
                 * placement when an expression has none"), so it is
                 * written the long way, `(*(f() + i))`, which it accepts
                 * and which means the same thing in standard C. */
                fprintf(out, "(*(");
                print_expr(out, e->a);
                fprintf(out, " + ");
                print_expr(out, e->b);
                fprintf(out, "))");
                break;
            }
            print_expr(out, e->a);
            fprintf(out, "[");
            print_expr(out, e->b);
            fprintf(out, "]");
            break;
        case AST_CAST:
            /* An explicit "(Type)expr", introduced only by lower.c's
             * finalize_call (see AST_CAST's own doc comment in ast.h) --
             * confirmed valid Vircon32 syntax (Matthew tested
             * "(Node *) 0" directly against the real compiler). */
            fprintf(out, "((");
            print_type(out, e->type);
            fprintf(out, ")");
            print_expr(out, e->a);
            fprintf(out, ")");
            break;
        case AST_SIZEOF:
            /* Literal `sizeof(...)` -- exactly one of type/a is set
             * (see AST_SIZEOF's own doc comment in ast.h), so exactly
             * one of these two branches ever fires for a given node. */
            fprintf(out, "sizeof(");
            if (e->type != NULL) {
                print_type(out, e->type);
            } else {
                print_expr(out, e->a);
            }
            fprintf(out, ")");
            break;
        case AST_QUALIFIED_ID:
            /* Not expected as a general expression (this project's
             * grammar only ever produces one as the class-name marker on
             * an out-of-line FuncDef's own `b`, which codegen never
             * reads as an expression at all) -- flattened to its last
             * component anyway, matching print_type's own convention,
             * as a best-effort fallback rather than emitting nothing. */
            if (e->list.count > 0) {
                fprintf(out, "%s", e->list.items[e->list.count - 1]->str1);
            }
            break;
        case AST_NEW:
        case AST_DELETE:
            /* Shouldn't happen -- lower.c phase 6 rewrites every one of
             * these into an AST_CALL before codegen ever runs. */
            fprintf(out, "0 /* WARNING: unlowered New/Delete reached codegen */");
            break;
        case AST_STRING_LIT:
            /* The lexer stores the string's INNER text -- quotes already
             * stripped, escape sequences deliberately left as literal
             * backslash pairs (see lexer.l's STRING_LITERAL rule, which
             * memcpy's yytext+1 .. len-2). So re-quote it here. No
             * re-escaping pass is needed: the lexer's own regex only
             * admits a '"' inside a string as part of a \" pair, which is
             * already exactly the escape syntax the output C wants, so
             * wrapping the stored text back in quotes round-trips it.
             * Found via a real user program (a Space Invaders port)
             * calling drawText(video, "SPACE INVADERS", ...) -- the call
             * parsed and resolved fine, then silently emitted a bare 0
             * here, which Vircon32 rejected at the call site.
             * Vircon32 mode rewrites the escapes its lexer doesn't read
             * (see print_vircon32_string). */
            if (g_target == TARGET_VIRCON32) print_vircon32_string(out, e->str1);
            else fprintf(out, "\"%s\"", e->str1);
            break;
        default:
            fprintf(out, "0 /* WARNING: unhandled expression kind in codegen */");
            break;
    }
}

/* ---- statement printing --------------------------------------------------
 *
 * Prints a statement in valid Vircon32 C syntax, `indent` levels deep
 * (4 spaces per level, matching this module's existing struct-field
 * indentation). Every statement kind a fully-lowered body can contain is
 * handled; anything else is a genuine gap, flagged loudly rather than
 * silently dropped.
 */
static void print_stmt(FILE *out, const AstNode *s, int indent, int strip_return_value);

static void indent_spaces(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) fprintf(out, "    ");
}

/* Prints "Type name" or "Type name = init", with NO trailing semicolon
 * and NO indentation/newline of its own -- shared by an ordinary
 * AST_VAR_DECL statement (which adds the semicolon/newline/indent
 * itself) and a for-loop's init clause (which needs this sitting inline
 * inside the for(...) header instead). */
static void print_var_decl_inline(FILE *out, const AstNode *n) {
    print_type_and_name(out, n->type, n->str1); /* handles the
        function-pointer-in-standard-mode special case internally;
        see its own doc comment */
    if (n->a != NULL) {
        fprintf(out, " = ");
        print_expr(out, n->a);
    }
}

/* `strip_return_value`: 1 only when printing `main`'s own body (see
 * emit_function_definition) -- Vircon32 requires `void main()`, so a
 * `return expr;` inside it (perfectly ordinary in the C++ source, which
 * might have declared `int main()`) has to become `expr; return;`
 * instead: evaluate the expression as a statement, for any side effects
 * it might have, then a bare `return;` to satisfy the void signature.
 * Threaded through every recursive call rather than re-detected at each
 * AST_RETURN, since a return can be arbitrarily nested inside main's own
 * if/while/for/block structure and there's nothing about a RETURN
 * statement itself that says which function it belongs to. */
static int g_debug_last_cpp_line = -1;

/* --line-markers (see codegen_set_line_markers): where the preprocessor's own line count stands. */
static int g_line_markers = 0;
static const char *g_lm_main_file = NULL;
static int g_lm_valid = 0, g_lm_out_line = 0, g_lm_line = 0;
static const char *g_lm_file = NULL;

void codegen_set_line_markers(int on, const char *main_file) {
    g_line_markers = on;
    g_lm_main_file = main_file;
    g_lm_valid = 0;
}

/* Writes `#line LINE "FILE"` unless the preprocessor would already count LINE for the line about
 * to be written (it counts one per output line since the last directive). Always at the start of
 * an output line: print_stmt and the function definition are only ever reached there. */
static void line_marker(FILE *out, int line, const char *file) {
    if (!g_line_markers || line <= 0) return;
    const char *f = file != NULL ? file : g_lm_main_file;
    if (f == NULL) return;
    if (g_lm_valid && strcmp(f, g_lm_file) == 0 &&
        g_lm_line + (g_codegen_out_line - g_lm_out_line) == line) return;
    char esc[1024];
    size_t n = 0;
    for (const char *c = f; *c != '\0' && n + 2 < sizeof esc; c++) {
        if (*c == '\\' || *c == '"') esc[n++] = '\\';
        esc[n++] = *c;
    }
    esc[n] = '\0';
    fprintf(out, "#line %d \"%s\"\n", line, esc);
    g_lm_valid = 1;
    g_lm_file = f;                     /* callers pass interned or long-lived strings */
    g_lm_line = line;
    g_lm_out_line = g_codegen_out_line; /* the line after the directive is LINE */
}
static const char *g_debug_cur_file = NULL; /* source file of the function being emitted */

static void print_stmt(FILE *out, const AstNode *s, int indent, int strip_return_value) {
    if (s == NULL) return;
    /* -g support: a new debug_map entry whenever this statement's own
     * source line differs from the last one recorded -- matches the
     * sparse, "only where the mapping actually changes" structure of
     * the example .asm.debug file (see debugmap.h's own doc comment),
     * not an exhaustive per-output-line table. Fires for every
     * statement kind, AST_BLOCK included -- a block's own opening-brace
     * line occasionally duplicates its first child's, in which case
     * this check simply skips the redundant second entry on its own. A
     * lowering-synthesized node (a destructor invocation at scope exit,
     * say) still carries SOME line -- typically the nearest real
     * statement's own, reused -- so this stays reasonably accurate even
     * for code this project's own lowering passes generated, not just
     * for what the person directly wrote; it is not perfectly precise
     * for every synthesized statement, and isn't claimed to be. */
    if (s->kind != AST_BLOCK) line_marker(out, s->line, g_debug_cur_file);
    if (s->line != g_debug_last_cpp_line) {
        debug_map_record(g_codegen_out_line, s->line, g_debug_cur_file, NULL);
        g_debug_last_cpp_line = s->line;
    }
    switch (s->kind) {
        case AST_BLOCK:
            indent_spaces(out, indent);
            fprintf(out, "{\n");
            for (int i = 0; i < s->list.count; i++) {
                print_stmt(out, s->list.items[i], indent + 1, strip_return_value);
            }
            indent_spaces(out, indent);
            fprintf(out, "}\n");
            break;
        case AST_IF:
            indent_spaces(out, indent);
            fprintf(out, "if (");
            print_expr(out, s->a);
            fprintf(out, ")\n");
            print_stmt(out, s->b, indent, strip_return_value); /* s->b is itself an AST_BLOCK -- prints its own braces */
            if (s->c != NULL) {
                indent_spaces(out, indent);
                fprintf(out, "else\n");
                print_stmt(out, s->c, indent, strip_return_value);
            }
            break;
        case AST_WHILE:
            if (s->ival == 1) {
                /* do-while -- test after the body runs once
                 * unconditionally, not before (see AST_WHILE's own doc
                 * comment in ast.h for why this reuses the same node
                 * kind, distinguished only by this flag). */
                indent_spaces(out, indent);
                fprintf(out, "do\n");
                print_stmt(out, s->b, indent, strip_return_value);
                indent_spaces(out, indent);
                fprintf(out, "while (");
                print_expr(out, s->a);
                fprintf(out, ");\n");
                break;
            }
            indent_spaces(out, indent);
            fprintf(out, "while (");
            print_expr(out, s->a);
            fprintf(out, ")\n");
            print_stmt(out, s->b, indent, strip_return_value);
            break;
        case AST_FOR:
            indent_spaces(out, indent);
            fprintf(out, "for (");
            if (s->a != NULL) {
                if (s->a->kind == AST_VAR_DECL) {
                    print_var_decl_inline(out, s->a);
                } else if (s->a->kind == AST_EXPR_STMT) {
                    print_expr(out, s->a->a);
                }
            }
            fprintf(out, "; ");
            print_expr(out, s->b); /* NULL prints nothing -- "for(;;)" is valid C */
            fprintf(out, "; ");
            print_expr(out, s->c);
            fprintf(out, ")\n");
            print_stmt(out, s->d, indent, strip_return_value);
            break;
        case AST_RETURN:
            if (strip_return_value && s->a != NULL) {
                indent_spaces(out, indent);
                print_expr(out, s->a);
                fprintf(out, ";\n");
                indent_spaces(out, indent);
                fprintf(out, "return;\n");
                break;
            }
            indent_spaces(out, indent);
            fprintf(out, "return");
            if (s->a != NULL) {
                fprintf(out, " ");
                print_expr(out, s->a);
            }
            fprintf(out, ";\n");
            break;
        case AST_BREAK:
            /* Ordinary C, no Vircon32-specific quirk here -- unlike
             * several other statement/declarator forms in this file,
             * this one needed no confirmation against the real compiler
             * before being confident about it. */
            indent_spaces(out, indent);
            fprintf(out, "break;\n");
            break;
        case AST_CONTINUE:
            indent_spaces(out, indent);
            fprintf(out, "continue;\n");
            break;
        case AST_GOTO:
            indent_spaces(out, indent);
            fprintf(out, "goto %s;\n", s->str1);
            break;
        case AST_LABEL:
            /* Real C's own label syntax: the label itself is NOT
             * indented to the current statement level (conventional C
             * style -- a label sits at column 0, or close to it,
             * visually separate from the code around it), but the
             * statement it precedes still gets ordinary indentation,
             * printed via the normal recursive call. */
            fprintf(out, "%s:\n", s->str1);
            print_stmt(out, s->a, indent, strip_return_value);
            break;
        case AST_SWITCH:
            /* No lowering transformation happened for this node at all
             * (see AST_SWITCH's own doc comment in ast.h) -- straight,
             * literal C switch/case/default, since Vircon32 C already
             * has this natively. Each body item's own indent: a CASE/
             * DEFAULT label sits one level in from the switch's own
             * braces; an ordinary statement between labels sits one
             * level deeper than that (conventional C style -- purely
             * cosmetic, doesn't affect what the generated C actually
             * does, but matches how a person would write this by hand). */
            indent_spaces(out, indent);
            fprintf(out, "switch (");
            print_expr(out, s->a);
            fprintf(out, ") {\n");
            for (int i = 0; i < s->list.count; i++) {
                AstNode *item = s->list.items[i];
                int item_indent = (item->kind == AST_CASE || item->kind == AST_DEFAULT) ? indent + 1 : indent + 2;
                print_stmt(out, item, item_indent, strip_return_value);
            }
            indent_spaces(out, indent);
            fprintf(out, "}\n");
            break;
        case AST_CASE:
            indent_spaces(out, indent);
            fprintf(out, "case ");
            print_expr(out, s->a);
            fprintf(out, ":\n");
            break;
        case AST_DEFAULT:
            indent_spaces(out, indent);
            fprintf(out, "default:\n");
            break;
        case AST_EXPR_STMT:
            /* -vv support: two lowering-synthesized call PATTERNS get
             * explained here, detected structurally (never by anything
             * the C++ source itself could have written) rather than by
             * a dedicated AST flag -- both naming conventions are
             * reserved ones only this project's own lowering ever
             * produces (a "__dtor__void"-suffixed mangled name; a
             * "->vtable->" member-access chain), so recognizing them by
             * shape is exactly as reliable as a flag would be, without
             * needing one threaded through from lower.c. */
            if (s->a != NULL && s->a->kind == AST_CALL && s->a->a != NULL) {
                const AstNode *callee = s->a->a;
                if (callee->kind == AST_IDENT && callee->str1 != NULL) {
                    size_t len = strlen(callee->str1);
                    const char *suffix = "__dtor__void";
                    size_t suffix_len = strlen(suffix);
                    if (len >= suffix_len && strcmp(callee->str1 + len - suffix_len, suffix) == 0) {
                        explain(out, indent, "destructor invoked automatically here -- lower.c's phase 9 inserts this at scope exit, the same place C++ itself would silently run it");
                    }
                } else if (callee->kind == AST_MEMBER && callee->a != NULL
                           && callee->a->kind == AST_MEMBER
                           && callee->a->str2 != NULL && strcmp(callee->a->str2, "vtable") == 0) {
                    explain(out, indent, "virtual call: dispatched through the vtable at runtime, not a fixed function -- which override actually runs depends on the object's real (dynamic) type, not this pointer's declared (static) type");
                }
            }
            indent_spaces(out, indent);
            print_expr(out, s->a);
            fprintf(out, ";\n");
            break;
        case AST_VAR_DECL:
            /* -vv support: a VarDecl whose own initializer is (possibly
             * through an AST_CAST -- lower.c's phase 6a implicit-upcast
             * insertion) a call to a "v32_new_"-prefixed name is exactly
             * what `new` lowers to (lower.c phase 6) -- detected the
             * same "reserved prefix only this project's own lowering
             * ever produces" way as the destructor/vtable patterns
             * above, not via a dedicated flag. */
            if (s->a != NULL) {
                const AstNode *init = s->a;
                if (init->kind == AST_CAST) init = init->a; /* unwrap an implicit-upcast cast, if any */
                if (init != NULL && init->kind == AST_CALL && init->a != NULL
                    && init->a->kind == AST_IDENT && init->a->str1 != NULL
                    && strncmp(init->a->str1, "v32_new_", 8) == 0) {
                    explain(out, indent, "'new' lowers to a call to this project's own allocator function (defined near the top of this file) -- allocate, then construct, in one step");
                }
            }
            indent_spaces(out, indent);
            print_var_decl_inline(out, s);
            fprintf(out, ";\n");
            break;
        case AST_DELETE:
            /* Shouldn't happen as a bare statement either -- an
             * AST_DELETE used as an AST_EXPR_STMT's own child (the only
             * way it appears in source, `delete p;`) is rewritten to an
             * AST_CALL by lower.c phase 6 before codegen ever runs, so
             * AST_EXPR_STMT's own case above prints the resulting call,
             * never reaching this case directly. */
            indent_spaces(out, indent);
            fprintf(out, "/* WARNING: unlowered AST_DELETE reached codegen */;\n");
            break;
        case AST_ASM:
            /* Pure pass-through: the Vircon32 C compiler is the one that
             * actually assembles this. TARGET_VIRCON32 always emits the
             * brace form regardless of which dialect was written, since
             * that's the only form Vircon32's compiler accepts; TARGET_
             * STANDARD keeps the dialect as written (gcc/clang accept the
             * parenthesized basic-asm spelling natively, and a brace-form
             * body re-wrapped in parentheses is equally valid there).
             * One string literal per line, matching how video.h itself
             * formats multi-instruction blocks -- purely cosmetic.
             * Re-quoting each literal is the same round-trip rule as
             * AST_STRING_LIT's own case above: the lexer stored the inner
             * text with escapes still raw, so wrapping it back in quotes
             * reproduces the source literal exactly, `{param}` braces
             * included. */
            if (s->ival == 1 && g_target == TARGET_STANDARD) {
                indent_spaces(out, indent);
                fprintf(out, "asm(");
                for (int i = 0; i < s->list.count; i++) {
                    if (i > 0) fprintf(out, " ");
                    fprintf(out, "\"%s\"", s->list.items[i]->str1);
                }
                fprintf(out, ");\n");
            }
            else {
                indent_spaces(out, indent);
                fprintf(out, "asm\n");
                indent_spaces(out, indent);
                fprintf(out, "{\n");
                for (int i = 0; i < s->list.count; i++) {
                    indent_spaces(out, indent + 1);
                    fprintf(out, "\"%s\"\n", s->list.items[i]->str1);
                }
                indent_spaces(out, indent);
                fprintf(out, "}\n");
            }
            break;
        default:
            indent_spaces(out, indent);
            fprintf(out, "/* WARNING: unhandled statement kind in codegen */;\n");
            break;
    }
}

/* ---- method/function prototypes and definitions --------------------------
 *
 * Both a prototype and a full definition print the same "ReturnType
 * MangledName(Type1 name1, Type2 name2, ...)" header; only the
 * terminator differs (";" vs. " { ...body... }"). Shared here so the two
 * can never drift out of sync with each other.
 *
 * Only ever called with an AST_FUNC_DEF -- a genuine body to emit.
 * Deliberately NOT called for a prototype-only AST_FUNC_DECL (a
 * constructor/destructor/method declared but never defined anywhere,
 * e.g. Timer's constructor in tests/sample1.cpp): this project has
 * nothing to emit for one (no body exists, and this-injection never
 * touches anything that isn't AST_FUNC_DEF, so its parameter list
 * wouldn't even have the receiver in it). KNOWN, DELIBERATE LIMITATION:
 * if a prototype-only method is ever actually CALLED somewhere, the
 * generated C will fail to COMPILE (an undeclared-identifier error, no
 * prototype exists for the call to resolve against) rather than fail to
 * LINK the way a merely-unimplemented-but-declared C function normally
 * would. Revisit once this project has any notion of an abstract/pure-
 * virtual method that's expected to be called polymorphically without
 * ever having its own body.
 */
/* `name` is passed in rather than re-derived, since emit_function_
 * definition below needs it too (to decide whether to strip return
 * values in the body) and there's no reason to look it up twice. */
static void emit_function_header(FILE *out, const AstNode *func, const char *name) {
    int force_void_main = (strcmp(name, "main") == 0 && g_target == TARGET_VIRCON32);
    /* Vircon32 requires `void main()` specifically, regardless of what
     * the C++ source actually declared (commonly `int main()`) -- see
     * mangle()'s own doc comment in sema.c for why this is forced here
     * rather than requiring the source to already declare it that way.
     * Standard mode deliberately does NOT force this: unlike every
     * other --target difference this project has, forcing `void` here
     * changes actual PROGRAM BEHAVIOR (an exit code becomes
     * unobservable), not just surface syntax -- docs/VIRCON32_QUIRKS.md
     * flagged this as needing a deliberate decision rather than a
     * silent default when this flag got built, and the decision made
     * is: standard mode honors whatever `main`'s own C++ declaration
     * actually said (typically `int`), the ordinary, unsurprising
     * choice for real, portable C. */

    if (force_void_main) {
        fprintf(out, "void");
    } else {
        print_type(out, func->type);
    }
    fprintf(out, " %s(", name);
    if (func->list.count == 0) {
        fprintf(out, "void"); /* Vircon32/C: an empty parameter list needs
            an explicit "void", not bare "()" (which in C means "unspecified
            parameters", not "no parameters") */
    }
    for (int p = 0; p < func->list.count; p++) {
        if (p > 0) fprintf(out, ", ");
        AstNode *param = func->list.items[p];
        print_type(out, param->type);
        fprintf(out, " %s", param->str1);
    }
    fprintf(out, ")");
}

static void emit_function_prototype(FILE *out, const AstNode *func) {
    FuncSemaInfo *info = (FuncSemaInfo *)func->sema_info;
    const char *name = (info != NULL) ? info->mangled_name : func->str1;
    emit_function_header(out, func, name);
    fprintf(out, ";\n");
}

/* Does the subtree under `n` use the identifier `name` anywhere? */
static int uses_ident(const AstNode *n, const char *name) {
    if (n == NULL) return 0;
    if (n->kind == AST_IDENT && n->str1 != NULL && strcmp(n->str1, name) == 0) return 1;
    if (uses_ident(n->a, name) || uses_ident(n->b, name) || uses_ident(n->c, name) ||
        uses_ident(n->d, name)) return 1;
    for (int i = 0; i < n->list.count; i++)
        if (uses_ident(n->list.items[i], name)) return 1;
    return 0;
}

/* --silence-unused-args: the Vircon32 compiler warns "argument X is not
 * used" for every parameter a function ignores -- `this` of a method that
 * touches no member, a callback's argument its type requires -- and stops
 * after 10 warnings. A bare `X;` statement counts as a use, so each such
 * parameter gets one as the first statement of the body. */
static void silence_unused_params(const AstNode *func) {
    AstNode *body = func->a;
    if (body == NULL || body->kind != AST_BLOCK) return;
    int first = 0;
    for (int i = 0; i < func->list.count; i++) {
        const AstNode *param = func->list.items[i];
        if (param == NULL || param->kind != AST_PARAM || param->str1 == NULL || param->str1[0] == '\0')
            continue;
        if (uses_ident(body, param->str1)) continue;
        AstNode *use = ast_new(AST_IDENT, func->line);
        use->str1 = strdup(param->str1);
        AstNode *stmt = ast_new(AST_EXPR_STMT, func->line);
        stmt->a = use;
        ast_list_append(&body->list, stmt);               /* then rotate it to the front */
        for (int j = body->list.count - 1; j > first; j--)
            body->list.items[j] = body->list.items[j - 1];
        body->list.items[first++] = stmt;
    }
}

static void emit_function_definition(FILE *out, const AstNode *func) {
    FuncSemaInfo *info = (FuncSemaInfo *)func->sema_info;
    const char *name = (info != NULL) ? info->mangled_name : func->str1;
    int force_void_main = (strcmp(name, "main") == 0 && g_target == TARGET_VIRCON32);
    /* Matches emit_function_header's own identical condition just
     * above -- deliberately kept as the exact same expression in both
     * places rather than, say, computing it once and threading it
     * through as a parameter, so the two can never drift apart and
     * disagree about whether THIS particular `main` gets the void-
     * forcing/return-value-stripping treatment. strip_return_value
     * (passed to print_stmt below) only ever needs to fire alongside
     * the void-forcing above: stripping `return expr;` down to
     * `expr; return;` only makes sense when the signature genuinely
     * became `void`, never when standard mode is honoring the
     * source's own real return type instead. */

    /* -g support: this function's own first line of C output always
     * gets an entry, function_name included -- unlike print_stmt's own
     * "only if the line changed" check, a function boundary is always
     * worth recording explicitly, even on the rare chance its own line
     * happens to match whatever print_stmt last recorded for some
     * earlier function. Records against `name` (the MANGLED, C-side
     * name that actually appears in the output -- e.g. "Square__area__void",
     * not func->str1's original "area") -- this entry describes what the
     * .c file's own function is called, not what the .cpp called it,
     * matching which side of the mapping the function-name column is
     * actually documenting. */
    /* The function's own file stands for every statement in it: a function
     * never spans files, and statements synthesized by lowering don't
     * carry a reliable file of their own (see AstNode.file). */
    g_debug_cur_file = func->file;
    debug_map_record(g_codegen_out_line, func->line, g_debug_cur_file, name);
    g_debug_last_cpp_line = func->line;

    /* this-injection (lower.c phase 2) gives every METHOD an explicit
     * first parameter literally named "this" -- checked by name here
     * rather than by, say, "is this func a method at all" (which would
     * need walking back up to the owning class), since the parameter's
     * own name is already the simplest, always-available signal that
     * this happened. Explained once, at the definition (not also at
     * the prototype -- emit_function_prototype shares this same
     * emit_function_header, but a one-line prototype declaration isn't
     * where a reader benefits from a multi-line explanation the way a
     * function's own body is). */
    if (func->list.count > 0 && strcmp(func->list.items[0]->str1, "this") == 0) {
        explain(out, 0, "C has no implicit object parameter -- this-injection gives every method an explicit 'this' as its first parameter, standing in for what C++'s own 'this' would otherwise mean");
    }
    if (g_silence_unused && g_target == TARGET_VIRCON32) silence_unused_params(func);
    line_marker(out, func->line, g_debug_cur_file);
    emit_function_header(out, func, name);
    fprintf(out, "\n");
    print_stmt(out, func->a, 0, force_void_main); /* func->a is the body, an AST_BLOCK */
    fprintf(out, "\n");
}

/* ---- top-level walks for functions/methods ------------------------------
 *
 * Mirrors the class-vs-free-function split this project has used
 * consistently since lower.c's own phases (finalize_calls_classes/
 * finalize_calls_free_functions, fix_references_classes/..., etc.) --
 * same reasoning applies here: a class's methods live in its
 * ClassLayout, a free function is walked directly off the namespace/
 * program decl list, and `n->b == NULL` is still what distinguishes a
 * genuine free function from an out-of-line method definition's own
 * top-level duplicate (see attach_out_of_line).
 */
/* Prints a prototype for a METHOD specifically -- unlike
 * emit_function_prototype (used for anything already this-injected, or
 * a free function, neither of which needs special handling), a
 * prototype-only AST_FUNC_DECL method has never been through this-
 * injection at all (phase 2 only ever touches AST_FUNC_DEF), so its own
 * parameter list has no receiver in it. Reconstructed explicitly here,
 * using `class_decl`'s own name directly -- simpler than
 * find_declaring_class's ancestor-walking, since we're already iterating
 * this exact class's own methods list, not looking anything up through
 * an object expression.
 *
 * THIS CLOSES A REAL, CONFIRMED BUG: a prototype-only method OR free
 * function that's genuinely called somewhere (not just declared and
 * ignored) needs a prototype in the generated output regardless of
 * whether it also gets a body here -- Matthew's test build hit exactly
 * this for tests/sample14.cpp's `doubleIt` (declared, deliberately never
 * defined in that file -- Vircon32's lack of any multi-file compilation
 * model means it's expected to be satisfied by something else entirely
 * at the eventual all-in-one-file compile step, but the CALL inside this
 * file still needs a prototype to type-check against). Emitting NO
 * prototype at all for a declared-but-undefined function was simply
 * wrong, not merely a narrow edge case -- fixed for both methods (this
 * function) and free functions (see emit_function_prototypes_free_
 * functions below). */
static void emit_method_prototype(FILE *out, const AstNode *class_decl, const AstNode *method) {
    if (method->kind == AST_FUNC_DEF) {
        emit_function_prototype(out, method); /* already this-injected -- print as-is */
        return;
    }

    FuncSemaInfo *info = (FuncSemaInfo *)method->sema_info;
    const char *name = (info != NULL) ? info->mangled_name : method->str1;

    print_type(out, method->type);
    fprintf(out, " %s(", name);
    print_class_type_name(out, class_decl->str1);
    fprintf(out, " *this");
    for (int p = 0; p < method->list.count; p++) {
        fprintf(out, ", ");
        AstNode *param = method->list.items[p];
        print_type(out, param->type);
        fprintf(out, " %s", param->str1);
    }
    fprintf(out, ");\n");
}

static void emit_function_prototypes_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    emit_method_prototype(out, n, layout->methods.items[j]);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_function_prototypes_classes(out, &n->list);
        }
    }
}

/* Tracks which mangled free-function names have already had a prototype
 * printed, so an ordinary declare-then-define free function (register_
 * free_function in sema.c dedupes these at the REGISTRY level, for
 * overload-resolution purposes -- but this walk reads the raw AST decls
 * directly, which still has both the separate FUNC_DECL and FUNC_DEF
 * nodes) doesn't get an identical prototype line printed twice. Harmless
 * either way in C (a redundant, identical redeclaration is legal, and
 * this exact duplication compiled cleanly before this fix existed) --
 * this is purely about not cluttering generated output with a needless
 * duplicate line, for a project whose generated output is also meant to
 * be read as teaching material. */
typedef struct {
    const char **names;
    int count;
    int capacity;
} SeenNames;

static int seen_names_contains(const SeenNames *seen, const char *name) {
    for (int i = 0; i < seen->count; i++) {
        if (strcmp(seen->names[i], name) == 0) return 1;
    }
    return 0;
}

static void seen_names_add(SeenNames *seen, const char *name) {
    if (seen->count == seen->capacity) {
        seen->capacity = seen->capacity ? seen->capacity * 2 : 8;
        seen->names = realloc(seen->names, sizeof(char *) * (size_t)seen->capacity);
    }
    seen->names[seen->count++] = name;
}

static void emit_function_prototypes_free_functions(FILE *out, const AstList *decls, SeenNames *seen) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            emit_function_prototypes_free_functions(out, &n->list, seen);
        } else if ((n->kind == AST_FUNC_DEF && n->b == NULL) || n->kind == AST_FUNC_DECL) {
            /* Covers both a genuine free function's body (n->b == NULL
             * excludes an out-of-line method definition's own top-level
             * duplicate) and a prototype-only free function (e.g.
             * `int doubleIt(int x);` with no body anywhere in this file
             * -- no this-injection concern at all here, that only ever
             * applies to methods, so its own parameter list is already
             * exactly right). */
            FuncSemaInfo *info = (FuncSemaInfo *)n->sema_info;
            const char *name = (info != NULL) ? info->mangled_name : n->str1;
            if (!seen_names_contains(seen, name)) {
                emit_function_prototype(out, n);
                seen_names_add(seen, name);
            }
        }
    }
}

static void emit_function_definitions_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (m->kind == AST_FUNC_DEF) {
                        emit_function_definition(out, m);
                        fprintf(out, "\n");
                    }
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_function_definitions_classes(out, &n->list);
        }
    }
}

static void emit_function_definitions_free_functions(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            emit_function_definitions_free_functions(out, &n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            emit_function_definition(out, n);
            fprintf(out, "\n");
        }
    }
}

/* ---- new/delete runtime function definitions ----------------------------
 *
 * lower.c's phase 6 lowers `new T(args)`/`delete expr` into calls to
 * "v32_new_<something>"/"v32_delete" -- this is where those functions
 * actually get DEFINED, using Vircon32's real `malloc()`/`free()`
 * (`misc.h`, confirmed against the real Vircon32 C standard library
 * Matthew provided -- not invented or assumed). Closes the gap
 * tests/sprite.cpp found directly: `v32_new_Player` was an undefined
 * stub, "identifier ... has not been declared". See
 * docs/DESIGN_NOTES.md for the full story.
 *
 * NAMING, matching lower.c's new_delete_rewrite_expr exactly (the two
 * have to agree, or a call site would target a name nothing here
 * defines): when a specific constructor overload was resolved for a
 * `new T(args)` (sema.c's resolve_new_expr), the allocator is named
 * after THAT constructor's own mangled name and takes exactly its
 * parameter signature -- REGARDLESS of whether that constructor has a
 * body. A bodyless one (declared, never defined -- tests/sample17.cpp's
 * Widget, sample18.cpp's Point) still gets a correctly-parameterized
 * allocator that allocates and returns, simply never calling anything
 * (there's nothing to call); the arguments are accepted to match the
 * call site, then unused. Naming by the bare type name alone would be
 * WRONG the moment a class has more than one constructor (C has no
 * function overloading, so two overloads sharing one allocator name
 * couldn't both be right) or the moment a bodyless constructor takes
 * any arguments at all (Point's 2-argument case -- a bare-name fallback
 * only makes sense for a truly argument-free allocator). Only actually
 * falls back to the bare type name when NO constructor was resolved at
 * all -- the class genuinely has none, which is unambiguous since
 * there's nothing to disambiguate between.
 *
 * DELIBERATELY STILL MISSING: destructor invocation. `v32_delete` is a
 * single, generic function that just calls `free()` -- it does NOT call
 * a destructor first, because this project has no destructor-invocation
 * machinery at all yet (a separate, still-unstarted piece of work,
 * roughly the mirror image of phase 7's constructor invocation but for
 * teardown instead of construction). `delete obj;` on a class with a
 * real, meaningful destructor will currently just free the memory
 * without running it -- a real, known gap, not silently papered over.
 */

/* ---- allocation hook, arena runtime (--alloc=arena) --------------------------
 *
 * Every allocation the generated program makes goes through one pair of
 * calls, `__v32_alloc(words)` / `__v32_release(ptr)`. Without --alloc=arena
 * they are not emitted at all: the helpers print plain `malloc`/`free`, so
 * the output is what it always was. With it, the runtime below is emitted
 * (emit_arena_runtime): one block taken from the SDK's malloc at the start of
 * main(), bumped by __v32_alloc, never freed by __v32_release.
 *
 * An object whose class has a destructor with a body gets a 4-word RECORD
 * in front of it: [previous record's offset, class id, count, words from the
 * record's end to the first element]. Releasing back to a mark walks the
 * records newest first and runs the destructors (__v32_arena_dtor, a switch
 * on the class id -- no function pointers needed). `delete` runs the
 * destructor itself and sets the record's id to 0, so release skips it.
 *
 * Units are the SDK's sizeof unit: words on Vircon32, bytes in standard C,
 * where sizes are rounded to 16 so that every object stays aligned. */
static int arena_rec_units(void) { return g_target == TARGET_STANDARD ? 16 : 4; }
static int array_hdr_units(void) { return g_target == TARGET_STANDARD ? 16 : 1; }

static const AstNode *g_arena_classes[256];
static int g_arena_class_count = 0;

static const AstNode *class_dtor_with_body(const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return NULL;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (m->str1 == NULL || m->str1[0] != '~') continue;
        if (m->kind != AST_FUNC_DEF) continue;
        return m;
    }
    return NULL;
}

static void collect_arena_classes(const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            if (class_dtor_with_body(n) != NULL && g_arena_class_count < 256)
                g_arena_classes[g_arena_class_count++] = n;
        } else if (n->kind == AST_NAMESPACE_DECL) {
            collect_arena_classes(&n->list);
        }
    }
}

/* the id a class's destructor records carry (1..), 0: none needed */
static int arena_class_id(const AstNode *class_decl) {
    if (!g_alloc_arena) return 0;
    for (int i = 0; i < g_arena_class_count; i++)
        if (g_arena_classes[i] == class_decl) return i + 1;
    return 0;
}

static const char *dtor_mangled_name(const AstNode *dtor) {
    FuncSemaInfo *info = (FuncSemaInfo *)dtor->sema_info;
    return (info != NULL) ? info->mangled_name : dtor->str1;
}

/* `malloc(sizeof(T))`, or the arena's call for one object of the class */
static void emit_obj_alloc(FILE *out, const AstNode *cd) {
    if (!V32_ALLOC_HOOK) {
        fprintf(out, "malloc(sizeof(");
        print_class_type_name(out, cd->str1);
        fprintf(out, "))");
        return;
    }
    int id = arena_class_id(cd);
    fprintf(out, id ? "__v32_alloc_rec(sizeof(" : "__v32_alloc(sizeof(");
    print_class_type_name(out, cd->str1);
    fprintf(out, id ? "), %d, 1, 0)" : "))", id);
}

/* out of space: say so on the screen (stderr in standard C) and stop; shared by the arena and the gc */
static void emit_alloc_fatal(FILE *out, const char *fname, const char *std_text, const char *screen_text) {
    fprintf(out, "void %s(void)\n{\n", fname);
    if (g_target == TARGET_STANDARD) {
        fprintf(out, "    fputs(\"v32c++: out of memory (%s)\\n\", stderr);\n    exit(1);\n", std_text);
    } else {
        fprintf(out, "    clear_screen(color_black);\n    select_texture(-1);\n"
                     "    print_at(8, 8, \"%s\");\n"
                     "    print_at(8, 24, \"See --heap-words\");\n"
                     "    end_frame();\n    asm {\n        \"hlt\"\n    }\n", screen_text);
    }
    fprintf(out, "}\n\n\n");
}

static void emit_arena_runtime(FILE *out, const AstList *decls) {
    int rec = arena_rec_units();
    int std = (g_target == TARGET_STANDARD);
    fprintf(out, "/* ---- --alloc=arena runtime (see docs/AGENTS.md, \"Memory management\") ---- */\n");
    fprintf(out, "char *__v32_arena_base = NULL;\nint __v32_arena_size = 0;\nint __v32_arena_top = 0;\nint __v32_arena_rec = -1;\n\n\n");

    /* out of space: say so on the screen (stderr in standard C) and stop */
    emit_alloc_fatal(out, "__v32_arena_fatal", "--alloc=arena heap is full", "OUT OF MEMORY (arena full)");

    fprintf(out, "void __v32_arena_init(void)\n{\n"
                 "    __v32_arena_size = %d * sizeof(int);\n"
                 "    __v32_arena_base = (char *)malloc(__v32_arena_size);\n"
                 "    if (__v32_arena_base == NULL) __v32_arena_fatal();\n"
                 "    __v32_arena_top = 0;\n    __v32_arena_rec = -1;\n}\n\n\n", g_heap_words);

    fprintf(out, "void *__v32_alloc(int n)\n{\n    char *p;\n");
    if (std) fprintf(out, "    n = (n + 15) & ~15;\n");
    fprintf(out, "    if (n < 0 || n > __v32_arena_size - __v32_arena_top) __v32_arena_fatal();\n"
                 "    p = __v32_arena_base + __v32_arena_top;\n"
                 "    __v32_arena_top = __v32_arena_top + n;\n    return p;\n}\n\n\n");

    fprintf(out, "void __v32_release(void *ptr)\n{\n    /* an arena frees nothing one by one */\n    ptr = ptr;\n}\n\n\n");

    fprintf(out, "void *__v32_alloc_rec(int n, int id, int count, int skip)\n{\n"
                 "    int off = __v32_arena_top;\n    int *r = (int *)__v32_alloc(%d + n);\n"
                 "    r[0] = __v32_arena_rec;\n    r[1] = id;\n    r[2] = count;\n    r[3] = skip;\n"
                 "    __v32_arena_rec = off;\n    return (char *)r + %d;\n}\n\n\n", rec, rec);

    fprintf(out, "void __v32_arena_kill(void *payload)\n{\n"
                 "    int *r = (int *)((char *)payload - %d);\n    r[1] = 0;\n}\n\n\n", rec);

    fprintf(out, "void __v32_arena_dtor(int id, void *obj, int count)\n{\n    int i;\n    i = 0;\n");
    fprintf(out, "    obj = obj;\n    count = count;\n");
    for (int k = 0; k < g_arena_class_count; k++) {
        const AstNode *cd = g_arena_classes[k];
        fprintf(out, "    if (id == %d) {\n        for (i = count - 1; i >= 0; i--) %s(((", k + 1,
                dtor_mangled_name(class_dtor_with_body(cd)));
        print_class_type_name(out, cd->str1);
        fprintf(out, " *)obj) + i);\n        return;\n    }\n");
    }
    fprintf(out, "}\n\n\n");

    fprintf(out, "void __v32_arena_release(int mark)\n{\n"
                 "    if (mark < 0 || mark > __v32_arena_top) return;\n"
                 "    while (__v32_arena_rec >= mark) {\n"
                 "        int *r = (int *)(__v32_arena_base + __v32_arena_rec);\n"
                 "        __v32_arena_rec = r[0];\n"
                 "        if (r[1] != 0) __v32_arena_dtor(r[1], (char *)r + %d + r[3], r[2]);\n"
                 "    }\n    __v32_arena_top = mark;\n}\n\n\n", rec);
    fprintf(out, "int __v32_arena_mark(void)\n{\n    return __v32_arena_top;\n}\n\n\n");
    fprintf(out, "void __v32_arena_reset(void)\n{\n    __v32_arena_release(0);\n}\n\n\n");
    fprintf(out, "int __v32_arena_used(void)\n{\n    return __v32_arena_top;\n}\n\n\n");
    fprintf(out, "int __v32_arena_free(void)\n{\n    return __v32_arena_size - __v32_arena_top;\n}\n\n\n");
    (void)decls;
}


/* ---- --alloc=gc runtime -------------------------------------------------------
 *
 * The same hook as the arena (`__v32_alloc` / `__v32_release`, `__v32_gc_init()`
 * first in main by lower.c phase 9d), over a conservative mark-sweep collector.
 * The runtime is the template `gc_runtime_tpl` below (plain C, valid Vircon32 C
 * as it stands: no ternary, no unsigned, declarations first, pointer returns
 * assigned to a local before they are indexed), with a few numbers and the
 * target-dependent pieces substituted by emit_gc_runtime:
 *   - roots: a table of {address, size} of every file-scope variable that can
 *     hold a pointer (`__v32_gc_roots_init`, built here from the declarations),
 *     plus the machine stack from the stack pointer to its top;
 *   - Vircon32: the stack pointer is read with inline asm after R0-R10 were
 *     pushed (so values living only in registers are on the scanned stack), the
 *     top of the stack is 0x400000 (RAM is 0..0x3FFFFF, SP starts at 0x3FFFFF);
 *   - standard C: __builtin_unwind_init + setjmp spill the callee-saved
 *     registers, the scan starts at the frame of __v32_gc_run and ends at glibc's
 *     __libc_stack_end (or the caller-of-init frame elsewhere).
 * Units are the SDK's sizeof unit: words on Vircon32, bytes in standard C, where
 * blocks are 16-aligned and a "slot" (the unit a pointer is scanned in) is 8. */
static const char *const gc_runtime_tpl[] = {
    "/* ---- --alloc=gc runtime (see docs/AGENTS.md, \"Memory management\") ----\n",
    " * Conservative mark-sweep over one block taken from the SDK's malloc.\n",
    " * Blocks tile [0, top) of the region; each has a 3-int header\n",
    " * [size incl. header, state (0 free, 1 used, 3 used+marked), next free offset]. */\n",
    "void __v32_gc_roots_init(void);\n",
    "char *__v32_gc_base = NULL;\n",
    "int __v32_gc_size = 0;\n",
    "int __v32_gc_top = 0;\n",
    "int __v32_gc_inuse = 0;\n",
    "int __v32_gc_since = 0;\n",
    "int __v32_gc_thresh = 0;\n",
    "int __v32_gc_count = 0;\n",
    "int __v32_gc_ovf = 0;\n",
    "int __v32_gc_msp = 0;\n",
    "int __v32_gc_nroots = 0;\n",
    "@IT@ __v32_gc_lo = 0;\n",
    "@IT@ __v32_gc_stack_top = 0;\n",
    "int *__v32_gc_page = NULL;\n",
    "int *__v32_gc_ms = NULL;\n",
    "@FLISTDECL@;\n",
    "@ROOTADECL@;\n",
    "@ROOTNDECL@;\n",
    "\n",
    "\n",
    "int *__v32_gc_hdr(int off)\n",
    "{\n",
    "    return (int *)(__v32_gc_base + off);\n",
    "}\n",
    "\n",
    "\n",
    "/* a free block goes on the list of its size class (0: large, first fit) */\n",
    "void __v32_gc_push_free(int off)\n",
    "{\n",
    "    int *h;\n",
    "    int cls;\n",
    "    h = __v32_gc_hdr(off);\n",
    "    cls = h[0] - @HDR@;\n",
    "    if (cls <= @NCLS@ * @G@) cls = cls / @G@;\n",
    "    else cls = 0;\n",
    "    h[1] = 0;\n",
    "    h[2] = __v32_gc_flist[cls];\n",
    "    __v32_gc_flist[cls] = off;\n",
    "}\n",
    "\n",
    "\n",
    "/* the free block at off becomes a used block of `need` units; the rest is split off */\n",
    "int __v32_gc_carve(int off, int need)\n",
    "{\n",
    "    int *h;\n",
    "    int *r;\n",
    "    int rest;\n",
    "    h = __v32_gc_hdr(off);\n",
    "    rest = h[0] - need;\n",
    "    if (rest >= @MINBLK@) {\n",
    "        h[0] = need;\n",
    "        r = __v32_gc_hdr(off + need);\n",
    "        r[0] = rest;\n",
    "        __v32_gc_push_free(off + need);\n",
    "    }\n",
    "    h[1] = 1;\n",
    "    __v32_gc_inuse = __v32_gc_inuse + h[0];\n",
    "    return off;\n",
    "}\n",
    "\n",
    "\n",
    "/* offset of a used block of `need` units (header included), or -1 */\n",
    "int __v32_gc_find(int need)\n",
    "{\n",
    "    int cls;\n",
    "    int c;\n",
    "    int off;\n",
    "    int prev;\n",
    "    int *h;\n",
    "    int *p;\n",
    "    cls = need - @HDR@;\n",
    "    if (cls <= @NCLS@ * @G@) cls = cls / @G@;\n",
    "    else cls = 0;\n",
    "    if (cls > 0) {\n",
    "        off = __v32_gc_flist[cls];\n",
    "        if (off >= 0) {\n",
    "            h = __v32_gc_hdr(off);\n",
    "            __v32_gc_flist[cls] = h[2];\n",
    "            return __v32_gc_carve(off, need);\n",
    "        }\n",
    "    }\n",
    "    if (need <= __v32_gc_size - __v32_gc_top) {\n",
    "        off = __v32_gc_top;\n",
    "        h = __v32_gc_hdr(off);\n",
    "        h[0] = need;\n",
    "        h[1] = 1;\n",
    "        h[2] = -1;\n",
    "        __v32_gc_top = __v32_gc_top + need;\n",
    "        __v32_gc_inuse = __v32_gc_inuse + need;\n",
    "        return off;\n",
    "    }\n",
    "    for (c = cls + 1; cls > 0 && c <= @NCLS@; c++) {\n",
    "        off = __v32_gc_flist[c];\n",
    "        if (off >= 0) {\n",
    "            h = __v32_gc_hdr(off);\n",
    "            __v32_gc_flist[c] = h[2];\n",
    "            return __v32_gc_carve(off, need);\n",
    "        }\n",
    "    }\n",
    "    prev = -1;\n",
    "    off = __v32_gc_flist[0];\n",
    "    while (off >= 0) {\n",
    "        h = __v32_gc_hdr(off);\n",
    "        if (h[0] >= need) {\n",
    "            if (prev < 0) {\n",
    "                __v32_gc_flist[0] = h[2];\n",
    "            } else {\n",
    "                p = __v32_gc_hdr(prev);\n",
    "                p[2] = h[2];\n",
    "            }\n",
    "            return __v32_gc_carve(off, need);\n",
    "        }\n",
    "        prev = off;\n",
    "        off = h[2];\n",
    "    }\n",
    "    return -1;\n",
    "}\n",
    "\n",
    "\n",
    "/* v: a word that might be a pointer into the heap (anywhere inside a used block) */\n",
    "void __v32_gc_mark(@IT@ v)\n",
    "{\n",
    "    int off;\n",
    "    int b;\n",
    "    int *h;\n",
    "    if (v < __v32_gc_lo) return;\n",
    "    v = v - __v32_gc_lo;\n",
    "    if (v >= __v32_gc_top) return;\n",
    "    off = (int)v;\n",
    "    b = __v32_gc_page[off / @PAGE@];\n",
    "    h = __v32_gc_hdr(b);\n",
    "    while (b + h[0] <= off) {\n",
    "        b = b + h[0];\n",
    "        h = __v32_gc_hdr(b);\n",
    "    }\n",
    "    if (h[1] != 1) return;\n",
    "    h[1] = 3;\n",
    "    if (__v32_gc_msp < @MSTACK@) {\n",
    "        __v32_gc_ms[__v32_gc_msp] = b;\n",
    "        __v32_gc_msp = __v32_gc_msp + 1;\n",
    "    } else {\n",
    "        __v32_gc_ovf = 1;\n",
    "    }\n",
    "}\n",
    "\n",
    "\n",
    "/* every slot of [lo, hi) is a candidate pointer */\n",
    "void __v32_gc_scan(@IT@ lo, @IT@ hi)\n",
    "{\n",
    "    @IT@ a;\n",
    "    a = (lo + @SLOT@ - 1) / @SLOT@ * @SLOT@;\n",
    "    while (a + @SLOT@ <= hi) {\n",
    "        __v32_gc_mark(*(@WORD@ *)a);\n",
    "        a = a + @SLOT@;\n",
    "    }\n",
    "}\n",
    "\n",
    "\n",
    "void __v32_gc_drain(void)\n",
    "{\n",
    "    int b;\n",
    "    int *h;\n",
    "    @IT@ a;\n",
    "    while (__v32_gc_msp > 0) {\n",
    "        __v32_gc_msp = __v32_gc_msp - 1;\n",
    "        b = __v32_gc_ms[__v32_gc_msp];\n",
    "        h = __v32_gc_hdr(b);\n",
    "        a = __v32_gc_lo + b + @HDR@;\n",
    "        __v32_gc_scan(a, a + (h[0] - @HDR@));\n",
    "    }\n",
    "}\n",
    "\n",
    "\n",
    "/* the mark stack overflowed: rescan every marked block until nothing is left behind */\n",
    "void __v32_gc_rescan(void)\n",
    "{\n",
    "    int off;\n",
    "    int *h;\n",
    "    @IT@ a;\n",
    "    while (__v32_gc_ovf) {\n",
    "        __v32_gc_ovf = 0;\n",
    "        off = 0;\n",
    "        while (off < __v32_gc_top) {\n",
    "            h = __v32_gc_hdr(off);\n",
    "            if (h[1] == 3) {\n",
    "                a = __v32_gc_lo + off + @HDR@;\n",
    "                __v32_gc_scan(a, a + (h[0] - @HDR@));\n",
    "            }\n",
    "            off = off + h[0];\n",
    "        }\n",
    "        __v32_gc_drain();\n",
    "    }\n",
    "}\n",
    "\n",
    "\n",
    "/* page table: the block that holds the first unit of each page */\n",
    "void __v32_gc_index(void)\n",
    "{\n",
    "    int off;\n",
    "    int end;\n",
    "    int pg;\n",
    "    int *h;\n",
    "    off = 0;\n",
    "    pg = 0;\n",
    "    while (off < __v32_gc_top) {\n",
    "        h = __v32_gc_hdr(off);\n",
    "        end = off + h[0];\n",
    "        while (pg * @PAGE@ < end) {\n",
    "            __v32_gc_page[pg] = off;\n",
    "            pg = pg + 1;\n",
    "        }\n",
    "        off = end;\n",
    "    }\n",
    "}\n",
    "\n",
    "\n",
    "/* a run of free blocks [run, run + size) is joined; at the end of the heap it goes back to the wilderness */\n",
    "void __v32_gc_close_run(int run, int size)\n",
    "{\n",
    "    int *h;\n",
    "    if (run + size >= __v32_gc_top) {\n",
    "        __v32_gc_top = run;\n",
    "        return;\n",
    "    }\n",
    "    h = __v32_gc_hdr(run);\n",
    "    h[0] = size;\n",
    "    __v32_gc_push_free(run);\n",
    "}\n",
    "\n",
    "\n",
    "void __v32_gc_sweep(void)\n",
    "{\n",
    "    int off;\n",
    "    int sz;\n",
    "    int run;\n",
    "    int runsz;\n",
    "    int i;\n",
    "    int *h;\n",
    "    for (i = 0; i <= @NCLS@; i++) __v32_gc_flist[i] = -1;\n",
    "    __v32_gc_inuse = 0;\n",
    "    run = -1;\n",
    "    runsz = 0;\n",
    "    off = 0;\n",
    "    while (off < __v32_gc_top) {\n",
    "        h = __v32_gc_hdr(off);\n",
    "        sz = h[0];\n",
    "        if (h[1] == 3) {\n",
    "            h[1] = 1;\n",
    "            __v32_gc_inuse = __v32_gc_inuse + sz;\n",
    "            if (run >= 0) {\n",
    "                __v32_gc_close_run(run, runsz);\n",
    "                run = -1;\n",
    "                runsz = 0;\n",
    "            }\n",
    "        } else {\n",
    "            if (run < 0) run = off;\n",
    "            runsz = runsz + sz;\n",
    "        }\n",
    "        off = off + sz;\n",
    "    }\n",
    "    if (run >= 0) __v32_gc_close_run(run, runsz);\n",
    "}\n",
    "\n",
    "\n",
    "@RUNDECL@void __v32_gc_run(@IT@ sp)\n",
    "{\n",
    "    int i;\n",
    "@SPDECL@    if (__v32_gc_base == NULL) return;\n",
    "@SPSET@    __v32_gc_index();\n",
    "    __v32_gc_msp = 0;\n",
    "    __v32_gc_ovf = 0;\n",
    "    for (i = 0; i < __v32_gc_nroots; i++)\n",
    "        __v32_gc_scan(__v32_gc_root_a[i], __v32_gc_root_a[i] + __v32_gc_root_n[i]);\n",
    "    __v32_gc_scan(sp, __v32_gc_stack_top);\n",
    "    __v32_gc_drain();\n",
    "    __v32_gc_rescan();\n",
    "    __v32_gc_sweep();\n",
    "    __v32_gc_since = 0;\n",
    "    __v32_gc_count = __v32_gc_count + 1;\n",
    "}\n",
    "\n",
    "\n",
    "@COLLECT@\n",
    "\n",
    "void *__v32_alloc(int n)\n",
    "{\n",
    "    int need;\n",
    "    int off;\n",
    "    char *p;\n",
    "    if (n < 1) n = 1;\n",
    "    if (n > __v32_gc_size) __v32_gc_fatal();\n",
    "    n = n + 1;   /* one trailing unit (as in Boehm): a pointer one past the end of the object, as an\n",
    "                  * iterator or `p + n` makes, still lies inside its own block, not in the next one */\n",
    "@ROUNDN@    need = n + @HDR@;\n",
    "    if (need > __v32_gc_size) __v32_gc_fatal();\n",
    "    if (__v32_gc_since >= __v32_gc_thresh) __v32_gc_collect();\n",
    "    off = __v32_gc_find(need);\n",
    "    if (off < 0) {\n",
    "        __v32_gc_collect();\n",
    "        off = __v32_gc_find(need);\n",
    "        if (off < 0) __v32_gc_fatal();\n",
    "    }\n",
    "    __v32_gc_since = __v32_gc_since + need;\n",
    "    p = __v32_gc_base + off + @HDR@;\n",
    "    memset((void *)p, 0, n);\n",
    "    return p;\n",
    "}\n",
    "\n",
    "\n",
    "/* delete: the block is free at once (a pointer that is not a live block's start is ignored) */\n",
    "void __v32_release(void *ptr)\n",
    "{\n",
    "    int off;\n",
    "    int *h;\n",
    "    if (ptr == NULL) return;\n",
    "    if ((@IT@)ptr < __v32_gc_lo + @HDR@) return;\n",
    "    off = (int)((@IT@)ptr - __v32_gc_lo) - @HDR@;\n",
    "    if (off < 0 || off >= __v32_gc_top) return;\n",
    "    h = __v32_gc_hdr(off);\n",
    "    if (h[1] != 1) return;\n",
    "    __v32_gc_inuse = __v32_gc_inuse - h[0];\n",
    "    __v32_gc_push_free(off);\n",
    "}\n",
    "\n",
    "\n",
    "void __v32_gc_init(void)\n",
    "{\n",
    "    int i;\n",
    "    int pages;\n",
    "    __v32_gc_size = @SIZEEXPR@;\n",
    "    pages = __v32_gc_size / @PAGE@ + 2;\n",
    "    __v32_gc_base = (char *)malloc(@HDR@ + __v32_gc_size + (pages + @MSTACK@) * sizeof(int));\n",
    "    if (__v32_gc_base == NULL) __v32_gc_fatal();\n",
    "    __v32_gc_base = __v32_gc_base + @HDR@;   /* a stale copy of malloc's pointer is then not inside any block */\n",
    "    __v32_gc_lo = (@IT@)__v32_gc_base;\n",
    "    __v32_gc_page = (int *)(__v32_gc_base + __v32_gc_size);\n",
    "    __v32_gc_ms = (int *)(__v32_gc_base + __v32_gc_size + pages * sizeof(int));\n",
    "    __v32_gc_top = 0;\n",
    "    __v32_gc_inuse = 0;\n",
    "    __v32_gc_since = 0;\n",
    "    __v32_gc_count = 0;\n",
    "    __v32_gc_msp = 0;\n",
    "    __v32_gc_ovf = 0;\n",
    "    __v32_gc_thresh = @THRESHEXPR@;\n",
    "    for (i = 0; i <= @NCLS@; i++) __v32_gc_flist[i] = -1;\n",
    "    __v32_gc_roots_init();\n",
    "@STACKTOP@}\n",
    "\n",
    "\n",
    "int __v32_gc_get_used(void)\n",
    "{\n",
    "    return __v32_gc_inuse;\n",
    "}\n",
    "\n",
    "\n",
    "int __v32_gc_get_free(void)\n",
    "{\n",
    "    return __v32_gc_size - __v32_gc_inuse;\n",
    "}\n",
    "\n",
    "\n",
    "int __v32_gc_get_collections(void)\n",
    "{\n",
    "    return __v32_gc_count;\n",
    "}\n",
    NULL
};

/* could a file-scope variable of this type hold a heap pointer? Pointers, class
 * objects (scanned whole, conservatively) and arrays of them; not plain numbers,
 * enums or function pointers (code addresses). */
static int gc_type_may_hold_pointer(const AstNode *t, int depth) {
    while (t != NULL && (t->kind == AST_CONST_TYPE || t->kind == AST_ARRAY_TYPE)) t = t->a;
    if (t == NULL || depth > 8) return 1;
    switch (t->kind) {
        case AST_FUNC_PTR_TYPE: return 0;
        case AST_IDENT: {
            static const char *const plain[] = { "int", "float", "char", "bool", "void", "double",
                "short", "long", "unsigned", "signed", "size_t", NULL };
            for (int i = 0; plain[i] != NULL; i++)
                if (t->str1 != NULL && strcmp(t->str1, plain[i]) == 0) return 0;
            if (t->str1 != NULL) {
                AstNode *tag = find_enum_or_union_decl(&g_program->list, t->str1);
                if (tag != NULL && tag->kind == AST_ENUM_DECL) return 0;
            }
            const AstNode *under = resolve_typedef_chain(t);
            if (under != NULL && under != t) return gc_type_may_hold_pointer(under, depth + 1);
            return 1;
        }
        default: return 1;
    }
}

static int gc_root_wanted(const AstNode *n) {
    if (n->kind != AST_VAR_DECL || n->str1 == NULL || n->is_extern) return 0;
    if (n->attr_flags & ATTR_ROM) return 0;
    const AstNode *t = n->type;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t != NULL && t->kind == AST_ARRAY_TYPE && t->ival <= 0 && t->b == NULL && n->a == NULL) return 0; /* unsized, no storage here */
    return gc_type_may_hold_pointer(n->type, 0);
}

static int gc_count_roots(const AstList *decls) {
    int c = 0;
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_VAR_DECL) c += gc_root_wanted(n);
        else if (n->kind == AST_NAMESPACE_DECL) c += gc_count_roots(&n->list);
    }
    return c;
}

/* one assignment pair per root: the table is filled at run time, not as a file-scope
 * initializer, so no address constant is needed in a Vircon32 data declaration */
static int gc_emit_root_entries(FILE *out, const AstList *decls, int k, const char *it) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_VAR_DECL && gc_root_wanted(n)) {
            const AstNode *t = n->type;
            while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
            fprintf(out, "    __v32_gc_root_a[%d] = (%s)&%s", k, it, n->str1);
            while (t != NULL && t->kind == AST_ARRAY_TYPE) {   /* the first element: arrays are not taken by & */
                fprintf(out, "[0]");
                t = t->a;
                while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
            }
            fprintf(out, ";\n    __v32_gc_root_n[%d] = sizeof(%s);\n", k, n->str1);
            k++;
        } else if (n->kind == AST_NAMESPACE_DECL) {
            k = gc_emit_root_entries(out, &n->list, k, it);
        }
    }
    return k;
}

/* the template, with @NAME@ replaced */
static void gc_emit_template(FILE *out, const char *const *lines, const char *const *names, const char *const *values) {
    for (int i = 0; lines[i] != NULL; i++) {
        for (const char *p = lines[i]; *p != '\0'; ) {
            int hit = 0;
            if (*p == '@') {
                for (int k = 0; names[k] != NULL; k++) {
                    size_t len = strlen(names[k]);
                    if (strncmp(p + 1, names[k], len) == 0 && p[1 + len] == '@') {
                        fputs(values[k], out);
                        p += len + 2;
                        hit = 1;
                        break;
                    }
                }
            }
            if (!hit) { fputc(*p, out); p++; }
        }
    }
}

static void emit_gc_runtime(FILE *out, const AstList *decls) {
    int std = (g_target == TARGET_STANDARD);
    int nroots = gc_count_roots(decls);
    char nr[32], size_expr[96], thresh_expr[96], nr1[32];
    snprintf(nr, sizeof nr, "%d", nroots > 0 ? nroots : 1);
    snprintf(nr1, sizeof nr1, "%d", 32 + 1);
    if (std) snprintf(size_expr, sizeof size_expr, "(%d * sizeof(int)) / 16 * 16", g_heap_words);
    else snprintf(size_expr, sizeof size_expr, "%d * sizeof(int)", g_heap_words);
    if (g_gc_threshold > 0) snprintf(thresh_expr, sizeof thresh_expr, "%d * sizeof(int)", g_gc_threshold);
    else snprintf(thresh_expr, sizeof thresh_expr, "__v32_gc_size / 4");

    const char *collect =
        std ?
        "#if defined(__GNUC__)\n__attribute__((noinline))\n#endif\n"
        "void __v32_gc_collect(void)\n{\n"
        "    jmp_buf jb;\n"
        "    /* callee-saved registers: into this frame, which __v32_gc_run scans */\n"
        "#if defined(__GNUC__)\n    __builtin_unwind_init();\n#endif\n"
        "    setjmp(jb);\n"
        "    __v32_gc_run(0);\n}\n"
        :
        "void __v32_gc_collect(void)\n{\n"
        "    int stk;\n"
        "    stk = 0;\n"
        "    /* Vircon32: PUSH decrements SP, then stores. Pushing R0-R10 puts every value that lives\n"
        "     * only in a register on the stack, below the caller's frames; SP then points at the\n"
        "     * lowest of them. They are not popped: this function's epilogue resets SP from BP, and\n"
        "     * __v32_gc_run must run with them still in place. */\n"
        "    asm {\n"
        "        \"push R0\"\n        \"push R1\"\n        \"push R2\"\n        \"push R3\"\n"
        "        \"push R4\"\n        \"push R5\"\n        \"push R6\"\n        \"push R7\"\n"
        "        \"push R8\"\n        \"push R9\"\n        \"push R10\"\n"
        "        \"mov {stk}, SP\"\n"
        "    }\n"
        "    __v32_gc_run(stk);\n}\n";
    const char *stacktop =
        std ?
        "#if defined(__GLIBC__)\n    __v32_gc_stack_top = (intptr_t)__libc_stack_end;\n"
        "#elif defined(__GNUC__)\n    __v32_gc_stack_top = (intptr_t)__builtin_frame_address(1);\n"
        "#else\n    { char m; __v32_gc_stack_top = (intptr_t)&m; }\n#endif\n"
        :
        "    /* RAM is 0x000000-0x3FFFFF and the stack starts at its last word (v32_restart sets\n"
        "     * SP to 0x003FFFFF and it grows down): scan [SP, 0x400000) */\n"
        "    __v32_gc_stack_top = 4194304;\n";
    const char *names[] = { "IT", "WORD", "SLOT", "HDR", "G", "NCLS", "NCLS1", "PAGE", "MSTACK", "MINBLK", "NR",
        "SIZEEXPR", "THRESHEXPR", "RUNDECL", "SPDECL", "SPSET", "ROUNDN", "COLLECT", "STACKTOP",
        "FLISTDECL", "ROOTADECL", "ROOTNDECL", NULL };
    /* Vircon32 C spells an array `int [n] name`, standard C `int name[n]` */
    char flist_decl[96], roota_decl[96], rootn_decl[96];
    snprintf(flist_decl, sizeof flist_decl, std ? "int __v32_gc_flist[%s]" : "int [%s] __v32_gc_flist", nr1);
    snprintf(roota_decl, sizeof roota_decl, std ? "intptr_t __v32_gc_root_a[%s]" : "int [%s] __v32_gc_root_a", nr);
    snprintf(rootn_decl, sizeof rootn_decl, std ? "int __v32_gc_root_n[%s]" : "int [%s] __v32_gc_root_n", nr);
    const char *values[] = {
        std ? "intptr_t" : "int",
        std ? "__v32_gc_word" : "int",
        std ? "(int)sizeof(intptr_t)" : "1",
        std ? "16" : "3",
        std ? "16" : "1",
        "32", nr1, "256", "512",
        std ? "32" : "4",
        nr, size_expr, thresh_expr,
        std ? "#if defined(__GNUC__)\n__attribute__((noinline))\n#endif\n" : "",
        std ? "    char marker;\n" : "",
        std ? "#if defined(__GNUC__)\n    sp = (intptr_t)__builtin_frame_address(0);\n#else\n    sp = (intptr_t)&marker;\n#endif\n" : "",
        std ? "    n = (n + 15) / 16 * 16;\n" : "",
        collect, stacktop, flist_decl, roota_decl, rootn_decl };

    fprintf(out, "/* ---- --alloc=gc runtime: see docs/AGENTS.md, \"Memory management\" ---- */\n");
    if (std) {
        fprintf(out, "#if defined(__GNUC__)\ntypedef intptr_t __attribute__((may_alias)) __v32_gc_word;\n#else\n"
                     "typedef intptr_t __v32_gc_word;\n#endif\n"
                     "#if defined(__GLIBC__)\nextern void *__libc_stack_end;\n#endif\n");
    }
    emit_alloc_fatal(out, "__v32_gc_fatal", "--alloc=gc heap is full", "OUT OF MEMORY (gc heap full)");
    gc_emit_template(out, gc_runtime_tpl, names, values);
    fprintf(out, "\n\n/* the roots: every file-scope variable that can hold a pointer */\n"
                 "void __v32_gc_roots_init(void)\n{\n");
    gc_emit_root_entries(out, decls, 0, std ? "intptr_t" : "int");
    fprintf(out, "    __v32_gc_nroots = %d;\n}\n\n\n", nroots);
}

/* the zero-argument constructor (defaults allowed) of a class, or NULL */
static const AstNode *class_zero_ctor(const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return NULL;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (strcmp(m->str1, class_decl->str1) != 0) continue;
        if (m->kind != AST_FUNC_DEF) continue;
        int all_defaulted = 1;
        for (int p = 1; p < m->list.count; p++)
            if (m->list.items[p]->a == NULL) { all_defaulted = 0; break; }
        if (all_defaulted) return m;
    }
    return NULL;
}

static void emit_new_delete_runtime(FILE *out, const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return;

    int found_ctor = 0;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (strcmp(m->str1, class_decl->str1) != 0) continue; /* not a constructor at all */
        found_ctor = 1;

        FuncSemaInfo *ctor_info = (FuncSemaInfo *)m->sema_info;
        const char *ctor_mangled = (ctor_info != NULL) ? ctor_info->mangled_name : m->str1;
        int has_body = (m->kind == AST_FUNC_DEF);
        /* This-injection only ever touches a constructor WITH a body
         * (phase 2 only ever processes AST_FUNC_DEF) -- a prototype-only
         * one's own parameter list has no injected "this" in it at all,
         * so where the EXPLICIT parameters start differs depending on
         * which case this is. Same care emit_vtable_struct/other phases
         * already need for the identical reason. */
        int start = has_body ? 1 : 0;

        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *v32_new_%s(", ctor_mangled);
        if (start == m->list.count) {
            fprintf(out, "void");
        }
        for (int p = start; p < m->list.count; p++) {
            if (p > start) fprintf(out, ", ");
            AstNode *param = m->list.items[p];
            print_type(out, param->type);
            fprintf(out, " %s", param->str1);
        }
        fprintf(out, ")\n{\n");
        explain(out, 1, "C++'s 'new' has no C equivalent -- this function does what 'new' does under the hood: allocate raw memory, then call the constructor on it explicitly");
        fprintf(out, "    ");
        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *self = (");
        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *)");
        emit_obj_alloc(out, class_decl);
        fprintf(out, ";\n");
        if (has_body) {
            fprintf(out, "    %s(self", ctor_mangled);
            for (int p = start; p < m->list.count; p++) {
                fprintf(out, ", %s", m->list.items[p]->str1);
            }
            fprintf(out, ");\n");
        }
        /* else: no body to call at all (a declared-but-never-defined
         * constructor -- tests/sample17.cpp's Widget, sample18.cpp's
         * Point) -- the parameters above are accepted, to match exactly
         * what the call site forwards, but simply go unused here: there
         * is nothing to construct with them. A real destination for
         * that gap once this project's constructor-body-required
         * checking (if it ever gets one) exists; not silently pretended
         * to be handled here. */
        fprintf(out, "    return self;\n");
        fprintf(out, "}\n\n\n");
    }

    if (!found_ctor) {
        /* A class with NO user-declared constructor at all has no
         * constructor BODY for phase 8 (inject_vtable_init_classes,
         * lower.c) to inject its `this->vtable = &ClassName_
         * vtable_instance;` assignment into -- that phase only ever
         * walks a class's own declared constructors, and finds none
         * here, so a class with a vtable but no constructor previously
         * got NO vtable-pointer initialization ANYWHERE: `new`ing it
         * left `self->vtable` as raw, uninitialized malloc'd memory,
         * and the very first virtual call through it dispatched through
         * garbage. A real, serious, previously-undiscovered bug --
         * confirmed directly (not guessed at) with an isolated repro
         * (`class Shape { virtual int area(){...} }; class Square :
         * public Shape { virtual int area(){...} }; new Square()`,
         * calling `->area()` through it) that segfaulted every time
         * before this fix, and compiled+ran clean after it. Fixed the
         * same way inject_vtable_init_classes' own doc comment
         * describes for a class that DOES have a constructor -- set the
         * vtable pointer to the class's own `ClassName_vtable_instance`
         * (emit_vtable_instance's own naming, which this has to agree
         * with, same cross-module constraint as everywhere else in this
         * pair of functions) -- just emitted directly as C text here,
         * since there's no AST constructor body available to inject an
         * AST_ASSIGN into for this case; codegen already builds this
         * exact allocator function as raw text regardless, so adding
         * one more line to it is the natural, minimal fix rather than
         * inventing a synthetic AST-level default constructor. */
        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *v32_new_%s(void)\n{\n    ", class_decl->str1);
        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *self = (");
        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *)");
        emit_obj_alloc(out, class_decl);
        fprintf(out, ";\n");
        if (layout->vtable != NULL) {
            fprintf(out, "    self->vtable = &%s_vtable_instance;\n", class_decl->str1);
        } else if (g_target == TARGET_VIRCON32 && lower_class_has_union(class_decl)) {
            /* a union in it takes no brace list: `P zero;` and one assignment per member */
            AstList st = ast_list_new();
            AstNode *lv = ast_ident("zero", class_decl->line);
            if (lower_zero_stmts(class_decl, lv, &st)) {
                fprintf(out, "    ");
                print_class_type_name(out, class_decl->str1);
                fprintf(out, " zero;\n");
                for (int k = 0; k < st.count; k++) print_stmt(out, st.items[k], 1, 0);
                fprintf(out, "    *self = zero;\n");
            }
        } else if (class_decl->list.count > 0) {
            /* `new P()` / `new P{}` value-initializes a plain aggregate: zero every member (a pointer
             * member to NULL, which is -1 here). A complete list, since a short one zeroes nothing in a local. */
            AstNode *zero = lower_zero_initializer(class_decl);
            if (zero != NULL && zero->list.count > 0) {
                fprintf(out, "    ");
                print_class_type_name(out, class_decl->str1);
                fprintf(out, " zero = ");
                print_expr(out, zero);
                fprintf(out, ";\n    *self = zero;\n");
            }
        }
        fprintf(out, "    return self;\n}\n\n\n");
    }
}

static void emit_new_delete_runtime_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            emit_new_delete_runtime(out, n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_new_delete_runtime_classes(out, &n->list);
        }
    }
}

/* `v32_new_arr_ClassName` -- lower.c's `new_delete_rewrite_expr` names
 * `new T[N]` this way (distinct from the "v32_new_ClassName..." family
 * used for single-object `new`, and never ambiguous the way multiple
 * constructor overloads can be, since there's exactly one shape of
 * array-new per class). Allocation only -- `malloc(N * sizeof(ClassName))`,
 * cast, return -- deliberately no per-element construction; see ast.h's
 * own doc comment on AST_NEW for why. Emitted unconditionally for every
 * class, same trade-off already made for `v32_new_ClassName`/
 * `v32_delete_ClassName` (an allocator for a class never actually used
 * with array-`new` goes unused rather than being scoped out by an
 * "only if actually used" scan this project hasn't built). */
static void emit_array_new_runtime(FILE *out, const char *type_name) {
    /* a primitive element type: no constructor, no destructor, no header */
    print_class_type_name(out, type_name);
    fprintf(out, " *v32_new_arr_%s(int n)\n{\n    return (", type_name);
    print_class_type_name(out, type_name);
    fprintf(out, " *)%s(n * sizeof(", V32_ALLOC_HOOK ? "__v32_alloc" : "malloc");
    print_class_type_name(out, type_name);
    fprintf(out, "));\n}\n\n\n");
}

/* `_rest` of a class with no constructor: zero elements k..n-1. A pointer member must read NULL (-1 on
 * Vircon32), which a byte-wise zero cannot give, so such a class is filled from its complete zero initializer. */
static void emit_rest_zero_body(FILE *out, const AstNode *cd)
{
    AstNode *zero = (g_target == TARGET_VIRCON32 && lower_class_holds_pointer(cd)) ? lower_zero_initializer(cd) : NULL;
    if (g_target == TARGET_VIRCON32 && lower_class_has_union(cd)) {
        AstList st = ast_list_new();
        AstNode *lv = ast_ident("zero", cd->line);
        if (lower_zero_stmts(cd, lv, &st)) {
            fprintf(out, " *p, int n, int k)\n{\n    int i;\n    ");
            print_class_type_name(out, cd->str1);
            fprintf(out, " zero;\n");
            for (int k = 0; k < st.count; k++) print_stmt(out, st.items[k], 1, 0);
            fprintf(out, "    if (p == NULL) return;\n    if (k < 0) k = 0;\n    if (n < k) n = k;\n"
                         "    for (i = k; i < n; i++) p[i] = zero;\n}\n\n\n");
            return;
        }
    }
    if (zero != NULL && zero->list.count > 0) {
        fprintf(out, " *p, int n, int k)\n{\n    int i;\n    ");
        print_class_type_name(out, cd->str1);
        fprintf(out, " zero = ");
        print_expr(out, zero);
        fprintf(out, ";\n    if (p == NULL) return;\n    if (k < 0) k = 0;\n    if (n < k) n = k;\n"
                     "    for (i = k; i < n; i++) p[i] = zero;\n}\n\n\n");
        return;
    }
    fprintf(out, " *p, int n, int k)\n{\n    char *z;\n    int i;\n    if (p == NULL) return;\n"
                 "    if (k < 0) k = 0;\n    if (n < k) n = k;\n    z = (char *)p;\n    for (i = k * sizeof(");
    print_class_type_name(out, cd->str1);
    fprintf(out, "); i < n * sizeof(");
    print_class_type_name(out, cd->str1);
    fprintf(out, "); i++) z[i] = 0;\n}\n\n\n");
}

/* A list-initialized `new T[n]{...}` of a primitive (or of a class with no constructor, no
 * destructor and no vtable) is lowered to
 *     v32_new_arr_T_init(n, k)      the allocation, nothing built (n < k allocates k)
 *     ... the caller builds elements 0..k-1 from the list ...
 *     v32_new_arr_T_rest(p, n, k)   zeroes the elements k..n-1
 * so the elements come into being in order, as in C++. */
static void emit_array_new_runtime_init(FILE *out, const char *type_name, const AstNode *cd)
{
    print_class_type_name(out, type_name);
    fprintf(out, " *v32_new_arr_%s_init(int n, int k)\n{\n    if (k < 0) k = 0;\n    if (n < k) n = k;\n"
                 "    return (", type_name);
    print_class_type_name(out, type_name);
    fprintf(out, " *)%s(n * sizeof(", V32_ALLOC_HOOK ? "__v32_alloc" : "malloc");
    print_class_type_name(out, type_name);
    fprintf(out, "));\n}\n\n\n");
    fprintf(out, "void v32_new_arr_%s_rest(", type_name);
    print_class_type_name(out, type_name);
    if (cd != NULL) { emit_rest_zero_body(out, cd); return; }
    fprintf(out, " *p, int n, int k)\n{\n    char *z;\n    int i;\n    if (p == NULL) return;\n"
                 "    if (k < 0) k = 0;\n    if (n < k) n = k;\n    z = (char *)p;\n    for (i = k * sizeof(");
    print_class_type_name(out, type_name);
    fprintf(out, "); i < n * sizeof(");
    print_class_type_name(out, type_name);
    fprintf(out, "); i++) z[i] = 0;\n}\n\n\n");
}

/* `new T[n]` of a class: every element is default-constructed, in order.
 * A class with a destructor gets one header word in front of the elements
 * holding n (the pointer handed out is past it), so that `delete[]` can
 * destroy them last first; a class without one has no header and
 * `delete[]` is a plain release. n < 0 counts as 0. */
static void emit_array_new_runtime_class(FILE *out, const AstNode *cd, int mode)
{
    /* mode 0: `v32_new_arr_T(n)`. mode 1/2: the `_init(n, k)` / `_rest(p, n, k)` pair behind
     * `new T[n]{...}` (see emit_array_new_runtime_init): _init allocates (the count header
     * included) and builds nothing, _rest default-constructs the elements k..n-1. */
    const AstNode *dtor = class_dtor_with_body(cd);
    const AstNode *ctor = class_zero_ctor(cd);
    ClassLayout *layout = (ClassLayout *)cd->sema_info;
    int vt = (layout != NULL && layout->vtable != NULL);
    int hdr = array_hdr_units();
    if (dtor == NULL && ctor == NULL && !vt) {
        if (mode == 0) emit_array_new_runtime(out, cd->str1);
        else if (mode == 1) emit_array_new_runtime_init(out, cd->str1, cd);   /* defines _rest too */
        return;
    }
    if (mode == 2) {
        if (ctor == NULL && !vt) {          /* a destructor only: the rest is zero */
            fprintf(out, "void v32_new_arr_%s_rest(", cd->str1);
            print_class_type_name(out, cd->str1);
            emit_rest_zero_body(out, cd);
            return;
        }
        fprintf(out, "void v32_new_arr_%s_rest(", cd->str1);
        print_class_type_name(out, cd->str1);
        fprintf(out, " *p, int n, int k)\n{\n    int i;\n    if (p == NULL) return;\n"
                     "    if (k < 0) k = 0;\n    if (n < k) n = k;\n");
        if (ctor != NULL) {
            fprintf(out, "    for (i = k; i < n; i++) %s(&p[i]", dtor_mangled_name(ctor));
            for (int k = 1; k < ctor->list.count; k++) {
                fprintf(out, ", ");
                print_expr(out, ctor->list.items[k]->a);
            }
            fprintf(out, ");\n");
        } else {
            fprintf(out, "    for (i = k; i < n; i++) p[i].vtable = &%s_vtable_instance;\n", cd->str1);
        }
        fprintf(out, "}\n\n\n");
        return;
    }

    print_class_type_name(out, cd->str1);
    if (mode == 1) fprintf(out, " *v32_new_arr_%s_init(int n, int k)\n{\n    ", cd->str1);
    else fprintf(out, " *v32_new_arr_%s(int n)\n{\n    ", cd->str1);
    print_class_type_name(out, cd->str1);
    fprintf(out, " *p;\n");
    if (mode == 0) fprintf(out, "    int i;\n");
    if (dtor != NULL) fprintf(out, "    char *raw;\n    int *hdr;\n");
    fprintf(out, "    if (n < 0) n = 0;\n");
    if (mode == 1) fprintf(out, "    if (k < 0) k = 0;\n    if (n < k) n = k;\n");
    if (dtor != NULL) {
        if (g_alloc_arena)
            fprintf(out, "    raw = (char *)__v32_alloc_rec(n * sizeof(");
        else if (g_alloc_gc)   /* collected objects get no destructor: no record */
            fprintf(out, "    raw = (char *)__v32_alloc(n * sizeof(");
        else
            fprintf(out, "    raw = (char *)malloc(n * sizeof(");
        print_class_type_name(out, cd->str1);
        if (g_alloc_arena) fprintf(out, ") + %d, %d, n, %d);\n", hdr, arena_class_id(cd), hdr);
        else fprintf(out, ") + %d);\n", hdr);
        fprintf(out, "    if (raw == NULL) return NULL;\n    hdr = (int *)raw;\n    hdr[0] = n;\n    p = (");
        print_class_type_name(out, cd->str1);
        fprintf(out, " *)(raw + %d);\n", hdr);
    } else {
        fprintf(out, "    p = (");
        print_class_type_name(out, cd->str1);
        fprintf(out, " *)%s(n * sizeof(", V32_ALLOC_HOOK ? "__v32_alloc" : "malloc");
        print_class_type_name(out, cd->str1);
        fprintf(out, "));\n    if (p == NULL) return NULL;\n");
    }
    if (mode == 0 && ctor != NULL) {
        fprintf(out, "    for (i = 0; i < n; i++) %s(&p[i]", dtor_mangled_name(ctor));
        for (int k = 1; k < ctor->list.count; k++) {
            fprintf(out, ", ");
            print_expr(out, ctor->list.items[k]->a);
        }
        fprintf(out, ");\n");
    } else if (mode == 0 && vt) {
        fprintf(out, "    for (i = 0; i < n; i++) p[i].vtable = &%s_vtable_instance;\n", cd->str1);
    }
    fprintf(out, "    return p;\n}\n\n\n");
}

/* `delete[] p` where p's static class is known: destroys the elements last
 * first (each through its own class's destructor, not the vtable: the
 * array is of exactly this type -- through a Base * to a Derived array it
 * is undefined, as in C++), then releases the block from its header. */
static void emit_array_delete_runtime(FILE *out, const AstNode *cd) {
    const AstNode *dtor = class_dtor_with_body(cd);
    fprintf(out, "void v32_delete_arr_%s(", cd->str1);
    print_class_type_name(out, cd->str1);
    fprintf(out, " *ptr)\n{\n");
    if (dtor == NULL) {
        fprintf(out, "    if (ptr == NULL) return;\n");
        if (V32_ALLOC_HOOK) fprintf(out, "    __v32_release(ptr);\n");
        else fprintf(out, "    free(ptr);\n");
        fprintf(out, "}\n\n\n");
        return;
    }
    fprintf(out, "    char *raw;\n    int i;\n    if (ptr == NULL) return;\n"
                 "    raw = (char *)ptr - %d;\n"
                 "    for (i = ((int *)raw)[0] - 1; i >= 0; i--) %s(&ptr[i]);\n",
            array_hdr_units(), dtor_mangled_name(dtor));
    if (g_alloc_arena) fprintf(out, "    __v32_arena_kill(raw);\n");
    else if (g_alloc_gc) fprintf(out, "    __v32_release(raw);\n");
    else fprintf(out, "    free(raw);\n");
    fprintf(out, "}\n\n\n");
}

static void emit_array_new_runtime_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            emit_array_new_runtime_class(out, n, 0);
            if (g_uses_list_new) {
                emit_array_new_runtime_class(out, n, 1);
                emit_array_new_runtime_class(out, n, 2);
            }
            emit_array_delete_runtime(out, n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_array_new_runtime_classes(out, &n->list);
        }
    }
}

/* v32_new_arr_int/float/char/bool -- array-new isn't only ever used on a
 * class (`new int[5]` is genuinely legal C++, and this project's own
 * grammar accepts it -- tests/sample29.cpp uses exactly this), so the
 * per-CLASS emission above (emit_array_new_runtime_classes) isn't
 * enough on its own: a class is something this project's AST walks
 * naturally find by iterating declarations, but a primitive type isn't
 * declared anywhere to walk over at all. Emitted unconditionally
 * whenever misc.h is included at all, same "over-generate rather than
 * risk under-generating" trade-off already made for every other
 * runtime function in this file -- one of these going uncalled is
 * harmless; a program calling one that was never defined is a fatal
 * compile error, the exact category of bug this project has hit and
 * fixed more than once already (v32_new_Player, early on; this,
 * caught directly by Matthew's own test run of tests/sample29.cpp
 * rather than here first). `void` deliberately excluded -- `new
 * void[N]` isn't meaningful. */
static void emit_primitive_array_new_runtime(FILE *out) {
    emit_array_new_runtime(out, "int");
    emit_array_new_runtime(out, "float");
    emit_array_new_runtime(out, "char");
    emit_array_new_runtime(out, "bool");
    if (g_uses_ptr_array_new)         /* `new T*[n]`: n pointers, whatever they point to */
        fprintf(out, "int **v32_new_arr_ptr(int n)\n{\n    return (int **)%s(n * sizeof(int *));\n}\n\n\n",
                V32_ALLOC_HOOK ? "__v32_alloc" : "malloc");
    if (g_uses_ptr_array_new && g_uses_list_new) {   /* `new T*[n]{p, q}`: the listed ones, then null */
        fprintf(out, "int **v32_new_arr_ptr_init(int n, int k)\n{\n    if (k < 0) k = 0;\n    if (n < k) n = k;\n"
                     "    return (int **)%s(n * sizeof(int *));\n}\n\n\n", V32_ALLOC_HOOK ? "__v32_alloc" : "malloc");
        fprintf(out, "void v32_new_arr_ptr_rest(int **p, int n, int k)\n{\n    int i;\n    if (p == NULL) return;\n"
                     "    if (k < 0) k = 0;\n    if (n < k) n = k;\n    for (i = k; i < n; i++) p[i] = NULL;\n}\n\n\n");
    }
    {   /* `new int(5)`, `new float`, `new Node*`: one word through the allocation hook */
        static const char *const nm[] = { "", "int", "float", "char", "bool" };
        for (int k = 1; k <= 4; k++)
            if (g_uses_scalar_new & (1 << k))
                fprintf(out, "%s *v32_new_%s(%s v)\n{\n    %s *p = (%s *)%s(sizeof(%s));\n    if (p == NULL) return p;\n"
                             "    *p = v;\n    return p;\n}\n\n\n", nm[k], nm[k], nm[k], nm[k], nm[k],
                        V32_ALLOC_HOOK ? "__v32_alloc" : "malloc", nm[k]);
        if (g_uses_scalar_new & (1 << 5))
            fprintf(out, "int **v32_new_ptr(int *v)\n{\n    int **p = (int **)%s(sizeof(int *));\n    if (p == NULL) return p;\n"
                         "    *p = v;\n    return p;\n}\n\n\n", V32_ALLOC_HOOK ? "__v32_alloc" : "malloc");
    }
    if (g_uses_list_new) {
        emit_array_new_runtime_init(out, "int", NULL);
        emit_array_new_runtime_init(out, "float", NULL);
        emit_array_new_runtime_init(out, "char", NULL);
        emit_array_new_runtime_init(out, "bool", NULL);
    }
}

static void emit_v32_delete(FILE *out) {
    /* The NULL guard is REQUIRED on Vircon32, not cosmetic: C++ 'delete'
     * on a null pointer is a no-op, but this project's own generated
     * call sites delete unconditionally (e.g. a Game destructor or
     * reset path deleting a maybe-never-allocated member), and
     * Vircon32's NULL is -1 (address 0 is a legitimate address there) --
     * free() has no reason to treat -1 specially the way standard C's
     * free(NULL) must. */
    if (V32_ALLOC_HOOK)
        fprintf(out, "void v32_delete(void *ptr)\n{\n    if (ptr == NULL) return;\n    __v32_release(ptr);\n}\n\n\n");
    else
        fprintf(out, "void v32_delete(void *ptr)\n{\n    if (ptr == NULL) return;\n    free(ptr);\n}\n\n\n");
}

/* ---- destructor invocation via delete ------------------------------------
 *
 * lower.c's new_delete_rewrite_expr now names a `delete obj;` whose
 * operand's static class is known as "v32_delete_ClassName" rather than
 * the fully generic "v32_delete" -- this is where THAT gets defined,
 * mirroring emit_new_delete_runtime's own shape but for teardown:
 * `v32_delete_ClassName` calls `ClassName`'s own destructor (if one
 * exists and has a body -- same "must have a body" reasoning as every
 * other constructor/destructor lookup in this project) before `free()`.
 *
 * Unlike `new`, there's no per-overload naming question here at all --
 * C++ never allows more than one destructor per class (they take no
 * parameters and can't be overloaded), so "v32_delete_ClassName" is
 * always unambiguous whenever a class has one.
 *
 * DELIBERATELY NOT VIRTUAL DISPATCH. `delete basePtr;` where `basePtr`
 * statically types as an ancestor but actually points at a derived
 * object will call the ANCESTOR's destructor, not the derived one --
 * exactly the classic "non-virtual destructor through a base pointer"
 * C++ footgun, except this project doesn't even check whether the
 * destructor was declared `virtual` before deciding this; it always
 * behaves as if it weren't. Virtual destructor dispatch would need
 * `new_delete_rewrite_expr`'s AST_DELETE case to route through the same
 * vtable-dispatch shape finalize_call already builds for an ordinary
 * virtual method call -- a real, separate piece of future work, not
 * silently assumed handled by what's here.
 *
 * The fully generic `v32_delete(void *ptr)` (emit_v32_delete, above)
 * remains, unconditionally, as the fallback lower.c uses whenever a
 * delete operand's static class can't be determined at all -- it just
 * frees, calling nothing, same as before this round.
 */
/* Finds `class_decl`'s own vtable slot for a destructor, if its
 * destructor is virtual at all -- returns the CANONICAL declaring
 * method (the class whose own vtable struct type first declared this
 * field, needed to know whether the receiver argument needs a cast the
 * same way an ordinary virtual call already does -- see
 * emit_vtable_instance's own doc comment for the identical reasoning),
 * or NULL if this class has no vtable, or its vtable has no destructor
 * slot at all (destructor not virtual, or no destructor declared). A
 * class can only ever have one destructor (never overloaded), so
 * there's no ambiguity to resolve if a slot is found. */
static AstNode *find_dtor_canonical(const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL || layout->vtable == NULL) return NULL;
    for (int i = 0; i < layout->vtable->count; i++) {
        AstNode *canon = layout->vtable->entries[i].canonical_method;
        if (canon->str1 != NULL && canon->str1[0] == '~') return canon;
    }
    return NULL;
}

static void emit_delete_runtime(FILE *out, const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return;

    AstNode *dtor = NULL;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (m->str1 == NULL || m->str1[0] != '~') continue; /* not a destructor */
        if (m->kind != AST_FUNC_DEF) continue; /* no body -- nothing to call */
        dtor = m;
        break; /* at most one can ever exist -- no ambiguity to resolve */
    }

    fprintf(out, "void v32_delete_%s(", class_decl->str1);
    print_class_type_name(out, class_decl->str1);
    fprintf(out, " *ptr)\n{\n");
    /* NULL guard, same reasoning as emit_v32_delete's own copy: C++
     * 'delete' on null is a no-op, but Vircon32's NULL is -1 (address
     * 0 is legitimate memory there), so free() can't be relied on to
     * special-case it. Emitted as the FIRST statement inside the
     * braces, after the header -- never before the function's opening,
     * where it would land at file scope. */
    fprintf(out, "    if (ptr == NULL) return;\n");
    explain(out, 1, "C++'s 'delete' has no C equivalent -- this function does what 'delete' does under the hood: call the destructor explicitly, then free the memory");

    /* VIRTUAL DESTRUCTOR DISPATCH: if this class's destructor is
     * virtual (or overrides one), route through the vtable instead of
     * calling a statically-named function -- this is what makes
     * `delete basePtr;` correctly call the DERIVED destructor when
     * basePtr's static type is an ancestor but it actually points at a
     * derived object. Previously this always called the statically-
     * resolved destructor regardless, a real, documented gap (see
     * lower.h's phase 6 entry) -- fixed here, at the one place that
     * actually needed to change: v32_delete_ClassName ITSELF, not
     * lower.c's own naming logic (which already correctly names the
     * allocator after the operand's static class; that naming was never
     * the problem -- what that per-class function DID internally was).
     * Same receiver-cast reasoning as an ordinary virtual method call
     * (finalize_call, lower.c) -- the vtable access itself never needs a
     * cast (every class's own vtable struct redeclares every canonical
     * field name, regardless of static type), but the argument passed
     * to the slot does, whenever this class isn't itself the canonical
     * declarer. */
    AstNode *dtor_canonical = find_dtor_canonical(class_decl);
    if (dtor_canonical != NULL) {
        FuncSemaInfo *canon_info = (FuncSemaInfo *)dtor_canonical->sema_info;
        const char *field_name = (canon_info != NULL) ? canon_info->mangled_name : dtor_canonical->str1;
        const AstNode *canonical_class = find_declaring_class(class_decl, dtor_canonical);
        if (canonical_class != NULL && canonical_class != class_decl) {
            explain(out, 1, "virtual destructor: dispatched through the vtable, not called by a fixed name -- this is what makes 'delete basePtr' correctly run the DERIVED class's destructor when basePtr actually points at a derived object");
            fprintf(out, "    ptr->vtable->%s((", field_name);
            print_class_type_name(out, canonical_class->str1);
            fprintf(out, " *)ptr);\n");
        } else {
            explain(out, 1, "virtual destructor: dispatched through the vtable, not called by a fixed name -- this is what makes 'delete basePtr' correctly run the DERIVED class's destructor when basePtr actually points at a derived object");
            fprintf(out, "    ptr->vtable->%s(ptr);\n", field_name);
        }
    } else if (dtor != NULL) {
        FuncSemaInfo *dtor_info = (FuncSemaInfo *)dtor->sema_info;
        const char *dtor_mangled = (dtor_info != NULL) ? dtor_info->mangled_name : dtor->str1;
        fprintf(out, "    %s(ptr);\n", dtor_mangled);
    }
    /* else: no destructor with a body exists for this class -- nothing
     * to call, same as a class with no constructor gets no call from
     * v32_new_ClassName either. */
    if (g_alloc_arena) {
        /* the destructor has run: the arena's record must not run it again */
        if (class_dtor_with_body(class_decl) != NULL) fprintf(out, "    __v32_arena_kill(ptr);\n");
    } else if (g_alloc_gc) {
        /* the destructor has run: the block goes back to the collector's free lists at once */
        fprintf(out, "    __v32_release(ptr);\n");
    } else {
        fprintf(out, "    free(ptr);\n");
    }
    fprintf(out, "}\n\n\n");
}

static void emit_delete_runtime_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            emit_delete_runtime(out, n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_delete_runtime_classes(out, &n->list);
        }
    }
}

static int program_has_any_class(const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) return 1;
        if (n->kind == AST_NAMESPACE_DECL && program_has_any_class(&n->list)) return 1;
    }
    return 0;
}

/* Re-emits every '#'-line the lexer captured (driver.h's
 * g_preprocessor_lines), verbatim, in original order -- see that
 * struct's own doc comment for the full reasoning. Deliberately goes
 * FIRST, ahead of even the conditional `#include "misc.h"` codegen_run
 * may add on its own -- whatever the person actually wrote at the top
 * of their own file stays at the top of the output, matching ordinary
 * expectations for a C file, with anything this project itself needs
 * to add coming after. */
static void emit_preprocessor_passthrough(FILE *out) {
    for (int i = 0; i < g_preprocessor_lines.count; i++) {
        fprintf(out, "%s\n", g_preprocessor_lines.lines[i]);
    }
}

/* Emits one `#define NAME id` per recognized cart hint (driver.h's
 * g_cart_textures/g_cart_sounds -- populated by lexer.l's own
 * `#texture`/`#sound` recognition), textures first, then sounds, each
 * group in original declaration order -- id is always just that
 * entry's own position in its list, matching the "id == declaration
 * order == XML position" invariant this mirrors from v32lua's own cart
 * hints. A true compile-time constant, not a global needing runtime
 * initialization -- C supports #define where v32lua's own Lua target
 * doesn't, so a texture/sound name a person's C++ source goes on to
 * use (as select_texture(BACKGROUND), say) resolves to its id entirely
 * at the C-compiler level, not anything this project's own sema needs
 * to know about at all. Runs right after the pass-through above,
 * deliberately -- these came from '#'-lines too, so they belong
 * grouped with everything else that did, ahead of anything this
 * project itself goes on to add (misc.h, generated declarations, ...). */
static void emit_cart_hint_defines(FILE *out) {
    for (int i = 0; i < g_cart_textures.count; i++) {
        fprintf(out, "#define %s %d\n", g_cart_textures.items[i].name, i);
    }
    for (int i = 0; i < g_cart_sounds.count; i++) {
        fprintf(out, "#define %s %d\n", g_cart_sounds.items[i].name, i);
    }
}

/* `extern T x;` of a variable a pass-through header defines, inside a namespace block: not printed */
static void drop_header_var_externs(AstList *decls) {
    int k = 0;
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d != NULL && d->kind == AST_VAR_DECL && (d->extern_decl || d->is_extern) && prescan_is_header_var(d->str1)) continue;
        if (d != NULL && d->kind == AST_NAMESPACE_DECL) drop_header_var_externs(&d->list);
        decls->items[k++] = d;
    }
    decls->count = k;
}

void codegen_run(const AstNode *program, FILE *out, int verbose_comments) {
    /* Reset -g's own tracking state -- main.c is single-shot per process
     * today, so this never actually matters in practice, but costs
     * nothing and avoids a latent bug if that ever changes. */
    g_codegen_out_line = 1;
    g_debug_last_cpp_line = -1;
    g_lm_valid = 0;
    g_verbose_comments = verbose_comments;
    emit_preprocessor_passthrough(out);
    emit_cart_hint_defines(out);
    if (g_target == TARGET_STANDARD) {
        /* `bool` is a native keyword in Vircon32 C (confirmed by
         * Matthew), needing nothing extra there, but standard C only
         * gets it from <stdbool.h> -- and this project's own generated
         * code uses the bare word `bool` in more places than just a
         * program that itself declares a bool variable: every class
         * unconditionally gets a `v32_new_arr_bool` allocator helper
         * (emit_primitive_array_new_runtime, used or not, the same
         * "over-generate rather than under-generate" trade-off this
         * file already makes for every other runtime helper), and
         * `true`/`false` literals print as the bare words regardless
         * of target. A real, confirmed gap, not a hypothetical:
         * running this project's own test suite's --target=standard
         * output through gcc showed this broke standard-mode
         * compilation for nearly every class-having sample, not just
         * ones that happened to declare a `bool` themselves. Emitted
         * unconditionally in standard mode (not gated on "does this
         * program actually use bool anywhere", the way misc.h/
         * stdlib.h's own inclusion below is gated on "does this
         * program have a class") -- a program with NO class and no
         * `new`/`delete` at all can still declare a plain `bool` local
         * with nothing else pulling stdlib.h in (tests/48sample.cpp,
         * `58sample.cpp`, `59sample.cpp` all do exactly this), so
         * gating this on the same `needs_misc` check just below would
         * have missed them; <stdbool.h> is a small, always-safe
         * standard header with no real cost to including even when
         * genuinely unused, unlike misc.h's own much larger Vircon32-
         * specific runtime surface, so there's no real trade-off here
         * to gate on in the first place. */
        fprintf(out, "#include <stdbool.h>\n");
    }
    /* misc.h (Vircon32's real malloc()/free(), among other things) is
     * only included when the program has at least one class -- every
     * class gets a v32_new_* allocator (even one never actually used
     * with `new` -- see emit_new_delete_runtime's own doc comment for
     * why this round didn't build the extra "only if actually new-ed"
     * scan that would let this be scoped more tightly), so "any class
     * exists" and "malloc is needed somewhere" are equivalent here.
     * Conditional specifically so a program with no classes at all
     * (tests/sample21.cpp, say) doesn't need to expose misc.h's own
     * names (malloc/free/rand/exit/...) into scope for no reason.
     *
     * ALSO true whenever the program uses `new`/`delete` in ANY form at
     * all (g_uses_new_or_delete, driver.h), regardless of whether it
     * has a class -- a real, confirmed bug otherwise: a program with
     * only free functions can still write `new int[5]` (this project's
     * primitive-typed array-new; tests/sample29.cpp is exactly this),
     * which "any class exists" alone would never catch, silently
     * dropping misc.h and every runtime function that depends on it
     * from the output despite being called. */
    int needs_misc = program_has_any_class(&program->list) || g_uses_new_or_delete || V32_ALLOC_HOOK;
    if (needs_misc && g_c_mode && !g_uses_new_or_delete && !V32_ALLOC_HOOK) {
        /* C input: a struct is only data, and nothing from misc.h is
         * needed on its account. */
        needs_misc = 0;
    }
    if (needs_misc) {
        /* Vircon32 mode: "misc.h", Vircon32's own header providing
         * malloc/free/rand/srand/exit (among others). Standard mode:
         * <stdlib.h>, the portable C header providing the same set --
         * this project's own generated code never emits memset/memcpy
         * calls of its own (confirmed directly, not assumed; a real
         * user-written call to either still needs its own <string.h>,
         * but that's the source's own responsibility the same way it
         * already is in Vircon32 mode, unrelated to this include). */
        if (g_target == TARGET_STANDARD) {
            fprintf(out, "#include <stdlib.h>\n");
        } else {
            fprintf(out, "#include \"misc.h\"\n");
        }
        if (V32_ALLOC_HOOK) {
            /* the arena's out-of-space message */
            if (g_target == TARGET_STANDARD) fprintf(out, "#include <stdio.h>\n");
            else fprintf(out, "#include \"video.h\"\n#include \"time.h\"\n");
        }
        if (g_alloc_gc && g_target == TARGET_STANDARD)   /* memset, setjmp, intptr_t for the collector */
            fprintf(out, "#include <string.h>\n#include <setjmp.h>\n#include <stdint.h>\n");
    }
    fprintf(out, "/* Auto-generated Vircon32 C -- do not edit by hand. */\n\n");
    /* Structs read from a pass-through header (prescan.c) are declared for
     * the transpiler's benefit only: the header defines them in the output
     * already, so they are left out of everything printed below. */
    AstList visible = program->list;
    visible.items = malloc(sizeof(AstNode *) * (size_t)(program->list.count + 1));
    visible.count = 0;
    for (int i = 0; i < program->list.count; i++) {
        AstNode *d = program->list.items[i];
        if (d != NULL && d->kind == AST_CLASS_DECL && prescan_is_header_struct(d->str1)) continue;
        if (d != NULL && d->kind == AST_VAR_DECL && (d->extern_decl || d->is_extern) && prescan_is_header_var(d->str1))
            continue;       /* `extern int x;` of a variable the header defines: that definition is the variable */
        if (d != NULL && d->kind == AST_NAMESPACE_DECL) drop_header_var_externs(&d->list);
        visible.items[visible.count++] = d;
    }
    AstNode view = *program;
    view.list = visible;
    program = &view;
    if (g_c_mode)
        for (int i = 0; i < g_c_fwd_tags.count; i++)
            fprintf(out, "struct %s;\n", g_c_fwd_tags.items[i]->str1);
    emit_forward_declarations(out, &program->list);
    fprintf(out, "\n");
    emit_typedefs(out, &program->list);
    fprintf(out, "\n");
    emit_enums(out, &program->list);
    emit_classes(out, &program->list);
    /* C input: a file-scope initializer may name a function (a table of
     * handlers), so the prototypes go ahead of the variables there. */
    int globals_after = g_c_mode || g_globals_name_functions;   /* see g_globals_name_functions */
    if (!globals_after) emit_globals(out, &program->list);
    emit_function_prototypes_classes(out, &program->list);
    SeenNames seen = {0};
    emit_function_prototypes_free_functions(out, &program->list, &seen);
    free(seen.names);
    if (globals_after) emit_globals(out, &program->list);
    fprintf(out, "\n");
    emit_vtable_instances_classes(out, &program->list);
    if (needs_misc) {
        if (g_alloc_arena) {
            g_arena_class_count = 0;
            collect_arena_classes(&program->list);
            emit_arena_runtime(out, &program->list);
        } else if (g_alloc_gc) {
            emit_gc_runtime(out, &program->list);
        }
        emit_new_delete_runtime_classes(out, &program->list);
        emit_array_new_runtime_classes(out, &program->list);
        emit_primitive_array_new_runtime(out);
        emit_v32_delete(out);
        emit_delete_runtime_classes(out, &program->list);
    }
    emit_function_definitions_classes(out, &program->list);
    emit_function_definitions_free_functions(out, &program->list);
}
