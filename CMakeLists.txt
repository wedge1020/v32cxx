# ----------------------------------------------------------------------------
#   CMake build for v32c++, the C/C++ to Vircon32 C transpiler
#
#   Laid   out   after   the   Vircon32   DevTools'   own   CMake   setup
#   (ComputerSoftware/DevelopmentTools/CMakeLists.txt) and v32opt's. This
#   is an ALTERNATIVE  to the base Makefile, which  keeps working exactly
#   as before (`make`, `make test`, `make realcheck`, `make sysinstall`);
#   the two never share build products. Build out of tree:
#
#       mkdir build && cd build
#       cmake ..                 (MSYS2/MinGW: cmake -G "MSYS Makefiles" ..)
#       cmake --build .
#       ctest                    (transpiles every test sample)
#       cmake --install .        (may need sudo / an elevated shell)
#       cmake --build . --target uninstall
#       cpack                    (.tar.gz / .deb / .rpm, or .zip on Windows)
#
#   Default install  locations (the  community tools  live in  v32tools/,
#   next    to     the    DevTools;     v32lua's    includes     go    in
#   v32tools/include/v32lua):
#
#       Linux / macOS: /usr/local/bin/v32c++ and v32c (a copy, for C input)
#                      /usr/local/share/man/man1/v32c++.1.gz
#                      /usr/local/share/man/man3/*.3v32.gz (header pages)
#                      (and the Vircon32 SDK's own pages, man/vircon32/,
#                       beside them: Vircon32.1, compile.1, string.h.3v32 ...)
#                      /usr/local/Vircon32/v32tools/include/v32c++/v32/*.hpp
#                      /usr/local/Vircon32/v32tools/include/v32c++/libc/
#                      /usr/local/Vircon32/v32tools/docs/v32c++/
#       Windows      : <Program Files>/Vircon32/v32tools/v32c++.exe and v32c.exe
#                      <Program Files>/Vircon32/v32tools/include/v32c++/v32/*.hpp
#                      <Program Files>/Vircon32/v32tools/include/v32c++/libc/
#                      <Program Files>/Vircon32/v32tools/docs/v32c++/
#                       (the README, the other documents, and the manual
#                       pages in man1/ and man3/)
#
#   Change the  root with -DCMAKE_INSTALL_PREFIX=<dir>  when configuring.
#   The  header directory  is compiled  into  the transpiler  (config.h's
#   V32CXX_INCLUDE_PATH), so  `#include <v32/video.hpp>`  works from  any
#   directory without  -I, and  C input  finds the  C library  (`#include
#   <v32libc.c>`)  without   -I  too.  The   path  compiled  in   is  the
#   configure-time one:  `cmake --install  . --prefix  <dir>` copies  the
#   files  elsewhere  but  the  transpiler   still  looks  where  it  was
#   configured to. The Vircon32 SDK header directory v32c++ falls back on
#   (config.h's V32CXX_SDK_INCLUDE_PATH)  is where  the DevTools  install
#   theirs:            <prefix>/Vircon32/DevTools/include,             or
#   <prefix>/DevTools/include on Windows.
#
#   flex and  bison (3.x) are used  when found; otherwise the  parser and
#   lexer   already  generated   in   the   source  tree   (src/parser.c,
#   inc/parser.h, src/lexer.c) are compiled as  they are, so neither tool
#   is needed  for a  plain build  -- only  for changing  src/parser.y or
#   src/lexer.l.
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.13...3.31)

# Refuse an in-source  build: CMake's generated Makefile  would overwrite
# the project's own hand-written base Makefile.
#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "v32c++: in-source CMake builds are not supported -- they would "
        "overwrite the base Makefile. Build in a separate directory:\n"
        "    mkdir build && cd build && cmake ..\n"
        "First remove the CMakeCache.txt and CMakeFiles/ just created here.")
endif()

# By default, project configuration will be  Release (must be done before
# the project() statement)
#
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# ----------------------------------------------------------------------------
#   VERSION (single source: inc/v32cxx.h)
# ----------------------------------------------------------------------------

# The  full  version string,  e.g.  "20261010-dev",  read from  the  same
# #define that `v32c++  --version` prints and `make  version` stamps into
# the man page. Configuration re-runs automatically when it changes.
#
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/inc/v32cxx.h" V32CXX_VERSION_LINE
     REGEX "^#define[ \t]+VERSION[ \t]+\"[^\"]*\"")
string(REGEX REPLACE "^#define[ \t]+VERSION[ \t]+\"([^\"]*)\".*" "\\1"
       V32CXX_VERSION "${V32CXX_VERSION_LINE}")
if(NOT V32CXX_VERSION)
    message(FATAL_ERROR "v32c++: could not read VERSION from inc/v32cxx.h")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/inc/v32cxx.h")

