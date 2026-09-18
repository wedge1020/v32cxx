# Vircon32-specific output quirks

Every place this project's generated C deliberately diverges from
standard C, in one list, organized by quirk rather than by when it was
discovered. `docs/DESIGN_NOTES.md` has the full round-by-round story of
*how* each of these got found (usually the hard way, against the real
compiler); this document exists for a narrower purpose -- a checklist
for whenever a `--standard-c` (or similar) output mode gets built, so
that work is "go through this list and make each entry conditional"
rather than a re-investigation of the whole codebase.

**The `--target` flag now exists** (`vircon32`/`v32`, the default, or
`standard`/`std`) -- this checklist did exactly what it was written
for. Each entry below is now marked IMPLEMENTED, NOT YET IMPLEMENTED
(a real, deliberate gap), or N/A (nothing needed changing). See
`docs/DESIGN_NOTES.md`'s own entry on this round for the fuller story,
including two real bugs found only by testing actual
`--target=standard` output rather than trusting the plan on paper.

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
- **For a standard-C mode**: `print_type` needed to ADD `struct` for a
  class reference in standard mode (via `type_to_class`, confirming
  the name is actually a class and not a primitive/typedef) rather
  than the reverse -- this list's own original prediction
  ("nothing needs to change... actually the cheapest entry on this
  whole list") turned out wrong once actually built. The real
  surprise: several OTHER functions build a class's own type name by
  hand -- `emit_new_delete_runtime`, `emit_array_new_runtime`,
  `emit_vtable_struct`, `emit_vtable_instance`, `emit_method_
  prototype` -- entirely bypassing `print_type`, so fixing `print_type`
  alone silently missed all of them. Found only by actually running
  `--target=standard` against a real class-having test and reading the
  output (`struct Shape *v32_new_Shape...` next to a bare, un-prefixed
  `Shape *self = ...` a few lines later), not by re-deriving every call
  site from the plan on paper. Fixed with a new `print_class_type_name`
  helper, applied at every one of those call sites -- **IMPLEMENTED**.

## 2. Array declarators reversed

- **Vircon32 requires**: `int [8] myarray;` -- length before the name.
- **Standard C**: `int myarray[8];` -- length after.
- **Status**: CONFIRMED against the real compiler -- clean build,
  transpile, and compile of the array-using test (`tests/sample26.c`,
  Matthew's report). Function parameters (decay-to-pointer) and array
  initializer lists (`= {1, 2, 3}`), both originally scoped out here,
  are now ALSO implemented (a later round) -- see `var_decl`'s own
  grammar comments and ast.h's `AST_INIT_LIST`. The initializer-list
  VALUE syntax itself (`{1, 2, 3}`) is positional, not C99 designated --
  same reasoning as entry 4's cast-syntax choice below: fewer
  independent pieces of unconfirmed Vircon32-specific syntax to be
  wrong about at once. That specific piece is UNCONFIRMED as of this
  writing (this whole later round was written without bison available,
  same constraint as the original array-declarator work) -- needs
  Matthew's own build, same as every grammar change in this project
  does before being treated as settled.
- **Where**: `print_type()` (codegen.c) emits `ElementType [N]` for an
  `AST_ARRAY_TYPE` node; every call site already appends the variable
  name afterward the same way it does for every other type, so no
  caller needed to change at all once `print_type` handled the new
  kind. `parser.y`'s `var_decl` accepts array declarators on the input
  side in BOTH the standard-C form (length after the name) and an
  alternate Vircon32-native-style form (length before the name) --
  see quirk-tracking entry below, "Two accepted C++-side input forms."
  `param` separately accepts `int arr[]`/`int arr[8]`, decaying straight
  to an ordinary pointer type at parse time (real C/C++ semantics
  exactly) -- no AST_ARRAY_TYPE involved for a parameter at all, so no
  further sema/lower/codegen work was needed for that specific piece.
- **For a standard-C mode**: this prediction held up exactly --
  `print_type`'s own "prefix, then caller appends the name"
  architecture genuinely can't produce `ElementType name[N]` on its
  own, since the bracket has to come AFTER the name, not before. Fixed
  with a new `print_array_suffix` helper, called by whichever CALLER
  prints the name -- found by tracing every `print_type` call site in
  codegen.c and confirming only two actually need it
  (`print_var_decl_inline`, covering both local variables and globals;
  `emit_struct`'s own data-field loop) -- a function's own return type
  and every parameter's own type can never be arrays at all (illegal
  to return an array by value in C/C++; `param`'s own grammar decays
  an array parameter straight to a pointer at parse time, so no
  `AST_ARRAY_TYPE` node is ever built for one). Multi-dimensional
  arrays (nested `AST_ARRAY_TYPE`) work correctly in both modes with
  no extra code -- **IMPLEMENTED**. The initializer-list syntax itself
  needed no change at all, exactly as predicted -- `{1, 2, 3}` is
  valid, unmodified standard C too.

## Two accepted C++-side input forms for arrays (not a Vircon32-vs-
standard-C quirk itself, but a related design decision worth tracking
in the same place)

Matthew asked whether the C++-side input could accept EITHER standard
array syntax (`int scores[8];`) OR Vircon32's own native style
(`int [8] scores;`) as valid input, specifically so someone already
fluent in Vircon32 C -- or transitioning from it -- never has to learn
a second, unrelated declarator convention if they don't want to, while
someone coming from ordinary C++ can just write what they already know.
Both produce an identical `AST_ARRAY_TYPE`; the AST carries no memory of
which spelling was used, and output is always Vircon32's required form
regardless. Confirmed feasible without grammar ambiguity by reasoning
through the LALR(1) lookahead at each decision point (not yet confirmed
by an actual bison build -- see the array-declarators entry above).
Matthew's own framing for this, worth keeping as a standing principle
for future syntax work rather than a one-off: "let the C++ appear
normal, even if the transpile has to adjust things" -- extend the same
dual-acceptance approach to function-pointer declarators whenever that
work happens, rather than deciding it fresh each time. If dual
acceptance for some future construct turns out to create real grammar
conflicts, the fallback is the plain standard-C form alone, not the
Vircon32-native one -- ordinary C++ readability was judged the more
important default to protect.

## 3. Function-pointer declarator syntax reversed

- **Vircon32 requires**: `ReturnType(ParamTypes)* name;` -- e.g.
  `int(Shape *)* Shape__area__void;` for a vtable slot.
- **Standard C**: `ReturnType (*name)(ParamTypes);` -- e.g.
  `int (*Shape__area__void)(Shape *);`.
- **Status**: Confirmed against the real compiler (vtable struct field
  emission, `tests/sample7.cpp`/`sample14.cpp` onward).
- **Where**: `emit_vtable_struct` (codegen.c) builds this declarator
  directly, inline, rather than through `print_type` for the vtable-
  slot case specifically. A later round also added an
  `AST_FUNC_PTR_TYPE` case to `print_type` itself, for the separate
  C++-side function-pointer variable-declaration feature (`int (*fp)
  (int, int);` and Vircon32's own `int(int, int)* fp;` spelling) --
  that case has the identical reversed-declarator shape.
- **For a standard-C mode**: NOT YET IMPLEMENTED -- a deliberate,
  documented boundary, not an oversight. Unlike array declarators
  (#2 above), the name has to sit INSIDE the parens for standard C
  (`ReturnType (*name)(Params)`), not get appended after the way
  every other case in `print_type` works -- the same "print a suffix
  after the name" fix that solved arrays doesn't apply here; it would
  need its own prefix/suffix split (print `ReturnType (*` before the
  name, a new suffix function prints `)(Params)` after it) at every
  call site that can reach a function-pointer-typed declaration, in
  both `print_type` and `emit_vtable_struct`. Scoped out of the
  `--target` round deliberately: function pointers are rare enough,
  and this split genuinely involved enough, that getting it right
  under time pressure alongside everything else that round touched
  felt riskier than flagging it clearly and coming back to it later.
  Matthew's own confirmation when this was raised: the C++-side
  dual-input-syntax acceptance for function pointers was always a
  Vircon32-specific nicety, not something standard mode needs to
  preserve either. Standard-mode output involving a function-pointer-
  typed variable, parameter, field, or vtable slot is therefore NOT
  valid standard C yet -- `print_type`'s own `AST_FUNC_PTR_TYPE` case,
  and `emit_vtable_struct`/`emit_vtable_instance`'s own doc comments,
  all point back here.

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
- **For a standard-C mode**: NOT YET IMPLEMENTED, same boundary and
  same reasoning as #3 above -- deferred deliberately, not an
  oversight.

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
  substitution -- **N/A for now**, since nothing yet emits a null
  pointer at all; revisit whenever the first phase that does gets
  built.

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
- **For a standard-C mode**: the deliberate decision this list itself
  flagged as needed got made -- standard mode honors whatever return
  type the C++ source actually declared for `main` (typically `int`),
  preserving real `return` statements rather than stripping them, since
  forcing `void` would have changed actual program behavior (an exit
  code becoming unobservable), not just surface syntax. Implemented as
  a single `force_void_main` condition
  (`name == "main" && g_target == TARGET_VIRCON32`), computed
  identically in both `emit_function_header` and `emit_function_
  definition` rather than threaded through as a parameter, specifically
  so the two can never independently drift out of sync about whether a
  given `main` gets the void-forcing/return-stripping treatment --
  **IMPLEMENTED**.

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
  itself -- **N/A, confirmed**. The `v32_new_`/`v32_delete` NAME
  PREFIX itself was briefly reconsidered while building `--target`
  (would a standard-C mode want a less Vircon32-flavored prefix?) and
  deliberately left alone: it's an internal naming convention
  (matching `__v32_ret_tmp0` and others), not a portability concern,
  and this entry's own framing here already said so.

## 9. `misc.h` is Vircon32's own header name for `malloc`/`free`/etc.

- **Vircon32 requires**: `#include "misc.h"` for `malloc`, `free`,
  `memset`, `memcpy`, `rand`, `srand`, `exit`, and others.
- **Standard C**: `#include <stdlib.h>` (`malloc`/`free`/`rand`/
  `srand`/`exit`), `#include <string.h>` (`memset`/`memcpy`).
- **Status**: Confirmed -- `misc.h` provided directly (Matthew's own
  copy of Vircon32's standard library source), and `#include "misc.h"`
  compiles clean end to end (`tests/sample17.c` onward).
- **Where**: `codegen_run`'s own `needs_misc` block (codegen.c).
- **For a standard-C mode**: implemented as predicted -- simple string
  substitution, `#include <stdlib.h>` in place of `"misc.h"` under the
  same `needs_misc` condition. Confirmed directly (not assumed) that
  this project's own generated code never emits a `memset`/`memcpy`
  call of its own, so `<string.h>` was never actually needed --
  **IMPLEMENTED**.

---

## 10. The ternary operator is not supported at all, not just spelled differently

- Not a declarator-shape or keyword divergence like every entry above
  -- Matthew reported the real Vircon32 C compiler rejects `cond ? a :
  b` outright, in any form. Unlike every other entry in this list, this
  one isn't about EMITTING the right spelling; it's about not being
  able to emit the construct at all.
- **Standard C**: supports the ternary operator natively; standard-mode
  output keeps `cond ? a : b` exactly as the C++ source wrote it, no
  rewriting at all.
- **Status**: Reported directly by Matthew, not yet independently
  confirmed against the real compiler the way most of this list's
  other entries have been (no test transpiled and compiled end to end
  specifically to trigger the rejection) -- treated as reliable given
  the source, but worth noting the asymmetry with the rest of this
  list's own evidentiary standard.
- **Where**: a new, dedicated lowering phase (`lower.c`, phase 10,
  `rewrite_ternary_*`) rather than a `codegen.c` conditional -- this is
  a genuine AST-level rewrite (ternary expression -> if/else
  statement), not a printing choice, so it couldn't live in codegen.c
  the way every other entry's own fix does.
- **For Vircon32 mode**: **IMPLEMENTED**, with a real, stated scope
  boundary -- only a ternary that is DIRECTLY a var_decl's own
  initializer, DIRECTLY the rhs of a plain `=` assignment to a bare
  identifier, or DIRECTLY a return expression gets rewritten (each of
  these three shapes already has a natural place to put the value, so
  no temporary variable is ever needed). Chained/nested ternaries
  within that same set of shapes (`cond1 ? a : cond2 ? b : c`, a
  common, idiomatic pattern, not a rare edge case -- confirmed via
  `tests/sample58.cpp`'s own `classify`, already in this project's
  suite before this phase existed) are fully unwound via recursion on
  each newly-built branch, not just the outermost level -- a real gap
  caught by actually testing that existing sample, not anticipated
  from the plan alone. A ternary nested any OTHER way -- inside a call
  argument, as part of a larger arithmetic expression, inside a
  for-loop's own clauses, assigned through anything other than a bare
  identifier -- is left completely untouched and will not compile on
  the real Vircon32 toolchain; see `rewrite_ternary_stmt`'s own doc
  comment in lower.c for the full reasoning, including why the
  assignment case specifically needed a narrower check than "any
  assignment" (an arbitrary lvalue duplicated across both branches
  would risk double-evaluating a side effect inside it).

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
