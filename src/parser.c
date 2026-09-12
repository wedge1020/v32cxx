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

#line 81 "src/parser.c"

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
  YYSYMBOL_PUBLIC = 10,                    /* PUBLIC  */
  YYSYMBOL_PRIVATE = 11,                   /* PRIVATE  */
  YYSYMBOL_PROTECTED = 12,                 /* PROTECTED  */
  YYSYMBOL_NAMESPACE = 13,                 /* NAMESPACE  */
  YYSYMBOL_TYPEDEF = 14,                   /* TYPEDEF  */
  YYSYMBOL_RETURN = 15,                    /* RETURN  */
  YYSYMBOL_IF = 16,                        /* IF  */
  YYSYMBOL_ELSE = 17,                      /* ELSE  */
  YYSYMBOL_WHILE = 18,                     /* WHILE  */
  YYSYMBOL_FOR = 19,                       /* FOR  */
  YYSYMBOL_INT_KW = 20,                    /* INT_KW  */
  YYSYMBOL_FLOAT_KW = 21,                  /* FLOAT_KW  */
  YYSYMBOL_VOID_KW = 22,                   /* VOID_KW  */
  YYSYMBOL_BOOL_KW = 23,                   /* BOOL_KW  */
  YYSYMBOL_CHAR_KW = 24,                   /* CHAR_KW  */
  YYSYMBOL_NEW = 25,                       /* NEW  */
  YYSYMBOL_DELETE = 26,                    /* DELETE  */
  YYSYMBOL_THIS = 27,                      /* THIS  */
  YYSYMBOL_VIRTUAL = 28,                   /* VIRTUAL  */
  YYSYMBOL_TRUE_KW = 29,                   /* TRUE_KW  */
  YYSYMBOL_FALSE_KW = 30,                  /* FALSE_KW  */
  YYSYMBOL_OPERATOR = 31,                  /* OPERATOR  */
  YYSYMBOL_COLONCOLON = 32,                /* COLONCOLON  */
  YYSYMBOL_ARROW = 33,                     /* ARROW  */
  YYSYMBOL_EQ = 34,                        /* EQ  */
  YYSYMBOL_NE = 35,                        /* NE  */
  YYSYMBOL_LE = 36,                        /* LE  */
  YYSYMBOL_GE = 37,                        /* GE  */
  YYSYMBOL_ANDAND = 38,                    /* ANDAND  */
  YYSYMBOL_OROR = 39,                      /* OROR  */
  YYSYMBOL_PLUSEQ = 40,                    /* PLUSEQ  */
  YYSYMBOL_MINUSEQ = 41,                   /* MINUSEQ  */
  YYSYMBOL_STAREQ = 42,                    /* STAREQ  */
  YYSYMBOL_SLASHEQ = 43,                   /* SLASHEQ  */
  YYSYMBOL_INC = 44,                       /* INC  */
  YYSYMBOL_DEC = 45,                       /* DEC  */
  YYSYMBOL_46_ = 46,                       /* '='  */
  YYSYMBOL_47_ = 47,                       /* '<'  */
  YYSYMBOL_48_ = 48,                       /* '>'  */
  YYSYMBOL_49_ = 49,                       /* '+'  */
  YYSYMBOL_50_ = 50,                       /* '-'  */
  YYSYMBOL_51_ = 51,                       /* '*'  */
  YYSYMBOL_52_ = 52,                       /* '/'  */
  YYSYMBOL_53_ = 53,                       /* '%'  */
  YYSYMBOL_LOWER_THAN_ELSE = 54,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_55_ = 55,                       /* ';'  */
  YYSYMBOL_56_ = 56,                       /* '{'  */
  YYSYMBOL_57_ = 57,                       /* '}'  */
  YYSYMBOL_58_ = 58,                       /* ':'  */
  YYSYMBOL_59_ = 59,                       /* '!'  */
  YYSYMBOL_60_ = 60,                       /* '['  */
  YYSYMBOL_61_ = 61,                       /* ']'  */
  YYSYMBOL_62_ = 62,                       /* '('  */
  YYSYMBOL_63_ = 63,                       /* ')'  */
  YYSYMBOL_64_ = 64,                       /* '~'  */
  YYSYMBOL_65_ = 65,                       /* ','  */
  YYSYMBOL_66_ = 66,                       /* '&'  */
  YYSYMBOL_67_ = 67,                       /* '.'  */
  YYSYMBOL_YYACCEPT = 68,                  /* $accept  */
  YYSYMBOL_program = 69,                   /* program  */
  YYSYMBOL_top_decl_list = 70,             /* top_decl_list  */
  YYSYMBOL_top_decl = 71,                  /* top_decl  */
  YYSYMBOL_namespace_decl = 72,            /* namespace_decl  */
  YYSYMBOL_73_1 = 73,                      /* $@1  */
  YYSYMBOL_class_decl = 74,                /* class_decl  */
  YYSYMBOL_75_2 = 75,                      /* $@2  */
  YYSYMBOL_opt_base = 76,                  /* opt_base  */
  YYSYMBOL_member_list = 77,               /* member_list  */
  YYSYMBOL_member = 78,                    /* member  */
  YYSYMBOL_access_spec = 79,               /* access_spec  */
  YYSYMBOL_opt_virtual = 80,               /* opt_virtual  */
  YYSYMBOL_func_name = 81,                 /* func_name  */
  YYSYMBOL_operator_symbol = 82,           /* operator_symbol  */
  YYSYMBOL_func_header = 83,               /* func_header  */
  YYSYMBOL_84_3 = 84,                      /* $@3  */
  YYSYMBOL_85_4 = 85,                      /* $@4  */
  YYSYMBOL_func_decl = 86,                 /* func_decl  */
  YYSYMBOL_func_def = 87,                  /* func_def  */
  YYSYMBOL_out_of_line_def = 88,           /* out_of_line_def  */
  YYSYMBOL_89_5 = 89,                      /* $@5  */
  YYSYMBOL_90_6 = 90,                      /* $@6  */
  YYSYMBOL_opt_param_list = 91,            /* opt_param_list  */
  YYSYMBOL_param_list = 92,                /* param_list  */
  YYSYMBOL_param = 93,                     /* param  */
  YYSYMBOL_pointer_opt = 94,               /* pointer_opt  */
  YYSYMBOL_type_spec = 95,                 /* type_spec  */
  YYSYMBOL_name_tok = 96,                  /* name_tok  */
  YYSYMBOL_qname_prefix = 97,              /* qname_prefix  */
  YYSYMBOL_qualified_type = 98,            /* qualified_type  */
  YYSYMBOL_qualified_id_expr = 99,         /* qualified_id_expr  */
  YYSYMBOL_var_decl = 100,                 /* var_decl  */
  YYSYMBOL_opt_initializer = 101,          /* opt_initializer  */
  YYSYMBOL_typedef_decl = 102,             /* typedef_decl  */
  YYSYMBOL_block = 103,                    /* block  */
  YYSYMBOL_104_7 = 104,                    /* $@7  */
  YYSYMBOL_stmt_list = 105,                /* stmt_list  */
  YYSYMBOL_stmt = 106,                     /* stmt  */
  YYSYMBOL_107_8 = 107,                    /* $@8  */
  YYSYMBOL_for_init = 108,                 /* for_init  */
  YYSYMBOL_expr_opt = 109,                 /* expr_opt  */
  YYSYMBOL_primary_expr = 110,             /* primary_expr  */
  YYSYMBOL_postfix_expr = 111,             /* postfix_expr  */
  YYSYMBOL_unary_expr = 112,               /* unary_expr  */
  YYSYMBOL_expr = 113,                     /* expr  */
  YYSYMBOL_opt_arg_list = 114,             /* opt_arg_list  */
  YYSYMBOL_arg_list = 115                  /* arg_list  */
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
#define YYLAST   650

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  68
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  48
/* YYNRULES -- Number of rules.  */
#define YYNRULES  157
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  273
/* YYMAXRHS -- Maximum number of symbols on right-hand side of rule.  */
#define YYMAXRHS 10
/* YYMAXLEFT -- Maximum number of symbols to the left of a handle
   accessed by $0, $-1, etc., in any rule.  */
