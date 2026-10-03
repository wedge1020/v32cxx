# Spy vs Spy: Road Wars — v32c++ Coverage Cart

A vertical-scrolling 3-lane driving shooter written in **regular C** and fed
straight to `v32c++` (Vircon32 target). It exists for two reasons:

1. **Exercise claimed-supported C** end-to-end. Every construct marked
   `[COV]` in `spyvsspy.c` is one the transpiler documentation says works
   today. If the file transpiles cleanly *and* the generated C compiles under
   the Vircon32 C compiler, coverage holds. Anything that breaks is a found
   bug.
2. **Stress the Vircon32 rewrites.** The game deliberately leans on the three
   rewrite-sensitive constructs (ternaries, comma operators, function
   pointers) in exactly the positions the rewriter handles, and never in the
   positions it doesn't.

`gap_probes.cpp` is the companion harness for constructs known to be missing:
each is quarantined behind its own `-DGAP_xxx` so failures stay attributable
to a single feature.

---

## Field report

### Gap #1 — indented preprocessor directives (FIXED in lexer.l)

**First transpile attempt died with:**
`spyvsspy.c:54: error: syntax error, unexpected invalid token, expecting end of file`

**Root cause:** the `#define BUILD_LABEL` lines inside the `#ifdef
__V32CXX__` block were *indented* (4 spaces). The prescan correctly accepts
indented directives (C says a directive is the first non-whitespace `#`) and
re-emits the active branch verbatim — indentation included. But every `#`
rule in `lexer.l` was column-0 anchored (`^"#texture"…`, `^"#"[^\n]*`), so
the re-emitted line's `#` reached the single-character catch-all rule, which
returns raw char code 35 — no `#` token exists in the grammar, hence
**"unexpected invalid token"** (reported line drifted 1–2 lines).

**Fix:** lexer rules now allow leading `[ \t]*` before the `#` (cart-hint
rules still first). The game keeps the indented defines deliberately, as a
regression test. Same bug class affects indented `#undef` and indented
pass-through `#include` lines — worth a look while the change is fresh.

Note: `__V32CXX__` is predefined in the prescan (`macro.h`, `is_builtin`),
so the active branch is `"V32C++ REGRESSION CART"`, expanded inline at its
use site.

### Gap #2 — elaborated type specifiers (CONFIRMED, source worked around)

**Second transpile attempt died with:**
`spyvsspy.c:169: error: syntax error, unexpected STRUCT`

