# Peculiaridades de la salida específicas de Vircon32

[Versión en inglés / in English](VIRCON32_QUIRKS.md) | [Versión en francés / en français](VIRCON32_QUIRKS.fr.md) <!-- langlinks -->

Todos los lugares donde el C generado por este proyecto se aparta deliberadamente
del C estándar, en una sola lista, organizada por peculiaridad y no por el momento
en que se descubrió. `docs/DESIGN_NOTES.md` (solo en inglés) tiene la historia
completa, ronda por ronda, de *cómo* se encontró cada una (normalmente por las
malas, frente al compilador real); este documento existe con un propósito más
estrecho: una lista de comprobación para cuando se construya un modo de salida
`--standard-c` (o similar), de modo que ese trabajo consista en «recorrer esta
lista y hacer condicional cada entrada» en lugar de volver a investigar todo el
código.

**La opción `--target` ya existe** (`vircon32`/`v32`, el valor por defecto, o
`standard`/`std`) — esta lista de comprobación hizo exactamente aquello para lo
que se escribió. Cada entrada de abajo está ahora marcada como IMPLEMENTADA, AÚN
NO IMPLEMENTADA (una carencia real y deliberada) o N/D (no hizo falta cambiar
nada). Véase la propia entrada de `docs/DESIGN_NOTES.md` sobre esta ronda para la
historia completa, incluidos dos errores reales descubiertos solo probando la
salida real de `--target=standard` en lugar de fiarse del plan sobre el papel.

**Convención de abajo**: «Vircon32 requiere» es la forma no estándar que este
proyecto emite actualmente siempre. «C estándar» es lo que en cambio espera/acepta
un compilador de C simple y portable. «Estado» dice si el comportamiento de
Vircon32 se ha confirmado con el compilador real o solo se ha deducido por
razonamiento. «Dónde» nombra la o las funciones que necesitarían una rama
condicional.

---

## Qué es este documento — y qué no es

**Estas entradas describen el C de Vircon32, el lenguaje de SALIDA, no lo que
puedes escribir.** Tu C++ (o C) sigue siendo corriente: v32c++ reescribe cada
construcción de abajo que el C de Vircon32 no puede aceptar, de modo que ninguna
es una restricción para tu entrada a menos que la tabla diga lo contrario. Los
encabezados enuncian la regla del C de Vircon32 («sin operador ternario») porque
la regla es aquello que la reescritura sortea; cada entrada dice qué hace v32c++
al respecto. Para lo que puedes y no puedes ESCRIBIR, lee la tabla de abajo y las
«Limitaciones conocidas» del README para el lenguaje en sí.

**También son válidos, y no necesitan entrada alguna:** los comentarios `//` y
`/* */` (una barra invertida al final de una línea `//` continúa el comentario,
como en C++), los literales de cadena adyacentes (`"ab" "cd"`, entre líneas o
alrededor de un comentario), una barra invertida seguida de salto de línea dentro
de un literal o de un `#define`, las macros de tipo función, el operador ternario,
el operador coma, los literales hexadecimales/octales/binarios, `1e3`, `.5` y `5.`
(`tests/133sample.cpp`, `134sample.c` y `135sample.cpp` los comprueban con la
cadena de herramientas y el emulador reales).

| # | Regla del C de Vircon32 | En tu código | Estado |
|---|---|---|---|
| 1 | sin palabra clave `struct` en un tipo | escribe C++ como de costumbre | manejado |
| 2 | declaradores de arreglo `type[n] name` | `int a[4]` o `int[4] a` | manejado |
| 3, 4 | sintaxis invertida de punteros a función | forma estándar o de Vircon32 | manejado |
| 5 | el puntero nulo necesita `NULL` | `p = 0;`, `p == 0` | manejado |
| 6 | solo `void main(void)` | `int main()` y `return 0;` | manejado |
| 7 | los parámetros de prototipos necesitan nombres | nombres opcionales | manejado |
| 8 | sin `new`/`delete` | `new`/`delete` funcionan (con `malloc`); `new T[n]` omite los constructores de cada elemento | manejado, con salvedades |
| 9 | `misc.h` para `malloc`/`rand`... | `<cstdlib>`, `<cstring>` | manejado |
| 10 | sin operador ternario | `?:` en cualquier lugar | **bajado** |
| 11 | parámetros y retornos de una palabra | estructuras y clases por valor | **bajado** (las uniones de más de una palabra siguen sin poder) |
| 12 | sin retornos de puntero ni de referencia | devuélvelos libremente | **corregido** |
| 13 | sin estructuras vacías | permitidas | **corregido** |
| 14 | `>>` no extiende el signo | `-8 >> 1` es `-4`; `unsigned` desplaza lógicamente | **bajado** (`--no-signed-shift` lo desactiva) |
| 15 | tipos antes de su uso | como en C++ | n/d |
| 16 | sin exponente en literales de coma flotante | `1e3`, `1.5e-3f`, `.5`, `5.` | manejado (se imprime en coma fija) |
| 17 | pocos escapes | octal, `\0`, `\x` | manejado |
| 18 | sin declaraciones tras una etiqueta `case` | pon llaves al cuerpo del case | **tu código** |
| 19 | tamaños de arreglo constantes | macros, enumeraciones, aritmética constante | manejado |
| 21 | sin operador coma | `a, b` en cualquier lugar, `for(i=0,j=n;..;i++,j--)` | **bajado** |
| 22 | un arreglo decae solo en `=`, inicialización, argumento | `arr + n`, `*arr`, `p == arr` | **bajado** |
| 23 | `const` acompaña a un valor que se lee | lee valores const libremente | **bajado** (no un descarte de `const T*` a `T*`) |
| 24 | sin `static`, `extern`, `volatile`, `unsigned`... | aceptados; `unsigned` es con signo (advertencia); los miembros `static` de clases y los campos de bits sin nombre son errores | manejado, con salvedades |
| 25 | `a op= b` evalúa el destino dos veces | cualquiera | **bajado** como sentencia; como valor dentro de una expresión mayor: advertencia |
| 26 | comparación de puntero con `void *` | entrada en C: manejado | manejado |
| 27 | escribir a través de un puntero devuelto ejecuta la llamada 2-3 veces | cualquiera | **bajado** como sentencia |
| 28 | los códigos de control en literales de cadena no pueden ensamblarse | cualquier literal, `"\025"` | **bajado** |

No cubierto por ninguna entrada porque v32c++ no tiene solución: plantillas,
excepciones, RTTI, herencia múltiple, `enum class`, `constexpr`, lambdas,
`mutable`, clases anidadas, miembros `static` de clases — véanse las «Limitaciones
conocidas» del README.

---

## 1. Sin palabra clave `struct` en una referencia a un tipo

- **Vircon32 requiere**: `struct` solo en una declaración adelantada
  (`struct Name;`) o en una definición completa (`struct Name { ... };`). Toda
  otra referencia — un campo puntero, un parámetro, una declaración local, una
  conversión, un `sizeof` — debe ser el nombre simple (`Name *next;`, nunca
  `struct Name *next;`). Esto es cierto desde la primera declaración adelantada
  en adelante, no solo después de que aparezca la definición completa.
- **C estándar**: `struct Name` es válido dondequiera que se nombra un tipo, con
  o sin `typedef`.
- **Estado**: Confirmado con el compilador real (`struct Name n;` da el error
  «expected '{'»; la primera versión de `emit_vtable_instance` tropezó
  exactamente con este error — véase DESIGN_NOTES.md).
- **Dónde**: `print_type()` (codegen.c) es la única función por la que pasa toda
  emisión de referencia a un tipo, precisamente para que esta regla solo
  necesitara cambiar en un lugar. `emit_vtable_struct`, `emit_classes` y
  `emit_forward_declarations` conservan correctamente `struct` por sí mismas
  (emiten definiciones/declaraciones adelantadas, no referencias) y seguirían
  haciéndolo también en modo C estándar, ya que el C estándar acepta
  `struct Name { ... };` del mismo modo.
- **Para un modo C estándar**: `print_type` necesitó AÑADIR `struct` a una
  referencia de clase en modo estándar (mediante `type_to_class`, confirmando que
  el nombre es realmente una clase y no un primitivo/typedef) y no al revés — la
  propia predicción original de esta lista («no hace falta cambiar nada... en
  realidad la entrada más barata de toda la lista») resultó errónea una vez
  construido de verdad. La verdadera sorpresa: varias OTRAS funciones construyen
  a mano el nombre de tipo propio de una clase — `emit_new_delete_runtime`,
  `emit_array_new_runtime`, `emit_vtable_struct`, `emit_vtable_instance`,
  `emit_method_prototype` — saltándose por completo `print_type`, de modo que
  arreglar solo `print_type` las pasó por alto en silencio. Se descubrió solo
  ejecutando de verdad `--target=standard` contra una prueba con clases y leyendo
  la salida (`struct Shape *v32_new_Shape...` junto a un `Shape *self = ...` sin
  prefijo unas líneas más abajo), no volviendo a derivar cada punto de llamada
  del plan sobre el papel. Se arregló con una nueva función auxiliar
  `print_class_type_name`, aplicada en cada uno de esos puntos de llamada —
  **IMPLEMENTADO**.

## 2. Declaradores de arreglo invertidos

