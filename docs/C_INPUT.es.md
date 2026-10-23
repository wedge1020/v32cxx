# Entrada en C

[Versión en inglés / in English](C_INPUT.md) | [Versión en francés / en français](C_INPUT.fr.md) <!-- langlinks -->

`v32c++ program.c` transpila **C**, no C++. Lo decide la extensión del archivo:
un nombre que termina en `.c` activa el modo C para toda la ejecución
(`g_c_mode`, `inc/driver.h`). Todo lo de este documento se aplica solo a la
entrada en C — excepto las *Funciones variádicas*, que ahora funcionan también
en C++ — y un archivo `.cpp` se lee, por lo demás, exactamente como antes.

v32c++ siempre leyó la mayor parte de C, como un subconjunto de C++. El modo C
es el resto: los lugares donde C y C++ discrepan, donde el C antiguo dice menos
de lo que exige C++, y donde el C de Vircon32 difiere de ambos. Cada elemento de
abajo se descubrió construyendo un programa real, Rogue 5.4.4 (`demos/c/rogue`),
cuyos 28 archivos fuente se transpilan byte por byte tal como se escribieron
para Unix. `tests/116sample.c` y `tests/117sample.c` son las muestras con
autocomprobación; ambas son también válidas para un compilador de C nativo.
`tests/118sample.c` cubre las etiquetas usadas sin su palabra clave, algo que
solo acepta el C de Vircon32.

## Qué puede usar un programa en C

