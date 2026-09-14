#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "sema.h"
#include "lower.h"
#include "codegen.h"
#include "driver.h" /* g_preprocessor_lines -- see its own doc comment there */

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
static void print_type(FILE *out, const AstNode *type) {
    if (type == NULL) {
        fprintf(out, "void");
        return;
    }
    switch (type->kind) {
        case AST_IDENT:
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
        case AST_ARRAY_TYPE:
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
             * used, and output is always this one form regardless. */
            print_type(out, type->a);
            fprintf(out, " [%d]", type->ival);
            break;
        default:
            fprintf(out, "void" /* unrecognized type node -- best-effort */);
            break;
    }
}

/* ---- typedefs -------------------------------------------------------- */

static void emit_typedefs(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_TYPEDEF_DECL) {
            fprintf(out, "typedef ");
            print_type(out, n->type);
            fprintf(out, " %s;\n", n->str1);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_typedefs(out, &n->list);
        }
    }
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
 */
static void emit_vtable_struct(FILE *out, const AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL || layout->vtable == NULL) return;

    fprintf(out, "struct %s_VTable {\n", class_decl->str1);
    for (int i = 0; i < layout->vtable->count; i++) {
        VtableEntry *entry = &layout->vtable->entries[i];
        AstNode *canonical = entry->canonical_method;
        FuncSemaInfo *info = (FuncSemaInfo *)canonical->sema_info;
        const char *field_name = (info != NULL) ? info->mangled_name : canonical->str1;
        const AstNode *canonical_class = find_declaring_class(class_decl, canonical);

        fprintf(out, "    ");
        print_type(out, canonical->type);
        fprintf(out, "(%s *", canonical_class->str1);

        int already_this_injected = (canonical->kind == AST_FUNC_DEF);
        int start = already_this_injected ? 1 : 0;
        for (int p = start; p < canonical->list.count; p++) {
            fprintf(out, ", ");
            print_type(out, canonical->list.items[p]->type);
        }
        fprintf(out, ")* %s;\n", field_name);
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

    fprintf(out, "struct %s {\n", class_decl->str1);
    for (int i = 0; i < layout->count; i++) {
        StructField *f = &layout->fields[i];
        fprintf(out, "    ");
        if (f->kind == FIELD_VTABLE_PTR) {
            /* Bare, no `struct` keyword -- this is a REFERENCE to the
             * vtable struct type emitted just above by
             * emit_vtable_struct(), not a definition. */
            fprintf(out, "%s_VTable *vtable;\n", class_decl->str1);
        } else {
            print_type(out, f->type);
            fprintf(out, " %s;\n", f->name);
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
    fprintf(out, "%s_VTable %s_vtable_instance = {\n", class_decl->str1, class_decl->str1);
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
                fprintf(out, "(");
                print_type(out, entry->canonical_method->type);
                fprintf(out, "(%s *", canonical_class->str1);
                int start = (entry->canonical_method->kind == AST_FUNC_DEF) ? 1 : 0;
                for (int p = start; p < entry->canonical_method->list.count; p++) {
                    fprintf(out, ", ");
                    print_type(out, entry->canonical_method->list.items[p]->type);
                }
                fprintf(out, ")*)&%s", impl_mangled);
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
    print_type(out, n->type);
    fprintf(out, " %s", n->str1);
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
static void print_stmt(FILE *out, const AstNode *s, int indent, int strip_return_value) {
    if (s == NULL) return;
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
        case AST_EXPR_STMT:
            indent_spaces(out, indent);
            print_expr(out, s->a);
            fprintf(out, ";\n");
            break;
        case AST_VAR_DECL:
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
    int is_main = (strcmp(name, "main") == 0);

    if (is_main) {
        /* Vircon32 requires `void main()` specifically -- see mangle()'s
         * own doc comment in sema.c for why this is forced here rather
         * than requiring the C++ source to already declare it that way.
         * func->type is deliberately ignored in this case, whatever the
         * C++ source actually declared (commonly `int main()`). */
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
    int is_main = (strcmp(name, "main") == 0);

    emit_function_header(out, func, name);
    fprintf(out, "\n");
    print_stmt(out, func->a, 0, is_main); /* func->a is the body, an AST_BLOCK */
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
    fprintf(out, " %s(%s *this", name, class_decl->str1);
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

        fprintf(out, "%s *v32_new_%s(", class_decl->str1, ctor_mangled);
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
        fprintf(out, "    %s *self = (%s *)malloc(sizeof(%s));\n",
                class_decl->str1, class_decl->str1, class_decl->str1);
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
        fprintf(out, "%s *v32_new_%s(void)\n{\n", class_decl->str1, class_decl->str1);
        fprintf(out, "    return (%s *)malloc(sizeof(%s));\n", class_decl->str1, class_decl->str1);
        fprintf(out, "}\n\n\n");
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
static void emit_array_new_runtime(FILE *out, const AstNode *class_decl) {
    fprintf(out, "%s *v32_new_arr_%s(int n)\n{\n", class_decl->str1, class_decl->str1);
    fprintf(out, "    return (%s *)malloc(n * sizeof(%s));\n", class_decl->str1, class_decl->str1);
    fprintf(out, "}\n\n\n");
}

static void emit_array_new_runtime_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            emit_array_new_runtime(out, n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_array_new_runtime_classes(out, &n->list);
        }
    }
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

    fprintf(out, "void v32_delete_%s(%s *ptr)\n{\n", class_decl->str1, class_decl->str1);
    if (dtor != NULL) {
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

void codegen_run(const AstNode *program, FILE *out) {
    emit_preprocessor_passthrough(out);
    /* misc.h (Vircon32's real malloc()/free(), among other things) is
     * only included when the program has at least one class -- every
     * class gets a v32_new_* allocator (even one never actually used
     * with `new` -- see emit_new_delete_runtime's own doc comment for
     * why this round didn't build the extra "only if actually new-ed"
     * scan that would let this be scoped more tightly), so "any class
     * exists" and "malloc is needed somewhere" are equivalent here.
     * Conditional specifically so a program with no classes at all
     * (tests/sample21.cpp, say) doesn't need to expose misc.h's own
     * names (malloc/free/rand/exit/...) into scope for no reason. */
    int needs_misc = program_has_any_class(&program->list);
    if (needs_misc) {
        fprintf(out, "#include \"misc.h\"\n");
    }
    fprintf(out, "/* Auto-generated Vircon32 C -- do not edit by hand. */\n\n");
    emit_forward_declarations(out, &program->list);
    fprintf(out, "\n");
    emit_typedefs(out, &program->list);
    fprintf(out, "\n");
    emit_classes(out, &program->list);
    emit_function_prototypes_classes(out, &program->list);
    SeenNames seen = {0};
    emit_function_prototypes_free_functions(out, &program->list, &seen);
    free(seen.names);
    fprintf(out, "\n");
    emit_vtable_instances_classes(out, &program->list);
    if (needs_misc) {
        emit_new_delete_runtime_classes(out, &program->list);
        emit_array_new_runtime_classes(out, &program->list);
        emit_v32_delete(out);
        emit_delete_runtime_classes(out, &program->list);
    }
    emit_function_definitions_classes(out, &program->list);
    emit_function_definitions_free_functions(out, &program->list);
}