#define YYMAXLEFT 0

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   301

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
       2,     2,     2,    59,     2,     2,     2,    53,    66,     2,
      62,    63,    51,    49,    65,    50,    67,    52,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    58,    55,
      47,    46,    48,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    60,     2,    61,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    56,     2,    57,    64,     2,     2,     2,
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
      45,    54
};

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   146,   146,   155,   156,   160,   161,   162,   163,   164,
     165,   166,   173,   172,   211,   210,   260,   261,   271,   276,
     284,   285,   289,   294,   295,   296,   300,   301,   302,   317,
     318,   366,   367,   371,   372,   373,   374,   375,   376,   377,
     378,   379,   380,   381,   382,   383,   384,   385,   386,   387,
     388,   392,   392,   409,   409,   419,   433,   450,   494,   494,
     505,   505,   553,   572,   573,   577,   578,   582,   594,   595,
     596,   602,   603,   604,   605,   606,   607,   608,   617,   618,
     622,   627,   635,   644,   655,   668,   669,   673,   687,   687,
     696,   697,   701,   702,   707,   712,   717,   717,   727,   732,
     733,   734,   739,   746,   747,   748,   756,   757,   774,   775,
     776,   777,   778,   779,   780,   781,   782,   783,   787,   788,
     794,   801,   808,   814,   820,   829,   830,   832,   834,   836,
     838,   840,   842,   844,   846,   851,   852,   853,   854,   855,
     856,   857,   858,   859,   860,   861,   862,   863,   864,   865,
     867,   869,   871,   873,   878,   879,   883,   884
};
#endif

