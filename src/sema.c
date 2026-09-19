#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "sema.h"

static int g_error_count = 0;

static void sema_error(int line, const char *fmt, ...) {
    fprintf(stderr, "semantic error at line %d: ", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    g_error_count++;
}

/* Same shape as sema_error above, deliberately -- same line-number-
 * prefixed stderr message, same varargs signature -- but for something
 * that should NOT stop the transpile: g_warning_count is tracked
 * entirely separately from g_error_count, and nothing in this project
 * ever checks it to decide pass/fail (main.c surfaces it purely for the
 * user's own visibility, via sema_get_warning_count() below, the same
 * "print a one-line summary if the count is nonzero" treatment
 * sema_error's own count already gets, just never treated as a reason
 * to report failure). First use: a user-written `dynamic_cast` (see
 * the AST_CAST case below) -- accepted and transpiled (as a bare
 * syntactic alias for a plain cast, per AST_CAST's own doc comment in
 * ast.h), but without the runtime type check real dynamic_cast's own
 * contract promises, since that requires RTTI, which this project has
 * never supported, by design. Silently accepting that gap would risk
 * someone relying on a safety guarantee that was never actually
 * implemented; a hard error would refuse to transpile code real C++
 * accepts, for a construct this project CAN still do something
 * reasonable with. A warning is the honest middle ground. */
static int g_warning_count = 0;
/* Non-fatal: reports something worth flagging without refusing to
 * transpile code real C++ (or, per the Vircon32 word-size check in
 * lower.c, real Vircon32 C) accepts. Exposed (not static) so lower.c
 * can reuse the same counting/formatting machinery for its own
 * warnings rather than duplicating it -- sema.c's own pass and
 * lower.c's own passes are both "diagnostics about accepted code",
 * conceptually the same kind of finding, just surfaced at different
 * points in the pipeline (sema.c before lowering runs at all; the
 * word-size check only after compute_struct_layouts has computed the
 * field counts it needs). */
void sema_warning(int line, const char *fmt, ...) {
    fprintf(stderr, "warning at line %d: ", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    g_warning_count++;
}

/* Read-only accessor for main.c -- same "call only after sema_run()"
 * convention as sema_program_has_main/sema_program_has_function, and
 * the same reason: this doesn't recompute or validate anything, it
 * just reports what sema_run()'s own walk already found. */
int sema_get_warning_count(void) {
    return g_warning_count;
}

/* ---- flat class registry, keyed by bare (unqualified) name -----------
 *
 * SIMPLIFICATION: keyed by bare name only, ignoring namespace nesting.
 * Fine for this skeleton's test programs (class names don't collide
 * across namespaces yet), but means a namespaced class's out-of-line
 * definitions resolve against *any* same-named class anywhere, not
 * specifically the one in the right namespace. Flagged again at the
 * out_of_line_def grammar rules in parser.y and in attach_out_of_line()
 * below, at the point it actually matters.
 */

typedef struct ClassRegEntry {
    const char *name;
    AstNode *decl;
    struct ClassRegEntry *next;
} ClassRegEntry;

static ClassRegEntry *g_class_registry = NULL;

static void register_class(AstNode *class_decl) {
    ClassRegEntry *e = malloc(sizeof(ClassRegEntry));
    e->name = class_decl->str1;
    e->decl = class_decl;
    e->next = g_class_registry;
    g_class_registry = e;
}

static AstNode *find_class(const char *name) {
    for (ClassRegEntry *e = g_class_registry; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) return e->decl;
    }
    return NULL;
}

static void free_registry(void) {
    ClassRegEntry *e = g_class_registry;
    while (e != NULL) {
        ClassRegEntry *next = e->next;
        free(e);
        e = next;
    }
    g_class_registry = NULL;
}

/* ---- flat typedef registry, keyed by bare (unqualified) name ----------
 *
 * NOTE: this is a SEPARATE mechanism from symtab.c's SYM_TYPEDEF entries,
 * not a duplicate of it. symtab's registry exists so the LEXER can decide
 * TYPE_NAME vs IDENTIFIER while parsing is still in progress; it doesn't
 * record what a typedef's underlying type actually IS, just that the name
 * is one. This registry exists later, after parsing, specifically to
 * answer "what does this typedef resolve to" for type comparison -- a
 * different question, needed by a different pass, which is why it's a
 * different, sema-owned structure rather than an extension of symtab's.
 *
 * Same flat/bare-name simplification as the class registry above, and for
 * the same reason: no attempt to scope typedefs by namespace. Only
 * top-level and namespace-nested typedefs are collected (see
 * collect_declarations below) -- typedefs local to a function body are
 * intentionally NOT registered here, since they're correctly invisible
 * outside that function in real C++ anyway (nothing outside it could
 * reference them in a parameter type), so there's nothing useful to
 * resolve them against for the purposes this registry exists for
 * (comparing PARAMETER types across declarations).
 */

typedef struct TypedefRegEntry {
    const char *name;
    AstNode *underlying_type;   /* the AST_TYPEDEF_DECL's own `type` field */
    struct TypedefRegEntry *next;
} TypedefRegEntry;

static TypedefRegEntry *g_typedef_registry = NULL;

static void register_typedef(AstNode *typedef_decl) {
    TypedefRegEntry *e = malloc(sizeof(TypedefRegEntry));
    e->name = typedef_decl->str1;
    e->underlying_type = typedef_decl->type;
    e->next = g_typedef_registry;
    g_typedef_registry = e;
}

static AstNode *find_typedef_target(const char *name) {
    for (TypedefRegEntry *e = g_typedef_registry; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) return e->underlying_type;
    }
    return NULL;
}

static void free_typedef_registry(void) {
    TypedefRegEntry *e = g_typedef_registry;
    while (e != NULL) {
        TypedefRegEntry *next = e->next;
        free(e);
        e = next;
    }
    g_typedef_registry = NULL;
}

/* ---- flat free-function registry, grouping ALL top-level (and
 * namespace-nested) functions -- entries share a name whenever two
 * functions are genuinely overloaded (different signatures), but
 * register_free_function (below) does dedupe an EXACT name+signature
 * match against an existing entry -- see that function's own doc
 * comment for why (an ordinary prototype-then-definition free function
 * would otherwise register as two candidates and misreport as an
 * ambiguous call). Used by call-site overload resolution (resolve_call,
 * further down) to collect every genuine candidate a free-function call
 * could mean. */

static int param_lists_match(const AstList *a, const AstList *b);

typedef struct FreeFuncRegEntry {
    AstNode *func;
    struct FreeFuncRegEntry *next;
} FreeFuncRegEntry;

static FreeFuncRegEntry *g_free_func_registry = NULL;

/* If an existing registered entry has the SAME name AND the SAME
 * parameter signature, this is the "other half" of an entirely ordinary
 * C++ pattern -- a free function's prototype, declared separately from
 * its own later definition -- not a genuine second candidate. Without
 * this check, resolve_call would see two identically-shaped candidates
 * for every call to that function and report it as ambiguous, which is
 * exactly the bug a near-miss while fixing tests/sample14.cpp surfaced
 * (see docs/DESIGN_NOTES.md) -- caught before it shipped as a workaround
 * in that one test file rather than fixed here, until now.
 *
 * Prefers keeping whichever entry HAS a body (AST_FUNC_DEF) if the two
 * disagree on that -- a prototype's own registration gets replaced by
 * its later definition, since the definition is what every later pass
 * (mangling already ran by the time this matters, but lowering and
 * codegen both need an actual body) needs to see. If the SAME kind
 * shows up twice (two prototypes, or -- a genuine redefinition error
 * this project doesn't otherwise diagnose -- two definitions), the
 * first one registered wins; not this pass's job to flag a duplicate
 * body as an error, only to avoid the ambiguous-overload
 * misdiagnosis. */
static void register_free_function(AstNode *func) {
    for (FreeFuncRegEntry *e = g_free_func_registry; e != NULL; e = e->next) {
        if (strcmp(e->func->str1, func->str1) == 0 && param_lists_match(&e->func->list, &func->list)) {
            if (func->kind == AST_FUNC_DEF && e->func->kind != AST_FUNC_DEF) {
                e->func = func;
            }
            return;
        }
    }
    FreeFuncRegEntry *e = malloc(sizeof(FreeFuncRegEntry));
    e->func = func;
    e->next = g_free_func_registry;
    g_free_func_registry = e;
}

static void free_free_func_registry(void) {
    FreeFuncRegEntry *e = g_free_func_registry;
    while (e != NULL) {
        FreeFuncRegEntry *next = e->next;
        free(e);
        e = next;
    }
    g_free_func_registry = NULL;
}

/* ---- pass 1: collect every class, typedef, AND free function, recursing
 * into namespace bodies (was collect_classes; renamed since it now does
 * all three) ------------------------------------------------------------ */

static void collect_declarations(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            register_class(n);
        } else if (n->kind == AST_TYPEDEF_DECL) {
            register_typedef(n);
        } else if ((n->kind == AST_FUNC_DECL || n->kind == AST_FUNC_DEF) && n->b == NULL) {
            /* n->b == NULL excludes an out-of-line method definition's
             * own top-level duplicate (see attach_out_of_line) -- that's
             * not a free function, it's a parse-time artifact of a
             * class's method being defined outside the class body, and
             * its real, callable identity lives on the class's member
             * list, found via the class registry instead. */
            register_free_function(n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            collect_declarations(&n->list);
        }
    }
}

/* ---- type-signature comparison and mangling ---------------------------
 *
 * Typedef-transparent as of this pass (previously purely syntactic -- see
 * resolve_typedef_chain below for the fix and its own remaining limits).
 */

/* Follows an AST_IDENT that names a registered typedef through to its
 * underlying type, repeatedly (a typedef may alias another typedef),
 * stopping at whichever comes first: a non-typedef AST_IDENT (a built-in
 * keyword type or an actual class name), or any non-AST_IDENT node
 * (pointer/reference/qualified-id) -- resolution doesn't need to chase
 * further at THIS level in that case, because types_equal's and
 * type_signature_str's own recursion into pointer/reference contents
 * (via ->a) re-invokes this same resolution one level down, so e.g.
 * `typedef int *IntPtr;` used against a literal `int *` still compares
 * correctly: resolving "IntPtr" yields the AST_POINTER_TYPE node itself,
 * whose ->a ("int") gets re-resolved (a no-op here, but would chase
 * further if int itself were, hypothetically, ALSO a typedef name) when
 * the pointee is compared.
 *
 * Defensively bounded against a typedef chain that resolves back to
 * itself, though that shouldn't be constructible in the first place given
 * this project's single-pass parsing model: a typedef can only name a
 * type that's already been declared, so `typedef A B; typedef B A;` can't
 * happen -- A would have to already exist, as something other than B,
 * before the second line could even parse.
 */
const AstNode *resolve_typedef_chain(const AstNode *type) {
    int guard = 0;
    while (type != NULL && type->kind == AST_IDENT && guard < 64) {
        AstNode *target = find_typedef_target(type->str1);
        if (target == NULL) break;
        type = target;
        guard++;
    }
    return type;
}

