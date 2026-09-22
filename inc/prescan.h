#ifndef PRESCAN_H
#define PRESCAN_H

#include <stdio.h>

/*
 * Include-resolution pre-scan -- the ONLY piece of preprocessor
 * behavior this project implements, ahead of the still-general
 * "#-line passes through verbatim" rule (see lexer.l and
 * PreprocessorLines in driver.h for that half).
 *
 * prescan_expand() reads input_filename and copies it line-by-line
 * into a temporary stream, EXCEPT that:
 *
 *   - an #include whose target ends in .hpp or .cpp is replaced by
 *     the fully (recursively) expanded contents of that file --
 *     .cpp included so a project can pool several files into one
 *     monolithic output, the standard Vircon32 C strategy;
 *   - every other #include (.h and anything else) is copied through
 *     verbatim, so it still reaches lexer.l's existing pass-through
 *     and lands at the top of the generated .c exactly as before;
 *   - a #pragma once line is consumed (never copied), recording the
 *     file so any LATER inclusion of it emits nothing -- the whole
 *     of this project's #ifndef-guard story, without any macros.
 *
 * Nested includes resolve relative to the directory of the file that
 * physically contains the #include line; quote form additionally
 * falls back to the -I dirs (angle form searches ONLY the -I dirs),
 * matching real-compiler lookup order.
 *
 * To keep diagnostics (and -g's debug map) accurate across all this
 * splicing, each expanded file's content is preceded by a GCC-style
 * line marker -- `# <lineno> "<file>"` -- and lexer.l consumes those
 * markers to re-target g_lex_lineno/g_current_filename. Markers are
 * also re-emitted after every dropped or expanded line, so dropping
 * an #include line can never desync the line count within a file.
 *
 * Returns a tmpfile()-backed FILE* rewound to its start (the caller
 * assigns it to yyin), or NULL with a message already on stderr.
 * The tmpfile() is the caller's to fclose, but note tmpfile() also
 * unlinks on close, so "fclose and forget" is the whole cleanup.
 */
FILE *prescan_expand(const char *input_filename, char *const *include_dirs, int include_dir_count);

/*
 * Returns a stable, interned copy of `filename` (a new copy the first
 * time each distinct string is seen, the same pointer every time
 * after). Exists because g_current_filename is a `const char *` that
 * must keep pointing at valid storage as line markers retarget it:
 * interned copies are never freed, which for a single-shot CLI tool is
 * the same leak-tolerance trade already made for g_preprocessor_lines'
 * strdup'd entries (freed only by the OS at exit).
 */
const char *prescan_intern_filename(const char *filename);

#endif /* PRESCAN_H */
