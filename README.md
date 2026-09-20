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
already in it (`tests/09sample.cpp`, `tests/15sample.cpp`, both passing
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
  downstream C/C++ compiler to catch. What IS fixed, found only by an
  actual Vircon32 compiler run (not gcc, which only ever warned):
  forwarding a `const T &`/`const T *` as a non-const method's receiver
  now gets an explicit const-stripping cast inserted at the call site
  (`(Shape *)s`) rather than a plain, uncasted pointer assignment —
  gcc only ever gave this a `-Wdiscarded-qualifiers` warning, but the
  real Vircon32 compiler rejects it outright ("cannot assign const
  struct Shape* to struct Shape*: discards const qualifier"), a hard
  error, not a warning. See `cast_receiver_if_needed`'s own doc
  comment in `lower.c` and `docs/DESIGN_NOTES.md` for the full account.
- ~~No function anywhere can return a pointer or reference type at
  all~~ — **FIXED**: `func_header` and `out_of_line_def` both now
  accept a `pointer_opt` between the return type and the function
  name, matching `var_decl`/`param`; the corresponding lowering
  (implicit address-of at a reference-returning `return` site, implicit
  dereference at a reference-returning call's use site, and the
  function's own return-type relabeling) is done too. See
  `docs/VIRCON32_QUIRKS.md`'s entry #12 for the full, bison-and-gcc-
  verified account, including two adjacent pre-existing bugs this fix
  surfaced along the way.
- ~~No direct-initialization with constructor arguments on a
  stack-allocated local~~ — **FIXED**: `Shape shape(7);` now parses and
  transpiles, alongside `new T(args)`'s own already-existing constructor-
  argument support. A new var_decl alternative
  (`type_spec IDENTIFIER '(' arg_list ')'`) accepts it, deliberately
  narrower than this grammar's other declarator forms in two ways: no
  `pointer_opt` (this is specifically for a VALUE local — a pointer
  local already has its own unambiguous spelling), and a REQUIRED,
  non-empty argument list — `Shape shape();` is deliberately still
  rejected, matching real C++'s own "most vexing parse" resolution
  (that spelling is a function declaration, not object construction).
  Constructor arguments parse onto a new `AST_DIRECT_INIT` marker node,
  resolved against the class's own constructor overloads by the exact
  same `resolve_new_expr` machinery `new T(args)` already uses (same
  "no matching overload"/"ambiguous" diagnostics, confirmed directly —
  a 3-argument call against a 1-argument constructor reports "'Shape'
  expects 1 argument(s), but 3 were given", same wording `new` already
  gives), then lowered by phase 7 into a direct call to that
  constructor with `&shape` as the receiver — the exact same shape
  phase 7's pre-existing zero-argument-constructor injection already
  produces, just with real arguments now. This needed a genuine, GLR-
  forking shift/reduce conflict (not one of this grammar's usual
  "bison's default shift already resolves it, single lookahead token is
  enough" conflicts) — right after `type_spec`, with an IDENTIFIER
  lookahead, deciding between this new alternative and every OTHER
  var_decl alternative starting `type_spec pointer_opt IDENTIFIER ...`
  genuinely needs more than one token of lookahead to settle (whatever
  comes right after the name — `(` vs `;`/`=`/`,`/`[`), which is exactly
  what `%glr-parser` was declared, at this file's very top, to let this
  grammar grow into. See `tests/75sample.cpp` for a worked example
  (single- and multi-argument constructors, plus overload resolution
  reached through this path specifically, not just through `new`).
- **Two narrower, deliberate scope boundaries from the multi-
  dimensional array work specifically**: a function PARAMETER's own
  array-to-pointer decay (`void foo(int arr[8])`) stays single-
  dimension only — a second dimension has genuinely different decay
  rules than a first one, not just "one more bracket" the way
  declaring a variable is — and a multi-dimensional array of function
  pointers is unsupported, an intentionally rare combination not
  pursued alongside everything else that round already touched.
- ~~`--target=standard` doesn't compile for almost any class-having
  program yet~~ — **FIXED**: the `bool`/`true`/`false` runtime-helper
  boilerplate every class triggers (`v32_new_arr_bool` and friends) uses
  `bool`/`true`/`false` unconditionally, which standard C only gets from
  `<stdbool.h>` (Vircon32 mode doesn't need this — `bool` is a native
  keyword there, confirmed by Matthew); `codegen_run` now emits
  `#include <stdbool.h>` unconditionally in standard mode, deliberately
  NOT gated on the same `needs_misc` check that guards `<stdlib.h>` —
  some samples declare a plain `bool` local with no class and no
  `new`/`delete` anywhere in the program at all (`tests/48sample.cpp`,
  `58sample.cpp`, `59sample.cpp`), so gating on `needs_misc` would have
  missed exactly those. Separately, a plain C-style `enum` or `union`
  type referenced by NAME anywhere other than its own definition (a
  parameter, a variable) never got its `enum`/`union` keyword back in
  standard mode the way a `class`/`struct` reference already correctly
  does — fixed with a new `find_enum_or_union_decl` helper (mirrors
  `program_has_any_class`'s recursive top-level/namespace scan, since
  sema.c keeps no enum/union registry the way it does for classes) that
  `print_type`'s `AST_IDENT` case now consults; confirmed directly
  (`tests/60sample.cpp`/`61sample.cpp`, an enum parameter and a union
  variable, both now compile under `--target=standard`). A full
  `--target=standard` + real-`gcc` sweep across every one of this
  project's 74 test samples now passes with zero failures (excluding
  three samples that intentionally `#include "video.h"`, Vircon32's own
  hardware API, which standard C obviously has no counterpart for and
  was never in scope here). One more real bug turned up by that same
  sweep, unrelated to either fix above: virtual-destructor dispatch's
  own base-class receiver cast (`v32_delete_ClassName`, `codegen.c`) hand
  -rolled `(ClassName *)` instead of going through the same
  `print_class_type_name` helper every OTHER class-typed cast in this
  file already uses, so it silently produced a bare, un-prefixed cast in
  standard mode (`(Shape *)ptr` instead of `(struct Shape *)ptr`) —
  fixed by routing it through that same helper like everywhere else.
- ~~A copy constructor (`Shape(const Shape &other);`) never actually
  resolved, and — once fixed — never actually received its argument
  correctly either~~ — **FIXED**, two stacked bugs found by an audit
  deliberately looking for gaps a student early in an intro OOP course
  might hit, not by a failing existing test. See `docs/DESIGN_NOTES.md`
  for the full account (overload resolution's `types_equal` requiring
  exact AST-shape identity instead of real C++'s own reference-binding
  rule; then, once that resolved, the argument itself still not getting
  the implicit `&` a reference parameter needs, because the one place
  that fix belonged needed to run BEFORE phase 5 relabels a resolved
  reference parameter to a plain pointer, not after). Exercised by
  `tests/76sample.cpp`, through both `new Shape(a)` and this round's own
  direct-init syntax.

## Gaps found auditing common intro-OOP patterns

A deliberate audit, going looking for the kind of thing a student early
in a C++ course tries first, turned up several more gaps beyond the
copy-constructor one above — confirmed directly by actually compiling
each one, not assumed from reading the grammar. The stack-array and
`nullptr` gaps below were fixed in the round right after this list was
first written (see docs/DESIGN_NOTES.md for both); the rest are listed
here so they're visible rather than silently discovered one at a time
later.

- **FIXED — default parameter values** (`void greet(int x, int y =
  5);`) **used to be a flat parse rejection.** Found trying to
  transpile a larger, real-world test file (a freestanding "Space
  Invaders" demo) whose very first blocker was `Random(unsigned int
  seed = 0x1234ABCDu) : mState(seed) {}` — `param` had no grammar
  shape for `= expr` after a parameter's name at all ("syntax error,
  unexpected '=', expecting ')'" for a plain function, or, thanks to
  this grammar's own GLR ambiguity resolution picking a different
  parse path first, the somewhat more confusing "unexpected INT_KW,
  expecting COLONCOLON" for a constructor). One of the most common
  things an intro course teaches early (a function callable with fewer
  arguments than it declares), and by far the most involved single fix
  on this list: a real default value has to work everywhere real C++
  allows a call to omit a trailing argument, not just the one obvious
  case.

  Fixed in four layers:
  - **Grammar** (`parser.y`): a new `param` alternative accepts `type_spec
    pointer_opt IDENTIFIER '=' expr`, storing the default expression in
    `AST_PARAM`'s own previously-unused `a` field. No arity/ordering
    validation happens here (real C++ requires every parameter after
    the first defaulted one to also be defaulted) — a malformed
    declaration that defaults an earlier parameter but not a later one
    is accepted rather than specially diagnosed, this project's usual
    "miss a case rather than guess wrong" stance for a pattern no real
    test is likely to hit by accident.
  - **Semantic analysis** (`sema.c`): overload resolution's own arity
    check, previously a single exact count, is now a RANGE — from
    `min_required_args()` (the count of leading non-defaulted
    parameters) up to the full declared parameter count — for both the
    single-candidate and genuinely-overloaded resolution paths.
  - **Lowering** (`lower.c`): Vircon32 C, like plain C, has no
    default-argument mechanism at all, so every call the generated code
    makes must supply every argument explicitly — `fill_default_args`
    splices cloned copies of the missing trailing parameters' own
    default-value expressions (`clone_default_expr`) into a resolved
    call's argument list. Hooked into every place this project builds a
    call against a possibly-defaulted target: an ordinary function or
    method call (`finalize_call`), a constructor invoked via
    direct-initialization or `new` (the shared `fixup_ctor_reference_args`
    both go through), and — found only by then actually testing the
    "declare a var with no args at all" case — the *implicit*
    zero-argument constructor call a plain `ClassName var;` declaration
    triggers, both for a single stack local and for a stack array's own
    per-element constructor loop, and a derived class's own implicit
    call to its base's constructor when no member-initializer list
    names the base at all. Real C++ treats a constructor whose real
    parameters are all defaulted as just as much a "default
    constructor" as one declaring none at all, and this project's own
    `find_zero_arg_constructor` (the single lookup all four of those
    call sites already shared) needed the identical widening
    `sema.c`'s own arity check got, plus a matching widening to
    `check_implicit_base_construction`'s independent copy of the same
    "does the base have a default constructor" question.

  Verified against tests/81sample.cpp, which exercises all four
  argument-filling shapes above end to end: not just a clean
  `--target=standard` + gcc compile, but an actual run of the compiled
  binary, whose exit code was checked against the exact expected value
  computed independently in Python (accounting for 32-bit signed
  overflow in the LCG arithmetic one of the test's own default-valued
  constructors uses) — confirming every filled-in default argument
  carries the right value at runtime, not just that the generated C
  happens to compile.
- **Static members** (`static int count;` inside a class body) — also
  a flat parse rejection ("syntax error, unexpected INT_KW, expecting
  COLONCOLON") — `member` has no grammar shape recognizing the `static`
  keyword at all, only ordinary instance fields/methods. A common
  early-OOP pattern (a class-wide counter, a singleton-style instance
  pointer) with no workaround in this project today.
- **FIXED — a stack array of class objects used to get NO per-element
  constructor call at all (SILENT, not a rejection).** `Shape
  shapes[3];` parsed and transpiled without any error, but the
  generated code was just `struct Shape shapes[3];` with nothing else
  — confirmed directly by reading the generated C, not assumed: if
  `Shape` had a real constructor body, every element was left with
  genuinely uninitialized memory, not the zero-argument-constructed
  objects real C++ would produce. This was the most dangerous gap on
  this list precisely because nothing about it looked wrong until the
  program ran. Fixed by extending phase 7's own existing per-element
  machinery (`inject_ctor_calls_block`, lower.c — the same phase this
  round's own direct-init work already extended once) with one more
  shape to build: a `for` loop over the array, calling the element
  class's own zero-argument constructor on `&arr[i]` for every index,
  built entirely out of AST node kinds this project already produces
  elsewhere (`AST_FOR`, `AST_SUBSCRIPT`, `post++`) — no new AST kind
  and no new lowering phase needed. Array-`new` (`new T[N]`) is a
  separate, still-open gap (see the note on `new`/`delete` above) —
  fixing the stack case didn't fix the heap one, since they're
  entirely different code paths with no shared machinery between them.
  See tests/78sample.cpp and docs/DESIGN_NOTES.md for the full story.
- **An in-class default member initializer (`int size = 5;` written
  directly on a class's own field declaration) is also SILENTLY
  dropped, not rejected.** `class Shape { public: int size = 5; };`
  parses without error, but confirmed directly by reading the generated
  C: the `= 5` simply never appears anywhere, and if the class has no
  other constructor, a `Shape` local is left with genuinely
  uninitialized memory instead of `size == 5`. A member's own
  `AST_VAR_DECL` keeps its initializer expression exactly like an
  ordinary local's would (the parser doesn't reject it, and doesn't
  even warn), but nothing downstream ever reads a MEMBER's initializer
  the way a zero-argument-constructor injection reads a class's actual
  constructor body — this project's whole member-initialization story
  runs entirely through explicit constructors (member-initializer
  lists, ordinary assignment in a constructor body); a bare default
  value on the field declaration itself was never wired into either.
- **FIXED — `nullptr` used to transpile as the literal, unmangled word
  `nullptr`, not Vircon32's own required `NULL`.** Confirmed directly:
  `Shape *p = nullptr;` produced `Shape * p = nullptr;` unchanged in
  Vircon32-mode output. `nullptr` isn't a keyword in C at all (it's
  C++11), so Vircon32's own C compiler would reject this outright as an
  undeclared identifier — the exact same "NULL, not a bare identifier"
  quirk this project already handled correctly for a literal `0` (see
  `docs/VIRCON32_QUIRKS.md`) had never been extended to this newer C++
  spelling of the same idea. Fixed with a new `AST_NULL_LIT` literal
  kind (mirroring `AST_BOOL_LIT`'s own existing shape) recognized by
  the lexer/grammar and printed as `NULL` by the code generator in
  BOTH target dialects — `NULL` is already in scope everywhere either
  one can reach: misc.h for `--target=vircon32`, `<stdlib.h>` for
  `--target=standard`. See tests/77sample.cpp.
- **Range-based `for` (`for (int x : arr)`) is a flat parse
  rejection** — `for_init`'s own grammar has no colon-based alternative
  at all, only the classic three-clause C-style form. A C++11 feature,
  increasingly taught early alongside ordinary arrays.
- **FIXED — `friend` (a friend function or friend class declaration
  inside a class body) used to be a flat parse rejection**, `member`
  having no grammar shape recognizing the `friend` keyword at all. Both
  forms are now supported: `friend class X;` (a bare, unresolved
  `IDENTIFIER`, deliberately not requiring `X` to already be a known
  type — friending a class not yet defined earlier in the same file is
  the whole point) and `friend ReturnType f(params);` (reusing
  `func_header` unchanged, then given its own `AST_FRIEND_FUNC_DECL`
  kind rather than an ordinary method, so it's registered as a plain
  free function — no `this`, no mangled `ClassName__` prefix, no vtable
  slot — while still granting the class's own private/protected access
  to it). `sema.c`'s existing `check_member_access` now consults a new
  `is_friend_of()` before enforcing access at all: a friend class is
  checked by class identity, a friend function by name only (a
  deliberately narrower, documented match than full signature
  resolution — see `ClassLayout.friend_function_names`'s own doc
  comment in `inc/sema.h`). Neither transitive nor inherited, matching
  real C++. See tests/79sample.cpp (a friend class and a friend
  function both granted access) and tests/80sample.cpp (a companion
  negative test confirming a non-friend class still gets the ordinary
  private-access error).

- **FIXED — a global (namespace-scope) object's own method calls used to
  transpile completely untranslated**, including when accessed with an
  explicit namespace qualifier from outside its own namespace
  (`si::g_rng.seed(...)` called from `main()`, outside `namespace si`).
  Found from a real user's Space Invaders program: `Random g_rng;` at
  namespace scope, then `g_rng.next(300)`/`g_rng.seed(...)` calls —
  real Vircon32 C, seeing the untranslated `.`-syntax, failed outright
  ("'next' is not a member of type 'Random'"), because `infer_expr_type`
  never checked a global-variable registry at all — one didn't exist.
  Fixed with a new flat global-variable registry (mirroring the
  existing free-function one), populated while walking top-level and
  namespace-nested declarations, and consulted as a fallback in a new
  shared `lookup_ident_expr_type` helper used by both a bare identifier
  and a namespace-qualified one (`AST_QUALIFIED_ID`, a second, separate
  gap the same investigation found — a qualified VALUE reference is a
  structurally different AST node from a bare identifier, and
  previously fell through to "unknown type" entirely). See
  tests/82sample.cpp.
- **FIXED — a `virtual ... const` method's vtable slot and its
  cast-on-override both silently dropped the `const`**, producing a
  real `-Wincompatible-pointer-types` warning from gcc (confirmed a
  hard error class this project treats seriously, per its own
  established const-correctness fixes elsewhere). Fixed in
  `emit_vtable_struct`/`emit_vtable_instance` (`codegen.c`) by checking
  the method's own declared constness (`canonical->str2`) and printing
  `const` on the receiver in both places, matching what the actual
  override function's own signature already correctly had.
- **FIXED — a derived-class object couldn't bind to a base-class
  reference parameter at all** (`bool collidesWith(const Entity &e)`
  called with a `Bullet`, an `Entity` subclass, flatly failed to
  resolve: "no matching overload"), despite being exactly the ordinary
  polymorphic upcast-on-binding real C++ allows, and exactly what this
  project's own single-inheritance struct-layout guarantee (a derived
  struct's fields are always a valid prefix of its base's) was already
  built to support. Fixed in `type_matches_param` (`sema.c`) by
  accepting a same-or-descendant class match for the reference-binding
  case specifically (a by-value base parameter accepting a derived
  argument, which would need actual struct slicing, remains
  unsupported and out of scope). A second, separate lowering gap
  surfaced right behind it: even once resolved, nothing ever inserted
  the base-class pointer CAST the resulting C code needs (`&derivedObj`
  is a `Derived *`, a real mismatch against a `const Base *`
  parameter) — fixed for both reference-parameter and plain
  pointer-parameter arguments (the latter needed for an entirely
  different reason: a single-candidate call skips argument type
  checking altogether, so the mismatch was never even caught) via a
  new shared `cast_ref_arg_if_needed` (`lower.c`), applied at every
  call/constructor-argument site. The identical gap for a plain
  pointer ASSIGNMENT (`Alien *a; a = new AlienTopRow(...);`, exactly
  the polymorphic-factory pattern `Swarm::spawn` uses) got the same
  treatment. See tests/82sample.cpp.
- **FIXED — an `operator[]` overload's result type wasn't tracked
  through further use**, breaking both overload resolution (`*list[i]`
  passed where a class-typed parameter was expected) and, more subtly,
  VIRTUAL DISPATCH lowering on a chained call (`list[i]->draw(...)`)
  — the latter only surfacing because lowering rewrites the subscript
  into a mangled-name call BEFORE the outer call's own virtual-dispatch
  logic runs, and the stale by-name lookup that logic depended on could
  never match the now-mangled name, so the call silently stayed
  unlowered. Fixed by checking an expression's own attached
  `CallResolution` first in `infer_expr_type`'s `AST_SUBSCRIPT` and
  `AST_CALL` cases (confirming, in the latter case, that
  `rewrite_operator_use` already carries the resolution forward onto
  the rewritten call node) before falling back to the older, by-name
  logic that only ever made sense pre-lowering. See tests/82sample.cpp.
- **FIXED — a namespace-qualified TYPE reference (`si::Game game;`,
  written from OUTSIDE the `si` namespace) never got the `struct`
  keyword under `--target=standard`**, a hard "unknown type name"
  compile error — even though every unqualified use of the exact same
  class throughout the rest of the file was correctly printed. Found
  from the real Space Invaders program's own `main()`, which declares
  its top-level objects this way. `print_type`'s `AST_QUALIFIED_ID`
  case had never been given the same struct/enum/union lookup its
  `AST_IDENT` case already does — fixed by mirroring that same logic
  (`codegen.c`).
- **FIXED — a class with a vtable but NO user-declared constructor at
  all got NO vtable-pointer initialization anywhere, on the stack OR
  the heap** — a serious, previously-undiscovered bug: `new Square()`
  (heap) or a plain `Square s;` (stack, scalar or array) left
  `vtable` as raw, uninitialized memory, and the very first virtual
  call through it dispatched through garbage — confirmed with an
  isolated repro that segfaulted on every one of those three shapes
  before this fix. Root cause: vtable-pointer initialization was only
  ever injected as the first statement of an EXISTING constructor body
  (phase 8, `inject_vtable_init_classes`) — exactly right for a class
  that has one, but a class with none has no body to inject into, and
  nothing else ever set the pointer at all. Fixed with three separate,
  narrowly-targeted additions rather than synthesizing a fake AST-level
  default constructor: `emit_new_delete_runtime`'s own fallback
  allocator (`codegen.c`, for the heap case) now sets `self->vtable`
  directly when the class has one; a new shared `build_vtable_init_stmt`
  (`lower.c`) builds the stack-object equivalent (`obj.vtable = &...`),
  used both for a plain scalar local and, via `build_array_ctor_loop`'s
  new no-constructor fallback, for each element of a stack array.
  Alongside this fix, a second, genuinely separate PRE-EXISTING bug in
  the same code surfaced and was fixed too: a bare pointer-typed local
  with no initializer (`Widget *p;`) was ALSO being treated as an
  object needing its own constructor called on it (`type_to_class`
  resolves straight through a pointer wrapper, which is right for most
  callers but wrong for "does this VarDecl need construction") —
  confirmed generating flatly wrong code (`Widget__Widget__void(&p)`,
  passing a `Widget **` where `Widget *` is expected) before this fix;
  now guarded out for both the scalar and array-of-pointers shapes.
  See tests/82sample.cpp (exercises the no-constructor stack/heap
  cases indirectly via `Dog`/`Cat`/`Animal`).
- **A global (namespace-scope) object direct-initialized WITH
  constructor arguments (`Counter g_counter(100);`) doesn't generate a
  working initializer at all** — codegen currently emits
  `struct Counter g_counter = 0 /* WARNING: unhandled expression kind
  in codegen */;`, an invalid initializer. Found alongside the
  global-variable-method-call fix above, while writing its regression
  test — deliberately left unfixed and out of scope for this round,
  since the real program that motivated this whole investigation only
  ever uses the always-supported zero-argument shape (`Random g_rng;`,
  seeded later via an ordinary method call) for its own namespace-scope
  objects. A real gap, not a guess: this project's constructor-call
  injection (phase 7, `lower.c`) only ever runs over function BODIES
  (`inject_ctor_calls_stmt`/`_block`), never over top-level/namespace-
  scope declarations, which have no enclosing function body for that
  walk to reach at all.

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

`-vvv` also prints a **lowering notes log**, right after the lowering
dump: one line per site where a lowering phase rewrote your code
specifically to route around a Vircon32 quirk, rather than for any
ordinary C++-to-C reason — a `?:` ternary hoisted into a temporary plus
an if/else (Vircon32's lexer doesn't recognize `?` at all), an implicit
`&function` inserted where standard C would decay the bare name on its
own, a const-discarding receiver cast, a base/derived pointer upcast.
Each line names the source line and the specific quirk behind it,
distinct from `-vv`'s inline comments above: those explain ordinary
C++-to-C mechanics (vtables, `this`, `new`/`delete`) inline in the
generated file itself, while this log is diagnostic-only output on
stdout, purely about the handful of rewrites this project's own
quirk-workarounds perform. `./bin/v32c++ -vvv -c tests/71sample.cpp`
(ternary rewrites) or `tests/73sample.cpp` (function-pointer
address-of) are good ones to try this on first.

With `-vv` (or higher — `-vvv` includes everything `-vv` does), the
generated C explains itself at the points where the C++-to-C
transformation is least obvious — a vtable's own struct and instance,
the explicit `this` parameter every method gets, what `new`/`delete`
actually become, a destructor invoked automatically at scope exit, a
virtual call dispatched through the vtable. Worth reading through on
its own, not just a build artifact — the C generated for
`tests/32sample.cpp` (vtables, a virtual destructor, `new`/`delete`) is
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
an equivalent `if`/`else` — the real Vircon32 C compiler doesn't
support the ternary operator at all, and its own lexer doesn't even
recognize `?` as a token, so this isn't a style preference, an
untranspiled ternary is a hard compile error there. Directly
initializing a variable, being directly assigned to a bare identifier,
or being a direct `return` expression (including a chain of these,
`cond1 ? a : cond2 ? b : c`, fully unwound) rewrites with no temporary
variable needed at all. Every OTHER position a ternary can appear in —
a call argument, part of a larger arithmetic/subscript/member
expression, assigned through anything but a bare identifier — is
handled too, by hoisting it into its own preceding temporary set via
an ordinary `if`/`else`; this was a real, previously-undiscovered
miscompile, found only by an actual Vircon32 compiler run
(`add(x > y ? x : y, 1)` transpiled with the literal `?`/`:` still in
it and failed to even lex). Standard mode keeps every ternary exactly
as written in every position, since real standard C supports it
natively. Two narrow boundaries remain, stated plainly: a ternary
inside a for-loop's own init/cond/incr clauses (would need restructuring
the loop itself, not attempted), and one reachable only through a
brace-less single-statement slot (`if (cond) foo(cond2 ? a : b);` with
no block around it — inserting a preceding temp declaration needs a
real statement list to insert into). See
`hoist_ternaries_in_expr`'s own doc comment in `lower.c` and
`docs/VIRCON32_QUIRKS.md`'s own entry #10 for the full reasoning.

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
