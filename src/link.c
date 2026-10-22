/* link.c -- whole-program linking of several source files. See link.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "driver.h"
#include "link.h"
#include "prescan.h"

typedef struct LinkSym {
    char *name;
    int tu;
    int is_static;
    int is_def;         /* a function body, or an initialised/non-extern variable */
    int is_var;
    int is_extern;
    const AstNode *node;
} LinkSym;

typedef struct LinkTab { LinkSym *items; int count; } LinkTab;

static int is_symbol_node(const AstNode *n) {
    if (n == NULL || n->str1 == NULL) return 0;
    if (n->kind != AST_FUNC_DEF && n->kind != AST_FUNC_DECL && n->kind != AST_VAR_DECL) return 0;
    return strstr(n->str1, "::") == NULL;       /* a method's out-of-class definition */
}

static const char *node_file(const AstNode *n) { return n->file != NULL ? n->file : g_current_filename; }

static const char *base_stem(const char *path, char *buf, size_t cap) {
    const char *b = strrchr(path, '/');
    const char *w = strrchr(path, '\\');
    if (w != NULL && (b == NULL || w > b)) b = w;
    b = b != NULL ? b + 1 : path;
    size_t k = 0;
    for (; *b != '\0' && *b != '.' && k + 1 < cap; b++)
        buf[k++] = ((*b >= 'a' && *b <= 'z') || (*b >= 'A' && *b <= 'Z') || (*b >= '0' && *b <= '9')) ? *b : '_';
    buf[k] = '\0';
    return buf;
}

/* ---- renaming a file's static, references included ------------------------ */

typedef struct Ren { const char *from; const char *to; int shadow; } Ren;

static void ren_walk(AstNode *n, Ren *r);

static void ren_list(AstList *l, Ren *r) {
    for (int i = 0; i < l->count; i++) ren_walk(l->items[i], r);
}

/* A local or parameter of the same name hides the static from here to the
 * end of its block: `shadow` counts them, restored when a scope closes. */
static void ren_walk(AstNode *n, Ren *r) {
    if (n == NULL) return;
    switch (n->kind) {
        case AST_IDENT:
            if (!r->shadow && n->str1 != NULL && strcmp(n->str1, r->from) == 0) n->str1 = strdup(r->to);
            return;
        case AST_BLOCK: {
            int saved = r->shadow;
            ren_list(&n->list, r);
            r->shadow = saved;
            return;
        }
        case AST_FOR: {
            int saved = r->shadow;
            ren_walk(n->a, r);
            ren_walk(n->b, r);
            ren_walk(n->c, r);
            ren_walk(n->d, r);
            r->shadow = saved;
            return;
        }
        case AST_VAR_DECL:
            /* a local: its own name hides the static from its declarator on */
            if (n->str1 != NULL && strcmp(n->str1, r->from) == 0) r->shadow++;
            ren_walk(n->a, r);
            return;
        case AST_FUNC_DEF: {
            int saved = r->shadow;
            for (int i = 0; i < n->list.count; i++) {
                const AstNode *p = n->list.items[i];
                if (p != NULL && p->str1 != NULL && strcmp(p->str1, r->from) == 0) r->shadow++;
            }
            ren_walk(n->a, r);
            r->shadow = saved;
            return;
        }
        default:
            break;
    }
    ren_walk(n->a, r);
    ren_walk(n->b, r);
    ren_walk(n->c, r);
    ren_walk(n->d, r);
    ren_list(&n->list, r);
}

/* ---- references, for "undefined reference" ---------------------------------- */

typedef struct NameSet { char **names; int count, cap; } NameSet;

static void set_add(NameSet *s, const char *name) {
    for (int i = 0; i < s->count; i++) if (strcmp(s->names[i], name) == 0) return;
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 64;
        s->names = realloc(s->names, (size_t)s->cap * sizeof *s->names);
    }
    s->names[s->count++] = strdup(name);
}

static int set_has(const NameSet *s, const char *name) {
    for (int i = 0; i < s->count; i++) if (strcmp(s->names[i], name) == 0) return 1;
    return 0;
}

static void collect_idents(const AstNode *n, NameSet *s) {
    if (n == NULL) return;
    if (n->kind == AST_IDENT && n->str1 != NULL) set_add(s, n->str1);
    collect_idents(n->a, s);
    collect_idents(n->b, s);
    collect_idents(n->c, s);
    collect_idents(n->d, s);
    for (int i = 0; i < n->list.count; i++) collect_idents(n->list.items[i], s);
}

/* ---- the pass ----------------------------------------------------------------- */