static int types_equal(const AstNode *t1, const AstNode *t2) {
    if (t1 == NULL || t2 == NULL) {
        return t1 == t2;
    }
    /* const is stripped from both sides before anything else -- BEFORE
     * resolve_typedef_chain too, since that function only recognizes a
     * bare AST_IDENT as a possible typedef name, and "const
     * SomeTypedefName" wrapped in AST_CONST_TYPE would never reach that
     * check otherwise. Deliberately made transparent for type-equality
     * purposes across the board here, not modeling real C++'s own
     * genuinely nuanced rules for when a const mismatch does or doesn't
     * matter for overload purposes (significant on a reference/pointer
     * parameter, ignored on a by-value one) -- this project's own
     * overload resolution is already best-effort, and treating a
     * const/non-const mismatch as a hard "not equal" would be a worse,
     * more surprising failure to match an otherwise-obvious overload
     * than simply ignoring the distinction. */
    while (t1->kind == AST_CONST_TYPE) t1 = t1->a;
    while (t2->kind == AST_CONST_TYPE) t2 = t2->a;
    t1 = resolve_typedef_chain(t1);
    t2 = resolve_typedef_chain(t2);
    if (t1 == NULL || t2 == NULL) {
        return t1 == t2;
    }
    if (t1->kind != t2->kind) {
        return 0;
    }
    switch (t1->kind) {
        case AST_IDENT:
            return strcmp(t1->str1, t2->str1) == 0;
        case AST_QUALIFIED_ID: {
            if (t1->list.count != t2->list.count) return 0;
            for (int i = 0; i < t1->list.count; i++) {
                if (strcmp(t1->list.items[i]->str1, t2->list.items[i]->str1) != 0) {
                    return 0;
                }
            }
            return 1;
        }
        case AST_POINTER_TYPE:
        case AST_REFERENCE_TYPE:
            return types_equal(t1->a, t2->a);
        default:
            /* Not a type-position AST kind -- shouldn't happen given what
             * the parser puts in a `type` slot, but fail closed (treat as
             * "not equal") rather than crash or silently match. */
            return 0;
    }
}

static int param_lists_match(const AstList *a, const AstList *b) {
    if (a->count != b->count) {
        return 0;
    }
    for (int i = 0; i < a->count; i++) {
        if (!types_equal(a->items[i]->type, b->items[i]->type)) {
            return 0;
        }
    }
    return 1;
}

/* Renders a type as a short, C-identifier-safe fragment for use inside a
 * mangled name -- e.g. Timer -> "Timer", `Timer *` -> "Timer_ptr",
 * `v32::Timer &` -> "v32_Timer_ref". "::" and "*"/"&" aren't legal in a C
 * identifier, hence the "_"-joining and "_ptr"/"_ref" suffixes instead of
 * just splicing the written syntax in verbatim.
 *
 * Typedef-transparent, same as types_equal (and via the same
 * resolve_typedef_chain): `typedef int MyInt; void f(MyInt);` mangles
 * using "int", not "MyInt". That's deliberate, not a missed rename -- it
 * keeps this consistent with types_equal considering the two signatures
 * identical; mangling one of them by its typedef spelling and the other
 * by its underlying spelling would produce two DIFFERENT C names for
 * what's supposed to be recognized as the same signature. */
static char *type_signature_str(const AstNode *type) {
    type = resolve_typedef_chain(type);
    if (type == NULL) {
        return strdup("void");
    }
    switch (type->kind) {
        case AST_IDENT:
            return strdup(type->str1);
        case AST_QUALIFIED_ID: {
            size_t len = 1;
            for (int i = 0; i < type->list.count; i++) {
                len += strlen(type->list.items[i]->str1) + 1;
            }
            char *out = malloc(len);
            out[0] = '\0';
            for (int i = 0; i < type->list.count; i++) {
                if (i > 0) strcat(out, "_");
                strcat(out, type->list.items[i]->str1);
            }
            return out;
        }
        case AST_POINTER_TYPE:
        case AST_REFERENCE_TYPE: {
            char *inner = type_signature_str(type->a);
            const char *suffix = (type->kind == AST_POINTER_TYPE) ? "ptr" : "ref";
            size_t len = strlen(inner) + strlen(suffix) + 2;
            char *out = malloc(len);
            snprintf(out, len, "%s_%s", inner, suffix);
            free(inner);
            return out;
        }
        case AST_CONST_TYPE:
            /* const is transparent for mangling purposes too, matching
             * types_equal's own identical decision just above in this
             * file -- "const int" and "int" mangle identically
             * (`_int`, not `_unknown` -- the default: case's own
             * fallback, which this would otherwise have fallen into,
             * silently and unhelpfully). Keeping this consistent with
             * types_equal matters: if overload resolution already
             * treats the two as the same parameter type, the mangled
             * name scheme should agree, not produce two different
             * answers to the same question. */
            return type_signature_str(type->a);
        default:
            return strdup("unknown");
    }
}

/* Joins every parameter's type_signature_str with "_", or "void" for an
 * empty parameter list -- matching the C convention of writing `f(void)`
 * for "takes nothing," which reads better in a mangled name than a bare
 * trailing "__". */
static char *param_signature_str(const AstList *params) {
    if (params->count == 0) {
        return strdup("void");
    }
    char *acc = strdup("");
    for (int i = 0; i < params->count; i++) {
        char *part = type_signature_str(params->items[i]->type);
        size_t len = strlen(acc) + strlen(part) + 2;
        char *joined = malloc(len);
        if (i == 0) {
            snprintf(joined, len, "%s", part);
        } else {
            snprintf(joined, len, "%s_%s", acc, part);
        }
        free(acc);
        free(part);
        acc = joined;
    }
    return acc;
}

/*
 * "Class__method__paramSig" for a member, "method__paramSig" for a free
 * function. Destructors get special-cased: the AST stores a destructor's
 * name as e.g. "~Counter" (see func_header in parser.y), and '~' is not a
 * legal C identifier character -- splicing it in unmodified would have
 * produced a mangled name codegen could never actually emit. "dtor" is
 * used in its place; the class name already disambiguates which class's
 * destructor it is, so the (redundant) original class name isn't repeated
 * inside the dtor's own name component.
 */
/* Maps an "operator+"-shaped name (always exactly this spelling -- see
 * operator_symbol in parser.y, which is the only place these strings get
 * constructed) to a C-identifier-safe fragment. Exact-string match, so
 * declaration order doesn't matter (e.g. checking "<=" before "<" isn't
 * required the way a prefix-based scheme would need). Falls back to
 * "op_unknown" for anything unrecognized, which shouldn't be reachable
 * given the grammar only ever builds names from this fixed list -- fails
 * safe (a working but oddly-named mangled name) rather than crashing if
 * that assumption is ever wrong. */
static const char *mangle_operator_symbol(const char *name) {
    const char *sym = name + 8; /* skip past the literal "operator" prefix */
    if (strcmp(sym, "+") == 0) return "op_add";
    if (strcmp(sym, "-") == 0) return "op_sub";
    if (strcmp(sym, "*") == 0) return "op_mul";
    if (strcmp(sym, "/") == 0) return "op_div";
    if (strcmp(sym, "=") == 0) return "op_assign";
    if (strcmp(sym, "!") == 0) return "op_not";
    if (strcmp(sym, "==") == 0) return "op_eq";
    if (strcmp(sym, "!=") == 0) return "op_ne";
    if (strcmp(sym, "<") == 0) return "op_lt";
    if (strcmp(sym, ">") == 0) return "op_gt";
    if (strcmp(sym, "<=") == 0) return "op_le";
    if (strcmp(sym, ">=") == 0) return "op_ge";
    if (strcmp(sym, "+=") == 0) return "op_addeq";
    if (strcmp(sym, "-=") == 0) return "op_subeq";
    if (strcmp(sym, "*=") == 0) return "op_muleq";
    if (strcmp(sym, "/=") == 0) return "op_diveq";
    if (strcmp(sym, "[]") == 0) return "op_index";
    if (strcmp(sym, "()") == 0) return "op_call";
    return "op_unknown";
}

static char *mangle(const char *class_name, const char *method_name, const AstList *params) {
    if (class_name == NULL && strcmp(method_name, "main") == 0) {
        /* Vircon32 requires an actual, unmangled `main` to exist (a
         * whole-cartridge entry point, not a library function -- there's
         * no OS to hand a mangled name to). Only ever special-cased for
         * a top-level free function (class_name == NULL); a METHOD
         * happening to be named "main" is an ordinary method, mangled
         * normally like anything else. See codegen.c's
         * emit_function_header/print_stmt for the matching special
         * cases this alone doesn't finish -- forcing `void` as the
         * printed return type, and stripping any `return expr;`'s value
         * (Vircon32 rejects returning a value from `void main()`, and
         * requiring the C++ source to already declare `void main()`
         * itself, rather than accepting `int main()` and quietly
         * changing its meaning, was ruled out precisely because it's
         * quietly changing behavior -- keeping the C++ source able to
         * read like ordinary C++ was judged more valuable here). */
        return strdup("main");
    }
    const char *name_part = method_name;
    if (method_name[0] == '~') {
        name_part = "dtor";
    } else if (strncmp(method_name, "operator", 8) == 0) {
        name_part = mangle_operator_symbol(method_name);
    }

    char *param_sig = param_signature_str(params);
    char *out;
    if (class_name == NULL) {
        size_t len = strlen(name_part) + 2 + strlen(param_sig) + 1;
        out = malloc(len);
        snprintf(out, len, "%s__%s", name_part, param_sig);
    } else {
        size_t len = strlen(class_name) + 2 + strlen(name_part) + 2 + strlen(param_sig) + 1;
        out = malloc(len);
        snprintf(out, len, "%s__%s__%s", class_name, name_part, param_sig);
    }
    free(param_sig);
    return out;
}

static FuncSemaInfo *make_func_info(const char *class_name, const char *method_name,
                                     const AstList *params, int is_out_of_line) {
    FuncSemaInfo *info = calloc(1, sizeof(FuncSemaInfo));
    info->mangled_name = mangle(class_name, method_name, params);
    info->is_out_of_line = is_out_of_line;
    return info;
}

/* ---- pass 2: attach out-of-line definitions onto their prototype ------ */

static void attach_out_of_line(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];

        if (n->kind == AST_NAMESPACE_DECL) {
            attach_out_of_line(&n->list);
            continue;
        }
        if (n->kind != AST_FUNC_DEF || n->b == NULL) {
            continue; /* an ordinary function/method def, not out-of-line */
        }

        /* n->b is an AST_QUALIFIED_ID: the qualifier chain the definition
         * was written against (e.g. [Player] for `Player::update`, or
         * [v32, Timer] for `v32::Timer::method`). We resolve using only
         * the LAST component's name -- see the registry's doc comment
         * above for why that's a real gap for namespaced classes. */
        AstList *qual_parts = &n->b->list;
        const char *class_name = qual_parts->items[qual_parts->count - 1]->str1;

        AstNode *class_decl = find_class(class_name);
        if (class_decl == NULL) {
            sema_error(n->line, "out-of-line definition of '%s' names unknown class '%s'",
                       n->str1, class_name);
            continue;
        }

        AstNode *target = NULL;
        for (int j = 0; j < class_decl->list.count; j++) {
            AstNode *member = class_decl->list.items[j];
            if (member->kind == AST_FUNC_DECL &&
                member->str1 != NULL && strcmp(member->str1, n->str1) == 0 &&
                param_lists_match(&member->list, &n->list)) {
                /* Full name-AND-signature match: this is what makes
                 * `Vector::Vector(int)` attach to the right constructor
                 * when Vector() and Vector(int) both exist, instead of
                 * both out-of-line ctor definitions colliding onto
                 * whichever same-named prototype happened to come first
                 * in the class body. */
                target = member;
                break;
            }
        }

        if (target == NULL) {
            sema_error(n->line, "no matching declaration for out-of-line definition of '%s' in class '%s'",
                       n->str1, class_name);
            continue;
        }

        target->kind = AST_FUNC_DEF;
        target->a = n->a;
        target->c = n->c;    /* the member-initializer list, if any -- a real
            bug, found and fixed the same round this field was added: only
            ->a (the body) used to be copied here, since ->c didn't exist
            yet when this function was first written. Missing this meant
            resolve_member_init_list (and later, lower.c's own phase 8b)
            always saw c == NULL on the AUTHORITATIVE node this function
            produces (the one actually stored in ClassLayout.methods,
            per the comment below) for every out-of-line constructor
            definition -- exactly this project's own established
            constructor-definition style, so the bug wasn't a rare edge
            case, it silently broke the feature for its own primary,
            intended use. Caught by generating real output and reading
            it, not by inspection alone -- see docs/DESIGN_NOTES.md. */
        target->sema_info = make_func_info(class_name, n->str1, &target->list, 0);

        /* Leave the top-level duplicate in the AST (codegen needs
         * *something* stable to skip over rather than silently vanishing
         * nodes mid-pass) but flag it so codegen knows not to re-emit it
         * -- the authoritative copy is now reachable via the class's
         * member list / ClassLayout.methods. */
        n->sema_info = make_func_info(class_name, n->str1, &n->list, 1);
    }
}