# project()  only takes  a numeric  version: derive  YYYY.MM.DD from  the
# leading date (the "-dev"/"-release" status is kept in V32CXX_VERSION).
#
if(V32CXX_VERSION MATCHES "^([0-9][0-9][0-9][0-9])([0-9][0-9])([0-9][0-9])")
    set(V32CXX_NUMERIC_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
else()
    set(V32CXX_NUMERIC_VERSION "0.0.0")
endif()

# ----------------------------------------------------------------------------
#   DEFINE THE PROJECT
# ----------------------------------------------------------------------------

project(v32cxx
        VERSION ${V32CXX_NUMERIC_VERSION}
        DESCRIPTION "C/C++ to Vircon32 C transpiler"
        HOMEPAGE_URL "https://github.com/wedge1020/v32cxx"
        LANGUAGES C)

# ----------------------------------------------------------------------------
#   IDENTIFY HOST ENVIRONMENT
# ----------------------------------------------------------------------------

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET_BITS "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(TARGET_BITS "32")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(TARGET_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TARGET_OS "mac")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TARGET_OS "linux")
else()
    set(TARGET_OS "${CMAKE_SYSTEM_NAME}")
endif()

# v32c++ uses  getopt_long() and <unistd.h>  (access, ...),  which MinGW,
# glibc, musl, macOS  and the BSDs all  provide but Visual C++  does not.
# The Vircon32 DevTools are likewise built with MSYS2 + MinGW on Windows.
#
include(CheckIncludeFile)
check_include_file(getopt.h V32CXX_HAVE_GETOPT_H)
check_include_file(unistd.h V32CXX_HAVE_UNISTD_H)
if(NOT V32CXX_HAVE_GETOPT_H OR NOT V32CXX_HAVE_UNISTD_H)
    message(FATAL_ERROR
        "v32c++: <getopt.h> or <unistd.h> not found. v32c++ needs a "
        "POSIX-style C library (getopt_long, access). On Windows, build "
        "with MSYS2 + MinGW, e.g. from a MinGW shell:\n"
        "    cmake -G \"MSYS Makefiles\" ..")
endif()

# ----------------------------------------------------------------------------
#   INSTALL LOCATIONS
# ----------------------------------------------------------------------------

# On Windows, CMake's  default prefix would be  "<Program Files>/v32cxx".
# Install under "<Program  Files>/Vircon32" instead -- the  DevTools' own
# root --  with v32c++.exe in v32tools/,  beside v32opt.exe, so  one PATH
# entry covers  the community  tools. An  explicit -DCMAKE_INSTALL_PREFIX
# always  wins.   (Only  when  the   default  has  the   usual  "<Program
# Files>/v32cxx" shape -- the parent is then whichever of "Program Files"
# or "Program  Files (x86)" CMake picked  for this toolchain,  exactly as
# for the DevTools.)
#
if(TARGET_OS STREQUAL "windows" AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    get_filename_component(V32CXX_PREFIX_NAME   "${CMAKE_INSTALL_PREFIX}" NAME)
    get_filename_component(V32CXX_PROGRAM_FILES "${CMAKE_INSTALL_PREFIX}" DIRECTORY)
    if(V32CXX_PREFIX_NAME STREQUAL PROJECT_NAME)
        set(CMAKE_INSTALL_PREFIX "${V32CXX_PROGRAM_FILES}/Vircon32" CACHE PATH
            "Install path prefix, prepended onto install directories." FORCE)
    endif()
endif()

include(GNUInstallDirs)   # bin, share/man (CMAKE_INSTALL_PREFIX defaults to /usr/local on Unix)

if(TARGET_OS STREQUAL "windows")
    set(V32CXX_TOOLS_ROOT          "v32tools")
    set(V32CXX_DEFAULT_BINDIR      "v32tools")
    set(V32CXX_DEFAULT_MANROOT     "v32tools/docs/v32c++")
    set(V32CXX_SDK_DEFAULT         "${CMAKE_INSTALL_PREFIX}/DevTools/include")
else()
    set(V32CXX_TOOLS_ROOT          "Vircon32/v32tools")
    set(V32CXX_DEFAULT_BINDIR      "${CMAKE_INSTALL_BINDIR}")
    set(V32CXX_DEFAULT_MANROOT     "${CMAKE_INSTALL_MANDIR}")
    set(V32CXX_SDK_DEFAULT         "${CMAKE_INSTALL_PREFIX}/Vircon32/DevTools/include")
endif()
set(V32CXX_DEFAULT_INCLUDEDIR      "${V32CXX_TOOLS_ROOT}/include/v32c++")
set(V32CXX_DEFAULT_DOCDIR          "${V32CXX_TOOLS_ROOT}/docs/v32c++")

set(V32CXX_INSTALL_BINDIR "${V32CXX_DEFAULT_BINDIR}" CACHE STRING
    "Where the v32c++ executable is installed (relative to CMAKE_INSTALL_PREFIX)")
# The root of a manual tree: man/man1/*.1 go into its man1/ and
# man/man3/*.3v32 into its man3/. (This replaces V32CXX_INSTALL_MANDIR,
# which named man1 itself; a new name, so an old cache can't misplace
# the pages.)
set(V32CXX_INSTALL_MANROOT "${V32CXX_DEFAULT_MANROOT}" CACHE STRING
    "Manual tree the man1/ and man3/ pages are installed under (relative to CMAKE_INSTALL_PREFIX)")
set(V32CXX_INSTALL_INCLUDEDIR "${V32CXX_DEFAULT_INCLUDEDIR}" CACHE STRING
    "Where v32/ (C++ headers) and libc/ go, relative to CMAKE_INSTALL_PREFIX or absolute; compiled into v32c++")
set(V32CXX_INSTALL_DOCDIR "${V32CXX_DEFAULT_DOCDIR}" CACHE STRING
    "Where the README and docs/ are installed (relative to CMAKE_INSTALL_PREFIX; empty = not installed)")
# Documentation language: all (default) installs every language, en only the
# English manual pages and documents, es / fr only that language (its manual
# pages directly in man1 / man3 and its documents under the plain names). The
# Vircon32 SDK's pages exist only in English and are always installed.
set(DEST_LANG "all" CACHE STRING "Documentation language to install: all, en, es or fr")
set_property(CACHE DEST_LANG PROPERTY STRINGS all en es fr)
if(DEST_LANG STREQUAL "")
    set(DEST_LANG "all")
endif()
if(NOT DEST_LANG MATCHES "^(all|en|es|fr)$")
    message(FATAL_ERROR "DEST_LANG must be all, en, es or fr (not '${DEST_LANG}')")
endif()
set(V32CXX_SDK_HEADER_DIR "" CACHE PATH
    "Fallback Vircon32 SDK header directory, compiled into v32c++ (empty = the DevTools' include/ under the prefix)")
if(V32CXX_SDK_HEADER_DIR)
    set(V32CXX_SDK_PATH "${V32CXX_SDK_HEADER_DIR}")
else()
    set(V32CXX_SDK_PATH "${V32CXX_SDK_DEFAULT}")
endif()

# The absolute header directory the transpiler is built to search.
#
if(IS_ABSOLUTE "${V32CXX_INSTALL_INCLUDEDIR}")
    set(V32CXX_HEADER_PATH "${V32CXX_INSTALL_INCLUDEDIR}")
else()
    set(V32CXX_HEADER_PATH "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_INCLUDEDIR}")
endif()

# ----------------------------------------------------------------------------
#   PARSER AND LEXER
# ----------------------------------------------------------------------------

option(V32CXX_REGENERATE_PARSER
       "Regenerate the parser and lexer with bison and flex (when both are found)" ON)

set(V32CXX_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(V32CXX_USE_SHIPPED_PARSER TRUE)
if(V32CXX_REGENERATE_PARSER)
    find_package(BISON 3.0)
    find_package(FLEX)
    if(BISON_FOUND AND FLEX_FOUND)
        file(MAKE_DIRECTORY "${V32CXX_GENERATED_DIR}")
        BISON_TARGET(v32cxx_parser "${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y"
                     "${V32CXX_GENERATED_DIR}/parser.c"
                     DEFINES_FILE "${V32CXX_GENERATED_DIR}/parser.h"
                     COMPILE_FLAGS "-t")
        FLEX_TARGET(v32cxx_lexer "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l"
                    "${V32CXX_GENERATED_DIR}/lexer.c")
        ADD_FLEX_BISON_DEPENDENCY(v32cxx_lexer v32cxx_parser)
        set(V32CXX_PARSER_SRC ${BISON_v32cxx_parser_OUTPUTS} ${FLEX_v32cxx_lexer_OUTPUTS})
        set(V32CXX_USE_SHIPPED_PARSER FALSE)
    endif()
endif()
if(V32CXX_USE_SHIPPED_PARSER)
    set(V32CXX_PARSER_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/parser.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.c")
endif()

# ----------------------------------------------------------------------------
#   SOURCE FILES
# ----------------------------------------------------------------------------

# Same set the base Makefile links into bin/v32c++.
#
set(V32CXX_CORE_SRC
    src/ast.c
    src/symtab.c
    src/prescan.c
    src/sema.c
    src/lower.c
    src/codegen.c
    src/pathutil.c
    src/compat.c
    src/generic.c
    src/stdstring.c
    src/cartxml.c
    src/buildscript.c
    src/debugmap.c
    src/macro.c
    src/cmode.c
    src/link.c
    src/archive.c
    src/warn.c
    src/main.c)

file(GLOB V32CXX_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h")

# ----------------------------------------------------------------------------
#   EXECUTABLE
# ----------------------------------------------------------------------------

add_executable(v32cxx ${V32CXX_CORE_SRC} ${V32CXX_PARSER_SRC} ${V32CXX_HEADERS})

# C99 with the platform's  extensions (gnu99): strdup/strndup/getline are
# declared by the system headers in that mode.
#
set_target_properties(v32cxx PROPERTIES
    OUTPUT_NAME "v32c++"
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON)

# v32c: the same program under the name to use for C input. A plain copy
# (not a link) so it also works on Windows; the build scripts v32c++ writes
# call whichever of the two names ran.
#
add_custom_command(TARGET v32cxx POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:v32cxx>"
            "$<TARGET_FILE_DIR:v32cxx>/v32c${CMAKE_EXECUTABLE_SUFFIX}"
    COMMENT "Creating v32c (a copy of v32c++)")

# A regenerated  parser.h (build tree) must  win over the one  shipped in
# inc/.
#
if(NOT V32CXX_USE_SHIPPED_PARSER)
    target_include_directories(v32cxx BEFORE PRIVATE "${V32CXX_GENERATED_DIR}")
endif()
target_include_directories(v32cxx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")

target_compile_definitions(v32cxx PRIVATE
    "V32CXX_INCLUDE_PATH=\"${V32CXX_HEADER_PATH}\""
    "V32CXX_SDK_INCLUDE_PATH=\"${V32CXX_SDK_PATH}\"")

# Warning flags, matching  the base Makefile where  the compiler supports
# them (flex/bison output trips -Wextra: that is theirs, not ours)
#
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(v32cxx PRIVATE -Wall -Wextra)
    set_source_files_properties(${V32CXX_PARSER_SRC} PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-function;-Wno-sign-compare;-Wno-unused-parameter")
endif()

# MinGW:  link  the  C  runtime  support  statically,  so  the  installed
# v32c++.exe needs no MinGW DLLs next to it.
#
option(V32CXX_STATIC_MINGW "Statically link MinGW runtime libraries" ON)
if(MINGW AND V32CXX_STATIC_MINGW)
    target_link_options(v32cxx PRIVATE -static)
endif()

# ----------------------------------------------------------------------------
#   SHOW BUILD INFORMATION IN PRETTY FORMAT
# ----------------------------------------------------------------------------

message(STATUS "******** v32c++ ********")
message(STATUS "Version: ${V32CXX_VERSION}")
if(NOT TARGET_OS STREQUAL "mac")
    message(STATUS "Target OS: ${TARGET_OS} ${TARGET_BITS}bit")
else()
    message(STATUS "Target OS: ${TARGET_OS} ${TARGET_BITS}bit (SDK: ${CMAKE_OSX_SYSROOT})")
endif()
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Build type: (chosen at build time)")
else()
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()
if(V32CXX_USE_SHIPPED_PARSER)
    message(STATUS "Parser: the generated src/parser.c and src/lexer.c (no bison/flex regeneration)")
else()
    message(STATUS "Parser: regenerated with bison ${BISON_VERSION} and flex ${FLEX_VERSION}")
endif()
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS " * executable  -> ${V32CXX_INSTALL_BINDIR}")
message(STATUS " * man pages   -> ${V32CXX_INSTALL_MANROOT}/man1, ${V32CXX_INSTALL_MANROOT}/man3")
message(STATUS " * headers     -> ${V32CXX_INSTALL_INCLUDEDIR}/v32 (compiled in: ${V32CXX_HEADER_PATH})")
message(STATUS " * C library   -> ${V32CXX_INSTALL_INCLUDEDIR}/libc")
if(V32CXX_INSTALL_DOCDIR)
    message(STATUS " * documents   -> ${V32CXX_INSTALL_DOCDIR}")
endif()
message(STATUS "SDK header fallback: ${V32CXX_SDK_PATH}")

# ----------------------------------------------------------------------------
#   DEFINE THE INSTALL PROCESS
# ----------------------------------------------------------------------------

install(TARGETS v32cxx
        RUNTIME DESTINATION "${V32CXX_INSTALL_BINDIR}"
        COMPONENT binaries)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/v32c${CMAKE_EXECUTABLE_SUFFIX}"
        DESTINATION "${V32CXX_INSTALL_BINDIR}"
        COMPONENT binaries)

# Every page in man/man1 and man/man3, the translations in man/es and man/fr,
# and the Vircon32 SDK's in man/vircon32/. `make version` in the base Makefile
# stamps the version and date into v32c++'s own (the translations included).
#
# Which are installed, and where, follows DEST_LANG (see above): "all" puts
# the English pages in <manroot>/man1 and man3 and the translations in the
# standard per-language trees <manroot>/es/man1 ... and <manroot>/fr/...; "en"
# only the English ones; "es" / "fr" that language's pages directly in
# <manroot>/man1 and man3. The SDK's pages always go in the plain trees.
#
# On Linux / macOS they are installed as the Makefile's `sysinstall` does:
# gzip -9'd at build time (target `manpages`, into <build>/man), mode 0644,
# and each alias page (a one-line `.so man3/<page>`) as a symbolic link to
# its target's .gz. The links are made at configure time; CMake installs a
# symbolic link as a link, so the uninstall manifest lists them too. On
# Windows, or with no gzip found, the pages are installed as they are.
#
# A "set" is <tag>|<source tree>|<install subdirectory of the manual root>
# (the SDK's pages ride with the plain set).
if(DEST_LANG STREQUAL "all")
    set(V32CXX_MAN_SETS "en|man|" "es|man/es|es" "fr|man/fr|fr")
elseif(DEST_LANG STREQUAL "en")
    set(V32CXX_MAN_SETS "en|man|")
else()
    set(V32CXX_MAN_SETS "${DEST_LANG}|man/${DEST_LANG}|")
endif()
if(NOT TARGET_OS STREQUAL "windows")
    find_program(V32CXX_GZIP gzip)
endif()
set(V32CXX_MAN_MODE OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
set(V32CXX_ALL_GZ "")
set(V32CXX_MAN_DIRS "")    # the folders to make readable, relative to the manual root
foreach(V32CXX_SET ${V32CXX_MAN_SETS})
    string(REPLACE "|" ";" V32CXX_SETL "${V32CXX_SET}")
    list(GET V32CXX_SETL 0 V32CXX_TAG)
    list(GET V32CXX_SETL 1 V32CXX_SRC)
    list(LENGTH V32CXX_SETL V32CXX_SETN)
    set(V32CXX_SUB "")
    if(V32CXX_SETN GREATER 2)
        list(GET V32CXX_SETL 2 V32CXX_SUB)
    endif()
    set(V32CXX_DEST "${V32CXX_INSTALL_MANROOT}")
    if(V32CXX_SUB)
        set(V32CXX_DEST "${V32CXX_INSTALL_MANROOT}/${V32CXX_SUB}")
        list(APPEND V32CXX_MAN_DIRS "${V32CXX_SUB}" "${V32CXX_SUB}/man1" "${V32CXX_SUB}/man3")
    endif()
    file(GLOB V32CXX_MAN1 "${CMAKE_CURRENT_SOURCE_DIR}/${V32CXX_SRC}/man1/*.1")
    file(GLOB V32CXX_MAN3 "${CMAKE_CURRENT_SOURCE_DIR}/${V32CXX_SRC}/man3/*.3v32")
    if(NOT V32CXX_SUB)
        # The Vircon32 SDK's own pages (emulator and DevTools commands, C
        # library headers) live in their own tree, man/vircon32/, and install
        # beside v32c++'s into the same man1 and man3. `make version` does not
        # stamp them (they carry the DevTools version, not v32c++'s).
        file(GLOB V32CXX_VMAN1 "${CMAKE_CURRENT_SOURCE_DIR}/man/vircon32/man1/*.1")
        file(GLOB V32CXX_VMAN3 "${CMAKE_CURRENT_SOURCE_DIR}/man/vircon32/man3/*.3v32")
        list(APPEND V32CXX_MAN1 ${V32CXX_VMAN1})
        list(APPEND V32CXX_MAN3 ${V32CXX_VMAN3})
    endif()
    if(V32CXX_GZIP)
        foreach(V32CXX_SECTION man1 man3)
            set(V32CXX_PAGES ${V32CXX_MAN1})
            if(V32CXX_SECTION STREQUAL "man3")
                set(V32CXX_PAGES ${V32CXX_MAN3})
            endif()
            set(V32CXX_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/man/${V32CXX_TAG}/${V32CXX_SECTION}")
            file(MAKE_DIRECTORY "${V32CXX_OUTDIR}")
            set(V32CXX_GZ_PAGES "")
            set(V32CXX_LINK_PAGES "")
            foreach(V32CXX_PAGE ${V32CXX_PAGES})
                get_filename_component(V32CXX_NAME "${V32CXX_PAGE}" NAME)
                # v32__<name> alias pages stand for v32::<name> (a file name
                # cannot hold "::" on Windows); installed under the real name
                if(NOT TARGET_OS STREQUAL "windows")
                    string(REGEX REPLACE "^v32__" "v32::" V32CXX_NAME "${V32CXX_NAME}")
                endif()
                file(STRINGS "${V32CXX_PAGE}" V32CXX_FIRST LIMIT_COUNT 1)
                if(V32CXX_FIRST MATCHES "^\\.so [^/]*/(.*)$")
                    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink
                                    "${CMAKE_MATCH_1}.gz" "${V32CXX_OUTDIR}/${V32CXX_NAME}.gz")
                    list(APPEND V32CXX_LINK_PAGES "${V32CXX_OUTDIR}/${V32CXX_NAME}.gz")
                else()
                    add_custom_command(
                        OUTPUT "${V32CXX_OUTDIR}/${V32CXX_NAME}.gz"
                        COMMAND "${CMAKE_COMMAND}" -E copy "${V32CXX_PAGE}" "${V32CXX_OUTDIR}/${V32CXX_NAME}"
                        COMMAND "${V32CXX_GZIP}" -9 -n -f "${V32CXX_OUTDIR}/${V32CXX_NAME}"
                        DEPENDS "${V32CXX_PAGE}"
                        COMMENT "gzip ${V32CXX_TAG}/${V32CXX_SECTION}/${V32CXX_NAME}"
                        VERBATIM)
                    list(APPEND V32CXX_GZ_PAGES "${V32CXX_OUTDIR}/${V32CXX_NAME}.gz")
                    list(APPEND V32CXX_ALL_GZ "${V32CXX_OUTDIR}/${V32CXX_NAME}.gz")
                endif()
            endforeach()
            install(FILES ${V32CXX_GZ_PAGES} ${V32CXX_LINK_PAGES}
                    DESTINATION "${V32CXX_DEST}/${V32CXX_SECTION}"
                    PERMISSIONS ${V32CXX_MAN_MODE}
                    COMPONENT documentation)
        endforeach()
    else()
        install(FILES ${V32CXX_MAN1}
                DESTINATION "${V32CXX_DEST}/man1"
                PERMISSIONS ${V32CXX_MAN_MODE}
                COMPONENT documentation)
        foreach(V32CXX_PAGE ${V32CXX_MAN3})
            get_filename_component(V32CXX_NAME "${V32CXX_PAGE}" NAME)
            if(NOT TARGET_OS STREQUAL "windows")
                string(REGEX REPLACE "^v32__" "v32::" V32CXX_NAME "${V32CXX_NAME}")
            endif()
            install(FILES "${V32CXX_PAGE}"
                    DESTINATION "${V32CXX_DEST}/man3"
                    RENAME "${V32CXX_NAME}"
                    PERMISSIONS ${V32CXX_MAN_MODE}
                    COMPONENT documentation)
        endforeach()
    endif()
endforeach()
if(V32CXX_GZIP)
    add_custom_target(manpages ALL DEPENDS ${V32CXX_ALL_GZ})
endif()

file(GLOB V32CXX_V32_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/v32/*.hpp")
install(FILES ${V32CXX_V32_HEADERS}
        DESTINATION "${V32CXX_INSTALL_INCLUDEDIR}/v32"
        COMPONENT headers)

file(GLOB V32CXX_LIBC "${CMAKE_CURRENT_SOURCE_DIR}/libc/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/libc/*.c")
install(FILES ${V32CXX_LIBC}
        DESTINATION "${V32CXX_INSTALL_INCLUDEDIR}/libc"
        COMPONENT headers)
install(DIRECTORY libc/sys
        DESTINATION "${V32CXX_INSTALL_INCLUDEDIR}/libc"
        COMPONENT headers)

if(V32CXX_INSTALL_DOCDIR)
    # The documents, in the language(s) DEST_LANG asks for. They are staged
    # at configure time into <build>/docs, then installed from there: for
    # "all" as they are; for "en" without the language-links line and the
    # translations; for "es" / "fr" the translation under the plain name
    # (README.es.md -> README.md), with that line dropped and links to X.es.md
    # rewritten to X.md, next to the documents that exist only in English.
    # (scripts/install_docs.sh does the same for the Makefile build.)
    set(V32CXX_DOCSTAGE "${CMAKE_CURRENT_BINARY_DIR}/docs")
    file(REMOVE_RECURSE "${V32CXX_DOCSTAGE}")
    file(MAKE_DIRECTORY "${V32CXX_DOCSTAGE}")
    file(GLOB V32CXX_DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/README*.md" "${CMAKE_CURRENT_SOURCE_DIR}/docs/*.md")
    foreach(V32CXX_DOC ${V32CXX_DOC_FILES})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${V32CXX_DOC}")
        get_filename_component(V32CXX_DOCNAME "${V32CXX_DOC}" NAME)
        get_filename_component(V32CXX_DOCSRCDIR "${V32CXX_DOC}" DIRECTORY)
        set(V32CXX_DOCOUT "")      # the staged name, when this file is installed
        set(V32CXX_DOCEDIT FALSE)  # rewritten for a single language?
        if(V32CXX_DOCNAME MATCHES "^(.*)\\.(es|fr)\\.md$")
            # a translation: as it is for "all", the plain name for its language
            if(DEST_LANG STREQUAL "all")
                set(V32CXX_DOCOUT "${V32CXX_DOCNAME}")
            elseif(DEST_LANG STREQUAL CMAKE_MATCH_2)
                set(V32CXX_DOCOUT "${CMAKE_MATCH_1}.md")
                set(V32CXX_DOCEDIT TRUE)
            endif()
        else()
            string(REGEX REPLACE "\\.md$" "" V32CXX_DOCSTEM "${V32CXX_DOCNAME}")
            if(DEST_LANG STREQUAL "all")
                set(V32CXX_DOCOUT "${V32CXX_DOCNAME}")
            elseif(NOT EXISTS "${V32CXX_DOCSRCDIR}/${V32CXX_DOCSTEM}.${DEST_LANG}.md")
                # English-only document, or English wanted
                set(V32CXX_DOCOUT "${V32CXX_DOCNAME}")
                set(V32CXX_DOCEDIT TRUE)
            endif()
        endif()
        if(NOT V32CXX_DOCOUT)
            continue()
        endif()
        if(V32CXX_DOCEDIT)
            file(READ "${V32CXX_DOC}" V32CXX_TEXT)
            string(REGEX REPLACE "[^\n]*<!-- langlinks -->[^\n]*\n?" "" V32CXX_TEXT "${V32CXX_TEXT}")
            string(REPLACE ".es.md" ".md" V32CXX_TEXT "${V32CXX_TEXT}")
            string(REPLACE ".fr.md" ".md" V32CXX_TEXT "${V32CXX_TEXT}")
            file(WRITE "${V32CXX_DOCSTAGE}/${V32CXX_DOCOUT}" "${V32CXX_TEXT}")
        else()
            configure_file("${V32CXX_DOC}" "${V32CXX_DOCSTAGE}/${V32CXX_DOCOUT}" COPYONLY)
        endif()
    endforeach()
    file(GLOB V32CXX_DOC_STAGED "${V32CXX_DOCSTAGE}/*.md")
    install(FILES ${V32CXX_DOC_STAGED} LICENSE
            DESTINATION "${V32CXX_INSTALL_DOCDIR}"
            COMPONENT documentation)
endif()

# Make everything installed readable by ordinary users, whatever umask the
# install ran under: sudo often has 077, and the directories CMake creates
# are masked by it (CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS is too).
# Only adds go+rX -- never removes a permission -- on v32c++'s own trees,
# the shared Vircon32/v32tools folders above them, the prefix, its bin/
# and share/, and the man1/man3 folders. Linux / macOS only; DESTDIR-aware.
if(NOT TARGET_OS STREQUAL "windows")
    set(V32CXX_READABLE_TREES
        "${V32CXX_HEADER_PATH}")
    set(V32CXX_READABLE_DIRS
        "${V32CXX_HEADER_PATH}/.."
        "${V32CXX_HEADER_PATH}/../.."
        "${V32CXX_HEADER_PATH}/../../.."
        "${CMAKE_INSTALL_PREFIX}"
        "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_BINDIR}"
        "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}/.."
        "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}"
        "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}/man1"
        "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}/man3")
    foreach(V32CXX_D ${V32CXX_MAN_DIRS})
        list(APPEND V32CXX_READABLE_DIRS "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}/${V32CXX_D}")
    endforeach()
    if(V32CXX_INSTALL_DOCDIR)
        list(APPEND V32CXX_READABLE_TREES "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_DOCDIR}")
        list(APPEND V32CXX_READABLE_DIRS "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_DOCDIR}/..")
    endif()
    install(CODE "
        foreach(T ${V32CXX_READABLE_TREES})
            if(IS_DIRECTORY \"\$ENV{DESTDIR}\${T}\")
                execute_process(COMMAND chmod -R go+rX \"\$ENV{DESTDIR}\${T}\")
            endif()
        endforeach()
        foreach(D ${V32CXX_READABLE_DIRS})
            if(IS_DIRECTORY \"\$ENV{DESTDIR}\${D}\")
                execute_process(COMMAND chmod go+rx \"\$ENV{DESTDIR}\${D}\")
            endif()
        endforeach()
    " COMPONENT documentation)
endif()

if(TARGET_OS STREQUAL "windows")
    install(CODE "message(STATUS \"v32c++: add ${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_BINDIR} to your PATH\")")
endif()

# `cmake --build . --target uninstall`  removes whatever the last install
# put   down   (CMake   has   no   built-in   uninstall;   this   replays
# install_manifest.txt), then the directories that leaves empty.
#
if(TARGET_OS STREQUAL "windows")
    set(V32CXX_UNIX_VIRCON32_DIR "")                              # that is the prefix
    # the manual pages live under the documents there: prune their folders
    set(V32CXX_UNINSTALL_MAN1 "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}/man1")
    set(V32CXX_UNINSTALL_MAN3 "${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}/man3")
else()
    set(V32CXX_UNIX_VIRCON32_DIR "${CMAKE_INSTALL_PREFIX}/Vircon32")
    # share/man/man1 and man3 are the system's: never removed
    set(V32CXX_UNINSTALL_MAN1 "")
    set(V32CXX_UNINSTALL_MAN3 "")
endif()
# the per-language manual trees ("all" installs man/es -> <manroot>/es/man1 ...):
# pruned when the uninstall leaves them empty (they are v32c++'s doing; the
# plain man1 and man3 above are not)
set(V32CXX_UNINSTALL_LANGDIRS "")
foreach(V32CXX_D ${V32CXX_MAN_DIRS})
    # quoted, one per word, for the foreach in the script (paths may hold spaces)
    set(V32CXX_UNINSTALL_LANGDIRS "\"${CMAKE_INSTALL_PREFIX}/${V32CXX_INSTALL_MANROOT}/${V32CXX_D}\" ${V32CXX_UNINSTALL_LANGDIRS}")
endforeach()
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        @ONLY)
    add_custom_target(uninstall
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        COMMENT "Uninstalling v32c++")
endif()

# ----------------------------------------------------------------------------
#   TESTS (ctest)
# ----------------------------------------------------------------------------

# The binary  reports the  expected version,  and every  sample the  base
# Makefile's `test`  recipe transpiles is a  test, read from  that recipe
# (one  source  of   truth):  each  `$(BIN)  <flags>   -o  out/NN<kind>.c
# tests/NNsample.<ext>` line becomes  a test with the same  flags, run in
# the source tree, writing into the  build tree. Lines starting `-$(BIN)`
# are the  deliberately invalid  samples, which  must FAIL  to transpile.
# Checking the  output with  the real Vircon32  toolchain is  still `make
# realcheck`.
#
option(BUILD_TESTING "Define the ctest tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_test(NAME version COMMAND v32cxx --version)
    set_tests_properties(version PROPERTIES PASS_REGULAR_EXPRESSION "${V32CXX_VERSION}")

    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Makefile" V32CXX_TEST_LINES
         REGEX "^\t-?(@if.*then |V32CXX_SDK_INCLUDE=[^ ]* )?\\$\\(BIN\\)[^\n]*tests/[0-9]+sample\\.c(pp)?")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/out")
    foreach(line IN LISTS V32CXX_TEST_LINES)
        if(line MATCHES "^\t@if" OR line MATCHES "V32CXX_SDK_INCLUDE=")
            continue()   # sample 98 needs the SDK headers: `make test` handles it
        endif()
        string(REGEX MATCH "tests/([0-9]+)sample\\.(cpp|c)" src "${line}")
        set(num "${CMAKE_MATCH_1}")
        string(REGEX MATCH " out/([0-9]+[a-z]+)\\.c " out "${line}")
        set(outname "${CMAKE_MATCH_1}")
        # the flags between $(BIN) and the output path; the last one ends in
        # the -o that names it, alone (`-o`) or clustered (`-bgo`)
        string(REGEX REPLACE "^\t-?\\$\\(BIN\\)[ ]+(.*)[ ]+out/[0-9]+[a-z]+\\.c .*$" "\\1" flags "${line}")
        separate_arguments(flags)
        list(REMOVE_ITEM flags "-vvv" "")
        list(LENGTH flags nflags)
        if(nflags GREATER 0)
            math(EXPR last "${nflags} - 1")
            list(GET flags ${last} lastflag)
            list(REMOVE_AT flags ${last})
            if(NOT lastflag STREQUAL "-o")
                string(REGEX REPLACE "o$" "" lastflag "${lastflag}")
                list(APPEND flags "${lastflag}")
            endif()
        endif()
        add_test(NAME "sample${num}"
                 COMMAND v32cxx ${flags} -o "${CMAKE_CURRENT_BINARY_DIR}/out/${outname}.c" "${src}"
                 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        if(line MATCHES "^\t-\\$\\(BIN\\)")
            set_tests_properties("sample${num}" PROPERTIES WILL_FAIL TRUE)
        endif()
    endforeach()
endif()

# ----------------------------------------------------------------------------
#   DEFINE THE PACKAGE CREATION WITH CPACK
# ----------------------------------------------------------------------------

# `cpack`  (from  the build  directory)  makes  binary packages,  as  the
# Vircon32 DevTools  do: .deb/.rpm/.tar.gz  on Linux,  .tar.gz on  macOS,
# .zip on Windows.  Packages carry the full version string  in their file
# name; the package-manager version is the numeric YYYY.MM.DD date.
#
set(CPACK_PACKAGE_NAME "v32cxx")
set(CPACK_PACKAGE_VENDOR "Matthew Haas")
set(CPACK_PACKAGE_CONTACT "Matthew Haas")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C/C++ to Vircon32 C transpiler")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/wedge1020/v32cxx")
set(CPACK_PACKAGE_FILE_NAME "v32cxx-${V32CXX_VERSION}-${TARGET_OS}${TARGET_BITS}")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(TARGET_OS STREQUAL "linux")
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Matthew Haas")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    set(CPACK_RPM_PACKAGE_LICENSE "BSD-3-Clause")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_GENERATOR "TGZ")
    find_program(V32CXX_DPKG_DEB dpkg-deb)
    if(V32CXX_DPKG_DEB)
        list(APPEND CPACK_GENERATOR "DEB")
    endif()
    find_program(V32CXX_RPMBUILD rpmbuild)
    if(V32CXX_RPMBUILD)
        list(APPEND CPACK_GENERATOR "RPM")
    endif()
elseif(TARGET_OS STREQUAL "windows")
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
