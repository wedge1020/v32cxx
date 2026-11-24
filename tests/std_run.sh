#!/bin/sh
# usage: sh tests/std_run.sh bin/v32c++ NAME [v32c++ flags...]
# Builds tests/NAME.cpp (or NAME.c) as STANDARD C (the real Vircon32 toolchain is not
# needed), links it with tests/stddriver.c and runs it: the sample is
# self-checking (test_errors must end at 0). Same idea as globals_destroy.sh.
V32=${1:-bin/v32c++}
NAME=$2
shift 2
CC=${CC:-cc}
if ! command -v "$CC" >/dev/null 2>&1; then echo "skip std_run $NAME (no C compiler)"; exit 0; fi
T=${TMPDIR:-/tmp}/v32sr.$$
mkdir -p "$T" || exit 1
rc=0
SRC="tests/$NAME.cpp"; [ -f "$SRC" ] || SRC="tests/$NAME.c"
"$V32" --no-build-scripts --target=standard "$@" -o "$T/p.c" "$SRC" >"$T/log" 2>&1 || { cat "$T/log"; rm -rf "$T"; exit 1; }
if "$CC" -w -Dmain=real_main -c "$T/p.c" -o "$T/p.o" 2>"$T/cc" &&
   "$CC" tests/stddriver.c "$T/p.o" -o "$T/p.exe"; then
    if "$T/p.exe"; then echo "ok   std_run $NAME $*"; else echo "FAIL std_run $NAME"; rc=1; fi
else
    cat "$T/cc"; rc=1
fi
rm -rf "$T"
exit $rc
