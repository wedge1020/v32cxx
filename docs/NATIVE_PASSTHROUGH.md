# Design: `native Name;` — opaque pass-through type declarations

> **Status (20260925-dev): implemented.** Grammar, AST, registry, and
> every sema rejection below have landed, with `tests/90sample.cpp`
> (legal uses) and `tests/91sample.cpp` (one error per rejected shape).
> Where the implementation departed from the plan below, the plan has
> been left as written and the difference is recorded in
> [As built](#as-built-20260925-dev) at the end of this file.

## Problem

`#include "X.h"` lines are pass-through text: v32c++re-emits them and never++  
++parses them. Consequence: every type defined in a C header (date\_info,++  
++time\_info, game\_signature) and every function taking/returning it is++  
++\*uncallable from v32c++ source\*, because the type name can't be lexed as  
TYPE\_NAME — it isn't in the symbol table, so `date_info* out;` doesn't parse  
as a declaration. time.hpp worked around this by re-expressing pure-math  
conversions in classes; **memcard.h cannot be worked around the same way**  
(`card_signature_matches` is hardware access — `cmps R0` — not math).  
This was pre-decided last round; this is the spec.

## Shape

```
native_decl:  NATIVE IDENTIFIER ';'   (and, later, qualified: NATIVE IDENTIFIER '{' ... — NOT now)
```

```cpp
native date_info;     // name becomes usable as a type in v32c++ source
native game_signature;

void test()
{
    date_info d;                  // NOT allowed (no layout known)
    date_info* p = 0;             // allowed: pointer to it
    translate_date( get_date(), p );   // the real C function, called
}
```

Semantics, precisely:

- `native X;` inserts `X` into the symbol table as **SYM\_TYPEDEF** (the exact  
kind `typedef_decl` inserts), at the current scope, so the lexer's  
TYPE\_NAME-vs-IDENTIFIER classification picks it up with zero new machinery.
- **Codegen emits NOTHING** for the declaration itself. The name resolves  
downstream against the real C header, exactly like a pass-through call.
- New AST node `AST_NATIVE_DECL` (str1 = name). It exists in the tree so the  
dump prints it and so a future `native` with a payload has a home; codegen's  
program walk skips it.

## The pointer-only rule (sema's one real job)

The transpiler knows NOTHING about a native type's layout, so by-value use is  
always a lie:

- **Allowed:** `X*`, `X&`, `X*` params/returns/locals/members, `X*` cast  
targets (once qualified casts parse).
- **Rejected, loudly:** by-value locals, params, returns, member fields, and  
`sizeof(X)`. Each is a sema error at the declaration site, e.g.  
`semantic error: native type 'date_info' may only be used by pointer or reference (its layout is defined in a C header this transpiler does not parse)`.

This is deliberately stricter than C, and that's correct: a by-value native  
would silently produce wrong-size stack slots.

## Implementation plan (four files, one small round)

1. **lexer.l** — add `NATIVE` keyword token (same pattern as STATIC/  
 TYPEDEF: `g_last_ident_sym = NULL; return NATIVE;`). Reserve the word.
2. **parser.y** —
  - `%token NATIVE` in the token block;
  - new top-level rule:
    ```
    native_decl: NATIVE IDENTIFIER ';'
        {
            symtab_insert(g_symtab, g_symtab->current, $2, SYM_TYPEDEF);
            $$ = ast_new(AST_NATIVE_DECL, @1.first_line);
            $$->str1 = strdup($2);
        }
    ```
    added to the top-level alternation (same position as typedef\_decl).
    GLR caution: this is an additive, unambiguous rule — but regenerate and
    check \`%expect\` anyway; every grammar change gets that.
3. **ast.h/ast.c** — `AST_NATIVE_DECL` kind + dump case (one line:  
 `NativeDecl str1=%s`).
4. **sema.c** —
  - `collect_declarations` should record native names in the SAME flat  
   typedef registry typedefs use, marked opaque (check how that registry  
   stores entries; a new `is_native` flag on the entry, or a parallel  
   set — match the registry's existing shape).
  - The pointer-only check: wherever a var\_decl/param/return type is  
  resolved, if the base type resolves to a native-registry name AND the  
  type isn't wrapped in pointer/reference, emit the loud error. Find the  
  central type-resolution point rather than checking at each site —  
  the enum-registry round's `infer_expr_type` lesson applies: one  
  chokepoint beats scattered lookups.
  - **Codegen emits nothing**, but `infer_expr_type` should treat  
  `native X*` like an unknown-but-valid pointer type, not an error.

## The one wrinkle to check before implementing

`typedef_decl`'s registry entry records the typedef's UNDERLYING type.  
A native has none. Check what consumers do with a NULL underlying type —  
if `print_type` or sema dereference it, the native entry needs a synthetic  
underlying (e.g. itself, or a never-emitted placeholder). This is the only  
place the "just reuse SYM\_TYPEDEF" shortcut might need a real decision;  
inspect `semac/typereg` (the flat typedef registry) consumers first.

## Also decides (record now, implement now): memcard pilot can follow

Once `native` lands, v32/memcard.hpp becomes writable:

```cpp
native game_signature;

namespace v32
{
    class CardScope { ... }   // save/restore pattern TBD
    bool signature_ok( game_signature* sig ) { return card_signature_matches( sig ); }
}
```

But the memcard pilot is a SEPARATE round — do not couple it to this one.

## Test plan: tests/90sample.cpp

- `native date_info;` + `native time_info;` at top level, then call the REAL  
`translate_date` / `translate_time` (the first time the C originals are  
ever called from v32c++ source) and print the fields — direct A/B against  
`v32::Date::set_from` on the same `get_date()` value; both must print the  
same Y-M-D. This also regression-proves the class conversion against the  
C API's own in the same run.
- Pointer use in all four positions: local, param, return, member.
- The loud rejections, one per line (by-value local, by-value param,  
by-value return, by-value field, sizeof) — sema must name the type in  
each error.
- A `--target=standard` gcc compile of the generated C (this is the one  
round where desktop gcc CAN verify end to end: date\_info/time\_info are  
ordinary C structs and translate\_date/translate\_time are pure C).
- Deliberately NOT tested: `native` of a name that doesn't exist downstream  
(that's the same class as typo'd pass-through calls — uncheckable until  
the v32pp sniffing fallback someday exists).

## Out of scope, stated plainly

- `native` with enum/union tags (`find_enum_or_union_decl` has its own  
emission path — only touch it if a pilot actually needs a native enum).
- Qualified native (`native v32::foo;`) — no consumer.
- v32pp struct/#define sniffing — the fallback for the OTHER half of the  
pass-through gap (#define visibility), separate round.

## As built (20260925-dev)

What actually shipped, and where it differs from the plan above.

**Grammar.** `native_decl` does **not** consume the `;` itself — the
top-level `native_decl ';'` alternative does, exactly like
`typedef_decl`. The first draft had the `;` in both places, so a plain
`native X;` was a syntax error at whatever came next and only
`native X;;` parsed. A second alternative, `NATIVE TYPE_NAME`, makes a
repeated declaration idempotent: once `X` is in the symbol table the
lexer returns `TYPE_NAME`, and several `v32/` headers may each declare
the same native. No `%expect` change was needed (still 27). Because
`native_decl` is a `top_decl`, a native can also be declared inside a
namespace body; a qualified use (`v32::game_signature*`) prints as the
bare C name, which is what the downstream header defines.

**Registry.** `TypedefRegEntry` gained an `is_native` flag, and
`typedef_registry_is_native(name)` (declared in `sema.h`) is the one
lookup every check uses. The synthetic self-named underlying type
(`n->type = ast_ident(name)`) is kept so nothing reading the node sees
NULL, **but `find_typedef_target` never returns it for a native.**
Returning it made `resolve_typedef_chain` loop X → X → X until its
64-step guard stopped it. Now the chain stops at the native name on the
first step. The "wrinkle" section's concern is settled that way.

**Conflicts.** `native X;` where `X` is already a class, struct, or
ordinary typedef is a semantic error (`'native X;' conflicts with an
earlier declaration...`). Silently marking a known-layout type as native
would retroactively forbid every by-value use of it.

**Where the pointer-only rule is checked.** Everything is in sema.c,
through `check_native_pointer_only` (which peels `const` first, so
`const date_info d` is rejected too):

| Position | Where |
| --- | --- |
| class/struct member field | `compute_layout` member walk |
| local variable | `check_node`, `AST_VAR_DECL` |
| global variable | `check_globals` |
| return type + every parameter, prototypes and definitions, free functions and methods | `check_native_signature`, called once per function from `check_function_body` / `access_check_free_functions` |
| `sizeof(X)` | `check_node`, `AST_SIZEOF` (`sizeof(X*)` passes) |
| `p->field` / `r.field` through a native | `check_node`, `AST_MEMBER` (via `native_base_of`) |

An earlier draft also called `check_native_pointer_only` from lower.c's
word-size check. That was a link error, because the function is static
to sema.c, and it would have reported every by-value parameter twice.
That call is gone. lower.c's multi-word warning is silent for natives
anyway: a native has no `StructLayout`.

**The test plan's A/B step can't be written yet.** The plan above calls
`translate_date` on storage v32c++ source owns and then prints the
fields. Both halves are now rejected by design: a by-value
`date_info d;` has no layout to allocate, and `p->year` is member access
through a native. `translate_date(get_date(), p)` needs a `p` pointing
at real storage, and v32c++ source can't make that storage. Today the
pointer has to come from C, for example a C-side global or a C function
returning `date_info*`. Two ways forward, both future work:

1. **Sized natives:** `native date_info[3];` (or similar) gives the word
   count, which permits by-value locals and `sizeof` while member
   access stays forbidden.
2. **Layout-bearing natives:** `native date_info { int year; int month;
   int day; };` declares the fields and emits nothing. This is what the
   "future `native` with a payload" note in the AST section was
   reserving room for.

The memcard pilot doesn't have this problem, because
`card_signature_matches(game_signature*)` only needs a pointer, and a
pointer to a `game_signature` defined on the C side is enough.

**Reference parameters to a native** (`date_info&`) are accepted and
lower to pointers like any other reference. `touch(d)` forwarding and
`touch(*p)` both come out correct. `&d` on a reference parameter does
**not**: it emits `(&d)`, a `date_info **`. That is not native-specific.
It's the general reference-lowering gap recorded in DESIGN_NOTES.md
("Round: `native` opaque types"), so sample90 doesn't use that spelling.


## The memcard pilot (landed)

`v32/memcard.hpp` is the first real consumer. It declares
`native game_signature;` at file scope and avoids the storage problem
above without any new language feature. A `game_signature` *is* a
`typedef int[20]`, so the veneer's API takes a plain `int*` to 20 words,
which the caller can declare (`int sig[20] = "MYGAME";`), and casts it
at the C boundary:

```cpp
bool card_matches( int* signature )
{
    return card_signature_matches( (game_signature*)signature );
}
```

Verified end to end: the real Vircon32 compiler accepts the cast, and
`tests/94sample.cpp` runs on the emulated console with a memory card
inserted. The card file afterwards holds the signature at word 0. The
sized/layout-bearing native options above are still the general answer
for C structs whose fields game code needs to read (`date_info`).
