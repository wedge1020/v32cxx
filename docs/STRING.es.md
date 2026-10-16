# std::string

[Versión en inglés / in English](STRING.md) | [Versión en francés / en français](STRING.fr.md) <!-- langlinks -->

`#include <string>` da a un programa `std::string`. No existe tal archivo en el
disco: la cabecera está incorporada en el transpilador (su fuente es
[`lib/string`](../lib/string)) y es una clase de C++ corriente, transpilada
junto con el resto del programa.

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

## La gran diferencia: una capacidad fija

Un `std::string` aquí guarda sus caracteres **dentro del objeto**, en un arreglo
de `V32_STRING_CAPACITY + 1` caracteres. Nunca reserva memoria.

Eso es lo que hace que la copia funcione. v32c++ no tiene constructores de copia:
un objeto se copia como memoria en bruto. Una cadena que apuntara a memoria del
montón sería compartida por sus copias y liberada dos veces. Con los caracteres
dentro del objeto, una copia es una copia real e independiente, de modo que una
cadena puede asignarse, pasarse, devolverse, almacenarse en un `std::vector` o en
un miembro de una clase, exactamente como en C++.

El precio:

- **Una cadena que fuera a crecer más allá de la capacidad se trunca, en
  silencio.** `size()` nunca supera a `capacity()`.
- La capacidad es de **63 caracteres** a menos que la establezcas, antes del
  `#include` o en la línea de comandos:

  ```cpp
  #define V32_STRING_CAPACITY 127
  #include <string>
  ```
  ```sh
  v32c++ -D V32_STRING_CAPACITY=127 game.cpp
  ```
  Es un único ajuste para todo el programa.
- En Vircon32 un `char` es una palabra completa de 32 bits, así que cada cadena
  ocupa capacidad + 2 palabras sea cual sea su longitud, y copiarla copia todas
  ellas. Pasa las cadenas como `const std::string &` siempre que puedas, y no
  hagas la capacidad mayor de lo que necesites si guardas muchas.

## Qué hay

| | |
|---|---|
| Construcción | `string()`, `string("text")`, `string("text", count)`, `string(count, 'c')`, `string(other, pos, count)` |
| Tamaño | `size` `length` `empty` `capacity` `max_size` `clear` `resize` `reserve` `shrink_to_fit` (las dos últimas no hacen nada) |
| Acceso | `s[i]` `at` `front` `back` `c_str` `data` `begin` `end` |
| Añadir | `push_back` `pop_back` `append` (texto, texto+cantidad, cadena, subcadena, cantidad+carácter) `+=` (cadena, texto, carácter) |
| Asignar | `=` (cadena, texto, carácter), `assign` |
| Editar | `erase(pos, count)` `insert(pos, ...)` `replace(pos, count, ...)` |
| Buscar | `find` `rfind` `find_first_of` `find_first_not_of` `find_last_of` `find_last_not_of` `starts_with` `ends_with` |
| Trozos | `substr(pos, count)` |
| Comparar | `compare`, `== != < > <= >=` contra una cadena o un texto, con el texto a cualquiera de los lados de `==` y `!=` |
| Unir | `a + b`, `a + "text"`, `a + 'c'`, `"text" + a` |
| Números | `std::to_string(int)`, `std::to_string(float)` (seis decimales), `std::stoi`, `std::stof` |

Un literal se convierte en cadena dondequiera que C++ lo convertiría:

```cpp
std::string name = "ann";            // initialization
greet("bob");                        // argument, by value or const reference
return "none";                       // return value
Player p("cy", 0);                   // constructor argument
names.push_back("dee");              // into a std::vector<std::string>
```

El `for` basado en rango recorre los caracteres (`for (char c : name)`), y
`using namespace std;` (o `using std::string;`) te permite escribir `string`.

## Diferencias con C++

- Capacidad fija y truncamiento, arriba.
- **Los tamaños y las posiciones son `int`**, y `std::string::npos` es `-1`.
  `if (s.find("x") != std::string::npos)` se lee como de costumbre.
- `at()`, `front()`, `back()`, `substr()` y el resto **no comprueban límites ni
  lanzan excepciones**: no hay excepciones. Las posiciones fuera de rango se
  recortan donde es barato hacerlo (`substr`, `erase`, `insert`), y en los demás
  casos el índice es cosa tuya.
- `stoi` y `stof` toman solo la cadena (sin argumentos de posición ni de base),
  leen hasta donde llegan los dígitos y devuelven 0 si no hay dígitos.
- `std::to_string(float)` siempre escribe seis decimales, redondeados como C++
  (`0.9999996` es `1.000000`).
- Los iteradores son `char *`. No hay un nombre de tipo `string::iterator`, ni
  iteradores inversos, ni `size_t`.
- Sin flujos (`<<`, `>>`, `getline`), sin `string_view`, sin cadenas anchas ni
  UTF, sin `std::swap`, sin `+` entre dos literales (eso tampoco es C++).
- Una conversión desde un literal **en la condición o el paso de un bucle**
  (`while (count("x") < n)`) es un error: construye la cadena en una variable
  antes del bucle. En todos los demás sitios el objeto de conversión se declara
  justo antes de la sentencia que lo necesita.
- `using std::string;` hace visibles todos los nombres de `std`, no solo
  `string`.

## Imprimir

Las funciones de texto del SDK toman un puntero simple. `c_str()` y `data()`
devuelven `const char *`, y el transpilador quita el `const` cuando el puntero va
a una función de C, de modo que esto funciona tal como está escrito:

```cpp
print(text.c_str());
print_at(10, 40, text.c_str());
```

## Cambiarlo

`lib/string` es la fuente. Tras editarla, ejecuta `sh tools/embed-header.sh`
desde la raíz del proyecto para regenerar `src/stdstring.c`, y luego `make`.
`tests/114sample.cpp` lo ejercita.

## Qué costó en el transpilador

Además de la cabecera en sí, `std::string` necesitó lo siguiente, todo ello de
carácter general:

- **Constructores de conversión**: un valor se convierte a una clase que pueda
  construirse a partir de él, en una inicialización, un argumento y un retorno.
  Cualquier clase con un constructor adecuado de un argumento obtiene esto, no
  solo `std::string`.
- La **asignación de copia implícita** sobrevive a que una clase declare
  `operator=` para otros tipos de lado derecho.
- Los operadores sobre un **puntero** a un objeto (`p[0]`, `p == q`) son
  operaciones de puntero, incluso cuando la clase los sobrecarga.
- Un operador libre con el objeto a la **derecha** (`"x" + s`), y los
  operadores libres solo se eligen cuando los tipos de los operandos encajan.
- `char` e `int` se sustituyen mutuamente en la comparación de sobrecargas, y un
  argumento arreglo decae a puntero.
