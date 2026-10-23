# Particularités de la sortie propres à Vircon32

[Version anglaise / in English](VIRCON32_QUIRKS.md) | [Version espagnole / en español](VIRCON32_QUIRKS.es.md) <!-- langlinks -->

Tous les endroits où le C généré par ce projet s'écarte délibérément du C standard,
réunis en une seule liste, organisés par particularité et non par moment de
découverte. `docs/DESIGN_NOTES.md` (en anglais uniquement) contient l'histoire
complète, tour par tour, de *la manière dont* chacune a été trouvée (en général à la
dure, face au vrai compilateur) ; ce document a un but plus étroit : une liste de
contrôle pour le jour où l'on construit un mode de sortie `--standard-c` (ou
équivalent), afin que ce travail consiste à « parcourir cette liste et rendre chaque
entrée conditionnelle » plutôt qu'à réenquêter sur toute la base de code.

**L'option `--target` existe désormais** (`vircon32`/`v32`, la valeur par défaut, ou
`standard`/`std`) — cette liste de contrôle a fait exactement ce pour quoi elle avait
été écrite. Chaque entrée ci-dessous est maintenant marquée IMPLÉMENTÉE, PAS ENCORE
IMPLÉMENTÉE (une lacune réelle et délibérée) ou N/A (aucun changement nécessaire).
Voir l'entrée de `docs/DESIGN_NOTES.md` consacrée à ce tour pour l'histoire complète,
y compris deux vrais bogues découverts uniquement en testant la sortie réelle de
`--target=standard` plutôt qu'en se fiant au plan sur le papier.

**Convention ci-dessous** : « Vircon32 exige » est la forme non standard que ce projet
émet actuellement toujours. « C standard » est ce qu'un compilateur C simple et
portable attend/accepte à la place. « Statut » indique si le comportement de Vircon32
a été confirmé avec le vrai compilateur ou seulement déduit par raisonnement. « Où »
nomme la ou les fonctions qui auraient besoin d'une branche conditionnelle.

---

## Ce qu'est ce document — et ce qu'il n'est pas

**Ces entrées décrivent le C de Vircon32, le langage de SORTIE, et non ce que vous
pouvez écrire.** Votre C++ (ou C) reste ordinaire : v32c++ réécrit chaque construction
ci-dessous que le C de Vircon32 ne peut pas accepter, de sorte qu'aucune n'est une
restriction pour votre entrée, sauf indication contraire du tableau. Les titres
énoncent la règle du C de Vircon32 (« pas d'opérateur ternaire ») parce que cette règle
est ce que la réécriture contourne ; chaque entrée dit ce que v32c++ fait à ce sujet.
Pour ce que vous pouvez et ne pouvez pas ÉCRIRE, lisez le tableau ci-dessous et les
« Limitations connues » du README pour le langage lui-même.

**Également valides, et sans entrée nécessaire :** les commentaires `//` et `/* */`
(une barre oblique inverse en fin de ligne `//` poursuit le commentaire, comme en C++),
les littéraux de chaîne adjacents (`"ab" "cd"`, sur plusieurs lignes ou autour d'un
commentaire), une barre oblique inverse suivie d'un saut de ligne dans un littéral ou un
`#define`, les macros fonctionnelles, l'opérateur ternaire, l'opérateur virgule, les
littéraux hexadécimaux/octaux/binaires, `1e3`, `.5` et `5.` (`tests/133sample.cpp`,
`134sample.c` et `135sample.cpp` les vérifient avec la vraie chaîne d'outils et le vrai
émulateur).

| # | Règle du C de Vircon32 | Dans votre code | Statut |
|---|---|---|---|
| 1 | pas de mot-clé `struct` dans un type | écrivez du C++ comme d'habitude | géré |
| 2 | déclarateurs de tableau `type[n] name` | `int a[4]` ou `int[4] a` | géré |
| 3, 4 | syntaxe inversée des pointeurs de fonction | forme standard ou Vircon32 | géré |
| 5 | le pointeur nul exige `NULL` | `p = 0;`, `p == 0` | géré |
| 6 | uniquement `void main(void)` | `int main()` et `return 0;` | géré |
| 7 | les paramètres de prototypes exigent des noms | noms facultatifs | géré |
| 8 | pas de `new`/`delete` | `new`/`delete` fonctionnent (via `malloc`) ; `new T[n]` saute les constructeurs de chaque élément | géré, avec réserves |
| 9 | `misc.h` pour `malloc`/`rand`... | `<cstdlib>`, `<cstring>` | géré |
| 10 | pas d'opérateur ternaire | `?:` partout | **abaissé** |
| 11 | paramètres et retours d'un mot | structures et classes par valeur | **abaissé** (les unions de plus d'un mot restent exclues) |
| 12 | pas de retours de pointeur ni de référence | retournez-en librement | **corrigé** |
| 13 | pas de structures vides | autorisées | **corrigé** |
| 14 | `>>` n'étend pas le signe | `-8 >> 1` vaut `-4` ; `unsigned` décale logiquement | **abaissé** (`--no-signed-shift` le désactive) |
| 15 | types avant leur usage | comme en C++ | n/a |
| 16 | pas d'exposant dans les littéraux flottants | `1e3`, `1.5e-3f`, `.5`, `5.` | géré (imprimé en virgule fixe) |
| 17 | peu d'échappements | octal, `\0`, `\x` | géré |
| 18 | pas de déclarations après une étiquette `case` | mettez des accolades au corps du case | **votre code** |
| 19 | tailles de tableau constantes | macros, énumérations, arithmétique constante | géré |
| 21 | pas d'opérateur virgule | `a, b` partout, `for(i=0,j=n;..;i++,j--)` | **abaissé** |
| 22 | un tableau ne dégénère qu'en `=`, initialisation, argument | `arr + n`, `*arr`, `p == arr` | **abaissé** |
| 23 | `const` accompagne une valeur que l'on lit | lisez librement les valeurs const | **abaissé** (pas un rejet de `const T*` vers `T*`) ; l'entrée C omet `const` de la sortie |
| 24 | pas de `static`, `extern`, `volatile`, `unsigned`... | acceptés ; `unsigned` est signé (avertissement) ; les membres `static` de classes et les champs de bits sans nom sont des erreurs | géré, avec réserves |
| 25 | `a op= b` évalue la cible deux fois | n'importe lequel | **abaissé** en instruction ; comme valeur dans une expression plus grande : avertissement |
| 26 | comparaison de pointeur avec `void *` | entrée C : géré | géré |
| 27 | écrire via un pointeur retourné exécute l'appel 2-3 fois | n'importe lequel | **abaissé** en instruction |
| 28 | les codes de contrôle dans les littéraux de chaîne ne s'assemblent pas | n'importe quel littéral, `"\025"` | **abaissé** |

Non couvert par une entrée parce que v32c++ n'a pas de solution : les templates, les
exceptions, la RTTI, l'héritage multiple, `enum class`, `constexpr`, les lambdas,
`mutable`, les classes imbriquées, les membres `static` de classes — voir les
« Limitations connues » du README.

---

## 1. Pas de mot-clé `struct` dans une référence à un type

- **Vircon32 exige** : `struct` uniquement dans une déclaration anticipée
  (`struct Name;`) ou une définition complète (`struct Name { ... };`). Toute autre
  référence — un champ pointeur, un paramètre, une déclaration locale, un cast, un
  `sizeof` — doit être le nom nu (`Name *next;`, jamais `struct Name *next;`). Cela
  vaut dès la première déclaration anticipée, pas seulement une fois la définition
  complète apparue.
- **C standard** : `struct Name` est valide partout où un type est nommé, avec ou sans
  `typedef`.
- **Statut** : Confirmé avec le vrai compilateur (`struct Name n;` donne l'erreur
  « expected '{' » ; la première version d'`emit_vtable_instance` a justement buté sur
  cette erreur — voir DESIGN_NOTES.md).
- **Où** : `print_type()` (codegen.c) est la seule fonction par laquelle passe toute
  émission de référence à un type, précisément pour que cette règle n'ait besoin d'être
  modifiée qu'à un seul endroit. `emit_vtable_struct`, `emit_classes` et
  `emit_forward_declarations` conservent à juste titre `struct` elles-mêmes (elles
  émettent des définitions/déclarations anticipées, pas des références) et continueraient
  de le faire en mode C standard, puisque le C standard accepte lui aussi
  `struct Name { ... };`.
- **Pour un mode C standard** : `print_type` a dû AJOUTER `struct` à une référence de
  classe en mode standard (via `type_to_class`, pour confirmer que le nom est bien une
  classe et non un primitif/typedef), et non l'inverse — la prédiction initiale de cette
  liste (« aucun changement nécessaire... en fait l'entrée la moins chère de toute la
  liste ») s'est avérée fausse une fois la chose réellement construite. La vraie surprise :
  plusieurs AUTRES fonctions construisent à la main le nom de type d'une classe —
  `emit_new_delete_runtime`, `emit_array_new_runtime`, `emit_vtable_struct`,
  `emit_vtable_instance`, `emit_method_prototype` — contournant complètement
  `print_type`, de sorte que corriger seulement `print_type` les a manquées en silence.
  Découvert en exécutant réellement `--target=standard` sur un test avec des classes et en
  lisant la sortie (`struct Shape *v32_new_Shape...` à côté d'un `Shape *self = ...` sans
  préfixe quelques lignes plus bas), et non en redérivant chaque site d'appel à partir du
  plan sur le papier. Corrigé par une nouvelle fonction utilitaire
  `print_class_type_name`, appliquée à chacun de ces sites d'appel — **IMPLÉMENTÉ**.

## 2. Déclarateurs de tableau inversés

