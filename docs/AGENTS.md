# v32c++ -- notes for agents

(`AGENTS.md` is the shared agent-instruction file; `CLAUDE.md` only imports it.)

v32c++ is a cfront-style transpiler: **C++ (or C) in, Vircon32 C out**, written
in C. Your source stays ordinary; only the output is translated. The Vircon32
C compiler is much more limited than C, and most of that is hidden from you.

## Writing source for v32c++ (C++ or C input)

**Just write it, these all work:** `//` and `/* */` comments; adjacent string
literals; backslash continuation (macros, literals, `//` comments); the ternary
and comma operators anywhere; `switch` with fall-through, `goto`, `do`/`while`;
classes with inheritance (the base may be a typedef of a class: `typedef Base B; class D : public B`, `D() : B(3)`; `ast.c` canonicalize_typedef_bases) and virtual functions, constructors/destructors (array members of
class type take `= {...}` / `: m{...}`; global objects, namespace ones too, are built at the start of `main` and
destroyed when it ends, and by v32libc's `exit()`),
operators (all of C++'s but unary `&`: see "Operator overloading" below), references (`T&`, `T*&`), `const`; objects and structs by value (parameters,
returns); `new`/`delete`; namespaces and `using`; `auto` and range-based `for`;
`std::string`, `std::array<T,N>`, `std::vector<T>` (fixed per-type forms, not
templates); `new T[n]`/`delete[]` that construct/destroy every element (`new T[n]{a, b}` too; `new T*[n]` is a plain array of pointers, `new T*[n]{p, q}` nulls the rest; aggregate elements `{ T(), T(1, 2) }` / `{ {1, 2}, {} }` are zeroed by `_rest` then assigned; `new U` / `new U[n]` of a union and `new E[n]` of an enum work: `new_union_of`, `lower_union_new_bits`, codegen `emit_union_new_runtime`, enums are `int` arrays with casts); `new int(5)`, `new float{2.5f}`, `new Node*` (one scalar word through the hook: helpers `v32_new_int/float/char/bool/ptr`, `g_uses_scalar_new`, `lower.c` phase 6); list-initialization wherever a constructor call goes (`P m{1, 2};` / `int w{5};` / `int a[3]{1, 2};` as in-class defaults (`ast.c` `apply_default_member_initializers`), `: w{5}` / `: p{1, 2}` (sema `ival` 1 / 4, `lower.c` 8a / 8-pre), `new P{1, 2}` (a comma sequence on `__v32_nl_tmpN`), `new Counter{3}`); brace-initialized unions (`U u = {1};` is a declaration plus an assignment of the first member, a global one at the start of `main`: `lower.c` phase 10b, `ui_init`); `enum class E : int {...}`; out-of-line definitions with namespace-qualified names (`int a::f(E e) {...}`, `void a::C::m()`, `a::C::C(int x) : ... {}`, `a::C::~C()`: `ast.c` `qualify_namespace_definitions` wraps each in `namespace a { ... }` where it stands and strips the namespace names from its qualifier, `parser.y` `ool_enter` points the lexer at the namespace for the parameter types (and clears the lexer's pending `C::` qualifier, so `a::C::operator-(D d)` sees `D`), `sema.c` `register_free_function` copies a prototype's default arguments onto the definition, and `check_qualified_definition` rejects one no declaration of that name AND signature in the namespace matches (`ATTR_QUALIFIED_DEF`, set by ast.c); sema's `canonical_type` makes every spelling of a parameter type (`a::D`, `D` by `using`, a typedef) one type for `types_equal` and the mangled name; lower.c's `c_resolved_type` does the same for the zero helpers, so members of a namespace-qualified type are zeroed); `--alloc=arena` or `--alloc=gc` with
`<v32/memory.hpp>` (see "Memory management" below); `static` locals and file-scope `static`/`extern`/`volatile`; float
literals of any C form (`1e3`, `.5`, `5.f`); octal/hex/binary literals and
escapes; `int main()` with `return 0;`; `0` as a null pointer; `<v32/*.hpp>`
wrappers around the Vircon32 libraries (see `docs/V32IO.md` for the keyboard
and mouse); `va_list`/`...`; inline `asm { "..." }`; `native Name;` types (a plain C struct in an SDK
header, like `malloc_block`, is read, so `b->size` works); `#include` inside a
function or `asm { }` body (expanded where it stands).
String literals holding control codes (BIOS glyphs 14-31, `"\025"`) are rewritten
into hidden arrays for you.
C input (`v32c`, see `docs/C_INPUT.md`) also takes: several files at once (`v32c a.c b.c lib.v32a`,
linked by v32c++ itself: prototypes bind across files, file-scope `static` stays private, each file
has its own macros, `--mklib` makes libraries); designated initializers including GNU `[a ... b]`;
compound literals (file scope too); `_Static_assert`; function-typed parameters; `_Noreturn` /
`noreturn`; GCC's `__atomic_*` / `__sync_*` builtins (one CPU: plain operations);
`__builtin_frame_address` / `__builtin_return_address`; asm operands `{&var}`, `{&func}`, `{*p}`;
`__attribute__((rom))` const arrays (data in the cartridge image); `--rename old=new` for SDK name clashes;
`--yield-points[=N]` (cooperative yield checks; `__attribute__((no_yield))` opts a function out);
`--warn-error[=kind,...]` / `--list-warnings`;
`--assembler=CMD` (or `$V32CXX_ASSEMBLER`) names the assembler in the generated Make.sh/Make.bat.

**Do not use** (clear error or unsupported): templates, exceptions, RTTI
(`dynamic_cast` warns only), multiple inheritance, lambdas,
`constexpr`, `mutable`, nested classes and class-scope enums, `static` class
members, 2-D array *parameters*, an object passed through `...`.

**Behaves differently than C++:**
- `unsigned`, `short`, `long`... are one signed 32-bit `int`; `double` is `float`.
- `>>` of a signed value is rewritten to shift arithmetically (a few extra instructions);
  an `unsigned` value shifts logically and is left alone. `--no-signed-shift` turns the
  rewrite off. `unsigned` `<`, `>`, `/`, `%` act unsigned too (`--no-unsigned-ops` turns that off).
- `delete[]` through a base pointer to a derived array is undefined. `new T[n]{a, b}` builds the listed elements and default-constructs the rest (n smaller than the list: the list's length is allocated; lowered in `lower.c` as a comma sequence on a function-level `__v32_nl_tmpN`, helpers `v32_new_arr_T_init/_rest`, only emitted when used). `delete[]` needs the element class (`delete_array_element_class`, `infer_expr_type`): unknown + some class with a destructor = error asking for a cast.
- A variable declared in a namespace is `a__x` in the C output (`a::inner::x` is `a__inner__x`), like a namespace function; file-scope
  and `extern` ones keep their names. So do the enumerators of a namespace's enum (`a::RED` is `a__RED`, `a::E::RED`, `using a::RED`,
  `case`, array sizes and default arguments resolve; the enum stays a C `enum`; its TYPE name is qualified too, `a::E` is `a__E` (parser.y `make_enum_decl`: the declaration is named that way, `AST_ENUM_DECL.str2` keeps the bare name, and `parse_type_name_node` / `parse_qualified_type_node` rewrite every type use the symbol table resolves to such an enum, so signatures, casts, `sizeof` and mangled names agree); `enum class Color` has `ival = 1`: its enumerators are registered in sema.c under a scope named `Color`, so `Color::Red` is `Color__Red` / `ns__Color__Red`; a bare `Red` that resolves to nothing else is the error `'Red' is a scoped enumerator: write 'Color::Red'` (`lower.c` phase 0c `nv_walk` asks `sema_scoped_enumerator_hint`; a local, parameter, variable, function, class, plain-enum constant or member of that name wins; the enum's own initializers see their enumerators bare, `Blue = Red + 5`)). lexer.l drops `extern` in C++ input, so prescan.c reads it from the text, per
  declarator, after macro expansion (`scan_extern_text`, `prescan_decl_is_extern`: `extern int a; int b;` is one of each; `extern "C"`
  is not in the grammar and is skipped), and `ast.c` `mark_extern_decls` sets `AstNode.extern_decl`. The rule for `extern` on the output
  (Vircon32 C has no `extern`, no linker): an extern variable nothing defines IS the variable, printed once as a plain declaration; a
  definition of the same C name anywhere (also another namespace block, or the file scope) absorbs it (`merge_tentative_globals` per
  list, `merge_extern_names` across lists); a variable a pass-through SDK header defines (`malloc_first_block` in misc.h; prescan.c `harvest_var_names`, names of depth-0 definitions, `prescan_is_header_var`) is the variable: the user's `extern` redeclaration is kept for sema's types and left out of the output (codegen `visible`). A local `extern T x;` in a C function with no other declaration is hoisted to file scope
  (`is_static_local == 2`, `hoist_static_locals`). sema.c's variable registry (`GlobalVarRegEntry`: namespace, C name, `resolve_global_var`;
  enumerators go in it too, `register_enumerators`, the entry's decl being the `AST_ENUM_VALUE`)
  does the C++ lookup (enclosing namespaces, then `using`; qualified relative to the current one; `a::E::RED` strips the enum name);
  `lower.c` phase 0c (`namespace_vars_rename`, right after this-injection) renames the declarations and every reference, a local or
  parameter hiding it, asm `{x}` operands too, enumerator values included. parser.y folds `a::RED` / `a::E::RED` in array sizes with a
  namespace-aware table (`parse_enum_value`). Later phases (global construction, the gc root table, `infer_expr_type`)
  see the C name only, and the registry answers to both.
- A leading `::` (`::x`, `::f()`, `::a::D`) never reaches the grammar (it would move the pinned conflict counts): `parse_lex` at the end of parser.y drops it, arms the file scope in the lexer and marks the next name token (`parse_take_rooted`); a rooted name is an `AST_QUALIFIED_ID` with `ival` 1, looked up in the file scope only (sema `sema_qualifier_of` writes it `::a`), and phase 0c renames a local that `::x` would be hidden by to `x__hid`. `::new`/`::delete`/`::operator` are an error.
- Structs, classes and unions are written in dependency order (`codegen.c` `agg_run`: a type after the aggregates it holds BY VALUE, through arrays, typedefs, inheritance and namespaces; source order where that already works; a by-value cycle is the error from `codegen_check_aggregates`), and every union gets a `union U;` forward declaration, so pointers need no order (QUIRKS #35, #36).
- A file- or namespace-scope function pointer (or an array of them) initialised with a function name gets the C name and
  the `&` (`fnptr_init`, `lower.c`, with the overload picked by the pointer's type, an error when none or several fit);
  prototypes are then printed ahead of the variables (`g_globals_name_functions`).
- Global objects in a namespace (nested too) are built and destroyed like top-level ones, in source order (phase 7b/9c in `lower.c`, `collect_scope_vars`); `static` class data members are an error; two `#texture`/`#sound` hints with one name are an error.
- `goto` into a scope past the declaration of an object with a destructor is an error; `const` is not enforced (C input drops it from the
  output, since the SDK compiler enforces it where C does not).
- Inline `asm` for another CPU (RISC-V) is passed on unchanged and cannot assemble.
- A class in a namespace keeps its bare C name (don't name your own class
  `String`, `Keyboard`, `Mouse`, `Channel`...).
- An SDK pointer's null is `-1`, and the SDK compiler drops a function nothing
  calls (and the `asm` data inside it): see QUIRKS #29.
- A declaration directly in a `case` body (no braces) is rejected by Vircon32 C.
- The SDK compiler warns "argument X is not used" (and stops after 10 warnings):
  `(void)x;` in source, or `--silence-unused-args` for every function.
  An unknown type makes `T * b;` read as a multiplication: v32c++ warns.
- An overloaded call on a raw C API result needs a typed local first.
- `--target=standard` prints standard C instead (ternary kept, `int main`).

`docs/VIRCON32_QUIRKS.md` has the full list **and a table of what each Vircon32 C
rule means for your source**. Read its first section before assuming a
construct is unsupported: its headings state the Vircon32 C *output* rule,
which v32c++ almost always works around.

## Operator overloading

`parser.y` `operator_symbol` builds the name (`operator%`, `operator<<=`, `operator->`, `operator,`...; every operator token the lexer
makes is there, and the grammar's `expr` already parses all of them, so no lexer or conflict change). `sema.c` resolves a use
(`resolve_operator_use`, tables `binop_operator_name` / `assign_operator_name` / `unop_operator_name`; a BINOP / ASSIGN / UNOP / SUBSCRIPT node
gets a `CallResolution`) and `lower.c` `rewrite_operator_use` turns it into the call, a reference return into a `deref` of it. Members win
unless none can take the operand and a free one can; free ones are looked up here and, failing that, in every namespace (by parameter
types: classes keep bare names, so that is C++'s ADL). Mangled names: `op_add op_sub op_mul op_div op_assign op_not op_eq op_ne op_lt op_gt op_le op_ge
op_addeq op_subeq op_muleq op_diveq op_index op_call op_inc op_dec` and `op_mod op_band op_bor op_bxor op_bnot op_shl op_shr op_modeq op_andeq op_oreq
op_xoreq op_shleq op_shreq op_land op_lor op_arrow op_comma`; the unary `operator*()` / `operator+()` (a member with no parameter, a free function
with one) are `op_deref` / `op_pos` (`mangle_operator_symbol`; `make_free_func_info` tells a free function from a member), unary `-` is `op_sub`.
Unary `~ + * ->` take only a zero-parameter member or a one-parameter free function (`strict_unary`); `*this << x` inside a member needs
`operator_is_late_addition` (infer_expr_type gives `this` its class, not a pointer; the older operators keep their old behaviour). `p->m`
(`check_node` AST_MEMBER) wraps `p` in an AST_UNOP `arrow` per `operator->`, again while the result is an object. `+x` is `AST_UNOP "+"`
(prints as `(x)`). Unary `&` is the error from `check_operator_decl`, which also rejects a free or parameterized `operator->`.
Evaluation order: Vircon32 C evaluates call arguments right to left, C++17 wants the left operand of `<< >> && || ,` first: `rewrite_operator_use`
marks the call (`seq_args`) and `lower.c` `sequence_first_operand` (phase 6) assigns a left operand with side effects to a `__v32_seq_tmpN` first.
The overloaded `&&` / `||` do not short-circuit (both sides evaluated), as in C++. An object returned by value and bound to a reference is parked in a
temporary (`abi_expr`, `(tmp = call(), &tmp)`); `--target=standard` still prints `&call()` there (gcc rejects it for a by-value rvalue). `tests/213sample.cpp`, `213bad.cpp`.

## Memory management (`--alloc=arena`, `--alloc=gc`)

Every generated allocation goes through one hook: `malloc`/`free` by default (the output is then
what it was before the flag existed), `__v32_alloc(units)` / `__v32_release(ptr)` with `--alloc=arena`
(`g_alloc_arena`, `g_heap_words`; `--heap-words=N`, default 262144). Where it lives:
`codegen.c` (`emit_obj_alloc`, `emit_array_new_runtime_class`, `emit_array_delete_runtime`,
`emit_arena_runtime`: bump allocator over one SDK `malloc` block, a 4-word destructor record
`[prev, class id, count, skip]` before each object whose class has a destructor with a body,
`__v32_arena_dtor` = a switch on the class id, so no function pointers), `generic.c` (`@ALLOC@`/`@FREE@`
in the `std::vector` template), `lower.c` phase 9d (`__v32_arena_init()` first in `main`, so a
`v32_restart` re-acquires the block) and phase 6 (`delete[]` names `v32_delete_arr_Class`), `macro.c`
(`__V32_ARENA__`, which `v32/memory.hpp` tests with `#error`). `new T[n]` of a class with a destructor
keeps `n` in one header word before the elements (`delete[]` reads it); a class without one has no
header. `std::string` is fixed-capacity (no heap), `v32::HeapBlock`/user `malloc` are not rerouted.
`--alloc=gc` (`g_alloc_gc`, `--gc-threshold=N`, `V32_ALLOC_HOOK` = arena or gc) reuses the hook, the
phase 9d slot (`__v32_gc_init();` instead) and `memory.hpp` (`__V32_GC__`, `gc_collect/gc_used/gc_free/gc_collections`;
the arena_* and gc_* functions each exist in their own mode only, so a call in the other one is "no function ... is declared").
`codegen.c` `emit_gc_runtime`: the runtime is the template `gc_runtime_tpl` (plain C that must stay valid Vircon32 C:
no ternary/unsigned, declarations first, a returned pointer goes to a local before it is indexed) with `@TOKEN@`s filled in
per target. One block from the SDK `malloc` (+ page table + 512-entry mark stack behind it); blocks tile `[0, top)`, header
`[size, state 0 free/1 used/3 marked, next-free offset]`, 32 exact size-class free lists + a first-fit list, wilderness
bump, sweep coalesces and gives a trailing free run back to the wilderness. Marking: page table (256 units, rebuilt at the start of every
collection) maps any interior word to its block; explicit mark stack, overflow = rescan all marked blocks until quiet.
Every block has one trailing unit (`n = n + 1` in `__v32_alloc`, Boehm style), so a pointer one past the end of an object (`p + n`, an `end()`) lies in its own block, not the next one's (`185sample`). Roots (namespace-scope variables included, nested too, under their C names `a__x`): `__v32_gc_roots_init` (built from the file-scope `AST_VAR_DECL`s: pointers, class objects, arrays of them, `std::vector`s;
not plain numbers/enums/function pointers/`extern`/rom; hoisted function statics are file-scope, so they are included) and the
stack: Vircon32 `__v32_gc_collect` pushes R0-R10 in asm, reads SP, scans `[SP, 0x400000)`; standard C uses
`__builtin_unwind_init` + `setjmp` and scans from `__v32_gc_run`'s frame to `__libc_stack_end`. `delete` frees at once
(`__v32_release` ignores a pointer that is not a live block start, so a double delete is harmless); no destructor records exist in
gc mode, collected objects get no destructor. Block memory is zeroed on allocation. Out of space after a collection:
`__v32_gc_fatal` (shares `emit_alloc_fatal` with the arena). Not done: precise per-class pointer maps, finalizers.
Tests: `182sample` (`std_run` with `--alloc=gc --heap-words=16384`, also with `-DGC_NO_THRESHOLD --gc-threshold=100000000`),
`tests/gc_full.sh` (182full). Conservative scanning makes the sample's "dropped" objects depend on stale stack words: it
`scrub()`s the stack first and is meant for the default `-O0` build (at `-O2` the cycle can be retained).
`goto` destructors: `lower.c` phase 9 (`goto_prepass`, `destruct_goto`), phase 0b splits `label: D d;`.
Behaviour tests built as standard C: `tests/std_run.sh NAME [flags]` with `tests/stddriver.c`
(179, 180, 181, 182, 183-194; `tests/globals_destroy.sh` runs 177 and 186), `tests/arena_full.sh` and `tests/gc_full.sh` for the out-of-space path.
`tools/vircon32/check.sh` runs the arena and gc builds on the REAL machine too: the `make test` outputs `out/181program.c` (arena), `182`, `185` (gc, `--heap-words=16384`: 182's threshold checks assume that heap)
and every line of `tests/real_variants.txt` (`name source flags`: the std_run arena/gc builds of 179, 184, 186gc, 187-195, plus 182/185 with `-DGC_NO_THRESHOLD`), transpiled by check.sh itself to
`out/<name>program.c`, compiled by the stock compiler and run in v32sim (`-F 6000` frames: the gc churn tests need more than 1200; `V32_FRAMES` overrides). Add a sample's variants there.
The gc runtime (`push R0`..`mov {stk}, SP`, stack top 0x400000) and the arena run on hardware this way. Raw-text runtime code must be valid Vircon32 C too: `int [n] name` arrays
(`FLISTDECL`/`ROOTADECL`/`ROOTNDECL` in `gc_runtime_tpl`), no `&expr[i]` of a cast pointer, no `(void)` cast.

## Working on the project

- Build: `make` (bin/v32c++, plus bin/v32c, a copy of it for C input). **The gate is `make realcheck`**: it runs `make test`, then
  `tools/vircon32/check.sh`, which compiles, assembles and packs every output with the REAL Vircon32 toolchain and runs the
  self-checking samples in v32sim. `make test` alone proves little (it only transpiles, and runs the standard-C builds). A change is done
  when `make realcheck` reports 0 failed and all cartridges passing, and `V32CXX_SDK_INCLUDE=<dir of the SDK headers> make test` passes too.
- The real toolchain lives in `tools/vircon32/bin` (never rebuild it casually). `tools/vircon32/build-tools.sh [ComputerSoftware]` builds it
  through v32sim's `tools/oracle/build.sh`, finding `../v32sim` (or `../v32sim.git`) and `../ComputerSoftware` (or `../ComputerSoftware.git`; with
  none it clones one beside the project at the verified commit), and writes `bin/BUILD-INFO` (ComputerSoftware path and git HEAD, v32sim path,
  `compile --version`). `check.sh` refuses a `bin/` without BUILD-INFO (built by an older script: rebuild it) and warns when the
  ComputerSoftware checkout moved since the build. When a whole run fails with `FAIL (toolchain)`, suspect a stale `bin/` first. `make realcheck` runs `build-tools.sh` itself when `bin/BUILD-INFO` is missing, and `make clean` removes `tools/vircon32/bin` (so never run it while you are testing with that toolchain). On macOS the check needs a v32sim with the macOS fixes (oracle shim `SDL_GetBasePath`, no `sed -i`).
- **v32sim is the one place for running, profiling and debugging** carts (`--frame-stats`, `--profile-*`, `--dump`, `tools/v32sim-selfcheck.sh`,
  `tools/v32io-script.py`, `tools/oracle`). Do not add runners, profilers or debuggers to this project: `tools/` holds `embed-header.sh`,
  `prune-release.sh` and `vircon32/{build-tools.sh,check.sh,bin}` only.
- Add a test: `tests/NNNsample.cpp` (or `.c`), a block in the Makefile `test:` list, a global `int test_errors = -1;` set to 0 when all is well
  (it is then run by `check.sh` automatically). `*bad.cpp` files hold the error checks (`! $(TBIN) ...; grep -q "..." out/NNNbad.txt`). A `.input`
  file beside a sample (made by v32sim's `tools/v32io-script.py` from `.actions`) feeds a v32io device. If the sample allocates, add its arena and gc
  builds to `tests/real_variants.txt` (`name source flags`; the std_run builds in the Makefile are standard C only, the variants go through the stock
  compiler and v32sim) -- note that the allocator runtime includes only `misc.h` (malloc) and `video.h` (the out-of-memory screen; `end_frame()` is written as its `wait` instruction, so `time.h` stays out and 169sample's own `get_cycle_counter` builds in every mode). The SDK headers DEFINE their functions: a C program defining one that `misc.h`/`video.h` also defines is the error of `codegen_check_sdk_clash` (names the header; `--rename`; QUIRKS #38, `tests/209bad.c`). Keep any new runtime text to those two headers, or extend that check.
  To find which line of a self-checking sample fails, replace ` e++;` with `{ if (e == 0) e = __LINE__; }` in a copy: `test_errors` then names it.
- Output changes must be explained: diff the generated C of every `tests/*`, `demos/` and `v32/` user, before and after, in the default,
  `--alloc=arena`, `--alloc=gc` and `--target=standard` modes.
- Conflict counts of the grammar are pinned (`%expect 83`, `%expect-rr 1` in parser.y): a change that moves them is a design question, not a fix.
  `src/parser.c` and `src/lexer.c` are checked in; regenerate with bison / flex 2.6.4 after editing `parser.y` / `lexer.l`. Where flex is not installed,
  edit `parser.y` only, and when `make` then tries to run flex `touch src/lexer.c` and run `make` again.
- Fix v32c++, don't work around it in demo or test source: the point is that
  input stays ordinary.
- Lowering lives in `src/lower.c` (numbered phases; the header comments say
  what each does). Printing is `src/codegen.c`. Vircon32-only rewrites are
  guarded by `g_target == TARGET_VIRCON32`.
- Version: `inc/v32cxx.h` (`#define VERSION "YYYYMMDD-dev"`); `make version`
  stamps the man pages, the Spanish and French ones (`man/es`, `man/fr`) with a
  localized date. Man pages are installed gzipped. `DEST_LANG=all|en|es|fr` (Makefile
  and CMake) selects which language's man pages and docs are installed. The GitHub workflow is
  manual (`workflow_dispatch`) only: a `release` run builds every platform's zip and creates the
  GitHub release `v<VERSION>` (a draft, unless `publish_release` is ticked) with the zips, a
  `SHA256SUMS.txt` and the text of `.github/release-notes.md`; update that text for each release.
  Man page names never contain `::` (Windows cannot check such a file out): the `v32::X` alias
  pages are stored as `man*/man3/v32__X.3v32` and the Makefile, CMake and `stage_package.sh`
  install them as `v32::X` on Unix.
- Docs: `README.md` (features, limits), `docs/VIRCON32_QUIRKS.md`,
  `docs/C_INPUT.md`, `docs/V32IO.md`, `docs/STRING.md`, `docs/GENERICS.md`,
  `docs/INLINE_ASSEMBLY_SUPPORT.md`, `docs/NATIVE_PASSTHROUGH.md`;
  `docs/DESIGN_NOTES.md` is the long development history (not a reference).
  README and the docs (except `DESIGN_NOTES.md` and `TRANSPILER_REPORT.md`) have
  Spanish and French translations (`*.es.md`, `*.fr.md`), and so do the v32c++ man
  pages (`man/es`, `man/fr`): when you change an English document or page, update
  the translations too, or say they are stale. Every document carries a
  `<!-- langlinks -->` line that the single-language install strips.
  When a limitation is lifted, update README "Known limitations", the quirks table,
  and this file.