/* ---- pass 3: per-class layout ----------------------------------------- */

/* ---- vtable slot assignment --------------------------------------------
 *
 * Single inheritance only (matches this project's whole scope), which is
 * what keeps this tractable: a derived class has exactly one base, so
 * "this class's vtable" is unambiguously "the base's vtable, with some
 * slots overridden and maybe some new ones appended" -- no diamond
 * inheritance, no virtual-base-class slot-sharing puzzles to solve.
 */

/* Two virtual methods occupy the SAME slot if they have the same name and
 * parameter signature -- EXCEPT destructors, which all share one
 * conceptual slot per class hierarchy regardless of their (necessarily
 * class-specific) literal spelling ("~Base" vs "~Derived" are different
 * strings but the same override relationship in real C++). */
static const char *vtable_slot_key(const AstNode *method) {
    return (method->str1[0] == '~') ? "~" : method->str1;
}

static int vtable_find_slot(const Vtable *vt, const AstNode *candidate) {
    const char *key = vtable_slot_key(candidate);
    for (int i = 0; i < vt->count; i++) {
        AstNode *existing = vt->entries[i].method;
        if (strcmp(vtable_slot_key(existing), key) == 0 &&
            param_lists_match(&existing->list, &candidate->list)) {
            return i;
        }
    }
    return -1;
}

static void vtable_append_slot(Vtable *vt, AstNode *method, AstNode *canonical_method) {
    if (vt->count == vt->capacity) {
        vt->capacity = vt->capacity ? vt->capacity * 2 : 4;
        vt->entries = realloc(vt->entries, sizeof(VtableEntry) * (size_t)vt->capacity);
    }
    vt->entries[vt->count].method = method;
    vt->entries[vt->count].canonical_method = canonical_method;
    vt->entries[vt->count].slot_index = vt->count;
    vt->count++;
}

/* Builds `layout->vtable` for the class `layout` belongs to. Must be
 * called only after layout->base_class_decl (if any) has ALREADY had its
 * own layout -- and therefore its own vtable -- computed; see the call
 * site in compute_layout() for how that's guaranteed regardless of
 * top-to-bottom visitation order. */
static void build_vtable(ClassLayout *layout) {
    Vtable *base_vtable = NULL;
    if (layout->base_class_decl != NULL) {
        ClassLayout *base_layout = (ClassLayout *)layout->base_class_decl->sema_info;
        base_vtable = (base_layout != NULL) ? base_layout->vtable : NULL;
    }

    Vtable *vt = calloc(1, sizeof(Vtable));

    /* Inherit every slot from the base's vtable first, pointing at
     * whichever AstNode currently implements it there -- overwritten
     * below wherever this class actually overrides it. canonical_method
     * is propagated unchanged, since it identifies WHICH slot this is
     * (fixed at whichever class first introduced it), not who currently
     * implements it. */
    if (base_vtable != NULL) {
        for (int i = 0; i < base_vtable->count; i++) {
            vtable_append_slot(vt, base_vtable->entries[i].method, base_vtable->entries[i].canonical_method);
        }
    }

    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        int slot = vtable_find_slot(vt, m);
        if (slot >= 0) {
            /* Overriding an inherited slot. Real C++ treats this as
             * virtual even if 'virtual' isn't repeated on the override --
             * match that here rather than requiring the keyword again at
             * every level of the hierarchy. canonical_method is left
             * untouched here -- it stays pointing at whichever class
             * first introduced this slot, not this override. */
            vt->entries[slot].method = m;
            m->ival = 1;
        } else if (m->ival == 1) {
            /* A genuinely new virtual method (or a virtual destructor
             * introduced at this level, if the base had none) -- this
             * class IS the canonical declarer for this brand-new slot. */
            vtable_append_slot(vt, m, m);
        }
        /* else: an ordinary, non-virtual, non-overriding method -- not
         * part of any vtable at all. */
    }

    if (vt->count == 0) {
        free(vt);
        layout->vtable = NULL;
    } else {
        layout->vtable = vt;
    }
}

static void compute_layout(AstNode *class_decl) {
    if (class_decl->sema_info != NULL) {
        return; /* already computed -- this happens when a class is
                  * visited here as another class's base before
                  * compute_layouts()'s own top-level loop reaches it
                  * directly; see the recursive call below. */
    }

    ClassLayout *layout = calloc(1, sizeof(ClassLayout));
    layout->data_members = ast_list_new();
    layout->methods = ast_list_new();

    /* C++'s default access before any explicit public:/private:/
     * protected: label: private for `class`, public for `struct` --
     * the ONLY real difference between the two keywords, read directly
     * from class_decl->ival (set by class_or_struct_kw in parser.y; see
     * AST_CLASS_DECL's own doc comment in ast.h for the full picture).
     * Every other piece of this project's own class machinery (vtables,
     * constructors, inheritance, access control) applies identically to
     * both -- this one line is the entire implementation of `struct`
     * support beyond parsing the keyword itself. */
    AccessSpec current_access = class_decl->ival ? ACC_PUBLIC : ACC_PRIVATE;

    for (int i = 0; i < class_decl->list.count; i++) {
        AstNode *member = class_decl->list.items[i];
        if (member->kind == AST_ACCESS_SPEC) {
            current_access = member->access;
            continue;
        }
        if (member->kind == AST_VAR_DECL) {
            member->access = current_access;
            ast_list_append(&layout->data_members, member);
        } else if (member->kind == AST_FUNC_DECL || member->kind == AST_FUNC_DEF) {
            member->access = current_access;
            ast_list_append(&layout->methods, member);
            if (member->sema_info == NULL) {
                /* Wasn't touched by attach_out_of_line (either it's an
                 * in-class-only method, or it never got a body at all --
                 * still fine to mangle a bodyless prototype). */
                member->sema_info = make_func_info(class_decl->str1, member->str1, &member->list, 0);
            }
        }
    }

    if (class_decl->str2 != NULL) {
        layout->base_class_decl = find_class(class_decl->str2);
        if (layout->base_class_decl == NULL) {
            sema_error(class_decl->line, "class '%s' inherits from unknown base '%s'",
                       class_decl->str1, class_decl->str2);
        } else {
            /* Ensure the base's layout (and vtable) exists BEFORE this
             * class's own vtable is built, regardless of which order
             * compute_layouts()'s top-level walk happens to visit
             * classes in. Can't cycle: the parser requires a base class
             * to already be a registered TYPE_NAME before it can be
             * named in `opt_base`, so a class can never (even
             * transitively) end up inheriting from itself. */
            compute_layout(layout->base_class_decl);
        }
        /* Deliberately not merging the base's DATA members into
         * data_members/methods here -- see the ClassLayout doc comment
         * in sema.h. Virtual METHODS are handled differently, by
         * build_vtable() below, since a vtable specifically needs
         * inherited slots carried forward. */
    }

    build_vtable(layout);

    class_decl->sema_info = layout;
}

static void compute_layouts(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            compute_layout(n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            compute_layouts(&n->list);
        }
    }
}

/* ---- pass 4: mangle free functions (anything not already handled by a
 * class's layout pass above) --------------------------------------------- */

static void mangle_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            mangle_free_functions(&n->list);
        } else if ((n->kind == AST_FUNC_DECL || n->kind == AST_FUNC_DEF) && n->sema_info == NULL) {
            n->sema_info = make_func_info(NULL, n->str1, &n->list, 0);
        }
    }
}

/* ---- pass 5: access-control enforcement --------------------------------
 *
 * Builds on the access TRACKING already stamped by compute_layout() above
 * (member->access) to actually check whether a given reference to a
 * member is legal from where it occurs -- the "calling context" this
 * project didn't have any notion of before this pass.
 *
 * SCOPE, deliberately bounded: this resolves the STATIC CLASS TYPE of a
 * limited set of expression shapes -- `this`, a function's own
 * parameters, local variables declared via `var_decl` (flat tracking,
 * not properly block-scoped -- see LocalVarType below), member-access
 * chains through any of those, and method-call return types. Anything
 * outside that (arithmetic results, free-function call results, anything
 * whose type this can't pin down) resolves to NULL and is silently
 * SKIPPED, never flagged as either legal or illegal. This is a best-
 * effort diagnostic, not a soundness guarantee: it catches real
 * violations it can definitely see, and says nothing about ones it
 * can't -- which is the honest, appropriately conservative choice here,
 * since a false "this access is fine" is worse than staying silent.
 *
 * Access legality, matching real C++'s common-case rules (a
 * simplification -- see the note on protected access below):
 *   - public:    always legal.
 *   - private:   legal only from the EXACT class that declared it (never
 *                a derived class, even though the member is inherited).
 *   - protected: legal from the declaring class OR any (transitive)
 *                derived class.
 *
 * NOT implemented: the C++ standard's more restrictive rule that a
 * derived class can only access an INHERITED protected member through an
 * object of ITS OWN (or further-derived) type, not through a
 * base-typed reference/pointer even from within a derived class's own
 * method. This project allows the simpler "derived class can touch any
 * protected member of any base" rule instead.
 */

LocalVarType *find_local(LocalVarType *locals, const char *name) {
    for (LocalVarType *lv = locals; lv != NULL; lv = lv->next) {
        if (strcmp(lv->name, name) == 0) return lv;
    }
    return NULL;
}

/* Resolves a type AST node (AST_IDENT/AST_QUALIFIED_ID, possibly wrapped
 * in AST_POINTER_TYPE/AST_REFERENCE_TYPE, possibly a typedef) down to the
 * AST_CLASS_DECL it names, or NULL if it doesn't name a registered class
 * at all (a builtin type, an unregistered/unknown name, ...). Pointers
 * and references are treated as resolving to the SAME class as their
 * pointee/referent -- accessing a member through `Foo*`/`Foo&` follows
 * the same rules as through a plain `Foo`, matching real C++. */
AstNode *type_to_class(const AstNode *type) {
    type = resolve_typedef_chain(type);
    if (type == NULL) return NULL;
    switch (type->kind) {
        case AST_POINTER_TYPE:
        case AST_REFERENCE_TYPE:
        case AST_CONST_TYPE:
            return type_to_class(type->a);
        case AST_IDENT:
            return find_class(type->str1);
        case AST_QUALIFIED_ID:
            if (type->list.count == 0) return NULL;
            return find_class(type->list.items[type->list.count - 1]->str1);
        default:
            return NULL;
    }
}

/* Searches `class_decl`'s own members first, then walks up base_class_decl
 * (single inheritance, so this is a simple chain, not a search tree),
 * looking for a member named `name`. Name-only match -- see the TODO
 * elsewhere in this file about overload-aware lookup; if a name has
 * multiple overloads with DIFFERING access levels (unusual, but legal
 * C++), this returns whichever one compute_layout() happened to list
 * first, not necessarily the one actually being called. Sets *owner_out
 * to the class that ACTUALLY declared the returned member (which may be
 * an ancestor of `class_decl`, not class_decl itself) -- callers need
 * this to distinguish "same class" from "derived class" for the private-
 * vs-protected legality check. */