| Construcción de C | Qué hace v32c++ con ella | Dónde |
|---|---|---|
| `#include "rogue.h"`, `#include "src/move.c"` | Los archivos `.h` y `.c` del propio programa se expanden y se transpilan. Las cabeceras del SDK de Vircon32 (`video.h`, `string.h`, ...) siguen pasando al compilador. | `prescan.c` |
| `#include <stdarg.h>` | No se lee ningún archivo; proporciona `va_list`. | `prescan.c` |
| `#define when break;case`, macros de tipo función | Cada uso se expande, y solo las constantes pasan al C generado (`#define MAXSTR 1024`, `#define STATLINE (NUMLINES - 1)`). Una constante octal nunca pasa: el C de Vircon32 la leería como decimal. | `macro.c` |
| `struct rdes { ... } rdes[9];` | Las etiquetas tienen su propio espacio de nombres. Una etiqueta que además es un nombre ordinario se renombra `rdes_tag` en la salida. | `parser.y`, `cmode.c` |
| `v32key *next;` tras `struct v32key { ... }` o `struct v32key;`, sin typedef | Una etiqueta usada sin su palabra clave, como permite el C de Vircon32, se lee como el tipo: miembros (incluida una estructura que apunta a sí misma), tipos de retorno, parámetros, variables locales, conversiones, `sizeof`. Donde el mismo nombre es también una variable, un miembro o un parámetro en el ámbito, es eso otro, de modo que `struct rdes rdes[9];` no se ve afectada. | `lexer.l` (`yylex`), `parser.y` (`c_tag_known`) |
| una estructura, unión o enumeración definida dentro de una declaración, con o sin etiqueta | La definición se traslada al ámbito de archivo, antes de la declaración; las que no tienen nombre se llaman `__v32_anonN`. | `parser.y` |
| `void fatal();` y luego `void fatal(char *s) { }`; `void leave(int);`; un prototipo dentro de una función | Una sola función: la primera declaración se reescribe para decir lo que dice la definición. Sin decoración de nombres. Una función declarada y nunca definida se descarta. | `cmode.c`, `sema.c` |
| `extern char prbuf[];` y, en otro lugar, `char prbuf[2*MAXSTR];` | Una sola variable; gana la declaración con el tamaño. | `ast.c` |
| `main(int argc, char **argv, char **envp)`, sin tipo de retorno | `void main(void)` cuyas primeras sentencias declaran `argc` (1), `argv` (`{"program", NULL}`) y `envp` (`{NULL}`). | `parser.y`, `cmode.c` |
| `new_item(sizeof (THING))` cuando la definición es `new_item()` | Los argumentos se descartan, con una advertencia. | `cmode.c` |
| un `char **v32_main_args(int *argc, char **argv)` definido por el programa | Se llama al comienzo de `main` (`argv = v32_main_args(&argc, argv);`): permite que una pantalla de título elija la línea de comandos del programa. | `cmode.c` |
| `int printf(char *fmt, ...)`, `va_start`, `va_arg`, `va_end` | Ver *Funciones variádicas* más abajo. | `parser.y`, `sema.c`, `cmode.c` |
| `char **argv`, `char **a, *b;` | Punteros a punteros (hasta tres niveles). | `parser.y` |
| `void (*func)()` como parámetro, `(void (*)())fn`; `void f(char *)` escrito como parámetro, con nombre o sin él (`int run(void f(char *), char *s)`, `void apply(void (char *))`) | Parámetros y conversiones de punteros a función; un parámetro de tipo función es el puntero a esa función, como en C. | `parser.y` |
| `thing == ptr` con un `void *ptr` | Se compara como direcciones. | `lower.c` fase 12 |
| `(p = f()) == 0`, `((char *)q) != 0`, `wait(0)`, `gettoken(ps, es, 0, 0)` | El `0` es un puntero nulo allí donde se espera un puntero: junto a una asignación o una conversión, o como argumento de un parámetro puntero. | `lower.c`, `sema.c` |
| `if (p < 0)` con un puntero; `(void *)p + n` (GNU) | Un puntero comparado con un entero se lee como el entero que es; la aritmética sobre un `void *` avanza una palabra, como con un `char *`. | `lower.c` |
| `const char *s`, `const struct S *p` | `const` se omite en la salida de la entrada en C: el C de Vircon32 lo hace cumplir donde C no (`*d++ = *s++` a través de un `const char *`, `&x` de un `x` constante para un parámetro puntero normal). | `codegen.c` |
| `struct cpu *cpu;` sin que se vea nunca un `struct cpu { }`; `struct pipe;` junto a `int pipe(int *)` | Una estructura a la que solo se apunta recibe en la salida su propia línea `struct cpu;`; una etiqueta que además es nombre de función o variable se renombra `pipe_tag`. | `cmode.c`, `codegen.c` |
| `__atomic_load_n`, `__atomic_store_n`, `__atomic_exchange_n`, `__atomic_fetch_add/sub`, `__sync_synchronize`, `__sync_lock_test_and_set`, `__sync_lock_release`, `__ATOMIC_*` | Una sola CPU, nada con lo que competir: la lectura, escritura o intercambio simples (unas pocas funciones auxiliares ocultas, `__v32_atomic_xchg` y similares, cuando un archivo las usa). | `prescan.c` |
| `(*d_func)(arg)` a través de un puntero declarado `void (*d_func)();` | El puntero se convierte al tipo que implica la llamada: `((void(int)*)d_func)(arg)`. | `lower.c` fase 12 |
| el nombre de una función como valor: un argumento, una entrada de tabla, `f == g` | `&name` | `lower.c` fase 12 |
| `NULL`, `p = 0`, `if (p)`, `!p`, `p && p->x` | Ver *El puntero nulo* más abajo. | `lower.c` fase 12, `codegen.c` |
| `char buf[sizeof table / sizeof table[0]]`, `sizeof "text"` | `sizeof` se evalúa en los tamaños de arreglos (los tamaños son en palabras). | `parser.y` |
| `"\033[2J"`, `"\b \b"` | Una cadena con un carácter de control se convierte en un arreglo de `char` a nivel de archivo. | `lower.c` fase 12 |
| `struct stats s = { 16, "1x4", 12 };` con `char s_dmg[13]` | La cadena se convierte en los caracteres del miembro, rellenados con ceros. | `lower.c` fase 12 |
| `{ [SYS_fork] = sys_fork, ... }`, `{ .b = 2, .a = 1 }`, `.s.b = 9`, `[2].x = 1` | Los inicializadores designados se convierten en listas posicionales. Un índice es un literal, un `#define`, un enumerador o aritmética sobre ellos; `[]` toma su longitud del índice más alto; un hueco es cero (un puntero nulo para los punteros); en una local se escribe cada posición. `[lo ... hi] = v` (GNU) escribe `v` en cada índice de `lo` a `hi`. Una unión solo puede designar su primer miembro. | `parser.y`, `lower.c` fase 0 |
| `__attribute__((noreturn))`, `__attribute`, `__declspec(...)`, `_Noreturn`, `noreturn` (`#include <stdnoreturn.h>`) | Se ignoran donde estén: al C de Vircon32 no le importa una función que termina con una llamada que nunca retorna. | `lexer.l`, `prescan.c` |
| `(char *[]){"/init", 0}`, `(struct P){1, 2}`, `&(struct P){...}` | Una local oculta `T __v32_cl_tmpN = {...};` antes de la sentencia; en un inicializador a nivel de archivo (`struct P *p = &(struct P){1, 2};`), una global oculta `__v32_cl_gN`. No se admite en el paso de un `for` ni en la condición de un `do-while` (los mismos límites que las llamadas variádicas). | `parser.y`, `cmode.c` |
| `enum { N = 250 };` dentro de una función | Se traslada al ámbito de archivo; si otra enumeración ya tiene un enumerador `N`, esta pasa a ser `N__eK` y el resto de su bloque se reescribe. | `parser.y` |
| `_Static_assert(cond, "msg")`, `static_assert(cond)` | Se evalúa al transpilar (literales, `#define`, enumeradores, `sizeof`, aritmética); un cero es un error con el mensaje; una condición que no puede evaluar es una advertencia. No se emite nada. | `parser.y` |
| una llamada a través de una función solo con prototipo: `me()->x += n;` | Se baja con una sola llamada, como cuando la definición es visible. | `cmode.c`, `sema.c` |
| `class`, `new`, `this`, `delete`, `virtual`, ... como nombres | Identificadores ordinarios, escritos con un guion bajo final (`this_`). | `lexer.l` |
| `auto`, `register`, `(void) f();` | Se aceptan y se descartan. | `lexer.l`, `lower.c` |

