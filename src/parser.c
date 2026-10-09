/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Skeleton implementation for Bison GLR parsers in C

   Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C GLR parser skeleton written by Paul Hilfinger.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "glr.c"

/* Pure parsers.  */
#define YYPURE 0






/* First part of user prologue.  */
#line 29 "src/parser.y"

/*
 * Classic yacc-style prologue instead of %code requires/%code top: some
 * systems (notably macOS, where /usr/bin/bison is frozen at GNU bison 2.3
 * for licensing reasons) predate %code, which bison only added in 2.4.
 * This form works on every bison version, old or new.
 *
 * NOTE: unlike %code requires, this block is emitted into parser.tab.c
 * but is NOT copied into the generated parser.tab.h. That's fine here:
 * the union below only needs AstNode/AstList/Symbol/etc. to be *visible*,
 * and every other file that includes parser.tab.h (lexer.l, main.c)
 * already includes driver.h -- which pulls in ast.h and symtab.h -- first.
 * If you add a new .c file that includes parser.tab.h directly, make sure
 * it includes driver.h (or ast.h+symtab.h) immediately before it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symtab.h"
#include "driver.h"
#include "generic.h"
#include "cmode.h"

/* Expand a string literal's stored inner text (quotes already stripped
 * by lexer.l's STRING_LITERAL rule; escape sequences still raw
 * backslash pairs -- the lexer's own comment on that rule flags
 * Vircon32's escape parity as unverified, which this sidesteps
 * entirely by emitting plain integers) into an AST_INIT_LIST of one
 * AST_INT_LIT per decoded character, plus a single trailing 0
 * terminator -- the exact C rule for `T name[N] = "...";`, where
 * the literal's own implicit terminator is part of the initializer.
 * Escape set mirrors lexer.l's CHAR_LITERAL rule exactly (n, t, r,
 * 0, backslash, both quote kinds) so nothing decodes differently
 * than the lexer itself would for a char literal; an unrecognized
 * escape falls back to the raw character, same best-effort stance
 * that rule already takes. No length-checking against the declared
 * size happens here (matching the braced alternative's own
 * documented gap): a nonterminal never sees its parent rule's
 * symbols, so padding/diagnosing belongs in the var_decl actions
 * or a post-parse pass, not this rule. */
static AstNode *string_literal_init_list(int line, const char *text)
{
    AstNode *list = ast_new(AST_INIT_LIST, line);
    list->list = ast_list_new();
    for (const char *p = text; *p != '\0'; ) {
        int ch = (unsigned char)*p++;
        if (ch == '\\' && *p != '\0')
            ch = ast_decode_escape(&p);  /* hex/octal too, not just \n etc. */
        AstNode *lit = ast_new(AST_INT_LIT, line);
        lit->ival = ch;
        ast_list_append(&list->list, lit);
    }
    /* Trailing terminator -- C's own rule: `char m[3] = "Hi"` means
     * {'H', 'i', 0}; the literal ALWAYS contributes one terminator. */
    AstNode *nul = ast_new(AST_INT_LIT, line);
    nul->ival = 0;
    ast_list_append(&list->list, nul);
    return list;
}

/* Concatenates two raw (escapes undecoded) string-literal bodies, freeing
 * both. If `a` ends in a hex escape (\x41) or an octal escape shorter
 * than three digits (\1, \12) and `b` starts with a character that would
 * extend it, that first character is written as a \xHH escape instead,
 * so the two literals keep their separate meanings (codegen then
 * normalizes every escape to the forms Vircon32 C reads). */
static char *join_string_literals(char *a, char *b) {
    size_t la = strlen(a), lb = strlen(b);
    int hex = 0, oct = 0;
    /* find the escape (if any) the left piece ends in */
    size_t i = la;
    while (i > 0 && strchr("0123456789abcdefABCDEF", a[i - 1])) i--;
    size_t digits = la - i;
    if (digits > 0 && i >= 2 && (a[i - 1] == 'x' || a[i - 1] == 'X') && a[i - 2] == '\\') {
        size_t bs = 0, k = i - 2;
        while (k > 0 && a[k - 1] == '\\') { bs++; k--; }
        hex = (bs % 2 == 0);
    } else if (digits > 0 && digits < 3 && i >= 1 && a[i - 1] == '\\') {
        size_t bs = 0, k = i - 1;
        while (k > 0 && a[k - 1] == '\\') { bs++; k--; }
        int all_octal = 1;
        for (size_t d = i; d < la; d++) if (a[d] > '7') all_octal = 0;
        oct = (bs % 2 == 0) && all_octal;
    }
    int hazard = lb > 0 && ((hex && strchr("0123456789abcdefABCDEF", b[0])) ||
                            (oct && b[0] >= '0' && b[0] <= '7'));
    char *r = malloc(la + lb + 4 + 1);
    memcpy(r, a, la);
    size_t n = la;
    size_t start = 0;
    if (hazard) {
        n += (size_t)sprintf(r + n, "\\x%02x", (unsigned char)b[0]);
        start = 1;
    }
    memcpy(r + n, b + start, lb - start + 1);
    free(a);
    free(b);
    return r;
}

/* Enum constants known so far, for folding array dimensions like
 * `int counts[COLOR_COUNT];` at parse time (array_dim). Filled by
 * enum_decl as each enum finishes parsing; enums are file/namespace
 * scope only in this project, so one flat table (latest definition of a
 * name wins) is the whole story. An enumerator whose own value isn't a
 * foldable constant -- or one implicitly following such -- is simply
 * left out, and a dimension naming it gets the ordinary "not an integer
 * constant expression" error. */
typedef struct EnumConst { char *name; int value; } EnumConst;
static EnumConst *g_enum_consts = NULL;
static int g_enum_const_count = 0;

static int parse_enum_value(const char *name, int *value) {
    for (int i = g_enum_const_count - 1; i >= 0; i--)
        if (strcmp(g_enum_consts[i].name, name) == 0) {
            *value = g_enum_consts[i].value;
            return 1;
        }
    return 0;
}

static void parse_record_enum_values(const AstList *enumerators) {
    int next = 0, known = 1;
    for (int i = 0; i < enumerators->count; i++) {
        const AstNode *ev = enumerators->items[i];
        int v;
        if (ev->a != NULL) known = ast_fold_int(ev->a, parse_enum_value, &v);
        else v = next;
        if (!known) continue;
        g_enum_consts = realloc(g_enum_consts, sizeof(EnumConst) * (size_t)(g_enum_const_count + 1));
        g_enum_consts[g_enum_const_count].name = strdup(ev->str1);
        g_enum_consts[g_enum_const_count].value = v;
        g_enum_const_count++;
        next = v + 1;
    }
}

/* ---- tag typedefs (`typedef struct Actor Actor;` and friends) ----------
 *
 * C needs `typedef struct Actor Actor;` to use the bare name; C++ (and so
 * this transpiler -- the lexer already classifies every class/union/enum
 * tag as TYPE_NAME) does not. So the C idioms are accepted and desugared
 * right here in the parser, leaving sema/lower/codegen untouched:
 *
 *   typedef struct Actor Actor;        -> nothing (the tag IS the name);
 *                                         registers Actor as a type name
 *                                         if the definition comes later
 *   struct Actor;                      -> nothing (codegen already forward-
 *                                         declares every class it defines)
 *   typedef struct Tag { ... } Name;   -> struct Tag {...}; typedef Tag Name;
 *   typedef struct Tag { ... } Tag;    -> struct Tag {...};
 *   typedef struct { ... } Name;       -> struct Name {...};
 *   (same for union and enum)
 *
 * "Nothing" and "two declarations" are both expressed as an
 * AST_VAR_DECL_GROUP -- ast_list_append_flatten splices a group's entries
 * into the surrounding list, so an empty group vanishes and a two-entry
 * group lands as two ordinary top-level declarations. */
/* pointer_opt's value: 0 nothing, 1 `*`, 2 `&`, and N+1 for N >= 2 stars
 * (`char **argv` is 3). */
static AstNode *apply_ptr(AstNode *base, int ptr, int line) {
    if (ptr == 2) return ast_wrap_reference(base, line);
    int depth = (ptr == 1) ? 1 : (ptr >= 3 ? ptr - 1 : 0);
    while (depth-- > 0) base = ast_wrap_pointer(base, line);
    return base;
}

/* ---- C's tag namespace ----------------------------------------------------
 *
 * In C, `struct rdes { ... } rdes[9];` is fine: tags (struct, union and
 * enum names) live in a namespace of their own, and are only ever
 * written after their keyword. The generated Vircon32 C has ONE namespace
 * -- a struct is used by its bare name there -- so for C input:
 *   - a tag is never entered in the symbol table (the lexer must keep
 *     seeing `rdes` as an ordinary identifier);
 *   - every node that names a tag is remembered in g_tag_refs, and
 *     ast_separate_c_tags (ast.c) renames a tag that collides with an
 *     ordinary name, in its declaration and all of those nodes.
 * A struct/union/enum DEFINED inside another declaration (`struct x {..}
 * v;`, an anonymous struct member, one inside a function) is moved out to
 * file scope, ahead of the declaration that contained it. */
AstList g_tag_refs;
AstList g_tag_decls;
int c_tag_known(const char *name);
static AstList g_hoisted_tags;

/* Vircon32 C lets a struct be named without its keyword (`v32key *next;`
 * with no typedef), and C written for it does. Standard C does not, so
 * the tag still stays out of the symbol table; instead every tag seen so
 * far -- defined, being defined (a member pointing at its own struct) or
 * forward-declared -- is remembered here, and the lexer (yylex, lexer.l)
 * hands the grammar a TAG_NAME when such a name stands where only a type
 * can, and is not also an ordinary name in scope. `struct rdes rdes[9];`
 * and Rogue's other tag/name pairs read exactly as before. */
static char **g_known_tags = NULL;
static int g_known_tag_count = 0;

static void c_tag_note(const char *name) {
    if (!g_c_mode || c_tag_known(name)) return;
    g_known_tags = realloc(g_known_tags, (g_known_tag_count + 1) * sizeof *g_known_tags);
    g_known_tags[g_known_tag_count++] = strdup(name);
}

int c_tag_known(const char *name) {
    for (int i = 0; i < g_known_tag_count; i++)
        if (strcmp(g_known_tags[i], name) == 0) return 1;
    return 0;
}

/* A struct/union/enum declaration with a tag (C input only). */
static AstNode *tag_decl(AstNode *decl) {
    c_tag_note(decl->str1);
    if (g_c_mode) ast_list_append(&g_tag_decls, decl);
    return decl;
}

static AstNode *tag_ref(const char *name, int line) {
    AstNode *n = ast_ident(name, line);
    c_tag_note(name);       /* `struct v32key;`, `struct v32key *p;` */
    if (g_c_mode) ast_list_append(&g_tag_refs, n);
    return n;
}

static AstNode *hoist_tag_def(AstNode *decl) {
    ast_list_append(&g_hoisted_tags, decl);
    return tag_ref(decl->str1, decl->line);
}

static char *anon_tag_name(void) {
    static int g_anon_tag_counter = 0;
    char buf[40];
    snprintf(buf, sizeof buf, "__v32_anon%d", g_anon_tag_counter++);
    return strdup(buf);
}

/* A parameter nobody named (`void leave(int);`). */
static char *unnamed_param_name(void) {
    static int unnamed_counter = 0;
    char buf[40];
    snprintf(buf, sizeof buf, "__v32_unnamed%d", unnamed_counter++);
    return strdup(buf);
}

static AstNode *make_func_header(AstNode *ret, const char *name, AstList params, int is_const, int line) {
    AstNode *f = ast_new(AST_FUNC_DECL, line);
    f->str1 = strdup(name);
    f->type = ret;
    f->list = params;
    f->str2 = is_const ? strdup("const") : NULL;
    return f;
}

/* ---- sizeof in a constant expression --------------------------------------
 *
 * `bool used[sizeof rainbow / sizeof (char *)];` needs the size of things
 * while still parsing. File-scope declarations are remembered as they are
 * reduced, and sizes are counted in words (everything scalar is one word
 * on Vircon32). Anything this cannot size -- a class with a base or
 * virtual functions, a local variable -- just fails to fold. */
static AstList g_parse_decls;

static void parse_register_decl(AstNode *d) {
    if (d == NULL) return;
    if (d->kind == AST_VAR_DECL_GROUP) {
        for (int i = 0; i < d->list.count; i++) parse_register_decl(d->list.items[i]);
        return;
    }
    if (d->kind == AST_VAR_DECL || d->kind == AST_TYPEDEF_DECL || d->kind == AST_CLASS_DECL ||
        d->kind == AST_UNION_DECL || d->kind == AST_ENUM_DECL)
        ast_list_append(&g_parse_decls, d);
}

/* The latest declaration of `name`: a variable (want_type 0) or a type. */
static AstNode *parse_find_decl(const char *name, int want_type) {
    for (int i = g_parse_decls.count - 1; i >= 0; i--) {
        AstNode *d = g_parse_decls.items[i];
        if ((d->kind == AST_VAR_DECL) == want_type) continue;
        if (d->str1 != NULL && strcmp(d->str1, name) == 0) return d;
    }
    return NULL;
}

static int parse_type_words(const AstNode *t, int *out);

static int parse_members_words(const AstNode *decl, int is_union, int *out) {
    int total = 0;
    if (decl->kind == AST_CLASS_DECL && decl->str2 != NULL) return 0;
    for (int i = 0; i < decl->list.count; i++) {
        const AstNode *m = decl->list.items[i];
        int w;
        if (m->kind == AST_ACCESS_SPEC) continue;
        if (m->kind != AST_VAR_DECL || !parse_type_words(m->type, &w)) return 0;
        if (is_union) { if (w > total) total = w; }
        else total += w;
    }
    *out = total;
    return 1;
}

static int parse_type_words(const AstNode *t, int *out) {
    int inner;
    if (t == NULL) return 0;
    switch (t->kind) {
        case AST_POINTER_TYPE:
        case AST_REFERENCE_TYPE:
        case AST_FUNC_PTR_TYPE:
            *out = 1;
            return 1;
        case AST_CONST_TYPE:
            return parse_type_words(t->a, out);
        case AST_ARRAY_TYPE:
            if (t->ival < 0 || !parse_type_words(t->a, &inner)) return 0;
            *out = t->ival * inner;
            return 1;
        case AST_IDENT: {
            const char *n = t->str1;
            if (strcmp(n, "int") == 0 || strcmp(n, "char") == 0 ||
                strcmp(n, "bool") == 0 || strcmp(n, "float") == 0) { *out = 1; return 1; }
            const AstNode *d = parse_find_decl(n, 1);
            if (d == NULL) return 0;
            if (d->kind == AST_TYPEDEF_DECL) return parse_type_words(d->type, out);
            if (d->kind == AST_ENUM_DECL) { *out = 1; return 1; }
            return parse_members_words(d, d->kind == AST_UNION_DECL, out);
        }
        default:
            return 0;
    }
}

static int parse_fold_sizeof(const AstNode *e, int *out) {
    if (e->type != NULL) return parse_type_words(e->type, out);
    const AstNode *x = e->a;
    if (x == NULL) return 0;
    if (x->kind == AST_STRING_LIT) {
        int n = 1;      /* the terminator */
        for (const char *p = x->str1; *p != '\0'; n++) {
            if (*p++ == '\\' && *p != '\0') ast_decode_escape(&p);
        }
        *out = n;
        return 1;
    }
    if (x->kind == AST_IDENT) {
        const AstNode *d = parse_find_decl(x->str1, 0);
        return d != NULL && parse_type_words(d->type, out);
    }
    return 0;
}

static AstNode *decl_group_new(int line) {
    return ast_new(AST_VAR_DECL_GROUP, line);
}

/* Make `name` lex as TYPE_NAME from here on, unless it already does. */
static void declare_type_name(const char *name, SymbolKind kind) {
    if (g_c_mode && kind != SYM_TYPEDEF) {         /* a tag: its own namespace */
        c_tag_note(name);
        return;
    }
    Symbol *s = symtab_lookup(g_symtab, name);
    if (s != NULL && (s->kind == SYM_CLASS || s->kind == SYM_UNION ||
                      s->kind == SYM_ENUM  || s->kind == SYM_TYPEDEF)) return;
    symtab_insert(g_symtab, g_symtab->current, name, kind);
}

/* `typedef <tag definition> [*|&] name` -> the definition itself, plus a
 * typedef unless `name` is just the tag again. */
static AstNode *tag_typedef_group(AstNode *decl, int ptr, const char *name, int line) {
    AstNode *g = decl_group_new(line);
    ast_list_append(&g->list, decl);
    if (ptr != 0 || strcmp(decl->str1, name) != 0) {
        AstNode *t = tag_ref(decl->str1, line);
        AstNode *td = ast_new(AST_TYPEDEF_DECL, line);
        td->str1 = strdup(name);
        td->type = apply_ptr(t, ptr, line);
        symtab_insert(g_symtab, g_symtab->current, name, SYM_TYPEDEF);
        ast_list_append(&g->list, td);
    }
    return g;
}

/* `T name[] = { ... }`: take the first dimension from the initializer. */
static void size_unsized_array(AstList dims, const AstNode *init, int line) {
    if (dims.count == 0 || dims.items[0]->ival >= 0) return;
    if (init != NULL && init->kind == AST_INIT_LIST && init->list.count > 0) {
        dims.items[0]->ival = init->list.count;
    } else if (init == NULL && g_c_mode) {
        /* `extern char names[];` -- the definition, elsewhere in the
         * file, has the size: merge_tentative_globals (ast.c) takes it
         * from there, and reports the array nobody defines. */
    } else {
        fprintf(stderr, "%s:%d: error: an array declared with empty brackets "
                "needs an initializer to take its size from\n",
                g_current_filename, line);
        g_parse_errors++;
        dims.items[0]->ival = 1;
    }
}

/* Turns `first` plus the carriers more_plain_declarators collected into
 * the declaration(s) of one statement: `first` alone, or an
 * AST_VAR_DECL_GROUP. Each carrier holds its own pointer flag (ival),
 * array dimensions (list, empty for a plain declarator) and initializer
 * (a); the base type is shared. */
static AstNode *finish_declarators(AstNode *first, AstNode *base, AstList more, int line) {
    if (more.count == 0) return first;
    AstNode *group = ast_new(AST_VAR_DECL_GROUP, line);
    ast_list_append(&group->list, first);
    for (int i = 0; i < more.count; i++) {
        AstNode *spec = more.items[i];
        symtab_insert(g_symtab, g_symtab->current, spec->str1, SYM_VAR);
        AstNode *resolved = ast_new(AST_VAR_DECL, spec->line);
        resolved->str1 = spec->str1;
        AstNode *t = apply_ptr(base, spec->ival, line);
        if (spec->list.count > 0) {
            size_unsized_array(spec->list, spec->a, spec->line);
            t = ast_wrap_array_dims(t, spec->list, line);
        }
        resolved->type = t;
        resolved->a = spec->a;
        ast_list_append(&group->list, resolved);
    }
    return group;
}

#line 484 "src/parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.h"

/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_IDENTIFIER = 3,                 /* IDENTIFIER  */
  YYSYMBOL_TYPE_NAME = 4,                  /* TYPE_NAME  */
  YYSYMBOL_TAG_NAME = 5,                   /* TAG_NAME  */
  YYSYMBOL_STRING_LITERAL = 6,             /* STRING_LITERAL  */
  YYSYMBOL_CLASS_FINAL = 7,                /* CLASS_FINAL  */
  YYSYMBOL_INT_LITERAL = 8,                /* INT_LITERAL  */
  YYSYMBOL_CHAR_LITERAL = 9,               /* CHAR_LITERAL  */
  YYSYMBOL_FLOAT_LITERAL = 10,             /* FLOAT_LITERAL  */
  YYSYMBOL_CLASS = 11,                     /* CLASS  */
  YYSYMBOL_STRUCT = 12,                    /* STRUCT  */
  YYSYMBOL_ENUM = 13,                      /* ENUM  */
  YYSYMBOL_UNION = 14,                     /* UNION  */
  YYSYMBOL_PUBLIC = 15,                    /* PUBLIC  */
  YYSYMBOL_PRIVATE = 16,                   /* PRIVATE  */
  YYSYMBOL_PROTECTED = 17,                 /* PROTECTED  */
  YYSYMBOL_NAMESPACE = 18,                 /* NAMESPACE  */
  YYSYMBOL_TYPEDEF = 19,                   /* TYPEDEF  */
  YYSYMBOL_RETURN = 20,                    /* RETURN  */
  YYSYMBOL_IF = 21,                        /* IF  */
  YYSYMBOL_ELSE = 22,                      /* ELSE  */
  YYSYMBOL_DO = 23,                        /* DO  */
  YYSYMBOL_WHILE = 24,                     /* WHILE  */
  YYSYMBOL_FOR = 25,                       /* FOR  */
  YYSYMBOL_BREAK = 26,                     /* BREAK  */
  YYSYMBOL_CONTINUE = 27,                  /* CONTINUE  */
  YYSYMBOL_GOTO = 28,                      /* GOTO  */
  YYSYMBOL_SWITCH = 29,                    /* SWITCH  */
  YYSYMBOL_CASE = 30,                      /* CASE  */
  YYSYMBOL_DEFAULT = 31,                   /* DEFAULT  */
  YYSYMBOL_INT_KW = 32,                    /* INT_KW  */
  YYSYMBOL_FLOAT_KW = 33,                  /* FLOAT_KW  */
  YYSYMBOL_VOID_KW = 34,                   /* VOID_KW  */
  YYSYMBOL_BOOL_KW = 35,                   /* BOOL_KW  */
  YYSYMBOL_CHAR_KW = 36,                   /* CHAR_KW  */
  YYSYMBOL_NEW = 37,                       /* NEW  */
  YYSYMBOL_DELETE = 38,                    /* DELETE  */
  YYSYMBOL_THIS = 39,                      /* THIS  */
  YYSYMBOL_VIRTUAL = 40,                   /* VIRTUAL  */
  YYSYMBOL_TRUE_KW = 41,                   /* TRUE_KW  */
  YYSYMBOL_FALSE_KW = 42,                  /* FALSE_KW  */
  YYSYMBOL_NULLPTR_KW = 43,                /* NULLPTR_KW  */
  YYSYMBOL_OPERATOR = 44,                  /* OPERATOR  */
  YYSYMBOL_SIZEOF = 45,                    /* SIZEOF  */
  YYSYMBOL_CONST = 46,                     /* CONST  */
  YYSYMBOL_FRIEND = 47,                    /* FRIEND  */
  YYSYMBOL_STATIC_CAST = 48,               /* STATIC_CAST  */
  YYSYMBOL_DYNAMIC_CAST = 49,              /* DYNAMIC_CAST  */
  YYSYMBOL_CONST_CAST = 50,                /* CONST_CAST  */
  YYSYMBOL_REINTERPRET_CAST = 51,          /* REINTERPRET_CAST  */
  YYSYMBOL_COLONCOLON = 52,                /* COLONCOLON  */
  YYSYMBOL_ARROW = 53,                     /* ARROW  */
  YYSYMBOL_EQ = 54,                        /* EQ  */
  YYSYMBOL_NE = 55,                        /* NE  */
  YYSYMBOL_LE = 56,                        /* LE  */
  YYSYMBOL_GE = 57,                        /* GE  */
  YYSYMBOL_ANDAND = 58,                    /* ANDAND  */
  YYSYMBOL_OROR = 59,                      /* OROR  */
  YYSYMBOL_PLUSEQ = 60,                    /* PLUSEQ  */
  YYSYMBOL_MINUSEQ = 61,                   /* MINUSEQ  */
  YYSYMBOL_STAREQ = 62,                    /* STAREQ  */
  YYSYMBOL_SLASHEQ = 63,                   /* SLASHEQ  */
  YYSYMBOL_INC = 64,                       /* INC  */
  YYSYMBOL_DEC = 65,                       /* DEC  */
  YYSYMBOL_SHL = 66,                       /* SHL  */
  YYSYMBOL_SHR = 67,                       /* SHR  */
  YYSYMBOL_ANDEQ = 68,                     /* ANDEQ  */
  YYSYMBOL_OREQ = 69,                      /* OREQ  */
  YYSYMBOL_XOREQ = 70,                     /* XOREQ  */
  YYSYMBOL_SHLEQ = 71,                     /* SHLEQ  */
  YYSYMBOL_SHREQ = 72,                     /* SHREQ  */
  YYSYMBOL_ASM = 73,                       /* ASM  */
  YYSYMBOL_VOLATILE = 74,                  /* VOLATILE  */
  YYSYMBOL_NATIVE = 75,                    /* NATIVE  */
  YYSYMBOL_MODEQ = 76,                     /* MODEQ  */
  YYSYMBOL_STATIC = 77,                    /* STATIC  */
  YYSYMBOL_STD_ARRAY = 78,                 /* STD_ARRAY  */
  YYSYMBOL_STD_VECTOR = 79,                /* STD_VECTOR  */
  YYSYMBOL_ELLIPSIS = 80,                  /* ELLIPSIS  */
  YYSYMBOL_ANON_STRUCT = 81,               /* ANON_STRUCT  */
  YYSYMBOL_ANON_UNION = 82,                /* ANON_UNION  */
  YYSYMBOL_ANON_ENUM = 83,                 /* ANON_ENUM  */
  YYSYMBOL_EXTERN = 84,                    /* EXTERN  */
  YYSYMBOL_VA_ARG = 85,                    /* VA_ARG  */
  YYSYMBOL_86_ = 86,                       /* '='  */
  YYSYMBOL_87_ = 87,                       /* '?'  */
  YYSYMBOL_88_ = 88,                       /* '|'  */
  YYSYMBOL_89_ = 89,                       /* '^'  */
  YYSYMBOL_90_ = 90,                       /* '&'  */
  YYSYMBOL_91_ = 91,                       /* '<'  */
  YYSYMBOL_92_ = 92,                       /* '>'  */
  YYSYMBOL_93_ = 93,                       /* '+'  */
  YYSYMBOL_94_ = 94,                       /* '-'  */
  YYSYMBOL_95_ = 95,                       /* '*'  */
  YYSYMBOL_96_ = 96,                       /* '/'  */
  YYSYMBOL_97_ = 97,                       /* '%'  */
  YYSYMBOL_SIZEOF_TYPE_PREC = 98,          /* SIZEOF_TYPE_PREC  */
  YYSYMBOL_LOWER_THAN_ELSE = 99,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_100_ = 100,                     /* ';'  */
  YYSYMBOL_101_ = 101,                     /* '{'  */
  YYSYMBOL_102_ = 102,                     /* '}'  */
  YYSYMBOL_103_ = 103,                     /* ':'  */
  YYSYMBOL_104_ = 104,                     /* '!'  */
  YYSYMBOL_105_ = 105,                     /* '['  */
  YYSYMBOL_106_ = 106,                     /* ']'  */
  YYSYMBOL_107_ = 107,                     /* '('  */
  YYSYMBOL_108_ = 108,                     /* ')'  */
  YYSYMBOL_109_ = 109,                     /* '~'  */
  YYSYMBOL_110_ = 110,                     /* ','  */
  YYSYMBOL_111_ = 111,                     /* '.'  */
  YYSYMBOL_YYACCEPT = 112,                 /* $accept  */
  YYSYMBOL_program = 113,                  /* program  */
  YYSYMBOL_top_decl_list = 114,            /* top_decl_list  */
  YYSYMBOL_top_decl = 115,                 /* top_decl  */
  YYSYMBOL_native_decl = 116,              /* native_decl  */
  YYSYMBOL_namespace_decl = 117,           /* namespace_decl  */
  YYSYMBOL_118_1 = 118,                    /* $@1  */
  YYSYMBOL_class_decl = 119,               /* class_decl  */
  YYSYMBOL_120_2 = 120,                    /* $@2  */
  YYSYMBOL_class_body = 121,               /* class_body  */
  YYSYMBOL_class_or_struct_kw = 122,       /* class_or_struct_kw  */
  YYSYMBOL_opt_class_final = 123,          /* opt_class_final  */
  YYSYMBOL_opt_base = 124,                 /* opt_base  */
  YYSYMBOL_member_list = 125,              /* member_list  */
  YYSYMBOL_member = 126,                   /* member  */
  YYSYMBOL_access_spec = 127,              /* access_spec  */
  YYSYMBOL_opt_virtual = 128,              /* opt_virtual  */
  YYSYMBOL_opt_const = 129,                /* opt_const  */
  YYSYMBOL_func_name = 130,                /* func_name  */
  YYSYMBOL_operator_symbol = 131,          /* operator_symbol  */
  YYSYMBOL_func_header = 132,              /* func_header  */
  YYSYMBOL_133_3 = 133,                    /* $@3  */
  YYSYMBOL_134_4 = 134,                    /* $@4  */
  YYSYMBOL_implicit_int_header = 135,      /* implicit_int_header  */
  YYSYMBOL_136_5 = 136,                    /* $@5  */
  YYSYMBOL_anon_tag_decl = 137,            /* anon_tag_decl  */
  YYSYMBOL_138_6 = 138,                    /* $@6  */
  YYSYMBOL_func_decl = 139,                /* func_decl  */
  YYSYMBOL_func_def = 140,                 /* func_def  */
  YYSYMBOL_out_of_line_def = 141,          /* out_of_line_def  */
  YYSYMBOL_142_7 = 142,                    /* $@7  */
  YYSYMBOL_143_8 = 143,                    /* $@8  */
  YYSYMBOL_opt_param_list = 144,           /* opt_param_list  */
  YYSYMBOL_param_list = 145,               /* param_list  */
  YYSYMBOL_param = 146,                    /* param  */
  YYSYMBOL_pointer_opt = 147,              /* pointer_opt  */
  YYSYMBOL_tag_def_type = 148,             /* tag_def_type  */
  YYSYMBOL_type_spec = 149,                /* type_spec  */
  YYSYMBOL_name_tok = 150,                 /* name_tok  */
  YYSYMBOL_qname_prefix = 151,             /* qname_prefix  */
  YYSYMBOL_qualified_type = 152,           /* qualified_type  */
  YYSYMBOL_qualified_id_expr = 153,        /* qualified_id_expr  */
  YYSYMBOL_var_decl = 154,                 /* var_decl  */
  YYSYMBOL_more_plain_declarators = 155,   /* more_plain_declarators  */
  YYSYMBOL_func_ptr_param_type = 156,      /* func_ptr_param_type  */
  YYSYMBOL_func_ptr_param_list = 157,      /* func_ptr_param_list  */
  YYSYMBOL_opt_func_ptr_param_list = 158,  /* opt_func_ptr_param_list  */
  YYSYMBOL_array_bracket_list = 159,       /* array_bracket_list  */
  YYSYMBOL_array_dim = 160,                /* array_dim  */
  YYSYMBOL_braced_init = 161,              /* braced_init  */
  YYSYMBOL_init_items = 162,               /* init_items  */
  YYSYMBOL_init_item = 163,                /* init_item  */
  YYSYMBOL_opt_array_initializer = 164,    /* opt_array_initializer  */
  YYSYMBOL_opt_initializer = 165,          /* opt_initializer  */
  YYSYMBOL_tag_typedef_decl = 166,         /* tag_typedef_decl  */
  YYSYMBOL_167_9 = 167,                    /* $@9  */
  YYSYMBOL_typedef_decl = 168,             /* typedef_decl  */
  YYSYMBOL_enum_decl = 169,                /* enum_decl  */
  YYSYMBOL_enumerator_list = 170,          /* enumerator_list  */
  YYSYMBOL_enumerator = 171,               /* enumerator  */
  YYSYMBOL_union_decl = 172,               /* union_decl  */
  YYSYMBOL_union_member_list = 173,        /* union_member_list  */
  YYSYMBOL_block = 174,                    /* block  */
  YYSYMBOL_175_10 = 175,                   /* $@10  */
  YYSYMBOL_stmt_list = 176,                /* stmt_list  */
  YYSYMBOL_stmt = 177,                     /* stmt  */
  YYSYMBOL_178_11 = 178,                   /* $@11  */
  YYSYMBOL_for_open = 179,                 /* for_open  */
  YYSYMBOL_for_init = 180,                 /* for_init  */
  YYSYMBOL_comma_expr = 181,               /* comma_expr  */
  YYSYMBOL_comma_expr_opt = 182,           /* comma_expr_opt  */
  YYSYMBOL_switch_body = 183,              /* switch_body  */
  YYSYMBOL_primary_expr = 184,             /* primary_expr  */
  YYSYMBOL_postfix_expr = 185,             /* postfix_expr  */
  YYSYMBOL_unary_expr = 186,               /* unary_expr  */
  YYSYMBOL_cpp_cast_kw = 187,              /* cpp_cast_kw  */
  YYSYMBOL_expr = 188,                     /* expr  */
  YYSYMBOL_opt_arg_list = 189,             /* opt_arg_list  */
  YYSYMBOL_arg_list = 190,                 /* arg_list  */
  YYSYMBOL_string_seq = 191,               /* string_seq  */
  YYSYMBOL_asm_string_list = 192,          /* asm_string_list  */
  YYSYMBOL_opt_member_init_list = 193,     /* opt_member_init_list  */
  YYSYMBOL_member_init_list = 194,         /* member_init_list  */
  YYSYMBOL_member_init = 195               /* member_init  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Default (constant) value used for initialization for null
   right-hand sides.  Unlike the standard yacc.c template, here we set
   the default value of $$ to a zeroed-out value.  Since the default
   value is undefined, this behavior is technically correct.  */
static YYSTYPE yyval_default;
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;



#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif
#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YYFREE
# define YYFREE free
#endif
#ifndef YYMALLOC
# define YYMALLOC malloc
#endif
#ifndef YYREALLOC
# define YYREALLOC realloc
#endif

#ifdef __cplusplus
  typedef bool yybool;
# define yytrue true
# define yyfalse false
#else
  /* When we move to stdbool, get rid of the various casts to yybool.  */
  typedef signed char yybool;
# define yytrue 1
# define yyfalse 0
#endif

#ifndef YYSETJMP
# include <setjmp.h>
# define YYJMP_BUF jmp_buf
# define YYSETJMP(Env) setjmp (Env)
/* Pacify Clang and ICC.  */
# define YYLONGJMP(Env, Val)                    \
 do {                                           \
   longjmp (Env, Val);                          \
   YY_ASSERT (0);                               \
 } while (yyfalse)
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* The _Noreturn keyword of C11.  */
#ifndef _Noreturn
# if (defined __cplusplus \
      && ((201103 <= __cplusplus && !(__GNUC__ == 4 && __GNUC_MINOR__ == 7)) \
          || (defined _MSC_VER && 1900 <= _MSC_VER)))
#  define _Noreturn [[noreturn]]
# elif ((!defined __cplusplus || defined __clang__) \
        && (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) \
            || (!defined __STRICT_ANSI__ \
                && (4 < __GNUC__ + (7 <= __GNUC_MINOR__) \
                    || (defined __apple_build_version__ \
                        ? 6000000 <= __apple_build_version__ \
                        : 3 < __clang_major__ + (5 <= __clang_minor__))))))
   /* _Noreturn works as-is.  */
