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

## Virtual destructor dispatch -- narrower fix than expected, unverified due to a build gap

Confirmed `break`/`continue` fully working end to end first -- Matthew's
rebuild showed a clean transpile, `sample30.cpp`'s destructor-across-
three-exit-paths logic working correctly, `sample31.cpp`'s
outside-a-loop error firing as expected, and `sample30.c` compiling
clean on the real compiler. Phase 9's promised revisit is now genuinely
confirmed, not just reasoned through.

Moved on to virtual destructor dispatch. Investigated before assuming a
large new mechanism was needed, and found the actual gap was much
narrower than expected: a virtual destructor already participates
correctly in sema.c's existing vtable-slot machinery entirely by
accident of how that machinery was already written -- `build_vtable`
never special-cases destructors at all, just checks the same generic
"is this virtual" flag any method has, and `vtable_slot_key` already
normalizes every destructor's own name to the literal string `"~"`
regardless of which class it belongs to, so a derived override already
matched and correctly replaced its inherited base slot the same way an
ordinary virtual method already does. None of that needed to change at
all.

The ONLY actual gap was in codegen.c's `emit_delete_runtime`: the per-
static-type deallocator (`v32_delete_ClassName`) always called a
statically-named function regardless of whether the destructor was
virtual. Fixed there specifically -- not in lower.c's own `AST_DELETE`
naming logic, which was never the problem (naming the deallocator after
the operand's STATIC class remains exactly correct; what that function
does INTERNALLY is what needed to change). A virtual destructor now
makes `v32_delete_ClassName` dispatch through `ptr->vtable->...`
instead of a direct call, the same shape `finalize_call` already builds
for an ordinary virtual method call, with the identical receiver-cast
reasoning (the vtable access itself never needs a cast; the argument
passed to the slot does, whenever the class isn't its own canonical
declarer).

**Traced by hand in full before writing anything up**, since no working
build was available this round (see below): for `tests/sample32.cpp`'s
`Shape`/`Square` pair, `v32_delete_Shape`'s new
`ptr->vtable->Shape__dtor__void(ptr)` correctly resolves to
`Square__dtor__void` at runtime when `ptr` actually points at a
`Square` object, because that object's own vtable instance (set during
`Square`'s own construction, phase 8) is what `ptr->vtable` genuinely
points at -- the field NAME is shared across every class's vtable
struct (always the canonical one), but the VALUE stored there differs
per actual object, which is the entire mechanism this fix now finally
uses correctly for destruction too.

**A separate, genuine uncertainty flagged directly in the test file
itself, not hidden**: `tests/sample32.cpp`'s
`Shape *shapePtr = new Square(4);` assigns a derived pointer to a
base-typed variable with no explicit cast -- this project's grammar has
no C-style cast expression at all (confirmed directly: nothing in
`parser.y` produces one from source; `AST_CAST` only ever comes from
lowering's own receiver-cast insertion, never from something a person
could write). If Vircon32 rejects this the way it's already shown
itself strict about other pointer-type mismatches, that would be a
real, separate, pre-existing gap (no cast-insertion for an ordinary
assignment, only for a method call's own receiver) -- not a problem
with the destructor-dispatch fix this test actually exists to confirm.
Worth knowing which of the two is actually at fault if `sample32.c`
doesn't compile cleanly.

**Genuinely could not build-test any of this round's own work,
unlike most recent rounds.** The `lexer.c` attached alongside
`sample30.txt`/`sample31.txt`'s confirmation turned out to be several
rounds stale -- missing not just `break`/`continue` support but the
`g_preprocessor_lines` global from an EARLIER round too, confirmed by
attempting the build directly (`'g_preprocessor_lines' undeclared`).
`parser.c`/`parser.h` were current (confirmed: `BREAK`/`CONTINUE` appear
correctly in the token enum); only `lexer.c` was the wrong vintage --
likely attached by oversight alongside the `lexer.l` that was
genuinely used to (successfully) regenerate it on Matthew's own end.
Every piece of this round's own C code still syntax-checks clean
individually, and the destructor-dispatch logic was traced by hand as
thoroughly as this project's discipline requires when a real build
isn't available -- but "traced correctly" and "confirmed against an
actual compile" remain two different claims, and this round is
genuinely still the former, not the latter. Needs a fresh `lexer.c`
before `tests/sample32.cpp` is more than reasoned-through.

## Virtual destructor dispatch, confirmed -- and a second, real bug found by the same test

Matthew's rebuild confirmed the vtable-dispatch fix itself is sound, but
`sample32.c` failed with a DIFFERENT error, at the exact line the
test's own comment had already flagged as genuinely uncertain:
`Shape *shapePtr = new Square(4);` -- "types are not compatible: cannot
assign struct Square* to struct Shape*". Vircon32 rejects the implicit
derived-to-base pointer conversion outright, stricter than real C++,
which allows it freely as an ordinary upcast.

**Fixed with a new lowering phase (6a), inserted specifically before
phase 6**, not after: `insert_pointer_cast_stmt` walks every VarDecl,
and whenever its own pointer-typed initializer resolves (via
`infer_expr_type`) to a DIFFERENT, related class than its declared
type, wraps the initializer in an explicit `AST_CAST` to the declared
type -- same underlying justification `cast_receiver_if_needed`
already relies on for a method call's own receiver (this project's
single-inheritance struct layout guarantees the conversion is
genuinely safe; C's type system, and evidently Vircon32's own, just
has no way to know that without being told explicitly). The ordering
requirement is real, not incidental: `infer_expr_type` needs to see the
ORIGINAL `AST_NEW` node to infer "pointer to Square" at all (its own
`AST_NEW` case builds that type directly from the node itself) -- once
phase 6 has already turned it into a call to
`v32_new_Square__Square__int`, there's no `AST_NEW` left to ask, just
an ordinary function call this project's type inference has no special
knowledge of. Verified directly: `shapePtr`'s initializer is now
`((Shape *)v32_new_Square__Square__int(4))`, and re-running the full
32-sample suite confirmed zero regressions -- specifically checked that
already-matching-type `new` assignments (`sample17`/`18`/`23`, none of
which have this mismatch at all) get no cast inserted at all, confirming
the fix doesn't over-apply.

**Scope, stated plainly rather than left to be assumed broader than it
is**: only a VarDecl's own initializer is covered. The identical
mismatch could just as easily arise in a plain assignment after the
fact, a function argument, or a return value -- none of those are
covered by this phase, a real, documented gap rather than something
quietly assumed handled too.

`ast.h`'s own `AST_CAST` documentation updated to reflect there are now
TWO lowering phases that produce this node kind, not one, both for the
same underlying reason. `tests/sample32.cpp`'s own comment updated from
"genuinely uncertain, might be a separate gap" to a plain statement of
what was actually found and fixed, now that it's confirmed rather than
speculated about.

## Confirmed: virtual destructor dispatch + pointer-cast fix, both clean on the real compiler

`sample32.c` compiles cleanly. Both pieces from the last two rounds --
vtable-dispatched destruction through a base-typed pointer, and the new
lowering phase inserting an explicit cast for a VarDecl's mismatched
pointer initializer -- are now confirmed working end to end, not just
reasoned through.

## Verbosity levels, and a default-output-filename change -- main.c redesigned

Matthew asked for this project to stop always dumping its full internal
state and start behaving like an ordinary tool: silent by default,
verbosity opt-in and stackable (`-v`/`-vv`/`-vvv`), matching how the
real Vircon32 C compiler and v32lua both already work. Also asked for
`-o`'s absence to mean "derive the output filename from the input" (also
matching both sibling tools) rather than "dump to stdout instead," and
for `-g` to be explicitly reserved, unimplemented, for a future special
debug-output mode.

**`main.c` redesigned around a `verbosity` counter**, incremented once
per `-v` (getopt_long's own short-option bundling already turns `-vvv`
into three separate `'v'` cases, identical to `-v -v -v`, so nothing
extra was needed to support both spellings). Level 0 (the default, no
`-v` at all): completely silent on success -- no stage messages, no
dumps, nothing to stdout at all; genuine errors still go to stderr
regardless of verbosity, since those were never "diagnostic" output in
the first place. Level 1: four stage-progress lines
("stage N: running ..."), adapted to this project's ACTUAL pipeline
rather than copying v32lua's own wording verbatim -- lexer and parser
are reported as one combined stage ("running lexer/parser"), since
this project's flex/bison architecture genuinely interleaves them
(`yyparse()` calls `yylex()` as needed, they were never two sequential,
discrete passes the way a traditional lex-then-parse pipeline would
have them), and there's no separate "preprocessor" stage either, since
`#`-line pass-through happens inline within the lexer, not as its own
pass. Level 2: everything from level 1, plus the full AST/semantic-
analysis/lowering dumps this project has always produced -- exactly what
used to happen unconditionally, now opt-in. Level 3: reserved for
future, even-more-detailed output Matthew may want to add later; no new
content exists for it yet, so it currently behaves identically to level
2 -- stated plainly as a placeholder, not silently pretended to already
do something.

**Default output filename**: a new `derive_output_filename` strips the
input's own extension (matching everything after the last `.`, but
only when that `.` comes after the last `/` too, so a directory
component that happens to contain its own `.` can't be mistaken for
the input's extension) and appends `.c` -- `foo/bar.cpp` becomes
`foo/bar.c`, matching exactly how the real Vircon32 C compiler and
v32lua both already behave. `-o` still overrides this when given. The
old "no `-o` means dump the generated C to stdout instead" behavior is
gone entirely -- v32c++ now always writes an actual file, one way or
the other.

**`-g` reserved, not implemented**: deliberately left out of the
`getopt_long` options string entirely, rather than added as a silent
no-op -- using it today fails loudly ("invalid option") instead of
appearing to do something it doesn't yet.

**The Makefile's own `test` target needed real, not cosmetic,
updating** -- every one of its 32 commands relied on the old
always-verbose, dump-to-stdout-by-default behavior that no longer
exists; without `-vv`, `make test`'s own `.txt` captures would have
gone essentially empty. Regenerated all 32 lines programmatically
(a small Python script, not 32 individual hand-edits -- lower risk of a
transcription mistake at this scale) rather than hand-editing each one:
every sample now gets `-vv` (to keep capturing the full dumps this
suite has always relied on for review) and `-o out/sampleN.c` (so the
now-always-written generated C lands in `out/` alongside its own
`.txt` dump, rather than cluttering `tests/` with 32 files that don't
belong there, now that omitting `-o` no longer means "print to stdout
instead"). Verified directly: re-ran the full suite, confirmed exactly
the 7 expected failures (`4`/`5`/`10`/`11`/`19`/`20`/`31`), all 32
`.txt` dumps present, and exactly 25 `.c` files (32 minus the 7
deliberately-invalid samples, which produce no output at all) --
matching precisely. Spot-checked that `sample32.c`'s own content is
byte-identical to the last confirmed-working version, confirming the
verbosity/output-path changes didn't touch the actual generated code
at all, only how and where it's surfaced.

`README.md`'s "Trying it out" section rewritten to match -- it still
described the old, always-verbose default.

## A real milestone, a man page, and `make install`

Matthew ran `sprite.cpp` -- the very first hand-written test that found
a real bug in this project, several rounds back, before `new` even
allocated real memory -- all the way through to a running Vircon32
cart. Worth marking explicitly: this is the first time the full chain,
`v32c++` through the real Vircon32 build process to an actual running
program, has been confirmed end to end. Everything built since then
(constructors, destructors, vtables, arrays, `break`/`continue`, the
verbosity work) has been in service of exactly this.

**Synced Matthew's own Makefile updates**: a new `$(BIN)` variable used
throughout instead of repeating `./$(BIN_DIR)/v32c++`; `make test`'s 32
commands switched from `2>&1 | tee out/sampleN.txt` to
`1> out/sampleN.txt 2>&1` -- genuinely silent during the build now
(`tee` still echoes to the terminal WHILE writing the file; a plain
redirect doesn't), consistent with this project's own new
silent-by-default philosophy from the verbosity round; and `clean`'s own
removal of the bison/flex-generated files commented out, so `make clean
&& make` no longer requires bison/flex to be available at all when
the existing generated files are still valid for the current grammar.

**`make install`/`make uninstall` added**, matching the specific
request: `install` depends on `all` (builds first if needed), copies
`bin/v32c++` to `~/bin/` (creating the directory if it doesn't exist
yet), and prints a one-line reminder that `~/bin` needs to be on `PATH`
to actually run it as `v32c++` from anywhere -- a real, common gotcha
worth naming rather than leaving silently. `uninstall` removes it again.
Both tested directly with a temporary `$HOME` (not the sandbox's real
one) to confirm the actual copy/creation/removal behavior, not just
that the Makefile syntax was valid.

**A proper Unix section 1 man page** (`man/v32c++.1`), in real
troff/groff, not plain text formatted to merely resemble one --
NAME/SYNOPSIS/DESCRIPTION/OPTIONS/EXIT STATUS/EXAMPLES/LANGUAGE
NOTES/FILES/SEE ALSO/BUGS/AUTHOR, the conventional section-1 structure.
LANGUAGE NOTES is deliberately a brief summary pointing to the README
and `docs/DESIGN_NOTES.md` for the full picture, not a duplicate of
either -- a man page is supposed to be a quick reference, not the
complete story. Genuinely could not render this to actually confirm it
looks right end to end -- neither `man`, `groff`, nor `nroff` produced
usable output in this sandbox (`man`'s own local system is a minimized
install with `man-db` unavailable; installing `groff` directly was
blocked by the same network restriction already hit and confirmed
several rounds back), so this is reasoned-through, careful manual
review, not a rendered confirmation. Specifically checked: every
option's own dash uses the conventional `\-` escape (so it renders as a
literal hyphen-minus a reader could safely copy into a shell, not a
typographic en-dash) while ordinary mid-word hyphens were correctly
left as plain `-`; and every paired block macro (`.RS`/`.RE`, `.nf`/
`.fi`) is balanced. Stated plainly rather than assumed: this still
needs Matthew's own `man` (or `groff -man -Tascii`, or similar) to
actually confirm the rendering, the same category of verification gap
this project has hit before with anything requiring a tool this sandbox
doesn't have.

## Cart-packing XML generation, modeled directly on v32lua's own `emit_cart_xml`

Matthew provided v32lua's full source (`v32lua.c`, `.h`, `parser.y`,
`lexer.l`) specifically so this round could be modeled on an existing,
working implementation rather than designed from scratch -- investigated
v32lua's own `emit_cart_xml` directly before writing anything, rather
than guessing at the XML format or Vircon32's own expectations for it.

**Confirmed from v32lua's own source, not assumed**: `emit_cart_xml` is
called unconditionally (no opt-out exists in v32lua itself) right after
its own assembly output is successfully written, using v32lua's OWN
output filename (not the original `.lua`) to derive both the XML's own
filename (extension swapped for `.xml`) and the `<binary path="...">`
element it contains (extension swapped for `.vbin` -- the binary
Vircon32's own assembler/linker will eventually produce from v32lua's
assembly, the same role a v32c++-produced `.vbin` will eventually play
once the Vircon32 C compiler processes v32c++'s own `.c` output).
`<textures>`/`<sounds>` are non-empty only when v32lua's own `--#`
comment-based cart hints populated its texture/sound linked lists during
compilation; otherwise, self-closing empty elements. Exact format
transcribed directly from v32lua.c's own `fprintf` calls, not
approximated.

**v32c++ has no cart-hint functionality of any kind yet** -- Matthew's
own framing for this round -- so the new `emit_cart_xml` (`cartxml.c`)
is a deliberately narrower slice of v32lua's own: always empty
`<textures>`/`<sounds>`, and `title`/`version` always v32lua's own
documented defaults ("Vircon32 Program" / "1.0"), verbatim, for
consistency between the two sibling projects rather than inventing new
ones. No resource-list walking, no ID-consistency checking (v32lua's own
version warns if a resource's assigned ID doesn't match its position in
the list -- meaningless here, since nothing populates a list at all yet).
A real, deliberate scope boundary, not an oversight -- revisiting this
properly once cart-hint support exists in v32c++ too is real, separate
future work, not something silently deferred by omission.

**A small refactor made along the way, not just a new addition**:
`main.c`'s own `derive_output_filename` (from the verbosity round) does
the identical "swap the file extension" logic the new XML generation
also needs, for both the `.xml` path and the `.vbin` reference inside
it -- extracted into a new, shared `pathutil.c`/`.h`
(`replace_extension`, taking the new extension as a parameter) rather
than let `cartxml.c` duplicate the same logic a second time that could
quietly drift from the original. `main.c` now calls the shared version
too, in place of its own former private copy.

**`-x`/`--no-xml`**, matching the specific request: generation is
automatic whenever a `.c` file is actually written (matching "whenever
we output a .c file, XML generation should be activated" precisely --
runs regardless of `-c`, since a library/module fragment still gets its
own `.c` written the same as a complete program does), opt-out via
either spelling. Runs strictly AFTER the `.c` write already succeeded --
a failed XML write is reported to stderr but doesn't undo an otherwise-
successful transpile (`emit_cart_xml`'s own error path never touches
`rc`).

**Verified directly, both the specific behavior and the full suite**:
transpiled a real test file with `-v`, confirmed the generated
`<binary path="...vbin">` correctly points at the `.vbin` derived from
the ACTUAL output path used (not hardcoded), confirmed `-x` and
`--no-xml` both correctly suppress the `.xml` write while the `.c` still
gets written normally, and re-ran the full 32-sample suite: exactly the
7 expected failures, 25 `.c` files, and now 25 matching `.xml` files (one
per successfully-transpiled sample, including the ones using `-c` for
library fragments) -- confirming the feature applies consistently
across every successful invocation, not just the specific case tested
by hand.

`man/v32c++.1` gets a new CART PACKAGING section and a `.xml` FILES
entry; `README.md`'s "Trying it out" and feature-summary sections both
updated to mention this as a headline capability, matching how arrays
were introduced a few rounds back once that work was complete.

## Cart hints: `#texture`/`#sound`, mapped to `#define`d resource ids

Matthew asked specifically to explore this, not to start on a real
preprocessor -- a targeted, lexer-level special case for exactly two
directive forms, kept deliberately separate from (and handled before)
the existing generic `#`-line pass-through, not a step toward general
macro expansion or conditional compilation.

**Investigated v32lua's own `--#texture`/`--#sound` handling directly
before designing anything**, the same discipline as last round's cart-
XML work -- traced the full path: `make_node_cart_hint` parses a raw
hint string with `sscanf("%63s %127s \"%255[^\"]\"", ...)`, assigns
`next_texture_id++`/`next_sound_id++`, registers into a linked list via
`cart_resource_append` (confirmed: id == list position == XML position,
never stored as an independent field read back out later). The
genuinely useful discovery: v32lua's own codegen for a cart hint
(`node_cart_hint`) emits *runtime assembly* that initializes a real Lua
global variable to the resource's id -- because Lua has no compile-
time-constant concept of its own, so a runtime global is the only
option v32lua ever had. C isn't under that constraint. Matthew's own
phrasing ("#defines *or* variables") left the choice open; `#define`
was the right one -- a true compile-time constant, zero runtime cost,
more idiomatic C, and the first option he named.

**`driver.h`**: new `CartResource`/`CartResourceList` (name + filename
pairs, growable array, id implicit as position -- matching v32lua's own
invariant exactly), plus `g_cart_textures`/`g_cart_sounds` globals,
defined in `main.c` alongside the other driver-state definitions.

**`lexer.l`**: two new rules, `^"#texture"...` and `^"#sound"...`,
placed BEFORE the existing generic `^"#"[^\n]*` pass-through rule --
had to be first, deliberately: both match the same full line (same
length), and flex's own "first rule wins on a length tie" behavior
means rule ORDER in the file is what actually decides whether a
`#texture`/`#sound` line gets recognized or just falls through to
plain, unrecognized pass-through. Each rule `sscanf`s out NAME and the
quoted filename, then appends to the appropriate list via a small
`register_cart_resource` helper (mirroring `cart_resource_append`'s own
growable-list-append shape). Recognized hints do NOT also land in
`g_preprocessor_lines` -- a hint gets translated into a `#define`
instead of passed through verbatim, unlike a genuinely unrecognized
`#`-line. Caught, and fixed, before handing off: `sscanf` needs
`<stdio.h>`, which this file didn't explicitly include -- an easy thing
to miss relying on flex's own generated code happening to pull it in
transitively; added explicitly rather than left to chance.

**`codegen.c`**: new `emit_cart_hint_defines`, walking
`g_cart_textures` then `g_cart_sounds` (each in original order) and
emitting `#define NAME i` for each -- runs immediately after the
existing preprocessor pass-through, deliberately grouped with it (both
came from `#`-lines), ahead of anything this project itself goes on to
add.

**`cartxml.c`**: `emit_resource_list`, shared by both `<textures>`
and `<sounds>` (a `tag`/`ext` pair distinguishes them -- "texture"/
".vtex" or "sound"/".vsnd"), replacing last round's always-empty
placeholders. Falls back to the previous self-closing empty form when
a list has nothing in it, so a hint-free program's XML is byte-
identical to what it was before this round. Each filename's own
extension gets swapped via the SAME shared `replace_extension`
(`pathutil.c`) the `.xml`/`.vbin` derivation already uses -- one
implementation serving a third caller now, not a new copy.

**The critical thing verified before writing any of this**: does
`sema.c` reject `select_texture(Background)` when `Background` is
never declared anywhere in the C++ source itself (no local, no member,
just a name a `#texture` hint introduced)? Traced three places
directly, not assumed: `infer_expr_type`'s own `AST_IDENT` case returns
NULL (not an error) for an unresolved name; `check_node`'s own
`AST_IDENT` case only runs an access check when the name resolves to an
actual class member, and does nothing at all otherwise; `codegen.c`'s
own `AST_IDENT` case prints `e->str1` directly, no resolution required.
Confirms the design end to end: a `#define`'d name flows through this
project's own pipeline completely untouched, and resolves to its id
entirely at the C-compiler level once `codegen.c`'s own `#define` line
is in place -- no sema or codegen changes needed beyond what's listed
above.

**A real, honest limitation of this round**: flex still isn't
available in this sandbox (the same constraint this project has hit
every round that touched the grammar or lexer), so `lexer.c` could not
actually be regenerated or run here. Verified everything that COULD be
verified without it: full syntax-check of every OTHER changed file
(`driver.h`, `codegen.c`, `cartxml.c`, `main.c`) -- all clean; careful
manual review of `lexer.l`'s own new rules (regex patterns, `sscanf`
formats, the newline-handling convention matching the file's existing
rules exactly, confirmed against the existing `\n { g_lex_lineno++; }`
rule); and, to verify the REST of the pipeline still builds and links
correctly with the new globals/functions in place, temporarily touched
the stale, already-generated `lexer.c`/`parser.c`/`parser.h` to bypass
make's own (correctly-firing) regeneration check -- confirmed a clean
build and a full, regression-free 32-sample suite run against the OLD
lexer, plus the new `sample33.cpp` transpiling "successfully" but
inertly (its `#texture`/`#sound` lines just fall through to the old,
generic pass-through, exactly as expected, since the old lexer has no
idea these are special yet). This confirms everything EXCEPT the lexer
rules themselves. Matthew's own `bison -d`/`flex` regeneration (the
project's established pattern for every grammar/lexer round) is still
the real, only confirmation for those -- stated plainly, not glossed
over.

**`tests/sample33.cpp`** added and wired into the Makefile's `test`
target (its own `main`, no `-c` needed) -- two textures, two sounds,
deliberately mixed-case names (`Background`, `player`, `EXPLOSION`,
`jump_sfx`) to exercise "never forced to any particular case" directly,
not just claim it in a comment.