AstNode *find_member_in_hierarchy(AstNode *class_decl, const char *name, AstNode **owner_out) {
    while (class_decl != NULL) {
        ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
        if (layout != NULL) {
            for (int i = 0; i < layout->data_members.count; i++) {
                if (strcmp(layout->data_members.items[i]->str1, name) == 0) {
                    if (owner_out != NULL) *owner_out = class_decl;
                    return layout->data_members.items[i];
                }
            }
            for (int i = 0; i < layout->methods.count; i++) {
                if (strcmp(layout->methods.items[i]->str1, name) == 0) {
                    if (owner_out != NULL) *owner_out = class_decl;
                    return layout->methods.items[i];
                }
            }
        }
        class_decl = (layout != NULL) ? layout->base_class_decl : NULL;
    }
    return NULL;
}

/* Is `class_decl` the same as `ancestor`, or (transitively) derived from
 * it? Used for the protected-access rule. */
static int is_same_or_descendant(AstNode *class_decl, AstNode *ancestor) {
    while (class_decl != NULL) {
        if (class_decl == ancestor) return 1;
        ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
        class_decl = (layout != NULL) ? layout->base_class_decl : NULL;
    }
    return 0;
}

static void check_member_access(int line, const char *member_name, const AstNode *member,
                                 AstNode *owner, AstNode *current_class) {
    if (member->access == ACC_PUBLIC) {
        return;
    }
    if (member->access == ACC_PRIVATE) {
        if (current_class == owner) return;
        sema_error(line, "'%s' is a private member of class '%s' and cannot be accessed here",
                   member_name, owner->str1);
        return;
    }
    /* ACC_PROTECTED */
    if (current_class != NULL && is_same_or_descendant(current_class, owner)) return;
    sema_error(line, "'%s' is a protected member of class '%s' and cannot be accessed here",
               member_name, owner->str1);
}

/* The core of this pass's expression handling: infers which class (if
 * any) an expression's static type resolves to. Returns NULL for
 * anything outside the deliberately-bounded scope described above --
 * NULL means "unknown", not "not a class", so callers must treat it as
 * "nothing to check" rather than an error. */
/* The general form: infers an expression's static TYPE (not just "which
 * class", though that remains the most common case that matters
 * elsewhere in this file) -- literals get a synthesized builtin-keyword
 * type node, `this` gets the enclosing class's name, and everything else
 * follows the same identifier/member/call resolution the access-control
 * pass already relied on (originally written directly inside what's now
 * just a thin wrapper, resolve_expr_class, kept for the call sites that
 * only ever wanted a class and predate this generalization).
 *
 * Note on AST_CALL here specifically: this does NOT consult a call's own
 * already-computed CallResolution (if any) -- it re-derives a return type
 * via first-found-by-name lookup, the same approximation access control
 * always used. That means a nested call used as an argument to an outer,
 * overloaded call (`outer(inner())`) has its type inferred from
 * whichever overload of `inner` happens to be found first, not
 * necessarily the one `inner`'s own call actually resolved to. Narrow,
 * rare case (return-type-based evidence feeding an overload decision on
 * an enclosing call) -- worth knowing about, not worth the pass-ordering
 * dependency avoiding it would require (resolve_call, further down, is
 * itself what populates a call's CallResolution, so relying on it here
 * would mean this function's correctness depends on being called AFTER
 * resolution has already happened for every nested call, which the
 * single-pass tree walk doesn't guarantee in general). */
AstNode *infer_expr_type(const AstNode *expr, AstNode *current_class, LocalVarType *locals) {
    if (expr == NULL) return NULL;
    switch (expr->kind) {
        case AST_INT_LIT: return ast_ident("int", expr->line);
        case AST_FLOAT_LIT: return ast_ident("float", expr->line);
        case AST_BOOL_LIT: return ast_ident("bool", expr->line);
        case AST_CHAR_LIT: return ast_ident("char", expr->line);
        case AST_THIS:
            return (current_class != NULL) ? ast_ident(current_class->str1, expr->line) : NULL;
        case AST_IDENT: {
            LocalVarType *lv = find_local(locals, expr->str1);
            if (lv != NULL) return lv->type;
            if (current_class != NULL) {
                AstNode *owner = NULL;
                AstNode *member = find_member_in_hierarchy(current_class, expr->str1, &owner);
                if (member != NULL && member->kind == AST_VAR_DECL) return member->type;
            }
            return NULL;
        }
        case AST_MEMBER: {
            AstNode *obj_class = type_to_class(infer_expr_type(expr->a, current_class, locals));
            if (obj_class == NULL) return NULL;
            AstNode *owner = NULL;
            AstNode *member = find_member_in_hierarchy(obj_class, expr->str2, &owner);
            if (member == NULL || member->kind != AST_VAR_DECL) return NULL;
            return member->type;
        }
        case AST_NEW: {
            /* `new T(...)`'s type is "pointer to T" -- reuses expr->type
             * (the type node the parser already built) as the pointee,
             * the same way every other case here reuses an existing type
             * node by reference rather than deep-copying it. */
            AstNode *ptr = ast_new(AST_POINTER_TYPE, expr->line);
            ptr->a = expr->type;
            return ptr;
        }
        case AST_CALL: {
            const AstNode *callee = expr->a;
            AstNode *owner = NULL;
            AstNode *method = NULL;
            if (callee != NULL && callee->kind == AST_MEMBER) {
                AstNode *obj_class = type_to_class(infer_expr_type(callee->a, current_class, locals));
                if (obj_class == NULL) return NULL;
                method = find_member_in_hierarchy(obj_class, callee->str2, &owner);
            } else if (callee != NULL && callee->kind == AST_IDENT && current_class != NULL) {
                /* Unqualified call inside a method -- could be an
                 * implicit this->method(). Free-function calls (when
                 * current_class is NULL, or the name isn't a member)
                 * aren't resolved here at all; that's what
                 * resolve_call()'s free-function registry is for, kept
                 * deliberately separate from this type-inference helper. */
                method = find_member_in_hierarchy(current_class, callee->str1, &owner);
            }
            if (method == NULL || (method->kind != AST_FUNC_DECL && method->kind != AST_FUNC_DEF)) return NULL;
            return method->type;
        }
        case AST_SUBSCRIPT: {
            /* `arr[i]`'s type is arr's ELEMENT type, whether `arr`
             * itself is an actual array (AST_ARRAY_TYPE -- unwrap to
             * a->a) or a pointer being indexed pointer-arithmetic-style
             * (AST_POINTER_TYPE -- same unwrap, ordinary C). Added
             * alongside AST_ARRAY_TYPE itself, since without it every
             * subscript expression's type silently fell through to the
             * default "unknown" case below -- harmless today (this
             * project doesn't yet support arrays of class objects, so
             * nothing currently exercises this), but a real correctness
             * gap waiting to matter the moment it does. */
            const AstNode *base_type = resolve_typedef_chain(infer_expr_type(expr->a, current_class, locals));
            if (base_type != NULL && (base_type->kind == AST_ARRAY_TYPE || base_type->kind == AST_POINTER_TYPE)) {
                return base_type->a;
            }
            return NULL;
        }
        case AST_UNOP: {
            /* Same reasoning as AST_SUBSCRIPT just above -- without
             * this, `*p` and `&x` both silently fell through to
             * "unknown" here, discovered when a dereferenced pointer
             * passed as a reference-parameter argument
             * (`getArea(*shape)`) went unwrapped by
             * address_of_if_needed (lower.c) because THIS function
             * told it "unknown type, don't guess" for `*shape` --
             * exactly the same failure mode AST_SUBSCRIPT's own
             * comment already describes, just a different expression
             * kind hitting it. `deref` (`*p`) unwraps one level of
             * AST_POINTER_TYPE (the pointee type) -- NULL if the
             * operand's own type wasn't a pointer (best-effort: don't
             * guess a type for what's already invalid code). `addr`
             * (`&x`) does the reverse: wraps the operand's own type in
             * a fresh AST_POINTER_TYPE. Every other AST_UNOP kind this
             * project produces (`neg`, `not`, `bitnot`, `preinc`, ...)
             * falls through to the default "unknown" case below,
             * unchanged -- none of them change an expression's own
             * static type from its operand's. */
            if (expr->str1 != NULL && strcmp(expr->str1, "deref") == 0) {
                const AstNode *operand_type = infer_expr_type(expr->a, current_class, locals);
                return (operand_type != NULL && operand_type->kind == AST_POINTER_TYPE)
                       ? operand_type->a : NULL;
            }
            if (expr->str1 != NULL && strcmp(expr->str1, "addr") == 0) {
                AstNode *operand_type = infer_expr_type(expr->a, current_class, locals);
                if (operand_type == NULL) return NULL;
                AstNode *ptr = ast_new(AST_POINTER_TYPE, expr->line);
                ptr->a = operand_type;
                return ptr;
            }
            return NULL;
        }
        case AST_TERNARY: {
            /* A ternary's own static type is whichever of its two
             * branches actually resolves -- real C++ requires both
             * branches to have a common type and would pick the more
             * general of the two, but this project's own best-effort
             * philosophy (matching every other case here) is simpler:
             * try the then-branch first, fall back to the else-branch
             * only if the then-branch's own type couldn't be
             * determined at all. Added specifically so lower.c's
             * ternary-hoisting phase (10) can declare a correctly-typed
             * temporary for a ternary nested somewhere a direct if/else
             * rewrite can't reach (a call argument, an arbitrary
             * subexpression) -- without this, EVERY such ternary fell
             * through to the default "unknown" case below. */
            AstNode *t = infer_expr_type(expr->b, current_class, locals);
            if (t != NULL) return t;
            return infer_expr_type(expr->c, current_class, locals);
        }
        default:
            return NULL;
    }
}

/* Thin wrapper kept for the (access-control) call sites that only ever
 * wanted "is this expression's type a class, and if so which one" --
 * everything they relied on now lives in the more general
 * infer_expr_type above. */
AstNode *resolve_expr_class(const AstNode *expr, AstNode *current_class, LocalVarType *locals) {
    return type_to_class(infer_expr_type(expr, current_class, locals));
}

