#!/bin/sh
# ----------------------------------------------------------------------------
#   Release smoke test for a built v32c++ binary
#
#   usage: scripts/smoke_test.sh <v32c++ binary> [<version>]
#
#   Stages the binary with scripts/stage_package.sh (so the package layout is
#   what is tested), then, from an unrelated directory:
#     1. `v32c++ --version` must run (and print <version>, when given);
#     2. a C++ program that #includes <v32/math.hpp> and uses v32::clamp must
#        transpile, finding the headers in the staged include/v32c++ through
#        $V32CXX_INCLUDE (the compiled-in default points at the install
#        location, which a CI runner does not have);
#     3. a C program that #includes the staged C library (v32libc.c) must
#        transpile in the same way;
#     4. every manual page alias (`.so man3/<page>`) in the staged docs must
#        point at a page that is there.
#
#   Plain POSIX sh: Linux, macOS, MSYS2, OpenBSD (ksh) and Haiku. Run from the
#   top of the source tree. Used by .github/workflows/release-builds.yml.
# ----------------------------------------------------------------------------
set -eu

if [ $# -lt 1 ]; then
    echo "usage: $0 <v32c++ binary> [<version>]" >&2
    exit 2
fi
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
want=${2:-}
src=$(pwd)

fail() {
    echo "smoke test FAILED: $*" >&2
    exit 1
}

[ -f "$bin" ] || fail "no binary at $bin"

tmp=$(mktemp -d 2>/dev/null || mktemp -d -t v32cxx-smoke)
trap 'rm -rf "$tmp"' EXIT INT TERM

sh "$src/scripts/stage_package.sh" "$bin" "$tmp/pkg" >/dev/null || fail "could not stage the package"
case "$bin" in
    *.exe) exe=v32c++.exe; exec_c=v32c.exe ;;
    *)     exe=v32c++;     exec_c=v32c ;;
esac
mkdir -p "$tmp/work"
cd "$tmp/work"

# 1. The binary runs and reports the expected version.
out=$("$tmp/pkg/$exe" --version | head -n 1) || fail "'$exe --version' did not run"
echo "$out"
if [ -n "$want" ]; then
    case "$out" in
        *"$want"*) ;;
        *) fail "--version printed '$out', expected '$want'" ;;
    esac
fi

V32CXX_INCLUDE="$tmp/pkg/include/v32c++"
# a native Windows v32c++.exe wants a Windows path (MSYS2 has cygpath)
if command -v cygpath >/dev/null 2>&1; then
    V32CXX_INCLUDE=$(cygpath -m "$V32CXX_INCLUDE")
fi
export V32CXX_INCLUDE

# 2. The C++ headers are found through the include path.
cat > smoke.cpp <<'SRC'
#include <v32/math.hpp>

void main()
{
    int level = v32::clamp( 150, 0, 100 );
    level = level + 1;
}
SRC
"$tmp/pkg/$exe" -o smoke.c smoke.cpp > smoke.log 2>&1 || { cat smoke.log; fail "a program using <v32/math.hpp> did not transpile"; }
grep -q "clamp" smoke.c || fail "the transpiled program does not mention clamp"

# 3. The C library for C input is found too.
cat > smokec.c <<'SRC'
#include <v32libc.c>

void main()
{
    int length = strlen( "smoke" );
    length = length + 1;
}
SRC
"$tmp/pkg/$exec_c" --version >/dev/null 2>&1 || fail "'$exec_c --version' did not run"
"$tmp/pkg/$exec_c" -o smokec_out.c smokec.c > smokec.log 2>&1 || { cat smokec.log; fail "a C program using the C library did not transpile"; }

# 4. Every alias page in the staged manual tree points at a page that exists.
root="$tmp/pkg/docs/v32c++"
bad=0
for page in "$root"/man1/* "$root"/man3/*; do
    first=$(head -n 1 "$page")
    case "$first" in
        ".so "*)
            target=${first#.so }
            [ -f "$root/$target" ] || { echo "alias $page -> $target is dangling" >&2; bad=1; } ;;
    esac
done
[ "$bad" = 0 ] || fail "dangling manual page aliases"

echo "smoke test passed"
