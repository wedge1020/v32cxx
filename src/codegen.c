#include <stdio.h>
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

void codegen_run(const AstNode *program, FILE *out) {
    fprintf(out, "/* Auto-generated Vircon32 C -- do not edit by hand. */\n\n");
    emit_forward_declarations(out, &program->list);
    fprintf(out, "\n");
    emit_typedefs(out, &program->list);
    fprintf(out, "\n");
    emit_classes(out, &program->list);
}
