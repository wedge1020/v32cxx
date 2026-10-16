# Genéricos a nivel del transpilador: `std::array<T, N>` y `std::vector<T>`

[Versión en inglés / in English](GENERICS.md) | [Versión en francés / en français](GENERICS.fr.md) <!-- langlinks -->

v32c++ no hace plantillas. Lo que ofrece en su lugar es un pequeño conjunto de
contenedores genéricos *incorporados* con una escritura compatible con la STL:
`std::array<T, N>` y `std::vector<T>`. Ambos usan la misma maquinaria.

La técnica es la que usaba el propio C++ antes de que existieran las plantillas
(el `<generic.h>` de cfront): por cada `array<T, N>` distinto que nombra un
programa, el transpilador escribe una clase ordinaria con `T` y `N` rellenados, y
luego trata esa clase exactamente como una escrita a mano.

```cpp
#include <array>

std::array<Enemy, 8> enemies;     // becomes:  array_Enemy_8 enemies;
enemies[0].hp = 3;
for (int i = 0; i < enemies.size(); i++) ...
```

## Qué está soportado

| Miembro | Notas |
| --- | --- |
| `operator[]`, `at(i)` | devuelven `T &`; `at` no comprueba límites (no hay excepciones) |
| `front()`, `back()` | devuelven `T &` |
| `size()`, `max_size()`, `empty()` | `int` / `bool` |
| `data()`, `begin()`, `end()` | `T *` simple |
| `fill(value)` | toma `const T &` |
| `= {1, 2, 3}` | inicialización de agregado, local o global |
| `b = a` | copia del arreglo completo (asignación simple de estructura) |

Tipos de elemento: `int`, `float`, `bool`, `char`, cualquier nombre de
estructura/clase/unión/enumeración (cualificado o no), un puntero a cualquiera de
ellos, u otro `std::array`. Los elementos de un tipo clase se construyen cuando
se construye el arreglo y se destruyen cuando este se destruye
(`tests/105sample.cpp` cubre la maquinaria de arreglos miembro en la que esto se
apoya). La longitud es un literal entero, una constante definida con `#define` o
una constante con nombre como una constante de enumeración.

`using namespace std;` se acepta y permite la escritura simple `array<T, N>`. Una
variable del propio programa llamada `array` no se ve afectada.

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

| Miembro | Notas |
| --- | --- |
| `push_back(value)`, `pop_back()` | la capacidad se duplica (4, 8, 16, ...) |
| `size()`, `capacity()`, `empty()` | `int` / `bool` |
| `operator[]`, `at(i)`, `front()`, `back()` | devuelven `T &`; sin comprobación de límites |
| `data()`, `begin()`, `end()` | `T *` simple |
| `reserve(n)`, `clear()` | `clear` conserva el almacenamiento |
| `resize(n)`, `resize(n, value)` | los elementos nuevos se rellenan con ceros / con copias de `value` |
| `erase(pos)`, `insert(pos, value)` | `pos` es un `T *`: `v.begin() + i` |
| `a = b` | copia los elementos |

Cosas que conviene saber (tests/108sample.cpp las ejercita todas):

- **Los elementos son memoria en bruto.** El vector copia los elementos con una
  asignación simple y nunca ejecuta el constructor ni el destructor de un
  elemento. Los números, punteros, estructuras simples y clases cuyo constructor
  solo rellena campos se comportan como se espera, funciones virtuales
  incluidas. Una clase con un *destructor* recibe una advertencia: el vector no
  lo llamará. Guarda punteros (`std::vector<Enemy *>`) para tales clases.
- **Se rechazan las copias distintas de `a = b`.** `std::vector<T> b = a;`,
  pasar uno por valor y devolver uno por valor son errores: sin constructores de
  copia, cada uno dejaría dos vectores dueños de un mismo búfer. Pasa
  `std::vector<T> &`.