**Root cause:** `typedef struct Actor Actor;`. `typedef_decl` only accepts a
`type_spec` after TYPEDEF — and the STRUCT/UNION/ENUM keywords are not in
`type_spec`'s first set (only `int/float/void/bool/char`, a TYPE_NAME, a
qualified name, or `const`). The same holds for *any* declaration position:
`struct Actor x;` as a local/parameter/global also fails ("unexpected
STRUCT"), and `union ShadeWord u;` fails as "unexpected UNION". The tag
keywords only work at the *top-level declaration* position
(`top_decl: class_decl ';' | enum_decl ';' | union_decl ';'`).

**Workaround in this source:** the typedefs were deleted, and
`union ShadeWord u;` in `dim_color` became `ShadeWord u;`. Both are safe
because `class_decl`/`union_decl` register the tag in the symbol table
(SYM_CLASS/SYM_UNION) and the lexer hack classifies those names as
TYPE_NAME — the bare tag name works everywhere the elaborated form doesn't.
Probe: `gap_probes.cpp -DGAP_ELABORATED`.

**Transpiler-side fix — concrete parser.y placement:**

1. `type_spec` is the rule listing `INT_KW | FLOAT_KW | VOID_KW | BOOL_KW
   | CHAR_KW | TYPE_NAME | qualified_type | CONST type_spec`. Add three
   alternatives (put them next to the TYPE_NAME one):

   ```
   | class_or_struct_kw name_tok  { $ = ast_ident($2, @2.first_line); }
   | UNION name_tok               { $ = ast_ident($2, @2.first_line); }
   | ENUM name_tok                { $ = ast_ident($2, @2.first_line); }
   ```

   Use the existing `name_tok` (`IDENTIFIER | TYPE_NAME`), **not** bare
   IDENTIFIER: elaborated specifiers are most used when the tag is
   *already registered* (`struct Actor x;` after the struct is defined),
   and the lexer hack then hands the tag back as TYPE_NAME — a bare
   IDENTIFIER alternative would fail on exactly the common case. The
   action copies the TYPE_NAME alternative verbatim (an `ast_ident` from
   the tag), so sema/codegen see the same AST the bare-name spelling
   already produces — the known-good path.

2. That one change fixes `typedef struct X Y;` **for free**: typedef_decl's
   plain alternative is `TYPEDEF type_spec pointer_opt IDENTIFIER`, and
   the new type_spec alternative slots into position 2. The separate
   `TYPEDEF class_or_struct_kw ...` typedef_decl alternative is NOT
   needed — skip it.

3. Follow-on for the canonical same-name idiom `typedef struct Actor
   Actor;`: the ALIAS `Actor` is already SYM_CLASS, so the lexer returns
   TYPE_NAME there too and $4 (IDENTIFIER) rejects it — "unexpected
   TYPE_NAME". If the same-name idiom should parse, widen typedef_decl's
   plain alternative to `TYPEDEF type_spec pointer_opt name_tok`.
   Distinct-alias forms (`typedef struct Point2 Point2Alias;`) work with
   step 1 alone.

4. Re-run bison: `%expect 27` will hard-error until updated. STRUCT+CLASS
   (`class_or_struct_kw`), UNION and ENUM are **four new leading tokens**
   for type_spec, and this grammar's documented pattern is one new
   out_of_line_def-vs-func_def shift/reduce conflict per leading token
   per declaration-start context (the `const` bump was exactly +3 for one
   token). Also expect a new class_decl-vs-var_decl fork at `struct Tag`
   (lookahead `{`/`:` → class_decl; IDENTIFIER/`*`/`=`/`;` → declaration),
   decided by a single token of lookahead. Read every counterexample with
   `bison -Wcounterexamples` before accepting the new number — the file's
   own postmortem insists on that drill.

### Gap #3 — named parameters in function-pointer types (CONFIRMED, source worked around)

**Third transpile attempt died with:**
`spyvsspy.c:172: error: syntax error, unexpected IDENTIFIER, expecting ')'`

**Root cause:** `typedef void (*ActorFn)( Actor* a );` — the parameter
*name* inside a function-pointer type's parameter list (reported line
drifted ~25; the real line is the ActorFn typedef). `func_ptr_param_type`
is exactly `type_spec pointer_opt`, so after `Actor*` the only legal
tokens are `,` and `)`; `a` is neither. FrameFn's `()` was already fine.

The grammar's own comment calls this deliberate — "a function-pointer
TYPE's own parameter list carries bare TYPES only, matching real C++
exactly (`int (*)(int, float)`, never `int (*)(int x, float y)`)" — but
that rationale is mistaken: real C **and** C++ allow a parameter name in
any function declarator, function-pointer typedefs included; the name is
parsed and discarded (`typedef void (*Fn)(int x);` is valid, portable
C). So this is a true coverage gap, not a correctly-drawn scope boundary.

**Workaround in this source:** `typedef void (*ActorFn)( Actor* );` —
name dropped. Probe: `gap_probes.cpp -DGAP_FP_PARAM_NAME` (probes both
declarator spellings through the same `opt_func_ptr_param_list`).

**Transpiler-side fix — one alternative in `func_ptr_param_type`**
(the rule sitting just above `func_ptr_param_list`, at the end of the
function-pointer section):

```
func_ptr_param_type:
      type_spec pointer_opt                 { ...existing action... }
    | type_spec pointer_opt name_tok        { ...identical wrap action;
                                               the name is parsed and
                                               DISCARDED, exactly as real
                                               C's declarator grammar
                                               treats it... }
    ;
```

No new conflict risk: inside an fp parameter list the only tokens that can
follow a complete type are `,` and `)`, so an IDENTIFIER/TYPE_NAME there
can only be a discarded name — nothing else in the grammar competes for
it. Use `name_tok` rather than IDENTIFIER for the same lexer-hack reason
as in gap #2's fix (a parameter named after a registered type tokenizes
as TYPE_NAME). All three fp typedef alternatives and both fp var_decl
declarator spellings route through the same `opt_func_ptr_param_list`,
so this single change covers every spelling. And update the rule's
comment: "matching real C++ exactly" is the part that's wrong.