- **Vircon32 requiere**: `int [8] myarray;` — la longitud antes del nombre.
- **C estándar**: `int myarray[8];` — la longitud después.
- **Estado**: CONFIRMADO con el compilador real — construcción, transpilación y
  compilación limpias de la prueba que usa arreglos (`tests/sample26.c`, informe
  de Matthew). Los parámetros de función (decaimiento a puntero) y las listas de
  inicializadores de arreglos (`= {1, 2, 3}`), ambos excluidos originalmente del
  alcance aquí, están ahora TAMBIÉN implementados (una ronda posterior) — véanse
  los comentarios de gramática de `var_decl` y `AST_INIT_LIST` de ast.h. La
  sintaxis del VALOR de la lista de inicializadores en sí (`{1, 2, 3}`) es
  posicional, no la designada de C99 — con el mismo razonamiento que la elección
  de sintaxis de conversión de la entrada 4 más abajo: menos piezas
  independientes de sintaxis específica de Vircon32 sin confirmar en las que
  equivocarse a la vez. Esa pieza concreta está SIN CONFIRMAR en el momento de
  escribir esto (toda esta ronda posterior se escribió sin bison disponible, la
  misma limitación que el trabajo original de los declaradores de arreglo) —
  necesita la propia compilación de Matthew, como todo cambio de gramática en
  este proyecto antes de darse por asentado.
- **Dónde**: `print_type()` (codegen.c) emite `ElementType [N]` para un nodo
  `AST_ARRAY_TYPE`; cada punto de llamada ya añade después el nombre de la
  variable igual que con cualquier otro tipo, de modo que ningún llamador
  necesitó cambiar en absoluto una vez que `print_type` manejó el nuevo tipo.
  El `var_decl` de `parser.y` acepta declaradores de arreglo en el lado de la
  entrada en AMBAS formas: la del C estándar (longitud tras el nombre) y una
  forma alternativa al estilo nativo de Vircon32 (longitud antes del nombre) —
  véase la entrada de seguimiento de peculiaridades de abajo, «Dos formas de
  entrada aceptadas en el lado de C++». `param` acepta por separado
  `int arr[]`/`int arr[8]`, decayendo directamente a un tipo puntero corriente en
  el momento del análisis (semántica exacta de C/C++ real) — no interviene
  ningún AST_ARRAY_TYPE para un parámetro en absoluto, así que no hizo falta más
  trabajo de sema/lower/codegen para esa pieza concreta.
- **Para un modo C estándar**: esta predicción se cumplió exactamente — la propia
  arquitectura de `print_type` («prefijo, y luego el llamador añade el nombre»)
  realmente no puede producir `ElementType name[N]` por sí sola, ya que el
  corchete tiene que ir DESPUÉS del nombre, no antes. Se arregló con una nueva
  función auxiliar `print_array_suffix`, llamada por el LLAMADOR que imprime el
  nombre — se encontró rastreando todos los puntos de llamada de `print_type` en
  codegen.c y confirmando que solo dos la necesitan realmente
  (`print_var_decl_inline`, que cubre tanto variables locales como globales; el
  bucle de campos de datos de `emit_struct`) — el tipo de retorno de una función
  y el tipo de cada parámetro nunca pueden ser arreglos en absoluto (es ilegal
  devolver un arreglo por valor en C/C++; la propia gramática de `param` hace
  decaer un parámetro arreglo a puntero en el análisis, así que nunca se
  construye un nodo `AST_ARRAY_TYPE` para uno). Los arreglos multidimensionales
  (`AST_ARRAY_TYPE` anidados) funcionan correctamente en ambos modos sin código
  extra — **IMPLEMENTADO**. La sintaxis de la lista de inicializadores en sí no
  necesitó ningún cambio, exactamente como se predijo — `{1, 2, 3}` es también C
  estándar válido sin modificar.

## Dos formas de entrada aceptadas en el lado de C++ para los arreglos (no es en sí una peculiaridad de Vircon32 frente al C estándar, pero sí una decisión de diseño relacionada que conviene seguir en el mismo lugar)

Matthew preguntó si la entrada del lado de C++ podía aceptar O BIEN la sintaxis
estándar de arreglos (`int scores[8];`) O BIEN el estilo nativo propio de Vircon32
(`int [8] scores;`) como entrada válida, específicamente para que alguien ya
fluido en el C de Vircon32 — o en transición desde él — nunca tenga que aprender
una segunda convención de declaradores sin relación si no quiere, mientras que
quien viene del C++ corriente puede escribir sencillamente lo que ya sabe. Ambas
producen un `AST_ARRAY_TYPE` idéntico; el AST no guarda memoria de cuál fue la
escritura usada, y la salida es siempre la forma que requiere Vircon32. Se
confirmó la viabilidad sin ambigüedad de gramática razonando sobre la anticipación
LALR(1) en cada punto de decisión (aún no confirmado con una compilación real de
bison — véase la entrada de declaradores de arreglo de arriba). El propio
planteamiento de Matthew para esto, que merece conservarse como principio
permanente para el trabajo de sintaxis futuro y no como algo puntual: «que el C++
parezca normal, aunque la transpilación tenga que ajustar las cosas» — extender el
mismo enfoque de doble aceptación a los declaradores de punteros a función cuando
se haga ese trabajo, en lugar de decidirlo de nuevo cada vez. Si la doble
aceptación de alguna construcción futura llegara a crear conflictos reales de
gramática, la alternativa es solo la forma del C estándar, no la nativa de
Vircon32 — se juzgó más importante proteger como valor por defecto la legibilidad
del C++ corriente.

## 3. Sintaxis de declaradores de punteros a función invertida

- **Vircon32 requiere**: `ReturnType(ParamTypes)* name;` — p. ej.
  `int(Shape *)* Shape__area__void;` para una ranura de vtable.
- **C estándar**: `ReturnType (*name)(ParamTypes);` — p. ej.
  `int (*Shape__area__void)(Shape *);`.
- **Estado**: Confirmado con el compilador real (emisión de campos de la
  estructura vtable, de `tests/sample7.cpp`/`sample14.cpp` en adelante).
- **Dónde**: `emit_vtable_struct` (codegen.c) construye este declarador
  directamente, en línea, en lugar de pasar por `print_type` para el caso
  específico de la ranura de vtable. Una ronda posterior añadió también un caso
  `AST_FUNC_PTR_TYPE` al propio `print_type`, para la característica aparte de
  declaración de variables de puntero a función del lado de C++
  (`int (*fp)(int, int);` y la escritura propia de Vircon32
  `int(int, int)* fp;`) — ese caso tiene la misma forma de declarador invertido.
- **Para un modo C estándar**: IMPLEMENTADO, en una ronda posterior. Resuelto de
  forma distinta a como podría haberlo logrado `print_type` solo: un nuevo
  `print_type_and_name` (codegen.c) construye el declarador ENTERO del modo
  estándar (tipo de retorno, nombre, cualquier dimensión de arreglo, lista de
  parámetros) como una unidad autocontenida, en cada uno de los dos puntos de
  llamada que pueden llegar a una declaración de tipo puntero a función
  (`print_var_decl_inline`, el bucle de campos de `emit_struct` — y, a través de
  este, también `emit_unions` — se confirmó directamente que `ast_wrap_func_ptr`
  solo se llama desde la gramática de `var_decl`, y `var_decl` es aquello a lo
  que se reducen tanto una declaración de variable como un miembro de clase;
  ningún parámetro ni tipo de retorno puede ser de tipo puntero a función en esta
  gramática). La composición de arreglos de punteros a función también funciona
  (`ReturnType (*name[N])(Params);`, la propia sintaxis del C estándar para ello),
  confirmado directamente con la salida de `tests/sample66.cpp`. A
  `emit_vtable_struct` y `emit_vtable_instance` (cuyo texto de declarador/conversión
  en línea nunca pasa por `print_type`) se les aplicó por separado la misma
  corrección, confirmada con la salida de vtable de `tests/sample14.cpp`. No
  hicieron falta cambios de gramática para nada de esto — toda la corrección vive
  en codegen.c.

## 4. Sintaxis de CONVERSIÓN de puntero a función, el mismo patrón invertido

- **Vircon32 requiere**: `(ReturnType(ParamTypes)*)expr` — que sigue el patrón del
  declarador sin nombre dentro de los paréntesis. P. ej.
  `(int(Shape *)*)&Square__area__void`.
- **C estándar**: `(ReturnType (*)(ParamTypes))expr` — p. ej.
  `(int (*)(Shape *))&Square__area__void`.
- **Estado**: Confirmado con el compilador real (`tests/sample24.c`, una
  compilación limpia de extremo a extremo — esta era la única pieza realmente
  incierta del trabajo de la instancia de vtable, ahora resuelta).
- **Dónde**: `emit_vtable_instance` (codegen.c), la rama de inserción de
  conversión para una ranura de vtable cuya clase declarante de la implementación
  actual difiere de la declarada canónicamente para el campo.
- **Para un modo C estándar**: IMPLEMENTADO junto con el n.º 3 de arriba, en la
  propia rama de inserción de conversión de `emit_vtable_instance` — la propia
  conversión de puntero a función del C estándar tiene un `()` VACÍO donde iría el
  nombre de un declarador (no hay nombre que dar a una conversión),
  `(ReturnType (*)(Params))expr`, confirmado directamente con la salida de
  `tests/sample14.cpp`.

## 5. La inicialización a nulo de un puntero requiere `NULL`, no `0` — MANEJADO

