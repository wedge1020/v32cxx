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

**Parsing and semantic analysis** cover namespaces, classes and plain
C-style `struct`s alike — the only real difference (default member
access: private for `class`, public for `struct`) is handled correctly,
with every other class feature (vtables, constructors, inheritance,
access control) shared identically between the two — with single
inheritance and access sections, constructors and destructors (in-class
or out-of-line), `virtual` functions with correctly-recognized overrides,
function/operator overloading with call-site resolution, qualified
names, pointers and references, arrays, global (file-scope) variables,
`break`/`continue` (rejected outside a loop or `switch`, not just
accepted blindly — and correctly distinguished from each other: `break`
is valid inside a `switch` alone, `continue` is not), `switch`/`case`/
`default` with real C fall-through semantics, bitwise operators
(`& | ^ << >>` and their compound-assignment forms, at C's own correct
precedence — including the classic `a & b == c` gotcha, handled exactly
right, not approximated), C-style casts (`(int)x`, `(Base *)ptr`) and
C++-style casts (`static_cast`/`const_cast`/`reinterpret_cast` — all
three collapse to the same generated cast as the C-style form, which is
the semantically correct simplification once the target is C; see
`dynamic_cast` below), hex/octal/binary integer literals (`0x1F`,
`013`, `0b1010` — binary literals are a **C++14 addition**, not part of
every C++ standard, accepted here as a convenience regardless) and
integer/float literal suffixes (`42u`, `100L`, `3.9f`, accepted and
ignored — this project has exactly one integer type and one
floating-point type, so a suffix has nothing left to disambiguate),
the ternary operator (`cond ? a : b`, at C's own correct precedence —
between assignment and `||`, right-associative, so chaining nests the
way real C++ does), `do`/`while` (the body runs at least once,
unconditionally, before the first check — reuses the exact same
loop-depth and destructor-boundary handling as `while`/`for`, verified
directly rather than assumed), `enum` (top-level/namespace-level;
auto-incrementing or explicit values, resolved by the C compiler
downstream exactly as real C++ would, not computed by this project
itself), plain C-style `union`s (top-level/namespace-level; not routed
through the same machinery as `class`/`struct`, since real C++ itself
restricts what a union may contain far more than either), source-level
`sizeof` (both `sizeof(Type)` and `sizeof expr`/`sizeof(expr)`, with a
call inside the expression form correctly resolved, not skipped),
`goto`/labeled statements (a real, deliberately-flagged limitation
here: no validation that a label actually exists, and no destructor
invocation for a `goto` that jumps into or out of a scope holding a
live class-typed local — see the note below), and function pointers —
**both** Vircon32's own quirky declarator style (`ReturnType(ParamTypes)*
name;`) and standard C's (`ReturnType (*name)(ParamTypes);`) are
accepted as input, including arrays of either
(`ReturnType (*name[N])(ParamTypes);` and, confirmed against the real
compiler, `ReturnType(ParamTypes)* [N] name;`); output is always
Vircon32's own required form regardless of which one the source used,
the same dual-acceptance treatment array declarators already have. A
function-pointer `typedef` (both spellings) is supported too. A real,
previously-undiscovered gap was found and fixed while verifying this
end to end (actually compiling generated output with `gcc`, not just
transpiling it): a bare function NAME used as a plain value — the
overwhelmingly common way to initialize a function pointer at all
(`Callback cb = doubleIt;`) — was never rewritten to that function's
own mangled name the way a CALL's callee already is, so the generated
C referenced a symbol (`doubleIt`) that doesn't exist (only
`doubleIt__int` does); fixed in `lower.c`'s `finalize_calls_expr`,
deliberately conservative (only rewritten when exactly one free
function matches the name — an overloaded function used as a bare
value has no argument list here to disambiguate against, so an
ambiguous case is left untouched rather than guessed at). A second,
separate gap in the same area was found only by an actual Vircon32 C
compiler run (not `gcc`): unlike standard C, Vircon32 C does NOT
implicitly decay a bare function name to a function-pointer value —
`Callback cb = doubleIt__int;` is rejected outright ("types are not
compatible: cannot assign int(int) to int(int)*"), requiring an
explicit `&`. Fixed in `lower.c` by inserting that `&` automatically,
in exactly the two places a bare function name can end up as a
function-pointer value — a `VarDecl` initializer and a plain `=`
assignment (including into an array element, e.g.
`ops[0] = add;`) — checked against the target's declared/inferred
type (through any typedef chain) before the identifier is even
mangled, so a user-written `&doubleIt` is never double-wrapped and a
plain function-pointer-to-function-pointer variable copy
(`Callback cb2 = cb;`) is correctly left alone. The inserted `&` is
valid, idiomatic standard C too (`&funcname` and a bare `funcname`
are identical pointer values there), so this fix applies unconditionally in both targets rather than
being gated on `--target`. See `docs/DESIGN_NOTES.md` for the full
verification. Arrays can
be multi-dimensional too (`int grid[8][4];` or `int [8][4] grid;`,
dimensions nested outermost-first, matching real C exactly), with
chained subscripting (`grid[i][j]`) needing no new work at all since
it already composed through the existing, already-left-recursive
subscript grammar. `const` is accepted as a type prefix everywhere a
type can appear (`const int x`, `const int *p`, a `const` parameter or
return type) — resolving method calls and overloads correctly through
a const-qualified class type, not just parsing the keyword — and as a
trailing qualifier on member functions too (`int getValue() const {
... }`, both declared in-class and defined out-of-line), which
actually propagates: the generated `this` parameter for a const method
is `const ClassName *`, not a plain pointer with the qualifier
silently dropped. This project doesn't enforce const-correctness
itself anywhere, though; see the note below for the exact boundary. A
reference-typed parameter's own call sites correctly insert the
implicit address-of a C++ reference argument needs once it lowers to a
plain C pointer (`getArea(shape)` → `getArea__Shape_ref((&shape))`),
including through a dereferenced pointer argument (`getArea(*ptr)`).
The usual
statement/expression language is covered throughout. Access control
is enforced (including through inheritance); overload resolution uses
argument
count and, when needed to disambiguate, argument type, never guessing
when it can't confidently resolve something.

`dynamic_cast` is accepted too, but honestly, not silently: it parses
and transpiles exactly like the other casts, but since it doesn't
actually perform the RTTI-backed runtime check real `dynamic_cast`
promises (this project has never supported RTTI, by design), using it
produces a **warning**, not an error — printed to stderr, never
blocking the transpile. A second warning was added later, `--target=
vircon32`-only: the real Vircon32 C compiler only accepts parameters
and return values that are exactly one word (32 bits) — no by-value
struct, union, or array larger than that, a pointer must be used
instead (confirmed directly by Matthew, including that `char`/`short`/
`double`/etc are alias syntactic sugar over the same 4-byte word
underneath, making every supported primitive type uniformly one word).
A bare (non-pointer, non-reference) class or struct parameter or
return type larger than one word — its own field count, via
`StructLayout` — triggers this warning; running it against this
project's own existing test suite actually found two real violations
already in it (`tests/sample9.cpp`, `tests/sample15.cpp`, both passing
a two-field `Vector2D` by value throughout their own operator
overloads). Both have since been rewritten to return `Vector2D *`
instead (heap-allocated via `new`), once entry #12 below made that
possible, eliminating the warning entirely rather than leaving it
documented as a known limitation. See `docs/VIRCON32_QUIRKS.md`'s own
entry #11 for the full reasoning and stated scope gaps (an array- or
nested-struct-typed field can still under-count; unions aren't checked
at all).

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

**Implicit base-class construction** works too now — a derived
constructor that never writes `: Base(args)` at all still gets a call
to the base's own zero-argument constructor inserted automatically,
matching real C++. A base with no constructor of its own needs nothing
called (also matching real C++'s own implicitly-default-constructible
rule); a base with some constructor but none callable with zero
arguments is a genuine error (`'Base' has no default constructor...`),
not a silently uninitialized base subobject. This check caught two
real, pre-existing bugs in this project's own test suite the moment it
shipped — confirmed directly, not just reasoned through.

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

- **`goto` and destructors don't interact correctly.** `goto` and
  labeled statements are supported, but this project makes no attempt
  to invoke destructors for a class-typed local when a `goto` jumps out
  of (or into) the scope that local lives in — every other exit path
  (`break`, `continue`, `return`) is handled correctly by a dedicated
  pass that knows exactly what needs destroying at that point; `goto`
  has no equivalent. There's also no validation that a `goto`'s own
  label actually exists anywhere in the function. Keep `goto` to flat
  scopes with no destructible (class-typed) locals in play until this
  is addressed.
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
- **The original "basic, non-OOP C syntax" gap list is now complete.**
  Bitwise operators, `switch`/`case`, bare `struct`, C-style casts,
  C++-style casts, hex/octal/binary literals with suffixes, ternary,
  `do`/`while`, `enum`, `union`, source-level `sizeof`, `goto`,
  function pointers, and multi-dimensional arrays are all supported
  now. A fresh audit (confirmed directly by checking the grammar, not
  assumed) turned up a further, separate list of common C features
  still missing — see the next bullet.
- **A second round of basic C gaps, found by a fresh audit**: `const`
  is now supported (see below), and so are multiple declarators in one
  statement and function-pointer `typedef`s (both also below) —
  `volatile`, `static`, `extern`, `inline`, and `register` are still
  not (none of these keywords are recognized at all); no adjacent
  string-literal concatenation (`"foo" "bar"` does not become
  `"foobar"`); no bit-fields (`unsigned x : 4;` inside a `struct`/
  `union`); no comma operator (`a, b, c` as a single expression, e.g.
  in a `for` loop's own increment clause). None of these remaining
  ones are implemented yet.
- **Multiple declarators in one statement** (`int a, b, c;`,
  `int a, *b, c = 5;`) are now accepted — pointer-ness is genuinely
  PER-declarator, matching real C++ (`int *a, b;` makes `a` a pointer
  and `b` a plain int, not two pointers). Deliberately narrower than
  real C++'s full declarator grammar: only a PLAIN declarator (bare or
  pointer/reference-wrapped) can appear in a multi-declarator
  statement — an array or function-pointer declarator mixed in
  (`int a, arr[8];`) isn't supported. Works for locals, globals, and
  class members; a for-loop's own init clause deliberately does NOT
  support it (`for (int i = 0, j = 0; ...)` is a real, stated gap —
  reported directly at parse time rather than silently mishandled).
- **Function-pointer `typedef`s** (`typedef int (*Callback)(int);`,
  and Vircon32's own `typedef int(int)* Callback;` spelling — both
  accepted, the same dual-acceptance treatment every other
  function-pointer declarator in this project already has) are now
  supported, emitting the correct declarator shape for either
  `--target`.
- **`const` doesn't cover a const POINTER itself** — only `const T`
  and `const T *` (pointer to const, the pointee can't change) are
  accepted; `T * const p` (the pointer itself can't be reassigned) is
  not. This project also makes no attempt to actually ENFORCE
  const-correctness anywhere it does accept the syntax — no error for
  reassigning a const variable, no error for calling a non-const
  method through a const reference — the syntax is accepted and
  correctly emitted in generated C, with real violations left for the
  downstream C/C++ compiler to catch.
- ~~No function anywhere can return a pointer or reference type at
  all~~ — **FIXED**: `func_header` and `out_of_line_def` both now
  accept a `pointer_opt` between the return type and the function
  name, matching `var_decl`/`param`; the corresponding lowering
  (implicit address-of at a reference-returning `return` site, implicit
  dereference at a reference-returning call's use site, and the
  function's own return-type relabeling) is done too. See
  `docs/VIRCON32_QUIRKS.md`'s entry #12 for the full, bison-and-gcc-
  verified account, including two adjacent pre-existing bugs this fix
  surfaced along the way. One related, narrower gap remains open: this
  project still doesn't model const-correctness on a method's own
  `this` receiver, so forwarding a `const T &` as a method receiver can
  produce a `-Wdiscarded-qualifiers` warning in the generated C (a
  warning, not a hard type error) — see entry #12's own note on this.
- **No direct-initialization with constructor arguments on a
  stack-allocated local** (`Shape shape(7);` — valid, idiomatic C++,
  confirmed a real gap, not a rejected feature). Only two forms exist
  for a class-typed local: an explicit initializer via `=`, or no
  initializer at all (which triggers this project's own zero-argument-
  constructor injection). `opt_initializer` has no grammar shape for
  constructor arguments in parentheses. Workaround in the meantime:
  default-construct, then set public fields directly (`Shape shape;
  shape.size = 7;`). **On the list for an upcoming round.**
- **Two narrower, deliberate scope boundaries from the multi-
  dimensional array work specifically**: a function PARAMETER's own
  array-to-pointer decay (`void foo(int arr[8])`) stays single-
  dimension only — a second dimension has genuinely different decay
  rules than a first one, not just "one more bracket" the way
  declaring a variable is — and a multi-dimensional array of function
  pointers is unsupported, an intentionally rare combination not
  pursued alongside everything else that round already touched.
- **`--target=standard` doesn't compile for almost any class-having
  program yet, found by actually running `gcc` against a full sweep of
  this project's own test suite, not assumed from the plan on paper**:
  the `bool`/`true`/`false` runtime-helper boilerplate every class
  triggers (`v32_new_arr_bool` and friends) uses `bool`/`true`/`false`
  unconditionally without `#include <stdbool.h>` in standard mode
  (Vircon32 mode doesn't need this — `bool` is a native keyword
  there); confirmed to break the standard-mode build of nearly every
  class-having sample in this project's own suite. Separately, a plain
  C-style `enum` or `union` type referenced by NAME anywhere other
  than its own definition (a parameter, a variable) never gets its
  `enum`/`union` keyword back in standard mode the way a `class`/
  `struct` reference already correctly does (`print_type`'s own
  `AST_IDENT` case only checks the class registry via `type_to_class`,
  which has no notion of enums/unions at all) — confirmed directly
  (`tests/sample60.cpp`/`sample61.cpp`, an enum parameter and a union
  variable, both fail standard-mode compilation: "unknown type name
  'Color'"/"'Value'; use 'union' keyword"). Neither of these affects
  Vircon32-mode output (this project's actual primary target) at all;
  found while auditing `--target=standard`'s own maturity, not fixed
  yet, and not something either of this round's own two features
  (function-pointer typedefs, multi-declarator statements) touches or
  causes. **Flagged by the user as a priority to fix in an upcoming
  round** (raised alongside their own real-Vircon32-compiler report
  that led to the function-pointer address-of fix above) — not
  addressed yet, but explicitly no longer just a passively-noted gap.

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

`--target=vircon32` (or `v32`, the default) targets the real Vircon32 C
compiler's own quirks throughout; `--target=standard` (or `std`)
targets plain, portable C instead, for using this project as an
ordinary C++-to-C transpiler on some other system entirely — no cart
XML or debug map either way for `--target=standard`, since both are
Vircon32-platform concepts with no meaning outside it (`-x`/`-g` are
simply ignored in that mode, not an error). One single pipeline
either way, not two separate compilation passes — function pointers
(including arrays of them, and virtual-dispatch vtables) now produce
correct standard-C declarator/cast syntax in `standard` mode too, not
just Vircon32's own reversed form — see `docs/VIRCON32_QUIRKS.md` for
the itemized checklist of every place this actually branches. One real
behavioral difference worth knowing, not just a syntax swap: `main`'s
own return type. Vircon32 mode still always forces `void main(void)`,
discarding whatever the C++ source actually declared (matching the
real hardware's own requirement); standard mode honors it, typically
`int`, with real `return` statements preserved rather than stripped.

Vircon32 mode ALSO rewrites the ternary operator (`cond ? a : b`) into
an equivalent `if`/`else` wherever it directly initializes a variable,
is directly assigned to a bare identifier, or is directly a `return`
expression (including a chain of these, `cond1 ? a : cond2 ? b : c`,
fully unwound) — the real Vircon32 C compiler doesn't support the
ternary operator at all. Standard mode keeps it exactly as written,
since real standard C supports it natively. A ternary nested any other
way (a call argument, part of a larger expression, a for-loop's own
clauses, assigned through anything but a bare identifier) is left
untouched in either mode — a stated scope boundary, not silently
mishandled; see `docs/VIRCON32_QUIRKS.md`'s own entry #10 for the full
reasoning.

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
  main.c        CLI entry point (-o, -c, -v, -x, -b, -g, --target, --version)
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