#define YYPACT_NINF (-230)
#define YYTABLE_NINF (-80)

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -230,    11,   395,  -230,  -230,   -29,    26,    32,   102,  -230,
    -230,  -230,  -230,  -230,  -230,  -230,  -230,   -10,    10,    -6,
    -230,  -230,    15,    19,    18,    -8,    24,    30,    13,  -230,
     -14,    39,  -230,  -230,   -15,    76,    48,    25,  -230,  -230,
    -230,  -230,   108,    22,  -230,   -29,   128,   101,  -230,  -230,
    -230,    47,  -230,    79,   131,  -230,    75,  -230,  -230,  -230,
     498,    80,    90,   113,    84,    85,  -230,   102,   136,   144,
     145,    94,  -230,  -230,   102,    95,  -230,  -230,  -230,  -230,
    -230,  -230,  -230,  -230,  -230,  -230,  -230,  -230,  -230,  -230,
    -230,  -230,  -230,    99,    98,  -230,  -230,   341,  -230,  -230,
     111,   112,   127,  -230,   -14,  -230,  -230,  -230,  -230,   417,
     130,  -230,   183,  -230,  -230,   102,   113,  -230,  -230,  -230,
    -230,   102,   341,  -230,  -230,  -230,   341,   341,   341,   341,
     341,   341,   341,   341,    60,  -230,  -230,   -21,  -230,   577,
     102,    48,    48,   102,   161,   256,  -230,  -230,   341,   132,
     133,   134,  -230,  -230,   -14,    74,   156,   159,  -230,  -230,
     555,   137,  -230,  -230,  -230,  -230,  -230,  -230,  -230,   414,
    -230,  -230,   113,   212,  -230,  -230,   341,   341,   213,   341,
     341,   341,   341,   341,   341,   341,   341,   341,   341,   341,
     341,   341,   341,   341,   341,   341,   341,   154,  -230,  -230,
    -230,  -230,  -230,  -230,  -230,  -230,  -230,   162,   164,  -230,
     166,   167,   577,   341,   341,  -230,  -230,  -230,  -230,  -230,
    -230,  -230,   527,   577,   160,   165,  -230,   452,   452,    78,
      78,   597,   118,   577,   577,   577,   577,   577,    78,    78,
      16,    16,  -230,  -230,  -230,    48,  -230,  -230,  -230,  -230,
     444,   474,   331,  -230,  -230,   341,  -230,   267,   267,  -230,
     169,   577,   577,   208,  -230,   341,   267,   171,  -230,   341,
     168,   267,  -230
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,    78,    76,     0,     0,     0,    71,
      72,    73,    74,    75,    30,     4,     5,     0,     0,     0,
       7,    11,     0,     0,     0,    77,     0,     0,    16,    12,
      68,     0,    77,     6,    76,     0,    56,     0,     8,    79,
      69,    70,     0,     0,    80,    82,     0,     0,    60,     9,
      10,     0,    14,     0,     0,    53,     0,    88,    57,    31,
       0,     0,    85,    31,     0,     0,    81,    63,     0,     0,
       0,     0,     3,    87,    63,     0,    90,    39,    40,    43,
      44,    45,    46,    47,    48,    37,    41,    42,    33,    34,
      35,    36,    38,     0,     0,    32,    51,     0,    84,    58,
       0,     0,    64,    65,    68,    17,    18,    19,    20,    29,
       0,    55,     0,    49,    50,    63,   108,   111,   109,   112,
     110,     0,     0,   115,   113,   114,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   116,   118,   125,   135,    86,
      63,     0,     0,     0,     0,    29,    13,    54,   106,     0,
       0,     0,   102,    89,    68,     0,     0,     0,    92,    91,
       0,     0,   133,   134,   131,   132,   128,   130,   126,     0,
     127,   129,    83,     0,   123,   124,     0,   154,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    62,    61,
      66,    67,    26,    27,    28,    15,    21,     0,     0,    24,
       0,     0,   107,     0,     0,    96,    99,   100,   101,    52,
     117,   121,     0,   156,     0,   155,   120,   145,   146,   143,
     144,   147,   148,   150,   151,   152,   153,   149,   141,   142,
     139,   140,   136,   137,   138,     0,    22,    23,    25,    98,
       0,     0,   103,   122,   119,     0,    59,     0,     0,   104,
       0,   105,   157,    93,    95,   106,     0,     0,    94,   106,
       0,     0,    97
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -230,  -230,   157,  -230,  -230,  -230,  -230,  -230,  -230,  -230,
    -230,  -230,  -230,   189,  -230,  -230,  -230,  -230,    91,    92,
    -230,  -230,  -230,   -67,  -230,   100,   -28,    42,   -16,    -2,
       3,  -230,    -1,  -230,     8,   -27,  -230,  -230,  -196,  -230,
    -230,  -229,  -230,  -230,    51,   -93,  -230,  -230
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,    15,    16,    53,    17,    71,    52,   145,
     206,   207,    18,    61,    95,    36,   115,    74,    19,    20,
      21,   140,    67,   101,   102,   103,    42,   154,    23,   134,
      32,   135,   156,    98,   157,   158,    76,   112,   159,   252,
     260,   211,   136,   137,   138,   160,   224,   225
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      24,    26,    54,   -79,   139,    25,    31,   110,    47,    58,
      27,     3,   173,     4,    34,    47,    31,   -79,     4,    39,
      43,     4,    45,   174,   175,    63,    39,    47,    59,    28,
       9,    10,    11,    12,    13,    29,   267,    40,   169,   176,
     270,   177,     4,    45,    22,    33,   178,    55,   161,    38,
      30,    44,    41,    60,    48,   212,    60,    68,    69,    70,
      37,   263,   264,   172,    39,    31,    40,   194,   195,   196,
     268,    51,    31,   197,    35,   272,   144,   172,    45,    49,
      56,    41,    46,   222,   223,    50,   227,   228,   229,   230,
     231,   232,   233,   234,   235,   236,   237,   238,   239,   240,
     241,   242,   243,   244,    57,     4,     5,    24,    26,   104,
     155,    62,    25,    31,   198,   199,   104,    27,    47,    31,
     250,   251,     9,    10,    11,    12,    13,   192,   193,   194,
     195,   196,    65,    66,    73,    72,    97,    75,    31,    47,
     105,    31,    96,    31,   210,   -78,    99,   100,   106,   107,
     108,    22,   179,   180,   181,   182,   183,   104,   111,   261,
     113,   114,   262,   162,   201,   190,   191,   192,   193,   194,
     195,   196,   212,   163,   141,   142,   212,   164,   165,   166,
     167,   168,   104,   170,   171,   104,   116,     5,   117,   118,
     119,   120,   143,   147,   213,   214,   215,     8,   148,   149,
     219,   150,   151,     9,    10,    11,    12,    13,   121,   122,
     123,   216,   124,   125,   217,   221,   226,   245,   256,   247,
     246,   248,   249,   254,   265,   266,   269,   126,   127,   109,
     255,   271,    64,   128,   129,     0,   208,   209,   152,    57,
     153,     0,   130,   200,     0,   131,     0,   132,     0,   133,
     155,   259,     0,     0,     0,   155,   155,     0,     0,     4,
       5,     0,     0,     0,   155,     0,   202,   203,   204,   155,
     116,     5,   117,   118,   119,   120,     9,    10,    11,    12,
      13,     8,   148,   149,    14,   150,   151,     9,    10,    11,
      12,    13,   121,   122,   123,     0,   124,   125,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   126,   127,   205,     0,     0,     0,   128,   129,     0,
       0,     0,   152,    57,     0,     0,   130,     0,     0,   131,
       0,   132,     0,   133,   116,     5,   117,   118,   119,   120,
       0,     0,     0,     0,   116,    39,   117,   118,   119,   120,
       0,     9,    10,    11,    12,    13,   121,   122,   123,     0,
     124,   125,     0,     0,     0,     0,   121,   122,   123,     0,
     124,   125,     0,     0,     0,   126,   127,     0,     0,     0,
       0,   128,   129,     0,     0,   126,   127,     0,     0,     0,
     130,   128,   129,   131,     0,   132,     0,   133,     4,     5,
     130,     0,     0,   131,     6,   132,     0,   133,     7,     8,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
       4,     5,     0,    14,     0,     0,     6,     0,     0,     0,
       7,     8,     0,     0,     0,     0,     0,     9,    10,    11,
      12,    13,     0,     0,     0,    14,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,   -29,
     189,   190,   191,   192,   193,   194,   195,   196,     0,     0,
       0,     0,     0,     0,   146,     0,     0,   220,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,   181,   182,
     189,   190,   191,   192,   193,   194,   195,   196,     0,   190,
     191,   192,   193,   194,   195,   196,     0,   257,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,     0,
     189,   190,   191,   192,   193,   194,   195,   196,     0,     0,
       0,     0,    77,    78,    79,    80,     0,   258,    81,    82,
      83,    84,     0,     0,    85,    86,    87,    88,    89,    90,
      91,     0,     0,     0,     0,     0,     0,    92,    93,     0,
      94,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,     0,   189,   190,   191,   192,   193,   194,   195,
     196,     0,     0,     0,     0,     0,     0,     0,   253,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,     0,
       0,   189,   190,   191,   192,   193,   194,   195,   196,     0,
     218,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,     0,   189,   190,   191,   192,   193,   194,   195,
     196,   179,   180,   181,   182,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   190,   191,   192,   193,   194,   195,
     196
};

