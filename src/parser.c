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
static AstNode *decl_group_new(int line) {
    return ast_new(AST_VAR_DECL_GROUP, line);
}

/* Make `name` lex as TYPE_NAME from here on, unless it already does. */
static void declare_type_name(const char *name, SymbolKind kind) {
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
        AstNode *t = ast_ident(decl->str1, line);
        AstNode *td = ast_new(AST_TYPEDEF_DECL, line);
        td->str1 = strdup(name);
        td->type = (ptr == 1) ? ast_wrap_pointer(t, line)
                 : (ptr == 2) ? ast_wrap_reference(t, line)
                 : t;
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
        AstNode *t = (spec->ival == 1) ? ast_wrap_pointer(base, line)
                   : (spec->ival == 2) ? ast_wrap_reference(base, line)
                   : base;
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

#line 288 "src/parser.c"

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
  YYSYMBOL_STRING_LITERAL = 5,             /* STRING_LITERAL  */
  YYSYMBOL_INT_LITERAL = 6,                /* INT_LITERAL  */
  YYSYMBOL_CHAR_LITERAL = 7,               /* CHAR_LITERAL  */
  YYSYMBOL_FLOAT_LITERAL = 8,              /* FLOAT_LITERAL  */
  YYSYMBOL_CLASS = 9,                      /* CLASS  */
  YYSYMBOL_STRUCT = 10,                    /* STRUCT  */
  YYSYMBOL_ENUM = 11,                      /* ENUM  */
  YYSYMBOL_UNION = 12,                     /* UNION  */
  YYSYMBOL_PUBLIC = 13,                    /* PUBLIC  */
  YYSYMBOL_PRIVATE = 14,                   /* PRIVATE  */
  YYSYMBOL_PROTECTED = 15,                 /* PROTECTED  */
  YYSYMBOL_NAMESPACE = 16,                 /* NAMESPACE  */
  YYSYMBOL_TYPEDEF = 17,                   /* TYPEDEF  */
  YYSYMBOL_RETURN = 18,                    /* RETURN  */
  YYSYMBOL_IF = 19,                        /* IF  */
  YYSYMBOL_ELSE = 20,                      /* ELSE  */
  YYSYMBOL_DO = 21,                        /* DO  */
  YYSYMBOL_WHILE = 22,                     /* WHILE  */
  YYSYMBOL_FOR = 23,                       /* FOR  */
  YYSYMBOL_BREAK = 24,                     /* BREAK  */
  YYSYMBOL_CONTINUE = 25,                  /* CONTINUE  */
  YYSYMBOL_GOTO = 26,                      /* GOTO  */
  YYSYMBOL_SWITCH = 27,                    /* SWITCH  */
  YYSYMBOL_CASE = 28,                      /* CASE  */
  YYSYMBOL_DEFAULT = 29,                   /* DEFAULT  */
  YYSYMBOL_INT_KW = 30,                    /* INT_KW  */
  YYSYMBOL_FLOAT_KW = 31,                  /* FLOAT_KW  */
  YYSYMBOL_VOID_KW = 32,                   /* VOID_KW  */
  YYSYMBOL_BOOL_KW = 33,                   /* BOOL_KW  */
  YYSYMBOL_CHAR_KW = 34,                   /* CHAR_KW  */
  YYSYMBOL_NEW = 35,                       /* NEW  */
  YYSYMBOL_DELETE = 36,                    /* DELETE  */
  YYSYMBOL_THIS = 37,                      /* THIS  */
  YYSYMBOL_VIRTUAL = 38,                   /* VIRTUAL  */
  YYSYMBOL_TRUE_KW = 39,                   /* TRUE_KW  */
  YYSYMBOL_FALSE_KW = 40,                  /* FALSE_KW  */
  YYSYMBOL_NULLPTR_KW = 41,                /* NULLPTR_KW  */
  YYSYMBOL_OPERATOR = 42,                  /* OPERATOR  */
  YYSYMBOL_SIZEOF = 43,                    /* SIZEOF  */
  YYSYMBOL_CONST = 44,                     /* CONST  */
  YYSYMBOL_FRIEND = 45,                    /* FRIEND  */
  YYSYMBOL_STATIC_CAST = 46,               /* STATIC_CAST  */
  YYSYMBOL_DYNAMIC_CAST = 47,              /* DYNAMIC_CAST  */
  YYSYMBOL_CONST_CAST = 48,                /* CONST_CAST  */
  YYSYMBOL_REINTERPRET_CAST = 49,          /* REINTERPRET_CAST  */
  YYSYMBOL_COLONCOLON = 50,                /* COLONCOLON  */
  YYSYMBOL_ARROW = 51,                     /* ARROW  */
  YYSYMBOL_EQ = 52,                        /* EQ  */
  YYSYMBOL_NE = 53,                        /* NE  */
  YYSYMBOL_LE = 54,                        /* LE  */
  YYSYMBOL_GE = 55,                        /* GE  */
  YYSYMBOL_ANDAND = 56,                    /* ANDAND  */
  YYSYMBOL_OROR = 57,                      /* OROR  */
  YYSYMBOL_PLUSEQ = 58,                    /* PLUSEQ  */
  YYSYMBOL_MINUSEQ = 59,                   /* MINUSEQ  */
  YYSYMBOL_STAREQ = 60,                    /* STAREQ  */
  YYSYMBOL_SLASHEQ = 61,                   /* SLASHEQ  */
  YYSYMBOL_INC = 62,                       /* INC  */
  YYSYMBOL_DEC = 63,                       /* DEC  */
  YYSYMBOL_SHL = 64,                       /* SHL  */
  YYSYMBOL_SHR = 65,                       /* SHR  */
  YYSYMBOL_ANDEQ = 66,                     /* ANDEQ  */
  YYSYMBOL_OREQ = 67,                      /* OREQ  */
  YYSYMBOL_XOREQ = 68,                     /* XOREQ  */
  YYSYMBOL_SHLEQ = 69,                     /* SHLEQ  */
  YYSYMBOL_SHREQ = 70,                     /* SHREQ  */
  YYSYMBOL_ASM = 71,                       /* ASM  */
  YYSYMBOL_VOLATILE = 72,                  /* VOLATILE  */
  YYSYMBOL_NATIVE = 73,                    /* NATIVE  */
  YYSYMBOL_MODEQ = 74,                     /* MODEQ  */
  YYSYMBOL_STATIC = 75,                    /* STATIC  */
  YYSYMBOL_76_ = 76,                       /* '='  */
  YYSYMBOL_77_ = 77,                       /* '?'  */
  YYSYMBOL_78_ = 78,                       /* '|'  */
  YYSYMBOL_79_ = 79,                       /* '^'  */
  YYSYMBOL_80_ = 80,                       /* '&'  */
  YYSYMBOL_81_ = 81,                       /* '<'  */
  YYSYMBOL_82_ = 82,                       /* '>'  */
  YYSYMBOL_83_ = 83,                       /* '+'  */
  YYSYMBOL_84_ = 84,                       /* '-'  */
  YYSYMBOL_85_ = 85,                       /* '*'  */
  YYSYMBOL_86_ = 86,                       /* '/'  */
  YYSYMBOL_87_ = 87,                       /* '%'  */
  YYSYMBOL_SIZEOF_TYPE_PREC = 88,          /* SIZEOF_TYPE_PREC  */
  YYSYMBOL_LOWER_THAN_ELSE = 89,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_90_ = 90,                       /* ';'  */
  YYSYMBOL_91_ = 91,                       /* '{'  */
  YYSYMBOL_92_ = 92,                       /* '}'  */
  YYSYMBOL_93_ = 93,                       /* ':'  */
  YYSYMBOL_94_ = 94,                       /* '!'  */
  YYSYMBOL_95_ = 95,                       /* '['  */
  YYSYMBOL_96_ = 96,                       /* ']'  */
  YYSYMBOL_97_ = 97,                       /* '('  */
  YYSYMBOL_98_ = 98,                       /* ')'  */
  YYSYMBOL_99_ = 99,                       /* '~'  */
  YYSYMBOL_100_ = 100,                     /* ','  */
  YYSYMBOL_101_ = 101,                     /* '.'  */
  YYSYMBOL_YYACCEPT = 102,                 /* $accept  */
  YYSYMBOL_program = 103,                  /* program  */
  YYSYMBOL_top_decl_list = 104,            /* top_decl_list  */
  YYSYMBOL_top_decl = 105,                 /* top_decl  */
  YYSYMBOL_native_decl = 106,              /* native_decl  */
  YYSYMBOL_namespace_decl = 107,           /* namespace_decl  */
  YYSYMBOL_108_1 = 108,                    /* $@1  */
  YYSYMBOL_class_decl = 109,               /* class_decl  */
  YYSYMBOL_110_2 = 110,                    /* $@2  */
  YYSYMBOL_class_body = 111,               /* class_body  */
  YYSYMBOL_class_or_struct_kw = 112,       /* class_or_struct_kw  */
  YYSYMBOL_opt_base = 113,                 /* opt_base  */
  YYSYMBOL_member_list = 114,              /* member_list  */
  YYSYMBOL_member = 115,                   /* member  */
  YYSYMBOL_access_spec = 116,              /* access_spec  */
  YYSYMBOL_opt_virtual = 117,              /* opt_virtual  */
  YYSYMBOL_opt_const = 118,                /* opt_const  */
  YYSYMBOL_func_name = 119,                /* func_name  */
  YYSYMBOL_operator_symbol = 120,          /* operator_symbol  */
  YYSYMBOL_func_header = 121,              /* func_header  */
  YYSYMBOL_122_3 = 122,                    /* $@3  */
  YYSYMBOL_123_4 = 123,                    /* $@4  */
  YYSYMBOL_func_decl = 124,                /* func_decl  */
  YYSYMBOL_func_def = 125,                 /* func_def  */
  YYSYMBOL_out_of_line_def = 126,          /* out_of_line_def  */
  YYSYMBOL_127_5 = 127,                    /* $@5  */
  YYSYMBOL_128_6 = 128,                    /* $@6  */
  YYSYMBOL_opt_param_list = 129,           /* opt_param_list  */
  YYSYMBOL_param_list = 130,               /* param_list  */
  YYSYMBOL_param = 131,                    /* param  */
  YYSYMBOL_pointer_opt = 132,              /* pointer_opt  */
  YYSYMBOL_type_spec = 133,                /* type_spec  */
  YYSYMBOL_name_tok = 134,                 /* name_tok  */
  YYSYMBOL_qname_prefix = 135,             /* qname_prefix  */
  YYSYMBOL_qualified_type = 136,           /* qualified_type  */
  YYSYMBOL_qualified_id_expr = 137,        /* qualified_id_expr  */
  YYSYMBOL_var_decl = 138,                 /* var_decl  */
  YYSYMBOL_more_plain_declarators = 139,   /* more_plain_declarators  */
  YYSYMBOL_func_ptr_param_type = 140,      /* func_ptr_param_type  */
  YYSYMBOL_func_ptr_param_list = 141,      /* func_ptr_param_list  */
  YYSYMBOL_opt_func_ptr_param_list = 142,  /* opt_func_ptr_param_list  */
  YYSYMBOL_array_bracket_list = 143,       /* array_bracket_list  */
  YYSYMBOL_array_dim = 144,                /* array_dim  */
  YYSYMBOL_braced_init = 145,              /* braced_init  */
  YYSYMBOL_init_items = 146,               /* init_items  */
  YYSYMBOL_init_item = 147,                /* init_item  */
  YYSYMBOL_opt_array_initializer = 148,    /* opt_array_initializer  */
  YYSYMBOL_opt_initializer = 149,          /* opt_initializer  */
  YYSYMBOL_tag_typedef_decl = 150,         /* tag_typedef_decl  */
  YYSYMBOL_151_7 = 151,                    /* $@7  */
  YYSYMBOL_typedef_decl = 152,             /* typedef_decl  */
  YYSYMBOL_enum_decl = 153,                /* enum_decl  */
  YYSYMBOL_enumerator_list = 154,          /* enumerator_list  */
  YYSYMBOL_enumerator = 155,               /* enumerator  */
  YYSYMBOL_union_decl = 156,               /* union_decl  */
  YYSYMBOL_union_member_list = 157,        /* union_member_list  */
  YYSYMBOL_block = 158,                    /* block  */
  YYSYMBOL_159_8 = 159,                    /* $@8  */
  YYSYMBOL_stmt_list = 160,                /* stmt_list  */
  YYSYMBOL_stmt = 161,                     /* stmt  */
  YYSYMBOL_162_9 = 162,                    /* $@9  */
  YYSYMBOL_for_init = 163,                 /* for_init  */
  YYSYMBOL_comma_expr = 164,               /* comma_expr  */
  YYSYMBOL_comma_expr_opt = 165,           /* comma_expr_opt  */
  YYSYMBOL_switch_body = 166,              /* switch_body  */
  YYSYMBOL_primary_expr = 167,             /* primary_expr  */
  YYSYMBOL_postfix_expr = 168,             /* postfix_expr  */
  YYSYMBOL_unary_expr = 169,               /* unary_expr  */
  YYSYMBOL_cpp_cast_kw = 170,              /* cpp_cast_kw  */
  YYSYMBOL_expr = 171,                     /* expr  */
  YYSYMBOL_opt_arg_list = 172,             /* opt_arg_list  */
  YYSYMBOL_arg_list = 173,                 /* arg_list  */
  YYSYMBOL_string_seq = 174,               /* string_seq  */
  YYSYMBOL_asm_string_list = 175,          /* asm_string_list  */
  YYSYMBOL_opt_member_init_list = 176,     /* opt_member_init_list  */
  YYSYMBOL_member_init_list = 177,         /* member_init_list  */
  YYSYMBOL_member_init = 178               /* member_init  */
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
#define YYLAST   2398

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  102
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  77
/* YYNRULES -- Number of rules.  */
#define YYNRULES  283
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  580
/* YYMAXRHS -- Maximum number of symbols on right-hand side of rule.  */
#define YYMAXRHS 13
/* YYMAXLEFT -- Maximum number of symbols to the left of a handle
   accessed by $0, $-1, etc., in any rule.  */
#define YYMAXLEFT 0

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   332

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
       2,     2,     2,    94,     2,     2,     2,    87,    80,     2,
      97,    98,    85,    83,   100,    84,   101,    86,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    93,    90,
      81,    76,    82,    77,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    95,     2,    96,    79,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    91,    78,    92,    99,     2,     2,     2,
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
      75,    88,    89
};

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   525,   525,   534,   535,   539,   540,   541,   542,   543,
     544,   545,   546,   547,   548,   557,   562,   563,   583,   589,
     600,   599,   638,   637,   698,   709,   710,   714,   715,   725,
     730,   738,   739,   743,   748,   749,   750,   751,   763,   794,
     822,   823,   824,   839,   840,   854,   855,   909,   910,   914,
     915,   916,   917,   918,   919,   920,   921,   922,   923,   924,
     925,   926,   927,   928,   929,   930,   931,   935,   935,   963,
     963,   973,   987,  1004,  1060,  1060,  1079,  1079,  1129,  1148,
    1149,  1150,  1154,  1155,  1159,  1168,  1200,  1220,  1240,  1241,
    1242,  1243,  1255,  1256,  1257,  1258,  1259,  1260,  1261,  1262,
    1263,  1264,  1265,  1302,  1303,  1307,  1312,  1320,  1329,  1340,
    1376,  1436,  1457,  1483,  1511,  1529,  1551,  1594,  1595,  1604,
    1628,  1634,  1643,  1645,  1650,  1651,  1687,  1692,  1702,  1720,
    1747,  1749,  1751,  1756,  1757,  1761,  1762,  1766,  1767,  1768,
    1790,  1791,  1792,  1801,  1803,  1805,  1808,  1807,  1829,  1836,
    1847,  1869,  1899,  1932,  1943,  1944,  1946,  1951,  1953,  1978,
    1988,  1989,  1995,  1995,  2004,  2005,  2009,  2010,  2015,  2020,
    2025,  2037,  2055,  2055,  2077,  2091,  2099,  2101,  2103,  2105,
    2135,  2142,  2155,  2164,  2172,  2180,  2181,  2182,  2187,  2194,
    2195,  2201,  2217,  2218,  2228,  2229,  2245,  2246,  2253,  2259,
    2277,  2278,  2279,  2280,  2281,  2282,  2283,  2284,  2285,  2286,
    2287,  2291,  2292,  2298,  2305,  2312,  2318,  2324,  2333,  2334,
    2336,  2338,  2340,  2342,  2344,  2346,  2348,  2350,  2352,  2367,
    2369,  2380,  2406,  2456,  2469,  2510,  2511,  2512,  2513,  2517,
    2518,  2519,  2520,  2521,  2522,  2523,  2524,  2525,  2526,  2527,
    2528,  2529,  2530,  2531,  2543,  2544,  2545,  2546,  2547,  2549,
    2551,  2553,  2555,  2557,  2559,  2561,  2563,  2565,  2567,  2569,
    2597,  2598,  2602,  2603,  2614,  2615,  2626,  2633,  2659,  2660,
    2664,  2665,  2669,  2678
};
#endif

