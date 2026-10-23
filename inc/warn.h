#ifndef V32CXX_WARN_H
#define V32CXX_WARN_H

/*
 * Warnings with a kind name.
 *
 * Every warning v32c++ itself writes goes through v32_warnf(kind, ...): the
 * text is as before, with ` [-Wkind]` added to its last line, so a build can
 * name what it wants to treat as an error (--warn-error[=kind,...]).
 * Warnings of the Vircon32 compiler are not ours and are not covered.
 *
 * The kinds (`v32c++ --list-warnings` prints them):
 *   unsigned dropped-args lib-duplicate build-script cart-name
 *   vector-destructor ptr-return-write compound-side-effect macro-redefined
 *   include-guard include-in-body sdk-header-missing pragma static-assert
 *   bit-field undefined-reference no-effect dynamic-cast
 */

/* Prints a warning (format as for printf, ending in a newline) and counts
 * it. Needs `kind` to be one of the names above. */
void v32_warnf(const char *kind, const char *fmt, ...);

/* --warn-error / --warn-error=a,b : `list` NULL or "" means every kind.
 * Returns 0, or -1 (message printed) for a name that is not a kind. */
int warn_error_enable(const char *list);

/* Warnings written so far, and those of them that are errors. */
int warn_count(void);
int warn_error_count(void);

/* `--list-warnings`: the kinds, one per line, with what each is about. */
void warn_list_kinds(void);

#endif
