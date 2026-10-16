# Prise en charge de l'assembleur en ligne pour v32c++

[Version anglaise / in English](INLINE_ASSEMBLY_SUPPORT.md) | [Version espagnole / en español](INLINE_ASSEMBLY_SUPPORT.es.md) <!-- langlinks -->

## Objectif

Permettre à la source v32c++ de contenir de l'assembleur en ligne, en le
transmettant tel quel au compilateur C de Vircon32 sous-jacent (qui le prend en
charge nativement — voir le `video.h` de la bibliothèque standard, où chaque
fonction du GPU est une mince enveloppe `asm { ... }`). Accepter aussi les autres
écritures courantes de l'assembleur en ligne rencontrées dans la nature (l'`asm("...")`
basique de GCC/Clang, `__asm`/`__asm__`, `asm volatile`), pourvu que, **lorsqu'on
cible Vircon32, la sortie soit toujours la forme à accolades du C de Vircon32**.

## Quels formats existent, et ce que nous faisons de chacun

| Forme dans la source | Exemple | Traitement |
| --- | --- | --- |
| Native Vircon32 | `asm { "out GPU_Command, GPUCommand_DrawRegion" }` | Transmission telle quelle (l'interpolation `{param}` à l'intérieur des littéraux de chaîne est résolue par le compilateur C de Vircon32, pas par nous) |
| `asm` basique de GCC/Clang | `asm("nop");`, `asm ("a" "b");` | Accepté ; réémis sous la forme à accolades de Vircon32 avec `--target=vircon32` (la valeur par défaut), conservé entre parenthèses avec `--target=standard` |
| Variantes d'écriture | `__asm`, `__asm__`, `asm volatile`, `__volatile__` | Toutes sont lexées comme le même jeton ; le qualificatif est accepté et ignoré (il n'affecte que l'optimisation, que nous ne faisons pas) |
| `asm` étendu de GCC | `asm("..." : "=r"(x) : "r"(y) : "cc")` | **Rejeté avec un diagnostic ciblé** — les contraintes d'opérandes n'ont pas d'équivalent Vircon32 ; le substitut prévu est l'interpolation `{param}` du dialecte Vircon32 |
| `__asm { mov eax, 1 }` de MSVC (instructions sans guillemets) | — | Rejeté : le corps n'est pas un littéral de chaîne, il n'y a rien à traduire. La forme à accolades n'est acceptée que lorsque chaque instruction est un littéral entre guillemets, c'est-à-dire exactement la forme Vircon32 |

Seul l'assembleur **au niveau de l'instruction** est pris en charge. Cela suffit
pour tout le schéma de la bibliothèque standard (`int f() { asm { "in R0, X" } }`
— le bloc asm laisse son résultat dans R0 et le compilateur Vircon32 le traite
comme la valeur de retour). Les *expressions* asm de style GCC
(`int x = asm(...);`) n'existent de toute façon pas dans l'asm basique et restent
non prises en charge.

## Conception

Un nouveau nœud d'AST, un nouveau jeton, un nouvel ensemble de règles de
grammaire, un nouveau cas de génération de code. Aucun changement dans sema.c ni
dans lower.c : une instruction asm n'a pas d'enfants à réécrire — ni `this`, ni
références, ni opérateurs — et chaque parcours d'abaissement existant utilise
`default: break;`, donc un genre d'instruction inconnu passe déjà sans être
touché. (Cela vaut quand même la peine d'ajouter AST\_ASM à leurs `switch` comme
cas sans effet avec un commentaire, selon le propre style du code qui documente
les omissions délibérées.)

Le corps de l'asm est stocké comme une **AstList de nœuds AST\_STRING\_LIT, un par
littéral de chaîne consécutif dans l'ordre de la source** — pas une seule chaîne
concaténée — afin que la génération de code puisse imprimer chaque littéral sur sa
propre ligne, ce qui est la manière dont `video.h` lui-même met en forme les blocs
multi-instructions et qui garde le C généré lisible. La règle STRING\_LITERAL de
l'analyseur lexical retire déjà les guillemets et conserve les échappements sous
forme de paires d'antislash brutes, et la règle de remise entre guillemets
existante de la génération de code l'aller-retourne exactement (le même
raisonnement déjà documenté au cas d'impression d'AST\_STRING\_LIT), de sorte que
les accolades de style `{texture_id}` et tout échappement survivent tels quels.