#define YYPACT_NINF (-462)
#define YYTABLE_NINF (-279)

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -462,    53,   361,  -462,  -462,     9,  -462,  -462,   114,   114,
      74,  2076,  -462,  -462,  -462,  -462,  -462,  -462,  2091,   141,
    -462,    -9,  -462,    11,   114,  1449,    66,  -462,  -462,    24,
     120,    22,    82,    94,   111,   124,   162,   173,  -462,   175,
     100,  -462,    42,    51,    91,   114,    91,   205,  -462,    91,
      91,   114,   114,   114,  -462,  -462,  -462,  -462,  -462,    87,
     -12,   265,     1,    91,  -462,   107,  -462,   232,  1068,    28,
      36,  -462,     9,   276,   231,  -462,  -462,  -462,  -462,  -462,
    -462,   285,  -462,  -462,   206,   285,  -462,   207,   114,    45,
     209,    30,   114,   114,  -462,  -462,  -462,  -462,   181,  -462,
    -462,   199,   215,   210,    48,  1328,  -462,   249,  -462,  -462,
    -462,  -462,  2091,  1133,  -462,  -462,  -462,  -462,  1393,  -462,
    -462,  -462,  -462,  1328,  1328,  1328,  1328,  1328,  1328,  -462,
     873,  1328,   254,  -462,   211,  -462,   110,  -462,   221,  2159,
     301,    -7,  1612,    38,   233,  1328,   213,  -462,  2127,   235,
       3,  -462,  1558,  -462,    14,  1575,  -462,  -462,   114,  1627,
    -462,  -462,  -462,   304,   313,   314,   209,  2127,   222,   225,
     226,   219,  -462,  -462,  -462,  -462,  1813,   227,  2159,   112,
     118,   229,  -462,   873,  -462,  -462,  -462,  -462,  -462,  -462,
    -462,    91,   271,   155,  2159,  -462,   249,  -462,   324,  -462,
    -462,  1328,  1328,   325,  2091,  1328,  1328,  1328,  1328,  1328,
    1328,  1328,  1328,  1328,  1328,  1328,  1328,  1328,  1328,  1328,
    1328,  1328,  1328,  1328,  1328,  1328,  1328,  1328,  1328,  1328,
    1328,  1328,  1328,  1328,  1328,  -462,  1198,   -32,  -462,   328,
      91,  -462,   236,   234,   249,   238,    44,  -462,   237,   239,
     240,   241,   242,  -462,    91,  1328,  -462,   285,  -462,    79,
     251,  1522,   114,   114,  1485,  -462,   340,   246,  -462,  -462,
    -462,  -462,   247,  -462,  1328,  1328,   215,  -462,  -462,  -462,
    -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,
    -462,  -462,  -462,  -462,   250,   252,  -462,  -462,  -462,  1328,
    1328,  1328,  1328,    91,   253,  -462,  1328,  -462,  1859,   255,
     259,  -462,    91,   467,   467,   208,   208,  2231,  2062,  2159,
    2159,  2159,  2159,   200,   200,  2159,  2159,  2159,  2159,  2159,
    2159,  2159,  1949,  2267,  2275,  2311,   208,   208,   113,   113,
    -462,  -462,  -462,   938,  -462,  2159,  -462,   261,    88,   114,
    2091,   267,  -462,  -462,   301,  -462,   210,   262,  2091,   359,
    2159,  -462,     2,  -462,  -462,  -462,  -462,  -462,  -462,  -462,
    1449,  -462,  -462,   270,   277,  -462,    95,   268,   290,  -462,
     278,   281,  -462,   679,  -462,  -462,  2127,  2159,  1904,   282,
    -462,   284,  1328,  2159,  -462,  -462,   306,  1328,  -462,  -462,
      75,  -462,  2159,   261,    91,  1328,   289,  -462,  -462,    37,
    2127,  -462,   210,  -462,   -11,    -6,   114,   299,  -462,  -462,
    -462,  1328,   303,   393,  -462,  -462,   -29,  2091,  1328,   307,
     776,   309,   310,   311,   320,   400,   316,    71,   343,  2091,
    -462,  -462,   321,   326,  -462,  -462,    26,   317,  -462,  -462,
     477,  -462,   322,  2195,  -462,  1003,   414,   327,  2091,   342,
    1328,   332,  -462,  1328,  1263,   334,  -462,  2120,  2091,  -462,
     776,   331,   345,  1328,   410,  1328,  -462,  -462,  -462,   348,
    1328,   336,   417,   417,   344,   350,  -462,  -462,  -462,   398,
    1328,  -462,  -462,    -6,   346,   349,  -462,   347,   398,  2159,
    -462,   352,  -462,  -462,   351,  -462,  -462,  1622,   353,  1669,
     873,  -462,  1705,   417,  -462,    23,     7,   417,  -462,  -462,
    -462,  1741,   -32,  -462,   354,   342,   449,   210,  -462,  -462,
     776,  1328,   776,  -462,   363,   331,   367,    25,  -462,  -462,
    -462,   364,    32,  -462,  -462,  2091,  -462,   233,  -462,   439,
    1777,  -462,  1328,  -462,   371,  -462,   372,   365,  -462,   776,
     374,   375,   582,  -462,  -462,   233,  -462,  -462,  1328,  1328,
     376,  -462,  -462,  -462,   373,  1991,  -462,   776,  -462,  -462
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       3,     0,     2,     1,   103,    97,    25,    26,     0,     0,
       0,     0,    92,    93,    94,    95,    96,    44,     0,     0,
       4,     0,     5,     0,     0,     0,     0,     9,    17,    88,
       0,     0,   101,     0,     0,     0,     0,     0,   104,   100,
      99,    20,     0,     0,    88,   146,    88,     0,   101,    88,
      88,     0,     0,     0,   102,    18,    19,    16,     6,    98,
      97,     0,    72,    88,    10,     0,    90,    89,     0,     0,
       0,   105,   107,     0,     0,    76,    11,    13,    12,     7,
       8,     0,    15,   160,     0,     0,   160,    99,     0,    98,
       0,     0,     0,     0,   100,    99,    98,    14,     0,    22,
      69,     0,     0,     0,     0,     0,    91,   200,   274,   201,
     204,   202,     0,     0,   208,   205,   206,   207,     0,   235,
     238,   236,   237,     0,     0,     0,     0,     0,     0,   127,
       0,     0,     0,   209,     0,   211,   218,   239,     0,   129,
     203,   140,   124,     0,   137,     0,     0,   106,    79,   157,
       0,   154,     0,     3,     0,     0,   143,    31,     0,   124,
     150,   145,   144,     0,     0,     0,     0,    79,     0,     0,
       0,   279,   280,   162,    73,    47,     0,     0,   272,     0,
     226,     0,   229,     0,   233,   224,   225,   222,   221,   223,
     219,    88,     0,     0,   192,   220,   108,   126,     0,   216,
     217,     0,   270,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   275,     0,   137,   117,     0,
      88,   122,   125,     0,    47,     0,     0,   112,     0,     0,
      94,     0,    81,    82,    88,     0,   153,   156,   159,    88,
       0,    43,     0,     0,    43,   147,     0,     0,    28,    29,
      30,    23,     0,    71,   270,   270,     0,   164,    55,    56,
      59,    60,    61,    62,    63,    64,    53,    57,    58,    49,
      50,    51,    52,    54,     0,     0,    48,    67,   110,     0,
       0,   270,     0,    88,     0,   210,     0,   214,     0,     0,
     271,   213,    88,   249,   250,   247,   248,   251,   252,   259,
     260,   261,   262,   256,   257,   263,   264,   266,   267,   268,
     265,   258,     0,   254,   255,   253,   245,   246,   243,   244,
     240,   241,   242,     0,   142,   141,   117,   109,     0,   120,
       0,     0,    74,   138,   139,   128,     0,   278,     0,     0,
     158,   155,     0,   161,    21,   149,   148,    40,    41,    42,
       0,    24,    32,     0,     0,    35,     0,     0,     0,    70,
       0,     0,   281,     0,    65,    66,    79,   273,     0,     0,
     230,     0,     0,   193,   215,   212,     0,     0,   130,   136,
       0,   133,   135,   111,    88,     0,     0,   121,   123,     0,
      79,    78,     0,    83,    84,   140,     0,     0,    33,    34,
      36,     0,     0,     0,   282,   283,   200,     0,   194,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     188,   163,     0,     0,   166,   165,     0,     0,   228,   227,
     232,   231,     0,   269,   131,     0,     0,     0,   124,   140,
       0,     0,    77,     0,     0,    98,    39,     0,   124,   152,
       0,   195,     0,     0,     0,     0,   172,   176,   177,     0,
       0,     0,     0,     0,     0,     0,   185,   186,   187,    45,
       0,   132,   134,   140,     0,     0,   114,     0,    45,    85,
      86,     0,    38,    37,     0,   179,   175,     0,     0,     0,
     189,   178,     0,     0,   276,     0,     0,     0,   174,    46,
      68,     0,   137,   118,     0,   140,     0,     0,    87,   151,
       0,     0,     0,   190,     0,   191,     0,     0,   277,   180,
     184,     0,     0,   234,   119,   124,   113,   137,    75,   167,
       0,   169,   194,   196,     0,   181,     0,     0,   116,     0,
       0,     0,     0,   183,   182,   137,   168,   170,   194,     0,
       0,   171,   199,   115,     0,     0,   198,     0,   197,   173
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -462,  -462,   319,  -462,  -462,  -462,  -462,   462,  -462,   308,
       4,  -462,  -462,  -462,  -462,  -462,   -22,   355,  -462,   116,
    -462,  -462,   223,   228,  -462,  -462,  -462,  -132,  -462,   119,
     143,    -1,     5,    -2,    16,  -462,     0,   145,   139,  -462,
    -155,  -133,  -134,   -97,  -462,    39,  -218,  -391,  -462,  -462,
      18,   482,   411,   243,   486,   413,  -100,  -462,  -462,  -408,
    -462,  -462,  -123,  -461,  -462,  -462,  -462,   -52,  -462,    15,
    -188,   396,   256,  -405,   146,  -462,   230
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,    20,    21,    22,    84,    23,   166,   158,
      53,    99,   264,   372,   373,    25,   520,   177,   296,    62,
     386,   167,    26,    27,    28,   410,   148,   251,   252,   253,
      69,   259,    30,   132,    48,   133,   442,   347,   241,   242,
     243,    70,   134,   399,   400,   401,   247,   238,    34,    90,
     443,    36,   150,   151,    37,   152,   444,   277,   383,   445,
     510,   534,   446,   472,   562,   135,   136,   137,   138,   194,
     309,   310,   140,   515,   103,   171,   172
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      31,    29,    33,   174,   267,   415,    24,   193,   237,    47,
      46,   248,   538,    39,    40,    45,    47,    54,    32,   346,
      35,  -103,   474,    47,    63,     4,    72,    65,   538,    59,
     538,   141,    38,     4,    38,   272,    74,   538,  -104,   144,
     459,   244,    38,  -103,   246,     4,    38,    39,    87,   108,
      89,   175,    74,     3,     4,    38,    94,    95,    96,  -104,
     193,   182,   505,   145,   470,   463,   184,   143,   496,   236,
     236,   185,   186,   187,   188,   189,   190,    41,   516,   195,
     176,    57,    65,   139,   464,   100,   380,   381,    68,    68,
     176,   561,  -278,   156,   102,   256,   160,   161,   162,   142,
     540,    58,   523,   257,    66,   541,   262,   574,   537,    67,
      47,   180,   542,   389,   257,   539,   488,     4,    38,    68,
     178,    73,   549,   554,   551,   142,   306,   159,   192,   191,
     556,   145,   460,    85,   546,   343,   -27,    74,    98,   344,
      47,   240,    86,   481,    55,    56,    47,   254,    74,   353,
      47,   566,   260,    47,   572,   260,    64,    47,   240,    66,
     139,   198,   482,   265,    67,    47,   254,   454,   483,   579,
      71,    66,   199,   200,    68,   455,    67,    97,   -27,    75,
      98,   192,   303,   405,    76,   420,   406,    88,   421,    91,
      82,    83,    92,    93,   163,   164,   165,    74,   232,   233,
     234,    77,    47,   312,   105,   201,   104,   202,     4,    72,
     298,   203,   299,   300,    78,   301,   308,   178,   169,   170,
     313,   314,   315,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,   327,   328,   329,   330,   331,   332,
     333,   334,   335,   336,   337,   338,   339,   340,   341,   342,
     390,   345,    79,   305,   447,   306,   411,   196,    38,    31,
      29,    33,    47,    80,   376,    24,    81,   365,   366,   101,
     360,   457,   215,   216,   196,    72,   106,    32,   461,    35,
     146,   147,   237,   230,   231,   232,   233,   234,   149,   178,
     178,   230,   231,   232,   233,   234,   168,   153,    83,  -103,
     157,   173,   204,   495,   544,   471,   235,   197,   268,   246,
     249,   255,   462,   504,   387,   388,   178,   269,   270,   276,
     273,   393,   274,   275,   297,   302,   497,   307,   311,   558,
     501,   348,   351,   355,   304,   352,   350,   356,   -80,   357,
     451,   363,   358,   377,   378,   379,   384,   573,    47,   240,
     385,   392,   409,   395,   407,   102,    47,   254,   402,   299,
     522,   404,   414,   418,     4,     5,   422,   419,    47,    63,
       6,     7,     8,     9,   416,   423,   424,    10,    11,   425,
     449,   192,   450,   349,    47,   254,   458,   535,   452,   466,
     557,    12,    13,    14,    15,    16,   469,   359,   451,    17,
     468,   477,   362,   479,   473,    18,   475,   476,    47,   254,
     478,   486,   453,   480,   484,   489,   487,   493,   236,   490,
     139,   465,   514,   494,   502,    47,    46,   548,   192,   471,
     498,   306,   508,   513,    19,   506,   467,    47,   511,   485,
     518,   517,   519,   526,   524,   471,   391,   525,   528,   529,
     531,   545,   547,   552,   555,   396,    47,   240,   553,   559,
     -43,   563,   564,   565,   567,   568,    47,   240,   192,   576,
     402,   577,   261,    44,   271,   139,   527,   413,   499,   139,
     107,    38,   108,   109,   110,   111,   417,   374,   507,   408,
     509,   403,   375,    49,   492,   512,   154,    50,   245,   155,
     361,   179,   354,   412,     0,   521,   382,     0,   192,     0,
     533,     0,   112,   113,   114,     0,   115,   116,   117,     0,
     118,   207,   208,   119,   120,   121,   122,     0,   192,     0,
     192,   215,   216,     0,     0,     0,     0,     0,     0,   123,
     124,     0,     0,    47,   240,     0,   550,   456,   228,   229,
     230,   231,   232,   233,   234,     0,     0,   192,     0,     0,
     192,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   128,     0,     0,   130,   192,   131,     0,     0,     0,
       0,     0,     0,     0,   575,   426,     5,   108,   109,   110,
     111,     6,     7,    51,    52,     0,     0,     0,     0,   427,
     428,   429,     0,   430,   431,   432,   433,   434,   435,   436,
     569,   570,    12,    13,    14,    15,    16,   112,   113,   114,
       0,   115,   116,   117,     0,   118,    18,     0,   119,   120,
     121,   122,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   123,   124,     0,     0,     0,     0,
       0,     0,     0,   437,   438,     0,     0,   439,     0,     0,
       0,     0,   125,     0,     0,     0,   126,   127,     0,     0,
       0,     0,   440,   173,   571,     0,   128,     0,     0,   130,
       0,   131,   426,     5,   108,   109,   110,   111,     6,     7,
      51,    52,     0,     0,     0,     0,   427,   428,   429,     0,
     430,   431,   432,   433,   434,   435,   436,     0,     0,    12,
      13,    14,    15,    16,   112,   113,   114,     0,   115,   116,
     117,     0,   118,    18,     0,   119,   120,   121,   122,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   123,   124,     0,     0,     0,     0,     0,     0,     0,
     437,   438,     0,     0,   439,     0,     0,     0,     0,   125,
       0,     0,     0,   126,   127,     0,     0,     0,     0,   440,
     173,   441,     0,   128,     0,     0,   130,     0,   131,   426,
       5,   108,   109,   110,   111,     6,     7,    51,    52,     0,
       0,     0,     0,   427,   428,   429,     0,   430,   431,   432,
     433,   434,   435,   436,     0,     0,    12,    13,    14,    15,
      16,   112,   113,   114,     0,   115,   116,   117,     0,   118,
      18,     0,   119,   120,   121,   122,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   123,   124,
       0,     0,     0,     0,     0,     0,     0,   437,   438,     0,
       0,   439,     0,     0,     0,     0,   125,     0,     0,     0,
     126,   127,     0,     0,     0,     0,   440,   173,     0,     0,
     128,     0,     0,   130,     0,   131,   107,     5,   108,   109,
     110,   111,     6,     7,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    12,    13,    14,    15,    16,   112,   113,
     114,     0,   115,   116,   117,     0,   118,    18,     0,   119,
     120,   121,   122,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   123,   124,     0,     0,     0,
       0,   107,    38,   108,   109,   110,   111,     0,     0,     0,
       0,     0,     0,   125,     0,     0,     0,   126,   127,     0,
       0,     0,     0,     0,     0,     0,     0,   128,     0,     0,
     130,     0,   131,   112,   113,   114,     0,   115,   116,   117,
       0,   118,     0,     0,   119,   120,   121,   122,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     123,   124,     0,     0,     0,     0,   107,    38,   108,   109,
     110,   111,     0,     0,     0,     0,     0,     0,   125,     0,
       0,     0,   126,   127,     0,     0,     0,     0,     0,   343,
     398,     0,   128,     0,     0,   130,     0,   131,   112,   113,
     114,     0,   115,   116,   117,     0,   118,     0,     0,   119,
     120,   121,   122,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   123,   124,     0,     0,     0,
       0,   107,    38,   108,   109,   110,   111,     0,     0,     0,
       0,     0,     0,   125,     0,     0,     0,   126,   127,     0,
       0,     0,     0,     0,   343,   491,     0,   128,     0,     0,
     130,     0,   131,   112,   113,   114,     0,   115,   116,   117,
       0,   118,     0,     0,   119,   120,   121,   122,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     123,   124,     0,     0,     0,     0,   107,    38,   108,   109,
     110,   111,     0,     0,     0,     0,     0,     0,   125,     0,
       0,     0,   126,   127,     0,     0,     0,     0,     0,     0,
       0,     0,   128,     0,   129,   130,     0,   131,   112,   113,
     114,     0,   115,   116,   117,     0,   118,     0,     0,   119,
     120,   121,   122,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   123,   124,     0,     0,     0,
       0,   107,    38,   108,   109,   110,   111,     0,     0,     0,
       0,     0,     0,   125,     0,     0,     0,   126,   127,     0,
       0,     0,     0,     0,     0,     0,     0,   128,   181,     0,
     130,     0,   131,   112,   113,   114,     0,   115,   116,   117,
       0,   118,     0,     0,   119,   120,   121,   122,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     123,   124,     0,     0,     0,     0,   107,    38,   108,   109,
     110,   111,     0,     0,     0,     0,     0,     0,   125,     0,
       0,     0,   126,   127,     0,     0,     0,     0,     0,   343,
       0,     0,   128,     0,     0,   130,     0,   131,   112,   113,
     114,     0,   115,   116,   117,     0,   118,     0,     0,   119,
     120,   121,   122,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   123,   124,     0,     0,     0,
       0,   107,    38,   108,   109,   110,   111,     0,     0,     0,
       0,     0,     0,   125,     0,     0,     0,   126,   127,     0,
       0,     0,     0,     0,     0,     0,     0,   128,     0,   500,
     130,     0,   131,   112,   113,   114,     0,   115,   116,   117,
       0,   118,     0,     0,   119,   120,   121,   122,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     123,   124,     0,     0,     0,     0,   107,    38,   108,   109,
     110,   111,     0,     0,     0,     0,     0,     0,   125,     0,
       0,     0,   126,   127,     0,     0,     0,     0,     0,     0,
       0,     0,   128,     0,     0,   130,     0,   131,   112,   113,
     114,     0,   115,   116,   117,     0,   118,     0,     0,   119,
     120,   121,   122,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     4,    60,     0,   123,   124,     0,     6,     7,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   125,     0,     0,     0,   126,   127,    12,
      13,    14,    15,    16,     0,     0,     0,   128,     4,     5,
     183,     0,   131,    18,     6,     7,    51,    52,   367,   368,
     369,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    12,    13,    14,    15,    16,
       0,     0,     0,    17,     0,     4,     5,     0,     0,    18,
     370,     6,     7,     8,     9,     0,     0,     0,    10,    11,
       0,     0,     0,     0,     0,     0,     0,     0,    61,     0,
       0,     0,    12,    13,    14,    15,    16,     0,     0,     0,
      17,     4,     5,     0,     0,     0,    18,     6,     7,    51,
      52,     0,     0,     0,     0,     0,     0,   371,     4,     5,
       0,     0,     0,     0,     6,     7,    51,    52,    12,    13,
      14,    15,    16,     0,     0,    19,     0,     0,     0,     0,
       0,     0,    18,     0,     0,    12,    13,    14,    15,    16,
       0,     0,     0,     0,   364,     4,     5,     0,     0,    18,
       0,     6,     7,    51,    52,     0,     0,     0,     0,     0,
       4,     5,     0,     0,     0,     0,     6,     7,    51,    52,
       0,     0,    12,    13,    14,    15,    16,     0,     0,     0,
     258,     0,     0,     0,     0,     0,    18,    12,    13,    14,
      15,    16,     0,     0,     0,     0,     0,   263,     0,     0,
       0,    18,     0,     0,   205,   206,   207,   208,   209,   210,
     211,   212,   213,   214,     0,     0,   215,   216,   217,   218,
     219,   220,   221,     0,     0,     0,   222,   239,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
       0,     0,   266,     0,     0,     0,     0,     0,     0,     0,
     530,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,     0,     0,   215,   216,   217,   218,   219,   220,   221,
       0,     0,     0,   222,     0,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   532,     0,   215,
     216,   217,   218,   219,   220,   221,     0,     0,     0,   222,
       0,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   536,     0,   215,   216,   217,   218,   219,
     220,   221,     0,     0,     0,   222,     0,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   205,
     206,   207,   208,   209,   210,   211,   212,   213,   214,   543,
       0,   215,   216,   217,   218,   219,   220,   221,     0,     0,
       0,   222,     0,   223,   224,   225,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   278,   279,   280,   281,     0,
       0,   282,   283,   284,   285,   560,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   286,
       0,     0,     0,     0,   287,   288,   289,   290,   291,   292,
       0,     0,     0,     0,     0,     0,     0,   293,   294,     0,
     295,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,     0,     0,   215,   216,   217,   218,   219,   220,   221,
       0,     0,     0,   222,     0,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,     0,     0,     0,
       0,     0,     0,     0,     0,   394,   205,   206,   207,   208,
     209,   210,   211,   212,   213,   214,     0,     0,   215,   216,
     217,   218,   219,   220,   221,     0,     0,     0,   222,     0,
     223,   224,   225,   226,   227,   228,   229,   230,   231,   232,
     233,   234,     0,     0,     0,     0,     0,     0,     0,     0,
     448,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,     0,     0,   215,   216,   217,   218,   219,   220,   221,
       0,     0,     0,   222,     0,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,     0,     0,     0,
       0,     0,   397,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,     0,     0,   215,   216,   217,   218,   219,
     220,   221,     0,     0,     0,   222,     0,   223,   224,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,     4,
       5,     0,     0,     0,   578,     6,     7,    42,    43,     0,
       0,     0,     0,     0,     4,     5,     0,     0,     0,     0,
       6,     7,    51,    52,     0,     0,    12,    13,    14,    15,
      16,     0,     0,     0,   205,   206,   207,   208,   209,     0,
      18,    12,    13,    14,    15,    16,   215,   216,     0,     0,
       4,     5,     0,     0,     0,    18,     6,     7,    51,    52,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
       0,     0,     0,     0,     0,     0,     0,    12,    13,   250,
      15,    16,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    18,   205,   206,   207,   208,   209,   210,   211,   212,
     213,   214,     0,     0,   215,   216,   217,   218,   219,   220,
     221,     0,     0,     0,   222,     0,   223,   224,   225,   226,
     227,   228,   229,   230,   231,   232,   233,   234,     0,     0,
     503,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,     0,     0,   215,   216,   217,   218,   219,   220,   221,
       0,     0,     0,   222,     0,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   205,   206,   207,
     208,   209,   210,     0,     0,     0,     0,     0,     0,   215,
     216,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   205,   206,   207,   208,     0,     0,     0,
       0,     0,     0,     0,     0,   215,   216,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   225,
     226,   227,   228,   229,   230,   231,   232,   233,   234,   205,
     206,   207,   208,     0,     0,     0,     0,   205,   206,   207,
     208,   215,   216,     0,     0,     0,     0,     0,     0,   215,
     216,     0,     0,     0,     0,     0,   226,   227,   228,   229,
     230,   231,   232,   233,   234,   227,   228,   229,   230,   231,
     232,   233,   234,   205,   206,   207,   208,     0,     0,     0,
       0,     0,     0,     0,     0,   215,   216,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   228,   229,   230,   231,   232,   233,   234
};