### Gap #4 — pointer arithmetic reaches Vircon32 C verbatim (CONFIRMED, source UNTOUCHED)

**First downstream compile (the v32c++ transpile itself now succeeds)
died with:**

```
obj/spyvsspy.c:414:31: error: invalid operands for addition
obj/spyvsspy.c:414:17: error: types are not compatible: cannot assign int to struct Actor*
obj/spyvsspy.c:431:50: error: invalid operands for subtraction
obj/spyvsspy.c:611:31: error: invalid operands for addition
obj/spyvsspy.c:611:17: error: types are not compatible: cannot assign int to struct Actor*
obj/spyvsspy.c:1129:31: error: invalid operands for addition
obj/spyvsspy.c:1129:17: error: types are not compatible: cannot assign int to struct Actor*
```

**Root cause:** valid C that v32c++ parses and emits verbatim, but
Vircon32 C rejects. Its pointer support accepts `++p`, `p != q` and
`p->m`, but NOT `p + i` / `i + p` ("invalid operands for addition") and
NOT `p - q` ("invalid operands for subtraction"). The cart's four sites:
`Actor* end = g_actors + MAX_ACTORS;` in spawn_actor, update_bullet and
play_frame (each `+` yields two errors — the invalid addition, then its
int result assigned to `Actor*`), and `(int)( it - g_actors )` in
spawn_actor. This is the SILENT-LEAK class: transpile succeeds, the
generated C looks plausible, only the downstream compiler catches it.
No entry in docs/VIRCON32_QUIRKS.md yet — this needs a new one
("Vircon32 C has no pointer arithmetic; ++/-- and comparisons only").

**Source stays as-is** — per the coverage-cart policy, the pointer
iteration is the point. Probe: `gap_probes.cpp -DGAP_PTR_ARITH`.

**Transpiler-side fix:** a Vircon32-mode lowering pass in lower.c (same
"genuine AST-level rewrite, not a printing choice" reasoning as the
ternary phase), gated `g_target == TARGET_VIRCON32`:

- `p + i` / `i + p` / `p - i` (T* p) → `&p[ i ]` / `&p[ -i ]` — the C
  identity `&p[i] ≡ p + i` — IF Vircon32 C accepts subscripting a
  POINTER lvalue (the cart only proves it for arrays);
- `p - q` → `((int)p - (int)q) / (int)sizeof(T)` — IF pointer↔int casts
  and runtime `sizeof` work (Vircon32 C spelling: bare tag, `sizeof(S)`);
- both primitives need one two-minute hand probe against `compile`
  before choosing the shape:

```c
/* ptr_probe.c -- hand-feed to the Vircon32 C compiler */
struct S { int x; int y; };
struct S g_pool[ 8 ];

void main( void )
{
    struct S* p = g_pool;
    struct S* q;
    int d = 2;

    q = &p[ 2 ];                          /* A: subscript on pointer */
    q = &p[ d ];                          /* B: same, variable index */
    d = ( (int)q - (int)g_pool ) / (int)sizeof( S );   /* C: casts+sizeof */
    d = q - g_pool;                       /* D: ptr - ptr (expect reject) */
    q = p + 2;                            /* E: ptr + int (expect reject) */
}
```

- same family to cover in the same pass: `p += i` / `p -= i` (nobody
  used them in the cart yet, but they will leak identically);
- if NEITHER primitive exists, the fallback is index-rewriting (keep an
  array base + integer index instead of pointer locals) — much more
  invasive; decide only after the probe results.

### Gap #5 — ternary temps vs Vircon32 C's const strictness (CONFIRMED, source UNTOUCHED)

**Same downstream compile died with:**

```
obj/spyvsspy.c:238:31: error: cannot assign const int to int: discards const qualifier
obj/spyvsspy.c:244:35: error: cannot assign const int to int: discards const qualifier
obj/spyvsspy.c:248:35: error: cannot assign const int to int: discards const qualifier
```