`ival` sur le nœud consigne le *dialecte tel qu'écrit* (`0` = forme à accolades,
`1` = forme entre parenthèses/GCC), ce qui ne compte qu'en mode
`--target=standard` ; en mode Vircon32 les deux dialectes émettent la forme à
accolades.

---

## Correctif 1 — `lexer.l`

Ajouter au bloc des mots-clés (à côté de `"goto"`, `"sizeof"`, etc.) :

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

Remarque : `volatile` n'est actuellement pas du tout un mot-clé (le README le
liste comme non pris en charge). En faire un uniquement pour que `asm volatile`
s'analyse est sans danger — il était auparavant lu comme un IDENTIFIER, mais
aucun programme réel de ce sous-ensemble n'aurait pu l'utiliser comme nom de
variable sans avoir déjà été signalé. Si vous préférez ne pas le réserver
globalement, une alternative est une règle lexicale dédiée `asm[ \t]+volatile`
appariée *avant* la table des mots-clés ; l'approche par mot-clé est plus simple
et correspond à la façon dont GCC le traite.

## Correctif 2 — `parser.y`

**Déclarations** (avec les autres `%token` ; `%glr-parser` et `%locations` sont
déjà activés, rien de nouveau n'est nécessaire de ce côté) :

```bison
%token ASM
%token VOLATILE
```

**Nouveau non-terminal** (à placer près de `arg_list`) :

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

(Vérifiez la signature d'`ast_list_append` — si elle prend un `AstList*` au lieu
de renvoyer le nœud, stockez les littéraux dans un nœud enveloppe en forme
d'`AST_INIT_LIST` comme le fait `opt_member_init_list` et placez cette enveloppe
dans le champ `list` de l'AST\_ASM. L'une ou l'autre forme convient ; choisissez
celle qui s'accorde aux règles environnantes.)

**Règles d'instruction** (à l'intérieur de `stmt:`) :

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

**Diagnostic ciblé pour l'asm étendu.** Plutôt que de laisser
`asm("..." : "=r"(x) : ...)` mourir dans GLR avec un générique « syntax error,
unexpected ':' », ajoutez une règle qui reconnaît la forme des contraintes et le
signale proprement :

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

(Ajoutez d'autres alternatives avec `:` si vous voulez que le message se
déclenche aussi pour 2–3 sections séparées par des deux-points ; avec
`%define parse.error verbose`, la règle unique ci-dessus plus un commentaire dans
DESIGN\_NOTES suffit probablement — le premier `:` est ce qui apparaît toujours.)

Remarque sur la forme à accolades : `ASM '{' ... '}'` n'introduit aucune ambiguïté
de grammaire dont il faille s'inquiéter — `asm` est un jeton dédié, donc
l'analyseur ne le confond jamais avec une instruction-expression, et aucune
scission GLR n'a lieu au-delà de ce que le fichier gère déjà.

## Correctif 3 — `ast.h`

Ajouter à `AstKind` (région des instructions, près d'AST\_GOTO/AST\_LABEL) avec le
style de commentaires de documentation du code :

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

Mettez aussi à jour le `switch` d'`ast_dump` dans `ast.c` s'il énumère les genres
(ajoutez un cas imprimant le nombre de littéraux).

## Correctif 4 — `codegen.c`

Dans le `switch` de `print_stmt` (le même `switch` dont le `default:` imprime
aujourd'hui « unhandled statement kind » — c'est exactement la lacune qui se
déclenche aujourd'hui sur toute instruction asm) :

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

(Utilisez le nom réel de la globale de mode cible du fichier — les résultats de
recherche montrent que les deux modes de `--target` existent dans codegen.c ; si
elle s'écrit autrement, par ex. une énumération `g_target_mode`, substituez-la.
L'auxiliaire `explain()` peut aussi recevoir une ligne pour le mode verbeux, par
ex. « inline assembly passed through verbatim; Vircon32's C compiler assembles
it ».)

## Correctif 5 — `lower.c` / `sema.c` (sans effet, mais explicite)

Aucune des deux passes n'a besoin de toucher AST\_ASM — pas de `this`, pas de
références, pas d'opérateurs à l'intérieur d'un corps de littéral de chaîne — et
toutes deux font déjà `default: break;` sur les genres d'instructions inconnus.
Selon la convention propre au code de documenter les non-actions délibérées,
ajoutez à chaque `switch` qui parcourt les instructions :

```c
        case AST_ASM:
            /* Nothing to do: an asm body is opaque string literals --
             * no `this`, member references, or references to rewrite.
             * Passed through to codegen.c verbatim. */
            break;
```

Une vraie réserve qui mérite une entrée dans DESIGN\_NOTES : un `goto` qui saute
*par-dessus* une instruction asm, ou une instruction asm placée entre la
déclaration d'une variable locale et l'appel de son destructeur en sortie de
portée, se comporte comme n'importe quelle autre instruction opaque — l'abaissement
émet déjà les appels de destructeurs autour des frontières d'instructions, et
l'asm ne les perturbe pas. Mais un asm qui écrase des registres que le compilateur
croit vivants à travers lui est le problème du programmeur, exactement comme en C
réel.

---

## Vérification aller-retour avec `video.h`

Chaque schéma de l'en-tête fourni se transpile sans changement :

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

- lexical : `asm` → ASM ; les deux STRING\_LITERAL conservent brut le texte
  intérieur `"mov R0, {texture_id}"` (les accolades sont des caractères de chaîne
  ordinaires pour flex).
- syntaxique : la règle à accolades se déclenche, deux AST\_STRING\_LIT dans
  `list`, `ival = 0`.
- sema/lower : sans effet.
- génération de code : imprime le bloc à accolades identique, un littéral par
  ligne.

La forme qui renvoie une valeur fonctionne aussi :

```cpp
int get_selected_texture()
{
    asm
    {
        "in R0, GPU_SelectedTexture"
    }
}
```

L'asm au niveau de l'instruction laisse le résultat dans R0, et le C de Vircon32
le traite comme la valeur de retour de la fonction — aucune forme d'expression
n'est nécessaire.

Et un fichier utilisateur en dialecte GCC :

```cpp
void nop() { asm("nop"); }
```

produit, en mode Vircon32 :

```c
void nop( void )
{
    asm
    {
        "nop"
    }
}
```

## Ajouts de tests suggérés

1. Forme à accolades à un et à plusieurs littéraux (copiez deux fonctions telles
   quelles de `video.h`, par ex. `select_texture` et `get_drawing_point` — cette
   dernière exerce aussi l'idiome de sortie par pointeur `"push R1"` /
   `"mov [R1], R0"`).
2. `asm` basique de GCC, avec et sans `volatile`, avec les trois écritures du
   mot-clé.
3. Interpolation `{param}` avec un paramètre, une variable locale et (dans une
   méthode) un champ à `this` injecté — confirmez que l'injection de `this` ne
   réécrit PAS à l'intérieur des littéraux (elle ne doit pas : `{this->x}` n'est
   pas une syntaxe d'asm Vircon32 valide ; l'appelant devrait d'abord la sortir
   dans une variable locale).
4. `asm` étendu → attendez-vous au diagnostic ciblé, pas à une erreur de syntaxe
   générique.
5. Aller-retour `--target=standard` des deux dialectes à travers gcc.
6. asm dans une méthode (après l'injection de `this`) et dans un corps de boucle —
   confirmez que les parcours d'instructions le laissent passer.

## À l'usage : `v32/v32io.hpp` (20261009-dev)

L'en-tête clavier/souris est le premier en-tête de `v32/` à dépendre de
l'assembleur en ligne, à l'intérieur d'une méthode. `v32::IoDevice` écrit une
routine en code machine de 68 mots dans un tableau membre dans son constructeur
et l'appelle depuis `scan()` :

```cpp
entry = (int)(&routine[ 0 ]);
asm
{
    "PUSH  R0"
    "MOV   R0, {entry}"
    "CALL  R0"
    "POP   R0"
}
```

`{entry}` est une variable locale de la méthode, de sorte que l'interpolation
`{param}` du compilateur Vircon32 la résout (un membre, `{this->...}`, ne serait
pas valide ; d'où la variable locale). Vérifié de bout en bout sur la console sans
écran par `tests/119sample.cpp`. Voir `docs/V32IO.fr.md`.
