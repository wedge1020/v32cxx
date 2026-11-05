#!/bin/sh
# v32c++ finds its v32/ headers next to the executable (no -I, no env, no compiled-in path hit).
# usage: exe_relative.sh BIN
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/bin" "$T/Vircon32/v32tools/include/v32c++"
cp "$BIN" "$T/bin/v32c++"
cp -r v32 "$T/Vircon32/v32tools/include/v32c++/v32"
printf '#include <v32/math.hpp>\nint main(){return 0;}\n' > "$T/t.cpp"
cd "$T" || exit 1
V32CXX_INCLUDE= "$T/bin/v32c++" --no-build-scripts -o "$T/t.c" "$T/t.cpp" >"$T/log" 2>&1
if grep -q "cannot find #include" "$T/log" || [ ! -s "$T/t.c" ]; then cat "$T/log"; echo "FAIL exe_relative"; exit 1; fi
echo "ok   exe_relative"