Documentation updated: `man/v32c++.1`'s CART PACKAGING section rewritten
with the full hint syntax, id-assignment rule, and what's still not
supported (`#title`/`#version` hints; no duplicate-name check yet --
same gap v32lua's own texture/sound handling has, not something this
round tried to exceed); `README.md`'s feature summary and "Trying it
out" section both updated to match.

## `#title`/`#version` hints, `-b` (BIOS mode), and `-g` (debug maps)

Three requests in one round: extend the cart-hint work with two more
hint forms, a new BIOS-specific transpile mode with its own validation,
and a genuinely new capability -- tracking, line by line, which C++
source line produced which line of generated C.

Matthew provided a regenerated `lexer.c`/`parser.c`/`parser.h` (from
last round's `#texture`/`#sound` additions) plus the results of running
`sample33.cpp` through them -- confirmed, directly, the full round-trip
this project could previously only verify by careful tracing: the AST
dump shows `Background`/`player`/`EXPLOSION`/`jump_sfx` as plain
`Ident` nodes (exactly the "flows through untouched" behavior the
`infer_expr_type`/`check_node`/codegen tracing predicted), the
generated `.c` shows the correct `#define`s in declaration order, and
the generated `.xml` populates `<textures>`/`<sounds>` correctly. A
real milestone -- the cart-hint design from two rounds ago is now
confirmed working end to end, not just reasoned through.

### `#title`/`#version`

Same targeted, lexer-level pattern as `#texture`/`#sound` -- two new
rules ahead of the generic pass-through, `sscanf`-parsed, stored in new
`g_cart_title`/`g_cart_version` (`driver.h`, both `char *`, NULL until
set). `title` is quoted (may contain spaces); `version` is a bare
token, matching v32lua's own `--#version` (confirmed: never quoted
there either). `cartxml.c` falls back to v32lua's own documented
defaults when either is still NULL. Last hint seen wins if either
appears more than once -- v32lua's own behavior too (it just overwrites
the same fixed buffer each time).

### `-b` (BIOS mode)

`cartxml.c`'s `emit_cart_xml` gained an `is_bios` parameter, controlling
only the `<rom>` element's own `type` attribute ("bios" vs
"cartridge") -- this function itself enforces none of the three BIOS
constraints; that's `main.c`'s own job, run BEFORE `emit_cart_xml` is
ever reached, so by the time it runs for a `-b` build those constraints
are already known to hold.

Generalized `sema.c`'s existing `decls_have_main`/`sema_program_has_main`
into `decls_have_function`/`sema_program_has_function(program, name)` --
one real behavior change (an arbitrary name, not just "main"), not a
new mechanism, so `error_handler` gets exactly the same "found anywhere,
namespace included" search `main` already gets.

New `validate_bios_constraints` in `main.c`, checked as a THIRD
condition in the same if/else-if/else chain `require_main` already
lives in (after sema succeeds, after the `main`-exists check, before
lowering/codegen). Reports EVERY violation it finds, not just the
first -- these are three independent constraints (texture count, sound
count, `error_handler` existence), and stopping at the first one just
means re-running to discover the second one at a time for no real
reason.

**Verified directly, not just reasoned through** -- five separate
test invocations covering every failure mode individually (zero
textures, two textures, two sounds, missing `error_handler`) plus one
combined case confirming multiple violations get reported TOGETHER in
a single run, not one at a time across repeated invocations. All five
produced exactly the expected error message and exit code.

### `-g` (debug maps)

New `debugmap.h`/`.c`: a `DebugMap` (growable array of `{c_line,
cpp_line, function_name}`), `debug_map_record` (append), `debug_map_write`
(format: `c_path,c_line,cpp_path,cpp_line[,function_name]`, matching
Matthew's own example `.asm.debug` file exactly -- confirmed by reading
it directly: a SPARSE table, one entry per point the mapping actually
changes, never a duplicate consecutive (line,line) pair, an optional
trailing function-name column).

**The real architectural question**: `codegen.c` writes through
`fprintf` from roughly 30 different emission functions (~155
`fprintf(out, ...)` call sites total), none of them tracking their own
output line number. Individually instrumenting every call site would
have been the obvious approach and also a genuinely bad one at this
scale -- high risk of missing one, no single place to reason about
correctness. Instead: confirmed (via `grep`, not assumption) that
EVERY one of those 155 call sites already targets `out` specifically
(nothing in this file ever writes to stderr or anywhere else), which
makes a single `#define fprintf tracked_fprintf` -- scoped to this one
translation unit, placed right after the `#include`s that declare the
real `fprintf` -- exactly equivalent to touching every call site, without
actually touching any of them. `tracked_fprintf` formats into a buffer
(stack-allocated for the common case; falls back to a correctly-sized
heap buffer on the rare chance a single call's output would exceed
1KB, rather than silently truncating and miscounting newlines), counts
`'\n'` occurrences in the ACTUAL formatted text (not derivable from
`vsnprintf`'s return value alone, which is why the buffer step exists
at all), then writes through unchanged. Returns the character count
`fprintf` itself would (NOT `fputs`'s own return convention, which
means something different) -- fixed after first getting this wrong,
before it became a real bug for any future caller that checks a
return value.

`g_codegen_out_line` (module-static, starts at 1, reset at the top of
`codegen_run` for hygiene even though this project is single-shot per
process today) always reflects the line about to be written NEXT.
Two hook points decide WHEN a new entry is worth recording:
`print_stmt` (records whenever the current statement's own source line
differs from the last one recorded -- fires for every statement kind,
`AST_BLOCK` included, which occasionally produces a harmless redundant
check that its own "only if changed" condition then skips) and
`emit_function_definition` (always records its own function's first
line, function-name column included -- a function boundary is worth
marking explicitly even on the rare chance its own line matches
whatever was last recorded for some earlier function). The function-name
column records the MANGLED name (what the .c file's own function is
actually called -- e.g. `Square__area__void`), not the C++ source's
own name, matching which side of the mapping that column documents.

`main.c`: `-g` writes `<output>.c.debug` -- APPENDED, not
extension-swapped (Matthew's own spec: "filename.c.debug" specifically,
not "filename.debug") -- via `debug_map_write(debug_filename,
output_filename, input_filename)`. `cpp_path` is always
`input_filename`, the ORIGINAL `.cpp` given on the command line, never
anything post-processed -- deliberately, per Matthew's own note about a
possible future, separate preprocessor tool: revisiting this becomes
that future tool's own concern (it would be what hands `v32c++` an
already-`#include`-resolved `.cpp`), not something this round tries to
anticipate further than recording the plain fact that it's a decision
worth revisiting then, not now.

**Verified directly against real output, not just described**: ran
`-g` on a `#texture`/`#sound` test file, then manually cross-checked
every single line of the resulting `.c.debug` against the actual
generated `.c` (with `cat -n`) line by line -- every entry correct,
including the function-name column on the `main` entry and the correct
skip of the redundant opening-brace line. Repeated with the BIOS test
sample (`error_handler`'s own mangled name, `error_handler__void`,
correctly appears) and again with `-b`/`-g`/`-x` combined together in
one run, confirming the three flags compose cleanly (BIOS validation
passes, XML correctly suppressed, debug map correctly written, all in
the same invocation).

### What's confirmed vs. what still needs Matthew's own lexer regeneration

Built and tested everything above against the `lexer.c`/`parser.c`/
`parser.h` Matthew provided -- but those predate THIS round's
`#title`/`#version` lexer rules (confirmed directly: grepped that
`lexer.c` for `cart_title`/`cart_version` before building against it --
absent, exactly as expected). So: `-b`'s three validation checks, `-g`'s
debug-map generation, and BIOS-mode XML's `type="bios"` are all
confirmed working directly, against real output. `#title`/`#version`
themselves are implemented and syntax-checked, but not yet confirmed
running -- a test with both hints present shows the XML falling back to
its defaults (title/version), and the raw, unrecognized `#title "..."`/
`#version ...` lines instead surface verbatim near the top of the
generated `.c` (harmless -- still just the existing generic pass-through
behavior working exactly as designed for anything the lexer doesn't yet
recognize -- but not the new feature actually firing). Needs a fresh
`bison -d`/`flex` regeneration from the `.l`/`.y` now in the sandbox,
the same as every round that's touched the lexer.

### Test coverage

`tests/sample34.cpp` added: a BIOS-shaped program exercising `#title`,
`#version`, one `#texture`, one `#sound`, and both `main`/
`error_handler` together -- run with `-b -g` in the Makefile's own test
target. `sample33.cpp`'s own test command gained `-g` too, so the debug-
map path is exercised by the standard suite go forward, not only by
hand. Makefile's own "which samples have their own `main`" comment
block updated to include both.

## `#title`/`#version`/`-b`/`-g` confirmed working end to end, and `-vvv` explanatory comments

Matthew provided a fresh `lexer.c`/`parser.c`/`parser.h` regenerated
from this round's `#title`/`#version` lexer additions, plus the actual
output of running both `sample33.cpp` and `sample34.cpp` through them.
Confirmed directly, not just re-asserted: `sample34.xml` shows
`title="Vircon32 BIOS Test" version="0.9"` and `type="bios"`, exactly
matching the source's own `#title`/`#version` hints and `-b` flag —
the entire cart-hint arc from the last two rounds (`#texture`/`#sound`,
then `#title`/`#version`, then `-b`'s own validation) is now confirmed
working end to end against a real build, not only reasoned through.
Rebuilt and re-ran the full 34-sample suite against these files:
identical result to every previous confirmation, exactly the 7 expected
failures.

## `-vvv`: explanatory comments in the generated C

Matthew's own framing: enhance the tool's learning value by having the
generated C explain itself, at `-vvv`, wherever the C++-to-C
transformation is least obvious to someone reading the output rather
than the original source. `-vvv` had been sitting as a documented,
literal no-op (behaving identically to `-vv`) since the verbosity
round specifically for this kind of future use -- this is that use.

**Detection strategy, decided before writing any instrumentation**: by
the time `codegen.c` runs, lowering has already baked its own
synthesized constructs (a vtable-pointer assignment in a constructor, a
destructor call at scope exit, a virtual dispatch through `->vtable->`)
into ordinary-looking AST nodes with no dedicated flag marking them as
synthesized. Two ways to recognize them at codegen time: thread a new
"why this node exists" flag through from lower.c (robust, but touches
ast.h and potentially every lowering phase that synthesizes anything),
or recognize them STRUCTURALLY, by the reserved naming conventions this
project's own lowering already uses and C++ source could never itself
produce -- a `vtable` field name (not a declarable C++ member), a
`v32_new_`-prefixed call, a `__dtor__void`-suffixed mangled name, a
`->vtable->` member-access chain. Went with the second: every one of
these patterns is 100% unambiguous (none is a valid C++-source-level
name a person could type), so name/shape-based detection is exactly as
reliable as a dedicated flag would have been, at a fraction of the
footprint -- confined entirely to `codegen.c`, nothing else touched.

**Threading the flag through**: `codegen_run` gained a `verbose_comments`
parameter (main.c passes `verbosity >= 3`), stored in a module-static
`g_verbose_comments`, reset at the top of every run -- same pattern
`g_codegen_out_line`/`g_debug_last_cpp_line` already established last
round for exactly this kind of "avoid threading a new parameter through
~30 emission functions" problem. One new helper, `explain(out, indent,
comment)`: a plain, single-line C block comment at the given
indentation, a complete no-op when `g_verbose_comments` is false --
every call site stays a plain, unconditional one-liner rather than
wrapped in its own `if`. Goes through the same `tracked_fprintf` (via
the `#define fprintf` from last round) every other emission in this
file does, so `-g`'s own line-count tracking correctly accounts for
comment lines shifting everything after them -- confirmed this
matters, not just assumed, since `-g` and `-vvv` can be given together.

**What actually got instrumented**, chosen for where the transformation
is least obvious, not exhaustively everywhere a comment COULD go:
the vtable pointer field and the "fields above this point are
inherited" boundary in `emit_struct`; the vtable struct type and the
shared instance in `emit_vtable_struct`/`emit_vtable_instance`; the
explicit `this` parameter, explained once at a method's own definition
(not also at its prototype, which is one line and gets no benefit from
a multi-line comment) in `emit_function_definition`; the
allocator/deleter functions themselves, and virtual destructor
dispatch specifically, in `emit_new_delete_runtime`/
`emit_delete_runtime`; three call-SITE explanations in `print_stmt` --
a destructor invoked automatically at scope exit, a virtual call
dispatched through the vtable (both in the `AST_EXPR_STMT` case), and
a `new`-lowered allocator call (in the `AST_VAR_DECL` case, unwrapping
an `AST_CAST` first when phase 6a's own implicit-upcast insertion put
one there).

**A real bug found by actually reading a `-vvv` run's output, not
missed**: `emit_struct`'s own per-field `fprintf(out, "    ")` used to
run BEFORE the `FIELD_VTABLE_PTR`/data-member branch, so inserting an
`explain()` call inside that branch put it AFTER the indent had already
been written -- `explain()`'s own trailing newline started a fresh
line the old indent was no longer positioned to cover, leaving the
FIELD line itself with zero indentation (`Shape_VTable *vtable;` at
column 0, not 4). Caught by generating real output against
`tests/sample32.cpp` and reading it line by line, not by inspection of
the code alone. Fixed by moving `explain()` (and its own dedicated
indent) BEFORE the field's own `fprintf(out, "    ")`, so each keeps
its own line and its own correct indentation.

**Verified thoroughly after the fix**: re-generated `sample32.cpp`'s
output with `-vvv` and read the WHOLE file end to end -- 14 comments
across every instrumented point (vtable types/instances/pointer field,
inherited-fields boundary, `this`-injection on every method, both
allocator functions, both deleter functions with virtual-dispatch
explanation, the `new`-lowered call site, and the virtual-dispatch call
site), every one at the correct location with correct indentation.
Confirmed NO leakage at lower verbosity: grepped default-verbosity and
`-vv` output for any `explain()`-style comment -- zero matches at
either level, and default-verbosity output diffed byte-identical
against the last confirmed-correct `sample32.c`, confirming this round
changed nothing about what `v32c++` already produced for anyone not
using `-vvv`. Re-ran the full 34-sample suite: identical 7 expected
failures, no regressions.

`tests/sample32.cpp`'s own Makefile test command upgraded from `-vv` to
`-vvv` specifically (the one sample in the suite exercising this
feature -- chosen for already having the richest relevant feature mix:
vtables, a virtual destructor, `new`/`delete`). Every other sample stays
at `-vv` deliberately -- `-vvv`'s comments would only clutter their own
review value for no real benefit.

`man/v32c++.1` gets a new EXPLANATORY COMMENTS section and an updated
`-vvv` OPTIONS entry (no longer "reserved for a future release");
`README.md`'s feature summary and "Trying it out" sections both updated
to match, the latter pointing at `sample32.cpp` as a first thing to try
`-vvv` on.

## Polish round: `-vv`/`-vvv` swapped, README audit, version bump to 20260915-dev

Three smaller, usability-focused requests, closing out this stretch of
CLI/output-quality work before returning to C++ language features.

**`-vv` and `-vvv` swapped**: `-vv` is now the explanatory-comments
level (was `-vvv`), `-vvv` is now the AST/semantic-analysis/lowering
dumps level (was `-vv`) -- Matthew's own framing: `-v` progress, `-vv`
comments, `-vvv` deep internals, each level strictly more detail than
the last, which reads more naturally than the previous ordering. Four
call sites in `main.c` swapped (`ast_dump`/`sema_dump`/`lower_dump`
gated on `verbosity >= 3` now, `codegen_run`'s own `verbose_comments`
argument on `verbosity >= 2`), `print_usage`'s own text rewritten to
match, and every `-vvv` reference in `codegen.c`'s own comments (the
ones describing WHY a given `explain()` call exists, not the levels
themselves) updated to say `-vv`.

**A real, inherent side effect of this swap, not a bug**: `-vvv` is
still cumulative (includes everything `-vv` does), so `-vvv` now
ALSO includes the explanatory comments, not just the dumps -- there is
no longer a way to get "dumps only, no comments," since dumps moved to
the top of the stack. Every sample in the Makefile's own `test` target
needed `-vv` upgraded to `-vvv` to keep capturing the dumps this suite
has always relied on for review (regenerated programmatically again,
same approach as previous rounds needing a bulk, consistent edit across
every sample line) -- and as a direct consequence, every sample's own
generated `.c` in `out/` now carries explanatory comments too, not just
sample32's (which no longer needs its own special-casing -- removed the
now-stale Makefile comment explaining why sample32 alone got the higher
flag). Verified directly: reran the full 34-sample suite (identical 7
expected failures, no regressions) and confirmed `-v`/`-vv`/`-vvv`
individually produce exactly the expected combination of stage
messages, comments, and dumps -- not just that the build succeeded.