- **El almacenamiento es `malloc()`/`free()`** de `misc.h` de Vircon32. No hay
  excepciones, así que quedarse sin memoria no se notifica. Llama a `reserve` de
  antemano cuando se conozca el tamaño: crecer copia todos los elementos.
- **Una referencia a un elemento de un vector muere cuando este crece**, igual
  que en C++: `Enemy &e = v[0]; v.push_back(x);` deja `e` colgando.
- **`std::vector<std::vector<int> >`** necesita el espacio entre los dos `>` (`>>`
  es aquí el operador de desplazamiento), y sus vectores internos son memoria en
  bruto como cualquier otro elemento: no se construyen.
- Un `std::vector` a nivel de archivo funciona: los globales se construyen ahora
  al comienzo de `main()` (tests/109sample.cpp).

## `for` basado en rango

```cpp
for (Enemy &e : enemies) e.x += e.speed;      // changes land in the vector
for (int n : scores) total += n;              // a copy of each element
for (Shape *s : shapes) s->draw();            // pointer elements
```

Se escribe en el momento del análisis como el bucle con puntero que representa:

```cpp
for (Enemy *it = enemies.begin(); it != enemies.end(); ++it) {
    Enemy &e = *it;
    e.x += e.speed;
}
```

Funciona sobre cualquier cosa con `begin()` y `end()` que devuelvan `T *`:
`std::array`, `std::vector` o una clase propia. `break` y `continue` se
comportan como de costumbre. El tipo del elemento puede escribirse o dejarse a
`auto` (`for (auto &e : enemies)`, `for (const auto &e : enemies)`,
`for (auto e : enemies)` para una copia). Un arreglo C simple no tiene
`begin()`/`end()` a los que llamar, así que no puede ser el rango. A diferencia
de C++, se pide `end()` en cada pasada, y la expresión del rango se evalúa una
vez para `begin()` y una vez por cada `end()`: usa una variable, no una llamada a
función, como rango. `tests/110sample.cpp` lo cubre.

## Los accesores se escriben en el lugar

`v[i]` se resuelve en una llamada de función como cualquier operador
sobrecargado. En esta consola eso cuesta una llamada, un retorno y una
desreferencia por elemento, así que para las clases generadas el transpilador
escribe el cuerpo del accesor allí donde estaría la llamada:

| Fuente | C generado (vector) | (array) |
| --- | --- | --- |
| `v[i]`, `v.at(i)` | `v.m_data[i]` | igual |
| `v.front()` | `v.m_data[0]` | igual |
| `v.back()` | `v.m_data[v.m_size - 1]` | `v.m_data[N - 1]` |
| `v.size()` | `v.m_size` | `N` |
| `v.capacity()` | `v.m_capacity` | |
| `v.empty()` | `(v.m_size == 0)` | `false` |
| `v.data()`, `v.begin()` | `v.m_data` | igual |
| `v.end()` | `(v.m_data + v.m_size)` | `&v.m_data[N]` |

Todo lo que cambia el contenedor (`push_back`, `erase`, `resize`, ...) sigue
siendo una llamada. Medido en el emulador, un bucle que suma un vector de 1000
elementos 300 veces con `v[i]` y `v.size()` tardó unos 19 fotogramas de tiempo de
CPU escrito en el lugar, frente a unos 61 como llamadas.

Un accesor que mencionaría el contenedor dos veces, o ninguna, solo se escribe en
el lugar cuando la expresión del contenedor no contiene ninguna llamada:
`makeList().back()` conserva su llamada, de modo que `makeList()` se sigue
ejecutando una vez. `--no-inline-containers` lo desactiva todo, lo cual es útil
para leer el C generado junto a la clase de la que procede.

## Cómo funciona

1. **`src/prescan.c`** consume `#include <array>` (tal archivo no existe, y la
   línea no debe llegar al C generado) y activa la característica.
2. **`src/lexer.l`** devuelve un único token, `STD_ARRAY`, para `std::array`
   (`STD_VECTOR` para `std::vector`).