- **Vircon32 exige** : `int [8] myarray;` — la longueur avant le nom.
- **C standard** : `int myarray[8];` — la longueur après.
- **Statut** : CONFIRMÉ avec le vrai compilateur — construction, transpilation et
  compilation propres du test utilisant des tableaux (`tests/sample26.c`, rapport de
  Matthew). Les paramètres de fonction (dégénérescence en pointeur) et les listes
  d'initialisation de tableau (`= {1, 2, 3}`), tous deux exclus du périmètre à l'origine,
  sont maintenant AUSSI implémentés (un tour ultérieur) — voir les commentaires de
  grammaire de `var_decl` et `AST_INIT_LIST` dans ast.h. La syntaxe de la VALEUR de la
  liste d'initialisation elle-même (`{1, 2, 3}`) est positionnelle, et non la syntaxe
  désignée du C99 — avec le même raisonnement que le choix de syntaxe de cast de
  l'entrée 4 ci-dessous : moins de morceaux indépendants de syntaxe propre à Vircon32 non
  confirmée sur lesquels se tromper à la fois. Ce morceau précis est NON CONFIRMÉ au
  moment de l'écriture (tout ce tour ultérieur a été écrit sans bison disponible, la même
  limitation que le travail d'origine sur les déclarateurs de tableau) — il a besoin de la
  propre compilation de Matthew, comme tout changement de grammaire de ce projet avant
  d'être tenu pour acquis.
- **Où** : `print_type()` (codegen.c) émet `ElementType [N]` pour un nœud
  `AST_ARRAY_TYPE` ; chaque site d'appel ajoute ensuite le nom de la variable comme pour
  tout autre type, de sorte qu'aucun appelant n'a eu besoin de changer une fois que
  `print_type` gérait le nouveau type. Le `var_decl` de `parser.y` accepte des
  déclarateurs de tableau côté entrée sous LES DEUX formes : celle du C standard
  (longueur après le nom) et une forme alternative dans le style natif de Vircon32
  (longueur avant le nom) — voir l'entrée de suivi des particularités ci-dessous, « Deux
  formes d'entrée acceptées côté C++ ». `param` accepte séparément `int arr[]`/
  `int arr[8]`, dégénérant directement en type pointeur ordinaire à l'analyse (sémantique
  exacte du vrai C/C++) — aucun AST_ARRAY_TYPE n'intervient pour un paramètre, donc aucun
  travail supplémentaire de sema/lower/codegen n'était nécessaire pour ce morceau.
