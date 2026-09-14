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

/*
 * Preprocessor-directive pass-through (interim measure -- see lexer.l's
 * own file-header comment for why this project still has no real
 * preprocessor at all). A line starting with '#' is no longer silently
 * dropped: the lexer captures it here verbatim, in original order, and
 * codegen.c re-emits every captured line at the very top of the
 * generated file, ahead of anything else (including the conditional
 * `#include "misc.h"` codegen_run may add on its own). NOT real
 * preprocessing -- no macro expansion, no #include resolution, no
 * #ifdef evaluation, nothing interprets what a captured line actually
 * means. It only stops throwing the line away, so a `#include "video.h"`
 * a person actually wrote doesn't have to be manually re-added to every
 * transpile output by hand, the way it did before this existed.
 */
typedef struct PreprocessorLines {
    char **lines;
    int count;
    int capacity;
} PreprocessorLines;

extern PreprocessorLines g_preprocessor_lines;

int yylex(void);
void yyerror(const char *msg);

#endif /* DRIVER_H */