3. **`src/parser.y`** tiene una alternativa extra de `type_spec`,
   `STD_ARRAY '<' type ',' length '>'`. Como las cuatro palabras clave de
   conversión, es una forma fija, no una regla general `name<args>`, de modo que
   `<` y `>` nunca compiten con los operadores de comparación. Su acción pide a
   `generic.c` el nombre de la clase y lo usa como tipo.
4. **`src/generic.c`** guarda la clase como texto fuente de C++ con
   marcadores `@T@`, `@N@` y `@NAME@`. Tras el análisis principal escribe una
   copia por instanciación, ejecuta el mismo analizador sobre ese texto, e
   inserta cada clase justo antes de la primera declaración de nivel superior que
   la usa.

Un contenedor nombrado dentro de un espacio de nombres (`std::vector<Bullet *>`
dentro de `namespace si`, donde `Bullet` está declarado) se genera dentro de ese
mismo espacio de nombres, de modo que el tipo del elemento significa lo que
significaba donde se escribió (`tests/111sample.cpp`).

A partir de ahí nada sabe que la clase fue generada. Para ver exactamente lo que
obtuvo un programa, lee `ARRAY_TEMPLATE` y `VECTOR_TEMPLATE` en `src/generic.c`, o
mira las estructuras y funciones `array_*` del C generado.

Nombres: `array_<T>_<N>`, con `::` y `*` escritos `_` y `_ptr`
(`std::array<Enemy *, 2>` es `array_Enemy_ptr_2`; `std::vector<int>` es
`vector_int`). Para un tipo de elemento puntero, el texto generado usa un nombre
de `typedef`, porque esta gramática no tiene un declarador `T *&`; una vez
analizado, el `typedef` se sustituye por el tipo puntero real y nunca llega al C
generado.

## Detalles específicos de Vircon32

- `= {1, 2, 3}` se emite como `{{1, 2, 3}}`. El C de Vircon32 no permite omitir
  las llaves internas («too many values to assign to structure»).
- `size()` se escribe con `sizeof` en lugar de `return N;`. El compilador de C de
  Vircon32 avisa por cada argumento sin usar, y un `size()` que ignora `this`
  produce tres de esas advertencias por instanciación, lo que pronto alcanza el
  límite de advertencias del compilador.
- `f()[i].member` se emite como `(*(f() + i)).member`. El compilador de C de
  Vircon32 se detiene con «cannot emit memory placement when an expression has
  none» ante la forma directa. Esto se aplica a cualquier programa, no solo a
  `data()[i]`.

## En un programa real

`demos/cxx/space_invaders` usa ambos, por valor: las bombas de los alienígenas
son un `std::vector<Bullet>` rellenado con `mBombs.push_back(Bullet(...))` (antes
llevaba su propia clase `BombList` de punteros para esto) y los búnkeres un
`std::array<Bunker, BUNKER_COUNT>`. El dibujo y las colisiones usan `for` basado
en rango; los bucles que borran bombas por el camino son bucles con índice.

## Carencias conocidas

- **Los arreglos miembro multidimensionales de objetos de clase** (`Counter
  grid[2][2];` como miembro) siguen sin construirse. Un `std::array` de
  `std::array` está bien: cada nivel es su propia clase.
- **Las longitudes con nombre se comparan por nombre.** `std::array<int, MAX>` y
  `std::array<int, 8>` son tipos distintos incluso cuando `MAX` vale 8. Un
  `#define` no se ve afectado (se expande antes del análisis).
- Sin nombres de tipo `::iterator` (los iteradores son `T *`; `auto it =
  v.begin();` funciona), y sin `for` basado en rango sobre un arreglo C simple.
- `auto` es para variables locales con un inicializador `= expresión`, y para el
  `for` basado en rango. No está soportado para globales, miembros, parámetros ni
  tipos de retorno.
