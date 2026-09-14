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

## Lowering, phase 2: this-injection

Turns a method's implicit receiver into an explicit first parameter
(`ClassName *this`), and rewrites every reference to it — `this` itself,
an explicit `this->x` (unchanged, already in the target form), an
implicit bare identifier that meant `this->x`, and an unqualified call to
another method (`foo()` meaning `this->foo()`) — into one consistent
explicit form. After this phase, a method body has no implicit member
access left in it anywhere.

**Reuse over duplication:** this needs to answer exactly the same
question access-control enforcement already had to answer — "is this
bare name a local/parameter, or does it refer to a member (possibly
inherited)?" — so rather than writing a second implementation of that
logic in `lower.c` and risking it drifting out of sync with the original
over time, `find_member_in_hierarchy`, `find_local`, and the
`LocalVarType` struct were pulled out of `sema.c`'s `static` scope and
exposed via `sema.h` instead. Same known limitation as before: local
variable tracking is flat, not properly block-scoped — a variable's name
correctly shadows a same-named member for as long as it's "in scope" by
this pass's simplified accounting, which is what `tests/sample13.cpp`'s
`getValue()` (a local `value` shadowing the member `value`) specifically
exercises.

**Mechanism:** unlike the read-only walks so far (`check_node` for access
control/overload resolution), this pass genuinely MUTATES the AST —
`rewrite_expr`/`rewrite_stmt` take `AstNode **` (a pointer to the SLOT
holding a node — `&n->a`, an element of a param/arg list, ...) rather
than `AstNode *`, specifically so a node can be swapped out for a
different one entirely (an `AST_IDENT` becoming an `AST_MEMBER`, an
`AST_THIS` becoming an `AST_IDENT` named `"this"`), not just have its
own fields edited in place.

**A real mutation hazard, worth remembering:** `this_inject_method`
mutates `method->list` (the parameter list) in place, prepending the new
`this` parameter. That's safe for this project's CURRENT pipeline
ordering, because `sema_run()` has already finished and cached everything
it computed (mangled names, vtable slots) as plain data before
`lower_run()` ever touches the AST — but it does mean `sema_run()` must
never be invoked again on an AST that's already been through
this-injection. Signature-matching logic like `attach_out_of_line`'s
would see the injected `this` parameter and misbehave. Not a live concern
today (one parse, one `sema_run()`, one `lower_run()`, done), but exactly
the kind of ordering invariant that's easy to violate by accident if this
project ever grows an incremental or re-analysis mode later.

## Lowering, phase 3: vtable dispatch codegen (call finalization)

Rewrites every call's callee into its final, codegen-ready form: a
virtual method call becomes `obj->vtable->FIELD(obj, args...)`; a
non-virtual method call becomes a direct call to the mangled function
name (with `obj` prepended as the first argument); a free-function call
becomes a direct call to its own mangled name.

**Driven by `CallResolution`, not re-derived independently:** this needs
to know exactly which method/function a call refers to, which is exactly
what sema.c's call-site overload resolution already computed — and did
so more precisely (arity- and argument-type-matched) than re-deriving it
via `find_member_in_hierarchy` alone would (name-only, first-found). So
this phase reads `call->sema_info` (the `CallResolution` sema attached)
as its source of truth, rather than re-resolving the callee from scratch.
A call sema couldn't resolve is left completely untouched here too — same
best-effort philosophy as everywhere else in this project: better to
leave something for a human (or a future pass) to notice than to guess
and possibly generate wrong code silently.