- **Vircon32 requiere**: `NULL` (o una conversión explícita, `(Type *)0`) para un
  puntero nulo. Un `0` simple se rechaza («cannot assign int to ... pointer»).
- **C estándar**: `0` y `NULL` son intercambiables en un inicializador/asignación
  de puntero.
- **Estado**: Confirmado con el compilador real (`(Node *) 0` probado
  directamente, según la ronda de conversión del receptor de DESIGN_NOTES.md).
- **Para el modo Vircon32**: **MANEJADO**: un 0 literal asignado a un puntero,
  comparado con uno o devuelto como tal se imprime `NULL` (`p = 0;`, `p != 0`,
  `fn = 0;`; véase el n.º 24 para el lado del preprocesador).
  `tests/135sample.cpp`.
- **Para un modo C estándar**: emitir un `0` simple ya sería C estándar correcto,
  así que — como el n.º 1 — el modo C estándar es el valor por defecto más simple
  y sin modificar; el modo Vircon32 es el que necesita la sustitución.

## 6. `void main(void)`, incondicionalmente, sea cual sea la declaración en el código fuente

- **Vircon32 requiere**: el punto de entrada del programa debe ser exactamente
  `void main(void)` — sin valor de retorno, nunca, sea lo que sea que declarara el
  código fuente de C++ (`int main()`, `void main()` o cualquier otra cosa).
  `return expr;` dentro de él se convierte en `expr; return;` (evaluar, descartar,
  retornar sin valor).
- **C estándar**: `main` devuelve convencionalmente `int` (`return 0;` o un código
  de salida real), aunque en la práctica también se tolera ampliamente un
  `void main()` específico de algún compilador.
- **Estado**: Confirmado con el compilador real (presente desde la primera ronda
  de `-c`/`-o`; toda prueba que tiene `main` compila limpia).
- **Dónde**: la pasada de decoración de nombres de sema.c trata el nombre `main`
  como caso especial para que permanezca sin decorar; `emit_function_definition`
  (codegen.c) fuerza el tipo de retorno a `void` y aplica `strip_return_value`
  específicamente para él.
- **Para un modo C estándar**: se tomó la decisión deliberada que esta propia lista
  señalaba como necesaria — el modo estándar respeta el tipo de retorno que el
  código fuente de C++ declaró realmente para `main` (normalmente `int`),
  conservando las sentencias `return` reales en lugar de eliminarlas, ya que
  forzar `void` habría cambiado el comportamiento real del programa (un código de
  salida pasando a ser inobservable), no solo la sintaxis superficial. Se
  implementó como una única condición `force_void_main`
  (`name == "main" && g_target == TARGET_VIRCON32`), calculada de forma idéntica
  en `emit_function_header` y en `emit_function_definition` en lugar de pasarla
  como parámetro, específicamente para que las dos nunca puedan desviarse por
  separado respecto a si un `main` dado recibe el tratamiento de forzar `void` y
  eliminar el retorno — **IMPLEMENTADO**.

## 7. Los prototipos de función requieren nombres explícitos de parámetro

- **Vircon32 requiere**: cada parámetro de un PROTOTIPO (no solo de una
  definición) necesita un nombre — `void foo(int x, int y);`, no
  `void foo(int, int);`.
- **C estándar**: los nombres de parámetro en un prototipo son opcionales.
- **Estado**: Confirmado con el compilador real (presente desde la primera ronda
  de emisión de prototipos que funcionó).
- **Dónde**: `emit_function_prototype`/`emit_method_prototype` (codegen.c) ya
  imprimen siempre el nombre del parámetro, tomado directamente del AST — nunca
  hubo una ruta de código que lo omitiera, así que no hay nada específico que
  conmutar aquí.
- **Para un modo C estándar**: no hace falta ningún cambio — nombrar cada
  parámetro es también C estándar válido y corriente. En realidad no es una
  divergencia que este proyecto necesitara revertir, solo un requisito de
  Vircon32 que resulta ser ya un subconjunto estricto de lo que permite el C
  estándar.

## 8. `new`/`delete` no tienen equivalente nativo en NINGUNO de los dos dialectos

- No es en absoluto una divergencia entre Vircon32 y el C estándar — el C simple
  (el de Vircon32 o cualquier otro) no tiene la palabra clave `new`/`delete`. La
  traducción `v32_new_*`/`v32_delete` de este proyecto (reserva basada en
  `malloc`/`free` más una llamada explícita al constructor) seguiría siendo
  exactamente la misma en un modo C estándar; solo cambia la cabecera que
  proporciona `malloc`/`free` (n.º 9, más abajo).
- **Dónde**: `new_delete_rewrite_expr` de `lower.c`,
  `emit_new_delete_runtime`/`emit_v32_delete` de `codegen.c`.
- **Para un modo C estándar**: ningún cambio en la estrategia de traducción en sí —
  **N/D, confirmado**. El PREFIJO de nombre `v32_new_`/`v32_delete` se
  reconsideró brevemente al construir `--target` (¿querría un modo C estándar un
  prefijo menos orientado a Vircon32?) y se dejó deliberadamente como estaba: es
  una convención interna de nombres (igual que `__v32_ret_tmp0` y otros), no una
  preocupación de portabilidad, y el planteamiento de esta entrada ya lo decía.

## 9. `misc.h` es el nombre de cabecera propio de Vircon32 para `malloc`/`free`/etc.

- **Vircon32 requiere**: `#include "misc.h"` para `malloc`, `free`, `memset`,
  `memcpy`, `rand`, `srand`, `exit` y otras.
- **C estándar**: `#include <stdlib.h>` (`malloc`/`free`/`rand`/`srand`/`exit`),
  `#include <string.h>` (`memset`/`memcpy`).
- **Estado**: Confirmado — `misc.h` proporcionado directamente (la copia propia de
  Matthew del código fuente de la biblioteca estándar de Vircon32), y
  `#include "misc.h"` compila limpio de extremo a extremo (`tests/sample17.c` en
  adelante).
- **Dónde**: el bloque `needs_misc` de `codegen_run` (codegen.c).
- **Para un modo C estándar**: implementado como se predijo — simple sustitución
  de cadenas, `#include <stdlib.h>` en lugar de `"misc.h"` bajo la misma condición
  `needs_misc`. Se confirmó directamente (no se dio por supuesto) que el código
  generado por este proyecto nunca emite por sí mismo una llamada a
  `memset`/`memcpy`, de modo que `<string.h>` nunca fue realmente necesaria —
  **IMPLEMENTADO**.

---

## 10. El C de Vircon32 no tiene operador ternario — BAJADO (`?:` está bien en tu código)

- No es una divergencia de forma de declarador ni de palabra clave como todas las
  entradas anteriores — Matthew informó de que el compilador real de C de Vircon32
  rechaza `cond ? a : b` de plano, en cualquier forma. A diferencia de las demás
  entradas de esta lista, esta no trata de EMITIR la escritura correcta; trata de
  no poder emitir la construcción en absoluto.
- **C estándar**: soporta el operador ternario de forma nativa; la salida en modo
  estándar conserva `cond ? a : b` exactamente como lo escribió el código fuente
  de C++, sin ninguna reescritura.

- **Estado**: CONFIRMADO con una ejecución real del compilador de Vircon32: un
  ternario anidado dentro de un argumento de llamada (`add(x > y ? x : y, 1)`) se
  transpiló con los caracteres literales `?`/`:` todavía dentro, y el compilador
  real de C de Vircon32 lo rechazó con «character '?' is not a valid identifier
  start» — su propio analizador léxico ni siquiera reconoce el carácter, lo que
  confirma que es un rechazo duro e incondicional, no meramente a nivel del
  analizador sintáctico.

- **Dónde**: una nueva fase de bajada dedicada (`lower.c`, fase 10,
  `rewrite_ternary_*`) en lugar de un condicional de `codegen.c` — es una
  auténtica reescritura a nivel de AST (expresión ternaria → sentencia if/else),
  no una elección de impresión, de modo que no podía vivir en codegen.c como lo
  hace la corrección de cada una de las demás entradas.