# elif (2 < __GNUC__ + (8 <= __GNUC_MINOR__) || defined __clang__ \
        || 0x5110 <= __SUNPRO_C)
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   3080

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  112
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  84
/* YYNRULES -- Number of rules.  */
#define YYNRULES  325
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  704
/* YYMAXRHS -- Maximum number of symbols on right-hand side of rule.  */
#define YYMAXRHS 13
/* YYMAXLEFT -- Maximum number of symbols to the left of a handle
   accessed by $0, $-1, etc., in any rule.  */
#define YYMAXLEFT 0

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   342

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   104,     2,     2,     2,    97,    90,     2,
     107,   108,    95,    93,   110,    94,   111,    96,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   103,   100,
      91,    86,    92,    87,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   105,     2,   106,    89,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   101,    88,   102,   109,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    98,    99
};

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   750,   750,   759,   760,   777,   778,   779,   780,   781,
     782,   783,   784,   785,   786,   791,   801,   802,   807,   814,
     823,   828,   829,   849,   855,   866,   865,   904,   903,   973,
     984,   985,   993,   994,   998,   999,  1009,  1014,  1022,  1023,
    1027,  1032,  1033,  1075,  1076,  1077,  1120,  1151,  1179,  1180,
    1181,  1196,  1197,  1211,  1212,  1213,  1285,  1286,  1290,  1291,
    1292,  1293,  1294,  1295,  1296,  1297,  1298,  1299,  1300,  1301,
    1302,  1303,  1304,  1305,  1306,  1307,  1308,  1309,  1313,  1313,
    1340,  1340,  1350,  1366,  1366,  1377,  1376,  1391,  1397,  1407,
    1424,  1480,  1480,  1504,  1504,  1554,  1573,  1574,  1575,  1579,
    1580,  1584,  1591,  1621,  1639,  1654,  1662,  1668,  1684,  1692,
    1701,  1702,  1703,  1704,  1705,  1706,  1726,  1727,  1728,  1729,
    1733,  1734,  1735,  1736,  1737,  1738,  1739,  1741,  1742,  1743,
    1745,  1746,  1763,  1776,  1790,  1827,  1828,  1832,  1837,  1845,
    1854,  1865,  1874,  1884,  1918,  1978,  1997,  2023,  2049,  2065,
    2085,  2126,  2127,  2136,  2160,  2164,  2171,  2173,  2178,  2179,
    2215,  2220,  2230,  2248,  2275,  2277,  2279,  2284,  2285,  2289,
    2290,  2294,  2295,  2296,  2318,  2319,  2320,  2329,  2331,  2333,
    2336,  2335,  2357,  2364,  2375,  2395,  2423,  2454,  2466,  2467,
    2469,  2474,  2476,  2501,  2512,  2513,  2519,  2519,  2528,  2529,
    2533,  2534,  2539,  2544,  2549,  2561,  2579,  2653,  2675,  2689,
    2697,  2699,  2701,  2703,  2733,  2740,  2753,  2762,  2770,  2778,
    2779,  2786,  2791,  2791,  2797,  2798,  2803,  2812,  2816,  2817,
    2823,  2839,  2840,  2850,  2851,  2867,  2868,  2875,  2881,  2899,
    2900,  2901,  2902,  2903,  2904,  2929,  2930,  2931,  2932,  2933,
    2934,  2935,  2952,  2953,  2974,  2981,  2988,  2994,  3000,  3009,
    3010,  3012,  3014,  3016,  3018,  3020,  3022,  3024,  3026,  3028,
    3043,  3045,  3056,  3063,  3087,  3135,  3148,  3187,  3188,  3189,
    3190,  3194,  3195,  3196,  3197,  3198,  3199,  3200,  3201,  3202,
    3203,  3204,  3205,  3206,  3207,  3208,  3220,  3221,  3222,  3223,
    3224,  3226,  3228,  3230,  3232,  3234,  3236,  3238,  3240,  3242,
    3244,  3246,  3274,  3275,  3279,  3280,  3291,  3292,  3303,  3310,
    3336,  3337,  3341,  3342,  3346,  3355
};
#endif

#define YYPACT_NINF (-500)
#define YYTABLE_NINF (-321)

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -500,   136,   759,  -500,    -9,   123,  -500,  -500,  -500,   202,
     202,   194,  2312,  -500,  -500,  -500,  -500,  -500,  -500,  2317,
     311,   122,   127,  -500,   165,   170,  2173,  -500,   143,  -500,
     175,   202,  1888,   252,   187,   201,  -500,  -500,    29,    71,
     225,    30,   196,   219,   230,   237,   256,   277,  -500,  -500,
    -500,   250,   254,  -500,    85,    98,    29,   202,    29,   356,
    -500,    29,    29,   202,   202,   202,  -500,  -500,  -500,  2317,
    2317,   281,  -500,   381,   202,  -500,   202,  1888,  -500,   287,
      75,   288,  -500,  -500,  -500,  -500,   114,    24,   385,   128,
      29,  -500,  -500,  -500,  -500,  -500,  -500,    79,   389,   286,
    1526,    36,    63,  -500,   123,   391,   345,  -500,  -500,  -500,
    -500,  -500,  -500,  2226,   381,  -500,  -500,   297,   381,  -500,
     299,   202,    86,   281,    38,   202,   202,  -500,  -500,  -500,
      29,    29,  -500,  -500,  2033,   315,   109,  -500,  -500,  -500,
      43,  -500,  -500,  -500,   300,  -500,   295,   363,   303,    66,
    -500,  -500,   312,    95,  1748,   358,   301,  -500,  -500,  -500,
    -500,  2317,  1544,  -500,  -500,  -500,  -500,  1766,  -500,  -500,
    -500,  -500,  1748,  1748,   304,  1748,  1748,  1748,  1748,  -500,
    1406,  1748,   366,  -500,   306,  -500,   173,  -500,   324,  2873,
     410,   150,   988,   253,   331,  1748,   313,  -500,  2226,   316,
    -500,   317,   309,  -500,    29,   163,  2072,  -500,   166,  2087,
    -500,   202,  2125,  -500,  -500,  -500,   318,   329,  1925,  -500,
     326,  1748,  -500,   381,    95,   283,  -500,  2226,   322,   327,
     328,   330,  -500,  -500,  -500,   725,   334,  1192,  -500,  1637,
     134,  -500,  2873,   144,  1748,    13,   332,  -500,  1406,  -500,
    -500,  -500,  1748,  -500,  -500,  -500,  -500,    39,    29,   368,
     184,  2873,  -500,   358,  -500,   433,  -500,  -500,  1748,  1748,
     434,  2317,  1748,  1748,  1748,  1748,  1748,  1748,  1748,  1748,
    1748,  1748,  1748,  1748,  1748,  1748,  1748,  1748,  1748,  1748,
    1748,  1748,  1748,  1748,  1748,  1748,  1748,  1748,  1748,  1748,
    1748,  1748,  -500,   134,  -500,   442,    29,  -500,   336,   339,
     358,   343,    90,  -500,   352,   351,   353,  -500,  2263,    42,
    -500,  -500,  1980,   202,   202,  -500,   448,   357,   132,  -500,
    -500,  -500,  -500,  1888,  -500,  -500,   361,    72,  -500,   186,
    -500,  2873,  -500,   456,   462,   464,   281,   362,  -500,  1748,
    1748,   363,  -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,
    -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,  -500,
     365,   364,  -500,  -500,    52,  2317,  1748,   371,  1299,   373,
     374,   369,   382,   480,   377,    26,   412,  2211,  1873,  -500,
    -500,    87,   387,   390,  -500,  -500,   841,     5,   924,  -500,
    2873,  -500,   388,  -500,  1748,   383,   393,  1748,  1748,  1748,
      29,  2311,   267,  -500,  1748,  -500,  2620,   384,  -500,    29,
     167,   167,   268,   268,  2931,   982,  2873,  2873,  2873,  2873,
     216,   216,  2873,  2873,  2873,  2873,  2873,  2873,  2873,  2726,
    2975,  2983,   251,   268,   268,   227,   227,  -500,  -500,  -500,
    -500,   388,   183,   202,  2317,   400,  -500,  -500,   410,  -500,
     303,   394,  -500,   142,   395,   411,  -500,  -500,  -500,   399,
     416,   420,   421,   202,   414,  -500,   507,  -500,  -500,  1748,
    -500,  -500,  -500,  -500,  -500,   417,   419,  -500,  -500,  -500,
    2226,  1299,   418,   430,  1748,   508,  1748,  -500,  -500,  -500,
     435,  1748,   426,   531,   531,   431,   439,   440,    89,   441,
      55,  -500,  -500,   103,  -500,   444,   418,  -500,  -500,  -500,
     180,  -500,  2873,   388,    29,  2873,  -500,  2673,   437,  -500,
     272,  2317,   447,  1748,  2873,  -500,  -500,   454,  1748,   388,
    1748,   443,  -500,  -500,    68,  2226,  -500,   303,  1748,  1655,
    -500,    33,   453,   546,  -500,  -500,   461,  -500,   467,  2826,
    -500,  -500,   446,  -500,  -500,  2356,   465,  2400,  -500,  2444,
     531,  -500,    54,    49,   531,  -500,  -500,    53,  -500,    21,
      56,  1748,  -500,  1424,   565,  -500,  -500,   514,    29,   466,
    -500,   469,  2917,   471,  2317,   487,  1748,   472,  -500,  2873,
    -500,   475,   474,   476,  2317,  -500,  -500,  -500,  -500,   538,
    1299,  1748,  1299,   484,    77,  -500,  -500,  -500,   486,    80,
      37,  -500,     9,   488,  -500,  -500,    95,   479,   482,  1748,
     483,   485,  -500,   489,   538,  -500,   490,  2317,   492,  -500,
     587,   572,  2488,  -500,  -500,   496,  -500,   498,  2226,  1748,
    1748,   134,  -500,  -500,  2317,  2532,   494,   487,   600,    47,
    2317,   497,  -500,  -500,  1299,   504,  1085,  -500,  -500,   499,
    2576,   501,  -500,   506,  -500,  2317,  -500,   331,  -500,   509,
    -500,  -500,  -500,  1748,   503,  -500,  -500,   516,  1299,  1299,
     511,   512,  -500,  -500,  2776,  -500,  -500,  -500,  -500,  1748,
     331,  -500,  -500,  -500
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       3,     0,     2,     1,   135,   125,   126,    30,    31,     0,
       0,     0,     0,   120,   121,   122,   123,   124,    52,     0,
       0,     0,     0,    85,     0,     0,    51,     4,     0,     5,
     116,     0,     0,     0,   119,     0,     9,    22,   110,   110,
       0,     0,   130,     0,     0,     0,   118,   117,    83,   135,
     136,   129,   128,    25,     0,     0,   110,   180,   110,     0,
     130,   110,   110,     0,     0,     0,   134,    23,    24,     0,
       0,     0,   194,     0,     0,   116,     0,     0,   119,     0,
     110,     0,   118,   117,    21,     6,   127,   125,     0,    89,
     110,    17,   196,    18,    14,    10,   114,   113,     0,     0,
       0,     0,     0,   137,   139,     0,     0,    93,    11,    13,
      12,     7,     8,    96,     0,    20,   194,     0,     0,   194,
     128,     0,   127,     0,     0,     0,     0,   129,   128,   127,
     110,   110,    38,    86,     0,   191,     0,   188,    89,    16,
       0,    15,    33,    19,    34,    80,     0,     0,     0,     0,
     198,   115,   111,   174,     0,   239,   136,   316,   240,   243,
     241,     0,     0,   248,   245,   246,   247,     0,   277,   280,
     278,   279,     0,     0,     0,     0,     0,     0,     0,   161,
       0,     0,     0,   249,     0,   252,   259,   281,     0,   163,
     242,   174,   158,     0,   171,     0,     0,   138,    96,   122,
     107,     0,    98,    99,   110,     0,     0,     3,     0,     0,
     177,     0,   158,   184,   179,   178,     0,     0,    51,    87,
       0,     0,    88,   190,   174,     0,    27,    96,     0,     0,
       0,   321,   322,    90,    56,     0,     0,     0,   112,     0,
     171,   151,   314,     0,   312,   267,     0,   270,     0,   275,
     265,   266,     0,   263,   262,   264,   260,   125,   110,     0,
       0,   231,   261,   140,   160,     0,   257,   258,     0,   312,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   317,   171,   151,     0,   110,   156,   159,     0,
      56,     0,     0,   146,     0,     0,     0,    84,     0,   105,
     187,   193,    51,     0,     0,   181,     0,     0,     0,   132,
      48,    49,    50,     0,    29,    39,     0,     0,    43,     0,
     195,   192,   189,     0,     0,     0,     0,     0,    82,   312,
     312,     0,    64,    65,    68,    69,    70,    71,    72,    73,
      76,    77,    62,    66,    67,    58,    59,    60,    61,    63,
       0,     0,    57,    78,   239,     0,   233,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   226,
     197,   110,     0,     0,   200,   199,   228,     0,     0,   176,
     175,   151,   141,   144,     0,     0,   313,     0,   312,     0,
     110,     0,     0,   250,     0,   255,     0,     0,   254,   110,
     291,   292,   289,   290,   293,   294,   301,   302,   303,   304,
     298,   299,   305,   306,   308,   309,   310,   307,   300,     0,
     296,   297,   295,   287,   288,   285,   286,   282,   283,   284,
     151,   143,     0,   154,     0,     0,    91,   172,   173,   162,
       0,   320,   100,   101,     0,     0,    26,   183,   182,     0,
       0,     0,     0,     0,     0,    40,     0,    41,    44,     0,
      35,    36,    37,    28,    81,     0,     0,   323,    74,    75,
      96,     0,   234,     0,     0,     0,     0,   227,   210,   211,
       0,     0,     0,     0,     0,     0,     0,     0,   110,     0,
       0,   219,   224,   110,   229,     0,   230,   225,   164,   170,
       0,   167,   169,   142,   110,   315,   251,     0,     0,   271,
       0,     0,     0,     0,   232,   256,   253,     0,     0,   145,
       0,     0,   155,   157,     0,    96,    95,     0,     0,     0,
     106,     0,     0,     0,   133,   131,   127,    47,     0,     0,
     324,   325,     0,   213,   209,     0,     0,     0,   212,     0,
       0,   318,     0,     0,     0,   208,   221,     0,   220,   174,
       0,   233,   165,     0,     0,   269,   268,   274,   110,     0,
     273,     0,   311,     0,   158,   174,     0,     0,    94,   102,
     103,     0,     0,     0,   158,   186,    46,    42,    45,    53,
       0,     0,     0,     0,     0,   319,   214,   218,     0,     0,
     174,   222,   174,     0,   166,   168,   174,     0,     0,     0,
       0,     0,   148,     0,    53,   104,     0,   158,     0,    54,
      79,   201,     0,   203,   235,     0,   215,     0,    96,     0,
     233,   171,   152,   244,   158,     0,     0,   174,     0,     0,
     158,     0,   185,    55,     0,     0,     0,   217,   216,     0,
       0,     0,   153,     0,   276,   158,   147,   171,    92,     0,
     109,   202,   204,     0,     0,   205,   238,     0,     0,     0,
       0,     0,   150,   108,     0,   237,   223,   206,   207,     0,
     171,   236,   272,   149
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -500,  -500,   415,  -500,  -500,  -500,  -500,     6,  -500,  -114,
      35,  -500,  -500,  -500,  -500,  -500,   598,    -8,   432,  -500,
     -50,  -500,  -500,  -500,  -500,    10,  -500,   -15,   413,  -500,
    -500,  -500,  -195,  -500,   314,    23,  -500,     3,   445,    -2,
      17,  -500,    61,  -279,   176,  -500,  -198,  -147,  -191,  -174,
    -500,    46,  -233,  -152,  -500,  -500,    18,    11,   131,   425,
      19,   177,   -31,  -500,  -500,  -362,  -500,  -500,  -500,  -154,
    -499,  -500,  -500,  -500,  -124,  -500,   458,  -241,   495,   321,
    -440,   178,  -500,   285
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,    27,    28,    29,   117,    75,   346,   133,
      65,   144,   226,   218,   335,   336,    32,   640,   236,   372,
      89,   490,   227,    33,   113,    78,    71,    35,    36,    37,
     545,   198,   201,   202,   203,    98,    38,   306,    40,   182,
      60,   183,   392,   402,   307,   308,   309,   102,   184,   519,
     520,   521,   313,   304,    44,   123,   393,    82,   136,   137,
      83,   134,   394,   150,   237,   395,   648,   396,   515,   397,
     493,   666,   185,   186,   187,   188,   261,   405,   406,   190,
     572,   148,   231,   232
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      41,   241,    93,   316,   314,    39,   240,   401,    30,   211,
      59,    79,    34,    46,   327,    58,   495,    59,    56,    42,
      45,    47,    66,    61,    59,   451,   260,   138,   417,    80,
      59,    62,   347,    49,   104,    90,   602,    31,   247,   191,
      50,    49,    50,   249,   303,   463,   224,    57,   250,   251,
     663,   253,   254,   255,   256,   615,   620,   262,   579,   622,
     615,    76,   101,    43,   573,   399,   194,    59,    59,   234,
     450,   595,   130,   131,    99,    59,  -136,   303,    99,   121,
      90,   124,   623,   615,   125,   126,   615,    81,    49,    50,
      99,  -136,    99,   142,   260,   239,   157,   235,    48,   193,
     502,    49,    50,   140,  -135,   517,    99,   239,   485,   486,
     235,    59,   649,   149,   100,   414,   204,   233,   407,    96,
     408,   142,   523,   239,    97,   151,   100,   503,   621,   563,
     614,   145,    59,   504,   619,   471,     3,    80,   457,   105,
     472,   603,   100,   192,   -56,   212,   244,   464,    92,   465,
     192,   671,   617,   216,   217,   491,   616,   618,   476,    59,
     192,    96,   192,   192,   245,    96,    97,   528,   195,    76,
      97,   539,   477,   596,   152,  -136,   100,    96,   259,    96,
     100,   239,    97,   258,    97,   645,   118,   -32,   647,   -32,
      59,   398,   100,    96,   100,   220,    59,    53,    97,   119,
     100,   204,  -135,   337,    59,    49,    50,    59,   100,    80,
      59,   222,    80,    69,   143,   -32,    59,   -32,    70,   223,
     312,    80,   492,   274,   275,    59,   265,   319,   548,  -320,
     204,   147,   483,   282,   283,   259,   239,   266,   267,   195,
     391,    76,   516,    84,    76,   205,   259,   549,   641,   208,
     643,   410,   403,    76,   404,   100,   310,    50,   295,   296,
     297,   298,   299,   300,   301,   320,    72,   220,   323,    59,
     220,    73,    76,   223,   419,    85,   223,   103,   268,   339,
     269,   412,   582,   474,   270,   529,   478,    94,   540,   479,
     583,   541,   413,   206,   414,   562,   209,   235,   343,   344,
     345,    95,   681,   107,   686,   272,   273,   274,   275,   297,
     298,   299,   300,   301,    67,    68,    59,   282,   283,   108,
      41,   204,   299,   300,   301,    39,   697,   698,    30,   453,
     109,    59,    34,    46,   282,   283,    90,   110,   507,    42,
      45,    47,   295,   296,   297,   298,   299,   300,   301,   593,
     597,   114,    91,    92,   115,   116,   111,    31,   601,    49,
     104,   297,   298,   299,   300,   301,   229,   230,   473,   263,
      50,   263,   104,    59,   532,   533,   259,   112,    58,   532,
     587,   391,   132,    43,   135,    59,    59,   139,   141,   146,
      80,   508,   153,   154,   259,   196,   631,   197,   207,   513,
     116,   221,   228,   225,    92,   633,   638,   238,   244,   590,
    -135,   252,   264,    76,   510,   271,   302,   312,   672,   318,
     315,   329,    76,    76,   -97,   317,   340,   492,   328,   546,
     348,    76,   303,   530,   349,   350,   415,   418,   409,   661,
     351,   373,   537,   632,   692,   452,   454,   455,   506,   509,
     456,   469,    59,   669,    51,    52,   673,   514,   459,   460,
     480,   461,   679,   590,   475,   470,   481,   703,   482,   498,
     484,   488,   489,   303,   652,   303,    86,   691,   494,   651,
     496,   497,   499,   500,   501,   505,   106,   511,    59,   259,
     512,   526,   536,   204,   391,   544,   492,   147,   524,    51,
     120,   550,   122,   404,   106,   676,   551,   552,   127,   128,
     129,   553,   554,   555,   557,   558,   598,   155,   156,   120,
     157,   122,   158,   159,   160,   560,    76,   561,   414,    59,
     564,   577,   566,   570,   588,   568,   580,   571,   574,   575,
     576,   578,   589,    59,   581,   586,   591,   584,   204,   605,
     594,   161,   162,   163,   609,   164,   165,   166,   189,   167,
     604,   606,   168,   169,   170,   171,   210,   607,   626,   213,
     214,   215,   611,   239,   628,   702,   629,   630,   172,   173,
     634,   635,   636,   637,   639,   644,   646,   653,   650,   654,
     663,   656,    59,   657,   664,   658,   667,   660,   668,   174,
     662,   675,    59,   677,   682,   680,   695,   687,   259,   689,
     259,   627,   242,   391,   690,   391,   696,   693,   178,   699,
     700,   180,   322,   181,    77,   311,   659,   106,   678,   625,
     543,   338,   462,   458,     0,    59,   487,     0,   106,   547,
       0,     0,     0,     0,     0,    76,    59,    76,   342,   243,
       0,   204,    59,   189,     0,     0,   325,     0,    59,     0,
       0,     0,   259,     0,   259,     0,     0,   391,     0,   391,
       0,     0,     0,    59,     0,     0,     0,     0,     0,   341,
       0,     0,     0,     0,     0,     0,   259,   259,     0,     0,
       0,   391,   391,     0,     0,     0,     0,   400,     0,    76,
       0,    76,   242,     0,   106,     0,     0,     0,     0,     0,
     411,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    76,    76,     0,   416,   242,     0,     0,
     420,   421,   422,   423,   424,   425,   426,   427,   428,   429,
     430,   431,   432,   433,   434,   435,   436,   437,   438,   439,
     440,   441,   442,   443,   444,   445,   446,   447,   448,   449,
       0,     0,     4,     5,     6,     0,     0,     0,   467,   468,
       7,     8,     9,    10,     0,     0,     0,    11,    12,   352,
     353,   354,   355,     0,     0,   356,   357,   358,   359,   360,
     361,    13,    14,    15,    16,    17,     0,     0,     0,    18,
       0,     0,     0,     0,     0,    19,     0,   242,   242,     0,
       0,   362,     0,     0,     0,     0,   363,   364,   365,   366,
     367,   368,     0,     0,     0,     0,     0,     0,     0,   369,
     370,     0,   371,     0,    20,     0,     0,    21,    22,     0,
      23,    24,    25,    26,   155,   257,     6,   157,     0,   158,
     159,   160,     7,     8,     9,    74,   522,     0,     0,     0,
       0,     0,   525,     0,     0,   527,   242,     0,   -51,     0,
       0,     0,   534,    13,    14,    15,    16,    17,   161,   162,
     163,     0,   164,   165,   166,     0,   167,    19,     0,   168,
     169,   170,   171,     0,     0,     0,     0,     0,   542,     0,
       0,     0,     0,     0,     0,   172,   173,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   556,    21,
      22,     0,    23,    24,    25,     0,   174,   155,   156,     0,
     157,   175,   158,   159,   160,   176,   177,   559,     0,     0,
       0,     0,     0,     0,     0,   178,     0,     0,   180,     0,
     181,     0,   565,     0,   567,     0,     0,     0,     0,   569,
       0,   161,   162,   163,     0,   164,   165,   166,     0,   167,
       0,     0,   168,   169,   170,   171,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   172,   173,
       0,    49,     5,     6,     0,     0,   592,     0,   189,     7,
       8,    63,    64,     0,     0,     0,   599,   189,     0,   174,
       0,     0,     0,     0,   175,     0,     0,     0,   176,   177,
      13,    14,    15,    16,    17,   398,   518,     0,   178,     0,
       0,   180,     0,   181,    19,     0,   272,   273,   274,   275,
     276,   522,     0,     0,     0,     0,     0,     0,   282,   283,
       0,     0,     0,     0,   189,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    21,    22,     0,   642,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
       0,     0,     0,   305,     0,     0,     0,   655,   374,   257,
       6,   157,     0,   158,   159,   160,     7,     8,     9,    74,
       0,     0,     0,     0,   375,   376,   377,   670,   378,   379,
     380,   381,   382,   383,   384,   683,   684,    13,    14,    15,
      16,    17,   161,   162,   163,     0,   164,   165,   166,     0,
     167,    19,     0,   168,   169,   170,   171,     0,     0,     0,
       0,   694,     0,     0,     0,     0,     0,     0,     0,   172,
     173,     0,     0,     0,     0,     0,     0,     0,   385,   386,
       0,     0,   387,    21,    22,     0,    23,    24,    25,   388,
     174,     0,     0,     0,     0,   175,     0,     0,     0,   176,
     177,     0,     0,     0,     0,   389,    92,   685,     0,   178,
       0,     0,   180,     0,   181,   374,   257,     6,   157,     0,
     158,   159,   160,     7,     8,     9,    74,     0,     0,     0,
       0,   375,   376,   377,     0,   378,   379,   380,   381,   382,
     383,   384,     0,     0,    13,    14,    15,    16,    17,   161,
     162,   163,     0,   164,   165,   166,     0,   167,    19,     0,
     168,   169,   170,   171,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   172,   173,     0,     0,
       0,     0,     0,     0,     0,   385,   386,     0,     0,   387,
      21,    22,     0,    23,    24,    25,   388,   174,     0,     0,
       0,     0,   175,     0,     0,     0,   176,   177,     0,     0,
       0,     0,   389,    92,   390,     0,   178,     0,     0,   180,
       0,   181,   374,   257,     6,   157,     0,   158,   159,   160,
       7,     8,     9,    74,     0,     0,     0,     0,   375,   376,
     377,     0,   378,   379,   380,   381,   382,   383,   384,     0,
       0,    13,    14,    15,    16,    17,   161,   162,   163,     0,
     164,   165,   166,     0,   167,    19,     0,   168,   169,   170,
     171,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   172,   173,     0,     0,     0,     0,     0,
       0,     0,   385,   386,     0,     0,   387,    21,    22,     0,
      23,    24,    25,   388,   174,     0,     0,     0,     0,   175,
       0,     0,     0,   176,   177,     0,     0,     0,     0,   389,
      92,     0,     0,   178,     0,     0,   180,     0,   181,   155,
     257,     6,   157,     0,   158,   159,   160,     7,     8,    63,
      64,     0,     0,     0,     0,     0,     0,   155,   156,     0,
     157,     0,   158,   159,   160,     0,     0,     0,    13,    14,
      15,    16,    17,   161,   162,   163,     0,   164,   165,   166,
       0,   167,    19,     0,   168,   169,   170,   171,     0,     0,
       0,   161,   162,   163,     0,   164,   165,   166,     0,   167,
     172,   173,   168,   169,   170,   171,     0,     0,     0,     0,
       0,     0,     0,     0,    21,    22,     0,     0,   172,   173,
       0,   174,     0,     0,     0,     0,   175,     0,     0,     0,
     176,   177,     0,     0,     0,     0,     0,     0,     0,   174,
     178,     0,     0,   180,   175,   181,     0,     0,   176,   177,
       0,     0,     0,     0,     0,   398,   624,     0,   178,   155,
     156,   180,   157,   181,   158,   159,   160,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   155,   156,     0,
     157,     0,   158,   159,   160,     0,     0,     0,     0,     0,
       0,     0,     0,   161,   162,   163,     0,   164,   165,   166,
       0,   167,     0,     0,   168,   169,   170,   171,     0,     0,
       0,   161,   162,   163,     0,   164,   165,   166,     0,   167,
     172,   173,   168,   169,   170,   171,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   172,   173,
       0,   174,     0,     0,     0,     0,   175,     0,     0,     0,
     176,   177,     0,     0,     0,     0,     0,     0,     0,   174,
     178,     0,   179,   180,   175,   181,     0,     0,   176,   177,
     155,   156,     0,   157,     0,   158,   159,   160,   178,   246,
       0,   180,     0,   181,     0,     0,     0,     0,   155,   156,
       0,   157,     0,   158,   159,   160,     0,     0,     0,     0,
       0,     0,     0,     0,   161,   162,   163,     0,   164,   165,
     166,     0,   167,     0,     0,   168,   169,   170,   171,     0,
       0,     0,   161,   162,   163,     0,   164,   165,   166,     0,
     167,   172,   173,   168,   169,   170,   171,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   172,
     173,     0,   174,     0,     0,     0,     0,   175,     0,     0,
       0,   176,   177,     0,     0,     0,     0,     0,   398,     0,
     174,   178,     0,     0,   180,   175,   181,     0,     0,   176,
     177,   155,   156,     0,   157,     0,   158,   159,   160,   178,
       0,   600,   180,     0,   181,     0,     0,     0,     0,   155,
     156,     0,   157,     0,   158,   159,   160,     0,     0,     0,
       0,     0,     0,     0,     0,   161,   162,   163,     0,   164,
     165,   166,     0,   167,     0,     0,   168,   169,   170,   171,
       0,     0,     0,   161,   162,   163,     0,   164,   165,   166,
       0,   167,   172,   173,   168,   169,   170,   171,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     172,   173,     0,   174,     0,     0,     0,     0,   175,     0,
       0,     0,   176,   177,     0,     0,     0,     0,     0,     0,
       0,   174,   178,     0,     0,   180,   175,   181,     0,     0,
     176,   177,     0,     0,     0,     0,     0,     0,     0,     0,
     178,     0,     0,   248,     0,   181,    49,    87,     6,     0,
       0,     0,     0,     0,     7,     8,     9,    74,     0,     0,
       0,    49,    87,     6,     0,     0,     0,     0,     0,     7,
       8,    63,    64,     0,     0,    13,    14,    15,    16,    17,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    19,
      13,    14,    15,    16,    17,     0,     0,     0,    49,     5,
       6,     0,     0,     0,    19,     0,     7,     8,     9,    74,
     330,   331,   332,     0,     0,     0,     0,     0,     0,     0,
       0,    21,    22,     0,    23,    24,    25,    13,    14,    15,
      16,    17,     0,     0,     0,    18,    21,    22,     0,     0,
       0,    19,   333,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    88,     4,     5,     6,     0,     0,     0,     0,
       0,     7,     8,     9,    10,     0,     0,    88,    11,    12,
       0,     0,     0,    21,    22,     0,    23,    24,    25,     0,
       0,     0,    13,    14,    15,    16,    17,     0,     0,     0,
      18,     0,     0,     0,     0,     0,    19,   334,     0,     0,
       0,     0,     0,     0,     0,     0,    49,     5,     6,     0,
       0,     0,     0,     0,     7,     8,     9,    74,     0,     0,
       0,     0,     0,     0,     0,    20,     0,     0,    21,    22,
       0,    23,    24,    25,    26,    13,    14,    15,    16,    17,
       0,     0,     0,     0,     0,    49,     5,     6,     0,    19,
       0,     0,   466,     7,     8,     9,    74,     0,     0,     0,
      49,     5,     6,     0,     0,     0,     0,     0,     7,     8,
       9,    74,     0,     0,    13,    14,    15,    16,    17,     0,
       0,    21,    22,     0,    23,    24,    25,     0,    19,    13,
      14,    15,    16,    17,     0,     0,     0,     0,    49,     5,
       6,     0,     0,    19,     0,   219,     7,     8,    63,    64,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      21,    22,     0,    23,    24,    25,     0,    13,    14,    15,
      16,    17,     0,     0,     0,    21,    22,     0,    23,    24,
      25,    19,     0,     0,   321,     0,    49,     5,     6,     0,
       0,     0,     0,     0,     7,     8,     9,    74,     0,   324,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    21,    22,    13,    14,    15,    16,    17,
       0,     0,     0,    18,    49,     5,     6,     0,     0,    19,
     326,     0,     7,     8,     9,    74,     0,     0,     0,    49,
       5,     6,     0,     0,     0,     0,     0,     7,     8,    63,
      64,     0,     0,    13,    14,    15,    16,    17,     0,     0,
       0,    21,    22,     0,    23,    24,    25,    19,    13,    14,
     199,    16,    17,     0,     0,     0,    49,     5,     6,     0,
       0,     0,    19,     0,     7,     8,    63,    64,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    21,
      22,     0,    23,    24,    25,    13,    14,    15,    16,    17,
       0,     0,     0,     0,    21,    22,   200,     0,     0,    19,
       0,     0,     0,     0,     0,    49,     5,     6,     0,     0,
      49,     5,     6,     7,     8,    54,    55,     0,     7,     8,
      63,    64,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    21,    22,   200,    13,    14,    15,    16,    17,    13,
      14,    15,    16,    17,     0,     0,     0,     0,    19,     0,
       0,     0,     0,    19,     0,   272,   273,   274,   275,   276,
     277,   278,   279,   280,   281,     0,     0,   282,   283,   284,
     285,   286,   287,   288,     0,     0,     0,   289,     0,     0,
      21,    22,     0,     0,     0,    21,    22,   290,   291,   292,
     293,   294,   295,   296,   297,   298,   299,   300,   301,     0,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
       0,   531,   282,   283,   284,   285,   286,   287,   288,     0,
       0,     0,   289,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   610,     0,   282,   283,   284,   285,
     286,   287,   288,     0,     0,     0,   289,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   281,   612,     0,
     282,   283,   284,   285,   286,   287,   288,     0,     0,     0,
     289,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,   301,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   613,     0,   282,   283,   284,   285,   286,   287,
     288,     0,     0,     0,   289,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   665,     0,   282,   283,
     284,   285,   286,   287,   288,     0,     0,     0,   289,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     674,     0,   282,   283,   284,   285,   286,   287,   288,     0,
       0,     0,   289,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   688,     0,   282,   283,   284,   285,
     286,   287,   288,     0,     0,     0,   289,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,     0,     0,
       0,     0,     0,     0,     0,     0,   535,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,     0,     0,   282,
     283,   284,   285,   286,   287,   288,     0,     0,     0,   289,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   290,
     291,   292,   293,   294,   295,   296,   297,   298,   299,   300,
     301,     0,     0,     0,     0,     0,     0,     0,     0,   585,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
       0,     0,   282,   283,   284,   285,   286,   287,   288,     0,
       0,     0,   289,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,     0,     0,     0,     0,     0,   538,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
       0,     0,   282,   283,   284,   285,   286,   287,   288,     0,
       0,     0,   289,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,     0,     0,     0,     0,     0,   701,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
       0,     0,   282,   283,   284,   285,   286,   287,   288,     0,
       0,     0,   289,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,     0,     0,   608,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,     0,     0,   282,
     283,   284,   285,   286,   287,   288,     0,     0,     0,   289,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   290,
     291,   292,   293,   294,   295,   296,   297,   298,   299,   300,
     301,   272,   273,   274,   275,   276,   277,     0,     0,     0,
       0,     0,     0,   282,   283,   272,   273,   274,   275,     0,
       0,     0,     0,     0,     0,     0,     0,   282,   283,     0,
       0,     0,     0,     0,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   301,     0,     0,     0,     0,   292,
     293,   294,   295,   296,   297,   298,   299,   300,   301,   272,
     273,   274,   275,     0,     0,     0,     0,   272,   273,   274,
     275,   282,   283,     0,     0,     0,     0,     0,     0,   282,
     283,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   293,   294,   295,   296,   297,   298,
     299,   300,   301,   294,   295,   296,   297,   298,   299,   300,
     301
};

