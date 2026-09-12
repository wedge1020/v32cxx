# Design notes and internals

*This is the deep-dive companion to the [project README](../README.md) —
conflict-analysis internals, symbol-table gotchas, and a running log of
what's implemented vs. deliberately deferred. Read the README first if
you're just trying to build and run the thing; come here when you want to
know *why* something is built the way it is, or what to watch out for
before extending it.*

---

## Scope and status

Front end for the C++-subset-to-Vircon32-C transpiler: flex lexer, bison
GLR parser, an AST, and a first-slice semantic-analysis pass. Parses
namespaces; classes with single inheritance, public/private/protected
sections, constructors/destructors defined either in-class or out-of-line
(`Class::method(...) {}`); virtual function declarations; function
overloading (parsed, not yet resolved); qualified names (`v32::Timer`);
and a conventional statement/expression language. `sema.c` then walks that
AST to attach out-of-line definitions to their prototypes, compute a
layout for every class, and assign every function/method a first-cut
mangled name.

Confirmed building and running (bison 3.8.2, out-of-tree `bin`/`obj`/`inc`
layout) — this is past the "I wrote it by hand without a compiler" stage
mentioned in earlier revisions of this file. Ongoing development still
follows the same pattern: change something, rebuild, paste real output
back for the next round, rather than trusting untested guesses.

## Build

```sh
make          # builds bin/v32c++
make test     # builds and parses tests/sample1.cpp, dumps the AST + sema summary
```

The project has since moved to a `src/`/`inc/`/`obj/`/`bin/` layout (see
`Makefile`) rather than the original single `build/` directory — if
you're comparing against an older checkout, that's the diff, not a
regression.

Needs `flex`, `bison` (3.x — GLR has been supported for a long time, but
some directives here want 3.0+), and a C compiler. Note: `%code
requires`/`%code top` are NOT used (see the comment at the top of
`parser.y`) specifically so this also builds on the ancient bison 2.3
frozen into `/usr/bin/bison` on macOS.

## Testing the new features

```sh
./bin/v32c++ tests/sample2_outofline.cpp             # regular/ctor/dtor out-of-line, all succeed
./bin/v32c++ tests/sample3_namespaced_outofline.cpp   # namespaced out-of-line -- see the known gap noted in the file
./bin/v32c++ tests/sample4_bad_syntax.cpp             # deliberate parse error -- confirm clean failure, no crash
./bin/v32c++ tests/sample5_bad_semantics.cpp          # deliberate semantic errors -- confirm both are reported, no crash
```

## The core design idea: lexer-driven qualifier feedback

C++ (like C) needs the *lexer* to know whether an identifier names a type,
so `Foo * x;` can be told apart from the expression `Foo * x;` (multiplying
two variables) — the classic "lexer hack". This project implements it via
`symtab.h`/`symtab.c`: a scoped symbol table the lexer consults on every
identifier it scans (see the `{ALPHA}{ALNUM}*` rule in `lexer.l`).

