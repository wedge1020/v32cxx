# Vircon32-specific output quirks

Every place this project's generated C deliberately diverges from
standard C, in one list, organized by quirk rather than by when it was
discovered. `docs/DESIGN_NOTES.md` has the full round-by-round story of
*how* each of these got found (usually the hard way, against the real
compiler); this document exists for a narrower purpose -- a checklist
for whenever a `--standard-c` (or similar) output mode gets built, so
that work is "go through this list and make each entry conditional"
rather than a re-investigation of the whole codebase.

**Convention below**: "Vircon32 requires" is the non-standard form this
project currently always emits. "Standard C" is what a plain, portable C
compiler expects/accepts instead. "Status" says whether the Vircon32
behavior has been confirmed against the real compiler or is still
reasoned-through only. "Where" names the function(s) that would need a
conditional branch.

---

## 1. No `struct` keyword on a type reference

- **Vircon32 requires**: `struct` only in a forward declaration
  (`struct Name;`) or a full definition (`struct Name { ... };`). Every
  other reference -- a pointer field, a parameter, a local declaration,
  a cast, a `sizeof` -- must be the bare name (`Name *next;`, never
  `struct Name *next;`). This is true from the first forward declaration
  onward, not just after the full definition appears.
- **Standard C**: `struct Name` is valid everywhere a type is named,
  with or without a `typedef`.
- **Status**: Confirmed against the real compiler (`struct Name n;`
  errors "expected '{'"; `emit_vtable_instance`'s own first version hit
  this exact error directly -- see DESIGN_NOTES.md).
- **Where**: `print_type()` (codegen.c) is the single function every
  type-reference emission funnels through, specifically so this rule
  only needs to change in one place. `emit_vtable_struct`,
  `emit_classes`, and `emit_forward_declarations` correctly keep
  `struct` themselves (they emit definitions/forward-declarations, not
  references) and would keep doing so in standard-C mode too, since
  standard C accepts `struct Name { ... };` the same way.
- **For a standard-C mode**: nothing needs to change here at all --
  `struct Name` as a reference is valid in both dialects. Vircon32 mode
  is the one that needs `print_type` to STRIP the keyword; standard-C
  mode is simply the default, unmodified behavior. Actually the
  cheapest entry on this whole list.

## 2. Array declarators reversed

- **Vircon32 requires**: `int [8] myarray;` -- length before the name.
- **Standard C**: `int myarray[8];` -- length after.
- **Status**: Confirmed via Vircon32 documentation/examples, not
  exercised by any codegen phase yet -- this project's grammar has no
  array-type declarator at all (`AST_SUBSCRIPT` exists as an expression,
  `a[i]`, but nothing in `var_decl`/`param`/`typedef` grammar can
  produce an array TYPE).
- **Where**: not yet implemented anywhere. Whoever adds array-type
  support to the grammar needs `print_type()` to special-case an array
  type's declarator specifically for the DECLARATION form (use, once
  declared, is ordinary `myarray[2]` subscript syntax in both dialects
  -- only the declarator itself is reversed).
- **For a standard-C mode**: `print_type` would need an
  Vircon32-vs-standard branch specifically for this declarator shape
  once array types exist at all.

## 3. Function-pointer declarator syntax reversed

- **Vircon32 requires**: `ReturnType(ParamTypes)* name;` -- e.g.
  `int(Shape *)* Shape__area__void;` for a vtable slot.
- **Standard C**: `ReturnType (*name)(ParamTypes);` -- e.g.
  `int (*Shape__area__void)(Shape *);`.
- **Status**: Confirmed against the real compiler (vtable struct field
  emission, `tests/sample7.cpp`/`sample14.cpp` onward).
- **Where**: `emit_vtable_struct` (codegen.c) builds this declarator
  directly, inline, rather than through `print_type` (function-pointer
  types were never threaded through that function generically).
- **For a standard-C mode**: `emit_vtable_struct` needs its own
  Vircon32-vs-standard branch, since the declarator shape is inverted,
  not just a keyword toggle.

## 4. Function-pointer CAST syntax, same reversed pattern

- **Vircon32 requires**: `(ReturnType(ParamTypes)*)expr` -- matching
  the declarator pattern with no name inside the parens. E.g.
  `(int(Shape *)*)&Square__area__void`.
- **Standard C**: `(ReturnType (*)(ParamTypes))expr` -- e.g.
  `(int (*)(Shape *))&Square__area__void`.
- **Status**: Confirmed against the real compiler
  (`tests/sample24.c`, a clean compile end to end -- this was the one
  genuinely uncertain piece of the vtable-instance work, now settled).
- **Where**: `emit_vtable_instance` (codegen.c), the cast-insertion
  branch for a vtable slot whose current implementation's declaring
  class differs from the field's canonically-declared one.
- **For a standard-C mode**: same shape of fix as #3 -- its own
  conditional branch, not a `print_type` toggle.

## 5. Pointer null-initialization requires `NULL`, not `0`

