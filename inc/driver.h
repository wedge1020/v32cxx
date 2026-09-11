#ifndef DRIVER_H
#define DRIVER_H

#include "ast.h"
#include "symtab.h"

/*
 * Global driver state, shared between lexer.l and parser.y.
 *
 * SIMPLIFICATION NOTE: a production tool should make the scanner reentrant
 * (`%option reentrant bison-bridge` in flex, `%define api.pure full` in
 * bison) and thread this state through %parse-param/%lex-param instead of
 * using globals, both for thread-safety and so you can parse more than one
 * translation unit's worth of state in the same process (useful for a
 * language server, or for a driver that processes many .cpp files without
 * re-exec'ing). Globals are fine for a single-shot CLI tool and keep this
 * skeleton readable; that's the trade-off being made here.
 */

extern SymTab *g_symtab;
extern AstNode *g_program;

/* The class currently being parsed (its body), used so constructor/
 * destructor declarations and self-referential members can find "my own
 * class's" symbol. NULL outside of a class body. Single-level only --
 * nested classes aren't supported by this skeleton; turn this into a
 * stack if you add them. */
extern Symbol *g_current_class_sym;

extern int g_lex_lineno;
extern const char *g_current_filename;

int yylex(void);
void yyerror(const char *msg);

#endif /* DRIVER_H */