- **Para el modo Vircon32**: **IMPLEMENTADO**, y ahora cubre toda posición en que
  puede aparecer un ternario, en dos niveles. Un ternario que es DIRECTAMENTE el
  inicializador de un var_decl, DIRECTAMENTE el lado derecho de una asignación
  simple `=` a un identificador desnudo, o DIRECTAMENTE una expresión de retorno,
  se reescribe SIN ninguna variable temporal (cada una de estas tres formas ya
  tiene un lugar natural donde poner el valor). Los ternarios encadenados/anidados
  dentro de ese mismo conjunto de formas (`cond1 ? a : cond2 ? b : c`, un patrón
  habitual e idiomático, no un caso límite raro — confirmado mediante el propio
  `classify` de `tests/sample58.cpp`, que ya estaba en la batería de este proyecto
  antes de que existiera esta fase) se desenrollan por completo mediante recursión
  sobre cada rama recién construida, no solo en el nivel más externo. Toda OTRA
  posición — dentro de un argumento de llamada, como parte de una expresión mayor
  aritmética/de subíndice/de miembro, asignado a través de cualquier cosa que no
  sea un identificador desnudo — está ahora TAMBIÉN manejada, elevando el ternario
  a su propia variable temporal recién declarada (`__v32_tern_tmpN`), establecida
  mediante un if/else corriente insertado inmediatamente antes de la sentencia
  actual; confirmado de extremo a extremo con el `add(x > y ? x : y, 1)` de
  `tests/sample71.cpp`, que ahora se transpila sin ningún carácter `?`/`:` en
  ninguna parte de la salida y, ejecutado en modo estándar, calcula el valor
  correcto (11). Quedan dos límites estrechos, enunciados con claridad en lugar de
  pasados por alto en silencio: un ternario dentro de las cláusulas de
  inicio/condición/incremento de un bucle for (no se intenta reestructurar el
  propio bucle, p. ej. en un `while` equivalente); y uno alcanzable solo a través
  de una ranura de una única sentencia sin llaves (`if (cond) foo(cond2 ? a : b);`
  sin un bloque alrededor) — insertar una declaración temporal previa necesita una
  lista real de sentencias en la que injertarla, y solo la proporciona un bloque.
  Véase el comentario de documentación de `hoist_ternaries_in_expr` en `lower.c`
  para el razonamiento completo, incluido por qué la forma de asignación directa
  todavía necesita una comprobación más estrecha que «cualquier asignación» (un
  lvalue arbitrario duplicado en ambas ramas arriesgaría evaluar dos veces un
  efecto secundario dentro de él — la elevación genérica lo evita por completo,
  ya que introduce una variable temporal nueva en lugar de duplicar el lvalue
  original).
- **Actualización (20261001-dev): todas las posiciones, con las reglas de
  evaluación de C++.** La demo TEMPEST 32K se topó con un `?` que se coló en el C
  generado a pesar de lo anterior. Un barrido de 31 contextos encontró las
  carencias reales: cuerpos de `case` de un switch (nunca se visitaban),
  condiciones de `else if`, las tres cláusulas de `for`, cuerpos sin llaves,
  sentencias con etiqueta, listas de inicializadores y un ternario usado como
  condición de otro ternario. La fase 10 se reescribió para cubrirlos todos — los
  dos «límites estrechos» de arriba han desaparecido — y para conservar las reglas
  de evaluación de C++, que la elevación antigua no conservaba: las ramas se
  reescriben dentro de su propio if/else (el lado no tomado nunca se ejecuta, de
  modo que `p ? p->x : 0` es seguro), un ternario a la derecha de `&&`/`||` se
  ejecuta solo cuando el lado izquierdo no decide, y un ternario en la condición
  de un bucle se reevalúa en cada pasada (el bucle se convierte en `while (1)` con
  la prueba al principio; una bandera de primera pasada maneja el do-while y el
  incremento del `for` para que `continue` conserve su significado). Tras la fase,
  `check_no_ternaries` garantiza que ningún `?` llegue a la salida: un resto fuera
  del cuerpo de una función se pliega si es una constante entera, y en caso
  contrario se informa como error en su línea. `tests/97sample.cpp` comprueba todo
  esto en tiempo de ejecución en el emulador.

---

## 11. Los parámetros y los valores de retorno deben ser exactamente de una palabra — ninguna estructura/unión/arreglo por valor mayor que eso — BAJADO para estructuras

- **Actualización (20261003)**: ya no es una advertencia para estructuras y
  clases. Confirmado con el compilador real («functions cannot return values of
  size > 1», «functions cannot pass arguments of size > 1»), y **reescrito** en
  modo Vircon32 por la fase 9b de lower.c:
  - una función que devuelve una estructura de varias palabras pasa a ser `void` y
    toma un `T *__v32_ret` oculto (justo después de `this` en un método);
    `return E;` se convierte en `*__v32_ret = E; return;`
  - un parámetro de estructura de varias palabras pasa a ser
    `const T *__v32_byval_name`, y el llamado empieza con
    `T name = *__v32_byval_name;`, de modo que aún tiene su propia copia que
    modificar;
  - en la llamada, `T c = f(a);` -> `T c; f(&c, &a);`, `c = f(a);` ->
    `f(&c, &a);`, `return f(a);` -> `f(__v32_ret, &a); return;`. En cualquier otro
    lugar el resultado va a una variable temporal declarada al comienzo de la
    función, injertada como `(f(&tmp, &a), tmp)` y luego aplanada por la bajada de
    la coma (n.º 21), lo que mantiene correctas las condiciones de bucle y
    `&&` / `||`;
  - los tipos de puntero a función (typedefs, variables, miembros, parámetros,
    ranuras de vtable) se reescriben de la misma manera. Los nombres decorados no
    cambian.
  - Una estructura de UNA palabra sigue viajando de forma nativa, pero `one(42).v`
    no se acepta más adelante («cannot emit memory placement when an expression
    has none»); el resultado se aparca antes en una variable temporal.
  Coste: una copia de estructura por cada parámetro por valor en cada llamada, y
  una para un resultado que no se escribe directamente en su destino. No cubierto:
  uniones. `tests/101sample.cpp` comprueba todo esto en tiempo de ejecución. El
  texto de abajo es la entrada original.


- **Vircon32 requiere**: los parámetros y el valor de retorno de una función deben
  ser, cada uno, exactamente de una palabra de tamaño. Una estructura, unión o
  arreglo de más de una palabra no puede pasarse ni devolverse POR VALOR en
  absoluto — debe usarse en su lugar un puntero a él (que coincide con la elección
  ya existente de este proyecto para el receptor de cada método, `ClassName *this`,
  ya basado en puntero por una razón no relacionada). Informado directamente por
  Matthew, citando la propia documentación de Vircon32: «functions cannot use
  parameters or return values of size different from 1. That is: they cannot use
  arrays, unions or structures (unless their size is just a single word). Instead
  they must operate with pointers to them.» Pasar un arreglo DECAYENDO A UN
  PUNTERO (`void foo(int arr[8])`, que es ya como la gramática `param` de este
  proyecto trata un parámetro arreglo — véase la entrada n.º 2 de arriba) está
  explícitamente permitido, igual que en el C estándar; la restricción trata
  específicamente de pasar/devolver un AGREGADO de tamaño fijo (estructura/unión/
  arreglo) como una auténtica copia por valor.
- **C estándar**: no existe tal restricción — una estructura, unión o (mediante una
  estructura envoltorio, ya que el propio C tampoco permite un parámetro ni tipo de
  retorno de arreglo desnudo) arreglo corriente de cualquier tamaño puede pasarse
  o devolverse por valor.
- **Estado**: Informado directamente por Matthew, citando la propia documentación
  de Vircon32 — aún no confirmado de forma independiente con el compilador real
  transpilando y compilando un parámetro o tipo de retorno por valor
  deliberadamente sobredimensionado para ver el error concreto que produce
  Vircon32, la misma carencia de pruebas que ya tiene la entrada n.º 10 (ternario).
- **Dónde**: `check_word_sizes_classes`/`check_word_sizes_free_functions` de
  `lower.c`, ejecutadas inmediatamente después de `compute_struct_layouts` (necesita
  los recuentos de campos que esta calcula) pero antes que ninguna otra cosa en
  `lower_run` — una pasada que solo avisa, no una transformación, de modo que no
  necesita una ranura de fase numerada propia.
- **Para un modo C estándar**: no hace falta ningún cambio — esta restricción es
  específica de Vircon32, con la misma forma que la entrada n.º 1 («el modo
  Vircon32 es el que necesita el tratamiento extra»); la comprobación en sí está
  condicionada a `g_target == TARGET_VIRCON32` en cada punto de llamada.
- **IMPLEMENTADO**, una vez que Matthew confirmó las cifras que la primera versión
  de esta propia entrada decía que faltaban: Vircon32 es una máquina de 32 bits
  basada en palabras (1 palabra = 32 bits), y `int`/`float`/todo puntero ya son
  exactamente una palabra — y, lo que es crucial, `char`/`short`/`double`/etc.
  (alias de compiladores recientes) son «mero azúcar sintáctico» sobre la misma
  palabra de 4 bytes subyacente, de modo que TODO campo de CUALQUIER tipo
  primitivo soportado es exactamente una palabra, sin necesidad de ninguna tabla
  de tamaños por tipo. El tamaño de una clase/estructura en palabras es, por
  tanto, simplemente el recuento de campos de su `StructLayout` (miembros de datos
  más un puntero a vtable, si lo hay) — `sema_warning` (expuesta desde sema.c, ya
  no estática, de modo que lower.c puede reutilizar la misma maquinaria de
  recuento/formateo en lugar de duplicarla) salta cuando un parámetro o tipo de
  retorno de clase/estructura desnudo (ni puntero ni referencia — el calificado
  con `const` sigue contando) tiene más de uno de esos campos.
  Queda una carencia real, aunque estrecha: un MIEMBRO DE DATOS de tipo arreglo o
  de tipo estructura anidada solo aporta UNO al recuento de su propio
  `StructLayout` aquí, aunque él mismo pueda tener varias palabras de ancho, de
  modo que una estructura con exactamente uno de esos campos podría seguir
  contándose por defecto como «una palabra» cuando en realidad es más — la misma
  dirección conservadora (pasar por alto una infracción en lugar de avisar sobre
  código válido) que toda otra comprobación de mejor esfuerzo de este proyecto.
  Las uniones deliberadamente no se comprueban en absoluto: una unión corriente de
  miembros primitivos simples ya es exactamente una palabra por construcción (los
  miembros se superponen, no se apilan), de modo que solo una unión que contenga un
  miembro arreglo o estructura anidada lo bastante grande como para exceder por sí
  mismo una palabra infringiría esto — más estrecho todavía que la carencia de las
  estructuras de arriba, y no se persiguió en esta ronda.
