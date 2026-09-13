#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "sema.h"
#include "lower.h"
#include "codegen.h"

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
 * Walks up `class_decl`'s own ancestry to find whichever class's OWN
 * ClassLayout.methods list literally contains `target_method` (a
 * pointer-identity search, not a name match -- a name match could pick
 * the wrong overload/override). Needed because a vtable slot's
 * `canonical_method` (sema.h) can belong to an ANCESTOR, not necessarily
 * `class_decl` itself, and this module needs to know that ancestor's own
 * name to print the receiver parameter's type correctly. Falls back to
 * `class_decl` itself if the search somehow comes up empty (best-effort,
 * shouldn't happen for a canonical_method that genuinely came from this
 * hierarchy in the first place).
 */
static const AstNode *find_declaring_class(const AstNode *class_decl, const AstNode *target_method) {
    const AstNode *cur = class_decl;
    while (cur != NULL) {
        ClassLayout *layout = (ClassLayout *)cur->sema_info;
        if (layout == NULL) break;
        for (int i = 0; i < layout->methods.count; i++) {
            if (layout->methods.items[i] == target_method) return cur;
        }
        cur = layout->base_class_decl;
    }
    return class_decl;
}

/* Emits `class_decl`'s vtable struct TYPE -- one field per virtual slot,
 * each a function pointer. Does nothing if the class has no vtable at
 * all (layout->vtable == NULL).
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
        fprintf(out, " (*%s)(%s *", field_name, canonical_class->str1);

        int already_this_injected = (canonical->kind == AST_FUNC_DEF);
        int start = already_this_injected ? 1 : 0;
        for (int p = start; p < canonical->list.count; p++) {
            fprintf(out, ", ");
            print_type(out, canonical->list.items[p]->type);
        }
        fprintf(out, ");\n");
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
static void print_stmt(FILE *out, const AstNode *s, int indent);

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

static void print_stmt(FILE *out, const AstNode *s, int indent) {
    if (s == NULL) return;
    switch (s->kind) {
        case AST_BLOCK:
            indent_spaces(out, indent);
            fprintf(out, "{\n");
            for (int i = 0; i < s->list.count; i++) {
                print_stmt(out, s->list.items[i], indent + 1);
            }
            indent_spaces(out, indent);
            fprintf(out, "}\n");
            break;
        case AST_IF:
            indent_spaces(out, indent);
            fprintf(out, "if (");
            print_expr(out, s->a);
            fprintf(out, ")\n");
            print_stmt(out, s->b, indent); /* s->b is itself an AST_BLOCK -- prints its own braces */
            if (s->c != NULL) {
                indent_spaces(out, indent);
                fprintf(out, "else\n");
                print_stmt(out, s->c, indent);
            }
            break;
        case AST_WHILE:
            indent_spaces(out, indent);
            fprintf(out, "while (");
            print_expr(out, s->a);
            fprintf(out, ")\n");
            print_stmt(out, s->b, indent);
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
            print_stmt(out, s->d, indent);
            break;
        case AST_RETURN:
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
static void emit_function_header(FILE *out, const AstNode *func) {
    FuncSemaInfo *info = (FuncSemaInfo *)func->sema_info;
    const char *name = (info != NULL) ? info->mangled_name : func->str1;

    print_type(out, func->type);
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
    emit_function_header(out, func);
    fprintf(out, ";\n");
}

static void emit_function_definition(FILE *out, const AstNode *func) {
    emit_function_header(out, func);
    fprintf(out, "\n");
    print_stmt(out, func->a, 0); /* func->a is the body, an AST_BLOCK */
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
static void emit_function_prototypes_classes(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (m->kind == AST_FUNC_DEF) emit_function_prototype(out, m);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            emit_function_prototypes_classes(out, &n->list);
        }
    }
}

static void emit_function_prototypes_free_functions(FILE *out, const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            emit_function_prototypes_free_functions(out, &n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            emit_function_prototype(out, n);
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

void codegen_run(const AstNode *program, FILE *out) {
    fprintf(out, "/* Auto-generated Vircon32 C -- do not edit by hand. */\n\n");
    emit_forward_declarations(out, &program->list);
    fprintf(out, "\n");
    emit_typedefs(out, &program->list);
    fprintf(out, "\n");
    emit_classes(out, &program->list);
    emit_function_prototypes_classes(out, &program->list);
    emit_function_prototypes_free_functions(out, &program->list);
    fprintf(out, "\n");
    emit_function_definitions_classes(out, &program->list);
    emit_function_definitions_free_functions(out, &program->list);
}
