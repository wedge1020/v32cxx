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
    STRING_LITERAL = 260,          /* STRING_LITERAL  */
    INT_LITERAL = 261,             /* INT_LITERAL  */
    CHAR_LITERAL = 262,            /* CHAR_LITERAL  */
    FLOAT_LITERAL = 263,           /* FLOAT_LITERAL  */
    CLASS = 264,                   /* CLASS  */
    STRUCT = 265,                  /* STRUCT  */
    ENUM = 266,                    /* ENUM  */
    UNION = 267,                   /* UNION  */
    PUBLIC = 268,                  /* PUBLIC  */
    PRIVATE = 269,                 /* PRIVATE  */
    PROTECTED = 270,               /* PROTECTED  */
    NAMESPACE = 271,               /* NAMESPACE  */
    TYPEDEF = 272,                 /* TYPEDEF  */
    RETURN = 273,                  /* RETURN  */
    IF = 274,                      /* IF  */
    ELSE = 275,                    /* ELSE  */
    DO = 276,                      /* DO  */
    WHILE = 277,                   /* WHILE  */
    FOR = 278,                     /* FOR  */
    BREAK = 279,                   /* BREAK  */
    CONTINUE = 280,                /* CONTINUE  */
    GOTO = 281,                    /* GOTO  */
    SWITCH = 282,                  /* SWITCH  */
    CASE = 283,                    /* CASE  */
    DEFAULT = 284,                 /* DEFAULT  */
    INT_KW = 285,                  /* INT_KW  */
    FLOAT_KW = 286,                /* FLOAT_KW  */
    VOID_KW = 287,                 /* VOID_KW  */
    BOOL_KW = 288,                 /* BOOL_KW  */
    CHAR_KW = 289,                 /* CHAR_KW  */
    NEW = 290,                     /* NEW  */
    DELETE = 291,                  /* DELETE  */
    THIS = 292,                    /* THIS  */
    VIRTUAL = 293,                 /* VIRTUAL  */
    TRUE_KW = 294,                 /* TRUE_KW  */
    FALSE_KW = 295,                /* FALSE_KW  */
    OPERATOR = 296,                /* OPERATOR  */
    SIZEOF = 297,                  /* SIZEOF  */
    STATIC_CAST = 298,             /* STATIC_CAST  */
    DYNAMIC_CAST = 299,            /* DYNAMIC_CAST  */
    CONST_CAST = 300,              /* CONST_CAST  */
    REINTERPRET_CAST = 301,        /* REINTERPRET_CAST  */
    COLONCOLON = 302,              /* COLONCOLON  */
    ARROW = 303,                   /* ARROW  */
    EQ = 304,                      /* EQ  */
    NE = 305,                      /* NE  */
    LE = 306,                      /* LE  */
    GE = 307,                      /* GE  */
    ANDAND = 308,                  /* ANDAND  */
    OROR = 309,                    /* OROR  */
    PLUSEQ = 310,                  /* PLUSEQ  */
    MINUSEQ = 311,                 /* MINUSEQ  */
    STAREQ = 312,                  /* STAREQ  */
    SLASHEQ = 313,                 /* SLASHEQ  */
    INC = 314,                     /* INC  */
    DEC = 315,                     /* DEC  */
    SHL = 316,                     /* SHL  */
    SHR = 317,                     /* SHR  */
    ANDEQ = 318,                   /* ANDEQ  */
    OREQ = 319,                    /* OREQ  */
    XOREQ = 320,                   /* XOREQ  */
    SHLEQ = 321,                   /* SHLEQ  */
    SHREQ = 322,                   /* SHREQ  */
    SIZEOF_TYPE_PREC = 323,        /* SIZEOF_TYPE_PREC  */
    LOWER_THAN_ELSE = 324          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 118 "src/parser.y"

    AstNode *node;
    AstList list;
    char *str;
    int ival;
    double fval;
    AccessSpec access;

#line 137 "inc/parser.h"

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