static const yytype_int16 yycheck[] =
{
       2,     2,    30,    32,    97,     2,     8,    74,    24,    36,
       2,     0,    33,     3,     4,    31,    18,    32,     3,     4,
      22,     3,     4,    44,    45,     3,     4,    43,     3,     3,
      20,    21,    22,    23,    24,     3,   265,    51,   131,    60,
     269,    62,     3,     4,     2,    55,    67,    62,   115,    55,
       8,    32,    66,    31,    62,   148,    31,    10,    11,    12,
      18,   257,   258,     3,     4,    67,    51,    51,    52,    53,
     266,    58,    74,   140,    64,   271,   104,     3,     4,    55,
       4,    66,    64,   176,   177,    55,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   196,    56,     3,     4,   109,   109,    67,
     112,     3,   109,   115,   141,   142,    74,   109,   134,   121,
     213,   214,    20,    21,    22,    23,    24,    49,    50,    51,
      52,    53,     4,    32,     3,    56,    46,    62,   140,   155,
       4,   143,    62,   145,   145,    32,    62,    62,     4,     4,
      56,   109,    34,    35,    36,    37,    38,   115,    63,   252,
      61,    63,   255,   121,     3,    47,    48,    49,    50,    51,
      52,    53,   265,   122,    63,    63,   269,   126,   127,   128,
     129,   130,   140,   132,   133,   143,     3,     4,     5,     6,
       7,     8,    65,    63,    62,    62,    62,    14,    15,    16,
      63,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    55,    29,    30,    55,     3,     3,    63,   245,    55,
      58,    55,    55,    63,    55,    17,    55,    44,    45,    72,
      65,    63,    43,    50,    51,    -1,   145,   145,    55,    56,
      57,    -1,    59,   143,    -1,    62,    -1,    64,    -1,    66,
     252,   252,    -1,    -1,    -1,   257,   258,    -1,    -1,     3,
       4,    -1,    -1,    -1,   266,    -1,    10,    11,    12,   271,
       3,     4,     5,     6,     7,     8,    20,    21,    22,    23,
      24,    14,    15,    16,    28,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    -1,    29,    30,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    44,    45,    57,    -1,    -1,    -1,    50,    51,    -1,
      -1,    -1,    55,    56,    -1,    -1,    59,    -1,    -1,    62,
      -1,    64,    -1,    66,     3,     4,     5,     6,     7,     8,
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,     8,
      -1,    20,    21,    22,    23,    24,    25,    26,    27,    -1,
      29,    30,    -1,    -1,    -1,    -1,    25,    26,    27,    -1,
      29,    30,    -1,    -1,    -1,    44,    45,    -1,    -1,    -1,
      -1,    50,    51,    -1,    -1,    44,    45,    -1,    -1,    -1,
      59,    50,    51,    62,    -1,    64,    -1,    66,     3,     4,
      59,    -1,    -1,    62,     9,    64,    -1,    66,    13,    14,
      -1,    -1,    -1,    -1,    -1,    20,    21,    22,    23,    24,
       3,     4,    -1,    28,    -1,    -1,     9,    -1,    -1,    -1,
      13,    14,    -1,    -1,    -1,    -1,    -1,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    28,    -1,    -1,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    -1,    64,
      46,    47,    48,    49,    50,    51,    52,    53,    -1,    -1,
      -1,    -1,    -1,    -1,    57,    -1,    -1,    63,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    36,    37,
      46,    47,    48,    49,    50,    51,    52,    53,    -1,    47,
      48,    49,    50,    51,    52,    53,    -1,    63,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    -1,    -1,
      46,    47,    48,    49,    50,    51,    52,    53,    -1,    -1,
      -1,    -1,    34,    35,    36,    37,    -1,    63,    40,    41,
      42,    43,    -1,    -1,    46,    47,    48,    49,    50,    51,
      52,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    -1,
      62,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    -1,    -1,    46,    47,    48,    49,    50,    51,    52,
      53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    61,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    -1,
      -1,    46,    47,    48,    49,    50,    51,    52,    53,    -1,
      55,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    -1,    -1,    46,    47,    48,    49,    50,    51,    52,
      53,    34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    47,    48,    49,    50,    51,    52,
      53
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    69,    70,     0,     3,     4,     9,    13,    14,    20,
      21,    22,    23,    24,    28,    71,    72,    74,    80,    86,
      87,    88,    95,    96,    97,    98,   100,   102,     3,     3,
      95,    97,    98,    55,     4,    64,    83,    95,    55,     4,
      51,    66,    94,    97,    32,     4,    64,    96,    62,    55,
      55,    58,    76,    73,    94,    62,     4,    56,   103,     3,
      31,    81,     3,     3,    81,     4,    32,    90,    10,    11,
      12,    75,    56,     3,    85,    62,   104,    34,    35,    36,
      37,    40,    41,    42,    43,    46,    47,    48,    49,    50,
      51,    52,    59,    60,    62,    82,    62,    46,   101,    62,
      62,    91,    92,    93,    95,     4,     4,     4,    56,    70,
      91,    63,   105,    61,    63,    84,     3,     5,     6,     7,
       8,    25,    26,    27,    29,    30,    44,    45,    50,    51,
      59,    62,    64,    66,    97,    99,   110,   111,   112,   113,
      89,    63,    63,    65,    94,    77,    57,    63,    15,    16,
      18,    19,    55,    57,    95,    97,   100,   102,   103,   106,
     113,    91,    95,   112,   112,   112,   112,   112,   112,   113,
     112,   112,     3,    33,    44,    45,    60,    62,    67,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    46,
      47,    48,    49,    50,    51,    52,    53,    91,   103,   103,
      93,     3,    10,    11,    12,    57,    78,    79,    86,    87,
     100,   109,   113,    62,    62,    62,    55,    55,    55,    63,
      63,     3,   113,   113,   114,   115,     3,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,    63,    58,    55,    55,    55,
     113,   113,   107,    61,    63,    65,   103,    63,    63,   100,
     108,   113,   113,   106,   106,    55,    17,   109,   106,    55,
     109,    63,   106
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    68,    69,    70,    70,    71,    71,    71,    71,    71,
      71,    71,    73,    72,    75,    74,    76,    76,    76,    76,
      77,    77,    78,    78,    78,    78,    79,    79,    79,    80,
      80,    81,    81,    82,    82,    82,    82,    82,    82,    82,
      82,    82,    82,    82,    82,    82,    82,    82,    82,    82,
      82,    84,    83,    85,    83,    83,    86,    87,    89,    88,
      90,    88,    88,    91,    91,    92,    92,    93,    94,    94,
      94,    95,    95,    95,    95,    95,    95,    95,    96,    96,
      97,    97,    98,    99,   100,   101,   101,   102,   104,   103,
     105,   105,   106,   106,   106,   106,   107,   106,   106,   106,
     106,   106,   106,   108,   108,   108,   109,   109,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   111,   111,
     111,   111,   111,   111,   111,   112,   112,   112,   112,   112,
     112,   112,   112,   112,   112,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   114,   114,   115,   115
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     2,     1,     2,     2,
       2,     1,     0,     6,     0,     7,     0,     3,     3,     3,
       0,     2,     2,     2,     1,     2,     1,     1,     1,     0,
       1,     1,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     0,     6,     0,     5,     4,     2,     3,     0,     8,
       0,     6,     6,     0,     1,     1,     3,     3,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     3,     2,     2,     4,     0,     2,     4,     0,     4,
       0,     2,     1,     5,     7,     5,     0,    10,     3,     2,
       2,     2,     1,     0,     1,     1,     0,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     3,     1,     4,
       3,     3,     4,     2,     2,     1,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     1,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     0,     1,     1,     3
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
       0,     0,     0,     0,     0,     0,     0,     0
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
       0,     0,     0,     0,     0,     0,     0,     0
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
       0,     0,     0,     0,     0,     0,     0,     0
};

/* YYCONFLP[YYPACT[STATE-NUM]] -- Pointer into YYCONFL of start of
   list of conflicting reductions corresponding to action entry for
   state STATE-NUM in yytable.  0 means no conflicts.  The list in
   yyconfl is terminated by a rule number of 0.  */