## Funciones variádicas

El C de Vircon32 no tiene `...`. Los argumentos extra viajan como un argumento
más: un puntero a un arreglo de palabras.

```c
int sum(int n, ...) {                    int sum(int n, int *__v32_va) {
    va_list ap;                              va_list ap;      /* int * */
    va_start(ap, n);                         ap = __v32_va;
    x = va_arg(ap, int);                     x = *((int *)((ap += 1) - 1));
    va_end(ap);                              ap = ((void *)0);
```

y cada llamada rellena primero un arreglo propio:

```c
msg("%s hits %d", name, dmg);            int [2] __v32_va_tmp0;   /* top of the function */
                                         ...
                                         __v32_va_tmp0[0] = (int)name;
                                         __v32_va_tmp0[1] = (int)dmg;
                                         msg("%s hits %d", __v32_va_tmp0);
```

Una llamada sin argumentos extra pasa un puntero nulo. Una `va_list` puede
pasarse a otra función (`vsprintf(buf, fmt, ap)`).

Importa dónde van las asignaciones:

- en la condición de un `while` o un `for`, deben ejecutarse cada vez que se da
  la vuelta, de modo que el bucle se convierte en
  `while (true) { <asignaciones> if (!(cond)) break; ... }`;
- a la derecha de `&&` o `||`, deben ejecutarse solo si ese lado se evalúa, de
  modo que la expresión se convierte en una bandera establecida por sentencias
  `if` antes de la sentencia;
- **no soportado**, y se informa como error: una llamada variádica con
  argumentos extra en la condición de un `do ... while`, en el paso de un `for`,
  o en una rama de `?:`.

Cada argumento extra se almacena como una palabra: una conversión `(int)` para
enteros, caracteres, enumeraciones y punteros, y para un `float` sus bits tal
cual (`((float *)__v32_va_tmp0)[1] = speed;`), que `va_arg(ap, float)` vuelve a
leer. (Antes de hacer esto, un `float` se convertía en `int` al entrar.) Una
estructura pasada por valor a través de `...` no está soportada.

