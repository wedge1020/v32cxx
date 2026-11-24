# v32c++ changes

## 20261123-dev

- **`new T[n]` constructs and `delete[]` destroys.** `new T[n]` default-constructs every element in order (a
  constructor with default arguments, or the vtable setup of a class with virtual functions and no constructor, as for
  a stack array); for a class with a destructor one header word in front of the elements holds `n` and the pointer
  returned is past it, so `delete[] p` (static type known) destroys the elements last first and frees from the header.
  A class without a destructor has no header. `n == 0`, a negative `n` (0) and `delete[]` of null work. `delete[]`
  through a base pointer to a derived array stays undefined, as in C++; `new T[n]{a, b}` is not supported.
  `codegen.c` `emit_array_new_runtime_class`/`emit_array_delete_runtime`, `lower.c` (`v32_delete_arr_Class`). Test:
  `tests/179sample.cpp`, run as standard C (`tests/std_run.sh`).
- **`--alloc=gc`, `--gc-threshold=N`: a conservative mark-sweep garbage collector** on the same hook as the arena
  (`__v32_alloc`/`__v32_release`; `__v32_gc_init()` first in `main`). One block of `--heap-words=N` from the SDK's
  `malloc`; block headers (size, mark, free link), exact size-class free lists for small blocks, first fit for large ones,
  coalescing sweep; a page table finds the block of any interior pointer; explicit mark stack with a rescan fallback on
  overflow. Roots: a table v32c++ emits of the file-scope variables that can hold a pointer (pointers, objects, arrays,
  `std::vector`s, hoisted statics) and the machine stack plus registers (Vircon32: `push R0..R10`, read `SP`, scan up to
  `0x400000`; standard C: `__builtin_unwind_init`/`setjmp` up to `__libc_stack_end`). Collects when an allocation does not
  fit (then retries; still no room: "OUT OF MEMORY (gc heap full)"), every `--gc-threshold=N` words (default a quarter
  of the heap) and on `v32::gc_collect()`; `gc_used/gc_free/gc_collections` report. `delete`/`delete[]` run the destructor and
  free at once; collected objects get no destructor. `<v32/memory.hpp>` takes `--alloc=arena` or `--alloc=gc`; the other
  mode's functions do not exist. Without the flag (or with `--alloc=arena`) the output is unchanged. The Vircon32 root
  scanning (`asm`) is written from the CPU docs and v32sim, not run on the real toolchain. Future work: precise per-class
  pointer maps, finalizers. Man pages (en/es/fr), README (en/es/fr). Tests `tests/182sample.cpp`, `182full.cpp`
  (`tests/gc_full.sh`).
- **`goto` out of a scope runs destructors.** The class objects (and arrays) live at the `goto` but not at its label
  are destroyed last built first before the jump: forward, backward (a loop made with `goto` destroys and rebuilds the
  object declared after the label), out of loops and nested blocks. A `goto` past the declaration of an object with a
  destructor (forward in one block, or into a block) is an error, as in C++. A label in front of a declaration
  (`again: D d(1);`, which left the declaration unconstructed) is split off first. `lower.c` phase 0b and phase 9.
  Tests `tests/180sample.cpp`, `tests/180bad.cpp`.
- **`--alloc=malloc|arena` and `--heap-words=N`; `<v32/memory.hpp>`.** Opt-in memory management, off by default
  (output unchanged). With `--alloc=arena`, `new`, `new[]` and `std::vector` buffers come from one block taken from the
  SDK's `malloc` at the start of `main` (so a restart re-acquires it); `delete` runs the destructor and frees nothing;
  `v32::arena_mark/arena_release/arena_reset/arena_used/arena_free` give space back in bulk, running the destructors of
  objects still alive (recorded in the arena), newest first, never twice for a `delete`d object or array. Out of space:
  "OUT OF MEMORY (arena full)" on screen and `hlt` (standard C: message on stderr, `exit(1)`). Including the header
  without the flag is a compile-time error. One shared hook, `__v32_alloc`/`__v32_release`, for a later `--gc`.
  `std::string` allocates nothing. Man pages (en/es/fr) incl. `memory.hpp(3v32)`, README (en/es/fr). Tests
  `tests/181sample.cpp`, `181full.cpp` (`tests/arena_full.sh`), `181string.cpp`.

