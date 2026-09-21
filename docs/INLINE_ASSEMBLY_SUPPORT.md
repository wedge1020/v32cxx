# Inline assembly support for v32c++

The transpiler currently has no `asm` support at all. Let me pull the key
parts of the  lexer, grammar, AST, and codegen so  I can propose concrete
changes.I have a full picture now.  The transpiler has zero `asm` support
today  (no token,  no AST  node, codegen  would hit  "unhandled statement
kind"). Since this is a substantial code deliverable, I'll write it up as
a canvas with the exact patches.

## Goal

Let  v32c++ source  contain inline  assembly, passing  it through  to the
underlying Vircon32  C compiler (which  supports it natively —  see the
standard library's `video.h`,  where every GPU function is a  thin `asm {
... }` wrapper).  Also accept the other  common inline-assembly spellings
found in the wild  (GCC/Clang basic `asm("...")`, `__asm`/`__asm__`, `asm
volatile`), so  long as  **when targeting Vircon32  the output  is always
Vircon32 C's brace form**.

## What formats exist, and what we do with each

| Source form | Example | Handling |
|---|---|---|
| Vircon32 native | `asm { "out GPU_Command, GPUCommand_DrawRegion" }` | Verbatim pass-through (the `{param}` interpolation inside the string literals is resolved by the Vircon32 C compiler, not by us) |
| GCC/Clang basic asm | `asm("nop");`, `asm ("a" "b");` | Accepted; re-emitted as Vircon32 brace form in `--target=vircon32` (default), kept parenthesized in `--target=standard` |
| Spelling variants | `__asm`, `__asm__`, `asm volatile`, `__volatile__` | All lex to the same token; the qualifier is accepted and ignored (it only affects optimization, which we don't do) |
| GCC extended asm | `asm("..." : "=r"(x) : "r"(y) : "cc")` | **Rejected with a targeted diagnostic** — operand constraints have no Vircon32 equivalent; the Vircon32 dialect's `{param}` interpolation is the intended substitute |
| MSVC `__asm { mov eax, 1 }` (unquoted instructions) | — | Rejected: not a string-literal body, nothing to translate. The brace form is only accepted when every instruction is a quoted literal, i.e. exactly the Vircon32 shape |

Only **statement-level**  asm is  supported. That  is sufficient  for the
entire standard library pattern (`int f() { asm { "in R0, X" } }` — the
asm block leaves  its result in R0 and the  Vircon32 compiler treats that
as the return  value). GCC-style asm *expressions* (`int  x = asm(...);`)
don't exist in basic asm anyway and are left unsupported.

## Design

One  new AST  node, one  new token,  one new  grammar rule  set, one  new
codegen case.  No changes to sema.c  or lower.c: an asm  statement has no
children to  rewrite — no `this`,  no references, no operators  — and
every  existing  lowering walk  uses  `default:  break;`, so  an  unknown
statement kind already flows through  untouched. (Worth adding AST_ASM to
their  `switch`es  anyway  as  a  no-op case  with  a  comment,  per  the
codebase's own style of documenting deliberate gaps.)

The asm body  is stored as an **AstList of  AST_STRING_LIT nodes, one per
consecutive string  literal in  source order**  — not  one concatenated
string — so  codegen can print each  literal on its own  line, which is
how  `video.h`  itself formats  multi-instruction  blocks  and keeps  the
generated  C readable.  The  lexer's STRING_LITERAL  rule already  strips
quotes and keeps  escapes as raw backslash pairs,  and codegen's existing
re-quoting  rule round-trips  that  exactly (the  same reasoning  already
documented  on  AST_STRING_LIT's  print  case),  so  `{texture_id}`-style
braces and any escapes survive verbatim.

`ival` on  the node records the  *dialect as written* (`0`  = brace form,
`1` = parenthesized/GCC form),  which only matters in `--target=standard`
mode; in Vircon32 mode both dialects emit the brace form.

---

## Patch 1 — `lexer.l`

Add to the keyword block (next to `"goto"`, `"sizeof"`, etc.):

```c
/* Inline assembly. All three spellings collapse to one token; the
 * optional volatile qualifier is handled in the grammar. Not reserved
 * as an identifier anywhere else, matching real C/C++ where asm is a
 * (nonstandard-but-universal) extension keyword rather than core. */
"asm"        { g_last_ident_sym = NULL; return ASM; }
"__asm"      { g_last_ident_sym = NULL; return ASM; }
"__asm__"    { g_last_ident_sym = NULL; return ASM; }
"volatile"         { g_last_ident_sym = NULL; return VOLATILE; }
"__volatile__"     { g_last_ident_sym = NULL; return VOLATILE; }
```

Note: `volatile`  is currently not a  keyword at all (README  lists it as
unsupported). Making it  one solely so `asm volatile` parses  is safe —
it was  previously lexed as  an IDENTIFIER, but  no real program  in this
subset  could have  used  it as  a variable  name  without already  being
flagged. If  you'd rather not  reserve it  globally, an alternative  is a
dedicated  lexer rule  `asm[ \t]+volatile`  matched *before*  the keyword
table; the keyword approach is simpler and matches how GCC treats it.

## Patch 2 — `parser.y`

**Declarations**   (with   the   other   `%token`s;   `%glr-parser`   and
`%locations` are already on, nothing new needed there):

```bison
%token ASM
%token VOLATILE
```

**New nonterminal** (place near `arg_list`):

```bison
/* Consecutive string literals forming one asm body -- `asm { "a" "b" }`
 * and asm("a" "b") both allow implicit concatenation, same as real C's
 * own adjacent-literal rule. One AST_STRING_LIT per literal, in source
 * order; codegen re-quotes and prints one per line. The lexer already
 * stripped the quotes and left escapes raw, so a `{param}` inside the
 * literal (Vircon32's own operand-interpolation syntax, see video.h)
 * survives this round trip verbatim -- we never interpret the body. */
asm_string_list:
      STRING_LITERAL           { $$ = ast_list_new(); ast_list_append(&$$, $1); }
    | asm_string_list STRING_LITERAL  { $$ = $1; ast_list_append(&$$, $2); }
    ;
```

(Check `ast_list_append`'s  signature —  if it takes  `AstList*` rather
than  returning the  node,  store literals  in an  `AST_INIT_LIST`-shaped
wrapper  node  the same  way  `opt_member_init_list`  does and  put  that
wrapper in the  AST_ASM's `list` field instead. Either  shape works; pick
whichever matches the surrounding rules.)

**Statement rules** (inside `stmt:`):

```bison
    | ASM '{' asm_string_list '}'
        {
            /* Vircon32 C's own native form -- pure pass-through. */
            $$ = ast_new(AST_ASM, @1.first_line);
            $$->list = $3;
            $$->ival = 0;  /* written in brace form */
        }
    | ASM '(' asm_string_list ')' ';'
        {
            /* GCC/Clang basic asm. No operands allowed (basic asm has
             * none); codegen re-emits this as Vircon32 brace form when
             * targeting Vircon32, or keeps the parenthesized spelling
             * in --target=standard. An extended asm (with ':'
             * constraint sections) does NOT parse here -- the ':' after
             * the string list is a syntax error at this rule; see the
             * targeted diagnostic note below. */
            $$ = ast_new(AST_ASM, @1.first_line);
            $$->list = $3;
            $$->ival = 1;  /* written in GCC parenthesized form */
        }
    | VOLATILE ASM '(' asm_string_list ')' ';'
        {
            /* `asm volatile("...")` -- the qualifier only governs
             * optimization/reordering, which this transpiler performs
             * neither of, so it is accepted and dropped. */
            $$ = ast_new(AST_ASM, @2.first_line);
            $$->list = $4;
            $$->ival = 1;
        }
    | ASM VOLATILE '(' asm_string_list ')' ';'
        {
            /* __volatile__ spelled after the keyword (GCC documents
             * both orders historically; harmless to accept). */
            $$ = ast_new(AST_ASM, @1.first_line);
            $$->list = $4;
            $$->ival = 1;
        }
```

**Targeted diagnostic  for extended asm.**  Rather than let  `asm("..." :
"=r"(x) : ...)` die in GLR with a generic "syntax error, unexpected ':'",
add one rule that matches the constraint shape and reports cleanly:

```bison
    | ASM '(' asm_string_list ':' /* deliberately incomplete */
        {
            yyerror("extended asm with operand constraints is not "
                    "supported: Vircon32 C uses '{param}' interpolation "
                    "inside the literal instead (see video.h); write the "
                    "operands directly in the instruction text");
            YYERROR;
        }
```

(Add further  `:` alternatives if  you want the  message to also  fire on
2–3 colon sections; with `%define  parse.error verbose` the single rule
above plus a comment in DESIGN_NOTES is probably enough — the first `:`
is what always appears.)

Note on the brace form: `ASM '{' ... '}'` introduces no grammar ambiguity
worth worrying about — `asm` is  a dedicated token, so the parser never
confuses it  with an  expression statement, and  no GLR  splitting occurs
beyond what the file already handles.

## Patch 3 — `ast.h`

Add  to `AstKind`  (statement region,  near AST_GOTO/AST_LABEL)  with the
codebase's doc-comment style:

```c
    AST_ASM,              /* list=AST_STRING_LIT per string literal of
                              the body, in source order; str1 unused.
                              ival=0 when written in Vircon32 brace form
                              (`asm { "..." }`), 1 when written in GCC
                              basic-asm parenthesized form (`asm("...")`)
                              -- which only changes what --target=standard
                              mode prints, since Vircon32 output is ALWAYS
                              the brace form (the Vircon32 C compiler is
                              the one that understands it). Pure
                              pass-through: nothing in sema.c or lower.c
                              interprets the body, and the `{param}`
                              interpolation inside each literal (see
                              video.h's own GPU wrappers) is resolved by
                              the Vircon32 C compiler, not by us. The
                              lexer already stripped each literal's quotes
                              and left escapes raw; codegen.c's existing
                              re-quote rule (see AST_STRING_LIT's own
                              print case) round-trips them exactly. A leaf
                              statement -- never an expression, matching
                              basic asm in real compilers; the standard
                              library's own `int f() { asm { "in R0, X" } }`
                              pattern works as-is because Vircon32 C
                              treats the block's R0 as the return value. */
```

Also update  `ast.c`'s `ast_dump`  switch if it  enumerates kinds  (add a
case printing the literal count).

## Patch 4 — `codegen.c`

In  `print_stmt`'s switch  (the  same switch  whose `default:`  currently
prints "unhandled statement kind" — this  is exactly the gap that fires
today on any asm statement):

```c
        case AST_ASM:
            /* Pure pass-through: the Vircon32 C compiler is the one that
             * actually assembles this. TARGET_VIRCON32 always emits the
             * brace form regardless of which dialect was written, since
             * that's the only form Vircon32's compiler accepts; TARGET_
             * STANDARD keeps the dialect as written (gcc/clang accept the
             * parenthesized basic-asm spelling natively, and a brace-form
             * body re-wrapped in parentheses is equally valid there).
             * One string literal per line, matching how video.h itself
             * formats multi-instruction blocks -- purely cosmetic.
             * Re-quoting each literal is the same round-trip rule as
             * AST_STRING_LIT's own case above: the lexer stored the inner
             * text with escapes still raw, so wrapping it back in quotes
             * reproduces the source literal exactly, `{param}` braces
             * included. */
            if (s->ival == 1 && g_target == TARGET_STANDARD) {
                indent_spaces(out, indent);
                fprintf(out, "asm(");
                for (int i = 0; i < s->list.count; i++) {
                    if (i > 0) fprintf(out, " ");
                    fprintf(out, "\"%s\"", s->list.items[i]->str1);
                }
                fprintf(out, ");\n");
            }
            else {
                indent_spaces(out, indent);
                fprintf(out, "asm\n");
                indent_spaces(out, indent);
                fprintf(out, "{\n");
                for (int i = 0; i < s->list.count; i++) {
                    indent_spaces(out, indent + 1);
                    fprintf(out, "\"%s\"\n", s->list.items[i]->str1);
                }
                indent_spaces(out, indent);
                fprintf(out, "}\n");
            }
            break;
```

(Use  whatever the  file's actual  target-mode global  is —  the search
hits  show both  `--target` modes  exist  in codegen.c;  if it's  spelled
differently,  e.g. a  `g_target_mode` enum,  substitute. The  `explain()`
helper can also  get a one-liner for verbose mode,  e.g. "inline assembly
passed through verbatim; Vircon32's C compiler assembles it".)

## Patch 5 — `lower.c` / `sema.c` (no-op, but explicit)

Neither pass  needs to  touch AST_ASM  — no  `this`, no  references, no
operators inside  a string-literal  body —  and both  already `default:
break;` on unknown statement kinds.  Per the codebase's own convention of
documenting deliberate non-actions, add to each statement-walking switch:

```c
        case AST_ASM:
            /* Nothing to do: an asm body is opaque string literals --
             * no `this`, member references, or references to rewrite.
             * Passed through to codegen.c verbatim. */
            break;
```

One real caveat worth a DESIGN_NOTES entry: a `goto` that jumps *over* an
asm statement, or  an asm statement placed between  a local's declaration
and its  destructor call  at scope  exit, behaves  like any  other opaque
statement —  lowering already  emits destructor calls  around statement
boundaries, and asm doesn't disturb that. But asm that clobbers registers
the compiler believes live across it is the programmer's problem, exactly
as in real C.

---

## Round-trip check against `video.h`

Every pattern in the uploaded header transpiles unchanged:

```cpp
void select_texture( int texture_id )
{
    asm
    {
        "mov R0, {texture_id}"
        "out GPU_SelectedTexture, R0"
    }
}
```

- lexer: `asm` → ASM; the two STRING_LITERALs keep `"mov R0, {texture_id}"`
  inner text raw (braces are ordinary string characters to flex).
- parser: brace rule fires, two AST_STRING_LITs in `list`, `ival = 0`.
- sema/lower: no-op.
- codegen: prints the identical brace block, one literal per line.

The value-returning shape works too:

```cpp
int get_selected_texture()
{
    asm
    {
        "in R0, GPU_SelectedTexture"
    }
}
```

Statement-level asm leaves  the result in R0, and Vircon32  C treats that
as the function's return value — no expression form needed.

And a GCC-dialect user file:

```cpp
void nop() { asm("nop"); }
```

produces, in Vircon32 mode:

```c
void nop( void )
{
    asm
    {
        "nop"
    }
}
```

## Suggested test additions

1. Single- and multi-literal brace form (copy two functions verbatim from `video.h`, e.g. `select_texture` and `get_drawing_point` — the latter also exercises the `"push R1"` / `"mov [R1], R0"` pointer-output idiom).
2. GCC basic asm, with and without `volatile`, all three keyword spellings.
3. `{param}` interpolation with a parameter, a local, and (in a method) an
   injected-`this` field — confirm this-injection does NOT rewrite inside the
   literals (it must not: `{this->x}` is not valid Vircon32 asm syntax; the
   caller should hoist to a local first).
4. Extended asm → expect the targeted diagnostic, not a generic syntax error.
5. `--target=standard` round trip of both dialects through gcc.
6. asm inside a method (after this-injection) and inside a loop body — confirm
   the statement walks pass it through.

I dug through the v32c++ sources in the library and confirmed the current
state: there's  no `asm`  support at all  — no token  in the  lexer, no
statement rule  in the grammar,  no AST node, and  codegen's `print_stmt`
would hit its  "unhandled statement kind" warning. So this  needs a small
feature, not just a fix.

I've written up  a complete design with patches in  a canvas ("v32c++ —
Inline Assembly Support"), covering:

**What gets supported**
- **Vircon32 native brace form** (`asm { "out GPU_Command, ..." }`) — verbatim pass-through; the `{param}` interpolation stays in the string literal because the Vircon32 C compiler is what resolves it, matching every wrapper in `video.h`
- **GCC/Clang basic asm** — `asm("...")`, `__asm`, `__asm__`, `asm volatile` (qualifier accepted and dropped, since the transpiler doesn't reorder anything). When targeting Vircon32 these are re-emitted in brace form; `--target=standard` keeps the parenthesized spelling
- **Statement-level only** — that's all `video.h` needs, including the value-returning idiom (`asm` leaves its result in R0, which Vircon32 C treats as the return value)
- **GCC extended asm with operand constraints is rejected with a targeted diagnostic** — constraints have no Vircon32 equivalent; `{param}` interpolation is the substitute. MSVC's unquoted `__asm { mov ... }` form is also rejected (nothing to translate)

**The  changes**: one  keyword block  in `lexer.l`,  an `asm_string_list`
nonterminal  plus four  statement rules  in `parser.y`  (with a  targeted
error  rule  for  extended  asm),  an  `AST_ASM`  kind  in  `ast.h`  (one
`AST_STRING_LIT` per literal, `ival`  recording the written dialect), one
`print_stmt` case in `codegen.c`, and  explicit documented no-op cases in
`lower.c`/`sema.c` — neither pass needs to  touch it, since an asm body
contains no `this`, references, or operators to rewrite.

One nice property: the lexer's existing string handling (quotes stripped,
escapes kept raw)  plus codegen's existing re-quote  rule round-trips the
literals exactly, so `{texture_id}` braces survive verbatim with zero new
machinery. The canvas also includes a round-trip walkthrough of `video.h`
functions and a suggested test list.
