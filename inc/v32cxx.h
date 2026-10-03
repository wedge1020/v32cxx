#ifndef V32CXX_H
#define V32CXX_H

/*
 * Project-wide identifying information (build-time configuration lives
 * in config.h) -- modeled after the sibling v32lua project's own
 * v32lua.h, which every one of that project's source files includes.
 * THIS project doesn't need that same everything-includes-it structure
 * (v32c++'s files are already more narrowly, individually scoped -- ast.h,
 * sema.h, lower.h, codegen.h, driver.h, symtab.h each own a specific
 * piece), so only whatever actually needs something declared here
 * includes this file. Currently: main.c (VERSION/AUTHOR/URL, for
 * --version). Build-time configuration lives in config.h.
 *
 * VERSION follows v32lua's own YYYYMMDD + single-digit same-day sequence
 * + "-dev" scheme, for consistency between the two sibling projects.
 *
 * The #define below is the SINGLE SOURCE of the version. `v32c++ --version`
 * reads it directly; everything else that prints it (the man page's .TH
 * header) is stamped from it by `make version` in the base Makefile,
 * which parses the #define below with sed -- so keep it on one line, in
 * this exact `#define VERSION "..."` shape. To release: edit the string,
 * then run `make version`.
 *
 * Installation-dependent defaults (the include search path, ...) live in
 * config.h, not here -- this file is identity, that one is configuration.
 */

#define  VERSION  "20261003-dev"
#define  AUTHOR   "Matthew Haas"
#define  URL      "https://github.com/wedge1020/v32cxx"

#endif /* V32CXX_H */
