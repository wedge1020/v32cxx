#!/bin/sh
# v32c++ -g with v32cc as the compiler: usage  tests/debug_flow.sh <v32c++ binary>
#   1. -g --compiler=v32cc writes #line markers by itself (no --line-markers);
#   2. the generated Make.sh calls  v32cc -S --target-compat=v32cxx -g  and  v32as -g, and repeats --compiler for its own v32c++ run;
#   3. with the stock compiler (the default) -g writes no #line, and Make.sh calls `compile` as before;
#   4. --compiler-style and the V32CXX_COMPILER variable pick the v32cc dialect too.
set -eu
bin=${1:?usage: debug_flow.sh <v32c++ binary>}
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")
cd "$(dirname "$0")"
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT INT TERM
fail() { echo "FAIL debug_flow: $*" >&2; exit 1; }
cp line_markers.cpp "$t/lm.cpp"
( cd "$t" && "$bin" -g --compiler=v32cc --assembler=v32as -o lm.c lm.cpp >/dev/null 2>&1 ) || fail "transpile -g --compiler=v32cc"
grep -q '^#line' "$t/lm.c" || fail "no #line with -g and v32cc"
grep -q '^v32cc -S --target-compat=v32cxx -g lm.c -o lm.asm' "$t/Make.sh" || fail "Make.sh compile line: $(grep v32cc "$t/Make.sh")"
grep -q '^v32as -g -B -c lm.asm -o lm.vbin' "$t/Make.sh" || fail "Make.sh assemble line"
grep -q '^v32c++ -g --compiler=v32cc ' "$t/Make.sh" || fail "Make.sh transpile line"
rm -f "$t"/Make.* "$t"/lm.*[!p]; cp line_markers.cpp "$t/lm.cpp"
( cd "$t" && "$bin" -g -o lm.c lm.cpp >/dev/null 2>&1 ) || fail "transpile -g (stock)"
! grep -q '^#line' "$t/lm.c" || fail "#line with -g and the stock compiler"
grep -q '^compile lm.c -o lm.asm' "$t/Make.sh" || fail "stock Make.sh compile line"
rm -f "$t"/Make.*
( cd "$t" && "$bin" -g --compiler=./tools/mycc --compiler-style=v32cc -o lm.c lm.cpp >/dev/null 2>&1 ) || fail "transpile --compiler-style"
grep -q '^./tools/mycc -S --target-compat=v32cxx -g ' "$t/Make.sh" || fail "--compiler-style=v32cc"
echo "ok   debug_flow"