- **Un hallazgo realmente valioso al probar esto con la batería existente, no
  inventado para la ocasión**: `tests/sample9.cpp` y `tests/sample15.cpp` (ambas
  ejercitan la sobrecarga de operadores en una clase `Vector2D` — `x`/`y`, dos
  campos `int`, dos palabras) activaron esta advertencia repetidamente. Matthew
  pidió que se corrigieran. Originalmente se corrigió PARCIALMENTE: cada parámetro
  `Vector2D` tomaba `const Vector2D &` en lugar de por valor, lo que eliminaba esa
  mitad de las advertencias, pero el lado del RETORNO no podía arreglarse en
  absoluto — véase la entrada n.º 12 de abajo, la carencia real, aparte y hasta
  entonces no descubierta que este intento de corrección sacó a la luz (ninguna
  función en toda esta gramática podía devolver un tipo puntero o referencia).
  AHORA corregido por completo, una vez cerrada la entrada n.º 12: ambas muestras
  se reescribieron de nuevo para devolver `Vector2D *` (reservado en el montón con
  `new`) desde cada operador que produce un valor, eliminando por completo la
  advertencia de retorno por valor en lugar de limitarse a documentarla como una
  limitación conocida.

## 12. Ninguna función puede devolver un tipo puntero o referencia — CORREGIDO

- No es una peculiaridad específica de Vircon32 — es una carencia general, hasta
  entonces no descubierta, de la propia gramática de este proyecto, sacada a la luz
  al intentar arreglar los hallazgos de `sample9`/`sample15` de la entrada n.º 11
  haciendo que un operador sobrecargado que produce un valor devolviera un puntero
  en lugar de una estructura de varias palabras por valor.
- **Confirmado directamente**: `int *getPtr(int x) { return &x; }` e
  `int &getRef();` (dentro de una clase) fallaban ambos al analizarse — la propia
  gramática de `func_header` (`type_spec func_name '(' ...`) no tenía ningún
  `pointer_opt` entre el tipo de retorno y el nombre de la función, a diferencia de
  `var_decl`/`param`, que ya lo tenían ambos. Esto se aplicaba a TODA función de
  esta gramática, no solo a los operadores — un `Shape *makeShape()` totalmente
  corriente fallaba igual. `out_of_line_def` tenía la misma carencia en su propia
  producción de gramática, aparte.
- **Corrección**: se añadió `pointer_opt` a las primeras alternativas tanto de
  `func_header` como de `out_of_line_def`, envolviendo el tipo de retorno mediante
  `ast_wrap_pointer`/`ast_wrap_reference` exactamente como ya hace `var_decl`.
  `%expect` en realidad BAJÓ, de 26 a 25 — verificado con una ejecución real de
  `bison -d`/`bison -v` (no dado por supuesto): dar a `out_of_line_def` su propio
  `pointer_opt` justo después de `type_spec`, igual que `var_decl`, significa que
  las dos producciones ahora coinciden en desplazar a través de `pointer_opt`
  primero, eliminando una bifurcación desplazamiento/reducción previa entre ellas
  en lugar de añadir una nueva. Véase el comentario sobre `%expect` en `parser.y`
  para el razonamiento completo verificado con bison.
- **Bajada, en tres partes**, todas en `lower.c`:
  1. Un RETORNO por referencia necesita el mismo tratamiento implícito de «tomar la
     dirección» que un parámetro por REFERENCIA ya recibe en su punto de llamada —
     `inject_reference_return_address_stmt` envuelve `return expr` en
     `address_of_if_needed` siempre que el tipo de retorno de la función que lo
     contiene es `AST_REFERENCE_TYPE`, ejecutada en la fase 3/4 (antes de que la
     fase 5 llegue a reetiquetar `AST_REFERENCE_TYPE` como `AST_POINTER_TYPE`, el
     mismo requisito de orden que ya documenta la corrección existente de los
     parámetros por referencia).
  2. `fix_references_in_method`/`fix_references_free_functions` (fase 5) ahora
     también reetiquetan el PROPIO tipo de retorno de la FUNCIÓN de
     `AST_REFERENCE_TYPE` a `AST_POINTER_TYPE` — antes esta fase solo tocaba los
     tipos de parámetros/variables locales, nunca el tipo de retorno de la propia
     función.
  3. Una LLAMADA que devuelve una referencia recibe ahora una desreferencia
     explícita insertada en su punto de uso (el caso `AST_CALL` de
     `finalize_calls_expr`) — el caso simétrico de (1): un retorno por referencia de
     C++ actúa como el propio referente, de modo que `int x = obj.getRef();`
     necesita un `*` insertado alrededor de la llamada (que ahora devuelve un
     puntero), o asignaría la dirección en lugar del valor. Descubierto compilando
     de verdad el código generado, no adivinado (véase abajo).
- **Dos errores adyacentes, preexistentes, encontrados y corregidos por el camino**,
  ambos sacados a la luz solo cuando esta ronda dispuso por fin de una cadena de
  herramientas bison+flex que funcionaba para COMPILAR de verdad la salida
  generada, y no solo comprobar que se analizaba:
  - `address_of_if_needed` solo reconocía `AST_POINTER_TYPE` como «ya es de tipo
    puntero, no añadir `&`» — no `AST_REFERENCE_TYPE`. Como la fase 3/4 se ejecuta
    antes de que la fase 5 reetiquete las referencias como punteros, un parámetro
    por REFERENCIA desnudo reenviado como otro argumento de tipo referencia (p. ej.
    `addThem(const Vector2D &a, const Vector2D &b)` calculando `a + b`, que se
    resuelve a `operator+(const Vector2D &other)`) recibía un `&` extra insertado
    por error, produciendo un puntero doble (`const Vector2D **` donde se esperaba
    `const Vector2D *`) — un error de compilación real, descubierto construyendo de
    verdad el C generado de `tests/sample15.cpp`. Corregido tratando también
    `AST_REFERENCE_TYPE` como «ya enlazado a puntero» en `address_of_if_needed`.
  - Un resto de esta misma investigación, que quedó abierto en su momento —
    **corregido desde entonces, en una ronda posterior, después de que una
    ejecución real del compilador de Vircon32 lo convirtiera de una advertencia de
    gcc en un informe de error real**: este proyecto sigue sin MODELAR la
    corrección de `const` en el receptor `this` de un método (no hay error por
    llamar a un método no const a través de una referencia const en primer lugar —
    véase la entrada sobre `const` en la propia lista «lo que todavía no existe» del
    README), pero reenviar un `const Shape &`/`const Shape *` como receptor de un
    método no const recibe ahora una conversión explícita que quita el const
    insertada (`Shape__area__void((Shape *)s)`) en lugar de una asignación de
    puntero simple sin conversión. gcc solo daba a esto una ADVERTENCIA
    `-Wdiscarded-qualifiers` (`const struct Vector2D *` pasado donde se espera un
    simple `struct Vector2D *this`); el compilador real de Vircon32 lo rechaza de
    PLANO («cannot assign const struct Shape* to struct Shape*: discards const
    qualifier»), un error de tipo duro, confirmado directamente con el
    `getArea(const Shape &s) { return s.area(); }` de `tests/sample68.cpp`.
    Corregido en `cast_receiver_if_needed` (`lower.c`): la conversión que esta
    función ya insertaba para un DESAJUSTE entre clase base/derivada se inserta
    ahora TAMBIÉN siempre que el objeto que se reenvía está calificado como const
    pero el `this` inyectado del método de destino no lo está (un método const que
    recibe un objeto const no necesita conversión alguna — ambos lados ya
    coinciden, comprobado mediante el tipo del parámetro `this` del destino,
    `receiver_type_is_const`). Esta es la corrección honesta dada la elección
    deliberada de este proyecto de no imponer la corrección de const — hace que el
    caso permitido pero no comprobado realmente COMPILE, del mismo modo que lo haría
    un `const_cast` explícito en C++ real, en lugar de empezar a rechazar código que
    este proyecto nunca ha rechazado antes.
- **Verificado de extremo a extremo**, no solo comprobado que se analiza: con una
  cadena de herramientas real bison+flex construida desde el código fuente en el
  propio entorno aislado de esta ronda (flex no estaba disponible allí antes;
  véase `docs/DESIGN_NOTES.md`), `tests/sample9.cpp`, `sample15.cpp` y la nueva
  `tests/sample72.cpp` dedicada (una prueba de retorno de puntero/referencia que no
  es de operadores: un método que devuelve un puntero, dentro de la clase y fuera
  de ella, una función libre que devuelve un puntero y un método que devuelve una
  referencia) se transpilan limpiamente, y su C generado compila con `gcc` simple
  sin ningún error. `sample72.cpp` se compiló y se EJECUTÓ además directamente
  (`--target=standard`), produciendo exactamente la salida esperada
  (`viaPointerMethod=7 viaFreeFunction=7 viaReference=7`).
- **Dónde**: `func_header`/`out_of_line_def` (`parser.y`);
  `inject_reference_return_address_stmt`, `finalize_calls_in_method`,
  `finalize_calls_free_functions`, `fix_references_in_method`,
  `fix_references_free_functions`, el caso `AST_CALL` de `finalize_calls_expr` y
  `address_of_if_needed` (`lower.c`).
