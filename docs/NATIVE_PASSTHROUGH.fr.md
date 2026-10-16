# Conception : `native Name;` — déclarations de types opaques transmis tels quels

[Version anglaise / in English](NATIVE_PASSTHROUGH.md) | [Version espagnole / en español](NATIVE_PASSTHROUGH.es.md) <!-- langlinks -->

> **État (20260925-dev) : implémenté.** La grammaire, l'AST, le registre et chaque
> rejet sémantique ci-dessous sont en place, avec `tests/90sample.cpp` (usages
> légaux) et `tests/91sample.cpp` (une erreur par forme rejetée). Là où
> l'implémentation s'est écartée du plan ci-dessous, le plan a été laissé tel
> quel et la différence est consignée dans
> [Tel que construit](#tel-que-construit-20260925-dev) à la fin de ce fichier.

## Problème

Les lignes `#include "X.h"` sont du texte transmis tel quel : v32c++ les réémet
et ne les analyse jamais. Conséquence : tout type défini dans un en-tête C
(date\_info, time\_info, game\_signature) et toute fonction qui le prend ou le
renvoie est \*inappelable depuis la source v32c++\*, parce que le nom du type ne
peut pas être lexé comme TYPE\_NAME — il n'est pas dans la table des symboles,
donc `date_info* out;` ne s'analyse pas comme une déclaration. time.hpp a
contourné le problème en réexprimant des conversions de pur calcul dans des
classes ; **memcard.h ne peut pas être contourné de la même façon**
(`card_signature_matches` est un accès au matériel — `cmps R0` — pas du calcul).
Cela avait été décidé d'avance au tour précédent ; ceci en est la spécification.

## Forme

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

Sémantique, précisément :

- `native X;` insère `X` dans la table des symboles comme **SYM\_TYPEDEF** (le
  genre exact qu'insère `typedef_decl`), dans la portée courante, de sorte que la
  classification TYPE\_NAME-contre-IDENTIFIER de l'analyseur lexical le prend en
  compte sans aucune mécanique nouvelle.
- **La génération de code n'émet RIEN** pour la déclaration elle-même. Le nom se
  résout en aval contre le véritable en-tête C, exactement comme un appel
  transmis tel quel.
- Un nouveau nœud d'AST, `AST_NATIVE_DECL` (str1 = nom). Il existe dans l'arbre
  pour que le vidage l'imprime et pour qu'un futur `native` avec charge utile ait
  un foyer ; le parcours du programme par la génération de code l'ignore.

## La règle du pointeur seul (le vrai travail de sema)

Le transpileur ne sait RIEN de la disposition d'un type natif, donc l'usage par
valeur est toujours un mensonge :

- **Autorisé :** `X*`, `X&`, paramètres/retours/variables locales/membres `X*`,
  cibles de conversion `X*` (une fois que les conversions qualifiées seront
  analysées).
- **Rejeté, bruyamment :** variables locales par valeur, paramètres, retours,
  champs membres, et `sizeof(X)`. Chacun est une erreur de sema à l'endroit de la
  déclaration, par ex. `semantic error: native type 'date_info' may only be used by pointer or reference (its layout is defined in a C header this transpiler does not parse)`.

C'est volontairement plus strict que C, et c'est correct : un natif par valeur
produirait silencieusement des emplacements de pile de la mauvaise taille.

## Plan d'implémentation (quatre fichiers, un petit tour)

1. **lexer.l** — ajouter le jeton de mot-clé `NATIVE` (même schéma que
   STATIC/TYPEDEF : `g_last_ident_sym = NULL; return NATIVE;`). Réserver le mot.
2. **parser.y** —
   - `%token NATIVE` dans le bloc de jetons ;
   - nouvelle règle de niveau supérieur :
     ```
     native_decl: NATIVE IDENTIFIER ';'
         {
             symtab_insert(g_symtab, g_symtab->current, $2, SYM_TYPEDEF);
             $$ = ast_new(AST_NATIVE_DECL, @1.first_line);
             $$->str1 = strdup($2);
         }
     ```
     ajoutée à l'alternance de niveau supérieur (à la même position que
     typedef\_decl). Prudence GLR : c'est une règle additive et sans ambiguïté,
     mais régénérez et vérifiez quand même `%expect` ; tout changement de
     grammaire reçoit ce traitement.
3. **ast.h/ast.c** — le genre `AST_NATIVE_DECL` et le cas du vidage (une ligne :
   `NativeDecl str1=%s`).
4. **sema.c** —
   - `collect_declarations` devrait enregistrer les noms natifs dans le MÊME
     registre plat de typedefs que celui des typedefs, marqués opaques (vérifiez
     comment ce registre stocke ses entrées ; un nouvel indicateur `is_native`
     sur l'entrée, ou un ensemble parallèle — alignez-vous sur la forme existante
     du registre).
   - La vérification du pointeur seul : partout où le type d'un
     var\_decl/paramètre/retour est résolu, si le type de base se résout en un
     nom du registre des natifs ET que le type n'est pas enveloppé dans un
     pointeur/une référence, émettre l'erreur bruyante. Trouvez le point central
     de résolution des types plutôt que de vérifier à chaque endroit — la leçon
     d'`infer_expr_type` du tour du registre des énumérations s'applique : un
     goulot d'étranglement unique vaut mieux que des recherches éparpillées.
   - **La génération de code n'émet rien**, mais `infer_expr_type` devrait traiter
     `native X*` comme un type pointeur inconnu-mais-valide, pas comme une erreur.

## La seule ride à vérifier avant d'implémenter

L'entrée de registre de `typedef_decl` consigne le type SOUS-JACENT du typedef.
Un natif n'en a aucun. Vérifiez ce que font les consommateurs d'un type
sous-jacent NULL — si `print_type` ou sema le déréférencent, l'entrée native a
besoin d'un sous-jacent synthétique (par ex. elle-même, ou un substitut jamais
émis). C'est le seul endroit où le raccourci « réutiliser SYM\_TYPEDEF » pourrait
exiger une vraie décision ; inspectez d'abord les consommateurs de
`semac/typereg` (le registre plat de typedefs).

