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

## Method/function body emission

`codegen.c` now emits actual C statement/expression text for every
method and free function that has a body -- `print_stmt`/`print_expr`
(every fully-lowered statement and expression kind, since lowering has
already reduced the AST to something C-shaped) plus a shared
`emit_function_header` for prototypes and definitions alike. Full
reasoning, including two real risks found by tracing through by hand
rather than assumed away, lives in `codegen.h`'s own doc comment (kept
there rather than duplicated here, since that's where anyone touching
this code will actually look first) -- summarized:

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
- **A real, unresolved risk**: a virtual call's `this` argument is
  passed exactly as this-injection typed it (the calling class's own
  receiver type), with no cast, even when the vtable field being called
  through was declared using an ancestor's receiver type -- which it
  always is, for an inherited-but-overridden slot. Concretely,
  `Circle::describeTwice` (`tests/sample14.cpp`) passes a `Circle *`
  where the vtable field's declared type is `Shape *`, no cast inserted
  anywhere. Given how strict Vircon32 has already shown itself to be,
  this needs a real compile to confirm whether it's tolerated or not --
  not fixed here, deliberately, rather than guess at both whether it's a
  problem and what the right cast syntax is.
- **A separate, adjacent gap**: this project has no special handling for
  a user-defined `main` at all -- it gets mangled like anything else
  (`main__void`), so generated C has no actual `main` entry point, and
  nothing enforces Vircon32's `void main()` requirement on the C++
  source either. Needs an actual design decision, not a quick fix.

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

## Future CLI/design considerations (not urgent, filed for later)

Two things Matthew flagged explicitly as "eventually, not now" while
reviewing the first codegen round -- recorded here so they're available
when the time comes, not lost in conversation history.

**A "standard C mode" command-line flag.** As codegen leans further into
Vircon32-specific quirks (no `struct` keyword on a type reference, and
eventually the reversed array-declarator order once array-type support
exists), it's worth being able to switch that behavior off and emit
plain, portable C instead -- for testing against a normal C compiler, or
for anyone who wants to retarget this project's output elsewhere. This
would need, at minimum: (a) emitting `struct Name` (with the keyword) on
every type reference, not just the definition, since standard C doesn't
auto-typedef; (b) once array-type support exists, emitting the ordinary
`T name[N];` declarator order instead of Vircon32's `T [N] name;`. Not
urgent -- there's exactly one target today -- but worth keeping
`print_type()` (codegen.c) and whatever eventually handles array
declarators structured so a mode flag can cleanly select between the two
forms, rather than the Vircon32-specific choices being hardwired in a way
that's painful to unwind later.

**Keep every non-code dump section, permanently -- eventually behind a
flag, not by default removing any of it.** `v32c++`'s current behavior
(AST dump, semantic-analysis summary, struct layouts, fully-lowered
method bodies, generated C -- all unconditional, every run) is explicitly
valuable as-is for the course this project is teaching material for:
seeing every stage of the pipeline is the point, not a debug-mode
side effect to be trimmed away once codegen matures. The only thing
worth adding later is a CLI flag to select a QUIET mode (generated C
only, for anyone who just wants to build something) -- the full,
verbose, every-stage-visible output should remain the default, or at
minimum trivially available, indefinitely. Whatever flag-parsing
eventually gets added to `main.c` (there isn't any yet -- `argv[1]` is
just the filename) should keep this distinction in mind rather than
accidentally making the teaching-relevant output an opt-in afterthought.

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