- **Impacto sobre la entrada n.º 11 de arriba**: esto cierra el motivo por el que
  `sample9`/`sample15` solo pudieron corregirse parcialmente — ambas devuelven ahora
  `Vector2D *` (reservado en el montón con `new`) en lugar de una estructura de
  varias palabras por valor, evitando por completo la advertencia de tamaño de
  palabra, y ambas se reescriben para hacerlo como parte de esta corrección.

---

## 13. Las estructuras vacías se rechazan de plano — CORREGIDO

- **Vircon32 requiere**: que toda definición de `struct` tenga al menos un miembro.
  Una clase sin miembros de datos y sin vtable propia — una clase «verbo» solo de
  métodos y sin estado, como una clase accesora/auxiliar a la que se concede
  `friend` (el `BoxPrinter` de `tests/79sample.cpp`) — se baja si no a una
  `struct BoxPrinter {\n};\n` genuinamente vacía.
- **C estándar**: una estructura vacía es una conocida extensión silenciosa de gcc
  — aceptada sin ninguna advertencia ni siquiera con `-Wall -Wextra`, que es
  exactamente la razón por la que los barridos de verificación de
  `--target=standard` + gcc de este proyecto nunca la detectaron; solo lo hizo una
  ejecución real del compilador de Vircon32.
- **Estado**: Informado directamente por el usuario, citando textualmente el
  propio error del compilador real: `structures must have at least 1 member`.
- **Dónde**: `emit_struct` (`codegen.c`).
- **Corrección**: cuando el `StructLayout` de una clase tiene cero campos (ni
  miembros de datos, ni puntero a vtable), `emit_struct` inserta un único campo
  marcador de posición sin usar `char __v32_empty_struct_pad;` — nunca referenciado
  por nombre en ningún otro lugar del código generado, que está ahí solo para dar a
  la estructura un miembro. Aplicado en AMBOS dialectos de destino, no limitado solo
  al modo Vircon32: una estructura válida en uno de los modos de salida de este
  proyecto y no en el otro, por una diferencia que el usuario nunca escribió, sería
  una divergencia confusa y puramente accidental entre ambos.
- **IMPLEMENTADO** tanto para `--target=vircon32` como para `--target=standard`.

---

## 14. El desplazamiento a la derecha no extiende el signo de los valores negativos — BAJADO

- **C de Vircon32**: `>>` es un desplazamiento lógico. La CPU tiene una sola
  instrucción de desplazamiento, `SHL`, y una cuenta negativa desplaza a la
  derecha sin extender el signo, de modo que `-8 >> 1` es `0x7FFFFFFC`, no `-4`. El
  C estándar deja `>>` sobre un `int` negativo como definido por la implementación,
  pero gcc, clang y todo compilador con el que se prueba un programa desplazan
  aritméticamente.
- **Para el modo Vircon32**: **BAJADO** (20261016, lower.c fase 11,
  `v32_signed_shift`). Un `>>` o `>>=` de un valor CON SIGNO se convierte en

  ```c
  x >> n    ->   ((x ^ s) >> n) ^ s        s = -(x >> 31)     // 0, or -1 if x < 0
  ```

  Para una x negativa, `~x` es no negativo, de modo que desplazarlo lógicamente ES
  aritmético, y complementar el resultado da `floor(x / 2^n)`. n = 0 funciona; como
  en C, n debe valer de 0 a 31. x se escribe tres veces, así que solo se escribe
  expandida cuando es una variable, un miembro, un `*p`, o un elemento con un
  índice simple (sin llamadas, sin `++`); cualquier otro operando pasa por una
  llamada a `__v32_sar(x, n)`, una pequeña función que se añade al comienzo del C
  generado cuando hace falta. `x >>= n` se convierte en `x = <lo anterior>` (el C de
  Vircon32 evalúa por sí mismo dos veces un destino compuesto, véase el n.º 25; un
  destino con efecto secundario se ha extraído de la sentencia antes de esto).
- **Lo que sigue siendo un `>>` simple de una instrucción**:
  - un operando `unsigned`, que C también desplaza lógicamente. Para esto sirve
    ahora `unsigned`: el tipo lo recuerda (`AstNode.is_unsigned`, establecido por el
    analizador léxico para `unsigned`, un sufijo `u`, o un literal por encima de
    INT_MAX; a través de typedefs, conversiones y aritmética sobre él), aunque se
    imprima como `int`;
  - un valor que se sabe no negativo: un literal, `x & 0xFF`, una comparación;
  - un desplazamiento enmascarado por una constante positiva pequeña,
    `(x >> 4) & 15`, `(state >> 16) & 32767`: los bits que rellenaría un
    desplazamiento aritmético se vuelven a enmascarar;
  - `x >> 0` (simplemente x), y dos constantes, que se pliegan (`-8 >> 1` se escribe
    `(-4)`).
- **Coste**: unas cinco instrucciones en lugar de una para un operando que sea una
  variable; una llamada para cualquier otra cosa. En un bucle caliente de
  matemáticas de coma fija, declara el valor `unsigned` si nunca es negativo, o
  construye con `--no-signed-shift` para imprimir cada `>>` tal como está escrito
  (entonces un operando negativo desplaza lógicamente, como antes;
  `tests/137sample.cpp`).
- **No hecho**: `unsigned` sigue siendo una palabra con signo para `/`, `%`, `<`,
  `>`, `<=`, `>=` y las conversiones, de modo que estos se comportan mal por encima
  de INT_MAX (el analizador léxico lo avisa una vez). Solo su `>>` es correcto.
- Destino estándar: nada que hacer. `tests/136sample.cpp` (C++),
  `tests/138sample.c` (C) y `tests/137sample.cpp` (la opción) comprueban todo esto
  en el emulador, incluidos valores impares negativos, INT_MIN, un efecto
  secundario en el destino y operandos `unsigned`.

---

## 15. Los tipos deben declararse antes de su uso (como en C++)

los tipos deben declararse antes de su uso — una enumeración usada antes de su
declaración falla con un engañoso error «expecting COLONCOLON», porque la
clasificación TYPE_NAME del analizador léxico depende del orden de registro.» Un
diagnóstico de sema/léxico más amable (p. ej. «GameDifficulty used before
declaration») haría esto mucho más fácil de detectar que el error del analizador
sintáctico.

---

## 16. Literales de coma flotante: sin forma exponencial, y un número entero necesita su `.0` — CORREGIDO

- **C de Vircon32**: rechaza de plano la notación exponencial — `1e8`, `1.0e8` y
  `1.5e-3` fallan todos con «bad floating point literal» (confirmado con el
  compilador real). `5.` y `0.0015` están bien; `.5` no.
- **El error que esto dejó al descubierto en v32c++**: los literales de coma
  flotante se imprimían con `%.17g`, que (a) quita el punto decimal a los números
  enteros, de modo que `t / 34.0` salía como la división ENTERA `t / 34` — compilaba
  limpiamente y producía valores erróneos en silencio (el retroceso de la cámara de
  TEMPEST 32K que nunca se movía), y además afectaba al
  `value / 2147483647.0` de `v32/math.hpp` — y (b) cambia a forma exponencial por
  debajo de 1e-4, que el analizador léxico de Vircon32 rechaza entonces.
- **Escrituras de entrada (20261016)**: el analizador léxico acepta toda forma de
  coma flotante de C — `1e3`, `1e3f`, `5.`, `.5`, `1.e3`, `2E+2`, `3.14f`. (Hasta
  entonces solo coincidía `dígitos.dígitos`: `1e3` y `.5` eran errores de sintaxis.)
  `tests/135sample.cpp`.
- **Para el modo Vircon32 (y el estándar)**: **CORREGIDO** — el
  `format_float_literal` de `codegen.c` imprime en coma fija con el menor número de
  decimales que vuelvan a leerse como el mismo valor (`34.0`, `0.1`, `0.00001`),
  nunca un exponente.

---

## 17. Escapes: sin octal, sin `\0`, `\x` toma exactamente dos dígitos — CORREGIDO

- **C de Vircon32**: su analizador léxico conoce `\n \r \t \\ \' \"` y `\xHH` con
  EXACTAMENTE dos dígitos hexadecimales. Cualquier otra cosa — incluidos `\0` y todo
  escape octal — provoca «unknown escape character» y se decodifica como el propio
  carácter: `'\0'` se convierte en `'0'` (48), de modo que una prueba `c != '\0'`
  compara con el valor equivocado.
- **Para el modo Vircon32**: **CORREGIDO** — los literales de carácter imprimen
  `'\x00'` para NUL (y `\xHH` para cualquier otro no imprimible); en los literales de
  cadena todo escape fuera de ese conjunto se reescribe como `\xHH`
  (`print_vircon32_string`). El lado de C++ ahora también decodifica correctamente los
  escapes hexadecimales y octales (`ast_decode_escape`): `'\x41'` solía leerse como
  `'x'`.

---

## 18. Sin declaraciones en el cuerpo de un switch tras una etiqueta case — pon llaves al cuerpo del case

- **C de Vircon32**: «variables cannot be declared in a switch after case/default are
  used» — cualquier declaración al nivel superior del cuerpo de un switch tras la
  primera etiqueta, con o sin inicialización (confirmado con el compilador real).
  Dentro de un bloque `{ }` anidado dentro del case está bien.
