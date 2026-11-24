#!/bin/sh
# usage: sh tests/globals_destroy.sh bin/v32c++
# Builds tests/177sample.cpp, 177ret.cpp and 177end.cpp as STANDARD C (the real
# Vircon32 toolchain is not needed), links each with tests/177driver.c and runs
# it: destructors of file-scope objects run after main() has returned, so only
# a driver outside main() can see them all.
V32=${1:-bin/v32c++}
CC=${CC:-cc}
if ! command -v "$CC" >/dev/null 2>&1; then echo "skip globals_destroy (no C compiler)"; exit 0; fi
T=${TMPDIR:-/tmp}/v32gd.$$
mkdir -p "$T" || exit 1
rc=0
for t in 177sample 177ret 177end; do
    "$V32" --no-build-scripts --target=standard -o "$T/$t.c" "tests/$t.cpp" >"$T/$t.log" 2>&1 || { cat "$T/$t.log"; rc=1; continue; }
    "$CC" -w -Dmain=real_main -c "$T/$t.c" -o "$T/$t.o" 2>"$T/$t.cc" || { cat "$T/$t.cc"; rc=1; continue; }
    "$CC" tests/177driver.c "$T/$t.o" -o "$T/$t.exe" || { rc=1; continue; }
    "$T/$t.exe" || { echo "FAIL $t"; rc=1; continue; }
    echo "ok   globals_destroy $t"
done
rm -rf "$T"
exit $rc
