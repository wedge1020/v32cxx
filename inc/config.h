#ifndef V32CXX_CONFIG_H
#define V32CXX_CONFIG_H

// ============================================================================
// v32c++ build-time configuration
// ----------------------------------------------------------------------------
//
// Installation-dependent  defaults  live   here  (identity  --  VERSION,
// AUTHOR,  URL --  lives  in v32cxx.h).  Every value  is  wrapped in  an
// #ifndef guard  so it can be  overridden at build time  without editing
// this file, e.g.:
//
//     make CFLAGS="-Wall -Wextra -g -Iinc -MMD -MP -DV32CXX_INCLUDE_PATH='\"/opt/v32tools/include/v32c++\"'"
//
// Modeled on the sibling v32lua project's own config.h.
//
// ============================================================================

// ----------------------------------------------------------------------------
// #include search path
// ----------------------------------------------------------------------------
//
// An #include of a .hpp/.cpp file  (the ones v32c++ resolves and inlines
// itself;  .h  headers pass  through  to  the  Vircon32 C  compiler)  is
// searched for in this order:
//
//     "quote" form                      <angle> form
//     -------------------------------   -------------------------------
//     1. the including file's own dir   (skipped)
//     2. each -I dir, in order          1. each -I dir, in order
//     3. the current directory          (skipped)
//     4. each $V32CXX_INCLUDE dir       2. each $V32CXX_INCLUDE dir
//     5. V32CXX_INCLUDE_PATH            3. V32CXX_INCLUDE_PATH
//
// V32CXX_INCLUDE_PATH  is where  an  installed copy  of  the v32  veneer
// headers (v32/math.hpp, v32/audio.hpp,  ...) is expected to  live, so a
// program can #include <v32/math.hpp> from any project directory without
// -I or  copying the headers  around. The environment variable  named by
// V32CXX_INCLUDE_ENV_VAR  (a  colon-separated  list of  directories)  is
// searched  BEFORE this  default, letting  individual projects  point at
// their  own header  copies without  installing anything  system-wide. A
// directory that doesn't exist is simply skipped.
//
//
// The installed layout groups the community tools  under v32tools/, next
// to the  Vircon32 DevTools:  v32tools/include/v32c++/ holds  v32/ (the
// C++ headers) and libc/ (the C library  for C input, which C input also
// searches,  as  V32CXX_INCLUDE_PATH/libc);  v32lua's  includes  go  in
// v32tools/include/v32lua/ alongside.  The CMake  build sets  this from
// its install prefix; `make sysinstall` installs to this path.
#ifndef V32CXX_INCLUDE_PATH
    #define V32CXX_INCLUDE_PATH "/usr/local/Vircon32/v32tools/include/v32c++"
#endif

#ifndef V32CXX_INCLUDE_ENV_VAR
    #define V32CXX_INCLUDE_ENV_VAR "V32CXX_INCLUDE"
#endif

// ----------------------------------------------------------------------------
// Vircon32 SDK headers
// ----------------------------------------------------------------------------
//
// .h  headers pass  through to  the generated  C, where  the Vircon32  C
// compiler includes them. v32c++ also READS  them when it can find them,
// so their macros (screen_width,  color_red, pi, ...) and struct/typedef
// names (date_info,  game_signature, ...)  are visible  on the  C++ side
// too. A  .h is looked for  with the normal #include  search first, then
// in:
//
//     1. each directory in $V32CXX_SDK_INCLUDE (colon-separated)
//     2. the include/ folder next to the `compile` found on $PATH -- the
//        very folder the Vircon32 C compiler itself searches -- when it
//        holds video.h
//     3. V32CXX_SDK_INCLUDE_PATH -- the Vircon32 DevTools' default install
//        location on Linux and macOS
//
// A header found nowhere is skipped, exactly as before this existed.
//
#ifndef V32CXX_SDK_INCLUDE_PATH
    #define V32CXX_SDK_INCLUDE_PATH "/usr/local/Vircon32/DevTools/include"
#endif

#ifndef V32CXX_SDK_INCLUDE_ENV_VAR
    #define V32CXX_SDK_INCLUDE_ENV_VAR "V32CXX_SDK_INCLUDE"
#endif

// The Vircon32 C compiler's executable name, looked up on $PATH (item 2).
//
#ifndef V32CXX_SDK_COMPILER_NAME
    #define V32CXX_SDK_COMPILER_NAME "compile"
#endif

// Upper bound on include directories from all sources combined (-I flags
// plus $V32CXX_INCLUDE entries plus V32CXX_INCLUDE_PATH).
//
#ifndef V32CXX_MAX_INCLUDE_DIRS
    #define V32CXX_MAX_INCLUDE_DIRS 64
#endif

// ----------------------------------------------------------------------------
// #define handling (prescan.c)
// ----------------------------------------------------------------------------
//
// How deep one macro may expand  into another before v32c++ gives up and
// reports a (probably accidental)  runaway definition. Self-reference is
// already blocked the standard way  (a macro is never re-expanded inside
// its own expansion), so this only guards against pathological chains.
//
#ifndef V32CXX_MAX_MACRO_DEPTH
    #define V32CXX_MAX_MACRO_DEPTH 64
#endif

// ----------------------------------------------------------------------------
// Tuning
// ----------------------------------------------------------------------------
//
// Symbol  table hash  bucket  count  (symtab.c/symtab.h) --  correctness
// never  depends  on  the  value: more  buckets  mean  fewer  hash-chain
// collisions at the cost of a larger fixed per-scope allocation.
//
#ifndef SYMTAB_BUCKETS
    #define SYMTAB_BUCKETS 64
#endif

#if V32CXX_MAX_INCLUDE_DIRS < 2
    #error "V32CXX_MAX_INCLUDE_DIRS must be at least 2"
#endif

#if SYMTAB_BUCKETS < 1
    #error "SYMTAB_BUCKETS must be at least 1"
#endif

#endif /* V32CXX_CONFIG_H */
