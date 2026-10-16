# Entrée en C

[Version anglaise / in English](C_INPUT.md) | [Version espagnole / en español](C_INPUT.es.md) <!-- langlinks -->

`v32c++ program.c` transpile du **C**, pas du C++. C'est l'extension du fichier
qui décide : un nom se terminant par `.c` active le mode C pour toute
l'exécution (`g_c_mode`, `inc/driver.h`). Tout ce document ne s'applique qu'à
l'entrée en C — sauf les *Fonctions variadiques*, qui fonctionnent désormais
aussi en C++ — et un fichier `.cpp` est par ailleurs lu exactement comme avant.

v32c++ a toujours lu l'essentiel du C, en tant que sous-ensemble du C++. Le
mode C, c'est le reste : les endroits où C et C++ divergent, où le vieux C en
dit moins que ce qu'exige C++, et où le C de Vircon32 diffère des deux. Chaque
élément ci-dessous a été découvert en construisant un vrai programme, Rogue
5.4.4 (`demos/c/rogue`), dont les 28 fichiers source sont transpilés octet pour
octet tels qu'ils ont été écrits pour Unix. `tests/116sample.c` et
`tests/117sample.c` sont les échantillons auto-vérifiants ; les deux sont aussi
valides pour un compilateur C natif. `tests/118sample.c` couvre les étiquettes
utilisées sans leur mot-clé, ce que seul le C de Vircon32 accepte.

## Ce qu'un programme C peut utiliser