static const yytype_int8 yyconflp[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    15,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    31,
      33,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    35,    37,    39,    41,
      43,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     1,     3,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     5,     7,     9,    11,    13,
      17,    19,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    21,    23,    25,
      27,    29,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
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
       0,    29,     0,    29,     0,    29,     0,    29,     0,    29,
       0,    29,     0,    29,     0,    68,     0,    29,     0,    29,
       0,    29,     0,    29,     0,    29,     0,    29,     0,    29,
       0,    29,     0,    29,     0,    29,     0,    29,     0,    29,
       0,    29,     0,    29,     0
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
  "FLOAT_LITERAL", "CLASS", "PUBLIC", "PRIVATE", "PROTECTED", "NAMESPACE",
  "TYPEDEF", "RETURN", "IF", "ELSE", "WHILE", "FOR", "INT_KW", "FLOAT_KW",
  "VOID_KW", "BOOL_KW", "CHAR_KW", "NEW", "DELETE", "THIS", "VIRTUAL",
  "TRUE_KW", "FALSE_KW", "OPERATOR", "COLONCOLON", "ARROW", "EQ", "NE",
  "LE", "GE", "ANDAND", "OROR", "PLUSEQ", "MINUSEQ", "STAREQ", "SLASHEQ",
  "INC", "DEC", "'='", "'<'", "'>'", "'+'", "'-'", "'*'", "'/'", "'%'",
  "LOWER_THAN_ELSE", "';'", "'{'", "'}'", "':'", "'!'", "'['", "']'",
  "'('", "')'", "'~'", "','", "'&'", "'.'", "$accept", "program",
  "top_decl_list", "top_decl", "namespace_decl", "$@1", "class_decl",
  "$@2", "opt_base", "member_list", "member", "access_spec", "opt_virtual",
  "func_name", "operator_symbol", "func_header", "$@3", "$@4", "func_decl",
  "func_def", "out_of_line_def", "$@5", "$@6", "opt_param_list",
  "param_list", "param", "pointer_opt", "type_spec", "name_tok",
  "qname_prefix", "qualified_type", "qualified_id_expr", "var_decl",
  "opt_initializer", "typedef_decl", "block", "$@7", "stmt_list", "stmt",
  "$@8", "for_init", "expr_opt", "primary_expr", "postfix_expr",
  "unary_expr", "expr", "opt_arg_list", "arg_list", YY_NULLPTR
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
#line 147 "src/parser.y"
        {
            g_program = ast_new(AST_PROGRAM, 1);
            g_program->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node) = g_program;
        }
#line 1644 "src/parser.c"
    break;

  case 3: /* top_decl_list: %empty  */
#line 155 "src/parser.y"
                                { ((*yyvalp).list) = ast_list_new(); }
#line 1650 "src/parser.c"
    break;

  case 4: /* top_decl_list: top_decl_list top_decl  */
#line 156 "src/parser.y"
                                { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 1656 "src/parser.c"
    break;

  case 5: /* top_decl: namespace_decl  */
#line 160 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 1662 "src/parser.c"
    break;

  case 6: /* top_decl: class_decl ';'  */
#line 161 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 1668 "src/parser.c"
    break;

  case 7: /* top_decl: func_def  */
#line 162 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 1674 "src/parser.c"
    break;

  case 8: /* top_decl: func_decl ';'  */
#line 163 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 1680 "src/parser.c"
    break;

  case 9: /* top_decl: var_decl ';'  */
#line 164 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 1686 "src/parser.c"
    break;

  case 10: /* top_decl: typedef_decl ';'  */
#line 165 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 1692 "src/parser.c"
    break;

  case 11: /* top_decl: out_of_line_def  */
#line 166 "src/parser.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 1698 "src/parser.c"
    break;

  case 12: /* $@1: %empty  */
#line 173 "src/parser.y"
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
#line 1728 "src/parser.c"
    break;

  case 13: /* namespace_decl: NAMESPACE IDENTIFIER $@1 '{' top_decl_list '}'  */
#line 199 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_NAMESPACE_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 1739 "src/parser.c"
    break;

  case 14: /* $@2: %empty  */
#line 211 "src/parser.y"
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
#line 1776 "src/parser.c"
    break;

  case 15: /* class_decl: CLASS IDENTIFIER opt_base $@2 '{' member_list '}'  */
#line 244 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_CLASS_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->str2 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node) ? strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node)->str1) : NULL;
            /* Inheritance access-specifier (public/private/protected),
             * carried on $3 (see opt_base) -- meaningless when $3/str2 is
             * NULL (no base at all), but ACC_PUBLIC is a harmless inert
             * default for that case rather than leaving it uninitialized. */
            ((*yyvalp).node)->access = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node) ? (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node)->access : ACC_PUBLIC;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            g_current_class_sym = NULL;
        }
#line 1794 "src/parser.c"
    break;

  case 16: /* opt_base: %empty  */
#line 260 "src/parser.y"
                                 { ((*yyvalp).node) = NULL; }
#line 1800 "src/parser.c"
    break;

  case 17: /* opt_base: ':' PUBLIC TYPE_NAME  */
#line 262 "src/parser.y"
        {
            /* Represented as a plain AST_IDENT carrying the base name in
             * str1 and the inheritance access-specifier in ->access --
             * reusing ast_ident() rather than adding a new semantic-value
             * type just to pair a string with an enum. class_decl's
             * action below unpacks both fields. */
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PUBLIC;
        }
#line 1814 "src/parser.c"
    break;

  case 18: /* opt_base: ':' PRIVATE TYPE_NAME  */
#line 272 "src/parser.y"
        {
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PRIVATE;
        }
#line 1823 "src/parser.c"
    break;

  case 19: /* opt_base: ':' PROTECTED TYPE_NAME  */
#line 277 "src/parser.y"
        {
            ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = ACC_PROTECTED;
        }
#line 1832 "src/parser.c"
    break;

  case 20: /* member_list: %empty  */
#line 284 "src/parser.y"
                           { ((*yyvalp).list) = ast_list_new(); }
#line 1838 "src/parser.c"
    break;

  case 21: /* member_list: member_list member  */
#line 285 "src/parser.y"
                           { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 1844 "src/parser.c"
    break;

  case 22: /* member: access_spec ':'  */
#line 290 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_ACCESS_SPEC, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->access = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.access);
        }
#line 1853 "src/parser.c"
    break;

  case 23: /* member: func_decl ';'  */
#line 294 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 1859 "src/parser.c"
    break;

  case 24: /* member: func_def  */
#line 295 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 1865 "src/parser.c"
    break;

  case 25: /* member: var_decl ';'  */