static const yytype_int16 yycheck[] =
{
       2,     2,     2,   103,   159,     3,     2,   130,   141,    11,
      11,   145,     5,     8,     9,    11,    18,    18,     2,   237,
       2,    50,   430,    25,    25,     3,     4,     3,     5,    24,
       5,     3,     4,     3,     4,   167,    31,     5,    50,     3,
       3,     3,     4,    50,    76,     3,     4,    42,    43,     5,
      45,     3,    47,     0,     3,     4,    51,    52,    53,    50,
     183,   113,   470,    95,    93,    76,   118,    69,   459,    76,
      76,   123,   124,   125,   126,   127,   128,     3,   483,   131,
      42,    90,     3,    68,    95,    97,   274,   275,    95,    95,
      42,   552,    91,    88,    93,    92,    91,    92,    93,    97,
      93,    90,   493,   100,    80,    98,    92,   568,   513,    85,
     112,   112,   517,   301,   100,    92,    90,     3,     4,    95,
     105,    99,   530,    98,   532,    97,   100,    97,   130,   130,
      98,    95,    95,    91,   525,    91,    91,   132,    93,   236,
     142,   142,    91,    72,     3,     4,   148,   148,   143,   246,
     152,   559,   152,   155,   562,   155,    90,   159,   159,    80,
     145,    51,    91,   158,    85,   167,   167,    92,    97,   577,
      50,    80,    62,    63,    95,   100,    85,    90,    91,    97,
      93,   183,   183,    95,    90,    90,    98,    44,    93,    46,
      90,    91,    49,    50,    13,    14,    15,   192,    85,    86,
      87,    90,   204,   204,    97,    95,    63,    97,     3,     4,
      98,   101,   100,    95,    90,    97,   201,   202,     3,     4,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     302,   236,    90,    98,   386,   100,   356,     3,     4,   261,
     261,   261,   264,    90,   264,   261,    91,   262,   263,     4,
     255,   405,    64,    65,     3,     4,    44,   261,   410,   261,
       4,    50,   415,    83,    84,    85,    86,    87,     3,   274,
     275,    83,    84,    85,    86,    87,    97,    91,    91,    50,
      91,    91,    81,   458,   522,   428,     5,    96,     4,    76,
      97,    76,   412,   468,   299,   300,   301,     4,     4,   100,
      98,   306,    97,    97,    97,    96,   460,     3,     3,   547,
     464,     3,    98,    96,   191,    97,   100,    98,    98,    98,
     392,    90,   100,     3,    98,    98,    96,   565,   350,   350,
      98,    98,    85,    98,   349,    93,   358,   358,   343,   100,
     493,   100,     3,    93,     3,     4,    98,    90,   370,   370,
       9,    10,    11,    12,   370,    85,    98,    16,    17,    98,
      98,   383,    98,   240,   386,   386,    97,   510,    82,    90,
     545,    30,    31,    32,    33,    34,     3,   254,   450,    38,
      97,    90,   259,     3,    97,    44,    97,    97,   410,   410,
      90,    90,   397,    97,    71,    98,    90,     3,    76,    97,
     405,   416,     5,    96,    90,   427,   427,   527,   430,   552,
      98,   100,    22,    97,    73,    90,   421,   439,    90,   439,
      90,    97,    44,    96,    98,   568,   303,    98,    96,    98,
      97,    97,     3,    90,    90,   312,   458,   458,    91,    20,
      99,    90,    90,    98,    90,    90,   468,   468,   470,    93,
     455,    98,   153,    11,   166,   460,   498,   358,   463,   464,
       3,     4,     5,     6,     7,     8,   370,   264,   473,   350,
     475,   346,   264,    11,   455,   480,    85,    11,   143,    86,
     257,   105,   246,   357,    -1,   490,   276,    -1,   510,    -1,
     510,    -1,    35,    36,    37,    -1,    39,    40,    41,    -1,
      43,    54,    55,    46,    47,    48,    49,    -1,   530,    -1,
     532,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    62,
      63,    -1,    -1,   545,   545,    -1,   531,   404,    81,    82,
      83,    84,    85,    86,    87,    -1,    -1,   559,    -1,    -1,
     562,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    94,    -1,    -1,    97,   577,    99,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   569,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,    17,
      18,    19,    -1,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    39,    40,    41,    -1,    43,    44,    -1,    46,    47,
      48,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    71,    72,    -1,    -1,    75,    -1,    -1,
      -1,    -1,    80,    -1,    -1,    -1,    84,    85,    -1,    -1,
      -1,    -1,    90,    91,    92,    -1,    94,    -1,    -1,    97,
      -1,    99,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    -1,    -1,    -1,    -1,    17,    18,    19,    -1,
      21,    22,    23,    24,    25,    26,    27,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    39,    40,
      41,    -1,    43,    44,    -1,    46,    47,    48,    49,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      71,    72,    -1,    -1,    75,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    84,    85,    -1,    -1,    -1,    -1,    90,
      91,    92,    -1,    94,    -1,    -1,    97,    -1,    99,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    -1,
      -1,    -1,    -1,    17,    18,    19,    -1,    21,    22,    23,
      24,    25,    26,    27,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    39,    40,    41,    -1,    43,
      44,    -1,    46,    47,    48,    49,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    71,    72,    -1,
      -1,    75,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,
      84,    85,    -1,    -1,    -1,    -1,    90,    91,    -1,    -1,
      94,    -1,    -1,    97,    -1,    99,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    39,    40,    41,    -1,    43,    44,    -1,    46,
      47,    48,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    62,    63,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,    -1,    -1,
      97,    -1,    99,    35,    36,    37,    -1,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    47,    48,    49,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      62,    63,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,
      -1,    -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    94,    -1,    -1,    97,    -1,    99,    35,    36,
      37,    -1,    39,    40,    41,    -1,    43,    -1,    -1,    46,
      47,    48,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    62,    63,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,    91,    92,    -1,    94,    -1,    -1,
      97,    -1,    99,    35,    36,    37,    -1,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    47,    48,    49,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      62,    63,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,
      -1,    -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    96,    97,    -1,    99,    35,    36,
      37,    -1,    39,    40,    41,    -1,    43,    -1,    -1,    46,
      47,    48,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    62,    63,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,    95,    -1,
      97,    -1,    99,    35,    36,    37,    -1,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    47,    48,    49,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      62,    63,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,
      -1,    -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    91,
      -1,    -1,    94,    -1,    -1,    97,    -1,    99,    35,    36,
      37,    -1,    39,    40,    41,    -1,    43,    -1,    -1,    46,
      47,    48,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    62,    63,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,    -1,    96,
      97,    -1,    99,    35,    36,    37,    -1,    39,    40,    41,
      -1,    43,    -1,    -1,    46,    47,    48,    49,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      62,    63,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,
      -1,    -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    -1,    97,    -1,    99,    35,    36,
      37,    -1,    39,    40,    41,    -1,    43,    -1,    -1,    46,
      47,    48,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,    -1,    62,    63,    -1,     9,    10,
      11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    84,    85,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    94,     3,     4,
      97,    -1,    99,    44,     9,    10,    11,    12,    13,    14,
      15,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      -1,    -1,    -1,    38,    -1,     3,     4,    -1,    -1,    44,
      45,     9,    10,    11,    12,    -1,    -1,    -1,    16,    17,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      38,     3,     4,    -1,    -1,    -1,    44,     9,    10,    11,
      12,    -1,    -1,    -1,    -1,    -1,    -1,    92,     3,     4,
      -1,    -1,    -1,    -1,     9,    10,    11,    12,    30,    31,
      32,    33,    34,    -1,    -1,    73,    -1,    -1,    -1,    -1,
      -1,    -1,    44,    -1,    -1,    30,    31,    32,    33,    34,
      -1,    -1,    -1,    -1,    92,     3,     4,    -1,    -1,    44,
      -1,     9,    10,    11,    12,    -1,    -1,    -1,    -1,    -1,
       3,     4,    -1,    -1,    -1,    -1,     9,    10,    11,    12,
      -1,    -1,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      92,    -1,    -1,    -1,    -1,    -1,    44,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    92,    -1,    -1,
      -1,    44,    -1,    -1,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    -1,    -1,    64,    65,    66,    67,
      68,    69,    70,    -1,    -1,    -1,    74,    85,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      -1,    -1,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      98,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    -1,    -1,    64,    65,    66,    67,    68,    69,    70,
      -1,    -1,    -1,    74,    -1,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    98,    -1,    64,
      65,    66,    67,    68,    69,    70,    -1,    -1,    -1,    74,
      -1,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    98,    -1,    64,    65,    66,    67,    68,
      69,    70,    -1,    -1,    -1,    74,    -1,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    98,
      -1,    64,    65,    66,    67,    68,    69,    70,    -1,    -1,
      -1,    74,    -1,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    52,    53,    54,    55,    -1,
      -1,    58,    59,    60,    61,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,
      -1,    -1,    -1,    -1,    81,    82,    83,    84,    85,    86,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,    95,    -1,
      97,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    -1,    -1,    64,    65,    66,    67,    68,    69,    70,
      -1,    -1,    -1,    74,    -1,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    -1,    -1,    64,    65,
      66,    67,    68,    69,    70,    -1,    -1,    -1,    74,    -1,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      96,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    -1,    -1,    64,    65,    66,    67,    68,    69,    70,
      -1,    -1,    -1,    74,    -1,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    -1,    -1,    -1,
      -1,    -1,    93,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    -1,    -1,    64,    65,    66,    67,    68,
      69,    70,    -1,    -1,    -1,    74,    -1,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,     3,
       4,    -1,    -1,    -1,    93,     9,    10,    11,    12,    -1,
      -1,    -1,    -1,    -1,     3,     4,    -1,    -1,    -1,    -1,
       9,    10,    11,    12,    -1,    -1,    30,    31,    32,    33,
      34,    -1,    -1,    -1,    52,    53,    54,    55,    56,    -1,
      44,    30,    31,    32,    33,    34,    64,    65,    -1,    -1,
       3,     4,    -1,    -1,    -1,    44,     9,    10,    11,    12,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    44,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    -1,    -1,    64,    65,    66,    67,    68,    69,
      70,    -1,    -1,    -1,    74,    -1,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    -1,    -1,
      90,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    -1,    -1,    64,    65,    66,    67,    68,    69,    70,
      -1,    -1,    -1,    74,    -1,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    52,    53,    54,
      55,    56,    57,    -1,    -1,    -1,    -1,    -1,    -1,    64,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    52,    53,    54,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    52,
      53,    54,    55,    -1,    -1,    -1,    -1,    52,    53,    54,
      55,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    64,
      65,    -1,    -1,    -1,    -1,    -1,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    80,    81,    82,    83,    84,
      85,    86,    87,    52,    53,    54,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    64,    65,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    81,    82,    83,    84,    85,    86,    87
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   103,   104,     0,     3,     4,     9,    10,    11,    12,
      16,    17,    30,    31,    32,    33,    34,    38,    44,    73,
     105,   106,   107,   109,   112,   117,   124,   125,   126,   133,
     134,   135,   136,   138,   150,   152,   153,   156,     4,   134,
     134,     3,    11,    12,   109,   112,   133,   135,   136,   153,
     156,    11,    12,   112,   133,     3,     4,    90,    90,   134,
       4,    99,   121,   133,    90,     3,    80,    85,    95,   132,
     143,    50,     4,    99,   134,    97,    90,    90,    90,    90,
      90,    91,    90,    91,   108,    91,    91,   134,   132,   134,
     151,   132,   132,   132,   134,   134,   134,    90,    93,   113,
      97,     4,    93,   176,   132,    97,    44,     3,     5,     6,
       7,     8,    35,    36,    37,    39,    40,    41,    43,    46,
      47,    48,    49,    62,    63,    80,    84,    85,    94,    96,
      97,    99,   135,   137,   144,   167,   168,   169,   170,   171,
     174,     3,    97,   135,     3,    95,     4,    50,   128,     3,
     154,   155,   157,    91,   154,   157,   134,    91,   111,    97,
     134,   134,   134,    13,    14,    15,   110,   123,    97,     3,
       4,   177,   178,    91,   158,     3,    42,   119,   171,   173,
     133,    95,   169,    97,   169,   169,   169,   169,   169,   169,
     169,   133,   135,   164,   171,   169,     3,    96,    51,    62,
      63,    95,    97,   101,    81,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    64,    65,    66,    67,    68,
      69,    70,    74,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,     5,    76,   143,   149,    85,
     133,   140,   141,   142,     3,   119,    76,   148,   144,    97,
      32,   129,   130,   131,   133,    76,    92,   100,    92,   133,
     138,   104,    92,    92,   114,   134,    85,   142,     4,     4,
       4,   111,   129,    98,    97,    97,   100,   159,    52,    53,
      54,    55,    58,    59,    60,    61,    76,    81,    82,    83,
      84,    85,    86,    94,    95,    97,   120,    97,    98,   100,
      95,    97,    96,   133,   132,    98,   100,     3,   171,   172,
     173,     3,   133,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   171,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   171,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   171,    91,   145,   171,   148,   139,     3,   132,
     100,    98,    97,   145,   174,    96,    98,    98,   100,   132,
     171,   155,   132,    90,    92,   134,   134,    13,    14,    15,
      45,    92,   115,   116,   124,   125,   138,     3,    98,    98,
     172,   172,   178,   160,    96,    98,   122,   171,   171,   172,
     169,   132,    98,   171,    96,    98,   132,    93,    92,   145,
     146,   147,   171,   139,   100,    95,    98,   134,   140,    85,
     127,   158,   176,   131,     3,     3,   112,   121,    93,    90,
      90,    93,    98,    85,    98,    98,     3,    17,    18,    19,
      21,    22,    23,    24,    25,    26,    27,    71,    72,    75,
      90,    92,   138,   152,   158,   161,   164,   129,    96,    98,
      98,   169,    82,   171,    92,   100,   132,   144,    97,     3,
      95,   129,   158,    76,    95,   134,    90,   171,    97,     3,
      93,   164,   165,    97,   161,    97,    97,    90,    90,     3,
      97,    72,    91,    97,    71,   138,    90,    90,    90,    98,
      97,    92,   147,     3,    96,   142,   149,   144,    98,   171,
      96,   144,    90,    90,   142,   161,    90,   171,    22,   171,
     162,    90,   171,    97,     5,   175,   175,    97,    90,    44,
     118,   171,   143,   149,    98,    98,    96,   118,    96,    98,
      98,    97,    98,   138,   163,   164,    98,   175,     5,    92,
      93,    98,   175,    98,   148,    97,   149,     3,   158,   161,
     171,   161,    90,    91,    98,    90,    98,   142,   148,    20,
      98,   165,   166,    90,    90,    98,   161,    90,    90,    28,
      29,    92,   161,   148,   165,   171,    93,    98,    93,   161
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,   102,   103,   104,   104,   105,   105,   105,   105,   105,
     105,   105,   105,   105,   105,   105,   105,   105,   106,   106,
     108,   107,   110,   109,   111,   112,   112,   113,   113,   113,
     113,   114,   114,   115,   115,   115,   115,   115,   115,   115,
     116,   116,   116,   117,   117,   118,   118,   119,   119,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   122,   121,   123,
     121,   121,   124,   125,   127,   126,   128,   126,   126,   129,
     129,   129,   130,   130,   131,   131,   131,   131,   132,   132,
     132,   132,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,   134,   134,   135,   135,   136,   137,   138,
     138,   138,   138,   138,   138,   138,   138,   139,   139,   139,
     140,   140,   141,   141,   142,   142,   143,   143,   143,   144,
     145,   145,   145,   146,   146,   147,   147,   148,   148,   148,
     149,   149,   149,   150,   150,   150,   151,   150,   150,   150,
     152,   152,   152,   153,   154,   154,   154,   155,   155,   156,
     157,   157,   159,   158,   160,   160,   161,   161,   161,   161,
     161,   161,   162,   161,   161,   161,   161,   161,   161,   161,
     161,   161,   161,   161,   161,   161,   161,   161,   161,   163,
     163,   163,   164,   164,   165,   165,   166,   166,   166,   166,
     167,   167,   167,   167,   167,   167,   167,   167,   167,   167,
     167,   168,   168,   168,   168,   168,   168,   168,   169,   169,
     169,   169,   169,   169,   169,   169,   169,   169,   169,   169,
     169,   169,   169,   169,   169,   170,   170,   170,   170,   171,
     171,   171,   171,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   171,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   171,   171,   171,   171,   171,   171,   171,   171,
     172,   172,   173,   173,   174,   174,   175,   175,   176,   176,
     177,   177,   178,   178
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     2,     2,     2,     1,
       2,     2,     2,     2,     3,     3,     2,     1,     2,     2,
       0,     6,     0,     5,     3,     1,     1,     0,     3,     3,
       3,     0,     2,     2,     2,     1,     2,     4,     4,     3,
       1,     1,     1,     0,     1,     0,     1,     1,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     0,     8,     0,
       5,     4,     2,     4,     0,    10,     0,     7,     6,     0,
       1,     1,     1,     3,     3,     5,     5,     6,     0,     1,
       1,     2,     1,     1,     1,     1,     1,     1,     2,     2,
       2,     1,     2,     1,     1,     2,     3,     2,     2,     5,
       5,     6,     4,    10,     8,    13,    11,     0,     5,     6,
       2,     3,     1,     3,     0,     1,     3,     2,     4,     1,
       2,     3,     4,     1,     3,     1,     1,     0,     2,     2,
       0,     2,     2,     4,     4,     4,     0,     5,     6,     6,
       4,    10,     8,     5,     1,     3,     2,     1,     3,     5,
       0,     3,     0,     4,     0,     2,     1,     5,     7,     5,
       7,     7,     0,    10,     3,     3,     2,     2,     3,     3,
       4,     5,     6,     6,     4,     2,     2,     2,     1,     0,
       1,     1,     1,     3,     0,     1,     0,     4,     3,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     1,     4,     3,     3,     4,     2,     2,     1,     2,
       2,     2,     2,     2,     2,     2,     2,     5,     5,     2,
       4,     5,     5,     2,     8,     1,     1,     1,     1,     1,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     5,
       0,     1,     1,     3,     1,     2,     1,     2,     0,     2,
       1,     3,     4,     4
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0
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
       0,     0,     0,     0
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
       0,     0,     0,     0
};

