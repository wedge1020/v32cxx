#!/bin/sh
# *****************************************************************************
#  tools/vircon32/build-tools.sh — build the real Vircon32 toolchain, headless
#
#  Builds, from the official vircon32/ComputerSoftware sources:
#    bin/compile   the Vircon32 C compiler
#    bin/assemble  the Vircon32 assembler
#    bin/packrom   the ROM packer (XML rom-definition -> .v32 cartridge)
#    bin/wav2vircon  the sound converter (.wav -> .vsnd), for profiling
#                  real cartridges such as the demos
#    bin/v32run    a headless console runner (this directory's v32run.cpp,
#                  linked against the emulator's own ConsoleLogic)
#    bin/v32prof   a headless CPU profiler (v32prof.cpp): scripted input,
#                  per-frame CPU/GPU load, cycles per function
#    bin/v32shot   headless screenshots (v32shot.cpp): scripted input, the
#                  screen at chosen frames written as PPM images
#  plus bin/include/ (the SDK's standard C headers, which `compile` finds
#  next to itself) and bin/StandardBios.v32.
#
#  No SDL, OpenGL or OpenAL needed: the dev tools only use SDL to locate
#  their own directory (shim/SDL.h stands in), and ConsoleLogic takes its
#  video/audio as callbacks, which v32run leaves as no-ops.
#
#  usage: tools/vircon32/build-tools.sh [path-to-existing-ComputerSoftware]
#  Needs: git (unless a checkout is given), g++ with C++17.
# *****************************************************************************
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
BIN="$HERE/bin"
SRC="${1:-$HERE/ComputerSoftware}"

# The commit these tools were verified against (Vircon32 compiler v26.04.24).
PINNED=5219741ce473ab686931db0933131c317bd50343

if [ ! -d "$SRC" ]; then
    git clone --quiet https://github.com/vircon32/ComputerSoftware "$SRC"
    git -C "$SRC" checkout --quiet "$PINNED"
fi

mkdir -p "$BIN"
DT="$SRC/DevelopmentTools"
CXX="${CXX:-g++}"
FLAGS="-O2 -std=c++17 -w -I$HERE/shim -I$DT -I$SRC -I$DT/DevToolsInfrastructure"
INFRA="$DT/DevToolsInfrastructure/*.cpp"

# Source lists match DevelopmentTools/CMakeLists.txt. (Globbing CCompiler/*.cpp
# does NOT work: the directory also holds a stale, unbuilt source file.)
CC_SRCS="CNodes CTokens CheckBinaryOperations CheckNodes CheckUnaryOperations
         CompilerInfrastructure DataTypes DebugInfo EmitBinaryOperationNodes
         EmitExpressionNodes EmitNonExpressionNodes EmitUnaryOperationNodes
         Globals Main MemoryPlacement Operators RegisterAllocation SourceLocation
         StaticValue VirconCAnalyzer VirconCEmitter VirconCLexer VirconCParser
         VirconCPreprocessor"
CC_FILES=""
for f in $CC_SRCS; do CC_FILES="$CC_FILES $DT/CCompiler/$f.cpp"; done

echo "building compile..."
$CXX $FLAGS -I$DT/CCompiler $CC_FILES $INFRA -o "$BIN/compile"

echo "building assemble..."
$CXX $FLAGS -I$DT/Assembler $DT/Assembler/*.cpp $INFRA -o "$BIN/assemble"

echo "building wav2vircon..."
$CXX $FLAGS -I$DT/WAV2Vircon $DT/WAV2Vircon/wav2vircon.cpp $INFRA -o "$BIN/wav2vircon"

echo "building packrom..."
$CXX $FLAGS -I$DT/RomPacker -I$DT/ExternalLibraries/tinyxml2 \
    $DT/RomPacker/*.cpp $INFRA $DT/ExternalLibraries/tinyxml2/tinyxml2.cpp \
    -o "$BIN/packrom"

echo "building v32run..."
CL="$SRC/DesktopEmulator/ConsoleLogic"
$CXX -O2 -std=c++17 -w -I$CL -I$SRC/DesktopEmulator "$HERE/v32run.cpp" $CL/*.cpp -o "$BIN/v32run"

echo "building v32prof..."
$CXX -O2 -std=c++17 -w -I$CL -I$SRC/DesktopEmulator "$HERE/v32prof.cpp" $CL/*.cpp -o "$BIN/v32prof"

echo "building v32shot..."
$CXX -O2 -std=c++17 -w -I$CL -I$SRC/DesktopEmulator "$HERE/v32shot.cpp" $CL/*.cpp -o "$BIN/v32shot"

rm -rf "$BIN/include"
cp -r "$DT/Data/include" "$BIN/include"
cp "$SRC/DesktopEmulator/Data/Bios/StandardBios.v32" "$BIN/StandardBios.v32"

echo "done: $BIN"
"$BIN/compile" --version