static const yytype_int16 yycheck[] =
{
       2,   153,    33,   198,   195,     2,   153,   240,     2,   123,
      12,    26,     2,     2,   212,    12,   378,    19,    12,     2,
       2,     2,    19,    12,    26,   304,   180,    77,   269,    26,
      32,    12,   227,     3,     4,    32,     3,     2,   162,     3,
       4,     3,     4,   167,   191,     3,     3,    12,   172,   173,
       3,   175,   176,   177,   178,     6,     3,   181,     3,     3,
       6,    26,    39,     2,   504,   239,     3,    69,    70,     3,
     303,     3,    69,    70,     3,    77,    52,   224,     3,    56,
      77,    58,   581,     6,    61,    62,     6,    26,     3,     4,
       3,    52,     3,     7,   248,    86,     6,    44,   107,   101,
      74,     3,     4,    80,    52,   100,     3,    86,   349,   350,
      44,   113,   103,    90,   105,   110,   113,   148,   105,    90,
     107,     7,   401,    86,    95,    46,   105,   101,   107,   491,
     570,   107,   134,   107,   574,     3,     0,   134,   312,   109,
       8,   108,   105,   107,   107,   107,   107,   105,   101,   107,
     107,   650,   103,   130,   131,   103,   102,   108,    86,   161,
     107,    90,   107,   107,   161,    90,    95,   408,   105,   134,
      95,   450,   100,   105,    95,    52,   105,    90,   180,    90,
     105,    86,    95,   180,    95,   108,   101,   101,   108,   103,
     192,   101,   105,    90,   105,   134,   198,     3,    95,   101,
     105,   198,    52,   218,   206,     3,     4,   209,   105,   206,
     212,   102,   209,    91,   100,   101,   218,   103,    91,   110,
      86,   218,   376,    56,    57,   227,    53,   204,    86,   101,
     227,   103,   346,    66,    67,   237,    86,    64,    65,   105,
     237,   206,   396,   100,   209,   114,   248,   105,   610,   118,
     612,   248,   108,   218,   110,   105,     3,     4,    91,    92,
      93,    94,    95,    96,    97,   102,   101,   206,   102,   271,
     209,   101,   237,   110,   271,   100,   110,    52,   105,   218,
     107,   258,   102,   333,   111,   409,   100,   100,   105,   103,
     110,   108,   108,   116,   110,   490,   119,    44,    15,    16,
      17,   100,   664,   107,   666,    54,    55,    56,    57,    93,
      94,    95,    96,    97,     3,     4,   318,    66,    67,   100,
     322,   318,    95,    96,    97,   322,   688,   689,   322,   306,
     100,   333,   322,   322,    66,    67,   333,   100,   388,   322,
     322,   322,    91,    92,    93,    94,    95,    96,    97,   540,
     545,   101,   100,   101,   100,   101,   100,   322,   549,     3,
       4,    93,    94,    95,    96,    97,     3,     4,   333,     3,
       4,     3,     4,   375,   107,   108,   378,   100,   375,   107,
     108,   378,   101,   322,     3,   387,   388,   100,   100,     4,
     387,   388,     3,   107,   396,     4,   594,    52,   101,   396,
     101,    86,   107,   103,   101,   596,   604,    95,   107,   533,
      52,   107,   106,   378,   391,    91,     6,    86,   651,   110,
     107,    92,   387,   388,   108,   108,   100,   581,   110,   460,
     108,   396,   579,   410,   107,   107,     3,     3,   106,   637,
     110,   107,   419,   595,   677,     3,   110,   108,   387,   388,
     107,     3,   454,   648,     9,    10,   654,   396,   106,   108,
       4,   108,   660,   587,   103,   108,     4,   700,     4,   100,
     108,   106,   108,   620,   626,   622,    31,   675,   107,   626,
     107,   107,   100,     3,   107,    73,    41,   100,   490,   491,
     100,   108,   108,   490,   491,    95,   650,   103,   110,    54,
      55,   106,    57,   110,    59,   657,    95,   108,    63,    64,
      65,    95,    92,    92,   100,     8,   547,     3,     4,    74,
       6,    76,     8,     9,    10,   108,   491,   108,   110,   531,
     100,   508,    24,   107,   531,   100,   513,     6,   107,   100,
     100,   100,    95,   545,   100,   108,    92,   524,   545,     3,
     107,    37,    38,    39,   108,    41,    42,    43,   100,    45,
     107,   100,    48,    49,    50,    51,   121,   100,     3,   124,
     125,   126,   107,    86,   108,   699,   107,   106,    64,    65,
     108,   106,   108,   107,    46,   101,   100,   108,   100,   107,
       3,   108,   594,   108,    22,   106,   100,   107,   100,    85,
     108,   107,   604,     3,   100,   108,   103,   108,   610,   108,
     612,   588,   154,   610,   108,   612,   100,   108,   104,   108,
     108,   107,   207,   109,    26,   193,   634,   182,   659,   583,
     454,   218,   318,   312,    -1,   637,   351,    -1,   193,   461,
      -1,    -1,    -1,    -1,    -1,   610,   648,   612,   223,   154,
      -1,   648,   654,   195,    -1,    -1,   211,    -1,   660,    -1,
      -1,    -1,   664,    -1,   666,    -1,    -1,   664,    -1,   666,
      -1,    -1,    -1,   675,    -1,    -1,    -1,    -1,    -1,   221,
      -1,    -1,    -1,    -1,    -1,    -1,   688,   689,    -1,    -1,
      -1,   688,   689,    -1,    -1,    -1,    -1,   239,    -1,   664,
      -1,   666,   244,    -1,   259,    -1,    -1,    -1,    -1,    -1,
     252,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   688,   689,    -1,   268,   269,    -1,    -1,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
      -1,    -1,     3,     4,     5,    -1,    -1,    -1,   323,   324,
      11,    12,    13,    14,    -1,    -1,    -1,    18,    19,    54,
      55,    56,    57,    -1,    -1,    60,    61,    62,    63,    64,
      65,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    -1,    -1,    -1,    -1,    46,    -1,   349,   350,    -1,
      -1,    86,    -1,    -1,    -1,    -1,    91,    92,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,
     105,    -1,   107,    -1,    75,    -1,    -1,    78,    79,    -1,
      81,    82,    83,    84,     3,     4,     5,     6,    -1,     8,
       9,    10,    11,    12,    13,    14,   398,    -1,    -1,    -1,
      -1,    -1,   404,    -1,    -1,   407,   408,    -1,   109,    -1,
      -1,    -1,   414,    32,    33,    34,    35,    36,    37,    38,
      39,    -1,    41,    42,    43,    -1,    45,    46,    -1,    48,
      49,    50,    51,    -1,    -1,    -1,    -1,    -1,   453,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   473,    78,
      79,    -1,    81,    82,    83,    -1,    85,     3,     4,    -1,
       6,    90,     8,     9,    10,    94,    95,   479,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   104,    -1,    -1,   107,    -1,
     109,    -1,   494,    -1,   496,    -1,    -1,    -1,    -1,   501,
      -1,    37,    38,    39,    -1,    41,    42,    43,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    64,    65,
      -1,     3,     4,     5,    -1,    -1,   538,    -1,   540,    11,
      12,    13,    14,    -1,    -1,    -1,   548,   549,    -1,    85,
      -1,    -1,    -1,    -1,    90,    -1,    -1,    -1,    94,    95,
      32,    33,    34,    35,    36,   101,   102,    -1,   104,    -1,
      -1,   107,    -1,   109,    46,    -1,    54,    55,    56,    57,
      58,   583,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,
      -1,    -1,    -1,    -1,   596,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    -1,   611,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      -1,    -1,    -1,    95,    -1,    -1,    -1,   629,     3,     4,
       5,     6,    -1,     8,     9,    10,    11,    12,    13,    14,
      -1,    -1,    -1,    -1,    19,    20,    21,   649,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    -1,    41,    42,    43,    -1,
      45,    46,    -1,    48,    49,    50,    51,    -1,    -1,    -1,
      -1,   683,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,
      -1,    -1,    77,    78,    79,    -1,    81,    82,    83,    84,
      85,    -1,    -1,    -1,    -1,    90,    -1,    -1,    -1,    94,
      95,    -1,    -1,    -1,    -1,   100,   101,   102,    -1,   104,
      -1,    -1,   107,    -1,   109,     3,     4,     5,     6,    -1,
       8,     9,    10,    11,    12,    13,    14,    -1,    -1,    -1,
      -1,    19,    20,    21,    -1,    23,    24,    25,    26,    27,
      28,    29,    -1,    -1,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    41,    42,    43,    -1,    45,    46,    -1,
      48,    49,    50,    51,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    64,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    -1,    -1,    77,
      78,    79,    -1,    81,    82,    83,    84,    85,    -1,    -1,
      -1,    -1,    90,    -1,    -1,    -1,    94,    95,    -1,    -1,
      -1,    -1,   100,   101,   102,    -1,   104,    -1,    -1,   107,
      -1,   109,     3,     4,     5,     6,    -1,     8,     9,    10,
      11,    12,    13,    14,    -1,    -1,    -1,    -1,    19,    20,
      21,    -1,    23,    24,    25,    26,    27,    28,    29,    -1,
      -1,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      41,    42,    43,    -1,    45,    46,    -1,    48,    49,    50,
      51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    -1,    -1,    77,    78,    79,    -1,
      81,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    90,
      -1,    -1,    -1,    94,    95,    -1,    -1,    -1,    -1,   100,
     101,    -1,    -1,   104,    -1,    -1,   107,    -1,   109,     3,
       4,     5,     6,    -1,     8,     9,    10,    11,    12,    13,
      14,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,    -1,
       6,    -1,     8,     9,    10,    -1,    -1,    -1,    32,    33,
      34,    35,    36,    37,    38,    39,    -1,    41,    42,    43,
      -1,    45,    46,    -1,    48,    49,    50,    51,    -1,    -1,
      -1,    37,    38,    39,    -1,    41,    42,    43,    -1,    45,
      64,    65,    48,    49,    50,    51,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    78,    79,    -1,    -1,    64,    65,
      -1,    85,    -1,    -1,    -1,    -1,    90,    -1,    -1,    -1,
      94,    95,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    85,
     104,    -1,    -1,   107,    90,   109,    -1,    -1,    94,    95,
      -1,    -1,    -1,    -1,    -1,   101,   102,    -1,   104,     3,
       4,   107,     6,   109,     8,     9,    10,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,    -1,
       6,    -1,     8,     9,    10,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    39,    -1,    41,    42,    43,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    -1,    -1,
      -1,    37,    38,    39,    -1,    41,    42,    43,    -1,    45,
      64,    65,    48,    49,    50,    51,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    64,    65,
      -1,    85,    -1,    -1,    -1,    -1,    90,    -1,    -1,    -1,
      94,    95,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    85,
     104,    -1,   106,   107,    90,   109,    -1,    -1,    94,    95,
       3,     4,    -1,     6,    -1,     8,     9,    10,   104,   105,
      -1,   107,    -1,   109,    -1,    -1,    -1,    -1,     3,     4,
      -1,     6,    -1,     8,     9,    10,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    37,    38,    39,    -1,    41,    42,
      43,    -1,    45,    -1,    -1,    48,    49,    50,    51,    -1,
      -1,    -1,    37,    38,    39,    -1,    41,    42,    43,    -1,
      45,    64,    65,    48,    49,    50,    51,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    64,
      65,    -1,    85,    -1,    -1,    -1,    -1,    90,    -1,    -1,
      -1,    94,    95,    -1,    -1,    -1,    -1,    -1,   101,    -1,
      85,   104,    -1,    -1,   107,    90,   109,    -1,    -1,    94,
      95,     3,     4,    -1,     6,    -1,     8,     9,    10,   104,
      -1,   106,   107,    -1,   109,    -1,    -1,    -1,    -1,     3,
       4,    -1,     6,    -1,     8,     9,    10,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    39,    -1,    41,
      42,    43,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      -1,    -1,    -1,    37,    38,    39,    -1,    41,    42,    43,
      -1,    45,    64,    65,    48,    49,    50,    51,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      64,    65,    -1,    85,    -1,    -1,    -1,    -1,    90,    -1,
      -1,    -1,    94,    95,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    85,   104,    -1,    -1,   107,    90,   109,    -1,    -1,
      94,    95,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     104,    -1,    -1,   107,    -1,   109,     3,     4,     5,    -1,
      -1,    -1,    -1,    -1,    11,    12,    13,    14,    -1,    -1,
      -1,     3,     4,     5,    -1,    -1,    -1,    -1,    -1,    11,
      12,    13,    14,    -1,    -1,    32,    33,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      32,    33,    34,    35,    36,    -1,    -1,    -1,     3,     4,
       5,    -1,    -1,    -1,    46,    -1,    11,    12,    13,    14,
      15,    16,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    78,    79,    -1,    81,    82,    83,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    40,    78,    79,    -1,    -1,
      -1,    46,    47,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   109,     3,     4,     5,    -1,    -1,    -1,    -1,
      -1,    11,    12,    13,    14,    -1,    -1,   109,    18,    19,
      -1,    -1,    -1,    78,    79,    -1,    81,    82,    83,    -1,
      -1,    -1,    32,    33,    34,    35,    36,    -1,    -1,    -1,
      40,    -1,    -1,    -1,    -1,    -1,    46,   102,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
      -1,    -1,    -1,    -1,    11,    12,    13,    14,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    75,    -1,    -1,    78,    79,
      -1,    81,    82,    83,    84,    32,    33,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,    46,
      -1,    -1,   102,    11,    12,    13,    14,    -1,    -1,    -1,
       3,     4,     5,    -1,    -1,    -1,    -1,    -1,    11,    12,
      13,    14,    -1,    -1,    32,    33,    34,    35,    36,    -1,
      -1,    78,    79,    -1,    81,    82,    83,    -1,    46,    32,
      33,    34,    35,    36,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,    -1,    46,    -1,   102,    11,    12,    13,    14,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      78,    79,    -1,    81,    82,    83,    -1,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    78,    79,    -1,    81,    82,
      83,    46,    -1,    -1,   102,    -1,     3,     4,     5,    -1,
      -1,    -1,    -1,    -1,    11,    12,    13,    14,    -1,   102,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    78,    79,    32,    33,    34,    35,    36,
      -1,    -1,    -1,    40,     3,     4,     5,    -1,    -1,    46,
      95,    -1,    11,    12,    13,    14,    -1,    -1,    -1,     3,
       4,     5,    -1,    -1,    -1,    -1,    -1,    11,    12,    13,
      14,    -1,    -1,    32,    33,    34,    35,    36,    -1,    -1,
      -1,    78,    79,    -1,    81,    82,    83,    46,    32,    33,
      34,    35,    36,    -1,    -1,    -1,     3,     4,     5,    -1,
      -1,    -1,    46,    -1,    11,    12,    13,    14,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,
      79,    -1,    81,    82,    83,    32,    33,    34,    35,    36,
      -1,    -1,    -1,    -1,    78,    79,    80,    -1,    -1,    46,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,    -1,
       3,     4,     5,    11,    12,    13,    14,    -1,    11,    12,
      13,    14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    78,    79,    80,    32,    33,    34,    35,    36,    32,
      33,    34,    35,    36,    -1,    -1,    -1,    -1,    46,    -1,
      -1,    -1,    -1,    46,    -1,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    -1,    -1,    66,    67,    68,
      69,    70,    71,    72,    -1,    -1,    -1,    76,    -1,    -1,
      78,    79,    -1,    -1,    -1,    78,    79,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    -1,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,   110,    66,    67,    68,    69,    70,    71,    72,    -1,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,   108,    -1,    66,    67,    68,    69,
      70,    71,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,   108,    -1,
      66,    67,    68,    69,    70,    71,    72,    -1,    -1,    -1,
      76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,   108,    -1,    66,    67,    68,    69,    70,    71,
      72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,   108,    -1,    66,    67,
      68,    69,    70,    71,    72,    -1,    -1,    -1,    76,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
     108,    -1,    66,    67,    68,    69,    70,    71,    72,    -1,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,   108,    -1,    66,    67,    68,    69,
      70,    71,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   106,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    -1,    -1,    66,
      67,    68,    69,    70,    71,    72,    -1,    -1,    -1,    76,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    66,    67,    68,    69,    70,    71,    72,    -1,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    -1,    -1,    -1,    -1,    -1,   103,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    66,    67,    68,    69,    70,    71,    72,    -1,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    -1,    -1,    -1,    -1,    -1,   103,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    66,    67,    68,    69,    70,    71,    72,    -1,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    -1,    -1,   100,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    -1,    -1,    66,
      67,    68,    69,    70,    71,    72,    -1,    -1,    -1,    76,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    54,    55,    56,    57,    58,    59,    -1,    -1,    -1,
      -1,    -1,    -1,    66,    67,    54,    55,    56,    57,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    -1,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    54,
      55,    56,    57,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    66,
      67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    90,    91,    92,    93,    94,    95,    96,
      97
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   113,   114,     0,     3,     4,     5,    11,    12,    13,
      14,    18,    19,    32,    33,    34,    35,    36,    40,    46,
      75,    78,    79,    81,    82,    83,    84,   115,   116,   117,
     119,   122,   128,   135,   137,   139,   140,   141,   148,   149,
     150,   151,   152,   154,   166,   168,   169,   172,   107,     3,
       4,   150,   150,     3,    13,    14,   119,   122,   149,   151,
     152,   169,   172,    13,    14,   122,   149,     3,     4,    91,
      91,   138,   101,   101,    14,   119,   122,   128,   137,   139,
     149,   154,   169,   172,   100,   100,   150,     4,   109,   132,
     149,   100,   101,   174,   100,   100,    90,    95,   147,     3,
     105,   147,   159,    52,     4,   109,   150,   107,   100,   100,
     100,   100,   100,   136,   101,   100,   101,   118,   101,   101,
     150,   147,   150,   167,   147,   147,   147,   150,   150,   150,
     149,   149,   101,   121,   173,     3,   170,   171,   132,   100,
     147,   100,     7,   100,   123,   107,     4,   103,   193,   147,
     175,    46,    95,     3,   107,     3,     4,     6,     8,     9,
      10,    37,    38,    39,    41,    42,    43,    45,    48,    49,
      50,    51,    64,    65,    85,    90,    94,    95,   104,   106,
     107,   109,   151,   153,   160,   184,   185,   186,   187,   188,
     191,     3,   107,   151,     3,   105,     4,    52,   143,    34,
      80,   144,   145,   146,   149,   170,   173,   101,   170,   173,
     150,   121,   107,   150,   150,   150,   147,   147,   125,   102,
     154,    86,   102,   110,     3,   103,   124,   134,   107,     3,
       4,   194,   195,   174,     3,    44,   130,   176,    95,    86,
     159,   165,   188,   190,   107,   149,   105,   186,   107,   186,
     186,   186,   107,   186,   186,   186,   186,     4,   149,   151,
     181,   188,   186,     3,   106,    53,    64,    65,   105,   107,
     111,    91,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    66,    67,    68,    69,    70,    71,    72,    76,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,     6,   159,   165,    95,   149,   156,   157,   158,
       3,   130,    86,   164,   160,   107,   144,   108,   110,   147,
     102,   102,   114,   102,   102,   150,    95,   158,   110,    92,
      15,    16,    17,    47,   102,   126,   127,   139,   140,   154,
     100,   188,   171,    15,    16,    17,   120,   144,   108,   107,
     107,   110,    54,    55,    56,    57,    60,    61,    62,    63,
      64,    65,    86,    91,    92,    93,    94,    95,    96,   104,
     105,   107,   131,   107,     3,    19,    20,    21,    23,    24,
      25,    26,    27,    28,    29,    73,    74,    77,    84,   100,
     102,   149,   154,   168,   174,   177,   179,   181,   101,   161,
     188,   164,   155,   108,   110,   189,   190,   105,   107,   106,
     149,   188,   147,   108,   110,     3,   188,   189,     3,   149,
     188,   188,   188,   188,   188,   188,   188,   188,   188,   188,
     188,   188,   188,   188,   188,   188,   188,   188,   188,   188,
     188,   188,   188,   188,   188,   188,   188,   188,   188,   188,
     164,   155,     3,   147,   110,   108,   107,   161,   191,   106,
     108,   108,   146,     3,   105,   107,   102,   150,   150,     3,
     108,     3,     8,   122,   132,   103,    86,   100,   100,   103,
       4,     4,     4,   121,   108,   189,   189,   195,   106,   108,
     133,   103,   181,   182,   107,   177,   107,   107,   100,   100,
       3,   107,    74,   101,   107,    73,   154,   132,   149,   154,
     147,   100,   100,   149,   154,   180,   181,   100,   102,   161,
     162,   163,   188,   155,   110,   188,   108,   188,   189,   186,
     147,   110,   107,   108,   188,   106,   108,   147,   103,   155,
     105,   108,   150,   156,    95,   142,   174,   193,    86,   105,
     106,    95,   108,    95,    92,    92,   150,   100,     8,   188,
     108,   108,   144,   177,   100,   188,    24,   188,   100,   188,
     107,     6,   192,   192,   107,   100,   100,   147,   100,     3,
     147,   100,   102,   110,   147,   106,   108,   108,   149,    95,
     186,    92,   188,   160,   107,     3,   105,   144,   174,   188,
     106,   160,     3,   108,   107,     3,   100,   100,   100,   108,
     108,   107,   108,   108,   192,     6,   102,   103,   108,   192,
       3,   107,     3,   182,   102,   163,     3,   147,   108,   107,
     106,   158,   165,   160,   108,   106,   108,   107,   158,    46,
     129,   177,   188,   177,   101,   108,   100,   108,   178,   103,
     100,   159,   165,   108,   107,   188,   108,   108,   106,   129,
     107,   158,   108,     3,    22,   108,   183,   100,   100,   144,
     188,   182,   164,   158,   108,   107,   165,     3,   174,   158,
     108,   177,   100,    30,    31,   102,   177,   108,   108,   108,
     108,   158,   164,   108,   188,   103,   100,   177,   177,   108,
     108,   103,   186,   164
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,   112,   113,   114,   114,   115,   115,   115,   115,   115,
     115,   115,   115,   115,   115,   115,   115,   115,   115,   115,
     115,   115,   115,   116,   116,   118,   117,   120,   119,   121,
     122,   122,   123,   123,   124,   124,   124,   124,   125,   125,
     126,   126,   126,   126,   126,   126,   126,   126,   127,   127,
     127,   128,   128,   129,   129,   129,   130,   130,   131,   131,
     131,   131,   131,   131,   131,   131,   131,   131,   131,   131,
     131,   131,   131,   131,   131,   131,   131,   131,   133,   132,
     134,   132,   132,   136,   135,   138,   137,   137,   137,   139,
     140,   142,   141,   143,   141,   141,   144,   144,   144,   145,
     145,   146,   146,   146,   146,   146,   146,   146,   146,   146,
     147,   147,   147,   147,   147,   147,   148,   148,   148,   148,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   150,   150,   151,   151,   152,
     153,   154,   154,   154,   154,   154,   154,   154,   154,   154,
     154,   155,   155,   155,   156,   156,   157,   157,   158,   158,
     159,   159,   159,   160,   161,   161,   161,   162,   162,   163,
     163,   164,   164,   164,   165,   165,   165,   166,   166,   166,
     167,   166,   166,   166,   168,   168,   168,   169,   170,   170,
     170,   171,   171,   172,   173,   173,   175,   174,   176,   176,
     177,   177,   177,   177,   177,   177,   177,   177,   177,   177,
     177,   177,   177,   177,   177,   177,   177,   177,   177,   177,
     177,   177,   178,   177,   177,   177,   177,   179,   180,   180,
     180,   181,   181,   182,   182,   183,   183,   183,   183,   184,
     184,   184,   184,   184,   184,   184,   184,   184,   184,   184,
     184,   184,   185,   185,   185,   185,   185,   185,   185,   186,
     186,   186,   186,   186,   186,   186,   186,   186,   186,   186,
     186,   186,   186,   186,   186,   186,   186,   187,   187,   187,
     187,   188,   188,   188,   188,   188,   188,   188,   188,   188,
     188,   188,   188,   188,   188,   188,   188,   188,   188,   188,
     188,   188,   188,   188,   188,   188,   188,   188,   188,   188,
     188,   188,   189,   189,   190,   190,   191,   191,   192,   192,
     193,   193,   194,   194,   195,   195
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     2,     2,     2,     1,
       2,     2,     2,     2,     2,     3,     3,     2,     2,     3,
       3,     2,     1,     2,     2,     0,     6,     0,     6,     3,
       1,     1,     0,     1,     0,     3,     3,     3,     0,     2,
       2,     2,     4,     1,     2,     4,     4,     3,     1,     1,
       1,     0,     1,     0,     1,     2,     1,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     1,     1,     0,     8,
       0,     5,     4,     0,     5,     0,     3,     4,     4,     2,
       4,     0,    10,     0,     7,     6,     0,     1,     1,     1,
       3,     3,     5,     5,     6,     2,     4,     1,     9,     8,
       0,     2,     3,     1,     1,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     2,
       1,     7,     5,     7,     2,     1,     1,     2,     3,     2,
       2,     5,     6,     5,     5,     6,     4,    10,     8,    13,
      11,     0,     5,     6,     2,     3,     1,     3,     0,     1,
       3,     2,     4,     1,     2,     3,     4,     1,     3,     1,
       1,     0,     2,     2,     0,     2,     2,     4,     4,     4,
       0,     5,     6,     6,     4,    10,     8,     5,     1,     3,
       2,     1,     3,     5,     0,     3,     0,     4,     0,     2,
       1,     5,     7,     5,     7,     7,     8,     8,     3,     3,
       2,     2,     3,     3,     4,     5,     6,     6,     4,     2,
       3,     3,     0,     8,     2,     2,     1,     2,     0,     1,
       1,     1,     3,     0,     1,     0,     4,     3,     2,     1,
       1,     1,     1,     1,     7,     1,     1,     1,     1,     1,
       3,     4,     1,     4,     3,     3,     4,     2,     2,     1,
       2,     2,     2,     2,     2,     2,     2,     2,     5,     5,
       2,     4,    11,     5,     5,     2,     8,     1,     1,     1,
       1,     1,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     5,     0,     1,     1,     3,     1,     2,     1,     2,
       0,     2,     1,     3,     4,     4
};


/* YYDPREC[RULE-NUM] -- Dynamic precedence of rule #RULE-NUM (0 if none).  */
static const yytype_int8 yydprec[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     2,     1,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0
};

/* YYMERGER[RULE-NUM] -- Index of merging function for rule #RULE-NUM.  */
static const yytype_int8 yymerger[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0
};

/* YYIMMEDIATE[RULE-NUM] -- True iff rule #RULE-NUM is not to be deferred, as
   in the case of predicates.  */
static const yybool yyimmediate[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0
};

/* YYCONFLP[YYPACT[STATE-NUM]] -- Pointer into YYCONFL of start of
   list of conflicting reductions corresponding to action entry for
   state STATE-NUM in yytable.  0 means no conflicts.  The list in
   yyconfl is terminated by a rule number of 0.  */
static const yytype_uint8 yyconflp[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    61,     0,     0,     0,    65,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     133,     0,   135,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   137,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    67,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   101,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    63,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    69,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     1,     3,     5,     0,     0,     0,     0,     0,
       7,     9,    11,    13,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    15,    17,    19,    21,    23,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    25,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    27,    29,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    71,    73,
      75,     0,     0,     0,     0,     0,    77,    79,    81,    83,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    85,    87,    89,
      91,    93,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    95,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   103,   105,   107,     0,     0,     0,     0,
       0,   109,   111,   113,   115,     0,     0,     0,     0,     0,
       0,     0,     0,    97,    99,     0,     0,     0,     0,     0,
       0,     0,   117,   119,   121,   123,   125,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   127,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   129,   131,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    31,    33,    35,     0,
       0,     0,     0,     0,    37,    39,    41,    43,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    45,    47,    49,    51,    53,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    55,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    57,    59,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0
};

/* YYCONFL[I] -- lists of conflicting rule numbers, each terminated by
   0, pointed into by YYCONFLP.  */
static const short yyconfl[] =
{
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,   110,     0,   130,     0,   110,     0,   125,     0,   122,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,   125,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,    51,     0,    51,     0,    51,     0,    51,
       0,    51,     0,   110,     0,   110,     0,   110,     0
};


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

# define YYRHSLOC(Rhs, K) ((Rhs)[K].yystate.yyloc)


YYSTYPE yylval;
YYLTYPE yylloc;

int yynerrs;
int yychar;

enum { YYENOMEM = -2 };

typedef enum { yyok, yyaccept, yyabort, yyerr, yynomem } YYRESULTTAG;

#define YYCHK(YYE)                              \
  do {                                          \
    YYRESULTTAG yychk_flag = YYE;               \
    if (yychk_flag != yyok)                     \
      return yychk_flag;                        \
  } while (0)

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYMAXDEPTH * sizeof (GLRStackItem)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

/* Minimum number of free items on the stack allowed after an
   allocation.  This is to allow allocation and initialization
   to be completed by functions that call yyexpandGLRStack before the
   stack is expanded, thus insuring that all necessary pointers get
   properly redirected to new data.  */
#define YYHEADROOM 2

#ifndef YYSTACKEXPANDABLE
#  define YYSTACKEXPANDABLE 1
#endif

#if YYSTACKEXPANDABLE
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyexpandGLRStack (Yystack);                       \
  } while (0)
#else
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyMemoryExhausted (Yystack);                      \
  } while (0)
#endif

/** State numbers. */
typedef int yy_state_t;

/** Rule numbers. */
typedef int yyRuleNum;

/** Item references. */
typedef short yyItemNum;

typedef struct yyGLRState yyGLRState;
typedef struct yyGLRStateSet yyGLRStateSet;
typedef struct yySemanticOption yySemanticOption;
typedef union yyGLRStackItem yyGLRStackItem;
typedef struct yyGLRStack yyGLRStack;

struct yyGLRState
{
  /** Type tag: always true.  */
  yybool yyisState;
  /** Type tag for yysemantics.  If true, yyval applies, otherwise
   *  yyfirstVal applies.  */
  yybool yyresolved;
  /** Number of corresponding LALR(1) machine state.  */
  yy_state_t yylrState;
  /** Preceding state in this stack */
  yyGLRState* yypred;
  /** Source position of the last token produced by my symbol */
  YYPTRDIFF_T yyposn;
  union {
    /** First in a chain of alternative reductions producing the
     *  nonterminal corresponding to this state, threaded through
     *  yynext.  */
    yySemanticOption* yyfirstVal;
    /** Semantic value for this state.  */
    YYSTYPE yyval;
  } yysemantics;
  /** Source location for this state.  */
  YYLTYPE yyloc;
};

struct yyGLRStateSet
{
  yyGLRState** yystates;
  /** During nondeterministic operation, yylookaheadNeeds tracks which
   *  stacks have actually needed the current lookahead.  During deterministic
   *  operation, yylookaheadNeeds[0] is not maintained since it would merely
   *  duplicate yychar != YYEMPTY.  */
  yybool* yylookaheadNeeds;
  YYPTRDIFF_T yysize;
  YYPTRDIFF_T yycapacity;
};

struct yySemanticOption
{
  /** Type tag: always false.  */
  yybool yyisState;
  /** Rule number for this reduction */
  yyRuleNum yyrule;
  /** The last RHS state in the list of states to be reduced.  */
  yyGLRState* yystate;
  /** The lookahead for this reduction.  */
  int yyrawchar;
  YYSTYPE yyval;
  YYLTYPE yyloc;
  /** Next sibling in chain of options.  To facilitate merging,
   *  options are chained in decreasing order by address.  */
  yySemanticOption* yynext;
};

/** Type of the items in the GLR stack.  The yyisState field
 *  indicates which item of the union is valid.  */
union yyGLRStackItem {
  yyGLRState yystate;
  yySemanticOption yyoption;
};

struct yyGLRStack {
  int yyerrState;
  /* To compute the location of the error token.  */
  yyGLRStackItem yyerror_range[3];

  YYJMP_BUF yyexception_buffer;
  yyGLRStackItem* yyitems;
  yyGLRStackItem* yynextFree;
  YYPTRDIFF_T yyspaceLeft;
  yyGLRState* yysplitPoint;
  yyGLRState* yylastDeleted;
  yyGLRStateSet yytops;
};

#if YYSTACKEXPANDABLE
static void yyexpandGLRStack (yyGLRStack* yystackp);
#endif

_Noreturn static void
yyFail (yyGLRStack* yystackp, const char* yymsg)
{
  if (yymsg != YY_NULLPTR)
    yyerror (yymsg);
  YYLONGJMP (yystackp->yyexception_buffer, 1);
}

_Noreturn static void
yyMemoryExhausted (yyGLRStack* yystackp)
{
  YYLONGJMP (yystackp->yyexception_buffer, 2);
}

