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
- **For a standard-C mode**: IMPLEMENTED, in a follow-up round.
  Solved differently than `print_type` alone could manage: a new
  `print_type_and_name` (codegen.c) builds the ENTIRE standard-mode
  declarator (return type, name, any array dimensions, parameter
  list) as one self-contained unit, at each of the two call sites
  that can ever reach a function-pointer-typed declaration
  (`print_var_decl_inline`, `emit_struct`'s own field loop -- and,
  through it, `emit_unions` too -- confirmed directly that
  `ast_wrap_func_ptr` is only ever called from `var_decl`'s own
  grammar, and `var_decl` is what both a variable declaration and a
  class member reduce to; no parameter or return type can ever be
  function-pointer-typed in this grammar). Array-of-function-pointers
  composition works too (`ReturnType (*name[N])(Params);`, standard
  C's own syntax for it), confirmed directly against
  `tests/sample66.cpp`'s own output. `emit_vtable_struct` and
  `emit_vtable_instance` (their own inline declarator/cast text,
  never routed through `print_type` at all) got the identical fix
  applied separately, confirmed against `tests/sample14.cpp`'s own
  vtable output. No grammar changes were needed for any of this --
  every part of the fix lives in codegen.c.

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
- **For a standard-C mode**: IMPLEMENTED alongside #3 above, in
  `emit_vtable_instance`'s own cast-insertion branch -- standard C's
  own function-pointer cast has an EMPTY `()` where a declarator's
  own name would go (there's no name to give a cast), `(ReturnType
  (*)(Params))expr`, confirmed directly against `tests/sample14.cpp`'s
  own output.

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
- **Status**: Reported directly by Matthew, and independently
  CONFIRMED by an actual Vircon32 compiler run: a ternary nested inside
  a call argument (`add(x > y ? x : y, 1)`) transpiled with the literal
  `?`/`:` characters still in it, and the real Vircon32 C compiler
  rejected it with "character '?' is not a valid identifier start" --
  its own lexer doesn't even recognize the character, confirming this
  is a hard, unconditional rejection, not merely a parser-level one.
- **Where**: a new, dedicated lowering phase (`lower.c`, phase 10,
  `rewrite_ternary_*`) rather than a `codegen.c` conditional -- this is
  a genuine AST-level rewrite (ternary expression -> if/else
  statement), not a printing choice, so it couldn't live in codegen.c
  the way every other entry's own fix does.
- **For Vircon32 mode**: **IMPLEMENTED**, now covering every position a
  ternary can appear in, in two tiers. A ternary that is DIRECTLY a
  var_decl's own initializer, DIRECTLY the rhs of a plain `=`
  assignment to a bare identifier, or DIRECTLY a return expression is
  rewritten with NO temporary variable at all (each of these three
  shapes already has a natural place to put the value). Chained/nested
  ternaries within that same set of shapes (`cond1 ? a : cond2 ? b :
  c`, a common, idiomatic pattern, not a rare edge case -- confirmed
  via `tests/sample58.cpp`'s own `classify`, already in this project's
  suite before this phase existed) are fully unwound via recursion on
  each newly-built branch, not just the outermost level. Every OTHER
  position -- inside a call argument, as part of a larger arithmetic/
  subscript/member expression, assigned through anything other than a
  bare identifier -- is now ALSO handled, by hoisting the ternary into
  its own freshly-declared temporary (`__v32_tern_tmpN`), set via an
  ordinary if/else inserted immediately before the current statement;
  confirmed end to end against `tests/sample71.cpp`'s own `add(x > y ?
  x : y, 1)`, which now transpiles with zero `?`/`:` characters
  anywhere in the output and, run in standard mode, computes the
  correct value (11). Two narrow boundaries remain, stated plainly
  rather than silently missed: a ternary inside a for-loop's own init/
  cond/incr clauses (restructuring the loop itself, e.g. into an
  equivalent `while`, isn't attempted); and one reachable only through
  a brace-less single-statement slot (`if (cond) foo(cond2 ? a : b);`
  with no block around it) -- inserting a preceding temp declaration
  needs a real statement list to splice into, which only a block
  provides. See `hoist_ternaries_in_expr`'s own doc comment in
  `lower.c` for the full reasoning, including why the direct-assignment
  shape specifically still needs a narrower check than "any
  assignment" (an arbitrary lvalue duplicated across both branches
  would risk double-evaluating a side effect inside it -- the generic
  hoist sidesteps this entirely, since it introduces a fresh temp
  rather than duplicating the original lvalue).

---

## 11. Parameters and return values must be exactly one word -- no by-value structs/unions/arrays larger than that

- **Vircon32 requires**: a function's own parameters and return value
  must each be exactly one word in size. A struct, union, or array
  larger than one word cannot be passed or returned BY VALUE at all --
  a pointer to it must be used instead (matching this project's own
  existing choice for every method's own receiver, `ClassName *this`,
  already pointer-based for an unrelated reason). Reported directly by
  Matthew, quoting Vircon32's own documentation: "functions cannot use
  parameters or return values of size different from 1. That is: they
  cannot use arrays, unions or structures (unless their size is just a
  single word). Instead they must operate with pointers to them."
  Passing an array BY DECAYING TO A POINTER (`void foo(int arr[8])`,
  already how this project's own `param` grammar treats an array
  parameter -- see entry #2 above) is explicitly fine, same as
  standard C; the restriction is specifically about passing/returning
  a fixed-size AGGREGATE (struct/union/array) as a genuine, by-value
  copy.
- **Standard C**: no such restriction -- an ordinary struct, union, or
  (via a wrapping struct, since C itself doesn't allow a bare array
  parameter or return type either) array of any size can be passed or
  returned by value.
- **Status**: Reported directly by Matthew, quoting Vircon32's own
  documentation -- not yet independently confirmed against the real
  compiler by transpiling and compiling a deliberately-oversized
  by-value parameter or return type to see the specific error Vircon32
  produces, the same evidentiary gap entry #10 (ternary) already has.
- **Where**: `lower.c`'s `check_word_sizes_classes`/`check_word_sizes_
  free_functions`, run immediately after `compute_struct_layouts`
  (needs the field counts it computes) but before anything else in
  `lower_run` -- a warning-only pass, not a transformation, so it
  doesn't need a numbered phase slot of its own.
- **For a standard-C mode**: no change needed -- this restriction is
  Vircon32-specific, matching entry #1's own "vircon32 mode is the one
  that needs the extra treatment" shape; the check itself is gated on
  `g_target == TARGET_VIRCON32` at each call site.
- **IMPLEMENTED**, once Matthew confirmed the numbers this entry's own
  first version said were missing: Vircon32 is a 32-bit, word-based
  machine (1 word = 32 bits), and `int`/`float`/every pointer are
  already exactly one word -- crucially, `char`/`short`/`double`/etc
  (recent-compiler aliases) are "mere syntactic sugar" over the same
  4-byte word underneath, so EVERY field of ANY supported primitive
  type is exactly one word, with no per-type size table needed at
  all. A class/struct's own size in words is therefore just its
  `StructLayout`'s own field count (data members plus a vtable
  pointer, if any) -- `sema_warning` (exposed from sema.c, not static
  anymore, so lower.c can reuse the same counting/formatting machinery
  rather than duplicating it) fires when a bare (not pointer, not
  reference -- `const`-qualified still counts) class/struct parameter
  or return type has more than one such field.
  A real, if narrow, gap remains: an array-typed or nested-struct-
  typed DATA MEMBER only ever contributes ONE to its own `StructLayout`
  count here, even though it may itself be several words wide, so a
  struct with exactly one such field could still under-count as
  "one word" when it's actually more -- the same conservative
  direction (miss a violation rather than warn on valid code) as
  every other best-effort check in this project. Unions are
  deliberately not checked at all: an ordinary union of simple
  primitive members is already exactly one word by construction
  (members overlap, not stack), so only a union containing an array or
  nested-struct member large enough to itself exceed one word would
  violate this -- narrower still than the struct gap above, and not
  pursued this round.
- **A genuinely valuable finding from testing this against the
  existing suite, not invented for the occasion**: `tests/sample9.cpp`
  and `tests/sample15.cpp` (both exercising operator overloading on a
  `Vector2D` class -- `x`/`y`, two `int` fields, two words) both
  triggered this warning repeatedly. Matthew asked for these to be
  corrected. Originally PARTIALLY fixed: every `Vector2D` PARAMETER
  took `const Vector2D &` instead of by value, eliminating that half of
  the warnings, but the RETURN side couldn't be fixed at all -- see
  entry #12 below, the real, separate, previously-undiscovered gap this
  correction attempt is what surfaced (no function anywhere in this
  grammar could return a pointer or reference type). NOW FULLY fixed,
  once entry #12 closed: both samples were rewritten again to return
  `Vector2D *` (heap-allocated via `new`) from every value-producing
  operator, eliminating the by-value-return warning entirely rather
  than just documenting it as a known limitation.

## 12. No function can return a pointer or reference type at all -- FIXED

- Not a Vircon32-specific quirk -- a general, previously-undiscovered
  gap in this project's own grammar, surfaced while trying to fix
  entry #11's own `sample9`/`sample15` findings by having a value-
  producing operator overload return a pointer instead of a multi-word
  struct by value.
- **Confirmed directly**: `int *getPtr(int x) { return &x; }` and
  `int &getRef();` (inside a class) both used to fail to parse --
  `func_header`'s own grammar (`type_spec func_name '(' ...`) had no
  `pointer_opt` between the return type and the function name at all,
  unlike `var_decl`/`param`, which both already did. This applied to
  EVERY function in this grammar, not just operators -- an entirely
  ordinary `Shape *makeShape()` failed the same way. `out_of_line_def`
  had the identical gap in its own, separate grammar production.
- **Fix**: `pointer_opt` added to both `func_header`'s and
  `out_of_line_def`'s first alternatives, wrapping the return type via
  `ast_wrap_pointer`/`ast_wrap_reference` exactly like `var_decl`
  already does. `%expect` actually went DOWN, from 26 to 25 -- verified
  with a real `bison -d`/`bison -v` run (not assumed): giving
  `out_of_line_def` its own `pointer_opt` right after `type_spec`,
  matching `var_decl`, means the two productions now agree on shifting
  through `pointer_opt` first, eliminating a pre-existing 1-shift/
  reduce fork between them rather than adding a new one. See the
  comment on `%expect` in `parser.y` for the full, bison-verified
  reasoning.
- **Lowering, three parts**, all in `lower.c`:
  1. A reference RETURN needs the same implicit "take the address"
     treatment a reference PARAMETER already gets at its call site --
     `inject_reference_return_address_stmt` wraps `return expr` in
     `address_of_if_needed` whenever the enclosing function's return
     type is `AST_REFERENCE_TYPE`, run in phase 3/4 (before phase 5
     ever relabels `AST_REFERENCE_TYPE` to `AST_POINTER_TYPE`, the same
     ordering requirement the existing reference-parameter fix already
     documents).
  2. `fix_references_in_method`/`fix_references_free_functions` (phase
     5) now also relabel the FUNCTION'S OWN return type from
     `AST_REFERENCE_TYPE` to `AST_POINTER_TYPE` -- previously this
     phase only ever touched parameter/local types, never a function's
     own return type.
  3. A reference-returning CALL now gets an explicit dereference
     inserted at its use site (`finalize_calls_expr`'s `AST_CALL`
     case) -- the symmetric case to (1): a C++ reference return acts
     like the referent itself, so `int x = obj.getRef();` needs a
     `*` inserted around the (now pointer-returning) call, or it would
     assign the address instead of the value. Caught by actually
     compiling generated code, not guessed at (see below).
- **Two adjacent, pre-existing bugs found and fixed along the way**,
  both surfaced only once this round finally had a working bison+flex
  toolchain available to actually COMPILE generated output, not just
  parse-check it:
  - `address_of_if_needed` only recognized `AST_POINTER_TYPE` as
    "already pointer-like, don't add `&`" -- not `AST_REFERENCE_TYPE`.
    Since phase 3/4 runs before phase 5 relabels references to
    pointers, a bare reference PARAMETER forwarded as another
    reference-typed argument (e.g. `addThem(const Vector2D &a, const
    Vector2D &b)` computing `a + b`, resolving to `operator+(const
    Vector2D &other)`) got a wrongly-inserted extra `&`, producing a
    double pointer (`const Vector2D **` where `const Vector2D *` was
    expected) -- a real compile error, caught by actually building
    `tests/sample15.cpp`'s generated C. Fixed by also treating
    `AST_REFERENCE_TYPE` as "already pointer-bound" in
    `address_of_if_needed`.
  - A leftover from this same investigation, left open at the time --
    **since FIXED, in a later round, after an actual Vircon32 compiler
    run turned it from a gcc warning into a real bug report**: this
    project still doesn't MODEL const-correctness on a method's own
    `this` receiver (no error for calling a non-const method through a
    const reference in the first place -- see entry on `const` in the
    README's own "what doesn't exist yet" list), but forwarding a
    `const Shape &`/`const Shape *` as a non-const method's receiver
    now gets an explicit const-stripping cast inserted
    (`Shape__area__void((Shape *)s)`) rather than a plain, uncasted
    pointer assignment. gcc only ever gave this a
    `-Wdiscarded-qualifiers` WARNING (`const struct Vector2D *` passed
    where a plain `struct Vector2D *this` is expected); the real
    Vircon32 compiler rejects it OUTRIGHT ("cannot assign const struct
    Shape* to struct Shape*: discards const qualifier"), a hard type
    error, confirmed directly against `tests/sample68.cpp`'s own
    `getArea(const Shape &s) { return s.area(); }`. Fixed in
    `cast_receiver_if_needed` (`lower.c`): the cast this function
    already inserted for a base/derived class MISMATCH is now ALSO
    inserted whenever the object being forwarded is const-qualified but
    the target method's own injected `this` isn't (a const method
    receiving a const object needs no cast at all -- both sides already
    agree, checked via the target's own `this`-parameter type,
    `receiver_type_is_const`). This is the honest fix given this
    project's own deliberate choice not to enforce const-correctness --
    it makes the permitted-but-unchecked case actually COMPILE, the
    same way an explicit `const_cast` would in real C++, rather than
    starting to reject code this project has never rejected before.
- **Verified end-to-end**, not just parse-checked: with a real
  bison+flex toolchain built from source in this round's own sandbox
  (flex wasn't previously available here; see `docs/DESIGN_NOTES.md`),
  `tests/sample9.cpp`, `sample15.cpp`, and the new, dedicated
  `tests/sample72.cpp` (a non-operator pointer/reference-return test:
  an in-class + out-of-line pointer-returning method, a free function
  returning a pointer, and a reference-returning method) all transpile
  cleanly, and their generated C compiles with plain `gcc` with zero
  errors. `sample72.cpp` was additionally compiled and RUN directly
  (`--target=standard`), producing exactly the expected output
  (`viaPointerMethod=7 viaFreeFunction=7 viaReference=7`).
- **Where**: `func_header`/`out_of_line_def` (`parser.y`);
  `inject_reference_return_address_stmt`, `finalize_calls_in_method`,
  `finalize_calls_free_functions`, `fix_references_in_method`,
  `fix_references_free_functions`, `finalize_calls_expr`'s `AST_CALL`
  case, and `address_of_if_needed` (`lower.c`).
- **Impact on entry #11 above**: this closes the reason `sample9`/
  `sample15` could only be partially corrected -- both now return
  `Vector2D *` (heap-allocated via `new`) instead of a multi-word
  struct by value, avoiding the word-size warning entirely, and both
  are rewritten to do so as part of this fix.

---

## 13. Empty structs are rejected outright -- FIXED

- **Vircon32 requires**: every `struct` definition to have at least
  one member. A class with no data members and no vtable of its own --
  a method-only "verb" class with no state, such as a `friend`-granted
  accessor/helper class (`tests/79sample.cpp`'s own `BoxPrinter`) --
  otherwise lowers to a genuinely empty `struct BoxPrinter {\n};\n`.
- **Standard C**: an empty struct is a well-known, silent gcc extension
  -- accepted with no warning at all even under `-Wall -Wextra`, which
  is exactly why this project's own `--target=standard` + gcc
  verification sweeps never caught it; only a real Vircon32 compile
  run did.
- **Status**: Reported directly by the user, quoting the real
  compiler's own error verbatim: `structures must have at least 1
  member`.
- **Where**: `emit_struct` (`codegen.c`).
- **Fix**: when a class's own `StructLayout` has zero fields (no data
  members, no vtable pointer), `emit_struct` inserts a single unused
  `char __v32_empty_struct_pad;` placeholder field -- never referenced
  by name anywhere else in the generated code, purely there to give
  the struct one member. Applied in BOTH target dialects, not gated to
  Vircon32-mode only: a struct that's valid in one of this project's
  own output modes and not the other, for a difference the user never
  wrote themselves, would be a confusing, purely accidental divergence
  between the two.
- **IMPLEMENTED** for both `--target=vircon32` and `--target=standard`.

---

## 14. Right shift does not sign-extend negative values

Worth  noting for  your  quirks doc:  "shift-right  does not  sign-extend
negative values"  is a  new Vircon32 C  entry in the  same family  as the
arg-staging bug — and this  one is particularly sneaky because standard
C  makes  >> on  negative  ints  implementation-defined, so  gcc-verified
output  can silently  diverge  from  Vircon32 behavior.  If  you want,  a
--target=vircon32 lowering  rewrite (expanding  x >>  n on  signed values
into  the mask-and-subtract  form)  would be  the  v32c++-side fix,  same
pattern as the ternary rewrite. ---

---

## 15. Types must be declared before use

types must be declared before use — an enum used before its declaration
fails with a  misleading expecting COLONCOLON error,  because the lexer's
TYPE_NAME classification  is registration-order dependent."  A friendlier
sema/lexer  diagnostic (e.g.  "GameDifficulty  used before  declaration")
would make this much easier to spot than the parser error.

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
