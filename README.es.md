# v32cxx: transpilador de C/C++ para Vircon32

[Versión en inglés / in English](README.md) | [Versión en francés / en français](README.fr.md) <!-- langlinks -->

**Un transpilador de un subconjunto de C/C++ a C, orientado al compilador de C de la
consola de fantasía [Vircon32](https://www.vircon32.com).**

El proyecto es **v32cxx**; construye dos programas, `v32c++` (entrada en C++ o
en C) y `v32c` (el mismo programa, con nombre propio para la entrada en C).

`v32c++` te permite escribir juegos y programas para Vircon32 en un subconjunto
de C++ — clases, herencia, constructores y destructores, funciones virtuales,
sobrecarga de operadores, referencias, `std::string`, `std::vector` — y los
traduce al C que la cadena de herramientas de Vircon32 ya sabe construir. Es el
enfoque que adoptó el `cfront` original de Bjarne Stroustrup en los años 80: en
lugar de compilar C++ directamente a código máquina, traducirlo primero a C y
dejar que un compilador de C existente y de confianza haga el resto. También
lee C puro (incluido el C antiguo de Unix), y se encarga de todo lo que el
dialecto de C de Vircon32 hace de forma distinta.

> **Estado: funcional.** Toda la canalización — preprocesamiento, análisis
> sintáctico, análisis semántico, bajada (*lowering*) y generación de código —
> funciona de principio a fin y se comprueba con el compilador, el ensamblador y
> la consola **reales** de Vircon32 (`make realcheck`: 117 programas compilados
> y ensamblados, 48 de ellos con autocomprobación en tiempo de ejecución en una
> consola sin pantalla). Con él se construyen cinco demos de buen tamaño (tres
> juegos en C++, un port de Rogue y un Spy vs Spy en C puro). Consulta las [Limitaciones
> conocidas](#limitaciones-conocidas) para ver con honestidad lo que falta.

## Por qué existe esto

Cuatro públicos, un solo proyecto:

0. **Una exploración histórica.** La primera implementación de C++ (`cfront`)
   fue un transpilador que funcionaba de manera similar. Emprender algo así ha
   sido muy interesante: ha revelado patrones y una serie de consideraciones que
   de otro modo quizá no habrían salido a la luz.

1. **Una herramienta de enseñanza.** Nació como material para un curso que
   enseña programación de videojuegos en Vircon32, empezando por C puro y
   avanzando hacia C++ y el diseño orientado a objetos. Mostrar a los
   estudiantes *cómo* una función virtual o un constructor se analiza, se
   resuelve y se baja a C puro es tan importante como tener una herramienta que
   funcione — por eso el C generado está pensado para leerse (ver `-vv`).

2. **Una herramienta para la comunidad.** La comunidad de homebrew de Vircon32
   escribe sus juegos en C; esto permite optar por un pequeño subconjunto de
   C++ bien entendido, sin el peso de las plantillas, las excepciones o una STL
   completa.

3. **Una puerta de entrada para C normal.** El compilador de C de Vircon32
   implementa un subconjunto de C con diversas peculiaridades sintácticas
   (las declaraciones de arreglos y la sintaxis de declaración de punteros a
   función, entre otras). Si usas `v32c++` como analizador de entrada, también
   traducirá C normal al dialecto de C de Vircon32 para su compilación,
   reduciendo al mínimo la curva de aprendizaje para empezar a desarrollar.

**No** pretende ser compatible con todo C++ — ver [Deliberadamente fuera del
alcance](#deliberadamente-fuera-del-alcance).

## Contenido

- [Qué soporta](#qué-soporta)
- [Cabeceras de C++ para la API de Vircon32](#cabeceras-de-c-para-la-api-de-vircon32)
- [Teclado y ratón: v32io](#teclado-y-ratón-v32io)
- [Limitaciones conocidas](#limitaciones-conocidas)
- [Requisitos](#requisitos) · [Compilación](#compilación) · [Verificación con la cadena de herramientas real](#verificación-con-la-cadena-de-herramientas-real)
- [Uso](#uso) · [Empaquetado del cartucho](#empaquetado-del-cartucho) · [Dialectos de salida](#dialectos-de-salida)
- [Demos](#demos) · [Documentación](#documentación) · [Estructura del proyecto](#estructura-del-proyecto)
- [Instalación](#instalación) · [Versionado](#versionado) · [Comentarios](#comentarios)

## Qué soporta

### Clases y objetos

- `class` y `struct` (la única diferencia es el acceso por defecto),
  `public`/`protected`/`private` (se aplican, también a través de la herencia),
  clases y funciones `friend`, declaraciones adelantadas (`class X;`), clases
  dentro de espacios de nombres.

- **Herencia simple**, con conversiones de puntero y de referencia de derivada
  a base.

- **Constructores y destructores**, dentro o fuera de la clase; constructores
  sobrecargados; constructores de copia; argumentos por defecto. Listas de
  inicialización de miembros para clases base (`: Base(args)`), miembros
  primitivos (`: x(v)`, en orden de *declaración*, como exige C++) y miembros
  que son objetos (`: mVel(vx, vy)`). Inicializadores de miembros por defecto
  dentro de la clase (`int mLives = 3;`). Una base con constructor por defecto
  se construye implícitamente; los destructores se encadenan a los miembros y a
  las bases.

- Objetos en la pila (`Shape s;`, `Shape s(7);`), como temporales sin nombre
  (`return Vec(x, y);`, `v.push_back(Enemy(1, 2, 3));`), en arreglos
  (construidos y destruidos elemento por elemento, incluidos los arreglos
  miembro), en el ámbito de archivo (construidos al comienzo de `main`, con los
  argumentos del constructor si se indican) y en el montón con `new`/`delete`.
  Los destructores se ejecutan en cada salida de ámbito: la llave de cierre,
  `return`, `break`, `continue`.

- Funciones **`virtual`** y despacho dinámico mediante vtables, funciones
  virtuales puras (`= 0`), destructores virtuales (`delete` a través de un
  puntero a la base).

- **Sobrecarga de operadores**: aritméticos, de comparación, de asignación
  compuesta, el menos unario y `!`, asignación, `[]`, llamada de función `()`
  (objetos función: `f(5)` sobre un objeto) e `++`/`--` prefijo y posfijo
  (`operator++()` / `operator++(int)` de C++), como miembros o como funciones
  libres, usados con sintaxis natural (`a + b`, `list[i]->draw()`, `++it`,
  `it++`).

- **Operadores de conversión** (`operator int() const`, `operator bool()`,
  `operator Vec2 *()`), dentro o fuera de la clase, también heredados: se
  llaman dondequiera que el objeto se use como un valor de ese tipo — un
  inicializador, una asignación, un `return`, una condición (`if (handle)`,
  `!handle`, `&&`), una conversión explícita (*cast*), un argumento (también al
  elegir entre sobrecargas), aritmética y comparación, un índice. Un
  `operator int()` solo también sirve para un destino `float` o `bool`; con
  varios candidatos y ninguno exacto, informa de una ambigüedad.

- **`override` y `final`** en funciones miembro, y `final` en una clase, se
  comprueban como lo hace C++ (un `override` que no sobrescribe nada,
  sobrescribir una función `final` y derivar de una clase `final` son errores);
  en cualquier otro lugar siguen siendo nombres normales (`int final;`).
  `explicit` se acepta.

- **Sobrecarga** de funciones y métodos, resuelta por número y tipo de
  argumentos; informa de una ambigüedad en lugar de adivinar.

- Objetos **por valor**: una estructura o clase de más de una palabra puede
  pasarse y devolverse por valor; v32c++ la reescribe en los punteros ocultos
  que necesita el C de Vircon32 (`docs/VIRCON32_QUIRKS.es.md` n.º 11).

- **Referencias** — parámetros, valores de retorno, variables locales,
  miembros de datos (`int &r;`, enlazada en la lista de inicialización del
  constructor), `const T &` enlazada a temporales.

- `const` (variables, punteros a const, `T * const`, funciones miembro
  `const`) — se acepta y se emite, pero no se aplica.

### El resto del lenguaje

- Espacios de nombres (anidados, llamadas cualificadas, nombres de C con
  espacio de nombres para las funciones: `v32::draw(int,int,int)` se convierte
  en `v32__draw__int_int_int`) y `using`: `using namespace v32;` en el ámbito de
  archivo o en un bloque, `using v32::Keyboard;` y `using Points = int;`. Las
  funciones encontradas mediante una directiva se sobrecargan junto con las que
  están en el ámbito, como en C++.

- Variables locales `auto` y `for` basado en rango (sobre arreglos,
  `std::array`, `std::vector` y cualquier clase con `begin()`/`end()`).

- **`std::string`** (`#include <string>`), y **`std::array<T, N>`** y
  **`std::vector<T>`** (`#include <array>`, `<vector>`) — no son plantillas,
  sino formas fijas que el transpilador expande para cada tipo de elemento.
  Ver [`docs/STRING.es.md`](docs/STRING.es.md) y
  [`docs/GENERICS.es.md`](docs/GENERICS.es.md).

- Todo el lenguaje de sentencias y expresiones de C: `switch` con caída real
  (*fall-through*), `do`/`while`, `goto`, el operador ternario y el operador
  coma (ambos reescritos para el C de Vircon32, que no tiene ninguno), los
  operadores a nivel de bits con la precedencia de C, `sizeof`, conversiones al
  estilo de C y de C++ (`dynamic_cast` con una advertencia: no hay RTTI),
  `enum`, `union`, `typedef`, punteros a función en la sintaxis de declaración
  estándar **o** la de Vircon32 (la salida es siempre la que necesita el
  destino), arreglos multidimensionales con tamaños dados por expresiones
  constantes, inicializadores con llaves, literales hexadecimales, octales y
  binarios con sufijos, `nullptr`, concatenación de cadenas adyacentes,
  declaradores múltiples (`int a, *b, c[4];`, y objetos:
  `Point a(1, 2), b(3, 4);`).

- **Funciones variádicas**: `...` con `va_list`, `va_start`, `va_arg` y
  `va_end` de `<cstdarg>`, en funciones libres, espacios de nombres y métodos,
  como en C (ver [`docs/C_INPUT.es.md`](docs/C_INPUT.es.md)). Cada argumento
  extra ocupa una palabra: un `int`, `char`, `bool`, `enum`, puntero o `float`
  — un objeto es un error (pasa un puntero, o el `c_str()` de una cadena). Una
  sobrecarga con `...` queda en último lugar, como en C++.

- Variables locales `static` (movidas al ámbito de archivo con nombres únicos),
  `static` a nivel de archivo, `extern`, `volatile`, `inline`, `register` (se
  aceptan; el C de Vircon32 no tiene ninguno de ellos). `unsigned`, `signed`,
  `short` y `long` son el único tipo `int` de 32 bits (`unsigned` con una
  advertencia); `double` y `long double` son `float`, el único tipo de coma
  flotante de Vircon32. Los campos de bits se aceptan como palabras completas
  con una advertencia, o se rechazan con `--reject-bit-fields`.

- **Ensamblador en línea** con la forma `asm { "..." }` de Vircon32, además del
  `asm("...")` básico de GCC reemitido en esa forma — ver
  [`docs/INLINE_ASSEMBLY_SUPPORT.es.md`](docs/INLINE_ASSEMBLY_SUPPORT.es.md).

- **`native Name;`** declara un tipo definido por una cabecera de C de paso
  directo (`date_info`, `game_signature`) para poder usarlo mediante punteros —
  ver [`docs/NATIVE_PASSTHROUGH.es.md`](docs/NATIVE_PASSTHROUGH.es.md). Los
  nombres de estructuras y de `typedef` de las cabeceras del SDK que v32c++
  puede leer se declaran `native` automáticamente.

### Un preprocesador del lado de C++

Antes de analizar, v32c++ hace lo que haría un preprocesador de C++:

- Los `#include` de archivos `.hpp`/`.cpp` se insertan, de forma recursiva, con
  `#pragma once`. La forma con comillas busca junto al archivo que incluye,
  luego en cada directorio `-I`, y después en el directorio actual; la forma
  con ángulos solo en los directorios `-I`. Ambas recurren después a
  `$V32CXX_INCLUDE` y a las cabeceras instaladas en
  `/usr/local/Vircon32/v32tools/include/v32c++` (el valor por defecto,
  definido en [`inc/config.h`](inc/config.h)). Los proyectos de varios
  archivos funcionan de la manera habitual, y los errores indican el archivo en
  que se producen.
- `#define`/`#undef` (de tipo objeto y de tipo función, `#`, `##`,
  `__VA_ARGS__`), `-D`/`-U`, los predefinidos `__V32CXX__`, `__FILE__`,
  `__LINE__`; `#if`/`#ifdef`/`#elif`/... con `defined()`; `#error`,
  `#warning`.
- Los `#include` de `.h` pasan al C generado, **y además** se leen cuando
  v32c++ puede encontrarlos (`$V32CXX_SDK_INCLUDE`, la carpeta `include/` junto
  al `compile` de Vircon32 que esté en tu `PATH`, y después
  `/usr/local/Vircon32/DevTools/include`), de modo que macros del SDK como
  `screen_width` o `color_red` funcionan en tamaños de arreglos y en `#if`, y
  quedan tipadas para la resolución de sobrecargas. `-v` lista las carpetas
  consultadas.
- Los `#define` pasan al C generado, y las constantes con nombre conservan allí
  sus nombres; las macros que el preprocesador propio de Vircon32 no admite se
  expanden en su lugar. Los números de línea en los diagnósticos y en los mapas
  de `-g` apuntan al archivo y a la línea originales.

### Entrada en C

Un archivo con extensión `.c` se transpila como C: sus propios `#include` de
`.h`/`.c` se expanden, las etiquetas de `struct` tienen su propio espacio de
nombres, los prototipos al estilo K&R que dicen menos que la definición se
unifican, las funciones variádicas funcionan, el puntero nulo es 0 y se acepta
`main(argc, argv)`. Con la pequeña biblioteca de C y curses de `libc/`, eso
basta para construir el juego de Unix Rogue a partir de sus fuentes
(`demos/c/rogue`). Ver [`docs/C_INPUT.es.md`](docs/C_INPUT.es.md).

`v32c` es el mismo programa con un segundo nombre, para C: `v32c juego.c` y
`v32c++ juego.c` hacen hoy exactamente lo mismo (decide la extensión del
archivo), y `make`/`cmake` construyen e instalan ambos. Los scripts de
compilación llaman al nombre con el que ejecutaste. La salida por defecto de un
archivo C sería él mismo (`juego.c` → `juego.c`), así que sin `-o` v32c++ escribe
`juego-transpiled.c`, y un `-o` que nombre la entrada es un error.

## Cabeceras de C++ para la API de Vircon32

`v32/` contiene cabeceras de C++ en el espacio de nombres `v32::`, una por cada
cabecera del SDK — `video.hpp`, `input.hpp`, `string.hpp`, `time.hpp`,
`audio.hpp`, `math.hpp`, `misc.hpp`, `memcard.hpp` — más **`v32io.hpp`**,
**`keyboard.hpp`** y **`mouse.hpp`** (teclado y ratón, más abajo). Cada
envoltorio del SDK deja pasar su `.h` y añade solo lo que la API de C no puede
expresar bien: enumeraciones tipadas en lugar de `#define`, conjuntos de
sobrecargas (`v32::minimum`, `v32::clamp`, `v32::absolute` para `int` y
`float`), guardas de ámbito RAII para el estado «seleccionado» de la consola
(textura, gamepad, sonido, canal: `TextureScope`, `GamepadScope`,
`SoundScope`, `ChannelScope`, `FrameScope`) y pequeñas clases manejadoras
(`v32::Channel`, `v32::MemoryCard`, `v32::HeapBlock`, `v32::String`,
`v32::Stopwatch`). Todo acceso al hardware sigue pasando por la función de C
real, y los nombres de los envoltorios nunca reutilizan el nombre de una
función de C (`minimum`, no `min`), de modo que la API de C en bruto sigue
siendo invocable junto a ellos.

Úsalas con `-I` apuntando al directorio que contiene `v32/` (`v32c++
-I ruta/a/v32c++ game.cpp`, y luego `#include <v32/video.hpp>`), o instálalas
(ver [Instalación](#instalación)).

## Teclado y ratón: v32io

Tres cabeceras dan a los programas un **teclado** y un **ratón** reales,
divididas igual que los controladores en C de
[v32io](https://github.com/wedge1020/v32io) para Vircon32 que portan,
protocolo por protocolo:

| Cabecera | Clase | De la biblioteca de C |
| -------- | ----- | --------------------- |
| `v32/v32io.hpp` | `v32::IoDevice`: los 11 controles de un gamepad leídos como datos (el núcleo) | `v32io.h` |
| `v32/keyboard.hpp` | `v32::Keyboard`: eventos de teclas, caracteres escritos (mayúsculas y bloqueo de mayúsculas, y caracteres acentuados y de dibujo de cuadros enviados por el dispositivo), teclas mantenidas | `keyboard.h` |
| `v32/mouse.hpp` | `v32::Mouse`: un puntero acotado, movimiento, tres botones | `mouse.h` |

Un programa incluye el o los controladores que necesite; cada uno incluye el
núcleo.

```cpp
#include <v32/keyboard.hpp>
#include <v32/mouse.hpp>

v32::Keyboard keyboard( v32::SecondGamepadPort );   // "Gamepad 2"
v32::Mouse    mouse( v32::ThirdGamepadPort );       // "Gamepad 3"
...
keyboard.probe();  mouse.probe();                   // una vez CADA fotograma
for( int key = keyboard.read(); key > 0; key = keyboard.read() )
    if( keyboard.glyph( key ) > 0 ) text.add( keyboard.glyph( key ) );  // é, ß, ┌ ...
if( mouse.pressed( v32::MouseLeft ) ) click( mouse.x(), mouse.y() );
```

> **⚠ Esto REQUIERE un dispositivo v32io.** La consola no tiene puerto de
> teclado ni de ratón: un dispositivo v32io es un gamepad cuyos controles
> transportan datos de teclado o de ratón, y algo externo a la consola debe
> producirlos — **o bien** el **adaptador de hardware v32io** (un teclado o
> ratón USB en la placa v32io-pico, que funciona con cualquier emulador de
> Vircon32), **o bien** un **DesktopEmulator de Vircon32 modificado** con los
> dispositivos de gamepad `v32kbd`/`v32mouse`: el fork en
> <https://github.com/wedge1020/ComputerSoftware> (**rama `v32io`**), o el
> emulador estándar con los parches del repositorio v32io aplicados. El
> dispositivo *Keyboard* propio del emulador estándar **no** es un teclado
> v32io.

La documentación completa — configuración, la API, los protocolos, el port
desde la biblioteca de C, cómo escribir caracteres acentuados y de dibujo de
cuadros, y cómo probar sin un dispositivo — está en
**[`docs/V32IO.es.md`](docs/V32IO.es.md)**. En
[`demos/cxx/v32io`](demos/cxx/v32io/README.md) hay cinco ejemplos listos para
construir (una máquina de escribir, un programa de dibujo, un monitor de
puertos para comprobar una configuración, un bloc de notas que usa ambos
dispositivos, y un recorrido por todos los glifos de la fuente de la BIOS y
cómo escribirlos).

## Limitaciones conocidas

Comprobadas con el transpilador actual y el compilador real de Vircon32
(octubre de 2026), no arrastradas de notas anteriores.

**No soportado** (un error claro, salvo indicación contraria):

- Miembros `static` de clases.

- `enum class`, `constexpr`, lambdas, `mutable`, clases anidadas y
  enumeraciones en el ámbito de una clase, declaradores apilados como `T *&`.

- Un parámetro que sea un arreglo bidimensional (`void f(int g[4][4])`).
- Plantillas, excepciones, RTTI y herencia múltiple: nunca (ver más abajo)

**Soportado con salvedades:**

- **`new` de arreglos** (`new T[n]`) reserva memoria pero no ejecuta los
  constructores de cada elemento; `delete[]` no ejecuta destructores, y
  `delete[]` a través de un puntero a la base sobre un arreglo de derivadas es
  indefinido. Prefiere `std::vector` o un arreglo en la pila, que se construyen
  y se destruyen correctamente.
- **`goto`** funciona, pero no ejecuta los destructores de las variables
  locales de tipo clase de las que sale; úsalo solo en ámbitos que no las
  tengan. Un `goto` a una etiqueta inexistente lo detecta el compilador de
  Vircon32, no v32c++.
- **`>>` de un valor con signo cuesta unas pocas instrucciones.** El
  desplazamiento de Vircon32 es lógico, así que v32c++ reescribe `x >> n` y
  `x >>= n` como un desplazamiento aritmético (`-8 >> 1` es `-4`, como en C).
  Una variable o miembro se escribe expandido en el lugar (unas cinco
  instrucciones en vez de una); cualquier otra cosa pasa por una pequeña
  función auxiliar. Los valores `unsigned`, las constantes y los
  desplazamientos enmascarados por una constante positiva pequeña
  (`(x >> 4) & 15`) siguen siendo una sola instrucción; `--no-signed-shift`
  desactiva la reescritura. `/`, `%` y las comparaciones de un valor `unsigned`
  por encima de `INT_MAX` siguen comportándose como con signo. Ver
  `docs/VIRCON32_QUIRKS.es.md` n.º 14.
- **`const` no se aplica** — las infracciones se dejan al compilador de C.
- **Los códigos de control en literales de cadena se reescriben**: la cadena
  de herramientas de Vircon32 no puede ensamblar una cadena que contenga los
  códigos 14–31 de la fuente de la BIOS (`┌ ─ ┐ │ ░ ▒ ▓ █` ...) u otros
  códigos de control, `"\025"`. v32c++ traslada ese literal a un arreglo
  global oculto de `char` (`__v32_strN`); `sizeof` y el ensamblador en línea se
  dejan intactos. Ver `docs/VIRCON32_QUIRKS.es.md` n.º 28.
- **Las clases dentro de un espacio de nombres conservan su nombre de C
  simple** (`v32::String` es `String` en el C generado), de modo que un
  programa no puede tener una clase propia con el mismo nombre que una de una
  cabecera que incluya (`String`, `Channel`, `Keyboard`, `Mouse`, ...). Las
  funciones libres sí tienen su espacio de nombres.
- **Sobrecargas y valores de paso directo**: v32c++ no puede ver el tipo del
  resultado de una llamada a la API de C. Una llamada sobrecargada aún se
  resuelve cuando sus demás argumentos lo deciden (`v32::minimum(rand(), 10)`);
  cuando ninguno lo hace (`v32::absolute(rand())`) es un error — guarda antes
  el valor en una variable local tipada.
- **Los tipos `native`** pueden apuntarse pero no crearse; el almacenamiento
  debe venir del lado de C.
- Las **uniones** de más de una palabra no pueden pasarse ni devolverse por
  valor (v32c++ no avisa; lo informa el compilador de Vircon32).
- Los **arreglos multidimensionales de objetos** se construyen pero no se
  destruyen al salir del ámbito.
- **Indicaciones de cartucho**: dos indicaciones `#texture` (o `#sound`) con el
  mismo nombre no se notifican — el C generado define el nombre dos veces y la
  última gana en silencio.
- Un `#include` de `.h` que no se encuentra en ningún sitio se pasa tal cual
  cada vez que aparece (el compilador de C informa entonces de que falta).
  `docs/DESIGN_NOTES.md` (solo en inglés) contiene la historia de desarrollo
  más detallada, incluidas las carencias que se han cerrado y cómo.

## Deliberadamente fuera del alcance

Para que el proyecto sea terminable, estos son límites de diseño permanentes,
no «todavía no»:

- Plantillas (los contenedores de arriba son un sustituto a nivel del
  transpilador)
- Excepciones y RTTI
- Herencia múltiple
- La STL completa

## Requisitos

Como mínimo:

- Un compilador de C (gcc o clang) y `make`.

Opcionalmente:

- **flex**
- **bison**, versión 3.x. (En macOS el `/usr/bin/bison` del sistema es GNU
  bison 2.3, congelado por razones de licencia: `brew install bison` y ponlo
  primero en tu `PATH`.)

Construido y probado en Linux y macOS; el flujo de trabajo de lanzamientos (véase
[Lanzamientos y compilaciones automáticas](#lanzamientos-y-compilaciones-automáticas))
también lo construye y le pasa una prueba de humo en Windows (x86_64 y arm64) y
Raspberry Pi y, como extras opcionales, en OpenBSD y Haiku. Para Windows (MinGW / MSYS2), las pocas
llamadas POSIX que necesita el código fuente (`realpath`, `getline`,
`strndup`, `tmpfile`, el separador de `PATH`) pasan por `src/compat.c`, que
tiene una rama para Windows para cada una.

Para construir cartuchos también necesitas las DevTools de Vircon32
(`compile`, `assemble`, `packrom`, además de `png2vircon`/`wav2vircon` para los
recursos) en tu `PATH`.

## Compilación

```sh
git clone https://github.com/wedge1020/v32cxx v32c++
cd v32c++
make            # builds bin/v32c++
make test       # transpiles every sample in tests/ into out/
```

`make test` muestra que cada transpilación tuvo éxito (o, en el caso de las
muestras deliberadamente inválidas, que falló limpiamente).

### Con CMake

Para quienes lo prefieran, `CMakeLists.txt` construye el mismo transpilador a
partir de las mismas fuentes (el Makefile sigue siendo la compilación
principal, y `make realcheck` sigue siendo la comprobación con la cadena de
herramientas real):

```sh
cmake -S . -B build              # Release by default
cmake --build build              # build/v32c++
ctest --test-dir build           # transpile every sample (read from the Makefile's test list)
sudo cmake --install build       # see "Installing"
```

flex y bison se usan cuando se encuentran (`-DV32CXX_REGENERATE_PARSER=OFF` los
omite); en caso contrario, los ya generados `src/parser.c`, `inc/parser.h` y
`src/lexer.c` se compilan tal cual, de modo que una compilación sencilla solo
necesita un compilador de C y CMake 3.10+. En **Windows**, construye con
MinGW-w64 (por ejemplo desde una consola MSYS2 MINGW64, `cmake -S . -B build
-G "MinGW Makefiles"`); MSVC no está soportado (el código usa `getopt_long` y
`<unistd.h>`). `cpack --config build/CPackConfig.cmake` genera un `.tar.gz`
(además de `.deb` / `.rpm` donde estén disponibles `dpkg-deb` / `rpmbuild`), o
un `.zip` en Windows.

## Verificación con la cadena de herramientas real

`make realcheck` va más allá: compila, ensambla y empaqueta cada programa
generado con las herramientas **oficiales** de Vircon32, y luego arranca cada
muestra con autocomprobación en una consola sin pantalla con la BIOS estándar y
una tarjeta de memoria nueva. Construye esas herramientas una vez (aproximadamente
un minuto; necesita `git` y un `g++` con C++17, sin SDL):

```sh
tools/vircon32/build-tools.sh
make realcheck
```

Una muestra con autocomprobación declara `int test_errors = -1;` y guarda allí
su recuento de errores antes de detenerse; la comprobación exige 0 (ver
`tests/94sample.cpp`). Una muestra con un `tests/NNsample.input` a su lado se
reproduce con esa entrada de gamepad guionizada (`tests/119sample.cpp`, la
prueba de v32io, la usa para el tráfico de teclado y ratón).

`build-tools.sh` también construye herramientas sin pantalla que son útiles por
sí solas, todas dirigidas por el mismo formato de guion de entrada:

| Herramienta | Qué hace |
| ----------- | -------- |
| `v32run`  | arranca un cartucho e informa de una palabra de RAM (lo que usa `realcheck`) |
| `v32prof` | carga de CPU/GPU por fotograma y ciclos por función, a partir de una compilación con `-g` |
| `v32shot` | la pantalla en los fotogramas elegidos, como imágenes PPM |
| `v32peek` | estado de la CPU y palabras de RAM en un fotograma elegido |

El `string.h` de la cadena de herramientas es el `c_api/string.h` del propio
proyecto: `build-tools.sh` lo instala sobre el de las DevTools fijadas, que es
anterior a la corrección del acarreo de redondeo de `ftoa()` que comprueba
`tests/130sample.cpp`.

`tools/vircon32/v32io-script.py` escribe tales guiones a partir de acciones de
teclado y ratón (`docs/V32IO.es.md`). `wav2vircon` también se construye cuando
hay un SDL2 real instalado (`sdl2-config`), para perfilar demos que empaquetan
sonidos.

## Uso

```sh
v32c++ game.cpp                       # writes game.c and game.xml, silently
v32c++ -I path/to/v32c++ -o obj/game.c game.cpp
```

| Opción | Significado |
| ------ | ----------- |
| `-o file.c` | ruta de salida (por defecto: el nombre de la entrada con `.c`; `nombre-transpiled.c` si eso sobrescribiría la entrada, como con un archivo `.c`) |
| `-c` | no exige un `main` (un fragmento de biblioteca/módulo) |
| `-I dir`, `--include=dir` | añade un directorio a la ruta de búsqueda de `.hpp`/`.cpp` (repetible) |
| `-D name[=value]`, `-U name` | define / elimina la definición de una macro |
| `-v`, `-vv`, `-vvv` | progreso; más comentarios explicativos en el C generado; más volcados del AST, semánticos y de bajada y el registro de notas de bajada |
| `-x`, `--no-xml` | no escribe el XML del cartucho |
| `--no-build-scripts` | no escribe `Make.sh` / `Make.bat` (véase más abajo) |
| `--title-opt=TEXTO` | título del cartucho optimizado, como una directiva `#titleopt` (y la anula) |
| `--no-diff-opt-title` | da al cartucho optimizado el mismo título que al normal, sin ` [optimized]` |
| `--xml-preserve-paths` | conserva en el XML las rutas de `#texture`/`#sound` tal como se escribieron, y escribe el XML junto al fuente |
| `-b` | transpila una BIOS en lugar de un cartucho |
| `-g` | escribe un mapa de depuración línea de C / línea de C++, `<salida>.c.debug` |
| `--target=vircon32`/`v32` | C de Vircon32 (el valor por defecto) |
| `--target=standard`/`std` | C estándar portable en su lugar |
| `--reject-bit-fields` | convierte un campo de bits en un error en lugar de una advertencia |
| `--no-signed-shift` | imprime `>>` / `>>=` tal como están escritos (el desplazamiento de Vircon32 es lógico; por defecto un desplazamiento con signo se reescribe para que sea aritmético) |
| `--no-inline-containers` | mantiene los accesores de `std::array`/`std::vector` como llamadas |
| `--version` | imprime la versión y termina |

Es silencioso cuando todo va bien, como el compilador de Vircon32 y v32lua; los
errores y advertencias van a la salida de error estándar con el archivo y la
línea, y el estado de salida es distinto de cero ante cualquier error. La
página de manual (`man ./man/es/man1/v32c++.1`) describe cada opción con todo
detalle.

**`-vv`** esparce comentarios en el `.c` generado allí donde la traducción de
C++ a C es menos obvia — vtables, el `this` explícito, en qué se convierten
`new`/`delete`, los destructores llamados al salir del ámbito, las llamadas
virtuales — para que la salida pueda leerse por sí sola (prueba
`tests/32sample.cpp`). Nunca cambia el código en sí. **`-vvv`** añade los
volcados internos y un **registro de notas de bajada**: una línea por cada
lugar donde v32c++ reescribió código para sortear una peculiaridad de Vircon32
(un ternario elevado a `if`/`else`, un `&function` implícito, una conversión
del receptor que descarta `const`) — prueba `tests/71sample.cpp`.

**`-g`** escribe `<salida>.c.debug`, una tabla dispersa
(`c_path,c_line,cpp_path,cpp_line[,function_name]`) que relaciona las líneas
del C generado con el código fuente, según el modelo de los propios mapas de
depuración de Vircon32.

**`-b`** transpila una BIOS: el `<rom>` del XML recibe `type="bios"`, y v32c++
comprueba que haya exactamente un `#texture`, como máximo un `#sound` y una
`void error_handler()`, informando de todas las infracciones a la vez.

## Empaquetado del cartucho

Se escribe un XML de empaquetado del cartucho junto al C generado (`game.xml`),
de modo que `packrom` no necesita un archivo escrito a mano. Cinco
indicaciones en el código fuente lo rellenan, según el modelo de las de v32lua:

```cpp
#title   "My Game"
#titleopt "My Game (fast)"
#version 1.0
#texture Background "background.png"
#sound   jump_sfx   "jump.wav"
```

Cada nombre de `#texture`/`#sound` se convierte en un `#define` de su
identificador (0, 1, 2… en orden, contando por separado texturas y sonidos),
utilizable en cualquier lugar donde se admita un entero
(`select_texture( Background )`), y rellena `<textures>`/`<sounds>` del XML en
ese orden con la extensión cambiada a `.vtex`/`.vsnd`. `#title`/`#version`
establecen los atributos de `<rom>` (en caso contrario «Vircon32 Program» /
«1.0»). `#titleopt` (opcional) fija el título en el XML de la versión
optimizada, `gameOpt.xml`; sin él, ese título es el del cartucho con
` [optimized]` añadido (`--no-diff-opt-title` los mantiene iguales). El `<binary path>` del XML es la ruta de `-o` con `.vbin`.

`packrom` lee todas las rutas del XML relativas al propio XML, así que por
defecto el XML contiene solo nombres de archivo: con `-o obj/game.c` y
`#texture Ship "textures/ship.png"` se obtiene `obj/game.xml` con
`<binary path="game.vbin" />` y `<texture path="ship.vtex" />`; es decir,
se espera que los `.vbin`/`.vtex`/`.vsnd` estén junto al XML. Si dos
recursos acaban con el mismo nombre, el primero lo conserva y el segundo
mantiene los directorios justos para distinguirse: `textures/ship.png`
seguido de `textures/extra/ship.png` da `ship.vtex` y `extra/ship.vtex` (el
segundo `.vtex` se espera en `obj/extra/`). Junto al XML se escribe un gemelo
para la compilación optimizada (`obj/gameOpt.xml`, cuyo `<binary>` es
`gameOpt.vbin`). `--xml-preserve-paths` conserva en cambio la ruta de cada directiva tal
como se escribió (`textures/ship.vtex`), escribe el XML en el directorio
del *archivo fuente* y hace que `<binary>` sea la ruta del `.vbin`
relativa a él.

### Scripts de compilación

Siempre que se escribe el XML, v32c++ escribe también `Make.sh` (bash) y
`Make.bat` (Windows), al estilo de los proyectos de ejemplo de Vircon32, con la
misma información: transpilar (v32c++ de nuevo, con tus opciones
`-I`/`-D`/`-U`/`-b`/`-g` y `--no-build-scripts`), `compile`, `assemble`,
`png2vircon` para cada `#texture`, `wav2vircon` para cada `#sound` y `packrom`.
Se escriben junto al fuente, o en su directorio padre cuando el fuente está en
un directorio `src/` y ese padre tiene (o, por `-o`, recibe) un `obj/`; todas
las rutas son relativas a esa carpeta, así que se ejecutan desde ella. Cada
script comprueba antes que las herramientas que usa están en el `PATH` (si falta
`v32c++`, `compile`..., se detiene con «X is needed to build»). Si `v32opt` está
disponible al ejecutar el script, también se hace la versión optimizada
(`v32opt -O3`, `gameOpt.vbin`, `gameOpt.xml`, `bin/gameOpt.v32`); si no, se omite
con un aviso. Un `Make.sh`/`Make.bat` que v32c++ no escribió nunca se
sobrescribe (se avisa). `--no-build-scripts` lo desactiva todo, igual que `-x`.

## Dialectos de salida

`--target=vircon32` (el valor por defecto) emite C de Vircon32, con todas las
peculiaridades de ese dialecto resueltas: sin la palabra clave `struct` en las
referencias a tipos, declaradores de arreglos y de punteros a función
invertidos, `void main(void)`, `NULL`, sin operador ternario ni coma, parámetros
y retornos de una palabra, `&` en los nombres de función, y el resto
catalogado en [`docs/VIRCON32_QUIRKS.es.md`](docs/VIRCON32_QUIRKS.es.md).
`--target=standard` emite en su lugar C portable (sin XML ni mapa de
depuración), para usar v32c++ como un traductor corriente de C++ a C o para
comprobar la salida con `gcc`; allí `main` conserva su tipo de retorno
declarado.

## Demos

| Demo | Lenguaje | Qué es |
| ---- | -------- | ------ |
| [`demos/cxx/tempest_32k`](demos/cxx/tempest_32k/README.md) | C++ | un juego de disparos en tubo al estilo TEMPEST, de varios archivos |
| [`demos/cxx/space_invaders`](demos/cxx/space_invaders/README.md) | C++ | «SPACE INVADERS++», con potenciadores, de varios archivos |
| `demos/cxx/star_raiders` | C++ | un juego espacial al estilo Star Raiders |
| [`demos/cxx/v32io`](demos/cxx/v32io/README.md) | C++ | ejemplos de teclado y ratón (necesitan un dispositivo v32io) |
| [`demos/c/rogue`](demos/c/rogue/README.md) | C | Rogue 5.4.4 de Unix, transpilado a partir de sus fuentes |
| [`demos/c/spyvsspy`](demos/c/spyvsspy/README.md) | C | un juego de Spy vs Spy en C puro, y una sonda de cobertura de C |

`make -C demos` construye cada demo que tenga un Makefile (cada una necesita
`v32c++` y las DevTools en el `PATH`; los juegos en C++ usan además `v32opt`
cuando está instalado).

## Documentación

| Documento | Qué cubre |
| --------- | --------- |
| [`man/es/man1/v32c++.1`](man/es/man1/v32c++.1) | la página de manual: cada opción, el lenguaje, el preprocesador, el empaquetado |
| [`man/es/man3/`](man/es/man3) | páginas de la sección 3v32 para las cabeceras: `v32c++-headers(3v32)` para empezar, luego `video.hpp`, ..., `v32c++-string`, `v32libc`, `v32curses`, ... |
| [`docs/V32IO.es.md`](docs/V32IO.es.md) | soporte de teclado y ratón (`v32/keyboard.hpp`, `v32/mouse.hpp`) |
| [`docs/C_INPUT.es.md`](docs/C_INPUT.es.md) | transpilar C, y la biblioteca de C de `libc/` |
| [`docs/STRING.es.md`](docs/STRING.es.md) | el `std::string` incorporado |
| [`docs/GENERICS.es.md`](docs/GENERICS.es.md) | `std::array` y `std::vector` |
| [`docs/NATIVE_PASSTHROUGH.es.md`](docs/NATIVE_PASSTHROUGH.es.md) | tipos `native` para estructuras de cabeceras de C |
| [`docs/INLINE_ASSEMBLY_SUPPORT.es.md`](docs/INLINE_ASSEMBLY_SUPPORT.es.md) | sentencias `asm` |
| [`docs/VIRCON32_QUIRKS.es.md`](docs/VIRCON32_QUIRKS.es.md) | todas las formas en que el C de Vircon32 difiere del C estándar, y cómo se maneja cada una |
| [`docs/DESIGN_NOTES.md`](docs/DESIGN_NOTES.md) | detalles internos y la historia completa del desarrollo (solo en inglés) |
| [`docs/TRANSPILER_REPORT.md`](docs/TRANSPILER_REPORT.md) | el informe de carencias del primer port de Space Invaders, con el estado actual (solo en inglés) |

Los documentos están disponibles en inglés, español (`.es.md`) y francés
(`.fr.md`). Las páginas de manual traducidas viven en `man/es/` y `man/fr/`.

## Estructura del proyecto

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
                translated (`man -M man/es 3v32 v32libc`); vircon32/, the
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

(El árbol se deja en inglés a propósito: son los nombres y comentarios de los
archivos tal como están en el repositorio.)

## Instalación

Una instalación para todo el sistema deja todo donde lo espera una
configuración de Vircon32. El binario y la página de manual van donde van los
comandos y las páginas de manual; las cabeceras, la biblioteca de C y los
documentos van bajo `v32tools/`, la carpeta que comparten las herramientas de
la comunidad (los archivos de v32opt, y las cabeceras de v32lua en
`v32tools/include/v32lua`), junto a las `DevTools/` oficiales:

| | Linux / macOS | Windows (CMake) |
| --- | --- | --- |
| transpilador (`v32c` es una copia) | `/usr/local/bin/v32c++`, `/usr/local/bin/v32c` | `C:\Program Files\Vircon32\v32tools\v32c++.exe`, `v32c.exe` |
| páginas de manual (las de v32c++, y las del SDK de Vircon32 de `man/vircon32/`, juntas) | `/usr/local/share/man/man1/*.1.gz`, `/usr/local/share/man/man3/*.3v32.gz` (comprimidas con gzip; los alias son enlaces simbólicos) | `...\v32tools\docs\v32c++\man1`, `...\man3` |
| cabeceras de C++ (`v32/`) | `/usr/local/Vircon32/v32tools/include/v32c++/v32` | `...\v32tools\include\v32c++\v32` |
| biblioteca de C (`libc/`) | `/usr/local/Vircon32/v32tools/include/v32c++/libc` | `...\v32tools\include\v32c++\libc` |
| documentación | `/usr/local/Vircon32/v32tools/docs/v32c++` | `...\v32tools\docs\v32c++` |

Cualquiera de las dos compilaciones lo hace:

```sh
sudo make sysinstall                          # Linux / macOS
sudo make sysuninstall

sudo cmake --install build                    # any system (prefix: /usr/local,
sudo cmake --build build --target uninstall   #   or C:/Program Files/Vircon32)
```

El directorio de cabeceras se compila dentro del transpilador, de modo que tras
instalar, `#include <v32/video.hpp>` (o `<v32/keyboard.hpp>`, ...) funciona
desde cualquier directorio sin `-I`, y la entrada en C encuentra la `libc`
instalada del mismo modo. Con CMake, elige otra ubicación al configurar
(`cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/vircon32`) y el transpilador
buscará allí; `-DV32CXX_INSTALL_INCLUDEDIR=...` (y `BINDIR`, `MANDIR`,
`DOCDIR`) mueven una sola parte. Al desinstalar se eliminan solo los archivos
propios de v32c++, y luego `v32tools/` (y en Linux / macOS `Vircon32/`) solo si
no queda nada más en ellos. En Windows, añade
`C:\Program Files\Vircon32\v32tools` a tu `PATH`, como con las DevTools.

`make install` / `make uninstall` copian en cambio solo el binario a `~/bin`.
Los valores por defecto de instalación de la compilación con Makefile (el
directorio de cabeceras, los nombres de las variables de entorno, algunos
límites) viven en [`inc/config.h`](inc/config.h); cámbialos allí y recompila, o
sobrescríbelos al construir:

```sh
make CFLAGS="-Wall -Wextra -g -Iinc -MMD -MP -DV32CXX_INCLUDE_PATH='\"/opt/v32tools/include/v32c++\"'"
```

**Idioma de la documentación.** Por defecto, ambas compilaciones instalan los
documentos en todos los idiomas (`README.md`, `README.es.md`, `README.fr.md`,
`docs/V32IO.es.md`, ...) y las páginas de manual en inglés más las traducidas
en los árboles de idioma estándar (`man/es/` se instala en
`/usr/local/share/man/es/man1` y `man3`, `man/fr/` en `.../fr/man1` y `man3`,
de modo que `man` elige el idioma según `LANG`). `DEST_LANG=es` o `DEST_LANG=fr`
(`sudo make sysinstall DEST_LANG=es`, o `cmake -S . -B build -DDEST_LANG=es`)
instala en cambio solo ese idioma: el README y los documentos traducidos con
esos nombres simples (y los enlaces entre ellos ajustados), junto a los
documentos que existen solo en inglés (`docs/DESIGN_NOTES.md`, ...), y las
páginas de manual en ese idioma directamente en `man1` y `man3`.
`DEST_LANG=en` instala solo los documentos y las páginas de manual en inglés.
Las páginas del SDK de Vircon32 (`man/vircon32/`) existen solo en inglés y se
instalan siempre.

## Versionado

La versión es `YYYYMMDD-estado` (`-dev` o `-release`), el mismo esquema que
v32lua, y vive en un solo lugar: `VERSION` en [`inc/v32cxx.h`](inc/v32cxx.h),
que es lo que imprime `v32c++ --version`. Para cambiarla, edítala allí y
ejecuta `make version`, que la estampa (con la fecha de hoy) en la cabecera
`.TH` de cada página de manual de `man/man1` y `man/man3` que nombre a v32c++, y
en las de `man/es/` y `man/fr/` (con la fecha en el idioma de cada una). Las
páginas alias y las páginas de otros proyectos (las del SDK de Vircon32, en
`man/vircon32/`) se dejan intactas, y una página que ya está al día no se
reescribe.

## Lanzamientos y compilaciones automáticas

[`.github/workflows/release-builds.yml`](.github/workflows/release-builds.yml)
(según el modelo del de v32lua) se ejecuta solo cuando se inicia a mano, desde
la pestaña Actions, como una ejecución `test` o `release` (nada se ejecuta al
hacer push). Los trabajos:

| Trabajo | Qué hace |
| ------- | -------- |
| Read Version | lee `VERSION` de `inc/v32cxx.h`; comprueba que `make version` la ha estampado en las páginas de manual (una advertencia en una ejecución `test`, un error en una `release`), que `man/vircon32/` y `man/` no comparten ningún nombre de página, y que cada página alias conduce a algún lugar |
| Test Against the Vircon32 Toolchain | `make realcheck` (arriba), con la cadena de herramientas en caché entre ejecuciones |
| CMake Build, Test and Install | configurar, construir, `ctest`, y luego `cmake --install` en un directorio temporal, comprobando el binario, las cabeceras y cada página de manual, comprimida, con los alias como enlaces (Linux y macOS) |
| Build | Windows (x86_64, arm64), macOS (x86_64, arm64), Linux (x86_64, arm64, Raspberry Pi) y, opcionalmente, OpenBSD y Haiku, cada uno con prueba de humo (`scripts/smoke_test.sh`) y comprimido en zip (`scripts/stage_package.sh`) |
| Release | para una ejecución `release`: un lanzamiento de GitHub `v<VERSION>` para el commit que se construyó, con todos los zips, un `SHA256SUMS.txt` y el texto de [`.github/release-notes.md`](.github/release-notes.md). Se deja como borrador para revisarlo y publicarlo a mano, salvo que la ejecución se iniciara con `publish_release` marcado, lo que lo publica de inmediato; una versión `-dev` se marca como prelanzamiento |

Un zip de lanzamiento se descomprime directamente en `v32tools/`
(`/usr/local/Vircon32/v32tools` o `<Program Files>/Vircon32/v32tools`): el
binario, `include/v32c++/` (`v32/` y `libc/`) y `docs/v32c++/` con los
documentos y las páginas de manual en `man1/` y `man3/`, las de v32c++ y las
del SDK de Vircon32 juntas (`man -M docs/v32c++ 3v32 string.h`). Para hacer un
lanzamiento: establece `VERSION`, ejecuta `make version`, actualiza el texto de
`.github/release-notes.md` (`@VERSION@` se convierte en la versión), haz commit
e inicia el flujo de trabajo con `release`; después revisa el borrador en la
página de Releases y pulsa **Publish release** (o marca `publish_release` al
iniciar el flujo de trabajo, para saltarte ese paso).

## Colaboración con IA

Esta herramienta se desarrolló en estrecha colaboración con una IA. Dudo en
llamarla «vibe-coded», ya que tengo cierto conocimiento y experiencia con lo
básico. Pero, ciertamente, no habría logrado este nivel de funcionalidad en tan
poco tiempo si la IA no hubiera participado activamente. Siento que asumí más
bien un papel de «jefe de proyecto»: dictar la dirección, resolver problemas,
depurar cuando hizo falta.

Aunque quizá no conozca cada rincón del código tan bien como si lo hubiera
escrito yo mismo por completo, aprecio el apalancamiento que ofrece para crear
nuevas herramientas que puedo usar en el aula, compartir con la comunidad, y
hacer realidad las ideas con mayor rapidez.

Sé que esto puede cruzar, o ser, una línea de dudosa difusidad/aceptabilidad
para algunos. Pero, siendo una herramienta de nicho en un entorno de nicho,
puede ser igual de fácil ignorarla.

## Comentarios

Esto se construye a la vista de todos como un proyecto de enseñanza y se
comparte con la comunidad de Vircon32 — si algo se rompe, se transpila mal, o
te gustaría que se priorizara alguna característica concreta de C++, por favor
abre una incidencia en <https://github.com/wedge1020/v32cxx>.