- **Para el modo Vircon32**: las variables temporales que introduce el propio v32c++
  en el cuerpo de un case (el `__v32_tern_tmpN` de la reescritura del ternario) se
  envuelven en su propio bloque. Una declaración escrita directamente en el cuerpo de
  un case sigue siendo un error del propio programa; pon llaves al cuerpo del case.

---

## 19. Los tamaños de arreglo pueden ser expresiones constantes

- **C de Vircon32**: acepta macros, constantes de enumeración y aritmética constante
  en la dimensión de un arreglo: `int[ M * 2 ] b;`, `int[ N2 ] c;`, `int[ B ] d;`
  compilan todos (confirmado con el compilador real).
- **Para el modo Vircon32**: v32c++ pliega la dimensión a su valor en el momento del
  análisis (todo lo que viene después necesita el número) y la imprime de nuevo tal
  como estaba escrita cuando son solo literales, constantes con nombre y aritmética
  (`int [MAX] a;`, `int [(ROWS * COLS)] flat;`); una dimensión que nombra una
  constante de enumeración imprime su valor, ya que no se garantiza que el nombre de
  C de un enumerador coincida.

---

## 20. NO es una peculiaridad: las globales son escribibles, y los valores const pueden leerse

- Registrado aquí porque las notas de la demo TEMPEST 32K (y el texto de limitaciones
  construido a partir de ellas) decían lo contrario: que las variables a nivel de
  archivo son ROM de solo lectura a menos que se declaren con la palabra clave
  `global` de Vircon32, y que leer un `const` en una variable simple falla con
  «discards const qualifier».
- **Confirmado con el compilador real (v26.04.24)**: ninguna de las dos cosas es
  cierta. `int counter = 5; ... counter += 10;` y un `int[ 4 ] table;` a nivel de
  archivo escrito en tiempo de ejecución funcionan ambos (las globales viven en RAM,
  inicializadas al arrancar), y `const int K = 7; int x = K;` compila y lee 7. Las
  muestras con autocomprobación dependen de esto: `test_errors` es una global en la que
  escribe el programa. (Asignar un `const T*` a un `T*` simple es otra cuestión y aún
  puede rechazarse.)
- **Más estrecho de lo que parecía (20261003)**: esa prueba leía el const en un
  INICIALIZADOR, que es la única posición en la que funciona. `x = K;`, `f( K )` y
  `return K;` se rechazan todos — véase el n.º 23.

---

## 21. No existe el operador coma — BAJADO

- **C de Vircon32**: sin operador coma (`a, b` como una sola expresión).
- **Para el modo Vircon32**: **BAJADO** (20261001-dev), en la misma fase que la
  reescritura del ternario (n.º 10): el lado izquierdo se convierte en una sentencia
  propia, ejecutada primero, y la expresión es el lado derecho. Un bucle cuya
  condición o incremento usa uno se convierte en `while (1)` con la prueba al
  principio y una bandera de primera pasada que ejecuta el incremento, de modo que
  `continue` conserva su significado (`for (i = 0, j = n; i < j; i++, j--)`). El modo
  estándar lo imprime tal como está escrito. `tests/97sample.cpp` lo comprueba en
  tiempo de ejecución.
- Relacionado: `for (int i = 0, j = 5; ...)` (dos declaraciones, no el operador coma)
  se convierte en un bloque que contiene las declaraciones, alrededor de un bucle con
  una cláusula de inicio vacía — el mismo ámbito, C simple en ambos casos.

---

## 22. Un arreglo solo es un puntero en una asignación, un inicializador o un argumento

- **C estándar**: un arreglo decae a un puntero a su primer elemento casi en todas
  partes, de modo que `arr + n`, `p - arr`, `p == arr`, `*arr` y `(int)arr` son todas
  expresiones de puntero.
- **C de Vircon32**: decae solo en `p = arr;`, `T* p = arr;` y `f( arr )`. Como
  OPERANDO el arreglo sigue siendo un arreglo: `g_actors + MAX_ACTORS` -> «invalid
  operands for addition» (seguido de «cannot assign int to struct Actor*»),
  `it - g_actors` -> «invalid operands for subtraction», `p == arr` -> «invalid
  operands for equality comparison», `(int)arr` -> «cannot convert expression type
  from struct S[8] to int», `*arr` -> «dereference can only be applied to pointers».
- **No es la peculiaridad que parecía al principio**: la aritmética de punteros en sí
  funciona. `p + 2`, `2 + p`, `p - 1`, `p += 2`, `q - p`, `p < q`, `&p[ n ]` compilan
  todos, tanto para `int*` como para punteros a estructuras. Solo el operando arreglo
  es el problema (descubierto por demos/c/spyvsspy, cuyo `Actor* end = g_actors +
  MAX_ACTORS;` se interpretó al principio como «sin aritmética de punteros»).
- **Para el modo Vircon32**: **BAJADO** (lower.c fase 11). Un operando de tipo
  arreglo de `+ - == != < > <= >=`, de una conversión, o de `*` unario se reescribe
  como `&arr[0]`. `sizeof arr` se deja intacto. Comprobado en tiempo de ejecución por
  `tests/100sample.cpp`.

---

## 23. `const` acompaña a un valor que simplemente se lee

- **C estándar**: copiar un objeto const produce un valor simple; `const` solo
  restringe la escritura en el propio objeto.
- **C de Vircon32**: «cannot assign const int to int: discards const qualifier» para
  un valor const en una asignación (`n = c;`), como argumento de llamada (`f( c )`),
  en un `return c;`, y bajo el menos unario (`n = -c;`). Lo mismo para cualquier cosa
  leída A TRAVÉS de const: `n = cp->x;`, `n = cip[ 1 ];`, `n = *cip;`, `s = *cp;` (una
  estructura entera). Todos los tipos escalares, y los punteros almacenados en una
  estructura const.
- Aceptado tal cual: un inicializador (`int n = c;`, `{ c, c }`), una asignación
  compuesta (`n += c;`), cualquier expresión binaria (`n = c + 1`, `c * c`), una
  condición (`if( c )`), un índice (`a[ c ]`), y la conversión a otro tipo
  (`float f = c;`).
- Una conversión explícita al propio tipo del valor se acepta en todas partes:
  `n = (int)c;`, `q = (S*)cp->next;`.
- **Para el modo Vircon32**: **BAJADO** (lower.c fase 11, que se ejecuta después de la
  reescritura del ternario para que se cubran sus variables temporales). En el lado
  derecho de una asignación, en un argumento de llamada y en un valor de retorno, la
  lectura de un escalar o puntero const se envuelve en una conversión a su propio tipo
  sin calificar; una estructura const se lee como `*((S *)&expr)`. Un descarte genuino
  sobre un PUNTERO (`S* q = cp;` con `const S* cp`) se deja para que lo rechace el
  compilador. `strip_const_member_read` (fase 3) es la versión anterior, solo para
  miembros, de la misma corrección y todavía se ejecuta. `tests/100sample.cpp`.

---

## 24. Sin `static`, `extern`, `volatile`, `unsigned`/`signed`/`short`/`long` — aceptados, reescritos

- **C de Vircon32**: ninguna de estas palabras clave existe («identifier "static" has
  not been declared»); un `extern int x;` sin definición es «declared but not fully
  defined». Hay un solo tipo entero.
- **Para el modo Vircon32** (todo en el analizador léxico más dos reescrituras
  posteriores al análisis en ast.c, de modo que nada posterior las ve):
  - `static` a nivel de archivo y todo `extern`, `volatile`: descartados. Dos
    declaraciones a nivel de archivo de una variable (`extern int x;` ... `int x =
    5;`) se fusionan en una;
  - una variable LOCAL `static` pasa a ser una variable a nivel de archivo llamada
    `__static<N>_<function>_<name>`, declarada antes de su función, con cada uso
    renombrado (respetando el ocultamiento);
  - `static` en el cuerpo de una clase sigue siendo un error (no hay miembros
    estáticos);
  - `signed`, `short`, `long`, `long long` y sus combinaciones con `int`/`char` son
    `int`. También lo es `unsigned`, con una advertencia (una vez por ejecución): los
    valores por encima de INT_MAX, y `/`, `%`, las comparaciones sobre ellos, se
    comportan como con signo (`>>` no: véase el n.º 14). INT_MIN y `0x80000000` se
    imprimen `(-2147483647 - 1)`, ya que el C de Vircon32 rechaza el literal
    2147483648. `4000000000u` conserva su patrón de 32 bits.
- También aceptado ahora, pasado directamente (el C de Vircon32 lo tiene): `%=`,
  inicializadores de estructuras y con llaves anidadas, `int a, b[ 4 ];`. `int t[] = {
  ... }` obtiene su longitud del inicializador (el C de Vircon32 no tiene `int[] t`),
  y `T* const p` se emite como `T* p`.
- `NULL` es una PALABRA CLAVE del C de Vircon32, de modo que `#ifndef NULL / #define
  NULL 0` solía tomar su alternativa y envenenar la salida; `NULL` está ahora
  predefinido a efectos de `#ifdef`. `fn = 0;` / `fn != 0` sobre un puntero a función
  reciben la misma reescritura 0 -> NULL que ya tenían los punteros a datos.