## Décide aussi (à consigner maintenant, à implémenter maintenant) : le pilote memcard peut suivre

Une fois `native` en place, v32/memcard.hpp devient écrivable :

```cpp
native game_signature;

namespace v32
{
    class CardScope { ... }   // save/restore pattern TBD
    bool signature_ok( game_signature* sig ) { return card_signature_matches( sig ); }
}
```

Mais le pilote memcard est un tour À PART — ne le couplez pas à celui-ci.

## Plan de test : tests/90sample.cpp

- `native date_info;` + `native time_info;` au niveau supérieur, puis appeler les
  VRAIES `translate_date` / `translate_time` (la première fois que les originaux
  C sont appelés depuis une source v32c++) et afficher les champs — comparaison
  A/B directe avec `v32::Date::set_from` sur la même valeur de `get_date()` ; les
  deux doivent afficher le même A-M-J. Cela prouve aussi l'absence de régression
  de la conversion de classe face à l'API C elle-même dans la même exécution.
- Usage de pointeurs aux quatre positions : variable locale, paramètre, retour,
  membre.
- Les rejets bruyants, un par ligne (variable locale par valeur, paramètre par
  valeur, retour par valeur, champ par valeur, sizeof) — sema doit nommer le type
  dans chaque erreur.
- Une compilation `--target=standard` avec gcc du C généré (c'est le tour où gcc
  de bureau PEUT vérifier de bout en bout : date\_info/time\_info sont des
  structures C ordinaires et translate\_date/translate\_time sont du C pur).
