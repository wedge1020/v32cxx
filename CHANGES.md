# v32c++ changes

## 20261123-dev

- **`V32CXX_SDK_INCLUDE=` (set but empty) turns the SDK header search off.** Setting the variable only ever added
  folders in front of the search, so on a machine with the DevTools installed (`compile` on `PATH`, or
  `/usr/local/Vircon32/DevTools/include`) the SDK headers were still found and sample 154, which checks the
  "SDK header not found" warning, failed `make test`. Empty now means no SDK headers at all (`main.c`); unset keeps the
  full search. `make test` also runs sample 154 with a fake SDK on `PATH` to cover it.

- **Namespace-scope variables get their namespace in their C name.** `namespace a { int x = 1; } namespace b { int x = 2; }`
  was `int x = 1; int x = 2;` (a redefinition, or with no initializers one merged variable), and both `a::x` and
  `b::x` read `x`; so did a namespace variable that clashed with a file-scope one. Now a variable declared in a
  namespace is `a__x` (`a__inner__x` when nested), the scheme namespace functions already use; file-scope variables and
  `extern` declarations (names defined elsewhere: `lexer.l` drops `extern` in C++ input, so `prescan.c` notes the lines
  that hold it, `prescan_line_has_extern`) keep their names. `sema.c` registers every variable with its namespace and
  C name (`GlobalVarRegEntry`, `resolve_global_var`: the enclosing namespaces from the innermost out, then `using`
  directives and declarations; a qualified name relative to the current namespace first) and `infer_expr_type` uses the
  scoped lookup, so `a::c.get()` and `b::c.get()` find the right class; `lower.c` phase 0c (`namespace_vars_rename`,
  right after this-injection, so a class member is already `this->m`) renames the declarations and every reference:
  qualified, unqualified inside the namespace (reopened, in a function or a method, out of line too), `using namespace a;`,
  `using a::x;`, nested namespaces and asm `{x}` operands, a local or parameter hiding the variable.
  Global construction/destruction and the gc root table work on the C names (`__v32_gc_root_a[i] = (int)&app__deep__tail;`).
  A call through a namespace's function-pointer variable (`n::hp(5)`) was "no function 'hp' is declared in namespace 'n'";
  it is a call through the pointer now. Enumerators of a namespace's enum are not renamed. Output change: every
  namespace variable in a program is renamed (`82sample`, `125sample`, `184sample`, `186sample`/`186gc`, the Space
  Invaders demo's `SINE16`, `g_rng`...); `v32/*.hpp` and the other demos are unchanged. Tests `tests/188sample.cpp`
  (default, arena, gc), `188extern.cpp`.
- **Function-pointer initializers at file and namespace scope use the C name.** `D *(*g)() = f;`, `int (*h)(int) = &square;`
  and `int (*ops[2])(int) = { inc, dec };` kept the source name (`= f`: no such function in the output; the `&` that
  Vircon32 needs was missing too); locals and assignments always worked. They now go through the same rewrite
  (`fnptr_init` in `lower.c`): the mangled name with an explicit `&`, namespace-qualified names and arrays of pointers
  (typedef'd too) included, and the function prototypes are printed ahead of the variables when such an initializer
  names a function (`g_globals_name_functions`), as for C input. An OVERLOADED name is now picked by the pointer's own type
  (`int (*p)(float) = over;`, also for locals and plain assignments; `sema_function_matches_pointer_type`); when no
  overload, or more than one, has that signature it is an error, not a guess. Member functions are not covered. Output
  change: C input's file-scope function tables now carry the `&` in `--target=standard` output as well
  (`156sample.c`, `159lib_a.c`, `172sample.c`; the Vircon32 output already had it). Tests `tests/189sample.cpp`
  (default, arena, gc), `189bad.cpp`.
- **`new T*[n]` and `T*&`.** Grammar only, no change to the conflict counts: `pointer_opt` accepts `*&`, `new T*[n]` /
  `new T**[n]` is a production of its own (the `NEW type_spec` rule has the lowest precedence so a `*` after `new T` is
  part of the type, as in C++). `new T*[n]` is an array of pointers: plain pointer-sized words, no constructors, no count
  header (`v32_new_arr_ptr(n)`, emitted only when used, in every `--alloc` mode, `sizeof(int *)` words), cast to `T **`;
  `delete[]` releases it plainly, leaving the pointed-to objects to the caller. `T*&` is lowered to `T**` like any
  reference (phase 5): the call site passes the address of a variable, a member or an array element, the body assigns
  through it, reads it, passes it on to another `T*&` function, a `T*&` local binds to a variable, and a function may
  return one. Tests `tests/190sample.cpp` (default, arena, gc).

- **`native X;` accepted when the real SDK headers are found.** With the Vircon32 SDK installed, prescan harvests
  `struct date_info` / `struct time_info` from `time.h`, and sema then rejected `native date_info;` as a conflict
  with an earlier struct (sample 90 failed `make test` on any machine with the DevTools installed). A `native`
  redeclaration of a struct that came from a pass-through SDK header is now redundant, not a conflict: the known
  layout is kept (`sema.c`). `make test` runs sample 90 a second time against a stub `tests/sdkstub/time.h`, so the
  SDK-present path is covered without the SDK. A `native` that conflicts with the program's own struct or typedef is
  still an error (sample 91).

- **`delete[]` finds the element class in every spelling.** A class with a destructor keeps a count header in front of
  its array, so `delete[]` of an operand whose class was not resolved (the fallback was the plain `v32_delete`) freed the
  wrong address. `infer_expr_type` (`sema.c`) now types pointer arithmetic (`p + n`, `n + p`, `p - n`) and a call
  through a function pointer; a pointer to pointers is an array of pointers (a plain release, as it always should have
  been); typedef'd, member (`.`/`->`/inherited/`this`), indexed, returned, cast, `auto`, namespace and
  `std::vector<T *>`/`std::array<T *, N>` operands were already found. Where the type is still unknown (a `void *`) and
  the program has a class with a destructor, it is now an error asking for a cast (`lower.c`
  `delete_array_element_class`); a primitive, or a program without such a class, stays a plain free. Also fixed on
  the way: an array of a typedef'd pointer to a class (`typedef D *DP; DP a[2];`, a member too) was built and destroyed as
  an array of objects (`type_to_value_class`). Tests `tests/184sample.cpp`, `184bad.cpp`, `184ok.cpp`.
- **`new T[n]{a, b, ...}` (list-initialized array new).** The listed elements are built from the list (`{x, y}` calls a
  constructor, `T(x)` or a plain value a converting or copy one, a scalar is assigned), then the rest are
  default-constructed (zero for a scalar or a class without constructors); `new T[n]{}` is `new T[n]`. Grammar: one
  new production, no conflicts (`%expect` unchanged). `ast.c` (`lift_objects_expr`) turns the items into in-place
  constructor calls like a local array's, `sema.c` `check_list_new` checks them (a class with constructors but no
  default one needs a constant `n` not above the list), `lower.c` phase 6 lowers it to a comma sequence on a
  function-level temporary, so it works in any expression:
  `(__v32_nl_n0 = n, __v32_nl_tmp0 = v32_new_arr_T_init(__v32_nl_n0, 2), T__T__int(&__v32_nl_tmp0[0], 1), ..., v32_new_arr_T_rest(__v32_nl_tmp0, __v32_nl_n0, 2), __v32_nl_tmp0)`.
  The helpers (`codegen.c`, emitted only for a program that uses the form) allocate with the count header in every
  `--alloc` mode and leave elements `0..k-1` to the caller, so elements are built in order. `n` smaller than the list
  is undefined in C++ (`std::bad_array_new_length`): the list's length is allocated. Tests `tests/187sample.cpp`
  (default, arena, gc), `187bad.cpp`.
- **Objects in a namespace are constructed and destroyed.** `namespace game { Counter g(1); Counter arr[2] = {...}; }`
  (nested and reopened namespaces too) was emitted as a broken initializer and never run; now phases 7b/9c
  (`construct_globals_in_main`, `destroy_globals_in_main`) collect the variables of namespaces in source order with the
  top-level ones, and destroy them in reverse. The gc root table already covered namespace scope (checked,
  `tests/186gc.cpp`). `static` class data members are not supported by this compiler (an error). Tests
  `tests/186sample.cpp` (with `tests/177driver.c`), `186gc.cpp`. Output change: `82sample` now constructs its
  namespaced `Counter`.
- **A typedef as a base class.** `typedef Base B; class D : public B {...};` was "inherits from unknown base"; the
  base (and a base named in a constructor's initializer list, in the class or out of line, `D() : B(3) {}`) is renamed
  to the class the typedef chain ends at before sema (`ast.c` `canonicalize_typedef_bases`). Test `tests/183sample.cpp`
  (virtual functions through the typedef'd base).
- **`--alloc=gc`: a pointer one past the end of an array keeps its own block alive.** Blocks lie one after the other, so
  `p + n` (an iterator's `end()`) pointed at the header of the NEXT block: that one was kept and the array was freed.
  Every block now has one trailing unit (`n = n + 1` in `__v32_alloc`, as in Boehm's collector), so the end pointer
  stays inside its block; the page-table lookup is unchanged. Default and arena output are unchanged; the gc runtime
  text gains two lines. Test `tests/185sample.cpp` (an `int[8]` and an array of objects with a count header, each
  reachable only through its one-past-end pointer, survive and the neighbour is freed; fails with the old runtime).

- **`new T[n]` constructs and `delete[]` destroys.** `new T[n]` default-constructs every element in order (a
  constructor with default arguments, or the vtable setup of a class with virtual functions and no constructor, as for
  a stack array); for a class with a destructor one header word in front of the elements holds `n` and the pointer
  returned is past it, so `delete[] p` (static type known) destroys the elements last first and frees from the header.
  A class without a destructor has no header. `n == 0`, a negative `n` (0) and `delete[]` of null work. `delete[]`
  through a base pointer to a derived array stays undefined, as in C++ (`new T[n]{a, b}`: see the entry above).
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