const AstNode *find_declaring_class(const AstNode *class_decl, const AstNode *target_method) {
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

/* Walks every statement/expression reachable from `n`, performing the
 * access check wherever a member is actually referenced (explicitly via
 * `.`/`->`, or implicitly via a bare identifier that resolves to an
 * INHERITED member -- e.g. a derived class's method naming a base
 * class's private data member directly). Also accumulates local variable
 * declarations into `*locals` as they're encountered, so later
 * statements can resolve references to them -- see LocalVarType's doc
 * comment for the block-scoping caveat. */
/* ---- call-site overload resolution --------------------------------------
 *
 * Picks which specific FUNC_DECL/FUNC_DEF a call expression refers to,
 * among however many same-named candidates exist, by matching argument
 * COUNT always and argument TYPES (via infer_expr_type + types_equal)
 * whenever every argument's type can be confidently determined.
 *
 * Deliberately best-effort, same philosophy as access control: if even
 * one argument's type can't be pinned down and there's more than one
 * candidate, this makes NO attempt to guess -- it silently leaves the
 * call unresolved rather than risk a wrong pick. The single-candidate
 * case is the one exception that doesn't need argument types at all
 * (see resolve_call's comment on why that matters in practice).
 *
 * NOT implemented: any notion of implicit conversions (int-to-float
 * promotion, a class-to-base-class pointer conversion, ...) -- argument
 * types must match a candidate's parameter types EXACTLY (typedef-
 * transparent, via the same types_equal used elsewhere in this file, but
 * nothing more permissive than that). A call that real C++ would resolve
 * via an implicit conversion may report "no matching overload" here
 * instead.
 */

/* Collects every same-named FUNC_DECL/FUNC_DEF member candidate for a
 * call, matching real C++ name-hiding: if `class_decl` itself declares
 * ANY overload of `name` at all, only THOSE are candidates -- a derived
 * class's own declarations hide a base's same-named ones entirely
 * (no `using`-declaration support to bring them back). Only falls
 * through to search the base class when `class_decl` has NONE. */
static void collect_method_candidates(AstNode *class_decl, const char *name,
                                       AstNode ***out, int *out_count, int *out_cap) {
    while (class_decl != NULL) {
        ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
        int found_here = 0;
        if (layout != NULL) {
            for (int i = 0; i < layout->methods.count; i++) {
                AstNode *m = layout->methods.items[i];
                if (strcmp(m->str1, name) == 0) {
                    if (*out_count == *out_cap) {
                        *out_cap = *out_cap ? *out_cap * 2 : 4;
                        *out = realloc(*out, sizeof(AstNode *) * (size_t)(*out_cap));
                    }
                    (*out)[(*out_count)++] = m;
                    found_here = 1;
                }
            }
        }
        if (found_here) break; /* name hiding: stop at the first class that declares this name at all */
        class_decl = (layout != NULL) ? layout->base_class_decl : NULL;
    }
}

void collect_free_function_candidates(const char *name, AstNode ***out, int *out_count, int *out_cap) {
    for (FreeFuncRegEntry *e = g_free_func_registry; e != NULL; e = e->next) {
        if (strcmp(e->func->str1, name) == 0) {
            if (*out_count == *out_cap) {
                *out_cap = *out_cap ? *out_cap * 2 : 4;
                *out = realloc(*out, sizeof(AstNode *) * (size_t)(*out_cap));
            }
            (*out)[(*out_count)++] = e->func;
        }
    }
}

/* The name-matching/arity/type-matching/diagnostic core shared by every
 * kind of "which overload does this refer to" resolution in this
 * project: an explicit call (resolve_call), a constructor invoked via
 * `new T(args)` (resolve_new_expr), and -- as of this round, moved here
 * from lower.c specifically so it gets the same diagnostics and access-
 * control treatment a regular call gets -- an operator used via natural
 * syntax like `a + b` (resolve_operator_use). `site` is whichever node
 * the resulting CallResolution should be attached to (an AST_CALL, an
 * AST_NEW, or the BinOp/Assign/Unop/Subscript node itself for an
 * operator); `args`/`arg_count` are the actual argument EXPRESSIONS,
 * kept as an explicit array rather than assumed to live in `site->list`
 * because an operator's operands live in `->a`/`->b`, not a list. `name`
 * is used only for diagnostic text. */
static void resolve_overload_generic(AstNode *site, const char *name, AstNode **candidates, int count,
                                      AstNode **args, int arg_count,
                                      AstNode *current_class, LocalVarType *locals) {
    if (count == 0) {
        free(candidates);
        return; /* nothing named this at all -- not this pass's job to
                    diagnose "no such function", only to resolve overloads
                    among candidates that DO exist by that name */
    }

    if (count == 1) {
        /* Only one candidate exists at all -- no real overload ambiguity
         * to resolve, so this doesn't need every argument's type known.
         * That matters: most calls in an ordinary program aren't
         * overloaded at all, and requiring full argument-type resolution
         * even for those would make this pass far less useful than it
         * should be. Still worth checking arity even here, though --
         * real C++ would reject a call with the wrong number of
         * arguments even when there's only one candidate to consider. */
        if (candidates[0]->list.count == arg_count) {
            CallResolution *cr = calloc(1, sizeof(CallResolution));
            cr->resolved_target = candidates[0];
            site->sema_info = cr;
        } else {
            sema_error(site->line, "'%s' expects %d argument(s), but %d were given",
                       name, candidates[0]->list.count, arg_count);
        }
        free(candidates);
        return;
    }

    /* Genuinely overloaded: need every argument's type resolved to
     * confidently pick among candidates. */
    AstNode **arg_types = calloc((size_t)arg_count, sizeof(AstNode *));
    int all_known = 1;
    for (int i = 0; i < arg_count; i++) {
        arg_types[i] = infer_expr_type(args[i], current_class, locals);
        if (arg_types[i] == NULL) all_known = 0;
    }

    if (!all_known) {
        /* Best-effort: can't confidently disambiguate without knowing
         * every argument's type, so this doesn't guess -- silently
         * skipped rather than risking a false error or a wrong pick. */
        free(arg_types);
        free(candidates);
        return;
    }

    AstNode *match = NULL;
    int match_count = 0;
    for (int i = 0; i < count; i++) {
        AstNode *cand = candidates[i];
        if (cand->list.count != arg_count) continue;
        int ok = 1;
        for (int j = 0; j < cand->list.count; j++) {
            if (!types_equal(cand->list.items[j]->type, arg_types[j])) { ok = 0; break; }
        }
        if (ok) { match = cand; match_count++; }
    }

    if (match_count == 1) {
        CallResolution *cr = calloc(1, sizeof(CallResolution));
        cr->resolved_target = match;
        site->sema_info = cr;
    } else if (match_count == 0) {
        sema_error(site->line, "no matching overload of '%s' for this call", name);
    } else {
        sema_error(site->line, "call to '%s' is ambiguous between %d matching overloads", name, match_count);
    }

    free(arg_types);
    free(candidates);
}

static void resolve_call(AstNode *call, AstNode *current_class, LocalVarType *locals) {
    const AstNode *callee = call->a;
    if (callee == NULL) return;

    const char *name;
    AstNode **candidates = NULL;
    int count = 0, cap = 0;

    if (callee->kind == AST_MEMBER) {
        AstNode *obj_class = resolve_expr_class(callee->a, current_class, locals);
        if (obj_class == NULL) return; /* can't enumerate candidates without knowing the object's class */
        name = callee->str2;
        collect_method_candidates(obj_class, name, &candidates, &count, &cap);
    } else if (callee->kind == AST_IDENT) {
        name = callee->str1;
        if (current_class != NULL) {
            collect_method_candidates(current_class, name, &candidates, &count, &cap);
        }
        if (count == 0) {
            /* Real C++ lookup order: member functions (just tried above)
             * take precedence over free functions of the same name when
             * called unqualified from inside a method. */
            collect_free_function_candidates(name, &candidates, &count, &cap);
        }
    } else {
        return; /* other callee shapes (e.g. a call through a computed
                    function pointer) not handled */
    }

    resolve_overload_generic(call, name, candidates, count, call->list.items, call->list.count,
                              current_class, locals);
}

/* Resolves which constructor overload a `new T(args)` expression refers
 * to -- a constructor is exactly a same-named ("ClassName") member of
 * the class being allocated, so collect_method_candidates already finds
 * these correctly (constructors are stored in ClassLayout.methods like
 * any other method). Reuses the exact same matching/diagnostic core a
 * regular call uses, so an arity/type mismatch here gets the same
 * "no matching overload" treatment, not a silent fallback to whatever a
 * lowering-time placeholder might guess. If `T` has no declared
 * constructor at all (count == 0), this is silently a no-op -- not this
 * pass's job to diagnose "no such constructor"; an implicitly
 * default-constructible type is a perfectly ordinary case. */
static void resolve_new_expr(AstNode *new_node, AstNode *current_class, LocalVarType *locals) {
    AstNode *cls = type_to_class(new_node->type);
    if (cls == NULL) return; /* not a registered class at all */

    AstNode **candidates = NULL;
    int count = 0, cap = 0;
    collect_method_candidates(cls, cls->str1, &candidates, &count, &cap);

    resolve_overload_generic(new_node, cls->str1, candidates, count,
                              new_node->list.items, new_node->list.count, current_class, locals);
}

/* Resolves `func`'s own member-initializer list (func->c, an
 * AST_MEMBER_INIT_LIST -- NULL if none was written at all, the ordinary
 * case for every method that isn't a constructor written with one).
 *
 * A member-initializer list on anything OTHER than a constructor (an
 * ordinary method, a destructor) is itself an error -- real C++ rejects
 * this too, and the parser doesn't (see AST_MEMBER_INIT's own doc
 * comment in ast.h for why that split exists).
 *
 * Each entry's own str1 is checked against, in order:
 *   1. The class's own DIRECT base class's name -- base-class delegation.
 *      Resolved against the base's own constructor overloads through the
 *      exact same resolve_overload_generic core resolve_call/
 *      resolve_new_expr both already go through, attaching a
 *      CallResolution* to the AST_MEMBER_INIT node's own sema_info
 *      exactly like those two attach one to their own site node --
 *      lower.c's phase 8b reads this.
 *   2. An actual, declared data member's own name. If that member's own
 *      type is a BARE class type (not a pointer/reference TO one --
 *      `Foo x;`, not `Foo *x;`/`Foo &x;`, which are ordinary pointer/
 *      reference values needing no constructor call at all), this is
 *      "not yet supported" -- a class-typed member's own constructor
 *      invocation is real, separate complexity this project doesn't
 *      support anywhere yet, not something to fold into this round.
 *      Otherwise (a primitive, or a pointer/reference TO a class),
 *      requires exactly one argument (matching real C++'s own
 *      direct-initialization rule for a non-class member -- `: x(a, b)`
 *      for a plain `int x` isn't legal C++ either) and marks the entry
 *      resolved via `entry->ival = 1` -- a lighter-weight marker than a
 *      CallResolution, deliberately: there's no overload to resolve for
 *      a primitive assignment, just a name and one argument expression,
 *      both already on the node. lower.c's own new phase reads this
 *      flag directly, distinguishing a resolved member-field entry
 *      (ival == 1, sema_info == NULL) from a resolved base-class
 *      delegation (ival == 0, sema_info != NULL) from anything
 *      unresolved (neither set).
 *   3. Anything else -- "not a base class or member" error.
 * A name matching BOTH (unusual, but syntactically legal C++ -- a data
 * member that happens to share its own base class's name) is treated as
 * base-class delegation, checked first -- matching real C++'s own rule
 * that a member-initializer-list entry naming a direct base is ALWAYS
 * base-class initialization, never a member with the same name. */
static void resolve_member_init_list(AstNode *func, AstNode *current_class, LocalVarType *locals) {
    if (func->c == NULL) return; /* no ": ..." was written -- ordinary case */

    if (current_class == NULL || strcmp(func->str1, current_class->str1) != 0) {
        sema_error(func->c->line, "a member-initializer list is only allowed on a constructor");
        return;
    }

    ClassLayout *layout = (ClassLayout *)current_class->sema_info;
    AstNode *base_class = (layout != NULL) ? layout->base_class_decl : NULL;

    /* Detect a duplicate name (the same field or base named more than
     * once in one list) BEFORE the main resolution loop below -- real
     * C++ treats this as ill-formed (a hard compile error: "multiple
     * initializations given for X"), not something to silently accept
     * and let whichever entry lower.c's own phase 8a happens to find
     * first quietly win. Reported once per duplicate OCCURRENCE (the
     * second, third, ... time a name repeats), each at its own line --
     * `: x(a), x(b), x(c)` gets two errors, one for the second x and
     * one for the third, not just one. Doesn't skip resolving a
     * duplicate entry afterward -- the diagnostic was the missing
     * piece, not the resolution itself, which was already harmless
     * (just silent) before this check existed. */
    for (int i = 0; i < func->c->list.count; i++) {
        for (int j = 0; j < i; j++) {
            if (strcmp(func->c->list.items[i]->str1, func->c->list.items[j]->str1) == 0) {
                sema_error(func->c->list.items[i]->line,
                           "'%s' is initialized more than once in this constructor's initializer list",
                           func->c->list.items[i]->str1);
                break;
            }
        }
    }

    for (int i = 0; i < func->c->list.count; i++) {
        AstNode *entry = func->c->list.items[i];

        if (base_class != NULL && strcmp(entry->str1, base_class->str1) == 0) {
            AstNode **candidates = NULL;
            int count = 0, cap = 0;
            collect_method_candidates(base_class, base_class->str1, &candidates, &count, &cap);
            resolve_overload_generic(entry, base_class->str1, candidates, count,
                                      entry->list.items, entry->list.count, current_class, locals);
            continue;
        }

        AstNode *member_field = NULL;
        if (layout != NULL) {
            for (int j = 0; j < layout->data_members.count; j++) {
                if (strcmp(layout->data_members.items[j]->str1, entry->str1) == 0) {
                    member_field = layout->data_members.items[j];
                    break;
                }
            }
        }

        if (member_field != NULL) {
            const AstNode *resolved_type = resolve_typedef_chain(member_field->type);
            int is_bare_class_type = (resolved_type != NULL && resolved_type->kind == AST_IDENT
                                       && find_class(resolved_type->str1) != NULL);
            if (is_bare_class_type) {
                sema_error(entry->line,
                           "member initializers for class-typed fields aren't supported yet -- "
                           "'%s' has a class type", entry->str1);
            } else if (entry->list.count != 1) {
                sema_error(entry->line, "member initializer for '%s' takes exactly one argument",
                           entry->str1);
            } else {
                entry->ival = 1; /* resolved: a valid, single-argument primitive
                    member-field initializer -- see this function's own doc
                    comment above for what this flag means to lower.c */
            }
        } else {
            sema_error(entry->line, "'%s' is not a base class or member of '%s'",
                       entry->str1, current_class->str1);
        }
    }
}

static const char *binop_operator_name(const char *op) {
    if (strcmp(op, "+") == 0) return "operator+";
    if (strcmp(op, "-") == 0) return "operator-";
    if (strcmp(op, "*") == 0) return "operator*";
    if (strcmp(op, "/") == 0) return "operator/";
    if (strcmp(op, "==") == 0) return "operator==";
    if (strcmp(op, "!=") == 0) return "operator!=";
    if (strcmp(op, "<") == 0) return "operator<";
    if (strcmp(op, ">") == 0) return "operator>";
    if (strcmp(op, "<=") == 0) return "operator<=";
    if (strcmp(op, ">=") == 0) return "operator>=";
    return NULL; /* &&/|| aren't in this project's supported operator_symbol
                  * list at all (see parser.y), so they're never overloadable
                  * here -- always a plain built-in BinOp. */
}

static const char *assign_operator_name(const char *op) {
    if (strcmp(op, "=") == 0) return "operator=";
    if (strcmp(op, "+=") == 0) return "operator+=";
    if (strcmp(op, "-=") == 0) return "operator-=";
    if (strcmp(op, "*=") == 0) return "operator*=";
    if (strcmp(op, "/=") == 0) return "operator/=";
    return NULL;
}

static const char *unop_operator_name(const char *op) {
    /* Only "neg" (unary minus) and "!" correspond to operators this
     * project's grammar actually supports overloading (see
     * operator_symbol in parser.y) -- "~", "addr", "deref", and the
     * pre/post ++/-- forms have no overload syntax to have matched, so
     * they're never rewritten here, always plain built-in AST_UNOP. */
    if (strcmp(op, "neg") == 0) return "operator-";
    if (strcmp(op, "!") == 0) return "operator!";
    return NULL;
}

/* Resolves natural operator syntax (`a + b`, `a == b`, `v[i]`) against a
 * class's declared `operatorX` overloads -- MOVED HERE from lower.c this
 * round specifically so it gets the same treatment a regular call
 * already gets: a genuine mismatch is now a real sema_error() (not
 * silently left as a plain built-in operation the way lowering-time
 * resolution had to, having no diagnostic mechanism reachable from
 * there), and a resolved MEMBER operator now goes through
 * check_member_access() exactly like an explicit member call would --
 * closing an access-control gap that existed for as long as operator
 * resolution has existed in this project.
 *
 * Member operators take precedence over a free-function operator of the
 * same name, matching how collect_method_candidates/resolve_call already
 * treat member-vs-free precedence for an ordinary unqualified call: if
 * `obj_class` declares ANY overload of `op_name` at all, resolution
 * commits to that candidate set and reports "no matching overload" on a
 * mismatch rather than silently falling through to check for a
 * free-function version too. This project doesn't implement argument-
 * dependent lookup, so that's a reasoned simplification, not an
 * oversight -- and it's the more diagnostic-friendly choice besides.
 *
 * `lhs_or_operand` is never itself counted as an explicit argument: for
 * a member match it's the implicit receiver (like `this`); only for the
 * free-function fallback does it become arg[0], since a free function
 * has no implicit receiver at all. */
static void resolve_operator_use(AstNode *node, const char *op_name, AstNode *lhs_or_operand,
                                  AstNode *rhs_or_null, AstNode *current_class, LocalVarType *locals) {
    if (op_name == NULL) return;

    AstNode *obj_class = resolve_expr_class(lhs_or_operand, current_class, locals);
    if (obj_class == NULL) return; /* not class-typed -- a plain built-in
        op on primitives, nothing for this pass to resolve at all */

    AstNode *member_args[1];
    int member_arg_count = 0;
    if (rhs_or_null != NULL) member_args[member_arg_count++] = rhs_or_null;

    AstNode **member_candidates = NULL;
    int member_count = 0, member_cap = 0;
    collect_method_candidates(obj_class, op_name, &member_candidates, &member_count, &member_cap);

    if (member_count > 0) {
        resolve_overload_generic(node, op_name, member_candidates, member_count,
                                  member_args, member_arg_count, current_class, locals);
        CallResolution *cr = (CallResolution *)node->sema_info;
        if (cr != NULL && cr->resolved_target != NULL) {
            cr->is_member = 1;
            AstNode *owner = NULL;
            find_member_in_hierarchy(obj_class, op_name, &owner); /* just to
                recover `owner`; name-hiding guarantees this is the same
                class collect_method_candidates already committed to */
            check_member_access(node->line, op_name, cr->resolved_target, owner, current_class);
        }
        return; /* a member overload exists by this name at all -- commit
            to that candidate set, same precedence rule resolve_call uses */
    }

    /* No member operator at all -- fall back to a free-function one. */
    AstNode *free_args[2];
    int free_arg_count = 0;
    free_args[free_arg_count++] = lhs_or_operand;
    if (rhs_or_null != NULL) free_args[free_arg_count++] = rhs_or_null;

    AstNode **free_candidates = NULL;
    int free_count = 0, free_cap = 0;
    collect_free_function_candidates(op_name, &free_candidates, &free_count, &free_cap);

    if (free_count == 0) {
        free(free_candidates);
        return; /* no operator overload declared anywhere applicable at
            all -- leave this as a plain built-in operation */
    }

    resolve_overload_generic(node, op_name, free_candidates, free_count,
                              free_args, free_arg_count, current_class, locals);
    if (node->sema_info != NULL) {
        ((CallResolution *)node->sema_info)->is_member = 0;
    }
}

/* Tracks whether check_node's own walk is currently inside a loop body,
 * for break/continue validity checking -- real C++/C both reject either
 * appearing outside a loop, and the generated C would fail to compile
 * on this exact point too if this project let one through. File-local,
 * not threaded through check_node's own parameter list or exposed via
 * driver.h -- this walk is single-threaded and strictly depth-first, so
 * a global incremented/decremented around a loop body's own recursive
 * call is sufficient and far simpler than threading a new parameter
 * through every existing call site in this function. */
static int g_sema_loop_depth = 0;
static int g_sema_switch_depth = 0; /* same "global, incremented/decremented
    around the relevant body" approach as g_sema_loop_depth, tracked
    separately since `break` is valid inside EITHER a loop or a switch
    (whichever is innermost) while `continue` is valid ONLY inside a
    loop -- a single shared counter couldn't distinguish the two. */

static void check_node(AstNode *n, AstNode *current_class, LocalVarType **locals) {
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) check_node(n->list.items[i], current_class, locals);
            break;
        case AST_IF:
            check_node(n->a, current_class, locals);
            check_node(n->b, current_class, locals);
            check_node(n->c, current_class, locals);
            break;
        case AST_LABEL:
            /* The labeled statement itself (n->a) needs exactly the
             * same walk any other statement in this position would --
             * a label is transparent to everything this function
             * checks (break/continue validity, loop depth, variable
             * declarations, call resolution, ...), it just marks a
             * jump target on top of an otherwise ordinary statement. */
            check_node(n->a, current_class, locals);
            break;
        case AST_WHILE:
            check_node(n->a, current_class, locals);
            g_sema_loop_depth++;
            check_node(n->b, current_class, locals);
            g_sema_loop_depth--;
            break;
        case AST_FOR:
            check_node(n->a, current_class, locals);
            check_node(n->b, current_class, locals);
            check_node(n->c, current_class, locals);
            g_sema_loop_depth++;
            check_node(n->d, current_class, locals);
            g_sema_loop_depth--;
            break;
        case AST_SWITCH:
            /* Only g_sema_switch_depth changes -- g_sema_loop_depth is
             * deliberately left untouched, so `continue` inside this
             * switch's own body still resolves against whatever loop
             * (if any) already encloses the switch, exactly real C's
             * own rule (continue always means "the nearest enclosing
             * LOOP", never a switch, even when a switch is closer). */
            check_node(n->a, current_class, locals); /* discriminant */
            g_sema_switch_depth++;
            for (int i = 0; i < n->list.count; i++) check_node(n->list.items[i], current_class, locals);
            g_sema_switch_depth--;
            break;
        case AST_CASE:
            check_node(n->a, current_class, locals); /* case value expr */
            break;
        case AST_DEFAULT:
            break; /* no fields -- a bare label */
        case AST_BREAK:
            /* Valid inside EITHER a loop or a switch, whichever is
             * innermost -- real C's own rule. (Which one it actually
             * exits, for the purposes of lower.c's own destructor-
             * invocation phase, is a SEPARATE question that phase
             * answers itself via its own break_boundary/loop_boundary
             * distinction -- this check only needs to know "is break
             * legal here at all", not "which construct does it target".) */
            if (g_sema_loop_depth == 0 && g_sema_switch_depth == 0) {
                sema_error(n->line, "'break' statement not within a loop or switch");
            }
            break;
        case AST_CONTINUE:
            /* Loop-only, unlike AST_BREAK above -- a switch alone (with
             * no enclosing loop) does NOT make continue valid, matching
             * real C exactly. */
            if (g_sema_loop_depth == 0) {
                sema_error(n->line, "'continue' statement not within a loop");
            }
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
        case AST_DELETE:
            check_node(n->a, current_class, locals);
            break;
        case AST_CAST:
            /* Recursion into n->a added here specifically for a USER-
             * WRITTEN cast ("(int)someFunc()") -- lowering-synthesized
             * casts (an implicit upcast, a receiver cast) never reach
             * check_node at all, since they're inserted after sema.c
             * has already finished running; only a cast parsed directly
             * from source does, and its own inner expression can
             * contain anything an ordinary expression can, including a
             * call that needs its own resolution the same as it would
             * anywhere else.
             *
             * ival==1 marks a cast that was specifically written as
             * `dynamic_cast<T>(...)` (see cpp_cast_kw in parser.y) --
             * warned about here, not rejected: this project transpiles
             * it as a bare, ordinary cast (see AST_CAST's own doc
             * comment in ast.h), which is NOT what real dynamic_cast
             * actually promises (a runtime type check, requiring RTTI
             * this project has never supported, by design). One
             * warning per occurrence, not deduplicated -- matches how
             * sema_error already reports every occurrence of a real
             * error, not just the first. */
            if (n->ival == 1) {
                sema_warning(n->line,
                             "'dynamic_cast' is accepted but not truly implemented -- "
                             "no runtime type check is performed (this project has no "
                             "RTTI); it transpiles as an ordinary cast, identical to "
                             "'static_cast' here");
            }
            check_node(n->a, current_class, locals);
            break;
        case AST_SIZEOF:
            /* Only one of type/a is ever set (see AST_SIZEOF's own doc
             * comment in ast.h) -- the type-taking form (`sizeof(int)`)
             * has nothing to recurse into at all; only the expression-
             * taking form (`sizeof(someFunc())`) can contain a call or
             * other construct needing its own resolution. check_node's
             * own NULL guard (same as every other recursive walker in
             * this file) makes an explicit `if (n->a != NULL)` check
             * here unnecessary -- calling it with a NULL a is already
             * a safe no-op. */
            check_node(n->a, current_class, locals);
            break;
        case AST_VAR_DECL: {
            check_node(n->a, current_class, locals); /* initializer, if any */
            LocalVarType *lv = calloc(1, sizeof(LocalVarType)); /* calloc: zero-inits was_reference too */
            lv->name = n->str1;
            lv->type = n->type;
            lv->next = *locals;
            *locals = lv;
            break;
        }
        case AST_BINOP:
            check_node(n->a, current_class, locals);
            check_node(n->b, current_class, locals);
            resolve_operator_use(n, binop_operator_name(n->str1), n->a, n->b, current_class, *locals);
            break;
        case AST_ASSIGN:
            check_node(n->a, current_class, locals);
            check_node(n->b, current_class, locals);
            resolve_operator_use(n, assign_operator_name(n->str1), n->a, n->b, current_class, *locals);
            break;
        case AST_SUBSCRIPT:
            check_node(n->a, current_class, locals);
            check_node(n->b, current_class, locals);
            resolve_operator_use(n, "operator[]", n->a, n->b, current_class, *locals);
            break;
        case AST_UNOP:
            check_node(n->a, current_class, locals);
            resolve_operator_use(n, unop_operator_name(n->str1), n->a, NULL, current_class, *locals);
            break;
        case AST_TERNARY:
            /* No resolve_operator_use call -- the ternary operator isn't
             * in this project's own overloadable set at all (matching
             * "&&"/"||", never in binop_operator_name's own list
             * either), so it's always a plain, built-in construct, not
             * something to check against a class's own operator
             * overloads. All three children walked, unlike AST_CAST's
             * single child -- a ternary's condition, true-branch, and
             * false-branch can each independently contain a call or
             * other construct needing its own resolution. */
            check_node(n->a, current_class, locals);
            check_node(n->b, current_class, locals);
            check_node(n->c, current_class, locals);
            break;
        case AST_NEW:
            for (int i = 0; i < n->list.count; i++) check_node(n->list.items[i], current_class, locals);
            check_node(n->a, current_class, locals); /* array-new's own size expression, if any (NULL otherwise) */
            resolve_new_expr(n, current_class, *locals);
            break;
        case AST_INIT_LIST:
            /* `= {1, 2, 3}` -- each value needs the same checking any
             * other expression gets (a value could itself be a call, an
             * operator use, a member access, etc). No length-checking
             * against the array's own declared size happens here or
             * anywhere else yet -- see parser.y's own doc comment on
             * opt_array_initializer for that gap. */
            for (int i = 0; i < n->list.count; i++) check_node(n->list.items[i], current_class, locals);
            break;
        case AST_IDENT: {
            /* A bare name that resolves to an INHERITED member (not a
             * local/param, which would shadow it) is an implicit
             * this->member access, and needs the same legality check an
             * explicit one would get -- this is what catches a derived
             * class quietly reading/writing a base class's private data
             * by name alone. */
            if (current_class != NULL && find_local(*locals, n->str1) == NULL) {
                AstNode *owner = NULL;
                AstNode *member = find_member_in_hierarchy(current_class, n->str1, &owner);
                if (member != NULL) {
                    check_member_access(n->line, n->str1, member, owner, current_class);
                }
            }
            break;
        }
        case AST_MEMBER:
            check_node(n->a, current_class, locals); /* the object -- catches chains like a.b.c */
            {
                AstNode *obj_class = resolve_expr_class(n->a, current_class, *locals);
                if (obj_class != NULL) {
                    AstNode *owner = NULL;
                    AstNode *member = find_member_in_hierarchy(obj_class, n->str2, &owner);
                    /* member == NULL means we resolved the object's class
                     * but not this specific member name -- not this
                     * pass's job to diagnose "no such member", only
                     * access legality for ones it DID find. */
                    if (member != NULL) {
                        check_member_access(n->line, n->str2, member, owner, current_class);
                    }
                }
            }
            break;
        case AST_CALL:
            check_node(n->a, current_class, locals); /* callee -- if AST_MEMBER, already checked above */
            for (int i = 0; i < n->list.count; i++) check_node(n->list.items[i], current_class, locals);
            /* Post-order: children (including any nested calls used as
             * arguments) are fully processed above before this call
             * itself attempts resolution. */
            resolve_call(n, current_class, *locals);
            break;
        default:
            /* Literals, AST_THIS, AST_QUALIFIED_ID, AST_DELETE's own
             * operand (already handled via the AST_DELETE case above,
             * which shares AST_RETURN/AST_EXPR_STMT's single-child
             * recursion), AST_TYPEDEF_DECL, AST_ACCESS_SPEC, ... --
             * nothing to check or recurse into. */
            break;
    }
}

