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
    PUBLIC = 266,                  /* PUBLIC  */
    PRIVATE = 267,                 /* PRIVATE  */
    PROTECTED = 268,               /* PROTECTED  */
    NAMESPACE = 269,               /* NAMESPACE  */
    TYPEDEF = 270,                 /* TYPEDEF  */
    RETURN = 271,                  /* RETURN  */
    IF = 272,                      /* IF  */
    ELSE = 273,                    /* ELSE  */
    WHILE = 274,                   /* WHILE  */
    FOR = 275,                     /* FOR  */
    BREAK = 276,                   /* BREAK  */
    CONTINUE = 277,                /* CONTINUE  */
    SWITCH = 278,                  /* SWITCH  */
    CASE = 279,                    /* CASE  */
    DEFAULT = 280,                 /* DEFAULT  */
    INT_KW = 281,                  /* INT_KW  */
    FLOAT_KW = 282,                /* FLOAT_KW  */
    VOID_KW = 283,                 /* VOID_KW  */
    BOOL_KW = 284,                 /* BOOL_KW  */
    CHAR_KW = 285,                 /* CHAR_KW  */
    NEW = 286,                     /* NEW  */
    DELETE = 287,                  /* DELETE  */
    THIS = 288,                    /* THIS  */
    VIRTUAL = 289,                 /* VIRTUAL  */
    TRUE_KW = 290,                 /* TRUE_KW  */
    FALSE_KW = 291,                /* FALSE_KW  */
    OPERATOR = 292,                /* OPERATOR  */
    COLONCOLON = 293,              /* COLONCOLON  */
    ARROW = 294,                   /* ARROW  */
    EQ = 295,                      /* EQ  */
    NE = 296,                      /* NE  */
    LE = 297,                      /* LE  */
    GE = 298,                      /* GE  */
    ANDAND = 299,                  /* ANDAND  */
    OROR = 300,                    /* OROR  */
    PLUSEQ = 301,                  /* PLUSEQ  */
    MINUSEQ = 302,                 /* MINUSEQ  */
    STAREQ = 303,                  /* STAREQ  */
    SLASHEQ = 304,                 /* SLASHEQ  */
    INC = 305,                     /* INC  */
    DEC = 306,                     /* DEC  */
    SHL = 307,                     /* SHL  */
    SHR = 308,                     /* SHR  */
    ANDEQ = 309,                   /* ANDEQ  */
    OREQ = 310,                    /* OREQ  */
    XOREQ = 311,                   /* XOREQ  */
    SHLEQ = 312,                   /* SHLEQ  */
    SHREQ = 313,                   /* SHREQ  */
    LOWER_THAN_ELSE = 314          /* LOWER_THAN_ELSE  */
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

#line 127 "inc/parser.h"

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
