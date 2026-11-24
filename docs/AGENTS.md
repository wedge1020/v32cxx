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
operators, references, `const`; objects and structs by value (parameters,
returns); `new`/`delete`; namespaces and `using`; `auto` and range-based `for`;
`std::string`, `std::array<T,N>`, `std::vector<T>` (fixed per-type forms, not
templates); `new T[n]`/`delete[]` that construct/destroy every element (`new T[n]{a, b}` too); `--alloc=arena` or `--alloc=gc` with
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
(`dynamic_cast` warns only), multiple inheritance, lambdas, `enum class`,
`constexpr`, `mutable`, nested classes and class-scope enums, `static` class
members, `T *&`, 2-D array *parameters*, an object passed through `...`.

**Behaves differently than C++:**
- `unsigned`, `short`, `long`... are one signed 32-bit `int`; `double` is `float`.
- `>>` of a signed value is rewritten to shift arithmetically (a few extra instructions);
  an `unsigned` value shifts logically and is left alone. `--no-signed-shift` turns the
  rewrite off. `unsigned` `<`, `>`, `/`, `%` act unsigned too (`--no-unsigned-ops` turns that off).
- `delete[]` through a base pointer to a derived array is undefined. `new T[n]{a, b}` builds the listed elements and default-constructs the rest (n smaller than the list: the list's length is allocated; lowered in `lower.c` as a comma sequence on a function-level `__v32_nl_tmpN`, helpers `v32_new_arr_T_init/_rest`, only emitted when used). `delete[]` needs the element class (`delete_array_element_class`, `infer_expr_type`): unknown + some class with a destructor = error asking for a cast.
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
Every block has one trailing unit (`n = n + 1` in `__v32_alloc`, Boehm style), so a pointer one past the end of an object (`p + n`, an `end()`) lies in its own block, not the next one's (`185sample`). Roots (namespace-scope variables included, nested too): `__v32_gc_roots_init` (built from the file-scope `AST_VAR_DECL`s: pointers, class objects, arrays of them, `std::vector`s;
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
(179, 180, 181, 182, 183-187; `tests/globals_destroy.sh` runs 177 and 186), `tests/arena_full.sh` and `tests/gc_full.sh` for the out-of-space path.

## Working on the project

- Build: `make` (bin/v32c++, plus bin/v32c, a copy of it for C input). Tests: `make test`, then
  `tools/vircon32/check.sh` -- compiles, assembles and packs every output with the
  REAL Vircon32 toolchain (built by v32sim's tools/oracle) and runs the self-checking
  samples in v32sim (`V32SIM_HOME`). Passing `make test` alone proves little.
- Add a test: `tests/NNNsample.cpp` (or `.c`), a line in the Makefile `test:`
  list, a global `int test_errors = -1;` set to 0 when all is well. A `.input`
  file beside it (made by v32sim's `tools/v32io-script.py` from `.actions`) feeds a
  v32io device. Screenshots and CPU load: v32sim (`--frame-stats`, `tools/oracle`).
- Fix v32c++, don't work around it in demo or test source: the point is that
  input stays ordinary. Generated files `src/lexer.c` and `src/parser.c` are
  checked in: regenerate with flex 2.6.4 / bison after editing `lexer.l` / `parser.y`
  (`flex --outfile=src/lexer.c src/lexer.l`).
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