| Construction C | Ce que v32c++ en fait | Où |
|---|---|---|
| `#include "rogue.h"`, `#include "src/move.c"` | Les fichiers `.h` et `.c` propres au programme sont développés et transpilés. Les en-têtes du SDK Vircon32 (`video.h`, `string.h`, ...) sont toujours transmis au compilateur. | `prescan.c` |
| `#include <stdarg.h>` | Aucun fichier n'est lu ; il fournit `va_list`. | `prescan.c` |
| `#define when break;case`, macros de type fonction | Chaque utilisation est développée, et seules les constantes sont transmises au C généré (`#define MAXSTR 1024`, `#define STATLINE (NUMLINES - 1)`). Une constante octale n'est jamais transmise : le C de Vircon32 la lirait comme du décimal. | `macro.c` |
| `struct rdes { ... } rdes[9];` | Les étiquettes ont leur propre espace de noms. Une étiquette qui est aussi un nom ordinaire est renommée `rdes_tag` en sortie. | `parser.y`, `cmode.c` |
| `v32key *next;` après `struct v32key { ... }` ou `struct v32key;`, sans typedef | Une étiquette utilisée sans son mot-clé, comme le permet le C de Vircon32, est lue comme le type : membres (y compris une structure qui pointe sur elle-même), types de retour, paramètres, variables locales, conversions, `sizeof`. Là où le même nom est aussi une variable, un membre ou un paramètre dans la portée, c'est cela qui l'emporte, si bien que `struct rdes rdes[9];` n'est pas affectée. | `lexer.l` (`yylex`), `parser.y` (`c_tag_known`) |
| une structure, union ou énumération définie à l'intérieur d'une déclaration, avec ou sans étiquette | La définition est déplacée à la portée du fichier, avant la déclaration ; celles sans nom s'appellent `__v32_anonN`. | `parser.y` |
| `void fatal();` puis `void fatal(char *s) { }` ; `void leave(int);` ; un prototype à l'intérieur d'une fonction | Une seule fonction : la première déclaration est réécrite pour dire ce que dit la définition. Pas de décoration de noms. Une fonction déclarée et jamais définie est supprimée. | `cmode.c`, `sema.c` |
| `extern char prbuf[];` puis, ailleurs, `char prbuf[2*MAXSTR];` | Une seule variable ; la déclaration avec la taille l'emporte. | `ast.c` |
| `main(int argc, char **argv, char **envp)`, sans type de retour | `void main(void)` dont les premières instructions déclarent `argc` (1), `argv` (`{"program", NULL}`) et `envp` (`{NULL}`). | `parser.y`, `cmode.c` |
| `new_item(sizeof (THING))` alors que la définition est `new_item()` | Les arguments sont supprimés, avec un avertissement. | `cmode.c` |
| un `char **v32_main_args(int *argc, char **argv)` défini par le programme | Appelé en tête de `main` (`argv = v32_main_args(&argc, argv);`) : permet à un écran-titre de choisir la ligne de commande du programme. | `cmode.c` |
| `int printf(char *fmt, ...)`, `va_start`, `va_arg`, `va_end` | Voir *Fonctions variadiques* ci-dessous. | `parser.y`, `sema.c`, `cmode.c` |
| `char **argv`, `char **a, *b;` | Pointeurs de pointeurs (jusqu'à trois niveaux). | `parser.y` |
| `void (*func)()` comme paramètre, `(void (*)())fn` | Paramètres et conversions de pointeurs de fonction. | `parser.y` |
| `thing == ptr` avec un `void *ptr` | Comparés comme des adresses. | `lower.c` phase 12 |
| `(*d_func)(arg)` via un pointeur déclaré `void (*d_func)();` | Le pointeur est converti vers le type qu'implique l'appel : `((void(int)*)d_func)(arg)`. | `lower.c` phase 12 |
| le nom d'une fonction comme valeur : un argument, une entrée de table, `f == g` | `&name` | `lower.c` phase 12 |
| `NULL`, `p = 0`, `if (p)`, `!p`, `p && p->x` | Voir *Le pointeur nul* ci-dessous. | `lower.c` phase 12, `codegen.c` |
| `char buf[sizeof table / sizeof table[0]]`, `sizeof "text"` | `sizeof` est évalué dans les tailles de tableaux (les tailles sont en mots). | `parser.y` |
| `"\033[2J"`, `"\b \b"` | Une chaîne contenant un caractère de contrôle devient un tableau de `char` à la portée du fichier. | `lower.c` phase 12 |
| `struct stats s = { 16, "1x4", 12 };` avec `char s_dmg[13]` | La chaîne devient les caractères du membre, complétés par des zéros. | `lower.c` phase 12 |
| `class`, `new`, `this`, `delete`, `virtual`, ... comme noms | Identifiants ordinaires, écrits avec un tiret bas final (`this_`). | `lexer.l` |
| `auto`, `register`, `(void) f();` | Acceptés et supprimés. | `lexer.l`, `lower.c` |

## Fonctions variadiques

Le C de Vircon32 n'a pas de `...`. Les arguments supplémentaires voyagent comme
un argument de plus : un pointeur vers un tableau de mots.

```c
int sum(int n, ...) {                    int sum(int n, int *__v32_va) {
    va_list ap;                              va_list ap;      /* int * */
    va_start(ap, n);                         ap = __v32_va;
    x = va_arg(ap, int);                     x = *((int *)((ap += 1) - 1));
    va_end(ap);                              ap = ((void *)0);
```

et chaque appel remplit d'abord un tableau qui lui est propre :

```c
msg("%s hits %d", name, dmg);            int [2] __v32_va_tmp0;   /* top of the function */
                                         ...
                                         __v32_va_tmp0[0] = (int)name;
                                         __v32_va_tmp0[1] = (int)dmg;
                                         msg("%s hits %d", __v32_va_tmp0);
```

Un appel sans argument supplémentaire passe un pointeur nul. Une `va_list` peut
être transmise à une autre fonction (`vsprintf(buf, fmt, ap)`).

L'endroit où vont les affectations compte :

- dans la condition d'un `while` ou d'un `for`, elles doivent s'exécuter à
  chaque tour, donc la boucle devient
  `while (true) { <affectations> if (!(cond)) break; ... }` ;
- à droite de `&&` ou de `||`, elles ne doivent s'exécuter que si ce côté est
  évalué, donc l'expression devient un indicateur positionné par des
  instructions `if` placées avant l'instruction ;
- **non pris en charge**, et signalé comme une erreur : un appel variadique avec
  des arguments supplémentaires dans la condition d'un `do ... while`, dans le
  pas d'un `for`, ou dans une branche de `?:`.

Chaque argument supplémentaire est stocké comme un mot : une conversion `(int)`
pour les entiers, caractères, énumérations et pointeurs, et pour un `float` ses
bits tels quels (`((float *)__v32_va_tmp0)[1] = speed;`), que `va_arg(ap, float)`
relit. (Avant cela, un `float` était converti en `int` à l'entrée.) Une
structure passée par valeur à travers `...` n'est pas prise en charge.

Cela fonctionne de même en C++ (`#include <cstdarg>` ou `<stdarg.h>`), dans les
fonctions libres, les espaces de noms et les méthodes. La réécriture s'exécute
après l'analyse sémantique, qui indique quels appels passent par `...` — elle
résout les surcharges (une surcharge `...` passe après toutes les autres, donc
`pick(1)` prend `pick(int)` plutôt que `pick(int, ...)`), les méthodes et les
espaces de noms, et marque les arguments `float` — et rejette un objet passé par
`...` (passez un pointeur vers lui, ou le `c_str()` d'une chaîne). Le nom décoré
d'une fonction variadique se termine par `va` : `Log__print__char_ptr_va`. En C,
un appel que sema n'a pas pu résoudre est quand même reconnu par le nom de la
fonction.

## Le pointeur nul

En C le pointeur nul vaut 0, et les programmes en dépendent : mémoire mise à
zéro, statiques non initialisées, `if (p)`. Le `NULL` du C de Vircon32 vaut -1,
il n'accepte aucun `int` là où un pointeur est attendu, ni aucun pointeur là où
une condition est attendue. Pour l'entrée en C :

- `NULL`, et un `0` littéral affecté à un pointeur, passé comme pointeur ou
  initialisant un pointeur, s'écrivent `((void *)0)` ;
- `p == NULL` et `p != NULL` deviennent `((int)p) == 0` ;
- un pointeur utilisé comme condition (`if`, `while`, `for`, `!`, `&&`, `||`)
  devient `((int)p)`.

Les fonctions propres au SDK parlent toujours en -1 (`malloc` le renvoie en cas
d'échec). `libc/` enveloppe celles qu'appelle un programme C ; ne passez pas
directement un pointeur nul C à une fonction du SDK.

## La bibliothèque C

`libc/` contient la tranche de la bibliothèque standard dont Rogue avait besoin,
plus curses, écrite en C et transpilée avec le programme :

    v32c++ -I <v32c++>/libc ... program.c

(sans `-I` une fois v32c++ installé : l'entrée en C cherche aussi dans le
`libc/` du répertoire d'en-têtes installé,
`/usr/local/Vircon32/v32tools/include/v32c++/libc` par défaut), avec
`#include <v32libc.c>` (et `<v32curses.c>`) une seule fois dans l'unique
fichier du programme. Voir `libc/v32libc.h`.

| Partie | Ce qu'elle apporte à un programme C |
|---|---|
| `v32libc.c` | la famille `printf`, `sscanf`, `malloc`/`free` avec un pointeur nul C, les fonctions de chaînes et de ctype qui manquent au SDK, `time`/`localtime`, `exit` (attend START, puis redémarre la cartouche), `strerror` |
| `v32term.c` | Un écran de texte 80x24 de cellules 8x15 dessinées avec la police du BIOS, avec couleur, vidéo inverse et les blocs d'ombrage (caractères 17-20) ; lecture de la manette ; un clavier à l'écran |
| `v32curses.c` | curses sur cet écran : fenêtres, `move`/`addch`/`printw`/`mvwinch`, `getch`, rafraîchissement |
| `v32file.c` | Fichiers sur la carte mémoire : `fopen`, `fread`, `fwrite`, `getc`, `putc`, `fclose`, `rewind`, `fseek`, `remove`, `rename`, `unlink`, `stat` |

### Fichiers sur la carte mémoire

Les 20 premiers mots de la carte sont la signature de jeu qu'écrit tout jeu
Vircon32 (`v32file_signature`, définie par le programme). Viennent ensuite un
répertoire de 8 fichiers nommés et les fichiers eux-mêmes, chacun une suite
contiguë de mots. Un fichier ouvert est conservé en entier en RAM et réécrit à la
fermeture, au vidage ou au rembobinage, de sorte que les fichiers peuvent
changer de taille librement. Une carte absente ou appartenant à un autre jeu
n'est jamais écrite : `fopen` échoue et `errno` dit pourquoi. Une carte vierge
est formatée à la première écriture.

Les tailles sont en mots : `sizeof(int) == sizeof(char) == 1`. Le code qui écrit
« les 4 octets d'un int » (`fwrite(&n, 1, 4, f)`) lit et écrit trois mots
au-delà de la variable ; il doit dire `sizeof n`. C'est le seul genre de
changement dont le code de sauvegarde de Rogue a eu besoin.

### Une ligne de commande

`main(argc, argv)` reçoit `argc == 1` sauf si le programme définit
`char **v32_main_args(int *argc, char **argv)`, que v32c++ appelle alors en tête
de `main`. L'écran-titre de Rogue est cette fonction : ses entrées de menu
représentent `rogue`, `rogue -r` et `rogue -s`.

## Limites

- Une seule unité de traduction, comme toujours : `static` à la portée du fichier
  ne signifie rien, et les fonctions `static` de même nom de deux fichiers
  entrent en collision.
- `unsigned` est `int` (signalé une fois) ; `char`, `short` et `long` sont un
  mot de 32 bits, donc `sizeof(char) == sizeof(int) == 1`.
- `double` et `long double` sont `float`, l'unique type à virgule flottante de 32
  bits de Vircon32 (le C généré dit `float`).
- Une union ne peut pas être initialisée avec des accolades (C de Vircon32).
- Le code qui suppose que `sizeof(int) == 4` (voir *Fichiers sur la carte
  mémoire*).
- Une affectation composée dont la cible a un effet de bord, utilisée comme
  valeur à l'intérieur d'une expression plus grande, est mal compilée par le C
  de Vircon32 (particularité 25 de `VIRCON32_QUIRKS.fr.md`) ; comme instruction,
  elle est gérée.
- Les déclarations de paramètres de style K&R (`f(a, b) int a; char *b; { }`)
  ne sont pas analysées ; les définitions de style prototype le sont.
- Champs de bits : comme pour C++ (un avertissement, ou `--reject-bit-fields`).