/* YYCONFLP[YYPACT[STATE-NUM]] -- Pointer into YYCONFL of start of
   list of conflicting reductions corresponding to action entry for
   state STATE-NUM in yytable.  0 means no conflicts.  The list in
   yyconfl is terminated by a rule number of 0.  */
static const yytype_int8 yyconflp[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    25,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    27,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     1,     3,     0,     0,     0,     0,
       5,     7,     9,    11,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    13,    15,    17,    19,    21,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    23,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    55,    57,
       0,     0,     0,     0,    59,    61,    63,    65,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    67,    69,    71,    73,    75,
       0,     0,     0,     0,     0,    31,    33,     0,     0,    77,
       0,    35,    37,    39,    41,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    43,    45,    47,    49,    51,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0
};

/* YYCONFL[I] -- lists of conflicting rule numbers, each terminated by
   0, pointed into by YYCONFLP.  */
static const short yyconfl[] =
{
       0,    43,     0,    43,     0,    43,     0,    43,     0,    43,
       0,    43,     0,    43,     0,    43,     0,    43,     0,    43,
       0,    43,     0,    43,     0,    88,     0,   101,     0,    88,
       0,    43,     0,    43,     0,    43,     0,    43,     0,    43,
       0,    43,     0,    43,     0,    43,     0,    43,     0,    43,
       0,    43,     0,    43,     0,    43,     0,    43,     0,    43,
       0,    43,     0,    43,     0,    43,     0,    43,     0,    43,
       0,    43,     0,    43,     0,    43,     0,    43,     0
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
  "TYPE_NAME", "STRING_LITERAL", "INT_LITERAL", "CHAR_LITERAL",
  "FLOAT_LITERAL", "CLASS", "STRUCT", "ENUM", "UNION", "PUBLIC", "PRIVATE",
  "PROTECTED", "NAMESPACE", "TYPEDEF", "RETURN", "IF", "ELSE", "DO",
  "WHILE", "FOR", "BREAK", "CONTINUE", "GOTO", "SWITCH", "CASE", "DEFAULT",
  "INT_KW", "FLOAT_KW", "VOID_KW", "BOOL_KW", "CHAR_KW", "NEW", "DELETE",
  "THIS", "VIRTUAL", "TRUE_KW", "FALSE_KW", "NULLPTR_KW", "OPERATOR",
  "SIZEOF", "CONST", "FRIEND", "STATIC_CAST", "DYNAMIC_CAST", "CONST_CAST",
  "REINTERPRET_CAST", "COLONCOLON", "ARROW", "EQ", "NE", "LE", "GE",
  "ANDAND", "OROR", "PLUSEQ", "MINUSEQ", "STAREQ", "SLASHEQ", "INC", "DEC",
  "SHL", "SHR", "ANDEQ", "OREQ", "XOREQ", "SHLEQ", "SHREQ", "ASM",
  "VOLATILE", "NATIVE", "MODEQ", "STATIC", "'='", "'?'", "'|'", "'^'",
  "'&'", "'<'", "'>'", "'+'", "'-'", "'*'", "'/'", "'%'",
  "SIZEOF_TYPE_PREC", "LOWER_THAN_ELSE", "';'", "'{'", "'}'", "':'", "'!'",
  "'['", "']'", "'('", "')'", "'~'", "','", "'.'", "$accept", "program",
  "top_decl_list", "top_decl", "native_decl", "namespace_decl", "$@1",
  "class_decl", "$@2", "class_body", "class_or_struct_kw", "opt_base",
  "member_list", "member", "access_spec", "opt_virtual", "opt_const",
  "func_name", "operator_symbol", "func_header", "$@3", "$@4", "func_decl",
  "func_def", "out_of_line_def", "$@5", "$@6", "opt_param_list",
  "param_list", "param", "pointer_opt", "type_spec", "name_tok",
  "qname_prefix", "qualified_type", "qualified_id_expr", "var_decl",
  "more_plain_declarators", "func_ptr_param_type", "func_ptr_param_list",
  "opt_func_ptr_param_list", "array_bracket_list", "array_dim",
  "braced_init", "init_items", "init_item", "opt_array_initializer",
  "opt_initializer", "tag_typedef_decl", "$@7", "typedef_decl",
  "enum_decl", "enumerator_list", "enumerator", "union_decl",
  "union_member_list", "block", "$@8", "stmt_list", "stmt", "$@9",
  "for_init", "comma_expr", "comma_expr_opt", "switch_body",
  "primary_expr", "postfix_expr", "unary_expr", "cpp_cast_kw", "expr",
  "opt_arg_list", "arg_list", "string_seq", "asm_string_list",
  "opt_member_init_list", "member_init_list", "member_init", YY_NULLPTR
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
#line 526 "src/parser.y"
        {
            g_program = ast_new(AST_PROGRAM, 1);
            g_program->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node) = g_program;
        }