Lo mismo funciona en C++ (`#include <cstdarg>` o `<stdarg.h>`), en funciones
libres, espacios de nombres y métodos. La reescritura se ejecuta después del
análisis semántico, que indica qué llamadas pasan por `...` — resuelve las
sobrecargas (una sobrecarga con `...` queda después de todas las demás, de modo
que `pick(1)` toma `pick(int)` antes que `pick(int, ...)`), los métodos y los
espacios de nombres, y marca los argumentos `float` — y rechaza un objeto pasado
por `...` (pasa un puntero a él, o el `c_str()` de una cadena). El nombre
decorado de una función variádica termina en `va`: `Log__print__char_ptr_va`.
En C, una llamada que sema no pudo resolver se sigue reconociendo por el nombre
de la función.

## El puntero nulo

En C el puntero nulo es 0, y los programas dependen de ello: memoria puesta a
cero, estáticas sin inicializar, `if (p)`. El `NULL` del C de Vircon32 es -1,
no acepta ningún `int` donde se espera un puntero, ni ningún puntero donde se
espera una condición. Para la entrada en C:

- `NULL`, y un `0` literal asignado a un puntero, pasado como puntero o usado
  para inicializar uno, se escriben `((void *)0)`;
- `p == NULL` y `p != NULL` se convierten en `((int)p) == 0`;
- un puntero usado como condición (`if`, `while`, `for`, `!`, `&&`, `||`) se
  convierte en `((int)p)`.

Las propias funciones del SDK siguen hablando en -1 (`malloc` lo devuelve al
fallar). `libc/` envuelve las que llama un programa en C; no pases un puntero
nulo de C directamente a una función del SDK.

## Enlazar varios archivos

    v32c main.c kernel.c lib.c -o prog.c

Si se le dan varios archivos a `v32c` (o a `v32c++`, para fuentes en C++), los
enlaza él mismo, como un solo programa. El compilador de Vircon32 no tiene
enlazador y rechaza una función o una variable `extern` sin definir, así que los
archivos se unen antes de que él los vea:

- Un prototipo o una declaración `extern` en un archivo se enlaza con la
  definición de otro. Una cabecera (`.h`, `.hpp`) que un archivo ya leyó no la
  vuelven a leer los demás, tenga o no guardas, como si dijera `#pragma once`:
  sus declaraciones están una sola vez en el programa.
- Una función o variable `static` a nivel de archivo es privada de su archivo.
  Si otro archivo tiene algo con el mismo nombre, se renombra en el archivo que
  la posee (`helper` en `b.c` pasa a ser `helper__b`), usos incluidos; una
  variable local o un parámetro con el mismo nombre la sigue ocultando. Una
  `static inline` en una cabecera existe una vez y se comparte.
- Una función definida en dos archivos, o una variable inicializada en dos, es
  un error: `multiple definition of 'f' (first defined at a.c:3)`.
- Una función o variable `extern` que se usa y nunca se define es un error:
  `undefined reference to 'read'`. Un prototipo que nadie llama no importa.
  `--allow-undefined` lo convierte en advertencia: el SDK de Vircon32 declara
  sus propias funciones, de modo que un prototipo de una de ellas se
  denunciaría (una función del SDK declarada por la propia cabecera del SDK no
  necesita nada).
- Todas las entradas deben ser C, o todas C++ (la comprobación de referencias
  sin definir es solo para C). El XML del cartucho, `Make.sh` y `Make.bat` se
  llaman como el primer archivo; los scripts de construcción repiten los demás.
- Cada archivo tiene sus propias macros: lo que un archivo `#define`a no está
  definido en el siguiente (`-D` y `-U` valen para todos los archivos), como en
  una compilación real. Una estructura, `typedef` o enumeración con el mismo
  nombre en dos archivos debe ser el mismo tipo. Una función declarada dentro
  del cuerpo de otra (`extern int userret();` allí) no la ve la comprobación de
  referencias sin definir.
