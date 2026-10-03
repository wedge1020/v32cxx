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

### Gap #2 — elaborated type specifiers (FIXED in parser.y)

**Second transpile attempt died with:**
`spyvsspy.c:169: error: syntax error, unexpected STRUCT`

**Root cause:** `typedef struct Actor Actor;` — `struct`/`union`/`enum`
could not start a type anywhere but a top-level definition.

**Fix:** `type_spec` takes `struct X` / `class X` / `union X` / `enum X`,
and the tag-typedef idioms are desugared in the parser: `typedef struct
Actor Actor;` (before or after the definition) emits nothing, `struct
Actor;` forward declarations are accepted, and `typedef struct [Tag] {
... } Name;` works for struct, union and enum. `%expect` went 27 → 39;
the twelve new conflicts are the existing `opt_virtual` family reached
through four more leading tokens (see the comment at `%expect`).
The cart has its typedefs and `union ShadeWord u;` back.
Regression test: `tests/99sample.cpp`.

### Gap #3 — named parameters in function-pointer types (FIXED in parser.y)

**Third transpile attempt died with:**
`spyvsspy.c:172: error: syntax error, unexpected IDENTIFIER, expecting ')'`

**Root cause:** `typedef void (*ActorFn)( Actor* a );` —
`func_ptr_param_type` took bare types only. C and C++ allow (and
discard) a name in any function declarator.

**Fix:** `func_ptr_param_type: type_spec pointer_opt name_tok`. The cart
has the named parameter back.

### Gap #4 — arrays as operands (FIXED in lower.c, phase 11)

**First downstream compile died with:**

```
obj/spyvsspy.c:414:31: error: invalid operands for addition
obj/spyvsspy.c:414:17: error: types are not compatible: cannot assign int to struct Actor*
obj/spyvsspy.c:431:50: error: invalid operands for subtraction
```

**Root cause — not what it first looked like.** This was filed as
"Vircon32 C has no pointer arithmetic". Probing the real compiler one
construct at a time says otherwise: `p + 2`, `2 + p`, `p - 1`, `p += 2`,
`q - p` and `&p[ n ]` all compile. What it refuses is an ARRAY as an
operand: an array only decays to a pointer in a plain assignment,
initializer or argument, so `g_actors + MAX_ACTORS`, `it - g_actors`,
`p == g_actors`, `(int)g_actors` and `*g_actors` are all errors.

**Fix:** in Vircon32 mode an array-typed operand of `+ - == != < > <=
>=`, of a cast, or of unary `*` is emitted as `&arr[0]`:
`Actor * end = ((&g_actors[0]) + MAX_ACTORS);`. `sizeof arr` is left
alone. docs/VIRCON32_QUIRKS.md #22; `tests/100sample.cpp`.

### Gap #5 — const strictness (FIXED in lower.c, phase 11)

**Same downstream compile died with:**

```
obj/spyvsspy.c:238:31: error: cannot assign const int to int: discards const qualifier
```

**Root cause — wider than the ternary temps.** Vircon32 C rejects a
const value in ANY plain assignment (`n = c;`), call argument
(`f( c )`), `return c;`, and under unary minus (`n = -c;`), and the same
for anything read through a pointer-to-const (`n = cp->x;`). Only
initializers, compound assignments and binary expressions are exempt.
`clampi` hit it through the ternary temps; user-written `n = lo;` would
have died the same way.

**Fix:** at those three read positions a const scalar or pointer gets a
cast to its own unqualified type (`__v32_ret_tmp0 = ((int)lo);`); a
const struct is read as `*((S *)&expr)`. The phase runs after ternary
lowering, so synthesized temporaries are covered. Real qualifier
discards on pointers are still left for the compiler to reject.
docs/VIRCON32_QUIRKS.md #23; `tests/100sample.cpp`.

### Gaps closed from `gap_probes.cpp` (20261003)

Every probe now transpiles and compiles under the real Vircon32 C
compiler. Details in docs/VIRCON32_QUIRKS.md
#24; all of these are exercised at run time by `tests/100sample.cpp`.

| Probe | Outcome |
|---|---|
| STATIC | file-scope `static` dropped; static LOCALS hoisted to uniquely named globals |
| EXTERN | dropped; `extern int x;` + `int x = 5;` merge into one definition |
| VOLATILE | dropped (still a token in `asm volatile`) |
| UNSIGNED | `signed`/`short`/`long`/`unsigned` (+`int`/`char`) are `int`; `unsigned` warns once |
| CONST_PTR | `T* const p` emitted as `T* p` |
| COMMA_RETURN | `return a += 1, a + b;` parses; lowered like any comma operator |
| MULTIDECL_ARRAY | `int a, b[ 4 ];` and `int b[ 4 ], a;` |
| STRUCT_INIT / NESTED_INIT | braced initializers nest to any depth, for structs and arrays |
| UNSAVED_ARRAY | `int t[] = { ... };` sized from the initializer |
| MODASSIGN | `%=` |
| NULL_INIT | `NULL` is predefined (it is a Vircon32 C keyword), so `#ifndef NULL` fallbacks no longer leak a `#define NULL 0` into the output |
| FP_PARAM_NAME | also needed `fn = 0;` / `fn != 0` → `NULL` for function pointers |
| VOID_PARAM, ARRAY_PARAM, TERNARY_ARG, TERNARY_MEMBER, COMMA_ARG, STRUCT_ASSIGN, CHAR, BITNOT | already worked; the table at the bottom of `gap_probes.cpp` was out of date |
| BITFIELD | accepted with a warning, stored as full-word members; `--reject-bit-fields` makes it an error |
| STRUCT_RETURN | multi-word structs returned or passed by value are rewritten to travel by address (hidden result pointer, callee-side copy of parameters); docs/VIRCON32_QUIRKS.md #11, `tests/101sample.cpp` |

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

### Revert queue — done

The three workarounds are gone: `typedef struct Actor Actor;` and
`typedef struct GameState GameState;` are back, `dim_color` declares
`union ShadeWord u;`, and `ActorFn` names its parameter. `spyvsspy.c`
transpiles, compiles with no errors or warnings, assembles, packs and
runs on the headless console.

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
| Structs with the C tag typedefs (`typedef struct Actor Actor;`) | `Actor`, `GameState` |
| Enum, explicit **and** auto values | `enum ActorKind`, `enum GamePhase` |
| Union with array member; elaborated `union ShadeWord u;` local | `union ShadeWord` (in `dim_color`) |
| Function-pointer typedefs, standard spelling, named parameter | `ActorFn`, `FrameFn` |
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