#line 2629 "src/parser.c"
    break;

  case 3: /* top_decl_list: %empty  */
#line 534 "src/parser.y"
                                { ((*yyvalp).list) = ast_list_new(); }
#line 2635 "src/parser.c"
    break;

  case 4: /* top_decl_list: top_decl_list top_decl  */
#line 535 "src/parser.y"
                                { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 2641 "src/parser.c"
    break;

  case 5: /* top_decl: namespace_decl  */
#line 539 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2647 "src/parser.c"
    break;

  case 6: /* top_decl: class_decl ';'  */
#line 540 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2653 "src/parser.c"
    break;

  case 7: /* top_decl: enum_decl ';'  */
#line 541 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2659 "src/parser.c"
    break;

  case 8: /* top_decl: union_decl ';'  */
#line 542 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2665 "src/parser.c"
    break;

  case 9: /* top_decl: func_def  */
#line 543 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2671 "src/parser.c"
    break;

  case 10: /* top_decl: func_decl ';'  */
#line 544 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2677 "src/parser.c"
    break;

  case 11: /* top_decl: var_decl ';'  */
#line 545 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2683 "src/parser.c"
    break;

  case 12: /* top_decl: typedef_decl ';'  */
#line 546 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2689 "src/parser.c"
    break;

  case 13: /* top_decl: tag_typedef_decl ';'  */
#line 547 "src/parser.y"
                           { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2695 "src/parser.c"
    break;

  case 14: /* top_decl: class_or_struct_kw name_tok ';'  */
#line 549 "src/parser.y"
        {
            /* Forward declaration, `struct Actor;` -- only has to make
             * the name usable as a type before its definition. Emits
             * nothing: emit_forward_declarations already writes
             * `struct X;` for every class this file defines. */
            declare_type_name((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_CLASS);
            ((*yyvalp).node) = decl_group_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 2708 "src/parser.c"
    break;

  case 15: /* top_decl: UNION name_tok ';'  */
#line 558 "src/parser.y"
        {
            declare_type_name((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_UNION);
            ((*yyvalp).node) = decl_group_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 2717 "src/parser.c"
    break;

  case 16: /* top_decl: native_decl ';'  */
#line 562 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2723 "src/parser.c"
    break;

  case 17: /* top_decl: out_of_line_def  */
#line 563 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2729 "src/parser.c"
    break;

  case 18: /* native_decl: NATIVE IDENTIFIER  */
#line 584 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_TYPEDEF);
            ((*yyvalp).node) = ast_new(AST_NATIVE_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
        }
#line 2739 "src/parser.c"
    break;

  case 19: /* native_decl: NATIVE TYPE_NAME  */
#line 590 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_NATIVE_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
        }
#line 2748 "src/parser.c"
    break;

  case 20: /* $@1: %empty  */
#line 600 "src/parser.y"
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
#line 2778 "src/parser.c"
    break;

  case 21: /* namespace_decl: NAMESPACE IDENTIFIER $@1 '{' top_decl_list '}'  */
#line 626 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_NAMESPACE_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2789 "src/parser.c"
    break;

  case 22: /* $@2: %empty  */
#line 638 "src/parser.y"
        {
            /* Register the class *before* the body is scanned, so that
             * self-referential members (`Node *next;`) and constructor/
             * destructor declarations -- which re-mention the class's own
             * name, now classified as TYPE_NAME by the lexer -- resolve
             * correctly. See driver.h for the single-class-at-a-time
             * caveat (no nested classes yet). */
            g_current_class_sym = symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_CLASS);
            symtab_push_scope(g_symtab, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), 1);
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
            Symbol *injected = symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_CLASS);
            injected->inner_scope = g_symtab->current;
        }
#line 2826 "src/parser.c"
    break;

  case 23: /* class_decl: class_or_struct_kw name_tok opt_base $@2 class_body  */
#line 671 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_CLASS_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->str2 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node) ? strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node)->str1) : NULL;
            /* Inheritance access-specifier (public/private/protected),
             * carried on $3 (see opt_base) -- meaningless when $3/str2 is
             * NULL (no base at all), but ACC_PUBLIC is a harmless inert
             * default for that case rather than leaving it uninitialized. */
            ((*yyvalp).node)->access = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node) ? (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node)->access : ACC_PUBLIC;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival); /* is_struct -- see class_or_struct_kw below and
                AST_CLASS_DECL's own doc comment in ast.h for what this
                controls (only the default member-access level; every
                other piece of this project's class machinery applies
                identically either way) */
            g_current_class_sym = NULL;
        }
#line 2849 "src/parser.c"
    break;

  case 24: /* class_body: '{' member_list '}'  */
#line 698 "src/parser.y"
                           { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); }
#line 2855 "src/parser.c"
    break;

  case 25: /* class_or_struct_kw: CLASS  */
#line 709 "src/parser.y"
              { ((*yyvalp).ival) = 0; }
#line 2861 "src/parser.c"
    break;

  case 26: /* class_or_struct_kw: STRUCT  */
#line 710 "src/parser.y"
              { ((*yyvalp).ival) = 1; }
#line 2867 "src/parser.c"
    break;

  case 27: /* opt_base: %empty  */
#line 714 "src/parser.y"
                                 { ((*yyvalp).node) = NULL; }
#line 2873 "src/parser.c"
    break;

  case 28: /* opt_base: ':' PUBLIC TYPE_NAME  */
#line 716 "src/parser.y"
        {
            /* Represented as a plain AST_IDENT carrying the base name in
             * str1 and the inheritance access-specifier in ->access --
             * reusing ast_ident() rather than adding a new semantic-value
             * type just to pair a string with an enum. class_decl's
             * action below unpacks both fields. */
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PUBLIC;
        }
#line 2887 "src/parser.c"
    break;

  case 29: /* opt_base: ':' PRIVATE TYPE_NAME  */
#line 726 "src/parser.y"
        {
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PRIVATE;
        }
#line 2896 "src/parser.c"
    break;

  case 30: /* opt_base: ':' PROTECTED TYPE_NAME  */
#line 731 "src/parser.y"
        {
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PROTECTED;
        }
#line 2905 "src/parser.c"
    break;

  case 31: /* member_list: %empty  */
#line 738 "src/parser.y"
                           { ((*yyvalp).list) = ast_list_new(); }
#line 2911 "src/parser.c"
    break;

  case 32: /* member_list: member_list member  */
#line 739 "src/parser.y"
                           { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 2917 "src/parser.c"
    break;

  case 33: /* member: access_spec ':'  */
#line 744 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_ACCESS_SPEC, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.access);
        }
#line 2926 "src/parser.c"
    break;

  case 34: /* member: func_decl ';'  */
#line 748 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2932 "src/parser.c"
    break;

  case 35: /* member: func_def  */
#line 749 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2938 "src/parser.c"
    break;

  case 36: /* member: var_decl ';'  */