/* Checks whether `func` (a constructor) needs an IMPLICIT base-class
 * constructor call -- real C++'s own rule: a derived class's
 * constructor that does NOT explicitly delegate to its base (no
 * ": Base(args)" written at all) still calls the base's own DEFAULT
 * (zero-argument) constructor automatically, if one exists.
 *
 * Three outcomes:
 *   - No base class at all -- nothing to check.
 *   - Already explicitly delegating (func->c has an entry naming the
 *     base) -- nothing more needed; checked directly against func->c
 *     here, not against anything resolve_member_init_list attached,
 *     so this function stays independent of that one's own ordering.
 *   - No explicit delegation: if the base has NO constructor of its
 *     own at all, nothing is required (matching real C++'s own
 *     implicitly-default-constructible rule for a class with no
 *     user-declared constructor -- and matching how this project
 *     ALREADY treats "no constructor exists" as a no-op everywhere
 *     else, e.g. lower.c's own find_zero_arg_constructor simply
 *     returning NULL). If the base DOES have a constructor, but none
 *     of its overloads is callable with zero arguments and has a body,
 *     this is a genuine, real C++ compile error ("no default
 *     constructor exists for base class") -- reported here, not
 *     silently left as an uninitialized base subobject the way it
 *     would have been before this check existed.
 *
 * Attaches nothing anywhere -- lower.c's own phase 8b independently
 * re-derives "does this constructor need an implicit base call" using
 * the identical "find a zero-arg, has-a-body base constructor" logic,
 * since by the time lowering runs, this function has already confirmed
 * (by not erroring) that either none is needed at all, or one
 * genuinely exists to insert. */