static int count_tus_with(const LinkTab *t, const char *name, int *first_static_tu) {
    int seen_mask_count = 0;
    int last = -1;
    (void)first_static_tu;
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->items[i].name, name) != 0 || t->items[i].tu == last) continue;
        /* count distinct TUs: the table is in TU order, so equal TUs are adjacent */
        seen_mask_count++;
        last = t->items[i].tu;
    }
    return seen_mask_count;
}

int link_merge(AstList *decls, const char *const *files, int nfiles, int allow_undefined) {
    int errors = 0;
    int n = decls->count;
    int *tu_of = calloc((size_t)n + 1, sizeof *tu_of);
    AstList kept = ast_list_new();
    int *kept_tu = calloc((size_t)n + 1, sizeof *kept_tu);

    /* 1. which file each declaration is from, by the markers; markers go */
    int tu = 0;
    size_t mlen = strlen(LINK_TU_MARKER);
    for (int i = 0; i < n; i++) {
        AstNode *d = decls->items[i];
        if (d != NULL && d->kind == AST_VAR_DECL && d->str1 != NULL &&
            strncmp(d->str1, LINK_TU_MARKER, mlen) == 0) {
            tu = atoi(d->str1 + mlen);
            continue;
        }
        kept_tu[kept.count] = tu;
        ast_list_append(&kept, d);
    }
    (void)tu_of;

    /* 2. every function and variable declared at file scope */
    LinkTab tab = { NULL, 0 };
    tab.items = calloc((size_t)kept.count + 1, sizeof *tab.items);
    for (int i = 0; i < kept.count; i++) {
        const AstNode *d = kept.items[i];
        if (!is_symbol_node(d)) continue;
        LinkSym *s = &tab.items[tab.count++];
        s->name = d->str1;
        s->tu = kept_tu[i];
        s->is_static = d->is_file_static;
        s->is_var = d->kind == AST_VAR_DECL;
        s->is_extern = s->is_var && d->is_extern;
        s->is_def = d->kind == AST_FUNC_DEF || (s->is_var && !d->is_extern && d->a != NULL);
        s->node = d;
    }

    /* 3. a static whose name is used by any other file is renamed, in the
     * file that owns it */
    for (int i = 0; i < tab.count; i++) {
        LinkSym *s = &tab.items[i];
        if (!s->is_static) continue;
        if (count_tus_with(&tab, s->name, NULL) < 2) continue;
        char stem[64], to[200];
        base_stem(files[s->tu < nfiles ? s->tu : 0], stem, sizeof stem);
        snprintf(to, sizeof to, "%s__%s", s->name, stem);
        /* never onto a name already in use */
        for (int k = 2;; k++) {
            int used = 0;
            for (int j = 0; j < tab.count; j++) if (strcmp(tab.items[j].name, to) == 0) { used = 1; break; }
            if (!used) break;
            snprintf(to, sizeof to, "%s__%s%d", s->name, stem, k);
        }
        const char *from = s->name;
        Ren r = { from, to, 0 };
        for (int j = 0; j < kept.count; j++) {
            if (kept_tu[j] != s->tu) continue;
            AstNode *d = kept.items[j];
            if (is_symbol_node(d) && d->is_file_static && strcmp(d->str1, from) == 0) d->str1 = strdup(to);
            /* the declaration's own body or initialiser; its name is no
             * local shadow of itself */
            r.shadow = 0;
            ren_walk(d->kind == AST_FUNC_DEF ? d : d->a, &r);
        }
        /* the symbol table follows the rename, for later statics */
        for (int j = 0; j < tab.count; j++)
            if (tab.items[j].tu == s->tu && tab.items[j].is_static && strcmp(tab.items[j].name, from) == 0)
                tab.items[j].name = ((AstNode *)tab.items[j].node)->str1;
    }

    /* 4. multiple definitions */
    for (int i = 0; i < tab.count; i++) {
        const LinkSym *a = &tab.items[i];
        if (!a->is_def) continue;
        for (int j = 0; j < i; j++) {
            const LinkSym *b = &tab.items[j];
            if (!b->is_def || b->is_var != a->is_var || strcmp(a->name, b->name) != 0) continue;
            fprintf(stderr, "%s:%d: error: multiple definition of '%s' (first defined at %s:%d)\n",
                    node_file(a->node), a->node->line, a->name, node_file(b->node), b->node->line);
            errors++;
            break;
        }
    }

    /* 5. a function or extern variable that is used and never defined */
    if (g_c_mode) {
        NameSet used = { NULL, 0, 0 };
        for (int i = 0; i < kept.count; i++) {
            const AstNode *d = kept.items[i];
            if (d->kind == AST_FUNC_DEF) collect_idents(d->a, &used);
            else if (d->kind == AST_VAR_DECL) collect_idents(d->a, &used);
        }
        NameSet reported = { NULL, 0, 0 };
        for (int i = 0; i < tab.count; i++) {
            const LinkSym *s = &tab.items[i];
            if (s->is_def || set_has(&reported, s->name)) continue;
            if (s->is_var && !s->is_extern) continue;          /* a tentative definition is one */
            int defined = 0;
            for (int j = 0; j < tab.count; j++) {
                const LinkSym *o = &tab.items[j];
                if (strcmp(o->name, s->name) == 0 && o->is_var == s->is_var &&
                    (o->is_def || (o->is_var && !o->is_extern))) { defined = 1; break; }
            }
            if (defined || !set_has(&used, s->name)) continue;
            set_add(&reported, s->name);
            fprintf(stderr, "%s:%d: %s: undefined reference to '%s'%s\n", node_file(s->node), s->node->line,
                    allow_undefined ? "warning" : "error", s->name,
                    s->is_var ? " (declared extern, never defined)" : " (declared, never defined)");
            if (!allow_undefined) errors++;
        }
    }

    *decls = kept;
    free(tab.items);
    free(kept_tu);
    free(tu_of);
    return errors;
}

