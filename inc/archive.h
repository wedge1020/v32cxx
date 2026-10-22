/* archive.h -- libraries for the linker (archive.c).
 *
 * A library is an index file, `libname.v32a`, over its member source files:
 * for each member, the symbols it defines and the names its code uses.
 * `v32c --mklib libm.v32a a.c b.c` writes one; `v32c main.c libm.v32a` (or
 * `-L dir -lm`) links main.c with only those members that define something
 * the program needs, taking in what they need in turn. The members stay
 * source files, found through the index's relative paths; the index
 * records each one's size so a changed member is reported, not ignored. */
#ifndef V32CXX_ARCHIVE_H
#define V32CXX_ARCHIVE_H

#include "ast.h"

int path_is_archive(const char *path);          /* ends in .v32a */

/* Writes `archive_path`, indexing `members`. `self` is how to run this
 * program again, `fwd` its parse-relevant options (-I, -D, ...). 0 on success. */
int archive_make(const char *archive_path, const char *self, const char *const *fwd, int nfwd,
                 const char *const *members, int nmembers);

/* Appends to `*inputs` the members of `libs` that the `nin` inputs need
 * (directly or through other members). 0 on success, -1 on error. */
int archive_select(const char *self, const char *const *fwd, int nfwd,
                   const char *const *libs, int nlibs,
                   const char ***inputs, int *nin);

#endif
