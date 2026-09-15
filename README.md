# v32c++

**A C++-subset-to-C transpiler, targeting the [Vircon32](https://www.vircon32.com) fantasy console's C compiler.**

`v32c++` lets you write games and programs for Vircon32 in a subset of
C++ — classes, inheritance, constructors/destructors, virtual functions
and dynamic dispatch, operator overloading, arrays — and have it
translated into plain C that the Vircon32 toolchain already knows how to
build. The approach is the same one Bjarne Stroustrup's original
`cfront` used in the 1980s: rather than compiling C++ straight to
machine code, translate it to C first and let an existing, trusted C
compiler do the rest.

> **Status: functional, still growing.** The full pipeline — parsing,
> semantic analysis, lowering, and code generation — works end to end,
> confirmed against the real Vircon32 C compiler across dozens of test
> programs (classes, inheritance, virtual dispatch, constructors and
> destructors, `new`/`delete`, and arrays all compile and run). It's
> still genuinely early, though: some pieces (notably `break`/`continue`,
> and a real preprocessor) don't exist yet, and a few others are
> deliberately partial for now — see [Current status](#current-status)
> for the honest, detailed picture, and
> [`docs/DESIGN_NOTES.md`](docs/DESIGN_NOTES.md) for the full
> round-by-round story of how it got here, including the real bugs
> found and fixed along the way.

## Why this exists

Two audiences, one project:

1. **A teaching tool.** This started as course material for a class that
   teaches game programming on Vircon32, starting from plain C and later
   moving into C++ and object-oriented design. Being able to show
   students *how* a C++ construct like a virtual function or a
   constructor actually gets parsed, resolved, and lowered into plain C
   is the point, as much as having a working tool.
2. **A community tool.** The wider Vircon32 homebrew community writes
   games in C today; the goal is to let people opt into a small, well-
   understood subset of C++ instead, without pulling in the weight of
   templates, exceptions, or a full STL.

This is **not** an attempt to support all of C++ — see
[What's deliberately out of scope](#whats-deliberately-out-of-scope).

## Current status

**Parsing and semantic analysis** cover namespaces, classes with single
inheritance and access sections, constructors and destructors (in-class
or out-of-line), `virtual` functions with correctly-recognized overrides,
function/operator overloading with call-site resolution, qualified
names, pointers and references, arrays, and the usual statement/
expression language. Access control is enforced (including through
inheritance); overload resolution uses argument count and, when needed
to disambiguate, argument type, never guessing when it can't confidently
resolve something.

**Lowering** — transforming the semantically-checked program into
something code generation can work from directly — runs through nine
phases: struct field layout (with correct vtable-pointer placement
across a hierarchy), `this`-injection, call finalization (including
virtual dispatch through the vtable, and natural operator syntax like
`a + b` resolved against declared overloads), reference-to-pointer
rewriting, `new`/`delete` lowering, vtable pointer initialization,
constructor invocation (both for stack-allocated locals and via `new`),
and destructor invocation (both via `delete` and automatically at scope
exit, including from an early `return`).

**Code generation** emits real, working Vircon32 C: struct and vtable
struct definitions, method/function bodies, constructors that actually
allocate and initialize, destructors that actually run, and virtual
dispatch that actually works on a real, constructed object — all
confirmed against the real Vircon32 compiler, not just reasoned through.

**Arrays** are supported end to end: standard C declarator syntax
(`int scores[8];`) *and*, as an alternate accepted spelling, Vircon32's
own native declarator style (`int [8] scores;`) — useful if you're
already fluent in Vircon32 C or transitioning from it and don't want to
learn a second convention just for this tool. Initializer lists
(`int scores[4] = {1, 2, 3, 4};`), array function parameters (decaying
to a pointer, matching ordinary C semantics), and heap-allocated arrays
(`new int[n]` / `delete[]`) are all supported too. Array-`new` is
allocation-only for now — no per-element construction happens yet, since
there's no per-element analogue of stack-array construction or any
loop-emission machinery in the code generator yet.

**A preprocessor pass-through exists, but there's still no real
preprocessor.** A `#include`, `#define`, or other `#`-line is no longer
silently discarded — it's captured and re-emitted verbatim at the top of
the generated file, so a `#include "video.h"` you actually wrote
survives the round trip. Nothing is interpreted, though: no macro
expansion, no `#include` resolution, no `#ifdef` evaluation. A real
preprocessor is still future work.

**Cart-packing XML is generated automatically**, alongside the
generated `.c`, matching v32lua's own output — one less manual,
repetitive step in the build process. Two cart hints are recognized
directly in C++ source now too: `#texture NAME "file.png"` and
`#sound NAME "file.wav"` (any case for `NAME`), modeled on v32lua's own
`--#texture`/`--#sound` hints — each becomes a `#define` mapping to that
resource's id (0, 1, 2… in declaration order, textures and sounds
counted separately), and populates the generated XML's
`<textures>`/`<sounds>` in that same order. A program with no hints at
all still gets an XML, with the previous empty `<textures />`/
`<sounds />`. See [Trying it out](#trying-it-out) for the `-x` opt-out.
Not yet supported: `#title`/`#version` hints (the XML's title/version
stay fixed at "Vircon32 Program"/"1.0" for now), and no check yet for
two hints reusing the same `NAME` (caught by the C compiler itself, as
a redefined macro, rather than by `v32c++`).

**What doesn't exist yet, worth knowing before you rely on it:**

- **`break`/`continue`** aren't in the grammar at all yet — planned, not
  forgotten.
- **Virtual destructor dispatch.** `delete basePtr;` through an
  ancestor-typed pointer calls the ancestor's destructor, not the
  derived one, regardless of whether it was declared `virtual`.
- **Base-class constructor delegation** (C++ member-initializer lists,
  `Derived::Derived() : Base(args) {}`) isn't supported — a derived
  class's constructor has to set inherited fields directly.
- **Standard-C output mode.** Every Vircon32-specific output quirk this
  project works around is tracked in
  [`docs/VIRCON32_QUIRKS.md`](docs/VIRCON32_QUIRKS.md) toward an eventual
  flag that targets an ordinary, portable C compiler instead — not
  implemented yet.

This is genuinely still growing — expect rough edges, and expect this
README to need updating again as things change.

## What's deliberately out of scope

To keep this project finishable, a few things are explicitly *not*
planned, ever, rather than "not yet":

- Templates
- Exceptions and RTTI
- Multiple inheritance
- The full STL — a small, purpose-built container library instead, once
  there's enough of the language surface to make it useful

## Requirements

- **flex**
- **bison**, version 3.x recommended. (If you're on macOS: the
  system `/usr/bin/bison` is a very old GNU bison 2.3, frozen there for
  licensing reasons. `brew install bison` and make sure it comes first on
  your `PATH`.)
- A C compiler (gcc or clang)

## Building

```sh
git clone <this repo>
cd v32c++
make
```

This builds `bin/v32c++`. To build and run it against the bundled sample
programs in one step:

```sh
make test
```

## Trying it out

```sh
./bin/v32c++ path/to/yourfile.cpp
```

Silent by default, matching the real Vircon32 C compiler and v32lua —
this writes `path/to/yourfile.c` (input filename, extension swapped for
`.c`) and produces no output at all on success. Pass `-o` to choose a
different output path instead. Verbosity is opt-in and stackable:
`-v` prints progress as each stage runs (lexer/parser, semantic
analyzer, lowering, code generator); `-vv` additionally prints the full
AST, semantic-analysis, and lowering dumps, useful for following along
with what the tool understood and how it transformed your code; `-vvv`
is reserved for even more detail in a future round. Use `-c` if your
input is a library/module fragment without its own `main`.

Alongside the generated `.c`, a Vircon32 cart-packing XML file is
written by default too (`path/to/yourfile.xml`) — the manual,
repetitive step of hand-writing that file for every build is
automated now, matching v32lua's own `emit_cart_xml`. Two cart hints
are recognized directly in C++ source:

```cpp
#texture Background "background.png"
#sound   jump_sfx    "jump.wav"
```

Each `NAME` (any case) becomes a `#define` mapping to its resource's
id — 0, 1, 2… in declaration order, textures and sounds counted
separately — usable anywhere an integer constant would be:
`select_texture(Background)`. That same order determines each
resource's position in the generated XML too, extensions swapped to
`.vtex`/`.vsnd`. No hints at all still gets an XML, with empty
`<textures />`/`<sounds />`. Modeled on v32lua's own
`--#texture`/`--#sound` hints — not yet supported: `#title`/`#version`
hints (title/version stay fixed at "Vircon32 Program"/"1.0" for now),
and no check yet for two hints reusing the same `NAME` (the C compiler
itself catches that, as a redefined macro, not `v32c++`). Pass `-x`
(or `--no-xml`) to skip XML generation entirely.

A large set of example inputs lives in `tests/`, including a couple that
are *deliberately* invalid (an undeclared type, an out-of-line
definition with no matching prototype) to show that errors are reported
cleanly rather than crashing the tool, and several real, hand-written
programs (not artificial unit tests) that found real bugs during
development — see `docs/DESIGN_NOTES.md` for that history.

## Project layout

```
src/
  lexer.l       flex scanner
  parser.y      bison GLR grammar
  ast.h/.c      the AST built while parsing
  symtab.h/.c   scoped symbol table (backs typedef/class-name lookup and
                the lexer's qualified-name handling)
  sema.h/.c     semantic-analysis pass: class layouts, out-of-line
                definition matching, name mangling, access control,
                call-site overload resolution
  lower.h/.c    lowering passes: struct layout, this-injection, call
                finalization, reference-to-pointer, new/delete,
                vtable init, constructor/destructor invocation
  codegen.h/.c  Vircon32 C code generator
  cartxml.h/.c  Vircon32 cart-packing XML generation
  pathutil.h/.c shared filename-extension-swapping helper
  driver.h      shared state between the lexer and parser
  v32cxx.h      project identity (VERSION/AUTHOR/URL) and build-time
                configuration constants
  main.c        CLI entry point (-o, -c, -v, -x, --version)
tests/          example .cpp inputs, including intentionally-invalid
                ones and several real, hand-written programs
docs/           design notes, implementation deep-dives, and the
                Vircon32-specific output-quirk catalog
man/            v32c++.1 -- a Unix section 1 manual page; view it
                directly with `man ./man/v32c++.1`, or `make install`
                to put v32c++ itself on your PATH (see below)
```

## Installing

```sh
make install
```

Copies `bin/v32c++` to `~/bin/`. Make sure `~/bin` is on your `PATH` to
run it as just `v32c++` from anywhere. `make uninstall` removes it
again.

## Feedback

This is being built in the open as a teaching project and shared with the
Vircon32 community for early feedback — if you try it and something
breaks, parses wrong, or you want to see a particular C++ feature
prioritized, that feedback is genuinely useful at this stage.