#line 750 "src/parser.y"
                       { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2944 "src/parser.c"
    break;

  case 37: /* member: var_decl ':' expr ';'  */
#line 752 "src/parser.y"
        {
            /* A bit-field. Recognised only to say so plainly: Vircon32 C
             * has none, and silently widening `int level : 6;` to a full
             * word would change both the struct's layout and what happens
             * when a value doesn't fit. */
            fprintf(stderr, "%s:%d: error: bit-fields are not supported (Vircon32 C "
                    "has none) -- declare the member as a plain int and mask it "
                    "where it is written\n", g_current_filename, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            g_parse_errors++;
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
        }
#line 2960 "src/parser.c"
    break;

  case 38: /* member: FRIEND class_or_struct_kw name_tok ';'  */
#line 764 "src/parser.y"
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
#line 2995 "src/parser.c"
    break;

  case 39: /* member: FRIEND func_header ';'  */
#line 795 "src/parser.y"
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
#line 3024 "src/parser.c"
    break;

  case 40: /* access_spec: PUBLIC  */
#line 822 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PUBLIC; }
#line 3030 "src/parser.c"
    break;

  case 41: /* access_spec: PRIVATE  */
#line 823 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PRIVATE; }
#line 3036 "src/parser.c"
    break;

  case 42: /* access_spec: PROTECTED  */
#line 824 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PROTECTED; }
#line 3042 "src/parser.c"
    break;

  case 43: /* opt_virtual: %empty  */
#line 839 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 3048 "src/parser.c"
    break;

  case 44: /* opt_virtual: VIRTUAL  */
#line 840 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 3054 "src/parser.c"
    break;

  case 45: /* opt_const: %empty  */
#line 854 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 3060 "src/parser.c"
    break;

  case 46: /* opt_const: CONST  */
#line 855 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 3066 "src/parser.c"
    break;

  case 47: /* func_name: IDENTIFIER  */
#line 909 "src/parser.y"
                            { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 3072 "src/parser.c"
    break;

  case 48: /* func_name: OPERATOR operator_symbol  */
#line 910 "src/parser.y"
                                 { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 3078 "src/parser.c"
    break;

  case 49: /* operator_symbol: '+'  */
#line 914 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator+"); }
#line 3084 "src/parser.c"
    break;

  case 50: /* operator_symbol: '-'  */
#line 915 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator-"); }
#line 3090 "src/parser.c"
    break;

  case 51: /* operator_symbol: '*'  */
#line 916 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator*"); }
#line 3096 "src/parser.c"
    break;

  case 52: /* operator_symbol: '/'  */
#line 917 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator/"); }
#line 3102 "src/parser.c"
    break;

  case 53: /* operator_symbol: '='  */
#line 918 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator="); }
#line 3108 "src/parser.c"
    break;

  case 54: /* operator_symbol: '!'  */
#line 919 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator!"); }
#line 3114 "src/parser.c"
    break;

  case 55: /* operator_symbol: EQ  */
#line 920 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator=="); }
#line 3120 "src/parser.c"
    break;

  case 56: /* operator_symbol: NE  */
#line 921 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator!="); }
#line 3126 "src/parser.c"
    break;

  case 57: /* operator_symbol: '<'  */
#line 922 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator<"); }
#line 3132 "src/parser.c"
    break;

  case 58: /* operator_symbol: '>'  */
#line 923 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator>"); }
#line 3138 "src/parser.c"
    break;

  case 59: /* operator_symbol: LE  */
#line 924 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator<="); }
#line 3144 "src/parser.c"
    break;

  case 60: /* operator_symbol: GE  */
#line 925 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator>="); }
#line 3150 "src/parser.c"
    break;

  case 61: /* operator_symbol: PLUSEQ  */
#line 926 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator+="); }
#line 3156 "src/parser.c"
    break;

  case 62: /* operator_symbol: MINUSEQ  */
#line 927 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator-="); }
#line 3162 "src/parser.c"
    break;

  case 63: /* operator_symbol: STAREQ  */
#line 928 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator*="); }
#line 3168 "src/parser.c"
    break;

  case 64: /* operator_symbol: SLASHEQ  */
#line 929 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator/="); }
#line 3174 "src/parser.c"
    break;

  case 65: /* operator_symbol: '[' ']'  */
#line 930 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator[]"); }
#line 3180 "src/parser.c"
    break;

  case 66: /* operator_symbol: '(' ')'  */
#line 931 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator()"); }
#line 3186 "src/parser.c"
    break;

  case 67: /* $@3: %empty  */
#line 935 "src/parser.y"
                                          { symtab_push_scope(g_symtab, NULL, 0); }
#line 3192 "src/parser.c"
    break;

  case 68: /* func_header: type_spec pointer_opt func_name '(' $@3 opt_param_list ')' opt_const  */
#line 936 "src/parser.y"
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
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node); /* pointer_opt lets a function's return type be
                              a pointer or reference to T -- see
                              docs/VIRCON32_QUIRKS.md entry #12 for why
                              this was missing and what closing it
                              required on the lowering side. */
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->str2 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival) ? strdup("const") : NULL; /* see AST_FUNC_DECL's
                own doc comment in ast.h for this field's meaning here --
                str2 is otherwise completely unused across this whole
                func_decl/func_def/out_of_line_def family, confirmed
                directly before repurposing it, not assumed. */
        }
#line 3224 "src/parser.c"
    break;

  case 69: /* $@4: %empty  */
#line 963 "src/parser.y"
                    { symtab_push_scope(g_symtab, NULL, 0); }
#line 3230 "src/parser.c"
    break;

  case 70: /* func_header: TYPE_NAME '(' $@4 opt_param_list ')'  */
#line 964 "src/parser.y"
        {
            /* Constructor: the name token is TYPE_NAME because it's the
             * enclosing class's own (already-registered) name -- see
             * class_decl above. No return type. */
            ((*yyvalp).node) = ast_new(AST_FUNC_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = NULL;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 3244 "src/parser.c"
    break;

  case 71: /* func_header: '~' TYPE_NAME '(' ')'  */
#line 974 "src/parser.y"
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
#line 3259 "src/parser.c"
    break;

  case 72: /* func_decl: opt_virtual func_header  */
#line 988 "src/parser.y"
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
#line 3277 "src/parser.c"
    break;

  case 73: /* func_def: opt_virtual func_header opt_member_init_list block  */
#line 1005 "src/parser.y"
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
#line 3301 "src/parser.c"
    break;

  case 74: /* $@5: %empty  */
#line 1060 "src/parser.y"
                                                     { symtab_push_scope(g_symtab, NULL, 0); }
#line 3307 "src/parser.c"
    break;

  case 75: /* out_of_line_def: type_spec pointer_opt qname_prefix func_name '(' $@5 opt_param_list ')' opt_const block  */
#line 1061 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_FUNC_DEF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node); /* see func_header's identical pointer_opt
                              handling above -- out-of-line definitions need
                              the same pointer/reference return-type
                              support */
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->str2 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) ? strdup("const") : NULL; /* see func_header's
                own identical assignment above, and AST_FUNC_DECL's doc
                comment in ast.h, for this field's meaning */
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->b->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.list);
            symtab_pop_scope(g_symtab);
        }
#line 3330 "src/parser.c"
    break;

  case 76: /* $@6: %empty  */
#line 1079 "src/parser.y"
                         { symtab_push_scope(g_symtab, NULL, 0); }
#line 3336 "src/parser.c"
    break;

  case 77: /* out_of_line_def: qualified_type '(' $@6 opt_param_list ')' opt_member_init_list block  */
#line 1080 "src/parser.y"
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
#line 3390 "src/parser.c"
    break;

  case 78: /* out_of_line_def: qname_prefix '~' TYPE_NAME '(' ')' block  */
#line 1130 "src/parser.y"
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
#line 3410 "src/parser.c"
    break;

  case 79: /* opt_param_list: %empty  */
#line 1148 "src/parser.y"
                    { ((*yyvalp).list) = ast_list_new(); }
#line 3416 "src/parser.c"
    break;

  case 80: /* opt_param_list: VOID_KW  */
#line 1149 "src/parser.y"
                     { ((*yyvalp).list) = ast_list_new(); /* `(void)` -- real C's own "no parameters" spelling, same fix as opt_func_ptr_param_list's own VOID_KW alternative. A genuine, PRE-EXISTING gap, unrelated to function pointers -- found only because a function-pointer test happened to also declare an ordinary function using this spelling. Unambiguous against param_list's own first alternative: a bare VOID_KW with nothing following only ever matches here, since param itself always requires a name after its own type. */ }
#line 3422 "src/parser.c"
    break;

  case 81: /* opt_param_list: param_list  */
#line 1150 "src/parser.y"
                     { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 3428 "src/parser.c"
    break;

  case 82: /* param_list: param  */
#line 1154 "src/parser.y"
                               { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 3434 "src/parser.c"
    break;

  case 83: /* param_list: param_list ',' param  */
#line 1155 "src/parser.y"
                               { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 3440 "src/parser.c"
    break;

  case 84: /* param: type_spec pointer_opt IDENTIFIER  */
#line 1160 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 3453 "src/parser.c"
    break;

  case 85: /* param: type_spec pointer_opt IDENTIFIER '=' expr  */
#line 1169 "src/parser.y"
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
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 3489 "src/parser.c"
    break;

  case 86: /* param: type_spec pointer_opt IDENTIFIER '[' ']'  */
#line 1201 "src/parser.y"
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
            AstNode *base = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                          : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                          : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->type = ast_wrap_pointer(base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 3513 "src/parser.c"
    break;

  case 87: /* param: type_spec pointer_opt IDENTIFIER '[' array_dim ']'  */
#line 1221 "src/parser.y"
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
            AstNode *base = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line)
                          : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line)
                          : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->type = ast_wrap_pointer(base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
        }
#line 3534 "src/parser.c"
    break;

  case 88: /* pointer_opt: %empty  */
#line 1240 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 3540 "src/parser.c"
    break;

  case 89: /* pointer_opt: '*'  */
#line 1241 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 3546 "src/parser.c"
    break;

  case 90: /* pointer_opt: '&'  */
#line 1242 "src/parser.y"
                   { ((*yyvalp).ival) = 2; }
#line 3552 "src/parser.c"
    break;

  case 91: /* pointer_opt: '*' CONST  */
#line 1243 "src/parser.y"
                   { ((*yyvalp).ival) = 1; /* `T* const p` -- a const POINTER (as opposed
                                to pointer-to-const, `const T*`). Accepted
                                and emitted as a plain `T*`: the qualifier
                                only forbids reseating p, which nothing in
                                this project enforces for any const, and
                                keeping it would only feed Vircon32 C's
                                const strictness (lower.c, phase 11). */ }
#line 3564 "src/parser.c"
    break;

  case 92: /* type_spec: INT_KW  */
#line 1255 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("int", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3570 "src/parser.c"
    break;

  case 93: /* type_spec: FLOAT_KW  */
#line 1256 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("float", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3576 "src/parser.c"
    break;

  case 94: /* type_spec: VOID_KW  */
#line 1257 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("void", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3582 "src/parser.c"
    break;

  case 95: /* type_spec: BOOL_KW  */
#line 1258 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("bool", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3588 "src/parser.c"
    break;

  case 96: /* type_spec: CHAR_KW  */
#line 1259 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("char", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3594 "src/parser.c"
    break;

  case 97: /* type_spec: TYPE_NAME  */
#line 1260 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3600 "src/parser.c"
    break;

  case 98: /* type_spec: class_or_struct_kw name_tok  */
#line 1261 "src/parser.y"
                                   { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3606 "src/parser.c"
    break;

  case 99: /* type_spec: UNION name_tok  */
#line 1262 "src/parser.y"
                                   { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3612 "src/parser.c"
    break;

  case 100: /* type_spec: ENUM name_tok  */
#line 1263 "src/parser.y"
                                   { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 3618 "src/parser.c"
    break;

  case 101: /* type_spec: qualified_type  */
#line 1264 "src/parser.y"
                     { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3624 "src/parser.c"
    break;

  case 102: /* type_spec: CONST type_spec  */
#line 1266 "src/parser.y"
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
#line 3657 "src/parser.c"
    break;

  case 103: /* name_tok: IDENTIFIER  */
#line 1302 "src/parser.y"
                  { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 3663 "src/parser.c"
    break;

  case 104: /* name_tok: TYPE_NAME  */
#line 1303 "src/parser.y"
                  { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 3669 "src/parser.c"
    break;

  case 105: /* qname_prefix: name_tok COLONCOLON  */
#line 1308 "src/parser.y"
        {
            ((*yyvalp).list) = ast_list_new();
            ast_list_append(&((*yyvalp).list), ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line));
        }
#line 3678 "src/parser.c"
    break;

  case 106: /* qname_prefix: qname_prefix name_tok COLONCOLON  */
#line 1313 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).list), ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line));
        }
#line 3687 "src/parser.c"
    break;

  case 107: /* qualified_type: qname_prefix TYPE_NAME  */
#line 1321 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).node)->list, ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line));
        }
#line 3697 "src/parser.c"
    break;

  case 108: /* qualified_id_expr: qname_prefix IDENTIFIER  */
#line 1330 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).node)->list, ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line));
        }
#line 3707 "src/parser.c"
    break;

  case 109: /* var_decl: type_spec pointer_opt IDENTIFIER opt_initializer more_plain_declarators  */
#line 1341 "src/parser.y"
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
            first->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                        : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                        : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node);
            first->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);

            ((*yyvalp).node) = finish_declarators(first, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 3747 "src/parser.c"
    break;

  case 110: /* var_decl: type_spec IDENTIFIER '(' arg_list ')'  */
#line 1377 "src/parser.y"
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
#line 3811 "src/parser.c"
    break;

  case 111: /* var_decl: type_spec pointer_opt IDENTIFIER array_bracket_list opt_array_initializer more_plain_declarators  */
#line 1437 "src/parser.y"
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
            AstNode *base = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line)
                          : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line)
                          : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node);
            size_unsized_array((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            first->type = ast_wrap_array_dims(base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            first->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            /* `int b[ 4 ], a;` -- further declarators after an array one */
            ((*yyvalp).node) = finish_declarators(first, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
        }
#line 3836 "src/parser.c"
    break;

  case 112: /* var_decl: type_spec array_bracket_list IDENTIFIER opt_array_initializer  */
#line 1458 "src/parser.y"
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
#line 3866 "src/parser.c"
    break;

  case 113: /* var_decl: type_spec pointer_opt '(' '*' IDENTIFIER ')' '(' opt_func_ptr_param_list ')' opt_initializer  */
#line 1484 "src/parser.y"
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
            AstNode *ret = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line)
                         : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line)
                         : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 3898 "src/parser.c"
    break;

  case 114: /* var_decl: type_spec pointer_opt '(' opt_func_ptr_param_list ')' '*' IDENTIFIER opt_initializer  */
#line 1512 "src/parser.y"
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
            AstNode *ret = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line)
                         : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line)
                         : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 3920 "src/parser.c"
    break;

  case 115: /* var_decl: type_spec pointer_opt '(' '*' IDENTIFIER '[' array_dim ']' ')' '(' opt_func_ptr_param_list ')' opt_array_initializer  */
#line 1530 "src/parser.y"
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
            AstNode *ret = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-11)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line)
                         : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-11)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line)
                         : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yysemantics.yyval.node);
            AstNode *fp = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_array(fp, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node)->ival, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node)->a;
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 3946 "src/parser.c"
    break;

  case 116: /* var_decl: type_spec pointer_opt '(' opt_func_ptr_param_list ')' '*' '[' array_dim ']' IDENTIFIER opt_array_initializer  */
#line 1552 "src/parser.y"
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
            AstNode *ret = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line)
                         : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line)
                         : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yysemantics.yyval.node);
            AstNode *fp = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type = ast_wrap_array(fp, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->ival, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
            ((*yyvalp).node)->type->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node)->a;
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 3972 "src/parser.c"
    break;

  case 117: /* more_plain_declarators: %empty  */
#line 1594 "src/parser.y"
        { ((*yyvalp).list) = ast_list_new(); }
#line 3978 "src/parser.c"
    break;

  case 118: /* more_plain_declarators: more_plain_declarators ',' pointer_opt IDENTIFIER opt_initializer  */
#line 1596 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.list);
            AstNode *spec = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            spec->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str));
            spec->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival);
            spec->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ast_list_append(&((*yyvalp).list), spec);
        }
#line 3991 "src/parser.c"
    break;

  case 119: /* more_plain_declarators: more_plain_declarators ',' pointer_opt IDENTIFIER array_bracket_list opt_array_initializer  */
#line 1605 "src/parser.y"
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
#line 4008 "src/parser.c"
    break;

  case 120: /* func_ptr_param_type: type_spec pointer_opt  */
#line 1629 "src/parser.y"
        {
            ((*yyvalp).node) = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line)
               : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line)
               : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 4018 "src/parser.c"
    break;

  case 121: /* func_ptr_param_type: type_spec pointer_opt name_tok  */
