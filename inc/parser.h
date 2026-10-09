/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Skeleton interface for Bison GLR parsers in C

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

#ifndef YY_YY_INC_PARSER_H_INCLUDED
# define YY_YY_INC_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    IDENTIFIER = 258,              /* IDENTIFIER  */
    TYPE_NAME = 259,               /* TYPE_NAME  */
    TAG_NAME = 260,                /* TAG_NAME  */
    STRING_LITERAL = 261,          /* STRING_LITERAL  */
    CLASS_FINAL = 262,             /* CLASS_FINAL  */
    INT_LITERAL = 263,             /* INT_LITERAL  */
    CHAR_LITERAL = 264,            /* CHAR_LITERAL  */
    FLOAT_LITERAL = 265,           /* FLOAT_LITERAL  */
    CLASS = 266,                   /* CLASS  */
    STRUCT = 267,                  /* STRUCT  */
    ENUM = 268,                    /* ENUM  */
    UNION = 269,                   /* UNION  */
    PUBLIC = 270,                  /* PUBLIC  */
    PRIVATE = 271,                 /* PRIVATE  */
    PROTECTED = 272,               /* PROTECTED  */
    NAMESPACE = 273,               /* NAMESPACE  */
    TYPEDEF = 274,                 /* TYPEDEF  */
    RETURN = 275,                  /* RETURN  */
    IF = 276,                      /* IF  */
    ELSE = 277,                    /* ELSE  */
    DO = 278,                      /* DO  */
    WHILE = 279,                   /* WHILE  */
    FOR = 280,                     /* FOR  */
    BREAK = 281,                   /* BREAK  */
    CONTINUE = 282,                /* CONTINUE  */
    GOTO = 283,                    /* GOTO  */
    SWITCH = 284,                  /* SWITCH  */
    CASE = 285,                    /* CASE  */
    DEFAULT = 286,                 /* DEFAULT  */
    INT_KW = 287,                  /* INT_KW  */
    FLOAT_KW = 288,                /* FLOAT_KW  */
    VOID_KW = 289,                 /* VOID_KW  */
    BOOL_KW = 290,                 /* BOOL_KW  */
    CHAR_KW = 291,                 /* CHAR_KW  */
    NEW = 292,                     /* NEW  */
    DELETE = 293,                  /* DELETE  */
    THIS = 294,                    /* THIS  */
    VIRTUAL = 295,                 /* VIRTUAL  */
    TRUE_KW = 296,                 /* TRUE_KW  */
    FALSE_KW = 297,                /* FALSE_KW  */
    NULLPTR_KW = 298,              /* NULLPTR_KW  */
    OPERATOR = 299,                /* OPERATOR  */
    SIZEOF = 300,                  /* SIZEOF  */
    CONST = 301,                   /* CONST  */
    FRIEND = 302,                  /* FRIEND  */
    STATIC_CAST = 303,             /* STATIC_CAST  */
    DYNAMIC_CAST = 304,            /* DYNAMIC_CAST  */
    CONST_CAST = 305,              /* CONST_CAST  */
    REINTERPRET_CAST = 306,        /* REINTERPRET_CAST  */
    COLONCOLON = 307,              /* COLONCOLON  */
    ARROW = 308,                   /* ARROW  */
    EQ = 309,                      /* EQ  */
    NE = 310,                      /* NE  */
    LE = 311,                      /* LE  */
    GE = 312,                      /* GE  */
    ANDAND = 313,                  /* ANDAND  */
    OROR = 314,                    /* OROR  */
    PLUSEQ = 315,                  /* PLUSEQ  */
    MINUSEQ = 316,                 /* MINUSEQ  */
    STAREQ = 317,                  /* STAREQ  */
    SLASHEQ = 318,                 /* SLASHEQ  */
    INC = 319,                     /* INC  */
    DEC = 320,                     /* DEC  */
    SHL = 321,                     /* SHL  */
    SHR = 322,                     /* SHR  */
    ANDEQ = 323,                   /* ANDEQ  */
    OREQ = 324,                    /* OREQ  */
    XOREQ = 325,                   /* XOREQ  */
    SHLEQ = 326,                   /* SHLEQ  */
    SHREQ = 327,                   /* SHREQ  */
    ASM = 328,                     /* ASM  */
    VOLATILE = 329,                /* VOLATILE  */
    NATIVE = 330,                  /* NATIVE  */
    MODEQ = 331,                   /* MODEQ  */
    STATIC = 332,                  /* STATIC  */
    STD_ARRAY = 333,               /* STD_ARRAY  */
    STD_VECTOR = 334,              /* STD_VECTOR  */
    ELLIPSIS = 335,                /* ELLIPSIS  */
    ANON_STRUCT = 336,             /* ANON_STRUCT  */
    ANON_UNION = 337,              /* ANON_UNION  */
    ANON_ENUM = 338,               /* ANON_ENUM  */
    EXTERN = 339,                  /* EXTERN  */
    VA_ARG = 340,                  /* VA_ARG  */
    SIZEOF_TYPE_PREC = 341,        /* SIZEOF_TYPE_PREC  */
    LOWER_THAN_ELSE = 342          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 671 "src/parser.y"

    AstNode *node;
    AstList list;
    char *str;
    int ival;
    double fval;
    AccessSpec access;
    /* INT_LITERAL / FLOAT_LITERAL / CHAR_LITERAL: the value plus the
     * object-like macro it was expanded from, if any (lexer.l reads the
     * pre-scan's @NAME@ annotation; see AstNode's macro_name). Carried in
     * the token's own semantic value -- never a side global -- because a
     * GLR parse may run this token's action long after the lexer moved
     * on. */
    struct { int ival; double fval; char *macro; } lit;

#line 162 "inc/parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


extern YYSTYPE yylval;
extern YYLTYPE yylloc;
int yyparse (void);

#endif /* !YY_YY_INC_PARSER_H_INCLUDED  */
