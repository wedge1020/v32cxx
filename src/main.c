#include <stdio.h>
#include <stdlib.h>
#include "driver.h"
#include "parser.h"
#include "sema.h"

/* Definitions for the globals declared extern in driver.h. */
SymTab *g_symtab = NULL;
AstNode *g_program = NULL;
Symbol *g_current_class_sym = NULL;
int g_lex_lineno = 1;
const char *g_current_filename = "<stdin>";

extern FILE *yyin;

int main(int argc, char **argv) {
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        if (f == NULL) {
            perror(argv[1]);
            return 1;
        }
        yyin = f;
        g_current_filename = argv[1];
    }

    g_symtab = symtab_create();

    int rc = yyparse();

    if (rc == 0 && g_program != NULL) {
        printf("---- parse OK: AST for %s ----\n", g_current_filename);
        ast_dump(g_program, 0);

        int sema_errors = sema_run(g_program);
        sema_dump(g_program);
        if (sema_errors > 0) {
            fprintf(stderr, "---- %d semantic error(s) in %s ----\n", sema_errors, g_current_filename);
            rc = 1;
        }
    } else {
        fprintf(stderr, "---- parse failed for %s ----\n", g_current_filename);
    }

    symtab_destroy(g_symtab);
    return rc;
}
