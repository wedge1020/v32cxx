# v32c++ changes

## 20261123-dev

- **Seven gaps in value-initialization, unions, list-initialization, scalar `new`, scoped enums and SDK variables.**
  Each has a self-checking sample that passes on the machine (`make realcheck`, also the arena/gc variants).
  - **`new P[n]{}` gave pointer members 0, not NULL (-1).** Its `v32_new_arr_P_rest` zeroed the elements byte-wise. For a class that holds a
    pointer (nested structs and arrays of them too) the helper now assigns a complete zero value to each element
    (`P zero = {0, NULL, 0.0}; ... p[i] = zero;`, `emit_rest_zero_body`, `lower_class_holds_pointer`); `new P()` and `new T*[n]{}` were
    right already and are tested now, and so is `fill_init`'s file-scope rule. A zero written for an `enum` member is `(E)0`: the stock
    compiler rejects an int for an enumeration (`S s{};`, `{1, 0}` and `new S()` of a struct with an enum member did not compile), QUIRKS #34.
    `tests/196sample.cpp`.
  - **Brace-initialized unions** (QUIRKS #33, was "your code"): `U u = {1};`, `U u{};`, `{}` and unions inside structs and arrays
    are lowered in Vircon32 mode to the plain declaration plus assignments (`lower.c` phase 10b, `ui_init`): a list initializes the first
    member as in C, the rest of the union is zeroed (`((int *)&u)[w] = 0`, a loop past 4 words), `{}` zeroes it all (a pointer first
    member is `NULL`), a missing item is the zero of its type. File-scope and namespace-scope ones are assigned at the start of `main`;
    `U a[] = {...}` is sized from its list; `new S()` / `new S[n]{}` of a struct holding a union write the zero as assignments
    (`v32_new_S` ended `S zero = {0, {0}};`, which the compiler rejects). Not lowered: a union declared in a `for`/`if` condition.
    Found, not fixed: unions are printed before the structs, so a union with a struct member fails (QUIRKS #35). `tests/197sample.cpp`.
  - **List-initialization in three more places** (as `tests/175sample.cpp`): an in-class default with braces (`P m{}; P n{1, 2}; int w{5};
    Counter c{3}; int a[3]{1, 2};` -- they parsed and were silently dropped; an array member's `a[3]{...}` was a parse error; the
    lists are now each constructor's `: m{...}` entry or element assignments), `: w{}` / `: w{5}` on a scalar member and `: p{1, 2}`
    on an aggregate member (sema `ival` 1 with an empty list, new `ival` 4), and `new P{}`, `new P{1, 2}`, `new Counter{3}`,
    `new int{7}` (grammar `NEW type_spec braced_init`, no new conflicts; an aggregate is the zeroed `v32_new_P()` plus the listed
    members on a function-level temporary, `new P(1, 2)` too). `tests/198sample.cpp`.
  - **Scalar `new`**: `new int`, `new int()`, `new int(5)`, `new float(2.5)`, `new int{7}`, `new char('x')`, `new bool(true)`, `new E(v)` and
    `new Node*` (one pointer, NULL, or `new Node*(p)`) allocate one word through the hook (`v32_new_int/float/char/bool/ptr`, emitted only when
    used, arena and gc included) and `delete` releases them (it called an undefined `v32_new_int`; `delete pp` of a `T **` called
    `v32_delete_T`). `tests/199sample.cpp`.
  - **`enum class`** (scoped enumerations): `enum class Color : int { Red, Green };` (the underlying type is accepted and ignored), reached as
    `Color::Red` (C names `Color__Red`, `ns__Color__Red`); `switch`/`case`, comparisons, parameters (`f(Color)` and `f(int)` overload),
    `static_cast<int>(c)`, arrays, members, default arguments work. A bare `Red` is not declared (the stock compiler says so) and a
    conversion to `int` needs no cast (more permissive than C++).
  - **Enum types are qualified by their namespace**: `a::E` is `a__E` in the C output (declaration, variables, parameters, casts, `sizeof`
    and the mangled names of functions that take it: `g__a__E`; they were `g__a_F`), so `namespace a { enum E } namespace b { enum E }` is
    legal (it was a sema error, `tests/193bad.cpp` is now a positive check). parser.y names the declaration and rewrites each type use the symbol
    table resolves to it. Output change: every enum of the `v32` wrappers is `v32__BlendMode`, `v32__Key`... and so are the mangled names of
    functions that take them. `tests/200sample.cpp`.
  - **A variable an SDK header defines, redeclared `extern`**: `extern malloc_block *malloc_first_block;` (misc.h defines it) was printed as a second
    definition ("already declared"). prescan.c now notes the names of the header's depth-0 variable definitions (`harvest_var_names`,
    `prescan_is_header_var`) and codegen leaves the `extern` redeclaration out; sema still sees it for the type. Tested with the real
    headers (`tests/201sample.cpp`) and `tests/sdkstub` (`tests/201stub.cpp`).
  Not done: `new T[n]()` (parse error), out-of-line `int a::f()` definitions in namespaces (parse error, as before), union before-struct
  order (#35).
- **`make clean` removes what older tooling left in the tree, and `tools/prune-release.sh` removes tracked copies.**
  `clean` now also deletes `tools/vircon32/ComputerSoftware` (the old in-tree clone) and `tests/Make.sh` /
  `tests/Make.bat`. Those two came from the sample 98 line in `make test`, which ran v32c++ without
  `--no-build-scripts` once the toolchain was built; it now uses `$(TBIN)` like every other test. `tests/line_markers.sh` had the same omission (both scripts are written to the current directory, which is `tests/` there). `make test` now fails if either file appears in `tests/`. The new
  `tools/prune-release.sh` (dry run by default, `--apply` to act; same shape as v32sim's) removes the clone, the
  private copies of v32sim's tools, those build scripts, bison reports, tracked build outputs and editor/merge
  leftovers, with `git rm` for tracked paths (staged) and `rm` otherwise.

- **The ComputerSoftware sources are looked up beside the project, not cloned into it.** `build-tools.sh` used to
  clone the 42 MB ComputerSoftware tree into `tools/vircon32/ComputerSoftware`. It now uses the path it is given,
  else `../ComputerSoftware`, else `../ComputerSoftware.git` (the same rule as v32sim), and only when there is none
  clones one to `../ComputerSoftware` at the verified commit. It says which checkouts it used, and notes when the
  checkout is not at the verified commit. An old `tools/vircon32/ComputerSoftware` can be deleted.
- **The last private copies of v32sim's tools are gone.** `tools/vircon32/` held `v32run.cpp`, `v32prof.cpp`,
  `v32shot.cpp`, `v32peek.cpp`, `shim/SDL.h` and `v32io-script.py`, left behind when the runners moved to v32sim
  (the shim and the script were byte-identical to v32sim's). The docs and sample comments now point at
  `$V32SIM_HOME/tools/v32io-script.py` and v32sim's `tools/oracle/bin/v32shot` / `v32prof`. `tools/vircon32/` is
  now just `build-tools.sh` and `check.sh`.

- **Real-toolchain fixes (arena/gc runtimes, null pointers, short initializer lists).** `make realcheck` compiled 150 programs with 7 failing
  and a failing self-check; now 181 programs, 0 failed, 112 cartridges all passing. Each was a real bug:
  - A literal `0` initializing a pointer at file or namespace scope (`Node *g = 0;`, `void (*hook)(int) = 0;`, `Node *t[2] = {0, 0};`, a
    pointer member in `S s = {1, 0};`), or assigned/compared through a `T*&` (`p = 0`, `p != 0`: the reference type hid the pointer, so
    `*p = 0` reached the compiler as an int) now prints `NULL` (`global_null_init`, `type_is_plain_pointer`; samples 177, 186, 190).
  - The gc runtime template declared `int __v32_gc_flist[33];` (raw text, so the `type[n] name` rewrite never saw it): the three arrays are
    tokens now (`@FLISTDECL@`...) spelled per target (samples 182, 185). The arena's destructor switch wrote `&((T *)obj)[i]`, which Vircon32 C
    rejects ("reference can only be applied to a memory address"): `((T *)obj) + i` (181).
  - `(void)x;` printed as written, which Vircon32 C rejects ("cannot convert expression to type void"); it is the bare statement `x;` now (181).
  - `DP q;` with `typedef D *DP;` got a destructor call `D__dtor__void(&q)` at scope exit (`make_destructible` used `type_to_class`, which looks
    through pointers; now `type_to_value_class`): "cannot assign struct D** to const-qualified struct D*" (184).
  - A short braced list zeroed nothing in a local (`P z{};` is `P z = {};`, `int a[4] = {0};`, `P p{1};`: the stock compiler writes only
    the listed values; sample 175 failed on the machine). Vircon32 mode now writes every list of an array or constructor-less struct in
    full (`fill_init`, `lower.c` phase 0: nested structs and arrays, `0.0`, `NULL`; a file-scope list only up to its last pointer slot, as the loader zeroes the rest;
    C input keeps its 0 null and its short file-scope lists; unions, brace-elided lists and non-aggregates are left alone). `new P()` and `new P[n]{}`
    zero what they allocate (`v32_new_P` ends `P zero = {0, 0}; *self = zero;`; `new T[n]{}` used to be the uninitialized `new T[n]`), `: m{}` on a member
    of a plain aggregate class zeroes it (it was ignored). The local string-to-array lists (`char w[8] = "hey"`) are padded too. See QUIRKS #31-#33.
  - `tools/vircon32/check.sh` also builds and runs the arena and gc variants listed in `tests/real_variants.txt` (the std_run builds) and the
    `out/18[125]program.c` ones, with `-F 6000` frames (`V32_FRAMES`); `182program` and `185program` are built with `--heap-words=16384` like their std_run.
    On the console the collector (push R0-R10, `mov {stk}, SP`, scan to 0x400000) collects cycles, keeps interior pointers and stack roots, and
    survives the mark-stack overflow and the 100x-heap churn; the out-of-space path (`182full`, `181full`) stops with its message.
  New sample `tests/195sample.cpp`. Output changes: every `--alloc=arena` program's destructor switch (`+ i`), every `--alloc=gc` program's three
  array declarations, `(void)x;` statements, global/namespace pointers (`= NULL`), `T*&` zero assignments, short local lists written out, a
  `P zero = {...}; *self = zero;` in `v32_new_P` of every constructor-less class (standard target too: `new P()` is zeroed there as well).
  Not fixed: `new int`/`new int()` (no `v32_new_int`), `new P{}` and `new T[n]()` (parse errors), a union with braces (#33) -- all as before.

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
  it is a call through the pointer now. (Enumerators of a namespace's enum were not renamed then; they are, see below.) Output change: every
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
- **Enumerators of a namespace's enum are namespaced.** `namespace a { enum E { RED }; } namespace b { enum F { RED }; }`
  printed two `RED`s in the C output. The enumerators are now `a__RED` / `b__RED` (`a__inner__RED` when nested), the scheme
  namespace variables got: sema.c registers them in the variable registry (`register_enumerators`; the entry's declaration
  is the `AST_ENUM_VALUE`) and lower.c phase 0c renames the enumerator and every reference, `case` labels and enumerators'
  values (`B = RED + 1`) included. All the ways of writing one resolve: `a::RED`, `a::E::RED`, unqualified inside the
  namespace (reopened too, methods too), `using namespace a;`, `using a::RED;` (the parser took only types, functions and
  variables there: "no 'RED' is declared in namespace 'a'"), default arguments, initializers, a local hiding it. Array
  sizes: `int g[a::BLUE];` and `int h[a::E::GREEN];` were "array size is not an integer constant expression"; the parser's
  enumerator table (`parse_enum_value`) is namespace-aware now (the enclosing namespaces first, then the bare name), and
  `ast_fold_int` folds a qualified id. The result stays a C `enum` (nothing became a `#define`); file-scope enums are
  unchanged. The enum's TYPE name is still not qualified (a type is `E` in the output, as a class is), so two enums of one
  name in different namespaces now give an error instead of a redefinition in the output. `enum class` is not supported (as
  before). Output change: every `<v32/*.hpp>` user (`v32__BlendAlpha`, `v32__FirstGamepadPort`, ...: those headers declare
  their enums in `namespace v32`); nothing else differs. Tests `tests/193sample.cpp` (default, arena, gc), `193bad.cpp`.
- **`extern` is read per declaration; an `extern` and the definition of the same variable are one.** The prescan noted
  the LINES that held the word `extern`, so `extern int a; int b;` in a namespace left both with their bare names. It now reads
  the declarator names that follow `extern` (up to the `;`, over continuation lines, after macro expansion: `#define EXT extern`
  works; comments, strings, `extern "C"` skipped; `prescan_decl_is_extern`), and `ast.c` `mark_extern_decls` records it on the
  declaration (`extern_decl`; C input has it from the grammar too). The lexer cannot keep the token (`lexer.l` stays as it is),
  and no marker the grammar already accepts would carry a name, so the text is read instead. What an `extern` variable becomes
  was not a bug: Vircon32 C has no `extern` ("not declared") and no linker, so the project's rule since `merge_tentative_globals`
  is one definition, no keyword: an `extern` nothing defines IS the variable (`namespace n { extern int c_side; }` is
  `int c_side;`, a file-scope `extern int x;` too), and one that something defines is dropped. What was wrong is that this only
  held for declarations in one list. `merge_extern_names` extends it across namespace blocks and to a file-scope definition of
  the same C name: `namespace n { extern int v; } namespace n { int v = 5; }` printed `int v;` and `int n__v = 5;`, `namespace hw
  { extern int v; } int v = 3;` printed `int v;` twice. Documented in `docs/VIRCON32_QUIRKS.md` #24. Not done: a variable an SDK
  header defines (`malloc_first_block`), redeclared `extern`, is defined a second time (v32c++ does not read variables out of
  headers). Tests `tests/192sample.cpp` (default, arena, gc), `192bad.cpp` (`extern "C"` is still not in the grammar).
- **`new T*[n]{p, q}`.** The grammar takes a braced list after `new T*[n]`; it is lowered like `new T[n]{...}`
  (`v32_new_arr_ptr_init(n, k)` allocates `max(n, k)` pointer words, the listed elements are plain assignments, `v32_new_arr_ptr_rest`
  nulls the rest, `NULL`: `-1` on Vircon32), on the same function-level temporaries and comma sequence, with casts between
  `T **` and the helpers' `int **`. `new T*[n]{}` is nulled too (unlike plain `new T*[n]`, as in C++11). Nothing is
  constructed or destroyed; any `--alloc` mode. Tests `tests/194sample.cpp` (default, arena, gc).
- **C input: a local `extern T x;` with no other declaration is the variable; `environ`.** Rogue stopped parsing at `src/save.c:167`,
  `extern char **environ;` inside `restore()` -- not at the `auto char buf[MAXSTR];` below it: `auto` has been accepted and dropped
  in C input all along (C++ input keeps type deduction), and still is. The real cause: `libc/v32libc.h` defined `environ` as the macro
  `((char **)0)`, so the declaration read `extern char ** ((char **)0);`. The header no longer declares it, as in POSIX; and a
  function-local `extern` used to declare nothing at all, so `environ = envp;` named a variable the output never had. It now behaves
  like a file-scope `extern` nothing defines (see above): hoisted to file scope (`is_static_local == 2`, `hoist_static_locals`) and dropped
  if the program declares or defines the name itself. Rogue now transpiles completely (every file, rc 0; the Vircon32 compiler
  was not available here, so nothing beyond the transpile step is claimed). `tests/191sample.c` (also built as standard C and run:
  `std_run.sh` takes a `.c` sample), and the Makefile transpiles the whole demo.
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