- **Pour un mode C standard** : cette prédiction s'est vérifiée exactement — la propre
  architecture de `print_type` (« préfixe, puis l'appelant ajoute le nom ») ne peut
  réellement pas produire `ElementType name[N]` à elle seule, puisque le crochet doit
  venir APRÈS le nom, pas avant. Corrigé par une nouvelle fonction utilitaire
  `print_array_suffix`, appelée par l'APPELANT qui imprime le nom — trouvée en traçant
  tous les sites d'appel de `print_type` dans codegen.c et en confirmant que seuls deux en
  ont réellement besoin (`print_var_decl_inline`, qui couvre les variables locales comme
  globales ; la boucle des champs de données d'`emit_struct`) — le type de retour d'une
  fonction et le type de chaque paramètre ne peuvent jamais être des tableaux (retourner
  un tableau par valeur est illégal en C/C++ ; la grammaire de `param` elle-même fait
  dégénérer un paramètre tableau en pointeur à l'analyse, de sorte qu'aucun nœud
  `AST_ARRAY_TYPE` n'est jamais construit pour un paramètre). Les tableaux
  multidimensionnels (`AST_ARRAY_TYPE` imbriqués) fonctionnent correctement dans les deux
  modes sans code supplémentaire — **IMPLÉMENTÉ**. La syntaxe de la liste d'initialisation
  elle-même n'a nécessité aucun changement, exactement comme prévu — `{1, 2, 3}` est aussi
  du C standard valide tel quel.

## Deux formes d'entrée acceptées côté C++ pour les tableaux (ce n'est pas en soi une particularité de Vircon32 par rapport au C standard, mais une décision de conception apparentée qu'il vaut mieux suivre au même endroit)

Matthew a demandé si l'entrée côté C++ pouvait accepter SOIT la syntaxe standard des
tableaux (`int scores[8];`) SOIT le style natif de Vircon32 (`int [8] scores;`) comme
entrée valide, précisément pour que quelqu'un déjà à l'aise avec le C de Vircon32 — ou en
transition depuis celui-ci — n'ait jamais à apprendre une seconde convention de
déclarateurs sans rapport s'il ne le souhaite pas, tandis que quelqu'un venant du C++
ordinaire peut simplement écrire ce qu'il connaît déjà. Les deux produisent un
`AST_ARRAY_TYPE` identique ; l'AST ne garde aucun souvenir de la graphie utilisée, et la
sortie est toujours la forme exigée par Vircon32. La faisabilité sans ambiguïté de
grammaire a été confirmée en raisonnant sur l'anticipation LALR(1) à chaque point de
décision (pas encore confirmée par une vraie compilation bison — voir l'entrée sur les
déclarateurs de tableau plus haut). Le cadrage de Matthew lui-même, qui mérite d'être
conservé comme principe permanent pour les travaux de syntaxe à venir et non comme une
remarque ponctuelle : « que le C++ ait l'air normal, quitte à ce que la transpilation
doive ajuster les choses » — étendre la même approche de double acceptation aux
déclarateurs de pointeurs de fonction lorsque ce travail sera fait, plutôt que de le
redécider à chaque fois. Si la double acceptation d'une construction future devait créer
de véritables conflits de grammaire, le repli est la forme du C standard seule, et non la
forme native de Vircon32 — la lisibilité du C++ ordinaire a été jugée plus importante à
protéger comme valeur par défaut.

## 3. Syntaxe de déclarateur de pointeur de fonction inversée

- **Vircon32 exige** : `ReturnType(ParamTypes)* name;` — p. ex.
  `int(Shape *)* Shape__area__void;` pour un emplacement de vtable.
- **C standard** : `ReturnType (*name)(ParamTypes);` — p. ex.
  `int (*Shape__area__void)(Shape *);`.
- **Statut** : Confirmé avec le vrai compilateur (émission des champs de la structure
  vtable, de `tests/sample7.cpp`/`sample14.cpp` en avant).
- **Où** : `emit_vtable_struct` (codegen.c) construit ce déclarateur directement, en
  ligne, au lieu de passer par `print_type` pour le cas précis de l'emplacement de vtable.
  Un tour ultérieur a aussi ajouté un cas `AST_FUNC_PTR_TYPE` à `print_type` lui-même,
  pour la fonctionnalité distincte de déclaration de variables pointeur de fonction côté
  C++ (`int (*fp)(int, int);` et l'écriture propre à Vircon32 `int(int, int)* fp;`) — ce
  cas a la même forme de déclarateur inversé.
- **Pour un mode C standard** : IMPLÉMENTÉ, dans un tour ultérieur. Résolu autrement que
  `print_type` seul n'aurait pu le faire : un nouveau `print_type_and_name` (codegen.c)
  construit le déclarateur ENTIER du mode standard (type de retour, nom, éventuelles
  dimensions de tableau, liste de paramètres) comme une unité autonome, à chacun des deux
  sites d'appel qui peuvent atteindre une déclaration de type pointeur de fonction
  (`print_var_decl_inline`, la boucle des champs d'`emit_struct` — et, par elle,
  `emit_unions` aussi — il a été directement confirmé qu'`ast_wrap_func_ptr` n'est appelée
  que depuis la grammaire de `var_decl`, et que `var_decl` est ce à quoi se réduisent aussi
  bien une déclaration de variable qu'un membre de classe ; aucun paramètre ni type de
  retour ne peut être un type pointeur de fonction dans cette grammaire). La composition de
  tableaux de pointeurs de fonction fonctionne aussi (`ReturnType (*name[N])(Params);`, la
  syntaxe propre du C standard pour cela), confirmée directement avec la sortie de
  `tests/sample66.cpp`. `emit_vtable_struct` et `emit_vtable_instance` (dont le texte de
  déclarateur/cast en ligne ne passe jamais par `print_type`) ont reçu séparément la même
  correction, confirmée avec la sortie de vtable de `tests/sample14.cpp`. Aucun changement
  de grammaire n'était nécessaire — toute la correction vit dans codegen.c.

## 4. Syntaxe de CAST de pointeur de fonction, même motif inversé

- **Vircon32 exige** : `(ReturnType(ParamTypes)*)expr` — qui suit le motif du déclarateur
  anonyme à l'intérieur des parenthèses. P. ex. `(int(Shape *)*)&Square__area__void`.
- **C standard** : `(ReturnType (*)(ParamTypes))expr` — p. ex.
  `(int (*)(Shape *))&Square__area__void`.
- **Statut** : Confirmé avec le vrai compilateur (`tests/sample24.c`, une compilation
  propre de bout en bout — c'était le seul morceau réellement incertain du travail sur
  l'instance de vtable, désormais résolu).
- **Où** : `emit_vtable_instance` (codegen.c), la branche d'insertion de cast pour un
  emplacement de vtable dont la classe déclarante de l'implémentation actuelle diffère de
  celle déclarée canoniquement pour le champ.
- **Pour un mode C standard** : IMPLÉMENTÉ avec le n° 3 ci-dessus, dans la branche
  d'insertion de cast d'`emit_vtable_instance` elle-même — le cast de pointeur de fonction
  du C standard a un `()` VIDE là où irait le nom d'un déclarateur (il n'y a pas de nom à
  donner à un cast), `(ReturnType (*)(Params))expr`, confirmé directement avec la sortie
  de `tests/sample14.cpp`.

## 5. L'initialisation à nul d'un pointeur exige `NULL`, pas `0` — GÉRÉ

- **Vircon32 exige** : `NULL` (ou un cast explicite, `(Type *)0`) pour un pointeur nul. Un
  simple `0` est rejeté (« cannot assign int to ... pointer »).
- **C standard** : `0` et `NULL` sont interchangeables dans un initialiseur/une
  affectation de pointeur.
- **Statut** : Confirmé avec le vrai compilateur (`(Node *) 0` testé directement, selon le
  tour sur le cast du receveur de DESIGN_NOTES.md).
- **Pour le mode Vircon32** : **GÉRÉ** : un 0 littéral affecté à un pointeur, comparé à
  l'un d'eux ou retourné comme tel est imprimé `NULL` (`p = 0;`, `p != 0`, `fn = 0;` ; voir
  le n° 24 pour le côté préprocesseur). `tests/135sample.cpp`.
- **Pour un mode C standard** : émettre un simple `0` serait déjà du C standard correct,
  donc — comme le n° 1 — le mode C standard est la valeur par défaut la plus simple et
  inchangée ; c'est le mode Vircon32 qui a besoin de la substitution.

## 6. `void main(void)`, inconditionnellement, quelle que soit la déclaration du source

- **Vircon32 exige** : le point d'entrée du programme doit être exactement
  `void main(void)` — pas de valeur de retour, jamais, quelle que soit la déclaration du
  source C++ (`int main()`, `void main()` ou autre). `return expr;` à l'intérieur devient
  `expr; return;` (évaluer, jeter, retourner sans valeur).
- **C standard** : `main` retourne conventionnellement `int` (`return 0;` ou un vrai code
  de sortie), même si un `void main()` propre à certains compilateurs est aussi largement
  toléré en pratique.
- **Statut** : Confirmé avec le vrai compilateur (présent depuis le premier tour de
  `-c`/`-o` ; tout test comportant `main` compile proprement).
- **Où** : la passe de décoration des noms de sema.c traite le nom `main` comme un cas
  particulier pour qu'il reste non décoré ; `emit_function_definition` (codegen.c) force le
  type de retour à `void` et applique `strip_return_value` spécifiquement pour lui.
- **Pour un mode C standard** : la décision délibérée que cette liste signalait elle-même
  comme nécessaire a été prise — le mode standard respecte le type de retour que le source
  C++ a réellement déclaré pour `main` (normalement `int`), en conservant les vraies
  instructions `return` au lieu de les supprimer, puisque forcer `void` aurait changé le
  comportement réel du programme (un code de sortie devenant inobservable), et pas
  seulement la syntaxe de surface. Implémenté comme une unique condition
  `force_void_main` (`name == "main" && g_target == TARGET_VIRCON32`), calculée de façon
  identique dans `emit_function_header` et `emit_function_definition` plutôt que passée en
  paramètre, précisément pour que les deux ne puissent jamais diverger séparément sur le
  fait qu'un `main` donné reçoive ou non le traitement « forcer void et supprimer le
  retour » — **IMPLÉMENTÉ**.

## 7. Les prototypes de fonction exigent des noms de paramètres explicites

- **Vircon32 exige** : chaque paramètre d'un PROTOTYPE (pas seulement d'une définition) a
  besoin d'un nom — `void foo(int x, int y);`, pas `void foo(int, int);`.
- **C standard** : les noms de paramètres dans un prototype sont facultatifs.
- **Statut** : Confirmé avec le vrai compilateur (présent depuis le premier tour
  d'émission de prototypes qui fonctionnait).
- **Où** : `emit_function_prototype`/`emit_method_prototype` (codegen.c) impriment déjà
  toujours le nom du paramètre, pris directement dans l'AST — il n'y a jamais eu de chemin
  de code qui l'omette, donc rien de spécifique à commuter ici.
- **Pour un mode C standard** : aucun changement nécessaire — nommer chaque paramètre est
  aussi du C standard valide et ordinaire. Ce n'est en réalité pas une divergence que ce
  projet aurait eu à annuler, seulement une exigence de Vircon32 qui se trouve être déjà un
  sous-ensemble strict de ce que permet le C standard.

## 8. `new`/`delete` n'ont pas d'équivalent natif dans AUCUN des deux dialectes

- Ce n'est pas du tout une divergence entre Vircon32 et le C standard — le C simple (celui
  de Vircon32 ou tout autre) n'a pas de mot-clé `new`/`delete`. La traduction
  `v32_new_*`/`v32_delete` de ce projet (allocation fondée sur `malloc`/`free` plus un
  appel explicite au constructeur) resterait exactement la même dans un mode C standard ;
  seul l'en-tête qui fournit `malloc`/`free` change (n° 9, ci-dessous).
- **Où** : `new_delete_rewrite_expr` de `lower.c`,
  `emit_new_delete_runtime`/`emit_v32_delete` de `codegen.c`.
- **Pour un mode C standard** : aucun changement à la stratégie de traduction elle-même —
  **N/A, confirmé**. Le PRÉFIXE de nom `v32_new_`/`v32_delete` a brièvement été
  reconsidéré lors de la construction de `--target` (un mode C standard voudrait-il un
  préfixe moins tourné vers Vircon32 ?) et délibérément laissé tel quel : c'est une
  convention de nommage interne (comme `__v32_ret_tmp0` et d'autres), pas une question de
  portabilité, et le cadrage de cette entrée le disait déjà.

## 9. `misc.h` est le nom d'en-tête propre à Vircon32 pour `malloc`/`free`/etc.

- **Vircon32 exige** : `#include "misc.h"` pour `malloc`, `free`, `memset`, `memcpy`,
  `rand`, `srand`, `exit` et d'autres.
- **C standard** : `#include <stdlib.h>` (`malloc`/`free`/`rand`/`srand`/`exit`),
  `#include <string.h>` (`memset`/`memcpy`).
- **Statut** : Confirmé — `misc.h` fourni directement (la propre copie de Matthew du code
  source de la bibliothèque standard de Vircon32), et `#include "misc.h"` compile
  proprement de bout en bout (`tests/sample17.c` en avant).
- **Où** : le bloc `needs_misc` de `codegen_run` (codegen.c).
- **Pour un mode C standard** : implémenté comme prévu — simple substitution de chaîne,
  `#include <stdlib.h>` à la place de `"misc.h"` sous la même condition `needs_misc`. Il a
  été directement confirmé (et non supposé) que le code généré par ce projet n'émet
  jamais lui-même d'appel à `memset`/`memcpy`, de sorte que `<string.h>` n'a en fait
  jamais été nécessaire — **IMPLÉMENTÉ**.

---

## 10. Le C de Vircon32 n'a pas d'opérateur ternaire — ABAISSÉ (`?:` est permis dans votre code)

- Ce n'est pas une divergence de forme de déclarateur ou de mot-clé comme toutes les
  entrées précédentes — Matthew a signalé que le vrai compilateur C de Vircon32 rejette
  catégoriquement `cond ? a : b`, sous toutes ses formes. Contrairement aux autres entrées
  de cette liste, il ne s'agit pas ici d'ÉMETTRE la bonne graphie ; il s'agit de ne pas
  pouvoir émettre la construction du tout.
- **C standard** : prend en charge nativement l'opérateur ternaire ; la sortie en mode
  standard conserve `cond ? a : b` exactement tel que le source C++ l'a écrit, sans aucune
  réécriture.

- **Statut** : CONFIRMÉ par une vraie exécution du compilateur Vircon32 : un ternaire
  imbriqué dans un argument d'appel (`add(x > y ? x : y, 1)`) a été transpilé avec les
  caractères littéraux `?`/`:` toujours dedans, et le vrai compilateur C de Vircon32 l'a
  rejeté avec « character '?' is not a valid identifier start » — son propre analyseur
  lexical ne reconnaît même pas le caractère, ce qui confirme un rejet dur et
  inconditionnel, et pas seulement au niveau de l'analyseur syntaxique.

- **Où** : une nouvelle phase d'abaissement dédiée (`lower.c`, phase 10,
  `rewrite_ternary_*`) plutôt qu'une condition dans `codegen.c` — c'est une véritable
  réécriture au niveau de l'AST (expression ternaire → instruction if/else), pas un choix
  d'impression, elle ne pouvait donc pas vivre dans codegen.c comme le fait la correction
  de chacune des autres entrées.
- **Pour le mode Vircon32** : **IMPLÉMENTÉ**, et couvre désormais toute position où un
  ternaire peut apparaître, à deux niveaux. Un ternaire qui est DIRECTEMENT l'initialiseur
  d'un var_decl, DIRECTEMENT le côté droit d'une simple affectation `=` à un identifiant
  nu, ou DIRECTEMENT une expression de retour est réécrit SANS aucune variable temporaire
  (chacune de ces trois formes a déjà un endroit naturel où mettre la valeur). Les
  ternaires chaînés/imbriqués au sein de ce même ensemble de formes
  (`cond1 ? a : cond2 ? b : c`, un motif courant et idiomatique, pas un cas limite rare —
  confirmé par le `classify` de `tests/sample58.cpp`, qui figurait déjà dans la batterie de
  ce projet avant l'existence de cette phase) sont entièrement déroulés par récursion sur
  chaque branche nouvellement construite, pas seulement au niveau le plus externe. Toute
  AUTRE position — à l'intérieur d'un argument d'appel, comme partie d'une expression plus
  grande arithmétique/d'indice/de membre, affectée via autre chose qu'un identifiant nu —
  est maintenant AUSSI gérée, en hissant le ternaire dans sa propre variable temporaire
  nouvellement déclarée (`__v32_tern_tmpN`), renseignée par un if/else ordinaire inséré
  immédiatement avant l'instruction courante ; confirmé de bout en bout avec le
  `add(x > y ? x : y, 1)` de `tests/sample71.cpp`, qui se transpile désormais sans aucun
  caractère `?`/`:` nulle part dans la sortie et, exécuté en mode standard, calcule la
  bonne valeur (11). Deux limites étroites subsistaient, énoncées clairement plutôt que
  passées sous silence : un ternaire dans les clauses d'initialisation/condition/
  incrément d'une boucle for (aucune tentative de restructurer la boucle elle-même, p. ex.
  en un `while` équivalent) ; et un ternaire atteignable seulement via un emplacement
  d'instruction unique sans accolades (`if (cond) foo(cond2 ? a : b);` sans bloc autour) —
  insérer une déclaration temporaire préalable exige une vraie liste d'instructions où la
  greffer, et seul un bloc en fournit une. Voir le commentaire de documentation de
  `hoist_ternaries_in_expr` dans `lower.c` pour le raisonnement complet, y compris la
  raison pour laquelle la forme d'affectation directe a encore besoin d'une vérification
  plus étroite que « toute affectation » (une lvalue arbitraire dupliquée dans les deux
  branches risquerait d'évaluer deux fois un effet de bord qu'elle contient — le hissage
  générique l'évite entièrement, puisqu'il introduit une nouvelle variable temporaire au
  lieu de dupliquer la lvalue d'origine).
- **Mise à jour (20261001-dev) : toutes les positions, avec les règles d'évaluation du
  C++.** La démo TEMPEST 32K a rencontré un `?` qui s'est glissé dans le C généré malgré ce
  qui précède. Un balayage de 31 contextes a trouvé les vraies lacunes : corps de `case`
  d'un switch (jamais visités), conditions de `else if`, les trois clauses de `for`, corps
  sans accolades, instructions étiquetées, listes d'initialisation, et un ternaire utilisé
  comme condition d'un autre ternaire. La phase 10 a été réécrite pour tous les couvrir —
  les deux « limites étroites » ci-dessus ont disparu — et pour conserver les règles
  d'évaluation du C++, que l'ancien hissage ne conservait pas : les branches sont réécrites
  dans leur propre if/else (le côté non pris n'est jamais exécuté, donc `p ? p->x : 0` est
  sûr), un ternaire à droite de `&&`/`||` ne s'exécute que lorsque le côté gauche ne
  tranche pas, et un ternaire dans la condition d'une boucle est réévalué à chaque tour (la
  boucle devient `while (1)` avec le test en tête ; un drapeau de premier passage gère le
  do-while et l'incrément du `for` pour que `continue` garde son sens). Après la phase,
  `check_no_ternaries` garantit qu'aucun `?` n'atteint la sortie : un résidu hors du corps
  d'une fonction est replié s'il s'agit d'une constante entière, sinon signalé comme erreur
  à sa ligne. `tests/97sample.cpp` vérifie tout cela à l'exécution dans l'émulateur.

---

## 11. Les paramètres et les valeurs de retour doivent faire exactement un mot — aucune structure/union/tableau par valeur au-delà — ABAISSÉ pour les structures

- **Mise à jour (20261003)** : n'est plus un avertissement pour les structures et les
  classes. Confirmé avec le vrai compilateur (« functions cannot return values of
  size > 1 », « functions cannot pass arguments of size > 1 »), et **réécrit** en mode
  Vircon32 par la phase 9b de lower.c :
  - une fonction qui retourne une structure de plusieurs mots devient `void` et prend un
    `T *__v32_ret` caché (juste après `this` dans une méthode) ; `return E;` devient
    `*__v32_ret = E; return;`
  - un paramètre de structure de plusieurs mots devient `const T *__v32_byval_name`, et le
    appelé commence par `T name = *__v32_byval_name;`, de sorte qu'il possède toujours sa
    propre copie à modifier ;
  - à l'appel, `T c = f(a);` -> `T c; f(&c, &a);`, `c = f(a);` -> `f(&c, &a);`,
    `return f(a);` -> `f(__v32_ret, &a); return;`. Partout ailleurs, le résultat va dans
    une variable temporaire déclarée en tête de fonction, greffée sous la forme
    `(f(&tmp, &a), tmp)` puis aplatie par l'abaissement de la virgule (n° 21), ce qui garde
    corrects les conditions de boucle et `&&` / `||` ;
  - les types pointeur de fonction (typedefs, variables, membres, paramètres, emplacements
    de vtable) sont réécrits de la même façon. Les noms décorés ne changent pas.
  - Une structure d'UN seul mot continue de voyager nativement, mais `one(42).v` n'est plus
    accepté en aval (« cannot emit memory placement when an expression has none ») ; le
    résultat est stocké au préalable dans une variable temporaire.
  Coût : une copie de structure par paramètre par valeur à chaque appel, et une pour un
  résultat qui n'est pas écrit directement dans sa destination. Non couvert : les unions.
  `tests/101sample.cpp` vérifie tout cela à l'exécution. Le texte ci-dessous est l'entrée
  d'origine.


- **Vircon32 exige** : les paramètres et la valeur de retour d'une fonction doivent chacun
  faire exactement un mot. Une structure, une union ou un tableau de plus d'un mot ne peut
  pas du tout être passé ou retourné PAR VALEUR — il faut utiliser à la place un pointeur
  vers lui (ce qui correspond au choix déjà existant de ce projet pour le receveur de
  chaque méthode, `ClassName *this`, déjà fondé sur un pointeur pour une raison sans
  rapport). Signalé directement par Matthew, citant la documentation même de Vircon32 :
  « functions cannot use parameters or return values of size different from 1. That is:
  they cannot use arrays, unions or structures (unless their size is just a single word).
  Instead they must operate with pointers to them. » Passer un tableau DÉGÉNÉRANT EN
  POINTEUR (`void foo(int arr[8])`, qui est déjà la manière dont la grammaire `param` de ce
  projet traite un paramètre tableau — voir l'entrée n° 2 ci-dessus) est explicitement
  permis, comme en C standard ; la restriction concerne spécifiquement le passage/retour
  d'un AGRÉGAT de taille fixe (structure/union/tableau) comme véritable copie par valeur.
- **C standard** : une telle restriction n'existe pas — une structure, une union ou (via
  une structure enveloppe, puisque le C lui-même n'autorise pas non plus un paramètre ou
  type de retour tableau nu) un tableau ordinaire de n'importe quelle taille peut être
  passé ou retourné par valeur.
- **Statut** : Signalé directement par Matthew, citant la documentation même de Vircon32
  — pas encore confirmé de façon indépendante avec le vrai compilateur en transpilant et
  compilant un paramètre ou type de retour par valeur délibérément surdimensionné pour voir
  l'erreur précise que produit Vircon32, la même lacune de test que l'entrée n° 10
  (ternaire).
- **Où** : `check_word_sizes_classes`/`check_word_sizes_free_functions` de `lower.c`,
  exécutées immédiatement après `compute_struct_layouts` (qui a besoin des nombres de
  champs qu'elle calcule) mais avant toute autre chose dans `lower_run` — une passe qui ne
  fait qu'avertir, pas une transformation, elle n'a donc pas besoin d'un emplacement de
  phase numéroté propre.
- **Pour un mode C standard** : aucun changement nécessaire — cette restriction est propre
  à Vircon32, de même forme que l'entrée n° 1 (« c'est le mode Vircon32 qui a besoin du
  traitement supplémentaire ») ; la vérification elle-même est conditionnée par
  `g_target == TARGET_VIRCON32` à chaque site d'appel.
- **IMPLÉMENTÉ**, une fois que Matthew a confirmé les chiffres que la première version de
  cette entrée disait manquer : Vircon32 est une machine 32 bits fondée sur des mots
  (1 mot = 32 bits), et `int`/`float`/tout pointeur font déjà exactement un mot — et,
  point crucial, `char`/`short`/`double`/etc. (alias des compilateurs récents) ne sont que
  du « sucre syntaxique » sur le même mot de 4 octets sous-jacent, de sorte que TOUT champ
  de N'IMPORTE QUEL type primitif pris en charge fait exactement un mot, sans qu'aucune
  table de tailles par type soit nécessaire. La taille d'une classe/structure en mots est
  donc simplement le nombre de champs de son `StructLayout` (membres de données plus un
  pointeur de vtable, le cas échéant) — `sema_warning` (exposée depuis sema.c, plus
  statique, pour que lower.c puisse réutiliser la même mécanique de comptage/formatage au
  lieu de la dupliquer) se déclenche lorsqu'un paramètre ou type de retour de classe/
  structure nu (ni pointeur ni référence — celui qualifié `const` compte encore) a plus
  d'un de ces champs.
  Il reste une lacune réelle mais étroite : un MEMBRE DE DONNÉES de type tableau ou de type
  structure imbriquée ne contribue que pour UN au décompte de son propre `StructLayout`
  ici, même s'il peut lui-même faire plusieurs mots de large, de sorte qu'une structure
  avec exactement un tel champ pourrait encore être comptée par défaut comme « un mot »
  alors qu'elle en fait en réalité plus — la même direction conservatrice (manquer une
  violation plutôt que d'avertir sur du code valide) que toute autre vérification au mieux
  de ce projet. Les unions ne sont délibérément pas vérifiées du tout : une union ordinaire
  de membres primitifs simples fait déjà exactement un mot par construction (les membres se
  superposent, ils ne s'empilent pas), de sorte que seule une union contenant un membre
  tableau ou structure imbriquée assez grand pour dépasser à lui seul un mot enfreindrait
  cela — encore plus étroit que la lacune des structures ci-dessus, et non poursuivi dans
  ce tour.
- **Une découverte réellement précieuse en testant ceci avec la batterie existante, non
  inventée pour l'occasion** : `tests/sample9.cpp` et `tests/sample15.cpp` (toutes deux
  exercent la surcharge d'opérateurs sur une classe `Vector2D` — `x`/`y`, deux champs
  `int`, deux mots) ont déclenché cet avertissement à répétition. Matthew a demandé qu'elles
  soient corrigées. Corrigé à l'origine seulement PARTIELLEMENT : chaque paramètre
  `Vector2D` prenait `const Vector2D &` au lieu de la valeur, ce qui supprimait cette moitié
  des avertissements, mais le côté du RETOUR ne pouvait pas du tout être corrigé — voir
  l'entrée n° 12 ci-dessous, la lacune réelle, distincte et jusque-là non découverte que
  cette tentative de correction a mise au jour (aucune fonction de toute cette grammaire ne
  pouvait retourner un type pointeur ou référence). MAINTENANT entièrement corrigé, une fois
  l'entrée n° 12 close : les deux échantillons ont été réécrits à nouveau pour retourner
  `Vector2D *` (alloué sur le tas avec `new`) depuis chaque opérateur qui produit une
  valeur, supprimant entièrement l'avertissement de retour par valeur au lieu de se
  contenter de le documenter comme limitation connue.

## 12. Aucune fonction ne peut retourner un type pointeur ou référence — CORRIGÉ

- Ce n'est pas une particularité propre à Vircon32 — c'est une lacune générale, jusque-là
  non découverte, de la grammaire même de ce projet, mise au jour en tentant de corriger
  les constats `sample9`/`sample15` de l'entrée n° 11 en faisant retourner un pointeur,
  plutôt qu'une structure de plusieurs mots par valeur, à un opérateur surchargé qui
  produit une valeur.
- **Confirmé directement** : `int *getPtr(int x) { return &x; }` et `int &getRef();` (dans
  une classe) échouaient tous deux à l'analyse — la grammaire de `func_header`
  (`type_spec func_name '(' ...`) n'avait aucun `pointer_opt` entre le type de retour et le
  nom de la fonction, contrairement à `var_decl`/`param`, qui en avaient déjà un chacun.
  Cela s'appliquait à TOUTE fonction de cette grammaire, pas seulement aux opérateurs — un
  `Shape *makeShape()` tout à fait ordinaire échouait de la même façon. `out_of_line_def`
  avait la même lacune dans sa propre production de grammaire, séparément.
- **Correction** : ajout de `pointer_opt` aux premières alternatives de `func_header` et
  d'`out_of_line_def`, enveloppant le type de retour via `ast_wrap_pointer`/
  `ast_wrap_reference` exactement comme `var_decl` le fait déjà. `%expect` a en réalité
  BAISSÉ, de 26 à 25 — vérifié par une vraie exécution de `bison -d`/`bison -v` (non
  supposé) : donner à `out_of_line_def` son propre `pointer_opt` juste après `type_spec`,
  comme `var_decl`, signifie que les deux productions s'accordent maintenant pour décaler à
  travers `pointer_opt` d'abord, éliminant une fourche décalage/réduction antérieure entre
  elles au lieu d'en ajouter une nouvelle. Voir le commentaire sur `%expect` dans
  `parser.y` pour le raisonnement complet vérifié avec bison.
- **Abaissement, en trois parties**, toutes dans `lower.c` :
  1. Un RETOUR par référence a besoin du même traitement implicite « prendre l'adresse »
     qu'un paramètre par RÉFÉRENCE reçoit déjà à son site d'appel —
     `inject_reference_return_address_stmt` enveloppe `return expr` dans
     `address_of_if_needed` chaque fois que le type de retour de la fonction englobante est
     `AST_REFERENCE_TYPE`, exécutée en phase 3/4 (avant que la phase 5 ne réétiquette
     `AST_REFERENCE_TYPE` en `AST_POINTER_TYPE`, la même contrainte d'ordre que celle déjà
     documentée par la correction existante des paramètres par référence).
  2. `fix_references_in_method`/`fix_references_free_functions` (phase 5) réétiquettent
     maintenant aussi le type de retour de la FONCTION elle-même de `AST_REFERENCE_TYPE` en
     `AST_POINTER_TYPE` — auparavant cette phase ne touchait que les types de paramètres/
     variables locales, jamais le type de retour de la fonction elle-même.
  3. Un APPEL qui retourne une référence reçoit maintenant un déréférencement explicite
     inséré à son point d'usage (le cas `AST_CALL` de `finalize_calls_expr`) — le cas
     symétrique de (1) : un retour par référence du C++ se comporte comme le référent
     lui-même, de sorte que `int x = obj.getRef();` a besoin d'un `*` inséré autour de
     l'appel (qui retourne maintenant un pointeur), sans quoi il affecterait l'adresse au
     lieu de la valeur. Découvert en compilant réellement le code généré, non deviné (voir
     ci-dessous).
- **Deux bogues adjacents, préexistants, trouvés et corrigés en chemin**, tous deux mis au
  jour seulement lorsque ce tour a enfin disposé d'une chaîne d'outils bison+flex
  fonctionnelle pour COMPILER réellement la sortie générée, et pas seulement vérifier
  qu'elle s'analyse :
  - `address_of_if_needed` ne reconnaissait que `AST_POINTER_TYPE` comme « déjà de type
    pointeur, ne pas ajouter `&` » — pas `AST_REFERENCE_TYPE`. Comme la phase 3/4 s'exécute
    avant que la phase 5 ne réétiquette les références en pointeurs, un paramètre par
    RÉFÉRENCE nu transmis comme autre argument de type référence (p. ex.
    `addThem(const Vector2D &a, const Vector2D &b)` calculant `a + b`, ce qui se résout en
    `operator+(const Vector2D &other)`) recevait à tort un `&` supplémentaire, produisant un
    double pointeur (`const Vector2D **` là où `const Vector2D *` était attendu) — une vraie
    erreur de compilation, découverte en construisant réellement le C généré de
    `tests/sample15.cpp`. Corrigé en traitant aussi `AST_REFERENCE_TYPE` comme « déjà lié
    à un pointeur » dans `address_of_if_needed`.
  - Un reste de cette même enquête, resté ouvert à l'époque — **corrigé depuis, dans un
    tour ultérieur, après qu'une vraie exécution du compilateur Vircon32 l'a transformé
    d'un avertissement gcc en un véritable rapport d'erreur** : ce projet ne MODÉLISE
    toujours pas la const-correction sur le receveur `this` d'une méthode (il n'y a pas
    d'erreur pour l'appel d'une méthode non const via une référence const au départ — voir
    l'entrée sur `const` dans la propre liste « ce qui n'existe pas encore » du README),
    mais le transfert d'un `const Shape &`/`const Shape *` comme receveur d'une méthode non
    const reçoit maintenant un cast explicite retirant le const (`Shape__area__void((Shape
    *)s)`) au lieu d'une simple affectation de pointeur sans cast. gcc ne donnait à cela
    qu'un AVERTISSEMENT `-Wdiscarded-qualifiers` (`const struct Vector2D *` passé là où un
    simple `struct Vector2D *this` est attendu) ; le vrai compilateur de Vircon32 le rejette
    CATÉGORIQUEMENT (« cannot assign const struct Shape* to struct Shape*: discards const
    qualifier »), une erreur de type dure, confirmée directement avec le
    `getArea(const Shape &s) { return s.area(); }` de `tests/sample68.cpp`. Corrigé dans
    `cast_receiver_if_needed` (`lower.c`) : le cast que cette fonction insérait déjà pour
    une DISCORDANCE classe de base/dérivée est maintenant AUSSI inséré chaque fois que
    l'objet transmis est qualifié const alors que le `this` injecté de la méthode cible ne
    l'est pas (une méthode const recevant un objet const n'a besoin d'aucun cast — les deux
    côtés concordent déjà, vérifié via le type du paramètre `this` de la cible,
    `receiver_type_is_const`). C'est la correction honnête étant donné le choix délibéré de
    ce projet de ne pas imposer la const-correction — elle fait réellement COMPILER le cas
    permis mais non vérifié, comme le ferait un `const_cast` explicite en vrai C++, au lieu
    de commencer à rejeter du code que ce projet n'a jamais rejeté auparavant.
- **Vérifié de bout en bout**, pas seulement contrôlé à l'analyse : avec une vraie chaîne
  d'outils bison+flex construite à partir des sources dans le propre bac à sable de ce tour
  (flex n'y était pas disponible auparavant ; voir `docs/DESIGN_NOTES.md`),
  `tests/sample9.cpp`, `sample15.cpp` et le nouveau `tests/sample72.cpp` dédié (un test de
  retour de pointeur/référence hors opérateurs : une méthode qui retourne un pointeur, dans
  la classe et hors d'elle, une fonction libre qui retourne un pointeur et une méthode qui
  retourne une référence) se transpilent proprement, et leur C généré compile avec un simple
  `gcc` sans aucune erreur. `sample72.cpp` a de plus été compilé et EXÉCUTÉ directement
  (`--target=standard`), produisant exactement la sortie attendue
  (`viaPointerMethod=7 viaFreeFunction=7 viaReference=7`).
- **Où** : `func_header`/`out_of_line_def` (`parser.y`) ;
  `inject_reference_return_address_stmt`, `finalize_calls_in_method`,
  `finalize_calls_free_functions`, `fix_references_in_method`,
  `fix_references_free_functions`, le cas `AST_CALL` de `finalize_calls_expr` et
  `address_of_if_needed` (`lower.c`).
- **Impact sur l'entrée n° 11 ci-dessus** : cela clôt la raison pour laquelle
  `sample9`/`sample15` n'avaient pu être corrigés que partiellement — elles retournent
  maintenant toutes deux `Vector2D *` (alloué sur le tas avec `new`) au lieu d'une
  structure de plusieurs mots par valeur, évitant entièrement l'avertissement sur la taille
  de mot, et toutes deux sont réécrites en ce sens dans le cadre de cette correction.

---

## 13. Les structures vides sont rejetées catégoriquement — CORRIGÉ

- **Vircon32 exige** : que toute définition de `struct` ait au moins un membre. Une classe
  sans membre de données et sans vtable propre — une classe « verbe » sans état avec
  uniquement des méthodes, comme une classe accesseur/auxiliaire à laquelle on accorde
  `friend` (le `BoxPrinter` de `tests/79sample.cpp`) — est sinon abaissée en une
  `struct BoxPrinter {\n};\n` véritablement vide.
- **C standard** : une structure vide est une extension silencieuse bien connue de gcc —
  acceptée sans aucun avertissement même avec `-Wall -Wextra`, ce qui est exactement la
  raison pour laquelle les balayages de vérification `--target=standard` + gcc de ce projet
  ne l'ont jamais détectée ; seule une vraie exécution du compilateur Vircon32 l'a fait.
- **Statut** : Signalé directement par l'utilisateur, citant mot pour mot l'erreur même du
  vrai compilateur : `structures must have at least 1 member`.
- **Où** : `emit_struct` (`codegen.c`).
- **Correction** : lorsque le `StructLayout` d'une classe a zéro champ (ni membres de
  données, ni pointeur de vtable), `emit_struct` insère un unique champ de remplissage
  inutilisé `char __v32_empty_struct_pad;` — jamais référencé par son nom ailleurs dans le
  code généré, présent uniquement pour donner un membre à la structure. Appliqué dans LES
  DEUX dialectes cibles, et pas seulement au mode Vircon32 : une structure valide dans l'un
  des modes de sortie de ce projet mais pas dans l'autre, pour une différence que
  l'utilisateur n'a jamais écrite, serait une divergence déroutante et purement accidentelle
  entre les deux.
- **IMPLÉMENTÉ** pour `--target=vircon32` comme pour `--target=standard`.

---

## 14. Le décalage à droite n'étend pas le signe des valeurs négatives — ABAISSÉ

- **C de Vircon32** : `>>` est un décalage logique. Le CPU n'a qu'une seule instruction de
  décalage, `SHL`, et un compte négatif décale vers la droite sans extension de signe, de
  sorte que `-8 >> 1` vaut `0x7FFFFFFC`, et non `-4`. Le C standard laisse `>>` sur un
  `int` négatif défini par l'implémentation, mais gcc, clang et tout compilateur avec lequel
  on teste un programme décalent arithmétiquement.
- **Pour le mode Vircon32** : **ABAISSÉ** (20261016, lower.c phase 11, `v32_signed_shift`).
  Un `>>` ou `>>=` sur une valeur SIGNÉE devient

  ```c
  x >> n    ->   ((x ^ s) >> n) ^ s        s = -(x >> 31)     // 0, or -1 if x < 0
  ```

  Pour un x négatif, `~x` est non négatif, donc le décaler logiquement EST arithmétique, et
  complémenter le résultat donne `floor(x / 2^n)`. n = 0 fonctionne ; comme en C, n doit
  valoir de 0 à 31. x est écrit trois fois, il n'est donc écrit développé que lorsque c'est
  une variable, un membre, un `*p` ou un élément à indice simple (pas d'appels, pas de
  `++`) ; tout autre opérande passe par un appel à `__v32_sar(x, n)`, une petite fonction
  ajoutée en tête du C généré lorsqu'elle est nécessaire. `x >>= n` devient
  `x = <ce qui précède>` (le C de Vircon32 évalue lui-même deux fois une cible composée,
  voir le n° 25 ; une cible à effet de bord a été extraite de l'instruction avant cela).
- **Ce qui reste un simple `>>` d'une instruction** :
  - un opérande `unsigned`, que le C décale lui aussi logiquement. C'est à cela que sert
    maintenant `unsigned` : le type s'en souvient (`AstNode.is_unsigned`, positionné par
    l'analyseur lexical pour `unsigned`, un suffixe `u`, ou un littéral supérieur à
    INT_MAX ; à travers les typedefs, les casts et l'arithmétique dessus), même s'il est
    imprimé comme `int` ;
  - une valeur connue pour être non négative : un littéral, `x & 0xFF`, une comparaison ;
  - un décalage masqué par une petite constante positive, `(x >> 4) & 15`,
    `(state >> 16) & 32767` : les bits qu'un décalage arithmétique remplirait sont de
    nouveau masqués ;
  - `x >> 0` (simplement x), et deux constantes, qui sont repliées (`-8 >> 1` s'écrit
    `(-4)`).
- **Coût** : environ cinq instructions au lieu d'une pour un opérande variable ; un appel
  pour tout le reste. Dans une boucle chaude de calcul en virgule fixe, déclarez la valeur
  `unsigned` si elle n'est jamais négative, ou construisez avec `--no-signed-shift` pour
  imprimer chaque `>>` tel qu'il est écrit (un opérande négatif décale alors
  logiquement, comme avant ; `tests/137sample.cpp`).
- `<`, `>`, `<=`, `>=`, `/` et `%` d'`unsigned` sont aussi réécrits (inversion du bit de
  signe, aides `__v32_udiv`/`__v32_umod`) ; `--no-unsigned-ops` le désactive
  (`tests/167sample.c`). **Non fait** : la conversion d'`unsigned` en flottant au-delà
  d'INT_MAX le lit encore comme négatif (l'analyseur lexical avertit une fois).
