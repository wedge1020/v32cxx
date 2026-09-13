# v32c++

**A C++-subset-to-C transpiler, targeting the [Vircon32](https://www.vircon32.com) fantasy console's C compiler.**

`v32c++` lets you write games and programs for Vircon32 in a subset of
C++ — classes, inheritance, namespaces, constructors/destructors,
operator and function overloading — and have it translated into plain C
that the Vircon32 toolchain already knows how to build. The approach is
the same one Bjarne Stroustrup's original `cfront` used in the 1980s:
rather than compiling C++ straight to machine code, translate it to C
first and let an existing, trusted C compiler do the rest.

> **Status: early work in progress.** The front end — lexer, parser, AST,
> and a first slice of semantic analysis — is up and running. **Code
> generation to Vircon32 C doesn't exist yet.** Right now, running
> `v32c++` on a `.cpp` file gets you a dump of what it parsed and
> understood, not a compilable `.c` file. See [Current status](#current-status)
> below for the honest, detailed picture, and
> [`docs/DESIGN_NOTES.md`](docs/DESIGN_NOTES.md) if you want the
> deep-dive on *why* things are built the way they are.

## Why this exists

Two audiences, one project:

1. **A teaching tool.** This started as course material for a class that
   teaches game programming on Vircon32, starting from plain C and later
   moving into C++ and object-oriented design. Being able to show
   students *how* a C++ construct like a virtual function or an
   out-of-line method definition actually gets parsed — and eventually,
   how it gets lowered to plain C — is the point, as much as having a
   working tool.
2. **A community tool.** The wider Vircon32 homebrew community writes
   games in C today; the goal is to let people opt into a small, well-
   understood subset of C++ instead, without pulling in the weight of
   templates, exceptions, or a full STL. A small, purpose-built
   `std::vector`-style container library is a likely eventual companion
   to this, once code generation exists to make it useful.

This is **not** an attempt to support all of C++ — see
[What's deliberately out of scope](#whats-deliberately-out-of-scope).

## Current status

What parses and is understood today:

- Namespaces (including reopening the same namespace across a file, e.g.
  for hardware-access namespaces like `v32::` or `ioports::`)
- Classes with single inheritance and `public`/`private`/`protected`
  sections
- Constructors and destructors, defined either inside the class body or
  out-of-line (`Player::Player(...) { ... }`)
- `virtual` function declarations, with overrides correctly recognized
  even when a derived class doesn't repeat the `virtual` keyword (real
  C++ semantics)
- Function and operator overloading, with parameter-type-aware name
  mangling so overloads don't collide (call-site resolution — knowing
  which overload a given call *expression* means — isn't implemented yet)
- Qualified names (`v32::Timer`), pointers and references (`Type*`,
  `Type&`)
- A conventional C-like statement/expression language: `if`/`else`,
  `while`, `for`, the usual operators, `new`/`delete`

A first slice of semantic analysis then runs over the parsed program and:

- Matches out-of-line method definitions back up to their in-class
  declaration — by name *and* parameter signature (typedef-transparent,
  so a method declared using a typedef and defined using its underlying
  type still matches correctly), so overloaded constructors/methods
  attach to the right one
- Computes each class's layout: its data members, its methods (each
  tagged with its actual access level — `public`/`private`/`protected`,
  correctly defaulting to private when a class body has no leading
  access-specifier), and its **vtable** — one slot per distinct virtual
  method in the hierarchy, with overrides correctly reusing their base's
  slot
- Assigns every function and method a mangled name that encodes its
  parameter types
- Reports errors (unknown types, mismatched out-of-line definitions)
  without crashing, so you see everything wrong in one run
- **Enforces access control**: walks every method/function body and flags
  illegal `private`/`protected` member access — including through
  inheritance (a derived class touching a base's private member, even
  implicitly) — using a best-effort read of each expression's type. It
  doesn't try to resolve everything (arithmetic results and free-function
  call results aren't type-checked at all yet), but what it does resolve,
  it checks correctly, and it never falsely flags what it can't resolve
- **Resolves call-site overloads**: for a call like `c.add(5, 10)`, picks
  which specific overload of `add` is actually meant, by argument count
  and (when more than one candidate shares that count) argument type —
  covering both method calls and free-function calls. No implicit
  conversions are modeled, and — same honesty as the point above — a call
  whose arguments it can't confidently type is left unresolved rather than
  guessed at, never silently assumed to be fine

Lowering — transforming what semantic analysis figured out into
something closer to what generated C needs — is now complete through six
phases: computing each class's flattened field layout (base class fields
folded in as a literal prefix, so single-inheritance polymorphism works
the same way it would in real C++, plus correct vtable-pointer placement
across a hierarchy); `this`-injection (a method's implicit receiver
becomes an explicit first parameter, and every implicit member reference
becomes explicit through it); call finalization (every call — including
natural operator syntax like `a + b`, resolved against declared operator
overloads for the first time anywhere in this project — becomes its
final, codegen-ready form: virtual calls dispatch through the vtable,
everything else becomes a direct call to the right mangled function);
reference-to-pointer rewriting; and a placeholder `new`/`delete` lowering
(explicitly not a real allocator yet — see `docs/DESIGN_NOTES.md` for
exactly what's simplified and why). Everything above is still a
transformed AST at this point, not emitted C syntax.

**What's still missing before this is a usable transpiler:** actual
Vircon32 C code generation — everything the lowering phases above decided
is now sitting in a fully-lowered AST, but nothing emits any of it into
real `.c` text yet. Templates
and exceptions are intentionally not planned at all (see below). This is
genuinely early — expect rough edges, and expect this README to need
updating often as things change.

**A note on the preprocessor:** there isn't one yet — `#include`/`#define`
lines are currently just discarded, not expanded (see
`docs/DESIGN_NOTES.md` for exactly what that breaks). The plan is a
small, purpose-built preprocessor for this project (`v32pp`), built once
it's actually needed rather than speculatively now; in the meantime,
piping source through a real preprocessor (GNU `cpp`) as a stopgap is a
reasonable option if you need `#include`/`#define` before `v32pp` exists.

## What's deliberately out of scope

To keep this project finishable, a few things are explicitly *not*
planned, ever, rather than "not yet":

- Templates
- Exceptions and RTTI
- Multiple inheritance
- The full STL — a small, purpose-built container library instead, once
  there's a code generator to make it useful

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
program in one step:

```sh
make test
```

## Trying it out

```sh
./bin/v32c++ path/to/yourfile.cpp
```

Right now this prints two things: a dump of the parsed AST, and a summary
from the semantic-analysis pass (class layouts, mangled names, any
errors found). That's diagnostic output for following along with what
the tool currently understands — not a `.c` file you can hand to the
Vircon32 compiler yet.

A handful of example inputs live in `tests/`, including a couple that are
*deliberately* invalid (an undeclared type, an out-of-line definition with
no matching prototype) to show that errors are reported cleanly rather
than crashing the tool.

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
  lower.h/.c    lowering passes, starting with class-to-struct field
                layout (more phases to come)
  driver.h      shared state between the lexer and parser
  main.c        CLI entry point
tests/          example .cpp inputs, including intentionally-invalid ones
docs/           design notes and implementation deep-dives
```

## Feedback

This is being built in the open as a teaching project and shared with the
Vircon32 community for early feedback — if you try it and something
breaks, parses wrong, or you want to see a particular C++ feature
prioritized, that feedback is genuinely useful at this stage.