- Los archivos C++ se enlazan igual (clases en un `.hpp` compartido, métodos
  definidos en otro archivo); solo C recibe la comprobación de referencias sin
  definir. El paso de las bibliotecas arranca una segunda copia del programa
  con `popen` (`_popen` en Windows, que no se ha probado).

### Bibliotecas

    v32c --mklib libm.v32a -I inc src/add.c src/mul.c src/deep.c
    v32c main.c libm.v32a -o prog.c           # o:  -L dir -lm

Una biblioteca es un índice, `libname.v32a`, sobre los archivos fuente de sus
miembros: de cada miembro, los símbolos que define (enlace externo) y los
nombres que usa su código. Al enlazar `main.c` con ella solo entran los
miembros que definen algo que el programa necesita, luego los que esos
necesitan, y así sucesivamente; el resto se deja fuera, de modo que su código y
sus datos no cuestan nada. Un miembro se enlaza como cualquier otro archivo de
entrada, así que sus nombres `static` se renombran si chocan y sus errores se
notifican como siempre. Nombrar también un miembro en la línea de comandos no
importa (no se toma dos veces).

- El índice guarda rutas relativas y el tamaño de cada miembro: deja los
  miembros donde estaban y reconstruye el índice tras cambiar uno (un miembro
  cambiado es un error, `library ... is out of date`, no una discrepancia
  silenciosa).
- Los `-I` y `-D` dados a `--mklib` se usan para leer los miembros; da los
  mismos al enlazar.
- Una referencia se resuelve por nombre: una variable local con el mismo nombre
  que una función de la biblioteca la oculta dentro de esa función.
- Los miembros son fuente, no código objeto, así que se leen de nuevo en cada
  enlace. `--list-symbols` (interno) es lo que `--mklib` y el enlace ejecutan,
  como una segunda copia del programa, para leer los símbolos de los archivos.

## La biblioteca de C

`libc/` contiene la porción de la biblioteca estándar que necesitó Rogue, más
curses, escrita en C y transpilada junto con el programa:

    v32c++ -I <v32c++>/libc ... program.c

(sin `-I` una vez instalado v32c++: la entrada en C también busca en el `libc/`
del directorio de cabeceras instalado, por defecto
`/usr/local/Vircon32/v32tools/include/v32c++/libc`), con `#include <v32libc.c>`
(y `<v32curses.c>`) una sola vez en el único archivo del programa. Ver
`libc/v32libc.h`.

| Parte | Qué da a un programa en C |
|---|---|
| `v32libc.c` | la familia `printf`, `sscanf`, `malloc`/`free` con un puntero nulo de C, funciones de cadenas y ctype que le faltan al SDK, `time`/`localtime`, `exit` (espera a START y luego reinicia el cartucho), `strerror` |
| `v32term.c` | Una pantalla de texto de 80x24 con celdas de 8x15 dibujadas con la fuente de la BIOS, con color, vídeo inverso y los bloques de sombreado (caracteres 17-20); lectura del gamepad; un teclado en pantalla |
| `v32curses.c` | curses sobre esa pantalla: ventanas, `move`/`addch`/`printw`/`mvwinch`, `getch`, refresco |
| `v32file.c` | Archivos en la tarjeta de memoria: `fopen`, `fread`, `fwrite`, `getc`, `putc`, `fclose`, `rewind`, `fseek`, `remove`, `rename`, `unlink`, `stat` |

### Archivos en la tarjeta de memoria

Las primeras 20 palabras de la tarjeta son la firma de juego que escribe todo
juego de Vircon32 (`v32file_signature`, establecida por el programa). Después
vienen un directorio de 8 archivos con nombre y los archivos mismos, cada uno
una serie contigua de palabras. Un archivo abierto se mantiene entero en RAM y
se vuelve a escribir al cerrarlo, vaciarlo o rebobinarlo, de modo que los
archivos pueden cambiar de tamaño libremente. Una tarjeta ausente o que
pertenece a otro juego nunca se escribe: `fopen` falla y `errno` dice por qué.
Una tarjeta en blanco se formatea en la primera escritura.

