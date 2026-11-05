#!/bin/sh
# *****************************************************************************
#  tools/vircon32/check.sh — verify `make test` output with the REAL toolchain
#
#  Run after `make test` (the Makefile's `realcheck` target does both):
#
#   1. every out/NNprogram.c goes through the real Vircon32 C compiler,
#      assembler and ROM packer -- a transpile that "worked" but produced
#      C the real compiler rejects fails here;
#   2. every sample whose source declares `int test_errors` is a
#      self-checking program: its cartridge is booted headless in v32sim with the
#      standard BIOS and a fresh memory card, run until the CPU halts, and
#      the RAM word holding test_errors must read 0 (it starts at -1, so a
#      program that crashes or never reaches the end also fails). A sample
#      with a tests/NNsample.input next to it is played with that input
#      script -- see 119sample.
#
#  Needs tools/vircon32/bin (build-tools.sh) and a v32sim checkout (V32SIM_HOME, default ../v32sim).
#  Exit status: 0 if everything passed, 1 otherwise.
# *****************************************************************************

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BIN="$HERE/bin"
OUT="$ROOT/out"

if [ ! -x "$BIN/compile" ] || [ ! -x "$BIN/assemble" ] || [ ! -x "$BIN/packrom" ]; then
    echo "check.sh: toolchain not built -- run tools/vircon32/build-tools.sh first" >&2
    exit 1
fi

failed=0
built=0
unpacked=0

for c in "$OUT"/*program.c; do
    [ -e "$c" ] || continue
    n=$(basename "$c" .c)
    if ! ( cd "$OUT" &&
           "$BIN/compile"  "$n.c"   -o "$n.asm"  > "$n.compile.log" 2>&1 &&
           "$BIN/assemble" "$n.asm" -o "$n.vbin" > "$n.asm.log"     2>&1 ); then
        echo "FAIL (toolchain) $n -- see out/$n.compile.log / out/$n.asm.log"
        failed=$((failed + 1))
        continue
    fi
    built=$((built + 1))
    # Packing also needs every #texture/#sound asset converted to
    # .vtex/.vsnd next to the XML. Test samples that declare assets don't
    # ship them, so a pack that fails ONLY for a missing asset is reported
    # but not counted as a failure; any other packer error is.
    # The XML holds bare file names (packrom resolves paths relative to the
    # XML), so it packs as is; the sed is a no-op kept for older XMLs.
    sed 's|<binary path="[^"]*/\([^"/]*\)"|<binary path="\1"|' "$OUT/$n.xml" > "$OUT/$n.pack.xml"
    if ! ( cd "$OUT" && "$BIN/packrom" "$n.pack.xml" -o "$n.v32" > "$n.pack.log" 2>&1 ); then
        if grep -q "cannot open \(texture\|sound\) file" "$OUT/$n.pack.log"; then
            unpacked=$((unpacked + 1))
        else
            echo "FAIL (packrom) $n -- see out/$n.pack.log"
            failed=$((failed + 1))
        fi
    fi
done
echo "real toolchain: $built program(s) compiled and assembled" \
     "($unpacked not packed: their #texture/#sound assets aren't in tests/); $failed failed"

# The self-checking programs run in v32sim (tools/v32sim-selfcheck.sh): halted within the frame limit and
# test_errors reads 0. A tests/NNsample.input beside a sample is played by it. V32SIM_HOME is the v32sim checkout
# (V32SIM overrides the binary); V32_ORACLE=1 runs them on the stock console logic instead.
V32SIM_HOME=${V32SIM_HOME:-$(cd "$ROOT/.." 2>/dev/null && pwd)/v32sim}
SELF="$V32SIM_HOME/tools/v32sim-selfcheck.sh"
[ -f "$SELF" ] || { echo "check.sh: no v32sim at $V32SIM_HOME (set V32SIM_HOME)" >&2; exit 1; }
carts=""
for src in "$ROOT"/tests/*sample.cpp "$ROOT"/tests/*sample.c; do
    [ -e "$src" ] || continue
    grep -q "^int test_errors" "$src" || continue
    num=$(basename "$src" | sed 's/sample\.c\(pp\)\{0,1\}$//')
    n="${num}program"
    if [ ! -e "$OUT/$n.v32" ]; then
        echo "FAIL (emulator) $n -- no cartridge built"
        failed=$((failed + 1))
        continue
    fi
    # the sample's input script (v32io-script.py, now v32sim's tools/) sits beside the cartridge
    [ -e "$ROOT/tests/${num}sample.input" ] && cp "$ROOT/tests/${num}sample.input" "$OUT/$n.input"
    carts="$carts $OUT/$n.v32"
done
ran=$(echo $carts | wc -w)
if [ -n "$carts" ]; then
    # shellcheck disable=SC2086
    V32BIOS="$BIN/StandardBios.v32" sh "$SELF" -M ${V32_ORACLE:+--oracle} $carts || failed=$((failed + 1))
fi
echo "emulator: $ran self-checking program(s) run"

[ "$failed" -eq 0 ]