/* ---- --list-symbols ------------------------------------------------------- */

/* names a function declares itself: its parameters and every local */
static void collect_locals(const AstNode *n, NameSet *s) {
    if (n == NULL) return;
    if (n->kind == AST_VAR_DECL && n->str1 != NULL) set_add(s, n->str1);
    collect_locals(n->a, s);
    collect_locals(n->b, s);
    collect_locals(n->c, s);
    collect_locals(n->d, s);
    for (int i = 0; i < n->list.count; i++) collect_locals(n->list.items[i], s);
}

/* the names a function's code uses that are not its own locals */
static void collect_free_idents(const AstNode *fn, NameSet *out) {
    NameSet own = { NULL, 0, 0 }, used = { NULL, 0, 0 };
    for (int i = 0; i < fn->list.count; i++)
        if (fn->list.items[i] != NULL && fn->list.items[i]->str1 != NULL) set_add(&own, fn->list.items[i]->str1);
    collect_locals(fn->a, &own);
    collect_idents(fn->a, &used);
    for (int i = 0; i < used.count; i++)
        if (!set_has(&own, used.names[i])) set_add(out, used.names[i]);
}

void link_list_symbols(AstList *decls, int nfiles, FILE *out) {
    size_t mlen = strlen(LINK_TU_MARKER);
    for (int t = 0; t < (nfiles > 0 ? nfiles : 1); t++) {
        NameSet defs = { NULL, 0, 0 }, all_defs = { NULL, 0, 0 }, refs = { NULL, 0, 0 };
        int tu = 0;
        for (int i = 0; i < decls->count; i++) {
            const AstNode *d = decls->items[i];
            if (d != NULL && d->kind == AST_VAR_DECL && d->str1 != NULL &&
                strncmp(d->str1, LINK_TU_MARKER, mlen) == 0) {
                tu = atoi(d->str1 + mlen);
                continue;
            }
            if (tu != t || !is_symbol_node(d)) continue;
            int is_def = d->kind == AST_FUNC_DEF || (d->kind == AST_VAR_DECL && !d->is_extern);
            if (!is_def) continue;
            set_add(&all_defs, d->str1);
            if (!d->is_file_static) set_add(&defs, d->str1);
        }
        tu = 0;
        for (int i = 0; i < decls->count; i++) {
            const AstNode *d = decls->items[i];
            if (d != NULL && d->kind == AST_VAR_DECL && d->str1 != NULL &&
                strncmp(d->str1, LINK_TU_MARKER, mlen) == 0) {
                tu = atoi(d->str1 + mlen);
                continue;
            }
            if (tu != t || d == NULL) continue;
            if (d->kind == AST_FUNC_DEF) collect_free_idents(d, &refs);
            else if (d->kind == AST_VAR_DECL) collect_idents(d->a, &refs);
        }
        fprintf(out, "tu %d\n", t);
        for (int i = 0; i < defs.count; i++) fprintf(out, "d %s\n", defs.names[i]);
        for (int i = 0; i < refs.count; i++)
            if (!set_has(&all_defs, refs.names[i])) fprintf(out, "r %s\n", refs.names[i]);
    }
}
