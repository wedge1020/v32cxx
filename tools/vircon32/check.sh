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
#      self-checking program: its cartridge is booted headless with the
#      standard BIOS and a fresh memory card, run until the CPU halts, and
#      the RAM word holding test_errors must read 0 (it starts at -1, so a
#      program that crashes or never reaches the end also fails). A sample
#      with a tests/NNsample.input next to it is played with that input
#      script (v32peek) instead of no input (v32run) -- see 119sample.
#
#  Needs tools/vircon32/bin, built by tools/vircon32/build-tools.sh.
#  Exit status: 0 if everything passed, 1 otherwise.
# *****************************************************************************

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BIN="$HERE/bin"
OUT="$ROOT/out"

if [ ! -x "$BIN/compile" ] || [ ! -x "$BIN/v32run" ]; then
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
    # v32c++ writes <binary path> exactly as -o was given ("out/NNprogram.vbin"),
    # meant for packing from the directory v32c++ ran in. packrom resolves
    # paths relative to the XML's own directory, so packing out/NN.xml
    # directly would look for out/out/NN.vbin. Pack a temporary copy whose
    # binary path is just the file name instead; the real XML is untouched.
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

ran=0
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
    addr=$(grep -m1 "%define global_test_errors " "$OUT/$n.asm" | awk '{print $3}')
    rm -f "$OUT/$n.memc"
    input="$ROOT/tests/${num}sample.input"
    if [ -e "$input" ]; then
        # A sample with an input script (tests/NNsample.input: scripted
        # gamepad presses, e.g. v32io keyboard/mouse traffic made by
        # v32io-script.py) runs under v32peek, which plays the script.
        result=$( cd "$OUT" && V32PEEK_CARD="$n.memc" "$BIN/v32peek" "$BIN/StandardBios.v32" "$n.v32" "$input" 6000 "$addr" 1 2> "$n.run.log" | tr '\n' ' ' )
        value=$(echo "$result" | sed -n 's/.*ram\[[0-9]*\.\.[0-9]*\]: *[0-9]*: *\(-\{0,1\}[0-9]*\).*/\1/p')
        halted=$(echo "$result" | grep -q " halted " && echo 1 || echo 0)
    else
        result=$( cd "$OUT" && "$BIN/v32run" "$BIN/StandardBios.v32" "$n.v32" "$n.memc" "$addr" 1200 2> "$n.run.log" )
        value=$(echo "$result" | sed -n 's/.*ram\[[0-9]*\]=\(-\{0,1\}[0-9]*\).*/\1/p')
        halted=$(echo "$result" | sed -n 's/.*halted=\([01]\).*/\1/p')
    fi
    ran=$((ran + 1))
    if [ "$halted" = "1" ] && [ "$value" = "0" ]; then
        echo "pass (emulator) $n"
    else
        echo "FAIL (emulator) $n -- $result (see out/$n.run.log)"
        failed=$((failed + 1))
    fi
done
echo "emulator: $ran self-checking program(s) run"

[ "$failed" -eq 0 ]
