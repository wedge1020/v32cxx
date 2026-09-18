#ifndef V32CXX_H
#define V32CXX_H

/*
 * Project-wide identifying information and build-time configuration
 * constants -- modeled after the sibling v32lua project's own
 * v32lua.h, which every one of that project's source files includes.
 * THIS project doesn't need that same everything-includes-it structure
 * (v32c++'s files are already more narrowly, individually scoped -- ast.h,
 * sema.h, lower.h, codegen.h, driver.h, symtab.h each own a specific
 * piece), so only whatever actually needs something declared here
 * includes this file. Currently: main.c (VERSION/AUTHOR/URL, for
 * --version) and symtab.h (SYMTAB_BUCKETS).
 *
 * VERSION follows v32lua's own YYYYMMDD + single-digit same-day sequence
 * + "-dev" scheme, for consistency between the two sibling projects --
 * adjust freely; there was no prior VERSION string to preserve
 * compatibility with, so this is a starting point, not a fixed
 * convention this project is locked into.
 */

#define  VERSION  "20260918-dev"
#define  AUTHOR   "Matthew Haas"
#define  URL      "https://github.com/wedge1020/v32cxx"

/* Symbol table hash bucket count (symtab.c/symtab.h) -- a tuning
 * constant, not something correctness depends on at any particular
 * value (more buckets: less hash-chain collision at the cost of a
 * larger fixed per-scope allocation; fewer buckets: the reverse).
 * Relocated here from symtab.h specifically as a small, real
 * demonstration of the "build-time configuration lives here for
 * convenience" pattern this file exists for -- not because symtab.h
 * itself needed to change. */
#define  SYMTAB_BUCKETS  64

#endif /* V32CXX_H */
