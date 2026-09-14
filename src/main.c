#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "driver.h"
#include "parser.h"
#include "sema.h"
#include "lower.h"
#include "codegen.h"
#include "v32cxx.h"

/* Definitions for the globals declared extern in driver.h. */
SymTab *g_symtab = NULL;
AstNode *g_program = NULL;
Symbol *g_current_class_sym = NULL;
int g_lex_lineno = 1;
const char *g_current_filename = "<stdin>";
PreprocessorLines g_preprocessor_lines = {NULL, 0, 0};
int g_uses_new_or_delete = 0;

extern FILE *yyin;

static void print_usage(const char *prog_name) {
    fprintf(stderr, "usage: %s [-o output.c] [-c] [--version] <input.cpp>\n", prog_name);
    fprintf(stderr, "  -o output.c   write generated Vircon32 C to this file\n"
                     "                (instead of the \"generated Vircon32 C\" dump section)\n");
    fprintf(stderr, "  -c            transpile without requiring a `main` to exist --\n"
                     "                like a real compiler's -c (\"compile only\"), for a\n"
                     "                library/module fragment rather than a complete,\n"
                     "                standalone-compilable program\n");
    fprintf(stderr, "  --version     print version information and exit\n");
}

static void print_version(void) {
    /* Matches v32lua's own --version format exactly, for consistency
     * between the two sibling projects -- see v32cxx.h for VERSION/
     * AUTHOR/URL themselves. */
    printf("v32c++ %s\n", VERSION);
    printf("C++ Transpiler for Vircon32 (v32c++) by %s\n", AUTHOR);
    printf("  github: %s\n", URL);
}

int main(int argc, char **argv) {
    const char *output_filename = NULL;
    int require_main = 1;
    int opt;

    /* "version" is long-option-only, deliberately -- a bare `-v` is
     * reserved for the future verbosity-level flag (see
     * docs/DESIGN_NOTES.md's CLI-considerations section), and giving
     * --version a short alias now would collide with that later. */
    static struct option long_options[] = {
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "o:c", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                output_filename = optarg;
                break;
            case 'c':
                require_main = 0;
                break;
            case 'V':
                print_version();
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input_filename = argv[optind];
    FILE *f = fopen(input_filename, "r");
    if (f == NULL) {
        perror(input_filename);
        return 1;
    }
    yyin = f;
    g_current_filename = input_filename;

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
        } else if (require_main && !sema_program_has_main(g_program)) {
            /* main.c's own CLI-level default, entirely separate from
             * anything sema_run() itself checks -- this project's
             * compiler has never required a `main` to exist to
             * transpile something correctly, and still doesn't; `-c`
             * (like a real compiler's "compile only" flag) opts out of
             * this default for a library/module fragment rather than a
             * complete, standalone-compilable program. See
             * sema_program_has_main's own doc comment in sema.h. */
            fprintf(stderr, "---- error: no 'main' function found in %s ----\n", g_current_filename);
            fprintf(stderr, "  (pass -c to transpile without requiring one)\n");
            rc = 1;
        } else {
            /* Lowering trusts sema's results (ClassLayout, vtables, ...)
             * to be complete and correct, so it only runs once sema has
             * come back clean -- see lower_run()'s precondition in
             * lower.h. */
            lower_run(g_program);
            lower_dump(g_program);
            /* codegen_run() only ever reads PER-NODE annotations
             * (sema_info/lower_info) already attached directly to the
             * tree -- never the global class/typedef/free-function
             * registries sema_cleanup() frees below -- so its ordering
             * relative to that cleanup call doesn't actually matter. Runs
             * here anyway, before cleanup, to keep the "don't free
             * anything a later step might still need" discipline this
             * project settled on after the vtable-dispatch registry-
             * lifetime bug, rather than re-litigating it per call site. */
            if (output_filename != NULL) {
                FILE *out = fopen(output_filename, "w");
                if (out == NULL) {
                    perror(output_filename);
                    rc = 1;
                } else {
                    codegen_run(g_program, out);
                    fclose(out);
                    printf("---- wrote generated Vircon32 C to %s ----\n", output_filename);
                }
            } else {
                printf("---- generated Vircon32 C ----\n");
                codegen_run(g_program, stdout);
            }
        }
        /* sema_cleanup() frees the class/typedef/free-function registries
         * sema_run() built -- deliberately called HERE, after lowering
         * has had its turn, not right after sema_run() returns above.
         * lower_run()'s vtable-dispatch phase depends on those same
         * registries (via resolve_expr_class); freeing them any earlier
         * silently breaks it -- see sema_cleanup()'s doc comment in
         * sema.h for the bug that shipped once already because of this. */
        sema_cleanup();
    } else {
        fprintf(stderr, "---- parse failed for %s ----\n", g_current_filename);
    }

    symtab_destroy(g_symtab);
    return rc;
}


