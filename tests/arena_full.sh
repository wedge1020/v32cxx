#!/bin/sh
# usage: sh tests/arena_full.sh bin/v32c++
# tests/181full.cpp, built as standard C with --alloc=arena --heap-words=64,
# fills the arena: it must stop with exit status 1 and a message on stderr.
V32=${1:-bin/v32c++}
CC=${CC:-cc}
if ! command -v "$CC" >/dev/null 2>&1; then echo "skip arena_full (no C compiler)"; exit 0; fi
T=${TMPDIR:-/tmp}/v32af.$$
mkdir -p "$T" || exit 1
rc=0
"$V32" --no-build-scripts --target=standard --alloc=arena --heap-words=64 -I . -o "$T/p.c" tests/181full.cpp >"$T/log" 2>&1 || { cat "$T/log"; rm -rf "$T"; exit 1; }
if "$CC" -w -Dmain=real_main -c "$T/p.c" -o "$T/p.o" 2>"$T/cc" && "$CC" tests/stddriver.c "$T/p.o" -o "$T/p.exe"; then
    "$T/p.exe" >"$T/out" 2>"$T/err"
    st=$?
    if [ "$st" = 1 ] && grep -q "out of memory" "$T/err" && ! grep -q FAIL "$T/out"; then echo "ok   arena_full"; else echo "FAIL arena_full (status $st): $(cat "$T/err" "$T/out")"; rc=1; fi
else
    cat "$T/cc"; rc=1
fi
rm -rf "$T"
exit $rc