**Root cause:** `clampi` — `inline int clampi( const int v, const int lo,
const int hi )` returns a ternary CHAIN. The direct-return position
avoids a temp, but the NESTED ternary in the else branch forces the
hoist path: the lowering declares `int __v32_tern_tmpN;` and assigns
`lo` / `hi` / `v` (const-qualified parameters) into it. Real C allows
by-value scalar copies from const lvalues; Vircon32 C is stricter and
rejects the const discard. Only clampi is hit — every other ternary in
the cart reads plain params (max2 etc.) and passes. Another new
quirks-doc entry: "Vircon32 C rejects `int t = c;` from a const scalar
(`discards const qualifier`), which real C permits".

**Source stays as-is.** Probe: `gap_probes.cpp -DGAP_CONST_TERNARY`.

**Transpiler-side fix:** in lower.c's ternary machinery, when a
synthesized assignment's RHS is a const-qualified scalar lvalue (a
local/param whose type is a bare `AST_CONST_TYPE`), wrap it in an
explicit cast to its own unqualified type — the rewrite emits
`tmp = (int)lo;`. A cast yields an rvalue, discarding const by design;
codegen-wise a no-op. Narrower alternative considered and rejected:
stripping const from emitted parameter types in Vircon32 mode would
change every generated signature and still leave user-written
`int x = lo;` to die downstream. The cast fix is local to synthesized
code; it can generalize into a full "const-strictness" pass over user
assignments later if wanted, but the ternary temps alone unblock the
cart.

### Not a gap — the fn_check declarator was invalid C (FIXED in source)

```
obj/spyvsspy.c:1265:21: error: cannot assign void(struct Actor*)* to const-qualified void(int)*
obj/spyvsspy.c:1266:18: error: cannot assign struct Actor* to const-qualified int
```

The downstream compiler being RIGHT: `void ( int )* fn_check;` cannot
hold `update_traffic` (a `void(Actor*)` function), and the call then
passed `Actor*` where `int` was declared. gcc rejects the same code —
this was a source-level type error, not a transpiler limitation, so
fixing it is not a workaround. The declarator now reads
`void ( Actor* )* fn_check;` — same Vircon32-native spelling, now
type-correct (and it exercises `func_ptr_param_type` with TYPE_NAME +
pointer, which the old `( int )` form never did). Optional transpiler
nicety, not a gap: v32c++'s own sema could flag incompatible
function-pointer assignments earlier than the downstream compiler does.

### Revert queue (apply when the parser fixes for gaps #2/#3 land)

Per the new policy — the cart demonstrates the transpiler, it doesn't
concede to it — these workaround removals go back to idiomatic C as soon
as the parser-side fixes are in:

1. Restore `typedef struct Actor Actor;` and
   `typedef struct GameState GameState;` (replacing the gap-#2 comment
   block before the ActorFn typedefs).
2. `dim_color`: `ShadeWord u;` → `union ShadeWord u;`.
3. ActorFn: restore the named parameter —
   `typedef void (*ActorFn)( Actor* a );`.

---

## Files

