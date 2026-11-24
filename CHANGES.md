# v32c++ changes

## 20261123-dev

Every feature below has a self-checking sample that passes on the machine (`make realcheck`: 233 programs compiled and
assembled, 0 failed, 164 cartridges all passing in v32sim, the `--alloc=arena` / `--alloc=gc` variants included).

### New features

- **Memory management, opt-in (output unchanged without the flags).**
  - `--alloc=arena`, `--heap-words=N` (default 262144): `new`, `new[]` and `std::vector` buffers come from one block taken from the
    SDK's `malloc` at the start of `main` (so a restart re-acquires it); `delete` runs the destructor and frees nothing;
    `v32::arena_mark/arena_release/arena_reset/arena_used/arena_free` give space back in bulk, running the destructors of the
    objects still alive (recorded in the arena), newest first, never twice for a `delete`d object or array. Out of space:
    "OUT OF MEMORY (arena full)" on screen and `hlt` (standard C: message on stderr, `exit(1)`).
  - `--alloc=gc`, `--gc-threshold=N`: a conservative mark-sweep garbage collector on the same hook (`__v32_alloc` / `__v32_release`;
    `__v32_gc_init()` first in `main`). One block of `--heap-words=N`; block headers (size, mark, free link), exact size-class free
    lists for small blocks, first fit for large ones, coalescing sweep; a page table finds the block of any interior pointer;
    explicit mark stack with a rescan fallback on overflow. Roots: a table of the file-scope variables that can hold a pointer
    (pointers, objects, arrays, `std::vector`s, hoisted statics, namespace-scope ones under their C names) and the machine stack
    plus registers (Vircon32: `push R0..R10`, read `SP`, scan up to `0x400000`; standard C: `__builtin_unwind_init` / `setjmp` up to
    `__libc_stack_end`). Collects when an allocation does not fit (then retries; still no room: "OUT OF MEMORY (gc heap full)"),
    every `--gc-threshold=N` words (default a quarter of the heap) and on `v32::gc_collect()`; `gc_used/gc_free/gc_collections`
    report. `delete` / `delete[]` run the destructor and free at once; collected objects get no destructor. Every block has one
    trailing unit (`n = n + 1`, as in Boehm's collector), so a pointer one past the end of an array (an `end()`) keeps its own block
    alive instead of pointing into the next one's header. Not done: precise per-class pointer maps, finalizers.
  - `<v32/memory.hpp>` takes `--alloc=arena` or `--alloc=gc` (including it without the flag is a compile-time error; the other
    mode's functions do not exist). `std::string` allocates nothing.
  - Verified on the machine: the collector (`push R0-R10`, `mov {stk}, SP`, scan to 0x400000) collects cycles, keeps interior
    pointers and stack roots, survives the mark-stack overflow and the 100x-heap churn; the out-of-space paths (`181full`,
    `182full`) stop with their messages. Tests `tests/181sample.cpp`, `181full.cpp`, `181string.cpp`, `182sample.cpp`,
    `182full.cpp`, `185sample.cpp` (the one-past-the-end pointer; fails with the old runtime).
- **Array `new` / `delete[]`.**
  - `new T[n]` default-constructs every element in order (a constructor with default arguments, or the vtable setup of a class with
    virtual functions and no constructor); for a class with a destructor one header word in front of the elements holds `n` and the
    pointer returned is past it, so `delete[] p` destroys the elements last first and frees from the header. A class without a
    destructor has no header. `n == 0`, a negative `n` and `delete[]` of null work; `delete[]` through a base pointer to a derived
    array is undefined, as in C++. `tests/179sample.cpp`.
  - `delete[]` finds the element class in every spelling: `infer_expr_type` types pointer arithmetic (`p + n`, `n + p`, `p - n`) and
    calls through a function pointer; a pointer to pointers is an array of pointers (a plain release); typedef'd, member, indexed,
    returned, cast, `auto`, namespace and `std::vector<T *>` / `std::array<T *, N>` operands are found. Where the type stays
    unknown (a `void *`) and the program has a class with a destructor, it is an error asking for a cast; a primitive, or a program
    without such a class, stays a plain free. An array of a typedef'd pointer to a class (`typedef D *DP; DP a[2];`) is an array of
    pointers (`type_to_value_class`). `tests/184sample.cpp`, `184bad.cpp`, `184ok.cpp`.
  - `new T[n]{a, b, ...}`: the listed elements are built from the list (`{x, y}` calls a constructor, `T(x)` or a plain value a converting
    or copy one, a scalar is assigned), the rest are default-constructed (zero for a scalar or a class without constructors); `n` smaller
    than the list allocates the list's length. One new grammar production, no conflicts; lowered (`lower.c` phase 6) to a comma
    sequence on a function-level temporary, so it works in any expression; the helpers `v32_new_arr_T_init/_rest` are emitted only
    for a program that uses the form. `tests/187sample.cpp`, `187bad.cpp`.
  - `new T[n]()` and `new int[n]()` (value-initialized array new) were a parse error; they mean `new T[n]{}`: elements
    value-initialized (a constructor, or zero). `tests/202sample.cpp`.
  - `new T*[n]` / `new T**[n]` is an array of pointers (plain words, no constructors, no count header; `v32_new_arr_ptr(n)`), `delete[]`
    releases it plainly; `new T*[n]{p, q}` assigns the listed pointers and nulls the rest (`NULL`: -1 on Vircon32), and `new T*[n]{}` nulls
    all. Grammar only (`pointer_opt` accepts `*&`; no change to the conflict counts). `tests/190sample.cpp`, `194sample.cpp`.
  - `T*&` (reference to a pointer) is lowered to `T**` like any reference: the call site passes the address of a variable, member or
    array element, the body assigns through it, a `T*&` local binds to a variable, and a function may return one. `tests/190sample.cpp`.
  - Scalar `new`: `new int`, `new int()`, `new int(5)`, `new float(2.5)`, `new int{7}`, `new char('x')`, `new bool(true)`, `new E(v)` and
    `new Node*` (one pointer, NULL, or `new Node*(p)`) allocate one word through the hook (`v32_new_int/float/char/bool/ptr`, emitted
    only when used, every `--alloc` mode); `delete` releases them (`delete pp` of a `T **` is a plain release). `tests/199sample.cpp`.
  - `new P{}`, `new P{1, 2}`, `new Counter{3}`: list-initialization of one object (grammar `NEW type_spec braced_init`; an aggregate is
    the zeroed `v32_new_P()` plus the listed members on a function-level temporary, `new P(1, 2)` too). `tests/198sample.cpp`.
- **List-initialization.**
  - `T x{...};` and `T x = {...};` (also in a `for` initializer, a `Counter a{1}, b{2};` tail and at file scope) of a class that declares a
    constructor is `T x(...)`; `{}` is `T x;`. A scalar becomes `int k = 3;` (`{}` is `0`; two or more values is an error). An aggregate
    and an array keep their C brace list; a nested brace item in a non-aggregate (`Pair x{{1},{2}}`) is a clear error. (They were
    emitted as C brace lists: the constructor never ran while the destructor did.) `ast.c` `rewrite_list_init`. `tests/175sample.cpp`,
    `175bad.cpp`.
  - In-class defaults with braces (`P m{}; P n{1, 2}; int w{5}; Counter c{3}; int a[3]{1, 2};`; they were dropped silently, an array
    member's `a[3]{...}` was a parse error), `: w{}` / `: w{5}` on a scalar member and `: p{1, 2}` on an aggregate member. `tests/198sample.cpp`.
  - `Counter arr[3] = { Counter(1), Counter(2), Counter(3) };` builds the elements in place (C++17 guaranteed elision; it used to build
    named temporaries and destroy both them and the elements: six destructor calls for three objects). Elements left out are
    default-constructed, `{2}` calls the constructor with those arguments, an element that is an object is copy-constructed in
    place, `Counter a[] = {...}` takes its length from the list, several dimensions and global arrays work (`ast.c`
    `keep_array_elements`, `lower.c` `construct_array_from_list`, `build_dtor_call_stmt`). `tests/174sample.cpp`.
  - Array members of class type take initializers: `class Holder { Counter m[2] = { {1}, Counter(2) }; };` and
    `Holder() : m{ {1}, {2} } {}` (`parser.y`: `IDENTIFIER braced_init` / `TYPE_NAME braced_init` in `member_init`; no conflict counts
    changed). Elements left out are default-constructed with the other members before the body; multi-dimensional member arrays are
    constructed in row order and destroyed backwards (they were not constructed at all). A list for an array of plain structs
    (`Pt m[2]` with `: m{ {1}, {2} }`) is an error. `tests/176sample.cpp`, `176bad.cpp`.
  - Brace-initialized unions (QUIRKS #33): `U u = {1};`, `U u{};`, unions inside structs and arrays are lowered in Vircon32 mode to the
    plain declaration plus assignments (`lower.c` phase 10b, `ui_init`): a list initializes the first member as in C, the rest is
    zeroed, `{}` zeroes it all (a pointer first member is `NULL`); file-scope and namespace-scope ones are assigned at the start of
    `main`; `new S()` / `new S[n]{}` of a struct holding a union write the zero as assignments. Not lowered: a union declared in a
    `for`/`if` condition. `tests/197sample.cpp`.
  - A typedef of a class names the class (`typedef Counter C; C x{1}; C y = {1}; C a[2] = { C(1), {2} };`), and a typedef can be a base:
    `typedef Base B; class D : public B {...};` (also `D() : B(3) {}`; `ast.c` `class_named`, `canonicalize_typedef_bases`).
    `tests/175sample.cpp`, `183sample.cpp`.
  - A brace list (`int a[2] = { count, get(count) };`) is walked by the this-injection, call-finalization, reference and `new`/`delete`
    passes like any other expression (a member name stayed bare in a method).
- **Globals, `goto` and `extern`.**
  - Global objects are destroyed in reverse construction order at every way out of `main` (after its own locals at a `return`, and when it
    falls off its end), by `__v32_destroy_globals()` with a once-only flag; a program that uses v32libc also sets
    `v32_exit_destructors`, which `v32_exit()` (`exit()`, `abort()`) runs before `v32_exit_hook`. Objects in a namespace (nested and
    reopened too) are constructed and destroyed like top-level ones, in source order (`lower.c` phases 7b/9c). `static` class data
    members are an error. `tests/177sample.cpp` (+ `177ret.cpp`, `177end.cpp`; `tests/globals_destroy.sh` runs them as standard C under
    `tests/177driver.c`), `186sample.cpp`, `186gc.cpp`.
  - `goto` out of a scope runs destructors: the objects live at the `goto` but not at its label are destroyed last built first before the
    jump (forward, backward, out of loops and nested blocks); a `goto` past the declaration of an object with a destructor is an error,
    as in C++. A label in front of a declaration (`again: D d(1);`) is split off first (`lower.c` phases 0b and 9). `tests/180sample.cpp`,
    `180bad.cpp`.
  - `extern` is read per declaration: the prescan reads the declarator names that follow `extern` (up to the `;`, over continuation lines,
    after macro expansion; comments, strings and `extern "C"` skipped) and `ast.c` `mark_extern_decls` records it, so `extern int a; int b;`
    is one of each. Vircon32 C has no `extern` and no linker, so an `extern` nothing defines IS the variable (printed once, plain) and a
    definition of the same C name anywhere (another namespace block, or file scope) absorbs it (`merge_extern_names`); QUIRKS #24.
    `tests/192sample.cpp`, `192bad.cpp`.
  - A variable an SDK header defines (`malloc_first_block` in misc.h), redeclared `extern`, is the header's variable: the redeclaration
    is kept for sema's types and left out of the output (it was a second definition, "already declared"; `prescan.c`
    `harvest_var_names`, `prescan_is_header_var`). `tests/201sample.cpp` (real headers), `201stub.cpp` (`tests/sdkstub`).
  - C input: a local `extern T x;` with no other declaration is the variable, hoisted to file scope (`is_static_local == 2`); `libc/v32libc.h`
    no longer defines `environ` as a macro (Rogue stopped parsing at `extern char **environ;`). Rogue transpiles completely.
    `tests/191sample.c`.
  - `native X;` is accepted when the real SDK headers are found and `X` is a struct harvested from a pass-through header (`date_info`
    from `time.h`): redundant, not a conflict. A `native` that conflicts with the program's own struct or typedef is still an error.
  - Two cart hints with one name (`#texture` / `#sound`) are an error naming both lines (`prescan.c` `check_cart_hint_name`).
    `tests/178bad.cpp`, `178bad_sound.cpp`.
- **Namespaces and enums.**
  - Out-of-line definitions with namespace-qualified names: `int a::f(E e) {...}`, `void a::C::m() {...}`, `a::C::C(int x) : ... {}`,
    `a::C::~C() {}`, nested (`a::b::h`) and operators too. They mean what they would inside the namespace: the C names are the
    namespace's (`a__f__E_int`), default arguments come from the declaration, the namespace's names (variables, enumerators, types,
    functions) are visible unqualified in the parameter list and the body, overloads are told apart. `ast.c`
    `qualify_namespace_definitions` wraps such a definition, where it stands, in `namespace a { ... }` and strips the namespace names from
    its qualifier (a parameter type that spells its own namespace out, `a::b::D`, is `D`); `parser.y` `ool_enter` makes the lexer look up the
    qualifier's namespace for the parameter types. No grammar conflicts. `tests/203sample.cpp`.
  - A free function's default arguments come from its prototype: `int f(int a, int b = 7); int f(int a, int b) {...}` (any namespace) was
    "expects 2 argument(s)" for `f(1)` (`register_free_function` copies them to the definition).
  - Variables declared in a namespace are `a__x` in the C output (`a__inner__x` nested), like namespace functions; file-scope variables and
    `extern` declarations keep their names. `sema.c` registers every variable with its namespace (`GlobalVarRegEntry`,
    `resolve_global_var`: the enclosing namespaces from the innermost out, then `using` directives and declarations); `lower.c` phase 0c
    (`namespace_vars_rename`) renames the declarations and every reference (qualified, unqualified inside a reopened namespace, a method,
    `using namespace a;`, `using a::x;`, asm `{x}` operands; a local or parameter hiding it). Global construction/destruction and the gc
    root table use the C names. A call through a namespace's function-pointer variable (`n::hp(5)`) works. `tests/188sample.cpp`, `188extern.cpp`.
  - Enumerators of a namespace's enum are `a__RED` (`a__inner__RED` nested); `a::RED`, `a::E::RED`, unqualified inside the namespace,
    `using namespace a;`, `using a::RED;`, `case` labels, default arguments and array sizes (`int g[a::BLUE];`: `parse_enum_value` is
    namespace-aware, `ast_fold_int` folds a qualified id) all resolve; the enum stays a C `enum`. `tests/193sample.cpp`, `193bad.cpp`.
  - Enum TYPE names are qualified too: `a::E` is `a__E` (declaration, variables, parameters, casts, `sizeof`, the mangled names of
    functions that take it: `g__a__E`), so `namespace a { enum E } namespace b { enum E }` is legal. `tests/200sample.cpp`.
  - `enum class Color : int { Red, Green };` (the underlying type is accepted and ignored), reached as `Color::Red` (C names
    `Color__Red`, `ns__Color__Red`); `switch`/`case`, comparisons, parameters (`f(Color)` and `f(int)` overload), `static_cast<int>(c)`,
    arrays, members and default arguments work, and a conversion to `int` needs no cast (more permissive than C++). Inside the enum,
    `Blue = Red + 5` works. A bare `Red` is an error: `'Red' is a scoped enumerator: write 'Color::Red'` (only for a name that resolves to
    nothing else: a variable, parameter, member, function, class or plain-enum constant of that name still works). `tests/205sample.cpp`,
    `205bad.cpp`.
- **Function-pointer initializers at file and namespace scope** use the C name: `D *(*g)() = f;`, `int (*h)(int) = &square;`,
  `int (*ops[2])(int) = { inc, dec };` printed the source name (no such function in the output) and lacked the `&`. They go through
  `fnptr_init` (`lower.c`): the mangled name with an explicit `&`, namespace-qualified names and arrays of pointers (typedef'd too)
  included, with the function prototypes printed ahead of the variables (`g_globals_name_functions`). An overloaded name is picked by the
  pointer's own type (`int (*p)(float) = over;`, locals and plain assignments too; `sema_function_matches_pointer_type`); no match, or
  more than one, is an error. Member functions are not covered. `tests/189sample.cpp`, `189bad.cpp`.
- **Options.** `V32CXX_SDK_INCLUDE=` (set but empty) turns the SDK header search off (setting the variable only ever added folders in
  front of the search, so on a machine with the DevTools installed sample 154, "SDK header not found", failed); unset keeps the full
  search.

### Fixes

- **Real-toolchain failures** found by `make realcheck` (150 programs compiled with 7 failing and a failing self-check at the time; each was a
  real bug):
  - A literal `0` initializing a pointer at file or namespace scope (`Node *g = 0;`, `void (*hook)(int) = 0;`, `Node *t[2] = {0, 0};`, a
    pointer member in `S s = {1, 0};`), or assigned / compared through a `T*&` (`p = 0`, `p != 0`) prints `NULL` (`global_null_init`,
    `type_is_plain_pointer`; samples 177, 186, 190).
  - The gc runtime declared `int __v32_gc_flist[33];` (raw text, so the `type[n] name` rewrite never saw it): the three arrays are tokens
    spelled per target (samples 182, 185). The arena's destructor switch wrote `&((T *)obj)[i]`, which Vircon32 C rejects: `((T *)obj) + i` (181).
  - `(void)x;` printed as written (rejected: "cannot convert expression to type void"); it is the bare statement `x;` now (181).
  - `DP q;` with `typedef D *DP;` got a destructor call at scope exit (`make_destructible` used `type_to_class`, which looks through pointers;
    now `type_to_value_class`; 184).
  - A short braced list zeroed nothing in a local (`P z{};`, `int a[4] = {0};`, `P p{1};`: the stock compiler writes only the listed
    values). Vircon32 mode writes every list of an array or constructor-less struct in full (`fill_init`, `lower.c` phase 0: nested structs
    and arrays, `0.0`, `NULL`; a file-scope list only up to its last pointer slot, as the loader zeroes the rest; C input keeps its 0 null
    and its short file-scope lists; unions, brace-elided lists and non-aggregates are left alone). `new P()` and `new P[n]{}` zero what they
    allocate (`v32_new_P` ends `P zero = {0, 0}; *self = zero;`; `new T[n]{}` used to be the uninitialized `new T[n]`), `: m{}` on a member of
    a plain aggregate class zeroes it. QUIRKS #31-#33. `tests/195sample.cpp`.
- **Value-initialization**: `new P[n]{}` gave pointer members 0, not NULL (-1): its `v32_new_arr_P_rest` zeroed the elements byte-wise, and now
  assigns a complete zero value to each (`P zero = {0, NULL, 0.0}; ... p[i] = zero;`, `lower_class_holds_pointer`). A zero written for an
  `enum` member is `(E)0`: the stock compiler rejects an int for an enumeration (`S s{};`, `{1, 0}` and `new S()` with an enum member did not
  compile; QUIRKS #34). `tests/196sample.cpp`.
- **Struct and union order**: every struct, class and union is written after the ones it holds by value (data members, inherited ones, array
  elements, typedef names, namespace members), in source order wherever that already works; unions used to be printed ahead of all structs, so
  `union V { int a; B b; };` failed in the stock compiler (QUIRKS #35). `union U;` is written for every union, so pointers to one may come
  first (#36). Two types that hold each other by value are an error (`'U' and 'S' hold each other by value`). `codegen.c` `agg_run`,
  `codegen_check_aggregates`. `tests/204sample.cpp`, `204bad.cpp`.
- **Scoped enumerators**: the stock compiler's answer to a bare `Red` was `identifier "Red" has not been declared`; v32c++ now says what to write
  (#37, above). `Blue = Red + 5` inside an `enum class` printed the bare `Red` and was rejected; it is `Color__Red`.
- **Definitions that spell a type differently** (`D operator+(D o)` declared, `a::b::D a::b::D::operator+(a::b::D o)` defined; `void f(D d)`
  declared in `a`, `void a::f(a::D d)` or `void b::q(a::D d)` with `using a::D;` defined) match the declaration and get **one C name**: sema's
  `canonical_type` (typedefs looked through, a qualified name reduced to its last component, as classes, unions and typedefs keep bare C names)
  now serves both `types_equal` and the mangling, so the prototype, the definition and every call agree for free functions, methods,
  constructors and operators. Before, the registry merged the pair but the output declared `b__q__D` and defined `b__q__a_D`. Visible in the
  output: parameters of a namespace-qualified class type mangle without the qualifier (`length_of__std_string_ref` is `length_of__string_ref`,
  `is__v32_String_ref_int_ptr` is `is__String_ref_int_ptr`). `tests/208sample.cpp` (greps for one prototype and one definition per name, and
  calls through every spelling). Left: a leading `::` (`::a::D`) is not in the grammar.
- **A qualified definition needs a matching declaration**: `int a::zz() {}` with no `zz` in `a`, and `int a::f(float) {}` when `a` only has
  `f(int)`, were accepted as new functions. Now `no declaration of 'zz' in namespace 'a' matches this definition` (`a::in` for a nested
  one); the class form (`void a::C::nope() {}`) was already `no matching declaration for out-of-line definition of 'nope' in class 'C'`.
  `ast.c` marks the definition it wraps in `namespace a` (`ATTR_QUALIFIED_DEF`), `sema.c` `check_qualified_definition`. `tests/207bad.cpp`.
- **Zeroing of members of a namespace-qualified type**: `new Holder`, `new Holder()`, `Holder h{}`, `new Holder[n]{}` / `()` (the `_rest`
  helper), short brace lists and unions did not zero a member of type `n::K` (struct/class), `n::N` (union), `n::E` (enum), a typedef of
  one, one in a nested namespace, or one seen through `using namespace n` (the zero helpers only recognized a plain identifier, and
  `lower_find_union` only the file scope). `c_resolved_type` now reduces a qualified name to its last component, `lower_find_union` looks into
  namespaces. Tested on the machine with the stack and the heap dirtied first, in the default, arena and gc builds. `tests/206sample.cpp`.
  Left for later: `new U` of a union itself (`v32_new_U` is never defined), `new E[n]` of an enum, `new T[n]{ T() }` for an aggregate (the
  listed element is not built).
- **`Counter arr[3]` and friends** no longer run constructors or destructors twice (see List-initialization).
- **Class members**: a class member named like an enumerator of an `enum class`, a local, a parameter or a function is not mistaken for it.

### Vircon32 output changes users will notice

- Enum and namespace names are mangled: every enum of the `v32` wrappers is `v32__BlendMode`, `v32__Key`..., enumerators `v32__BlendAlpha`,
  `v32__FirstGamepadPort`..., and so are the mangled names of functions that take them (`g__a__E`; they were `g__a_F`). Every `<v32/*.hpp>` user
  sees this; nothing else of those headers differs.
- Namespace variables are `a__x` (the Space Invaders demo's `SINE16`, `g_rng`, `82sample`, `125sample`, `184sample`, `186sample`); enumerators of a
  namespace's enum are `a__RED`; `enum class` enumerators are `Color__Red`.
- A pointer initialized or assigned `0` prints `NULL` (globals, namespace variables, `T*&` assignments); short local brace lists are written out in
  full; `new P()` / `new P[n]{}` of a constructor-less class end with `P zero = {...}; *self = zero;` (the standard target too), and a struct
  holding a union writes the zero as assignments.
- `--alloc=arena` programs: the destructor switch uses `+ i`; `--alloc=gc` programs: the three array declarations are spelled per target and
  the runtime gains the trailing-unit line; default output is unchanged.
- `(void)x;` is the statement `x;`.
- `--alloc=arena` / `--alloc=gc` programs no longer include `time.h`: the out-of-memory screen ends with the `wait` instruction, which is all
  `end_frame()` is, instead of calling it. A program defining its own `get_cycle_counter` (`169sample.c`) now builds with the allocators.
- Mangled names of functions taking a namespace-qualified class by type lose the qualifier (`length_of__string_ref`, not
  `length_of__std_string_ref`; `is__String_ref_int_ptr`, not `is__v32_String_ref_int_ptr`), so every spelling of the type agrees.
- Every union is forward-declared (`union U;`) and structs, classes and unions are written in dependency order, so a union now stands where the
  source put it instead of before every struct.
- C input: file-scope function tables carry the `&` in the `--target=standard` output too (`156sample.c`, `159lib_a.c`, `172sample.c`).
- A namespaced global object is constructed at the start of `main` (`82sample`'s `Counter`).

### Tooling and tests

- **`make clean` removes the stock toolchain too (`tools/vircon32/bin`), and `make realcheck` runs `tools/vircon32/build-tools.sh` first when
  `tools/vircon32/bin/BUILD-INFO` is missing**, so a fresh checkout or a cleaned tree needs only `make realcheck`.
- **The toolchain records how it was built.** `tools/vircon32/build-tools.sh` writes `bin/BUILD-INFO` (the ComputerSoftware path it used, that
  checkout's git `HEAD`, the v32sim path, and `compile --version`). `tools/vircon32/check.sh` refuses a `bin/` without it ("built by an older
  build-tools.sh ... rebuild it"), prints the compiler it is about to use, and warns when the ComputerSoftware checkout has moved since the
  build. `build-tools.sh` also smoke-tests the compiler it built: it compiles a file that includes `misc.h` from a scratch
  directory, as `check.sh` does, and stops with the compiler's message if the SDK headers can't be found. That is what made every
  C++ sample fail with `FAIL (toolchain)` / `cannot open include file "misc.h"` on macOS. Running the whole check on macOS needs a v32sim with
  the macOS fixes (oracle shim `SDL_GetBasePath`, no `sed -i` in `v32sim-selfcheck.sh`); update v32sim and rerun `build-tools.sh`.
- **The ComputerSoftware sources are looked up beside the project, not cloned into it.** `build-tools.sh` uses the path it is given, else
  `../ComputerSoftware`, else `../ComputerSoftware.git` (the same rule as v32sim), and only when there is none clones one to
  `../ComputerSoftware` at the verified commit; it says which checkouts it used and notes when the checkout is not at the verified commit.
  An old `tools/vircon32/ComputerSoftware` can be deleted. The last private copies of v32sim's tools are gone (`v32run.cpp`, `v32prof.cpp`,
  `v32shot.cpp`, `v32peek.cpp`, `shim/SDL.h`, `v32io-script.py`): docs and sample comments point at `$V32SIM_HOME/tools/v32io-script.py` and
  v32sim's `tools/oracle/bin/v32shot` / `v32prof`; `tools/vircon32/` is `build-tools.sh`, `check.sh` and the toolchain's `bin/`.
- **`make clean` removes what older tooling left in the tree, and `tools/prune-release.sh` removes tracked copies.** `clean` also deletes
  `tools/vircon32/ComputerSoftware`, `tests/Make.sh` and `tests/Make.bat` (sample 98 and `tests/line_markers.sh` ran v32c++ without
  `--no-build-scripts`; both use it now, and `make test` fails if either file appears in `tests/`). `tools/prune-release.sh` (dry run by
  default, `--apply` to act; same shape as v32sim's) removes the clone, the private copies of v32sim's tools, those build scripts, bison
  reports, tracked build outputs and editor/merge leftovers, with `git rm` for tracked paths (staged) and `rm` otherwise.
- **`make realcheck` is the gate.** It runs `make test`, then compiles, assembles and packs every output with the real Vircon32 toolchain and
  runs the self-checking samples in v32sim (`-F 6000` frames; `V32_FRAMES` overrides): 233 programs compiled, 0 failed, 164 cartridges passing.
  `tests/real_variants.txt` (`name source flags`) lists the arena and gc builds of the samples that allocate (179, 184, 186gc, 187-199, 102,
  105, 108, 110-112, 114-117, 125, 169 (with `--yield-points=10`), 176, 183, 202, 206, plus 182/185 with `-DGC_NO_THRESHOLD`); check.sh transpiles them itself. Every sample in
  `tests/` was also transpiled in both modes and compiled and assembled by the stock compiler: the one that failed in the allocator
  builds, 169sample (it defines the SDK's `get_cycle_counter`), passes since the runtime stopped including `time.h`. **A C program that
  defines a function the SDK headers also define** (`clear_screen`, `malloc`...) cannot take a header v32c++ itself adds (`misc.h` for `new`
  and the allocators, `video.h` for the out-of-memory message): that is now one error naming the function, the header and `--rename`
  (`codegen_check_sdk_clash`, `prescan_sdk_header_defines`; `tests/209bad.c`). New self-checking
  samples: `tests/174sample.cpp` ... `208sample.cpp` (with `*bad.cpp` error checks); a `.input` file beside a sample feeds a v32io device.
- `tests/std_run.sh NAME [flags]` builds a sample as standard C with `tests/stddriver.c` and runs it (default, `--alloc=arena`, `--alloc=gc`);
  `tests/arena_full.sh` and `tests/gc_full.sh` cover the out-of-space paths. `make test` also runs sample 90 and 154 against a stub SDK
  (`tests/sdkstub`) so the SDK-present paths are covered without the SDK.
- `make archive` writes `../v32cxx-project-YYYYMMDD.zip` (the date of `VERSION`); `make clean` removes `../v32cxx-project-*.zip`.

### Docs

- `docs/VIRCON32_QUIRKS.md` (en/es/fr): #24 (`extern`), #31-#34 (short lists, `void` casts, brace unions, int-to-enum), #35 rewritten (dependency
  order), #36 (forward-declared unions) and #37 (bare enumerator of an `enum class`) added.
- Man pages (en/es/fr) for `--alloc`, `--heap-words`, `--gc-threshold`, `<v32/memory.hpp>`; README (en/es/fr): the memory-management section,
  "Known limitations" and the tools section brought up to date; `docs/AGENTS.md` describes the current workflow.

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