**README audit, two confirmed-stale claims removed**: `break`/`continue`
were still listed under "What doesn't exist yet ... aren't in the
grammar at all yet" -- false; this has been implemented and tested
for many rounds now (sample30/31 specifically exercise it, including
its own destructor-at-scope-exit interaction). Virtual destructor
dispatch was still listed as broken ("calls the ancestor's destructor,
not the derived one") -- also false, and confirmed fixed and working
against the real Vircon32 C compiler two rounds ago (sample32). Both
bullets removed outright, not softened -- they were wrong, not merely
outdated in wording. Added a positive mention of both instead, in the
more detailed "Parsing and semantic analysis"/"Lowering" paragraphs
where the other implemented features are already described (loop-depth
validation for `break`/`continue`; correct destructor scoping at a
`break`/`continue`, not just at `return`/fall-through). Also swept the
whole file for every remaining `-vv`/`-vvv` reference to bring them in
line with the swap above (the "Trying it out" section's own multi-
paragraph explanation, and the feature-summary paragraph introducing
`-vv`'s comments).

**`v32cxx.h`'s `VERSION` bumped to `20260915-dev`**, per Matthew's own
stated convention of tracking the current date. Searched the whole tree
for every OTHER place the old string appeared, not just the obvious
one: `man/v32c++.1`'s own `.TH` line (updated), and two mentions in
THIS file's own history (the round that introduced `--version`,
documenting `20260914-dev` as that round's own starting value and what
`--version` printed at the time) -- left those alone, deliberately: this
file is a chronological log, not a living reference, and editing a past
entry to reflect a LATER version would misrepresent what actually
happened in that round.

## Base-class constructor delegation (`: Base(args)`)

Back to C++ language coverage after a stretch of CLI/tooling polish.
Matthew asked directly whether this was a worthy next target; the
genuinely compelling reason isn't style -- if a base class's own data
members are `private` (the properly encapsulated way to write one), a
derived class's constructor has NO legal way to initialize them at all
under this project's previous behavior (manually setting inherited
fields directly in the derived constructor's own body). This is a real
functional hole for anything written with proper encapsulation, not
just an awkward workaround.

**Scope, decided up front and stated to Matthew before writing any
code**: base-class delegation only (`: Base(args)`), not general
member-initializer syntax for ordinary fields (`: x(val)`). The two are
closely related but meaningfully different pieces of work -- ordinary
member initializers, especially for class-typed members, would need
their own real design (when does a member's own constructor get
invoked automatically?), not something to fold in as an afterthought.

**Confirmed reachable today, checked directly rather than assumed**: an
in-class constructor DEFINITION (a body written directly inside the
class, not just a prototype) is already grammatically possible via
`func_def`'s own existing `TYPE_NAME '(' ...' ')'` alternative in
`func_header` -- no test happens to use this style (every existing
constructor is declared in-class, defined out-of-line), but since the
grammar already allows it, the member-initializer-list syntax was added
to BOTH paths (`func_def` and `out_of_line_def`'s constructor
alternative), not just the one this project's own tests happen to use.

**AST**: two new kinds, `AST_MEMBER_INIT_LIST` (list=entries) and
`AST_MEMBER_INIT` (str1=name, list=call-style arguments), stored in
`AST_FUNC_DEF`'s previously-unused `c` slot -- `NULL` when no `: ...`
was written at all, matching how `b` is `NULL` for an in-class
definition (a null-means-absent convention this file already uses).
`ast_dump` needed no changes at all -- confirmed it already walks
`a`/`b`/`c`/`d` generically; only `kind_name`'s own switch needed the
two new cases (this project's own `-Wswitch`-relevant style requires
every `AstKind` listed explicitly, no `default:` to silently swallow a
missed one).

**Grammar**: `member_init` accepts BOTH `TYPE_NAME '(' args ')'` (a
registered class name -- base-class delegation, the case actually
acted on) and `IDENTIFIER '(' args ')'` (an ordinary field name -- not
yet acted on) at the SYNTAX level, deliberately -- rejecting the
member-field form as a parse error would be a confusing experience for
someone writing perfectly valid C++ that just isn't supported yet;
accepting it syntactically and giving a clear, explicit sema.c error
("not yet supported... initialize in the constructor body instead") is
a meaningfully better failure mode. A GENUINE, STATED RISK: this
project's grammar has a `%expect 22` directive (this file's own
documented protocol: bison ERRORS, not warns, if the actual conflict
count no longer matches, until `%expect` is updated to the new real
number). Adding an optional, comma-list-shaped production between a
constructor's own `')'` and its body could plausibly change this count
-- there's no way to check without bison itself, which this sandbox
still doesn't have. Told Matthew directly rather than silently hoping
it's still 22: if bison errors on the conflict count when he
regenerates, that's expected, and the file's own comment already
documents the recovery protocol (`bison -Wcounterexamples`, read every
new conflict, confirm real inputs still parse, then update the number).

**Semantic analysis**: new `resolve_member_init_list`, called from
`check_function_body` (once per constructor, params already bound to
`locals` by that point, since a delegated call's own arguments may
reference them). Reuses `resolve_overload_generic` -- the EXACT SAME
core `resolve_call`/`resolve_new_expr` already go through -- for
matching a `: Base(args)` entry against the base's own constructor
overloads, attaching a `CallResolution*` to the `AST_MEMBER_INIT`
node's own `sema_info` exactly like those two attach one to their own
site node. Three outcomes per entry: matches the class's own direct
base name -> resolved via the shared core; matches an actual declared
data member -> explicit "not yet supported" error; matches neither ->
"not a base class or member" error. A name matching a base class AND
(unusually) a same-named data member is treated as base-class
delegation, checked first -- matching real C++'s own rule here exactly,
not an arbitrary tie-break. A member-initializer list on anything other
than a constructor (an ordinary method, a destructor) is its own
explicit error too -- the grammar accepts the shape everywhere a
function body can appear (the same "parser accepts, sema.c diagnoses
misuse" split this project already relies on elsewhere, e.g. `break`
outside a loop), so this check is genuinely load-bearing, not
redundant with anything the parser itself already rejected.

**Lowering -- phase 8b**, immediately after phase 8 (vtable pointer
init), a real ordering dependency, not an arbitrary placement:  both
phases PREPEND a statement to the same constructor body, and the phase
that prepends LAST ends up FIRST in the final list -- phase 8 runs
first (vtable-init prepended), phase 8b runs right after (base-ctor-
call prepended), so the final order is [base-ctor-call, vtable-init,
...original body], matching real C++'s own construction timing (a base
subobject, vtable pointer included, is fully constructed before the
derived class's own vtable pointer overwrites it, which happens before
the derived constructor's own body runs). `this` is cast to `Base *`
via the EXISTING `cast_receiver_if_needed` helper (already relied on
for virtual dispatch and `delete` through a base-typed pointer) --
safe under this project's own struct-flattening strategy, where a
`Derived *` and `Base *` to the same object always share a compatible
leading-fields layout. Confirmed SAFE for this phase to run ahead of
`finalize_calls_classes` (further down the pipeline) even though both
touch constructor bodies: traced `finalize_call`'s own first line --
`if (cr == NULL || cr->resolved_target == NULL) return;` -- and the
`AST_CALL` phase 8b builds directly never has a `sema_info` set on it
at all, so even if `finalize_calls_classes` later walked over it,
touching it would be a guaranteed no-op, the identical protection
phase 7's own directly-built calls already rely on (confirmed by
reading, not assumed by analogy).

**Explicitly, deliberately NOT done this round**: no IMPLICIT
base-constructor call. In real C++, a derived class with a base class
but no explicit `: Base(...)` still gets the base's own default
constructor called automatically. This project's existing behavior
(nothing called at all) is already a deviation from real C++ semantics
-- but introducing an implicit call now, for every existing base class
with a zero-arg constructor, is a genuine BEHAVIOR CHANGE for already-
working code this sandbox has no way to verify without a working build
(the grammar change alone already blocks that). Left as a real, stated
gap for a future round, not silently mishandled -- documented in both
`README.md` and `man/v32c++.1`.

**Tests**: `tests/sample35.cpp` -- the actual motivating case, a
`private` base-class field a derived constructor has no other legal way
to set, with `area()` expected to return 16 (4*4), proving the field
was genuinely set through the base constructor rather than left as
garbage. `tests/sample36.cpp`/`sample37.cpp` -- deliberately invalid,
exercising the "unknown base or member" and "member-field initializer
not yet supported" error paths respectively (the second chosen
specifically because it's the failure mode someone is MOST likely to
actually hit in practice -- valid C++ they'd reasonably try, not an
obscure typo).

**Verified as much as this sandbox allows, stated plainly what
couldn't be**: full syntax-check across every changed file, and a
build+link using the EXISTING (pre-this-round) generated
`lexer.c`/`parser.c`/`parser.h` -- confirming `ast.c`/`sema.c`/`lower.c`
integrate correctly and the full 34-sample suite (everything before
this round's own new tests) still passes with zero regressions, since
none of those use a member-initializer list at all (`func->c` is
always `NULL` for them, and every new code path correctly no-ops on
that). Ran the three NEW tests against that same stale grammar too,
specifically to confirm they fail exactly and only at the new `:`
syntax (`"syntax error, unexpected ':', expecting '{'"`, at precisely
the line each one uses it) -- not some unrelated breakage, good
evidence the test files themselves are valid otherwise. The grammar
itself, and everything downstream of it (sema resolution, phase 8b's
own insertion, the private-base-field scenario actually working end to
end), still needs Matthew's own bison regeneration to confirm for
real -- stated directly, not glossed over.

## Base-class constructor delegation, confirmed working -- and a real bug found and fixed along the way

Matthew provided a fresh `lexer.c`/`parser.c`/`parser.h` (no bison
conflicts from last round's grammar addition -- `%expect 22` still
held) along with real output for `sample35`/`36`/`37`. Reading
`sample35.c` directly -- not just trusting the run succeeded -- surfaced
a real, serious bug: `Square__Square__int`'s generated body was
COMPLETELY EMPTY. `Shape__Shape__int(...)` was never called at all,
despite `sample35.cpp` writing `Square::Square(int side) : Shape(side)`
explicitly. The whole point of this feature, silently not happening.

**Root cause, found by direct debugging, not guessed at**: added
temporary `fprintf(stderr, ...)` tracing to the new lowering phase,
confirmed `m->c` was `NULL` at the exact point the phase checks it --
even though the parse-time AST dump (in `sample35.txt`, `-vvv`'s own
output) clearly showed `c: MemberInitList` correctly attached to the
constructor node right after parsing. Something was clearing it between
parse and lowering. Traced it to `attach_out_of_line` (`sema.c`) --
this project's existing machinery for merging an out-of-line
definition (`Square::Square(...) { ... }`) into its in-class prototype
node, which is what actually becomes the AUTHORITATIVE node stored in
`ClassLayout.methods` (per that function's own comment: "the
authoritative copy is now reachable via the class's member list").
That merge only ever copied `target->a = n->a` (the body) -- `target->c`
(this round's own new field) was never copied at all, since
`attach_out_of_line` was written long before `c` existed. Every
existing constructor in this project's own test suite is declared
in-class and defined out-of-line -- this project's OWN established,
exclusive style -- so this wasn't a rare edge case: it silently broke
the feature for its own primary, intended use case, 100% of the time,
for every out-of-line constructor that used it. `resolve_member_init_list`
(sema.c) was ALSO silently affected the same way (it runs on the same
authoritative node), though its own no-op-on-NULL guard meant it simply
never ran at all for an out-of-line constructor, rather than crashing
or misbehaving -- which is exactly why this surfaced as "nothing
happened" instead of a crash, and why it needed real generated output
to catch rather than code review alone.

**Fix**: one line, `target->c = n->c;`, added right next to the
existing `target->a = n->a;` in `attach_out_of_line`.

**A second, unrelated problem hit along the way, worth recording**: the
first attempt to verify the fix (a `touch src/sema.c src/lower.c` +
`make all`, not a full clean rebuild) produced wildly garbled output --
`void [0] this`, `while ()` -- classic stale-object-file/enum-mismatch
symptoms this project has hit before (documented in the Makefile's own
`clean` target comments). Not actually caused by anything new this
round; caused by not doing a full `make clean` before re-verifying after
swapping in the newly-regenerated grammar files earlier in the session.
A genuinely clean rebuild (`make clean` + full `make all`) produced
correct, sensible output immediately. Recorded here as a reminder, not
just a one-off inconvenience: this project's own established discipline
("any `ast.h` enum change needs a truly full rebuild") applies just as
much when SWAPPING IN a newly-generated `lexer.c`/`parser.c` mid-session
as it does to an enum edit itself, and skipping it produces symptoms
easy to mistake for a NEW bug rather than a stale build.

**Verified thoroughly after the real fix**: `sample35.c` (clean build)
now shows `Square__Square__int` correctly calling
`Shape__Shape__int(((Shape *)this), side)` as its sole statement, with
the surrounding `main()` also correct end to end (`new Square(4)`,
`sq->area()` called through the correctly-cast receiver, `delete sq`).
Full 37-sample suite (34 previous + this round's 3 new ones): exactly
the same 7 pre-existing expected failures, `sample35` now correctly
succeeds, `sample36`/`37` correctly fail with precisely their own
intended messages (confirmed directly: `'NotARealThing' is not a base
class or member of 'Widget'`; `member initializers for ordinary fields
aren't supported yet -- initialize 'value' in the constructor body
instead`) -- a stdout/stderr buffering quirk (unbuffered stderr writes
immediately, fully-buffered stdout only flushes at process exit, so an
error message lands at the TOP of a combined-redirect `.txt` file, not
the bottom) briefly looked like a missing error message on first
`tail`-based inspection; confirmed against `sample5.txt`, an
already-long-established error test, that this ordering is pre-existing,
unrelated project behavior, not something new or broken.

## Member-field initializers (`: x(val)`), with correct declaration-order semantics

The agreed-upon follow-up to base-class delegation: primitive-typed
member-field initializers, scoped up front to two explicit boundaries
-- no class-typed member support (invoking a member's own constructor
is real, separate complexity this project doesn't support anywhere
yet), and C++'s own declaration-order rule implemented correctly
rather than approximated with written order.

**`sema.c`**: `resolve_member_init_list` extended -- a data-member
match now branches on whether that field's own type is a BARE class
type (`resolve_typedef_chain` first, so a typedef to a class still
counts; deliberately NOT `type_to_class`, which unwraps pointer/
reference too and would have wrongly rejected a perfectly ordinary
`Foo *ptr;` member as "class-typed", since a pointer VALUE needs no
constructor call at all). A bare class type stays "not yet supported";
everything else requires exactly one argument (real C++'s own
direct-initialization rule for a non-class member) and gets marked
resolved via `entry->ival = 1` -- a lighter marker than the
`CallResolution*` base-class delegation uses, deliberately: there's no
overload to resolve for a primitive assignment, just a name and one
already-parsed argument expression. The two markers (`ival == 1` vs
`sema_info != NULL`) are mutually exclusive by construction, so
lower.c's two phases never conflict over the same entry.

**A second real bug, found proactively this time, not by luck**:
before writing any lowering code, checked whether `this`-injection
(phase 2, `this_inject_method`) already reached a member-initializer
list's own argument expressions -- it didn't, only the constructor
BODY (`method->a`). A member-init argument referencing another member
bare (`: y(x)`, not `: y(this->x)`) would have generated an
uncompilable, bare `x` reference in the output C, since C has no
implicit struct-field lookup the way this rewriting stands in for.
Exactly the same CLASS of bug as last round's `attach_out_of_line`
miss (old code that predates a new AST field simply not knowing to
touch it) -- caught this time by checking directly rather than
discovering it from broken generated output after the fact. Fixed:
`this_inject_method` now also runs `rewrite_expr` over every member-
init argument, using the same `locals` (constructor params only) the
body itself uses at that point -- correct, since a member-init argument
can only ever reference a parameter or a member, never a body-local
variable, which doesn't exist yet at this point in construction.
Verified directly: `: y(x)` now correctly generates
`this->y = this->x`.

**`lower.c` -- phase 8a**, member-field assignment insertion, placed
BETWEEN phase 8 (vtable-init) and phase 8b (base-ctor-call) in the
pipeline -- a real ordering requirement, not an arbitrary choice, using
the same prepend-order reasoning phase 8b's own doc comment already
established (whichever phase prepends LAST ends up FIRST in the final
body). Walks `layout->data_members` in DECLARATION order -- not
`m->c->list`'s own written order -- looking up a matching, resolved
(`ival == 1`) entry for each declared field in turn; an unlisted field
is simply skipped (unchanged, pre-existing behavior -- an un-listed
primitive member's own default-initialization in real C++ is none at
all). Final body order: `[base-ctor-call, member-inits (declaration
order), vtable-init, ...original body]`, matching real C++'s own
base-then-members-then-body construction timing for the two orderings
that have a real analogue to match (this project's own vtable-pointer
setup has no precise equivalent point in real C++'s own model, so its
position relative to member-inits is an implementation choice, not
something being matched to a reference the way base-before-members is).

**Verified thoroughly, every claim checked against real generated
output, not asserted**: the full 41-sample suite (37 previous + 4 new)
shows exactly 9 expected failures (the original 7, plus this round's
own `sample36`/`38`), zero regressions. `sample37` -- previously a
deliberately-invalid test for the "not yet supported" error this exact
syntax used to trigger -- repurposed into a genuine positive test now
that the syntax is supported, confirmed generating `this->value = v`
correctly. `sample38` (new) exercises the still-unsupported class-typed-
member boundary specifically, confirmed with the intended error message
verbatim. `sample39` (new) is the critical declaration-order check:
fields declared `y` then `x`, but the initializer list writes `x(a),
y(b)` (x first) -- confirmed the generated assignments are still
`this->y = b; this->x = a;`, declaration order, not list order.
`sample40` (new) combines base-class delegation and a member-field
initializer in the same list, confirmed the full combined ordering.
`sample41` (new) is a permanent regression test for the `this`-
injection fix specifically -- `: y(x)`, confirmed generating
`this->y = this->x`.

## A small, real gap found reviewing member-field initializers: duplicate entries

Matthew confirmed a clean build against `sample37`/`39`/`40`/`41` (all
five spot-checked directly against the actual generated C, not just
trusted from exit codes -- declaration order, the cross-member-
reference rewrite, and the combined base-delegation-plus-member-init
case all confirmed correct). Asked what's next and whether any gaps
remained; reviewing the whole feature end to end surfaced one real,
if small, one: `resolve_member_init_list` never checked for the SAME
field named twice in one list (`: x(a), x(b)`) -- real C++ treats this
as ill-formed outright, but this project's own linear "first match
wins" lookup (both in resolution and in `lower.c`'s own phase 8a) would
have silently picked one and said nothing.

Fixed with a small, dedicated pass at the top of
`resolve_member_init_list`, before the main resolution loop: an O(n^2)
scan (n is always small -- a member-initializer list realistically
has a handful of entries, never worth a hash-set for) reporting one
error per duplicate OCCURRENCE (the second, third, ... time a name
repeats), each at its own line. `tests/sample42.cpp` added, confirmed
producing exactly the intended message. Full 42-sample suite: exactly
10 expected failures now (the previous 9, plus this one), zero
regressions.

## Implicit base-class construction -- and two real, latent bugs it caught in the test suite itself

The agreed-upon follow-up to explicit base-class delegation: a derived
constructor that never writes `: Base(args)` at all still gets the
base's own zero-argument constructor called automatically, matching
real C++'s own rule -- closing the other half of what explicit
delegation deliberately left open two rounds ago.

**`sema.c` -- `check_implicit_base_construction`**, called alongside
`resolve_member_init_list` from `check_function_body` for every
constructor. Three outcomes: no base class at all (nothing to check);
already explicitly delegating (checked directly against `func->c`'s
own entries, not against anything `resolve_member_init_list` attached,
so the two checks stay independent of each other's ordering); or no
explicit delegation, in which case the base's own constructor set gets
inspected directly -- no constructor at all is fine (real C++'s own
implicitly-default-constructible rule, and this project's own
established "nothing to call" no-op everywhere else a class has no
constructor), but a base with SOME constructor and none of them
zero-arg is a genuine error, reported here rather than left as a
silently uninitialized base subobject.

**`lower.c` -- phase 8b extended**, not a new phase: the existing
base-ctor-call insertion now handles the implicit case too, reusing
`find_zero_arg_constructor` (already defined earlier in the file for
phase 7's own, analogous "stack-allocated local needs its default
constructor" question) rather than reimplementing the same lookup a
second time. By the time this phase runs, sema's own check has already
either confirmed nothing is needed or reported the real error, so
there's nothing left for lowering itself to diagnose.

**Two real, latent bugs this surfaced immediately on rebuild, not
edge cases invented to stress-test the feature**: `sample24.cpp` and
`sample32.cpp` -- the vtable-instance-population test and the
virtual-destructor-dispatch test, the latter specifically confirmed
against the real Vircon32 C compiler in an earlier round -- both had a
`Square` constructor that set `this->size` directly (legal only
because `size` is `protected`, not `private`) instead of delegating to
`Shape`'s own constructor at all. Real C++ would have rejected both
files outright from the start: `Shape` has no zero-argument
constructor, so `Square`'s own implicit base-construction was always
ill-formed -- this project simply had no way to notice until this
round's own check existed. Fixed with a one-line change to each
(`Square::Square(int side) : Shape(side) { }`, letting `Shape`'s own
constructor do what the body used to do directly) -- confirmed the fix
produces the exact same `this->size` value both ways, and re-verified
`sample32`'s own virtual-destructor-dispatch logic (`v32_delete_Square`
and its vtable-based dispatch) is completely untouched, exactly as
expected, since this round's changes only ever touch constructor
bodies.

**Verified thoroughly, both the feature itself and its side effects
on the existing suite**: three new tests specifically for this feature
-- `sample43.cpp` (the positive case: base has a zero-arg constructor,
derived doesn't mention it, confirmed the call `Base__Base__void(...)`
is correctly inserted ahead of the member-field-init assignment that
follows it) and `sample44.cpp` (base has NO constructor at all,
confirmed correctly generating no call whatsoever, the no-op case) and
`sample45.cpp` (deliberately invalid: base has only a non-zero-arg
constructor, confirmed the exact intended error message). Full
45-sample suite: exactly 11 expected failures now (the previous 10,
plus this round's own `sample45`), zero OTHER regressions -- confirmed
by actually running the full suite twice (once immediately after
implementing, which is what caught `sample24`/`32` in the first place;
once again after fixing both, confirming a clean run through to the
end).

## Bitwise operators and switch/case -- the first "basic C, not OOP" round

Matthew asked directly whether basic, non-OOP C syntax had gaps beyond
function pointers, framing a new use case explicitly: this project as
a general standard-C-to-Vircon32-C adaptor, not just a C++ subset.
Investigated the grammar directly rather than from memory (`grep`
across `parser.y`/`lexer.l` for every relevant token/keyword) before
answering, and the picture was bigger than expected: bitwise operators
were completely absent (not even the tokens existed for `|`/`^`/shift),
alongside `switch`, C-style casts, ternary, `do`/`while`, `enum`,
`union`, `goto`, bare `struct`, source-level `sizeof`, function-pointer
declarators, and multi-dimensional arrays. Agreed to start with
bitwise operators and switch, the two most consequential, then do a
fuller pass across the rest.

### Bitwise operators

**Confirmed before writing anything**: `&`, `|`, `^`, `~` are already
lexed as single-character fallback tokens (the existing
`.  { return yytext[0]; }` rule's own comment even lists them
explicitly) -- only the multi-character forms (`<<`, `>>`, and the five
compound-assignment forms) needed new lexer rules at all. Unary `~`
turned out to already be fully supported, grammar AND codegen both --
a genuine, pleasant surprise found by checking rather than assuming.

**Precedence**: this grammar uses `%left`/`%right` declarations rather
than a full precedence-level rule hierarchy, so getting C's own
precedence table right meant inserting the new operators at exactly
the right points in the existing list, not just adding productions.
Confirmed the famous "`a & b == c` means `a & (b == c)`" gotcha
directly as a test (`sample46.cpp`'s own `gotcha` variable), not just
asserted from memory -- chose a case where the two possible readings
actually produce DIFFERENT numeric results (`8 & 8 == 8`: correct
reading gives 0, the wrong one would give 1), so the generated value
itself proves which precedence a build actually used, not merely
which parenthesization.

**Zero changes needed in `codegen.c` or `sema.c`** for the core
feature -- confirmed by reading, not assumed: `AST_BINOP`/`AST_ASSIGN`
already print `str1` generically with no per-operator case, and
`resolve_operator_use`'s own first line (`if (op_name == NULL)
return;`) already makes an unrecognized operator (which `&`/`|`/`^`/
shift all are, by design -- not added to `binop_operator_name`'s own
overloadable set, matching `&&`/`||`'s existing treatment) a safe,
graceful no-op. Bitwise operator overloading (`operator&` and similar,
for this project's own OOP classes) was deliberately left out of scope
this round -- a "basic C" gap is what was asked about, and the
existing built-in-operator path already handles primitives completely
without it.

### `switch`/`case`/`default`

**Modeled as a flat, source-ordered list** (`AST_SWITCH`'s own `list`
holding `AST_CASE`/`AST_DEFAULT` labels interleaved directly with
ordinary statements, matching real C's own grammar shape exactly --
case/default are labels ON a statement, not containers holding their
own statement lists) -- deliberately, so real C's fall-through
behavior falls out of just walking the list in order, nothing this
project has to implement specially. Passed straight through to
`codegen.c` as literal `switch`/`case`/`default`, no lowering
transformation at all, since Vircon32 C already has this natively.

**The genuinely hard part was `break`'s interaction with `continue`
and destructor invocation**, not the parsing. Real C: `break` exits
the INNERMOST enclosing loop OR switch, whichever is closer;
`continue` always targets the nearest loop specifically, skipping
straight past any switch in between. This meant `sema.c` needed a
SEPARATE `g_sema_switch_depth` counter alongside the existing
`g_sema_loop_depth` (break valid under either; continue only under the
loop one), and -- the part requiring real care -- `lower.c`'s own
phase 9 (destructor invocation) needed a SEPARATE `break_boundary`
threaded alongside the existing `loop_boundary` through both
`destruct_scope_stmt` and `destruct_scope_block`, since a `break`
inside a switch nested in a loop needs to destroy only what's live
inside the SWITCH, not the whole loop, while a `continue` at that same
point still needs to reach past the switch to the loop's own boundary.
Traced the EXISTING `AST_WHILE`/`AST_FOR` pattern precisely before
writing the new `AST_SWITCH` case (the boundary passed to a loop body
is the scope the loop STATEMENT ITSELF lives in, not a newly-created
inner scope) and replicated it exactly for `break_boundary`, entering
a switch: only `break_boundary` becomes a new boundary; `loop_boundary`
passes through completely unchanged, which is precisely what makes
`continue` correctly skip past the switch to whichever loop actually
encloses it. Reused `destruct_scope_block` directly on the `AST_SWITCH`
node itself (not a dedicated walk) -- that function only ever reads/
writes `block->list`, never anything `AST_BLOCK`-specific, so a
switch's own flat body is exactly the shape it already knows how to
walk. One real, caught-immediately mistake along the way: an accidental
`check_node(n->a, NULL, NULL)` call for the discriminant expression --
`check_node` is a `sema.c` function, and this phase never touches
expressions at all (matching how `AST_WHILE`'s own condition is never
touched either) -- caught by the syntax-check step, not left in.

**A narrow, stated limitation, not silently glossed over**: a variable
declared directly in a switch body without its own `{ }` block (e.g.
`case 1: int x = 5; break;`) inherits real C's own notoriously tricky
scoping rules here (the variable's scope is the whole switch body, but
a case label can "jump over" its initializer) -- this project makes no
attempt to detect or specially handle that narrow, rare pattern; the
common, well-formed case (each case wrapping its own body in `{ }`
when it declares anything) works correctly via ordinary `AST_BLOCK`
nesting, unaffected.

**The `%expect` risk, flagged again**: this grammar change (a new
statement form, seven new tokens, restructured precedence
declarations) is very likely to change the conflict count from
whatever it currently is, on top of already having grammar changes
from two earlier rounds. Same recovery protocol as every previous
grammar round applies if bison errors on regeneration.

**Tests**: `sample46.cpp` (bitwise operators, including the precedence
gotcha with a value that actually distinguishes the two readings),
`sample47.cpp` (fall-through, with an observable, hand-traced result --
`classify(1)` returns 30 specifically because case 1 falls into case
2), `sample48.cpp` (a switch nested in a loop, `continue` inside it
confirmed to skip the switch and reach the loop, `break` inside the
same switch confirmed to target only the switch -- hand-traced to
`sum = 8`), `sample49.cpp` (deliberately invalid: `continue` inside a
switch with no enclosing loop at all, confirming the
`g_sema_loop_depth`/`g_sema_switch_depth` split genuinely distinguishes
the two rather than conflating them).

**Verified as much as this sandbox allows**: full syntax-check across
every changed file, a build against the EXISTING (pre-this-round)
generated grammar confirming every other file still integrates and
links correctly, and the full 45-sample suite re-run against that same
stale grammar with zero regressions. Ran all four new test files
directly against that stale grammar too, specifically to confirm each
fails EXACTLY and ONLY at its own new syntax (`sample46` at its first
`&`; `sample47`/`48`/`49` all at `switch (x) {`'s own unrecognized
`{`) -- not some unrelated breakage, real evidence the test files
themselves are valid otherwise. The grammar itself, and everything
downstream of it, still needs Matthew's own bison/flex regeneration to
confirm for real.

## Global variables (a real, confirmed bug fixed), `struct`, and C-style casts

Matthew's own confirmation of the bitwise-ops/switch round (all four
outputs spot-checked directly, precedence gotcha and the loop/switch
break-vs-continue distinction both confirmed correct) came with the
green light to continue the "basic C" pass: global variables first
(the open question from last round), then `struct` and casts.

### Global variables -- investigated, found a real bug, fixed

A direct test (`int counter = 0;` at file scope, referenced from a
function) surfaced a genuine, confirmed bug, not just an unsupported
feature: the declaration was silently DROPPED from generated output
entirely. `codegen_run` had no function walking top-level `AST_VAR_DECL`
nodes at all -- `emit_forward_declarations`/`emit_typedefs`/
`emit_classes`/etc never touched one. Any function referencing that
"global" produced C that referenced an undeclared identifier -- a real
downstream compile failure, not a graceful gap. `sema.c` had the
identical gap one level up: `collect_declarations` never registered a
global at all, so its own initializer expression (if it contained a
function call) would never get its `CallResolution` resolved, unlike
the identical expression as a local variable's own initializer one
function down.

Fixed both: `sema.c` gained `check_globals` (walks every top-level
`VarDecl`'s own initializer through `check_node`, wired into
`sema_run`); `codegen.c` gained `emit_globals` (reuses
`print_var_decl_inline` directly -- the C syntax for a global
declaration is identical to a local one), placed after `emit_classes`
so a class-typed global's own struct definition would already exist
(class-typed globals still aren't fully supported -- no constructor
gets invoked for one, the same gap class-typed member fields have --
but at least the DECLARATION itself is no longer silently dropped).
Re-ran the original failing test after the fix: `counter` now emits
correctly, referencing functions compile. `tests/sample50.cpp` added
as a permanent regression test. Full suite, zero regressions.

### `struct`

Real C++'s ONLY actual difference between `class` and `struct` is
default member access (private vs. public) before any explicit
`public:`/`private:`/`protected:` label -- everything else (vtables,
constructors, inheritance, access control once a label IS given)
already applies identically to both in real C++, and turned out to
already be true of this project's own `class` machinery too, since
none of it was ever conditioned on the keyword itself. Implementation
is correspondingly small: a new `ival` flag on `AST_CLASS_DECL` (0 for
`class`, 1 for `struct`), set by a new `class_or_struct_kw` grammar
production replacing the previously-hardcoded `CLASS` token at the
start of `class_decl`, read by exactly one line in `sema.c`'s
`compute_layout` (the `current_access` initializer). Confirmed directly
that no other file needed changes: `codegen.c` already always emits
`struct` in the generated C regardless of which keyword declared it
(Vircon32 C has no `class` at all, so this was already correct and
unrelated to this change); grepped `sema.c`/`lower.c`/`codegen.c` for
every place that mentions "class" in a comment or diagnostic message
and confirmed they're all using it in the general, standard C++ sense
(real compilers do the same -- "private member of class 'Foo'" is
standard diagnostic phrasing regardless of whether `Foo` was declared
`class` or `struct`), not something needing correction.

Three tests: `sample51.cpp` (a plain, C-style data struct with no
methods at all -- confirms both the default-public access AND that no
vtable/constructor machinery gets added just because the keyword was
`struct`, since that machinery was always conditional on actually
having virtual methods/constructors, never on the keyword itself),
`sample52.cpp` (a struct WITH a constructor and method, confirming
shared machinery works identically to `class`), `sample53.cpp`
(deliberately invalid -- a plain `class`, not `struct`, with the exact
same "access a field with no explicit `public:` label" shape, confirmed
STILL rejected -- proving `class`'s own default wasn't accidentally
weakened by sharing machinery with `struct`). `sample53` doesn't even
need the new grammar to confirm this -- it uses only `class`, already
recognized by the stale grammar -- and was run directly: produced
exactly the intended error, a genuine, real confirmation (not just
reasoned through) that `class`'s own behavior is unchanged.

### C-style casts

`AST_CAST` already existed -- lowering has synthesized one internally
for several rounds now (implicit-upcast insertion, receiver casts) --
so this was about making it reachable from user-written source, not
inventing new AST. Added to `unary_expr` (not `primary_expr`'s own
`'(' expr ')'`), matching real C's own `cast-expression: unary-
expression | '(' type-name ')' cast-expression` grammar exactly, which
is also what makes precedence correct for free (`(int)a + b` parses as
`((int)a) + b`, the cast binding to just `a`, since it's now part of
`unary_expr` specifically, not a full `expr`).

**Confirmed no genuine grammar ambiguity, not assumed**: checked
`primary_expr`'s own grammar directly before writing anything --  it
only ever accepts a bare `IDENTIFIER` as an expression-starting token,
never `TYPE_NAME`. So a `TYPE_NAME` (or a built-in type keyword)
immediately after `(` can only ever mean a cast is starting; it could
never be the start of a valid parenthesized expression instead, since
nothing in `expr`'s own first-set overlaps with `type_spec`'s.

**Two real, previously-latent gaps found by checking systematically,
not by accident**: before assuming `AST_CAST` was fully wired
everywhere, searched every function containing an `AST_UNOP` case (a
reasonable proxy for "expression-walking functions that should treat a
cast the same way") and checked each one directly. Two of four already
handled `AST_CAST` correctly (`fix_reference_access_expr`/phase 5,
`new_delete_rewrite_expr`/phase 6 -- both written defensively,
apparently anticipating this exact situation, per their own existing
comments: "handled on principle, not just for the cases seen so far").
The other two did not: `rewrite_expr` (phase 2, this-injection) and
`finalize_calls_expr` (phase 3, call finalization) both fell through to
a `default:` case that explicitly does nothing, with a comment
asserting the unhandled kinds "can't contain a `this` or a bare member
reference" -- true for literals, false for a cast's own wrapped
expression. Exactly the same CLASS of bug as the member-initializer-
list argument gap found two rounds ago (old code that predates a new
way for an AST shape to appear, simply not yet knowing to touch it) --
caught this time by checking proactively before it could surface as
broken generated output, not discovered from a failure after the fact.
Fixed both, matching the established single-child-recursion pattern
each function already uses for `AST_UNOP`/`AST_DELETE`. `sema.c`'s own
`check_node` (and `dump_calls_in_node`, the `-vv` summary walker) had
the identical gap for the same reason -- a user-written cast's own call
resolution (`(int)someVirtualCall()`) would never have been resolved at
all -- fixed the same way.

`tests/sample54.cpp` exercises all of this together: a primitive cast
(`(int)f`), a cast wrapping a bare member reference inside a method
body (`(int)size`, confirming the phase-2 fix -- this needs to become
`(int)this->size`, not a bare, undeclared `size`), and a cast wrapping
a virtual call through a pointer explicitly cast to a base type
(`(int)base->area()`, confirming the phase-3 fix reaches inside the
cast to finalize the virtual dispatch).

### Verification, across all three features

Full syntax-check on every changed file; a build against the EXISTING
(pre-this-round) generated grammar confirming every other file still
integrates and links; the full 50-sample suite (49 previous + the new
global-variable test, since that fix needed no grammar change and could
be verified directly) re-run against that build with zero regressions.
`sample51`/`52`/`53`/`54` (struct and casts, both needing the new
grammar) each confirmed to fail EXACTLY and ONLY at their own new
syntax against the stale grammar -- `sample51`/`52` at the unrecognized
`struct` keyword itself (lexed as a bare, unknown identifier, producing
a slightly different but still expected "expecting COLONCOLON" parse
error, not the "unexpected TOKEN" shape most other new-syntax failures
in this project produce -- worth noting the difference is cosmetic,
not a sign of anything wrong), `sample54` at its own first cast. The
grammar itself, and everything downstream of it, still needs Matthew's
own bison/flex regeneration to confirm for real -- the `%expect` risk
from every previous grammar round applies here too, now compounded
across four separate grammar-touching rounds without a regeneration in
between.

## Header reorganization (`src/*.h` -> `inc/*.h`), and a real float-literal lexer gap

Matthew confirmed all four struct/casts outputs directly (sample50-53
all correct as designed; sample53 in particular -- plain `class`,
needing no new grammar at all -- run directly and producing exactly
the intended error, real confirmation `class`'s own default wasn't
weakened) and, once again, zero new bison conflicts on regeneration.
Two things came with that: a standing preference that every `.h` live
in `inc/`, not scattered across `src/` (Matthew has been manually
separating them out by hand up to now -- this project's own layout
simply hadn't matched that expectation), and a real, confirmed bug in
`sample54.cpp` itself: `float f = 3.9f;` failed to parse.

### Header reorganization

All ten `.h` files that had been living in `src/` (`ast.h`, `cartxml.h`,
`codegen.h`, `debugmap.h`, `driver.h`, `lower.h`, `pathutil.h`,
`sema.h`, `symtab.h`, `v32cxx.h`) moved to `inc/`, alongside the
bison-generated `parser.h`, which was already there. Checked before
moving anything, rather than assumed: every `#include` in this project
already uses a bare, unqualified filename (`#include "ast.h"`, never a
path-prefixed one), and the Makefile's `CFLAGS` already carries
`-I$(INC_DIR)` with nothing anywhere hardcoding a `src/*.h` path. Both
facts together meant the move needed literally zero code or Makefile
changes -- C's own `"..."`-include search already falls back to the
`-I` path when a bare filename isn't found next to the including file,
so every existing `#include` line kept resolving correctly without
being touched at all. Confirmed with a genuinely clean rebuild
(`make clean` first) and the full suite, zero regressions.

Checked Matthew's own stated concern directly too -- whether any
LIVE, current-state documentation (README, man page) described headers
as living in `src/` -- and found none; the only historical mention
(`docs/DESIGN_NOTES.md`'s own entry from when `v32cxx.h` was first
created) is a chronological journal entry describing something that
was accurate AT THAT TIME, left untouched on purpose, consistent with
how this file has always been treated as append-only, not a live
reference to keep retroactively correct.

### The `3.9f` bug -- real, not intentional, fixed properly rather than just worked around

Confirmed directly before fixing anything: the float-literal lexer
rule (`{DIGIT}+"."{DIGIT}+([eE][+-]?{DIGIT}+)?`) had no suffix support
at all, so `3.9f` lexed as the float literal `3.9` followed by a
separate, unconsumed `f` -- explaining the exact "unexpected
IDENTIFIER, expecting ';'" error Matthew's own upload showed. Genuinely
Claude's own bug in the test file, not a deliberate edge case -- worth
being direct about, not glossing over.

Fixed as a real, small feature rather than just editing the test to
dodge it: real C++'s own `f`/`F` float-literal suffix (marking a
literal as `float` specifically rather than `double`) is now accepted
by the lexer, matched and simply ignored -- `atof()` itself already
stops at the first character it can't parse, so `atof("3.9f")` already
correctly yields `3.9` with no separate stripping needed. Harmless,
and arguably the more honest fix given this project's own type system:
there's no distinct `double` here for the suffix to ever have
disambiguated FROM (every floating-point value is already `float`), so
accepting it is purely for compatibility with real C++ source that
writes it out of habit, changing nothing about how the value itself is
treated.

**Verified as thoroughly as this sandbox allows, separating the two
concerns that were tangled together in the original failure**: since
the lexer fix itself needs flex regeneration to take effect (not yet
done), built a direct, minimal test isolating just the cast grammar
(`(int)f`, no float suffix at all) to confirm THAT part works correctly
independent of the suffix bug -- it does, cleanly. Then built a second
direct test: `sample54.cpp`'s own content with only the `f` suffix
stripped, run through the ACTUAL regenerated grammar, to get real
confirmation (not just reasoning) that both of last round's genuinely
tricky fixes work correctly together: `Shape::area()`'s own
`return (int)size;` correctly generated `((int)this->size)`, not a
bare, undeclared `size` (the this-injection/phase-2 fix); the virtual
call inside `(int)base->area()` correctly generated
`((int)base->vtable->Shape__area__void(base))`, going through real
vtable dispatch, not a plain direct call (the call-finalization/phase-3
fix). `sample54.cpp` itself was NOT edited to remove the suffix --
doing so would have thrown away the one thing actually still needing
verification (the suffix fix itself) in exchange for a test that could
already pass; it stays exactly as it was, still blocked purely on
Matthew's own next flex regeneration, at which point it should pass
outright with no further changes needed on either side.

## Numeric literal formats/suffixes, and C++-style casts (with an honest `dynamic_cast`)

Matthew confirmed `sample54` now builds correctly (the float-suffix fix
verified for real) and asked two direct questions: are there more
"suffix"-shaped gaps, and is C++-style casting (`static_cast` and
friends) in place alongside the C-style casts from two rounds ago.
Investigated both directly rather than from memory. Integer suffixes
were indeed missing, same shape as the float one -- but the bigger
finding was that hex/octal/binary literals didn't exist as separate
literal FORMATS at all, a different and arguably more consequential gap
for a fantasy-console target (colors, masks, flags are almost always
written in hex, and pair directly with last round's own bitwise
operators). C++-style casts were entirely absent -- zero matches for
any of the four keywords anywhere in the grammar. Matthew's own call on
scope: implement hex/octal/binary literals and integer suffixes;
implement `static_cast`/`const_cast`/`reinterpret_cast` properly;
accept `dynamic_cast` as a bare syntactic alias (parses, transpiles
identically to the others) but emit a transpiler WARNING, not silence
and not a hard error, since it doesn't actually perform the RTTI-backed
runtime check real `dynamic_cast` promises.

### Numeric literals

Confirmed directly, not assumed: the existing integer-literal rule
(`{DIGIT}+`) had no suffix support and no hex/octal/binary format at
all -- only plain decimal. Four rules now: hex (`0[xX][0-9a-fA-F]+`),
binary (`0[bB][01]+`), octal (`0[0-7]+`), and plain decimal, each with
an optional, permissive `[uUlL]*` suffix matched and ignored -- the
exact same "the parsing function already stops at the first character
it can't handle" reasoning the float suffix fix relied on last round,
just with `strtol()` in place of `atof()`. `strtol()` with an explicit
base of 16 already knows to skip a leading `0x`/`0X` itself; base 2
does NOT get the same courtesy (that convenience is specific to base 16
and base 0's own auto-detection per the C standard), so the binary
rule skips the `0b`/`0B` prefix explicitly (`yytext + 2`) before
calling it; base 8 needs no special handling at all, since a leading
`0` is just an ordinary base-8 digit to `strtol()`.

**A genuine flex ordering subtlety, reasoned through carefully, not
guessed at**: the octal rule had to be listed BEFORE the plain decimal
rule in the file. Flex's own longest-match-wins only decides between
rules of DIFFERENT match length; for two rules that would match the
identical text with the identical length (`013` matches both the octal
rule and, if it came first, the decimal one), flex breaks the tie by
rule ORDER, first-listed wins. Get this backwards and `013` would
silently (and wrongly) come out as decimal 13 rather than octal 11 --
confirmed the ordering in the file directly before considering this
settled, not just reasoned through in the abstract.

**Deliberately permissive on the suffix**, not strictly validating
which combinations real C++ allows (`lu` is valid, `uu` isn't, etc.) --
this project has exactly one integer type, so the suffix changes
nothing regardless of which letters appear or their order; matching
real C++'s own exact grammar here would cost real effort for zero
behavioral difference. Same reasoning extends to octal: a leading `0`
followed by an invalid digit (`089`) isn't flagged as the "invalid
digit in octal constant" error real C++ would give -- it simply falls
through to the decimal rule instead (silently read as decimal 89) --
consistent with this project's established best-effort philosophy
elsewhere: not pedantically strict, but never silently wrong about the
COMMON, well-formed case.

**Binary literals flagged clearly as a C++14 addition, per Matthew's
own explicit request**, not something every C++ standard has -- called
out in the README and man page both, not just here, so a user targeting
something that predates C++14 knows this is a convenience this project
adds, not a guarantee their own toolchain elsewhere would share.

`tests/sample55.cpp` exercises all four formats plus several suffix
combinations, with every expected value computed by hand in the test's
own comment.

### `static_cast` / `const_cast` / `reinterpret_cast`

All three collapse to the exact same `AST_CAST` node the existing
C-style cast already builds -- confirmed directly (not assumed) that
this is the semantically correct simplification, not a shortcut:
real C++'s own distinctions between the three (compile-time-only, no
runtime check for any of them) have nothing left to distinguish once
the target is C, which has no notion of any of these cast KINDS,
Vircon32 C included. Implementation: a new shared `cpp_cast_kw`
nonterminal (matching `class_or_struct_kw`'s own established pattern)
distinguishing which of the four keywords was used, feeding a single
new `unary_expr` alternative
(`cpp_cast_kw '<' type_spec pointer_opt '>' '(' expr ')'`) that builds
the identical `AST_CAST` shape the C-style cast production does.
`codegen.c` needed zero changes -- from its own perspective a
`static_cast<int>(x)` and a `(int)x` are now literally the same AST
node.

**Confirmed no genuine grammar ambiguity with `<`/`>` as comparison
operators**, not assumed: the new production only ever matches
immediately after one of the four fixed cast keywords, a grammar
position ordinary comparison-operator usage never occurs in -- this is
a fixed, four-keyword special form, not a general
`identifier < args >` production the way an actual template
instantiation would need, and doesn't open any door toward template
support (still never planned, by design).

### `dynamic_cast` -- honest about the gap, not silent about it

Bare syntactic alias, per Matthew's own direction: parses, transpiles
identically to the other three (same `AST_CAST` node), but marked
(`ival=1` on the node, checked by `sema.c`'s own `check_node`) so a
NEW, genuinely new diagnostic category can flag it -- a WARNING, not an
error. This project had no warning mechanism at all before this --
only `sema_error` (fatal, increments a count `main.c` checks to decide
pass/fail). Added `sema_warning` as a deliberate near-twin of
`sema_error` (identical shape: line-prefixed stderr message, varargs)
but tracked in a fully separate `g_warning_count`, with its own
accessor (`sema_get_warning_count()`, same "call only after
`sema_run()`" convention as `sema_program_has_main`) that `main.c`
checks independently and prints a summary line for
(`---- N warning(s) in FILE ----`) regardless of whether the overall
transpile succeeded -- a warning never sets the failure exit code,
matching Matthew's own request that this inform the user without
stopping the build.

The reasoning for warning rather than choosing either extreme:
silently accepting `dynamic_cast` and transpiling it as a plain cast
would risk someone relying on a safety guarantee (the RTTI-backed
runtime check, returning NULL on a failed downcast) that was never
actually implemented -- this project has never supported RTTI, by
design, and that hasn't changed. A hard error would refuse to
transpile code real C++ accepts, for a construct this project genuinely
CAN still do something reasonable with (an ordinary cast). A warning is
the honest middle ground Matthew asked for.

`tests/sample56.cpp` exercises `static_cast`/`const_cast`/
`reinterpret_cast` together (a primitive truncation plus a pointer
upcast-then-cast-again through a class hierarchy, confirming neither
cast interferes with virtual dispatch any differently than the
C-style cast already didn't). `tests/sample57.cpp` exercises
`dynamic_cast` specifically -- expected to produce exactly one warning
and STILL succeed (exit 0, a real `.c` file generated), confirming a
warning genuinely doesn't block the build the way an error would; it
deliberately does NOT get a `-` prefix in the Makefile despite
producing warning output, for exactly that reason.

### Verification

Both of my own newly-added grammar blocks (the `cpp_cast_kw`
nonterminal and its `unary_expr` production) checked directly for
balanced parens/braces in isolation, rather than trusting a whole-file
count -- this file's own extensive prose commenting makes a naive
whole-file paren count meaningless noise (natural-language parentheticals
like "(see below" routinely appear without a matching close in the same
comment), so the file-wide count was deliberately not treated as a
signal either way; the two blocks that actually changed were checked on
their own and came back balanced. Full syntax-check across every
changed file; a build against the EXISTING (pre-this-round) generated
grammar confirming every other file still integrates and links; the
full 54-sample suite re-run against that build with zero regressions.
`sample55`/`56`/`57` each confirmed to fail EXACTLY at their own first
new-syntax line against the stale grammar (`sample55` at its own first
hex literal; `sample56` at `static_cast`'s own first use; `sample57` at
`dynamic_cast`'s own first use) -- not some unrelated breakage. The
grammar itself, and everything downstream of it -- crucially including
whether `sema_warning`'s new machinery actually fires and prints
correctly for `dynamic_cast`, which nothing in this sandbox can confirm
without a real build -- still needs Matthew's own bison/flex
regeneration to confirm for real.

## Ternary, do-while, and enum -- plus a real stale-object-file lesson worth remembering

Matthew confirmed the dynamic_cast warning fired correctly (exactly
once, build still succeeded) and greenlit the next batch: ternary,
do-while, and enum together.

### Ternary (`?:`)

New `AST_TERNARY` (a=cond, b=true-branch, c=false-branch). The real
design work was precedence, not the AST shape. This grammar uses flat
`%left`/`%right` declarations rather than a full precedence-level rule
hierarchy, so placing `?` correctly meant a new `%right '?'` level
between assignment (loosest) and `||`, matching real C++'s own
conditional-expression placement -- and, critically, an EXPLICIT
`%prec '?'` on the grammar rule rather than trusting bison's own
default (the last terminal in the rule, which would otherwise be `:`).
`:` is reused across many unrelated contexts elsewhere in this grammar
(switch/case labels, access specifiers, base-class lists, member-init
lists) with no precedence declared for it anywhere -- leaning on its
implicit precedence here would have been both meaningless (it has
none) and a real risk of entangling this new rule with all those
unrelated ones. `?` itself was confirmed completely unused anywhere in
the grammar before this, and turned out to already be mechanically
lexed by the existing single-character fallback rule (`.`) -- its own
comment just hadn't listed it, since nothing had needed it yet; updated
the comment rather than adding a redundant new rule.

### do-while

Reuses `AST_WHILE` with a new `ival=1` flag (test-after) rather than a
new node kind -- confirmed directly, not just designed this way and
assumed it would work, that EVERY existing pass touching `AST_WHILE`
(sema.c's loop-depth tracking, and all four of lower.c's own
expression/statement walkers plus its destructor-boundary phase) is
completely agnostic to the flag, treating a loop body as a loop body
regardless of when its condition is tested. Checked every one of those
`case AST_WHILE:` sites directly before considering this settled.
Result: `sema.c` and `lower.c` needed ZERO changes; only `codegen.c`'s
own printing needed the `ival` check, emitting `do { ... } while
(cond);` instead of `while (cond) { ... }`. `tests/sample59.cpp`
deliberately includes a `break` inside a do-while specifically to
verify this claim for real, not just trust the reasoning -- confirming
the shared loop-depth/destructor-boundary machinery genuinely works
unmodified.

### enum

Top-level/namespace-level only, by deliberate scope decision -- NOT
supported as a class member (a nested enum), a real C++ pattern this
grammar doesn't parse. `namespace_decl`'s own body already reuses
`top_decl_list`, confirmed directly before relying on it, so adding
`enum_decl` as a new `top_decl` alternative made it valid in both
places automatically, no separate namespace-level grammar work needed.

New `SYM_ENUM` symbol kind, wired into the exact same check the
lexer's TYPE_NAME-vs-IDENTIFIER hack already uses for
classes/typedefs (`sym->kind == SYM_CLASS || sym->kind ==
SYM_TYPEDEF`, now also `|| sym->kind == SYM_ENUM`) -- found the precise
line before touching it, rather than guessing at the mechanism.
Registered via a single, non-split action (unlike `class_decl`'s own
mid-rule registration before its body is parsed) since nothing inside
an enum's own body can ever reference the enum's own name recursively
-- there's no ordering requirement a mid-rule action would exist to
satisfy here, so the simpler `typedef_decl`-style single action was the
right precedent to follow, not `class_decl`'s more complex one.

Passed straight through as literal, unmodified C enum syntax --
`codegen.c`'s new `emit_enums` needed no lowering transformation at
all, the same "Vircon32 C already has this natively" treatment
`AST_SWITCH` already established. An omitted enumerator's own value
(the auto-increment case) is never computed by this project itself --
real C's own compiler resolves it downstream, exactly matching real
C++'s rule, so there was nothing to implement here beyond emitting the
name and, when present, an explicit `= value`.

### The systematic sweep for `AST_TERNARY`, same discipline as `AST_CAST` two rounds ago

Before considering the ternary implementation complete, searched every
function containing an `AST_UNOP` case (the same proxy used to catch
the `AST_CAST` gaps two rounds ago) and checked each one directly
rather than assuming ternary's three children would be walked
correctly by whatever `default:` case existed. Found the identical
class of gap: two locations in `sema.c` (`check_node`,
`dump_calls_in_node`) and four in `lower.c` (`rewrite_expr`,
`finalize_calls_expr`, `fix_reference_access_expr`,
`new_delete_rewrite_expr`) all needed a new `AST_TERNARY` case, each
recursing into all three children (a, b, c) rather than the single
child `AST_CAST`'s own cases needed -- a ternary's condition,
true-branch, and false-branch can each independently contain a `this`
reference, a member access, or a call needing its own resolution, e.g.
`flag ? this->x : someVirtualCall()`. `check_node`'s own case
deliberately does NOT call `resolve_operator_use` -- the ternary
operator was never added to this project's overloadable-operator set,
matching `&&`/`||`'s own existing treatment, so there's no operator-
overload resolution that could ever apply to it. Confirmed exactly
seven total `AST_TERNARY` cases across the three files afterward (two
sema.c, four lower.c, one codegen.c) -- matching the sweep precisely,
not left to chance.

### A real, caught-immediately stale-object-file bug -- worth remembering for any future symtab.h change

Adding `SYM_ENUM` to `symtab.h`'s own `SymbolKind` enum, inserted
BETWEEN `SYM_CLASS` and `SYM_NAMESPACE` rather than appended at the
end, shifted the integer value of `SYM_NAMESPACE` (and everything
after it) by one. This project's Makefile has no per-file header-
dependency tracking (a known, established limitation from earlier
rounds) -- so touching `symtab.h` alone does NOT trigger recompilation
of every `.c` file that includes it, only the ones explicitly `touch`ed
or directly edited. The result: a `make test` run immediately after
this change broke `sample1.cpp` with a bizarre, seemingly unrelated
parse error (`v32::Timer *invincibilityTimer;`, nothing to do with
enums at all) -- caught IMMEDIATELY by running the full suite before
declaring this round done, not left in. Root cause: `symtab.c` itself
(and potentially other files) hadn't been recompiled against the NEW
`symtab.h`, while files explicitly edited this round (`sema.c`,
`lower.c`) HAD been -- an inconsistent build, part of it linked against
the old enum layout, part against the new one. Fixed with a genuinely
full `make clean` and rebuild from scratch, confirmed to resolve it
completely (`sample1.cpp` passes again, identical 13 expected failures
as before). The lesson, worth remembering explicitly rather than
re-discovering by accident again: inserting (not just appending) a new
value into an existing, shared enum is exactly the kind of header
change this project's own lack of dependency tracking makes
dangerous -- a full clean rebuild is the only reliably safe response,
not a targeted `touch` of the files that seem obviously affected.

### Verification

Full syntax-check across every changed file; a build against the
EXISTING (pre-this-round) generated grammar -- this time via a
genuinely full clean rebuild specifically because of the `symtab.h`
change above, not the lighter `touch`-based shortcut used in most prior
rounds -- confirming every other file still integrates and links; the
full 57-sample suite re-run against that build with zero regressions
(after the stale-object-file fix). `sample58`/`59`/`60` each confirmed
to fail EXACTLY at their own first new-syntax line against the stale
grammar (`sample58` at its own first `?`; `sample59` at its own first
`do {`; `sample60` at its own first `enum Color {`) -- not some
unrelated breakage. The grammar itself, and everything downstream of
it, still needs Matthew's own bison/flex regeneration to confirm for
real.

## `union`, `sizeof`, and `goto` -- including this round's genuinely highest-risk grammar change

Matthew confirmed the ternary/do-while/enum outputs directly and the
stale-object-file fix resolved cleanly, then greenlit the next batch:
`union`, `goto`, and source-level `sizeof`.

### `union`

New `AST_UNION_DECL`, new `SYM_UNION` symbol kind wired into the same
TYPE_NAME-recognition check the lexer already uses for class/typedef/
enum. Deliberately NOT routed through `class_decl` the way `struct`
is, unlike that earlier decision -- real C++ restricts what a union
can contain far more than a struct (no virtual functions, no base
classes, no vtable-requiring members at all), so reusing `class_decl`'s
full machinery here would have silently implied capabilities a union
doesn't actually have. Union members reuse `var_decl` directly (a
member is syntactically just "type name;"), and `codegen.c`'s new
`emit_unions` is a near-copy of `emit_enums`, passed straight through
as literal C union syntax -- no lowering, Vircon32 C already has this
natively. One real ordering bug caught immediately by the syntax
checker, not left in: the existing forward declaration for
`print_var_decl_inline` sat AFTER where `emit_unions` needed to call
it (it had only ever needed to precede `emit_globals` before); moved
it earlier rather than adding a second, redundant one.

### `sizeof`

New `AST_SIZEOF`, exactly one of `type`/`a` ever set -- matching real
C++'s own dual grammar (`sizeof(Type)` vs. `sizeof expr` /
`sizeof(expr)`), disambiguated the identical way the C-style cast's
two possible readings already are (`type_spec`'s own first-set never
overlaps with `expr`'s, confirmed directly before relying on it again,
not re-derived from scratch). Two real bugs caught by the compiler
itself before they could ship, worth naming plainly since they're
exactly the kind of thing easy to miss by hand: `AST_CAST` had no
trailing comma (it was the enum's last member before this) -- inserting
`AST_SIZEOF` right after it without adding one would have been a
genuine compile error, caught by `gcc -fsyntax-only`; and a missing
`kind_name` case for `AST_SIZEOF` in `ast.c`, caught by `-Wswitch`
firing on the very next syntax-check run. Worth noting for future
rounds: `-Wswitch` is a real, mechanical safety net for `kind_name`
specifically (no `default:` case there to suppress it), but NOT for
any of the `check_node`/`rewrite_*`/`finalize_*` walkers, which all
have their own `default:` -- so the systematic per-function sweep
remains the only way to catch a gap in those, `-Wswitch` won't do it
for you. The full seven-location sweep (two `sema.c`, four `lower.c`,
one `codegen.c`) confirmed complete afterward, same method as
`AST_TERNARY` two rounds ago -- each new case calls its own function
unconditionally on `n->a` rather than guarding with an explicit
NULL check first, since every one of those functions already
NULL-guards its own input at the top (confirmed directly for all of
them before relying on it, not assumed) -- the type-taking form's
NULL `a` is already a safe no-op without any extra code.

### `goto` and labeled statements -- the real risk, handled carefully rather than rushed

New `AST_GOTO` (a leaf, no children) and `AST_LABEL` (wraps the one
statement it precedes, matching real C++'s own grammar exactly -- a
label is not a container). This is genuinely the highest-risk grammar
change of any round so far, and treated that way throughout rather
than added casually:

**The ambiguity, reasoned through before writing the grammar, not
after**: a labeled statement (`label: stmt`) and an ordinary
expression-statement (`expr ';'`, where `expr` can reduce from a bare
IDENTIFIER through `primary_expr`) both legally start with the exact
same token in the exact same grammar position. The parser cannot know
which one it's building until it sees whether `:` follows the
identifier or something else does. Confirmed this is not a novel
problem before proceeding -- real C's own yacc/bison grammars have
successfully modeled labeled-statement vs. expression-statement this
exact way for decades -- and confirmed this project's own grammar
already carries a `%glr-parser` declaration specifically for this
class of situation, with its own header comment already anticipating
the grammar growing into "genuinely ambiguous" territory. GLR defers
the reduce decision, exploring both readings until the next token
resolves it, rather than requiring one token of lookahead to be enough
the way plain LALR(1) would need. Proceeded on that basis, but flagged
this specific production with an unusually long comment directly in
`parser.y` calling out that it's more likely than this round's other
additions to shift the `%expect` count by more than one, or, in the
worst case, surface a genuine reduce/reduce conflict GLR can't resolve
on its own -- want this to be a prepared-for possibility on
regeneration, not a surprise.

**Scope, stated explicitly rather than silently gapped**: this project
makes no attempt to validate that a `goto`'s own label actually exists
anywhere in the function, and -- the more consequential gap -- no
attempt to handle destructor invocation correctly for a `goto` that
jumps into or out of a scope with a live destructible (class-typed)
local. Every other exit path this project already handles specially
(`break`, `continue`, `return`) goes through `lower.c`'s own
`destruct_scope` phase, which knows exactly which destructibles need
invoking at that exact point; `goto` has no equivalent, and doesn't
get one this round. `AST_LABEL` itself IS threaded through every
scope-tracking pass correctly (the label is transparent to loop-depth
tracking, this-injection, call finalization, and destructor-boundary
computation for the statement it wraps -- confirmed via the same
nine-location sweep method used for `AST_TERNARY`/`AST_SIZEOF`, this
time using `AST_IF` as the proxy for "statement-level walkers," since
`AST_LABEL` wraps an arbitrary STATEMENT rather than being a leaf or
an expression -- two in `sema.c`, seven in `lower.c`, confirmed nine
total cases after) -- what's NOT handled is `goto`'s own jump
interacting with destruction, a real, separate concern from the label
itself being walked correctly. Documented explicitly in `AST_GOTO`'s
own doc comment in `ast.h`, not left implicit; `tests/sample63.cpp`
deliberately stays within a single function's own flat, no-
destructible-locals scope specifically to demonstrate the feature
without exercising the known gap.

### Verification

Full syntax-check across every changed file; a genuinely full `make
clean` rebuild (not the lighter touch-based shortcut) specifically
because `symtab.h` changed again this round (`SYM_UNION`, inserted
before `SYM_NAMESPACE` the same way `SYM_ENUM` was two rounds ago) --
applying that lesson deliberately this time rather than re-discovering
it by accident again; the full 60-sample suite re-run against that
build with zero regressions. `sample61`/`62`/`63` each confirmed to
fail EXACTLY at their own first new-syntax line against the stale
grammar (`sample61` at its own `union Value {`; `sample62` at its own
first `sizeof(int)`; `sample63` at its own `loop:` label) -- not some
unrelated breakage. The grammar itself, and everything downstream of
it -- especially whether the `goto`/label production actually
regenerates cleanly, the one genuinely uncertain part of this round --
still needs Matthew's own bison/flex regeneration to confirm for real.

## Function pointers -- both declarator styles, arrays of them, and a real bonus bug found along the way

Matthew confirmed the clean bison build (25 -> 22 conflicts, exactly as
predicted -- the `sizeof`/cast ambiguity really was the only genuinely
new one, `union`/`goto`/`AST_LABEL` added none at all) and the
sample61-63 outputs, then asked for function pointers -- explicitly
both Vircon32's own quirky declarator style and standard C's, plus
arrays of them. Before implementing anything, directly confirmed the
`sizeof` fix on the exact case it was designed for (`sizeof(int) - 5`,
`sizeof(int) & 1`, `sizeof(int) * 2` all correctly parse as `sizeof`
completing at its own closing paren, not swallowing the operator into
a cast) -- the earlier clean build only proved zero CONFLICTS, not that
the chosen resolution was semantically the one intended; worth
verifying separately, and it was.

### Investigated before designing anything, not guessed at

`docs/VIRCON32_QUIRKS.md` already had both forms confirmed against the
real compiler from earlier vtable-slot emission work: Vircon32 requires
`ReturnType(ParamTypes)* name;`, standard C is
`ReturnType (*name)(ParamTypes);`. More valuably, it already carried an
explicit standing principle from Matthew, quoted directly in that file:
extend the SAME dual-acceptance approach array declarators already use
(accept both spellings as C++-side input, produce one identical AST
shape regardless, always emit Vircon32's own required form on output)
to function-pointer declarators whenever that work happened, rather
than deciding the approach fresh. Followed that precedent exactly
rather than inventing a new one -- traced `ast_wrap_array`'s own
implementation and every `print_type` call site's "print the type,
then the caller appends the name" convention before writing a single
line of new grammar.

### Design: one type node, reused for the array case for free

New `AST_FUNC_PTR_TYPE` (type=return type, list=bare parameter TYPES,
no names -- matching real C++'s own function-pointer type exactly).
Deliberately NOT a parallel struct with its own array-of variant: since
`AST_FUNC_PTR_TYPE` is just another ordinary type node, wrapping one
with the EXISTING `ast_wrap_array` helper (the same one every other
array-typed declaration already uses) produces "array of function
pointers" with no new composition code at all -- `print_type`'s own
existing `AST_ARRAY_TYPE` case already recurses into whatever its own
element type turns out to be, function-pointer type included, once
`AST_FUNC_PTR_TYPE` had its own case to recurse into. Four new
`var_decl` grammar productions: standard-C plain, Vircon32-style plain,
standard-C array, Vircon32-style array -- each pair disambiguated from
its sibling by the very next token after the shared `type_spec
pointer_opt '('` prefix (a bare `'*'` can only ever start the
standard-C form, since `type_spec`'s own first-set never includes
`'*'`, so the two forms never actually compete for the same lookahead
-- confirmed by this same first-set reasoning already proven correct
for the C-style cast and `sizeof` disambiguations in earlier rounds,
not re-derived from scratch this time).

A new, separate `func_ptr_param_type`/`func_ptr_param_list`/
`opt_func_ptr_param_list` grammar family, deliberately NOT reusing the
existing `param`/`param_list`/`opt_param_list` (an ordinary function's
own parameters, which DO carry names) -- a function-pointer TYPE's own
parameter list is bare types only. Supports the `(void)` "no
parameters" spelling explicitly, real C's own idiom for this.

`codegen.c`'s new `AST_FUNC_PTR_TYPE` case in `print_type` emits
`ReturnType(ParamType, ParamType, ...)*` -- no name inside the parens
at all, matching the confirmed vtable-slot precedent exactly
(`int(Shape *)* Shape__area__void;`) -- with the caller appending
` name` afterward, the same pattern every other type in that function
already follows, which is also exactly what makes the array
composition work automatically.

### An honest gap, not silently extrapolated as if confirmed

The Vircon32-style ARRAY-of-function-pointers spelling specifically
(`ReturnType(ParamTypes)* [N] name;`) is this project's own
extrapolation from the two individually-confirmed patterns -- no
existing generated output anywhere in this project combines Vircon32's
own array-bracket placement with its own function-pointer placement in
the same declarator, so unlike the plain (non-array) Vircon32
function-pointer form, this specific combination has NOT been
confirmed against the real compiler. Said so directly in three places
(`AST_FUNC_PTR_TYPE`'s own doc comment in `ast.h`, that grammar
production's own comment in `parser.y`, and `tests/sample66.cpp`'s own
header comment) rather than presenting it with the same confidence as
the pieces that are actually settled.

### A genuine, separate bug found along the way, unrelated to function pointers at all

While writing `tests/sample64.cpp`, an ORDINARY function declared
`int getZero(void) { ... }` failed to parse -- nothing to do with
function pointers. Isolated and confirmed directly (a standalone
two-line test with no function-pointer syntax anywhere in it):
`opt_param_list` had never accepted the explicit `(void)` spelling for
"no parameters" at all, only bare `()` -- a real, pre-existing gap this
project simply hadn't been asked to close yet, `(void)` being extremely
common, idiomatic C. Fixed with the identical `VOID_KW` alternative
already added to `opt_func_ptr_param_list` for the same reason, same
unambiguous reasoning (a bare `VOID_KW` with nothing following it can
only ever match this new alternative, since `param` itself always
requires a name after its own type, so there's no competition with
`param_list`'s own first alternative). This is exactly the kind of
gap this project has repeatedly caught by testing real, complete
programs end to end rather than narrowly-scoped feature probes -- worth
naming as a pattern, not just this one instance of it.

### Tests

`sample64.cpp` (standard-C style, including the `(void)` no-params
case, both the fix above and the function-pointer form's own `(void)`
alternative), `sample65.cpp` (Vircon32-style, confirming identical
behavior to the standard-C form), `sample66.cpp` (arrays of function
pointers in both styles, the Vircon32 array form's own header comment
carrying the "not yet confirmed" flag described above). All three
exercise actual USAGE too, not just declaration syntax -- assigning a
function's own name to the pointer and calling through it -- confirming
(rather than assuming) that this project's existing, fully generic
expression grammar already handles both without any new work: a bare
function name is already an ordinary identifier expression, and
calling through a pointer already uses the same call syntax as calling
a function directly, so neither needed any new grammar at all, only
confirmation that nothing about the surrounding pipeline breaks it.

### Verification

Full syntax-check across every changed file; a build against the
EXISTING (pre-this-round) generated grammar confirming every other
file still integrates and links; the full 63-sample suite re-run
against that build with zero regressions. `sample64`/`65`/`66` each
confirmed to fail against the stale grammar in the expected place --
`sample65` and `sample66` exactly at their own first new declarator
syntax; `sample64` reported at its own `getZero(void)` line rather
than its later function-pointer syntax, consistent with (not
contradicting) the stale-grammar failure mode already seen in earlier
rounds, where GLR's own error reporting can surface a few tokens after
the actual point of divergence rather than exactly on it. `type_to_class`
(sema.c) confirmed directly to already handle an unrecognized type kind
gracefully via its own `default: return NULL;` -- no change needed
there for `AST_FUNC_PTR_TYPE` to be treated safely as "not a class."
The grammar itself, and everything downstream of it -- crucially
including whether the two disambiguation points (standard-C vs.
Vircon32-style function-pointer declarators, and the new `(void)`
alternative) actually regenerate cleanly -- still needs Matthew's own
bison/flex regeneration to confirm for real.

## Function-pointer grammar conflicts -- one real bug, one benign, resolved differently

Matthew's own bison run surfaced exactly what function pointers' extra
grammar risk was flagged as likely to produce: a reduce/reduce conflict
(new -- this project had never had one before) and a shift/reduce delta
of +1 (23 found against the 22 baseline). Read every counterexample
directly before responding, per this file's own standing instruction
at the `%expect` declaration itself (added at some earlier point in
this project's history, evidently from a previous close call: "read
every new counterexample... and confirm the actual concrete input you
care about still parses correctly before trusting the new number") --
not just the totals, and not assumed to be another instance of the
already-familiar `out_of_line_def`-vs-`func_def` family that accounts
for the bulk of the existing 22.

**The reduce/reduce conflict was a real, exact bug**, not a benign
default: `opt_func_ptr_param_list`'s own `(void)` alternative
(`VOID_KW`) was flagged as reachable two different ways for the exact
same input. Root cause: `func_ptr_param_type` is `type_spec
pointer_opt`, and `type_spec` already accepts a bare `VOID_KW` as a
complete, valid type on its own (needed for a `void *` parameter) --
`type_spec` has no way to know it's being used somewhere a bare `void`
isn't meaningful. So a lone `(void)` reduced BOTH via the dedicated
`VOID_KW` alternative added for exactly this spelling, AND via the
ordinary `type_spec`-accepts-`VOID_KW` path producing a single-entry
parameter list -- two genuinely different derivations for the same
input, which is exactly what a reduce/reduce conflict means and
exactly why bison refused to resolve it silently. Fixed by removing
the now-redundant dedicated alternative entirely, not by trying to
keep both and disambiguate between them: `(void)` still parses
correctly and still prints as `(void)` in generated output, now via
the one remaining path (a single-entry list whose entry is the bare
`void` type) -- neither the accepted input nor the produced output
actually changed, only which internal grammar path reaches it.

**The shift/reduce conflict was genuinely new but benign, accepted
rather than eliminated**: a qualified type name immediately followed
by `(` (`Namespace::ClassName (*fp)(int);` -- a function pointer whose
own return type happens to be qualified) is, at that exact point, also
a valid prefix of an out-of-line constructor definition for that same
qualified class (`Namespace::ClassName(int x) { ... }`), which this
grammar already supported before function pointers existed at all.
Bison's own default resolution (prefer shift) picks the out-of-line-
constructor reading -- and confirmed this is also the CORRECT default
to prefer, not merely convenient: a function pointer whose own return
type is a qualified name is a rare, unusual case, while out-of-line
constructor definitions are completely ordinary, so shift-preference
happens to match the far more likely intended reading, the same way
it already does for the unrelated, pre-existing dangling-else problem
elsewhere in this grammar. `%expect` bumped from 22 to 23 to account
for it, with the full reasoning recorded directly at the declaration
itself, not just the number changed silently.

**The two were NOT handled the same way, deliberately** -- a bug and a
benign conflict call for different responses, and treating them
identically (either fixing both, or accepting both) would have been
the wrong instinct either way. The reduce/reduce conflict was fixed
outright, keeping this project's own reduce/reduce count at its
permanent zero; the shift/reduce conflict was examined, confirmed
correct by default, and accepted with its own count and reasoning
updated to match, not fixed for its own sake when nothing was actually
wrong with the outcome it already produced.

### Verification

Full syntax-check across every changed file (the fix and the `%expect`
change are both confined to `parser.y`, touching no `.c` file at all);
a build against the EXISTING (pre-this-round) generated grammar
confirming nothing else regressed; the full 63-sample suite re-run
against that build with zero changes, exactly as expected since
neither fix altered any already-generated grammar behavior -- both
fixes are inert until Matthew's own next bison regeneration, which
remains the only way to confirm the reduce/reduce conflict is
genuinely gone and the shift/reduce count lands on exactly 23.

## Multi-dimensional arrays -- and a real dimension-order bug caught before it shipped

Matthew confirmed the clean bison build for function pointers (both
fixes worked -- zero reduce/reduce, exactly 23 shift/reduce) and all
three outputs, then asked for multi-dimensional arrays plus a quick
audit of remaining basic-C gaps.

### Design: generalize, don't special-case

`array_bracket_list` (new): collects one or more `[N]` bracket groups
in source order as a list of bare `AST_INT_LIT` nodes, reused for BOTH
accepted array-declarator forms (standard-C length-after-name,
Vircon32-style length-before-name) and for single- and multi-
dimensional arrays uniformly -- "exactly one bracket group" is simply
the one-element case of "one or more," so the existing two var_decl
array productions were extended in place rather than duplicated
alongside new multi-dimensional-specific ones. `ast_wrap_array_dims`
(new, `ast.c`) turns that source-order list into correctly-NESTED
`AST_ARRAY_TYPE` nodes -- no new AST node kind needed at all: `int
grid[8][4]`'s own type is simply an `AST_ARRAY_TYPE` (length 8) whose
own element type is ANOTHER `AST_ARRAY_TYPE` (length 4) wrapping plain
`int`, exactly the same "one type node wrapping another" structural
composition this project already used for "array of function
pointers" two rounds ago.

### A real bug, caught by tracing a concrete example before trusting the code, not after

`print_type`'s existing `AST_ARRAY_TYPE` case followed the pattern
"recurse into the element type, THEN append this node's own bracket" --
correct, and the only sensible choice, for a single dimension. Traced
it by hand against a concrete two-dimensional case
(`AST_ARRAY_TYPE(8, AST_ARRAY_TYPE(4, int))`, i.e. `int grid[8][4]`)
before assuming it would generalize, and it does NOT: recursing first
means the INNERMOST node's own bracket gets appended first as the
recursion unwinds, producing `int [4] [8]` -- backwards from the
correct `int [8][4]`, and a genuine change of meaning (which dimension
is which), not a cosmetic difference. Caught and fixed before this was
ever presented as working, not discovered from a failing test or a
user report -- rewrote the case to walk down through however many
`AST_ARRAY_TYPE` layers exist first, collecting each one's own length
in outermost-first order, THEN print the true base type once followed
by every collected bracket in that correct order. Re-traced both the
new (2D) and the pre-existing (1D) case by hand afterward to confirm
neither is wrong -- 1D still reduces to exactly the same output it
always produced (a single dimension collected, walked, and printed is
identical to the old direct approach), confirmed for real (not just by
the hand trace) via the full existing test suite, which already
exercises single-dimension arrays extensively and passed unchanged.

### Composability confirmed, not just assumed

Chained subscripting (`grid[i][j]`) needed no new grammar at all --
confirmed directly by checking, not assumed: `postfix_expr`'s own
subscript rule (`postfix_expr '[' expr ']'`) is already left-
recursive, so `grid[i][j]` already reduces to a nested `AST_SUBSCRIPT`
(itself a subscript of a subscript) via the exact same mechanism that
already lets `.`/`->`/call-chains compose. This project's own
expression-level machinery needed zero changes for multi-dimensional
array ACCESS; only the DECLARATOR grammar (how the array's own type is
written and parsed) needed new work at all.

### Scope, stated rather than silently left implicit

Deliberately did NOT extend this to two places where "one or more
bracket groups" already existed for a different reason: a function
PARAMETER'S own array-to-pointer decay (`void foo(int arr[8])` becomes
an ordinary pointer parameter, real C/C++ semantics), where a second
dimension has genuinely different (and more subtle) decay rules than a
first one does (`int arr[8][4]` as a parameter decays to a pointer to
a 4-element array, not a fully-decayed pointer) rather than simply
composing the same way declaring a variable does; and the function-
pointer array forms from two rounds ago, where a multi-dimensional
array of function pointers is a genuinely rare combination not worth
the added grammar risk alongside everything else this round already
touched. Both remain single-dimension only, unchanged from before this
round -- a real, bounded scope decision, not an oversight.

### Verification

Full syntax-check across every changed file; a build against the
EXISTING (pre-this-round) generated grammar confirming every other
file still integrates and links; the full 66-sample suite re-run
against that build with zero regressions -- including a direct spot-
check that an existing single-dimension array test's own generated
output is byte-for-byte unchanged, real confirmation of the hand-traced
1D case above, not just trust in the trace alone. `sample67.cpp`
confirmed to fail against the stale grammar exactly at its own first
multi-dimensional declarator (`int grid[3][3];`'s own SECOND bracket,
where the old single-bracket-only rule expected `;` instead) -- not
some unrelated breakage. This round's own grammar change (generalizing
an existing "exactly one bracket" shape to "one or more" at the same
two grammar positions) is structurally a narrower change than most of
this project's recent grammar rounds, but still needs Matthew's own
bison/flex regeneration to confirm it introduces no new conflicts, the
same as every grammar change in this project always has.

## `const` -- the highest-value item from the fresh audit, and four real gaps found by a systematic sweep before trusting it was done

Matthew's own "Continue" endorsed the audit's own suggested next step:
`const`, flagged as probably the single highest-value remaining gap
given how pervasive it is in real C/C++.

### Design: one new type node, added at the single point every type flows through

New `AST_CONST_TYPE` (wraps its own inner type, same "one type node
wrapping another" shape `AST_POINTER_TYPE`/`AST_REFERENCE_TYPE`/
`AST_ARRAY_TYPE` already use), new `ast_wrap_const` helper, new `CONST`
keyword, and a single new `CONST type_spec` alternative added directly
to `type_spec` ITSELF rather than to each of the many productions that
reference it (`var_decl`, `param`, `func_ptr_param_type`,
`typedef_decl`, a function's own return type, ...) -- since `type_spec`
is the one shared nonterminal every one of those already funnels
through, this single addition propagates `const` everywhere a type can
appear at once. Confirmed no new grammar ambiguity before relying on
this: `CONST` is a token unique among every one of `type_spec`'s
existing alternatives' own starting tokens, so it doesn't compete with
any of them at the point `type_spec` is expected.

### Scope, decided deliberately and stated plainly, not left implicit

Only `const` as a PREFIX before a type is accepted (`const int`,
`const int *` -- pointer to const, the pointee can't change); a const
POINTER itself (`int * const p` -- the pointer can't be reassigned) is
not supported -- `const` is only ever accepted before a `type_spec`,
never after a `pointer_opt`'s own `*`. No actual const-correctness
ENFORCEMENT exists anywhere either (no error for reassigning a const
variable, no error for calling a non-const method through a const
reference) -- accepted and correctly emitted in generated C so it
doesn't block valid code from transpiling, with real violations left
for the downstream C/C++ compiler to catch, this project's own
established best-effort philosophy applied here rather than departed
from. `const` MEMBER FUNCTIONS (`int getValue() const { ... }`) are
deliberately deferred entirely, not attempted this round: a different
grammar position (after the parameter list, in `func_header`, not
before a type at all), and meaningfully propagating it would need
threading through this-injection (emitting `const ClassName *this`)
to actually mean anything -- judged as real, separate scope rather
than folding it in alongside the core prefix-const work.

### The real work: a systematic sweep for existing type-walking functions, not just codegen

Before considering this done, searched every function switching on
`AST_POINTER_TYPE`/`AST_REFERENCE_TYPE` (the same proxy method used to
catch the `AST_CAST`/`AST_TERNARY` gaps in earlier rounds, here applied
to TYPE-walking functions instead of expression-walking ones) rather
than assuming `codegen.c`'s own `print_type` was the only place that
needed to know about the new node kind. Found four real gaps, each
with its own `default:` fallback that would have silently produced
something wrong rather than crashing -- exactly the kind of failure
mode that's easy to miss without checking systematically:

- `type_to_class` (`sema.c`) -- without a case here, `const Shape *s`
  would never have resolved to the `Shape` class at all (its own
  `default: return NULL;` catching it), silently breaking method-call
  resolution and access checking through any const-qualified class
  type. Fixed by adding `AST_CONST_TYPE` to the same case
  `AST_POINTER_TYPE`/`AST_REFERENCE_TYPE` already share (recurse into
  `type->a`).
- `types_equal` (`sema.c`, overload matching) -- `const int` and `int`
  would have compared as genuinely DIFFERENT types (the function's own
  `if (t1->kind != t2->kind) return 0;` catching it before even
  reaching the switch), silently breaking overload resolution for any
  const-qualified parameter. Fixed by stripping `const` from both
  sides before the kind comparison at all, deliberately treating it as
  fully transparent for matching purposes rather than attempting to
  model real C++'s own genuinely nuanced const-overload rules
  (significant on a reference/pointer parameter, ignored on a by-value
  one) -- a real, stated simplification, not an oversight: failing to
  match an otherwise-obvious overload over a const distinction this
  project doesn't enforce anyway would be a worse outcome than
  ignoring it.
- `type_signature_str` (`sema.c`, name mangling) -- a `const int`
  parameter would have mangled as the literal string `"unknown"`
  instead of `"int"`, via this function's own `default:` fallback.
  Fixed by making `const` transparent for mangling too, deliberately
  kept consistent with `types_equal`'s own identical decision -- if
  overload resolution already treats `const int` and `int` as the same
  parameter, the mangled name scheme agreeing with that matters, not
  just each fix being individually reasonable in isolation.
- `render_type` (`lower.c`, the `-vvv` struct-layout dump) -- the
  identical `default:`-fallback shape, but the RIGHT fix here is the
  opposite of the mangling one: this function is for human-readable
  display, so actually SHOWING `const` (prefixed, `"const int"`) is
  the more useful, accurate rendering here, not hiding it the way
  mangling needed to for a completely different reason. Two functions
  with the same structural gap, deliberately fixed differently because
  they serve different purposes -- worth being explicit that this
  wasn't a copy-paste of the same fix everywhere.

`codegen.c`'s own `print_type` case was the fifth and most
straightforward: `const` prefixes rather than suffixes when printed
(`const int`, never `int const`), unlike every other wrap this
function handles, which all recurse then append their own marker
after -- its own dedicated case, not a variant of the existing
pointer/reference/array pattern.

### A lesson from two rounds ago, applied deliberately this time, not re-learned by accident

`AST_CONST_TYPE` was inserted into the MIDDLE of the `AstKind` enum
(between `AST_REFERENCE_TYPE` and `AST_ARRAY_TYPE`), shifting every
subsequent enum value -- recognized immediately, before running any
build at all, as the identical risk class the `SYM_ENUM`/`SYM_UNION`
stale-object-file bug from two rounds back already taught: this
project's Makefile has no per-file header-dependency tracking, so an
incremental rebuild after an enum-shifting header change can leave
some `.c` files compiled against the OLD layout and others against the
NEW one, silently. Checked directly rather than assumed: a first
incremental `make all` after all the `const` work only recompiled 2 of
11 object files (only the two directly edited that build) -- confirmed
by comparing `sema.o`'s own timestamp against `sema.c`'s, which
happened to already be current from an earlier syntax-check-triggered
rebuild, but nothing guaranteed every OTHER file (`symtab.c`, `main.c`,
...) was equally current. Did a genuinely full `make clean` and
rebuild specifically because of this, not the lighter touch-based
shortcut most other rounds have used -- the lesson from before applied
on purpose this time, not re-discovered by a second accidental
regression.

### Verification

Full syntax-check across every changed file; the genuinely full clean
rebuild described above (not incremental); the full 67-sample suite
re-run against that build with zero regressions. `tests/sample68.cpp`
exercises `const` on a plain variable, a function parameter, pointee-
const through a pointer, and a `const Shape &` parameter specifically
(confirming the `type_to_class` fix actually matters for something
concrete -- a method call resolved and correctly mangled through a
const-qualified class reference, not just a synthetic case) -- confirmed
to fail against the stale grammar exactly at its own first `const`
usage, not some unrelated breakage. The grammar itself, and everything
downstream of it -- including whether the systematic sweep genuinely
caught every place that needed to know about `AST_CONST_TYPE` -- still
needs Matthew's own bison/flex regeneration and a real build to confirm
for real.

## const member functions -- addressing a real "pass-through" concern, not deferring it again

Matthew's own counterexamples.txt showed the `const` round's grammar
change landed exactly as predicted (+3 shift/reduce, all three
structurally identical to the existing INT_KW/FLOAT_KW/etc family, now
also triggered by CONST as a new type_spec-starting token; zero new
reduce/reduce) -- fixed with a straightforward `%expect` bump. Matthew
then pushed back, correctly, on the earlier decision to defer `const`
member functions: this project transpiles TO C, so silently discarding
a trailing `const` rather than propagating it to the generated `this`
parameter's own type would leave the emitted signature not actually
reflecting what the method promises -- a real problem, not a
theoretical one, exactly as flagged.

### Finding a genuinely free field, not assuming one

`const` sits in a NEW grammar position for this feature (after a
method's own closing `)`, in `func_header` AND, separately, in
`out_of_line_def` -- which has its own, non-shared grammar, not routed
through `func_header` at all, confirmed by reading it directly rather
than assumed). Needed a field on the shared `AST_FUNC_DECL`/
`AST_FUNC_DEF` node to carry "was this const" through to lower.c's
this-injection pass. `ival` was already claimed (virtual-ness, set by
the CALLER around `func_header`, not `func_header` itself) and `a` was
briefly considered before checking `func_def`'s own grammar action
directly -- which showed `a` becomes the body once re-kinded to
`AST_FUNC_DEF`, so it was never actually free. Checked every field
assignment across `func_header`/`func_decl`/`func_def`/
`out_of_line_def` before picking one: `str2` is the only field never
once assigned anywhere in this whole family, confirmed by grep across
every `.c` file, not assumed from the header comment alone.

### Propagating it where it actually matters

New `opt_const` (mirrors `opt_virtual`'s own 0/1 shape) added to both
`func_header`'s ordinary-method alternative and `out_of_line_def`'s
first alternative separately, storing `str2 = "const"` (a sentinel,
not a displayed string) when present. `this_inject_method` (lower.c) --
the actual point of this work -- now checks that flag and wraps the
injected `this` parameter's own type in `AST_CONST_TYPE` when set, so
a const method's generated C signature genuinely reads `const
ClassName *this`, not a plain pointer with the qualifier silently
dropped. This is the concrete fix for exactly the concern raised:
the generated code's own signature now reflects what the source
promised, rather than a `this` parameter identical to a non-const
method's own -- indistinguishable, which is what "passing it through
to C" without doing anything would have meant. Still no ENFORCEMENT
that the method body itself honors this (no error for writing through
`this` inside a const method) -- matching this project's existing
best-effort treatment of `const` everywhere else; a real downstream C
compiler, working from the correctly-qualified `this` this project now
emits, is what actually catches a genuine violation -- but the
signature itself is no longer silently wrong.

### A conflict-risk judgment made without being able to verify it directly

Unlike every other grammar addition this session, `%expect` was
deliberately NOT bumped for this one. Reasoning: `opt_virtual`'s own
documented 21-conflict contribution comes from sitting BEFORE
`type_spec`, where its own ε alternative leaves `func_decl`/`func_def`
and `var_decl` genuinely indistinguishable until further lookahead.
`opt_const` sits AFTER an entire parameter list has already been
shifted -- structurally a different position, past the point where
that particular ambiguity could still apply. Recorded this reasoning
directly in `opt_const`'s own comment, including explicitly that it
hasn't been confirmed by an actual bison run -- if wrong, the next
regeneration will error and name the real conflict, which is more
honest than guessing a number that might not hold up, especially
after already missing one conflict category earlier this session (the
`sizeof`/cast interaction) from assuming a pattern held without
checking the specific counterexample.

### Verification

Full syntax-check across every changed file; a build against the
EXISTING generated grammar confirming everything else still
integrates; the full 67-sample suite re-run with zero regressions.
`tests/sample69.cpp` exercises BOTH grammar positions this round
touched -- an in-class const declaration and a separately-defined
out-of-line const definition, on the same class -- confirmed to fail
against the stale grammar exactly at its own first const-method
declaration. Whether `opt_const`'s own conflict-risk reasoning holds,
and whether the `this`-const propagation produces the exact intended
`const Rectangle *this` in real generated output, both still need
Matthew's own bison regeneration and a real build to confirm.

## Full green build, confirmed end to end -- and a genuine mistake in my own test files, not a regression

Matthew's bison run came back clean: 0 reduce/reduce, and the
shift/reduce count matched the file's own `%expect 26` exactly --
including `opt_const`'s own deliberately-unbumped reasoning holding up
(no new conflicts from it at all). Synced the real generated
`lexer.c`/`parser.c`/`parser.h` and did a genuinely full clean rebuild
rather than trust anything incremental, then ran the real suite for
the first time with the actual grammar rather than reasoning about it
secondhand.

**Multi-dimensional arrays are now fully confirmed**: `sample67.c`'s
own generated output matches the design exactly --
`int [3][3] grid;`, dimensions outermost-first, chained subscripting
(`grid[i][j]`) working correctly. The dimension-order bug caught and
fixed two rounds ago (before it ever shipped) is confirmed correct in
real generated output, not just by hand-trace.

**`sample68`/`sample69` failed, but not from const or multi-dim
arrays at all** -- both errors were "unexpected '(', expecting ';'"
on a line reading `Shape shape(7);` / `Rectangle r(3, 4);`. Checked
directly rather than assumed: this project has NEVER supported direct-
initialization with constructor arguments on a stack-allocated local.
`opt_initializer` only ever accepts `= expr` or nothing --
`inject_ctor_calls_block` (lower.c) only ever looks up a ZERO-argument
constructor for an uninitialized class-typed local. This is a real,
pre-existing gap in the grammar that these two test files' own earlier
versions incorrectly assumed was supported -- a mistake made writing
the tests, not a defect this round introduced. Fixed by rewriting both
classes (`Shape`, `Rectangle`) to use a zero-argument constructor with
public fields set directly afterward (`shape.size = 7;`) instead of
constructor arguments -- confirmed correct directly against the real
parser this time, not just reasoned through: both now transpile
cleanly, and `sample69.c`'s own generated output confirms the actual
point of that test -- `int Rectangle__area__void(const Rectangle *
this)` for the const method, `void Rectangle__Rectangle__void(Rectangle
* this)` for the (non-const) constructor, exactly the intended
propagation, not just parsing success.

### A separate, genuinely new discovery along the way -- flagged, not fixed here

While isolating the `sample68` failure, noticed `getArea__Shape_ref`
being CALLED with a plain `shape` argument while its own parameter
type is `const Shape * s` (i.e., a pointer) -- a real type mismatch
that would fail to compile in real C. Before assuming this was
something the `const` work introduced, checked directly: a PLAIN
(non-const) `Shape &s` parameter has the exact same problem
(`getArea__Shape_ref(shape)`, no `&`), while the equivalent POINTER
parameter (`Shape *s`, called as `getArea(&shape)`) lowers correctly
(`getArea__Shape_ptr((&shape))`). This isolates the bug precisely:
reference-parameter call sites aren't getting the implicit
address-of a C++ reference needs when lowered to a plain C pointer --
whatever inserts `&` at a call site currently only fires when the
source itself wrote `&` explicitly (which a reference-typed call site
never does), not as part of reference lowering itself. This is
real, pre-existing, and unrelated to anything this round or the
const/multi-dim-array rounds touched -- worth its own focused pass
rather than a rushed fix folded into this one, so left alone here and
raised directly instead.

### Verification

Full syntax-check; the two test files re-verified directly against
the real, regenerated parser (not the stale one, for the first time
this session); the full 69-sample suite run end to end with the real
grammar -- every sample passes or fails exactly as expected (the same
13 deliberately-invalid samples, nothing else), the first genuinely
complete, unblocked green run since function pointers first
introduced grammar risk several rounds back.

## Reference-parameter call-site lowering, fixed for real -- and a second, narrower gap found while fixing it

Matthew asked for exactly what was flagged: address the reference-
lowering bug directly, rather than leaving it noted. VERSION bumped to
`20260918-dev` in `inc/v32cxx.h` alongside this.

### Root cause, confirmed by reading the actual pipeline, not guessed

`fix_references` (phase 5, lower.c) relabels a reference PARAMETER's
own AST_REFERENCE_TYPE to AST_POINTER_TYPE and fixes `.`-vs-`->`
access -- but only ever walks the CALLEE's own body. It has no
visibility into any call site that passes an argument TO that
parameter. Nothing else ever inserted the implicit address-of a C++
reference argument needs once its own parameter becomes a plain C
pointer. `address_of_if_needed` (lower.c) already existed and already
solved the structurally identical problem for a method's own RECEIVER
(`sprite.setx(320)` needing `(&sprite)`, a bug an earlier round already
found and fixed) -- the missing piece was applying that same helper to
ORDINARY call arguments too, not inventing new machinery.

### Where the fix had to live, and why timing mattered

`finalize_call` (phase 3+4) runs entirely BEFORE `fix_references`
(phase 5) ever mutates any AST_REFERENCE_TYPE anywhere in the program
-- confirmed by reading the actual phase ordering in lower_run(), not
assumed. This is why the new logic belongs in `finalize_call`, not in
phase 5 itself: at the point a call resolves (`CallResolution-
>resolved_target`), the resolved function's OWN parameter types are
still their true, original shape, regardless of which function gets
processed by phase 5 first -- phase 5 mutates a parameter's type in
place, on the SAME shared node every call site resolves to, so
checking this any later would already see every reference relabeled
away program-wide, with no way left to distinguish a true `Type *`
parameter from a lowered `Type &` one. For each argument in `call-
>list` (walked BEFORE any receiver-prepending below, which would shift
indices), if the matching parameter in the resolved target was
declared as a reference, wrap it via `address_of_if_needed` -- the
`this`-offset (+1 for a method call, whose own `target->list` already
has this-injection's `this` at index 0; +0 for a free function) keyed
off the exact same `callee->kind` branch `finalize_call` already used
below it, not a separate judgment call risking disagreement between
the two.

### A second, real gap found WHILE fixing the first -- checked before assuming it was covered

Testing surfaced `getArea(*shape)` (a dereferenced pointer passed as a
reference argument) NOT getting wrapped, while a plain variable
argument correctly was. Traced directly rather than assumed: `infer_
expr_type` (sema.c), which `address_of_if_needed` calls to decide
"is this already a pointer", never had a case for `AST_UNOP` at all
(`*p`, `&x`) -- every dereference or address-of expression fell
through to the function's own `default: return NULL;`, and `address_
of_if_needed`'s OWN best-effort philosophy ("couldn't determine its
type -- don't guess") then left it unwrapped, exactly the wrong call
for a genuinely determinable case. This is the same class of gap
AST_SUBSCRIPT's own comment in this same function already describes
(a real correctness hole silently doing nothing until something
exercised it) -- fixed the same way, with `deref` unwrapping one level
of AST_POINTER_TYPE and `addr` wrapping the operand's own type in a
fresh one. This fix improves BOTH the new reference-argument logic and
the older, pre-existing receiver-address logic equally, since both
route through the same `address_of_if_needed`/`infer_expr_type` pair.

### Verification

Full syntax-check; rebuilt against the real, regenerated grammar
(already synced this session, not the stale one); confirmed directly,
not just reasoned through -- a plain reference argument
(`getSize(a)` -> `getSize__Shape_ref((&a))`), a dereferenced-pointer
reference argument (`getSizeViaDeref(*b)` -> `getSizeViaDeref__
Shape_ref((&(*b)))`), and `sample68`'s own const-reference case
(`getArea__Shape_ref((&shape))`) all now lower correctly. Full
70-sample suite (`sample70.cpp` new, covering both of the above
directly) run end to end with zero regressions -- the same 13
deliberately-invalid samples, nothing else.

## The `--target` flag and the vircon32-mode ternary rewrite -- two features, one round, real bugs found in each

Matthew asked for an architectural assessment before committing to a
two-pass (emit standard C text, re-parse, re-emit Vircon32 C) design.
Investigated directly rather than reasoning abstractly: `lower.c`
already has essentially zero Vircon32-specific LOGIC (only the
`v32_new_`/`v32_delete` naming convention, cosmetic, not structural),
and `docs/VIRCON32_QUIRKS.md` -- accumulated round by round across this
whole project, not written for this occasion -- already enumerated
every place generated C diverges from standard C, each with its own
"for a standard-C mode" note. Recommended a single-pipeline,
target-dialect-flag design instead of literal two-pass reparsing
(which would mean re-solving C declarator parsing, exactly this
project's own repeated hard part, from scratch, on TEXT, having thrown
away the type information the AST already carries for free). Matthew
agreed; this round is that flag, built for real.

### `--target=vircon32|v32|standard|std`

New `CodegenTarget` global (`driver.h`/`main.c`), matching the
established `g_uses_new_or_delete` global pattern rather than
threading a parameter through every function that might eventually
need it. Cart XML and debug-map emission forced off entirely for
standard mode (Vircon32-platform concerns with no meaning outside it),
in one place right after CLI parsing, not as a second condition at
each of the two call sites -- avoids the two ever drifting out of sync
with each other.

Worked through `VIRCON32_QUIRKS.md`'s own checklist, entry by entry --
full detail lives there now (every entry marked IMPLEMENTED, NOT YET
IMPLEMENTED, or N/A), but two things are worth calling out here
specifically because they didn't match the plan on paper:

**The `struct`-keyword entry was predicted as "the cheapest on the
whole list" and turned out to be one of the more involved ones.**
Fixing `print_type` alone (the function this whole checklist assumed
was the single funnel point) wasn't enough: `emit_new_delete_runtime`,
`emit_array_new_runtime`, `emit_vtable_struct`, `emit_vtable_instance`,
and `emit_method_prototype` all build a class's own type name by hand,
entirely bypassing `print_type`. Found only by actually running
`--target=standard` against a real class-having test and reading the
output line by line (`struct Shape *v32_new_Shape...` sitting a few
lines above a bare, un-prefixed `Shape *self = ...`), not by
re-deriving every call site from the plan alone -- exactly the kind of
gap a systematic-sweep-by-reasoning-only would have missed. Fixed with
a new `print_class_type_name` helper, swept across every call site
found this way.

**A self-inflicted bug along the way, worth naming as a lesson**: a
doc comment written for `print_class_type_name` contained the literal
text `v32_new_*/v32_delete*`, which happens to contain the two-
character sequence `*/` -- prematurely closing the C comment itself
and turning the rest of it into malformed code. Caught immediately by
the very next syntax check (which this project runs after every edit,
not just logic changes), not shipped -- but a reminder that comment
edits need the same verification discipline as code edits, not less.

**The array-declarator entry's own prediction held up exactly**:
`print_type`'s "prefix, then caller appends the name" architecture
genuinely can't produce `name[N]` on its own, confirmed by
implementing the predicted fix (a new `print_array_suffix` helper,
called by the two call sites that can ever reach an array-typed
declaration -- traced directly, not assumed, that a return type or
parameter type never can).

**`void main(void)`**: the one entry the checklist itself flagged as
needing a deliberate decision rather than a toggle, since forcing
`void` changes real program behavior (an exit code becoming
unobservable). Decision made: standard mode honors whatever `main`'s
own C++ declaration actually said.

**Function-pointer declarators/casts (#3/#4) were scoped out of this
round entirely, deliberately.** The name has to sit INSIDE the parens
for standard C, not get appended after -- the same suffix trick that
solved arrays doesn't apply, and would need its own prefix/suffix
split at multiple call sites. Confirmed with Matthew before treating
this as settled: the C++-side dual-input-syntax acceptance for
function pointers was always a Vircon32-specific nicety, never
something standard mode needed to preserve, which removed one
possible reason to attempt the fuller fix under time pressure this
round.

### The ternary-to-if/else rewrite (`vircon32` mode only)

A genuinely new quirk, not previously on `VIRCON32_QUIRKS.md`'s own
list at all -- the real Vircon32 compiler doesn't support the ternary
operator, reported directly by Matthew alongside the `--target` work.
Added as entry #10 to that same checklist, with the same evidentiary
honesty the rest of the list holds itself to: flagged as
Matthew-reported, not yet independently confirmed against the real
compiler the way most other entries have been.

Design turned out simpler than first planned: `var_decl`, a plain `=`
assignment to a bare identifier, and `return` each already have a
natural place to put a value (the variable's own name, the lvalue, or
a return statement), so no temporary variable is needed at all --
`int x = cond ? a : b;` becomes `int x; if (cond) { x = a; } else
{ x = b; }`, and similarly for the other two shapes. Implemented as a
new, final lowering phase (phase 10), running only when `g_target ==
TARGET_VIRCON32` and only after every earlier phase that still treats
AST_TERNARY as an ordinary expression node has finished.

**A real gap caught by testing against an EXISTING sample, not a new
one written to order**: `tests/sample58.cpp`'s own `classify` function
(`x < 0 ? -1 : (x == 0 ? 0 : 1)`, already in this project's suite,
written to test chaining/precedence long before this phase existed)
initially only got its OUTERMOST ternary rewritten -- the nested one
sitting in the else-branch survived untouched, producing output that
would still fail to compile on the real Vircon32 toolchain. Fixed by
recursively re-checking each newly-built branch before wrapping it
into its own if/else, so a chain unwinds one level at a time until
nothing ternary-shaped remains -- confirmed directly against the same
sample afterward, not just reasoned to be fixed.

SCOPE, stated plainly rather than left implicit: only the three
DIRECT shapes above are rewritten. A ternary nested inside a call
argument, a larger arithmetic expression, a for-loop's own clauses, or
assigned through anything other than a bare identifier is left
completely untouched -- confirmed this fails predictably (not silently
mishandled) via a dedicated test case. The assignment case specifically
needed a narrower check than "any assignment": a general lvalue
duplicated across both an if-branch and an else-branch would risk
double-evaluating a side effect inside it (`arr[i++] = cond ? a : b;`);
a bare identifier has no such risk, so that's the line drawn.

### Verification

Full syntax-check across every changed file (including catching and
fixing the malformed-comment bug above before it went anywhere).
Rebuilt against the real, already-synced grammar (no grammar changes
at all this round -- everything here is `.c`-file work). Full 71-sample
suite in `vircon32` mode, zero regressions; a second full sweep of
every real sample under `--target=standard`, using each sample's own
exact Makefile flags (`-c` where needed) rather than a uniform
invocation, to avoid the false "no main" failures a naive re-run
produced at first -- zero unexpected failures there either.
`tests/sample71.cpp` (new) exercises all three ternary-rewrite shapes,
the chained case, and the deliberate call-argument boundary in one
file, confirmed directly: exactly one ternary survives in `vircon32`
mode (the documented boundary case) and all four survive, untouched,
in `standard` mode.

## Function-pointer standard-mode output, closed for real -- and a genuinely new gap found while scoping the array follow-up

Matthew asked to close the function-pointer standard-mode gap the
previous round deliberately deferred, then look at any remaining
array-related work.

### Function pointers: closed

Confirmed directly (not re-guessed) that `ast_wrap_func_ptr` is only
ever called from `var_decl`'s own grammar productions, and `var_decl`
is what both a variable declaration and a class member (`member: ...
| var_decl ';'`) reduce to -- meaning exactly two call sites
(`print_var_decl_inline`, `emit_struct`'s own field loop) can ever
need to print a function-pointer-typed declaration, and neither a
parameter nor a return type ever can. New `print_type_and_name`
(codegen.c) builds the entire standard-mode declarator as one self-
contained unit at those two sites, rather than trying to force the
name-goes-inside-the-parens shape through `print_type`'s own single-
type-in-single-string-out contract the way the array fix's suffix
trick could. `emit_vtable_struct`/`emit_vtable_instance` (their own
separate, inline declarator/cast text, never routed through
`print_type` at all) got the identical fix applied directly. No
grammar changes needed anywhere -- confirmed with a genuinely
comprehensive test pass this time, not just the two or three samples
checked after the earlier `--target` round: `sample64`/`66` (plain and
array-of function pointers) and `sample14` (vtable dispatch) all
produce correct standard C, read line by line, not just "exit code
0" checked.

### The array follow-up surfaced a real, deeper complication -- reported rather than rushed

Investigated multi-dimensional array PARAMETER decay (`void foo(int
arr[8][4])` -- currently single-dimension only) as the clearest
remaining array item within reach. Real C/C++ semantics here are more
involved than the single-dimension case already handled: only the
FIRST dimension decays to a pointer; the rest stay as array
dimensions, so the parameter's own true type is `int (*arr)[4]` -- a
pointer to a 4-element array of int, not a flat `int **arr`.

Tracing through what `print_type` would need to emit for that type
today surfaced a genuinely new problem, not specific to multi-
dimensional array parameters at all: this project has never once
needed to print a POINTER-TO-ARRAY type before, and `print_type`'s
existing `AST_POINTER_TYPE` case (`print_type(type->a); " *"`) gets it
wrong in EITHER dialect, not just standard mode -- for `AST_POINTER_
TYPE(a=AST_ARRAY_TYPE(4, int))`, it would recurse into the array case
first (`int [4]` in Vircon32 mode, `int` with a deferred suffix in
standard mode) and then append `*` OR expect the caller to eventually
append `[4]`, producing `int [4] *name` or a similarly wrong shape --
neither is `int (*name)[4]`, the correct form, which (like a function
pointer) needs the name INSIDE parentheses, sitting between the `*`
and the array bracket, not appended after either one. The exact same
class of problem the function-pointer fix just solved, but for a
different type combination this project has simply never produced
before.

Reported rather than rushed: implementing multi-dimensional array
parameter decay would mean discovering and fixing this pointer-to-
array declarator gap at the same time, under the same time pressure
that already produced one incomplete first attempt this session (the
original, print_type-only function-pointer fix that missed several
call sites two rounds back). Better to scope and verify this
deliberately in its own pass than repeat that pattern. The OTHER
lingering array item -- the Vircon32-style array-of-function-pointers
declarator spelling, still unconfirmed against the real compiler --
isn't something a round of local work can resolve at all; it
genuinely needs Matthew's own build.

### Verification

Full syntax-check; rebuilt against the real, already-synced grammar
(no grammar changes this round either). Full 71-sample suite in both
`vircon32` and `standard` mode, zero regressions. Function-pointer-
specific standard-mode output read directly, not just checked for a
clean exit: `int (*fp)(int, int);`, `int (*ops[2])(int, int);`,
`int (*Shape__area__void)(struct Shape *);`, and
`(int (*)(struct Shape *))&Circle__area__void` all confirmed correct
against real standard C declarator/cast rules.

## The one-word parameter/return check -- implemented once the numbers arrived, and a real finding in this project's own existing test suite

Matthew confirmed the array-of-function-pointers spelling (now marked
CONFIRMED everywhere it was flagged unconfirmed) and supplied exactly
the numbers the previous round's entry #11 said were missing:
Vircon32 is 32-bit and word-based, `int`/`float`/pointers are all one
word, and `char`/`short`/`double`/etc are alias syntactic sugar over
the same 4-byte word -- meaning EVERY supported primitive type is
uniformly one word, with no per-type size table needed at all.

### Why this made the check genuinely low-risk to build, not just theoretically possible

A class/struct's own size in words reduces to its `StructLayout`'s own
field count directly -- no arithmetic, no per-field width lookup. This
is the exact missing piece the deferral in the previous round was
about: implementing a size check without confirmed per-primitive sizes
risked getting the boundary wrong in either direction (false-warning
valid code, or missing genuine violations) -- with these numbers, the
computation is simple enough to trust.

### Where it had to live, discovered by checking timing, not assumed

Tried to hook this into `sema.c` first, matching `dynamic_cast`'s own
established `sema_warning()` precedent -- but `StructLayout` doesn't
exist yet when `sema.c` runs at all; `compute_struct_layouts` is the
very first step of `lower_run`, which runs strictly after semantic
analysis finishes. Moved the check to `lower.c` instead, right after
`compute_struct_layouts`, and exposed `sema_warning` itself (no longer
`static`) so `lower.c` could reuse the same counting/formatting
machinery rather than duplicating it -- the two passes are
conceptually the same kind of thing (diagnostics about accepted code),
just needing to run at different points in the pipeline.

### Scope, the same conservative direction as every other best-effort check here

Only a BARE (not pointer, not reference; `const`-qualified still
counts) class/struct parameter or return type with more than one
`StructLayout` field triggers the warning. Two deliberate, stated
gaps: an array- or nested-struct-typed DATA MEMBER only ever
contributes one to its own class's field count, even though it may
itself be several words -- a genuine under-count in a narrow case,
accepted in the same "miss a violation rather than false-warn"
direction this project already applies elsewhere; and unions aren't
checked at all, since an ordinary union of primitive members is
already one word by construction (members overlap, not stack).

### A real finding, not manufactured for the occasion

Running this against the EXISTING 71-sample suite (not a new test
written to demonstrate the feature) surfaced that `tests/sample9.cpp`
and `tests/sample15.cpp` -- both exercising operator overloading on a
two-field `Vector2D` class -- have every one of their `operator+`/
`operator-`/etc taking or returning `Vector2D` BY VALUE, two words,
over the limit. Both samples have "passed" throughout this entire
project's history; this warning is the first thing to ever surface
that their generated C would not actually compile on real Vircon32
hardware. Left the samples exactly as they are rather than quietly
rewriting them to pass by reference -- that's a real, separate
decision about what those two tests are meant to demonstrate, worth
Matthew's own call, not something to change as a side effect of
adding a diagnostic.

### Verification

Full syntax-check; rebuilt against the real, already-synced grammar
(no grammar changes -- this is a `.c`-file-only diagnostic pass).
Directly tested all four combinations: a two-word struct by value in
vircon32 mode (warns, on both the parameter and the return case), a
one-word struct by value in vircon32 mode (silent), a two-word struct
by pointer in vircon32 mode (silent), and the same two-word-by-value
case under `--target=standard` (silent, since the restriction doesn't
apply there). Full 71-sample suite run in both modes -- same pass/fail
outcome as before (warnings don't fail a build), with the two new,
genuine warnings on `sample9`/`sample15` confirmed by reading their
own source, not just trusted from the tool's own output.

## Correcting sample9/sample15 -- a partial, honest fix, and a significant new gap it surfaced

Matthew asked to correct the two samples the word-size check flagged,
since neither was meant to test a failure.

### The fix that worked: parameters

Every `Vector2D` parameter across both files now takes `const Vector2D
&` instead of by value -- straightforward, idiomatic, and confirmed
directly to eliminate every parameter-side warning while leaving the
generated C correct (reference-to-pointer lowering and call-site `&`
insertion, both fixed in earlier rounds, confirmed still working
correctly here too).

### The fix that couldn't happen, discovered by trying it

The natural fix for the RETURN side -- have a value-producing operator
like `operator+` return a pointer to a newly-`new`'d result, the
realistic Vircon32 idiom for this -- turned out to be blocked by a
significant, previously-undiscovered gap: NO function anywhere in this
grammar can return a pointer or reference type at all. Confirmed
directly with a minimal, unrelated test (`int *getPtr(int x) { return
&x; }` fails to parse) before concluding this wasn't specific to
operators -- `func_header`'s own grammar simply has no `pointer_opt`
between the return type and the function name, unlike `var_decl`/
`param`, which both do. An entirely ordinary `Shape *makeShape()`
fails the identical way.

Rather than expanding scope into a grammar change to unblock this
(risky under the circumstances -- a change I can't verify with bison
myself, discovered mid-task, for a correction that was supposed to be
narrow), reverted to the parameter-only fix and documented the
remaining return-side warnings honestly in each sample's own header
comment, rather than silently leaving them unexplained or forcing
through an untested grammar change to make them disappear. Added as
`docs/VIRCON32_QUIRKS.md`'s own entry #12, cross-referenced from entry
#11's own note about these two samples.

### Verification

Both samples transpile cleanly (`sample9`: 4 warnings, down from 8,
all parameter-side eliminated; `sample15`: 4 warnings, down from 12,
same pattern) -- confirmed by reading the actual remaining warnings,
not just a reduced count. `sample15`'s own generated output read
directly to confirm the reference lowering is genuinely correct in
this real case, not just trusted: `Vector2D__op_add__Vector2D_ref((&a),
(&b))` and similar at every call site. Full 71-sample suite, both
modes, zero regressions.

## Closing entry #12: pointer/reference return types

Matthew asked to actually close this gap rather than leave it
documented. Unlike the round above, this one had a genuine bison+flex
toolchain available (built from source in-sandbox this round -- bison
was already present, flex wasn't and had to be compiled from its own
release tarball, fetched over GitHub; see this file's own earlier
notes on why neither was assumed available before), so every claim
below is verified by actually running the real parser and actually
compiling the generated C, not reasoned about from the grammar alone.

### The grammar fix

`pointer_opt` added to `func_header`'s and `out_of_line_def`'s first
alternatives, matching `var_decl`/`param`'s existing shape exactly.
The interesting part: `%expect` didn't need to go UP. A real `bison -d`
run showed the conflict count dropping from 26 to 25 -- diffing
`bison -v`'s own `.output` state tables before and after the change
showed why: `out_of_line_def` used to lack `pointer_opt` entirely,
so a bare `type_spec` followed by an identifier forced a 1-shift/
reduce fork between "reduce pointer_opt to nothing (var_decl path)"
and "shift into qname_prefix (out_of_line_def path)". Giving
`out_of_line_def` its own `pointer_opt`, agreeing with `var_decl`,
merges that fork away instead of opening a new one. Recorded as a
lesson directly in `parser.y`'s own `%expect` comment: a plausible-
sounding "this shouldn't change the count" prediction was drafted
first and was WRONG (in the safe direction -- fewer conflicts, not
more -- but still a real miss), which is exactly the kind of guess
this project's own established rule (run bison, read the actual
conflicts, don't assume) exists to catch.

### The lowering fix

Three parts, mirroring the existing reference-PARAMETER handling
wherever a symmetric reference-RETURN case existed:
1. `inject_reference_return_address_stmt` -- implicit address-of at a
   `return expr` site when the function returns a reference, run
   before phase 5 relabels reference types to pointer types (same
   ordering hazard as the existing reference-parameter fix).
2. `fix_references_in_method`/`fix_references_free_functions` (phase
   5) now also relabel a function's OWN return type -- previously only
   ever touched parameters/locals.
3. Implicit dereference at a reference-returning CALL's use site
   (`finalize_calls_expr`'s `AST_CALL` case) -- the point at which this
   stopped being purely grammar-shaped and became a real, build-
   verified bug hunt: the first working build assigned a raw `int *`
   into a plain `int` local (`int viaReference = obj.getValueRef();`),
   caught immediately by actually compiling the generated C with gcc,
   not by reading the AST dump and assuming it was fine.

### Two adjacent, pre-existing bugs found by actually compiling output

Both invisible to every previous round, which could only parse-check,
never compile-check:
- `address_of_if_needed` treated `AST_POINTER_TYPE` as "already a
  pointer, don't add `&`" but not `AST_REFERENCE_TYPE` -- so a
  reference PARAMETER forwarded as another reference-typed argument
  (`sample15.cpp`'s `addThem(const Vector2D &a, ...)` computing
  `a + b`) got a wrongly-inserted extra `&`, producing a real double-
  pointer compile error (`const Vector2D **` where `const Vector2D *`
  was expected). This is a pre-existing bug, not something this
  round's own changes introduced -- `sample15.cpp` had this exact
  shape before this round too, it just had never been build-verified
  before. Fixed by treating `AST_REFERENCE_TYPE` the same as
  `AST_POINTER_TYPE` in `address_of_if_needed` (justified directly:
  `infer_expr_type` returns a reference parameter's DECLARED type
  as-is at this phase, before phase 5 ever mutates it, so a reference-
  typed identifier here is already known to be pointer-bound).
- Left OPEN, deliberately, as out of this entry's own scope: this
  project still doesn't model const-correctness on a method's own
  `this` receiver, so the same `sample15.cpp` scenario still produces
  a `-Wdiscarded-qualifiers` WARNING (not a hard error) when a `const
  Vector2D &` is forwarded as a method receiver. A real, narrower gap,
  worth its own entry if it ever blocks something the way entry #12
  itself did -- not folded into this fix's own scope since it's about
  const-modeling on receivers generally, not specific to return types.

### Verification

`sample9.cpp`/`sample15.cpp` rewritten again (their operators now
return `Vector2D *` via `new`, closing the by-value-return warning
entry #11 could only partially close before). New dedicated
`tests/sample72.cpp` added specifically for the GENERAL, non-operator
case (in-class + out-of-line pointer-returning method, free function
returning a pointer, reference-returning method) -- entries #9/#15
already covered the operator-specific angle, but neither exercises an
out-of-line pointer-returning method definition or a plain function
returning a reference on its own terms.

All three transpile cleanly and their generated C compiles with plain
`gcc` (`--target=standard`, `-fsyntax-only`) with zero errors.
`sample72.cpp` additionally compiled and RUN directly, producing
exactly the expected values (`viaPointerMethod=7 viaFreeFunction=7
viaReference=7`) -- genuine end-to-end verification, not just a clean
parse. Full 72-sample suite (`make test`) run clean: only the
already-documented, deliberately-invalid samples fail, matching the
Makefile's own `-`-prefix list exactly, zero new regressions.

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

## Function-pointer typedefs and multi-declarator statements -- plus a real, previously-undiscovered gap found and fixed while verifying both end to end

Two items from the "second round of basic C gaps" fresh-audit list,
tackled together: function-pointer `typedef`s and multiple declarators
in one statement (`int a, b, c;`).

### Function-pointer typedefs: a small, surgical addition

Two new `typedef_decl` grammar productions in `parser.y`, mirroring
`var_decl`'s own two function-pointer declarator alternatives exactly
(standard-C `typedef ReturnType (*Name)(ParamTypes);` and Vircon32-
native `typedef ReturnType(ParamTypes)* Name;`), reusing the existing
`AST_FUNC_PTR_TYPE`/`ast_wrap_func_ptr` machinery with zero new AST
work. The only codegen change needed: `emit_typedefs` was calling
plain `print_type` (which always emits Vircon32's own form,
unconditionally, per its own doc comment) directly followed by the
name -- routed through `print_type_and_name` instead (the same helper
`print_var_decl_inline`/`emit_struct` already use for this exact
reason), which already handles the standard-mode "name inside the
parens" shape correctly. A typedef is simply the THIRD place a
function-pointer type can be named, `print_type_and_name`'s own doc
comment having already confirmed (a previous round) that only two call
sites could ever reach one -- that comment is now stale in the narrow
sense that a third exists, but the function's own logic needed no
change at all to handle it, since it dispatches on the TYPE node, not
on which caller invoked it.

### Multi-declarator statements: a parser-internal carrier, not a new AST shape reaching sema/lower/codegen

`int a, b, c;` needs ONE grammar reduction to produce MULTIPLE
AST_VAR_DECL nodes -- not possible directly, since a bison action
produces exactly one `$$`. Solved with a new, deliberately narrow
`AST_VAR_DECL_GROUP` node (list of AST_VAR_DECLs, see its own doc
comment in `ast.h`) that never survives past parsing: every caller
that folds a `var_decl` into a surrounding list (`top_decl_list`, a
block's own `stmt_list`, `member_list`, `union_member_list`) now uses
a new `ast_list_append_flatten` (`ast.c`) instead of a plain
`ast_list_append`, expanding the group back into its own several
entries in the SAME action that built it. sema.c/lower.c/codegen.c
never gain a case for this node kind at all, because they never see
one.

Scoped deliberately narrow, matching this project's own established
pattern: only a PLAIN declarator (bare or pointer/reference-wrapped)
can follow the first comma -- an array or function-pointer declarator
mixed into a multi-declarator statement is not supported, and a
for-loop's own init clause (`for (int i = 0, j = 0; ...)`) is a
separate, stated gap, reported directly (`YYERROR` from within
`for_init`'s own grammar action -- this grammar has no `error`-token
recovery production anywhere, so this makes `yyparse()` itself fail
immediately, the same real, build-stopping outcome an ordinary syntax
error already has) rather than silently dropping every declarator but
the first, which a mere warn-and-degrade response would have done.

A new nonterminal, `more_plain_declarators`, hands back each
additional declarator as a bare, UNRESOLVED carrier (reusing the
`AST_VAR_DECL` node shape purely as a 3-field carrier: name, the raw
`pointer_opt` value in `->ival`, and the initializer) rather than a
resolved type, because a bison nonterminal can't see a PARENT rule's
own symbols -- `var_decl`'s own base `type_spec` isn't visible from a
separate production. `var_decl`'s own plain-declarator action, which
DOES have the base type in hand, resolves each carrier into a real
`AST_VAR_DECL` afterward, using the identical `pointer_opt`-to-type
logic the primary declarator already had.

### A stale-build false alarm, caught before it was reported as a real regression

Adding the typedef productions alone (bisected in isolation, before
the multi-declarator work was even added) appeared to break
`tests/sample1.cpp` with a completely unrelated semantic error
("'Entity' has no default constructor...") on a file containing no
typedefs at all -- alarming, since it suggested a grammar change was
somehow corrupting unrelated parsing. Investigated with an actual
bisect (copying changed files one at a time into a fresh pristine
checkout) before concluding anything, and `bison -Wcounterexamples`
confirmed the conflict structure was BYTE-IDENTICAL (same md5sum)
before and after the typedef change -- ruling out a grammar-ambiguity
explanation outright. The real cause: an incremental `make -j4` had
left a stale, ABI-mismatched object file linked into the test binary
(this project's own Makefile doesn't declare every `.c` file as
depending on `inc/parser.h`, so a header-only change doesn't always
trigger every dependent recompile). A full `make clean && make`
reproduced nothing -- the "regression" never existed. Recorded here
specifically because it's a real trap for future verification work in
this project: an incremental rebuild after a grammar/header change is
not sufficient evidence a change is safe or unsafe; a clean rebuild is
required before trusting either a pass or a failure.

### A real, previously-undiscovered gap found and fixed: function pointers never actually compiled when assigned a bare function name

Verifying the new typedef feature end to end (writing a test that
assigns a real function to a typedef'd function pointer, then actually
compiling the generated C with `gcc --target=standard`, not just
transpiling it) surfaced that this NEVER worked, typedef or not:
`Callback cb = doubleIt;` transpiled to the literal, unmangled
`(cb = doubleIt);`, but every function gets a mangled name regardless
of whether it's overloaded (`mangle()`'s own doc comment) -- only
`doubleIt__int` actually exists in the generated C, so this is a real
compile error (`'doubleIt' undeclared`), confirmed directly with gcc.
Reproduced on `tests/sample64.cpp` too, already in this project's
suite and "passing" in the sense of transpiling without error for its
entire history -- nothing had ever actually COMPILED it until this
round did.

Root cause: `finalize_calls_expr` (`lower.c`, phase 3/4) already
rewrites a CALL's own callee to its mangled name (via the call's own
`CallResolution`), but nothing rewrote a bare function-NAME reference
used as a plain VALUE -- the overwhelmingly common way to initialize
or assign a function pointer at all. Fixed with a new `AST_IDENT` case
in `finalize_calls_expr` itself: a local/parameter of the same name
(`find_local`) always wins first, matching real C++ scoping; otherwise
`collect_free_function_candidates` (the same primitive sema.c's own
call resolution already uses) is checked, and the identifier is
rewritten to the sole match's mangled name ONLY when exactly one
candidate exists -- an overloaded function used as a bare value has no
argument list here to disambiguate against, and real C++'s own rule
for that (matching against the function pointer's own declared type)
isn't modeled anywhere in this project, so an ambiguous case is left
completely untouched rather than guessed at. A name that isn't a free
function at all (a global variable, an enumerator) gets zero
candidates back and is left exactly as it was -- a strict no-op for
every identifier that isn't unambiguously a free function's own name.

### Verification

Full clean rebuild (`make clean && make`, not incremental -- see the
stale-build false alarm above), bison run confirming the exact same
`%expect 25`/zero-reduce-reduce baseline (no new conflicts from either
feature). Full 72-sample suite (`make test`), zero regressions --
diffed byte-for-byte against a pre-round baseline, confirming the
ONLY generated-output changes were the three function-pointer samples
now correctly mangled (`sample64`/`65`/`66`) and the deliberately new
ones added this round. `tests/sample73.cpp` (function-pointer
typedefs, both spellings, actually assigned and called through) and
`tests/sample74.cpp` (multi-declarator locals/globals/members, mixed
initializers, a pointer among plain declarators) both confirmed
correct in BOTH targets: read directly for correct declarator shape,
and (`--target=standard`) compiled with `gcc -fsyntax-only` (zero
errors) and actually RUN, producing the expected computed result in
each case (25 and 146 respectively) -- genuine end-to-end
verification, not just a clean parse.

### A separate, pre-existing `--target=standard` gap found while auditing, not fixed this round

Running the full test suite's own `--target=standard` output through
`gcc` (not just this round's own two new samples) surfaced that
`--target=standard` doesn't compile for almost any class-having
program in this project's ENTIRE existing suite: the `bool`/`true`/
`false` runtime boilerplate every class triggers has no
`#include <stdbool.h>` in standard mode. Separately,
`tests/sample60.cpp`/`sample61.cpp` (already in the suite) confirmed a
second, related gap: an `enum`/`union` type referenced by name outside
its own definition never gets its keyword back in standard mode the
way a class/struct reference already correctly does (`type_to_class`,
which `print_type`'s own standard-mode fix relies on, has no notion of
enums/unions -- only the class registry). Neither gap touches
Vircon32-mode output (this project's actual primary target) at all,
and neither is caused by or related to this round's own two features
-- reported here, not fixed, since `--target=standard`'s own maturity
is a separate concern from what this round was asked to do.

## Round: real-compiler-confirmed bug -- Vircon32 C does not implicitly decay a function name to a function pointer

The previous round's `AST_IDENT`-mangling fix (above) was verified
only against `gcc --target=standard` output, which made
`Callback cb = doubleIt__int;` look completely correct: standard C
implicitly decays a bare function name to its own address in this
position, so a mangled-but-unwrapped identifier compiles and runs
fine there. The user rebuilt this project themselves (their own
bison 3.8.2, since the delivered zip didn't include generated
parser/lexer output) and ran the ACTUAL Vircon32 C compiler against
`tests/sample73.cpp`'s Vircon32-mode output, which rejected exactly
those two lines:

```
sample73.c:31:17: error: types are not compatible: cannot assign int(int) to int(int)*
sample73.c:32:24: error: types are not compatible: cannot assign int(int) to int(int)*
```

This is a genuine, previously-unknown Vircon32 C quirk, invisible to
every check this project had run up to that point (gcc's own decay
rule papers directly over it): a bare function name in Vircon32 C has
plain function type (`int(int)`), not pointer-to-function type
(`int(int)*`), and does NOT implicitly convert between the two the
way standard C does -- an explicit `&` is required to actually form
the pointer value. Since the earlier fix only mangled the identifier
without ever taking its address, EVERY function-pointer
initialization/assignment this project had ever generated in
Vircon32 mode was broken this same way, not just the two new lines
the user happened to report -- including `tests/sample64.cpp`,
`sample65.cpp`, and `sample66.cpp`'s own `fp = add;`-style plain
assignments (already in the suite, "fixed" only in the gcc-verifiable
sense) and `sample66.cpp`'s array-of-function-pointers case
(`ops[0] = add;`).

Confirmed, before writing any fix, that this asymmetry between the
two dialects is actually safe to resolve by ALWAYS emitting the
explicit `&` form regardless of target: standard C treats a bare
function name and `&functionName` as identical pointer values in
this position (the implicit decay and the explicit address-of
produce the same result), so there's no dialect split to encode here
-- `Callback cb = &doubleIt__int;` is exactly as valid, idiomatic
standard C as the bare form, and is what real Vircon32 C requires.

Fixed in `lower.c` with two small, narrowly-scoped checks (not a
generic "always wrap a function reference in `&`" rule, which would
have double-wrapped a user-written `&doubleIt` -- see the reasoning
below):

- `finalize_calls_stmt`'s `AST_VAR_DECL` case: before recursing into
  the initializer, if the initializer (as the user actually wrote it)
  is a bare identifier naming exactly one free function
  (`is_bare_free_function_ref`, factoring out the same
  `find_local`-then-`collect_free_function_candidates` check the
  existing `AST_IDENT` case already used) AND the VarDecl's own
  declared type resolves, through any typedef chain
  (`resolve_typedef_chain`, newly exposed from `sema.c` for this --
  same reuse pattern as `type_to_class`/`sema_warning`/
  `collect_free_function_candidates` before it), to `AST_FUNC_PTR_TYPE`
  (`type_is_func_ptr`), the initializer is wrapped in an explicit
  `&` (`wrap_addr_of`, same "addr" `AST_UNOP` shape a user-written
  `&x` already parses to) BEFORE `finalize_calls_expr` recurses into
  it.
- `finalize_calls_expr`'s `AST_ASSIGN` case: same check, but against
  the LHS's INFERRED type (`infer_expr_type`, already exposed) rather
  than a VarDecl's own declared type -- covers `fp = add;` and
  `ops[0] = add;` alike, since `infer_expr_type` already resolves an
  `AST_SUBSCRIPT` target's element type. Scoped to plain `=` only
  (`n->str1 == "="`) -- compound assignment on a function pointer
  isn't meaningful and isn't supported anywhere else in this project.

Doing the check BEFORE recursing into the initializer/RHS (rather
than inspecting the result AFTER `finalize_calls_expr` already
mangled it) is what avoids the double-wrap risk: the check runs
against the ORIGINAL AST shape, so a user-written `&doubleIt`
(already an `AST_UNOP`, not a bare `AST_IDENT`) is correctly left
alone -- `finalize_calls_expr`'s existing `AST_UNOP` case recurses
into it exactly once, hitting the `AST_IDENT` case exactly once, with
no wrapping added by this round's new checks at all. Likewise, `is_bare_free_function_ref` reuses the EXACT same
"not a local, exactly one free-function candidate" test the existing
mangling case uses, so copying one function-pointer variable into
another (`Callback cb2 = cb;`) is correctly left untouched -- `cb` is
a local, not a free function, so no `&` is added and `cb2` still
correctly ends up holding the same pointer value `cb` does, not the
address of the variable `cb`.

### Verification

Full clean rebuild (`make clean`, remove all generated parser/lexer
output, `make -j4`) -- zero warnings. `tests/sample73.cpp` regenerated
in Vircon32 mode now reads
`Callback cb = (&doubleIt__int); NativeCallback ncb = (&tripleIt__int);`,
directly matching what the real Vircon32 compiler rejected before and
requires now. `sample64`/`65`/`66` (Vircon32 mode) all now correctly
read `(fp = (&add__int_int));`-shaped assignments, including
`sample66`'s array-of-function-pointers case
(`(ops[0] = (&add__int_int));`). Full `make test` (74 samples): no new
failures -- every flagged sample matches this project's own
already-expected-to-fail negative tests (the `-$(BIN)` Makefile
entries), confirmed by cross-referencing the Makefile itself, not
just eyeballing the log. Full `--target=standard` + `gcc -fsyntax-only`
sweep across all 74 samples: 34 pre-existing failures, all confirmed
to be the already-documented `<stdbool.h>`/enum-union-keyword
standard-mode gaps above (re-diffed each failure's actual gcc error
text against that known cause, not just counted) -- zero NEW
`--target=standard` failures from this round's own change, and
`sample64`/`65`/`66`/`73` specifically confirmed gcc-clean in standard
mode too (the inserted `&` is valid, idiomatic standard C, as
reasoned above).

### Priority-list update, per explicit user request

The user's report that surfaced this bug also asked, in the same
message, that the pre-existing `--target=standard` class-compilation
gap (documented in the round above -- missing `<stdbool.h>`, missing
enum/union keyword-on-reference) be added to "the priority list of
things to fix." Recorded here as that priority note; not addressed as
part of this round, which was scoped to the real-compiler-confirmed
function-pointer bug specifically. See README.md's own "What doesn't
exist yet" list for the tracked item.

## Round: two more real-compiler-confirmed bugs -- const receiver casting, and ternaries nested anywhere but the three "direct" shapes

The user rearranged their own copy of this project (renamed
`tests/sample##.cpp` to `##sample.cpp`, renamed transpiled output by
STATUS -- `##program.c` for working code, `##partial.c` for code
without `main()`, `##failure.c` for expected failures -- and added a
new `archive` Makefile target of their own for bundling the project
state for these updates) and ran the CURRENT project's own test suite
(`68sample.cpp`/`71sample.cpp`/`73sample.cpp`/`74sample.cpp`, i.e. this
project's own `sample68`/`71`/`73`/`74`) through a REAL Vircon32
compiler again. Two real compile failures came back, on files the
user expected to be genuinely working code (`##program.c`, not
`##failure.c`):

```
68program.c:108:48: error: cannot assign const struct Shape* to struct Shape*: discards const qualifier
71program.c:66:42: fatal error: character '?' is not a valid identifier start
```

Neither was a regression from anything recent -- both were
previously-DOCUMENTED, but previously believed either harmless
(the const one, gcc-only-warns) or fully out of scope (the ternary
one, only the "direct" three shapes were ever rewritten) -- exactly
the same pattern as the function-pointer `&`-insertion bug in the
round above: gcc's own leniency masked a real Vircon32-only rejection
that only an actual compiler run could surface.

### Bug 1: forwarding a const object as a non-const method's receiver

`tests/sample68.cpp`'s `getArea(const Shape &s) { return s.area(); }`
(`area()` itself NOT declared `const`) is exactly the scenario
`docs/VIRCON32_QUIRKS.md` entry #12 had already flagged as a KNOWN,
left-open gap: this project doesn't enforce const-correctness
anywhere (a deliberate, stated scope boundary -- no error for calling
a non-const method through a const reference, the way real C++ would
refuse to even compile it), so it happily generates
`Shape__area__void(s)` with `s` still `const Shape *` where the
callee's own injected `this` is a plain, non-const `Shape *`. gcc only
ever gave this a `-Wdiscarded-qualifiers` WARNING; the real Vircon32
compiler makes it a hard type ERROR instead.

Root cause, once traced: `cast_receiver_if_needed` (`lower.c`), the
function ALREADY responsible for inserting an explicit cast when a
receiver's own static class differs from the callee's declaring class
(a base/derived mismatch), only ever checked for THAT one reason to
cast -- `actual_class == expected_class` (both `Shape` here, since
`getArea` and `area()` are both dealing with the same class, no
base/derived relationship involved at all) short-circuited straight to
"no cast needed," never even looking at constness.

Fixed by giving `cast_receiver_if_needed` a second, independent reason
to cast: a new `receiver_type_is_const()` helper recognizes the exact
wrapper shape a const receiver's declared type has in this project's
own grammar (a bare `AST_CONST_TYPE`, or one level through
`AST_REFERENCE_TYPE`/`AST_POINTER_TYPE`) -- exactly the shape
`this_inject_method` already builds for a const method's own injected
`this` parameter (`PointerType(ConstType(ClassName))`), so the same
predicate answers both "is the object I'm passing const" and "is the
callee's own `this` const" with no new type-shape knowledge needed.
`finalize_call` now computes `needs_const_strip = object_is_const &&
!target_this_const` (a const object calling an ALSO-const method needs
no cast at all -- both sides already agree) and threads it into both
existing `cast_receiver_if_needed` call sites (virtual and non-virtual
dispatch). The function casts to the callee's OWN expected class
(stripping const in the process, since the cast target type is built
plain, un-const) whenever EITHER the original class-mismatch reason OR
this new const-mismatch reason applies -- `Shape__area__void((Shape
*)s)` in the concrete case, matching exactly what a caller writing an
explicit `const_cast` would produce in real C++. This is the honest
fix given this project's own already-chosen stance of not enforcing
const-correctness: making the permitted-but-unchecked case actually
COMPILE, not starting to reject code this project has never rejected
before.

### Bug 2: a ternary nested anywhere but the three "direct" statement shapes

`tests/sample71.cpp`'s `add(x > y ? x : y, 1)` -- a ternary used as a
CALL ARGUMENT -- was an already-DOCUMENTED scope boundary (phase 10's
own doc comment, `lower.c`, and `VIRCON32_QUIRKS.md` entry #10 both
stated plainly that only a ternary DIRECTLY initializing a var_decl,
DIRECTLY assigned to a bare identifier, or DIRECTLY a return
expression ever got rewritten -- anything nested any other way was
left completely untouched). The real Vircon32 C compiler doesn't
merely reject `?:` with a parse error the way a stricter grammar
might -- its own LEXER doesn't recognize `?` as a valid character at
all ("character '?' is not a valid identifier start"), confirming this
was never a style nicety to skip.

Fixed with a new, generic hoisting pass (`hoist_ternaries_in_expr`,
`lower.c`) that runs as a per-statement fallback inside the existing
`rewrite_ternary_block`, for any ternary NOT already reachable by the
three original direct, no-temp shapes: it walks the remaining
expression shapes those three cases don't cover (a call's own
arguments and callee expression, binary operators, subscripts, member
access, casts, sizeof, `new`'s constructor arguments and array-size
expression, and an assignment to anything other than a bare
identifier) and, for each `AST_TERNARY` it finds, hoists it into a
freshly-declared temporary (`__v32_tern_tmpN`, a per-method counter
matching the existing `__v32_ret_tmpN` naming convention) set via an
ordinary `if`/`else` spliced in immediately before the current
statement in its enclosing block -- e.g. `add(x > y ? x : y, 1);`
becomes `int __v32_tern_tmp0; if (x > y) { __v32_tern_tmp0 = x; } else
{ __v32_tern_tmp0 = y; } add(__v32_tern_tmp0, 1);`. Recursion is
post-order (a ternary's own condition/branches are hoisted first), so
a ternary buried inside ANOTHER ternary's own branch (reachable
through a call argument, not just the already-handled "chained"
`a?b:c?d:e` shape) unwinds correctly, innermost first.

This needed threading `AstNode *class_decl` and `LocalVarType
**locals` through the WHOLE phase 10 call chain
(`rewrite_ternary_free_functions`/`rewrite_ternary_classes` ->
`rewrite_ternary_in_method` -> `rewrite_ternary_stmt` ->
`rewrite_ternary_block`), which never needed either before (a
`build_ternary_if_else`-based rewrite never needed to know an
expression's TYPE) -- the generic hoist needs both, to look up the
temp's own type via `infer_expr_type` (given a ternary's then-branch,
falling back to the else-branch, then to `int` as a last resort) the
same way `finalize_calls_stmt` (phase 3/4) already does, seeded from
`seed_locals_from_params` and grown on every `var_decl` encountered so
far in the same block -- kept as its OWN independent locals list
rather than sharing phase 3/4's (matching the same "independently
reasoned about" principle already established for
`fix_references_in_method`'s own separate list). A small, related fix
was needed in `sema.c` too: `infer_expr_type` had no `AST_TERNARY`
case at all (fell through to "unknown"), so a ternary's own type could
never be inferred even when both branches WERE resolvable -- added,
same then-branch-first, else-branch-fallback logic.

Two boundaries remain, and are now stated explicitly in this phase's
own doc comment rather than left to be rediscovered the same way: a
ternary inside a for-loop's own init/cond/incr clauses (not a real
statement list to splice into; would need restructuring the loop
itself into an equivalent `while`, not attempted), and one reachable
only through a brace-less single-statement slot (`if (cond) foo(cond2
? a : b);` with no block around it) -- inserting a preceding temp
declaration needs a real list to insert into, the same structural
reason `var_decl`'s own direct rewrite was already documented as
unable to reach that shape.

### Verification

Full clean rebuild, zero warnings. `tests/sample68.cpp` (Vircon32
mode) now reads `Shape__area__void(((Shape *)s))`; `tests/sample71.cpp`
now reads with zero `?`/`:` characters anywhere in its own generated
output (grepped directly, not just spot-checked), including the new
`__v32_tern_tmp0` hoist for the call-argument case. A suite-wide grep
for `?` across every Vircon32-mode `out/*.c` file (all 74 samples)
came back with ZERO matches, confirming this isn't just the two
targeted samples. Full `make test`: no new failures -- every flagged
sample cross-referenced against the Makefile's own `-$(BIN)`
expected-fail entries, matching exactly (10 fewer manual checks than
last round, same discipline). Full `--target=standard` +
`gcc -fsyntax-only` sweep across all 74 samples: still exactly 34
pre-existing failures, re-diffed against the already-documented
`<stdbool.h>`/enum-union gaps -- `sample68` is among them for that
same already-known reason (confirmed by manually adding
`#include <stdbool.h>` and recompiling, which then succeeds cleanly),
not a new one. Both fixes additionally verified by actually RUNNING
the standard-mode output: `sample68` prints `sum=8 value=42
classValue=7` (all three matching the test's own documented expected
values) once `<stdbool.h>` is added; `sample71` prints `bigger=10
assigned=100 classified=1 viaCall=11` (the test file's own header
comment had `viaCall = 6`, stale from an earlier draft of the test
predating this fix -- corrected in the test file to the actual,
verified-correct value of 11: `x=5, y=10`, so the ternary picks `y`,
`add(10, 1) = 11`).

## Round: --target=standard's stdbool.h/enum-union gaps closed, a third bug they surfaced along the way, and a -vvv quirk-notes log

Continuation of the prior round's own real-Vircon32-compiler-driven
work, picking up the two gaps that round's own `--target=standard`
audit had flagged but not yet fixed (both explicitly called out as a
user-stated priority): `<stdbool.h>` never included in standard-mode
output despite every class unconditionally emitting `bool`/`true`/
`false`-using runtime helper boilerplate, and a bare `enum`/`union`
type reference never getting its keyword back in standard mode
(`print_type`'s `AST_IDENT` case only ever consulted the class
registry via `type_to_class`, which has no notion of enums or unions
at all -- sema.c keeps no registry for either, both are passed through
as literal, unmodified declarations).

### stdbool.h

First attempt gated the new `#include <stdbool.h>` inside the existing
`needs_misc`-gated block, alongside `<stdlib.h>` -- reasoning that the
`bool`-using boilerplate (`v32_new_arr_bool` and friends) is itself
gated on `needs_misc`. Caught before shipping by actually checking:
`tests/48sample.cpp`, `58sample.cpp`, and `59sample.cpp` all declare a
plain `bool` local with zero classes and zero `new`/`delete` anywhere
in the program, so `needs_misc` is false for exactly these -- gating
the include on it would have silently kept them broken. Fixed by
emitting `#include <stdbool.h>` unconditionally in `codegen_run`
whenever `g_target == TARGET_STANDARD`, independent of `needs_misc`
entirely; a small, always-safe standard header costs nothing to
include even on the rare program that turns out not to need it.

### enum/union keyword

New `find_enum_or_union_decl` helper added to codegen.c, deliberately
mirroring `program_has_any_class`'s own recursive top-level/namespace
scan shape rather than inventing a new traversal pattern -- the same
reasoning that helper was written with applies unchanged here.
`print_type`'s `AST_IDENT` case now falls through to this check
whenever the class-registry check misses, in standard mode only
(Vircon32 mode already gets its `enum`/`union` keyword from the
declaration's own auto-typedef, an entirely separate, Vircon32-only
mechanism unaffected by any of this). Verified directly against
`tests/60sample.cpp`/`61sample.cpp`.

### The third bug: virtual-destructor dispatch's own hand-rolled cast

A full `--target=standard` + real-`gcc` sweep across all 74 samples
(re-run after both fixes above, to actually confirm the previously-
documented pre-existing failure count dropped to zero rather than
assuming it from the diagnosis alone) turned up four remaining
failures. Three (`28sample`, `33sample`, `34sample`) are legitimately
out of scope -- all three `#include "video.h"`, Vircon32's own hardware
API, which has no standard-C counterpart and was never going to
compile there regardless of anything this round touches. The fourth
(`32sample`, virtual destructor dispatch through a base pointer) was a
real, previously-undiscovered bug: `emit_delete_runtime` (codegen.c)
builds the vtable-dispatched destructor call's own receiver cast by
hand, `fprintf(out, "(%s *)ptr", canonical_class->str1)`, instead of
going through `print_class_type_name` the way every OTHER class-typed
cast in this file already does -- so it silently produced a bare
`(Shape *)ptr` in standard mode instead of `(struct Shape *)ptr`,
undetected until now because nothing exercising THIS specific cast
path had ever been run through the standard-mode sweep before (the
enum/union fix, unrelated to destructors at all, is what got this test
compiling far enough to actually reach this line and expose it).
Fixed by routing that one `fprintf` through `print_class_type_name`
like everywhere else in the file.

### -vvv lowering-notes log

Separately requested: surfacing, in `-vvv` output, which lowering
rewrites happened specifically to route around a Vircon32 quirk (as
opposed to `-vv`'s existing inline-comment mechanism, which explains
ordinary C++-to-C mechanics in the GENERATED file itself -- vtables,
`this`, `new`/`delete` -- and was never about quirk-workarounds at
all). Added a small fixed-capacity notes log local to lower.c
(`lower_note(line, fmt, ...)`, a 512-entry cap, best-effort and
diagnostic-only -- nothing reads it back, so a dropped note past
capacity never changes generated output, only what gets reported about
it), reset at the top of every `lower_run()` call and printed by a new
`lower_notes_print()` from main.c right after the existing
`lower_dump()` call, same `-vvv` gate. Wired into the four sites that
actually perform a quirk-driven rewrite: `cast_receiver_if_needed`
(const-strip and base/derived receiver casts), `wrap_addr_of`
(function-pointer implicit `&`), `insert_pointer_cast_stmt` (the
base/derived VarDecl-initializer upcast), and all three ternary-
rewrite paths (`hoist_ternaries_in_expr`'s generic hoist, plus the two
direct `return`/assignment shapes in `rewrite_ternary_stmt`). Verified
against `tests/32sample.cpp`, `68sample.cpp`, `71sample.cpp`, and
`73sample.cpp` -- one sample per quirk category -- each producing the
expected, correctly-attributed note lines with no false positives or
misses.

### Verification

Full clean rebuild, zero warnings, for every change in this round.
Full `--target=standard` + `gcc -fsyntax-only` sweep across all 74
samples: zero failures once the three video.h/hardware-API samples are
excluded (never in scope for standard mode) -- down from the
previously-documented 34, then down further from the 4 remaining after
the two originally-scoped fixes, to 0. Full `make test` (Vircon32
mode): zero unexpected failures, the same 13 deliberately-`-`-prefixed
expected-fail entries as before, none of them new.
