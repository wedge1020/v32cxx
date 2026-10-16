# std::string

[Version anglaise / in English](STRING.md) | [Version espagnole / en español](STRING.es.md) <!-- langlinks -->

`#include <string>` donne à un programme `std::string`. Un tel fichier n'existe
pas sur le disque : l'en-tête est intégré au transpileur (sa source est
[`lib/string`](../lib/string)) et c'est une classe C++ ordinaire, transpilée
avec le reste du programme.

```cpp
#include <string>

std::string label(int score)
{
    std::string text = "SCORE ";
    text += std::to_string(score);
    return text;
}

// ...
print(label(1500).c_str());
```

## La grande différence : une capacité fixe

Un `std::string` garde ici ses caractères **à l'intérieur de l'objet**, dans un
tableau de `V32_STRING_CAPACITY + 1` caractères. Il n'alloue jamais.

C'est ce qui fait fonctionner la copie. v32c++ n'a pas de constructeurs de
copie : un objet est copié comme de la mémoire brute. Une chaîne qui pointerait
sur de la mémoire du tas serait partagée par ses copies et libérée deux fois.
Avec les caractères dans l'objet, une copie est une vraie copie indépendante, si
bien qu'une chaîne peut être affectée, passée, retournée, stockée dans un
`std::vector` ou dans un membre de classe, exactement comme en C++.

Le prix :

- **Une chaîne qui dépasserait la capacité est tronquée, silencieusement.**
  `size()` ne dépasse jamais `capacity()`.
- La capacité est de **63 caractères** sauf si vous la définissez, avant
  l'`#include` ou sur la ligne de commande :

  ```cpp
  #define V32_STRING_CAPACITY 127
  #include <string>
  ```
  ```sh
  v32c++ -D V32_STRING_CAPACITY=127 game.cpp
  ```
  C'est un réglage unique pour tout le programme.
- Sur Vircon32 un `char` est un mot complet de 32 bits, donc chaque chaîne occupe
  capacité + 2 mots quelle que soit sa longueur, et en copier une les copie tous.
  Passez les chaînes en `const std::string &` quand vous le pouvez, et ne rendez
  pas la capacité plus grande que nécessaire si vous en gardez beaucoup.

## Ce qui existe

| | |
|---|---|
| Construction | `string()`, `string("text")`, `string("text", count)`, `string(count, 'c')`, `string(other, pos, count)` |
| Taille | `size` `length` `empty` `capacity` `max_size` `clear` `resize` `reserve` `shrink_to_fit` (les deux derniers ne font rien) |
| Accès | `s[i]` `at` `front` `back` `c_str` `data` `begin` `end` |
| Ajout | `push_back` `pop_back` `append` (texte, texte+nombre, chaîne, sous-chaîne, nombre+caractère) `+=` (chaîne, texte, caractère) |
| Affectation | `=` (chaîne, texte, caractère), `assign` |
| Édition | `erase(pos, count)` `insert(pos, ...)` `replace(pos, count, ...)` |
| Recherche | `find` `rfind` `find_first_of` `find_first_not_of` `find_last_of` `find_last_not_of` `starts_with` `ends_with` |
| Morceaux | `substr(pos, count)` |
| Comparaison | `compare`, `== != < > <= >=` avec une chaîne ou un texte, le texte pouvant se trouver de chaque côté de `==` et `!=` |
| Concaténation | `a + b`, `a + "text"`, `a + 'c'`, `"text" + a` |
| Nombres | `std::to_string(int)`, `std::to_string(float)` (six décimales), `std::stoi`, `std::stof` |

Un littéral se convertit en chaîne partout où C++ le convertirait :

```cpp
std::string name = "ann";            // initialization
greet("bob");                        // argument, by value or const reference
return "none";                       // return value
Player p("cy", 0);                   // constructor argument
names.push_back("dee");              // into a std::vector<std::string>
```

Le `for` sur intervalle parcourt les caractères (`for (char c : name)`), et
`using namespace std;` (ou `using std::string;`) permet d'écrire `string`.

## Différences avec C++

- Capacité fixe et troncature, ci-dessus.
- **Les tailles et les positions sont des `int`**, et `std::string::npos` vaut
  `-1`. `if (s.find("x") != std::string::npos)` se lit comme d'habitude.
- `at()`, `front()`, `back()`, `substr()` et les autres **ne vérifient pas les
  bornes et ne lèvent pas d'exception** : il n'y a pas d'exceptions. Les
  positions hors limites sont ramenées dans l'intervalle là où c'est peu coûteux
  (`substr`, `erase`, `insert`), et sinon l'indice est votre affaire.
- `stoi` et `stof` ne prennent que la chaîne (pas d'argument de position ni de
  base), lisent tant qu'il y a des chiffres, et renvoient 0 s'il n'y en a pas.
- `std::to_string(float)` écrit toujours six décimales, arrondies comme en C++
  (`0.9999996` donne `1.000000`).
- Les itérateurs sont des `char *`. Il n'y a pas de nom de type
  `string::iterator`, pas d'itérateurs inverses, ni de `size_t`.
- Pas de flux (`<<`, `>>`, `getline`), pas de `string_view`, pas de chaînes
  larges ni UTF, pas de `std::swap`, pas de `+` entre deux littéraux (ce n'est
  pas du C++ non plus).
- Une conversion depuis un littéral **dans la condition ou le pas d'une boucle**
  (`while (count("x") < n)`) est une erreur : construisez la chaîne dans une
  variable avant la boucle. Partout ailleurs, l'objet de conversion est déclaré
  juste avant l'instruction qui en a besoin.
- `using std::string;` rend visible tous les noms de `std`, pas seulement
  `string`.

## Affichage

Les fonctions de texte du SDK prennent un simple pointeur. `c_str()` et `data()`
renvoient `const char *`, et le transpileur retire le `const` lorsque le
pointeur va à une fonction C, de sorte que ceci fonctionne tel quel :

```cpp
print(text.c_str());
print_at(10, 40, text.c_str());
```

## Le modifier

`lib/string` est la source. Après l'avoir modifiée, lancez
`sh tools/embed-header.sh` depuis la racine du projet pour régénérer
`src/stdstring.c`, puis `make`. `tests/114sample.cpp` l'exerce.

## Ce que cela a demandé au transpileur

Outre l'en-tête lui-même, `std::string` a nécessité ce qui suit, le tout de
portée générale :

- **Constructeurs de conversion** : une valeur est convertie vers une classe qui
  peut être construite à partir d'elle, dans une initialisation, un argument et
  un retour. Toute classe dotée d'un constructeur adapté à un argument en
  bénéficie, pas seulement `std::string`.
- L'**affectation par copie implicite** subsiste lorsqu'une classe déclare
  `operator=` pour d'autres types de membre droit.
- Les opérateurs sur un **pointeur** vers un objet (`p[0]`, `p == q`) sont des
  opérations de pointeur, même quand la classe les surcharge.
- Un opérateur libre avec l'objet à **droite** (`"x" + s`), et les opérateurs
  libres ne sont choisis que lorsque les types des opérandes conviennent.
- `char` et `int` se remplacent mutuellement dans la correspondance des
  surcharges, et un argument tableau se réduit à un pointeur.
