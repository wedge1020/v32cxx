# Génériques au niveau du transpileur : `std::array<T, N>` et `std::vector<T>`

[Version anglaise / in English](GENERICS.md) | [Version espagnole / en español](GENERICS.es.md) <!-- langlinks -->

v32c++ ne fait pas de gabarits (*templates*). Ce qu'il offre à la place, c'est un
petit ensemble de conteneurs génériques *intégrés* à l'écriture compatible avec
la STL : `std::array<T, N>` et `std::vector<T>`. Les deux utilisent la même
mécanique.

La technique est celle qu'employait C++ lui-même avant l'existence des gabarits
(le `<generic.h>` de cfront) : pour chaque `array<T, N>` distinct qu'un programme
nomme, le transpileur écrit une classe ordinaire avec `T` et `N` renseignés,
puis traite cette classe exactement comme une classe écrite à la main.

```cpp
#include <array>

std::array<Enemy, 8> enemies;     // becomes:  array_Enemy_8 enemies;
enemies[0].hp = 3;
for (int i = 0; i < enemies.size(); i++) ...
```

## Ce qui est pris en charge

| Membre | Notes |
| --- | --- |
| `operator[]`, `at(i)` | renvoient `T &` ; `at` ne vérifie pas les bornes (pas d'exceptions) |
| `front()`, `back()` | renvoient `T &` |
| `size()`, `max_size()`, `empty()` | `int` / `bool` |
| `data()`, `begin()`, `end()` | simples `T *` |
| `fill(value)` | prend `const T &` |
| `= {1, 2, 3}` | initialisation d'agrégat, locale ou globale |
| `b = a` | copie du tableau entier (simple affectation de structure) |

Types d'élément : `int`, `float`, `bool`, `char`, tout nom de
structure/classe/union/énumération (qualifié ou non), un pointeur vers l'un de
ceux-ci, ou un autre `std::array`. Les éléments d'un type classe sont construits
quand le tableau l'est et détruits quand il l'est (`tests/105sample.cpp` couvre
la mécanique des tableaux membres sur laquelle cela repose). La longueur est un
littéral entier, une constante définie par `#define`, ou une constante nommée
comme une constante d'énumération.

`using namespace std;` est accepté et permet l'écriture simple `array<T, N>`. La
variable propre au programme nommée `array` n'est pas affectée.

## `std::vector<T>`

```cpp
#include <vector>

std::vector<Enemy> enemies;               // becomes:  vector_Enemy enemies;
enemies.push_back(Enemy(120, 40, 3));
for (int i = 0; i < enemies.size(); ) {
    if (enemies[i].dead()) enemies.erase(enemies.begin() + i);
    else i++;
}
```

| Membre | Notes |
| --- | --- |
| `push_back(value)`, `pop_back()` | la capacité double (4, 8, 16, ...) |
| `size()`, `capacity()`, `empty()` | `int` / `bool` |
| `operator[]`, `at(i)`, `front()`, `back()` | renvoient `T &` ; pas de vérification des bornes |
| `data()`, `begin()`, `end()` | simples `T *` |
| `reserve(n)`, `clear()` | `clear` conserve le stockage |
| `resize(n)`, `resize(n, value)` | les nouveaux éléments sont remplis de zéros / de copies de `value` |
| `erase(pos)`, `insert(pos, value)` | `pos` est un `T *` : `v.begin() + i` |
| `a = b` | copie les éléments |

À savoir (tests/108sample.cpp exerce tout cela) :

- **Les éléments sont de la mémoire brute.** Le vecteur copie les éléments par
  simple affectation et n'exécute jamais le constructeur ni le destructeur d'un
  élément. Les nombres, les pointeurs, les structures simples et les classes dont
  le constructeur ne fait que remplir des champs se comportent comme prévu, y
  compris avec des fonctions virtuelles. Une classe dotée d'un *destructeur*
  reçoit un avertissement : le vecteur ne l'appellera pas. Conservez des
  pointeurs (`std::vector<Enemy *>`) pour de telles classes.
- **Les copies autres que `a = b` sont rejetées.** `std::vector<T> b = a;`, en
  passer un par valeur et en retourner un par valeur sont des erreurs : sans
  constructeurs de copie, chacune laisserait deux vecteurs propriétaires d'un même
  tampon. Passez `std::vector<T> &`.
- **Le stockage est `malloc()`/`free()`** de `misc.h` de Vircon32. Il n'y a pas
  d'exceptions, donc un manque de mémoire n'est pas signalé. Appelez `reserve` au
  préalable quand la taille est connue : agrandir copie chaque élément.
- **Une référence vers un élément d'un vecteur meurt quand il grandit**, tout
  comme en C++ : `Enemy &e = v[0]; v.push_back(x);` laisse `e` pendante.
- **`std::vector<std::vector<int> >`** exige l'espace entre les deux `>` (`>>`
  est ici l'opérateur de décalage), et ses vecteurs internes sont de la mémoire
  brute comme tout autre élément : non construits.
- Un `std::vector` à la portée du fichier fonctionne : les globales sont
  désormais construites en tête de `main()` (tests/109sample.cpp).

## `for` sur intervalle

```cpp
for (Enemy &e : enemies) e.x += e.speed;      // changes land in the vector
for (int n : scores) total += n;              // a copy of each element
for (Shape *s : shapes) s->draw();            // pointer elements
```

Écrit à l'analyse sous la forme de la boucle à pointeur qu'il représente :

```cpp
for (Enemy *it = enemies.begin(); it != enemies.end(); ++it) {
    Enemy &e = *it;
    e.x += e.speed;
}
```

Cela fonctionne sur tout ce qui a `begin()` et `end()` renvoyant `T *` :
`std::array`, `std::vector`, ou une classe de votre cru. `break` et `continue` se
comportent comme d'habitude. Le type de l'élément peut être écrit ou laissé à
`auto` (`for (auto &e : enemies)`, `for (const auto &e : enemies)`,
`for (auto e : enemies)` pour une copie). Un simple tableau C n'a pas de
`begin()`/`end()` à appeler, donc il ne peut pas servir d'intervalle. À la
différence de C++, `end()` est demandé à chaque passage, et l'expression de
l'intervalle est évaluée une fois pour `begin()` et une fois par `end()` :
utilisez une variable, pas un appel de fonction, comme intervalle.
`tests/110sample.cpp` le couvre.

## Les accesseurs sont écrits sur place

`v[i]` se résout en un appel de fonction comme n'importe quel opérateur surchargé.
Sur cette console, cela coûte un appel, un retour et un déréférencement par
élément ; aussi, pour les classes générées, le transpileur écrit le corps de
l'accesseur à l'endroit où serait l'appel :

| Source | C généré (vector) | (array) |
| --- | --- | --- |
| `v[i]`, `v.at(i)` | `v.m_data[i]` | idem |
| `v.front()` | `v.m_data[0]` | idem |
| `v.back()` | `v.m_data[v.m_size - 1]` | `v.m_data[N - 1]` |
| `v.size()` | `v.m_size` | `N` |
| `v.capacity()` | `v.m_capacity` | |
| `v.empty()` | `(v.m_size == 0)` | `false` |
| `v.data()`, `v.begin()` | `v.m_data` | idem |
| `v.end()` | `(v.m_data + v.m_size)` | `&v.m_data[N]` |

Tout ce qui modifie le conteneur (`push_back`, `erase`, `resize`, ...) reste un
appel. Mesuré sur l'émulateur, une boucle sommant 300 fois un vecteur de 1000
éléments avec `v[i]` et `v.size()` a pris environ 19 images de temps CPU écrite
sur place, contre environ 61 sous forme d'appels.

Un accesseur qui mentionnerait le conteneur deux fois, ou pas du tout, n'est
écrit sur place que si l'expression du conteneur ne contient aucun appel :
`makeList().back()` garde son appel, si bien que `makeList()` ne s'exécute
toujours qu'une fois. `--no-inline-containers` désactive le tout, ce qui est
utile pour lire le C généré à côté de la classe dont il provient.

## Comment cela fonctionne

1. **`src/prescan.c`** consomme `#include <array>` (un tel fichier n'existe pas,
   et la ligne ne doit pas atteindre le C généré) et active la fonctionnalité.
2. **`src/lexer.l`** renvoie un seul jeton, `STD_ARRAY`, pour `std::array`
   (`STD_VECTOR` pour `std::vector`).
3. **`src/parser.y`** a une alternative supplémentaire de `type_spec`,
   `STD_ARRAY '<' type ',' length '>'`. Comme les quatre mots-clés de conversion,
   c'est une forme fixe, pas une règle générale `name<args>`, si bien que `<` et
   `>` ne rivalisent jamais avec les opérateurs de comparaison. Son action
   demande à `generic.c` le nom de la classe et l'utilise comme type.
4. **`src/generic.c`** détient la classe sous forme de texte source C++ avec des
   marqueurs `@T@`, `@N@` et `@NAME@`. Après l'analyse principale, il écrit une
   copie par instanciation, fait passer le même analyseur sur ce texte, et insère
   chaque classe juste avant la première déclaration de niveau supérieur qui
   l'utilise.

Un conteneur nommé dans un espace de noms (`std::vector<Bullet *>` dans
`namespace si`, où `Bullet` est déclaré) est généré dans ce même espace de noms,
de sorte que le type de l'élément signifie ce qu'il signifiait là où il a été
écrit (`tests/111sample.cpp`).

À partir de là, plus rien ne sait que la classe a été générée. Pour voir
exactement ce qu'un programme a reçu, lisez `ARRAY_TEMPLATE` et
`VECTOR_TEMPLATE` dans `src/generic.c`, ou regardez les structures et fonctions
`array_*` du C généré.

Noms : `array_<T>_<N>`, avec `::` et `*` écrits `_` et `_ptr`
(`std::array<Enemy *, 2>` est `array_Enemy_ptr_2` ; `std::vector<int>` est
`vector_int`). Pour un type d'élément pointeur, le texte généré utilise un nom de
`typedef`, parce que cette grammaire n'a pas de déclarateur `T *&` ; une fois
analysé, le `typedef` est remplacé par le vrai type pointeur et n'atteint jamais
le C généré.

## Détails propres à Vircon32

- `= {1, 2, 3}` est émis sous la forme `{{1, 2, 3}}`. Le C de Vircon32 n'autorise
  pas à omettre les accolades internes (« too many values to assign to
  structure »).
- `size()` est écrit avec `sizeof` plutôt que `return N;`. Le compilateur C de
  Vircon32 avertit pour chaque argument inutilisé, et un `size()` qui ignore
  `this` produit trois de ces avertissements par instanciation, ce qui atteint
  vite la limite d'avertissements du compilateur.
- `f()[i].member` est émis sous la forme `(*(f() + i)).member`. Le compilateur C
  de Vircon32 s'arrête sur « cannot emit memory placement when an expression has
  none » devant la forme directe. Cela vaut pour tout programme, pas seulement
  pour `data()[i]`.

## Dans un vrai programme

`demos/cxx/space_invaders` utilise les deux, par valeur : les bombes des
extraterrestres sont un `std::vector<Bullet>` rempli avec
`mBombs.push_back(Bullet(...))` (il embarquait auparavant sa propre classe
`BombList` de pointeurs pour cela) et les bunkers un
`std::array<Bunker, BUNKER_COUNT>`. Le dessin et les collisions utilisent le
`for` sur intervalle ; les boucles qui effacent des bombes en chemin sont des
boucles à indice.

## Lacunes connues

- **Les tableaux membres multidimensionnels d'objets de classe** (`Counter
  grid[2][2];` comme membre) ne sont toujours pas construits. Un `std::array` de
  `std::array` convient : chaque niveau est sa propre classe.
- **Les longueurs nommées sont comparées par leur nom.** `std::array<int, MAX>` et
  `std::array<int, 8>` sont des types différents même quand `MAX` vaut 8. Un
  `#define` n'est pas concerné (il est développé avant l'analyse).
- Pas de noms de type `::iterator` (les itérateurs sont des `T *` ; `auto it =
  v.begin();` fonctionne), et pas de `for` sur intervalle sur un simple tableau C.
- `auto` sert aux variables locales avec un initialiseur `= expression`, et au
  `for` sur intervalle. Il n'est pas pris en charge pour les globales, les
  membres, les paramètres ni les types de retour.
