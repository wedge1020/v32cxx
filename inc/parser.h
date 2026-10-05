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
    INT_LITERAL = 262,             /* INT_LITERAL  */
    CHAR_LITERAL = 263,            /* CHAR_LITERAL  */
    FLOAT_LITERAL = 264,           /* FLOAT_LITERAL  */
    CLASS = 265,                   /* CLASS  */
    STRUCT = 266,                  /* STRUCT  */
    ENUM = 267,                    /* ENUM  */
    UNION = 268,                   /* UNION  */
    PUBLIC = 269,                  /* PUBLIC  */
    PRIVATE = 270,                 /* PRIVATE  */
    PROTECTED = 271,               /* PROTECTED  */
    NAMESPACE = 272,               /* NAMESPACE  */
    TYPEDEF = 273,                 /* TYPEDEF  */
    RETURN = 274,                  /* RETURN  */
    IF = 275,                      /* IF  */
    ELSE = 276,                    /* ELSE  */
    DO = 277,                      /* DO  */
    WHILE = 278,                   /* WHILE  */
    FOR = 279,                     /* FOR  */
    BREAK = 280,                   /* BREAK  */
    CONTINUE = 281,                /* CONTINUE  */
    GOTO = 282,                    /* GOTO  */
    SWITCH = 283,                  /* SWITCH  */
    CASE = 284,                    /* CASE  */
    DEFAULT = 285,                 /* DEFAULT  */
    INT_KW = 286,                  /* INT_KW  */
    FLOAT_KW = 287,                /* FLOAT_KW  */
    VOID_KW = 288,                 /* VOID_KW  */
    BOOL_KW = 289,                 /* BOOL_KW  */
    CHAR_KW = 290,                 /* CHAR_KW  */
    NEW = 291,                     /* NEW  */
    DELETE = 292,                  /* DELETE  */
    THIS = 293,                    /* THIS  */
    VIRTUAL = 294,                 /* VIRTUAL  */
    TRUE_KW = 295,                 /* TRUE_KW  */
    FALSE_KW = 296,                /* FALSE_KW  */
    NULLPTR_KW = 297,              /* NULLPTR_KW  */
    OPERATOR = 298,                /* OPERATOR  */
    SIZEOF = 299,                  /* SIZEOF  */
    CONST = 300,                   /* CONST  */
    FRIEND = 301,                  /* FRIEND  */
    STATIC_CAST = 302,             /* STATIC_CAST  */
    DYNAMIC_CAST = 303,            /* DYNAMIC_CAST  */
    CONST_CAST = 304,              /* CONST_CAST  */
    REINTERPRET_CAST = 305,        /* REINTERPRET_CAST  */
    COLONCOLON = 306,              /* COLONCOLON  */
    ARROW = 307,                   /* ARROW  */
    EQ = 308,                      /* EQ  */
    NE = 309,                      /* NE  */
    LE = 310,                      /* LE  */
    GE = 311,                      /* GE  */
    ANDAND = 312,                  /* ANDAND  */
    OROR = 313,                    /* OROR  */
    PLUSEQ = 314,                  /* PLUSEQ  */
    MINUSEQ = 315,                 /* MINUSEQ  */
    STAREQ = 316,                  /* STAREQ  */
    SLASHEQ = 317,                 /* SLASHEQ  */
    INC = 318,                     /* INC  */
    DEC = 319,                     /* DEC  */
    SHL = 320,                     /* SHL  */
    SHR = 321,                     /* SHR  */
    ANDEQ = 322,                   /* ANDEQ  */
    OREQ = 323,                    /* OREQ  */
    XOREQ = 324,                   /* XOREQ  */
    SHLEQ = 325,                   /* SHLEQ  */
    SHREQ = 326,                   /* SHREQ  */
    ASM = 327,                     /* ASM  */
    VOLATILE = 328,                /* VOLATILE  */
    NATIVE = 329,                  /* NATIVE  */
    MODEQ = 330,                   /* MODEQ  */
    STATIC = 331,                  /* STATIC  */
    STD_ARRAY = 332,               /* STD_ARRAY  */
    STD_VECTOR = 333,              /* STD_VECTOR  */
    ELLIPSIS = 334,                /* ELLIPSIS  */
    ANON_STRUCT = 335,             /* ANON_STRUCT  */
    ANON_UNION = 336,              /* ANON_UNION  */
    ANON_ENUM = 337,               /* ANON_ENUM  */
    EXTERN = 338,                  /* EXTERN  */
    VA_ARG = 339,                  /* VA_ARG  */
    SIZEOF_TYPE_PREC = 340,        /* SIZEOF_TYPE_PREC  */
    LOWER_THAN_ELSE = 341          /* LOWER_THAN_ELSE  */
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

#line 161 "inc/parser.h"

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
