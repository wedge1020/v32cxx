#!/bin/sh
# ----------------------------------------------------------------------------
#   Lay out a v32c++ release package
#
#   usage: scripts/stage_package.sh <v32c++ binary> <destination directory>
#
#   Builds, in <destination directory>, the tree the release zips contain.
#   It mirrors the Windows install layout of the CMake build, so a zip can be
#   unpacked straight into <Program Files>/Vircon32/v32tools (or, on Linux
#   and macOS, /usr/local/Vircon32/v32tools):
#
#       v32c++[.exe]
#       LICENSE
#       include/v32c++/v32/*.hpp      the C++ headers
#       include/v32c++/libc/          the C library for C input (+ sys/)
#       docs/v32c++/README.md, *.md   the README and the documents
#       docs/v32c++/man1/*.1          manual pages, section 1: v32c++ and
#                                     the Vircon32 emulator / DevTools
#       docs/v32c++/man3/*.3v32       section 3v32: the v32c++ headers and
#                                     the Vircon32 C library headers
#       docs/v32c++/es/man1, man3     the Spanish manual pages (v32c++'s own;
#       docs/v32c++/fr/man1, man3     the SDK pages are English only), French
#                                     likewise; README.es.md, README.fr.md and
#                                     docs/*.es.md, *.fr.md are the translated
#                                     documents
#
#   man1 and man3 are kept as directories (not flattened) because an alias
#   page is a one-line `.so man3/<page>` relative to the manual root; read
#   them in place with  man -M docs/v32c++ 3v32 v32libc , or install them
#   with `make sysinstall` / `cmake --install` from a source checkout.
#
#   Plain POSIX sh, run from the top of the source tree: Linux, macOS,
#   MSYS2, OpenBSD and Haiku. Used by .github/workflows/release-builds.yml.
# ----------------------------------------------------------------------------
set -eu

if [ $# -ne 2 ]; then
    echo "usage: $0 <v32c++ binary> <destination directory>" >&2
    exit 2
fi
bin=$1
dest=$2

fail() {
    echo "stage_package: $*" >&2
    exit 1
}

[ -f "$bin" ]          || fail "no binary at $bin"
[ -d v32 ] && [ -d libc ] && [ -d man ] || fail "run this from the top of the v32c++ source tree"

case "$bin" in
    *.exe) exe=v32c++.exe ;;
    *)     exe=v32c++ ;;
esac

rm -rf "$dest"
mkdir -p "$dest/include/v32c++/v32" "$dest/include/v32c++/libc/sys" \
         "$dest/docs/v32c++/man1" "$dest/docs/v32c++/man3"
for lang in es fr; do
    mkdir -p "$dest/docs/v32c++/$lang/man1" "$dest/docs/v32c++/$lang/man3"
done

cp "$bin" "$dest/$exe"
[ -f LICENSE ] && cp LICENSE "$dest/"

cp v32/*.hpp                "$dest/include/v32c++/v32/"
cp libc/*.h libc/*.c        "$dest/include/v32c++/libc/"
cp libc/sys/*.h             "$dest/include/v32c++/libc/sys/"

cp README*.md               "$dest/docs/v32c++/"
cp docs/*.md                "$dest/docs/v32c++/"

# v32c++'s own pages, then the Vircon32 SDK's (man/vircon32/): same
# man1 / man3, no page name is shared between the two sets.
for set in man man/vircon32; do
    for page in "$set"/man1/*.1;     do [ -f "$page" ] && cp "$page" "$dest/docs/v32c++/man1/"; done
    for page in "$set"/man3/*.3v32;  do [ -f "$page" ] && cp "$page" "$dest/docs/v32c++/man3/"; done
done

# The Spanish and French pages: v32c++'s own only, in their own man roots
# (their alias pages resolve relative to that root).
for lang in es fr; do
    for page in man/$lang/man1/*.1;    do [ -f "$page" ] && cp "$page" "$dest/docs/v32c++/$lang/man1/"; done
    for page in man/$lang/man3/*.3v32; do [ -f "$page" ] && cp "$page" "$dest/docs/v32c++/$lang/man3/"; done
done

# Sanity: the pieces a package is useless without.
ls "$dest"/include/v32c++/v32/*.hpp >/dev/null 2>&1 || fail "no C++ headers were packaged"
[ -f "$dest/docs/v32c++/man1/v32c++.1" ]            || fail "v32c++.1 was not packaged"
[ -f "$dest/docs/v32c++/man1/compile.1" ]           || fail "the Vircon32 pages were not packaged"
[ -f "$dest/docs/v32c++/man3/string.h.3v32" ]       || fail "the Vircon32 C library pages were not packaged"

for lang in es fr; do
    [ -f "$dest/docs/v32c++/$lang/man1/v32c++.1" ] || fail "the $lang manual pages were not packaged"
    [ -f "$dest/docs/v32c++/README.$lang.md" ]     || fail "README.$lang.md was not packaged"
done

echo "staged $dest: $(ls "$dest/docs/v32c++/man1" | wc -l | tr -d ' ') section-1 and" \
     "$(ls "$dest/docs/v32c++/man3" | wc -l | tr -d ' ') section-3v32 manual pages"
