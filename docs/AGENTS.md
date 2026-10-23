# v32c++ -- notes for agents

(`AGENTS.md` is the shared agent-instruction file; `CLAUDE.md` only imports it.)

v32c++ is a cfront-style transpiler: **C++ (or C) in, Vircon32 C out**, written
in C. Your source stays ordinary; only the output is translated. The Vircon32
C compiler is much more limited than C, and most of that is hidden from you.

## Writing source for v32c++ (C++ or C input)

**Just write it, these all work:** `//` and `/* */` comments; adjacent string
literals; backslash continuation (macros, literals, `//` comments); the ternary
and comma operators anywhere; `switch` with fall-through, `goto`, `do`/`while`;
classes with inheritance and virtual functions, constructors/destructors,
operators, references, `const`; objects and structs by value (parameters,
returns); `new`/`delete`; namespaces and `using`; `auto` and range-based `for`;
`std::string`, `std::array<T,N>`, `std::vector<T>` (fixed per-type forms, not
templates); `static` locals and file-scope `static`/`extern`/`volatile`; float
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
`--warn-error[=kind,...]` / `--list-warnings`.

**Do not use** (clear error or unsupported): templates, exceptions, RTTI
(`dynamic_cast` warns only), multiple inheritance, lambdas, `enum class`,
`constexpr`, `mutable`, nested classes and class-scope enums, `static` class
members, `T *&`, 2-D array *parameters*, an object passed through `...`.

**Behaves differently than C++:**
- `unsigned`, `short`, `long`... are one signed 32-bit `int`; `double` is `float`.
- `>>` of a signed value is rewritten to shift arithmetically (a few extra instructions);
  an `unsigned` value shifts logically and is left alone. `--no-signed-shift` turns the
  rewrite off. `unsigned` `<`, `>`, `/`, `%` act unsigned too (`--no-unsigned-ops` turns that off).
- `new T[n]` / `delete[]` skip per-element constructors / destructors.
- `goto` out of a scope skips destructors; `const` is not enforced (C input drops it from the
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

## Working on the project

- Build: `make` (bin/v32c++, plus bin/v32c, a copy of it for C input). Tests: `make test`, then
  `tools/vircon32/check.sh` -- compiles, assembles and packs every output with the
  REAL Vircon32 toolchain and runs the self-checking samples in the headless
  emulator. Passing `make test` alone proves little.
- Add a test: `tests/NNNsample.cpp` (or `.c`), a line in the Makefile `test:`
  list, a global `int test_errors = -1;` set to 0 when all is well. A `.input`
  file beside it (made by `tools/vircon32/v32io-script.py` from `.actions`) feeds a
  v32io device. `v32shot` takes screenshots, `v32prof` measures CPU load.
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