static void check_implicit_base_construction(AstNode *func, AstNode *current_class) {
    if (current_class == NULL || strcmp(func->str1, current_class->str1) != 0) {
        return; /* not a constructor at all */
    }

    ClassLayout *layout = (ClassLayout *)current_class->sema_info;
    AstNode *base_class = (layout != NULL) ? layout->base_class_decl : NULL;
    if (base_class == NULL) return; /* no base class -- nothing to check */

    if (func->c != NULL) {
        for (int i = 0; i < func->c->list.count; i++) {
            if (strcmp(func->c->list.items[i]->str1, base_class->str1) == 0) {
                return; /* explicit delegation already covers this */
            }
        }
    }

    ClassLayout *base_layout = (ClassLayout *)base_class->sema_info;
    if (base_layout == NULL) return;

    int base_has_any_ctor = 0;
    int base_has_zero_arg_ctor = 0;
    for (int i = 0; i < base_layout->methods.count; i++) {
        AstNode *m = base_layout->methods.items[i];
        if (strcmp(m->str1, base_class->str1) != 0) continue; /* not a constructor */
        base_has_any_ctor = 1;
        if (m->kind == AST_FUNC_DEF && m->list.count == 0) { /* has a body, zero
            explicit params -- at sema time, before this-injection, so no
            "this" to account for yet (unlike lower.c's own equivalent
            check, which runs after this-injection and so compares
            against 1, not 0) */
            base_has_zero_arg_ctor = 1;
            break;
        }
    }

    if (base_has_any_ctor && !base_has_zero_arg_ctor) {
        sema_error(func->line,
                   "'%s' has no default constructor, but '%s' doesn't explicitly "
                   "call one of its base's own constructors -- add a member "
                   "initializer (': %s(args)') to '%s'",
                   base_class->str1, func->str1, base_class->str1, func->str1);
    }
}

static void check_function_body(AstNode *func, AstNode *current_class) {
    if (func->kind != AST_FUNC_DEF) return; /* only definitions have bodies to walk */
    LocalVarType *locals = NULL;
    for (int i = 0; i < func->list.count; i++) {
        AstNode *param = func->list.items[i];
        LocalVarType *lv = calloc(1, sizeof(LocalVarType)); /* calloc: zero-inits was_reference too */
        lv->name = param->str1;
        lv->type = param->type;
        lv->next = locals;
        locals = lv;
    }
    /* Resolved BEFORE walking the body -- func->c's own entries (a
     * base-class-delegation call's own arguments) may reference this
     * constructor's own parameters, which `locals` already has bound by
     * this point; nothing about walking the body itself depends on
     * func->c having been resolved first, so the ordering here is only
     * about `locals` being ready, not about any dependency the other
     * direction. */
    resolve_member_init_list(func, current_class, locals);
    check_implicit_base_construction(func, current_class);
    check_node(func->a, current_class, &locals);
    /* `locals` is deliberately never freed -- single-shot CLI tool, same
     * memory philosophy as the rest of this project (see e.g.
     * symtab_destroy's doc comment). */
}

static void access_check_methods(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    check_function_body(layout->methods.items[j], n);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            access_check_methods(&n->list);
        }
    }
}

static void access_check_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            access_check_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            /* n->b == NULL excludes out-of-line method definitions' own
             * top-level duplicate (see attach_out_of_line) -- those get
             * checked once already, as part of their owning class's
             * methods list above, with the correct calling-context class.
             * Checking them AGAIN here with current_class = NULL would
             * incorrectly flag every private/protected access in every
             * out-of-line method body. */
            check_function_body(n, NULL);
        }
    }
}

/* Walks every top-level (file-scope) AST_VAR_DECL's own initializer
 * expression, the same way check_function_body already walks a local
 * variable's own initializer within a function body -- a real,
 * previously-existing gap, not something this project ever deliberately
 * scoped out: without this, a global's own initializer (if it contains
 * a function call, say) would never get its own CallResolution
 * attached, silently differing from how the identical expression would
 * be treated as a local variable's own initializer one function down.
 * current_class is NULL (a global isn't a class member) and locals
 * starts empty each time (a global's own initializer can only ever
 * reference OTHER globals or free functions, never anything
 * function-local, which doesn't exist at file scope) -- a global
 * referencing another global by name simply isn't tracked as a `local`
 * at all (this project has no separate "globals" registry the way
 * `locals`/class members are tracked), so `infer_expr_type` returns
 * NULL for it the same graceful way it already does for any other
 * unresolved identifier (an undeclared library function name, say) --
 * not a bug, matching this project's own established "best-effort,
 * let the real C compiler catch what this one doesn't fully
 * understand" philosophy, and harmless here specifically because
 * codegen.c prints a bare identifier verbatim regardless of whether
 * sema.c ever resolved its type. */
