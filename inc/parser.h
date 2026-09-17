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
    PUBLIC = 267,                  /* PUBLIC  */
    PRIVATE = 268,                 /* PRIVATE  */
    PROTECTED = 269,               /* PROTECTED  */
    NAMESPACE = 270,               /* NAMESPACE  */
    TYPEDEF = 271,                 /* TYPEDEF  */
    RETURN = 272,                  /* RETURN  */
    IF = 273,                      /* IF  */
    ELSE = 274,                    /* ELSE  */
    DO = 275,                      /* DO  */
    WHILE = 276,                   /* WHILE  */
    FOR = 277,                     /* FOR  */
    BREAK = 278,                   /* BREAK  */
    CONTINUE = 279,                /* CONTINUE  */
    SWITCH = 280,                  /* SWITCH  */
    CASE = 281,                    /* CASE  */
    DEFAULT = 282,                 /* DEFAULT  */
    INT_KW = 283,                  /* INT_KW  */
    FLOAT_KW = 284,                /* FLOAT_KW  */
    VOID_KW = 285,                 /* VOID_KW  */
    BOOL_KW = 286,                 /* BOOL_KW  */
    CHAR_KW = 287,                 /* CHAR_KW  */
    NEW = 288,                     /* NEW  */
    DELETE = 289,                  /* DELETE  */
    THIS = 290,                    /* THIS  */
    VIRTUAL = 291,                 /* VIRTUAL  */
    TRUE_KW = 292,                 /* TRUE_KW  */
    FALSE_KW = 293,                /* FALSE_KW  */
    OPERATOR = 294,                /* OPERATOR  */
    STATIC_CAST = 295,             /* STATIC_CAST  */
    DYNAMIC_CAST = 296,            /* DYNAMIC_CAST  */
    CONST_CAST = 297,              /* CONST_CAST  */
    REINTERPRET_CAST = 298,        /* REINTERPRET_CAST  */
    COLONCOLON = 299,              /* COLONCOLON  */
    ARROW = 300,                   /* ARROW  */
    EQ = 301,                      /* EQ  */
    NE = 302,                      /* NE  */
    LE = 303,                      /* LE  */
    GE = 304,                      /* GE  */
    ANDAND = 305,                  /* ANDAND  */
    OROR = 306,                    /* OROR  */
    PLUSEQ = 307,                  /* PLUSEQ  */
    MINUSEQ = 308,                 /* MINUSEQ  */
    STAREQ = 309,                  /* STAREQ  */
    SLASHEQ = 310,                 /* SLASHEQ  */
    INC = 311,                     /* INC  */
    DEC = 312,                     /* DEC  */
    SHL = 313,                     /* SHL  */
    SHR = 314,                     /* SHR  */
    ANDEQ = 315,                   /* ANDEQ  */
    OREQ = 316,                    /* OREQ  */
    XOREQ = 317,                   /* XOREQ  */
    SHLEQ = 318,                   /* SHLEQ  */
    SHREQ = 319,                   /* SHREQ  */
    LOWER_THAN_ELSE = 320          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 97 "src/parser.y"

    AstNode *node;
    AstList list;
    char *str;
    int ival;
    double fval;
    AccessSpec access;

#line 133 "inc/parser.h"

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