**The one genuinely new piece of design: field-name stability.** A
vtable's whole point is that the SAME field, at the SAME position, means
the same thing (the same virtual method) regardless of which class's
object you're actually holding at runtime. That means the field name
used at a call site must come from whichever class ORIGINALLY declared
the slot (the base-most one), never from whichever override the call
happens to resolve to. `VtableEntry` gained a new field for this:
`canonical_method`, alongside the existing `method` (which class
currently implements the slot). `build_vtable` propagates
`canonical_method` forward unchanged on every override — only `method`
changes when a derived class overrides; `canonical_method` stays fixed to
whoever introduced the slot in the first place. `tests/sample14.cpp`
exercises this directly: `Circle::describeTwice()` calls the virtual
`area()`, which resolves to `Circle::area` (its own override, correctly
respecting real C++ name-hiding) — but the vtable field name used at that
call site is still `Shape__area__void`, Shape's mangled name, since
Shape is where the slot was born. Getting this wrong (using the resolved
override's name instead) would have generated a vtable struct with a
DIFFERENT field per class instead of one shared field — defeating the
entire mechanism.

**A postmortem: a real bug in this exact mechanism, caught by real test
output.** The first version of this phase shipped with virtual calls
silently failing to lower at all — `this->area()` stayed exactly as
this-injection left it, no vtable indirection, no prepended argument —
while non-virtual calls (`describe()`) worked fine. The tell was in
`tests/sample14.cpp`'s actual output: `Circle::describeTwice`'s call to
the virtual `area()` still showed `Member str1=-> str2=area`
(the ORIGINAL method name) instead of the expected two-level
`vtable`/`Shape__area__void` chain, with no `this` prepended to the
argument list either.

Root cause: `resolve_expr_class` (exposed from `sema.c` specifically so
this phase could reuse it) depends on `find_class`, which reads a class
registry that `sema_run()` builds during `collect_declarations` — and,
in the version that shipped, also FREES at its own completion, before
`lower_run()` ever runs. So `type_to_class` → `find_class("Shape")`
silently returned NULL (the registry was already torn down), which made
`resolve_expr_class` return NULL, which made `finalize_call`'s own
best-effort design correctly (by its own logic) bail out on "couldn't
determine the slot" — except the underlying reason wasn't legitimate
ambiguity, it was a dependency that had already been deallocated.
Non-virtual calls never hit this path at all (that branch only reads an
already-cached `FuncSemaInfo`, no registry needed), which is exactly why
the bug was selective rather than a hard crash or a total failure — the
kind of shape that's easy to miss without checking real output for the
SPECIFIC case (virtual, from an overriding class) that actually exercises
the broken path.

The fix: `sema_run()` no longer frees the class/typedef/free-function
registries at its own end — their useful lifetime now genuinely extends
past `sema_run()` itself, since `lower_run()` depends on them too. A new
`sema_cleanup()` is exposed instead, and `main.c` calls it once, after
BOTH `sema_run()` and `lower_run()` have finished, making the extended
lifetime an explicit part of the pipeline's contract rather than an
implicit assumption that happened to break the moment a sema-internal
helper got reused across the sema/lowering boundary. Worth remembering
generally: exposing a `static` helper for reuse across a pass boundary
(as this project has now done several times — `find_member_in_hierarchy`,
`find_local`, `resolve_expr_class`) means auditing not just what the
function COMPUTES, but what state it silently DEPENDS ON still being
alive at the point it gets called from its new caller. The computation
itself was correct the whole time; the bug was entirely about lifetime.

## Lowering, phases 4–6: operators, references, new/delete

Rather than three separate write-ups, these are grouped here since two of
them turned out to share the same important caveat, and the third is
explicitly a placeholder rather than a real feature.

**Phase 4 (operator overloads) closed a real, previously-unnoticed gap**:
sema.c has supported *declaring* an operator overload since the
operator-overload-syntax round, but nothing anywhere resolved natural
operator syntax (`a + b`) into a call to `operator+` — `sema_run()`'s
overload resolution only ever looked at explicit call syntax. This phase
is where that gets resolved for the first time, by construction: it
builds the equivalent `AST_CALL` (member-operator form checked first,
respecting name-hiding; free-function form as a fallback) and hands it
straight to phase 3's `finalize_call`, reusing every bit of dispatch
logic rather than re-implementing any of it. **The asymmetry worth
remembering**: because this resolution happens at lowering time instead
of during `sema_run()`, it doesn't get the same treatment a regular call
does — no "no matching operator overload" diagnostic on a genuine
mismatch (the expression is just silently left as a plain built-in
operation instead), and no access-control check for a private/protected
operator invoked somewhere it shouldn't be. Both real gaps, both
documented rather than silently present; moving this resolution earlier,
into `sema_run()` itself, would close them properly — future work.

**Phase 5 (references) has a subtle correctness trap it deliberately
avoids**: it needs its own, separate local-tracking seed function rather
than reusing phase 3's, specifically because if reference detection and
the `AST_REFERENCE_TYPE` → `AST_POINTER_TYPE` mutation happened during
phase 3's (earlier) pass, phase 5 running afterward would see the
already-mutated pointer type and incorrectly conclude a parameter was
never a reference at all. Keeping this phase's detection and mutation
together, in one place, after every earlier phase has left
`AST_REFERENCE_TYPE` untouched, avoids that staleness trap. `tests/
sample16.cpp` exercises the actual distinction that matters: a reference
parameter's `.` access becomes `->`, a plain by-value parameter's `.`
access stays `.`.

**Phase 6 (new/delete) is explicitly a placeholder, not a lowering**:
`new T` becomes a call to a per-type stub (`v32_new_T`); `delete expr`
becomes a call to a generic stub (`v32_delete`). Neither invokes a
constructor or computes a real allocation size. Two things block a
faithful version, discovered while scoping this phase rather than
assumed away: there's no `sizeof` AST representation anywhere in this
project, and — more fundamentally — `parser.y`'s `NEW type_spec`
production has *never* supported constructor arguments; `new Foo(1, 2)`
isn't even parseable today. This phase exists so the AST has some
concrete, C-shaped call here rather than an unlowerable `AST_NEW`/
`AST_DELETE` surviving into codegen; a real runtime library and the
grammar fix for constructor arguments are both tracked as future work,
not silently assumed solved by this phase's existence.

**What's NOT done yet**: actually emitting any of the above as C text —
every phase so far, 1 through 6, only produces a data structure or a
mutated AST, never generated syntax. That's the Vircon32 C code
generator's job, still ahead, and now has a genuinely complete lowered
AST to work from.

## A postmortem: the same dump bug, twice, in two different files

`lower.c`'s `dump_this_injected_methods` — the function behind the
"fully lowered method bodies" section — only ever walked `AST_CLASS_DECL`
entries, printing their methods. It had no branch for a top-level
`AST_FUNC_DEF` at all. For any test file consisting entirely of free
functions (`tests/sample16.cpp`, `tests/sample17.cpp`), that section
printed nothing whatsoever, even though the actual lowering underneath
had almost certainly run correctly — only the DISPLAY was broken. For a
mixed file (`tests/sample15.cpp`), it silently dropped every free
function (`addThem`, `checkEqual`, `getElement`, the free `operator*`,
`scaleIt`) while still showing the two methods, which is exactly the
free-function operator-overload path the test existed to demonstrate.

This is the same *category* of bug as the `dump_call_resolutions` fix
from a couple rounds earlier (`sema.c`) — a dump function iterating
`AstList *decls` at the top level and forgetting that "top-level
declaration" includes genuine free functions, not just classes and
namespaces — just missing the branch entirely this time, rather than
missing the `n->b == NULL` guard on an existing one. Caught by the same
method both times: real test output showing something that didn't match
what the code should have produced, not by reasoning about the code in
the abstract.

Given this happened twice, this round included a full audit of every
`decls`-walking function in both `sema.c` and `lower.c` — every pass that
genuinely needs to (`collect_declarations`, `mangle_free_functions`,
`access_check_free_functions`, `dump_decls`/`dump_call_resolutions`,
`finalize_calls_free_functions`, `fix_references_free_functions`,
`new_delete_rewrite_free_functions`) already has the free-function branch
correctly guarded with `n->b == NULL`. `dump_this_injected_methods` was
the only remaining gap, now fixed the same way.

## A third bug: comparing a count across a mutating-phase boundary

`resolve_operator_use` (phase 4) checked a member operator's arity via
`member->list.count == expected_params`. That's correct for a
prototype-only member (`AST_FUNC_DECL`) — but wrong for one with a body
(`AST_FUNC_DEF`), because by the time phase 4 runs, phase 2
(this-injection) has already prepended `this` onto every `AST_FUNC_DEF`'s
parameter list (it explicitly does NOT touch `AST_FUNC_DECL` — see
`this_inject_method`'s early return for anything that isn't
`AST_FUNC_DEF`). So a member operator WITH a body has a `list.count` one
higher than what it was declared with; one WITHOUT a body doesn't.

`tests/sample15.cpp` exposed this precisely: `operator+` and `operator==`
both have out-of-line bodies and were silently left as plain `BinOp`
nodes (arity check failed, fell through to the free-function fallback,
found nothing, gave up silently — exactly the "best-effort, no false
positive" design working as intended, just on a false premise).
`operator[]` has no body in that test and happened to work — not because
the logic was right for it, but because it was never this-injected in
the first place, so its count was never off. Same underlying bug, one
symptom visible, one accidentally masked — which is a genuinely
misleading way for a bug to present, and exactly why "operator[] worked,
so the mechanism is probably fine" would have been the wrong conclusion
to draw from a partial test result.

Fixed with `effective_param_count()`: `AST_FUNC_DEF` subtracts one (the
injected `this`) before comparing; anything else compares its raw count
directly. Audited every other `list.count ==` comparison in both files
afterward — the free-function fallback in this same function is safe
(free functions are never this-injected at all), and both remaining
occurrences in `sema.c` run entirely inside `sema_run()`, before
this-injection has touched anything. This was the only place actually
comparing a count on the wrong side of that mutation.

**The general lesson, worth naming since this project now has multiple
compiler passes that mutate the AST in place**: any comparison against a
count, an index, or anything else that a LATER pass might change needs
to be checked against exactly what state that data is in at the point
the comparison actually runs — not what it was when the comparing code
was written, and not assumed uniform just because it usually is. This is
a different flavor of the same discipline as the sema-registry-lifetime
bug from the vtable-dispatch round: reusing logic (or, here, a plain
struct field) across a pass boundary means auditing what's true on both
sides of that boundary, not just what the value meant where it was first
defined.

**Verified fixed against real output**, not just plausible-looking code:
`tests/sample15.cpp` now shows `addThem`'s `a + b` and `checkEqual`'s
`a == b` both correctly rewritten to `Call(Vector2D__op_add__Vector2D,
[a, b])` and `Call(Vector2D__op_eq__Vector2D, [a, b])` respectively —
the two cases that were silently broken before. Combined with
`getElement`'s subscript and the free `scaleIt`'s `operator*`, already
confirmed correct in an earlier round, every operator form this phase
supports (member binary, member subscript, free binary) has now been
checked against actual output, not just read as plausible code.

## Closing two documented gaps: operator diagnostics, and `new`'s arguments

Two gaps flagged clearly (not silently) when they were first introduced
got closed this round, together, because closing one made closing the
other substantially easier.

**Operator-overload resolution moved from lower.c into sema_run()
entirely.** When operator resolution was first built, it had to live in
`lower.c` because nothing else existed yet to resolve natural operator
syntax (`a + b`) into a call at all. That came with a real, explicitly
documented cost: no `sema_error()` on a genuine mismatch (a class-typed
operand with no matching operator was just silently left as a plain
built-in operation), and no access-control enforcement for a resolved
member operator. Both are closed now: `resolve_operator_use` lives in
`sema.c`, runs during `check_node`'s normal walk, and shares the exact
same matching/diagnostic core (`resolve_overload_generic`, extracted from
what used to be `resolve_call`'s own body) that a regular call already
used. A mismatch is now a real "no matching overload" error; a resolved
member operator now goes through `check_member_access()` exactly like an
explicit member call would.

This had a genuinely nice side effect, not just a diagnostics win:
sema_run() runs entirely BEFORE this-injection (a lower.c-only concern),
so the operator-arity comparison bug from a couple rounds back — where
`member->list.count` was one too high for any method that had already
been this-injected — can't happen here anymore, by construction, not by
remembering to call a workaround function. `effective_param_count()` is
gone from `lower.c` entirely; there's no timing hazard left for it to
guard against. `lower.c`'s phase 4 shrank to almost nothing: it just
checks whether sema already attached a `CallResolution` (now carrying an
`is_member` flag, since lowering still needs to know whether to prepend
the receiver as an explicit argument) and, if so, builds the equivalent
`AST_CALL` and hands it to `finalize_call` — no name lookup, no arity
checking, no candidate collection happens in `lower.c` anymore at all.

**`new T(args)` — the grammar gap — is fixed.** `parser.y`'s
`unary_expr` rule gained `NEW type_spec '(' opt_arg_list ')'` alongside
the existing bare `NEW type_spec`, reusing the same `opt_arg_list`/
`arg_list` nonterminals an ordinary call already uses. `AST_NEW` now
carries `list=constructor arguments` (empty for both `new T` and
`new T()` -- this project doesn't distinguish default- from
value-initialization). A new `resolve_new_expr` in `sema.c` resolves
which constructor overload a `new T(args)` refers to -- a constructor is
just a same-named (`ClassName`) member, so `collect_method_candidates`
already finds it correctly -- reusing the exact same
`resolve_overload_generic` core operators and regular calls now share.
An arity/type mismatch is a real sema-time error instead of nothing at
all having been checked.

**What this does NOT yet mean**: `new T(args)` still doesn't allocate a
real `sizeof(struct T)` or invoke the resolved constructor -- `lower.c`'s
phase 6 is still an explicitly-labeled placeholder (see that phase's own
doc comment), now just accepting and forwarding the (already resolved
and arity-checked) arguments to `v32_new_TypeName` rather than
discarding them. The two remaining blockers to a truly faithful `new`
lowering are unchanged: no `sizeof` AST representation, and no actual
constructor-invocation codegen yet. Both remain future work, now with
one fewer excuse (the grammar) standing in front of them.

**A side effect worth naming, since it came up while wiring this in**:
adding a `list` to `AST_NEW` meant every tree-walking pass that used to
treat `AST_NEW` as "an argument-free leaf, nothing to recurse into" had
to be checked -- and one of them (`lower.c`'s phase 2, `rewrite_expr`)
had an explicit comment claiming exactly that ("these can't contain a
`this` or a bare member reference"), which was true right up until this
change made it false (`new Foo(x, this->y)` very much can). Found and
fixed as part of this same round, not discovered later, specifically by
re-checking every switch statement in both files that dispatches on
`AST_BINOP`/`AST_UNOP`/etc. for a missing `AST_NEW` arm -- the same kind
of audit the `dump_this_injected_methods` postmortem already established
as worth doing whenever a node's shape changes, not just when a pass is
new. That same audit also caught `sema.c`'s `dump_calls_in_node` (the
function backing the "call resolutions:" dump section) not printing a
successful resolution for anything except `AST_CALL` -- meaning the two
new resolution paths this round added (operators, constructors) would
have been invisible in that dump even when they worked correctly,
exactly the "silently skipped and successfully resolved look identical"
failure mode that section's own doc comment warns about. Fixed in the
same pass, before it could ship as a fourth instance of the same mistake.

**Verified against real build output.** No `%expect` mismatch -- the new
`NEW type_spec '(' opt_arg_list ')'` grammar alternative introduced zero
new shift/reduce conflicts, so `%expect 22` needed no change.
`tests/sample18.cpp` (`new Point(3, 4)`) resolves to
`Point__Point__int_int` and lowers to `v32_new_Point(3, 4)` with both
arguments correctly forwarded. `sample19.cpp` (wrong constructor arity)
produces exactly `'Point' expects 2 argument(s), but 1 were given`.
`sample20.cpp` (private operator called from outside its class) produces
exactly `'operator+' is a private member of class 'Secret' and cannot
be accessed here`. The full existing suite (samples 1-17) shows zero
regressions -- every lowered body is byte-for-byte identical to before
this round's refactor, confirming the simplified `lower.c` phase 4
produces the same output as the old, more complex version it replaced.

A genuine bonus, not just "no regression": `sample15.cpp`'s `call
resolutions:` dump now shows all four operator resolutions AT SEMA TIME
(previously empty for these, since resolution used to happen only during
lowering) -- direct confirmation the architecture change works, and that
the `dump_calls_in_node` fix (printing a resolution for BinOp/Assign/
Subscript/Unop, not just Call) is correct. And `sample17.cpp` (written
in an earlier round, predating constructor-argument support, using a
bare `new Widget` with no parens) now ALSO shows a resolved constructor
call (`Widget__Widget__void`) with zero changes to that file -- a bare
`new Widget` has an implicitly-empty argument list, which correctly
matches Widget's declared 0-arg constructor. Good sign the design
generalizes rather than only handling the case it was written for.

## Code generation begins: struct and vtable-struct-type emission

A new `codegen.c`/`codegen.h` module -- the first piece of an actual
Vircon32 C code generator, as opposed to lowering (which only ever
produced a transformed AST, never generated syntax). Wired into
`main.c`, printing straight to stdout after lowering succeeds, same
"---- section header ----" convention every other pass uses.

**Deliberately narrow scope for this first round**: top-level typedefs,
each class's vtable struct TYPE (if it has any virtual methods), and each
class's own struct definition. NOT yet in scope: method/function BODY
emission, and vtable STATIC INSTANCE emission (the type exists for a
class's `vtable` field to point at; no actual populated instance of it
does yet). Both are substantial enough to deserve their own round,
verified against real output the same way every phase before this one
has been.

**Two Vircon32-specific quirks this module has to get right, neither of
them standard C:**
1. A `struct Name { ... };` definition is auto-typedef'd by Vircon32's
   compiler under its own tag name; every subsequent REFERENCE to that
   type must be bare (`Name *next;`) -- `struct Name *next;` is a compile
   ERROR there, not just redundant. Definitions still use the `struct`
   keyword; only references drop it. `print_type()` is the one function
   every other part of this module funnels type output through,
   specifically so this rule only has to be gotten right in one place.
2. Array declarators put the length before the name (`int [8] myarray;`),
   reversed from standard C -- but there's genuinely nothing to apply this
   to yet: this project's grammar has no array-type declarator at all
   (only `AST_SUBSCRIPT`, the `a[i]` *expression*, exists). Recorded in
   codegen.h's own doc comment so it isn't lost, rather than built
   speculatively against a type shape nothing can produce yet.

**A design wrinkle worth naming, because it's the same class of hazard
this project has hit twice already**: a vtable struct type's function-
pointer field needs `ClassName *` as its receiver parameter, but
`canonical_method` (the slot's original declarer) might be prototype-only
(this-injection never touches those -- see phase 2's early return for
anything that isn't `AST_FUNC_DEF`) or might already have a `this`
parameter injected, depending entirely on whether it happens to have a
body. Handled by reconstructing the receiver parameter explicitly every
time (via a new `find_declaring_class`, walking the class's own ancestry
by pointer identity to find whichever class actually owns
`canonical_method`) rather than assuming either shape -- exactly the kind
of "a count/shape differs depending on whether a mutating pass has
already touched it" check the operator-arity bug (a few rounds back)
should have gotten from the very start.

**The forward-reference-ordering question is resolved -- tested against
the real compiler, not guessed at.** Matthew ran a small probe file
through the actual Vircon32 toolchain and reported back the full error
progression, not just pass/fail, which settled several things at once:

- A standalone forward declaration (`struct Node;`, no body) is valid
  Vircon32 syntax on its own.
- **`struct Name` is NEVER valid as a type-USE expression, in any form --
  this is stricter than originally assumed.** `struct Node *first;`
  (pointer field, keyword form) failed with "expected a type" even
  though `Node` was already forward-declared. Once the keyword was
  dropped (`Node *first;`), that same line compiled fine. And it isn't
  only about pointers: `struct Container c;` (an ordinary variable
  declaration, keyword form) failed with "expected '{'" -- as if the
  parser were treating `struct Container` as the start of ANOTHER
  declaration and choking on finding `c` next. `struct` is apparently
  only meaningful in the two declaration forms themselves (a forward
  declaration or a full definition); every other reference, in every
  other context, needs the bare name -- and that's true starting from
  the FIRST forward declaration, not only once the full definition has
  appeared.
- Two more real quirks surfaced along the way, neither previously known:
  assigning the literal `0` to a pointer fails ("cannot assign int to
  ... pointer"; Vircon32 wants `NULL` specifically), and `main()` must
  be declared `void main()` with no return value at all -- there's no OS
  to return to on Vircon32; the cartridge itself is the whole running
  system. The `NULL`-vs-`0` rule matters for whenever a codegen phase
  starts generating pointer-initializing code (nothing does yet); the
  `main()` rule matters for whenever this project generates an actual
  program entry point (also not yet in scope).

`codegen.c` now emits a forward declaration for every class before
anything else (`emit_forward_declarations`), which resolves the general
case completely rather than merely working by source-order coincidence
for every test file so far -- two classes holding pointers to each
other, or simply one preceding another it points to, no longer depends
on luck.

**Verified against the full 20-sample suite, zero regressions.**
Forward declarations appear correctly ordered (before typedefs and
struct bodies) in every sample that reaches codegen; every sample that
should skip codegen (a parse or semantic error) correctly shows no
generated-C section at all. `sample12` (a 3-level hierarchy) confirmed
vtable-struct accumulation works correctly across more than one level,
not just two; `sample14` confirmed the canonical-field-naming rule holds
even for the simplest possible override case. No new bugs found in this
round.

## Method/function body emission, and fixing the receiver-cast risk it surfaced

`codegen.c` now emits actual C statement/expression text for every
method and free function that has a body -- `print_stmt`/`print_expr`
(every fully-lowered statement and expression kind, since lowering has
already reduced the AST to something C-shaped) plus a shared
`emit_function_header` for prototypes and definitions alike. Full
reasoning, including a real risk found by tracing through by hand and
then actually fixed once new information made that possible, lives in
`codegen.h`'s own doc comment (kept there rather than duplicated here,
since that's where anyone touching this code will actually look first)
-- summarized:

- **Parenthesization**: every binary/assignment/prefix-unary expression
  gets unconditional parens, rather than this project reconstructing
  C's precedence table and risking getting one operator's binding wrong.
  Noisier output, zero precedence bugs -- the right trade for a first
  pass.
- **Prototype-only methods** (declared, never defined) are deliberately
  not emitted at all -- there's no body, and this-injection never
  touches them, so they'd have no receiver parameter to work with
  anyway. If one is ever actually called, the generated C fails to
  *compile*, not merely to link.
- **A real risk found by hand-tracing, and fixed**: a call to an
  inherited method -- virtual OR non-virtual, tracing showed both are
  affected -- was passing its receiver argument through exactly as
  this-injection typed it (the calling class's own receiver type), with
  no cast, even when the callee's own declared receiver type was an
  ancestor's. Concretely, `Circle::describeTwice` (`tests/sample14.cpp`)
  passes a `Circle *` into both the inherited virtual `area()` (via the
  vtable) and the inherited non-virtual `describe()` (direct call),
  where each callee's declared receiver is `Shape *`. This got fixed,
  not just flagged, once Matthew confirmed against the real compiler
  that Vircon32 accepts an explicit C-style cast (`(Node *) 0` compiled
  fine) -- that confirmation was the missing piece; a fix attempted
  before knowing casts were even supported would have been another guess
  dressed up as a solution. `lower.c`'s `finalize_call` now inserts an
  explicit cast on a receiver argument whenever the callee's actual
  declaring class (via a new, shared `find_declaring_class` in
  `sema.h` -- previously a codegen.c-only helper, now used by both files
  for the same underlying reason, so it lives in one place instead of
  two that would eventually drift) differs from the caller's own static
  receiver type. A new `AST_CAST` node kind carries this through to
  codegen. Verified by hand against `tests/sample14.cpp`'s exact call
  sites, including confirming the fix does NOT over-apply:
  `Shape::describe`'s own call to `area()`, where caller and callee's
  declaring class are the same, correctly gets no cast. Still needs an
  actual compile to confirm this reasoning holds -- reasoning correctly
  through a mechanism isn't the same thing as a confirmed working build,
  and this project has a consistent practice of not calling something
  resolved until real output says so.
- **A separate, adjacent gap**: this project has no special handling for
  a user-defined `main` at all -- it gets mangled like anything else
  (`main__void`), so generated C has no actual `main` entry point, and
  nothing enforces Vircon32's `void main()` requirement on the C++
  source either. Needs an actual design decision, not a quick fix.

## First real compile attempt: two genuine bugs found, one non-issue

Matthew hand-compiled `tests/sample14.cpp`'s generated output against the
real Vircon32 compiler for the first time this round -- exactly the kind
of test this project has relied on throughout, and it caught two real
bugs the cast-fix round's reasoning-only verification couldn't have.

**Bug 1 -- Vircon32's function-pointer declarator syntax is not standard
C's.** `emit_vtable_struct` was emitting `ReturnType (*name)(ParamTypes);`
(the ordinary C form), which is exactly what failed to compile
(`expected a type`). Vircon32 wants `ReturnType(ParamTypes)* name;`
instead -- confirmed by Matthew hand-converting the struct and
compilation getting past that point -- matching the pattern in
Vircon32's own documented function-pointer example,
`void()* Action = &DoSomething;`. Fixed in `emit_vtable_struct`; the
receiver-reconstruction logic underneath (which parameter needs to be
skipped depending on whether this-injection already touched
`canonical_method`) was untouched, since that was never the broken part.

**Bug 2 -- a real, confirmed gap in prototype emission, not a narrow
edge case.** `doubleIt` (declared in `tests/sample14.cpp`, deliberately
never defined there) produced `identifier "doubleIt__int" has not been
declared` -- because this project's codegen only ever emitted a
prototype for something that ALSO got a body here. Given Vircon32
has no multi-file compilation model at all (an entire program becomes
one big file via `#include`, per Matthew), a declared-but-undefined
function is an entirely ordinary, expected pattern -- meant to be
satisfied by something `#include`d from elsewhere, not a mistake to work
around. Fixed for both free functions (`emit_function_prototypes_free_
functions` now also handles a bare `AST_FUNC_DECL`, no this-injection
concern at all since that never applies to free functions) and methods
(a new `emit_method_prototype`, reconstructing the receiver parameter
directly from `class_decl`'s own name when handed a prototype-only
`AST_FUNC_DECL` method -- simpler than `find_declaring_class`'s ancestor
walk, since iterating a class's own methods list already tells you
which class it belongs to). `tests/sample1.cpp`'s `Timer` constructor/
destructor/`getTicks` (declared, never defined, never called) now also
get prototypes as a side effect -- harmless, since nothing calls them,
but worth knowing generated output volume goes up slightly across the
whole suite, not just for this file.

**Non-issue, already fixed on this end**: the `-Wswitch` warning on
`AST_CAST` and the AST dump showing `?` instead of `Cast` both trace to
the same stale build -- `ast.c`'s `kind_name()` already has
`case AST_CAST: return "Cast";` in every file sent since the cast-fix
round. Re-sent this round to make sure the full, current file is what
gets built against, rather than a hand-patched or partially-synced copy.

**Noted, not acted on**: Vircon32's lack of multi-file compilation (one
`#include`-assembled file per program) is directly relevant context for
the still-open `main()` design question from the body-emission round --
there's genuinely only one `main` across an entire program on this
target, which narrows the design space once that gets tackled, but
doesn't change the fact that it still needs an actual decision, not a
quick fix bundled in here.

## Second compile attempt: `main()` finally handled, plus a gap found via a near-mistake

`tests/sample14.cpp` still didn't compile after the previous round's
fixes -- two new errors, both genuinely informative about how Vircon32
actually works, not just codegen bugs to patch quietly.

**`main` is not declared.** The deferred `main()` design question from
two rounds back stopped being deferrable -- it was now the thing
actually blocking a real compile. Fixed properly, not partially:
`sema.c`'s `mangle()` special-cases a top-level (never a method) free
function literally named `main`, keeping it unmangled -- and, critically,
`codegen.c` ALSO forces its printed return type to `void` regardless of
what the C++ source declared (commonly `int main()`), and strips any
`return expr;` inside it down to `expr; return;` (evaluating the
expression as a statement, for whatever side effects it might have,
before a bare `return;`). All three pieces were necessary together --
fixing only the name would have traded "main not declared" for "returns
a value from a void function" the moment `tests/sample2.cpp`'s
`int main() { return 0; }` got rebuilt. `print_stmt` now threads a
`strip_return_value` flag through every recursive call (a `return` can
be arbitrarily nested inside `main`'s own if/while/for/block structure,
and nothing about a bare `AST_RETURN` node says which function it
belongs to) -- deliberately not solved by mutating the AST during
lowering instead, since this is entirely about how Vircon32's `main` in
particular needs to be PRINTED, not a transformation any other consumer
of the lowered AST would ever need.

**`doubleIt` "declared but not fully defined."** A stricter, more
informative version of the earlier "not declared" error -- Vircon32
does whole-program analysis with no separate compile-then-link step, so
a bare prototype for a function that's genuinely never defined anywhere
in the compiled unit isn't tolerated at all, unlike ordinary C where a
matching prototype is enough at compile time and an unresolved body only
fails later, at link time. This is a `tests/sample14.cpp` completeness
issue, not a codegen bug -- `doubleIt` was deliberately prototype-only
there, to test call resolution without needing its own implementation,
which is a perfectly fine thing for OUR sema/lowering tests to check but
not something Vircon32 itself will ever compile standalone.

**Fixing that test file surfaced a genuine, previously-undiscovered gap,
caught before it shipped rather than after -- and now actually fixed,
not just worked around.** The natural fix -- add
`int doubleIt(int x);` followed later by `int doubleIt(int x) { ... }`,
an entirely ordinary C++ pattern -- would have registered TWO separate
entries for the same free function (nothing in this project pairs a free
function's prototype to its own later definition the way
`attach_out_of_line` does for methods). `resolve_call` would then see two
identically-shaped candidates for every call to `doubleIt()` and report
it as ambiguous. Worked around in the test file itself at the time (a
single combined declaration, no separate prototype), with the real fix
tracked here as a known gap rather than lost.

**Fixed properly once Matthew confirmed the compile succeeded and asked
about it directly.** `register_free_function` (sema.c) now checks, before
adding a new registry entry, whether an existing entry already shares the
SAME name and the SAME parameter signature -- if so, this is the "other
half" of a prototype-then-definition pair, not a genuine second
candidate, and no new entry gets added; if the two disagree on which one
has a body, the entry gets upgraded to whichever one is `AST_FUNC_DEF`
(the definition is more useful to lowering/codegen, which need an actual
body). `tests/sample14.cpp`'s `doubleIt` reverted back to the natural,
separated declare-then-define form as a real test of this (restoring it
was itself a verification step, not just a cosmetic revert), and a new,
deliberately narrow `tests/sample21.cpp` isolates the exact scenario --
a declared-then-separately-defined free function, called alongside a
second, unrelated free function -- apart from everything else
`sample14.cpp` also happens to exercise. A small, unrelated duplicated
doc comment above `param_lists_match` (pre-existing, noticed while
working in this exact area) was cleaned up in the same pass.

**Confirmed compiling cleanly** after the `main()`/`doubleIt`-body fixes
from the previous round (Matthew's report, before the free-function
dedup fix above). The only remaining warning is `-Wsign-compare` in
flex-generated `lexer.c` -- pre-existing, not something this project's
own source controls, and not a concern.

**One more Vircon32 quirk confirmed, needing no code change**: a
function prototype needs its parameter NAMES specified, not just types
(unlike standard C, where `int foo(int, int);` is valid on its own).
`emit_function_header` and `emit_method_prototype` (codegen.c) have
always printed a name for every parameter in every prototype they emit,
for both methods and free functions alike -- there was never a code path
that printed a bare, unnamed parameter type to begin with, so this one
turned out to already be correct by construction rather than needing a
fix. Confirmed rather than assumed, the same as everything else in this
file -- worth writing down precisely because it so easily could have
gone the other way.

**Verified, and one cosmetic follow-up.** `tests/sample21.cpp` confirmed
the actual fix works -- `call resolutions:` showed `square__int` and
`addOne__int` both resolving cleanly, no ambiguous-overload error -- but
the test file itself was missing a `main()` (an oversight, fixed), which
is what actually made `sample21.c` fail to compile, not the fix under
test. Separately, `codegen.c`'s free-function prototype walker still
printed an identical, duplicate prototype line for a declare-then-define
pair (harmless in C -- confirmed, `sample14.c` compiled cleanly with the
same duplication -- but needless clutter in a project whose generated
output doubles as teaching material). A small `SeenNames` tracker in
`emit_function_prototypes_free_functions` now skips a mangled name once
its prototype has already been printed once.

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

## Future CLI/design considerations

Started as a list of "eventually, not now" ideas from the first codegen
round; `-o` and `-c` (see below) have since actually been implemented,
so this section is now a mix of done and still-future -- kept together
rather than split, since the still-future items build directly on what's
now in place.

**A "standard C mode" command-line flag -- still future.** As codegen
leans further into Vircon32-specific quirks (no `struct` keyword on a
type reference, and eventually the reversed array-declarator order once
array-type support exists), it's worth being able to switch that
behavior off and emit plain, portable C instead -- for testing against a
normal C compiler, or for anyone who wants to retarget this project's
output elsewhere. This would need, at minimum: (a) emitting `struct Name`
(with the keyword) on every type reference, not just the definition,
since standard C doesn't auto-typedef; (b) once array-type support
exists, emitting the ordinary `T name[N];` declarator order instead of
Vircon32's `T [N] name;`. Not urgent -- there's exactly one target
today -- but worth keeping `print_type()` (codegen.c) and whatever
eventually handles array declarators structured so a mode flag can
cleanly select between the two forms, rather than the Vircon32-specific
choices being hardwired in a way that's painful to unwind later.

**Verbosity levels (`-v`, `-vv`, `-vvv`) -- still future, deliberately
NOT touched this round.** `v32c++`'s current behavior (AST dump,
semantic-analysis summary, struct layouts, fully-lowered method bodies,
generated C -- all unconditional, every run) is explicitly valuable
as-is for the course this project is teaching material for: seeing every
stage of the pipeline is the point, not a debug-mode side effect to be
trimmed away once codegen matures. Matthew's own shape for this, stated
directly: `-v` shows top-level actions only; `-v -v`/`-vv` shows more/all
of what's unconditionally shown today; a further `-vvv` split if a clear
enough division exists once this actually gets built. The full, verbose,
every-stage-visible output should remain the default (or at minimum
trivially available) regardless of how this ends up structured --
whatever eventually implements this needs to avoid accidentally making
the teaching-relevant output an opt-in afterthought. A genuinely bigger
restructuring than `-o`/`-c` (touches every dump function in main.c, not
just the two new flags added this round), which is exactly why it's
still deliberately deferred to its own round rather than folded in here.

**`-o <file>` -- IMPLEMENTED this round.** `main.c` now accepts
`-o output.c` (via `getopt`) to write generated Vircon32 C directly to a
file instead of interleaving it into the "generated Vircon32 C" dump
section on stdout -- `codegen_run()` already took a `FILE *` rather than
assuming stdout, so this needed no changes to codegen.c itself, only to
main.c's own driving logic.

**`v32c++` requiring `main()` by default, with `-c` to opt out --
IMPLEMENTED this round, superseding the "must never require main()"
framing from an earlier version of this section.** That earlier framing
was importantly half right and half wrong: the underlying TRANSPILATION
capability genuinely never has needed `main()` to exist and still
doesn't -- `sema_run()`, `lower_run()`, and `codegen_run()` have no
concept of "main" as anything other than an ordinary (if specially-
mangled) free function, and passing `-c` produces byte-identical output
to what every version of this tool has always produced. What changed is
main.c's own DEFAULT CLI behavior: unless `-c` is passed, it now checks
(`sema_program_has_main`, sema.c/sema.h -- a query, not a validation
sema_run() itself performs or cares about) for an actual top-level
`AST_FUNC_DEF` named `main`, and refuses to proceed to lowering/codegen
if none exists, printing an error suggesting `-c` instead. Matches a
real compiler's own `-c` ("compile only," no need for a complete,
linkable program) closely enough that the flag name was reused directly
rather than inventing a new one.

**This existing test suite is exactly the "-c" scenario, and needed
updating to reflect that.** Every sample file except `sample2.cpp`,
`sample14.cpp`, and `sample21.cpp` (the only three with a genuine
`main`) would have started failing `make test` immediately once the
new default landed -- not a regression in those files, just the new
default correctly recognizing what these files have always actually
been: focused unit tests of one specific compiler feature each, never
intended as complete, standalone-compilable programs. The Makefile's
`test` target now passes `-c` to every sample except those three.

**A future multi-file / `#include`-oriented transpile mode -- still an
idea, not a plan, and NOT the same thing as the `-c` flag above.**
Matthew's original framing of this (raised alongside the `-c` idea, in
the same message) was closer to a genuine "produce output meant to be
`#include`d into something else" mode -- which `-c` as actually
implemented doesn't do; it only disables the main-required check, it
doesn't change what gets emitted at all. That richer idea -- something
that actually shapes generated output for the "assemble a whole program
from several `#include`d pieces" workflow Vircon32's own lack of
multi-file compilation implies -- remains genuinely unbuilt and
undesigned. Filed as a real possibility worth having in mind for later,
not a commitment, and not to be confused with the `-c` flag that now
exists but does something narrower.

