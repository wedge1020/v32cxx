/* link.h -- whole-program linking of several source files (link.c).
 *
 * `v32c a.c b.c lib.c -o prog.c`: the files are expanded into one stream
 * (prescan_expand_many) and parsed as one program, with a marker
 * declaration between them. link_merge then does what a linker does:
 *
 *   - a file-scope `static` name that clashes with anything of the same
 *     name in another file is renamed in the file that owns it (`helper`
 *     in b.c becomes `helper__b`), references included;
 *   - a function defined twice, or a variable initialised twice, is an
 *     error ("multiple definition");
 *   - a function or `extern` variable that is used but never defined is
 *     an error ("undefined reference"), unless allow_undefined (the SDK
 *     declares its own functions; a prototype for one of those is
 *     legitimate).
 *
 * Prototypes and `extern` declarations are then matched with their
 * definitions by the passes that already exist for one file
 * (cmode_unify_prototypes, merge_tentative_globals). Returns the number
 * of errors. */
#ifndef V32CXX_LINK_H
#define V32CXX_LINK_H

#include "ast.h"

int link_merge(AstList *decls, const char *const *files, int nfiles, int allow_undefined);

#endif
