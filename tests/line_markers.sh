#!/bin/sh
# --line-markers: usage  tests/line_markers.sh <v32c++ binary> [<v32cc binary>]
#   1. without the option the C has no #line;
#   2. with it, every #line names tests/line_markers.cpp and the lines are within the file;
#   3. apart from the #line lines the two outputs are identical (markers cost nothing);
#   4. if v32cc is given (or V32CC is set): v32cc -g turns them into ';@src line_markers.cpp:N' for the
#      user's lines 2..6 and none in a function that is not in the file.
set -eu
bin=${1:?usage: line_markers.sh <v32c++ binary> [<v32cc binary>]}
cc=${2:-${V32CC:-}}
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")
[ -n "$cc" ] && cc=$(cd "$(dirname "$cc")" && pwd)/$(basename "$cc")
cd "$(dirname "$0")"
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT INT TERM
fail() { echo "FAIL line_markers: $*" >&2; exit 1; }
inc=
"$bin" $inc -o "$t/plain.c" line_markers.cpp >/dev/null 2>&1 || fail "transpile"
"$bin" $inc --line-markers -o "$t/lm.c" line_markers.cpp >/dev/null 2>&1 || fail "transpile --line-markers"
! grep -q '^#line' "$t/plain.c" || fail "#line without the option"
n=$(grep -c '^#line' "$t/lm.c"); [ "$n" -ge 4 ] || fail "too few #line ($n)"
bad=$(grep '^#line' "$t/lm.c" | grep -v '^#line [0-9][0-9]* "line_markers.cpp"$' || true)
[ -z "$bad" ] || fail "unexpected #line: $bad"
mx=$(grep '^#line' "$t/lm.c" | awk '{print $2}' | sort -n | tail -1); [ "$mx" -le 6 ] || fail "line $mx beyond the file"
grep -v '^#line' "$t/lm.c" > "$t/lm.stripped"
cmp -s "$t/plain.c" "$t/lm.stripped" || fail "output differs apart from #line"
if [ -n "$cc" ]; then
  "$cc" -g --target-compat=v32cxx -S "$t/lm.c" -o "$t/lm.asm" ${V32CC_INCLUDE:+-I "$V32CC_INCLUDE"} >/dev/null 2>&1 || fail "v32cc rejected the output"
  for l in 2 3 4 5 6; do grep -q "^;@src line_markers.cpp:$l\$" "$t/lm.asm" || fail "no ;@src line_markers.cpp:$l"; done
  ! grep '^;@src' "$t/lm.asm" | grep -qv 'line_markers.cpp\|^;@src -$' || fail "a ;@src names another file"
fi
echo "ok   line_markers"
