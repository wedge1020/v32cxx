#!/bin/sh
# usage: sh tests/gc_full.sh bin/v32c++
# tests/182full.cpp, built as standard C with --alloc=gc --heap-words=2048,
# fills the gc heap: it must stop with exit status 1 and a message on stderr.
V32=${1:-bin/v32c++}
CC=${CC:-cc}
if ! command -v "$CC" >/dev/null 2>&1; then echo "skip gc_full (no C compiler)"; exit 0; fi
T=${TMPDIR:-/tmp}/v32gf.$$
mkdir -p "$T" || exit 1
rc=0
"$V32" --no-build-scripts --target=standard --alloc=gc --heap-words=2048 -I . -o "$T/p.c" tests/182full.cpp >"$T/log" 2>&1 || { cat "$T/log"; rm -rf "$T"; exit 1; }
if "$CC" -w -Dmain=real_main -c "$T/p.c" -o "$T/p.o" 2>"$T/cc" && "$CC" tests/stddriver.c "$T/p.o" -o "$T/p.exe"; then
    "$T/p.exe" >"$T/out" 2>"$T/err"
    st=$?
    if [ "$st" = 1 ] && grep -q "out of memory" "$T/err" && ! grep -q FAIL "$T/out"; then echo "ok   gc_full"; else echo "FAIL gc_full (status $st): $(cat "$T/err" "$T/out")"; rc=1; fi
else
    cat "$T/cc"; rc=1
fi
rm -rf "$T"
exit $rc