/** Accessing symbol of state YYSTATE.  */
static inline yysymbol_kind_t
yy_accessing_symbol (yy_state_t yystate)
{
  return YY_CAST (yysymbol_kind_t, yystos[yystate]);
}

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "IDENTIFIER",
  "TYPE_NAME", "TAG_NAME", "STRING_LITERAL", "CLASS_FINAL", "INT_LITERAL",
  "CHAR_LITERAL", "FLOAT_LITERAL", "CLASS", "STRUCT", "ENUM", "UNION",
  "PUBLIC", "PRIVATE", "PROTECTED", "NAMESPACE", "TYPEDEF", "RETURN", "IF",
  "ELSE", "DO", "WHILE", "FOR", "BREAK", "CONTINUE", "GOTO", "SWITCH",
  "CASE", "DEFAULT", "INT_KW", "FLOAT_KW", "VOID_KW", "BOOL_KW", "CHAR_KW",
  "NEW", "DELETE", "THIS", "VIRTUAL", "TRUE_KW", "FALSE_KW", "NULLPTR_KW",
  "OPERATOR", "SIZEOF", "CONST", "FRIEND", "STATIC_CAST", "DYNAMIC_CAST",
  "CONST_CAST", "REINTERPRET_CAST", "COLONCOLON", "ARROW", "EQ", "NE",
  "LE", "GE", "ANDAND", "OROR", "PLUSEQ", "MINUSEQ", "STAREQ", "SLASHEQ",
  "INC", "DEC", "SHL", "SHR", "ANDEQ", "OREQ", "XOREQ", "SHLEQ", "SHREQ",
  "ASM", "VOLATILE", "NATIVE", "MODEQ", "STATIC", "STD_ARRAY",
  "STD_VECTOR", "ELLIPSIS", "ANON_STRUCT", "ANON_UNION", "ANON_ENUM",
  "EXTERN", "VA_ARG", "'='", "'?'", "'|'", "'^'", "'&'", "'<'", "'>'",
  "'+'", "'-'", "'*'", "'/'", "'%'", "SIZEOF_TYPE_PREC", "LOWER_THAN_ELSE",
  "';'", "'{'", "'}'", "':'", "'!'", "'['", "']'", "'('", "')'", "'~'",
  "','", "'.'", "$accept", "program", "top_decl_list", "top_decl",
  "native_decl", "namespace_decl", "$@1", "class_decl", "$@2",
  "class_body", "class_or_struct_kw", "opt_class_final", "opt_base",
  "member_list", "member", "access_spec", "opt_virtual", "opt_const",
  "func_name", "operator_symbol", "func_header", "$@3", "$@4",
  "implicit_int_header", "$@5", "anon_tag_decl", "$@6", "func_decl",
  "func_def", "out_of_line_def", "$@7", "$@8", "opt_param_list",
  "param_list", "param", "pointer_opt", "tag_def_type", "type_spec",
  "name_tok", "qname_prefix", "qualified_type", "qualified_id_expr",
  "var_decl", "more_plain_declarators", "func_ptr_param_type",
  "func_ptr_param_list", "opt_func_ptr_param_list", "array_bracket_list",
  "array_dim", "braced_init", "init_items", "init_item",
  "opt_array_initializer", "opt_initializer", "tag_typedef_decl", "$@9",
  "typedef_decl", "enum_decl", "enumerator_list", "enumerator",
  "union_decl", "union_member_list", "block", "$@10", "stmt_list", "stmt",
  "$@11", "for_open", "for_init", "comma_expr", "comma_expr_opt",
  "switch_body", "primary_expr", "postfix_expr", "unary_expr",
  "cpp_cast_kw", "expr", "opt_arg_list", "arg_list", "string_seq",
  "asm_string_list", "opt_member_init_list", "member_init_list",
  "member_init", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

/** Left-hand-side symbol for rule #YYRULE.  */
static inline yysymbol_kind_t
yylhsNonterm (yyRuleNum yyrule)
{
  return YY_CAST (yysymbol_kind_t, yyr1[yyrule]);
}

#if YYDEBUG

# ifndef YYFPRINTF
#  define YYFPRINTF fprintf
# endif

# define YY_FPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_FPRINTF_

# define YY_FPRINTF_(Args)                      \
  do {                                          \
    YYFPRINTF Args;                             \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)

# define YY_DPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_DPRINTF_

# define YY_DPRINTF_(Args)                      \
  do {                                          \
    if (yydebug)                                \
      YYFPRINTF Args;                           \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */



/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp);
  YYFPRINTF (yyo, ")");
}

# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                  \
  do {                                                                  \
    if (yydebug)                                                        \
      {                                                                 \
        YY_FPRINTF ((stderr, "%s ", Title));                            \
        yy_symbol_print (stderr, Kind, Value, Location);        \
        YY_FPRINTF ((stderr, "\n"));                                    \
      }                                                                 \
  } while (0)

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, YYPTRDIFF_T yyk,
                 yyRuleNum yyrule);

# define YY_REDUCE_PRINT(Args)          \
  do {                                  \
    if (yydebug)                        \
      yy_reduce_print Args;             \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;

static void yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
  YY_ATTRIBUTE_UNUSED;
static void yypdumpstack (yyGLRStack* yystackp)
  YY_ATTRIBUTE_UNUSED;

#else /* !YYDEBUG */

# define YY_DPRINTF(Args) do {} while (yyfalse)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_REDUCE_PRINT(Args)

#endif /* !YYDEBUG */

#ifndef yystrlen
# define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


/** Fill in YYVSP[YYLOW1 .. YYLOW0-1] from the chain of states starting
 *  at YYVSP[YYLOW0].yystate.yypred.  Leaves YYVSP[YYLOW1].yystate.yypred
 *  containing the pointer to the next state in the chain.  */
static void yyfillin (yyGLRStackItem *, int, int) YY_ATTRIBUTE_UNUSED;
static void
yyfillin (yyGLRStackItem *yyvsp, int yylow0, int yylow1)
{
  int i;
  yyGLRState *s = yyvsp[yylow0].yystate.yypred;
  for (i = yylow0-1; i >= yylow1; i -= 1)
    {
#if YYDEBUG
      yyvsp[i].yystate.yylrState = s->yylrState;
#endif
      yyvsp[i].yystate.yyresolved = s->yyresolved;
      if (s->yyresolved)
        yyvsp[i].yystate.yysemantics.yyval = s->yysemantics.yyval;
      else
        /* The effect of using yyval or yyloc (in an immediate rule) is
         * undefined.  */
        yyvsp[i].yystate.yysemantics.yyfirstVal = YY_NULLPTR;
      yyvsp[i].yystate.yyloc = s->yyloc;
      s = yyvsp[i].yystate.yypred = s->yypred;
    }
}


/** If yychar is empty, fetch the next token.  */
static inline yysymbol_kind_t
yygetToken (int *yycharp)
{
  yysymbol_kind_t yytoken;
  if (*yycharp == YYEMPTY)
    {
      YY_DPRINTF ((stderr, "Reading a token\n"));
      *yycharp = yylex ();
    }
  if (*yycharp <= YYEOF)
    {
      *yycharp = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YY_DPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (*yycharp);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }
  return yytoken;
}

/* Do nothing if YYNORMAL or if *YYLOW <= YYLOW1.  Otherwise, fill in
 * YYVSP[YYLOW1 .. *YYLOW-1] as in yyfillin and set *YYLOW = YYLOW1.
 * For convenience, always return YYLOW1.  */
static inline int yyfill (yyGLRStackItem *, int *, int, yybool)
     YY_ATTRIBUTE_UNUSED;
static inline int
yyfill (yyGLRStackItem *yyvsp, int *yylow, int yylow1, yybool yynormal)
{
  if (!yynormal && yylow1 < *yylow)
    {
      yyfillin (yyvsp, *yylow, yylow1);
      *yylow = yylow1;
    }
  return yylow1;
}

/** Perform user action for rule number YYN, with RHS length YYRHSLEN,
 *  and top stack item YYVSP.  YYLVALP points to place to put semantic
 *  value ($$), and yylocp points to place for location information
 *  (@$).  Returns yyok for normal return, yyaccept for YYACCEPT,
 *  yyerr for YYERROR, yyabort for YYABORT, yynomem for YYNOMEM.  */
static YYRESULTTAG
yyuserAction (yyRuleNum yyrule, int yyrhslen, yyGLRStackItem* yyvsp,
              yyGLRStack* yystackp, YYPTRDIFF_T yyk,
              YYSTYPE* yyvalp, YYLTYPE *yylocp)
{
  const yybool yynormal YY_ATTRIBUTE_UNUSED = yystackp->yysplitPoint == YY_NULLPTR;
  int yylow = 1;
  YY_USE (yyvalp);
  YY_USE (yylocp);
  YY_USE (yyk);
  YY_USE (yyrhslen);
# undef yyerrok
# define yyerrok (yystackp->yyerrState = 0)
# undef YYACCEPT
# define YYACCEPT return yyaccept
# undef YYABORT
# define YYABORT return yyabort
# undef YYNOMEM
# define YYNOMEM return yynomem
# undef YYERROR
# define YYERROR return yyerrok, yyerr
# undef YYRECOVERING
# define YYRECOVERING() (yystackp->yyerrState != 0)
# undef yyclearin
# define yyclearin (yychar = YYEMPTY)
# undef YYFILL
# define YYFILL(N) yyfill (yyvsp, &yylow, (N), yynormal)
# undef YYBACKUP
# define YYBACKUP(Token, Value)                                              \
  return yyerror (YY_("syntax error: cannot back up")),     \
         yyerrok, yyerr

  if (yyrhslen == 0)
    *yyvalp = yyval_default;
  else
    *yyvalp = yyvsp[YYFILL (1-yyrhslen)].yystate.yysemantics.yyval;
  /* Default location. */
  YYLLOC_DEFAULT ((*yylocp), (yyvsp - yyrhslen), yyrhslen);
  yystackp->yyerror_range[1].yystate.yyloc = *yylocp;
  /* If yyk == -1, we are running a deferred action on a temporary
     stack.  In that case, YY_REDUCE_PRINT must not play with YYFILL,
     so pretend the stack is "normal". */
  YY_REDUCE_PRINT ((yynormal || yyk == -1, yyvsp, yyk, yyrule));
  switch (yyrule)
    {
  case 2: /* program: top_decl_list  */
#line 751 "src/parser.y"
        {
            g_program = ast_new(AST_PROGRAM, 1);
            g_program->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node) = g_program;
        }
#line 3125 "src/parser.c"
    break;

  case 3: /* top_decl_list: %empty  */
#line 759 "src/parser.y"
                                { ((*yyvalp).list) = ast_list_new(); }
#line 3131 "src/parser.c"
    break;

  case 4: /* top_decl_list: top_decl_list top_decl  */
#line 761 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            /* struct/union/enum definitions found inside this declaration
             * (see hoist_tag_def) come out ahead of it */
            g_ast_sizeof_hook = parse_fold_sizeof;
            for (int i = 0; i < g_hoisted_tags.count; i++) {
                parse_register_decl(g_hoisted_tags.items[i]);
                ast_list_append(&((*yyvalp).list), g_hoisted_tags.items[i]);
            }
            g_hoisted_tags.count = 0;
            parse_register_decl((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
            ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 3149 "src/parser.c"
    break;

  case 5: /* top_decl: namespace_decl  */
#line 777 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3155 "src/parser.c"
    break;

  case 6: /* top_decl: class_decl ';'  */
#line 778 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3161 "src/parser.c"
    break;

  case 7: /* top_decl: enum_decl ';'  */
#line 779 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3167 "src/parser.c"
    break;

  case 8: /* top_decl: union_decl ';'  */
#line 780 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3173 "src/parser.c"
    break;

  case 9: /* top_decl: func_def  */
#line 781 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3179 "src/parser.c"
    break;

  case 10: /* top_decl: func_decl ';'  */
#line 782 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3185 "src/parser.c"
    break;

  case 11: /* top_decl: var_decl ';'  */
#line 783 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3191 "src/parser.c"
    break;

  case 12: /* top_decl: typedef_decl ';'  */
#line 784 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3197 "src/parser.c"
    break;

  case 13: /* top_decl: tag_typedef_decl ';'  */
#line 785 "src/parser.y"
                           { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3203 "src/parser.c"
    break;

  case 14: /* top_decl: anon_tag_decl ';'  */
#line 787 "src/parser.y"
        {
            /* `enum { RED, GREEN };` -- constants with no type name */
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 3212 "src/parser.c"
    break;

  case 15: /* top_decl: EXTERN var_decl ';'  */
#line 792 "src/parser.y"
        {
            /* C input only (see the lexer's "extern" rule) */
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            if (((*yyvalp).node)->kind == AST_VAR_DECL_GROUP) {
                for (int i = 0; i < ((*yyvalp).node)->list.count; i++) ((*yyvalp).node)->list.items[i]->is_extern = 1;
            } else {
                ((*yyvalp).node)->is_extern = 1;
            }
        }
#line 3226 "src/parser.c"
    break;

  case 16: /* top_decl: EXTERN func_decl ';'  */
#line 801 "src/parser.y"
                             { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3232 "src/parser.c"
    break;

  case 17: /* top_decl: implicit_int_header ';'  */
#line 803 "src/parser.y"
        {
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            symtab_pop_scope(g_symtab);
        }
#line 3241 "src/parser.c"
    break;

  case 18: /* top_decl: implicit_int_header block  */
#line 808 "src/parser.y"
        {
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->kind = AST_FUNC_DEF;
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            symtab_pop_scope(g_symtab);
        }
#line 3252 "src/parser.c"
    break;

  case 19: /* top_decl: class_or_struct_kw name_tok ';'  */
#line 815 "src/parser.y"
        {
            /* Forward declaration, `struct Actor;` -- only has to make
             * the name usable as a type before its definition. Emits
             * nothing: emit_forward_declarations already writes
             * `struct X;` for every class this file defines. */
            declare_type_name((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_CLASS);
            ((*yyvalp).node) = decl_group_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 3265 "src/parser.c"
    break;

  case 20: /* top_decl: UNION name_tok ';'  */
#line 824 "src/parser.y"
        {
            declare_type_name((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_UNION);
            ((*yyvalp).node) = decl_group_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 3274 "src/parser.c"
    break;

  case 21: /* top_decl: native_decl ';'  */
#line 828 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3280 "src/parser.c"
    break;

  case 22: /* top_decl: out_of_line_def  */
#line 829 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3286 "src/parser.c"
    break;

  case 23: /* native_decl: NATIVE IDENTIFIER  */
#line 850 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_TYPEDEF);
            ((*yyvalp).node) = ast_new(AST_NATIVE_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
        }
#line 3296 "src/parser.c"
    break;

  case 24: /* native_decl: NATIVE TYPE_NAME  */
#line 856 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_NATIVE_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
        }
#line 3305 "src/parser.c"
    break;

  case 25: /* $@1: %empty  */
#line 866 "src/parser.y"
        {
            /* Namespaces are reopenable: `namespace v32 { ... }` appearing
             * twice in the same TU should extend the same member set, not
             * create two disjoint ones (this matters for the hardware
             * namespaces -- v32::, ioports:: -- which real programs will
             * reopen across multiple headers).
             *
             * SIMPLIFICATION: this assumes a namespace is always reopened
             * at the same lexical nesting depth as its first opening (true
             * for flat top-level hardware namespaces). A production
             * version would separate "lexical parent scope" from
             * "namespace member set" the way real compilers do, so a
             * namespace could be legally reopened at different points
             * without corrupting the scope-pop parent chain. */
            Symbol *nsym = symtab_lookup_in(g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            if (nsym == NULL) {
                nsym = symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_NAMESPACE);
            }
            if (nsym->inner_scope == NULL) {
                symtab_push_scope(g_symtab, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), 0);
                nsym->inner_scope = g_symtab->current;
            } else {
                g_symtab->current = nsym->inner_scope;
            }
        }
#line 3335 "src/parser.c"
    break;

  case 26: /* namespace_decl: NAMESPACE IDENTIFIER $@1 '{' top_decl_list '}'  */
#line 892 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_NAMESPACE_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 3346 "src/parser.c"
    break;

  case 27: /* $@2: %empty  */
#line 904 "src/parser.y"
        {
            /* Register the class *before* the body is scanned, so that
             * self-referential members (`Node *next;`) and constructor/
             * destructor declarations -- which re-mention the class's own
             * name, now classified as TYPE_NAME by the lexer -- resolve
             * correctly. See driver.h for the single-class-at-a-time
             * caveat (no nested classes yet). */
            if (g_c_mode) {
                /* a tag: not a name the lexer may ever see as a type */
                c_tag_note((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
                g_current_class_sym = NULL;
                symtab_push_scope(g_symtab, "__tag", 1);
            } else {
            g_current_class_sym = symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str), SYM_CLASS);
            symtab_push_scope(g_symtab, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str), 1);
            g_current_class_sym->inner_scope = g_symtab->current;

            /* Injected class name (C++ [class.pre]): within its own body,
             * a class's name is implicitly visible as if it were a member
             * of itself. Without this, a self-qualified reference like
             * `Counter::Counter` breaks: the "::" arms pending_qualifier
             * to look *inside* Counter's own scope, but the constructor
             * rule (TYPE_NAME '(' ...) never inserts "Counter" as a
             * symbol under its own name -- it doesn't need to for the
             * in-class case, since it's just reusing the already-
             * registered class name. So the lookup for the second
             * "Counter" in an out-of-line `Counter::Counter(...)` or
             * `Counter::~Counter()` fails, falls back to plain
             * IDENTIFIER, and the parser -- correctly, given that input
             * -- rejects it expecting another "::". Confirmed against a
             * real failing build and a hand-traced parser.output before
             * landing on this as the actual root cause; see
             * docs/DESIGN_NOTES.md for the full story, including why an
             * earlier, different fix (in out_of_line_def's grammar) was
             * necessary but not sufficient on its own. */
            Symbol *injected = symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str), SYM_CLASS);
            injected->inner_scope = g_symtab->current;
            }
        }
#line 3390 "src/parser.c"
    break;

  case 28: /* class_decl: class_or_struct_kw name_tok opt_class_final opt_base $@2 class_body  */
#line 944 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_CLASS_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->str2 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node) ? strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node)->str1) : NULL;
            /* Inheritance access-specifier (public/private/protected),
             * carried on $4 (see opt_base) -- meaningless when $4/str2 is
             * NULL (no base at all), but ACC_PUBLIC is a harmless inert
             * default for that case rather than leaving it uninitialized. */
            ((*yyvalp).node)->access = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node) ? (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node)->access : ACC_PUBLIC;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->virt_spec = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival);
            ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.ival); /* is_struct -- see class_or_struct_kw below and
                AST_CLASS_DECL's own doc comment in ast.h for what this
                controls (only the default member-access level; every
                other piece of this project's class machinery applies
                identically either way) */
            g_current_class_sym = NULL;
            tag_decl(((*yyvalp).node));
        }
#line 3415 "src/parser.c"
    break;

  case 29: /* class_body: '{' member_list '}'  */
#line 973 "src/parser.y"
                           { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); }
#line 3421 "src/parser.c"
    break;

  case 30: /* class_or_struct_kw: CLASS  */
#line 984 "src/parser.y"
              { ((*yyvalp).ival) = 0; }
#line 3427 "src/parser.c"
    break;

  case 31: /* class_or_struct_kw: STRUCT  */
#line 985 "src/parser.y"
              { ((*yyvalp).ival) = 1; }
#line 3433 "src/parser.c"
    break;

  case 32: /* opt_class_final: %empty  */
#line 993 "src/parser.y"
                    { ((*yyvalp).ival) = 0; }
#line 3439 "src/parser.c"
    break;

  case 33: /* opt_class_final: CLASS_FINAL  */
#line 994 "src/parser.y"
                    { ((*yyvalp).ival) = VIRT_SPEC_FINAL; }
#line 3445 "src/parser.c"
    break;

  case 34: /* opt_base: %empty  */
#line 998 "src/parser.y"
                                 { ((*yyvalp).node) = NULL; }
#line 3451 "src/parser.c"
    break;

  case 35: /* opt_base: ':' PUBLIC TYPE_NAME  */
#line 1000 "src/parser.y"
        {
            /* Represented as a plain AST_IDENT carrying the base name in
             * str1 and the inheritance access-specifier in ->access --
             * reusing ast_ident() rather than adding a new semantic-value
             * type just to pair a string with an enum. class_decl's
             * action below unpacks both fields. */
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PUBLIC;
        }
#line 3465 "src/parser.c"
    break;

  case 36: /* opt_base: ':' PRIVATE TYPE_NAME  */
#line 1010 "src/parser.y"
        {
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PRIVATE;
        }
#line 3474 "src/parser.c"
    break;

  case 37: /* opt_base: ':' PROTECTED TYPE_NAME  */
#line 1015 "src/parser.y"
        {
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PROTECTED;
        }
#line 3483 "src/parser.c"
    break;

  case 38: /* member_list: %empty  */
#line 1022 "src/parser.y"
                           { ((*yyvalp).list) = ast_list_new(); }
#line 3489 "src/parser.c"
    break;

  case 39: /* member_list: member_list member  */
#line 1023 "src/parser.y"
                           { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 3495 "src/parser.c"
    break;

  case 40: /* member: access_spec ':'  */
#line 1028 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_ACCESS_SPEC, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.access);
        }
#line 3504 "src/parser.c"
    break;

  case 41: /* member: func_decl ';'  */
#line 1032 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3510 "src/parser.c"
    break;

  case 42: /* member: func_decl '=' INT_LITERAL ';'  */
#line 1034 "src/parser.y"
        {
            /* A pure virtual function, `virtual void draw(Video &v) = 0;`.
             * The vtable needs SOMETHING in the slot, so the declaration
             * becomes a definition with a do-nothing body (returning 0 /
             * a null pointer where a value is expected) -- exactly what
             * programs written for this transpiler used to spell out by
             * hand. Not enforced: nothing stops an abstract class from
             * being instantiated, and a derived class that forgets to
             * override gets the do-nothing body. */
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.lit).ival != 0 || (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->ival != 1) {
                yyerror("'= 0' is only valid on a virtual function (a pure virtual)");
                g_parse_errors++;
                YYERROR;
            }
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->kind = AST_FUNC_DEF;
            ((*yyvalp).node)->a = ast_new(AST_BLOCK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a->list = ast_list_new();
            AstNode *rt = ((*yyvalp).node)->type;
            while (rt != NULL && rt->kind == AST_CONST_TYPE) rt = rt->a;
            AstNode *value = NULL;
            if (rt != NULL && rt->kind == AST_POINTER_TYPE) {
                value = ast_new(AST_NULL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            } else if (rt != NULL && rt->kind == AST_IDENT && strcmp(rt->str1, "void") != 0) {
                Symbol *sym = symtab_lookup(g_symtab, rt->str1);
                AstNode *zero = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                zero->ival = 0;
                if (sym == NULL) {
                    value = zero;                 /* int, float, bool, char */
                } else if (sym->kind == SYM_ENUM) {
                    value = ast_new(AST_CAST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                    value->type = rt;
                    value->a = zero;
                }                                  /* a class: no value to make up */
            }
            if (value != NULL) {
                AstNode *ret = ast_new(AST_RETURN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                ret->a = value;
                ast_list_append(&((*yyvalp).node)->a->list, ret);
            }
        }
#line 3556 "src/parser.c"
    break;

  case 43: /* member: func_def  */
#line 1075 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3562 "src/parser.c"
    break;

  case 44: /* member: var_decl ';'  */
#line 1076 "src/parser.y"
                       { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3568 "src/parser.c"
    break;

  case 45: /* member: var_decl ':' expr ';'  */
#line 1078 "src/parser.y"
        {
            /* A bit-field, `int level : 6;`. Vircon32 C has none. By
             * default the member is kept as an ordinary full-word one and
             * the width is dropped, with a warning; --reject-bit-fields
             * makes it an error instead, for code that depends on the
             * packing or the wrap-around. The explanation is printed once
             * per run, each further bit-field gets one line. */
            static int explained = 0;
            const char *name = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->kind == AST_VAR_DECL && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->str1 != NULL) ? (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->str1 : "?";
            int width = 0;
            int have_width = ast_fold_int((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), parse_enum_value, &width);
            if (g_reject_bit_fields) {
                fprintf(stderr, "%s:%d: error: bit-field '%s' (--reject-bit-fields): "
                        "Vircon32 C has no bit-fields -- declare the member as a "
                        "plain int and mask it where it is written\n",
                        g_current_filename, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line, name);
                g_parse_errors++;
            } else {
                if (have_width)
                    fprintf(stderr, "%s:%d: warning: bit-field '%s : %d' is stored as a "
                            "full 32-bit member\n", g_current_filename, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line, name, width);
                else
                    fprintf(stderr, "%s:%d: warning: bit-field '%s' is stored as a "
                            "full 32-bit member\n", g_current_filename, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line, name);
                if (!explained) {
                    explained = 1;
                    fprintf(stderr,
                        "    note: Vircon32 C has no bit-fields, so each one becomes an ordinary\n"
                        "    member of its declared type and the width is ignored. What changes:\n"
                        "      - size and layout: every bit-field takes a whole word, so the struct\n"
                        "        is larger, sizeof differs, and it no longer matches a packed layout\n"
                        "        (a file format, a hardware register, a union overlay);\n"
                        "      - range: a value that does not fit the declared width is kept whole\n"
                        "        instead of being truncated -- no wrap-around on overflow, and a\n"
                        "        1-bit signed field holds 1, not -1;\n"
                        "      - a zero-width or unnamed padding bit-field is not supported.\n"
                        "    Code that only uses bit-fields as small flags and counters behaves the\n"
                        "    same. Use --reject-bit-fields to make this an error. (Shown once.)\n");
                }
            }
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
        }
#line 3615 "src/parser.c"
    break;

  case 46: /* member: FRIEND class_or_struct_kw name_tok ';'  */
#line 1121 "src/parser.y"
        {
            /* `friend class X;` / `friend struct X;`. name_tok since X
             * may or may not be a known type yet (a forward declaration
             * `class X;`, or X's own definition earlier in the file,
             * makes it a TYPE_NAME); class_or_struct_kw rather than a
             * literal CLASS so this rule and type_spec's elaborated
             * `class X` agree on the first reduction and the choice
             * between them is left to the ';' -- which removes the
             * shift/reduce conflict a literal `FRIEND CLASS IDENTIFIER`
             * has against type_spec. The original note follows.
             *
             * X is very commonly a class this file hasn't
             * DEFINED yet at this point in the source (the classic
             * mutually-friending pair, each one naming the other before
             * either body has been fully parsed), so it can't possibly
             * have been registered as a TYPE_NAME by the time this rule
             * fires -- see class_decl's own header-line action for
             * where that registration actually happens, always no
             * earlier than the friended class's own definition. Left
             * entirely unresolved here; sema.c's compute_layout does
             * the actual find_class lookup once the WHOLE program's
             * class registry is known, order-independent, exactly like
             * every other "class named before it's necessarily been
             * seen" case in this project (a base class named in
             * `opt_base` is the one existing precedent, though that one
             * DOES require a prior TYPE_NAME -- friendship is looser
             * than inheritance in real C++ specifically to allow this). */
            ((*yyvalp).node) = ast_new(AST_FRIEND_CLASS, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str));
        }
#line 3650 "src/parser.c"
    break;

  case 47: /* member: FRIEND func_header ';'  */
#line 1152 "src/parser.y"
        {
            /* `friend ReturnType f(params);` -- reuses func_header
             * completely unchanged (same AST_FUNC_DECL shape an
             * ordinary bodyless method prototype would get), then
             * relabels the node's own `kind` to AST_FRIEND_FUNC_DECL --
             * see that kind's own doc comment (ast.h) for why a
             * DIFFERENT kind, not a flag bit on AST_FUNC_DECL, is the
             * right shape here. `opt_virtual` deliberately NOT allowed
             * in front (unlike func_decl's own `opt_virtual func_header`)
             * -- a friend function isn't a member at all, so "virtual"
             * has no meaning on one; real C++ rejects this combination
             * outright, and this project matches that by simply never
             * offering the grammar shape rather than parsing it and
             * discarding the keyword silently. */
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->kind = AST_FRIEND_FUNC_DECL;
            symtab_pop_scope(g_symtab); /* func_header pushed a param
                scope at '(' (see its own header comment) that only
                func_decl's/func_def's own actions normally pop --
                bypassing both of those here (this is neither: no body,
                and not itself a member), so this rule pops it directly,
                same as func_decl's own action does for an ordinary
                bodyless prototype. */
        }
#line 3679 "src/parser.c"
    break;

  case 48: /* access_spec: PUBLIC  */
#line 1179 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PUBLIC; }
#line 3685 "src/parser.c"
    break;

  case 49: /* access_spec: PRIVATE  */
#line 1180 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PRIVATE; }
#line 3691 "src/parser.c"
    break;

  case 50: /* access_spec: PROTECTED  */
#line 1181 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PROTECTED; }
#line 3697 "src/parser.c"
    break;

  case 51: /* opt_virtual: %empty  */
#line 1196 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 3703 "src/parser.c"
    break;

  case 52: /* opt_virtual: VIRTUAL  */
#line 1197 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 3709 "src/parser.c"
    break;

  case 53: /* opt_const: %empty  */
#line 1211 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 3715 "src/parser.c"
    break;

  case 54: /* opt_const: CONST  */
#line 1212 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 3721 "src/parser.c"
    break;

  case 55: /* opt_const: opt_const IDENTIFIER  */
#line 1214 "src/parser.y"
        {
            /* C++11 virt-specifiers, `override` and `final`, after the
             * parameter list (and const). Contextual keywords, not
             * reserved words -- `int final;` stays an ordinary
             * declaration everywhere else -- so they arrive as an
             * IDENTIFIER, and only these two spellings are accepted in
             * this position. The value is a bit set: 1 const,
             * VIRT_SPEC_OVERRIDE, VIRT_SPEC_FINAL (ast.h). */
            int bit = 0;
            if (!g_c_mode && strcmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), "override") == 0) bit = VIRT_SPEC_OVERRIDE;
            else if (!g_c_mode && strcmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), "final") == 0) bit = VIRT_SPEC_FINAL;
            if (bit == 0) {
                yyerror("expected 'override', 'final', ';' or a function body "
                        "after the parameter list");
                g_parse_errors++;
                YYERROR;
            }
            ((*yyvalp).ival) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) | bit;
        }
#line 3745 "src/parser.c"
    break;

  case 56: /* func_name: IDENTIFIER  */
#line 1285 "src/parser.y"
                            { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 3751 "src/parser.c"
    break;

  case 57: /* func_name: OPERATOR operator_symbol  */
#line 1286 "src/parser.y"
                                 { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 3757 "src/parser.c"
    break;

  case 58: /* operator_symbol: '+'  */
#line 1290 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator+"); }
#line 3763 "src/parser.c"
    break;

  case 59: /* operator_symbol: '-'  */
#line 1291 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator-"); }
#line 3769 "src/parser.c"
    break;

  case 60: /* operator_symbol: '*'  */
#line 1292 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator*"); }
#line 3775 "src/parser.c"
    break;

  case 61: /* operator_symbol: '/'  */
#line 1293 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator/"); }
#line 3781 "src/parser.c"
    break;

  case 62: /* operator_symbol: '='  */
#line 1294 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator="); }
#line 3787 "src/parser.c"
    break;

  case 63: /* operator_symbol: '!'  */
#line 1295 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator!"); }
#line 3793 "src/parser.c"
    break;

  case 64: /* operator_symbol: EQ  */
#line 1296 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator=="); }
#line 3799 "src/parser.c"
    break;

  case 65: /* operator_symbol: NE  */
#line 1297 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator!="); }
#line 3805 "src/parser.c"
    break;

  case 66: /* operator_symbol: '<'  */
#line 1298 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator<"); }
#line 3811 "src/parser.c"
    break;

  case 67: /* operator_symbol: '>'  */
#line 1299 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator>"); }
#line 3817 "src/parser.c"
    break;

  case 68: /* operator_symbol: LE  */
#line 1300 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator<="); }
#line 3823 "src/parser.c"
    break;

  case 69: /* operator_symbol: GE  */
#line 1301 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator>="); }
#line 3829 "src/parser.c"
    break;

  case 70: /* operator_symbol: PLUSEQ  */
#line 1302 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator+="); }
#line 3835 "src/parser.c"
    break;

  case 71: /* operator_symbol: MINUSEQ  */
#line 1303 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator-="); }
#line 3841 "src/parser.c"
    break;

  case 72: /* operator_symbol: STAREQ  */
#line 1304 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator*="); }
#line 3847 "src/parser.c"
    break;

  case 73: /* operator_symbol: SLASHEQ  */
#line 1305 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator/="); }
#line 3853 "src/parser.c"
    break;

  case 74: /* operator_symbol: '[' ']'  */
#line 1306 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator[]"); }
#line 3859 "src/parser.c"
    break;

  case 75: /* operator_symbol: '(' ')'  */
#line 1307 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator()"); }
#line 3865 "src/parser.c"
    break;

  case 76: /* operator_symbol: INC  */
#line 1308 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator++"); }
#line 3871 "src/parser.c"
    break;

  case 77: /* operator_symbol: DEC  */
#line 1309 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator--"); }
#line 3877 "src/parser.c"
    break;

  case 78: /* $@3: %empty  */
#line 1313 "src/parser.y"
                                          { symtab_push_scope(g_symtab, NULL, 0); }
#line 3883 "src/parser.c"
    break;

  case 79: /* func_header: type_spec pointer_opt func_name '(' $@3 opt_param_list ')' opt_const  */
#line 1314 "src/parser.y"
        {
            /* Overload note: this inserts every overload of `name` into
             * the same bucket, later ones shadowing earlier ones for
             * lookup purposes. That's harmless for this skeleton, since
             * the symbol table is only consulted for type-vs-value
             * classification here, not signature matching -- but real
             * overload *resolution* (picking the right overload at a call
             * site) needs a proper per-scope overload set, which belongs
             * in the semantic-analysis pass over the AST, not in this
             * table. */
            symtab_insert(g_symtab, g_symtab->current->parent, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.str), SYM_FUNC);
            ((*yyvalp).node) = ast_new(AST_FUNC_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line); /* pointer_opt lets a function's return type be
                              a pointer or reference to T -- see
                              docs/VIRCON32_QUIRKS.md entry #12 for why
                              this was missing and what closing it
                              required on the lowering side. */
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->virt_spec = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival) & (VIRT_SPEC_OVERRIDE | VIRT_SPEC_FINAL);
            ((*yyvalp).node)->str2 = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival) & 1) ? strdup("const") : NULL; /* see AST_FUNC_DECL's
                own doc comment in ast.h for this field's meaning here --
                str2 is otherwise completely unused across this whole
                func_decl/func_def/out_of_line_def family, confirmed
                directly before repurposing it, not assumed. */
        }
#line 3914 "src/parser.c"
    break;

  case 80: /* $@4: %empty  */
#line 1340 "src/parser.y"
                    { symtab_push_scope(g_symtab, NULL, 0); }
#line 3920 "src/parser.c"
    break;

  case 81: /* func_header: TYPE_NAME '(' $@4 opt_param_list ')'  */