- **Vircon32 requires**: `NULL` (or an explicit cast, `(Type *)0`) for a
  null pointer. A bare `0` is rejected ("cannot assign int to ...
  pointer").
- **Standard C**: `0` and `NULL` are interchangeable for a pointer
  initializer/assignment.
- **Status**: Confirmed against the real compiler
  (`(Node *) 0` tested directly, per DESIGN_NOTES.md's receiver-cast
  round).
- **Where**: not yet acted on by any codegen phase -- nothing this
  project currently emits initializes a pointer to null. The first
  phase that does (destructor invocation's eventual "already freed"
  bookkeeping? a future feature?) needs to emit `NULL` specifically in
  Vircon32 mode.
- **For a standard-C mode**: emitting a bare `0` would already be
  correct standard C, so -- like #1 -- standard-C mode is the simpler,
  unmodified default; Vircon32 mode is the one that needs the
  substitution.

## 6. `void main(void)`, unconditionally, regardless of source declaration

- **Vircon32 requires**: the program's entry point must be exactly
  `void main(void)` -- no return value, ever, regardless of what the
  C++ source declared (`int main()`, `void main()`, or anything else).
  `return expr;` inside it becomes `expr; return;` (evaluate, discard,
  return with no value).
- **Standard C**: `main` conventionally returns `int` (`return 0;` or a
  real exit code), though a compiler-specific `void main()` is also
  widely tolerated in practice.
- **Status**: Confirmed against the real compiler (present since the
  first `-c`/`-o` round; every `main`-having test compiles clean).
- **Where**: sema.c's mangling pass special-cases the name `main` to
  stay unmangled; `emit_function_definition` (codegen.c) forces the
  return type to `void` and applies `strip_return_value` specifically
  for it.
- **For a standard-C mode**: this one is more of a design choice than a
  pure syntax toggle -- standard-C mode would presumably want to honor
  whatever return type the C++ source actually declared for `main`
  (typically `int`) rather than force `void`, which changes actual
  program behavior (an exit code becomes observable), not just surface
  syntax. Worth deciding deliberately when that mode gets built, not
  defaulting silently to "same as Vircon32 mode but keep the keyword."

## 7. Function prototypes require explicit parameter names

- **Vircon32 requires**: every parameter in a PROTOTYPE (not just a
  definition) needs a name -- `void foo(int x, int y);`, not
  `void foo(int, int);`.
- **Standard C**: parameter names in a prototype are optional.
- **Status**: Confirmed against the real compiler (present since the
  first working prototype-emission round).
- **Where**: `emit_function_prototype`/`emit_method_prototype`
  (codegen.c) already always print the parameter's own name, taken
  directly from the AST -- there was never a code path that omitted it,
  so there's nothing to toggle here specifically.
- **For a standard-C mode**: no change needed at all -- naming every
  parameter is valid, unremarkable standard C too. Not actually a
  divergence this project would need to reverse, just a Vircon32
  requirement that happens to already be a strict subset of what
  standard C allows.

## 8. `new`/`delete` have no native equivalent in EITHER dialect

- Not a Vircon32-vs-standard-C divergence at all -- plain C (Vircon32's
  or otherwise) has no `new`/`delete` keyword. This project's
  `v32_new_*`/`v32_delete` translation (`malloc`/`free`-based
  allocation plus an explicit constructor call) would stay exactly the
  same in a standard-C mode; only the header providing `malloc`/`free`
  changes (#9, below).
- **Where**: `lower.c`'s `new_delete_rewrite_expr`,
  `codegen.c`'s `emit_new_delete_runtime`/`emit_v32_delete`.
- **For a standard-C mode**: no change to the translation strategy
  itself.

## 9. `misc.h` is Vircon32's own header name for `malloc`/`free`/etc.

- **Vircon32 requires**: `#include "misc.h"` for `malloc`, `free`,
  `memset`, `memcpy`, `rand`, `srand`, `exit`, and others.
- **Standard C**: `#include <stdlib.h>` (`malloc`/`free`/`rand`/
  `srand`/`exit`), `#include <string.h>` (`memset`/`memcpy`).
- **Status**: Confirmed -- `misc.h` provided directly (Matthew's own
  copy of Vircon32's standard library source), and `#include "misc.h"`
  compiles clean end to end (`tests/sample17.c` onward).
- **Where**: `codegen_run`'s own `needs_misc` block (codegen.c).
- **For a standard-C mode**: the conditional include line would need to
  become `#include <stdlib.h>` (and `<string.h>` if this project ever
  emits `memset`/`memcpy` calls of its own) instead of `"misc.h"`.
  Simple string substitution, not a structural change.

---

## What this list does NOT cover

- Anything this project hasn't discovered yet -- this is a record of
  confirmed (or at least directly-reasoned) divergences found so far,
  not a claim of completeness. New Vircon32-specific requirements have
  been found in nearly every round of this project's development;
  expect more.
- Semantic/behavioral gaps that aren't about OUTPUT SYNTAX at all (no
  destructor invocation, no vtable population for a class with no
  constructor, etc.) -- those are tracked in `docs/DESIGN_NOTES.md`
  and aren't specific to Vircon32 vs. standard C; they're missing
  regardless of which C dialect this project targets.