**C++ input syntax should stay ordinary C++, regardless of what Vircon32
itself needs on the output side.** An explicit, important principle for
whenever array-type support and function-pointer types get added to the
grammar: the C++ SOURCE this project accepts should use normal,
unsurprising declarator syntax (`int arr[8];`, `ReturnType (*name)
(ParamTypes);`) -- never Vircon32's own reversed/quirky forms. Codegen's
job is the translation from normal C++ syntax to whatever Vircon32
actually wants on output (the reversed array-bracket order, the
`ReturnType(ParamTypes)* name;` function-pointer form already confirmed
and implemented for vtable struct fields) -- never the other way around,
and never by making the accepted C++ input itself quirky to match the
target. This is the same principle the "standard C mode" flag idea above
already points at from a different angle (being able to swap Vircon32's
output quirks for portable C without touching how source gets written or
parsed) -- worth keeping in mind together as this project's two type-
system-adjacent fronts (arrays, function pointers) actually get built,
not just the codegen-output side of each.

## `v32cxx.h`, and `--version`

A new `src/v32cxx.h` -- project-wide identifying information (`VERSION`,
`AUTHOR`, `URL`) plus a place for build-time configuration constants,
modeled directly on the sibling `v32lua` project's own `v32lua.h`.
Deliberately NOT structured the same way as `v32lua.h` (which every file
in that project includes) -- this project's headers are already more
narrowly scoped (`ast.h`, `sema.h`, `lower.h`, `codegen.h`, `driver.h`,
`symtab.h` each own one specific piece), so only what actually needs
something declared here includes it: `main.c` (for `--version`) and
`symtab.h` (for `SYMTAB_BUCKETS`, relocated there from its own previous
home as a small, real demonstration of the pattern rather than leaving
the file otherwise empty). `VERSION` follows `v32lua`'s own
YYYYMMDD-plus-same-day-sequence-plus-`-dev` scheme for consistency
between the two sibling projects -- there was no prior version string to
preserve compatibility with, so `20260914-dev` is a starting point, not
a convention this project is locked into.

