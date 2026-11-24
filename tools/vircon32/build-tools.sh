#!/bin/sh
# *****************************************************************************
#  tools/vircon32/build-tools.sh — build the stock Vircon32 toolchain for `make realcheck`
#
#    tools/vircon32/build-tools.sh [COMPUTERSOFTWARE_CHECKOUT]
#
#  v32sim is the one place for running, profiling and debugging carts, and for building the stock
#  tools that check against: this script only calls v32sim's tools/oracle/build.sh and leaves
#  bin/compile, bin/assemble, bin/packrom, bin/include/ and bin/StandardBios.v32 here. (The runners
#  v32run, v32prof, v32shot, v32peek and v32io-script.py that used to be copied into this directory
#  are v32sim's now: tools/oracle/ and tools/ there.)
#
#  Needs a v32sim checkout: V32SIM_HOME=/path/to/v32sim (default: ../v32sim beside this project, then ../v32sim.git).
#  The ComputerSoftware checkout is found the same way: the argument, else ../ComputerSoftware beside this
#  project, then ../ComputerSoftware.git. With none, one is cloned to ../ComputerSoftware at the commit the
#  tools were verified against, so later runs find it there. Nothing is cloned inside this project.
# *****************************************************************************
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
PARENT=$(cd "$HERE/../../.." 2>/dev/null && pwd)   # the folder holding this project and its siblings
if [ -z "$V32SIM_HOME" ]; then
    # ../v32sim beside this project, or ../v32sim.git for a checkout named after its repository
    V32SIM_HOME=$PARENT/v32sim
    [ -x "$V32SIM_HOME/tools/oracle/build.sh" ] || [ ! -x "$PARENT/v32sim.git/tools/oracle/build.sh" ] ||
        V32SIM_HOME=$PARENT/v32sim.git
fi
[ -x "$V32SIM_HOME/tools/oracle/build.sh" ] || {
    echo "build-tools.sh: no v32sim at $V32SIM_HOME (nor at $PARENT/v32sim.git; set V32SIM_HOME to a v32sim checkout)" >&2; exit 1; }
PINNED=5219741ce473ab686931db0933131c317bd50343     # Vircon32 compiler v26.04.24
if [ -n "$1" ]; then
    SRC=$1
elif [ -d "$PARENT/ComputerSoftware/DevelopmentTools" ]; then
    SRC=$PARENT/ComputerSoftware
elif [ -d "$PARENT/ComputerSoftware.git/DevelopmentTools" ]; then
    SRC=$PARENT/ComputerSoftware.git
else
    SRC=$PARENT/ComputerSoftware
    echo "build-tools.sh: no ComputerSoftware checkout beside this project; cloning one to $SRC"
    git clone --quiet https://github.com/vircon32/ComputerSoftware "$SRC"
    git -C "$SRC" checkout --quiet "$PINNED"
fi
[ -d "$SRC/DevelopmentTools" ] || { echo "build-tools.sh: $SRC is not a ComputerSoftware checkout" >&2; exit 1; }
echo "build-tools.sh: v32sim $V32SIM_HOME, ComputerSoftware $SRC"
HEAD=$(git -C "$SRC" rev-parse HEAD 2>/dev/null || true)
if [ -n "$HEAD" ] && [ "$HEAD" != "$PINNED" ]; then
    echo "build-tools.sh: note: $SRC is at $HEAD, not the verified $PINNED"
fi
ONLY="compile assemble packrom" sh "$V32SIM_HOME/tools/oracle/build.sh" "$SRC" "$HERE/bin"
# Smoke test: the stock compiler finds include/ beside its own executable (SDL_GetBasePath, from v32sim's oracle
# shim). Compile from a scratch directory, as check.sh does, so a compiler that can't locate its headers stops here.
SMOKE=$(mktemp -d)
printf '#include "misc.h"\nvoid main(void) { }\n' > "$SMOKE/smoke.c"
if ! ( cd "$SMOKE" && "$HERE/bin/compile" smoke.c -o smoke.asm > smoke.log 2>&1 ); then
    echo "build-tools.sh: the built compiler cannot find its SDK headers:" >&2
    sed 's/^/    /' "$SMOKE/smoke.log" >&2
    echo "build-tools.sh: it looks for include/ beside itself via v32sim's tools/oracle/shim/SDL.h" \
         "(SDL_GetBasePath); update v32sim if that shim doesn't support this OS" >&2
    rm -rf "$SMOKE"; rm -f "$HERE/bin/BUILD-INFO"; exit 1
fi
rm -rf "$SMOKE"
# Stamp what was built, so check.sh can tell a current toolchain from a stale one.
{
    echo "computersoftware=$(cd "$SRC" && pwd)"
    echo "head=${HEAD:-unknown}"
    echo "v32sim=$V32SIM_HOME"
    echo "compiler=$("$HERE/bin/compile" --version 2>/dev/null | head -n 1)"
} > "$HERE/bin/BUILD-INFO"
# The pinned commit's string.h predates the ftoa() rounding-carry fix (0.999999 came out as "0.1"). The project's
# c_api/string.h is the fixed one, so the real toolchain is tested with it (tests/130sample.cpp needs it).
if [ -f "$HERE/../../c_api/string.h" ] && ! cmp -s "$HERE/../../c_api/string.h" "$HERE/bin/include/string.h"; then
    cp "$HERE/../../c_api/string.h" "$HERE/bin/include/string.h"
    echo "installed the project's c_api/string.h (ftoa carry fix) into $HERE/bin/include"
fi