#line 296 "src/parser.y"
                       { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 1871 "src/parser.c"
    break;

  case 26: /* access_spec: PUBLIC  */
#line 300 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PUBLIC; }
#line 1877 "src/parser.c"
    break;

  case 27: /* access_spec: PRIVATE  */
#line 301 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PRIVATE; }
#line 1883 "src/parser.c"
    break;

  case 28: /* access_spec: PROTECTED  */
#line 302 "src/parser.y"
                 { ((*yyvalp).access) = ACC_PROTECTED; }
#line 1889 "src/parser.c"
    break;

  case 29: /* opt_virtual: %empty  */
#line 317 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 1895 "src/parser.c"
    break;

  case 30: /* opt_virtual: VIRTUAL  */
#line 318 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 1901 "src/parser.c"
    break;

  case 31: /* func_name: IDENTIFIER  */
#line 366 "src/parser.y"
                            { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 1907 "src/parser.c"
    break;

  case 32: /* func_name: OPERATOR operator_symbol  */
#line 367 "src/parser.y"
                                 { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 1913 "src/parser.c"
    break;

  case 33: /* operator_symbol: '+'  */
#line 371 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator+"); }
#line 1919 "src/parser.c"
    break;

  case 34: /* operator_symbol: '-'  */
#line 372 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator-"); }
#line 1925 "src/parser.c"
    break;

  case 35: /* operator_symbol: '*'  */
#line 373 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator*"); }
#line 1931 "src/parser.c"
    break;

  case 36: /* operator_symbol: '/'  */
#line 374 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator/"); }
#line 1937 "src/parser.c"
    break;

  case 37: /* operator_symbol: '='  */
#line 375 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator="); }
#line 1943 "src/parser.c"
    break;

  case 38: /* operator_symbol: '!'  */
#line 376 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator!"); }
#line 1949 "src/parser.c"
    break;

  case 39: /* operator_symbol: EQ  */
#line 377 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator=="); }
#line 1955 "src/parser.c"
    break;

  case 40: /* operator_symbol: NE  */
#line 378 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator!="); }
#line 1961 "src/parser.c"
    break;

  case 41: /* operator_symbol: '<'  */
#line 379 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator<"); }
#line 1967 "src/parser.c"
    break;

  case 42: /* operator_symbol: '>'  */
#line 380 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator>"); }
#line 1973 "src/parser.c"
    break;

  case 43: /* operator_symbol: LE  */
#line 381 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator<="); }
#line 1979 "src/parser.c"
    break;

  case 44: /* operator_symbol: GE  */
#line 382 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator>="); }
#line 1985 "src/parser.c"
    break;

  case 45: /* operator_symbol: PLUSEQ  */
#line 383 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator+="); }
#line 1991 "src/parser.c"
    break;

  case 46: /* operator_symbol: MINUSEQ  */
#line 384 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator-="); }
#line 1997 "src/parser.c"
    break;

  case 47: /* operator_symbol: STAREQ  */
#line 385 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator*="); }
#line 2003 "src/parser.c"
    break;

  case 48: /* operator_symbol: SLASHEQ  */
#line 386 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator/="); }
#line 2009 "src/parser.c"
    break;

  case 49: /* operator_symbol: '[' ']'  */
#line 387 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator[]"); }
#line 2015 "src/parser.c"
    break;

  case 50: /* operator_symbol: '(' ')'  */
#line 388 "src/parser.y"
                { ((*yyvalp).str) = strdup("operator()"); }
#line 2021 "src/parser.c"
    break;

  case 51: /* $@3: %empty  */
#line 392 "src/parser.y"
                              { symtab_push_scope(g_symtab, NULL, 0); }
#line 2027 "src/parser.c"
    break;

  case 52: /* func_header: type_spec func_name '(' $@3 opt_param_list ')'  */
#line 393 "src/parser.y"
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
            symtab_insert(g_symtab, g_symtab->current->parent, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str), SYM_FUNC);
            ((*yyvalp).node) = ast_new(AST_FUNC_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2048 "src/parser.c"
    break;

  case 53: /* $@4: %empty  */
#line 409 "src/parser.y"
                    { symtab_push_scope(g_symtab, NULL, 0); }
#line 2054 "src/parser.c"
    break;

  case 54: /* func_header: TYPE_NAME '(' $@4 opt_param_list ')'  */
#line 410 "src/parser.y"
        {
            /* Constructor: the name token is TYPE_NAME because it's the
             * enclosing class's own (already-registered) name -- see
             * class_decl above. No return type. */
            ((*yyvalp).node) = ast_new(AST_FUNC_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = NULL;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2068 "src/parser.c"
    break;

  case 55: /* func_header: '~' TYPE_NAME '(' ')'  */
#line 420 "src/parser.y"
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
#line 2083 "src/parser.c"
    break;

  case 56: /* func_decl: opt_virtual func_header  */
#line 434 "src/parser.y"
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
#line 2101 "src/parser.c"
    break;

  case 57: /* func_def: opt_virtual func_header block  */
#line 451 "src/parser.y"
        {
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->kind = AST_FUNC_DEF;
            ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival); /* see the comment in func_decl above */
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            symtab_pop_scope(g_symtab);
        }
#line 2113 "src/parser.c"
    break;

  case 58: /* $@5: %empty  */
#line 494 "src/parser.y"
                                         { symtab_push_scope(g_symtab, NULL, 0); }
#line 2119 "src/parser.c"
    break;

  case 59: /* out_of_line_def: type_spec qname_prefix func_name '(' $@5 opt_param_list ')' block  */
#line 495 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_FUNC_DEF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->b->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.list);
            symtab_pop_scope(g_symtab);
        }
#line 2134 "src/parser.c"
    break;

  case 60: /* $@6: %empty  */
#line 505 "src/parser.y"
                         { symtab_push_scope(g_symtab, NULL, 0); }
#line 2140 "src/parser.c"
    break;

  case 61: /* out_of_line_def: qualified_type '(' $@6 opt_param_list ')' block  */
#line 506 "src/parser.y"
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
            AstList *parts = &(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node)->list;
            int n = parts->count;
            ((*yyvalp).node) = ast_new(AST_FUNC_DEF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup(parts->items[n - 1]->str1);
            ((*yyvalp).node)->type = NULL;
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            for (int i = 0; i < n - 1; i++) {
                ast_list_append(&((*yyvalp).node)->b->list, parts->items[i]);
            }
            symtab_pop_scope(g_symtab);
        }
#line 2192 "src/parser.c"
    break;

  case 62: /* out_of_line_def: qname_prefix '~' TYPE_NAME '(' ')' block  */
#line 554 "src/parser.y"
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
#line 2212 "src/parser.c"
    break;

  case 63: /* opt_param_list: %empty  */
#line 572 "src/parser.y"
                    { ((*yyvalp).list) = ast_list_new(); }
#line 2218 "src/parser.c"
    break;

  case 64: /* opt_param_list: param_list  */
#line 573 "src/parser.y"
                     { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 2224 "src/parser.c"
    break;

  case 65: /* param_list: param  */
#line 577 "src/parser.y"
                               { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 2230 "src/parser.c"
    break;

  case 66: /* param_list: param_list ',' param  */
#line 578 "src/parser.y"
                               { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 2236 "src/parser.c"
    break;

  case 67: /* param: type_spec pointer_opt IDENTIFIER  */
#line 583 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_PARAM);
            ((*yyvalp).node) = ast_new(AST_PARAM, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 2249 "src/parser.c"
    break;

  case 68: /* pointer_opt: %empty  */
#line 594 "src/parser.y"
                   { ((*yyvalp).ival) = 0; }
#line 2255 "src/parser.c"
    break;

  case 69: /* pointer_opt: '*'  */
#line 595 "src/parser.y"
                   { ((*yyvalp).ival) = 1; }
#line 2261 "src/parser.c"
    break;

  case 70: /* pointer_opt: '&'  */
#line 596 "src/parser.y"
                   { ((*yyvalp).ival) = 2; }
#line 2267 "src/parser.c"
    break;

  case 71: /* type_spec: INT_KW  */
#line 602 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("int", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2273 "src/parser.c"
    break;

  case 72: /* type_spec: FLOAT_KW  */
#line 603 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("float", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2279 "src/parser.c"
    break;

  case 73: /* type_spec: VOID_KW  */
#line 604 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("void", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2285 "src/parser.c"
    break;

  case 74: /* type_spec: BOOL_KW  */
#line 605 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("bool", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2291 "src/parser.c"
    break;

  case 75: /* type_spec: CHAR_KW  */
#line 606 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident("char", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2297 "src/parser.c"
    break;

  case 76: /* type_spec: TYPE_NAME  */
#line 607 "src/parser.y"
                    { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2303 "src/parser.c"
    break;

  case 77: /* type_spec: qualified_type  */
#line 608 "src/parser.y"
                     { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2309 "src/parser.c"
    break;

  case 78: /* name_tok: IDENTIFIER  */
#line 617 "src/parser.y"
                  { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 2315 "src/parser.c"
    break;

  case 79: /* name_tok: TYPE_NAME  */
#line 618 "src/parser.y"
                  { ((*yyvalp).str) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 2321 "src/parser.c"
    break;

  case 80: /* qname_prefix: name_tok COLONCOLON  */
#line 623 "src/parser.y"
        {
            ((*yyvalp).list) = ast_list_new();
            ast_list_append(&((*yyvalp).list), ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line));
        }
#line 2330 "src/parser.c"
    break;

  case 81: /* qname_prefix: qname_prefix name_tok COLONCOLON  */
#line 628 "src/parser.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).list), ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line));
        }
#line 2339 "src/parser.c"
    break;

  case 82: /* qualified_type: qname_prefix TYPE_NAME  */
#line 636 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).node)->list, ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line));
        }
#line 2349 "src/parser.c"
    break;

  case 83: /* qualified_id_expr: qname_prefix IDENTIFIER  */
#line 645 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_QUALIFIED_ID, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
            ast_list_append(&((*yyvalp).node)->list, ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line));
        }
#line 2359 "src/parser.c"
    break;

  case 84: /* var_decl: type_spec pointer_opt IDENTIFIER opt_initializer  */
#line 656 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str), SYM_VAR);
            ((*yyvalp).node) = ast_new(AST_VAR_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2373 "src/parser.c"
    break;

  case 85: /* opt_initializer: %empty  */
#line 668 "src/parser.y"
                     { ((*yyvalp).node) = NULL; }
#line 2379 "src/parser.c"
    break;

  case 86: /* opt_initializer: '=' expr  */
#line 669 "src/parser.y"
                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2385 "src/parser.c"
    break;

  case 87: /* typedef_decl: TYPEDEF type_spec pointer_opt IDENTIFIER  */
#line 674 "src/parser.y"
        {
            symtab_insert(g_symtab, g_symtab->current, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), SYM_TYPEDEF);
            ((*yyvalp).node) = ast_new(AST_TYPEDEF_DECL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->type = ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 1) ? ast_wrap_pointer((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                     : ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.ival) == 2) ? ast_wrap_reference((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line)
                     : (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 2398 "src/parser.c"
    break;

  case 88: /* $@7: %empty  */
#line 687 "src/parser.y"
        { symtab_push_scope(g_symtab, NULL, 0); }
#line 2404 "src/parser.c"
    break;

  case 89: /* block: '{' $@7 stmt_list '}'  */
#line 688 "src/parser.y"
        {
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_BLOCK, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2414 "src/parser.c"
    break;

  case 90: /* stmt_list: %empty  */
#line 696 "src/parser.y"
                         { ((*yyvalp).list) = ast_list_new(); }
#line 2420 "src/parser.c"
    break;

  case 91: /* stmt_list: stmt_list stmt  */
#line 697 "src/parser.y"
                          { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 2426 "src/parser.c"
    break;

  case 92: /* stmt: block  */
#line 701 "src/parser.y"
                                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2432 "src/parser.c"
    break;

  case 93: /* stmt: IF '(' expr ')' stmt  */
#line 703 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_IF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = NULL;
        }
#line 2441 "src/parser.c"
    break;

  case 94: /* stmt: IF '(' expr ')' stmt ELSE stmt  */
#line 708 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_IF, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2450 "src/parser.c"
    break;

  case 95: /* stmt: WHILE '(' expr ')' stmt  */
#line 713 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_WHILE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2459 "src/parser.c"
    break;

  case 96: /* $@8: %empty  */
#line 717 "src/parser.y"
              { symtab_push_scope(g_symtab, NULL, 0); }
#line 2465 "src/parser.c"
    break;

  case 97: /* stmt: FOR '(' $@8 for_init ';' expr_opt ';' expr_opt ')' stmt  */
#line 718 "src/parser.y"
        {
            /* Own scope so a loop-local `int i` in for_init doesn't leak
             * into the enclosing block/function (and so a second, later
             * `for (int i ...)` in the same function doesn't collide with
             * it in the symbol table). */
            symtab_pop_scope(g_symtab);
            ((*yyvalp).node) = ast_new(AST_FOR, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->c = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->d = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2479 "src/parser.c"
    break;

  case 98: /* stmt: RETURN expr_opt ';'  */
#line 728 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_RETURN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 2488 "src/parser.c"
    break;

  case 99: /* stmt: var_decl ';'  */
#line 732 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2494 "src/parser.c"
    break;

  case 100: /* stmt: typedef_decl ';'  */
#line 733 "src/parser.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2500 "src/parser.c"
    break;

  case 101: /* stmt: expr ';'  */
#line 735 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 2509 "src/parser.c"
    break;

  case 102: /* stmt: ';'  */
#line 740 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 2517 "src/parser.c"
    break;

  case 103: /* for_init: %empty  */
#line 746 "src/parser.y"
                   { ((*yyvalp).node) = NULL; }
#line 2523 "src/parser.c"
    break;

  case 104: /* for_init: var_decl  */
#line 747 "src/parser.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2529 "src/parser.c"
    break;

  case 105: /* for_init: expr  */
#line 749 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_EXPR_STMT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2538 "src/parser.c"
    break;

  case 106: /* expr_opt: %empty  */
#line 756 "src/parser.y"
                   { ((*yyvalp).node) = NULL; }
#line 2544 "src/parser.c"
    break;

  case 107: /* expr_opt: expr  */
#line 757 "src/parser.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2550 "src/parser.c"
    break;

  case 108: /* primary_expr: IDENTIFIER  */
#line 774 "src/parser.y"
                        { ((*yyvalp).node) = ast_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2556 "src/parser.c"
    break;

  case 109: /* primary_expr: INT_LITERAL  */
#line 775 "src/parser.y"
                          { ((*yyvalp).node) = ast_new(AST_INT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival); }
#line 2562 "src/parser.c"
    break;

  case 110: /* primary_expr: FLOAT_LITERAL  */
#line 776 "src/parser.y"
                           { ((*yyvalp).node) = ast_new(AST_FLOAT_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->fval = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.fval); }
#line 2568 "src/parser.c"
    break;

  case 111: /* primary_expr: STRING_LITERAL  */
#line 777 "src/parser.y"
                            { ((*yyvalp).node) = ast_new(AST_STRING_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str); }
#line 2574 "src/parser.c"
    break;

  case 112: /* primary_expr: CHAR_LITERAL  */
#line 778 "src/parser.y"
                              { ((*yyvalp).node) = ast_new(AST_CHAR_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival); }
#line 2580 "src/parser.c"
    break;

  case 113: /* primary_expr: TRUE_KW  */
#line 779 "src/parser.y"
                               { ((*yyvalp).node) = ast_new(AST_BOOL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = 1; }
#line 2586 "src/parser.c"
    break;

  case 114: /* primary_expr: FALSE_KW  */
#line 780 "src/parser.y"
                                { ((*yyvalp).node) = ast_new(AST_BOOL_LIT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); ((*yyvalp).node)->ival = 0; }
#line 2592 "src/parser.c"
    break;

  case 115: /* primary_expr: THIS  */
#line 781 "src/parser.y"
                                  { ((*yyvalp).node) = ast_new(AST_THIS, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line); }
#line 2598 "src/parser.c"
    break;

  case 116: /* primary_expr: qualified_id_expr  */
#line 782 "src/parser.y"
                                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2604 "src/parser.c"
    break;

  case 117: /* primary_expr: '(' expr ')'  */
#line 783 "src/parser.y"
                                      { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 2610 "src/parser.c"
    break;

  case 118: /* postfix_expr: primary_expr  */
#line 787 "src/parser.y"
                                            { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2616 "src/parser.c"
    break;

  case 119: /* postfix_expr: postfix_expr '(' opt_arg_list ')'  */
#line 789 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_CALL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->list = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2626 "src/parser.c"
    break;

  case 120: /* postfix_expr: postfix_expr '.' IDENTIFIER  */
#line 795 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_MEMBER, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup(".");
            ((*yyvalp).node)->str2 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 2637 "src/parser.c"
    break;

  case 121: /* postfix_expr: postfix_expr ARROW IDENTIFIER  */
#line 802 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_MEMBER, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("->");
            ((*yyvalp).node)->str2 = strdup((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.str));
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node);
        }
#line 2648 "src/parser.c"
    break;

  case 122: /* postfix_expr: postfix_expr '[' expr ']'  */
#line 809 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_SUBSCRIPT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node);
            ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 2658 "src/parser.c"
    break;

  case 123: /* postfix_expr: postfix_expr INC  */
#line 815 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("post++");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 2668 "src/parser.c"
    break;

  case 124: /* postfix_expr: postfix_expr DEC  */
#line 821 "src/parser.y"
        {
            ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            ((*yyvalp).node)->str1 = strdup("post--");
            ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node);
        }
#line 2678 "src/parser.c"
    break;

  case 125: /* unary_expr: postfix_expr  */
#line 829 "src/parser.y"
                             { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2684 "src/parser.c"
    break;

  case 126: /* unary_expr: '!' unary_expr  */
#line 831 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("!"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2690 "src/parser.c"
    break;

  case 127: /* unary_expr: '~' unary_expr  */
#line 833 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("~"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2696 "src/parser.c"
    break;

  case 128: /* unary_expr: '-' unary_expr  */
#line 835 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("neg"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2702 "src/parser.c"
    break;

  case 129: /* unary_expr: '&' unary_expr  */
#line 837 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("addr"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2708 "src/parser.c"
    break;

  case 130: /* unary_expr: '*' unary_expr  */
#line 839 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("deref"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2714 "src/parser.c"
    break;

  case 131: /* unary_expr: INC unary_expr  */
#line 841 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("pre++"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2720 "src/parser.c"
    break;

  case 132: /* unary_expr: DEC unary_expr  */
#line 843 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_UNOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("pre--"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2726 "src/parser.c"
    break;

  case 133: /* unary_expr: NEW type_spec  */
#line 845 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_NEW, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->type = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2732 "src/parser.c"
    break;

  case 134: /* unary_expr: DELETE unary_expr  */
#line 847 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_DELETE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2738 "src/parser.c"
    break;

  case 135: /* expr: unary_expr  */
#line 851 "src/parser.y"
                           { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2744 "src/parser.c"
    break;

  case 136: /* expr: expr '*' expr  */
#line 852 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("*"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2750 "src/parser.c"
    break;

  case 137: /* expr: expr '/' expr  */
#line 853 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("/"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2756 "src/parser.c"
    break;

  case 138: /* expr: expr '%' expr  */
#line 854 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("%"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2762 "src/parser.c"
    break;

  case 139: /* expr: expr '+' expr  */
#line 855 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("+"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2768 "src/parser.c"
    break;

  case 140: /* expr: expr '-' expr  */
#line 856 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("-"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2774 "src/parser.c"
    break;

  case 141: /* expr: expr '<' expr  */
#line 857 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2780 "src/parser.c"
    break;

  case 142: /* expr: expr '>' expr  */
#line 858 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2786 "src/parser.c"
    break;

  case 143: /* expr: expr LE expr  */
#line 859 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("<="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2792 "src/parser.c"
    break;

  case 144: /* expr: expr GE expr  */
#line 860 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup(">="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2798 "src/parser.c"
    break;

  case 145: /* expr: expr EQ expr  */
#line 861 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("=="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2804 "src/parser.c"
    break;

  case 146: /* expr: expr NE expr  */
#line 862 "src/parser.y"
                       { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("!="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2810 "src/parser.c"
    break;

  case 147: /* expr: expr ANDAND expr  */
#line 863 "src/parser.y"
                        { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("&&"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2816 "src/parser.c"
    break;

  case 148: /* expr: expr OROR expr  */
#line 864 "src/parser.y"
                        { ((*yyvalp).node) = ast_new(AST_BINOP, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("||"); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2822 "src/parser.c"
    break;

  case 149: /* expr: expr '=' expr  */
#line 866 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2828 "src/parser.c"
    break;

  case 150: /* expr: expr PLUSEQ expr  */
#line 868 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("+="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2834 "src/parser.c"
    break;

  case 151: /* expr: expr MINUSEQ expr  */
#line 870 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("-="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2840 "src/parser.c"
    break;

  case 152: /* expr: expr STAREQ expr  */
#line 872 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("*="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2846 "src/parser.c"
    break;

  case 153: /* expr: expr SLASHEQ expr  */
#line 874 "src/parser.y"
        { ((*yyvalp).node) = ast_new(AST_ASSIGN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); ((*yyvalp).node)->str1 = strdup("/="); ((*yyvalp).node)->a = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node); ((*yyvalp).node)->b = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2852 "src/parser.c"
    break;

  case 154: /* opt_arg_list: %empty  */
#line 878 "src/parser.y"
                   { ((*yyvalp).list) = ast_list_new(); }
#line 2858 "src/parser.c"
    break;

  case 155: /* opt_arg_list: arg_list  */
#line 879 "src/parser.y"
                    { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 2864 "src/parser.c"
    break;

  case 156: /* arg_list: expr  */
#line 883 "src/parser.y"
                            { ((*yyvalp).list) = ast_list_new(); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 2870 "src/parser.c"
    break;

  case 157: /* arg_list: arg_list ',' expr  */
#line 884 "src/parser.y"
                             { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list); ast_list_append(&((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)); }
#line 2876 "src/parser.c"
    break;


#line 2880 "src/parser.c"

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




#line 887 "src/parser.y"


void yyerror(const char *msg) {
    fprintf(stderr, "%s:%d: error: %s\n", g_current_filename, g_lex_lineno, msg);
}
