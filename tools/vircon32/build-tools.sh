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
#  Needs a v32sim checkout: V32SIM_HOME=/path/to/v32sim (default: ../v32sim beside this project).
#  Without a ComputerSoftware checkout one is cloned at the commit the tools were verified against.
# *****************************************************************************
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
V32SIM_HOME=${V32SIM_HOME:-$(cd "$HERE/../../.." 2>/dev/null && pwd)/v32sim}
[ -x "$V32SIM_HOME/tools/oracle/build.sh" ] || {
    echo "build-tools.sh: no v32sim at $V32SIM_HOME (set V32SIM_HOME to a v32sim checkout)" >&2; exit 1; }
SRC="${1:-$HERE/ComputerSoftware}"
PINNED=5219741ce473ab686931db0933131c317bd50343     # Vircon32 compiler v26.04.24
if [ ! -d "$SRC" ]; then
    git clone --quiet https://github.com/vircon32/ComputerSoftware "$SRC"
    git -C "$SRC" checkout --quiet "$PINNED"
fi
ONLY="compile assemble packrom" sh "$V32SIM_HOME/tools/oracle/build.sh" "$SRC" "$HERE/bin"
# The pinned commit's string.h predates the ftoa() rounding-carry fix (0.999999 came out as "0.1"). The project's
# c_api/string.h is the fixed one, so the real toolchain is tested with it (tests/130sample.cpp needs it).
if [ -f "$HERE/../../c_api/string.h" ] && ! cmp -s "$HERE/../../c_api/string.h" "$HERE/bin/include/string.h"; then
    cp "$HERE/../../c_api/string.h" "$HERE/bin/include/string.h"
    echo "installed the project's c_api/string.h (ftoa carry fix) into $HERE/bin/include"
fi