- Cible standard : rien à faire. `tests/136sample.cpp` (C++), `tests/138sample.c` (C) et
  `tests/137sample.cpp` (l'option) vérifient tout cela dans l'émulateur, y compris les
  valeurs impaires négatives, INT_MIN, un effet de bord dans la cible et les opérandes
  `unsigned`.

---

## 15. Les types doivent être déclarés avant leur usage (comme en C++)

les types doivent être déclarés avant leur usage — une énumération utilisée avant sa
déclaration échoue avec une erreur trompeuse « expecting COLONCOLON », parce que la
classification TYPE_NAME de l'analyseur lexical dépend de l'ordre d'enregistrement. » Un
diagnostic sema/lexical plus aimable (p. ex. « GameDifficulty used before declaration »)
rendrait cela beaucoup plus facile à repérer que l'erreur de l'analyseur syntaxique.

---

## 16. Littéraux flottants : pas de forme exponentielle, et un nombre entier a besoin de son `.0` — CORRIGÉ

- **C de Vircon32** : rejette catégoriquement la notation exponentielle — `1e8`, `1.0e8` et
  `1.5e-3` échouent tous avec « bad floating point literal » (confirmé avec le vrai
  compilateur). `5.` et `0.0015` sont acceptés ; `.5` ne l'est pas.
- **Le bogue que cela a révélé dans v32c++** : les littéraux flottants étaient imprimés avec
  `%.17g`, qui (a) supprime le point décimal des nombres entiers, de sorte que `t / 34.0`
  sortait comme la division ENTIÈRE `t / 34` — compilait proprement et produisait en
  silence des valeurs fausses (le recul de caméra de TEMPEST 32K qui ne bougeait jamais), et
  touchait aussi le `value / 2147483647.0` de `v32/math.hpp` — et (b) bascule en forme
  exponentielle en dessous de 1e-4, que l'analyseur lexical de Vircon32 rejette alors.
- **Graphies en entrée (20261016)** : l'analyseur lexical accepte toutes les formes
  flottantes du C — `1e3`, `1e3f`, `5.`, `.5`, `1.e3`, `2E+2`, `3.14f`. (Jusque-là seul
  `chiffres.chiffres` était reconnu : `1e3` et `.5` étaient des erreurs de syntaxe.)
  `tests/135sample.cpp`.
- **Pour le mode Vircon32 (et standard)** : **CORRIGÉ** — le `format_float_literal` de
  `codegen.c` imprime en virgule fixe avec le moins de décimales qui se relisent comme la
  même valeur (`34.0`, `0.1`, `0.00001`), jamais d'exposant.

---

## 17. Échappements : pas d'octal, pas de `\0`, `\x` prend exactement deux chiffres — CORRIGÉ

- **C de Vircon32** : son analyseur lexical connaît `\n \r \t \\ \' \"` et `\xHH` avec
  EXACTEMENT deux chiffres hexadécimaux. Tout le reste — y compris `\0` et tout échappement
  octal — provoque « unknown escape character » et se décode comme le caractère lui-même :
  `'\0'` devient `'0'` (48), de sorte qu'un test `c != '\0'` compare avec la mauvaise
  valeur.
- **Pour le mode Vircon32** : **CORRIGÉ** — les littéraux de caractère impriment `'\x00'`
  pour NUL (et `\xHH` pour tout autre non imprimable) ; dans les littéraux de chaîne tout
  échappement hors de cet ensemble est réécrit en `\xHH` (`print_vircon32_string`). Le côté
  C++ décode aussi correctement les échappements hexadécimaux et octaux maintenant
  (`ast_decode_escape`) : `'\x41'` se lisait auparavant comme `'x'`.

---

## 18. Pas de déclarations dans le corps d'un switch après une étiquette case — mettez des accolades au corps du case

- **C de Vircon32** : « variables cannot be declared in a switch after case/default are
  used » — toute déclaration au niveau supérieur du corps d'un switch après la première
  étiquette, avec ou sans initialisation (confirmé avec le vrai compilateur). À l'intérieur
  d'un bloc `{ }` imbriqué dans le case, c'est bon.
- **Pour le mode Vircon32** : les variables temporaires que v32c++ introduit lui-même dans
  le corps d'un case (le `__v32_tern_tmpN` de la réécriture du ternaire) sont enveloppées
  dans leur propre bloc. Une déclaration écrite directement dans le corps d'un case reste
  une erreur du programme lui-même ; mettez des accolades au corps du case.

---

## 19. Les tailles de tableau peuvent être des expressions constantes

- **C de Vircon32** : accepte les macros, les constantes d'énumération et l'arithmétique
  constante dans la dimension d'un tableau : `int[ M * 2 ] b;`, `int[ N2 ] c;`,
  `int[ B ] d;` compilent tous (confirmé avec le vrai compilateur).
- **Pour le mode Vircon32** : v32c++ replie la dimension en sa valeur à l'analyse (tout ce
  qui suit a besoin du nombre) et la réimprime telle qu'écrite lorsqu'il ne s'agit que de
  littéraux, de constantes nommées et d'arithmétique (`int [MAX] a;`,
  `int [(ROWS * COLS)] flat;`) ; une dimension qui nomme une constante d'énumération
  imprime sa valeur, puisque le nom C d'un énumérateur n'est pas garanti de correspondre.