static void check_globals(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_VAR_DECL) {
            LocalVarType *locals = NULL;
            check_node(n->a, NULL, &locals);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            check_globals(&n->list);
        }
    }
}

/* Checks whether the program defines an actual `main` -- specifically a
 * top-level (never a method; see mangle()'s own class_name == NULL
 * restriction on the same special-casing) AST_FUNC_DEF, not merely a
 * bare prototype declaring one. Exists for main.c's own CLI-level
 * "require a complete, standalone-compilable program by default" check
 * (the `-c` flag disables it) -- entirely separate from anything
 * sema_run() itself validates; this project's compiler has never
 * required a `main` to exist to transpile something correctly, and
 * still doesn't -- this is purely about main.c's own default behavior
 * when the person hasn't asked for anything else. */
static int decls_have_function(const AstList *decls, const char *name) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            if (decls_have_function(&n->list, name)) return 1;
        } else if (n->kind == AST_FUNC_DEF && strcmp(n->str1, name) == 0) {
            return 1;
        }
    }
    return 0;
}

int sema_program_has_main(const AstNode *program) {
    return decls_have_function(&program->list, "main");
}

/* Generalizes sema_program_has_main's own "does a function with this
 * name exist anywhere, including inside a namespace" check to an
 * arbitrary name -- added for main.c's own -b (BIOS) validation, which
 * needs the same kind of check for `error_handler`, not just `main`.
 * Same CLI-level-default framing applies: this is not something sema_run()
 * itself enforces (a program missing `error_handler` still transpiles
 * correctly on its own terms), purely main.c's own default behavior when
 * `-b` was given. */
int sema_program_has_function(const AstNode *program, const char *name) {
    return decls_have_function(&program->list, name);
}

int sema_run(AstNode *program) {

    g_error_count = 0;
    g_warning_count = 0; /* defensive reset, same reasoning as g_error_count
        just above -- in case sema_run() is ever called twice in one process */
    free_registry();          /* defensive: in case sema_run() is ever called twice in one process */
    free_typedef_registry();  /* same */
    free_free_func_registry(); /* same */

    collect_declarations(&program->list);
    attach_out_of_line(&program->list);
    compute_layouts(&program->list);
    mangle_free_functions(&program->list);

    /* Access-control enforcement AND call-site overload resolution (the
     * latter happens inside check_node's AST_CALL case, called from the
     * former) both need find_class()/resolve_typedef_chain() (via
     * type_to_class) and the free-function registry populated, and every
     * class's ClassLayout (access stamps, vtable, base_class_decl)
     * already computed by compute_layouts() above. */
    access_check_methods(&program->list);
    access_check_free_functions(&program->list);
    check_globals(&program->list);

    /* NOTE: the registries are deliberately NOT freed here anymore.
     * lower.c's vtable-dispatch phase also depends on find_class() (via
     * resolve_expr_class, exposed from this file specifically so
     * lowering could reuse it) -- freeing the registries at the end of
     * THIS function would tear them down before lower_run() ever gets a
     * chance to use them, which is exactly the bug that shipped in an
     * earlier round: virtual calls silently failed to lower (obj_class
     * always resolved to NULL) while non-virtual ones worked fine (that
     * path never calls resolve_expr_class at all). See sema_cleanup()
     * below -- the caller (main.c) is now responsible for calling it
     * once ALL passes that might need these registries, sema AND
     * lowering alike, have finished. */
    return g_error_count;
}

/* Frees the class/typedef/free-function registries sema_run() builds.
 * Call this ONLY after every pass that might need them has finished --
 * that's sema_run() itself, AND lower_run() (via resolve_expr_class,
 * which lower.c's vtable-dispatch phase depends on). Calling this too
 * early silently breaks class-type resolution for whoever runs after --
 * see the long comment in sema_run() above for exactly how that failure
 * mode looks (a best-effort function quietly resolving to NULL instead
 * of crashing, which is what made it easy to ship once already). */
void sema_cleanup(void) {
    free_registry();
    free_typedef_registry();
    free_free_func_registry();
}

/* ---- dump ---------------------------------------------------------- */

static void indent_line(int indent) {
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
}

static const char *access_str(AccessSpec a) {
    switch (a) {
        case ACC_PUBLIC: return "public";
        case ACC_PRIVATE: return "private";
        case ACC_PROTECTED: return "protected";
    }
    return "?";
}

static void dump_class_layout(const AstNode *class_decl, int indent) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;

    indent_line(indent);
    printf("class %s", class_decl->str1);
    if (class_decl->str2 != NULL) printf(" : %s %s", access_str(class_decl->access), class_decl->str2);
    printf("\n");

    if (layout == NULL) {
        indent_line(indent + 1);
        printf("(no layout computed)\n");
        return;
    }

    indent_line(indent + 1);
    printf("data members:\n");
    for (int i = 0; i < layout->data_members.count; i++) {
        AstNode *dm = layout->data_members.items[i];
        indent_line(indent + 2);
        printf("%s [%s]\n", dm->str1, access_str(dm->access));
    }

    indent_line(indent + 1);
    printf("methods:\n");
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        FuncSemaInfo *info = (FuncSemaInfo *)m->sema_info;
        indent_line(indent + 2);
        printf("%s -> %s%s%s [%s]\n",
               m->str1,
               info != NULL ? info->mangled_name : "(unmangled)",
               m->kind == AST_FUNC_DEF ? " [has body]" : " [prototype only]",
               m->ival == 1 ? " [virtual]" : "",
               access_str(m->access));
    }

    if (layout->base_class_decl != NULL) {
        indent_line(indent + 1);
        printf("base class resolved: yes (%s)\n", layout->base_class_decl->str1);
    } else if (class_decl->str2 != NULL) {
        indent_line(indent + 1);
        printf("base class resolved: NO (dangling reference to '%s')\n", class_decl->str2);
    }

    indent_line(indent + 1);
    if (layout->vtable == NULL) {
        printf("vtable: (none -- no virtual methods, own or inherited)\n");
    } else {
        printf("vtable:\n");
        for (int i = 0; i < layout->vtable->count; i++) {
            AstNode *m = layout->vtable->entries[i].method;
            FuncSemaInfo *info = (FuncSemaInfo *)m->sema_info;
            indent_line(indent + 2);
            printf("[%d] %s -> %s\n",
                   layout->vtable->entries[i].slot_index,
                   m->str1,
                   info != NULL ? info->mangled_name : "(unmangled)");
        }
    }
}

static void dump_decls(const AstList *decls, int indent) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            dump_class_layout(n, indent);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            indent_line(indent);
            printf("namespace %s {\n", n->str1);
            dump_decls(&n->list, indent + 1);
            indent_line(indent);
            printf("}\n");
        } else if (n->kind == AST_FUNC_DECL || n->kind == AST_FUNC_DEF) {
            FuncSemaInfo *info = (FuncSemaInfo *)n->sema_info;
            if (info != NULL && info->is_out_of_line) {
                /* Skip the top-level duplicate left behind by an
                 * out-of-line definition -- its real entry already
                 * printed under the owning class above. */
                continue;
            }
            indent_line(indent);
            printf("function %s -> %s%s\n",
                   n->str1,
                   info != NULL ? info->mangled_name : "(unmangled)",
                   n->kind == AST_FUNC_DEF ? " [has body]" : " [prototype only]");
        }
    }
}

/* ---- call resolution dump -----------------------------------------------
 *
 * A separate, display-only walk (same relationship dump_class_layout has
 * to compute_layout: it doesn't perform resolution, just reports what the
 * resolution pass already decided) -- exists specifically so a
 * SUCCESSFUL resolution is visible. Without this, "resolved correctly"
 * and "silently skipped, best-effort" look identical from the outside:
 * both produce zero output. Only calls that DID resolve are listed;
 * skipped/no-match/ambiguous calls either produce no line here (skipped)
 * or already produced a "semantic error" line elsewhere (no-match/
 * ambiguous) -- not repeated here.
 */

static void print_resolution_if_any(const AstNode *n) {
    CallResolution *cr = (CallResolution *)n->sema_info;
    if (cr != NULL && cr->resolved_target != NULL) {
        FuncSemaInfo *info = (FuncSemaInfo *)cr->resolved_target->sema_info;
        indent_line(1);
        printf("call @line%d -> %s\n", n->line, info != NULL ? info->mangled_name : "(unmangled)");
    }
}

static void dump_calls_in_node(const AstNode *n) {
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) dump_calls_in_node(n->list.items[i]);
            break;
        case AST_IF:
            dump_calls_in_node(n->a); dump_calls_in_node(n->b); dump_calls_in_node(n->c);
            break;
        case AST_LABEL:
            dump_calls_in_node(n->a);
            break;
        case AST_WHILE:
            dump_calls_in_node(n->a); dump_calls_in_node(n->b);
            break;
        case AST_FOR:
            dump_calls_in_node(n->a); dump_calls_in_node(n->b);
            dump_calls_in_node(n->c); dump_calls_in_node(n->d);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
        case AST_DELETE:
        case AST_VAR_DECL:
        case AST_CAST:
            dump_calls_in_node(n->a);
            break;
        case AST_SIZEOF:
            dump_calls_in_node(n->a); /* NULL-safe when the type-taking form is used, same as AST_CAST's own case just above relies on for its own single child */
            break;
        case AST_BINOP:
        case AST_ASSIGN:
        case AST_SUBSCRIPT:
            /* These can carry their OWN CallResolution now too (an
             * operator used via natural syntax -- see resolve_operator_use)
             * -- not just AST_CALL. Print it the same way AST_CALL's own
             * case does, or a successful operator resolution would be
             * just as invisible here as the "dump_this_injected_methods"
             * gap made lowered free functions invisible a couple of
             * rounds back. Same category of bug, caught before it shipped
             * this time by remembering this dump function's whole stated
             * purpose is making resolution visible. */
            dump_calls_in_node(n->a); dump_calls_in_node(n->b);
            print_resolution_if_any(n);
            break;
        case AST_UNOP:
            dump_calls_in_node(n->a);
            print_resolution_if_any(n); /* same reasoning as BINOP/ASSIGN/SUBSCRIPT above */
            break;
        case AST_TERNARY:
            dump_calls_in_node(n->a); dump_calls_in_node(n->b); dump_calls_in_node(n->c);
            break;
        case AST_MEMBER:
            dump_calls_in_node(n->a);
            break;
        case AST_NEW:
            /* A constructor call, resolved by sema.c's resolve_new_expr --
             * same treatment. */
            for (int i = 0; i < n->list.count; i++) dump_calls_in_node(n->list.items[i]);
            print_resolution_if_any(n);
            break;
        case AST_CALL: {
            dump_calls_in_node(n->a);
            for (int i = 0; i < n->list.count; i++) dump_calls_in_node(n->list.items[i]);
            print_resolution_if_any(n);
            break;
        }
        default:
            break;
    }
}

static void dump_call_resolutions(const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            dump_call_resolutions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            /* n->b == NULL excludes an out-of-line method definition's
             * own top-level duplicate (see attach_out_of_line). Its body
             * is the SAME shared AstNode as the real in-class target's
             * (target->a = n->a, a shared pointer, not a copy) -- walking
             * both would print every call inside it twice. The
             * AST_CLASS_DECL branch below, via layout->methods, is the
             * one true walk for every method's body, in-class or
             * out-of-line alike; this branch is only for genuine free
             * functions. */
            dump_calls_in_node(n->a);
        } else if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    dump_calls_in_node(layout->methods.items[j]->a);
                }
            }
        }
    }
}

void sema_dump(const AstNode *program) {
    printf("---- semantic analysis summary ----\n");
    dump_decls(&program->list, 0);
    printf("call resolutions:\n");
    dump_call_resolutions(&program->list);
}