- **Campos de bits** (`int level : 6;`): el C de Vircon32 no los tiene. Se aceptan con
  una advertencia y se almacenan como un miembro corriente de palabra completa, con el
  ancho ignorado — la estructura es mayor y no está empaquetada, y un valor fuera de
  rango no se trunca. `--reject-bit-fields` lo convierte en un error. Los campos de
  bits sin nombre y de ancho cero no se analizan.
- Un nombre de función desnudo pasado como ARGUMENTO (`apply( add, a, b )`) recibe
  ahora el `&` que exige el C de Vircon32, como ya lo recibían las asignaciones y los
  inicializadores.

## 25. Una asignación compuesta evalúa su destino dos veces

El C de Vircon32 compila `target op= value` como `target = target op value`,
escribiendo el destino dos veces. Un efecto secundario en el destino ocurre, por
tanto, dos veces. Confirmado con el compilador real (v26.04.24):

```c
int[4] buf;  int* p = buf;
*(p++) ^= 16;      // reads buf[0], stores into buf[1], p ends 2 further on
rooms[rnd_room()].r_flags |= ISGONE;   // reads one random room, writes another
```

Descubierto a través de Rogue: el decodificador de su archivo de guardado
(`*start++ ^= key`) producía basura, y su generador de niveles marcaba como
«desaparecidas» las habitaciones equivocadas.

**Qué hace v32c++** (`lower.c`, fase 11, tanto para entrada en C como en C++):
cuando la asignación compuesta es una sentencia por sí misma y su destino tiene un
efecto secundario (`++`, `--`, una asignación, una llamada — un `operator[]`
sobrecargado cuenta), la dirección del destino se toma una sola vez:

```c
{ int * __v32_target0 = (&rooms[rnd_room()].r_flags); ((*__v32_target0) |= 2); }
```

o, cuando se desconoce el tipo del destino, los `++`/`--` de variables simples se
sacan de él (`{ *p ^= key; p++; }`). Una asignación compuesta con tal destino usada
como VALOR dentro de una expresión mayor se deja tal como está escrita, con una
advertencia.

## 26. Un puntero no se compara con un `void *`

`thing == ptr` con `THING *thing` y `void *ptr` es «invalid operands for equality
comparison»; C lo permite. Para la entrada en C ambos lados se comparan como
direcciones (`((int)thing) == ((int)ptr)`). Véase también `docs/C_INPUT.es.md` para
el puntero nulo, que es un tema mayor.

---

## 27. Escribir a través de un puntero que devolvió una llamada ejecuta la llamada más de una vez

El C de Vircon32 (v26.04.24) evalúa más de una vez una llamada que produce el puntero
a través del cual escribe una asignación, y hace lo mismo con un acceso a miembro
escrito como `(*call()).m`, incluso cuando solo lee. Confirmado en C de Vircon32
simple, contando llamadas (descubierto el 20261009 al añadir `operator++`):

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

C++ hace común esta forma: una llamada a cualquier función u operador que devuelve una
referencia es `*call(...)` en el C generado, de modo que `v[i] = 5` a través de un
`int &operator[]` de usuario, `++it` (que devuelve `*this`), `obj.ref() = x` y
`list[i].hp = 0` se topan todos con ello — un efecto secundario en la llamada (un
contador, un contenedor que crece de forma perezosa) ocurría dos veces, y cada acceso
de este tipo costaba dos o tres llamadas.

**Qué hace v32c++** (modo Vircon32):

- **lecturas**: codegen imprime `(*E).m` como `(E)->m` — lo mismo en C, y evaluado una
  sola vez (`codegen.c`, `AST_MEMBER`);
- **escrituras**: la fase 11 de bajada (`v32_hoist_called_pointer`, `lower.c`) guarda
  primero el puntero cuando una sentencia de expresión asigna, asigna de forma
  compuesta o hace `++`/`--` a través de un `*` o `->` cuyo puntero contiene una
  llamada:

  ```c
  get()->m = 7;   ->   { S * __v32_ptr0 = get(); (__v32_ptr0->m = 7); }
  ```

  Cuando no se puede averiguar el tipo del puntero, se deja tal como está escrito con
  una advertencia. Igual que con el n.º 25, una asignación usada como VALOR dentro de
  una expresión mayor no se reescribe.

`tests/124sample.cpp` cuenta las llamadas para cada forma.

## 28. Los códigos de control en un literal de cadena no pueden ensamblarse — BAJADO

- **C de Vircon32** (`compile`, v26.04.24): `EscapeCCharacter` solo escapa los bytes de
  0x80 en adelante (como `\xHH`), NUL, `\ " '`, y `\n \r \t`. Todo otro código de
  control — del 1 al 8, 11, 12, 14 a 31 y 127 — va en bruto al texto del `.asm`, y
  `assemble` se detiene con «character is not valid (non-printable ASCII)». El C
  escrito a mano se topa con ello del mismo modo: `"\x15"` compila y luego falla al
  ensamblar. El propio ensamblador acepta `\xHH` en una cadena, y un inicializador de
  arreglo de `int`/`char` con los mismos códigos compila y ensambla sin problema; solo
  falta el escapado del compilador. Los códigos 14 a 31 son los caracteres de dibujo de
  cuadros y sombreados de la fuente de la BIOS, así que esto es lo que los mantiene
  fuera del texto (véase `v32::Keyboard::glyph`).
- **Qué hace v32c++** (modo Vircon32, entrada en C++): la fase de bajada 12b
  (`cxx_hoist_strings`, `lower.c`) traslada cada literal de ese tipo a un arreglo
  global oculto y usa ese en su lugar, de modo que el código fuente de C++ sigue siendo
  un literal corriente:

  ```c
  puts( "\025\025ok" );   ->   char __v32_str0[ 5 ] = { 21, 21, 111, 107, 0 };
                                puts( &__v32_str0[ 0 ] );
  ```

  Un inicializador `char s[] = "..."` se convierte en la misma lista en el lugar. Los
  literales iguales comparten un arreglo (`__v32_strN`, declarado antes del programa).
  Los literales que el compilador sabe escribir (todo lo demás, letras acentuadas
  incluidas) se dejan intactos, de modo que los programas corrientes no cambian. No se
  reescribe: un literal dentro de `sizeof` (su valor no cambia) ni el `asm` en línea. La
  entrada en C tiene la misma reescritura, en la fase 12 (`c_hoist_string`). Un literal
  que es el inicializador de un puntero GLOBAL (`const char *p = "..."`) no está
  soportado por el compilador de Vircon32 para empezar.
- **Destino C estándar**: nada que hacer; cualquier compilador de C acepta el literal.
- **Al escribirlo**: prefiere los escapes octales (`"\025"`); un escape `\x` de C++ se
  traga todos los dígitos hexadecimales que le siguen (`"caf\xE9e"` no es café).

`tests/132sample.cpp` ejecuta las formas (`char[]` global y local, un método, un
ternario, un literal repetido, `\x`, DEL, texto mixto) con la cadena de herramientas y
el emulador reales. Una corrección aguas arriba serían tres líneas en `EscapeCCharacter`
(escapar los códigos de control como `\xHH`), tras lo cual esta fase podría eliminarse.

## 29. El `NULL` del SDK es -1, y una función a la que nada llama se descarta

- **Vircon32 hace**: definir `NULL` como `-1`, de modo que un puntero del SDK
  «nulo» vale -1 (`malloc_first_block` antes del primer `malloc`, el `next`
  del último `malloc_block`). Aparte, su compilador descarta una función a la
  que nada llama, junto con los datos de un bloque `asm { }` que contenga.
- **Para v32c++**: un `0` usado como puntero se escribe `NULL` (#5), así que
  comparar un puntero del SDK con `0` o `NULL` en el código fuente es
  correcto. Lo que no lo es: una vista entera del puntero (`(int)p == 0`, un
  recorrido sobre palabras `int *` que se detiene en 0): detente en `NULL`, o
  en -1. Una tabla guardada en un bloque `asm` (con `#include` desde un
  archivo: véase `INLINE_ASSEMBLY_SUPPORT.es.md`) vive en una función que debe
  llamarse.

## 30. «argument X is not used» — lo avisa el compilador, se silencia a petición

- **Vircon32 hace**: avisar por cada parámetro que una función nunca usa,
  incluidos el `this` oculto de un método que no toca ningún miembro y un
  parámetro de callback que exige su tipo de puntero a función; además deja de
  compilar tras 10 advertencias, así que corregir unas revela otras.
- **Para v32c++**: `(void)x;` en el código fuente silencia un parámetro. Para
  todos, `--silence-unused-args` empieza cada función así con un `x;` /
  `this;` suelto (solo modo Vircon32); cuesta un poco de código, por lo que
  está desactivada por omisión. `tests/153sample.cpp`.

## Qué NO cubre esta lista

- Nada de lo que este proyecto aún no haya descubierto — es un registro de divergencias
  confirmadas (o al menos razonadas directamente) encontradas hasta ahora, no una
  afirmación de que esté completa. Se han encontrado nuevos requisitos específicos de
  Vircon32 en casi todas las rondas del desarrollo de este proyecto; espera más.
- Las carencias semánticas/de comportamiento que no tienen que ver en absoluto con la
  SINTAXIS DE SALIDA (ninguna invocación de destructores, ninguna población de vtable
  para una clase sin constructor, etc.) — esas se siguen en `docs/DESIGN_NOTES.md` y no
  son específicas de Vircon32 frente al C estándar; faltan sea cual sea el dialecto de C
  al que apunte este proyecto.