Los tamaños son en palabras: `sizeof(int) == sizeof(char) == 1`. El código que
escribe «los 4 bytes de un int» (`fwrite(&n, 1, 4, f)`) lee y escribe tres
palabras más allá de la variable; tiene que decir `sizeof n`. Ese es el único
tipo de cambio que necesitó el código de guardado de Rogue.

### Una línea de comandos

`main(argc, argv)` recibe `argc == 1` a menos que el programa defina
`char **v32_main_args(int *argc, char **argv)`, a la que v32c++ llama entonces
al comienzo de `main`. La pantalla de título de Rogue es esa función: sus
entradas de menú representan `rogue`, `rogue -r` y `rogue -s`.

## Ayudas de bajo nivel

| Usted escribe | v32c++ hace |
|---|---|
| `__builtin_frame_address(0)`, `__builtin_return_address(0)` | Lee la cadena de marcos del compilador (`[BP]` = BP del llamador, `[BP+1]` = dirección de retorno). |
| `asm { "mov R1, {&buf}" }`, `{&func}`, `{*p}` | `{&x}` pasa a ser una variable local oculta `int *__v32_a_x = &x;` antes del asm (mantiene viva una función nombrada solo en asm); `{*p}`, un `int __v32_d_p = *p;` oculto. |
| `const int t[] __attribute__((rom)) = {...};` (ámbito de archivo; literales enteros, de carácter o flotantes) | Los datos se emiten como datos del ensamblador en la imagen del programa tras una función oculta `int *__v32_rom_t(void)`; cada uso de `t` pasa a ser esa llamada y `sizeof(t)` el número de elementos. Solo lectura; lo demás se avisa (`-Wrom`) y queda como global normal. |
| `<`, `>`, `<=`, `>=`, `/`, `%` de `unsigned` | Se hacen sin signo (`--no-unsigned-ops` los imprime tal cual). |
| `--rename viejo=nuevo` | Renombra función, variable o parámetro en todo el programa, para nombres que chocan con el SDK. |
| `--yield-points[=N]` | Inserta `if (get_cycle_counter() > N) yield();` en la entrada de funciones, cuerpos de bucle y etiquetas (`--yield-func=`, `--yield-counter=`); se omiten `__attribute__((no_yield))`, funciones con asm y auxiliares `__v32_*`. |
| `--warn-error[=tipo,...]`, `--list-warnings` | Convierte las advertencias de v32c++ (o solo los tipos indicados) en un fallo sin archivo de salida. |

## Límites

- Se enlazan varios archivos (ver arriba); un solo archivo es una unidad de
  traducción, y en él `static` a nivel de archivo no significa nada.
- `unsigned` es `int` (se avisa una vez); `char`, `short` y `long` son una
  palabra de 32 bits, de modo que `sizeof(char) == sizeof(int) == 1`.
- `double` y `long double` son `float`, el único tipo de coma flotante de 32
  bits de Vircon32 (el C generado dice `float`).
- Una unión no puede inicializarse con llaves (C de Vircon32).
- El `asm` en línea escrito para otra CPU (`asm volatile("wfi")` de RISC-V, accesos a CSR) pasa tal cual y no significa nada para el ensamblador de Vircon32: esa función hay que escribirla de nuevo.
- El código que da por hecho que `sizeof(int) == 4` (ver *Archivos en la
  tarjeta de memoria*).
- Una asignación compuesta cuyo destino tiene un efecto secundario, usada como
  valor dentro de una expresión mayor, la compila mal el C de Vircon32
  (peculiaridad 25 de `VIRCON32_QUIRKS.es.md`); como sentencia se maneja.
- Las declaraciones de parámetros al estilo K&R (`f(a, b) int a; char *b; { }`)
  no se analizan; las definiciones con prototipo sí.
- Campos de bits: igual que en C++ (una advertencia, o `--reject-bit-fields`).