---

## 20. PAS une particularité : les globales sont inscriptibles, et les valeurs const peuvent être lues

- Consigné ici parce que les notes de la démo TEMPEST 32K (et le texte de limitations
  construit à partir d'elles) disaient le contraire : que les variables de niveau fichier
  sont de la ROM en lecture seule à moins d'être déclarées avec le mot-clé `global` de
  Vircon32, et que lire un `const` dans une variable simple échoue avec « discards const
  qualifier ».
- **Confirmé avec le vrai compilateur (v26.04.24)** : ni l'un ni l'autre n'est vrai.
  `int counter = 5; ... counter += 10;` et un `int[ 4 ] table;` de niveau fichier écrit à
  l'exécution fonctionnent tous deux (les globales vivent en RAM, initialisées au
  démarrage), et `const int K = 7; int x = K;` compile et lit 7. Les échantillons
  autotestés en dépendent : `test_errors` est une globale dans laquelle le programme écrit.
  (Affecter un `const T*` à un simple `T*` est une autre question et peut encore être
  rejeté.)
- **Plus étroit qu'il n'y paraissait (20261003)** : ce test lisait le const dans un
  INITIALISEUR, qui est la seule position où cela fonctionne. `x = K;`, `f( K )` et
  `return K;` sont tous rejetés — voir le n° 23.

