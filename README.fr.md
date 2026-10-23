# v32cxx : transpileur C/C++ pour Vircon32

[Version anglaise / in English](README.md) | [Version espagnole / en español](README.es.md) <!-- langlinks -->

**Un transpileur d'un sous-ensemble de C/C++ vers C, destiné au compilateur C de
la console de fantaisie [Vircon32](https://www.vircon32.com).**

Le projet est **v32cxx** ; il construit deux programmes, `v32c++` (entrée en C++
ou en C) et `v32c` (le même programme, nommé pour l'entrée en C).

`v32c++` vous permet d'écrire des jeux et des programmes pour Vircon32 dans un
sous-ensemble de C++ — classes, héritage, constructeurs et destructeurs,
fonctions virtuelles, surcharge d'opérateurs, références, `std::string`,
`std::vector` — et les traduit en le C que la chaîne d'outils de Vircon32 sait
déjà construire. C'est l'approche qu'avait adoptée le `cfront` original de
Bjarne Stroustrup dans les années 1980 : plutôt que de compiler le C++
directement en code machine, le traduire d'abord en C et laisser un compilateur
C existant et éprouvé faire le reste. Il lit aussi le C pur (y compris le vieux
C d'Unix), et prend en charge tout ce que le dialecte C de Vircon32 fait
différemment.

> **État : fonctionnel.** Toute la chaîne — prétraitement, analyse syntaxique,
> analyse sémantique, abaissement (*lowering*) et génération de code —
> fonctionne de bout en bout et est vérifiée avec le compilateur, l'assembleur
> et la console **réels** de Vircon32 (`make realcheck` : 128 programmes
> compilés et assemblés, dont 59 s'auto-vérifient à l'exécution sur une
> console sans écran). Cinq démos d'une certaine taille (trois jeux en C++, un
> portage de Rogue et un Spy vs Spy en C pur) se construisent avec lui. Voir les [Limitations
> connues](#limitations-connues) pour un tableau honnête de ce qui manque.

## Pourquoi cela existe

Quatre publics, un seul projet :

0. **Une exploration historique.** La première implémentation de C++ (`cfront`)
   était un transpileur fonctionnant de manière similaire. Se lancer dans une
   telle entreprise a été très intéressant, révélant des schémas ainsi qu'un
   certain nombre de considérations qui ne seraient peut-être pas apparues
   autrement.

1. **Un outil pédagogique.** Il est né comme support d'un cours qui enseigne la
   programmation de jeux sur Vircon32, en commençant par le C pur puis en
   passant au C++ et à la conception orientée objet. Montrer aux étudiants
   *comment* une fonction virtuelle ou un constructeur est analysé, résolu puis
   abaissé en C pur compte autant que d'avoir un outil qui fonctionne — c'est
   pourquoi le C généré est fait pour être lu (voir `-vv`).

2. **Un outil communautaire.** La communauté homebrew de Vircon32 écrit ses jeux
   en C ; ceci permet d'opter à la place pour un petit sous-ensemble de C++ bien
   compris, sans le poids des gabarits (*templates*), des exceptions ni d'une
   STL complète.

3. **Une passerelle pour le C normal.** Le compilateur C de Vircon32 implémente
   un sous-ensemble de C avec diverses particularités syntaxiques (les
   déclarations de tableaux et la syntaxe de déclaration des pointeurs de
   fonction, entre autres). En utilisant `v32c++` comme analyseur frontal, vous
   obtenez aussi la traduction du C normal vers le dialecte C de Vircon32 pour
   la compilation, ce qui réduit au minimum la courbe d'apprentissage pour
   démarrer le développement.

Il ne cherche **pas** à prendre en charge tout le C++ — voir [Délibérément hors
du périmètre](#délibérément-hors-du-périmètre).

## Contenu

- [Ce qui est pris en charge](#ce-qui-est-pris-en-charge)
- [En-têtes C++ pour l'API Vircon32](#en-têtes-c-pour-lapi-vircon32)
- [Clavier et souris : v32io](#clavier-et-souris--v32io)
- [Limitations connues](#limitations-connues)
- [Prérequis](#prérequis) · [Compilation](#compilation) · [Vérification avec la vraie chaîne d'outils](#vérification-avec-la-vraie-chaîne-doutils)
- [Utilisation](#utilisation) · [Empaquetage de la cartouche](#empaquetage-de-la-cartouche) · [Dialectes de sortie](#dialectes-de-sortie)
- [Démos](#démos) · [Documentation](#documentation) · [Organisation du projet](#organisation-du-projet)
- [Installation](#installation) · [Gestion des versions](#gestion-des-versions) · [Retours](#retours)

## Ce qui est pris en charge

### Classes et objets

- `class` et `struct` (la seule différence est l'accès par défaut),
  `public`/`protected`/`private` (appliqués, y compris à travers l'héritage),
  classes et fonctions `friend`, déclarations anticipées (`class X;`), classes
  dans des espaces de noms.

- **Héritage simple**, avec conversions de pointeur et de référence de dérivée
  vers base.

- **Constructeurs et destructeurs**, dans la classe ou hors classe ;
  constructeurs surchargés ; constructeurs de copie ; arguments par défaut.
  Listes d'initialisation de membres pour les classes de base (`: Base(args)`),
  les membres primitifs (`: x(v)`, dans l'ordre de *déclaration*, comme l'exige
  C++) et les membres qui sont des objets (`: mVel(vx, vy)`). Initialiseurs de
  membres par défaut dans la classe (`int mLives = 3;`). Une base dotée d'un
  constructeur par défaut est construite implicitement ; les destructeurs
  s'enchaînent vers les membres et les bases.

- Objets sur la pile (`Shape s;`, `Shape s(7);`), comme temporaires anonymes
  (`return Vec(x, y);`, `v.push_back(Enemy(1, 2, 3));`), dans des tableaux
  (construits et détruits élément par élément, y compris les tableaux membres),
  à la portée du fichier (construits en début de `main`, avec les arguments du
  constructeur s'il y en a) et sur le tas avec `new`/`delete`. Les destructeurs
  s'exécutent à chaque sortie de portée : l'accolade fermante, `return`,
  `break`, `continue`.

- Fonctions **`virtual`** et répartition dynamique par vtables, fonctions
  virtuelles pures (`= 0`), destructeurs virtuels (`delete` à travers un
  pointeur de base).

- **Surcharge d'opérateurs** : arithmétiques, de comparaison, d'affectation
  composée, moins unaire et `!`, affectation, `[]`, appel de fonction `()`
  (objets fonction : `f(5)` sur un objet) et `++`/`--` préfixe et suffixe
  (`operator++()` / `operator++(int)` de C++), comme membres ou comme fonctions
  libres, utilisés avec la syntaxe naturelle (`a + b`, `list[i]->draw()`,
  `++it`, `it++`).

- **Opérateurs de conversion** (`operator int() const`, `operator bool()`,
  `operator Vec2 *()`), dans la classe ou hors classe, y compris hérités :
  appelés partout où l'objet est utilisé comme une valeur de ce type — un
  initialiseur, une affectation, un retour, une condition (`if (handle)`,
  `!handle`, `&&`), une conversion explicite (*cast*), un argument (y compris
  pour choisir entre des surcharges), de l'arithmétique et des comparaisons, un
  indice. Un `operator int()` seul sert aussi pour une cible `float` ou `bool` ;
  avec plusieurs candidats et aucun exact, il signale une ambiguïté.

- **`override` et `final`** sur les fonctions membres, et `final` sur une classe,
  vérifiés comme le fait C++ (un `override` qui ne redéfinit rien, la
  redéfinition d'une fonction `final` et la dérivation d'une classe `final`
  sont des erreurs) ; ailleurs ils restent des noms ordinaires (`int final;`).
  `explicit` est accepté.

- **Surcharge** des fonctions et des méthodes, résolue par le nombre et le type
  des arguments ; une ambiguïté est signalée plutôt que devinée.

- Objets **par valeur** : une structure ou une classe de plus d'un mot peut être
  passée et retournée par valeur ; v32c++ la réécrit avec les pointeurs cachés
  dont le C de Vircon32 a besoin (`docs/VIRCON32_QUIRKS.fr.md` n° 11).

- **Références** — paramètres, valeurs de retour, variables locales, membres de
  données (`int &r;`, liée dans la liste d'initialisation du constructeur),
  `const T &` liée à des temporaires.

- `const` (variables, pointeurs vers const, `T * const`, fonctions membres
  `const`) — accepté et émis, mais pas appliqué.

### Le reste du langage

- Espaces de noms (imbriqués, appels qualifiés, noms C avec espace de noms pour
  les fonctions : `v32::draw(int,int,int)` devient `v32__draw__int_int_int`) et
  `using` : `using namespace v32;` à la portée du fichier ou dans un bloc,
  `using v32::Keyboard;` et `using Points = int;`. Les fonctions trouvées par
  une directive sont surchargées avec celles de la portée, comme en C++.

- Variables locales `auto` et `for` sur intervalle (sur des tableaux,
  `std::array`, `std::vector` et toute classe dotée de `begin()`/`end()`).

- **`std::string`** (`#include <string>`), ainsi que **`std::array<T, N>`** et
  **`std::vector<T>`** (`#include <array>`, `<vector>`) — ce ne sont pas des
  gabarits, mais des formes fixes que le transpileur développe pour chaque type
  d'élément. Voir [`docs/STRING.fr.md`](docs/STRING.fr.md) et
  [`docs/GENERICS.fr.md`](docs/GENERICS.fr.md).

- Tout le langage d'instructions et d'expressions de C : `switch` avec vraie
  chute (*fall-through*), `do`/`while`, `goto`, l'opérateur ternaire et
  l'opérateur virgule (tous deux réécrits pour le C de Vircon32, qui n'a ni
  l'un ni l'autre), les opérateurs bit à bit avec la précédence de C, `sizeof`,
  les conversions de style C et de style C++ (`dynamic_cast` avec un
  avertissement : il n'y a pas de RTTI), `enum`, `union`, `typedef`, pointeurs
  de fonction dans la syntaxe de déclarateur standard **ou** celle de Vircon32
  (la sortie est toujours celle dont la cible a besoin), tableaux
  multidimensionnels à tailles en expressions constantes, initialiseurs entre
  accolades, littéraux hexadécimaux, octaux et binaires avec suffixes,
  `nullptr`, concaténation de chaînes adjacentes, déclarateurs multiples
  (`int a, *b, c[4];`, et des objets : `Point a(1, 2), b(3, 4);`).

- **Fonctions variadiques** : `...` avec `va_list`, `va_start`, `va_arg` et
  `va_end` de `<cstdarg>`, dans les fonctions libres, les espaces de noms et les
  méthodes, comme en C (voir [`docs/C_INPUT.fr.md`](docs/C_INPUT.fr.md)).
  Chaque argument supplémentaire tient en un mot : un `int`, `char`, `bool`,
  `enum`, pointeur ou `float` — un objet est une erreur (passez un pointeur, ou
  le `c_str()` d'une chaîne). Une surcharge `...` passe après toutes les
  autres, comme en C++.

- Variables locales `static` (déplacées à la portée du fichier sous des noms
  uniques), `static` à la portée du fichier, `extern`, `volatile`, `inline`,
  `register` (acceptés ; le C de Vircon32 n'en a aucun). `unsigned`, `signed`,
  `short` et `long` sont l'unique type `int` de 32 bits (`unsigned` avec un
  avertissement) ; `double` et `long double` sont `float`, l'unique type à
  virgule flottante de Vircon32. Les champs de bits sont acceptés comme des mots
  entiers avec un avertissement, ou rejetés avec `--reject-bit-fields`.

- **Assembleur en ligne** sous la forme `asm { "..." }` de Vircon32, ainsi que
  l'`asm("...")` basique de GCC réémis sous cette forme — voir
  [`docs/INLINE_ASSEMBLY_SUPPORT.fr.md`](docs/INLINE_ASSEMBLY_SUPPORT.fr.md).

- **`native Name;`** déclare un type défini par un en-tête C transmis tel quel
  (`date_info`, `game_signature`) afin de pouvoir l'utiliser par pointeur —
  voir [`docs/NATIVE_PASSTHROUGH.fr.md`](docs/NATIVE_PASSTHROUGH.fr.md). Les
  noms de structures et de `typedef` des en-têtes du SDK que v32c++ peut lire
  sont déclarés `native` automatiquement.
  Une structure C simple d'un tel en-tête (entiers, flottants, booléens,
  pointeurs, tableaux de longueur littérale : le `malloc_block` du SDK) est
  lue, et pas seulement nommée, si bien que `b->size`, `b->next`,
  `sizeof(malloc_block)` et les locales par valeur fonctionnent avec elle.
  Une structure que v32c++ ne sait pas suivre reste opaque.

### Un préprocesseur côté C++

Avant l'analyse, v32c++ fait ce que ferait un préprocesseur C++ :

- Les `#include` de fichiers `.hpp`/`.cpp` sont insérés, récursivement, avec
  `#pragma once`. La forme entre guillemets cherche à côté du fichier
  inclusif, puis dans chaque répertoire `-I`, puis dans le répertoire courant ;
  la forme entre chevrons seulement dans les répertoires `-I`. Les deux se
  rabattent ensuite sur `$V32CXX_INCLUDE` et sur les en-têtes installés dans
  `/usr/local/Vircon32/v32tools/include/v32c++` (la valeur par défaut, définie
  dans [`inc/config.h`](inc/config.h)). Les projets multi-fichiers fonctionnent
  de la manière habituelle, et les erreurs nomment le fichier concerné.
- Un `#include` à l'intérieur d'un corps `{ }` (une fonction, un bloc
  `asm { }`) est développé à l'endroit même où il se trouve, pour un fichier
  de n'importe quelle extension, de sorte que des tables générées (lignes
  `"integer 1, 2, 3"` pour un bloc `asm`) ou des suites d'instructions
  peuvent vivre dans leurs propres fichiers. Un en-tête du SDK ou un fichier
  introuvable passe toujours tel quel, déplacé en tête de la sortie, et
  v32c++ le signale par un avertissement.
- `#define`/`#undef` (de type objet et de type fonction, `#`, `##`,
  `__VA_ARGS__`), `-D`/`-U`, les prédéfinis `__V32CXX__`, `__FILE__`,
  `__LINE__` ; `#if`/`#ifdef`/`#elif`/... avec `defined()` ; `#error`,
  `#warning`.
- Les `#include` de `.h` sont transmis au C généré, **et** sont lus quand v32c++
  peut les trouver (`$V32CXX_SDK_INCLUDE`, le dossier `include/` situé à côté
  du `compile` de Vircon32 présent dans votre `PATH`, puis
  `/usr/local/Vircon32/DevTools/include`), de sorte que les macros du SDK comme
  `screen_width` ou `color_red` fonctionnent dans les tailles de tableaux et
  dans `#if`, et sont typées pour la résolution des surcharges. `-v` liste les
  dossiers consultés. Un `compile` qui est un lien symbolique est suivi. Quand un `#include` nomme un en-tête du SDK (`misc.h`, `video.h`, ...) introuvable, v32c++ avertit désormais, car un type qu'il définit (`malloc_block *b;`) serait sinon lu comme une multiplication.
- Les `#define` sont transmis au C généré, et les constantes nommées y gardent
  leur nom ; les macros que le préprocesseur propre à Vircon32 ne peut pas
  traiter sont développées à la place. Les numéros de ligne dans les
  diagnostics et dans les cartes de `-g` pointent vers le fichier et la ligne
  d'origine.

### Entrée en C

Un fichier nommé `.c` est transpilé comme du C : ses propres `#include` de
`.h`/`.c` sont développés, les étiquettes de `struct` ont leur propre espace de
noms, les prototypes de style K&R qui en disent moins que la définition sont
unifiés, les fonctions variadiques fonctionnent, le pointeur nul est 0, et
`main(argc, argv)` est accepté. Avec la petite bibliothèque C et curses de
`libc/`, cela suffit pour construire le jeu Unix Rogue à partir de ses sources
(`demos/c/rogue`). Voir [`docs/C_INPUT.fr.md`](docs/C_INPUT.fr.md).

`v32c` est le même programme sous un second nom, pour le C : `v32c jeu.c` et
`v32c++ jeu.c` font aujourd'hui exactement la même chose (l'extension du fichier
décide), et `make`/`cmake` construisent et installent les deux. Les scripts de
compilation appellent le nom que vous avez lancé. La sortie par défaut d'un
fichier C serait lui-même (`jeu.c` → `jeu.c`) ; sans `-o`, v32c++ écrit donc
`jeu-transpiled.c`, et un `-o` qui désigne l'entrée est une erreur.

## En-têtes C++ pour l'API Vircon32

`v32/` contient des en-têtes C++ dans l'espace de noms `v32::`, un par en-tête
du SDK — `video.hpp`, `input.hpp`, `string.hpp`, `time.hpp`, `audio.hpp`,
`math.hpp`, `misc.hpp`, `memcard.hpp` — plus **`v32io.hpp`**,
**`keyboard.hpp`** et **`mouse.hpp`** (clavier et souris, ci-dessous). Chaque
enveloppe du SDK laisse passer son `.h` et n'ajoute que ce que l'API C ne peut
pas bien exprimer : des énumérations typées au lieu de `#define`, des
ensembles de surcharges (`v32::minimum`, `v32::clamp`, `v32::absolute` pour
`int` et `float`), des gardes de portée RAII pour l'état « sélectionné » de la
console (texture, manette, son, canal : `TextureScope`, `GamepadScope`,
`SoundScope`, `ChannelScope`, `FrameScope`) et de petites classes de poignée
(`v32::Channel`, `v32::MemoryCard`, `v32::HeapBlock`, `v32::String`,
`v32::Stopwatch`). Tout accès au matériel passe toujours par la vraie fonction
C, et les noms des enveloppes ne réutilisent jamais le nom d'une fonction C
(`minimum`, pas `min`), si bien que l'API C brute reste appelable à côté.

Utilisez-les avec `-I` pointant vers le répertoire qui contient `v32/` (`v32c++
-I chemin/vers/v32c++ game.cpp`, puis `#include <v32/video.hpp>`), ou installez-
les (voir [Installation](#installation)).

## Clavier et souris : v32io

Trois en-têtes donnent aux programmes un vrai **clavier** et une vraie
**souris**, découpés de la même façon que les pilotes C
[v32io](https://github.com/wedge1020/v32io) pour Vircon32 qu'ils portent,
protocole pour protocole :

| En-tête | Classe | De la bibliothèque C |
| ------- | ------ | -------------------- |
| `v32/v32io.hpp` | `v32::IoDevice` : les 11 contrôles d'une manette lus comme des données (le noyau) | `v32io.h` |
| `v32/keyboard.hpp` | `v32::Keyboard` : événements de touches, caractères saisis (majuscules et verrouillage majuscule, et caractères accentués et de tracé de cadres envoyés par le périphérique), touches maintenues | `keyboard.h` |
| `v32/mouse.hpp` | `v32::Mouse` : un pointeur borné, le mouvement, trois boutons | `mouse.h` |

Un programme inclut le ou les pilotes dont il a besoin ; chacun inclut le noyau.

```cpp
#include <v32/keyboard.hpp>
#include <v32/mouse.hpp>

v32::Keyboard keyboard( v32::SecondGamepadPort );   // "Gamepad 2"
v32::Mouse    mouse( v32::ThirdGamepadPort );       // "Gamepad 3"
...
keyboard.probe();  mouse.probe();                   // une fois à CHAQUE image
for( int key = keyboard.read(); key > 0; key = keyboard.read() )
    if( keyboard.glyph( key ) > 0 ) text.add( keyboard.glyph( key ) );  // é, ß, ┌ ...
if( mouse.pressed( v32::MouseLeft ) ) click( mouse.x(), mouse.y() );
```

> **⚠ Ceci EXIGE un périphérique v32io.** La console n'a pas de port clavier ni
> souris : un périphérique v32io est une manette dont les contrôles
> transportent des données de clavier ou de souris, et quelque chose
> d'extérieur à la console doit les produire — **soit** l'**adaptateur matériel
> v32io** (un clavier ou une souris USB dans la carte v32io-pico, qui
> fonctionne avec n'importe quel émulateur Vircon32), **soit** un
> **DesktopEmulator Vircon32 modifié** avec les périphériques de manette
> `v32kbd`/`v32mouse` : le fork à
> <https://github.com/wedge1020/ComputerSoftware> (**branche `v32io`**), ou
> l'émulateur standard avec les correctifs du dépôt v32io appliqués. Le
> périphérique *Keyboard* propre à l'émulateur standard n'est **pas** un clavier
> v32io.

La documentation complète — installation, l'API, les protocoles, le portage
depuis la bibliothèque C, la saisie de caractères accentués et de tracé de
cadres, les tests sans périphérique — se trouve dans
**[`docs/V32IO.fr.md`](docs/V32IO.fr.md)**. Cinq exemples prêts à construire
(une machine à écrire, un programme de dessin, un moniteur de ports pour
vérifier une installation, un bloc-notes utilisant les deux périphériques, et
une visite de tous les glyphes de la police du BIOS et de la façon de les
saisir) se trouvent dans [`demos/cxx/v32io`](demos/cxx/v32io/README.md).

## Limitations connues

Vérifiées avec le transpileur actuel et le vrai compilateur Vircon32 (octobre
2026), et non reprises d'anciennes notes.

**Non pris en charge** (une erreur claire, sauf indication contraire) :

- Les membres `static` de classe.

- `enum class`, `constexpr`, les lambdas, `mutable`, les classes imbriquées et
  les énumérations à portée de classe, les déclarateurs empilés comme `T *&`.

- Un paramètre de tableau à deux dimensions (`void f(int g[4][4])`).
- Gabarits, exceptions, RTTI et héritage multiple : jamais (voir ci-dessous)

**Pris en charge avec réserves :**

- **`new` de tableau** (`new T[n]`) alloue mais n'exécute pas les constructeurs
  de chaque élément ; `delete[]` n'exécute aucun destructeur, et `delete[]` à
  travers un pointeur de base sur un tableau de dérivées est indéfini.
  Préférez `std::vector` ou un tableau sur la pile, qui sont construits et
  détruits correctement.
- **`goto`** fonctionne, mais n'exécute pas les destructeurs des variables
  locales de type classe dont il sort ; limitez-le aux portées qui n'en ont
  pas. Un `goto` vers une étiquette inexistante est détecté par le compilateur
  Vircon32, pas par v32c++.
- **`>>` d'une valeur signée coûte quelques instructions.** Le décalage de
  Vircon32 est logique, donc v32c++ réécrit `x >> n` et `x >>= n` en un
  décalage arithmétique (`-8 >> 1` vaut `-4`, comme en C). Une variable ou un
  membre est développé sur place (environ cinq instructions au lieu d'une) ;
  tout le reste passe par une petite fonction auxiliaire. Les valeurs
  `unsigned`, les constantes et les décalages masqués par une petite constante
  positive (`(x >> 4) & 15`) restent une seule instruction ;
  `--no-signed-shift` désactive la réécriture. `<`, `>`, `/` et `%` d'une valeur
  `unsigned` sont aussi faits en non signé (`--no-unsigned-ops` l'empêche) ;
  seule la conversion en flottant au-delà d'`INT_MAX` la lit encore comme négative. Voir `docs/VIRCON32_QUIRKS.fr.md` n° 14.
- **`const` n'est pas appliqué** — les violations sont laissées au compilateur C. L'entrée
  C omet `const` de la sortie (le compilateur de Vircon32 l'impose là où C ne le fait pas).
- **Les codes de contrôle dans les littéraux de chaîne sont réécrits** : la
  chaîne d'outils Vircon32 ne peut pas assembler une chaîne contenant les codes
  14–31 de la police du BIOS (`┌ ─ ┐ │ ░ ▒ ▓ █` ...) ou d'autres codes de
  contrôle, `"\025"`. v32c++ déplace un tel littéral dans un tableau global
  caché de `char` (`__v32_strN`) ; `sizeof` et l'assembleur en ligne sont
  laissés tels quels. Voir `docs/VIRCON32_QUIRKS.fr.md` n° 28.
- **Les classes dans un espace de noms gardent leur nom C simple**
  (`v32::String` est `String` dans le C généré), si bien qu'un programme ne peut
  pas avoir sa propre classe du même nom qu'une de celles d'un en-tête qu'il
  inclut (`String`, `Channel`, `Keyboard`, `Mouse`, ...). Les fonctions libres,
  elles, ont correctement leur espace de noms.
- **Surcharges et valeurs transmises telles quelles** : v32c++ ne peut pas voir
  le type du résultat d'un appel à l'API C. Un appel surchargé se résout tout de
  même quand ses autres arguments tranchent (`v32::minimum(rand(), 10)`) ;
  quand aucun ne le fait (`v32::absolute(rand())`) c'est une erreur — stockez
  d'abord la valeur dans une variable locale typée.
- **Les types `native`** peuvent être pointés mais pas créés ; le stockage doit
  venir du côté C.
- Les **unions** de plus d'un mot ne peuvent pas être passées ni retournées par
  valeur (v32c++ ne prévient pas ; c'est le compilateur Vircon32 qui le signale).
- Les **tableaux multidimensionnels d'objets** sont construits mais pas détruits
  à la sortie de portée.
- **Indications de cartouche** : deux indications `#texture` (ou `#sound`) de
  même nom ne sont pas signalées — le C généré définit le nom deux fois et la
  dernière l'emporte silencieusement.
- Un `#include` de `.h` introuvable est transmis tel quel à chaque occurrence
  (le compilateur C signale alors qu'il manque). `docs/DESIGN_NOTES.md`
  (en anglais seulement) contient l'historique de développement plus détaillé,
  y compris les lacunes comblées et la façon dont elles l'ont été.
- **`NULL` dans le SDK vaut `-1`.** v32c++ écrit un `0` utilisé comme pointeur
  nul sous la forme `NULL`, ce qui est juste pour tout pointeur, ceux du SDK
  compris (`malloc_first_block` vaut `NULL` avant le premier `malloc`). Seul
  un entier que vous comparez vous-même (`(int)p == 0`) voit la différence.
- **Le compilateur Vircon32 supprime les fonctions que rien n'appelle.** Les
  données d'un bloc `asm { }` d'une fonction jamais appelée disparaissent avec
  elle ; appelez la fonction (ou prenez son adresse) depuis un endroit qui
  subsiste.

- **Entrée C, liaison de plusieurs fichiers** (`docs/C_INPUT.fr.md`) : les
  fichiers sont fusionnés en un seul programme, donc une bibliothèque est du
  source relu à chaque édition de liens. Les fichiers C++ se lient aussi, avec
  pour l'instant un simple test léger ; la vérification des références non
  définies concerne C. L'étape des bibliothèques lance une seconde copie du
  programme avec `popen` (`_popen` sous Windows, pas encore essayé là).
- **Le code pour un autre processeur ne se traduit pas.** L'`asm` écrit pour
  RISC-V (le `wfi` de xv6, les accès aux CSR) passe tel quel ; cette fonction
  est à réécrire pour Vircon32.

## Délibérément hors du périmètre

Pour que le projet reste réalisable, ce sont des limites de conception
permanentes, pas des « pas encore » :

- Les gabarits (les conteneurs ci-dessus en sont un substitut au niveau du
  transpileur)
- Les exceptions et la RTTI
- L'héritage multiple
- La STL complète

## Prérequis

Au minimum :

- Un compilateur C (gcc ou clang) et `make`.

Facultativement :

- **flex**
- **bison**, version 3.x. (Sous macOS, le `/usr/bin/bison` du système est GNU
  bison 2.3, figé pour des raisons de licence : `brew install bison` et placez-
  le en tête de votre `PATH`.)

Construit et testé sous Linux et macOS ; le workflow de publication (voir
[Publications et compilations automatiques](#publications-et-compilations-automatiques))
le construit aussi et lui fait passer un test de fumée sous Windows (x86_64 et
arm64) et Raspberry Pi et, en option, sous OpenBSD et Haiku. Pour Windows (MinGW / MSYS2), les
quelques appels POSIX dont le code source a besoin (`realpath`, `getline`,
`strndup`, `tmpfile`, le séparateur de `PATH`) passent par `src/compat.c`, qui
a une branche Windows pour chacun.

Pour construire des cartouches, il vous faut aussi les DevTools de Vircon32
(`compile`, `assemble`, `packrom`, plus `png2vircon`/`wav2vircon` pour les
ressources) dans votre `PATH`.

## Compilation

```sh
git clone https://github.com/wedge1020/v32cxx v32c++
cd v32c++
make            # builds bin/v32c++
make test       # transpiles every sample in tests/ into out/
```

`make test` montre que chaque transpilation a réussi (ou, pour les échantillons
délibérément invalides, a échoué proprement).

### Avec CMake

Pour ceux qui le préfèrent, `CMakeLists.txt` construit le même transpileur à
partir des mêmes sources (le Makefile reste la compilation principale, et `make
realcheck` reste la vérification avec la vraie chaîne d'outils) :

```sh
cmake -S . -B build              # Release by default
cmake --build build              # build/v32c++
ctest --test-dir build           # transpile every sample (read from the Makefile's test list)
sudo cmake --install build       # see "Installing"
```

flex et bison sont utilisés quand ils sont trouvés
(`-DV32CXX_REGENERATE_PARSER=OFF` les ignore) ; sinon les `src/parser.c`,
`inc/parser.h` et `src/lexer.c` déjà générés sont compilés tels quels, de sorte
qu'une compilation simple n'a besoin que d'un compilateur C et de CMake 3.10+.
Sous **Windows**, construisez avec MinGW-w64 (par exemple depuis un shell MSYS2
MINGW64, `cmake -S . -B build -G "MinGW Makefiles"`) ; MSVC n'est pas pris en
charge (les sources utilisent `getopt_long` et `<unistd.h>`). `cpack --config
build/CPackConfig.cmake` produit un `.tar.gz` (plus un `.deb` / `.rpm` là où
`dpkg-deb` / `rpmbuild` sont disponibles), ou un `.zip` sous Windows.

## Vérification avec la vraie chaîne d'outils

`make realcheck` va plus loin : il compile, assemble et empaquette chaque
programme généré avec les outils **officiels** de Vircon32, puis démarre chaque
échantillon auto-vérifiant sur une console sans écran avec le BIOS standard et
une carte mémoire neuve. Construisez ces outils une fois (environ une minute ;
il faut `git` et un `g++` C++17, pas de SDL) :

```sh
tools/vircon32/build-tools.sh
make realcheck
```

Un échantillon auto-vérifiant déclare `int test_errors = -1;` et y stocke son
nombre d'erreurs avant de s'arrêter ; la vérification exige 0 (voir
`tests/94sample.cpp`). Un échantillon accompagné d'un `tests/NNsample.input` est
joué avec cette entrée de manette scriptée (`tests/119sample.cpp`, le test de
v32io, l'utilise pour le trafic clavier et souris).

`build-tools.sh` construit aussi des outils sans écran utiles en eux-mêmes,
tous pilotés par le même format de script d'entrée :

| Outil | Ce qu'il fait |
| ----- | ------------- |
| `v32run`  | démarre une cartouche et rapporte un mot de RAM (ce qu'utilise `realcheck`) |
| `v32prof` | charge CPU/GPU par image et cycles par fonction, à partir d'une compilation `-g` |
| `v32shot` | l'écran aux images choisies, en images PPM |
| `v32peek` | état du CPU et mots de RAM à une image choisie |

Le `string.h` de la chaîne d'outils est le `c_api/string.h` du projet :
`build-tools.sh` l'installe par-dessus celui des DevTools figées, qui précède la
correction de la retenue d'arrondi de `ftoa()` que vérifie
`tests/130sample.cpp`.

`tools/vircon32/v32io-script.py` écrit de tels scripts à partir d'actions de
clavier et de souris (`docs/V32IO.fr.md`). `wav2vircon` est aussi construit
lorsqu'un vrai SDL2 est installé (`sdl2-config`), pour profiler les démos qui
empaquettent des sons.

## Utilisation

```sh
v32c++ game.cpp                       # writes game.c and game.xml, silently
v32c++ -I path/to/v32c++ -o obj/game.c game.cpp
```

| Option | Signification |
| ------ | ------------- |
| `-o file.c` | chemin de sortie (par défaut : le nom de l'entrée avec `.c` ; `nom-transpiled.c` si cela écraserait l'entrée, comme pour un fichier `.c`) |
| `-c` | n'exige pas de `main` (un fragment de bibliothèque/module) |
| `-I dir`, `--include=dir` | ajoute un répertoire au chemin de recherche des `.hpp`/`.cpp` (répétable) |
| `-D name[=value]`, `-U name` | définit / supprime la définition d'une macro |
| `-v`, `-vv`, `-vvv` | progression ; plus des commentaires explicatifs dans le C généré ; plus des vidages de l'AST, sémantiques et d'abaissement et le journal des notes d'abaissement |
| `-x`, `--no-xml` | n'écrit pas le XML de la cartouche |
| `--no-build-scripts` | n'écrit pas `Make.sh` / `Make.bat` (voir plus bas) |
| `--title-opt=TEXTE` | titre de la cartouche optimisée, comme une directive `#titleopt` (et la remplace) |
| `--no-diff-opt-title` | donne à la cartouche optimisée le même titre qu'à la normale, sans ` [optimized]` |
| `--xml-preserve-paths` | conserve dans le XML les chemins de `#texture`/`#sound` tels qu'écrits, et écrit le XML à côté du source |
| `-b` | transpile un BIOS plutôt qu'une cartouche |
| `-g` | écrit une carte de débogage ligne C / ligne C++, `<sortie>.c.debug` |
| `--target=vircon32`/`v32` | C Vircon32 (la valeur par défaut) |
| `--target=standard`/`std` | C standard portable à la place |
| `--reject-bit-fields` | fait d'un champ de bits une erreur au lieu d'un avertissement |
| `--no-signed-shift` | imprime `>>` / `>>=` tels qu'écrits (le décalage de Vircon32 est logique ; par défaut un décalage signé est réécrit pour être arithmétique) |
| `--no-unsigned-ops` | imprime `<`, `>`, `/`, `%` d'`unsigned` tels quels (signés sur Vircon32) |
| `--warn-error[=type,...]` | transforme les avertissements de v32c++ en erreurs (pas de fichier de sortie, code 1) ; `--list-warnings` liste les types |
| `--yield-points[=N]` | insère `if (get_cycle_counter() > N) yield();` dans fonctions, boucles et étiquettes (`--yield-func=`, `--yield-counter=` ; `__attribute__((no_yield))` l'évite) |
| `--rename ancien=nouveau` | renomme une fonction/variable dans tout le programme (heurts avec le SDK) |
| `--silence-unused-args` | commence toute fonction qui ignore un paramètre (le `this` d'une méthode qui ne touche aucun membre, l'argument d'un callback) par un `nom;` isolé, que le compilateur Vircon32 compte comme un usage : plus d'avertissement `argument "x" is not used` (il s'arrête après 10). Un peu de code par paramètre |
| `--mklib lib.v32a` | écrit un index de bibliothèque sur les fichiers source donnés (voir `docs/C_INPUT.fr.md`, *Bibliothèques*) |
| `-L dir`, `-lname`, `lib.v32a` | lie avec une bibliothèque : seuls les membres dont le programme a besoin sont pris |
| `--allow-undefined` | avec plusieurs fichiers d'entrée, fait d'une fonction ou variable `extern` utilisée et jamais définie un avertissement plutôt qu'une erreur d'édition de liens |
| `--no-inline-containers` | garde les accesseurs de `std::array`/`std::vector` sous forme d'appels |
| `--version` | affiche la version et termine |

Il est silencieux en cas de succès, comme le compilateur Vircon32 et v32lua ;
les erreurs et avertissements vont sur la sortie d'erreur standard avec le
fichier et la ligne, et le code de sortie est non nul en cas d'erreur. La page
de manuel (`man ./man/fr/man1/v32c++.1`) décrit chaque option en détail.

**`-vv`** sème des commentaires dans le `.c` généré là où la traduction de C++
en C est la moins évidente — vtables, le `this` explicite, ce que deviennent
`new`/`delete`, les destructeurs appelés en sortie de portée, les appels
virtuels — afin que la sortie puisse se lire seule (essayez
`tests/32sample.cpp`). Il ne change jamais le code lui-même. **`-vvv`** ajoute
les vidages internes et un **journal des notes d'abaissement** : une ligne par
endroit où v32c++ a réécrit du code pour contourner une particularité de
Vircon32 (un ternaire remonté dans un `if`/`else`, un `&function` implicite,
une conversion du receveur qui écarte `const`) — essayez `tests/71sample.cpp`.

**`-g`** écrit `<sortie>.c.debug`, une table clairsemée
(`c_path,c_line,cpp_path,cpp_line[,function_name]`) qui rattache les lignes du C
généré à la source, sur le modèle des propres cartes de débogage de Vircon32.

**`-b`** transpile un BIOS : le `<rom>` du XML reçoit `type="bios"`, et v32c++
vérifie qu'il y a exactement un `#texture`, au plus un `#sound` et une `void
error_handler()`, en signalant toutes les violations à la fois.

## Empaquetage de la cartouche

Un XML d'empaquetage de cartouche est écrit à côté du C généré (`game.xml`), de
sorte que `packrom` n'a pas besoin d'un fichier écrit à la main. Cinq
indications dans la source le remplissent, sur le modèle de celles de v32lua :

```cpp
#title   "My Game"
#titleopt "My Game (fast)"
#version 1.0
#texture Background "background.png"
#sound   jump_sfx   "jump.wav"
```

Chaque nom de `#texture`/`#sound` devient un `#define` de son identifiant (0, 1,
2… dans l'ordre, textures et sons comptés séparément), utilisable partout où un
entier est admis (`select_texture( Background )`), et remplit `<textures>` /
`<sounds>` du XML dans cet ordre avec l'extension changée en `.vtex`/`.vsnd`.
`#title`/`#version` définissent les attributs de `<rom>` (sinon « Vircon32
Program » / « 1.0 »). `#titleopt` (facultatif) fixe le titre dans le XML de la
version optimisée, `gameOpt.xml` ; sans lui, ce titre est celui de la cartouche
suivi de ` [optimized]` (`--no-diff-opt-title` les garde identiques). Le `<binary path>` du XML est le chemin de `-o` avec
`.vbin`.

`packrom` lit tous les chemins du XML relativement au XML lui-même ; par
défaut le XML ne contient donc que des noms de fichier : avec `-o obj/game.c`
et `#texture Ship "textures/ship.png"` on obtient `obj/game.xml` avec
`<binary path="game.vbin" />` et `<texture path="ship.vtex" />`, c'est-à-dire
que les `.vbin`/`.vtex`/`.vsnd` sont attendus à côté du XML. Si deux
ressources aboutissent au même nom, la première le garde et la suivante
conserve juste assez de répertoires pour se distinguer : `textures/ship.png`
puis `textures/extra/ship.png` donnent `ship.vtex` et `extra/ship.vtex` (le
second `.vtex` est attendu dans `obj/extra/`). Un jumeau pour la version
optimisée est écrit à côté (`obj/gameOpt.xml`, dont `<binary>` est
`gameOpt.vbin`). `--xml-preserve-paths`
conserve au contraire le chemin de chaque directive tel qu'écrit
(`textures/ship.vtex`), écrit le XML dans le répertoire du *fichier source*
et fait de `<binary>` le chemin du `.vbin` relatif à celui-ci.

### Scripts de compilation

Chaque fois que le XML est écrit, v32c++ écrit aussi `Make.sh` (bash) et
`Make.bat` (Windows), dans le style des projets d'exemple Vircon32, à partir des
mêmes informations : transpiler (v32c++ de nouveau, avec vos options
`-I`/`-D`/`-U`/`-b`/`-g` et `--no-build-scripts`), `compile`, `assemble`,
`png2vircon` pour chaque `#texture`, `wav2vircon` pour chaque `#sound`, et
`packrom`. Ils sont écrits à côté du source, ou dans son répertoire parent quand
le source est dans un répertoire `src/` et que ce parent a (ou, via `-o`, reçoit)
un `obj/` ; tous les chemins sont relatifs à ce dossier, on les lance donc depuis
lui. Chaque script vérifie d'abord que les outils utilisés sont dans le `PATH`
(si `v32c++`, `compile`... manque, il s'arrête avec « X is needed to build »). Si
`v32opt` est trouvé à l'exécution, la version optimisée (`v32opt -O3`,
`gameOpt.vbin`, `gameOpt.xml`, `bin/gameOpt.v32`) est aussi produite ; sinon elle
est ignorée avec une note. Un `Make.sh`/`Make.bat` que v32c++ n'a pas écrit n'est
jamais écrasé (un avertissement est affiché). `--no-build-scripts` désactive
tout cela, tout comme `-x`.

## Dialectes de sortie

`--target=vircon32` (la valeur par défaut) émet du C Vircon32, avec toutes les
particularités de ce dialecte prises en charge : pas de mot-clé `struct` dans
les références de types, déclarateurs de tableaux et de pointeurs de fonction
inversés, `void main(void)`, `NULL`, pas d'opérateur ternaire ni virgule,
paramètres et retours d'un mot, `&` sur les noms de fonctions, et le reste
catalogué dans [`docs/VIRCON32_QUIRKS.fr.md`](docs/VIRCON32_QUIRKS.fr.md).
`--target=standard` émet à la place du C portable (sans XML ni carte de
débogage), pour utiliser v32c++ comme un traducteur C++ vers C ordinaire ou
pour vérifier la sortie avec `gcc` ; `main` y conserve son type de retour
déclaré.

## Démos

| Démo | Langage | Ce que c'est |
| ---- | ------- | ------------ |
| [`demos/cxx/tempest_32k`](demos/cxx/tempest_32k/README.md) | C++ | un jeu de tir en tube dans le style de TEMPEST, multi-fichiers |
| [`demos/cxx/space_invaders`](demos/cxx/space_invaders/README.md) | C++ | « SPACE INVADERS++ », avec bonus, multi-fichiers |
| `demos/cxx/star_raiders` | C++ | un jeu spatial dans le style de Star Raiders |
| [`demos/cxx/v32io`](demos/cxx/v32io/README.md) | C++ | exemples clavier et souris (nécessitent un périphérique v32io) |
| [`demos/c/rogue`](demos/c/rogue/README.md) | C | Rogue 5.4.4 d'Unix, transpilé à partir de ses sources |
| [`demos/c/spyvsspy`](demos/c/spyvsspy/README.md) | C | un jeu Spy vs Spy en C pur, et une sonde de couverture du C |

`make -C demos` construit chaque démo qui possède un Makefile (chacune a besoin
de `v32c++` et des DevTools dans le `PATH` ; les jeux C++ utilisent aussi
`v32opt` lorsqu'il est installé).

## Documentation

| Document | Ce qu'il couvre |
| -------- | --------------- |
| [`man/fr/man1/v32c++.1`](man/fr/man1/v32c++.1) | la page de manuel : chaque option, le langage, le préprocesseur, l'empaquetage |
| [`man/fr/man3/`](man/fr/man3) | pages de la section 3v32 pour les en-têtes : `v32c++-headers(3v32)` pour commencer, puis `video.hpp`, ..., `v32c++-string`, `v32libc`, `v32curses`, ... |
| [`docs/V32IO.fr.md`](docs/V32IO.fr.md) | prise en charge du clavier et de la souris (`v32/keyboard.hpp`, `v32/mouse.hpp`) |
| [`docs/C_INPUT.fr.md`](docs/C_INPUT.fr.md) | transpiler du C, et la bibliothèque C de `libc/` |
| [`docs/STRING.fr.md`](docs/STRING.fr.md) | le `std::string` intégré |
| [`docs/GENERICS.fr.md`](docs/GENERICS.fr.md) | `std::array` et `std::vector` |
| [`docs/NATIVE_PASSTHROUGH.fr.md`](docs/NATIVE_PASSTHROUGH.fr.md) | types `native` pour les structures d'en-têtes C |
| [`docs/INLINE_ASSEMBLY_SUPPORT.fr.md`](docs/INLINE_ASSEMBLY_SUPPORT.fr.md) | instructions `asm` |
| [`docs/VIRCON32_QUIRKS.fr.md`](docs/VIRCON32_QUIRKS.fr.md) | toutes les façons dont le C de Vircon32 diffère du C standard, et comment chacune est traitée |
| [`docs/DESIGN_NOTES.md`](docs/DESIGN_NOTES.md) | les rouages internes et tout l'historique du développement (en anglais seulement) |
| [`docs/TRANSPILER_REPORT.md`](docs/TRANSPILER_REPORT.md) | le rapport de lacunes du premier portage de Space Invaders, avec l'état actuel (en anglais seulement) |

Les documents sont disponibles en anglais, en espagnol (`.es.md`) et en français
(`.fr.md`). Les pages de manuel traduites se trouvent dans `man/es/` et
`man/fr/`.

## Organisation du projet

```
src/            implementation (.c, plus the flex/bison sources)
  lexer.l       flex scanner
  parser.y      bison GLR grammar
  prescan.c     the C++-side preprocessor: include resolution, #define,
                #if, line markers -- runs before the lexer
  macro.c       macro table, expander and #if evaluator for prescan.c
  ast.c         the AST built while parsing
  symtab.c      scoped symbol table (typedef/class/native names, and the
                lexer's qualified-name handling)
  sema.c        semantic analysis: class layouts, out-of-line definition
                matching, name mangling, access control, overload
                resolution, native-type checks
  lower.c       lowering: struct layout, this-injection, calls and
                virtual dispatch, references, new/delete, vtables,
                constructors/destructors, by-value structs, Vircon32
                quirk rewrites (ternary, comma, &function, casts)
  cmode.c       C input (.c files): tags, prototypes, main's parameters,
                variadic calls (docs/C_INPUT.md)
  codegen.c     C code generator (Vircon32 or --target=standard)
  generic.c     std::array<T, N> / std::vector<T> (docs/GENERICS.md)
  stdstring.c   the built-in <string> text (GENERATED from lib/string by
                tools/embed-header.sh; docs/STRING.md)
  cartxml.c     cart-packing XML generation
  debugmap.c    the -g debug map
  pathutil.c    filename-extension helper
  compat.c      the POSIX calls that differ on Windows
  main.c        command line (see "Using it")
inc/            headers for the above, plus:
  driver.h      state shared between the lexer and parser
  v32cxx.h      project identity: VERSION (the single source), AUTHOR, URL
  config.h      build-time configuration: include search paths, their
                environment variables, limits
lib/string      the source of the built-in <string> header
libc/           for C input: a small C library, an 80x24 text terminal on
                the BIOS font, curses, memory-card files
v32/            C++ headers for the Vircon32 C API, plus v32io.hpp,
                keyboard.hpp and mouse.hpp (keyboard and mouse)
c_api/          copies of the Vircon32 SDK's own C headers, for reference
tests/          NNsample.cpp / .c inputs run by `make test` (some
                deliberately invalid); self-checking ones run by
                `make realcheck`; 119sample.{actions,input} is scripted
                v32io input
demos/          example programs, C (demos/c) and C++ (demos/cxx)
docs/           the documents listed above (+ .es.md / .fr.md translations)
man/            man1/v32c++.1, the manual page (`man ./man/man1/v32c++.1`);
                man3/*.3v32, the header pages and their aliases
                (`man -M man 3v32 v32libc`); es/ and fr/, the same pages
                translated (`man -M man/fr 3v32 v32libc`); vircon32/, the
                Vircon32 SDK's own pages (emulator and DevTools commands in
                man1/, the C library headers in man3/:
                `man -M man/vircon32 3v32 string.h`)
CMakeLists.txt  the CMake build, install and packaging (see "With CMake")
cmake/          cmake_uninstall.cmake.in, for CMake's `uninstall` target
scripts/        smoke_test.sh and stage_package.sh (the release workflow's
                checks and package layout), install_docs.sh (documentation
                install, for DEST_LANG)
.github/        workflows/release-builds.yml, the build and release workflow;
                release-notes.md, the release page text it publishes
AGENTS.md       notes for AI coding agents working on the project
tools/
  embed-header.sh   regenerates src/stdstring.c from lib/string
  vircon32/         build-tools.sh (the real Vircon32 toolchain and the
                    headless tools), check.sh (`make realcheck`), the
                    headless tools' sources, and v32io-script.py
```

(L'arborescence est volontairement laissée en anglais : ce sont les noms et
commentaires des fichiers tels qu'ils figurent dans le dépôt.)

## Installation

Une installation pour tout le système place chaque chose là où une
configuration Vircon32 l'attend. Le binaire et la page de manuel vont là où
vont les commandes et les pages de manuel ; les en-têtes, la bibliothèque C et
les documents vont sous `v32tools/`, le dossier que partagent les outils de la
communauté (les fichiers de v32opt, et les en-têtes de v32lua dans
`v32tools/include/v32lua`), à côté des `DevTools/` officiels :

| | Linux / macOS | Windows (CMake) |
| --- | --- | --- |
| transpileur (`v32c` en est une copie) | `/usr/local/bin/v32c++`, `/usr/local/bin/v32c` | `C:\Program Files\Vircon32\v32tools\v32c++.exe`, `v32c.exe` |
| pages de manuel (celles de v32c++, et celles du SDK Vircon32 de `man/vircon32/`, côte à côte) | `/usr/local/share/man/man1/*.1.gz`, `/usr/local/share/man/man3/*.3v32.gz` (compressées avec gzip ; les alias sont des liens symboliques) | `...\v32tools\docs\v32c++\man1`, `...\man3` |
| en-têtes C++ (`v32/`) | `/usr/local/Vircon32/v32tools/include/v32c++/v32` | `...\v32tools\include\v32c++\v32` |
| bibliothèque C (`libc/`) | `/usr/local/Vircon32/v32tools/include/v32c++/libc` | `...\v32tools\include\v32c++\libc` |
| documentation | `/usr/local/Vircon32/v32tools/docs/v32c++` | `...\v32tools\docs\v32c++` |

L'une ou l'autre compilation s'en charge :

```sh
sudo make sysinstall                          # Linux / macOS
sudo make sysuninstall

sudo cmake --install build                    # any system (prefix: /usr/local,
sudo cmake --build build --target uninstall   #   or C:/Program Files/Vircon32)
```

Le répertoire des en-têtes est compilé dans le transpileur, si bien qu'après
l'installation, `#include <v32/video.hpp>` (ou `<v32/keyboard.hpp>`, ...)
fonctionne depuis n'importe quel répertoire sans `-I`, et l'entrée en C trouve
de la même façon la `libc` installée. Avec CMake, choisissez un autre
emplacement à la configuration (`cmake -S . -B build
-DCMAKE_INSTALL_PREFIX=/opt/vircon32`) et le transpileur cherchera là ;
`-DV32CXX_INSTALL_INCLUDEDIR=...` (et `BINDIR`, `MANDIR`, `DOCDIR`) déplacent
une seule partie. La désinstallation ne retire que les fichiers propres à
v32c++, puis `v32tools/` (et sous Linux / macOS `Vircon32/`) seulement s'il ne
reste rien d'autre dedans. Sous Windows, ajoutez
`C:\Program Files\Vircon32\v32tools` à votre `PATH`, comme pour les DevTools.

`make install` / `make uninstall` copient à la place uniquement le binaire dans
`~/bin`. Les valeurs d'installation par défaut de la compilation par Makefile
(le répertoire des en-têtes, les noms des variables d'environnement, quelques
limites) se trouvent dans [`inc/config.h`](inc/config.h) ; changez-en une là et
recompilez, ou remplacez-la lors de la construction :

```sh
make CFLAGS="-Wall -Wextra -g -Iinc -MMD -MP -DV32CXX_INCLUDE_PATH='\"/opt/v32tools/include/v32c++\"'"
```

**Langue de la documentation.** Par défaut, les deux compilations installent les
documents dans toutes les langues (`README.md`, `README.es.md`, `README.fr.md`,
`docs/V32IO.fr.md`, ...) et les pages de manuel en anglais ainsi que les pages
traduites dans les arborescences de langue standard (`man/es/` est installé dans
`/usr/local/share/man/es/man1` et `man3`, `man/fr/` dans `.../fr/man1` et
`man3`, de sorte que `man` choisit la langue d'après `LANG`). `DEST_LANG=es` ou
`DEST_LANG=fr` (`sudo make sysinstall DEST_LANG=fr`, ou `cmake -S . -B build
-DDEST_LANG=fr`) installe à la place uniquement cette langue : le README et les
documents traduits sous ces noms simples (avec les liens entre eux ajustés), à
côté des documents qui n'existent qu'en anglais (`docs/DESIGN_NOTES.md`, ...), et
les pages de manuel dans cette langue directement dans `man1` et `man3`.
`DEST_LANG=en` n'installe que les documents et les pages de manuel en anglais.
Les pages du SDK Vircon32 (`man/vircon32/`) n'existent qu'en anglais et sont
toujours installées.

## Gestion des versions

La version est `YYYYMMDD-statut` (`-dev` ou `-release`), le même schéma que
v32lua, et elle vit en un seul endroit : `VERSION` dans
[`inc/v32cxx.h`](inc/v32cxx.h), que `v32c++ --version` affiche. Pour la changer,
modifiez-la là et lancez `make version`, qui l'estampille (avec la date du jour)
dans l'en-tête `.TH` de chaque page de manuel de `man/man1` et `man/man3` qui
nomme v32c++, ainsi que dans celles de `man/es/` et `man/fr/` (avec la date dans
la langue de chacune). Les pages alias et les pages d'autres projets (celles du
SDK Vircon32, dans `man/vircon32/`) sont laissées intactes, et une page déjà à
jour n'est pas réécrite.

## Publications et compilations automatiques

[`.github/workflows/release-builds.yml`](.github/workflows/release-builds.yml)
(sur le modèle de celui de v32lua) ne s'exécute que lorsqu'il est lancé à la
main, depuis l'onglet Actions, comme une exécution `test` ou `release` (rien ne
s'exécute lors d'un push). Les tâches :

| Tâche | Ce qu'elle fait |
| ----- | --------------- |
| Read Version | lit `VERSION` dans `inc/v32cxx.h` ; vérifie que `make version` l'a estampillée dans les pages de manuel (un avertissement pour une exécution `test`, une erreur pour une `release`), que `man/vircon32/` et `man/` ne partagent aucun nom de page, et que chaque page alias mène quelque part |
| Test Against the Vircon32 Toolchain | `make realcheck` (ci-dessus), avec la chaîne d'outils mise en cache entre les exécutions |
| CMake Build, Test and Install | configurer, construire, `ctest`, puis `cmake --install` dans un répertoire temporaire, en vérifiant le binaire, les en-têtes et chaque page de manuel, compressée, les alias en liens (Linux et macOS) |
| Build | Windows (x86_64, arm64), macOS (x86_64, arm64), Linux (x86_64, arm64, Raspberry Pi) et, en option, OpenBSD et Haiku, chacun testé en fumée (`scripts/smoke_test.sh`) et zippé (`scripts/stage_package.sh`) |
| Release | pour une exécution `release` : une publication GitHub `v<VERSION>` pour le commit qui a été construit, avec tous les zips, un `SHA256SUMS.txt` et le texte de [`.github/release-notes.md`](.github/release-notes.md). Elle reste à l'état de brouillon, à relire et publier à la main, sauf si l'exécution a été lancée avec `publish_release` coché, ce qui la publie aussitôt ; une version `-dev` est marquée comme préversion |

Un zip de publication se décompresse directement dans `v32tools/`
(`/usr/local/Vircon32/v32tools` ou `<Program Files>/Vircon32/v32tools`) : le
binaire, `include/v32c++/` (`v32/` et `libc/`) et `docs/v32c++/` avec les
documents et les pages de manuel dans `man1/` et `man3/`, celles de v32c++ et
celles du SDK Vircon32 ensemble (`man -M docs/v32c++ 3v32 string.h`). Pour
publier : définissez `VERSION`, lancez `make version`, mettez à jour le texte de
`.github/release-notes.md` (`@VERSION@` y devient la version), faites un commit,
puis lancez le workflow avec `release` ; relisez ensuite le brouillon sur la page
Releases et cliquez sur **Publish release** (ou cochez `publish_release` en
lançant le workflow, pour sauter cette étape).

## Collaboration avec l'IA

Cet outil a été développé en étroite collaboration avec une IA. J'hésite à le
qualifier de « vibe-coded », car j'ai quelques connaissances et une certaine
expérience des bases. Mais je n'aurais certainement pas atteint ce niveau de
fonctionnalité en si peu de temps sans la participation active de l'IA. J'ai
le sentiment d'avoir tenu davantage un rôle de « chef de projet » : dicter la
direction, résoudre les problèmes, déboguer quand il le fallait.

Même si je ne connais peut-être pas chaque recoin du code aussi bien que si je
l'avais entièrement écrit moi-même, j'apprécie l'effet de levier que cela
procure pour créer de nouveaux outils que je peux utiliser en classe, partager
avec la communauté, et concrétiser plus vite mes idées.

Je sais que cela peut franchir, ou constituer, une limite d'un flou/d'une
acceptabilité douteux pour certains. Mais, comme outil de niche dans un
environnement de niche, il est tout aussi facile de l'ignorer.

## Retours

Ceci est construit au grand jour comme un projet pédagogique et partagé avec la
communauté Vircon32 — si quelque chose casse, se transpile mal, ou si vous
aimeriez qu'une fonctionnalité C++ particulière soit priorisée, veuillez ouvrir
un ticket sur <https://github.com/wedge1020/v32cxx>.