- **`Counter arr[3] = { Counter(1), Counter(2), Counter(3) };` builds the elements in place** (C++17 guaranteed copy
  elision). It was lowered to three named temporaries `__v32_obj0..2`, constructed, copied into `arr` (no element
  constructor ever ran) and then the destructors of both `arr[2..0]` and `__v32_obj2..0` ran at scope exit: three
  constructor calls, six destructor calls (found with v32sim: a `+1`/`-1` counter ended at `-3`). Now the declaration is
  `Counter arr[3];` followed by one constructor call per element, `Counter__Counter__int(&arr[0], 1)`, ...; elements left
  out (`Counter a[2] = { Counter(1) };`) are default-constructed, `{2}` elements call the constructor with those
  arguments, an element that is an object is copy-constructed in place (member-wise where the class has no copy
  constructor), `Counter a[] = {...}` takes its length from the list, and the same holds for several dimensions
  (`Counter g[2][2]`, which also had no constructor or destructor loops at all) and for a global array (built at the top
  of `main`). Destruction is the whole array, in reverse. Lowering: `ast.c`
  `keep_array_elements` (no temporary is declared), `lower.c` `construct_array_from_list` and `build_dtor_call_stmt`
  (nested loops). `Counter c = Counter(5);` already built no temporary.
- **Array members of class type take initializers.** `class Holder { Counter m[2] = { {1}, Counter(2) }; };` used to
  lose the list silently (the elements were only default-constructed); `Holder() : m{ {1}, {2} } {}` did not parse.
  Now every constructor that does not name `m` builds `m[0]`, `m[1]` in place from the list, and a constructor that
  does uses its own (`Holder() : m{ {1}, {2} }`, `Holder(int a) : m{ {a}, Counter(a + 1) }`; the arguments are ordinary
  expressions: parameters, other members, objects copied in place). Elements left out are default-constructed, in
  member declaration order with the other members, before the constructor's body; a class with no constructor gets an
  implicit one. The mem-initializer list takes a braced list (`parser.y`: `IDENTIFIER braced_init` / `TYPE_NAME
  braced_init` in `member_init`; no conflict counts changed), which for a scalar or object member is the same as the
  parenthesized form (`x{5}`). Multi-dimensional member arrays (`Counter g[2][2];`, which were not constructed at
  all, and whose destructors were skipped too) are now constructed in row order and destroyed backwards, by the
  user's or the implicit destructor, after its body. `ast.c` `desugar_member_init_arrays` and `add_array_member_init`
  (the default becomes a constructor-initializer entry), `sema.c` `resolve_member_init_list` (`ival` 3), `lower.c`
  phase 8-pre (a marker statement) and phase 7 (`inject_ctor_calls_block` expands it with `construct_array_from_list`).
  A list for an array of plain structs (`Pt m[2]` with `: m{ {1}, {2} }`) is an error.
- **Destructors of global objects run.** File-scope objects and arrays of them (built at the top of `main`) are now
  destroyed in reverse construction order at every way out of `main`: after its own locals at a `return` (the returned
  value is computed first) and when it falls off its end. The code is a `__v32_destroy_globals()` function with a
  once-only flag (`__v32_globals_live`, set after the construction). A program that uses v32libc (an `#include` of
  `v32libc.c`, or the library itself) also sets `v32_exit_destructors = &__v32_destroy_globals;` in `main`, a new
  hook that `v32_exit()` (so `exit()` and `abort()`, which never return) runs before `v32_exit_hook`; `libc/v32libc.c`
  and `v32libc.h` gained the variable. Nothing is added to a program without a global that has a destructor, or
  without `main`. Objects in a namespace are still not handled. `lower.c` phase 9c (`destroy_globals_in_main`).
- **A typedef of a class names the class** in `typedef Counter C; C x{1}; C y = {1}; C a[2] = { C(1), {2} };` and
  `C x = C(1);`: `T x{...}` skipped the constructor (and `C(3)` was read as a cast). `C x(1);` and arrays already
  resolved. `ast.c` `class_named` follows typedefs (chains too).
- **Two cart hints with the same name are an error** (two `#texture`, two `#sound`, or one of each): the message names
  both lines (`file:line: error: the cart hint name 'HERO' is already used: #texture here, and #texture at file:line`).
  The generated C defined the name twice and the later hint won in silence. Checked in `prescan.c`
  (`check_cart_hint_name`), which knows the line, in active regions only; `lexer.l` is unchanged.
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
- Tests: `tests/174sample.cpp` (self-checking, logs every constructor and destructor; part of `make realcheck`);
  `tests/175sample.cpp` (self-checking; now with typedef'd classes) and `tests/175bad.cpp`; `tests/176sample.cpp`
  (array members, self-checking; `tests/174bad.cpp`, the old "not supported" error, is gone) and `tests/176bad.cpp`;
  `tests/177sample.cpp` + `177ret.cpp` + `177end.cpp` (global destructors; `tests/globals_destroy.sh` builds them as
  standard C and runs them under `tests/177driver.c`, since what happens after `main` returns can only be seen from
  outside); `tests/178bad.cpp` and `178bad_sound.cpp` (duplicate cart hints).
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