`main.c` now parses `--version` via `getopt_long` (switched from plain
`getopt`, which only handles short options) and prints output matching
`v32lua --version`'s own format exactly:
```
v32c++ 20260914-dev
C++ Transpiler for Vircon32 (v32c++) by Matthew Haas
  github: https://github.com/wedge1020/v32cxx
```
`--version` is deliberately long-option-only, with no `-v` short alias --
a bare `-v` is reserved for the still-future verbosity-level flag (see
the CLI-considerations section above), and giving `--version` a `-v`
alias now would collide with that the moment it gets built.

## A real bug found by a genuinely simple hand-written test

Matthew wrote a small, deliberately simple `sprite.cpp`/`sprite2.cpp`
pair (a `Player` class with a constructor and two setter methods,
exercised via `new Player()` in one version and a plain stack-allocated
`Player sprite;` in the other) specifically to see how the transpiler
handled something outside the existing test suite's own coverage. Two
real compile errors came back, and they turned out to be two entirely
different kinds of problem, not one.

**`sprite.c` (the `new Player()` version): `v32_new_Player` undeclared.**
Exactly the already-documented placeholder gap -- `new` still doesn't
invoke a constructor or link to any real allocator, and never claimed
to. Not a new finding, not fixed here; genuinely needs the constructor-
invocation work this round didn't attempt.

