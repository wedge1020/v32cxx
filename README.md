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
> destructors, `new`/`delete`, arrays, and `break`/`continue` all
> compile and run). It's still genuinely early, though: a real
> preprocessor doesn't exist yet (only pass-through), and a few other
> pieces are deliberately partial for now — see
> [Current status](#current-status) for the honest, detailed picture,
> and [`docs/DESIGN_NOTES.md`](docs/DESIGN_NOTES.md) for the full
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
names, pointers and references, arrays, `break`/`continue` (rejected
outside a loop, not just accepted blindly), and the usual statement/
expression language. Access control is enforced (including through
inheritance); overload resolution uses argument count and, when needed
to disambiguate, argument type, never guessing when it can't confidently
resolve something.

**Lowering** — transforming the semantically-checked program into
something code generation can work from directly — runs through eleven
phases: struct field layout (with correct vtable-pointer placement
across a hierarchy), `this`-injection, call finalization (including
virtual dispatch through the vtable, and natural operator syntax like
`a + b` resolved against declared overloads), reference-to-pointer
rewriting, `new`/`delete` lowering, vtable pointer initialization,
member-field initializer assignments (in declaration order — see
below), base-class constructor delegation (an explicit `: Base(args)`
— see below), constructor invocation (both for stack-allocated locals
and via `new`), and destructor invocation (via `delete`, automatically
at scope exit including from an early `return`, and correctly scoped
at a `break`/`continue` too — only what's actually live inside the
loop gets destroyed, not everything above it).

**Base-class constructor delegation** — `Derived::Derived(args) :
Base(base_args) { ... }` — resolves the delegated call against the
base's own constructor overloads (same resolution machinery as any
other call) and inserts it as the very first statement in the derived
constructor, ahead of even the vtable-pointer initialization above, so
a base subobject is always fully constructed before anything else runs
— matching real C++'s own timing. This is the fix for a real gap, not
just a style preference: if a base class's own fields are `private`
(the properly encapsulated way to write one), a derived class's
constructor previously had no legal way to initialize them at all.

**Member-field initializers** — `: x(val)` for a primitive-typed field
— assign the field right after any base-class delegation and before
the constructor's own body, in the *declaration* order the field
appears in the class (not the order it's written in the initializer
list — a well-known real-C++ rule: `Pair(int a, int b) : x(a), y(b) {}`
with `y` declared before `x` initializes `y` first, using `x`'s
still-uninitialized value if `y`'s own initializer refers to it).
Reaches the argument expressions themselves too, so `: y(x)` (another
member referenced bare) correctly becomes `this->y = this->x` in the
generated C.

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
repetitive step in the build process. Four cart hints are recognized
directly in C++ source: `#texture NAME "file.png"` and
`#sound NAME "file.wav"` (any case for `NAME`), modeled on v32lua's own
`--#texture`/`--#sound` hints — each becomes a `#define` mapping to that
resource's id (0, 1, 2… in declaration order, textures and sounds
counted separately), and populates the generated XML's
`<textures>`/`<sounds>` in that same order. `#title "..."` and
`#version 1.0` override the XML's `<rom>` title/version attributes
(fixed at "Vircon32 Program"/"1.0" otherwise, matching v32lua's own
defaults) — last one seen wins if either appears more than once. A
program with no hints at all still gets an XML, with the previous empty
`<textures />`/`<sounds />`. See [Trying it out](#trying-it-out) for the
`-x` opt-out. Not yet supported: a check for two hints reusing the same
`NAME` (caught by the C compiler itself, as a redefined macro, rather
than by `v32c++`).

**`-b` transpiles a Vircon32 BIOS** rather than an ordinary cartridge —
the generated XML's `<rom>` gets `type="bios"`, and three constraints
are enforced (checked only under `-b`): exactly one `#texture` hint, at
most one `#sound` hint, and a `void error_handler()` function alongside
`main`. Every violation found is reported at once, not just the first.
`v32c++` itself only enforces these three things — everything else that
makes a valid, bootable BIOS is the C compiler's and assembler's own
concern downstream.

**`-g` writes a C-line/C++-line debug map** (`<output>.c.debug`) alongside
the generated C — a sparse table, modeled on a real Vircon32
C-to-assembly debug map, recording only where the mapping actually
changes rather than one row per output line, with an extra column
naming the generated C function wherever one begins.

**`-vv` sprinkles explanatory comments into the generated `.c` itself**
— vtable pointers/structs/instances, the explicit `this` parameter every
method gets, the malloc-based allocator/deleter functions `new`/`delete`
become, virtual destructor dispatch, and the automatic
constructor/destructor/virtual-dispatch calls lowering inserted with no
direct textual counterpart in the original C++. Intended to make the
generated C worth reading through on its own — genuine learning value,
not just a build artifact — for a course context where seeing *why*
the C looks the way it does is often the point. Pure commentary: never
changes what code is emitted, only whether a comment explaining it is
emitted alongside it.

**What doesn't exist yet, worth knowing before you rely on it:**

- **Implicit base-class construction.** A derived class with a base
  class but no explicit `: Base(args)` gets no base-constructor call
  inserted at all — unlike real C++, which would call the base's own
  default constructor automatically. Explicit delegation (below) is
  supported; only the implicit case is still a gap.
- **Class-typed member-field initializers** (`: thing(args)` where
  `thing`'s own type is a class, not a primitive/pointer/reference) are
  accepted syntactically but not yet acted on — reported as a clear
  "not yet supported" error. Invoking a member's own constructor is
  real, separate complexity this project doesn't support anywhere yet.
  A primitive-typed member-field initializer (`: x(val)` for a plain
  `int`/`float`/etc., or a pointer/reference) IS supported, with real
  C++'s own declaration-order semantics (members initialize in the
  order they're *declared*, not the order they're *written* in the
  list).
- **Standard-C output mode.** Every Vircon32-specific output quirk this
  project works around is tracked in
  [`docs/VIRCON32_QUIRKS.md`](docs/VIRCON32_QUIRKS.md) toward an eventual
  flag that targets an ordinary, portable C compiler instead — not
  implemented yet.
- **A real preprocessor.** Only pass-through exists (see above) — no
  macro expansion, `#include` resolution, or `#ifdef` evaluation.

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
analyzer, lowering, code generator); `-vv` additionally sprinkles
explanatory comments directly into the generated `.c` itself (see
below); `-vvv` additionally prints the full AST, semantic-analysis, and
lowering dumps, useful for following along with what the tool
understood and how it transformed your code. Use `-c` if your input is
a library/module fragment without its own `main`.

