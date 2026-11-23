# v32c++ changes

## 20261123-dev

- **`Counter arr[3] = { Counter(1), Counter(2), Counter(3) };` builds the elements in place** (C++17 guaranteed copy
  elision). It was lowered to three named temporaries `__v32_obj0..2`, constructed, copied into `arr` (no element
  constructor ever ran) and then the destructors of both `arr[2..0]` and `__v32_obj2..0` ran at scope exit: three
  constructor calls, six destructor calls (found with v32sim: a `+1`/`-1` counter ended at `-3`). Now the declaration is
  `Counter arr[3];` followed by one constructor call per element, `Counter__Counter__int(&arr[0], 1)`, ...; elements left
  out (`Counter a[2] = { Counter(1) };`) are default-constructed, `{2}` elements call the constructor with those
  arguments, an element that is an object is copy-constructed in place (member-wise where the class has no copy
  constructor), `Counter a[] = {...}` takes its length from the list, and the same holds for several dimensions
  (`Counter g[2][2]`, which also had no constructor or destructor loops at all) and for a global array (built at the top
  of `main`; globals are still never destroyed). Destruction is the whole array, in reverse. Lowering: `ast.c`
  `keep_array_elements` (no temporary is declared), `lower.c` `construct_array_from_list` and `build_dtor_call_stmt`
  (nested loops). `Counter c = Counter(5);` already built no temporary. An array **member** with a brace initializer of
  a class with a constructor (`Counter m[2] = { {1}, {2} };`) used to lose the list silently; it is now an error.
- **`T x{...};` and `T x = {...};` now do C++11 list-initialization.** Both were emitted as a C brace list, so for
  `Counter c{1};` the constructor never ran while the destructor did (a live count ended at `-2`), and `int k{3};`
  became `int k = {3};`. `T x{...}` (and `T x = {...}`, also in a `for` initializer, a `Counter a{1}, b{2};` tail and
  at file scope) of a class that declares a constructor is now the same as `T x(...)`; `{}` is `T x;`. A scalar
  (primitive or pointer) becomes `int k = 3;` (`{}` is `0`; two or more values is an error). An aggregate (no
  constructor) and an array keep their C brace list. A nested brace item in a non-aggregate (`Pair x{{1},{2}}`) is a
  clear error. `ast.c` `rewrite_list_init` (the grammar change is the `braced_init` alternative of `opt_initializer`).
- A brace list (`int a[2] = { count, get(count) };`) is now walked by the this-injection, call-finalization, reference
  and `new`/`delete` passes like any other expression (the list's values were left as written: a member name stayed
  bare in a method).
- Tests: `tests/174sample.cpp` (self-checking, logs every constructor and destructor; part of `make realcheck`) and
  `tests/174bad.cpp`; `tests/175sample.cpp` (self-checking) and `tests/175bad.cpp`.
- `make archive` writes `../v32cxx-project-YYYYMMDD.zip` (the date of `VERSION`); `make clean` removes
  `../v32cxx-project-*.zip`.
## 20261109-dev

- **`-g` works with v32cc.** `-g` now implies `--line-markers` when the compiler is v32cc, and the generated Make.sh /
  Make.bat pass `-g` to v32cc and v32as. New options `--compiler=CMD` (also `$V32CXX_COMPILER`) and
  `--compiler-style=stock|v32cc` (a name starting with `v32cc` is detected). Test: `tests/debug_flow.sh`.

## 20261108-dev

- **`make sysinstall` / `sysuninstall` honour `PREFIX` and `DESTDIR`** (they wrote to `/usr/local` whatever was
  asked, even for a staged install). The default is unchanged. With another `PREFIX` the headers go to
  `PREFIX/Vircon32/v32tools/include/v32c++`, which `v32c++` finds relative to its own directory
  (`../Vircon32/v32tools/include/v32c++`), so no rebuild is needed; `mandb` is not run on a staged tree.

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