- Délibérément NON testé : `native` d'un nom qui n'existe pas en aval (c'est la
  même classe que les appels transmis tels quels avec faute de frappe —
  invérifiable tant que le repli par reniflage de v32pp n'existera pas).

## Hors périmètre, dit clairement

- `native` avec des étiquettes d'énumération/union (`find_enum_or_union_decl` a
  son propre chemin d'émission — n'y touchez que si un pilote a réellement besoin
  d'une énumération native).
- Natif qualifié (`native v32::foo;`) — aucun consommateur.
- Le reniflage de structures/`#define` de v32pp — le repli pour l'AUTRE moitié de
  la lacune du passage tel quel (la visibilité des `#define`), tour séparé.

## Tel que construit (20260925-dev)

Ce qui a réellement été livré, et en quoi cela diffère du plan ci-dessus.

**Grammaire.** `native_decl` ne consomme **pas** le `;` elle-même — l'alternative
de niveau supérieur `native_decl ';'` le fait, exactement comme `typedef_decl`. Le
premier jet avait le `;` aux deux endroits, si bien qu'un simple `native X;` était
une erreur de syntaxe sur ce qui suivait et seul `native X;;` s'analysait. Une
seconde alternative, `NATIVE TYPE_NAME`, rend idempotente une déclaration répétée :
une fois `X` dans la table des symboles, l'analyseur lexical renvoie `TYPE_NAME`,
et plusieurs en-têtes de `v32/` peuvent chacun déclarer le même natif. Aucun
changement de `%expect` n'a été nécessaire (toujours 27). Comme `native_decl` est
une `top_decl`, un natif peut aussi être déclaré dans le corps d'un espace de
noms ; un usage qualifié (`v32::game_signature*`) s'imprime sous le nom C simple,
qui est ce que définit l'en-tête en aval.

**Registre.** `TypedefRegEntry` a gagné un indicateur `is_native`, et
`typedef_registry_is_native(name)` (déclarée dans `sema.h`) est l'unique
recherche que toutes les vérifications utilisent. Le type sous-jacent synthétique
portant son propre nom (`n->type = ast_ident(name)`) est conservé pour que rien
de ce qui lit le nœud ne voie NULL, **mais `find_typedef_target` ne le renvoie
jamais pour un natif.** Le renvoyer faisait boucler `resolve_typedef_chain` sur
X → X → X jusqu'à ce que sa garde de 64 pas l'arrête. Désormais la chaîne
s'arrête sur le nom natif dès le premier pas. La préoccupation de la section de
la « ride » est ainsi réglée.

**Conflits.** `native X;` alors que `X` est déjà une classe, une structure ou un
typedef ordinaire est une erreur sémantique
(`'native X;' conflicts with an earlier declaration...`). Marquer silencieusement
comme natif un type à disposition connue interdirait rétroactivement tout usage
par valeur de celui-ci.

**Où la règle du pointeur seul est vérifiée.** Tout est dans sema.c, via
`check_native_pointer_only` (qui ôte d'abord le `const`, de sorte que
`const date_info d` est lui aussi rejeté) :

| Position | Où |
| --- | --- |
| champ membre de classe/structure | parcours des membres de `compute_layout` |
| variable locale | `check_node`, `AST_VAR_DECL` |
| variable globale | `check_globals` |
| type de retour + chaque paramètre, prototypes et définitions, fonctions libres et méthodes | `check_native_signature`, appelée une fois par fonction depuis `check_function_body` / `access_check_free_functions` |
| `sizeof(X)` | `check_node`, `AST_SIZEOF` (`sizeof(X*)` passe) |
| `p->field` / `r.field` à travers un natif | `check_node`, `AST_MEMBER` (via `native_base_of`) |

Un jet antérieur appelait aussi `check_native_pointer_only` depuis la vérification
de taille de mot de lower.c. C'était une erreur d'édition de liens, car la
fonction est statique à sema.c, et cela aurait signalé deux fois chaque paramètre
par valeur. Cet appel a disparu. L'avertissement multi-mots de lower.c est de
toute façon silencieux pour les natifs : un natif n'a pas de `StructLayout`.

**L'étape A/B du plan de test ne peut pas encore s'écrire.** Le plan ci-dessus
appelle `translate_date` sur un stockage que possède la source v32c++ puis
affiche les champs. Les deux moitiés sont désormais rejetées par conception : un
`date_info d;` par valeur n'a pas de disposition à allouer, et `p->year` est un
accès de membre à travers un natif. `translate_date(get_date(), p)` a besoin d'un
`p` pointant vers un vrai stockage, et la source v32c++ ne peut pas créer ce
stockage. Aujourd'hui le pointeur doit venir du C, par exemple une globale côté C
ou une fonction C renvoyant `date_info*`. Deux voies à suivre, toutes deux du
travail futur :

1. **Natifs dimensionnés :** `native date_info[3];` (ou similaire) donne le nombre
   de mots, ce qui permet les variables locales par valeur et `sizeof` tandis que
   l'accès aux membres reste interdit.
2. **Natifs à disposition :** `native date_info { int year; int month; int day; };`
   déclare les champs et n'émet rien. C'est ce pour quoi la note sur un « futur
   `native` avec charge utile » de la section de l'AST réservait de la place.

Le pilote memcard n'a pas ce problème, parce que
`card_signature_matches(game_signature*)` n'a besoin que d'un pointeur, et un
pointeur vers un `game_signature` défini côté C suffit.

**Les paramètres de référence vers un natif** (`date_info&`) sont acceptés et
abaissés en pointeurs comme n'importe quelle autre référence. La transmission
`touch(d)` comme `touch(*p)` sortent correctes. `&d` sur un paramètre de
référence **ne** l'est **pas** : il émet `(&d)`, un `date_info **`. Ce n'est pas
propre aux natifs. C'est la lacune générale d'abaissement des références
consignée dans DESIGN_NOTES.md (« Round: `native` opaque types »), c'est pourquoi
sample90 n'utilise pas cette écriture.

## Le pilote memcard (livré)

`v32/memcard.hpp` est le premier vrai consommateur. Il déclare
`native game_signature;` à la portée du fichier et évite le problème de stockage
sans aucune nouvelle fonctionnalité du langage. Un `game_signature` *est* un
`typedef int[20]`, donc l'API de l'enveloppe prend un simple `int*` vers 20 mots,
que l'appelant peut déclarer (`int sig[20] = "MYGAME";`), et le convertit à la
frontière avec C :

```cpp
bool card_matches( int* signature )
{
    return card_signature_matches( (game_signature*)signature );
}
```

Vérifié de bout en bout : le vrai compilateur Vircon32 accepte la conversion, et
`tests/94sample.cpp` s'exécute sur la console émulée avec une carte mémoire
insérée. Le fichier de la carte contient ensuite la signature au mot 0. Les
options de natifs dimensionnés ou à disposition ci-dessus restent la réponse
générale pour les structures C dont le code du jeu doit lire les champs
(`date_info`).