#line 1635 "src/parser.y"
        {
            ((*yyvalp).node) = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
               : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
               : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 4028 "src/parser.c"
    break;

  case 122: /* func_ptr_param_list: func_ptr_param_type  */
#line 1644 "src/parser.y"
        { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4034 "src/parser.c"
    break;

  case 123: /* func_ptr_param_list: func_ptr_param_list ',' func_ptr_param_type  */
#line 1646 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4040 "src/parser.c"
    break;

  case 124: /* opt_func_ptr_param_list: %empty  */
#line 1650 "src/parser.y"
                           { ((*yyvalp).list) = ast_list_new(); }
#line 4046 "src/parser.c"
    break;

  case 125: /* opt_func_ptr_param_list: func_ptr_param_list  */
#line 1651 "src/parser.y"
                            { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 4052 "src/parser.c"
    break;

  case 126: /* array_bracket_list: '[' array_dim ']'  */
#line 1688 "src/parser.y"
        {
            ((*yyvalp).list) = ast_list_new();
            ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node));
        }
#line 4061 "src/parser.c"
    break;

  case 127: /* array_bracket_list: '[' ']'  */
#line 1693 "src/parser.y"
        {
            /* `int t[] = { 10, 20, 30 };` -- length left for the
             * initializer to decide; ival -1 until size_unsized_array
             * fills it in (Vircon32 C itself has no `int[] t`). */
            AstNode *dim = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            dim->ival = -1;
            ((*yyvalp).list) = ast_list_new();
            ast_list_append(&((*yyvalp).list), dim);
        }
#line 4075 "src/parser.c"
    break;

  case 128: /* array_bracket_list: array_bracket_list '[' array_dim ']'  */
#line 1703 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node));
        }
#line 4084 "src/parser.c"
    break;

  case 129: /* array_dim: expr  */
#line 1721 "src/parser.y"
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
#line 4107 "src/parser.c"
    break;

  case 130: /* braced_init: '{' '}'  */
#line 1748 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 4113 "src/parser.c"
    break;

  case 131: /* braced_init: '{' init_items '}'  */
#line 1750 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); }
#line 4119 "src/parser.c"
    break;

  case 132: /* braced_init: '{' init_items ',' '}'  */
#line 1752 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); }
#line 4125 "src/parser.c"
    break;

  case 133: /* init_items: init_item  */
