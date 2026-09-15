#ifndef PATHUTIL_H
#define PATHUTIL_H

/* Replaces `path`'s own extension (everything after its last '.', but
 * only when that '.' comes after `path`'s own last '/' too -- so a
 * directory component that happens to contain a '.' of its own can't
 * be mistaken for the file's actual extension) with `new_ext`, or
 * appends `new_ext` if `path` has no extension at all. `new_ext`
 * should include its own leading '.' (".c", ".xml", ".vbin", ...).
 *
 * "foo/bar.cpp" + ".c" -> "foo/bar.c"; "foo/bar" + ".c" -> "foo/bar.c".
 *
 * Originally main.c's own derive_output_filename (input .cpp -> output
 * .c); extracted here, generalized to take the new extension as a
 * parameter, once cartxml.c needed the IDENTICAL "swap the extension"
 * logic for deriving a .xml path and a .vbin path from the SAME output
 * filename main.c already computes -- one implementation, not two
 * copies that could quietly drift apart from each other over time.
 *
 * Caller owns the returned, malloc'd buffer, and must free it. */
char *replace_extension(const char *path, const char *new_ext);

#endif /* PATHUTIL_H */