---

## 21. Il n'existe pas d'opérateur virgule — ABAISSÉ

- **C de Vircon32** : pas d'opérateur virgule (`a, b` comme une seule expression).
- **Pour le mode Vircon32** : **ABAISSÉ** (20261001-dev), dans la même phase que la
  réécriture du ternaire (n° 10) : le côté gauche devient sa propre instruction, exécutée
  en premier, et l'expression est le côté droit. Une boucle dont la condition ou
  l'incrément en utilise un devient `while (1)` avec le test en tête et un drapeau de
  premier passage qui exécute l'incrément, de sorte que `continue` garde son sens
  (`for (i = 0, j = n; i < j; i++, j--)`). Le mode standard l'imprime tel qu'écrit.
  `tests/97sample.cpp` le vérifie à l'exécution.
- Apparenté : `for (int i = 0, j = 5; ...)` (deux déclarations, pas l'opérateur virgule)
  devient un bloc contenant les déclarations, autour d'une boucle à clause d'initialisation
  vide — même portée, du C simple dans les deux cas.

---

## 22. Un tableau n'est un pointeur que dans une affectation, un initialiseur ou un argument

- **C standard** : un tableau dégénère en pointeur vers son premier élément presque
  partout, de sorte que `arr + n`, `p - arr`, `p == arr`, `*arr` et `(int)arr` sont toutes
  des expressions de pointeur.
- **C de Vircon32** : il ne dégénère que dans `p = arr;`, `T* p = arr;` et `f( arr )`. En
  tant qu'OPÉRANDE, le tableau reste un tableau : `g_actors + MAX_ACTORS` -> « invalid
  operands for addition » (suivi de « cannot assign int to struct Actor* »),
  `it - g_actors` -> « invalid operands for subtraction », `p == arr` -> « invalid operands
  for equality comparison », `(int)arr` -> « cannot convert expression type from
  struct S[8] to int », `*arr` -> « dereference can only be applied to pointers ».
- **Ce n'est pas la particularité qu'elle semblait être au début** : l'arithmétique de
  pointeurs elle-même fonctionne. `p + 2`, `2 + p`, `p - 1`, `p += 2`, `q - p`, `p < q`,
  `&p[ n ]` compilent tous, pour `int*` comme pour les pointeurs vers structures. Seul
  l'opérande tableau pose problème (découvert par demos/c/spyvsspy, dont le
  `Actor* end = g_actors + MAX_ACTORS;` avait d'abord été interprété comme « pas
  d'arithmétique de pointeurs »).
- **Pour le mode Vircon32** : **ABAISSÉ** (lower.c phase 11). Un opérande de type tableau
  de `+ - == != < > <= >=`, d'un cast ou d'un `*` unaire est réécrit en `&arr[0]`.
  `sizeof arr` est laissé intact. Vérifié à l'exécution par `tests/100sample.cpp`.

---

## 23. `const` accompagne une valeur que l'on se contente de lire

- **C standard** : copier un objet const produit une valeur simple ; `const` ne restreint
  que l'écriture dans l'objet lui-même.
- **C de Vircon32** : « cannot assign const int to int: discards const qualifier » pour une
  valeur const dans une affectation (`n = c;`), comme argument d'appel (`f( c )`), dans un
  `return c;`, et sous le moins unaire (`n = -c;`). De même pour tout ce qui est lu À
  TRAVERS un const : `n = cp->x;`, `n = cip[ 1 ];`, `n = *cip;`, `s = *cp;` (une structure
  entière). Tous les types scalaires, et les pointeurs stockés dans une structure const.
- Accepté tel quel : un initialiseur (`int n = c;`, `{ c, c }`), une affectation composée
  (`n += c;`), toute expression binaire (`n = c + 1`, `c * c`), une condition (`if( c )`),
  un indice (`a[ c ]`), et la conversion vers un autre type (`float f = c;`).
- Un cast explicite vers le propre type de la valeur est accepté partout : `n = (int)c;`,
  `q = (S*)cp->next;`.
- **Pour le mode Vircon32** : **ABAISSÉ** (lower.c phase 11, qui s'exécute après la
  réécriture du ternaire afin que ses variables temporaires soient couvertes). Du côté
  droit d'une affectation, dans un argument d'appel et dans une valeur de retour, la lecture
  d'un scalaire ou pointeur const est enveloppée dans un cast vers son propre type non
  qualifié ; une structure const est lue comme `*((S *)&expr)`. Un rejet authentique sur un
  POINTEUR (`S* q = cp;` avec `const S* cp`) est laissé au compilateur. `strip_const_member_read`
  (phase 3) est la version antérieure, réservée aux membres, de la même correction et
  s'exécute toujours. `tests/100sample.cpp`.

---

## 24. Pas de `static`, `extern`, `volatile`, `unsigned`/`signed`/`short`/`long` — acceptés, réécrits

- **C de Vircon32** : aucun de ces mots-clés n'existe (« identifier "static" has not been
  declared ») ; un `extern int x;` sans définition est « declared but not fully defined ».
  Il n'y a qu'un seul type entier.