Qualified names (`v32::Timer`) need the same trick applied across `::`
boundaries — but *not* by having the parser tell the lexer "the next
identifier is qualified" from a grammar reduction. I started down that road
and it's a real trap: bison's lookahead (GLR or plain LALR, doesn't matter)
generally has to fetch the token *after* `::` before it can decide whether
to reduce a `Name ::` prefix, so a parser-side action arms the qualifier
one token too late — the lexer's already scanned and misclassified the
next identifier by the time the action runs.

The fix: the lexer handles the whole thing itself, since it produces
tokens in true scan order. When it scans an identifier, it remembers the
resolved `Symbol*` (`g_last_ident_sym` in `lexer.l`). The instant it then
scans a `::`, it arms `symtab->pending_qualifier` right there, before ever
calling back into the scanner for the next token. See the long comment on
`symtab_arm_qualifier()` in `symtab.h` for the full explanation — it's the
single most important design note in this codebase, worth reading before
you touch the qualified-name handling.

## A postmortem: when "benign conflict" was wrong

**Update:** the previous version of this section (and of `out_of_line_def`'s
constructor rule) claimed the +1 conflict from adding out-of-line
constructors was another instance of the same safe, deferred-by-default-shift
family as the 21 before it. **That was wrong.** It was caught the way
everything in this project has been caught so far: by actually building
and running it, not by re-reading the theory more carefully. `Counter::
Counter(int start) { ... }` — exactly the kind of input the feature was
built for — failed with `syntax error, unexpected '(', expecting COLONCOLON`.

**What actually went wrong:** the constructor rule was written as an
inline `qname_prefix TYPE_NAME '(' ...`, which has the *exact same*
right-hand side (`qname_prefix TYPE_NAME`) as `qualified_type`'s own
production, just as a prefix of a longer rule rather than a complete one.
Two different grammar rules sharing an identical right-hand side, where
one is a strict prefix of the other, is a shape that can make LALR state
construction lose the reachability of an action it should have kept —
which is what happened here: the shift action on `(` was simply absent
from the generated table at the point right after `qname_prefix TYPE_NAME`,
leaving only the path that continues chaining on another `::`. Working
this out convincingly by hand-deriving LALR states (rather than just
running bison) turned out to be genuinely hard and easy to get
confidently wrong about — which is exactly why the original
characterization was accepted too quickly. Confirming a conflict's *shape*
matches a known-benign family is not the same as confirming a specific
concrete input actually parses; this project has now hit that gap once.

**The fix:** `out_of_line_def`'s constructor alternative now reuses
`qualified_type` directly instead of duplicating its production:

```
| qualified_type '(' { ... } opt_param_list ')' block
```

`qualified_type` bundles the whole dotted name into one flat list (e.g.
`[Counter, Counter]`), so the action splits it: the last component is the
constructor's own name, everything before it is the qualifier chain
(matching what the other two `out_of_line_def` alternatives store in `b`).
This reduces the remaining ambiguity to "`qualified_type` used as a return
type" vs. "`qualified_type` followed directly by `(`" — structurally the
*same* fork the var_decl-vs-func_decl/func_def family already relies on
everywhere else in this grammar, which is the one fork actually confirmed
working across many existing test files.

**Verified after the fix:** rebuilding with `bison -Wcounterexamples`
against the corrected grammar reports 22 shift/reduce conflicts — the
*same* total as before the fix, but confirmed to mean something different
this time. Every one of the 22 derivation trees was checked, not just
counted: each routes through either the regular out-of-line method
alternative or the destructor alternative, both already part of the
documented family below. **The fixed constructor alternative
(`qualified_type '(' ...`) does not appear in any of the 22** — meaning
that specific fork is now fully and unambiguously resolved, not merely
passing by coincidence. `%expect 22` is re-pinned in `parser.y`, with the
reasoning above written into the comment right next to it so the next
person (or the next round of this project) doesn't have to redo this
derivation-by-derivation check from scratch.

**But the conflict analysis alone wasn't enough** — and this project
specifically predicted, in an earlier draft of this very paragraph, that
it might not be: `tests/sample2.cpp`'s actual `Counter::Counter(...)`
*still* failed to parse after this fix, with the identical error message
as before. That turned out to be a second, entirely different bug,
unrelated to the grammar — see the next section. The grammar fix above
was necessary but not sufficient, exactly the "necessary evidence, not
sufficient evidence" gap this project keeps re-learning the value of
checking concretely rather than assuming.

## A second, unrelated bug: the missing injected class name

After the `qualified_type` grammar fix above, `Counter::Counter(...)`
*still* produced `syntax error, unexpected '(', expecting COLONCOLON` —
identical to before. Tracing `parser.output` state-by-state for the
corrected grammar showed the tables were actually correct: State 45
(reached after the second `Counter` token, *if* it arrives as `TYPE_NAME`)
correctly defaults to reducing to `qualified_type` on any lookahead other
than `COLONCOLON`, including `(`. So the grammar fix was doing its job —
which meant the bug had to be somewhere upstream of the parser entirely:
in what token the *lexer* was actually handing over for that second
`Counter`.

**Root cause:** the in-class constructor rule (`TYPE_NAME '(' ...` in
`func_header`) never calls `symtab_insert` for the constructor's own
name — it doesn't need to, since it's just reusing the class's already-
registered name for the in-class case. But `Counter::Counter` requires
looking up the *second* `Counter` **inside `Counter`'s own class scope**
(that's what the `::` arms `pending_qualifier` to do) — and `Counter` was
never registered as a symbol inside its own scope. That lookup fails,
falls back to plain `IDENTIFIER`, and the parser correctly rejects it:
bare `IDENTIFIER` can only continue a `qname_prefix` chain expecting
another `::`, which is exactly the "expecting COLONCOLON" error. Real
C++ handles exactly this via what the standard calls the
**injected-class-name**: within its own body, a class's name is
implicitly visible as if it were a member of itself. This symbol table
never implemented that rule.

**The fix** (in `class_decl`'s action in `parser.y`, right after pushing
the class's own scope): insert a second `Symbol` for the class's own
name, this time *into* the class's own just-pushed scope, rather than
only the enclosing one. This is a one-line-of-intent fix with an
outsized effect — it doesn't just repair the out-of-line constructor/
destructor case, it also makes an unqualified, in-class reference to the
class's own name (e.g. a hypothetical `Counter *self;` member) resolve
correctly, matching real C++ semantics generally rather than patching
only the specific broken test case.

**Why this one was harder to find than the grammar bug:** it doesn't live
in the grammar or the generated tables at all, so no amount of re-reading
`parser.output` — however carefully — was ever going to surface it. It
only became visible by trusting the concrete failing output (the same
error, unchanged, after a fix that *should* have addressed it) over the
theoretical analysis, and tracing the actual token classification the
lexer was performing rather than assuming it. Two real bugs found in two
rounds, from two different layers of the same feature (grammar shape,
then symbol-table completeness) — a fair reminder that "the conflict
analysis checks out" and "the concrete input now parses" are genuinely
different claims, and this project has needed both, separately, twice.

## Why `%glr-parser`

`%glr-parser` is declared so the grammar can grow into genuinely ambiguous
C++ declarator territory later (function-pointer types, the "most vexing
parse", `operator` overload declarator syntax) without needing a
parser-generator switch — those are places where C++ grammar is
*genuinely* context-free-ambiguous and `%dprec`/`%merge` actions are the
right tool. Nothing in this grammar exercises real GLR forking today; the
remaining shift/reduce conflicts (`%expect 22`, see above) are all
resolved by bison's default shift preference, confirmed derivation-by-
derivation rather than assumed by pattern-matching this time.

One caveat worth remembering as this grammar grows into genuinely
ambiguous territory: GLR can speculatively explore multiple parses before
disambiguating, so a semantic action that mutates shared state (like
inserting into the symbol table) could in principle fire on a branch that
later gets discarded. Not a problem anywhere in the current grammar (no
real forking happens), but worth auditing for specifically in whatever
region introduces genuine ambiguity.

## Out-of-line member definitions

`out_of_line_def` in `parser.y` handles `RetType Class::method(...) {}`,
`Class::Class(...) {}` (constructor — see the postmortem above for a bug
that shipped in an earlier version of this exact rule and how it was
fixed), and `Class::~Class() {}` (destructor), reusing `qname_prefix`/
`qualified_type` for the qualifier chain. The parser doesn't try to verify
the qualifier names a real class or that a matching prototype exists in
it — it just records the chain on the resulting `AST_FUNC_DEF`'s `b` slot
(see `ast.h`) and moves on. That verification — and actually merging the
out-of-line body onto the right in-class prototype — is `sema.c`'s job
(next section).

**Scope limitation, same as everywhere else in this skeleton:**
single-level qualifiers only. `qname_prefix`/`qualified_type` will parse a
longer chain like `v32::Timer::method` without complaint, but semantic
analysis resolves against a *flat*, bare-name class registry using only
the chain's *last* component — so a namespaced class's out-of-line
definitions aren't correctly scoped to their namespace yet.
`tests/sample3_namespaced_outofline.cpp` demonstrates this working *by
coincidence* (there's only one class named `Timer` in that test) and
spells out exactly where it would break.

## The semantic-analysis pass (`sema.c`)

`sema_run()` walks the AST bison already built and, in order:

1. **Collects every class** into a flat registry, keyed by bare name
   (recursing into namespace bodies — see the scope limitation above).
2. **Attaches out-of-line definitions** onto their in-class prototype:
   resolves the qualifier chain's last component against the registry,
   finds the matching `AST_FUNC_DECL` in that class's member list — by
   name **and** parameter-type signature (`types_equal`/
   `param_lists_match`), so `Vector::Vector(int)` attaches to the right
   constructor even when `Vector()` also exists — and turns it into the
   authoritative `AST_FUNC_DEF` by copying the body over. The original
   top-level duplicate is left in the AST but flagged
   (`FuncSemaInfo.is_out_of_line`) so codegen knows to skip re-emitting
   it. Errors (unknown class, no matching prototype) are reported and
   counted, not fatal — the pass keeps going so you see everything wrong
   in one run, matching how `tests/sample5.cpp` is meant to be read.
3. **Computes a `ClassLayout`** per class: data members and methods split
   out of the raw member list (dropping `AST_ACCESS_SPEC` markers), the
   *resolved* base class node (not just its name string), and — since
   this round — its **vtable**: one slot per distinct virtual method in
   the hierarchy, with an override correctly reusing its base's slot
   (`build_vtable`/`vtable_find_slot` in sema.c) rather than creating a
   second, unrelated one. Base classes are guaranteed to have their own
   layout (and vtable) computed first via recursion in `compute_layout`,
   regardless of which order the top-level walk happens to visit classes
   in — see the comment there for why that recursion can't cycle.
4. **Mangles every function and method** into a name that folds in a
   parameter-type signature — `Counter__Counter__int`,
   `Vector__resize__int_int`, bare-name free functions like
   `clamp__int_int_int` — so overloads (including ones defined
   out-of-line) get distinct names instead of colliding. Destructors are
   special-cased to `Class__dtor__void` rather than splicing in the
   literal `~`, which isn't a legal C identifier character.

Computed results are attached to the AST itself via the new
`AstNode.sema_info` opaque annotation slot (see `ast.h`) rather than a
side table, so anything holding an `AstNode*` after `sema_run()` already
has access to what was computed for it. `sema_dump()` prints a summary in
the same spirit as `ast_dump()` — `main.c` calls it automatically after a
successful parse+sema run.

**Known gaps, deliberately deferred:**

- ~~**Purely syntactic type comparison.**~~ Fixed: `types_equal` (used by
  overload matching, vtable slot matching, and mangling) is now typedef-
  transparent — `void f(int)` and `void f(MyIntTypedef)` correctly compare
  equal, chasing typedef-of-typedef chains via `resolve_typedef_chain` and
  a new flat typedef registry in `sema.c` (a separate mechanism from
  `symtab.c`'s `SYM_TYPEDEF` tracking, which exists only to help the
  *lexer* classify tokens during parsing and doesn't record what a
  typedef actually resolves to). Still name-based, not full semantic
  equivalence — no notion of `const`/cv-qualifiers, since the type
  grammar doesn't parse those at all yet. `tests/sample8.cpp` exercises
  this: a method declared in-class using a typedef, defined out-of-line
  using the underlying type directly, still attaches correctly.
- ~~**No call-site overload resolution.**~~ Fixed: `resolve_call` (in
  sema.c, invoked from inside `check_node`'s `AST_CALL` case — the same
  body walk access-control enforcement already does, not a separate pass)
  picks which candidate a call refers to by name, then arity, then
  (when more than one candidate shares that arity) argument type via the
  same `infer_expr_type` this round generalized `resolve_expr_class`
  into. A single-candidate call resolves on arity alone, without needing
  every argument's type known — deliberate, since most calls in an
  ordinary program aren't overloaded at all, and requiring full argument
  typing even for those would make this far less useful. Genuinely
  overloaded calls fall back to the same best-effort philosophy as access
  control: an argument whose type can't be confidently determined means
  the call is left unresolved, not guessed at. No implicit conversions
  are modeled — argument types must match a candidate's parameter types
  exactly (typedef-transparent, nothing more permissive). A new flat
  free-function registry (grouped, not deduplicated, by name — an
  overload set IS multiple entries sharing a name) supports resolving
  free-function calls, not just methods. Resolution results attach to an
  `AST_CALL`'s `sema_info` as a `CallResolution`; `sema_dump()` gained a
  "call resolutions:" section specifically because a successful
  resolution and a silently-skipped one otherwise look identical from the
  outside (both produce zero other output) — this is the only way to
  positively confirm resolution happened, not just "didn't error".
  `tests/sample11.cpp` exercises method and free-function overloads
  disambiguated by both arity and argument type, and a genuine no-match
  error (a call whose argument count matches none of the candidates).
- **No inherited *data*-member merging.** `ClassLayout.base_class_decl`
  gives you the resolved base class's own `AstNode*` (and, via its
  `sema_info`, its own `ClassLayout`), but nothing walks that
  automatically to build a "flattened" view of "all data members
  including inherited ones." Whoever needs that (the lowering pass
  computing final struct layout) has to walk the base chain themselves.
  (Virtual *methods* don't have this gap — vtable slots ARE carried
  forward across the hierarchy, per point 3 above; it's specifically data
  members that aren't merged.)
- ~~**No access-control tracking.**~~ Fixed: `compute_layout()` stamps
  every member's `access` field as it walks the class body, tracking
  `AST_ACCESS_SPEC` markers and defaulting to `ACC_PRIVATE` when none
  precedes the first member (matching `class`'s C++ default). The
  inheritance access-specifier itself (`: public Base` vs `: private
  Base` vs `: protected Base`) is also captured — a small, closely-related
  parser gap fixed alongside this: `opt_base` used to discard which
  keyword was written, keeping only the base class's name; `protected`
  inheritance wasn't even parseable before (only `public`/`private`
  were). It's carried on `AST_CLASS_DECL.access` now.
- ~~**No access-control ENFORCEMENT.**~~ Fixed, with a bounded scope:
  `sema.c` now has a "calling context" concept (pass 5, run last in
  `sema_run`) that walks every method/function body and checks every
  actual member reference — explicit `.`/`->`, or an implicit `this->`
  via a bare name resolving to an INHERITED member — against the
  accessing class's relationship to whichever class declared it. This is
  a genuinely new kind of thing for this project: the first pass that
  walks into function/method BODIES (not just declarations) and does
  anything with what it finds there, which meant building a first slice
  of expression type inference (`resolve_expr_class` in sema.c) to know
  what class an expression like `b.protectedValue` or a bare
  `secretValue` even refers to. Deliberately best-effort, not sound:
  it resolves `this`, parameters, local variables (flat tracking, not
  properly block-scoped — see `LocalVarType`'s doc comment), and member-
  access/call chains through any of those; anything else (arithmetic
  results, free-function call results, anything it can't pin the type of)
  resolves to "unknown" and is silently skipped — never falsely flagged
  either way, which is the honest choice when the underlying inference is
  partial. Also NOT implemented: the C++ standard's more restrictive rule
  that a derived class can only reach an inherited protected member
  through an object of its OWN (or further-derived) type, not through a
  base-typed reference — this project allows the simpler "any protected
  member of any base" rule instead. `tests/sample10.cpp` exercises legal
  same-class private access, illegal derived-class private access (via
  implicit `this->`, not just explicit `.`), legal protected access from
  a derived class, illegal protected/private access from an unrelated
  class, and a free function with no calling context at all.
- **Vtable slot assignment stops at slot numbers.** `build_vtable`
  resolves which `AstNode` implements each slot for each class and
  assigns indices, but doesn't generate an actual C vtable struct
  definition or dispatch-through-a-function-pointer codegen — that's
  deliberately left for a later lowering/codegen pass to build on top of
  this. Also: only single inheritance is handled (matches this project's
  whole scope, and is what keeps slot assignment this simple — no
  diamond-inheritance slot-sharing puzzles).

## Lowering, phase 1: class-to-struct field layout (`lower.c`/`lower.h`)

The first lowering phase, and the first pass that's genuinely NOT
semantic analysis (checking/annotating what the source means) but
actual transformation toward what the generated C needs to look like.
Given its own file rather than folded into `sema.c`, matching the
project's one-file-per-concern pattern — and matching the project's own
"Suggested next steps" framing of lowering as its own distinct thing.

**What it computes:** for every class, the exact ORDERED sequence of
fields a generated `struct ClassName { ... };` needs — finally
implementing the "no inherited data-member merging" gap that's been
documented (and deliberately deferred) since the vtable-slot-assignment
round. A derived class's fields are the base's fields copied forward as
a literal PREFIX, not nested inside a sub-struct — that's what makes a
`Derived*` safely usable as a `Base*` in the generated C, the same way
it works in real C++. The vtable pointer (when a class has one at all)
is placed first, in whichever class's hierarchy first introduces
virtual-ness — every class further derived from it inherits that SAME
field rather than growing a second one, confirmed by `tests/
sample12.cpp`'s 3-level `Entity`→`Actor`→`Player` hierarchy (vtable
pointer appears once, at `Entity`, and is listed as inherited — not
duplicated — at every level below it) alongside a `Point` class with no
virtual methods anywhere, which correctly gets no vtable pointer field
at all.

**Architecture, deliberately mirroring `build_vtable`'s existing shape**:
`compute_struct_layout` recurses into a class's base FIRST (regardless of
which order the top-level walk visits classes in), for the same reason
and with the same non-cycling guarantee already established for vtable
computation. Results attach to a NEW, SEPARATE annotation slot,
`AstNode.lower_info` — not reusing `sema_info` — specifically because
this phase needs to READ each class's `ClassLayout` (via `sema_info`)
while producing its OWN, different output for the same node; a single
shared annotation slot couldn't hold both at once.

**Precondition:** `sema_run()` must have completed with zero errors
before `lower_run()` runs — `main.c` now only calls it in that case, on
the theory that lowering trusts `ClassLayout`/vtable data completely and
does no validation of its own, so running it against a program sema
already flagged as broken has undefined results, not a graceful
degradation.

**What's NOT done yet, in the order upcoming phases would tackle them:**
actually emitting a `struct` definition as C text (this phase only
computes the layout as a data structure, not any generated syntax);
`this`-injection (turning a method's implicit receiver into an explicit
first parameter); vtable dispatch codegen (turning a virtual call into
an indirect call through the field this phase locates); operator-
overload-to-function-call rewriting; reference-to-pointer rewriting;
`new`/`delete`-to-runtime-call rewriting.

## What's deliberately not here yet

- **Inheritance-aware name lookup at parse/lex time.** `Player : public
  Entity` records the base class name on the AST, and `sema.c` now
  resolves it to an actual `AstNode*` — but the *parse-time* symbol table
  still doesn't walk into the base class's scope when looking things up
  in a derived class. Doesn't break parsing (the lexer just won't
  classify inherited-member names as anything special, which is harmless
  for lexing), but anything besides sema's simple layout computation
  that wants "is this name valid in a derived class" needs its own logic.
- **Overload resolution at call sites.** Overloaded names all land in the
  same symbol-table bucket at parse time; last declaration wins for
  *lookup* purposes. Sema's mangling pass doesn't fix this either (see
  "known gaps" above) — resolving *which* overload a given call expression
  means is still unimplemented.
- ~~**Vtables / virtual dispatch lowering.**~~ Slot assignment done:
  `virtual` is now tracked (`AstNode.ival`, set at parse time and also by
  `sema.c` for silent overrides), and `sema.c`'s `build_vtable` assigns a
  slot per distinct virtual method per class, correctly reusing an
  inherited slot for an override. Still not done: emitting an actual C
  vtable struct or dispatch-through-a-function-pointer codegen — that's a
  lowering/codegen-pass concern building on top of the slot assignment
  that now exists, not something sema.c itself produces.
- ~~**Pointer/reference type wrapping.**~~ Fixed: `pointer_opt` (`*`/`&`)
  is now wrapped onto the declared type as `AST_POINTER_TYPE` /
  `AST_REFERENCE_TYPE` nodes in `var_decl`, `param`, and `typedef_decl`.
  Still only one level deep (`Type **pp` and `Type *&ref` aren't modeled)
  — see the doc comment on `ast_wrap_pointer` in `ast.h` if you need that.
- **The preprocessor.** Lines starting with `#` are silently skipped, no
  macro expansion happens. Decide early whether you want to shell out to
  a real preprocessor as a pass before this scanner runs, or write a
  minimal one — real Vircon32 example code almost certainly uses
  `#include` and `#define`.
- ~~**operator overload declarator syntax**~~ Done: `operator+`,
  `operator==`, `operator[]`, etc. are handled via a new `func_name`
  nonterminal (`IDENTIFIER | OPERATOR operator_symbol`) that drop-in
  replaces `IDENTIFIER` in the exact two spots a function's own name gets
  spelled out (`func_header`'s first alternative, `out_of_line_def`'s
  regular-method alternative) — so both in-class and out-of-line operator
  definitions work, and free-function operators (not just methods) do
  too, since `func_header` is already shared between both contexts.
  Supported: binary `+ - * /`, comparisons, compound assignment,
  assignment `=`, unary `!`, subscript `[]`, call `()`. Unary vs. binary
  `+`/`-` needed no special grammar handling — it falls out of parameter
  count alone, the same as any other overloaded name. Deliberately not
  supported: `<<`/`>>` (no shift-operator tokens exist in this grammar at
  all yet, unrelated to operator overloading specifically), `++`/`--`
  (real C++'s prefix-vs-postfix dummy-`int`-parameter convention is a
  genuine special case, deferred rather than rushed), and conversion
  operators (`operator Type()` — structurally different, no separate
  return-type token the way everything else here assumes). `sema.c`'s
  `mangle()` maps operator names to C-identifier-safe fragments (op_add,
  op_eq, ...), the same treatment it already gives a destructor's "~Foo".
  `tests/sample9.cpp` exercises member and out-of-line operators,
  a free-function operator, and confirms a unary/binary pair sharing the
  same name (`operator-`) mangle to different names. **Conflict count**:
  structural reasoning says this shouldn't change `%expect`'s pinned
  22 at all — `OPERATOR` is a brand-new token that only `func_name`'s
  second alternative ever claims, so it can't create ambiguity with
  anything that was already there. That reasoning hasn't been checked
  against a real `bison -Wcounterexamples` run, though (this project has
  been burned by trusting structural reasoning over real output before —
  see the postmortem above); the pinned `%expect 22` will fail the build
  outright if it's wrong, which is exactly the safety net it's there for.
- **Redefinition diagnostics** — the parse-time symbol table happily lets
  you redeclare things; `sema.c` adds its first two real diagnostics
  (unknown out-of-line qualifier, missing prototype) but there's plenty
  of room for more (e.g. redeclaring a class, conflicting types on a
  redeclared variable).

## Suggested next steps, roughly in order

1. ~~Compile it, fix whatever bison/flex complain about.~~ Done — building
   cleanly on bison 3.8.2 with zero warnings.
2. ~~Add a couple of deliberately-failing `.cpp` test files.~~ Done —
   `tests/sample4.cpp` (parse error) and `tests/sample5.cpp` (semantic
   errors, two in one file).
3. ~~Out-of-line member definitions.~~ Done — see the section above (and
   its postmortems: two real bugs found and fixed via actual test runs,
   not just grammar review).
4. ~~Start the semantic-analysis pass.~~ Done, first slice.
5. ~~Parameter-type-aware mangling + overload-aware out-of-line
   matching.~~ Done — `tests/sample6.cpp` exercises two overloaded
   constructors and two overloaded methods, confirming each gets a
   distinct mangled name and attaches to the right prototype.
6. ~~Vtable slot assignment for `virtual`.~~ Done — `tests/sample7.cpp`
   exercises inheritance, an override that doesn't repeat `virtual`, and
   a new virtual method introduced in a derived class.
7. ~~Typedef-transparent type comparison.~~ Done — `tests/sample8.cpp`'s
   `Box::setWidth` is declared in-class using a typedef (`Meters`) and
   defined out-of-line using the underlying type (`int`) directly,
   confirming they're still recognized as the same signature.
8. ~~Access-control tracking~~ (plus, found along the way, a small but
   closely-related parser gap: inheritance access-specifiers were parsed
   and discarded, and `protected` inheritance wasn't parseable at all).
   Done.
9. ~~`operator` overload declarator syntax.~~ Done — `tests/sample9.cpp`
   exercises member operators, out-of-line operator definitions, a
   free-function operator, and a unary/binary pair sharing a name.
10. ~~Access-control ENFORCEMENT~~, including a first "calling context"
    concept and the first slice of expression type inference this
    project has had. Done, deliberately best-effort (see the "Known
    gaps" entry above for exactly what it can and can't resolve).
    `tests/sample10.cpp` exercises legal/illegal private and protected
    access across an inheritance hierarchy, including an unrelated class
    and a free function with no calling context at all.
11. ~~Call-site overload resolution.~~ Done, built directly on the
    expression type-inference core (`infer_expr_type`, generalized from
    access control's `resolve_expr_class`) — extending it from "what
    class does this expression resolve to" into "what is this
    expression's full type" (including primitives), then using that to
    pick which overload a given call's arguments actually match. Tackled
    AFTER access control, not alongside it, specifically so any bug in
    the shared type-inference core would get caught by access control's
    tests first, before a second feature was built on top of it.
    `tests/sample11.cpp` exercises method and free-function overloads
    disambiguated by arity and by argument type, and a genuine no-match
    error — written this round, not yet confirmed against a real build.
12. **Lowering, tackled in bite-sized phases now that semantic analysis
    is solid enough to build on:**
    1. ~~Class-to-struct field layout.~~ Done — `lower.c`/`lower.h`,
       `tests/sample12.cpp` exercises a 3-level hierarchy confirming the
       vtable pointer is introduced exactly once and correctly inherited
       (not duplicated) further down, plus a class with no virtual
       methods at all getting no vtable pointer field.
    2. Next: `this`-injection (an implicit method receiver becomes an
       explicit first parameter).
    3. Then: vtable dispatch codegen (a virtual call becomes an indirect
       call through the field phase 1 locates).
    4. Then: operator-overload-to-function-call rewriting,
       reference-to-pointer rewriting, `new`/`delete`-to-runtime-call
       rewriting.
13. Remaining natural next candidates, independent of the lowering track
    above: the preprocessor gap (see the project README — a custom
    `v32pp` is the long-term plan, with `cpp` as a stopgap in the
    meantime).
14. Only after enough of the lowering phases above: the Vircon32 C code
    generator itself, pretty-printing whatever the lowering passes
    produce into actual `.c` text — the actual stated goal of this whole
    project.
