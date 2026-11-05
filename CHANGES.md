# v32c++ changes

## 20261105-dev

- **Headers next to the program**: like v32lua, v32c++ also looks for its headers relative to the executable (`include/v32c++`, `../include/v32c++`, `../Vircon32/v32tools/include/v32c++`; only those that exist, after the compiled-in path), so an install under any prefix or an unpacked release works with no `-I` or `$V32CXX_INCLUDE`. `tests/exe_relative.sh`. Windows and macOS ask the OS for the executable's path (as v32lua does). config.h gained the Windows defaults `V32CXX_WIN32_INCLUDE_PATH` (`C:/Program Files/Vircon32/v32tools/include/v32c++`) and `V32CXX_WIN32_SDK_INCLUDE_PATH` (`C:/Program Files/Vircon32/DevTools/include`), used when `_WIN32` is defined and no `-D` overrides.
- Based on the newer archive (linker `-L`/`-l`, `warn.c`, tests 151-173, spyvsspy demo). `--line-markers` is long-only (`0x10c`) because `-L` is the library path option there.
- `v32shot.cpp` was removed with the other private runners; its late `-` (no cartridge) feature is a v32sim item (VS-16).

- **`--line-markers`** (SX-1): `#line N "file.cpp"` before each function and statement of the generated C, so
  `v32cc -g --target-compat=v32cxx` writes `;@src game.cpp:N` (and `;@var`/`;@scope` for the C locals) and
  v32sim debugs by the C++ lines. Only written where the preprocessor's count would differ; the generated
  code is unchanged and, without the option, byte-identical to before (checked on all `tests/*sample.*`).
  Test: `tests/line_markers.sh` (part of `make test`; pass `V32CC=` to push the output through v32cc too).
  README "Debugging with v32sim" lists the names a debugger meets (SX-2).
- **Headless tools moved to v32sim** (SX-3): `tools/vircon32/{v32run,v32prof,v32shot,v32peek}.cpp`, `shim/` and
  `v32io-script.py` are removed. `build-tools.sh` calls v32sim's `tools/oracle/build.sh` for the stock
  `compile`/`assemble`/`packrom`; `check.sh` runs the self-checking samples with v32sim's
  `tools/v32sim-selfcheck.sh` (`V32SIM_HOME`, default `../v32sim`; `V32_ORACLE=1` for the stock console logic).
  All 47 self-checking samples pass in v32sim.
- `make version VERSION=...` now also rewrites `inc/v32cxx.h`. Version `20261105-dev`.
- Audio demos: `tempest_32k` already converts its m4a at build time; no other demo has a compressed source.