#line 1341 "src/parser.y"
        {
            /* Constructor: the name token is TYPE_NAME because it's the
             * enclosing class's own (already-registered) name -- see
             * class_decl above. No return type. */
            ((*yyvalp).node) = ast_new(AST_FUNC_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = NULL;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 3934 "src/parser.c"
    break;

  case 82: /* func_header: '~' TYPE_NAME '(' ')'  */
#line 1351 "src/parser.y"
        {
            symtab_push_scope(g_symtab, NULL, 0); /* kept for symmetry with the pop in func_decl/func_def */
            ((*yyvalp).node) = ast_new(AST_FUNC_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            size_t len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str)) + 2;
            char *dtor_name = malloc(len);
            snprintf(dtor_name, len, "~%s", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->str1 = dtor_name;
            ((*yyvalp).node)->type = NULL;
            ((*yyvalp).node)->list = ast_list_new();
        }
#line 3949 "src/parser.c"
    break;

  case 83: /* $@5: %empty  */
#line 1366 "src/parser.y"
                   { symtab_push_scope(g_symtab, NULL, 0); }
#line 3955 "src/parser.c"
    break;

  case 84: /* implicit_int_header: IDENTIFIER '(' $@5 opt_param_list ')'  */
#line 1367 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current->parent, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str), SYM_FUNC);
            ((*yyvalp).node) = make_func_header(ast_ident("int", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), 0, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 3964 "src/parser.c"
    break;

  case 85: /* $@6: %empty  */
#line 1377 "src/parser.y"
        {
            g_current_class_sym = NULL;
            symtab_push_scope(g_symtab, "__anonymous", 1);
        }
#line 3973 "src/parser.c"
    break;

  case 86: /* anon_tag_decl: ANON_STRUCT $@6 class_body  */
#line 1382 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_CLASS_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = anon_tag_name();
            ((*yyvalp).node)->str2 = NULL;
            ((*yyvalp).node)->access = ACC_PUBLIC;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 1;
        }
#line 3987 "src/parser.c"
    break;

  case 87: /* anon_tag_decl: ANON_UNION '{' union_member_list '}'  */
#line 1392 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_UNION_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = anon_tag_name();
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 3997 "src/parser.c"
    break;

  case 88: /* anon_tag_decl: ANON_ENUM '{' enumerator_list '}'  */
#line 1398 "src/parser.y"
        {
            parse_record_enum_values(&(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list));
            ((*yyvalp).node) = ast_new(AST_ENUM_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = anon_tag_name();
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 4008 "src/parser.c"
    break;

  case 89: /* func_decl: opt_virtual func_header  */
#line 1408 "src/parser.y"
        {
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival); /* virtual-ness, per parsing -- see AST_FUNC_DECL
                             * in ast.h. Note this reflects only what was
                             * literally written here; sema.c's vtable
                             * builder may ALSO set this to 1 on a class
                             * whose method silently overrides an inherited
                             * virtual slot without repeating the keyword
                             * (matching real C++), so by the time sema has
                             * run, ival means "is this virtual", not just
                             * "was 'virtual' written on this exact line". */
            symtab_pop_scope(g_symtab); /* prototype only; no body needs the param scope */
        }
#line 4026 "src/parser.c"
    break;

  case 90: /* func_def: opt_virtual func_header opt_member_init_list block  */
#line 1425 "src/parser.y"
        {
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->kind = AST_FUNC_DEF;
            ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival); /* see the comment in func_decl above */
            ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);    /* member-initializer list, or NULL -- see
                               opt_member_init_list's own doc comment.
                               Grammatically reachable after ANY func_header
                               (ordinary method, destructor, constructor
                               alike), not just a constructor's -- sema.c's
                               resolve_member_init_list is what actually
                               rejects a non-constructor writing one, the
                               same "parser accepts the shape, sema.c
                               diagnoses the misuse" split this grammar
                               already relies on elsewhere (e.g. `break`
                               outside a loop is a parse-time non-issue,
                               caught by sema.c instead). */
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            symtab_pop_scope(g_symtab);
        }
#line 4050 "src/parser.c"
    break;

  case 91: /* $@7: %empty  */
#line 1480 "src/parser.y"
                                                     { symtab_push_scope(g_symtab, NULL, 0); }
#line 4056 "src/parser.c"
    break;

  case 92: /* out_of_line_def: type_spec pointer_opt qname_prefix func_name '(' $@7 opt_param_list ')' opt_const block  */
#line 1481 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_FUNC_DEF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line); /* see func_header's identical pointer_opt
                              handling above -- out-of-line definitions need
                              the same pointer/reference return-type
                              support */
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) & (VIRT_SPEC_OVERRIDE | VIRT_SPEC_FINAL)) {
                /* C++ allows virt-specifiers only on the in-class
                 * declaration, never on an out-of-line definition. */
                yyerror("'override' and 'final' belong on the declaration "
                        "inside the class, not on an out-of-line definition");
                g_parse_errors++;
            }
            ((*yyvalp).node)->str2 = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) & 1) ? strdup("const") : NULL; /* see func_header's
                own identical assignment above, and AST_FUNC_DECL's doc
                comment in ast.h, for this field's meaning */
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->b->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.list);
            symtab_pop_scope(g_symtab);
        }
#line 4084 "src/parser.c"
    break;

  case 93: /* $@8: %empty  */
#line 1504 "src/parser.y"
                         { symtab_push_scope(g_symtab, NULL, 0); }
#line 4090 "src/parser.c"
    break;

  case 94: /* out_of_line_def: qualified_type '(' $@8 opt_param_list ')' opt_member_init_list block  */
#line 1505 "src/parser.y"
        {
            /* Constructor: Class::Class(...) {}.
             *
             * This used to be written as an inline "qname_prefix TYPE_NAME
             * '(' ..." -- i.e. a second, separate grammar rule with the
             * exact same right-hand side as qualified_type's own
             * production (qname_prefix TYPE_NAME), just followed by more
             * symbols. That turned out to be a real bug, not a benign
             * conflict: bison's generated parser rejected '(' after
             * "Counter::Counter" and only accepted a further "::",
             * confirmed against an actual failing build (see the
             * postmortem in docs/DESIGN_NOTES.md -- this is exactly the
             * kind of thing that needs to be verified against real bison
             * output, not just pattern-matched against a known-benign
             * conflict family, which is the mistake that let this ship in
             * the first place).
             *
             * Routing through qualified_type instead means there's only
             * ONE grammar path that ever reduces "qname_prefix TYPE_NAME",
             * and the only fork left is "qualified_type used as a return
             * type" (type_spec's existing alternative) vs "qualified_type
             * followed directly by '('" (this rule) -- structurally
             * identical to the var_decl-vs-func_decl/func_def fork this
             * grammar already relies on everywhere else, and which is
             * actually confirmed working (every existing test file
             * exercises it).
             *
             * $1 (qualified_type) bundles the WHOLE dotted name as a flat
             * list -- e.g. [Counter, Counter] for `Counter::Counter`, or
             * [v32, Timer, Timer] for a hypothetical nested case -- so the
             * constructor's own name is $1's LAST component, and the
             * qualifier chain (what the other two out_of_line_def
             * alternatives store in `b`) is everything before it.
             */
            AstList *parts = &(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node)->list;
            int n = parts->count;
            ((*yyvalp).node) = ast_new(AST_FUNC_DEF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup(parts->items[n - 1]->str1);
            ((*yyvalp).node)->type = NULL;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);    /* member-initializer list, or NULL -- see
                               opt_member_init_list's own doc comment */
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            for (int i = 0; i < n - 1; i++) {
                ast_list_append(&((*yyvalp).node)->b->list, parts->items[i]);
            }
            symtab_pop_scope(g_symtab);
        }
#line 4144 "src/parser.c"
    break;

  case 95: /* out_of_line_def: qname_prefix '~' TYPE_NAME '(' ')' block  */
#line 1555 "src/parser.y"
        {
            /* Destructor: Class::~Class() {} -- never takes parameters, so
             * no function-scope push/pop is needed here (unlike the two
             * alternatives above). */
            ((*yyvalp).node) = ast_new(AST_FUNC_DEF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            size_t len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str)) + 2;
            char *dtor_name = malloc(len);
            snprintf(dtor_name, len, "~%s", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->str1 = dtor_name;
            ((*yyvalp).node)->type = NULL;
            ((*yyvalp).node)->list = ast_list_new();
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->b->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.list);
        }
#line 4164 "src/parser.c"
    break;

  case 96: /* opt_param_list: %empty  */
#line 1573 "src/parser.y"
                    { ((*yyvalp).list) = ast_list_new(); }
#line 4170 "src/parser.c"
    break;

  case 97: /* opt_param_list: VOID_KW  */
#line 1574 "src/parser.y"
                              { ((*yyvalp).list) = ast_list_new(); /* `(void)` -- real C's own "no parameters" spelling, same fix as opt_func_ptr_param_list's own VOID_KW alternative. A genuine, PRE-EXISTING gap, unrelated to function pointers -- found only because a function-pointer test happened to also declare an ordinary function using this spelling. A bare VOID_KW also matches param's unnamed-parameter form (`void leave(int);`); %dprec picks this one. */ }
#line 4176 "src/parser.c"
    break;

  case 98: /* opt_param_list: param_list  */
