#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "sema.h"
#include "lower.h"
#include "codegen.h"
#include "driver.h" /* g_preprocessor_lines -- see its own doc comment there */
#include "debugmap.h"

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
                if (type_to_class(type) != NULL) {
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
        case AST_QUALIFIED_ID:
            if (type->list.count > 0) {
                fprintf(out, "%s", type->list.items[type->list.count - 1]->str1);
            } else {
                fprintf(out, "void" /* malformed -- shouldn't happen */);
            }
            break;
        case AST_POINTER_TYPE:
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
            fprintf(out, "const ");
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
            int dims[64]; /* generous fixed cap, not a dynamically-sized
                structure -- this is a purely local, transient printing
                operation, not part of the persistent AST, and a
                64-dimensional array is so far beyond anything remotely
                realistic that a fixed stack array is the simpler,
                completely sufficient choice here, matching how other
                bounded, small-scale bookkeeping already works elsewhere
                in this file (e.g. indent tracking). */
            int dim_count = 0;
            while (base->kind == AST_ARRAY_TYPE && dim_count < 64) {
                dims[dim_count++] = base->ival;
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
                fprintf(out, "%s[%d]", (i == 0) ? " " : "", dims[i]);
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
        fprintf(out, "[%d]", type->ival);
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
static void emit_unions(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_UNION_DECL) {
            fprintf(out, "union %s {\n", n->str1);
            for (int j = 0; j < n->list.count; j++) {
                fprintf(out, "    ");
                print_var_decl_inline(out, n->list.items[j]);
                fprintf(out, ";\n");
            }
            fprintf(out, "};\n\n");
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_unions(out, &n->list);
        }
    }
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
            print_class_type_name(out, canonical_class->str1);
            fprintf(out, " *");
            for (int p = start; p < canonical->list.count; p++) {
                fprintf(out, ", ");
                print_type(out, canonical->list.items[p]->type);
            }
            fprintf(out, ");\n");
        } else {
            print_type(out, canonical->type);
            fprintf(out, "(%s *", canonical_class->str1);
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
                    print_class_type_name(out, canonical_class->str1);
                    fprintf(out, " *");
                    for (int p = start; p < entry->canonical_method->list.count; p++) {
                        fprintf(out, ", ");
                        print_type(out, entry->canonical_method->list.items[p]->type);
                    }
                    fprintf(out, "))&%s", impl_mangled);
                } else {
                    fprintf(out, "(%s *", canonical_class->str1);
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

/* ---- top-level walk ---------------------------------------------------
 *
 * Emits every class's vtable struct type (if any) immediately followed
 * by its own struct definition, in natural source-encounter order,
 * recursing into namespaces (flattened -- see print_type's own doc
 * comment on AST_QUALIFIED_ID for why that's consistent with the rest of
 * this compiler rather than a new limitation). See codegen.h for the
 * open question this ordering doesn't resolve in general.
 */
static void emit_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            emit_vtable_struct(out, n);
            emit_struct(out, n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_classes(out, &n->list);
        }
    }
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
        case '\0': fprintf(out, "'\\0'"); return;
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
            fprintf(out, "%d", e->ival);
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
            /* %.17g, not %g -- guarantees a double round-trips through
             * source text exactly, at the cost of occasionally more
             * digits than a human would write by hand. Correctness over
             * cosmetics for generated code. */
            fprintf(out, "%.17g", e->fval);
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
            print_char_literal(out, e->ival);
            break;
        case AST_NULL_LIT:
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
    if (s->line != g_debug_last_cpp_line) {
        debug_map_record(g_codegen_out_line, s->line, NULL);
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
    debug_map_record(g_codegen_out_line, func->line, name);
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
        fprintf(out, " *)malloc(sizeof(");
        print_class_type_name(out, class_decl->str1);
        fprintf(out, "));\n");
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
        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *v32_new_%s(void)\n{\n    return (", class_decl->str1);
        print_class_type_name(out, class_decl->str1);
        fprintf(out, " *)malloc(sizeof(");
        print_class_type_name(out, class_decl->str1);
        fprintf(out, "));\n}\n\n\n");
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
    print_class_type_name(out, type_name);
    fprintf(out, " *v32_new_arr_%s(int n)\n{\n    return (", type_name);
    print_class_type_name(out, type_name);
    fprintf(out, " *)malloc(n * sizeof(");
    print_class_type_name(out, type_name);
    fprintf(out, "));\n}\n\n\n");
}

static void emit_array_new_runtime_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            emit_array_new_runtime(out, n->str1);
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
}

static void emit_v32_delete(FILE *out) {
    fprintf(out, "void v32_delete(void *ptr)\n{\n    free(ptr);\n}\n\n\n");
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
    fprintf(out, "    free(ptr);\n");
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

void codegen_run(const AstNode *program, FILE *out, int verbose_comments) {
    /* Reset -g's own tracking state -- main.c is single-shot per process
     * today, so this never actually matters in practice, but costs
     * nothing and avoids a latent bug if that ever changes. */
    g_codegen_out_line = 1;
    g_debug_last_cpp_line = -1;
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
    int needs_misc = program_has_any_class(&program->list) || g_uses_new_or_delete;
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
    }
    fprintf(out, "/* Auto-generated Vircon32 C -- do not edit by hand. */\n\n");
    emit_forward_declarations(out, &program->list);
    fprintf(out, "\n");
    emit_typedefs(out, &program->list);
    fprintf(out, "\n");
    emit_enums(out, &program->list);
    emit_unions(out, &program->list);
    emit_classes(out, &program->list);
    emit_globals(out, &program->list);
    emit_function_prototypes_classes(out, &program->list);
    SeenNames seen = {0};
    emit_function_prototypes_free_functions(out, &program->list, &seen);
    free(seen.names);
    fprintf(out, "\n");
    emit_vtable_instances_classes(out, &program->list);
    if (needs_misc) {
        emit_new_delete_runtime_classes(out, &program->list);
        emit_array_new_runtime_classes(out, &program->list);
        emit_primitive_array_new_runtime(out);
        emit_v32_delete(out);
        emit_delete_runtime_classes(out, &program->list);
    }
    emit_function_definitions_classes(out, &program->list);
    emit_function_definitions_free_functions(out, &program->list);
}
