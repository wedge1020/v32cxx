#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

static unsigned long hash_str(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + (unsigned long)c; /* h * 33 + c */
    return h;
}

static Scope *scope_new(const char *owner_name, int is_class_scope, Scope *parent) {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->parent = parent;
    sc->owner_name = owner_name ? strdup(owner_name) : NULL;
    sc->is_class_scope = is_class_scope;
    return sc;
}

SymTab *symtab_create(void) {
    SymTab *st = calloc(1, sizeof(SymTab));
    st->global = scope_new(NULL, 0, NULL);
    st->current = st->global;
    st->pending_qualifier = NULL;
    return st;
}

void symtab_destroy(SymTab *st) {
    /* Skeleton: scopes/symbols are small and live for the process lifetime
     * of the translation-unit compile, so we don't bother walking and
     * freeing the whole tree here. Fine for a single-shot CLI tool; revisit
     * if this ever becomes a long-lived library (e.g. a language server). */
    free(st);
}

Scope *symtab_push_scope(SymTab *st, const char *owner_name, int is_class_scope) {
    Scope *sc = scope_new(owner_name, is_class_scope, st->current);
    st->current = sc;
    return sc;
}

void symtab_pop_scope(SymTab *st) {
    if (st->current->parent != NULL) {
        st->current = st->current->parent;
    }
    /* Popping past the global scope is a caller bug; ignored defensively
     * rather than crashing mid-parse. */
}

static char *build_qualified_name(Scope *scope, const char *name) {
    /* Walk owner names from `scope` up to (but not including) the global
     * scope, prefixing each onto `name` with "::". */
    if (scope == NULL || scope->owner_name == NULL) {
        return strdup(name);
    }
    char *inner = build_qualified_name(scope->parent, scope->owner_name);
    size_t len = strlen(inner) + 2 + strlen(name) + 1;
    char *out = malloc(len);
    snprintf(out, len, "%s::%s", inner, name);
    free(inner);
    return out;
}

Symbol *symtab_insert(SymTab *st, Scope *scope, const char *name, SymbolKind kind) {
    unsigned long idx = hash_str(name) % SYMTAB_BUCKETS;
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->qualified_name = build_qualified_name(scope, name);
    sym->kind = kind;
    sym->inner_scope = NULL;
    sym->next = scope->buckets[idx];
    scope->buckets[idx] = sym;
    (void)st;
    return sym;
}

Symbol *symtab_lookup_in(Scope *scope, const char *name) {
    if (scope == NULL) return NULL;
    unsigned long idx = hash_str(name) % SYMTAB_BUCKETS;
    for (Symbol *s = scope->buckets[idx]; s != NULL; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

Symbol *symtab_lookup(SymTab *st, const char *name) {
    for (Scope *sc = st->current; sc != NULL; sc = sc->parent) {
        Symbol *found = symtab_lookup_in(sc, name);
        if (found != NULL) return found;
    }
    return NULL;
}

Symbol *symtab_lookup_qualified(SymTab *st, const char *name) {
    Symbol *result;
    if (st->pending_qualifier != NULL) {
        result = symtab_lookup_in(st->pending_qualifier, name);
        st->pending_qualifier = NULL; /* consumed: applies to exactly one token */
    } else {
        result = symtab_lookup(st, name);
    }
    return result;
}

int symtab_is_type_name(SymTab *st, const char *name) {
    Symbol *sym = symtab_lookup_qualified(st, name);
    if (sym == NULL) return 0;
    return sym->kind == SYM_CLASS || sym->kind == SYM_TYPEDEF;
}

void symtab_arm_qualifier(SymTab *st, Symbol *sym) {
    if (sym != NULL && (sym->kind == SYM_NAMESPACE || sym->kind == SYM_CLASS)) {
        st->pending_qualifier = sym->inner_scope;
    } else {
        st->pending_qualifier = NULL;
    }
}