| File | Purpose |
|---|---|
| `spyvsspy.c` | The game. Plain C (the transpiler's C-subset-of-C++). Must transpile cleanly as-is. |
| `gap_probes.cpp` | Known-gap harness. Must transpile cleanly with **no** `-D` flags; one `-DGAP_xxx` at a time otherwise. |
| `textures/spyvsspy_sprites.png` | The single texture atlas (256×256 RGBA). Must sit at this exact path — the cart XML references `textures/spyvsspy_sprites.vtex`, swapped from the `#texture` hint's path at XML-emission time. |
| `Makefile` | Full pipeline: transpile → compile → (opt) → assemble → png2vircon → packrom. |

---

## Build

With the project Makefile (transpiler → Vircon32 C → optional `v32opt` →
assemble → texture → pack):

```sh
make all            # full cart: bin/spyvsspy.v32 (+ Opt variants if v32opt is on PATH)
make debug          # same with -g flags throughout
make clean
```

The Makefile's key rule:

```sh
v32c++ -I ../../../c_api -I inc -o obj/spyvsspy.c spyvsspy.c
```

- The generated XML lands beside the output (`obj/spyvsspy.xml`), then moves
  to the project root for `packrom`.
- The XML's texture entry is `textures/spyvsspy_sprites.vtex`; the `vtex`
  target builds it from `textures/spyvsspy_sprites.png` with `png2vircon` —
  put the sprite sheet PNG there.

### Gap probes

```sh
v32c++ -c gap_probes.cpp                    # harness with nothing enabled: must succeed

v32c++ -c -DGAP_STATIC         gap_probes.cpp
v32c++ -c -DGAP_EXTERN         gap_probes.cpp
v32c++ -c -DGAP_VOLATILE       gap_probes.cpp
v32c++ -c -DGAP_UNSIGNED       gap_probes.cpp
v32c++ -c -DGAP_BITFIELD       gap_probes.cpp
v32c++ -c -DGAP_CONST_PTR      gap_probes.cpp
v32c++ -c -DGAP_VOID_PARAM     gap_probes.cpp
v32c++ -c -DGAP_ARRAY_PARAM    gap_probes.cpp
v32c++ -c -DGAP_TERNARY_ARG    gap_probes.cpp
v32c++ -c -DGAP_TERNARY_MEMBER gap_probes.cpp
v32c++ -c -DGAP_COMMA_ARG      gap_probes.cpp
v32c++ -c -DGAP_COMMA_RETURN   gap_probes.cpp
v32c++ -c -DGAP_MULTIDECL_ARRAY gap_probes.cpp
v32c++ -c -DGAP_STRUCT_INIT    gap_probes.cpp
v32c++ -c -DGAP_STRUCT_ASSIGN  gap_probes.cpp
v32c++ -c -DGAP_STRUCT_RETURN  gap_probes.cpp
v32c++ -c -DGAP_CHAR           gap_probes.cpp
v32c++ -c -DGAP_UNSAVED_ARRAY  gap_probes.cpp
v32c++ -c -DGAP_NESTED_INIT    gap_probes.cpp
v32c++ -c -DGAP_BITNOT         gap_probes.cpp
v32c++ -c -DGAP_MODASSIGN      gap_probes.cpp
v32c++ -c -DGAP_NULL_INIT      gap_probes.cpp
v32c++ -c -DGAP_ELABORATED     gap_probes.cpp
```

For every probe, record one of: **hard error** (gap confirmed — good),
**clean output, construct leaked verbatim** (bad — Vircon32 C will reject it
downstream; a silent-leak bug worth filing), or **clean output, correctly
rewritten** (gap closed — promote the construct into `spyvsspy.c` as `[COV]`
and update the docs). The expected-outcome table for all 23 probes is
embedded at the bottom of `gap_probes.cpp`.

---

## Controls

| Input | Action |
|---|---|
| D-pad left / right | Steer; on the title screen: pick Black Spy or White Spy |
| D-pad up / down | Accelerate / brake (0–5 speed steps) |
| Button A | Shoot (rate-limited) |
| START | Start race; after game over: restart |

Rival spies always drive the car you did **not** pick — a nod to the source
material.

## Gameplay notes

- **Oil slick**: steering becomes a slide, skid marks spawn, timer expires.
- **Spike strip**: flat tires — half pace and mushy steering for a while.
- **Barrel / crate**: solid — a crash.
- **Fuel can**: +35 fuel. Fuel drains continuously; running dry is a crash.
- **Rival spies** home in on your lane; shooting one is worth 250, traffic
  40, obstacles 25. Three lives, then GAME OVER.

---

## Coverage matrix — `[COV]` markers in `spyvsspy.c`

| Feature | Where in `spyvsspy.c` |
|---|---|
| Bare structs — tag names become TYPE_NAME via the lexer hack (no `typedef struct` needed, and `typedef struct X Y;` is Gap #2) | `Actor`, `GameState` used as bare names everywhere |
| Enum, explicit **and** auto values | `enum ActorKind`, `enum GamePhase` |
| Union with array member | `union ShadeWord` (in `dim_color`) |
| Function-pointer typedefs, standard spelling | `ActorFn`, `FrameFn` |
| Function-pointer local, **Vircon32 spelling** `void (int)*` | `fn_check` in `boot_game` |
| Arrays, standard spelling | `g_pattern_bits`, `digits[12]`, `buf[16]`, `msg_white[16]`, … |
| Arrays, **Vircon32 spelling** `int[N] name` | `g_pattern_speed` |
| Braced init lists | `g_pattern_bits`, `g_pattern_speed` |
| String literal as int-array init (global) | `g_msg_over[16] = "GAME OVER"` |
| String literal as int-array init (local) | `msg_white`, `msg_black`, `msg_retry` |
| Adjacent string-literal concatenation | `"A: SHOOT   " "PAD: STEER"` in `title_frame` |
| Switch: fall-through stacks, expr switch, default | `update_bullet`, `update_effect`, `collide_player`, `draw_road` |
| `do`/`while` | `format_int`, `main`'s START-release wait |
| `goto` + label (flat scope) | `restart_round` in `main` |
| `sizeof`: type, expression, no-paren `sizeof *a` | `boot_game`, `kill_actor`, `start_round` |
| Bitwise ops + compound (`&=`, `|=`) | actor flags throughout |
| `&` precedence trap (explicit parens) | `( a->flags & F_OIL ) != 0` |
| C-style casts, int→float | `draw_sprite_scaled`, `update_player` |
| Hex / octal / binary literals | regions, `0273` seed, `g_pattern_bits` `0b0101…` |
| Literal suffixes (`f`) | `1.0f`, `2.0f`, `0.5f`, `0.0f` |
| `inline`, `register` (ignored) | `clampi`, `draw_road` |
| `const` prefix (params) | `clampi`, `aabb_hit` |
| Multiple plain declarators | `int i, j;` in `draw_actors`, `boot_game` |
| Comma operator in for clauses / expr statements | `draw_actors`, `boot_game` |
| Pointer iteration, `++`/`!=`/difference | `spawn_actor`, `update_bullet`, `play_frame` |
| `0`-vs-pointer compare (auto-NULL rewrite) | `g_player != 0` |
| `nullptr` | `gap_probes.cpp` GAP_NULL_INIT |
| `bool` builtin | `Actor.alive`, `aabb_hit` |
| Globals uninitialized + initialized | `g_player`, `g_pattern_bits` |
| Prototype/definition dedupe | prototype block at top |
| Function-pointer tables + indexed calls | `g_kind_update`, `g_phase_frame` |
| `#texture`/`#title`/`#version` cart hints | top of file |
| `#ifdef`/`#else`/`#endif` against predefined `__V32CXX__` | `BUILD_LABEL` block (now column-0 — see field report) |
| Function-like macros with parenthesized bodies | `LANE_X` |
| `int main()` → forced `void main(void)` | `main` |
| Unary minus, negative literals | bullet `vy` |
| `continue`, early `return` | `collide_player`, effect updates |
| SDK pass-through includes | `video.h`, `input.h`, `time.h`, `misc.h` |

### Rewrite-stress spots to watch on the next transpile

1. **`int[ PATTERN_COUNT ]` Vircon32 declarator spelling** for an initialized
   global (`g_pattern_speed`).
2. **`void ( int )* fn_check;`** — the native function-pointer declarator as
   a *local*, then called through. Pure coverage demo, droppable.
3. **Ternary producing pointer values** (`msg_spy = … ? msg_white :
   msg_black`) — the rewrite must pick a pointer-typed temporary.
4. **String-literal array initializers on locals** (`msg_white`, `msg_retry`)
   — same feature as the global `g_msg_over`, but via the local-decl path.

String literals as **call arguments** (`print_at(12, 0, "SCORE")`) are
confirmed safe: `print_expr`'s `AST_STRING_LIT` case re-quotes them and
Vircon32 mode rewrites escapes via `print_vircon32_string` (found and fixed
via an earlier Space Invaders port, per the codegen notes).

## Expected-gap table

See the table embedded at the bottom of `gap_probes.cpp` — all 22 probes
with their expected outcome class (hard error vs leak risk).

## BIOS texture usage

The HUD and title banner are drawn from the BIOS font texture
(`select_texture(-1)`): region **20** = solid block, **19** = darker,
**18** = mid, **17** = lightest. Score/speed/fuel gauges and the title
banner are built entirely from these four cells, tinted via
`set_multiply_color` (and `dim_color` for shading) — no texture budget
spent on UI.