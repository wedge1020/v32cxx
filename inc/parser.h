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
    USING = 275,                   /* USING  */
    RETURN = 276,                  /* RETURN  */
    IF = 277,                      /* IF  */
    ELSE = 278,                    /* ELSE  */
    DO = 279,                      /* DO  */
    WHILE = 280,                   /* WHILE  */
    FOR = 281,                     /* FOR  */
    BREAK = 282,                   /* BREAK  */
    CONTINUE = 283,                /* CONTINUE  */
    GOTO = 284,                    /* GOTO  */
    SWITCH = 285,                  /* SWITCH  */
    CASE = 286,                    /* CASE  */
    DEFAULT = 287,                 /* DEFAULT  */
    INT_KW = 288,                  /* INT_KW  */
    FLOAT_KW = 289,                /* FLOAT_KW  */
    VOID_KW = 290,                 /* VOID_KW  */
    BOOL_KW = 291,                 /* BOOL_KW  */
    CHAR_KW = 292,                 /* CHAR_KW  */
    NEW = 293,                     /* NEW  */
    DELETE = 294,                  /* DELETE  */
    THIS = 295,                    /* THIS  */
    VIRTUAL = 296,                 /* VIRTUAL  */
    TRUE_KW = 297,                 /* TRUE_KW  */
    FALSE_KW = 298,                /* FALSE_KW  */
    NULLPTR_KW = 299,              /* NULLPTR_KW  */
    OPERATOR = 300,                /* OPERATOR  */
    SIZEOF = 301,                  /* SIZEOF  */
    CONST = 302,                   /* CONST  */
    FRIEND = 303,                  /* FRIEND  */
    STATIC_CAST = 304,             /* STATIC_CAST  */
    DYNAMIC_CAST = 305,            /* DYNAMIC_CAST  */
    CONST_CAST = 306,              /* CONST_CAST  */
    REINTERPRET_CAST = 307,        /* REINTERPRET_CAST  */
    COLONCOLON = 308,              /* COLONCOLON  */
    ARROW = 309,                   /* ARROW  */
    EQ = 310,                      /* EQ  */
    NE = 311,                      /* NE  */
    LE = 312,                      /* LE  */
    GE = 313,                      /* GE  */
    ANDAND = 314,                  /* ANDAND  */
    OROR = 315,                    /* OROR  */
    PLUSEQ = 316,                  /* PLUSEQ  */
    MINUSEQ = 317,                 /* MINUSEQ  */
    STAREQ = 318,                  /* STAREQ  */
    SLASHEQ = 319,                 /* SLASHEQ  */
    INC = 320,                     /* INC  */
    DEC = 321,                     /* DEC  */
    SHL = 322,                     /* SHL  */
    SHR = 323,                     /* SHR  */
    ANDEQ = 324,                   /* ANDEQ  */
    OREQ = 325,                    /* OREQ  */
    XOREQ = 326,                   /* XOREQ  */
    SHLEQ = 327,                   /* SHLEQ  */
    SHREQ = 328,                   /* SHREQ  */
    ASM = 329,                     /* ASM  */
    VOLATILE = 330,                /* VOLATILE  */
    NATIVE = 331,                  /* NATIVE  */
    MODEQ = 332,                   /* MODEQ  */
    STATIC = 333,                  /* STATIC  */
    STD_ARRAY = 334,               /* STD_ARRAY  */
    STD_VECTOR = 335,              /* STD_VECTOR  */
    ELLIPSIS = 336,                /* ELLIPSIS  */
    ANON_STRUCT = 337,             /* ANON_STRUCT  */
    ANON_UNION = 338,              /* ANON_UNION  */
    ANON_ENUM = 339,               /* ANON_ENUM  */
    EXTERN = 340,                  /* EXTERN  */
    VA_ARG = 341,                  /* VA_ARG  */
    SIZEOF_TYPE_PREC = 342,        /* SIZEOF_TYPE_PREC  */
    LOWER_THAN_ELSE = 343          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 763 "src/parser.y"

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

#line 163 "inc/parser.h"

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