#line 1575 "src/parser.y"
                              { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 4182 "src/parser.c"
    break;

  case 99: /* param_list: param  */
#line 1579 "src/parser.y"
                               { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4188 "src/parser.c"
    break;

  case 100: /* param_list: param_list ',' param  */
#line 1580 "src/parser.y"
                               { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4194 "src/parser.c"
    break;

  case 101: /* param: type_spec pointer_opt IDENTIFIER  */
#line 1585 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4205 "src/parser.c"
    break;

  case 102: /* param: type_spec pointer_opt IDENTIFIER '=' expr  */
#line 1592 "src/parser.y"
        {
            /* Default parameter value -- `void greet(int x, int y = 5);`
             * -- stored in AST_PARAM's own previously-unused `a` slot
             * (see its own doc comment in ast.h). Reuses `expr` directly
             * rather than a narrower "constant-expression-only"
             * production: real C++ allows any expression here (a call,
             * another parameter... no, not another parameter, but a
             * global, a class's own static member, etc.), and this
             * grammar has no comma operator at the `expr` level at all
             * (confirmed before relying on it -- see `expr`'s own
             * production further down), so there's no ambiguity between
             * this default value and `param_list`'s own comma
             * separators the way there would be in a grammar that DID
             * have one. No arity/ordering validation happens here (real
             * C++ requires every parameter AFTER the first defaulted one
             * to also have a default) -- sema.c's own overload-resolution
             * arity check is where a call missing a required, non-
             * defaulted argument gets caught; a malformed declaration
             * that defaults an EARLIER parameter but not a later one is
             * accepted rather than specially diagnosed, matching this
             * project's own "miss a case rather than guess wrong"
             * philosophy for a pattern no real intro-level test is
             * likely to hit deliberately. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str), SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4239 "src/parser.c"
    break;

  case 103: /* param: type_spec pointer_opt IDENTIFIER '[' ']'  */
#line 1622 "src/parser.y"
        {
            /* Array PARAMETER syntax, `void foo(int arr[])` -- matches
             * real C/C++'s own array-to-pointer decay: a parameter
             * declared this way is semantically IDENTICAL to a pointer
             * parameter with no size information preserved at all, so
             * it's represented as an ordinary AST_POINTER_TYPE directly,
             * not AST_ARRAY_TYPE (which owns and carries its own known
             * length -- a parameter never does). Nothing downstream
             * needs to know this parameter was ever spelled with
             * brackets at all; by the time sema.c or codegen.c sees it,
             * it's just a pointer, the same as `int *arr` would produce. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str), SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
            AstNode *base = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_pointer(base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 4261 "src/parser.c"
    break;

  case 104: /* param: type_spec pointer_opt IDENTIFIER '[' array_dim ']'  */
#line 1640 "src/parser.y"
        {
            /* Array parameter WITH a size written, `void foo(int
             * arr[8])` or `int arr[MAX]` -- real C++ accepts and silently
             * ignores the size here too (it plays no role at all; the
             * parameter is still just a pointer), so this project does
             * the same: $5 (the size) is intentionally unused. Same decay
             * reasoning as the empty-bracket alternative immediately
             * above. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            AstNode *base = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_pointer(base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
        }
#line 4280 "src/parser.c"
    break;

  case 105: /* param: type_spec pointer_opt  */
#line 1655 "src/parser.y"
        {
            /* `void leave(int);` -- a prototype may leave its parameters
             * unnamed. Vircon32 C requires a name, so one is made up. */
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = unnamed_param_name();
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
        }
#line 4292 "src/parser.c"
    break;

  case 106: /* param: type_spec pointer_opt '[' ']'  */
#line 1663 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = unnamed_param_name();
            ((*yyvalp).node)->type = ast_wrap_pointer(apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
        }
#line 4302 "src/parser.c"
    break;

  case 107: /* param: ELLIPSIS  */
#line 1669 "src/parser.y"
        {
            /* `...`: the extra arguments arrive as one pointer to an array
             * of words, which lower.c builds at each call (see va_rewrite
             * there). str2 marks the parameter. */
            if (!g_c_mode) {
                yyerror("variadic functions (`...`) are supported for C input only "
                        "(a .c file); in C++ use overloads or default arguments");
                g_parse_errors++;
            }
            symtab_insert(g_symtab, g_symtab->current, "__v32_va", SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("__v32_va");
            ((*yyvalp).node)->str2 = strdup("...");
            ((*yyvalp).node)->type = ast_wrap_pointer(ast_ident("int", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4322 "src/parser.c"
    break;

  case 108: /* param: type_spec pointer_opt '(' '*' IDENTIFIER ')' '(' opt_func_ptr_param_list ')'  */
#line 1685 "src/parser.y"
        {
            /* a function pointer parameter: `void (*func)(int)` */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str), SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = ast_wrap_func_ptr(apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line);
        }
#line 4334 "src/parser.c"
    break;

  case 109: /* param: type_spec pointer_opt '(' '*' ')' '(' opt_func_ptr_param_list ')'  */
#line 1693 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = unnamed_param_name();
            ((*yyvalp).node)->type = ast_wrap_func_ptr(apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
        }
#line 4344 "src/parser.c"
    break;

  case 110: /* pointer_opt: %empty  */
#line 1701 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 4350 "src/parser.c"
    break;

  case 111: /* pointer_opt: '*' '*'  */
#line 1702 "src/parser.y"
                   { ((*yyvalp).ival) = 3; }
#line 4356 "src/parser.c"
    break;

  case 112: /* pointer_opt: '*' '*' '*'  */
#line 1703 "src/parser.y"
                   { ((*yyvalp).ival) = 4; }
#line 4362 "src/parser.c"
    break;

  case 113: /* pointer_opt: '*'  */
#line 1704 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 4368 "src/parser.c"
    break;

  case 114: /* pointer_opt: '&'  */
#line 1705 "src/parser.y"
                   { ((*yyvalp).ival) = 2; }
#line 4374 "src/parser.c"
    break;

  case 115: /* pointer_opt: '*' CONST  */
#line 1706 "src/parser.y"
                   { ((*yyvalp).ival) = 1; /* `T* const p` -- a const POINTER (as opposed
                                to pointer-to-const, `const T*`). Accepted
                                and emitted as a plain `T*`: the qualifier
                                only forbids reseating p, which nothing in
                                this project enforces for any const, and
                                keeping it would only feed Vircon32 C's
                                const strictness (lower.c, phase 11). */ }
#line 4386 "src/parser.c"
    break;

  case 116: /* tag_def_type: class_decl  */
#line 1726 "src/parser.y"
                                   { ((*yyvalp).node) = hoist_tag_def((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4392 "src/parser.c"
    break;

  case 117: /* tag_def_type: union_decl  */
#line 1727 "src/parser.y"
                                   { ((*yyvalp).node) = hoist_tag_def((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4398 "src/parser.c"
    break;

  case 118: /* tag_def_type: enum_decl  */
#line 1728 "src/parser.y"
                                   { ((*yyvalp).node) = hoist_tag_def((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4404 "src/parser.c"
    break;

  case 119: /* tag_def_type: anon_tag_decl  */
#line 1729 "src/parser.y"
                                   { ((*yyvalp).node) = hoist_tag_def((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4410 "src/parser.c"
    break;

  case 120: /* type_spec: INT_KW  */
#line 1733 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("int", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4416 "src/parser.c"
    break;

  case 121: /* type_spec: FLOAT_KW  */
#line 1734 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("float", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4422 "src/parser.c"
    break;

  case 122: /* type_spec: VOID_KW  */
#line 1735 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("void", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4428 "src/parser.c"
    break;

  case 123: /* type_spec: BOOL_KW  */
#line 1736 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("bool", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4434 "src/parser.c"
    break;

  case 124: /* type_spec: CHAR_KW  */
#line 1737 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("char", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4440 "src/parser.c"
    break;

  case 125: /* type_spec: TYPE_NAME  */
#line 1738 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4446 "src/parser.c"
    break;

  case 126: /* type_spec: TAG_NAME  */
#line 1739 "src/parser.y"
                    { ((*yyvalp).node) = tag_ref((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); /* C input: a tag
                         used bare, Vircon32 C style -- see c_tag_known */ }
#line 4453 "src/parser.c"
    break;

  case 127: /* type_spec: class_or_struct_kw name_tok  */
#line 1741 "src/parser.y"
                                   { ((*yyvalp).node) = tag_ref((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4459 "src/parser.c"
    break;

  case 128: /* type_spec: UNION name_tok  */
#line 1742 "src/parser.y"
                                   { ((*yyvalp).node) = tag_ref((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4465 "src/parser.c"
    break;

  case 129: /* type_spec: ENUM name_tok  */
#line 1743 "src/parser.y"
                                   { ((*yyvalp).node) = tag_ref((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4471 "src/parser.c"
    break;

  case 130: /* type_spec: qualified_type  */
#line 1745 "src/parser.y"
                     { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4477 "src/parser.c"
    break;

  case 131: /* type_spec: STD_ARRAY '<' type_spec pointer_opt ',' INT_LITERAL '>'  */
#line 1747 "src/parser.y"
        {
            /* std::array<T, N> -- NOT template syntax. Like the four
             * cast keywords further down, this is one fixed form that
             * only exists right after the STD_ARRAY token, so '<' and
             * '>' here never compete with the comparison operators.
             * The type it produces is an ordinary class, written out
             * by generic.c; see generic.h. */
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 2) {
                yyerror("std::array cannot hold references");
                g_parse_errors++;
                YYERROR;
            }
            AstNode *elem = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line) : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node) = generic_array_type(elem, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.lit).ival, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            if (((*yyvalp).node) == NULL) YYERROR;
        }
#line 4498 "src/parser.c"
    break;

  case 132: /* type_spec: STD_VECTOR '<' type_spec pointer_opt '>'  */
#line 1764 "src/parser.y"
        {
            /* std::vector<T> -- same fixed form as std::array, one
             * argument. */
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 2) {
                yyerror("std::vector cannot hold references");
                g_parse_errors++;
                YYERROR;
            }
            AstNode *elem = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line) : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node) = generic_vector_type(elem, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            if (((*yyvalp).node) == NULL) YYERROR;
        }
#line 4515 "src/parser.c"
    break;

  case 133: /* type_spec: STD_ARRAY '<' type_spec pointer_opt ',' IDENTIFIER '>'  */
#line 1777 "src/parser.y"
        {
            /* The length as a named constant (an enum constant or a
             * `const int`). A #define'd length arrives as INT_LITERAL
             * above, already expanded by the pre-scan. */
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 2) {
                yyerror("std::array cannot hold references");
                g_parse_errors++;
                YYERROR;
            }
            AstNode *elem = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line) : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node) = generic_array_type(elem, 0, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            if (((*yyvalp).node) == NULL) YYERROR;
        }
#line 4533 "src/parser.c"
    break;

  case 134: /* type_spec: CONST type_spec  */
#line 1791 "src/parser.y"
        {
            /* `const T` -- a single new leading token (CONST) for
             * type_spec, unique among every one of its existing
             * alternatives' own starting tokens (INT_KW, FLOAT_KW,
             * VOID_KW, BOOL_KW, CHAR_KW, TYPE_NAME, IDENTIFIER-via-
             * qualified_type) -- so this doesn't create any NEW
             * ambiguity at the point type_spec itself is expected;
             * wherever type_spec could already start (var_decl, param,
             * func_ptr_param_type, typedef_decl, a function's own
             * return type, ...), `const` becomes available there too,
             * for free, with no need to touch each of those
             * productions individually. Right-recursive on type_spec
             * itself rather than a fixed "CONST base_type_only" shape,
             * so `const` composes correctly with a qualified type too
             * (`const Foo::Bar x;`) the same way plain type_spec
             * already does -- and, harmlessly, allows nonsensical
             * repetition like `const const int` to parse (matching
             * real C++'s own permissiveness here; redundant `const` is
             * legal, if pointless, in real C++ too). See
             * AST_CONST_TYPE's own doc comment in ast.h for this
             * project's own scope boundary: only ever a PREFIX before
             * a type (pointee-const), never a suffix after a pointer's
             * own '*' (a const POINTER itself, `int * const p`, isn't
             * accepted), and no actual const-correctness ENFORCEMENT
             * anywhere -- accepted and correctly emitted, not
             * validated. */
            ((*yyvalp).node) = ast_wrap_const((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
        }
#line 4566 "src/parser.c"
    break;

  case 135: /* name_tok: IDENTIFIER  */
#line 1827 "src/parser.y"
                  { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 4572 "src/parser.c"
    break;

  case 136: /* name_tok: TYPE_NAME  */
#line 1828 "src/parser.y"
                  { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 4578 "src/parser.c"
    break;

  case 137: /* qname_prefix: name_tok COLONCOLON  */
#line 1833 "src/parser.y"
        {
            ((*yyvalp).list) = ast_list_new();
            ast_list_append(&((*yyvalp).list), ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line));
        }
#line 4587 "src/parser.c"
    break;

  case 138: /* qname_prefix: qname_prefix name_tok COLONCOLON  */
#line 1838 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).list), ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line));
        }
#line 4596 "src/parser.c"
    break;

  case 139: /* qualified_type: qname_prefix TYPE_NAME  */
#line 1846 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).node)->list, ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line));
        }
#line 4606 "src/parser.c"
    break;

  case 140: /* qualified_id_expr: qname_prefix IDENTIFIER  */
#line 1855 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).node)->list, ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line));
        }
#line 4616 "src/parser.c"
    break;

  case 141: /* var_decl: tag_def_type pointer_opt IDENTIFIER opt_initializer more_plain_declarators  */
#line 1866 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str), SYM_VAR);
            AstNode *first = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            first->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
            first->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            first->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node) = finish_declarators(first, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 4629 "src/parser.c"
    break;

  case 142: /* var_decl: tag_def_type pointer_opt IDENTIFIER array_bracket_list opt_array_initializer more_plain_declarators  */
#line 1875 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_VAR);
            AstNode *first = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            first->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            size_unsized_array((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            first->type = ast_wrap_array_dims(apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            first->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node) = finish_declarators(first, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
        }
#line 4643 "src/parser.c"
    break;

  case 143: /* var_decl: type_spec pointer_opt IDENTIFIER opt_initializer more_plain_declarators  */
#line 1885 "src/parser.y"
        {
            /* The plain declarator, now with an optional comma-separated
             * tail of MORE plain declarators sharing this SAME base type
             * -- real C/C++'s own "multiple declarators in one
             * statement" idiom (`int a, b, c;`, `int a, *b, c = 5;`,
             * pointer-ness genuinely PER-declarator, matching real C++:
             * `int *a, b;` declares a pointer and a plain int, not two
             * pointers). Deliberately narrower than real C++'s full
             * declarator grammar: an array or function-pointer
             * declarator can't appear after the first comma here (or as
             * the first declarator when a comma follows) -- see
             * AST_VAR_DECL_GROUP's own doc comment in ast.h for the full
             * reasoning on that scope boundary; those shapes keep using
             * this production's own sibling alternatives below,
             * unchanged, exactly as before this round.
             *
             * more_plain_declarators can't see `$1` (a separate
             * nonterminal only ever sees its own RHS symbols in bison,
             * never a parent rule's), so it hands back each additional
             * declarator as a bare, UNRESOLVED carrier (name + pointer_
             * opt value in ->ival + initializer -- see its own comment)
             * for THIS action, which does have `$1`, to finish
             * resolving into a real AST_VAR_DECL each, the same
             * pointer_opt-to-type resolution the primary declarator just
             * below already does. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str), SYM_VAR);
            AstNode *first = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            first->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
            first->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            first->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);

            ((*yyvalp).node) = finish_declarators(first, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 4681 "src/parser.c"
    break;

  case 144: /* var_decl: type_spec IDENTIFIER '(' arg_list ')'  */
#line 1919 "src/parser.y"
        {
            /* Direct-initialization with constructor arguments on a
             * stack-allocated local -- `Shape shape(7);` -- a real,
             * previously-flagged gap (README.md, "What doesn't exist yet")
             * finally closed. Deliberately narrower than this grammar's
             * other var_decl alternatives in two ways, both scoping
             * choices rather than oversights:
             *
             *   1. No `pointer_opt` -- this production exists specifically
             *      for a VALUE local (matching every existing example and
             *      the user's own stated request); a pointer local
             *      already has its own, unambiguous direct-init spelling
             *      (`Shape *p = someShapePtr;`) that doesn't need this at
             *      all, and admitting one here would only reopen the
             *      "most vexing parse" ambiguity point 2 below sidesteps.
             *
             *   2. `arg_list`, not `opt_arg_list` -- empty parens
             *      (`Shape shape();`) are deliberately NOT accepted by
             *      this alternative at all, matching real C++'s own
             *      "most vexing parse" resolution: `Shape shape();` is a
             *      FUNCTION DECLARATION in real C++ (a function named
             *      `shape`, taking no arguments, returning `Shape`), not
             *      object construction, however surprising that reads to
             *      someone writing it for the first time. Requiring at
             *      least one argument here means this alternative's own
             *      first token after '(' is always something that starts
             *      an EXPRESSION (an identifier, a literal, a unary
             *      operator, `new`, `this`, ...) -- never something that
             *      starts a TYPE (a primitive keyword, TYPE_NAME, `const`),
             *      which is exactly what func_header's own parameter-list
             *      alternative starting from this same
             *      "type_spec IDENTIFIER '('" prefix requires instead.
             *      Those two first-sets are disjoint (confirmed directly
             *      against this grammar's own primary_expr, which never
             *      accepts a bare TYPE_NAME as an expression-starting
             *      token -- see the C-style-cast production's own doc
             *      comment above for that same fact stated and relied on
             *      already), so bison can tell the two productions apart
             *      by ordinary one-token lookahead the moment it sees
             *      what comes right after '(' -- no new shift/reduce
             *      conflict, confirmed by an actual clean bison
             *      regeneration with the pre-existing %expect count
             *      unchanged, not merely reasoned about on paper.
             *
             * Resolving WHICH constructor overload `arg_list` matches
             * happens later, in sema.c (mirroring `new T(args)`'s own
             * resolve_new_expr) -- the parser only records the raw
             * argument list here, on a dedicated AST_DIRECT_INIT marker
             * node (never anywhere an ordinary expression is expected;
             * see its own doc comment in ast.h for the full mechanism
             * and why it isn't just AST_NEW reused). */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_VAR);
            ((*yyvalp).node) = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            AstNode *direct_init = ast_new(AST_DIRECT_INIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            direct_init->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->a = direct_init;
        }
#line 4745 "src/parser.c"
    break;

  case 145: /* var_decl: type_spec pointer_opt IDENTIFIER array_bracket_list opt_array_initializer more_plain_declarators  */
#line 1979 "src/parser.y"
        {
            /* Standard C/C++ array declarator: length AFTER the name --
             * `int scores[8];`, or multi-dimensional (`int grid[8][4];`,
             * handled uniformly here since array_bracket_list already
             * accepts one OR MORE bracket groups -- see its own comment
             * below and ast_wrap_array_dims's in ast.c for how the
             * correct nesting gets built regardless of dimension
             * count), optionally `= {1, 2, 3};` alongside it. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_VAR);
            AstNode *first = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            first->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            AstNode *base = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            size_unsized_array((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            first->type = ast_wrap_array_dims(base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            first->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            /* `int b[ 4 ], a;` -- further declarators after an array one */
            ((*yyvalp).node) = finish_declarators(first, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
        }
#line 4768 "src/parser.c"
    break;

  case 146: /* var_decl: type_spec array_bracket_list IDENTIFIER opt_array_initializer  */
#line 1998 "src/parser.y"
        {
            /* Vircon32-native-style array declarator, accepted as an
             * ALTERNATE valid C++-side input form -- same meaning as
             * the standard-C alternative above, length BEFORE the name
             * instead of after (`int [8] scores;`, or multi-dimensional
             * `int [8][4] grid;`), matching Vircon32 C itself.
             * Deliberately no pointer_opt here (unlike the standard-C
             * form) -- this form exists specifically to let someone
             * already fluent in Vircon32 C, or transitioning from it,
             * keep writing what's already familiar to them without
             * having to also learn a second, unrelated declarator
             * convention; it isn't trying to be a general C-declarator
             * sublanguage of its own. Both forms produce an identical,
             * identically-nested AST_ARRAY_TYPE structure regardless of
             * dimension count -- codegen always emits Vircon32's own
             * required form regardless of which one the source used, so
             * this choice is purely a source-reading preference, never
             * a behavioral one. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_VAR);
            ((*yyvalp).node) = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str));
            size_unsized_array((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_array_dims((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4798 "src/parser.c"
    break;

  case 147: /* var_decl: type_spec pointer_opt '(' '*' IDENTIFIER ')' '(' opt_func_ptr_param_list ')' opt_initializer  */
#line 2024 "src/parser.y"
        {
            /* Standard-C function-pointer declarator --
             * `ReturnType (*name)(ParamTypes);`. Accepted as an
             * ALTERNATE valid C++-side input form alongside the
             * Vircon32-native one just below -- same "two spellings,
             * one AST shape, one always-Vircon32-style output"
             * treatment the array declarators above already
             * established (see AST_FUNC_PTR_TYPE's own doc comment in
             * ast.h, and docs/VIRCON32_QUIRKS.md's "Function-pointer
             * declarator syntax reversed" entry, confirmed against the
             * real compiler for vtable-slot emission well before this
             * grammar existed to reach the same node from a source-
             * level declaration). Disambiguated from the Vircon32-style
             * alternative below by the very next token after this
             * rule's own opening '(': a bare '*' can only ever start
             * THIS form (type_spec's own first-set never includes
             * '*'), so the two never actually compete for the same
             * lookahead. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.str), SYM_VAR);
            ((*yyvalp).node) = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.str));
            AstNode *ret = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4828 "src/parser.c"
    break;

  case 148: /* var_decl: type_spec pointer_opt '(' opt_func_ptr_param_list ')' '*' IDENTIFIER opt_initializer  */
#line 2050 "src/parser.y"
        {
            /* Vircon32-native function-pointer declarator --
             * `ReturnType(ParamTypes)* name;` -- see the standard-C
             * alternative just above for the full reasoning (shared
             * between both). Both alternatives build the identical
             * AST_FUNC_PTR_TYPE regardless of which one matched; the
             * AST itself carries no memory of which spelling the
             * source used. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_VAR);
            ((*yyvalp).node) = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str));
            AstNode *ret = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4848 "src/parser.c"
    break;

  case 149: /* var_decl: type_spec pointer_opt '(' '*' IDENTIFIER '[' array_dim ']' ')' '(' opt_func_ptr_param_list ')' opt_array_initializer  */
#line 2066 "src/parser.y"
        {
            /* Standard-C ARRAY-of-function-pointers declarator --
             * `ReturnType (*name[N])(ParamTypes);`. Builds an
             * AST_ARRAY_TYPE whose own element type is an
             * AST_FUNC_PTR_TYPE -- exactly the same structural
             * composition an ordinary array of any other type already
             * has (see AST_FUNC_PTR_TYPE's own doc comment in ast.h
             * for why this composes for free, needing no special
             * casing in ast_wrap_array or print_type beyond
             * AST_FUNC_PTR_TYPE having its own case). */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.str), SYM_VAR);
            ((*yyvalp).node) = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.str));
            AstNode *ret = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-11)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line);
            AstNode *fp = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_array(fp, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node)->ival, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node)->a;
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4872 "src/parser.c"
    break;

  case 150: /* var_decl: type_spec pointer_opt '(' opt_func_ptr_param_list ')' '*' '[' array_dim ']' IDENTIFIER opt_array_initializer  */
#line 2086 "src/parser.y"
        {
            /* Vircon32-native ARRAY-of-function-pointers declarator.
             * CONFIRMED against the real compiler (Matthew directly):
             * "ReturnType(ParamTypes)* [N] name;" -- the array
             * brackets positioned exactly where they'd go for an
             * ordinary array of any other Vircon32-style type,
             * directly before the name -- is genuinely correct
             * Vircon32 syntax, not just this project's own
             * extrapolation from the two individually-confirmed
             * patterns it was originally reasoned from. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_VAR);
            ((*yyvalp).node) = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str));
            AstNode *ret = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
            AstNode *fp = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_array(fp, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->ival, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->a;
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4896 "src/parser.c"
    break;

  case 151: /* more_plain_declarators: %empty  */
#line 2126 "src/parser.y"
        { ((*yyvalp).list) = ast_list_new(); }
#line 4902 "src/parser.c"
    break;

  case 152: /* more_plain_declarators: more_plain_declarators ',' pointer_opt IDENTIFIER opt_initializer  */
#line 2128 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.list);
            AstNode *spec = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            spec->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str));
            spec->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival);
            spec->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ast_list_append(&((*yyvalp).list), spec);
        }
#line 4915 "src/parser.c"
    break;

  case 153: /* more_plain_declarators: more_plain_declarators ',' pointer_opt IDENTIFIER array_bracket_list opt_array_initializer  */
#line 2137 "src/parser.y"
        {
            /* `int a, b[ 4 ];` -- an ARRAY declarator among the others.
             * Same carrier as the plain one, with its dimensions parked
             * in `list` for finish_declarators to wrap around the base. */
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.list);
            AstNode *spec = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            spec->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
            spec->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival);
            spec->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            spec->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ast_list_append(&((*yyvalp).list), spec);
        }
#line 4932 "src/parser.c"
    break;

  case 154: /* func_ptr_param_type: type_spec pointer_opt  */
#line 2161 "src/parser.y"
        {
            ((*yyvalp).node) = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
        }
#line 4940 "src/parser.c"
    break;

  case 155: /* func_ptr_param_type: type_spec pointer_opt name_tok  */
#line 2165 "src/parser.y"
        {
            ((*yyvalp).node) = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4948 "src/parser.c"
    break;

  case 156: /* func_ptr_param_list: func_ptr_param_type  */
#line 2172 "src/parser.y"
        { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4954 "src/parser.c"
    break;

  case 157: /* func_ptr_param_list: func_ptr_param_list ',' func_ptr_param_type  */
#line 2174 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4960 "src/parser.c"
    break;

  case 158: /* opt_func_ptr_param_list: %empty  */
#line 2178 "src/parser.y"
                           { ((*yyvalp).list) = ast_list_new(); }
#line 4966 "src/parser.c"
    break;

  case 159: /* opt_func_ptr_param_list: func_ptr_param_list  */
#line 2179 "src/parser.y"
                            { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 4972 "src/parser.c"
    break;

  case 160: /* array_bracket_list: '[' array_dim ']'  */
#line 2216 "src/parser.y"
        {
            ((*yyvalp).list) = ast_list_new();
            ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node));
        }
#line 4981 "src/parser.c"
    break;

  case 161: /* array_bracket_list: '[' ']'  */
#line 2221 "src/parser.y"
        {
            /* `int t[] = { 10, 20, 30 };` -- length left for the
             * initializer to decide; ival -1 until size_unsized_array
             * fills it in (Vircon32 C itself has no `int[] t`). */
            AstNode *dim = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            dim->ival = -1;
            ((*yyvalp).list) = ast_list_new();
            ast_list_append(&((*yyvalp).list), dim);
        }
#line 4995 "src/parser.c"
    break;

  case 162: /* array_bracket_list: array_bracket_list '[' array_dim ']'  */
#line 2231 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node));
        }
#line 5004 "src/parser.c"
    break;

  case 163: /* array_dim: expr  */
#line 2249 "src/parser.y"
        {
            int v = 0;
            if (!ast_fold_int((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), parse_enum_value, &v)) {
                fprintf(stderr, "%s:%d: error: array size is not an integer constant "
                        "expression (use a literal, a #define, an enum constant, or "
                        "arithmetic on those)\n", g_current_filename, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
                g_parse_errors++;
                v = 1;
            } else if (v < 0) {
                fprintf(stderr, "%s:%d: error: array size is negative (%d)\n",
                        g_current_filename, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line, v);
                g_parse_errors++;
                v = 1;
            }
            ((*yyvalp).node) = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->ival = v;
            ((*yyvalp).node)->a = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)->kind == AST_INT_LIT && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)->macro_name == NULL) ? NULL : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5027 "src/parser.c"
    break;

  case 164: /* braced_init: '{' '}'  */
#line 2276 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 5033 "src/parser.c"
    break;

  case 165: /* braced_init: '{' init_items '}'  */
#line 2278 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); }
#line 5039 "src/parser.c"
    break;

  case 166: /* braced_init: '{' init_items ',' '}'  */
#line 2280 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); }
#line 5045 "src/parser.c"
    break;

  case 167: /* init_items: init_item  */
#line 2284 "src/parser.y"
                                 { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5051 "src/parser.c"
    break;

  case 168: /* init_items: init_items ',' init_item  */
#line 2285 "src/parser.y"
                                 { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5057 "src/parser.c"
    break;

  case 169: /* init_item: expr  */
#line 2289 "src/parser.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5063 "src/parser.c"
    break;

  case 170: /* init_item: braced_init  */
#line 2290 "src/parser.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5069 "src/parser.c"
    break;

  case 171: /* opt_array_initializer: %empty  */
#line 2294 "src/parser.y"
                                      { ((*yyvalp).node) = NULL; }
#line 5075 "src/parser.c"
    break;

  case 172: /* opt_array_initializer: '=' braced_init  */
#line 2295 "src/parser.y"
                              { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5081 "src/parser.c"
    break;

  case 173: /* opt_array_initializer: '=' string_seq  */
#line 2297 "src/parser.y"
        {
            /* `int msg[8] = "Hello";` -- a string literal as an array
             * initializer, either accepted array-declarator spelling
             * (both route through opt_array_initializer). Expanded
             * here into the same AST_INIT_LIST shape `= {1, 2, 3}`
             * already produces -- one AST_INT_LIT per decoded
             * character plus the trailing terminator -- so sema.c's
             * existing item recursion, lower.c, and codegen.c's
             * print_expr all handle it unchanged, and the generated
             * C (`int msg[8] = {72, 101, ...};`) is valid under BOTH
             * --target modes, whereas a pass-through `= "Hello"`
             * would be invalid standard C for a non-char array and
             * would depend on unverified Vircon32 string-escape
             * parity. No new AST kind, no codegen changes. */
            ((*yyvalp).node) = string_literal_init_list((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
        }
#line 5102 "src/parser.c"
    break;

  case 174: /* opt_initializer: %empty  */
#line 2318 "src/parser.y"
                     { ((*yyvalp).node) = NULL; }
#line 5108 "src/parser.c"
    break;

  case 175: /* opt_initializer: '=' expr  */
#line 2319 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5114 "src/parser.c"
    break;

  case 176: /* opt_initializer: '=' braced_init  */
#line 2320 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); /* `Point2 p = { 1, 2 };` -- a struct
                                    (or any aggregate) initialized positionally */ }
#line 5121 "src/parser.c"
    break;

  case 177: /* tag_typedef_decl: TYPEDEF class_decl pointer_opt name_tok  */
#line 2330 "src/parser.y"
        { ((*yyvalp).node) = tag_typedef_group((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 5127 "src/parser.c"
    break;

  case 178: /* tag_typedef_decl: TYPEDEF union_decl pointer_opt name_tok  */
#line 2332 "src/parser.y"
        { ((*yyvalp).node) = tag_typedef_group((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 5133 "src/parser.c"
    break;

  case 179: /* tag_typedef_decl: TYPEDEF enum_decl pointer_opt name_tok  */
#line 2334 "src/parser.y"
        { ((*yyvalp).node) = tag_typedef_group((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 5139 "src/parser.c"
    break;

  case 180: /* $@9: %empty  */
#line 2336 "src/parser.y"
        {
            /* Anonymous: the typedef name (not seen yet) becomes the
             * struct's own name. Nothing inside the body can mention a
             * name it doesn't have, so a placeholder scope owner is
             * enough while the members are parsed. */
            g_current_class_sym = NULL;
            symtab_push_scope(g_symtab, "__anonymous", 1);
        }
#line 5152 "src/parser.c"
    break;

  case 181: /* tag_typedef_decl: TYPEDEF class_or_struct_kw $@9 class_body name_tok  */
#line 2345 "src/parser.y"
        {
            Scope *body = g_symtab->current;
            symtab_pop_scope(g_symtab);
            Symbol *sym = symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_CLASS);
            sym->inner_scope = body;
            ((*yyvalp).node) = ast_new(AST_CLASS_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->str2 = NULL;
            ((*yyvalp).node)->access = ACC_PUBLIC;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival);
        }
#line 5169 "src/parser.c"
    break;

  case 182: /* tag_typedef_decl: TYPEDEF UNION '{' union_member_list '}' name_tok  */
#line 2358 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_UNION);
            ((*yyvalp).node) = ast_new(AST_UNION_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 5180 "src/parser.c"
    break;

  case 183: /* tag_typedef_decl: TYPEDEF ENUM '{' enumerator_list '}' name_tok  */
#line 2365 "src/parser.y"
        {
            parse_record_enum_values(&(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list));
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_ENUM);
            ((*yyvalp).node) = ast_new(AST_ENUM_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 5192 "src/parser.c"
    break;

  case 184: /* typedef_decl: TYPEDEF type_spec pointer_opt name_tok  */
#line 2376 "src/parser.y"
        {
            /* name_tok, not IDENTIFIER: in `typedef struct Actor Actor;`
             * written after the struct's definition, the second Actor
             * already lexes as TYPE_NAME. */
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 0 && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node)->kind == AST_IDENT && strcmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node)->str1, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str)) == 0) {
                /* `typedef struct Actor Actor;` (or union/enum): the tag
                 * already is the type name here, so there is nothing to
                 * emit -- a `typedef Actor Actor;` in the output would
                 * only be a redefinition. See the tag-typedef note in the
                 * prologue. */
                declare_type_name((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_TYPEDEF);
                ((*yyvalp).node) = decl_group_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            } else {
                symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_TYPEDEF);
                ((*yyvalp).node) = ast_new(AST_TYPEDEF_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
                ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
                ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            }
        }
#line 5216 "src/parser.c"
    break;

  case 185: /* typedef_decl: TYPEDEF type_spec pointer_opt '(' '*' IDENTIFIER ')' '(' opt_func_ptr_param_list ')'  */
#line 2396 "src/parser.y"
        {
            /* Standard-C function-pointer typedef --
             * `typedef ReturnType (*Name)(ParamTypes);`. Reuses
             * AST_FUNC_PTR_TYPE/ast_wrap_func_ptr exactly as var_decl's
             * own standard-C function-pointer declarator does (see
             * var_decl's own comment on that production) -- a typedef
             * is just another place a function-pointer TYPE can be
             * named, and print_type_and_name (codegen.c) already
             * builds the correct declarator for either target from
             * this same AST shape, so no new codegen case was needed,
             * only routing emit_typedefs through print_type_and_name
             * instead of the plain print_type it used before this
             * production existed (print_type alone can't place a name
             * INSIDE the parens the standard-C form needs). Disambiguated
             * from the plain alternative above by the '(' immediately
             * after pointer_opt (an IDENTIFIER can't also be a '('), and
             * from the Vircon32-style alternative just below by the next
             * token after that same '(' -- a bare '*' can only ever start
             * THIS form, since type_spec's own first-set never includes
             * '*' (identical reasoning to var_decl's own two function-
             * pointer productions, not re-derived from scratch here). */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str), SYM_TYPEDEF);
            ((*yyvalp).node) = ast_new(AST_TYPEDEF_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            AstNode *ret = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line);
        }
#line 5248 "src/parser.c"
    break;

  case 186: /* typedef_decl: TYPEDEF type_spec pointer_opt '(' opt_func_ptr_param_list ')' '*' IDENTIFIER  */
#line 2424 "src/parser.y"
        {
            /* Vircon32-native function-pointer typedef --
             * `typedef ReturnType(ParamTypes)* Name;` -- see the
             * standard-C alternative just above for the full reasoning
             * (shared between both). Both alternatives build the
             * identical AST_FUNC_PTR_TYPE regardless of which one
             * matched, the same "AST carries no memory of which
             * spelling was used" treatment every other dual-accepted
             * declarator in this grammar already has. */
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_TYPEDEF);
            ((*yyvalp).node) = ast_new(AST_TYPEDEF_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            AstNode *ret = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
        }
#line 5268 "src/parser.c"
    break;

  case 187: /* enum_decl: ENUM name_tok '{' enumerator_list '}'  */
#line 2455 "src/parser.y"
        {
            parse_record_enum_values(&(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list));
            if (!g_c_mode) symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_ENUM);
            ((*yyvalp).node) = ast_new(AST_ENUM_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            tag_decl(((*yyvalp).node));
        }
#line 5281 "src/parser.c"
    break;

  case 188: /* enumerator_list: enumerator  */
#line 2466 "src/parser.y"
                                        { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5287 "src/parser.c"
    break;

  case 189: /* enumerator_list: enumerator_list ',' enumerator  */
#line 2468 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5293 "src/parser.c"
    break;

  case 190: /* enumerator_list: enumerator_list ','  */
#line 2470 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); /* trailing comma -- real C++ allows one after the last enumerator */ }
#line 5299 "src/parser.c"
    break;

  case 191: /* enumerator: IDENTIFIER  */
#line 2475 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ENUM_VALUE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str)); }
#line 5305 "src/parser.c"
    break;

  case 192: /* enumerator: IDENTIFIER '=' expr  */
#line 2477 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ENUM_VALUE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str)); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5311 "src/parser.c"
    break;

  case 193: /* union_decl: UNION name_tok '{' union_member_list '}'  */
#line 2502 "src/parser.y"
        {
            if (!g_c_mode) symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_UNION);
            ((*yyvalp).node) = ast_new(AST_UNION_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            tag_decl(((*yyvalp).node));
        }
#line 5323 "src/parser.c"
    break;

  case 194: /* union_member_list: %empty  */
#line 2512 "src/parser.y"
                                           { ((*yyvalp).list) = ast_list_new(); }
#line 5329 "src/parser.c"
    break;

  case 195: /* union_member_list: union_member_list var_decl ';'  */
#line 2513 "src/parser.y"
                                            { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node)); }
#line 5335 "src/parser.c"
    break;

  case 196: /* $@10: %empty  */
#line 2519 "src/parser.y"
        { symtab_push_scope(g_symtab, NULL, 0); }
#line 5341 "src/parser.c"
    break;

  case 197: /* block: '{' $@10 stmt_list '}'  */
#line 2520 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_BLOCK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 5351 "src/parser.c"
    break;

  case 198: /* stmt_list: %empty  */
#line 2528 "src/parser.y"
                         { ((*yyvalp).list) = ast_list_new(); }
#line 5357 "src/parser.c"
    break;

  case 199: /* stmt_list: stmt_list stmt  */
#line 2529 "src/parser.y"
                          { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5363 "src/parser.c"
    break;

  case 200: /* stmt: block  */
#line 2533 "src/parser.y"
                                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5369 "src/parser.c"
    break;

  case 201: /* stmt: IF '(' expr ')' stmt  */
#line 2535 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_IF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = NULL;
        }
#line 5378 "src/parser.c"
    break;

  case 202: /* stmt: IF '(' expr ')' stmt ELSE stmt  */
#line 2540 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_IF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5387 "src/parser.c"
    break;

  case 203: /* stmt: WHILE '(' expr ')' stmt  */
#line 2545 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_WHILE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5396 "src/parser.c"
    break;

  case 204: /* stmt: DO stmt WHILE '(' expr ')' ';'  */
#line 2550 "src/parser.y"
        {
            /* do-while -- reuses AST_WHILE (a=cond, b=body) with
             * ival=1 marking "test after", rather than a separate node
             * kind -- see AST_WHILE's own doc comment in ast.h for why
             * every OTHER pass that walks this node treats the two
             * identically, only codegen.c's own printing needs to
             * check the flag. */
            ((*yyvalp).node) = ast_new(AST_WHILE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->ival = 1;
        }
#line 5412 "src/parser.c"
    break;

  case 205: /* stmt: SWITCH '(' expr ')' '{' switch_body '}'  */
#line 2562 "src/parser.y"
        {
            /* Real C's own fall-through switch, passed through to
             * codegen.c essentially unchanged -- see AST_SWITCH's own
             * doc comment in ast.h for why no lowering transformation
             * happens here at all (Vircon32 C already has native
             * switch/case). switch_body builds one FLAT list mixing
             * CASE/DEFAULT labels with ordinary statements, in source
             * order -- not a list of separate per-case containers --
             * exactly matching real C's own grammar shape (a case/
             * default is a LABEL on the statement that follows it, not
             * a container), which is what makes fall-through "just
             * happen" rather than needing to be specially implemented
             * anywhere in this pipeline. */
            ((*yyvalp).node) = ast_new(AST_SWITCH, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 5434 "src/parser.c"
    break;

  case 206: /* stmt: for_open type_spec pointer_opt IDENTIFIER ':' expr ')' stmt  */
#line 2580 "src/parser.y"
        {
            /* Range-based for: `for (Enemy &e : enemies) body`. Written
             * out here, at parse time, as the loop it stands for --
             *
             *     for (Enemy *it = enemies.begin(); it != enemies.end(); ++it) {
             *         Enemy &e = *it;
             *         body
             *     }
             *
             * -- so every later pass sees an ordinary for loop over
             * pointers. Works for anything with begin()/end() returning
             * `T *`: std::array, std::vector, or a class of your own.
             * The element type may be `auto` (`auto`, `auto &`,
             * `const auto &`). A plain C array has no begin()/end() to
             * call.
             *
             * Differences from C++: the range expression is evaluated
             * once per begin() and once per end(), and end() is asked for
             * again on every pass rather than once up front. */
            symtab_pop_scope(g_symtab);
            static int range_counter = 0;
            char it_name[64];
            snprintf(it_name, sizeof it_name, "__v32_it%d", range_counter++);
            int line = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line;

            AstNode *elem = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node);                      /* element type, const dropped */
            while (elem->kind == AST_CONST_TYPE) elem = elem->a;
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.ival) == 1) elem = ast_wrap_pointer(elem, line);

            AstNode *begin = ast_new(AST_MEMBER, line);
            begin->str1 = strdup("."); begin->str2 = strdup("begin"); begin->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
            AstNode *begin_call = ast_new(AST_CALL, line);
            begin_call->a = begin; begin_call->list = ast_list_new();
            AstNode *end = ast_new(AST_MEMBER, line);
            end->str1 = strdup("."); end->str2 = strdup("end"); end->a = ast_clone_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node));
            AstNode *end_call = ast_new(AST_CALL, line);
            end_call->a = end; end_call->list = ast_list_new();

            AstNode *it_decl = ast_new(AST_VAR_DECL, line);
            it_decl->str1 = strdup(it_name);
            it_decl->type = ast_wrap_pointer(elem, line);
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.ival) != 1 && elem->kind == AST_IDENT && strcmp(elem->str1, "auto") == 0) {
                /* `for (auto &e : enemies)`: the iterator is whatever
                 * begin() returns, and sema deduces both from there. */
                it_decl->type = ast_ident("auto", line);
            }
            it_decl->a = begin_call;

            AstNode *cond = ast_new(AST_BINOP, line);
            cond->str1 = strdup("!=");
            cond->a = ast_ident(it_name, line);
            cond->b = end_call;

            AstNode *step = ast_new(AST_UNOP, line);
            step->str1 = strdup("pre++");
            step->a = ast_ident(it_name, line);

            AstNode *deref = ast_new(AST_UNOP, line);
            deref->str1 = strdup("deref");
            deref->a = ast_ident(it_name, line);
            AstNode *var = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            var->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            var->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.ival), line);
            var->a = deref;

            AstNode *body = ast_new(AST_BLOCK, line);
            body->list = ast_list_new();
            ast_list_append(&body->list, var);
            ast_list_append(&body->list, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));

            ((*yyvalp).node) = ast_new(AST_FOR, line);
            ((*yyvalp).node)->a = it_decl; ((*yyvalp).node)->b = cond; ((*yyvalp).node)->c = step; ((*yyvalp).node)->d = body;
        }
#line 5512 "src/parser.c"
    break;

  case 207: /* stmt: for_open for_init ';' comma_expr_opt ';' comma_expr_opt ')' stmt  */
#line 2654 "src/parser.y"
        {
            /* Own scope so a loop-local `int i` in for_init doesn't leak
             * into the enclosing block/function (and so a second, later
             * `for (int i ...)` in the same function doesn't collide with
             * it in the symbol table). */
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_FOR, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->d = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node) != NULL && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node)->kind == AST_VAR_DECL_GROUP) {
                /* `for (int i = 0, j = 5; ...)`: the declarations move
                 * into a block wrapped around the loop -- the same
                 * scope they had (nothing outside the loop sees them),
                 * and plain C in both output dialects. */
                AstNode *wrapper = ast_new(AST_BLOCK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
                wrapper->list = ast_list_new();
                ast_list_append_flatten(&wrapper->list, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node));
                ((*yyvalp).node)->a = NULL;
                ast_list_append(&wrapper->list, ((*yyvalp).node));
                ((*yyvalp).node) = wrapper;
            }
        }
#line 5538 "src/parser.c"
    break;

  case 208: /* stmt: STATIC var_decl ';'  */
#line 2676 "src/parser.y"
        {
            /* A static LOCAL: one object for the whole program, visible
             * only in this block. Marked here and moved to file scope
             * under a unique name by hoist_static_locals (ast.c), right
             * after the parse. File-scope `static` never reaches the
             * grammar (the lexer drops it there -- see lexer.l). */
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            if (((*yyvalp).node)->kind == AST_VAR_DECL_GROUP) {
                for (int i = 0; i < ((*yyvalp).node)->list.count; i++) ((*yyvalp).node)->list.items[i]->is_static_local = 1;
            } else {
                ((*yyvalp).node)->is_static_local = 1;
            }
        }
#line 5556 "src/parser.c"
    break;

  case 209: /* stmt: RETURN comma_expr_opt ';'  */
#line 2690 "src/parser.y"
        {
            /* comma_expr_opt, not expr_opt: `return a += 1, a + b;` is
             * the comma operator, which lower.c's phase 10 turns into
             * statements for Vircon32 like any other. */
            ((*yyvalp).node) = ast_new(AST_RETURN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 5568 "src/parser.c"
    break;

  case 210: /* stmt: BREAK ';'  */
#line 2698 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_BREAK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 5574 "src/parser.c"
    break;

  case 211: /* stmt: CONTINUE ';'  */
#line 2700 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_CONTINUE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 5580 "src/parser.c"
    break;

  case 212: /* stmt: GOTO IDENTIFIER ';'  */
#line 2702 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_GOTO, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str)); }
#line 5586 "src/parser.c"
    break;

  case 213: /* stmt: IDENTIFIER ':' stmt  */
#line 2704 "src/parser.y"
        {
            /* Labeled statement -- `label: stmt`. The one genuinely
             * higher-risk grammar addition of this round, worth being
             * direct about: a bare IDENTIFIER at the START of a
             * statement is ALSO how an ordinary expression-statement
             * begins (`expr ';'` below, where expr reduces from
             * IDENTIFIER through primary_expr -- `foo();`, `foo = 5;`,
             * ...), so the parser cannot know which production it's
             * building until it sees whether ':' or something else
             * follows the identifier. This project's own %glr-parser
             * declaration exists precisely for this kind of situation
             * (see this file's own header comment on it) -- GLR
             * defers the choice, exploring both readings until the
             * next token resolves it, rather than requiring one token
             * of lookahead to be enough the way plain LALR(1) would.
             * This is not a novel problem: real C's own yacc/bison
             * grammars have successfully modeled labeled-statement vs.
             * expression-statement this exact way for decades. Still,
             * this is a case actually worth Matthew's own attention on
             * regeneration -- more likely than most of this project's
             * other recent grammar additions to shift the %expect
             * count by more than one, or, in the worst case, to
             * surface a genuine reduce/reduce conflict GLR can't
             * resolve on its own -- flagged here and in
             * docs/DESIGN_NOTES.md, not discovered by surprise. */
            ((*yyvalp).node) = ast_new(AST_LABEL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5620 "src/parser.c"
    break;

  case 214: /* stmt: ASM '{' asm_string_list '}'  */
#line 2734 "src/parser.y"
        {
            /* Vircon32 C's own native form -- pure pass-through. */
            ((*yyvalp).node) = ast_new(AST_ASM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 0;  /* written in brace form */
        }
#line 5631 "src/parser.c"
    break;

  case 215: /* stmt: ASM '(' asm_string_list ')' ';'  */
#line 2741 "src/parser.y"
        {
            /* GCC/Clang basic asm. No operands allowed (basic asm has
             * none); codegen re-emits this as Vircon32 brace form when
             * targeting Vircon32, or keeps the parenthesized spelling
             * in --target=standard. An extended asm (with ':'
             * constraint sections) does NOT parse here -- the ':' after
             * the string list is a syntax error at this rule; see the
             * targeted diagnostic note below. */
            ((*yyvalp).node) = ast_new(AST_ASM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 1;  /* written in GCC parenthesized form */
        }
#line 5648 "src/parser.c"
    break;

  case 216: /* stmt: VOLATILE ASM '(' asm_string_list ')' ';'  */
#line 2754 "src/parser.y"
        {
            /* `asm volatile("...")` -- the qualifier only governs
             * optimization/reordering, which this transpiler performs
             * neither of, so it is accepted and dropped. */
            ((*yyvalp).node) = ast_new(AST_ASM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 1;
        }
#line 5661 "src/parser.c"
    break;

  case 217: /* stmt: ASM VOLATILE '(' asm_string_list ')' ';'  */
#line 2763 "src/parser.y"
        {
            /* __volatile__ spelled after the keyword (GCC documents
             * both orders historically; harmless to accept). */
            ((*yyvalp).node) = ast_new(AST_ASM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 1;
        }
#line 5673 "src/parser.c"
    break;

  case 218: /* stmt: ASM '(' asm_string_list ':'  */
#line 2771 "src/parser.y"
        {
            yyerror("extended asm with operand constraints is not "
                    "supported: Vircon32 C uses '{param}' interpolation "
                    "inside the literal instead; write the operands "
                    "directly in the instruction text");
            YYERROR;
        }
#line 5685 "src/parser.c"
    break;

  case 219: /* stmt: var_decl ';'  */
#line 2778 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 5691 "src/parser.c"
    break;

  case 220: /* stmt: EXTERN var_decl ';'  */
#line 2780 "src/parser.y"
        {
            /* `extern int seed;` inside a function names a file-scope
             * variable; it declares nothing here. (The lexer only returns
             * EXTERN inside a function body.) */
            ((*yyvalp).node) = decl_group_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 5702 "src/parser.c"
    break;

  case 221: /* stmt: EXTERN func_header ';'  */
#line 2787 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 5711 "src/parser.c"
    break;

  case 222: /* $@11: %empty  */
#line 2791 "src/parser.y"
                                           { symtab_push_scope(g_symtab, NULL, 0); }
#line 5717 "src/parser.c"
    break;

  case 223: /* stmt: type_spec pointer_opt IDENTIFIER '(' $@11 opt_param_list ')' ';'  */
#line 2792 "src/parser.y"
        {
            /* a function declared inside a function: just a prototype */
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = make_func_header(apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), 0, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
        }
#line 5727 "src/parser.c"
    break;

  case 224: /* stmt: typedef_decl ';'  */
#line 2797 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 5733 "src/parser.c"
    break;

  case 225: /* stmt: comma_expr ';'  */
#line 2799 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 5742 "src/parser.c"
    break;

  case 226: /* stmt: ';'  */
#line 2804 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 5750 "src/parser.c"
    break;

  case 227: /* for_open: FOR '('  */
#line 2812 "src/parser.y"
            { symtab_push_scope(g_symtab, NULL, 0); }
#line 5756 "src/parser.c"
    break;

  case 228: /* for_init: %empty  */
#line 2816 "src/parser.y"
                   { ((*yyvalp).node) = NULL; }
#line 5762 "src/parser.c"
    break;

  case 229: /* for_init: var_decl  */
#line 2817 "src/parser.y"
                    {
            /* A multi-declarator group (`int i = 0, j = 5`) is passed up
             * as-is: the FOR rule above wraps the loop in a block that
             * holds the declarations. */
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5773 "src/parser.c"
    break;

  case 230: /* for_init: comma_expr  */
#line 2824 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5782 "src/parser.c"
    break;

  case 231: /* comma_expr: expr  */
#line 2839 "src/parser.y"
                               { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5788 "src/parser.c"
    break;

  case 232: /* comma_expr: comma_expr ',' expr  */
#line 2841 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup(",");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5799 "src/parser.c"
    break;

  case 233: /* comma_expr_opt: %empty  */
#line 2850 "src/parser.y"
                       { ((*yyvalp).node) = NULL; }
#line 5805 "src/parser.c"
    break;

  case 234: /* comma_expr_opt: comma_expr  */
#line 2851 "src/parser.y"
                       { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5811 "src/parser.c"
    break;

  case 235: /* switch_body: %empty  */
#line 2867 "src/parser.y"
                                     { ((*yyvalp).list) = ast_list_new(); }
#line 5817 "src/parser.c"
    break;

  case 236: /* switch_body: switch_body CASE expr ':'  */
#line 2869 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            AstNode *c = ast_new(AST_CASE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            c->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ast_list_append(&((*yyvalp).list), c);
        }
#line 5828 "src/parser.c"
    break;

  case 237: /* switch_body: switch_body DEFAULT ':'  */
#line 2876 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            AstNode *d = ast_new(AST_DEFAULT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ast_list_append(&((*yyvalp).list), d);
        }
#line 5838 "src/parser.c"
    break;

  case 238: /* switch_body: switch_body stmt  */
#line 2882 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5844 "src/parser.c"
    break;

  case 239: /* primary_expr: IDENTIFIER  */
#line 2899 "src/parser.y"
                        { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 5850 "src/parser.c"
    break;

  case 240: /* primary_expr: INT_LITERAL  */
#line 2900 "src/parser.y"
                          { ((*yyvalp).node) = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).ival; ((*yyvalp).node)->macro_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).macro; }
#line 5856 "src/parser.c"
    break;

  case 241: /* primary_expr: FLOAT_LITERAL  */
#line 2901 "src/parser.y"
                           { ((*yyvalp).node) = ast_new(AST_FLOAT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->fval = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).fval; ((*yyvalp).node)->macro_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).macro; }
#line 5862 "src/parser.c"
    break;

  case 242: /* primary_expr: string_seq  */
#line 2902 "src/parser.y"
                            { ((*yyvalp).node) = ast_new(AST_STRING_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 5868 "src/parser.c"
    break;

  case 243: /* primary_expr: CHAR_LITERAL  */
#line 2903 "src/parser.y"
                              { ((*yyvalp).node) = ast_new(AST_CHAR_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).ival; ((*yyvalp).node)->macro_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).macro; }
#line 5874 "src/parser.c"
    break;

  case 244: /* primary_expr: VA_ARG '(' expr ',' type_spec pointer_opt ')'  */
#line 2905 "src/parser.y"
        {
            /* va_arg(ap, T): the next extra argument. A va_list is a
             * pointer into an array of words, one per argument (see the
             * ELLIPSIS parameter), so this is *((T *)((ap += 1) - 1)). */
            int line = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line;
            AstNode *one = ast_new(AST_INT_LIT, line);
            one->ival = 1;
            AstNode *step = ast_new(AST_ASSIGN, line);
            step->str1 = strdup("+=");
            step->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            step->b = one;
            AstNode *one2 = ast_new(AST_INT_LIT, line);
            one2->ival = 1;
            AstNode *back = ast_new(AST_BINOP, line);
            back->str1 = strdup("-");
            back->a = step;
            back->b = one2;
            AstNode *cast = ast_new(AST_CAST, line);
            cast->type = ast_wrap_pointer(apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), line), line);
            cast->a = back;
            ((*yyvalp).node) = ast_new(AST_UNOP, line);
            ((*yyvalp).node)->str1 = strdup("deref");
            ((*yyvalp).node)->a = cast;
        }
#line 5903 "src/parser.c"
    break;

  case 245: /* primary_expr: TRUE_KW  */
#line 2929 "src/parser.y"
                               { ((*yyvalp).node) = ast_new(AST_BOOL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = 1; }
#line 5909 "src/parser.c"
    break;

  case 246: /* primary_expr: FALSE_KW  */
#line 2930 "src/parser.y"
                                { ((*yyvalp).node) = ast_new(AST_BOOL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = 0; }
#line 5915 "src/parser.c"
    break;

  case 247: /* primary_expr: NULLPTR_KW  */
#line 2931 "src/parser.y"
                                 { ((*yyvalp).node) = ast_new(AST_NULL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 5921 "src/parser.c"
    break;

  case 248: /* primary_expr: THIS  */
#line 2932 "src/parser.y"
                                  { ((*yyvalp).node) = ast_new(AST_THIS, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 5927 "src/parser.c"
    break;

  case 249: /* primary_expr: qualified_id_expr  */
#line 2933 "src/parser.y"
                                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5933 "src/parser.c"
    break;

  case 250: /* primary_expr: '(' comma_expr ')'  */
#line 2934 "src/parser.y"
                                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 5939 "src/parser.c"
    break;

  case 251: /* primary_expr: TYPE_NAME '(' opt_arg_list ')'  */
#line 2936 "src/parser.y"
        {
            /* `Enemy(1, 2, 3)` as an EXPRESSION -- an unnamed object
             * (`v.push_back(Enemy(1, 2, 3));`, `return Vec(x, y);`) or,
             * for a typedef/enum name, a function-style cast
             * (`Fixed(3)`). Parsed into a marker node (AST_DIRECT_INIT
             * with str1 = the type's name) that never reaches sema:
             * desugar_unnamed_objects (ast.c) turns each one into an
             * ordinary named local, declared just ahead of the
             * statement that uses it, or into an AST_CAST. */
            ((*yyvalp).node) = ast_new(AST_DIRECT_INIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 5957 "src/parser.c"
    break;

  case 252: /* postfix_expr: primary_expr  */
#line 2952 "src/parser.y"
                                            { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5963 "src/parser.c"
    break;

  case 253: /* postfix_expr: postfix_expr '(' opt_arg_list ')'  */
#line 2954 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_CALL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            if (g_c_mode && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->kind == AST_IDENT && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list).count >= 1 &&
                (strcmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->str1, "va_start") == 0 || strcmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->str1, "va_end") == 0)) {
                /* va_start(ap, last): ap = the pointer `...` arrived as.
                 * va_end(ap): ap = 0. */
                int start = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->str1[3] == 's');
                ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                ((*yyvalp).node)->str1 = strdup("=");
                ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list).items[0];
                if (start) {
                    ((*yyvalp).node)->b = ast_ident("__v32_va", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                } else {
                    ((*yyvalp).node)->b = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                    ((*yyvalp).node)->b->ival = 0;
                }
            }
        }
#line 5988 "src/parser.c"
    break;

  case 254: /* postfix_expr: postfix_expr '.' IDENTIFIER  */
#line 2975 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_MEMBER, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup(".");
            ((*yyvalp).node)->str2 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 5999 "src/parser.c"
    break;

  case 255: /* postfix_expr: postfix_expr ARROW IDENTIFIER  */
#line 2982 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_MEMBER, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("->");
            ((*yyvalp).node)->str2 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 6010 "src/parser.c"
    break;

  case 256: /* postfix_expr: postfix_expr '[' expr ']'  */
#line 2989 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_SUBSCRIPT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 6020 "src/parser.c"
    break;

  case 257: /* postfix_expr: postfix_expr INC  */
#line 2995 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("post++");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 6030 "src/parser.c"
    break;

  case 258: /* postfix_expr: postfix_expr DEC  */
#line 3001 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("post--");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 6040 "src/parser.c"
    break;

  case 259: /* unary_expr: postfix_expr  */
#line 3009 "src/parser.y"
                             { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6046 "src/parser.c"
    break;

  case 260: /* unary_expr: '!' unary_expr  */
#line 3011 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("!"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6052 "src/parser.c"
    break;

  case 261: /* unary_expr: '~' unary_expr  */
#line 3013 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("~"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6058 "src/parser.c"
    break;

  case 262: /* unary_expr: '-' unary_expr  */
#line 3015 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("neg"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6064 "src/parser.c"
    break;

  case 263: /* unary_expr: '&' unary_expr  */
#line 3017 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("addr"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6070 "src/parser.c"
    break;

  case 264: /* unary_expr: '*' unary_expr  */
#line 3019 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("deref"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6076 "src/parser.c"
    break;

  case 265: /* unary_expr: INC unary_expr  */
#line 3021 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("pre++"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6082 "src/parser.c"
    break;

  case 266: /* unary_expr: DEC unary_expr  */
#line 3023 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("pre--"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6088 "src/parser.c"
    break;

  case 267: /* unary_expr: NEW type_spec  */
#line 3025 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_NEW, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6094 "src/parser.c"
    break;

  case 268: /* unary_expr: NEW type_spec '(' opt_arg_list ')'  */
#line 3027 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_NEW, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line); ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); }
#line 6100 "src/parser.c"
    break;

  case 269: /* unary_expr: NEW type_spec '[' expr ']'  */
#line 3029 "src/parser.y"
        {
            /* new T[N] -- heap-allocated array. N is any runtime
             * expression (not restricted to a compile-time constant the
             * way a stack array's own declared length is), held
             * directly as an expression node in `a`. Constructor
             * arguments alongside an array size (`new T[N](args)`)
             * aren't accepted by this grammar at all -- see ast.h's own
             * doc comment on AST_NEW for why, and lower.c/codegen.c for
             * how this lowers (allocation only, no per-element
             * construction yet). */
            ((*yyvalp).node) = ast_new(AST_NEW, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 6119 "src/parser.c"
    break;

  case 270: /* unary_expr: DELETE unary_expr  */
#line 3044 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_DELETE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6125 "src/parser.c"
    break;

  case 271: /* unary_expr: DELETE '[' ']' unary_expr  */
#line 3046 "src/parser.y"
        {
            /* delete[] ptr -- see ast.h's own doc comment on AST_DELETE
             * for why this currently lowers identically to plain
             * `delete` (no per-element destructor invocation exists for
             * either new[] or delete[] yet); the distinction is
             * recorded (ival=1) but not yet acted on anywhere. */
            ((*yyvalp).node) = ast_new(AST_DELETE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->ival = 1;
        }
#line 6140 "src/parser.c"
    break;

  case 272: /* unary_expr: '(' type_spec pointer_opt '(' '*' ')' '(' opt_func_ptr_param_list ')' ')' unary_expr  */
#line 3057 "src/parser.y"
        {
            /* a cast to a function pointer: `(void (*)(int))handler` */
            ((*yyvalp).node) = ast_new(AST_CAST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 6151 "src/parser.c"
    break;

  case 273: /* unary_expr: '(' type_spec pointer_opt ')' unary_expr  */
#line 3064 "src/parser.y"
        {
            /* C-style cast -- (Type)expr, (Type *)expr, (Type &)expr.
             * AST_CAST already existed (lower.c has been synthesizing
             * one internally for a while -- implicit-upcast insertion,
             * receiver casts -- see ast.h's own doc comment on it,
             * updated here now that user-written source can produce
             * one too, not just lowering).
             *
             * No genuine ambiguity with primary_expr's own
             * "'(' expr ')'" parenthesized-expression alternative,
             * confirmed directly from primary_expr's own grammar
             * before writing this, not assumed: primary_expr only ever
             * accepts a bare IDENTIFIER as an expression-starting
             * token, never TYPE_NAME -- so a TYPE_NAME (or a built-in
             * type keyword: INT_KW/FLOAT_KW/VOID_KW/BOOL_KW/CHAR_KW, all
             * of which start type_spec and nothing in expr's own
             * first-set) immediately after '(' can ONLY mean a cast is
             * starting here, never the start of a plain parenthesized
             * expression. */
            ((*yyvalp).node) = ast_new(AST_CAST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 6179 "src/parser.c"
    break;

  case 274: /* unary_expr: SIZEOF '(' type_spec pointer_opt ')'  */
#line 3088 "src/parser.y"
        {
            /* sizeof(Type) -- the type-taking form. Same disambiguation
             * reasoning as the C-style cast just above for WHETHER this
             * form or the expression form applies (a TYPE_NAME, or
             * built-in type keyword, immediately after SIZEOF's own
             * '(' can only mean this form, never the expression form
             * below, since type_spec's own first-set never overlaps
             * with expr's) -- but a SEPARATE, genuine ambiguity exists
             * even once that much is settled, and needs its own fix,
             * below.
             *
             * The %prec SIZEOF_TYPE_PREC is that fix, for a real bug
             * caught by bison itself on regeneration, not cosmetic:
             * without it, `sizeof(int) - 5` -- extremely ordinary code
             * -- would by default parse as `sizeof((int)(-5))` instead
             * of the obviously-intended `(sizeof(int)) - 5`. The root
             * cause: this rule's own closing ')' is ALSO exactly where
             * unary_expr's own C-style-cast production
             * ("'(' type_spec pointer_opt ')' unary_expr", just above)
             * could instead keep going, treating the same
             * "'(' type_spec pointer_opt ')'" prefix as the START of a
             * cast rather than a complete, standalone sizeof argument
             * -- so a lookahead token that could begin a new
             * unary_expr ('-', '&', '*', ...) creates a real shift/
             * reduce conflict: shift, and keep building toward
             * "sizeof applied to a cast-expression"; or reduce this
             * rule now, treating sizeof(Type) as already complete and
             * whatever follows as a separate, subsequent operator.
             * Real C++ always takes the second reading once the
             * parenthesized content is unambiguously a type -- sizeof's
             * own type-form terminates at its closing paren, full
             * stop, never continuing into a cast -- so REDUCE is the
             * only correct choice here, not merely bison's own
             * default. SIZEOF_TYPE_PREC is declared as the single
             * highest-precedence level in this grammar specifically so
             * this rule's own reduction always wins against any of
             * those lookahead tokens, whichever binary/unary operator
             * token it turns out to be -- deliberately a dedicated,
             * virtual token with no lexer rule ever returning it
             * (bison only needs it declared via the %nonassoc line
             * near the top of this file to have a precedence to
             * reference here), the same pattern LOWER_THAN_ELSE
             * already uses for the dangling-else problem elsewhere in
             * this grammar. */
            ((*yyvalp).node) = ast_new(AST_SIZEOF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 6231 "src/parser.c"
    break;

  case 275: /* unary_expr: SIZEOF unary_expr  */
#line 3136 "src/parser.y"
        {
            /* sizeof expr / sizeof(expr) -- the expression-taking form.
             * No separate parenthesized alternative needed here:
             * `sizeof(x)` where x is an ordinary expression already
             * reaches this same production, since unary_expr's own
             * reduction through primary_expr already covers
             * "'(' expr ')'" -- the parens aren't sizeof's own syntax
             * in that case, they're just an ordinary parenthesized
             * expression being sized, same as they'd be anywhere else. */
            ((*yyvalp).node) = ast_new(AST_SIZEOF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 6248 "src/parser.c"
    break;

  case 276: /* unary_expr: cpp_cast_kw '<' type_spec pointer_opt '>' '(' expr ')'  */
#line 3149 "src/parser.y"
        {
            /* C++-style cast -- static_cast<T>(x), const_cast<T>(x),
             * reinterpret_cast<T>(x), dynamic_cast<T>(x). All four
             * build the exact same AST_CAST node the C-style cast above
             * does -- see AST_CAST's own doc comment in ast.h for why
             * real C++'s distinctions between them collapse to nothing
             * once the target is C, which has no notion of any of
             * these cast KINDS, only a single generic cast syntax.
             * dynamic_cast is marked (ival=1) so sema.c's own
             * check_node can warn about the one real, substantive gap
             * this collapsing introduces: no actual RTTI-backed runtime
             * check happens, unlike what real dynamic_cast promises.
             *
             * The '<' '>' here are the same tokens comparison operators
             * already use, not new ones -- no ambiguity in practice,
             * since they only ever appear in THIS shape immediately
             * after one of the four cast keywords, a grammar position
             * comparison never occurs in. This is NOT template syntax
             * and doesn't open the door to one -- it's a fixed, four-
             * keyword special form, not a general
             * "identifier < args >" production the way an actual
             * template instantiation would need. */
            ((*yyvalp).node) = ast_new(AST_CAST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = apply_ptr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->ival = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.ival) == 3) ? 1 : 0; /* 1 only for dynamic_cast */
        }
#line 6280 "src/parser.c"
    break;

  case 277: /* cpp_cast_kw: STATIC_CAST  */
#line 3187 "src/parser.y"
                         { ((*yyvalp).ival) = 0; }
#line 6286 "src/parser.c"
    break;

  case 278: /* cpp_cast_kw: CONST_CAST  */
#line 3188 "src/parser.y"
                          { ((*yyvalp).ival) = 1; }
#line 6292 "src/parser.c"
    break;

  case 279: /* cpp_cast_kw: REINTERPRET_CAST  */
#line 3189 "src/parser.y"
                           { ((*yyvalp).ival) = 2; }
#line 6298 "src/parser.c"
    break;

  case 280: /* cpp_cast_kw: DYNAMIC_CAST  */
#line 3190 "src/parser.y"
                            { ((*yyvalp).ival) = 3; }
#line 6304 "src/parser.c"
    break;

  case 281: /* expr: unary_expr  */
#line 3194 "src/parser.y"
                           { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6310 "src/parser.c"
    break;

  case 282: /* expr: expr '*' expr  */
#line 3195 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("*"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6316 "src/parser.c"
    break;

  case 283: /* expr: expr '/' expr  */
#line 3196 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("/"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6322 "src/parser.c"
    break;

  case 284: /* expr: expr '%' expr  */
#line 3197 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("%"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6328 "src/parser.c"
    break;

  case 285: /* expr: expr '+' expr  */
#line 3198 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("+"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6334 "src/parser.c"
    break;

  case 286: /* expr: expr '-' expr  */
#line 3199 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("-"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6340 "src/parser.c"
    break;

  case 287: /* expr: expr '<' expr  */
#line 3200 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6346 "src/parser.c"
    break;

  case 288: /* expr: expr '>' expr  */
#line 3201 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6352 "src/parser.c"
    break;

  case 289: /* expr: expr LE expr  */
#line 3202 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6358 "src/parser.c"
    break;

  case 290: /* expr: expr GE expr  */
#line 3203 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6364 "src/parser.c"
    break;

  case 291: /* expr: expr EQ expr  */
#line 3204 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("=="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6370 "src/parser.c"
    break;

  case 292: /* expr: expr NE expr  */
#line 3205 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("!="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6376 "src/parser.c"
    break;

  case 293: /* expr: expr ANDAND expr  */
#line 3206 "src/parser.y"
                        { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("&&"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6382 "src/parser.c"
    break;

  case 294: /* expr: expr OROR expr  */
#line 3207 "src/parser.y"
                        { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("||"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6388 "src/parser.c"
    break;

  case 295: /* expr: expr '&' expr  */
#line 3209 "src/parser.y"
        {
            /* Binary bitwise-AND -- coexists with unary_expr's own
             * "'&' unary_expr" (address-of) the exact same way binary
             * '-' already coexists with unary_expr's own "'-' unary_expr"
             * (negation): the two never conflict, since unary_expr is a
             * different grammar POSITION (a prefix, at the start of an
             * operand) than this rule's own infix use (between two
             * already-reduced expr's) -- the same proven disambiguation
             * this grammar already relies on, not a new risk. */
            ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("&"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 6404 "src/parser.c"
    break;

  case 296: /* expr: expr '|' expr  */
#line 3220 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("|"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6410 "src/parser.c"
    break;

  case 297: /* expr: expr '^' expr  */
#line 3221 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("^"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6416 "src/parser.c"
    break;

  case 298: /* expr: expr SHL expr  */
#line 3222 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<<"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6422 "src/parser.c"
    break;

  case 299: /* expr: expr SHR expr  */
#line 3223 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">>"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6428 "src/parser.c"
    break;

  case 300: /* expr: expr '=' expr  */
#line 3225 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6434 "src/parser.c"
    break;

  case 301: /* expr: expr PLUSEQ expr  */
#line 3227 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("+="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6440 "src/parser.c"
    break;

  case 302: /* expr: expr MINUSEQ expr  */
#line 3229 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("-="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6446 "src/parser.c"
    break;

  case 303: /* expr: expr STAREQ expr  */
#line 3231 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("*="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6452 "src/parser.c"
    break;

  case 304: /* expr: expr SLASHEQ expr  */
#line 3233 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("/="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6458 "src/parser.c"
    break;

  case 305: /* expr: expr ANDEQ expr  */
#line 3235 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("&="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6464 "src/parser.c"
    break;

  case 306: /* expr: expr OREQ expr  */
#line 3237 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("|="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6470 "src/parser.c"
    break;

  case 307: /* expr: expr MODEQ expr  */
#line 3239 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("%="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6476 "src/parser.c"
    break;

  case 308: /* expr: expr XOREQ expr  */
#line 3241 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("^="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6482 "src/parser.c"
    break;

  case 309: /* expr: expr SHLEQ expr  */
#line 3243 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<<="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6488 "src/parser.c"
    break;

  case 310: /* expr: expr SHREQ expr  */
#line 3245 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">>="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 6494 "src/parser.c"
    break;

  case 311: /* expr: expr '?' expr ':' expr  */
#line 3247 "src/parser.y"
        {
            /* Ternary/conditional expression -- cond ? true : false.
             * Explicit %prec '?' overrides bison's own default (the
             * LAST terminal in the rule, which would otherwise be
             * ':') -- deliberately: ':' is reused across many other,
             * unrelated grammar contexts in this file (switch/case
             * labels, access specifiers, base-class lists, member-init
             * lists) with no precedence of its own declared anywhere,
             * so leaning on ITS precedence here would be both
             * meaningless (it has none) and risk entangling this
             * production with all those other, unrelated ones. '?'
             * appears NOWHERE else in this grammar, so giving it its
             * own dedicated precedence level (see the new %right '?'
             * declaration above, sitting between assignment and '||',
             * matching real C++'s own conditional-expression placement)
             * and pointing this rule at it explicitly is both correct
             * and fully self-contained. %right makes chaining
             * right-associative, matching real C++: `a ? b : c ? d : e`
             * parses as `a ? b : (c ? d : e)`. */
            ((*yyvalp).node) = ast_new(AST_TERNARY, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 6523 "src/parser.c"
    break;

  case 312: /* opt_arg_list: %empty  */
#line 3274 "src/parser.y"
                   { ((*yyvalp).list) = ast_list_new(); }
#line 6529 "src/parser.c"
    break;

  case 313: /* opt_arg_list: arg_list  */
#line 3275 "src/parser.y"
                    { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 6535 "src/parser.c"
    break;

  case 314: /* arg_list: expr  */
#line 3279 "src/parser.y"
                            { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 6541 "src/parser.c"
    break;

  case 315: /* arg_list: arg_list ',' expr  */
#line 3280 "src/parser.y"
                             { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 6547 "src/parser.c"
    break;

  case 316: /* string_seq: STRING_LITERAL  */
#line 3291 "src/parser.y"
                                    { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 6553 "src/parser.c"
    break;

  case 317: /* string_seq: string_seq STRING_LITERAL  */
#line 3292 "src/parser.y"
                                    { ((*yyvalp).str) = join_string_literals((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str)); }
#line 6559 "src/parser.c"
    break;

  case 318: /* asm_string_list: STRING_LITERAL  */
#line 3304 "src/parser.y"
        {
            ((*yyvalp).list) = ast_list_new();
            AstNode *lit = ast_new(AST_STRING_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            lit->str1 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str);
            ast_list_append(&((*yyvalp).list), lit);
        }
#line 6570 "src/parser.c"
    break;

  case 319: /* asm_string_list: asm_string_list STRING_LITERAL  */
#line 3311 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            AstNode *lit = ast_new(AST_STRING_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            lit->str1 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str);
            ast_list_append(&((*yyvalp).list), lit);
        }
#line 6581 "src/parser.c"
    break;

  case 320: /* opt_member_init_list: %empty  */
#line 3336 "src/parser.y"
                                     { ((*yyvalp).node) = NULL; }
#line 6587 "src/parser.c"
    break;

  case 321: /* opt_member_init_list: ':' member_init_list  */
#line 3337 "src/parser.y"
                                      { ((*yyvalp).node) = ast_new(AST_MEMBER_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 6593 "src/parser.c"
    break;

  case 322: /* member_init_list: member_init  */
#line 3341 "src/parser.y"
                                           { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 6599 "src/parser.c"
    break;

  case 323: /* member_init_list: member_init_list ',' member_init  */
#line 3342 "src/parser.y"
                                            { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 6605 "src/parser.c"
    break;

  case 324: /* member_init: IDENTIFIER '(' opt_arg_list ')'  */
#line 3347 "src/parser.y"
        {
            /* An ordinary member field's own name -- see this section's
             * own header comment above for why this is accepted
             * syntactically despite not being acted on yet. */
            ((*yyvalp).node) = ast_new(AST_MEMBER_INIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 6618 "src/parser.c"
    break;

  case 325: /* member_init: TYPE_NAME '(' opt_arg_list ')'  */
#line 3356 "src/parser.y"
        {
            /* A registered class name -- the base-class-delegation case
             * this round actually implements. Whether $1 is genuinely
             * THIS constructor's own direct base (as opposed to some
             * other, unrelated class name that merely happens to be
             * registered) is sema.c's job, not the parser's -- same
             * division of labor as everywhere else in this grammar. */
            ((*yyvalp).node) = ast_new(AST_MEMBER_INIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 6634 "src/parser.c"
    break;


#line 6638 "src/parser.c"

      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yylhsNonterm (yyrule), yyvalp, yylocp);

  return yyok;
# undef yyerrok
# undef YYABORT
# undef YYACCEPT
# undef YYNOMEM
# undef YYERROR
# undef YYBACKUP
# undef yyclearin
# undef YYRECOVERING
}


static void
yyuserMerge (int yyn, YYSTYPE* yy0, YYSTYPE* yy1)
{
  YY_USE (yy0);
  YY_USE (yy1);

  switch (yyn)
    {

      default: break;
    }
}

                              /* Bison grammar-table manipulation.  */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/** Number of symbols composing the right hand side of rule #RULE.  */
static inline int
yyrhsLength (yyRuleNum yyrule)
{
  return yyr2[yyrule];
}

static void
yydestroyGLRState (char const *yymsg, yyGLRState *yys)
{
  if (yys->yyresolved)
    yydestruct (yymsg, yy_accessing_symbol (yys->yylrState),
                &yys->yysemantics.yyval, &yys->yyloc);
  else
    {
#if YYDEBUG
      if (yydebug)
        {
          if (yys->yysemantics.yyfirstVal)
            YY_FPRINTF ((stderr, "%s unresolved", yymsg));
          else
            YY_FPRINTF ((stderr, "%s incomplete", yymsg));
          YY_SYMBOL_PRINT ("", yy_accessing_symbol (yys->yylrState), YY_NULLPTR, &yys->yyloc);
        }
#endif

      if (yys->yysemantics.yyfirstVal)
        {
          yySemanticOption *yyoption = yys->yysemantics.yyfirstVal;
          yyGLRState *yyrh;
          int yyn;
          for (yyrh = yyoption->yystate, yyn = yyrhsLength (yyoption->yyrule);
               yyn > 0;
               yyrh = yyrh->yypred, yyn -= 1)
            yydestroyGLRState (yymsg, yyrh);
        }
    }
}

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

/** True iff LR state YYSTATE has only a default reduction (regardless
 *  of token).  */
static inline yybool
yyisDefaultedState (yy_state_t yystate)
{
  return yypact_value_is_default (yypact[yystate]);
}

/** The default reduction for YYSTATE, assuming it has one.  */
static inline yyRuleNum
yydefaultAction (yy_state_t yystate)
{
  return yydefact[yystate];
}

#define yytable_value_is_error(Yyn) \
  0

/** The action to take in YYSTATE on seeing YYTOKEN.
 *  Result R means
 *    R < 0:  Reduce on rule -R.
 *    R = 0:  Error.
 *    R > 0:  Shift to state R.
 *  Set *YYCONFLICTS to a pointer into yyconfl to a 0-terminated list
 *  of conflicting reductions.
 */
static inline int
yygetLRActions (yy_state_t yystate, yysymbol_kind_t yytoken, const short** yyconflicts)
{
  int yyindex = yypact[yystate] + yytoken;
  if (yytoken == YYSYMBOL_YYerror)
    {
      // This is the error token.
      *yyconflicts = yyconfl;
      return 0;
    }
  else if (yyisDefaultedState (yystate)
           || yyindex < 0 || YYLAST < yyindex || yycheck[yyindex] != yytoken)
    {
      *yyconflicts = yyconfl;
      return -yydefact[yystate];
    }
  else if (! yytable_value_is_error (yytable[yyindex]))
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return yytable[yyindex];
    }
  else
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return 0;
    }
}

/** Compute post-reduction state.
 * \param yystate   the current state
 * \param yysym     the nonterminal to push on the stack
 */
static inline yy_state_t
yyLRgotoState (yy_state_t yystate, yysymbol_kind_t yysym)
{
  int yyr = yypgoto[yysym - YYNTOKENS] + yystate;
  if (0 <= yyr && yyr <= YYLAST && yycheck[yyr] == yystate)
    return yytable[yyr];
  else
    return yydefgoto[yysym - YYNTOKENS];
}

static inline yybool
yyisShiftAction (int yyaction)
{
  return 0 < yyaction;
}

static inline yybool
yyisErrorAction (int yyaction)
{
  return yyaction == 0;
}

                                /* GLRStates */

/** Return a fresh GLRStackItem in YYSTACKP.  The item is an LR state
 *  if YYISSTATE, and otherwise a semantic option.  Callers should call
 *  YY_RESERVE_GLRSTACK afterwards to make sure there is sufficient
 *  headroom.  */

static inline yyGLRStackItem*
yynewGLRStackItem (yyGLRStack* yystackp, yybool yyisState)
{
  yyGLRStackItem* yynewItem = yystackp->yynextFree;
  yystackp->yyspaceLeft -= 1;
  yystackp->yynextFree += 1;
  yynewItem->yystate.yyisState = yyisState;
  return yynewItem;
}

/** Add a new semantic action that will execute the action for rule
 *  YYRULE on the semantic values in YYRHS to the list of
 *  alternative actions for YYSTATE.  Assumes that YYRHS comes from
 *  stack #YYK of *YYSTACKP. */
static void
yyaddDeferredAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyGLRState* yystate,
                     yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yySemanticOption* yynewOption =
    &yynewGLRStackItem (yystackp, yyfalse)->yyoption;
  YY_ASSERT (!yynewOption->yyisState);
  yynewOption->yystate = yyrhs;
  yynewOption->yyrule = yyrule;
  if (yystackp->yytops.yylookaheadNeeds[yyk])
    {
      yynewOption->yyrawchar = yychar;
      yynewOption->yyval = yylval;
      yynewOption->yyloc = yylloc;
    }
  else
    yynewOption->yyrawchar = YYEMPTY;
  yynewOption->yynext = yystate->yysemantics.yyfirstVal;
  yystate->yysemantics.yyfirstVal = yynewOption;

  YY_RESERVE_GLRSTACK (yystackp);
}

                                /* GLRStacks */

/** Initialize YYSET to a singleton set containing an empty stack.  */
static yybool
yyinitStateSet (yyGLRStateSet* yyset)
{
  yyset->yysize = 1;
  yyset->yycapacity = 16;
  yyset->yystates
    = YY_CAST (yyGLRState**,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yystates[0]));
  if (! yyset->yystates)
    return yyfalse;
  yyset->yystates[0] = YY_NULLPTR;
  yyset->yylookaheadNeeds
    = YY_CAST (yybool*,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yylookaheadNeeds[0]));
  if (! yyset->yylookaheadNeeds)
    {
      YYFREE (yyset->yystates);
      return yyfalse;
    }
  memset (yyset->yylookaheadNeeds,
          0,
          YY_CAST (YYSIZE_T, yyset->yycapacity) * sizeof yyset->yylookaheadNeeds[0]);
  return yytrue;
}

static void yyfreeStateSet (yyGLRStateSet* yyset)
{
  YYFREE (yyset->yystates);
  YYFREE (yyset->yylookaheadNeeds);
}

/** Initialize *YYSTACKP to a single empty stack, with total maximum
 *  capacity for all stacks of YYSIZE.  */
static yybool
yyinitGLRStack (yyGLRStack* yystackp, YYPTRDIFF_T yysize)
{
  yystackp->yyerrState = 0;
  yynerrs = 0;
  yystackp->yyspaceLeft = yysize;
  yystackp->yyitems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yysize)
                         * sizeof yystackp->yynextFree[0]));
  if (!yystackp->yyitems)
    return yyfalse;
  yystackp->yynextFree = yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;
  return yyinitStateSet (&yystackp->yytops);
}


#if YYSTACKEXPANDABLE
# define YYRELOC(YYFROMITEMS, YYTOITEMS, YYX, YYTYPE)                   \
  &((YYTOITEMS)                                                         \
    - ((YYFROMITEMS) - YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX))))->YYTYPE

/** If *YYSTACKP is expandable, extend it.  WARNING: Pointers into the
    stack from outside should be considered invalid after this call.
    We always expand when there are 1 or fewer items left AFTER an
    allocation, so that we can avoid having external pointers exist
    across an allocation.  */
static void
yyexpandGLRStack (yyGLRStack* yystackp)
{
  yyGLRStackItem* yynewItems;
  yyGLRStackItem* yyp0, *yyp1;
  YYPTRDIFF_T yynewSize;
  YYPTRDIFF_T yyn;
  YYPTRDIFF_T yysize = yystackp->yynextFree - yystackp->yyitems;
  if (YYMAXDEPTH - YYHEADROOM < yysize)
    yyMemoryExhausted (yystackp);
  yynewSize = 2*yysize;
  if (YYMAXDEPTH < yynewSize)
    yynewSize = YYMAXDEPTH;
  yynewItems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yynewSize)
                         * sizeof yynewItems[0]));
  if (! yynewItems)
    yyMemoryExhausted (yystackp);
  for (yyp0 = yystackp->yyitems, yyp1 = yynewItems, yyn = yysize;
       0 < yyn;
       yyn -= 1, yyp0 += 1, yyp1 += 1)
    {
      *yyp1 = *yyp0;
      if (*YY_REINTERPRET_CAST (yybool *, yyp0))
        {
          yyGLRState* yys0 = &yyp0->yystate;
          yyGLRState* yys1 = &yyp1->yystate;
          if (yys0->yypred != YY_NULLPTR)
            yys1->yypred =
              YYRELOC (yyp0, yyp1, yys0->yypred, yystate);
          if (! yys0->yyresolved && yys0->yysemantics.yyfirstVal != YY_NULLPTR)
            yys1->yysemantics.yyfirstVal =
              YYRELOC (yyp0, yyp1, yys0->yysemantics.yyfirstVal, yyoption);
        }
      else
        {
          yySemanticOption* yyv0 = &yyp0->yyoption;
          yySemanticOption* yyv1 = &yyp1->yyoption;
          if (yyv0->yystate != YY_NULLPTR)
            yyv1->yystate = YYRELOC (yyp0, yyp1, yyv0->yystate, yystate);
          if (yyv0->yynext != YY_NULLPTR)
            yyv1->yynext = YYRELOC (yyp0, yyp1, yyv0->yynext, yyoption);
        }
    }
  if (yystackp->yysplitPoint != YY_NULLPTR)
    yystackp->yysplitPoint = YYRELOC (yystackp->yyitems, yynewItems,
                                      yystackp->yysplitPoint, yystate);

  for (yyn = 0; yyn < yystackp->yytops.yysize; yyn += 1)
    if (yystackp->yytops.yystates[yyn] != YY_NULLPTR)
      yystackp->yytops.yystates[yyn] =
        YYRELOC (yystackp->yyitems, yynewItems,
                 yystackp->yytops.yystates[yyn], yystate);
  YYFREE (yystackp->yyitems);
  yystackp->yyitems = yynewItems;
  yystackp->yynextFree = yynewItems + yysize;
  yystackp->yyspaceLeft = yynewSize - yysize;
}
#endif

static void
yyfreeGLRStack (yyGLRStack* yystackp)
{
  YYFREE (yystackp->yyitems);
  yyfreeStateSet (&yystackp->yytops);
}

/** Assuming that YYS is a GLRState somewhere on *YYSTACKP, update the
 *  splitpoint of *YYSTACKP, if needed, so that it is at least as deep as
 *  YYS.  */
static inline void
yyupdateSplit (yyGLRStack* yystackp, yyGLRState* yys)
{
  if (yystackp->yysplitPoint != YY_NULLPTR && yystackp->yysplitPoint > yys)
    yystackp->yysplitPoint = yys;
}

/** Invalidate stack #YYK in *YYSTACKP.  */
static inline void
yymarkStackDeleted (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    yystackp->yylastDeleted = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yystates[yyk] = YY_NULLPTR;
}

/** Undelete the last stack in *YYSTACKP that was marked as deleted.  Can
    only be done once after a deletion, and only when all other stacks have
    been deleted.  */
static void
yyundeleteLastStack (yyGLRStack* yystackp)
{
  if (yystackp->yylastDeleted == YY_NULLPTR || yystackp->yytops.yysize != 0)
    return;
  yystackp->yytops.yystates[0] = yystackp->yylastDeleted;
  yystackp->yytops.yysize = 1;
  YY_DPRINTF ((stderr, "Restoring last deleted stack as stack #0.\n"));
  yystackp->yylastDeleted = YY_NULLPTR;
}

static inline void
yyremoveDeletes (yyGLRStack* yystackp)
{
  YYPTRDIFF_T yyi, yyj;
  yyi = yyj = 0;
  while (yyj < yystackp->yytops.yysize)
    {
      if (yystackp->yytops.yystates[yyi] == YY_NULLPTR)
        {
          if (yyi == yyj)
            YY_DPRINTF ((stderr, "Removing dead stacks.\n"));
          yystackp->yytops.yysize -= 1;
        }
      else
        {
          yystackp->yytops.yystates[yyj] = yystackp->yytops.yystates[yyi];
          /* In the current implementation, it's unnecessary to copy
             yystackp->yytops.yylookaheadNeeds[yyi] since, after
             yyremoveDeletes returns, the parser immediately either enters
             deterministic operation or shifts a token.  However, it doesn't
             hurt, and the code might evolve to need it.  */
          yystackp->yytops.yylookaheadNeeds[yyj] =
            yystackp->yytops.yylookaheadNeeds[yyi];
          if (yyj != yyi)
            YY_DPRINTF ((stderr, "Rename stack %ld -> %ld.\n",
                        YY_CAST (long, yyi), YY_CAST (long, yyj)));
          yyj += 1;
        }
      yyi += 1;
    }
}

/** Shift to a new state on stack #YYK of *YYSTACKP, corresponding to LR
 * state YYLRSTATE, at input position YYPOSN, with (resolved) semantic
 * value *YYVALP and source location *YYLOCP.  */
static inline void
yyglrShift (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
            YYPTRDIFF_T yyposn,
            YYSTYPE* yyvalp, YYLTYPE* yylocp)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yytrue;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyval = *yyvalp;
  yynewState->yyloc = *yylocp;
  yystackp->yytops.yystates[yyk] = yynewState;

  YY_RESERVE_GLRSTACK (yystackp);
}

/** Shift stack #YYK of *YYSTACKP, to a new state corresponding to LR
 *  state YYLRSTATE, at input position YYPOSN, with the (unresolved)
 *  semantic value of YYRHS under the action for YYRULE.  */
static inline void
yyglrShiftDefer (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
                 YYPTRDIFF_T yyposn, yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;
  YY_ASSERT (yynewState->yyisState);

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yyfalse;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyfirstVal = YY_NULLPTR;
  yystackp->yytops.yystates[yyk] = yynewState;

  /* Invokes YY_RESERVE_GLRSTACK.  */
  yyaddDeferredAction (yystackp, yyk, yynewState, yyrhs, yyrule);
}

#if YYDEBUG

/*----------------------------------------------------------------------.
| Report that stack #YYK of *YYSTACKP is going to be reduced by YYRULE. |
`----------------------------------------------------------------------*/

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, YYPTRDIFF_T yyk,
                 yyRuleNum yyrule)
{
  int yynrhs = yyrhsLength (yyrule);
  int yylow = 1;
  int yyi;
  YY_FPRINTF ((stderr, "Reducing stack %ld by rule %d (line %d):\n",
               YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
  if (! yynormal)
    yyfillin (yyvsp, 1, -yynrhs);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YY_FPRINTF ((stderr, "   $%d = ", yyi + 1));
      yy_symbol_print (stderr,
                       yy_accessing_symbol (yyvsp[yyi - yynrhs + 1].yystate.yylrState),
                       &yyvsp[yyi - yynrhs + 1].yystate.yysemantics.yyval,
                       &(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL ((yyi + 1) - (yynrhs))].yystate.yyloc)                       );
      if (!yyvsp[yyi - yynrhs + 1].yystate.yyresolved)
        YY_FPRINTF ((stderr, " (unresolved)"));
      YY_FPRINTF ((stderr, "\n"));
    }
}
#endif

/** Pop the symbols consumed by reduction #YYRULE from the top of stack
 *  #YYK of *YYSTACKP, and perform the appropriate semantic action on their
 *  semantic values.  Assumes that all ambiguities in semantic values
 *  have been previously resolved.  Set *YYVALP to the resulting value,
 *  and *YYLOCP to the computed location (if any).  Return value is as
 *  for userAction.  */
static inline YYRESULTTAG
yydoAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
            YYSTYPE* yyvalp, YYLTYPE *yylocp)
{
  int yynrhs = yyrhsLength (yyrule);

  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      /* Standard special case: single stack.  */
      yyGLRStackItem* yyrhs
        = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yytops.yystates[yyk]);
      YY_ASSERT (yyk == 0);
      yystackp->yynextFree -= yynrhs;
      yystackp->yyspaceLeft += yynrhs;
      yystackp->yytops.yystates[0] = & yystackp->yynextFree[-1].yystate;
      return yyuserAction (yyrule, yynrhs, yyrhs, yystackp, yyk,
                           yyvalp, yylocp);
    }
  else
    {
      yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
      yyGLRState* yys = yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred
        = yystackp->yytops.yystates[yyk];
      int yyi;
      if (yynrhs == 0)
        /* Set default location.  */
        yyrhsVals[YYMAXRHS + YYMAXLEFT - 1].yystate.yyloc = yys->yyloc;
      for (yyi = 0; yyi < yynrhs; yyi += 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yystackp->yytops.yystates[yyk] = yys;
      return yyuserAction (yyrule, yynrhs, yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, yyk, yyvalp, yylocp);
    }
}

/** Pop items off stack #YYK of *YYSTACKP according to grammar rule YYRULE,
 *  and push back on the resulting nonterminal symbol.  Perform the
 *  semantic action associated with YYRULE and store its value with the
 *  newly pushed state, if YYFORCEEVAL or if *YYSTACKP is currently
 *  unambiguous.  Otherwise, store the deferred semantic action with
 *  the new state.  If the new state would have an identical input
 *  position, LR state, and predecessor to an existing state on the stack,
 *  it is identified with that existing state, eliminating stack #YYK from
 *  *YYSTACKP.  In this case, the semantic value is
 *  added to the options for the existing state's semantic value.
 */
static inline YYRESULTTAG
yyglrReduce (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
             yybool yyforceEval)
{
  YYPTRDIFF_T yyposn = yystackp->yytops.yystates[yyk]->yyposn;

  if (yyforceEval || yystackp->yysplitPoint == YY_NULLPTR)
    {
      YYSTYPE yyval;
      YYLTYPE yyloc;

      YYRESULTTAG yyflag = yydoAction (yystackp, yyk, yyrule, &yyval, &yyloc);
      if (yyflag == yyerr && yystackp->yysplitPoint != YY_NULLPTR)
        YY_DPRINTF ((stderr,
                     "Parse on stack %ld rejected by rule %d (line %d).\n",
                     YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
      if (yyflag != yyok)
        return yyflag;
      yyglrShift (yystackp, yyk,
                  yyLRgotoState (yystackp->yytops.yystates[yyk]->yylrState,
                                 yylhsNonterm (yyrule)),
                  yyposn, &yyval, &yyloc);
    }
  else
    {
      YYPTRDIFF_T yyi;
      int yyn;
      yyGLRState* yys, *yys0 = yystackp->yytops.yystates[yyk];
      yy_state_t yynewLRState;

      for (yys = yystackp->yytops.yystates[yyk], yyn = yyrhsLength (yyrule);
           0 < yyn; yyn -= 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yynewLRState = yyLRgotoState (yys->yylrState, yylhsNonterm (yyrule));
      YY_DPRINTF ((stderr,
                   "Reduced stack %ld by rule %d (line %d); action deferred.  "
                   "Now in state %d.\n",
                   YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule],
                   yynewLRState));
      for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
        if (yyi != yyk && yystackp->yytops.yystates[yyi] != YY_NULLPTR)
          {
            yyGLRState *yysplit = yystackp->yysplitPoint;
            yyGLRState *yyp = yystackp->yytops.yystates[yyi];
            while (yyp != yys && yyp != yysplit && yyp->yyposn >= yyposn)
              {
                if (yyp->yylrState == yynewLRState && yyp->yypred == yys)
                  {
                    yyaddDeferredAction (yystackp, yyk, yyp, yys0, yyrule);
                    yymarkStackDeleted (yystackp, yyk);
                    YY_DPRINTF ((stderr, "Merging stack %ld into stack %ld.\n",
                                 YY_CAST (long, yyk), YY_CAST (long, yyi)));
                    return yyok;
                  }
                yyp = yyp->yypred;
              }
          }
      yystackp->yytops.yystates[yyk] = yys;
      yyglrShiftDefer (yystackp, yyk, yynewLRState, yyposn, yys0, yyrule);
    }
  return yyok;
}

static YYPTRDIFF_T
yysplitStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      YY_ASSERT (yyk == 0);
      yystackp->yysplitPoint = yystackp->yytops.yystates[yyk];
    }
  if (yystackp->yytops.yycapacity <= yystackp->yytops.yysize)
    {
      YYPTRDIFF_T state_size = YYSIZEOF (yystackp->yytops.yystates[0]);
      YYPTRDIFF_T half_max_capacity = YYSIZE_MAXIMUM / 2 / state_size;
      if (half_max_capacity < yystackp->yytops.yycapacity)
        yyMemoryExhausted (yystackp);
      yystackp->yytops.yycapacity *= 2;

      {
        yyGLRState** yynewStates
          = YY_CAST (yyGLRState**,
                     YYREALLOC (yystackp->yytops.yystates,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewStates[0])));
        if (yynewStates == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yystates = yynewStates;
      }

      {
        yybool* yynewLookaheadNeeds
          = YY_CAST (yybool*,
                     YYREALLOC (yystackp->yytops.yylookaheadNeeds,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewLookaheadNeeds[0])));
        if (yynewLookaheadNeeds == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yylookaheadNeeds = yynewLookaheadNeeds;
      }
    }
  yystackp->yytops.yystates[yystackp->yytops.yysize]
    = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yylookaheadNeeds[yystackp->yytops.yysize]
    = yystackp->yytops.yylookaheadNeeds[yyk];
  yystackp->yytops.yysize += 1;
  return yystackp->yytops.yysize - 1;
}

/** True iff YYY0 and YYY1 represent identical options at the top level.
 *  That is, they represent the same rule applied to RHS symbols
 *  that produce the same terminal symbols.  */
static yybool
yyidenticalOptions (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  if (yyy0->yyrule == yyy1->yyrule)
    {
      yyGLRState *yys0, *yys1;
      int yyn;
      for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
           yyn = yyrhsLength (yyy0->yyrule);
           yyn > 0;
           yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
        if (yys0->yyposn != yys1->yyposn)
          return yyfalse;
      return yytrue;
    }
  else
    return yyfalse;
}

/** Assuming identicalOptions (YYY0,YYY1), destructively merge the
 *  alternative semantic values for the RHS-symbols of YYY1 and YYY0.  */
static void
yymergeOptionSets (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  yyGLRState *yys0, *yys1;
  int yyn;
  for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
       yyn = yyrhsLength (yyy0->yyrule);
       0 < yyn;
       yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
    {
      if (yys0 == yys1)
        break;
      else if (yys0->yyresolved)
        {
          yys1->yyresolved = yytrue;
          yys1->yysemantics.yyval = yys0->yysemantics.yyval;
        }
      else if (yys1->yyresolved)
        {
          yys0->yyresolved = yytrue;
          yys0->yysemantics.yyval = yys1->yysemantics.yyval;
        }
      else
        {
          yySemanticOption** yyz0p = &yys0->yysemantics.yyfirstVal;
          yySemanticOption* yyz1 = yys1->yysemantics.yyfirstVal;
          while (yytrue)
            {
              if (yyz1 == *yyz0p || yyz1 == YY_NULLPTR)
                break;
              else if (*yyz0p == YY_NULLPTR)
                {
                  *yyz0p = yyz1;
                  break;
                }
              else if (*yyz0p < yyz1)
                {
                  yySemanticOption* yyz = *yyz0p;
                  *yyz0p = yyz1;
                  yyz1 = yyz1->yynext;
                  (*yyz0p)->yynext = yyz;
                }
              yyz0p = &(*yyz0p)->yynext;
            }
          yys1->yysemantics.yyfirstVal = yys0->yysemantics.yyfirstVal;
        }
    }
}

/** Y0 and Y1 represent two possible actions to take in a given
 *  parsing state; return 0 if no combination is possible,
 *  1 if user-mergeable, 2 if Y0 is preferred, 3 if Y1 is preferred.  */
static int
yypreference (yySemanticOption* y0, yySemanticOption* y1)
{
  yyRuleNum r0 = y0->yyrule, r1 = y1->yyrule;
  int p0 = yydprec[r0], p1 = yydprec[r1];

  if (p0 == p1)
    {
      if (yymerger[r0] == 0 || yymerger[r0] != yymerger[r1])
        return 0;
      else
        return 1;
    }
  if (p0 == 0 || p1 == 0)
    return 0;
  if (p0 < p1)
    return 3;
  if (p1 < p0)
    return 2;
  return 0;
}

static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp);


/** Resolve the previous YYN states starting at and including state YYS
 *  on *YYSTACKP. If result != yyok, some states may have been left
 *  unresolved possibly with empty semantic option chains.  Regardless
 *  of whether result = yyok, each state has been left with consistent
 *  data so that yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveStates (yyGLRState* yys, int yyn,
                 yyGLRStack* yystackp)
{
  if (0 < yyn)
    {
      YY_ASSERT (yys->yypred);
      YYCHK (yyresolveStates (yys->yypred, yyn-1, yystackp));
      if (! yys->yyresolved)
        YYCHK (yyresolveValue (yys, yystackp));
    }
  return yyok;
}

/** Resolve the states for the RHS of YYOPT on *YYSTACKP, perform its
 *  user action, and return the semantic value and location in *YYVALP
 *  and *YYLOCP.  Regardless of whether result = yyok, all RHS states
 *  have been destroyed (assuming the user action destroys all RHS
 *  semantic values if invoked).  */
static YYRESULTTAG
yyresolveAction (yySemanticOption* yyopt, yyGLRStack* yystackp,
                 YYSTYPE* yyvalp, YYLTYPE *yylocp)
{
  yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
  int yynrhs = yyrhsLength (yyopt->yyrule);
  YYRESULTTAG yyflag =
    yyresolveStates (yyopt->yystate, yynrhs, yystackp);
  if (yyflag != yyok)
    {
      yyGLRState *yys;
      for (yys = yyopt->yystate; yynrhs > 0; yys = yys->yypred, yynrhs -= 1)
        yydestroyGLRState ("Cleanup: popping", yys);
      return yyflag;
    }

  yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred = yyopt->yystate;
  if (yynrhs == 0)
    /* Set default location.  */
    yyrhsVals[YYMAXRHS + YYMAXLEFT - 1].yystate.yyloc = yyopt->yystate->yyloc;
  {
    int yychar_current = yychar;
    YYSTYPE yylval_current = yylval;
    YYLTYPE yylloc_current = yylloc;
    yychar = yyopt->yyrawchar;
    yylval = yyopt->yyval;
    yylloc = yyopt->yyloc;
    yyflag = yyuserAction (yyopt->yyrule, yynrhs,
                           yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, -1, yyvalp, yylocp);
    yychar = yychar_current;
    yylval = yylval_current;
    yylloc = yylloc_current;
  }
  return yyflag;
}

#if YYDEBUG
static void
yyreportTree (yySemanticOption* yyx, int yyindent)
{
  int yynrhs = yyrhsLength (yyx->yyrule);
  int yyi;
  yyGLRState* yys;
  yyGLRState* yystates[1 + YYMAXRHS];
  yyGLRState yyleftmost_state;

  for (yyi = yynrhs, yys = yyx->yystate; 0 < yyi; yyi -= 1, yys = yys->yypred)
    yystates[yyi] = yys;
  if (yys == YY_NULLPTR)
    {
      yyleftmost_state.yyposn = 0;
      yystates[0] = &yyleftmost_state;
    }
  else
    yystates[0] = yys;

  if (yyx->yystate->yyposn < yys->yyposn + 1)
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, empty>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1));
  else
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, tokens %ld .. %ld>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1, YY_CAST (long, yys->yyposn + 1),
                 YY_CAST (long, yyx->yystate->yyposn)));
  for (yyi = 1; yyi <= yynrhs; yyi += 1)
    {
      if (yystates[yyi]->yyresolved)
        {
          if (yystates[yyi-1]->yyposn+1 > yystates[yyi]->yyposn)
            YY_FPRINTF ((stderr, "%*s%s <empty>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState))));
          else
            YY_FPRINTF ((stderr, "%*s%s <tokens %ld .. %ld>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState)),
                         YY_CAST (long, yystates[yyi-1]->yyposn + 1),
                         YY_CAST (long, yystates[yyi]->yyposn)));
        }
      else
        yyreportTree (yystates[yyi]->yysemantics.yyfirstVal, yyindent+2);
    }
}
#endif

static YYRESULTTAG
yyreportAmbiguity (yySemanticOption* yyx0,
                   yySemanticOption* yyx1)
{
  YY_USE (yyx0);
  YY_USE (yyx1);

#if YYDEBUG
  YY_FPRINTF ((stderr, "Ambiguity detected.\n"));
  YY_FPRINTF ((stderr, "Option 1,\n"));
  yyreportTree (yyx0, 2);
  YY_FPRINTF ((stderr, "\nOption 2,\n"));
  yyreportTree (yyx1, 2);
  YY_FPRINTF ((stderr, "\n"));
#endif

  yyerror (YY_("syntax is ambiguous"));
  return yyabort;
}

/** Resolve the locations for each of the YYN1 states in *YYSTACKP,
 *  ending at YYS1.  Has no effect on previously resolved states.
 *  The first semantic option of a state is always chosen.  */
static void
yyresolveLocations (yyGLRState *yys1, int yyn1,
                    yyGLRStack *yystackp)
{
  if (0 < yyn1)
    {
      yyresolveLocations (yys1->yypred, yyn1 - 1, yystackp);
      if (!yys1->yyresolved)
        {
          yyGLRStackItem yyrhsloc[1 + YYMAXRHS];
          int yynrhs;
          yySemanticOption *yyoption = yys1->yysemantics.yyfirstVal;
          YY_ASSERT (yyoption);
          yynrhs = yyrhsLength (yyoption->yyrule);
          if (0 < yynrhs)
            {
              yyGLRState *yys;
              int yyn;
              yyresolveLocations (yyoption->yystate, yynrhs,
                                  yystackp);
              for (yys = yyoption->yystate, yyn = yynrhs;
                   yyn > 0;
                   yys = yys->yypred, yyn -= 1)
                yyrhsloc[yyn].yystate.yyloc = yys->yyloc;
            }
          else
            {
              /* Both yyresolveAction and yyresolveLocations traverse the GSS
                 in reverse rightmost order.  It is only necessary to invoke
                 yyresolveLocations on a subforest for which yyresolveAction
                 would have been invoked next had an ambiguity not been
                 detected.  Thus the location of the previous state (but not
                 necessarily the previous state itself) is guaranteed to be
                 resolved already.  */
              yyGLRState *yyprevious = yyoption->yystate;
              yyrhsloc[0].yystate.yyloc = yyprevious->yyloc;
            }
          YYLLOC_DEFAULT ((yys1->yyloc), yyrhsloc, yynrhs);
        }
    }
}

/** Resolve the ambiguity represented in state YYS in *YYSTACKP,
 *  perform the indicated actions, and set the semantic value of YYS.
 *  If result != yyok, the chain of semantic options in YYS has been
 *  cleared instead or it has been left unmodified except that
 *  redundant options may have been removed.  Regardless of whether
 *  result = yyok, YYS has been left with consistent data so that
 *  yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp)
{
  yySemanticOption* yyoptionList = yys->yysemantics.yyfirstVal;
  yySemanticOption* yybest = yyoptionList;
  yySemanticOption** yypp;
  yybool yymerge = yyfalse;
  YYSTYPE yyval;
  YYRESULTTAG yyflag;
  YYLTYPE *yylocp = &yys->yyloc;

  for (yypp = &yyoptionList->yynext; *yypp != YY_NULLPTR; )
    {
      yySemanticOption* yyp = *yypp;

      if (yyidenticalOptions (yybest, yyp))
        {
          yymergeOptionSets (yybest, yyp);
          *yypp = yyp->yynext;
        }
      else
        {
          switch (yypreference (yybest, yyp))
            {
            case 0:
              yyresolveLocations (yys, 1, yystackp);
              return yyreportAmbiguity (yybest, yyp);
              break;
            case 1:
              yymerge = yytrue;
              break;
            case 2:
              break;
            case 3:
              yybest = yyp;
              yymerge = yyfalse;
              break;
            default:
              /* This cannot happen so it is not worth a YY_ASSERT (yyfalse),
                 but some compilers complain if the default case is
                 omitted.  */
              break;
            }
          yypp = &yyp->yynext;
        }
    }

  if (yymerge)
    {
      yySemanticOption* yyp;
      int yyprec = yydprec[yybest->yyrule];
      yyflag = yyresolveAction (yybest, yystackp, &yyval, yylocp);
      if (yyflag == yyok)
        for (yyp = yybest->yynext; yyp != YY_NULLPTR; yyp = yyp->yynext)
          {
            if (yyprec == yydprec[yyp->yyrule])
              {
                YYSTYPE yyval_other;
                YYLTYPE yydummy;
                yyflag = yyresolveAction (yyp, yystackp, &yyval_other, &yydummy);
                if (yyflag != yyok)
                  {
                    yydestruct ("Cleanup: discarding incompletely merged value for",
                                yy_accessing_symbol (yys->yylrState),
                                &yyval, yylocp);
                    break;
                  }
                yyuserMerge (yymerger[yyp->yyrule], &yyval, &yyval_other);
              }
          }
    }
  else
    yyflag = yyresolveAction (yybest, yystackp, &yyval, yylocp);

  if (yyflag == yyok)
    {
      yys->yyresolved = yytrue;
      yys->yysemantics.yyval = yyval;
    }
  else
    yys->yysemantics.yyfirstVal = YY_NULLPTR;
  return yyflag;
}

static YYRESULTTAG
yyresolveStack (yyGLRStack* yystackp)
{
  if (yystackp->yysplitPoint != YY_NULLPTR)
    {
      yyGLRState* yys;
      int yyn;

      for (yyn = 0, yys = yystackp->yytops.yystates[0];
           yys != yystackp->yysplitPoint;
           yys = yys->yypred, yyn += 1)
        continue;
      YYCHK (yyresolveStates (yystackp->yytops.yystates[0], yyn, yystackp
                             ));
    }
  return yyok;
}

/** Called when returning to deterministic operation to clean up the extra
 * stacks. */
static void
yycompressStack (yyGLRStack* yystackp)
{
  /* yyr is the state after the split point.  */
  yyGLRState *yyr;

  if (yystackp->yytops.yysize != 1 || yystackp->yysplitPoint == YY_NULLPTR)
    return;

  {
    yyGLRState *yyp, *yyq;
    for (yyp = yystackp->yytops.yystates[0], yyq = yyp->yypred, yyr = YY_NULLPTR;
         yyp != yystackp->yysplitPoint;
         yyr = yyp, yyp = yyq, yyq = yyp->yypred)
      yyp->yypred = yyr;
  }

  yystackp->yyspaceLeft += yystackp->yynextFree - yystackp->yyitems;
  yystackp->yynextFree = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yysplitPoint) + 1;
  yystackp->yyspaceLeft -= yystackp->yynextFree - yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;

  while (yyr != YY_NULLPTR)
    {
      yystackp->yynextFree->yystate = *yyr;
      yyr = yyr->yypred;
      yystackp->yynextFree->yystate.yypred = &yystackp->yynextFree[-1].yystate;
      yystackp->yytops.yystates[0] = &yystackp->yynextFree->yystate;
      yystackp->yynextFree += 1;
      yystackp->yyspaceLeft -= 1;
    }
}

static YYRESULTTAG
yyprocessOneStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk,
                   YYPTRDIFF_T yyposn)
{
  while (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    {
      yy_state_t yystate = yystackp->yytops.yystates[yyk]->yylrState;
      YY_DPRINTF ((stderr, "Stack %ld Entering state %d\n",
                   YY_CAST (long, yyk), yystate));

      YY_ASSERT (yystate != YYFINAL);

      if (yyisDefaultedState (yystate))
        {
          YYRESULTTAG yyflag;
          yyRuleNum yyrule = yydefaultAction (yystate);
          if (yyrule == 0)
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          yyflag = yyglrReduce (yystackp, yyk, yyrule, yyimmediate[yyrule]);
          if (yyflag == yyerr)
            {
              YY_DPRINTF ((stderr,
                           "Stack %ld dies "
                           "(predicate failure or explicit user error).\n",
                           YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          if (yyflag != yyok)
            return yyflag;
        }
      else
        {
          yysymbol_kind_t yytoken = yygetToken (&yychar);
          const short* yyconflicts;
          const int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
          yystackp->yytops.yylookaheadNeeds[yyk] = yytrue;

          for (/* nothing */; *yyconflicts; yyconflicts += 1)
            {
              YYRESULTTAG yyflag;
              YYPTRDIFF_T yynewStack = yysplitStack (yystackp, yyk);
              YY_DPRINTF ((stderr, "Splitting off stack %ld from %ld.\n",
                           YY_CAST (long, yynewStack), YY_CAST (long, yyk)));
              yyflag = yyglrReduce (yystackp, yynewStack,
                                    *yyconflicts,
                                    yyimmediate[*yyconflicts]);
              if (yyflag == yyok)
                YYCHK (yyprocessOneStack (yystackp, yynewStack,
                                          yyposn));
              else if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yynewStack)));
                  yymarkStackDeleted (yystackp, yynewStack);
                }
              else
                return yyflag;
            }

          if (yyisShiftAction (yyaction))
            break;
          else if (yyisErrorAction (yyaction))
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              break;
            }
          else
            {
              YYRESULTTAG yyflag = yyglrReduce (yystackp, yyk, -yyaction,
                                                yyimmediate[-yyaction]);
              if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr,
                               "Stack %ld dies "
                               "(predicate failure or explicit user error).\n",
                               YY_CAST (long, yyk)));
                  yymarkStackDeleted (yystackp, yyk);
                  break;
                }
              else if (yyflag != yyok)
                return yyflag;
            }
        }
    }
  return yyok;
}

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYSTACKP, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  */
static int
yypcontext_expected_tokens (const yyGLRStack* yystackp,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[yystackp->yytops.yystates[0]->yylrState];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}

static int
yy_syntax_error_arguments (const yyGLRStack* yystackp,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  yysymbol_kind_t yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yystackp,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}



static void
yyreportSyntaxError (yyGLRStack* yystackp)
{
  if (yystackp->yyerrState != 0)
    return;
  {
  yybool yysize_overflow = yyfalse;
  char* yymsg = YY_NULLPTR;
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount
    = yy_syntax_error_arguments (yystackp, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    yyMemoryExhausted (yystackp);

  switch (yycount)
    {
#define YYCASE_(N, S)                   \
      case N:                           \
        yyformat = S;                   \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysz
          = yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (YYSIZE_MAXIMUM - yysize < yysz)
          yysize_overflow = yytrue;
        else
          yysize += yysz;
      }
  }

  if (!yysize_overflow)
    yymsg = YY_CAST (char *, YYMALLOC (YY_CAST (YYSIZE_T, yysize)));

  if (yymsg)
    {
      char *yyp = yymsg;
      int yyi = 0;
      while ((*yyp = *yyformat))
        {
          if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
            {
              yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
              yyformat += 2;
            }
          else
            {
              ++yyp;
              ++yyformat;
            }
        }
      yyerror (yymsg);
      YYFREE (yymsg);
    }
  else
    {
      yyerror (YY_("syntax error"));
      yyMemoryExhausted (yystackp);
    }
  }
  yynerrs += 1;
}

/* Recover from a syntax error on *YYSTACKP, assuming that *YYSTACKP->YYTOKENP,
   yylval, and yylloc are the syntactic category, semantic value, and location
   of the lookahead.  */
static void
yyrecoverSyntaxError (yyGLRStack* yystackp)
{
  if (yystackp->yyerrState == 3)
    /* We just shifted the error token and (perhaps) took some
       reductions.  Skip tokens until we can proceed.  */
    while (yytrue)
      {
        yysymbol_kind_t yytoken;
        int yyj;
        if (yychar == YYEOF)
          yyFail (yystackp, YY_NULLPTR);
        if (yychar != YYEMPTY)
          {
            /* We throw away the lookahead, but the error range
               of the shifted error token must take it into account.  */
            yyGLRState *yys = yystackp->yytops.yystates[0];
            yyGLRStackItem yyerror_range[3];
            yyerror_range[1].yystate.yyloc = yys->yyloc;
            yyerror_range[2].yystate.yyloc = yylloc;
            YYLLOC_DEFAULT ((yys->yyloc), yyerror_range, 2);
            yytoken = YYTRANSLATE (yychar);
            yydestruct ("Error: discarding",
                        yytoken, &yylval, &yylloc);
            yychar = YYEMPTY;
          }
        yytoken = yygetToken (&yychar);
        yyj = yypact[yystackp->yytops.yystates[0]->yylrState];
        if (yypact_value_is_default (yyj))
          return;
        yyj += yytoken;
        if (yyj < 0 || YYLAST < yyj || yycheck[yyj] != yytoken)
          {
            if (yydefact[yystackp->yytops.yystates[0]->yylrState] != 0)
              return;
          }
        else if (! yytable_value_is_error (yytable[yyj]))
          return;
      }

  /* Reduce to one stack.  */
  {
    YYPTRDIFF_T yyk;
    for (yyk = 0; yyk < yystackp->yytops.yysize; yyk += 1)
      if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
        break;
    if (yyk >= yystackp->yytops.yysize)
      yyFail (yystackp, YY_NULLPTR);
    for (yyk += 1; yyk < yystackp->yytops.yysize; yyk += 1)
      yymarkStackDeleted (yystackp, yyk);
    yyremoveDeletes (yystackp);
    yycompressStack (yystackp);
  }

  /* Pop stack until we find a state that shifts the error token.  */
  yystackp->yyerrState = 3;
  while (yystackp->yytops.yystates[0] != YY_NULLPTR)
    {
      yyGLRState *yys = yystackp->yytops.yystates[0];
      int yyj = yypact[yys->yylrState];
      if (! yypact_value_is_default (yyj))
        {
          yyj += YYSYMBOL_YYerror;
          if (0 <= yyj && yyj <= YYLAST && yycheck[yyj] == YYSYMBOL_YYerror
              && yyisShiftAction (yytable[yyj]))
            {
              /* Shift the error token.  */
              int yyaction = yytable[yyj];
              /* First adjust its location.*/
              YYLTYPE yyerrloc;
              yystackp->yyerror_range[2].yystate.yyloc = yylloc;
              YYLLOC_DEFAULT (yyerrloc, (yystackp->yyerror_range), 2);
              YY_SYMBOL_PRINT ("Shifting", yy_accessing_symbol (yyaction),
                               &yylval, &yyerrloc);
              yyglrShift (yystackp, 0, yyaction,
                          yys->yyposn, &yylval, &yyerrloc);
              yys = yystackp->yytops.yystates[0];
              break;
            }
        }
      yystackp->yyerror_range[1].yystate.yyloc = yys->yyloc;
      if (yys->yypred != YY_NULLPTR)
        yydestroyGLRState ("Error: popping", yys);
      yystackp->yytops.yystates[0] = yys->yypred;
      yystackp->yynextFree -= 1;
      yystackp->yyspaceLeft += 1;
    }
  if (yystackp->yytops.yystates[0] == YY_NULLPTR)
    yyFail (yystackp, YY_NULLPTR);
}

#define YYCHK1(YYE)                             \
  do {                                          \
    switch (YYE) {                              \
    case yyok:     break;                       \
    case yyabort:  goto yyabortlab;             \
    case yyaccept: goto yyacceptlab;            \
    case yyerr:    goto yyuser_error;           \
    case yynomem:  goto yyexhaustedlab;         \
    default:       goto yybuglab;               \
    }                                           \
  } while (0)

/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
  int yyresult;
  yyGLRStack yystack;
  yyGLRStack* const yystackp = &yystack;
  YYPTRDIFF_T yyposn;

  YY_DPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY;
  yylval = yyval_default;
  yylloc = yyloc_default;

  if (! yyinitGLRStack (yystackp, YYINITDEPTH))
    goto yyexhaustedlab;
  switch (YYSETJMP (yystack.yyexception_buffer))
    {
    case 0: break;
    case 1: goto yyabortlab;
    case 2: goto yyexhaustedlab;
    default: goto yybuglab;
    }
  yyglrShift (&yystack, 0, 0, 0, &yylval, &yylloc);
  yyposn = 0;

  while (yytrue)
    {
      /* For efficiency, we have two loops, the first of which is
         specialized to deterministic operation (single stack, no
         potential ambiguity).  */
      /* Standard mode. */
      while (yytrue)
        {
          yy_state_t yystate = yystack.yytops.yystates[0]->yylrState;
          YY_DPRINTF ((stderr, "Entering state %d\n", yystate));
          if (yystate == YYFINAL)
            goto yyacceptlab;
          if (yyisDefaultedState (yystate))
            {
              yyRuleNum yyrule = yydefaultAction (yystate);
              if (yyrule == 0)
                {
                  yystack.yyerror_range[1].yystate.yyloc = yylloc;
                  yyreportSyntaxError (&yystack);
                  goto yyuser_error;
                }
              YYCHK1 (yyglrReduce (&yystack, 0, yyrule, yytrue));
            }
          else
            {
              yysymbol_kind_t yytoken = yygetToken (&yychar);
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
              if (*yyconflicts)
                /* Enter nondeterministic mode.  */
                break;
              if (yyisShiftAction (yyaction))
                {
                  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
                  yychar = YYEMPTY;
                  yyposn += 1;
                  yyglrShift (&yystack, 0, yyaction, yyposn, &yylval, &yylloc);
                  if (0 < yystack.yyerrState)
                    yystack.yyerrState -= 1;
                }
              else if (yyisErrorAction (yyaction))
                {
                  yystack.yyerror_range[1].yystate.yyloc = yylloc;
                  /* Issue an error message unless the scanner already
                     did. */
                  if (yychar != YYerror)
                    yyreportSyntaxError (&yystack);
                  goto yyuser_error;
                }
              else
                YYCHK1 (yyglrReduce (&yystack, 0, -yyaction, yytrue));
            }
        }

      /* Nondeterministic mode. */
      while (yytrue)
        {
          yysymbol_kind_t yytoken_to_shift;
          YYPTRDIFF_T yys;

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            yystackp->yytops.yylookaheadNeeds[yys] = yychar != YYEMPTY;

          /* yyprocessOneStack returns one of three things:

              - An error flag.  If the caller is yyprocessOneStack, it
                immediately returns as well.  When the caller is finally
                yyparse, it jumps to an error label via YYCHK1.

              - yyok, but yyprocessOneStack has invoked yymarkStackDeleted
                (&yystack, yys), which sets the top state of yys to NULL.  Thus,
                yyparse's following invocation of yyremoveDeletes will remove
                the stack.

              - yyok, when ready to shift a token.

             Except in the first case, yyparse will invoke yyremoveDeletes and
             then shift the next token onto all remaining stacks.  This
             synchronization of the shift (that is, after all preceding
             reductions on all stacks) helps prevent double destructor calls
             on yylval in the event of memory exhaustion.  */

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            YYCHK1 (yyprocessOneStack (&yystack, yys, yyposn));
          yyremoveDeletes (&yystack);
          if (yystack.yytops.yysize == 0)
            {
              yyundeleteLastStack (&yystack);
              if (yystack.yytops.yysize == 0)
                yyFail (&yystack, YY_("syntax error"));
              YYCHK1 (yyresolveStack (&yystack));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yystack.yyerror_range[1].yystate.yyloc = yylloc;
              yyreportSyntaxError (&yystack);
              goto yyuser_error;
            }

          /* If any yyglrShift call fails, it will fail after shifting.  Thus,
             a copy of yylval will already be on stack 0 in the event of a
             failure in the following loop.  Thus, yychar is set to YYEMPTY
             before the loop to make sure the user destructor for yylval isn't
             called twice.  */
          yytoken_to_shift = YYTRANSLATE (yychar);
          yychar = YYEMPTY;
          yyposn += 1;
          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            {
              yy_state_t yystate = yystack.yytops.yystates[yys]->yylrState;
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken_to_shift,
                              &yyconflicts);
              /* Note that yyconflicts were handled by yyprocessOneStack.  */
              YY_DPRINTF ((stderr, "On stack %ld, ", YY_CAST (long, yys)));
              YY_SYMBOL_PRINT ("shifting", yytoken_to_shift, &yylval, &yylloc);
              yyglrShift (&yystack, yys, yyaction, yyposn,
                          &yylval, &yylloc);
              YY_DPRINTF ((stderr, "Stack %ld now in state %d\n",
                           YY_CAST (long, yys),
                           yystack.yytops.yystates[yys]->yylrState));
            }

          if (yystack.yytops.yysize == 1)
            {
              YYCHK1 (yyresolveStack (&yystack));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yycompressStack (&yystack);
              break;
            }
        }
      continue;
    yyuser_error:
      yyrecoverSyntaxError (&yystack);
      yyposn = yystack.yytops.yystates[0]->yyposn;
    }

 yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;

 yybuglab:
  YY_ASSERT (yyfalse);
  goto yyabortlab;

 yyabortlab:
  yyresult = 1;
  goto yyreturnlab;

 yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;

 yyreturnlab:
  if (yychar != YYEMPTY)
    yydestruct ("Cleanup: discarding lookahead",
                YYTRANSLATE (yychar), &yylval, &yylloc);

  /* If the stack is well-formed, pop the stack until it is empty,
     destroying its entries as we go.  But free the stack regardless
     of whether it is well-formed.  */
  if (yystack.yyitems)
    {
      yyGLRState** yystates = yystack.yytops.yystates;
      if (yystates)
        {
          YYPTRDIFF_T yysize = yystack.yytops.yysize;
          YYPTRDIFF_T yyk;
          for (yyk = 0; yyk < yysize; yyk += 1)
            if (yystates[yyk])
              {
                while (yystates[yyk])
                  {
                    yyGLRState *yys = yystates[yyk];
                    yystack.yyerror_range[1].yystate.yyloc = yys->yyloc;
                    if (yys->yypred != YY_NULLPTR)
                      yydestroyGLRState ("Cleanup: popping", yys);
                    yystates[yyk] = yys->yypred;
                    yystack.yynextFree -= 1;
                    yystack.yyspaceLeft += 1;
                  }
                break;
              }
        }
      yyfreeGLRStack (&yystack);
    }

  return yyresult;
}

/* DEBUGGING ONLY */
#if YYDEBUG
/* Print *YYS and its predecessors. */
static void
yy_yypstack (yyGLRState* yys)
{
  if (yys->yypred)
    {
      yy_yypstack (yys->yypred);
      YY_FPRINTF ((stderr, " -> "));
    }
  YY_FPRINTF ((stderr, "%d@%ld", yys->yylrState, YY_CAST (long, yys->yyposn)));
}

/* Print YYS (possibly NULL) and its predecessors. */
static void
yypstates (yyGLRState* yys)
{
  if (yys == YY_NULLPTR)
    YY_FPRINTF ((stderr, "<null>"));
  else
    yy_yypstack (yys);
  YY_FPRINTF ((stderr, "\n"));
}

/* Print the stack #YYK.  */
static void
yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  yypstates (yystackp->yytops.yystates[yyk]);
}

/* Print all the stacks.  */
static void
yypdumpstack (yyGLRStack* yystackp)
{
#define YYINDEX(YYX)                                                    \
  YY_CAST (long,                                                        \
           ((YYX)                                                       \
            ? YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX)) - yystackp->yyitems \
            : -1))

  yyGLRStackItem* yyp;
  for (yyp = yystackp->yyitems; yyp < yystackp->yynextFree; yyp += 1)
    {
      YY_FPRINTF ((stderr, "%3ld. ",
                   YY_CAST (long, yyp - yystackp->yyitems)));
      if (*YY_REINTERPRET_CAST (yybool *, yyp))
        {
          YY_ASSERT (yyp->yystate.yyisState);
          YY_ASSERT (yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Res: %d, LR State: %d, posn: %ld, pred: %ld",
                       yyp->yystate.yyresolved, yyp->yystate.yylrState,
                       YY_CAST (long, yyp->yystate.yyposn),
                       YYINDEX (yyp->yystate.yypred)));
          if (! yyp->yystate.yyresolved)
            YY_FPRINTF ((stderr, ", firstVal: %ld",
                         YYINDEX (yyp->yystate.yysemantics.yyfirstVal)));
        }
      else
        {
          YY_ASSERT (!yyp->yystate.yyisState);
          YY_ASSERT (!yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Option. rule: %d, state: %ld, next: %ld",
                       yyp->yyoption.yyrule - 1,
                       YYINDEX (yyp->yyoption.yystate),
                       YYINDEX (yyp->yyoption.yynext)));
        }
      YY_FPRINTF ((stderr, "\n"));
    }

  YY_FPRINTF ((stderr, "Tops:"));
  {
    YYPTRDIFF_T yyi;
    for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
      YY_FPRINTF ((stderr, "%ld: %ld; ", YY_CAST (long, yyi),
                   YYINDEX (yystackp->yytops.yystates[yyi])));
    YY_FPRINTF ((stderr, "\n"));
  }
#undef YYINDEX
}
#endif

#undef yylval
#undef yychar
#undef yynerrs
#undef yylloc




#line 3369 "src/parser.y"


void yyerror(const char *msg) {
    fprintf(stderr, "%s:%d: error: %s\n", g_current_filename, g_lex_lineno, msg);
}