With `-vv` (or higher — `-vvv` includes everything `-vv` does), the
generated C explains itself at the points where the C++-to-C
transformation is least obvious — a vtable's own struct and instance,
the explicit `this` parameter every method gets, what `new`/`delete`
actually become, a destructor invoked automatically at scope exit, a
virtual call dispatched through the vtable. Worth reading through on
its own, not just a build artifact — the C generated for
`tests/sample32.cpp` (vtables, a virtual destructor, `new`/`delete`) is
a good one to try this on first. Pure commentary: never changes what
code is emitted, only whether a comment explaining it comes with it.

Alongside the generated `.c`, a Vircon32 cart-packing XML file is
written by default too (`path/to/yourfile.xml`) — the manual,
repetitive step of hand-writing that file for every build is
automated now, matching v32lua's own `emit_cart_xml`. Four cart hints
are recognized directly in C++ source:

```cpp
#title   "My Game"
#version 1.0
#texture Background "background.png"
#sound   jump_sfx    "jump.wav"
```

Each `NAME` (any case) becomes a `#define` mapping to its resource's
id — 0, 1, 2… in declaration order, textures and sounds counted
separately — usable anywhere an integer constant would be:
`select_texture(Background)`. That same order determines each
resource's position in the generated XML too, extensions swapped to
`.vtex`/`.vsnd`. `#title`/`#version` override the XML's `<rom>`
title/version (fixed at "Vircon32 Program"/"1.0" otherwise; last one
seen wins if given more than once) — `title` is quoted, `version` is a
bare token, matching v32lua's own hint. No hints at all still gets an
XML, with empty `<textures />`/`<sounds />`. Modeled throughout on
v32lua's own `--#texture`/`--#sound`/`--#title`/`--#version` hints — not
yet supported: a check for two hints reusing the same `NAME` (the C
compiler itself catches that, as a redefined macro, not `v32c++`). Pass
`-x` (or `--no-xml`) to skip XML generation entirely.

`-b` transpiles a Vircon32 **BIOS** instead of an ordinary cartridge —
the XML's `<rom>` gets `type="bios"`, and three constraints get checked
(only under `-b`): exactly one `#texture`, at most one `#sound`, and a
`void error_handler()` function alongside `main`. Every violation is
reported together, not one at a time. Everything else a valid, bootable
BIOS needs is the C compiler's and assembler's own job downstream —
`v32c++` only enforces these three things.

`-g` writes `<output>.c.debug` alongside the generated C — a sparse
table mapping generated-C lines back to the C++ source lines
responsible for them, modeled on a real Vircon32 C-to-assembly debug
map:

```
c_path,c_line,cpp_path,cpp_line[,function_name]
```

`function_name` appears only where a function's own C definition
begins, naming the generated C side (its mangled name, where this
project mangles one). `cpp_path` is always the original `.cpp` given on
the command line — never a preprocessed intermediate, even if a
separate preprocessor tool exists someday.

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
  debugmap.h/.c C-line/C++-line debug map (-g) tracking and output
  driver.h      shared state between the lexer and parser
  v32cxx.h      project identity (VERSION/AUTHOR/URL) and build-time
                configuration constants
  main.c        CLI entry point (-o, -c, -v, -x, -b, -g, --version)
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
