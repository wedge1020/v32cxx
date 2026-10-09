# v32c++

**A C++-subset-to-C transpiler, targeting the [Vircon32](https://www.vircon32.com) fantasy console's C compiler.**

`v32c++` lets you write games and programs for Vircon32 in a subset of
C++ — classes, inheritance, constructors and destructors, virtual
functions, operator overloading, references, `std::string`,
`std::vector` — and translates them into the C that the Vircon32
toolchain already knows how to build. It is the approach Bjarne
Stroustrup's original `cfront` took in the 1980s: rather than compiling
C++ straight to machine code, translate it to C first and let an
existing, trusted C compiler do the rest. It also reads plain C (old
Unix C included), handling everything Vircon32's own C dialect does
differently.

> **Status: functional, and approaching its first release.** The whole
> pipeline — preprocessing, parsing, semantic analysis, lowering and code
> generation — works end to end and is checked against the **real**
> Vircon32 compiler, assembler and console (`make realcheck`: 96
> programs built into cartridges, 30 of them self-checking at run time on
> a headless console). Five sizeable demos (three C++ games, Rogue, and a
> C game) build with it. See [Known limitations](#known-limitations) for
> the honest picture of what is missing.

## Why this exists

Two audiences, one project:

1. **A teaching tool.** It started as course material for a class that
   teaches game programming on Vircon32, beginning in plain C and moving
   on to C++ and object-oriented design. Showing students *how* a virtual
   function or a constructor gets parsed, resolved and lowered into plain
   C is as much the point as having a working tool — so the generated C
   is meant to be read (see `-vv`).
2. **A community tool.** The Vircon32 homebrew community writes games in
   C; this lets people opt into a small, well-understood subset of C++
   instead, without the weight of templates, exceptions or a full STL.

It is **not** an attempt to support all of C++ — see
[Deliberately out of scope](#deliberately-out-of-scope).

## Contents

- [What it supports](#what-it-supports)
- [C++ headers for the Vircon32 API](#c-headers-for-the-vircon32-api)
- [Keyboard and mouse: v32io](#keyboard-and-mouse-v32io)
- [Known limitations](#known-limitations)
- [Requirements](#requirements) · [Building](#building) · [Checking with the real toolchain](#checking-with-the-real-toolchain)
- [Using it](#using-it) · [Cart packaging](#cart-packaging) · [Output dialects](#output-dialects)
- [Demos](#demos) · [Documentation](#documentation) · [Project layout](#project-layout)
- [Installing](#installing) · [Versioning](#versioning) · [Feedback](#feedback)

## What it supports

### Classes and objects

- `class` and `struct` (the only difference is the default access),
  `public`/`protected`/`private` (enforced, through inheritance too),
  `friend` classes and functions, forward declarations (`class X;`),
  classes inside namespaces.
- **Single inheritance**, with derived-to-base pointer and reference
  conversions.
- **Constructors and destructors**, in-class or out-of-line; overloaded
  constructors; copy constructors; default arguments. Member-initializer
  lists for base classes (`: Base(args)`), primitive members (`: x(v)`,
  in *declaration* order, as C++ requires) and members that are objects
  (`: mVel(vx, vy)`). In-class default member initializers
  (`int mLives = 3;`). A base with a default constructor is constructed
  implicitly; destructors chain to members and bases.
- Objects on the stack (`Shape s;`, `Shape s(7);`), as unnamed
  temporaries (`return Vec(x, y);`, `v.push_back(Enemy(1, 2, 3));`), in
  arrays (constructed and destroyed element by element, member arrays
  included), at file scope (constructed at the top of `main`, with
  constructor arguments if given), and on the heap with `new`/`delete`.
  Destructors run at every scope exit: the closing brace, `return`,
  `break`, `continue`.
- **`virtual` functions** and dynamic dispatch through vtables, pure
  virtual functions (`= 0`), virtual destructors (`delete` through a
  base pointer).
- **Operator overloading**: arithmetic, comparison, compound assignment,
  unary minus and `!`, assignment, `[]`, function call `()` (function
  objects: `f(5)` on an object), and prefix/postfix `++`/`--` (C++'s
  `operator++()` / `operator++(int)`), as members or free functions,
  used with natural syntax (`a + b`, `list[i]->draw()`, `++it`, `it++`).
- **`override` and `final`** on member functions, and `final` on a class,
  checked as C++ checks them (an `override` that overrides nothing,
  overriding a `final` function and deriving from a `final` class are
  errors); they stay ordinary names elsewhere (`int final;`). `explicit`
  is accepted.
- **Overloading** of functions and methods, resolved by argument count
  and type; it reports an ambiguity instead of guessing.
- Objects **by value**: a struct or class larger than one word can be
  passed and returned by value; v32c++ rewrites it into the hidden
  pointers Vircon32 C needs (`docs/VIRCON32_QUIRKS.md` #11).
- **References** — parameters, return values, locals, data members
  (`int &r;`, bound in the constructor's initializer list), `const T &`
  binding to temporaries.
- `const` (variables, pointers to const, `T * const`, `const` member
  functions) — accepted and emitted, not enforced.

### The rest of the language

- Namespaces (nested, qualified calls, namespaced C names for functions:
  `v32::draw(int,int,int)` becomes `v32__draw__int_int_int`).
- `auto` locals and range-based `for` (over arrays, `std::array`,
  `std::vector`, and any class with `begin()`/`end()`).
- **`std::string`** (`#include <string>`), and **`std::array<T, N>`** and
  **`std::vector<T>`** (`#include <array>`, `<vector>`) — not templates,
  but fixed forms the transpiler expands per element type. See
  [`docs/STRING.md`](docs/STRING.md) and
  [`docs/GENERICS.md`](docs/GENERICS.md).
- The whole C statement and expression language: `switch` with real
  fall-through, `do`/`while`, `goto`, the ternary operator and the comma
  operator (both rewritten for Vircon32 C, which has neither), bitwise
  operators at C's precedence, `sizeof`, C-style and C++-style casts
  (`dynamic_cast` with a warning: there is no RTTI), `enum`, `union`,
  `typedef`, function pointers in standard **or** Vircon32 declarator
  syntax (output is always what the target needs), multi-dimensional
  arrays with constant-expression sizes, brace initializers, hex/octal/
  binary literals with suffixes, `nullptr`, adjacent string
  concatenation, multiple declarators (`int a, *b, c[4];`).
- `static` locals (moved to file scope under unique names), file-scope
  `static`, `extern`, `volatile`, `inline`, `register` (accepted;
  Vircon32 C has none of them). `unsigned`, `signed`, `short`, `long`
  are the one 32-bit `int` type (`unsigned` with a warning); `double`
  and `long double` are `float`, Vircon32's one floating type. Bit-fields
  are accepted as full words with a warning, or rejected with
  `--reject-bit-fields`.
- **Inline assembly** in Vircon32's `asm { "..." }` form, plus GCC basic
  `asm("...")` re-emitted in that form — see
  [`docs/INLINE_ASSEMBLY_SUPPORT.md`](docs/INLINE_ASSEMBLY_SUPPORT.md).
- **`native Name;`** declares a type defined by a pass-through C header
  (`date_info`, `game_signature`) so it can be used by pointer — see
  [`docs/NATIVE_PASSTHROUGH.md`](docs/NATIVE_PASSTHROUGH.md). Struct and
  typedef names from SDK headers v32c++ can read are declared native
  automatically.

### A C++-side preprocessor

Before parsing, v32c++ does what a C++ preprocessor would:

- `#include` of `.hpp`/`.cpp` files is inlined, recursively, with
  `#pragma once`. Quote form looks next to the including file, then in
  each `-I` directory, then the current directory; angle form only in the
  `-I` directories. Both then fall back to `$V32CXX_INCLUDE` and the
  installed header directory (`/usr/local/Vircon32/v32c++/include` by
  default, [`inc/config.h`](inc/config.h)). Multi-file projects work the
  usual way, and errors name the file they are in.
- `#define`/`#undef` (object-like and function-like, `#`, `##`,
  `__VA_ARGS__`), `-D`/`-U`, predefined `__V32CXX__`, `__FILE__`,
  `__LINE__`; `#if`/`#ifdef`/`#elif`/... with `defined()`; `#error`,
  `#warning`.
- `.h` includes pass through to the generated C, **and** are read when
  v32c++ can find them (`$V32CXX_SDK_INCLUDE`, the `include/` folder next
  to the Vircon32 `compile` on your `PATH`, then
  `/usr/local/Vircon32/DevTools/include`), so SDK macros like
  `screen_width` or `color_red` work in array sizes and `#if`, and are
  typed for overload resolution. `-v` lists the folders searched.
- Defines are passed through to the generated C, and named constants
  keep their names there; macros Vircon32's own preprocessor can't take
  are expanded instead. Line numbers in diagnostics and `-g` maps point
  at the original file and line.

### C input

A file named `.c` is transpiled as C: its own `.h`/`.c` includes are
expanded, struct tags get their own namespace, K&R-style prototypes that
say less than the definition are unified, variadic functions work, the
null pointer is 0, and `main(argc, argv)` is accepted. With the small C
library and curses in `libc/`, that is enough to build the Unix game
Rogue from its sources (`demos/c/rogue`). See
[`docs/C_INPUT.md`](docs/C_INPUT.md).

## C++ headers for the Vircon32 API

`v32/` holds C++ headers in the `v32::` namespace, one per SDK header —
`video.hpp`, `input.hpp`, `string.hpp`, `time.hpp`, `audio.hpp`,
`math.hpp`, `misc.hpp`, `memcard.hpp` — plus **`v32io.hpp`**,
**`keyboard.hpp`** and **`mouse.hpp`** (keyboard and mouse, below). Each SDK wrapper passes its `.h` through and adds only what
the C API can't express well: typed enums instead of `#define`s, overload
sets (`v32::minimum`, `v32::clamp`, `v32::absolute` for int and float),
RAII scope guards for the console's "selected" texture/gamepad/sound/
channel state (`TextureScope`, `GamepadScope`, `SoundScope`,
`ChannelScope`, `FrameScope`), and small handle classes (`v32::Channel`,
`v32::MemoryCard`, `v32::HeapBlock`, `v32::String`, `v32::Stopwatch`).
Every hardware access still goes through the real C function, and wrapper
names never reuse a C function's name (`minimum`, not `min`), so the raw C
API stays callable alongside them.

Use them with `-I` pointing at the directory that holds `v32/`
(`v32c++ -I path/to/v32c++ game.cpp`, then `#include <v32/video.hpp>`),
or install them (see [Installing](#installing)).

## Keyboard and mouse: v32io

Three headers give programs a real **keyboard** and **mouse**, split the
same way as the [v32io](https://github.com/wedge1020/v32io) Vircon32 C
drivers they port, protocol-for-protocol:

| Header | Class | The C library's |
| ------ | ----- | --------------- |
| `v32/v32io.hpp` | `v32::IoDevice`: a gamepad's 11 controls read as data (the core) | `v32io.h` |
| `v32/keyboard.hpp` | `v32::Keyboard`: key events, typed characters with shift and caps lock, held keys | `keyboard.h` |
| `v32/mouse.hpp` | `v32::Mouse`: a bounded pointer, movement, three buttons | `mouse.h` |

A program includes the driver(s) it needs; each includes the core.

```cpp
#include <v32/keyboard.hpp>
#include <v32/mouse.hpp>

v32::Keyboard keyboard( v32::SecondGamepadPort );   // "Gamepad 2"
v32::Mouse    mouse( v32::ThirdGamepadPort );       // "Gamepad 3"
...
keyboard.probe();  mouse.probe();                   // once EVERY frame
for( int key = keyboard.read(); key > 0; key = keyboard.read() ) ...
if( mouse.pressed( v32::MouseLeft ) ) click( mouse.x(), mouse.y() );
```

> **⚠ This REQUIRES a v32io device.** The console has no keyboard or
> mouse port: a v32io device is a gamepad whose controls carry keyboard
> or mouse data, and something outside the console must produce it —
> **either** the **v32io hardware adaptor** (a USB keyboard or mouse in
> the v32io-pico board, which works with any Vircon32 emulator), **or** a
> **modified Vircon32 DesktopEmulator** with the `v32kbd`/`v32mouse`
> gamepad devices: the fork at
> <https://github.com/wedge1020/ComputerSoftware> (**`v32io` branch**), or
> the stock emulator with the v32io repository's patches applied. The
> stock emulator's own *Keyboard* device is **not** a v32io keyboard.

Full documentation — setup, the API, the protocols, porting from the C
library, testing without a device — is in
**[`docs/V32IO.md`](docs/V32IO.md)**. Four ready-to-build examples
(a typewriter, a paint program, a port monitor for checking a setup, and
a notepad using both devices) are in
[`demos/cxx/v32io`](demos/cxx/v32io/README.md).

## Known limitations

Checked against the current transpiler and the real Vircon32 compiler
(October 2026), not carried over from older notes.

**Not supported** (a clear error unless noted):

- `static` class members.
- `enum class`, `constexpr`, lambdas, `using namespace`, `mutable`,
  conversion operators (`operator int()`), nested classes and
  class-scope enums, stacked declarators like `T *&`.
- A two-dimensional array parameter (`void f(int g[4][4])`).
- Templates, exceptions, RTTI and multiple inheritance: never (see below).

**Supported with caveats:**

- **Array `new`** (`new T[n]`) allocates but does not run constructors
  per element; `delete[]` runs no destructors, and `delete[]` through a
  base pointer to a derived array is undefined. Prefer `std::vector` or a
  stack array, which are constructed and destroyed properly.
- **`goto`** works, but does not run destructors for class-typed locals
  it jumps out of; keep it to scopes without them. A `goto` to a missing
  label is caught by the Vircon32 compiler, not by v32c++.
- **`const` is not enforced** — violations are left to the C compiler.
- **Classes in a namespace keep their bare C name** (`v32::String` is
  `String` in the generated C), so a program can't have its own class
  with the same name as one in a header it includes (`String`, `Channel`,
  `Keyboard`, `Mouse`, ...). Free functions are namespaced properly.
- **Overloads and pass-through values**: v32c++ can't see the type of a C
  API call's result. An overloaded call still resolves when its other
  arguments settle it (`v32::minimum(rand(), 10)`); when none do
  (`v32::absolute(rand())`) it is an error — store the value in a typed
  local first.
- **`native` types** can be pointed to but not created; the storage must
  come from the C side.
- **Unions** larger than one word can't be passed or returned by value
  (v32c++ doesn't warn; the Vircon32 compiler reports it).
- **Multi-dimensional arrays of objects** are constructed but not
  destroyed at scope exit.
- **Cart hints**: two `#texture` (or `#sound`) hints with the same name
  are not reported — the generated C defines the name twice and the
  later one silently wins.
- An `.h` include that can't be found anywhere is passed through each
  time it appears (the C compiler then reports it missing).

`docs/DESIGN_NOTES.md` has the full development history, including the
gaps that have been closed and how.

## Deliberately out of scope

To keep the project finishable, these are permanent design boundaries,
not "not yet":

- Templates (the containers above are a transpiler-level substitute)
- Exceptions and RTTI
- Multiple inheritance
- The full STL

## Requirements

- **flex**
- **bison**, version 3.x. (On macOS the system `/usr/bin/bison` is GNU
  bison 2.3, frozen for licensing reasons: `brew install bison` and put
  it first on your `PATH`.)
- A C compiler (gcc or clang) and `make`.

Built and tested on Linux and macOS. For Windows (MinGW / MSYS2), the few
POSIX calls the source needs (`realpath`, `getline`, `strndup`,
`tmpfile`, the `PATH` separator) go through `src/compat.c`, which has a
Windows branch for each.

To build cartridges you also need the Vircon32 DevTools (`compile`,
`assemble`, `packrom`, plus `png2vircon`/`wav2vircon` for assets) on your
`PATH`.

## Building

```sh
git clone https://github.com/wedge1020/v32cxx v32c++
cd v32c++
make            # builds bin/v32c++
make test       # transpiles every sample in tests/ into out/
```

`make test` shows that every transpile succeeded (or, for the
deliberately invalid samples, failed cleanly).

### With CMake

For those who prefer it, `CMakeLists.txt` builds the same transpiler
from the same sources (the Makefile stays the primary build, and
`make realcheck` is still the real-toolchain check):

```sh
cmake -S . -B build              # Release by default
cmake --build build              # build/v32c++
ctest --test-dir build           # transpile every sample (read from the Makefile's test list)
sudo cmake --install build       # see "Installing"
```

flex and bison are used when found (`-DV32CXX_REGENERATE_PARSER=OFF`
skips them); otherwise the already-generated `src/parser.c`,
`inc/parser.h` and `src/lexer.c` are compiled as they are, so a plain
build needs only a C compiler and CMake 3.10+. On **Windows**, build with
MinGW-w64 (for example from an MSYS2 MINGW64 shell, `cmake -S . -B build
-G "MinGW Makefiles"`); MSVC isn't supported (the sources use
`getopt_long` and `<unistd.h>`). `cpack --config build/CPackConfig.cmake`
makes a `.tar.gz` (plus `.deb` / `.rpm` where `dpkg-deb` / `rpmbuild` are
available), or a `.zip` on Windows.

## Checking with the real toolchain

`make realcheck` goes further: it compiles, assembles and packs every
generated program with the **official** Vircon32 tools, then boots every
self-checking sample on a headless console with the standard BIOS and a
fresh memory card. Build those tools once (about a minute; needs `git`
and a C++17 `g++`, no SDL):

```sh
tools/vircon32/build-tools.sh
make realcheck
```

A self-checking sample declares `int test_errors = -1;` and stores its
error count there before halting; the check requires 0 (see
`tests/94sample.cpp`). A sample with a `tests/NNsample.input` next to it
is played with that scripted gamepad input (`tests/119sample.cpp`, the
v32io test, uses it for keyboard and mouse traffic).

`build-tools.sh` also builds headless tools that are useful on their own,
all driven by the same input script format:

| Tool      | What it does                                                               |
| --------- | -------------------------------------------------------------------------- |
| `v32run`  | boots a cartridge and reports one RAM word (what `realcheck` uses)         |
| `v32prof` | per-frame CPU/GPU load and cycles per function, from a `-g` build          |
| `v32shot` | the screen at chosen frames, as PPM images                                 |
| `v32peek` | CPU state and RAM words at a chosen frame                                  |

`tools/vircon32/v32io-script.py` writes such scripts from keyboard and
mouse actions (`docs/V32IO.md`). `wav2vircon` is built too when a real
SDL2 is installed (`sdl2-config`), for profiling demos that pack sounds.

## Using it

```sh
v32c++ game.cpp                       # writes game.c and game.xml, silently
v32c++ -I path/to/v32c++ -o obj/game.c game.cpp
```

| Option | Meaning |
| ------ | ------- |
| `-o file.c` | output path (default: the input's name with `.c`) |
| `-c` | don't require a `main` (a library/module fragment) |
| `-I dir`, `--include=dir` | add a directory to the `.hpp`/`.cpp` search path (repeatable) |
| `-D name[=value]`, `-U name` | define / undefine a macro |
| `-v`, `-vv`, `-vvv` | progress; plus explanatory comments in the generated C; plus AST, semantic and lowering dumps and the lowering-notes log |
| `-x`, `--no-xml` | don't write the cart XML |
| `-b` | transpile a BIOS rather than a cartridge |
| `-g` | write a C-line/C++-line debug map, `<output>.c.debug` |
| `--target=vircon32`/`v32` | Vircon32 C (the default) |
| `--target=standard`/`std` | portable standard C instead |
| `--reject-bit-fields` | make a bit-field an error instead of a warning |
| `--no-inline-containers` | keep `std::array`/`std::vector` accessors as calls |
| `--version` | print the version and exit |

It is silent on success, like the Vircon32 compiler and v32lua; errors
and warnings go to standard error with the file and line, and the exit
status is non-zero on any error. The manual page (`man ./man/v32c++.1`)
describes every option in full.

**`-vv`** sprinkles comments into the generated `.c` where the C++-to-C
translation is least obvious — vtables, the explicit `this`, what
`new`/`delete` become, destructors called at scope exit, virtual calls —
so the output can be read on its own (try `tests/32sample.cpp`). It
never changes the code itself. **`-vvv`** adds the internal dumps and a
**lowering-notes log**: one line per place v32c++ rewrote code to work
around a Vircon32 quirk (a ternary hoisted into `if`/`else`, an implicit
`&function`, a const-discarding receiver cast) — try `tests/71sample.cpp`.

**`-g`** writes `<output>.c.debug`, a sparse table
(`c_path,c_line,cpp_path,cpp_line[,function_name]`) mapping generated C
lines back to the source, modeled on Vircon32's own debug maps.

**`-b`** transpiles a BIOS: the XML's `<rom>` gets `type="bios"`, and
v32c++ checks for exactly one `#texture`, at most one `#sound` and a
`void error_handler()`, reporting every violation at once.

## Cart packaging

A cart-packing XML is written next to the generated C (`game.xml`), so
`packrom` needs no hand-written file. Four hints in the source fill it
in, modeled on v32lua's:

```cpp
#title   "My Game"
#version 1.0
#texture Background "background.png"
#sound   jump_sfx   "jump.wav"
```

Each `#texture`/`#sound` name becomes a `#define` of its id (0, 1, 2… in
order, textures and sounds counted separately), usable anywhere an
integer is (`select_texture( Background )`), and fills the XML's
`<textures>`/`<sounds>` in that order with the extension swapped to
`.vtex`/`.vsnd`. `#title`/`#version` set the `<rom>` attributes
(otherwise "Vircon32 Program" / "1.0"). The XML's `<binary path>` is the
`-o` path with `.vbin`, so pack from the directory you transpiled in.

## Output dialects

`--target=vircon32` (the default) emits Vircon32 C, with every quirk of
that dialect handled: no `struct` keyword on type references, reversed
array and function-pointer declarators, `void main(void)`, `NULL`, no
ternary or comma operator, one-word parameters and returns, `&` on
function names, and the rest catalogued in
[`docs/VIRCON32_QUIRKS.md`](docs/VIRCON32_QUIRKS.md).
`--target=standard` emits portable C instead (no XML or debug map),
for using v32c++ as an ordinary C++-to-C translator or checking output
with `gcc`; `main` keeps its declared return type there.

## Demos

| Demo | Language | What it is |
| ---- | -------- | ---------- |
| [`demos/cxx/tempest_32k`](demos/cxx/tempest_32k/README.md) | C++ | a TEMPEST-style tube shooter, multi-file |
| [`demos/cxx/space_invaders`](demos/cxx/space_invaders/README.md) | C++ | "SPACE INVADERS++", with power-ups, multi-file |
| `demos/cxx/star_raiders` | C++ | a Star Raiders-style space game |
| [`demos/cxx/v32io`](demos/cxx/v32io/README.md) | C++ | keyboard and mouse examples (need a v32io device) |
| [`demos/c/rogue`](demos/c/rogue/README.md) | C | Unix Rogue 5.4.4, transpiled from its sources |
| [`demos/c/spyvsspy`](demos/c/spyvsspy/README.md) | C | a Spy vs Spy game in plain C, and a C coverage probe |

`make -C demos` builds every demo that has a Makefile (each needs
`v32c++` and the DevTools on the `PATH`; the C++ games use `v32opt` too
when it is installed).

## Documentation

| Document | What it covers |
| -------- | -------------- |
| [`man/v32c++.1`](man/v32c++.1) | the manual page: every option, the language, the preprocessor, packaging |
| [`docs/V32IO.md`](docs/V32IO.md) | keyboard and mouse support (`v32/keyboard.hpp`, `v32/mouse.hpp`) |
| [`docs/C_INPUT.md`](docs/C_INPUT.md) | transpiling C, and the `libc/` C library |
| [`docs/STRING.md`](docs/STRING.md) | the built-in `std::string` |
| [`docs/GENERICS.md`](docs/GENERICS.md) | `std::array` and `std::vector` |
| [`docs/NATIVE_PASSTHROUGH.md`](docs/NATIVE_PASSTHROUGH.md) | `native` types for C-header structs |
| [`docs/INLINE_ASSEMBLY_SUPPORT.md`](docs/INLINE_ASSEMBLY_SUPPORT.md) | `asm` statements |
| [`docs/VIRCON32_QUIRKS.md`](docs/VIRCON32_QUIRKS.md) | every way Vircon32 C differs from standard C, and how each is handled |
| [`docs/DESIGN_NOTES.md`](docs/DESIGN_NOTES.md) | internals and the full development history |
| [`docs/TRANSPILER_REPORT.md`](docs/TRANSPILER_REPORT.md) | the gap report from the first Space Invaders port, with current status |

## Project layout

```
src/            implementation (.c, plus the flex/bison sources)
  lexer.l       flex scanner
  parser.y      bison GLR grammar
  prescan.c     the C++-side preprocessor: include resolution, #define,
                #if, line markers -- runs before the lexer
  macro.c       macro table, expander and #if evaluator for prescan.c
  ast.c         the AST built while parsing
  symtab.c      scoped symbol table (typedef/class/native names, and the
                lexer's qualified-name handling)
  sema.c        semantic analysis: class layouts, out-of-line definition
                matching, name mangling, access control, overload
                resolution, native-type checks
  lower.c       lowering: struct layout, this-injection, calls and
                virtual dispatch, references, new/delete, vtables,
                constructors/destructors, by-value structs, Vircon32
                quirk rewrites (ternary, comma, &function, casts)
  cmode.c       C input (.c files): tags, prototypes, main's parameters,
                variadic calls (docs/C_INPUT.md)
  codegen.c     C code generator (Vircon32 or --target=standard)
  generic.c     std::array<T, N> / std::vector<T> (docs/GENERICS.md)
  stdstring.c   the built-in <string> text (GENERATED from lib/string by
                tools/embed-header.sh; docs/STRING.md)
  cartxml.c     cart-packing XML generation
  debugmap.c    the -g debug map
  pathutil.c    filename-extension helper
  compat.c      the POSIX calls that differ on Windows
  main.c        command line (see "Using it")
inc/            headers for the above, plus:
  driver.h      state shared between the lexer and parser
  v32cxx.h      project identity: VERSION (the single source), AUTHOR, URL
  config.h      build-time configuration: include search paths, their
                environment variables, limits
lib/string      the source of the built-in <string> header
libc/           for C input: a small C library, an 80x24 text terminal on
                the BIOS font, curses, memory-card files
v32/            C++ headers for the Vircon32 C API, plus v32io.hpp,
                keyboard.hpp and mouse.hpp (keyboard and mouse)
c_api/          copies of the Vircon32 SDK's own C headers, for reference
tests/          NNsample.cpp / .c inputs run by `make test` (some
                deliberately invalid); self-checking ones run by
                `make realcheck`; 119sample.{actions,input} is scripted
                v32io input
demos/          example programs, C (demos/c) and C++ (demos/cxx)
docs/           the documents listed above
man/            v32c++.1, the manual page (`man ./man/v32c++.1`)
CMakeLists.txt  the CMake build, install and packaging (see "With CMake")
cmake/          uninstall.cmake.in, for CMake's `uninstall` target
tools/
  embed-header.sh   regenerates src/stdstring.c from lib/string
  vircon32/         build-tools.sh (the real Vircon32 toolchain and the
                    headless tools), check.sh (`make realcheck`), the
                    headless tools' sources, and v32io-script.py
```

## Installing

A system-wide install puts everything where a Vircon32 setup expects it,
next to the DevTools:

| | Linux / macOS | Windows (CMake) |
| --- | --- | --- |
| transpiler | `/usr/local/bin/v32c++` | `C:\Program Files\Vircon32\v32c++\v32c++.exe` |
| man page | `/usr/local/share/man/man1/v32c++.1` | `...\v32c++\doc\v32c++.1` |
| C++ headers (`v32/`) | `/usr/local/Vircon32/v32c++/include/v32` | `...\v32c++\include\v32` |
| C library (`libc/`) | `/usr/local/Vircon32/v32c++/libc` | `...\v32c++\libc` |
| documentation | `/usr/local/share/doc/v32c++` (CMake) | `...\v32c++\doc` |

Either build does it:

```sh
sudo make sysinstall                          # Linux / macOS
sudo make sysuninstall

sudo cmake --install build                    # any system (prefix: /usr/local,
sudo cmake --build build --target uninstall   #   or C:/Program Files/Vircon32)
```

The header directory is compiled into the transpiler, so after
installing, `#include <v32/video.hpp>` (or `<v32/keyboard.hpp>`, ...)
works from any directory without `-I`. With CMake, pick a different
location at configure time (`cmake -S . -B build
-DCMAKE_INSTALL_PREFIX=/opt/vircon32`) and the transpiler will look
there. For C input, add `-I` with the installed `libc` directory. On
Windows, add `C:\Program Files\Vircon32\v32c++` to your `PATH`, as for
the DevTools.

`make install` / `make uninstall` instead copy just the binary to
`~/bin`. Installation defaults for the Makefile build (the header
directory, the environment variable names, a few limits) live in
[`inc/config.h`](inc/config.h); change one there and rebuild, or
override it when building:

```sh
make CFLAGS="-Wall -Wextra -g -Iinc -MMD -MP -DV32CXX_INCLUDE_PATH='\"/opt/v32c++/include\"'"
```

## Versioning

The version is `YYYYMMDD-status` (`-dev` or `-release`), the same scheme
as v32lua, and lives in one place: `VERSION` in
[`inc/v32cxx.h`](inc/v32cxx.h), which `v32c++ --version` prints. To
change it, edit it there and run `make version`, which stamps it (with
the current month) into the man page header.

## Feedback

This is built in the open as a teaching project and shared with the
Vircon32 community — if something breaks, transpiles wrong, or you'd
like a particular C++ feature prioritized, please open an issue at
<https://github.com/wedge1020/v32cxx>.