#line 1756 "src/parser.y"
                                 { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4131 "src/parser.c"
    break;

  case 134: /* init_items: init_items ',' init_item  */
#line 1757 "src/parser.y"
                                 { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4137 "src/parser.c"
    break;

  case 135: /* init_item: expr  */
#line 1761 "src/parser.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4143 "src/parser.c"
    break;

  case 136: /* init_item: braced_init  */
#line 1762 "src/parser.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4149 "src/parser.c"
    break;

  case 137: /* opt_array_initializer: %empty  */
#line 1766 "src/parser.y"
                                      { ((*yyvalp).node) = NULL; }
#line 4155 "src/parser.c"
    break;

  case 138: /* opt_array_initializer: '=' braced_init  */
#line 1767 "src/parser.y"
                              { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4161 "src/parser.c"
    break;

  case 139: /* opt_array_initializer: '=' string_seq  */
#line 1769 "src/parser.y"
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
#line 4182 "src/parser.c"
    break;

  case 140: /* opt_initializer: %empty  */
#line 1790 "src/parser.y"
                     { ((*yyvalp).node) = NULL; }
#line 4188 "src/parser.c"
    break;

  case 141: /* opt_initializer: '=' expr  */
#line 1791 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4194 "src/parser.c"
    break;

  case 142: /* opt_initializer: '=' braced_init  */
#line 1792 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); /* `Point2 p = { 1, 2 };` -- a struct
                                    (or any aggregate) initialized positionally */ }
#line 4201 "src/parser.c"
    break;

  case 143: /* tag_typedef_decl: TYPEDEF class_decl pointer_opt name_tok  */
#line 1802 "src/parser.y"
        { ((*yyvalp).node) = tag_typedef_group((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4207 "src/parser.c"
    break;

  case 144: /* tag_typedef_decl: TYPEDEF union_decl pointer_opt name_tok  */
#line 1804 "src/parser.y"
        { ((*yyvalp).node) = tag_typedef_group((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4213 "src/parser.c"
    break;

  case 145: /* tag_typedef_decl: TYPEDEF enum_decl pointer_opt name_tok  */
#line 1806 "src/parser.y"
        { ((*yyvalp).node) = tag_typedef_group((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4219 "src/parser.c"
    break;

  case 146: /* $@7: %empty  */
#line 1808 "src/parser.y"
        {
            /* Anonymous: the typedef name (not seen yet) becomes the
             * struct's own name. Nothing inside the body can mention a
             * name it doesn't have, so a placeholder scope owner is
             * enough while the members are parsed. */
            g_current_class_sym = NULL;
            symtab_push_scope(g_symtab, "__anonymous", 1);
        }
#line 4232 "src/parser.c"
    break;

  case 147: /* tag_typedef_decl: TYPEDEF class_or_struct_kw $@7 class_body name_tok  */
#line 1817 "src/parser.y"
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
#line 4249 "src/parser.c"
    break;

  case 148: /* tag_typedef_decl: TYPEDEF UNION '{' union_member_list '}' name_tok  */
#line 1830 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_UNION);
            ((*yyvalp).node) = ast_new(AST_UNION_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 4260 "src/parser.c"
    break;

  case 149: /* tag_typedef_decl: TYPEDEF ENUM '{' enumerator_list '}' name_tok  */
#line 1837 "src/parser.y"
        {
            parse_record_enum_values(&(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list));
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_ENUM);
            ((*yyvalp).node) = ast_new(AST_ENUM_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 4272 "src/parser.c"
    break;

  case 150: /* typedef_decl: TYPEDEF type_spec pointer_opt name_tok  */
#line 1848 "src/parser.y"
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
                ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                         : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                         : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
            }
        }
#line 4298 "src/parser.c"
    break;

  case 151: /* typedef_decl: TYPEDEF type_spec pointer_opt '(' '*' IDENTIFIER ')' '(' opt_func_ptr_param_list ')'  */
#line 1870 "src/parser.y"
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
            AstNode *ret = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line)
                         : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line)
                         : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line);
        }
#line 4332 "src/parser.c"
    break;

  case 152: /* typedef_decl: TYPEDEF type_spec pointer_opt '(' opt_func_ptr_param_list ')' '*' IDENTIFIER  */
#line 1900 "src/parser.y"
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
            AstNode *ret = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line)
                         : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line)
                         : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->type = ast_wrap_func_ptr(ret, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
        }
#line 4354 "src/parser.c"
    break;

  case 153: /* enum_decl: ENUM name_tok '{' enumerator_list '}'  */
#line 1933 "src/parser.y"
        {
            parse_record_enum_values(&(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list));
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_ENUM);
            ((*yyvalp).node) = ast_new(AST_ENUM_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 4366 "src/parser.c"
    break;

  case 154: /* enumerator_list: enumerator  */
#line 1943 "src/parser.y"
                                        { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4372 "src/parser.c"
    break;

  case 155: /* enumerator_list: enumerator_list ',' enumerator  */
#line 1945 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4378 "src/parser.c"
    break;

  case 156: /* enumerator_list: enumerator_list ','  */
#line 1947 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); /* trailing comma -- real C++ allows one after the last enumerator */ }
#line 4384 "src/parser.c"
    break;

  case 157: /* enumerator: IDENTIFIER  */
#line 1952 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ENUM_VALUE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str)); }
#line 4390 "src/parser.c"
    break;

  case 158: /* enumerator: IDENTIFIER '=' expr  */
#line 1954 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ENUM_VALUE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.str)); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4396 "src/parser.c"
    break;

  case 159: /* union_decl: UNION name_tok '{' union_member_list '}'  */
#line 1979 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str), SYM_UNION);
            ((*yyvalp).node) = ast_new(AST_UNION_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 4407 "src/parser.c"
    break;

  case 160: /* union_member_list: %empty  */
#line 1988 "src/parser.y"
                                           { ((*yyvalp).list) = ast_list_new(); }
#line 4413 "src/parser.c"
    break;

  case 161: /* union_member_list: union_member_list var_decl ';'  */
#line 1989 "src/parser.y"
                                            { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node)); }
#line 4419 "src/parser.c"
    break;

  case 162: /* $@8: %empty  */
#line 1995 "src/parser.y"
        { symtab_push_scope(g_symtab, NULL, 0); }
#line 4425 "src/parser.c"
    break;

  case 163: /* block: '{' $@8 stmt_list '}'  */
#line 1996 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_BLOCK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 4435 "src/parser.c"
    break;

  case 164: /* stmt_list: %empty  */
#line 2004 "src/parser.y"
                         { ((*yyvalp).list) = ast_list_new(); }
#line 4441 "src/parser.c"
    break;

  case 165: /* stmt_list: stmt_list stmt  */
#line 2005 "src/parser.y"
                          { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append_flatten(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4447 "src/parser.c"
    break;

  case 166: /* stmt: block  */
#line 2009 "src/parser.y"
                                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4453 "src/parser.c"
    break;

  case 167: /* stmt: IF '(' expr ')' stmt  */
#line 2011 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_IF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = NULL;
        }
#line 4462 "src/parser.c"
    break;

  case 168: /* stmt: IF '(' expr ')' stmt ELSE stmt  */
#line 2016 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_IF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4471 "src/parser.c"
    break;

  case 169: /* stmt: WHILE '(' expr ')' stmt  */
#line 2021 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_WHILE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4480 "src/parser.c"
    break;

  case 170: /* stmt: DO stmt WHILE '(' expr ')' ';'  */
#line 2026 "src/parser.y"
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
#line 4496 "src/parser.c"
    break;

  case 171: /* stmt: SWITCH '(' expr ')' '{' switch_body '}'  */
#line 2038 "src/parser.y"
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
#line 4518 "src/parser.c"
    break;

  case 172: /* $@9: %empty  */
#line 2055 "src/parser.y"
              { symtab_push_scope(g_symtab, NULL, 0); }
#line 4524 "src/parser.c"
    break;

  case 173: /* stmt: FOR '(' $@9 for_init ';' comma_expr_opt ';' comma_expr_opt ')' stmt  */
#line 2056 "src/parser.y"
        {
            /* Own scope so a loop-local `int i` in for_init doesn't leak
             * into the enclosing block/function (and so a second, later
             * `for (int i ...)` in the same function doesn't collide with
             * it in the symbol table). */
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_FOR, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->d = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node) != NULL && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node)->kind == AST_VAR_DECL_GROUP) {
                /* `for (int i = 0, j = 5; ...)`: the declarations move
                 * into a block wrapped around the loop -- the same
                 * scope they had (nothing outside the loop sees them),
                 * and plain C in both output dialects. */
                AstNode *wrapper = ast_new(AST_BLOCK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
                wrapper->list = ast_list_new();
                ast_list_append_flatten(&wrapper->list, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node));
                ((*yyvalp).node)->a = NULL;
                ast_list_append(&wrapper->list, ((*yyvalp).node));
                ((*yyvalp).node) = wrapper;
            }
        }
#line 4550 "src/parser.c"
    break;

  case 174: /* stmt: STATIC var_decl ';'  */
#line 2078 "src/parser.y"
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
#line 4568 "src/parser.c"
    break;

  case 175: /* stmt: RETURN comma_expr_opt ';'  */
#line 2092 "src/parser.y"
        {
            /* comma_expr_opt, not expr_opt: `return a += 1, a + b;` is
             * the comma operator, which lower.c's phase 10 turns into
             * statements for Vircon32 like any other. */
            ((*yyvalp).node) = ast_new(AST_RETURN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 4580 "src/parser.c"
    break;

  case 176: /* stmt: BREAK ';'  */
#line 2100 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_BREAK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 4586 "src/parser.c"
    break;

  case 177: /* stmt: CONTINUE ';'  */
#line 2102 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_CONTINUE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 4592 "src/parser.c"
    break;

  case 178: /* stmt: GOTO IDENTIFIER ';'  */
#line 2104 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_GOTO, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str)); }
#line 4598 "src/parser.c"
    break;

  case 179: /* stmt: IDENTIFIER ':' stmt  */
#line 2106 "src/parser.y"
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
#line 4632 "src/parser.c"
    break;

  case 180: /* stmt: ASM '{' asm_string_list '}'  */
#line 2136 "src/parser.y"
        {
            /* Vircon32 C's own native form -- pure pass-through. */
            ((*yyvalp).node) = ast_new(AST_ASM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 0;  /* written in brace form */
        }
#line 4643 "src/parser.c"
    break;

  case 181: /* stmt: ASM '(' asm_string_list ')' ';'  */
#line 2143 "src/parser.y"
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
#line 4660 "src/parser.c"
    break;

  case 182: /* stmt: VOLATILE ASM '(' asm_string_list ')' ';'  */
#line 2156 "src/parser.y"
        {
            /* `asm volatile("...")` -- the qualifier only governs
             * optimization/reordering, which this transpiler performs
             * neither of, so it is accepted and dropped. */
            ((*yyvalp).node) = ast_new(AST_ASM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 1;
        }
#line 4673 "src/parser.c"
    break;

  case 183: /* stmt: ASM VOLATILE '(' asm_string_list ')' ';'  */
#line 2165 "src/parser.y"
        {
            /* __volatile__ spelled after the keyword (GCC documents
             * both orders historically; harmless to accept). */
            ((*yyvalp).node) = ast_new(AST_ASM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->ival = 1;
        }
#line 4685 "src/parser.c"
    break;

  case 184: /* stmt: ASM '(' asm_string_list ':'  */
#line 2173 "src/parser.y"
        {
            yyerror("extended asm with operand constraints is not "
                    "supported: Vircon32 C uses '{param}' interpolation "
                    "inside the literal instead; write the operands "
                    "directly in the instruction text");
            YYERROR;
        }
#line 4697 "src/parser.c"
    break;

  case 185: /* stmt: var_decl ';'  */
#line 2180 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 4703 "src/parser.c"
    break;

  case 186: /* stmt: typedef_decl ';'  */
#line 2181 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 4709 "src/parser.c"
    break;

  case 187: /* stmt: comma_expr ';'  */
#line 2183 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 4718 "src/parser.c"
    break;

  case 188: /* stmt: ';'  */
#line 2188 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4726 "src/parser.c"
    break;

  case 189: /* for_init: %empty  */
#line 2194 "src/parser.y"
                   { ((*yyvalp).node) = NULL; }
#line 4732 "src/parser.c"
    break;

  case 190: /* for_init: var_decl  */
#line 2195 "src/parser.y"
                    {
            /* A multi-declarator group (`int i = 0, j = 5`) is passed up
             * as-is: the FOR rule above wraps the loop in a block that
             * holds the declarations. */
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4743 "src/parser.c"
    break;

  case 191: /* for_init: comma_expr  */
#line 2202 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4752 "src/parser.c"
    break;

  case 192: /* comma_expr: expr  */
#line 2217 "src/parser.y"
                               { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4758 "src/parser.c"
    break;

  case 193: /* comma_expr: comma_expr ',' expr  */
#line 2219 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup(",");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 4769 "src/parser.c"
    break;

  case 194: /* comma_expr_opt: %empty  */
#line 2228 "src/parser.y"
                       { ((*yyvalp).node) = NULL; }
#line 4775 "src/parser.c"
    break;

  case 195: /* comma_expr_opt: comma_expr  */
#line 2229 "src/parser.y"
                       { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4781 "src/parser.c"
    break;

  case 196: /* switch_body: %empty  */
#line 2245 "src/parser.y"
                                     { ((*yyvalp).list) = ast_list_new(); }
#line 4787 "src/parser.c"
    break;

  case 197: /* switch_body: switch_body CASE expr ':'  */
#line 2247 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            AstNode *c = ast_new(AST_CASE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            c->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ast_list_append(&((*yyvalp).list), c);
        }
#line 4798 "src/parser.c"
    break;

  case 198: /* switch_body: switch_body DEFAULT ':'  */
#line 2254 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            AstNode *d = ast_new(AST_DEFAULT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ast_list_append(&((*yyvalp).list), d);
        }
#line 4808 "src/parser.c"
    break;

  case 199: /* switch_body: switch_body stmt  */
#line 2260 "src/parser.y"
        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 4814 "src/parser.c"
    break;

  case 200: /* primary_expr: IDENTIFIER  */
#line 2277 "src/parser.y"
                        { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4820 "src/parser.c"
    break;

  case 201: /* primary_expr: INT_LITERAL  */
#line 2278 "src/parser.y"
                          { ((*yyvalp).node) = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).ival; ((*yyvalp).node)->macro_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).macro; }
#line 4826 "src/parser.c"
    break;

  case 202: /* primary_expr: FLOAT_LITERAL  */
#line 2279 "src/parser.y"
                           { ((*yyvalp).node) = ast_new(AST_FLOAT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->fval = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).fval; ((*yyvalp).node)->macro_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).macro; }
#line 4832 "src/parser.c"
    break;

  case 203: /* primary_expr: string_seq  */
#line 2280 "src/parser.y"
                            { ((*yyvalp).node) = ast_new(AST_STRING_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 4838 "src/parser.c"
    break;

  case 204: /* primary_expr: CHAR_LITERAL  */
#line 2281 "src/parser.y"
                              { ((*yyvalp).node) = ast_new(AST_CHAR_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).ival; ((*yyvalp).node)->macro_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit).macro; }
#line 4844 "src/parser.c"
    break;

  case 205: /* primary_expr: TRUE_KW  */
#line 2282 "src/parser.y"
                               { ((*yyvalp).node) = ast_new(AST_BOOL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = 1; }
#line 4850 "src/parser.c"
    break;

  case 206: /* primary_expr: FALSE_KW  */
#line 2283 "src/parser.y"
                                { ((*yyvalp).node) = ast_new(AST_BOOL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = 0; }
#line 4856 "src/parser.c"
    break;

  case 207: /* primary_expr: NULLPTR_KW  */
#line 2284 "src/parser.y"
                                 { ((*yyvalp).node) = ast_new(AST_NULL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4862 "src/parser.c"
    break;

  case 208: /* primary_expr: THIS  */
#line 2285 "src/parser.y"
                                  { ((*yyvalp).node) = ast_new(AST_THIS, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 4868 "src/parser.c"
    break;

  case 209: /* primary_expr: qualified_id_expr  */
#line 2286 "src/parser.y"
                                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4874 "src/parser.c"
    break;

  case 210: /* primary_expr: '(' comma_expr ')'  */
#line 2287 "src/parser.y"
                                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 4880 "src/parser.c"
    break;

  case 211: /* postfix_expr: primary_expr  */
#line 2291 "src/parser.y"
                                            { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4886 "src/parser.c"
    break;

  case 212: /* postfix_expr: postfix_expr '(' opt_arg_list ')'  */
#line 2293 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_CALL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 4896 "src/parser.c"
    break;

  case 213: /* postfix_expr: postfix_expr '.' IDENTIFIER  */
#line 2299 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_MEMBER, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup(".");
            ((*yyvalp).node)->str2 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 4907 "src/parser.c"
    break;

  case 214: /* postfix_expr: postfix_expr ARROW IDENTIFIER  */
#line 2306 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_MEMBER, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("->");
            ((*yyvalp).node)->str2 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 4918 "src/parser.c"
    break;

  case 215: /* postfix_expr: postfix_expr '[' expr ']'  */
#line 2313 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_SUBSCRIPT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 4928 "src/parser.c"
    break;

  case 216: /* postfix_expr: postfix_expr INC  */
#line 2319 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("post++");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 4938 "src/parser.c"
    break;

  case 217: /* postfix_expr: postfix_expr DEC  */
#line 2325 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("post--");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 4948 "src/parser.c"
    break;

  case 218: /* unary_expr: postfix_expr  */
#line 2333 "src/parser.y"
                             { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4954 "src/parser.c"
    break;

  case 219: /* unary_expr: '!' unary_expr  */
#line 2335 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("!"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4960 "src/parser.c"
    break;

  case 220: /* unary_expr: '~' unary_expr  */
#line 2337 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("~"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4966 "src/parser.c"
    break;

  case 221: /* unary_expr: '-' unary_expr  */
#line 2339 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("neg"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4972 "src/parser.c"
    break;

  case 222: /* unary_expr: '&' unary_expr  */
#line 2341 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("addr"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4978 "src/parser.c"
    break;

  case 223: /* unary_expr: '*' unary_expr  */
#line 2343 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("deref"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4984 "src/parser.c"
    break;

  case 224: /* unary_expr: INC unary_expr  */
#line 2345 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("pre++"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4990 "src/parser.c"
    break;

  case 225: /* unary_expr: DEC unary_expr  */
#line 2347 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("pre--"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 4996 "src/parser.c"
    break;

  case 226: /* unary_expr: NEW type_spec  */
#line 2349 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_NEW, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5002 "src/parser.c"
    break;

  case 227: /* unary_expr: NEW type_spec '(' opt_arg_list ')'  */
#line 2351 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_NEW, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line); ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); }
#line 5008 "src/parser.c"
    break;

  case 228: /* unary_expr: NEW type_spec '[' expr ']'  */
#line 2353 "src/parser.y"
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
#line 5027 "src/parser.c"
    break;

  case 229: /* unary_expr: DELETE unary_expr  */
#line 2368 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_DELETE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5033 "src/parser.c"
    break;

  case 230: /* unary_expr: DELETE '[' ']' unary_expr  */
#line 2370 "src/parser.y"
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
#line 5048 "src/parser.c"
    break;

  case 231: /* unary_expr: '(' type_spec pointer_opt ')' unary_expr  */
#line 2381 "src/parser.y"
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
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 5078 "src/parser.c"
    break;

  case 232: /* unary_expr: SIZEOF '(' type_spec pointer_opt ')'  */
#line 2407 "src/parser.y"
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
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 5132 "src/parser.c"
    break;

  case 233: /* unary_expr: SIZEOF unary_expr  */
#line 2457 "src/parser.y"
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
#line 5149 "src/parser.c"
    break;

  case 234: /* unary_expr: cpp_cast_kw '<' type_spec pointer_opt '>' '(' expr ')'  */
#line 2470 "src/parser.y"
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
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->ival = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.ival) == 3) ? 1 : 0; /* 1 only for dynamic_cast */
        }
#line 5183 "src/parser.c"
    break;

  case 235: /* cpp_cast_kw: STATIC_CAST  */
#line 2510 "src/parser.y"
                         { ((*yyvalp).ival) = 0; }
#line 5189 "src/parser.c"
    break;

  case 236: /* cpp_cast_kw: CONST_CAST  */
#line 2511 "src/parser.y"
                          { ((*yyvalp).ival) = 1; }
#line 5195 "src/parser.c"
    break;

  case 237: /* cpp_cast_kw: REINTERPRET_CAST  */
#line 2512 "src/parser.y"
                           { ((*yyvalp).ival) = 2; }
#line 5201 "src/parser.c"
    break;

  case 238: /* cpp_cast_kw: DYNAMIC_CAST  */
#line 2513 "src/parser.y"
                            { ((*yyvalp).ival) = 3; }
#line 5207 "src/parser.c"
    break;

  case 239: /* expr: unary_expr  */
#line 2517 "src/parser.y"
                           { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5213 "src/parser.c"
    break;

  case 240: /* expr: expr '*' expr  */
#line 2518 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("*"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5219 "src/parser.c"
    break;

  case 241: /* expr: expr '/' expr  */
#line 2519 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("/"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5225 "src/parser.c"
    break;

  case 242: /* expr: expr '%' expr  */
#line 2520 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("%"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5231 "src/parser.c"
    break;

  case 243: /* expr: expr '+' expr  */
#line 2521 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("+"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5237 "src/parser.c"
    break;

  case 244: /* expr: expr '-' expr  */
#line 2522 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("-"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5243 "src/parser.c"
    break;

  case 245: /* expr: expr '<' expr  */
#line 2523 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5249 "src/parser.c"
    break;

  case 246: /* expr: expr '>' expr  */
#line 2524 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5255 "src/parser.c"
    break;

  case 247: /* expr: expr LE expr  */
#line 2525 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5261 "src/parser.c"
    break;

  case 248: /* expr: expr GE expr  */
#line 2526 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5267 "src/parser.c"
    break;

  case 249: /* expr: expr EQ expr  */
#line 2527 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("=="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5273 "src/parser.c"
    break;

  case 250: /* expr: expr NE expr  */
#line 2528 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("!="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5279 "src/parser.c"
    break;

  case 251: /* expr: expr ANDAND expr  */
#line 2529 "src/parser.y"
                        { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("&&"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5285 "src/parser.c"
    break;

  case 252: /* expr: expr OROR expr  */
#line 2530 "src/parser.y"
                        { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("||"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5291 "src/parser.c"
    break;

  case 253: /* expr: expr '&' expr  */
#line 2532 "src/parser.y"
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
#line 5307 "src/parser.c"
    break;

  case 254: /* expr: expr '|' expr  */
#line 2543 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("|"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5313 "src/parser.c"
    break;

  case 255: /* expr: expr '^' expr  */
#line 2544 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("^"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5319 "src/parser.c"
    break;

  case 256: /* expr: expr SHL expr  */
#line 2545 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<<"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5325 "src/parser.c"
    break;

  case 257: /* expr: expr SHR expr  */
#line 2546 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">>"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5331 "src/parser.c"
    break;

  case 258: /* expr: expr '=' expr  */
#line 2548 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5337 "src/parser.c"
    break;

  case 259: /* expr: expr PLUSEQ expr  */
#line 2550 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("+="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5343 "src/parser.c"
    break;

  case 260: /* expr: expr MINUSEQ expr  */
#line 2552 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("-="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5349 "src/parser.c"
    break;

  case 261: /* expr: expr STAREQ expr  */
#line 2554 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("*="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5355 "src/parser.c"
    break;

  case 262: /* expr: expr SLASHEQ expr  */
#line 2556 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("/="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5361 "src/parser.c"
    break;

  case 263: /* expr: expr ANDEQ expr  */
#line 2558 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("&="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5367 "src/parser.c"
    break;

  case 264: /* expr: expr OREQ expr  */
#line 2560 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("|="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5373 "src/parser.c"
    break;

  case 265: /* expr: expr MODEQ expr  */
#line 2562 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("%="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5379 "src/parser.c"
    break;

  case 266: /* expr: expr XOREQ expr  */
#line 2564 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("^="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5385 "src/parser.c"
    break;

  case 267: /* expr: expr SHLEQ expr  */
#line 2566 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<<="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5391 "src/parser.c"
    break;

  case 268: /* expr: expr SHREQ expr  */
#line 2568 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">>="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 5397 "src/parser.c"
    break;

  case 269: /* expr: expr '?' expr ':' expr  */
#line 2570 "src/parser.y"
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
#line 5426 "src/parser.c"
    break;

  case 270: /* opt_arg_list: %empty  */
#line 2597 "src/parser.y"
                   { ((*yyvalp).list) = ast_list_new(); }
#line 5432 "src/parser.c"
    break;

  case 271: /* opt_arg_list: arg_list  */
#line 2598 "src/parser.y"
                    { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 5438 "src/parser.c"
    break;

  case 272: /* arg_list: expr  */
#line 2602 "src/parser.y"
                            { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5444 "src/parser.c"
    break;

  case 273: /* arg_list: arg_list ',' expr  */
#line 2603 "src/parser.y"
                             { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5450 "src/parser.c"
    break;

  case 274: /* string_seq: STRING_LITERAL  */
#line 2614 "src/parser.y"
                                    { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 5456 "src/parser.c"
    break;

  case 275: /* string_seq: string_seq STRING_LITERAL  */
#line 2615 "src/parser.y"
                                    { ((*yyvalp).str) = join_string_literals((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str)); }
#line 5462 "src/parser.c"
    break;

  case 276: /* asm_string_list: STRING_LITERAL  */
#line 2627 "src/parser.y"
        {
            ((*yyvalp).list) = ast_list_new();
            AstNode *lit = ast_new(AST_STRING_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            lit->str1 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str);
            ast_list_append(&((*yyvalp).list), lit);
        }
#line 5473 "src/parser.c"
    break;

  case 277: /* asm_string_list: asm_string_list STRING_LITERAL  */
#line 2634 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            AstNode *lit = ast_new(AST_STRING_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            lit->str1 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str);
            ast_list_append(&((*yyvalp).list), lit);
        }
#line 5484 "src/parser.c"
    break;

  case 278: /* opt_member_init_list: %empty  */
#line 2659 "src/parser.y"
                                     { ((*yyvalp).node) = NULL; }
#line 5490 "src/parser.c"
    break;

  case 279: /* opt_member_init_list: ':' member_init_list  */
#line 2660 "src/parser.y"
                                      { ((*yyvalp).node) = ast_new(AST_MEMBER_INIT_LIST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 5496 "src/parser.c"
    break;

  case 280: /* member_init_list: member_init  */
#line 2664 "src/parser.y"
                                           { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5502 "src/parser.c"
    break;

  case 281: /* member_init_list: member_init_list ',' member_init  */
#line 2665 "src/parser.y"
                                            { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 5508 "src/parser.c"
    break;

  case 282: /* member_init: IDENTIFIER '(' opt_arg_list ')'  */
#line 2670 "src/parser.y"
        {
            /* An ordinary member field's own name -- see this section's
             * own header comment above for why this is accepted
             * syntactically despite not being acted on yet. */
            ((*yyvalp).node) = ast_new(AST_MEMBER_INIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 5521 "src/parser.c"
    break;

  case 283: /* member_init: TYPE_NAME '(' opt_arg_list ')'  */
#line 2679 "src/parser.y"
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
#line 5537 "src/parser.c"
    break;


#line 5541 "src/parser.c"

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




#line 2692 "src/parser.y"


void yyerror(const char *msg) {
    fprintf(stderr, "%s:%d: error: %s\n", g_current_filename, g_lex_lineno, msg);
}