**`sprite2.c` (the stack-allocated version): a real, previously-
unexercised bug, now fixed.** `Player__setx__int(sprite, 320)` was
passing `sprite` -- a plain `struct Player` VALUE -- where the function
declares `Player *this`. Vircon32 correctly rejected it ("cannot assign
struct Player to ... struct Player*"). Root cause: `finalize_call`
(lower.c) has always assumed the object expression in a method call is
already pointer-typed -- true for `this` itself (this-injection
guarantees it), true for a `new`-allocated pointer, true for a
reference-turned-pointer parameter -- but never checked, because every
existing test's method calls happened to already satisfy that
assumption. A method called on a genuinely stack-allocated value
(`Player sprite; sprite.setx(320);`) was never exercised by this
project's own test suite until this hand-written one, and nothing had
ever needed the address-of that case actually requires.

Fixed with a new `address_of_if_needed` in `finalize_call`: uses a
newly-exposed `infer_expr_type` (sema.c/sema.h -- distinct from
`resolve_expr_class`, which deliberately unwraps the pointer/value
distinction away for its own purposes and so can't answer this
question) to check whether the object expression's OWN declared type is
already a pointer; if not, wraps it in an `AST_UNOP`/`"addr"` node
(reusing the exact shape this-injection and codegen already handle for
`&expr` elsewhere -- no new AST kind needed). Defaults to leaving the
expression untouched when the type can't be determined at all, rather
than guessing -- wrongly adding `&` to something already pointer-typed
would produce a double pointer, a strictly worse outcome than leaving
whatever already-known bug reaches that branch in place.

**Traced by hand for `sprite2.cpp`'s exact case, then confirmed against
the real compiler**: `sprite.setx(320)` -- `infer_expr_type` finds
`sprite` declared as bare `Player` (not a pointer) -- wraps it in
`&sprite` -- `target_class == obj_class` (no inheritance involved) so
no cast gets added on top -- final call:
`Player__setx__int((&sprite), 320)`. `sprite2.c` now compiles cleanly
on the actual Vircon32 toolchain (Matthew's report) -- reasoned through
correctly AND verified, not just the former. Also confirmed the fix
doesn't over-apply: every existing sample's method calls all use an
already-pointer receiver (`this`, `new`-allocated, or reference-turned-
pointer), so `infer_expr_type` returns `AST_POINTER_TYPE` for every one
of them and `address_of_if_needed` returns the original expression
completely unchanged -- generated output for the full existing suite
should be byte-identical to before this fix.

**Important: this does NOT make `sprite2.cpp` fully correct, only fixes
the pointer-type mismatch.** `Player sprite;` still never calls
`Player::Player()` -- `sprite.x`/`sprite.y` are still uninitialized
stack garbage before `setx(320)`/`sety(180)` overwrite them (which,
notably, is exactly what this particular test's own logic happens to do
immediately afterward, so THIS specific program would likely behave
correctly at runtime even without constructor invocation -- but only by
coincidence, not because the gap is closed). The constructor-invocation
work is still the next real piece, not resolved by this round.

**A separate, small sync issue, worth naming for the pattern rather than
the specific incident**: the `infer_expr_type` exposure (removing
`static` from its definition in sema.c, adding the matching prototype to
sema.h) built and compiled cleanly here, but the person's own build
failed with "static declaration follows non-static declaration" --
their `sema.h` had picked up the new prototype while their `sema.c`
still had the old `static` definition, from files having been applied
individually across a couple of rounds rather than as a matched pair.
Resolved by overwriting both files wholesale rather than hand-patching.
Not a code bug -- worth remembering as a recurring category of failure
mode this project has now hit more than once (the `AST_CAST` `-Wswitch`
warning from an earlier round was the same root cause): a change that
spans a `.h`/`.c` pair needs both halves applied together, and partial
application produces a compiler error that looks like a code problem but
isn't one.

## Constructor invocation begins: stack-allocated locals

Started the constructor-invocation work `sprite.cpp`/`sprite2.cpp`
surfaced the need for. A new lowering phase 7 (lower.c/lower.h) makes a
plain `ClassName var;` declaration -- no explicit initializer -- call a
matching zero-argument constructor immediately afterward, if one exists
and has a body. `tests/sample22.cpp` is this project's own copy of
Matthew's actual hand-written `sprite2.cpp`, added as a real test rather
than an artificial one.

**Deliberately narrow scope, three specific limitations documented in
both lower.h and the phase's own code comment, not silently absent:**
only a zero-argument constructor is matched (the `ClassName var;` syntax
form has no way to pass arguments at all); a prototype-only constructor
with no body is skipped rather than called (calling one would repeat the
exact `v32_new_Player`-shaped mistake -- a call to a C function that was
never emitted); a VarDecl inside a for-loop's own init clause
(`for (Player p; ...)`) isn't handled, since inserting the call would
need to land inside the loop body instead of right after the
declaration, meaningfully more involved for a pattern nothing currently
exercises.

**A class with virtual methods still gets its constructor called by this
phase, but that constructor does NOT populate `this->vtable`** -- there's
no static vtable INSTANCE for it to point at yet (the vtable struct TYPE
exists; a populated instance of one doesn't). This phase makes
non-virtual construction correct; it does not make polymorphic objects
safe to use yet. Vtable instance generation remains unstarted.

**A stale doc comment fixed in passing, unrelated to this phase's own
logic but noticed while touching this exact code**: lower.h's phase 6
documentation still claimed `new`'s grammar "has never supported
constructor arguments at all" -- true when that comment was written, but
wrong since the `NEW type_spec '(' opt_arg_list ')'` grammar addition and
`resolve_new_expr` several rounds ago. Corrected to reflect that
constructor ARGUMENTS parse and resolve correctly now; only actually
INVOKING the resolved constructor (and computing a real allocation size)
remains unimplemented.

**What's still missing for `new` specifically, genuinely blocked on
information this project doesn't have**: `new T(args)` still lowers to
a call to an undefined stub (`v32_new_T`) -- confirmed directly,
`tests/sprite.cpp` (Matthew's hand-written `new`-based version of the
same program) hits exactly this: `identifier "v32_new_Player" has not
been declared`. Making this work for real needs knowing how Vircon32
actually allocates heap memory -- is there a malloc-equivalent runtime
call, a fixed memory pool this project would need to manage itself,
something else entirely? Not guessed at here; asked directly instead
(see the conversation this round, not restated here since the answer
isn't in yet).

## Constructor invocation, part 2: `new` actually allocates and constructs -- confirmed working end to end

Matthew confirmed Vircon32's real C standard library has an actual
`malloc()`/`free()` (`misc.h`, attached rather than assumed) -- the
missing piece that made `new`'s placeholder allocator genuinely
unblockable before. `v32_new_*` now actually allocates via
`malloc(sizeof(ClassName))` and invokes the resolved constructor;
`v32_delete` calls `free()`. `tests/sample23.cpp` is this project's own
copy of Matthew's hand-written `sprite.cpp` -- the `new`-based sibling
of `sample22.cpp`'s `sprite2.cpp` -- added specifically because it's the
only existing test where the resolved constructor actually HAS a body
(`sample17.cpp`'s `Widget` and `sample18.cpp`'s `Point` are both
prototype-only), so it's the only one that exercises the "really
construct" path rather than the "just allocate" one.

**A real design mistake caught by tracing through existing tests before
shipping, not after -- twice, in the same sitting.** The first version of
this named every allocator by the resolved constructor's mangled name
ONLY when that constructor had a body, falling back to the bare type
name otherwise. Tracing that against `sample17.cpp`'s `Widget`
(prototype-only, zero args) looked fine at first, but `sample18.cpp`'s
`Point(int, int)` (prototype-only, TWO args) exposed the real problem:
the bare-name fallback only ever gets defined as a zero-argument
allocator, so a `new Point(3, 4)` call site would target
`v32_new_Point(3, 4)` while the only thing actually defined was
`v32_new_Point(void)` -- a second call/definition mismatch, not fixed by
the first pass at all. Corrected by naming the allocator after the
resolved constructor's own mangled name and matching its exact parameter
signature REGARDLESS of whether that constructor has a body -- a
bodyless one still gets a correctly-parameterized allocator, it just
accepts and discards the arguments rather than calling anything, since
there's nothing to call. Verified by hand against all three existing
`new`-using cases (`Widget` zero-arg/no-body, `Point` two-arg/no-body,
`Player` zero-arg/WITH body) before packaging, not just the one that
happened to prompt the fix.

**Confirmed against the real compiler, not just reasoned through.**
`sample23.c` compiles cleanly on the actual Vircon32 toolchain (Matthew's
report) -- the full `new`-based construction path, real `malloc()`
allocation through actual constructor invocation, verified working end
to end. `sample17`/`sample18`'s generated output also matches this
round's hand-traced predictions exactly (`v32_new_Widget__Widget__void
(void)`; `v32_new_Point__Point__int_int(int x, int y)`, correctly taking
both arguments rather than the buggy zero-arg fallback the first,
corrected-before-shipping attempt would have produced), and
`sample22.c`'s stack-allocated path (phase 7) still fires correctly
alongside this round's changes -- no regression there either.

**Scope, deliberately bounded, stated plainly rather than left
implicit:**
- `sizeof` still isn't a real AST concept in this project -- codegen.c
  emits the literal text `sizeof(TypeName)` directly (the C compiler
  evaluates it, not this one), which works fine for this specific,
  codegen-internal purpose but doesn't mean general `sizeof` expression
  support exists anywhere else.
- `#include "misc.h"` is emitted only when the program has at least one
  class (every class gets a `v32_new_*` allocator regardless of whether
  it's ever actually `new`-ed, so "any class exists" and "malloc is
  needed somewhere" are equivalent for now) -- specifically so a
  class-free program (`tests/sample21.cpp`, say) doesn't pick up
  `misc.h`'s own names (`malloc`/`free`/`rand`/`exit`/...) into scope
  for no reason. A tighter "only if `new`/`delete` is genuinely used
  somewhere" scan would be more precise but wasn't built this round --
  emitting an unused allocator for a class that's never `new`-ed is
  the accepted cost of the simpler version.
- **Destructor invocation is still completely missing.** `v32_delete`
  unconditionally calls `free()` and nothing else -- `delete obj;` on a
  class with a real, meaningful destructor currently just frees the
  memory without ever running it. A separate, still-unstarted piece of
  work, the natural mirror of phase 7 (constructor invocation) but for
  teardown -- both `delete obj;` and going out of scope would eventually
  need it, and neither triggers it today.

## Vtable static instances

Matthew clarified the earlier `.`/`->` question from a few rounds back
was his own adaptation oversight (already fixed in `sample23.cpp`), not
a real front-end gap -- confirmed, not something this project needs to
chase further.

Implemented vtable static instance population -- the piece that makes
virtual dispatch actually safe to use on a real object, not just
correct at the call site (which the `Circle`/`Shape` work several
rounds back already established). Two new pieces, working together:

**codegen.c's `emit_vtable_instance`**: emits a populated
`struct ClassName_VTable ClassName_vtable_instance = { ... };` for every
class with a vtable. The key distinction that makes this correct: a
slot's FIELD NAME always comes from `canonical_method` (stable across
the whole hierarchy -- matches what finalize_call already dispatches
through), but the VALUE stored there for THIS class's own instance comes
from `entry.method`, whichever implementation actually applies at this
level. Whenever `entry.method`'s own declaring class differs from
`canonical_method`'s, the value needs an explicit function-pointer cast
-- the same category of mismatch `cast_receiver_if_needed` already
handles at call sites, encountered here at initialization time instead.
A slot whose current implementation has no body at all gets a literal
`0` rather than a reference to something that doesn't exist.

**lower.c's phase 8**: prepends `this->vtable = &ClassName_vtable_
instance;` to the very start of every constructor that has a body, for
every class with a vtable -- before anything the constructor's own
body does, matching real C++'s own vtable-initialization timing.

**A design mistake caught mid-implementation by actually tracing an
existing test, not a new one.** Neither `sample7.cpp`, `sample12.cpp`,
nor `sample14.cpp` -- this project's only existing classes with vtables
-- declare a constructor at all. Phase 8 only ever injects into an
EXISTING constructor body, so none of them would exercise it at all;
without noticing this, this round's work could have shipped completely
unverified. Added `tests/sample24.cpp` specifically to close that: a
`Shape`/`Square` pair with both a virtual method AND a real constructor,
`Square::area` overriding `Shape::area` (exercising the cast), and
`main()` actually constructing and calling through both -- allocation,
construction, vtable population, and virtual dispatch, together, for the
first time in this project's test suite.

**A second thing caught before shipping the test itself**: the first
draft of `sample24.cpp`'s `main()` used `Shape shape(4);` -- direct
stack-initialization with constructor arguments. This project's grammar
support for that specific form was never confirmed, and there's no
reason to risk an unparseable test when `new`-based construction
(already confirmed working end to end against the real compiler,
`sample23.cpp`) does exactly what's needed instead. Rewritten to use
`new` before packaging, not shipped as written and hoped-to-work.

**Genuinely unconfirmed, stated plainly rather than left implicit**:
the function-pointer cast syntax `emit_vtable_instance` emits for a
mismatched slot -- `(ReturnType(ParamTypes)*)expr`, matching Vircon32's
own reversed declarator pattern with no name inside -- is this module's
best-reasoned attempt, not a verified-working one. Positional (not C99
designated) struct initialization was chosen deliberately, for the same
reason `print_type`'s function-pointer choices elsewhere in this file
were: fewer independent pieces of unconfirmed Vircon32-specific syntax
to be wrong about at once. Needs a real compile of `sample24.c` to
settle, the same as every other target-specific syntax choice in this
project.

**Still not covered by this round, worth restating**: a class with
virtual methods but NO constructor at all (or only a bodyless one) still
has no way to get its vtable pointer populated -- there's nowhere for
phase 8 to inject into. Real C++ would synthesize an implicit default
constructor for such a class; this project doesn't. Not new information,
but worth restating now that it's the concrete reason `sample7.cpp`/
`sample12.cpp`/`sample14.cpp` remain unable to safely instantiate their
own classes even after this round.

## Vtable instances: the real compile found a real regression

Matthew's real compile of `sample24.c` found a genuine mistake, not a
speculative "might be wrong" flagged in advance: `emit_vtable_instance`
hand-wrote `"struct %s_VTable %s_vtable_instance = {...}"` directly,
instead of following the "no `struct` keyword on a type REFERENCE"
rule this project has had centralized in `print_type` since the very
first codegen round. A type reference got the definition's own syntax
by mistake -- fatal to compile ("expected '{'"), exactly the category
of error this project's established discipline (test against the real
toolchain, don't guess) exists to catch. Fixed by dropping the `struct`
keyword; scanned every other hand-written `"struct` literal in
codegen.c afterward to confirm this was the only instance (the vtable
struct TYPE's own definition, the class struct's own definition, and
the forward-declaration emission all correctly keep `struct`, since
those genuinely are definitions/forward-declarations, not references).

Separately, `a`/`b` in `sample24.cpp`'s `main()` triggered unused-
variable warnings (non-fatal, but this project has held a zero-warnings
standard since the very first build). The test only ever cared whether
the calls compiled and dispatched correctly, not about observing their
results, so `main()` now discards `area()`'s return value directly
(`shape->area();`) instead of assigning it to a name nothing reads.

The genuinely unconfirmed piece flagged last round -- the function-
pointer cast syntax for a mismatched vtable slot,
`(int(Shape *)*)&Square__area__void` -- is NOT reported as a problem in
either report. Whether that means it's actually correct, or the second
error simply hadn't been reached yet by the time these specific two
issues were hit and reported, isn't known from what's been confirmed so
far.

**Now settled**: Matthew's next report was a clean build, clean
transpile, clean compile of `sample24.c` with no further errors --
confirming the vtable-instance cast syntax genuinely is correct, not
merely unreached. The full chain (allocation, construction, vtable
population, virtual dispatch through a base-typed pointer with a real
receiver cast) is now verified working end to end against the real
Vircon32 toolchain, not just reasoned through.

## Tracking Vircon32-specific quirks for a future standard-C mode

Matthew asked that every Vircon32-specific output divergence be tracked
deliberately, toward an eventual `--standard-c` (or similar) flag that
would let this project's output target an ordinary, portable C compiler
instead. `docs/VIRCON32_QUIRKS.md` is the result -- a dedicated,
checklist-organized catalog (by quirk, not chronologically, unlike this
file), covering: the `struct`-keyword-on-references rule, the reversed
array/function-pointer declarator forms, the matching reversed
function-pointer cast syntax, the `NULL`-not-`0` pointer rule, the
forced `void main(void)` shape, required prototype parameter names
(which turns out not to actually need a toggle -- it's already a subset
of standard C), and the `misc.h`-vs-`<stdlib.h>` header question. Each
entry states what Vircon32 requires, what standard C expects instead,
confirmation status, and which function(s) would need a conditional
branch -- specifically so building that mode later is "work through this
list" rather than re-discovering the whole surface area again. Kept
separate from this file on purpose: this file is the chronological story
of how each quirk got found; the new one is organized for someone
implementing a flag, who doesn't need that story to do the work.

Also fixed in passing while assembling this: `codegen.h`'s own
`main`-handling paragraph was stale, still describing a gap (no special
handling for `main`) that was actually resolved rounds ago -- `main`
stays unmangled and gets its return type forced to `void` already.
Corrected to point at the new quirks document instead of repeating
outdated narrative.

## Destructor invocation via `delete`

The mirror image of constructor invocation, for teardown instead of
construction. `delete obj;` now actually calls `obj`'s destructor (if
one exists and has a body) before freeing -- `v32_delete` used to just
call `free()` unconditionally, regardless of whatever cleanup logic a
class's destructor might contain.

**A real design question, worked through rather than assumed**: unlike
`new`, could the existing single, generic `v32_delete(void *ptr)`
just be extended to also call a destructor? No -- a `void *` genuinely
carries no type information at runtime (no RTTI in this C dialect), so
there's no way for a single shared function to know WHICH destructor to
call. This forced the same shape of change `new` already needed: `delete
obj;` is now named after `obj`'s own STATIC class
(`v32_delete_ClassName`), resolved in lower.c the same way a method
call's receiver class already gets resolved (`resolve_expr_class`).
Unlike `new`, there's no per-overload naming question at all here -- a
destructor can never be overloaded, so "v32_delete_ClassName" is
unambiguous whenever a class has one. The fully generic `v32_delete`
remains as the fallback for whenever an operand's static class can't be
determined.

**Threading `class_decl`/`locals` through a phase that never carried
them before.** `new_delete_rewrite_expr`/`_stmt`/`_classes`/
`_free_functions` had never needed either parameter until now (naming
`v32_new_TypeName`/`v32_delete` never depended on resolving anything
about the surrounding scope). Resolving a delete operand's class
needed both, so the whole chain now seeds and threads `locals` the same
way `finalize_calls_*` already does (reusing `seed_locals_from_params`
rather than re-deriving that logic a second time).

**Deliberately, explicitly NOT virtual dispatch.** `delete basePtr;`
where `basePtr` statically types as an ancestor but actually points at
a derived object will call the ancestor's destructor, not the derived
one -- the classic "non-virtual destructor through a base pointer" C++
footgun, except this project doesn't even check whether the destructor
was declared `virtual` before deciding this; it always behaves as if it
weren't. Real virtual destructor dispatch would need the `AST_DELETE`
lowering to route through the same vtable-dispatch shape
`finalize_call` already builds for an ordinary virtual method call -- a
real, separate piece of future work, not bundled into or silently
assumed handled by this round.

`tests/sample25.cpp` (`Logger`, a constructor and a destructor, both
with real bodies, no virtual methods at all -- deliberately kept
separate from `sample24.cpp`'s vtable concerns) is the first test in
this project's suite to actually exercise a destructor being called,
for the same reason `sample22`/`sample23`/`sample24` all needed adding:
nothing existing would have exercised the new code path at all
otherwise (`sample7.cpp`'s `~Shape()` is prototype-only, same gap
`sample24.cpp` closed for vtable instances last round).

## Array support: grammar work, and a genuinely unverified piece

First round to touch `parser.y` since the project's core grammar was
established -- everything before this was AST/sema/lower/codegen work
on an already-fixed grammar. Standard C++ array declaration syntax
(`int scores[8];`) is now supported, translated to Vircon32's own
required reversed declarator (`int [8] scores;`) on output.

**Matthew's specific ask**: accept BOTH standard array syntax and
Vircon32's own native-style syntax as valid C++-side INPUT, so someone
already fluent in Vircon32 C (or transitioning from it) never has to
learn a second declarator convention, while someone from ordinary C++
just writes what they already know -- with an explicit fallback to
"standard-C only" if dual acceptance turned out to create real grammar
conflicts. Implemented as asked: `parser.y`'s `var_decl` now has three
alternatives -- the original bare declarator, a standard-C array form
(`type_spec pointer_opt IDENTIFIER '[' INT_LITERAL ']'`), and a
Vircon32-native-style form (`type_spec '[' INT_LITERAL ']' IDENTIFIER`,
deliberately without `pointer_opt` -- this form exists to match
Vircon32's own convention exactly, not to become its own general
declarator sublanguage). Both produce an identical `AST_ARRAY_TYPE`
(ast.h) -- the AST carries no memory of which spelling was used, and
`print_type` always emits Vircon32's required form regardless.

**A genuine, upfront limitation: this round's grammar change is
reasoned-through, not built-and-verified.** Bison isn't available in the
sandbox this project has been developed in -- every other round's C
source could be syntax-checked directly with `gcc -fsyntax-only`, but
`parser.y` itself needs an actual bison run to confirm the new
alternatives don't introduce a grammar conflict beyond the existing,
already-verified `%expect 22`. The three-way `var_decl` split was
reasoned through carefully (each alternative's own first
distinguishing token -- `'*'`/`'&'` for a pointer, a bare `IDENTIFIER`
for the original form, `'['` immediately after `type_spec` for the
Vircon32-style array form, `'['` after the identifier for the
standard-C array form -- are all distinct at the single-token-lookahead
decision point LALR(1) needs), but "reasoned correctly" and "confirmed
against the actual tool" are not the same claim, and this project has
learned that distinction the hard way more than once already. Matthew's
own build is what actually confirms this.

**Found and fixed along the way, not part of the original ask but
directly relevant once arrays exist at all**: `infer_expr_type`
(sema.c) had never handled `AST_SUBSCRIPT` -- `arr[i]`'s type silently
fell through to "unknown" rather than resolving to the element type.
Harmless before this round (nothing could produce an array type to
subscript in the first place), but a real, waiting-to-matter gap the
moment one could. Fixed alongside the main work: unwraps either
`AST_ARRAY_TYPE` or `AST_POINTER_TYPE` (ordinary pointer-arithmetic-
style subscripting, `p[0]` on an `int *p`) to the element/pointee type.

**Scope limits, stated plainly rather than left implicit**: array
support is `var_decl` only -- covering local variables, class data
members, and a for-loop's own init clause, since all three already
share that one grammar rule. Function PARAMETERS of array type are
explicitly NOT covered (an array parameter decaying to a pointer, and
losing its size in the process, is a distinct C semantic this project
hasn't addressed at all); neither are array initializer lists
(`= {1, 2, 3}`, a different, unbuilt piece of grammar) -- a declared
array currently has no way to be initialized inline at all, standard-C
or Vircon32-style.

`tests/sample26.cpp` exercises both accepted input forms side by side
(a standard-C local array and a Vircon32-style one in the same
function), subscript read/write on both, and a class data member array
(`Scoreboard`'s `scores[4]`, constructed via phase 7's existing
stack-allocated constructor invocation, a natural integration point
with prior work rather than an isolated new test).

**For the standard-C mode this project is still tracking toward**:
`docs/VIRCON32_QUIRKS.md`'s array-declarator entry is updated to
"implemented," with a new adjacent entry recording Matthew's own
framing as a standing principle for future syntax work generally --
"let the C++ appear normal, even if the transpile has to adjust things"
-- specifically flagged to extend to function-pointer declarators
whenever that work happens, rather than deciding the dual-acceptance
question fresh each time.

## Destructor invocation at scope exit -- and a genuine testing milestone

The mirror of phase 7, for teardown instead of construction: a
stack-allocated local of class type, whose class has a destructor with a
body, now gets that destructor called wherever it goes out of scope.

**Scoped correctly on the first attempt, not narrowed after the fact.**
Real C++ RAII also has to handle `break`/`continue`/exceptions unwinding
a scope early. Before designing anything, checked directly whether this
project's grammar even has `break`/`continue` at all -- it doesn't (no
token, no AST kind, nothing in `lexer.l` or `parser.y`), and exceptions
are out of scope for this project entirely. That leaves exactly two ways
control can leave a block in the language this project actually accepts:
falling off the end, or `return`. Handling both IS the complete, general
solution here, not a scoped-down first slice of one -- worth stating
plainly rather than let it read like a partial feature the way phase 7
(constructor invocation) and the `delete`-invocation round both
genuinely were partial slices of their own larger problems.

**The algorithm**: walks each block maintaining a stack of per-block
"destructible locals" lists, each linked to its enclosing block's own
list. At a block's own end, appends destructor calls for THAT block's
own destructibles, reverse declaration order. At a `return`, walks the
full scope chain -- this block and every enclosing one, up to the
function's top -- emitting destructor calls for all of them,
innermost-first. A non-void `return expr;` needs a small rewrite:
`expr` must be evaluated before any destructor runs (a destructor could
depend on or invalidate something the expression reads), so it becomes
a small nested block -- a temporary holds the already-computed result,
the destructor calls run, then a bare `return` of the temporary. A bare
`return;` needs no temporary at all.

Deliberately built as a fresh, purpose-built structure rather than
reusing the existing `LocalVarType`/`locals` threading used everywhere
else in this file -- that tracking has a known, pre-existing
imprecision (a nested block's own locals can leak into an enclosing
scope's view, since several call sites pass `locals` through by address
rather than by value), harmless everywhere it's currently used, but this
phase specifically needs exact block-exit boundaries. A fresh structure
avoids inheriting that imprecision rather than working around it.

**A known, minor inefficiency, not a correctness issue**: a block whose
own last statement is always a `return` (like `compute()` in the new
test) still gets a fall-through destructor sequence appended after it,
which is then simply unreachable dead code. Detecting that would need
real reachability analysis; not attempted here, and harmless either way
-- unreachable code doesn't change behavior, just adds a few unused
lines to the generated output.

**A real testing milestone, not just another confirmed-working round.**
This is the first feature of real complexity in this project verified
directly, before ever reaching Matthew, rather than reasoned through and
handed off unverified: with the bison/flex-generated `parser.c`/
`lexer.c`/`parser.h` Matthew provided a couple of rounds back, a full
local `v32c++` build now exists and stayed current (only `lower.c`
changed this round -- no grammar involved, so no new generated files were
needed). Used it to actually transpile a purpose-built test
(`tests/sample27.cpp` -- an early `return` from inside a nested block,
with locals live at two scope levels simultaneously, plus a non-void
`return expr;` exercising the temporary-variable rewrite) and trace the
output by hand before writing it up. Went further than that: substituted
Vircon32's `struct`-keyword quirk back in by hand to approximate
standard C, and confirmed the new nested-block/temporary-variable output
is genuinely sound C syntax, independent of any Vircon32-specific
question -- `gcc -fsyntax-only` came back clean except for the one
already-known, expected `void main` warning. Also re-ran the full
existing 26-sample suite through the rebuilt binary to confirm zero
regressions, and spot-checked that a class only ever used as a pointer
(`tests/sample25.cpp`'s `Logger`, via `new`/`delete`) correctly gets NO
new scope-exit injection at all -- confirming the new phase doesn't
over-apply.

**Still needs Matthew's own build to confirm against the real Vircon32
compiler** -- standard-C-soundness is strong evidence, not the same
claim as a real compile. `tests/sample27.cpp` is the one to try first.

**Also found and fixed while documenting this round**: `lower.h`'s
own top-of-file phase-by-phase documentation had gone stale in two
places that predate this round entirely -- phase 6's entry still
described `delete` as calling a single generic `v32_delete` with no
destructor invocation at all (stale since the `delete`-invocation round
several sessions back), and phase 8 (vtable pointer init) was missing
from this documentation block entirely, never added when phase 8 itself
was built. Both corrected here, not just phase 9's own new entry added
on top of stale surrounding text.

## A large round: preprocessor pass-through, array completion, and a clarification on break/continue

Matthew clarified that `break`/`continue`'s absence from this project's
grammar (which phase 9's own "complete picture" reasoning explicitly
relied on) is temporary, not permanent -- they're planned. `lower.h`'s
phase 9 documentation updated to say so directly: the moment either
exists, phase 9 needs revisiting, since a `break`/`continue` inside a
loop can exit a scope exactly as early as a `return` does, with the
same destruction need. Recorded as a standing note rather than left to
go stale silently the way two OTHER paragraphs in that same file
already had (phase 6 and phase 8's own documentation, both caught and
fixed a couple of rounds back) -- naming that pattern explicitly in the
new note too, so it's harder to repeat a third time.

**Preprocessor pass-through** (Matthew's specific ask): a `#`-line is no
longer silently dropped. `lexer.l` captures it verbatim into a new
global (`driver.h`'s `g_preprocessor_lines`); `codegen.c` re-emits every
captured line at the very top of the generated file, ahead of even the
conditional `#include "misc.h"` this project may add on its own. NOT
real preprocessing -- no macro expansion, no `#include` resolution, no
`#ifdef` evaluation, nothing interprets what was captured at all, stated
plainly in three separate places (driver.h, lexer.l, and here) rather
than left to be discovered the hard way. What it DOES fix directly:
`tests/sample22.cpp` onward have all needed `#include "video.h"`
manually re-added by hand after every transpile, specifically because
this didn't exist yet -- `tests/sample28.cpp` is the new test confirming
that's no longer necessary.

**Array completion**, the three pieces explicitly requested to close out
the array work before advertising it:

- **Initializer lists** (`int arr[4] = {1, 2, 3, 4};`) -- new
  `AST_INIT_LIST` node, new `opt_array_initializer` grammar rule
  (reusing the existing `opt_arg_list` machinery rather than building a
  parallel comma-list rule from scratch), wired into both accepted array
  declarator forms. A real gap found and fixed in the same pass, not
  assumed away: `sema.c`'s `check_node` isn't a fully generic walker --
  unhandled node kinds fall through to a no-op default case -- so
  without an explicit `AST_INIT_LIST` case, the values inside an
  initializer list would never have been checked at all (a call or
  operator use inside one would have silently skipped semantic
  analysis). No length-checking against the array's own declared size
  happens anywhere (neither "too many initializers" nor padding a short
  list with zeros) -- a real, documented gap, not silently handled.
- **Array function parameters** (`void foo(int arr[])`) -- decays
  straight to an ordinary pointer type AT PARSE TIME, matching real
  C/C++ semantics exactly (an array parameter has no size information
  preserved at all regardless). This meant no `AST_ARRAY_TYPE` is ever
  involved for a parameter, and so no sema/lower/codegen changes were
  needed for this piece at all -- the simplest of the three by a wide
  margin, once designed correctly.
- **`new T[N]` / `delete[]`** -- allocation only, deliberately: no
  per-element construction or destruction happens for either, since
  there's no per-element analogue of phase 7's stack-array support or
  any loop-emission machinery anywhere in this project. Named
  `v32_new_arr_ClassName`, distinct from the per-constructor-overload
  `v32_new_ClassName...` family used for single-object `new` -- never
  ambiguous the same way, since there's exactly one shape of array-new
  per class regardless of what constructors it declares.
  `delete[] ptr;` currently lowers IDENTICALLY to plain `delete ptr;`
  (the distinction is recorded on the AST, `AST_DELETE`'s `ival`, but
  nothing downstream acts on it yet) -- for the same underlying reason.

**A real correctness question investigated before committing to the
`sum(fixed, 4)` test, not assumed safe.** Would `resolve_overload_generic`
correctly accept an array-typed argument passed to a decayed-pointer
parameter? Traced through it directly: when there's only ONE candidate
for a name (true for `sum`, which isn't overloaded), resolution checks
arity only, never types at all -- full type-checking only happens once
there are multiple candidates needing disambiguation. Confirms the
planned test is safe, but also surfaces a genuine, separate, pre-existing
limitation worth naming: if a function WERE overloaded between an array
parameter and something else, `types_equal` almost certainly doesn't
know about array-to-pointer decay, and would likely reject a call that
real C++ accepts. Narrow enough (overloading specifically on array vs.
pointer) that it wasn't fixed this round, but recorded rather than
quietly left for someone to rediscover.

**Found while touching `AST_NEW` for array-new -- FOUR separate places
in `lower.c` independently walk `AST_NEW`'s children, and every one of
them needed the same fix**: this-injection's `rewrite_expr`, phase 3's
`finalize_calls_expr`, phase 5's `fix_reference_access_expr`, and phase
6's actual lowering. None of them originally knew about `AST_NEW`'s new
`a` field (the array-size expression) -- each would have silently
skipped processing it (a `this`-reference inside `new int[this->count]`
never rewritten, a method call inside the size expression never
resolved, a reference-typed local inside it never fixed up) had all
four not been updated together. Caught by deliberately grepping for
every `case AST_NEW:` in the file rather than trusting memory of where
they all were.

**Genuinely unverified as of this writing, same constraint as the
original array-support round**: this entire round touches `parser.y`
and `lexer.l` again (the array grammar extensions, the preprocessor
capture), and bison still isn't available in this sandbox. Every `.c`
file syntax-checks clean individually, and the design was reasoned
through carefully at each grammar decision point, but none of it has
been run through an actual build yet. `tests/sample28.cpp` (preprocessor
pass-through) and `tests/sample29.cpp` (initializer lists, array
parameters, `new[]`/`delete[]` together) are the two new tests -- both
need Matthew's own rebuild before either is more than reasoned-through.

**`README.md` rewritten, not just amended.** It had gone badly stale --
still describing a state from before code generation existed at all
("Code generation to Vircon32 C doesn't exist yet," still listing
method-body emission and vtable instances as the next missing pieces),
predating essentially everything this project has actually built.
Rewritten to reflect the real current state -- the full nine-phase
lowering pipeline, working code generation confirmed against the real
compiler, arrays as a headline feature (per Matthew's specific request,
once this round's array-completion work was done), and an honest list of
what's still missing (`break`/`continue`, virtual destructor dispatch,
base-class constructor delegation, standard-C mode) rather than nothing
at all. Kept the same honest-status-callout structure the original had,
since that framing itself was good; the content underneath it just
hadn't kept up.

## Two real, fatal bugs found by Matthew's own rebuild -- both fixed and re-verified before handing back

Matthew's fresh build (with the new `parser.c`/`lexer.c`/`parser.h`)
surfaced two genuine issues, one cosmetic-adjacent, one a real,
fatal-to-compile bug -- plus confirmed the two apparent build problems
from immediately before this were sync issues, not code bugs.

**Confirmed, not just assumed, that the warnings/linker error were
stale-file sync issues.** Rebuilt from scratch locally with Matthew's
fresh generated files: `ast.c` produced zero `-Wswitch` warnings (the
`AST_INIT_LIST` case was already there), and the link succeeded cleanly
(`g_preprocessor_lines` was already correctly defined in `main.c`). Both
match this project's own established pattern for this exact failure
mode (the `infer_expr_type` incident several rounds back was the same
root cause) -- not re-litigated at length here since the pattern is
already documented, just confirmed directly rather than assumed.

**A real, fatal bug, found via my own first real test run of
`tests/sample29.cpp`**: `new int[5]` generated a call to
`v32_new_arr_int`, but nothing in the output ever DEFINED that function
-- the exact "identifier not declared" failure mode `v32_new_Player`
originally had, early in this project. Two compounding causes, both
fixed:

1. Array-new's type-naming reused `type_to_class`, which only resolves
   an actual registered class -- for `int`, correctly `NULL`, with the
   fallback being the literal string `"unknown"`. Produced
   `v32_new_arr_unknown` instead of `v32_new_arr_int`. Fixed with a new
   `type_name_for_new` helper (lower.c) that falls back to the type
   node's own name for a bare, non-class type -- `new int`/`new
   int[N]` are both genuinely legal C++, not something only this
   project's own classes need.
2. Even with the name fixed, nothing would have defined it anyway:
   `needs_misc` (codegen.c) -- whether to `#include "misc.h"` and emit
   every runtime function that depends on it -- was gated purely on
   "does the program have at least one class," which is false for
   `sample29.cpp` (only free functions). Fixed two ways: a new
   `g_uses_new_or_delete` global (driver.h), set by lower.c's
   `new_delete_rewrite_expr` the moment it actually lowers ANY
   `new`/`delete`/`new[]`/`delete[]`, now also gates `needs_misc`; and a
   new `emit_primitive_array_new_runtime` (codegen.c) unconditionally
   emits `v32_new_arr_int`/`float`/`char`/`bool` whenever `misc.h` is
   included at all, since a primitive type has nowhere to be "found" by
   walking declarations the way a class does.

**Re-verified thoroughly before handing anything back, not just
fixed and assumed correct**: rebuilt from scratch, confirmed
`sample29.c` now has `#include "misc.h"` and a matching, fully-defined
`v32_new_arr_int`; re-ran the full 29-sample suite to confirm zero
regressions; specifically checked that every existing `new`/`delete`
test (`sample17`/`18`/`23`/`24`/`25`) still gets `misc.h` correctly
(now over-generating the 4 primitive allocators alongside their own
per-class ones, an accepted, deliberate trade-off, same "over-generate
rather than risk under-generating" reasoning already used for the
per-class allocators); and specifically confirmed `sample21.cpp` (no
classes, no `new`/`delete` at all) still correctly gets NEITHER
`misc.h` nor any allocator -- confirming the gate is still a genuine
gate, not accidentally always-true now.

**Also addressed, at Matthew's request**: the pre-existing, known
`-Wsign-compare` warning in flex's own generated `yy_get_next_buffer`
(present since this project's very first build, previously just
documented as expected) now has a `#pragma GCC diagnostic ignored
"-Wsign-compare"` added to `lexer.l`'s own prologue, which flex copies
verbatim into the generated file ahead of the code that triggers it.
UNVERIFIED as of this writing -- needs a fresh `lexer.c` regenerated
from the updated `lexer.l` to actually confirm the warning is gone,
since this project still has no flex available in this sandbox to
regenerate it directly.

## `break`/`continue`, and phase 9's promised revisit

Second round to touch `parser.y`/`lexer.l` (after the array-completion
round). `break`/`continue` are now real grammar productions
(`BREAK ';'`/`CONTINUE ';'`), each becoming its own AST node
(`AST_BREAK`/`AST_CONTINUE` -- leaf statements, no fields at all).
`sema.c` rejects either appearing outside a loop (a file-local
`g_sema_loop_depth` counter, incremented/decremented around a loop
body's own walk -- not threaded through `check_node`'s parameter list,
since that walk is single-threaded and strictly depth-first, so a
global serves the purpose far more simply than a new parameter at every
existing call site would). `codegen.c` emits each as the literal C
keyword -- no Vircon32-specific quirk here, confirmed by there simply
being nothing unusual to reason about, unlike most of this project's
other syntax choices.

**The real work was phase 9's promised revisit, exactly the consequence
flagged in `lower.h`'s own documentation several rounds back.** A
`break`/`continue` can exit a scope exactly as early as `return` does,
needing the same destruction -- but only up to the boundary of the loop
actually being exited, not all the way to the function's own top the
way `return` does (anything declared outside that loop stays alive,
same as it would after the loop ends normally). Implemented by
threading a new `loop_boundary` parameter through
`destruct_scope_stmt`/`destruct_scope_block`: `AST_WHILE`/`AST_FOR` set
a fresh boundary (the scope in effect right before entering their own
body) when recursing into it; `AST_IF` passes whatever boundary it was
already given straight through, since an `if` doesn't introduce a loop
of its own. A shared `install_destructor_sequence` helper factors out
the "walk from here to a stop point, destroying everything found, then
install the (possibly rewritten) tail statement" logic now common to
`return` (stop point: the true top, `NULL`) and `break`/`continue`
(stop point: `loop_boundary`) -- the two only ever differed in where
the walk stops and what the tail statement is, never in the walking or
destroying itself, so duplicating that logic a second time would have
been a real invitation for the two to quietly drift apart later.

**A genuine subtlety `return`'s existing logic didn't have to deal
with, kept as its own explicit branch rather than forced through the
shared helper**: a non-void `return expr;` always needs its temporary-
holding `VarDecl` installed, even when nothing in scope needs
destroying at all (the shared helper's own "nothing to destroy, skip
the wrapping block entirely" shortcut would otherwise silently drop
it). `break`/`continue` have no such requirement -- there's no value to
preserve -- so they route through the shared helper's short-circuit
cleanly; `return expr;` doesn't, and stays as its own explicit path
that always builds the full sequence.

**A real mistake caught by my own regression run, not shipped
unnoticed**: recompiling only the files I thought were affected by
`ast.h`'s enum change (`ast.c`/`sema.c`/`lower.c`/`codegen.c`) while
leaving `symtab.o`/`main.o` stale produced deeply confusing, garbled
output across nearly the entire test suite -- inserting `AST_BREAK`/
`AST_CONTINUE` into the middle of the enum shifts every subsequent
value's number, and linking object files compiled against different
numberings of the same enum silently misinterprets node kinds across
the object-file boundary. Not a logic bug at all -- a full, `rm -f
obj/*.o`-then-rebuild-everything cycle resolved it completely, and the
suite came back to exactly the expected 6 failing samples. Worth
recording as a standing reminder for this project specifically: any
change to `ast.h`'s enum needs a truly full rebuild, not a targeted one
based on which files seemed obviously affected -- the enum-numbering
fragility touches every translation unit that includes `ast.h`, not
just the ones with new logic in them.

**Verification status, stated precisely rather than left ambiguous**:
the C-side implementation (sema.c's check, lower.c's phase 9 extension,
codegen.c's emission) compiles and links cleanly, and the full existing
29-sample suite shows zero regressions -- confirming the parts of this
round that don't depend on the grammar are sound. The grammar change
itself (parser.y/lexer.l) is, like the array-completion round before
it, reasoned through but not yet built -- this sandbox still has
neither bison nor flex. Tried running `tests/sample30.cpp`/
`sample31.cpp` against the still-stale (pre-this-round) grammar as a
sanity check; both "succeeded," but only because the old grammar still
treats `break`/`continue` as ordinary, undeclared identifiers rather
than keywords -- `continue;` parses as a bare-identifier expression
statement under the old grammar and happens to print as the literal
text "continue;" by coincidence, not because any of this round's new
logic actually ran. None of this round's own break/continue-specific
code has been genuinely exercised yet; `tests/sample30.cpp` (the
loop-with-all-three-exit-paths test) and `tests/sample31.cpp` (the
deliberately-invalid outside-a-loop test) both need Matthew's own
rebuild before either is more than reasoned-through.

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
    2. ~~`this`-injection.~~ Done — an implicit method receiver becomes
       an explicit first parameter, and every implicit member reference
       (bare identifier, unqualified method call) becomes explicit
       through it. `tests/sample13.cpp` exercises explicit vs. implicit
       `this->x`, an unqualified method call, and a local variable
       correctly shadowing a same-named member instead of being rewritten.
    3. ~~Vtable dispatch codegen (call finalization).~~ Done — every
       call's callee is now rewritten to its final form (virtual dispatch
       through `obj->vtable->FIELD`, or a direct mangled-name call),
       driven by sema's already-overload-aware `CallResolution` rather
       than re-resolving names independently. `tests/sample14.cpp`
       specifically exercises vtable field-name stability across an
       overriding class — the trickiest part of this phase to get right.
    4. ~~Operator-overload-to-function-call rewriting.~~ Done — closed a
       real gap along the way: sema_run() never resolved natural operator
       syntax (`a + b`) at all before this, only explicit call syntax.
       `tests/sample15.cpp` exercises member and free-function operators
       via natural syntax.
    5. ~~Reference-to-pointer rewriting.~~ Done — `tests/sample16.cpp`
       confirms a reference parameter's `.` becomes `->` while a
       by-value parameter's `.` stays untouched.
    6. ~~`new`/`delete`-to-runtime-call rewriting.~~ Done, deliberately a
       placeholder (no `sizeof`, no constructor invocation — the grammar
       doesn't even parse constructor arguments in `new` yet). `tests/
       sample17.cpp` exercises the placeholder calls.
    7. ~~The lowering track is complete through phase 6, and fully
       verified against real output~~ — including two real bugs (phase
       4's operator-arity comparison, and the `dump_this_injected_methods`
       display gap) found and fixed along the way, not just plausible-
       looking code taken on faith. Done.
13. Remaining natural next candidates, independent of the lowering track
    above: the preprocessor gap (see the project README — a custom
    `v32pp` is the long-term plan, with `cpp` as a stopgap in the
    meantime).
14. ~~The Vircon32 C code generator itself~~ — STARTED, not finished.
    `codegen.c`/`codegen.h` emit typedefs, vtable struct types, and class
    struct definitions (see the "Code generation begins" section above
    for the two Vircon32-specific quirks this had to get right, the
    design wrinkle it ran into, and the open forward-reference-ordering
    question it deliberately doesn't resolve). NOT yet done: method/
    function body emission, and vtable static instance emission — both
    substantial enough for their own round.