- **Pour le mode Vircon32** (tout dans l'analyseur lexical plus deux réécritures après
  l'analyse dans ast.c, de sorte que rien en aval ne les voit) :
  - `static` au niveau fichier et tout `extern`, `volatile` : supprimés. Deux déclarations
    de niveau fichier d'une variable (`extern int x;` ... `int x = 5;`) sont fusionnées en
    une seule ;
  - une variable LOCALE `static` devient une variable de niveau fichier nommée
    `__static<N>_<function>_<name>`, déclarée avant sa fonction, chaque usage étant renommé
    (en respectant le masquage) ;
  - `static` dans le corps d'une classe reste une erreur (pas de membres statiques) ;
  - `signed`, `short`, `long`, `long long` et leurs combinaisons avec `int`/`char` valent
    `int`. `unsigned` aussi, avec un avertissement (une fois par exécution) : les valeurs
    au-delà d'INT_MAX, et `/`, `%`, les comparaisons sur elles, se font en non signé
    (voir le n° 14 ; `--no-unsigned-ops` l'empêche). INT_MIN et `0x80000000` sont imprimés
    `(-2147483647 - 1)`, puisque le C de Vircon32 rejette le littéral 2147483648.
    `4000000000u` conserve son motif de 32 bits.
- Également accepté maintenant, transmis tel quel (le C de Vircon32 l'a) : `%=`, les
  initialiseurs de structures et à accolades imbriquées, `int a, b[ 4 ];`. `int t[] = { ... }`
  tire sa longueur de l'initialiseur (le C de Vircon32 n'a pas `int[] t`), et `T* const p`
  est émis comme `T* p`.
- `NULL` est un MOT-CLÉ du C de Vircon32, de sorte que `#ifndef NULL / #define NULL 0`
  prenait autrefois sa branche et empoisonnait la sortie ; `NULL` est maintenant prédéfini
  pour `#ifdef`. `fn = 0;` / `fn != 0` sur un pointeur de fonction reçoivent la même
  réécriture 0 -> NULL que les pointeurs de données avaient déjà.
- **Champs de bits** (`int level : 6;`) : le C de Vircon32 n'en a pas. Ils sont acceptés
  avec un avertissement et stockés comme un membre ordinaire d'un mot entier, la largeur
  étant ignorée — la structure est plus grande et non compactée, et une valeur hors plage
  n'est pas tronquée. `--reject-bit-fields` en fait une erreur. Les champs de bits sans nom
  et de largeur nulle ne sont pas analysés.
- Un nom de fonction nu passé en ARGUMENT (`apply( add, a, b )`) reçoit maintenant le `&`
  qu'exige le C de Vircon32, comme les affectations et les initialiseurs le recevaient déjà.

## 25. Une affectation composée évalue sa cible deux fois

Le C de Vircon32 compile `target op= value` en `target = target op value`, en écrivant la
cible deux fois. Un effet de bord dans la cible se produit donc deux fois. Confirmé avec le
vrai compilateur (v26.04.24) :

```c
int[4] buf;  int* p = buf;
*(p++) ^= 16;      // reads buf[0], stores into buf[1], p ends 2 further on
rooms[rnd_room()].r_flags |= ISGONE;   // reads one random room, writes another
```

Découvert via Rogue : le décodeur de son fichier de sauvegarde (`*start++ ^= key`)
produisait des données illisibles, et son générateur de niveaux marquait comme « disparues »
les mauvaises salles.

**Ce que fait v32c++** (`lower.c`, phase 11, pour l'entrée C comme C++) : lorsque
l'affectation composée est une instruction à elle seule et que sa cible a un effet de bord
(`++`, `--`, une affectation, un appel — un `operator[]` surchargé compte), l'adresse de la
cible est prise une seule fois :

```c
{ int * __v32_target0 = (&rooms[rnd_room()].r_flags); ((*__v32_target0) |= 2); }
```

ou, lorsque le type de la cible est inconnu, les `++`/`--` de variables simples en sont
extraits (`{ *p ^= key; p++; }`). Une affectation composée avec une telle cible utilisée
comme VALEUR au sein d'une expression plus grande est laissée telle qu'écrite, avec un
avertissement.

## 26. Un pointeur ne se compare pas à un `void *`

`thing == ptr` avec `THING *thing` et `void *ptr` donne « invalid operands for equality
comparison » ; le C le permet. Pour l'entrée C, les deux côtés sont comparés comme des
adresses (`((int)thing) == ((int)ptr)`). Voir aussi `docs/C_INPUT.fr.md` pour le pointeur
nul, qui est un sujet plus vaste.

---

## 27. Écrire à travers un pointeur retourné par un appel exécute l'appel plus d'une fois

Le C de Vircon32 (v26.04.24) évalue plus d'une fois un appel qui produit le pointeur à
travers lequel écrit une affectation, et fait de même avec un accès de membre écrit
`(*call()).m`, même lorsqu'il ne fait que lire. Confirmé en C de Vircon32 simple, en
comptant les appels (découvert le 20261009 lors de l'ajout d'`operator++`) :

```c
*geti() = 5;          // geti() runs 2 times
*geti() += 1;         // 3
(*geti())++;          // 2
get()->m = 7;         // 2
get()->m += 1;        // 3
(*get()).m = 7;       // 3
r = (*get()).n;       // 2  -- a read
r = get()->n;         // 1
r = *geti();          // 1
buf[idx()] = 3;       // 1  -- subscripts are fine
*p++ = 7;             // p advances once -- fine
```

Le C++ rend cette forme courante : un appel à toute fonction ou tout opérateur qui retourne
une référence est `*call(...)` dans le C généré, de sorte que `v[i] = 5` via un
`int &operator[]` utilisateur, `++it` (qui retourne `*this`), `obj.ref() = x` et
`list[i].hp = 0` en font tous les frais — un effet de bord dans l'appel (un compteur, un
conteneur qui grandit paresseusement) se produisait deux fois, et chaque accès de ce genre
coûtait deux ou trois appels.

**Ce que fait v32c++** (mode Vircon32) :

- **lectures** : codegen imprime `(*E).m` sous la forme `(E)->m` — identique en C, et évalué
  une seule fois (`codegen.c`, `AST_MEMBER`) ;
- **écritures** : la phase 11 d'abaissement (`v32_hoist_called_pointer`, `lower.c`) stocke
  d'abord le pointeur lorsqu'une instruction d'expression affecte, affecte de façon composée
  ou fait `++`/`--` à travers un `*` ou un `->` dont le pointeur contient un appel :

  ```c
  get()->m = 7;   ->   { S * __v32_ptr0 = get(); (__v32_ptr0->m = 7); }
  ```

  Lorsque le type du pointeur ne peut pas être déterminé, il est laissé tel qu'écrit avec un
  avertissement. Comme pour le n° 25, une affectation utilisée comme VALEUR au sein d'une
  expression plus grande n'est pas réécrite.

`tests/124sample.cpp` compte les appels pour chaque forme.

## 28. Les codes de contrôle dans un littéral de chaîne ne peuvent pas être assemblés — ABAISSÉ

- **C de Vircon32** (`compile`, v26.04.24) : `EscapeCCharacter` n'échappe que les octets à
  partir de 0x80 (sous la forme `\xHH`), NUL, `\ " '`, et `\n \r \t`. Tout autre code de
  contrôle — 1 à 8, 11, 12, 14 à 31 et 127 — va brut dans le texte du `.asm`, et
  `assemble` s'arrête avec « character is not valid (non-printable ASCII) ». Le C écrit à la
  main y bute de la même façon : `"\x15"` compile puis échoue à l'assemblage. L'assembleur
  accepte lui-même `\xHH` dans une chaîne, et un initialiseur de tableau d'`int`/`char`
  avec les mêmes codes compile et s'assemble sans problème ; seul l'échappement du
  compilateur manque. Les codes 14 à 31 sont les caractères de tracé de cadres et
  d'ombrage de la police du BIOS, c'est donc ce qui les empêche d'être dans le texte (voir
  `v32::Keyboard::glyph`).
- **Ce que fait v32c++** (mode Vircon32, entrée C++) : la phase d'abaissement 12b
  (`cxx_hoist_strings`, `lower.c`) déplace chaque littéral de ce type vers un tableau
  global caché et utilise celui-ci à la place, de sorte que le source C++ reste un littéral
  ordinaire :

  ```c
  puts( "\025\025ok" );   ->   char __v32_str0[ 5 ] = { 21, 21, 111, 107, 0 };
                                puts( &__v32_str0[ 0 ] );
  ```

  Un initialiseur `char s[] = "..."` devient la même liste sur place. Les littéraux
  identiques partagent un tableau (`__v32_strN`, déclaré avant le programme). Les littéraux
  que le compilateur sait écrire (tout le reste, lettres accentuées comprises) sont laissés
  intacts, de sorte que les programmes ordinaires ne changent pas. Non réécrits : un
  littéral dans `sizeof` (sa valeur ne change pas) et l'`asm` en ligne. L'entrée C a la
  même réécriture, en phase 12 (`c_hoist_string`). Un littéral qui est l'initialiseur d'un
  pointeur GLOBAL (`const char *p = "..."`) n'est de toute façon pas pris en charge par le
  compilateur Vircon32.
- **Cible C standard** : rien à faire ; tout compilateur C accepte le littéral.
- **En l'écrivant** : préférez les échappements octaux (`"\025"`) ; un échappement `\x` du
  C++ avale tous les chiffres hexadécimaux qui le suivent (`"caf\xE9e"` n'est pas « café »).

`tests/132sample.cpp` exécute les formes (`char[]` global et local, une méthode, un
ternaire, un littéral répété, `\x`, DEL, texte mixte) avec la vraie chaîne d'outils et le
vrai émulateur. Une correction en amont tiendrait en trois lignes dans `EscapeCCharacter`
(échapper les codes de contrôle en `\xHH`), après quoi cette phase pourrait être supprimée.

## 29. Le `NULL` du SDK vaut -1, et une fonction que rien n'appelle est supprimée

- **Vircon32 fait** : définir `NULL` comme `-1`, de sorte qu'un pointeur du
  SDK « nul » contient -1 (`malloc_first_block` avant le premier `malloc`, le
  `next` du dernier `malloc_block`). Par ailleurs, son compilateur supprime
  une fonction que rien n'appelle, avec les données d'un bloc `asm { }` qu'elle
  contient.
- **Pour v32c++** : un `0` utilisé comme pointeur s'écrit `NULL` (#5) ;
  comparer un pointeur du SDK à `0` ou `NULL` dans le source est donc juste.
  Ce qui ne l'est pas : une vue entière du pointeur (`(int)p == 0`, un parcours
  de mots `int *` qui s'arrête à 0) : arrêtez-vous à `NULL`, ou à -1. Une table
  gardée dans un bloc `asm` (inclus depuis un fichier : voir
  `INLINE_ASSEMBLY_SUPPORT.fr.md`) vit dans une fonction qui doit être appelée.

## 30. « argument X is not used » — signalé par le compilateur, tu sur demande

- **Vircon32 fait** : avertir pour chaque paramètre qu'une fonction n'utilise
  jamais, y compris le `this` caché d'une méthode qui ne touche aucun membre et
  un paramètre de callback qu'exige son type de pointeur de fonction ; il
  cesse aussi de compiler après 10 avertissements, si bien qu'en corriger
  certains en révèle d'autres.
- **Pour v32c++** : `(void)x;` dans le source fait taire un paramètre. Pour
  tous, `--silence-unused-args` commence chaque fonction ainsi par un `x;` /
  `this;` isolé (mode Vircon32 seulement) ; cela coûte un peu de code, donc
  désactivée par défaut. `tests/153sample.cpp`.

## Ce que cette liste ne couvre PAS

- Tout ce que ce projet n'a pas encore découvert — c'est un registre des divergences
  confirmées (ou au moins directement raisonnées) trouvées jusqu'ici, et non une
  affirmation d'exhaustivité. De nouvelles exigences propres à Vircon32 ont été trouvées à
  presque chaque tour du développement de ce projet ; attendez-vous à en voir d'autres.
- Les lacunes sémantiques/comportementales sans rapport avec la SYNTAXE DE SORTIE
  (aucune invocation de destructeurs, aucun remplissage de vtable pour une classe sans
  constructeur, etc.) — celles-ci sont suivies dans `docs/DESIGN_NOTES.md` et ne sont pas
  propres à Vircon32 par rapport au C standard ; elles manquent quel que soit le dialecte C
  visé par ce projet.
