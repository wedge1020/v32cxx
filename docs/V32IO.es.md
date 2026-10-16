# v32io: entrada de teclado y ratón

[Versión en inglés / in English](V32IO.md) | [Versión en francés / en français](V32IO.fr.md) <!-- langlinks -->

Tres cabeceras de `v32/` dan a los programas de v32c++ un **teclado** y un
**ratón** reales en la consola Vircon32. Son un port a C++ de los controladores
en C [v32io](https://github.com/wedge1020/v32io) para Vircon32, divididos de la
misma manera: los mismos protocolos y la misma decodificación, envueltos en tres
clases.

| Cabecera           | Clase           | Sustituye (biblioteca de C)              | Qué es                                         |
| ------------------ | --------------- | ---------------------------------------- | ---------------------------------------------- |
| `v32/v32io.hpp`    | `v32::IoDevice` | `v32io.h`: `v32io` / `v32io_*()`         | el núcleo: los 11 controles de un gamepad leídos como datos |
| `v32/keyboard.hpp` | `v32::Keyboard` | `keyboard.h`: `v32kbd` / `v32kbd_*()`    | eventos de teclas, caracteres escritos, teclas mantenidas |
| `v32/mouse.hpp`    | `v32::Mouse`    | `mouse.h`: `v32mouse` / `v32mouse_*()`   | un puntero, movimiento y tres botones          |

Un programa incluye el o los controladores que necesite (`keyboard.hpp`,
`mouse.hpp` o ambos); cada uno incluye el núcleo, `v32io.hpp`, que solo se
incluye directamente para usar `v32::IoDevice` por sí solo.

```cpp
#include <v32/keyboard.hpp>

v32::Keyboard keyboard( v32::SecondGamepadPort );

void main()
{
    while( true )
    {
        keyboard.probe();                 // once on EVERY frame
        int key = keyboard.read();
        while( key > 0 )                  // every key pressed this frame
        {
            if( key == v32::KeyEnter ) { /* ... */ }
            else if( key >= 32 && key < 127 ) { /* a typed character */ }
            key = keyboard.read();
        }
        end_frame();
    }
}
```

---

## ⚠ Requisito: un dispositivo v32io

**Esta funcionalidad REQUIERE o bien el adaptador de hardware v32io o bien un
DesktopEmulator de Vircon32 modificado.** No funciona con el emulador estándar
por sí solo, y nada de un cartucho puede cambiar eso.

La consola Vircon32 no tiene puerto de teclado ni de ratón. Un dispositivo v32io
llega hasta ella a través de un **puerto de gamepad**: la consola ve un gamepad
corriente, pero sus 11 controles (cuatro direcciones y siete botones)
transportan datos de teclado o de ratón en lugar de pulsaciones de juego. Algo
externo a la consola debe producir esos datos:

1. **El adaptador de hardware v32io** — firmware propio (`v32io-pico`) en una
   placa Waveshare RP2350-USB-A (con su resistencia R13 retirada). Conecta a ella
   un teclado o ratón USB estándar, y conéctala al ordenador: se presenta como un
   joystick llamado `v32io:kbd` o `v32io:mouse`. Como para el ordenador es
   «simplemente un gamepad», funciona con **cualquier** emulador de Vircon32,
   estándar o modificado, una vez que un perfil de joystick lo asigna (ver
   [Perfil de joystick para el adaptador](#perfil-de-joystick-para-el-adaptador)).
   Notas de firmware y de construcción: el repositorio v32io,
   `firmware/RP2350-USB-A/`.
2. **Un DesktopEmulator de Vircon32 modificado** — no hace falta hardware; el
   teclado y el ratón del propio ordenador anfitrión se convierten en
   dispositivos v32io. Puede ser:
   - el fork en <https://github.com/wedge1020/ComputerSoftware>, **rama
     `v32io`**, que tiene los dispositivos `v32kbd` y `v32mouse` incorporados; o
   - las fuentes del DesktopEmulator estándar con los dos parches del
     repositorio v32io aplicados (`emulator/v32kbd.patch`,
     `emulator/v32mouse.patch`).

> El dispositivo de gamepad **Keyboard** propio del emulador estándar *no* es un
> teclado v32io: asigna unas pocas teclas a botones de gamepad. Un
> `v32::Keyboard` que lo lea ve disparates. Del mismo modo, un gamepad corriente
> en el puerto en que un programa espera un dispositivo v32io produce eventos de
> teclas y movimientos de ratón aleatorios.

Sin un dispositivo, un programa que use estas cabeceras igualmente se construye y
se ejecuta: el puerto simplemente no está conectado (`connected()` es falso) o no
muestra nada, y no llega ningún evento. Los ejemplos imprimen en ese caso un
mensaje de «sin dispositivo».

---

## Configuración

### Números de gamepad

Los menús del emulador cuentan los gamepads desde 1; la consola (y esta cabecera)
cuenta los identificadores de puerto desde 0:

| Menú del emulador | Id de puerto | Constante                  |
| ----------------- | ------------ | -------------------------- |
| Gamepad 1         | 0            | `v32::FirstGamepadPort`    |
| Gamepad 2         | 1            | `v32::SecondGamepadPort`   |
| Gamepad 3         | 2            | `v32::ThirdGamepadPort`    |
| Gamepad 4         | 3            | `v32::FourthGamepadPort`   |

La convención que usan los ejemplos es el **Gamepad 2** para el teclado (y para
el ratón cuando está solo), el **Gamepad 3** para el ratón cuando se usan ambos,
dejando el Gamepad 1 para un gamepad corriente. Sirve cualquier puerto; el
programa decide.

### DesktopEmulator modificado

Abre el menú *Gamepads*, elige el gamepad que espera tu programa y selecciona
**`v32kbd`** o **`v32mouse`**.

- `v32kbd` no puede seleccionarse mientras algún gamepad use el dispositivo
  *Keyboard* estándar, y viceversa: ambos necesitan el teclado anfitrión.
- Las teclas rápidas del emulador (Esc, F2, F4, F5 y los atajos con Ctrl)
  siguen funcionando, y además se envían al programa.
- Con `v32mouse`, un **clic izquierdo sobre la pantalla del juego captura el
  puntero** (el clic en sí no se envía al programa). Pulsa **Ctrl izquierdo + Alt
  izquierdo** para liberarlo (el Alt *izquierdo*, para no chocar con las
  distribuciones con AltGr).

### Perfil de joystick para el adaptador

El adaptador aparece como un joystick, `v32io:kbd` o `v32io:mouse` según lo que
se conecte a él, y cada uno necesita un perfil de joystick en el emulador (hecho
con *EditControls*; el adaptador tiene un modo de configuración para esto,
descrito en el README del firmware de v32io). Ambos perfiles asignan igual —
**el botón *n* al control *n***:

```xml
<joystick nickname="v32io-kbd">
    <guid>...</guid>              <!-- from EditControls, per computer -->
    <name>v32io: kbd</name>
    <left         button="0" />
    <right        button="1" />
    <up           button="2" />
    <down         button="3" />
    <button-start button="4" />
    <button-a     button="5" />
    <button-b     button="6" />
    <button-x     button="7" />
    <button-y     button="8" />
    <button-l     button="9" />
    <button-r     button="10" />
</joystick>
```

El GUID y el nombre exacto dependen del ordenador, así que tómalos de
*EditControls* con el adaptador conectado. Después asigna el joystick al gamepad
que espera el programa.

**Comprueba una configuración con `demos/cxx/v32io/monitor`.** Muestra, para los
cuatro puertos, los 11 controles exactamente como los ve la consola y lo que
significan para cada controlador. Un control que nunca se enciende, o dos que
siempre se mueven juntos, son un error de asignación del perfil.

---

## Uso de las cabeceras

Añade con `-I` la raíz del repositorio (el directorio que contiene `v32/`), o
instala las cabeceras en todo el sistema (`sudo make sysinstall`, o `sudo cmake
--install build`; ver «Instalación» en el README), tras lo cual no hace falta
`-I`:

```sh
v32c++ -I path/to/v32c++ -o obj/game.c game.cpp
```

Como toda cabecera de `v32/`, son código fuente de C++ insertado en tu programa.
Son autocontenidas: no se necesita la biblioteca **de C** de v32io para construir
un programa de v32c++ (y no están pensadas para mezclarse en un mismo programa).
El núcleo incluye los `input.h`, `time.h` y `misc.h` del SDK, que pasan al C
generado.

### La única regla: sondea en cada fotograma

Llama a `probe()` **una vez en cada fotograma** para **cada** dispositivo, antes
de `end_frame()`. La consola actualiza los gamepads solo entre fotogramas, y un
teclado informa como máximo de un evento de tecla por fotograma: un fotograma sin
sondeo puede perder una tecla, o interpretar mal el movimiento del ratón. Las
llamadas extra a `probe()` en el mismo fotograma no hacen nada, así que es seguro
sondear desde más de un lugar.

Los programas con varias pantallas (un título, un menú, el juego) deberían sondear
en cada una de ellas — o mantener un único sondeo al comienzo de un solo bucle
principal.

### Objetos

Un `Keyboard` o un `Mouse` es un objeto corriente: una global, o una variable
local de `main`. No hay nada que liberar. Crea uno por dispositivo, cada uno con
su propio puerto; un teclado y un ratón pueden usarse a la vez. Copiar uno es
inofensivo pero inútil (la copia tiene su propia cola y estado); pasa en su lugar
un puntero o una referencia.

### `v32::Keyboard`

```cpp
v32::Keyboard keyboard( v32::SecondGamepadPort );
```

El constructor toma el estado actual del dispositivo como punto de partida (el
adaptador conserva su último evento a través de los reinicios de la consola, de
modo que un evento antiguo no se lee como uno nuevo).

| Miembro                        | Qué hace                                                                                         |
| ------------------------------ | ------------------------------------------------------------------------------------------------ |
| `bool probe()`                 | Una vez por fotograma. Encola el nuevo evento de tecla o carácter escrito, si lo hay; verdadero cuando llegó uno. |
| `int read()`                   | La siguiente **pulsación** de tecla como su carácter escrito (con mayúsculas y bloqueo de mayúsculas aplicados, compatible con la fuente de la BIOS). Las teclas sin carácter devuelven su código de tecla (por debajo de 32, o 127). Los **caracteres escritos** enviados por el dispositivo (é, ß, ┌ ...) vuelven como su código en la fuente de la BIOS, más `v32::KeyboardGlyph` cuando es menor que 32. Las liberaciones se omiten. 0 cuando está vacía. |
| `int read_event(bool* pressed)` | El siguiente evento, pulsación **o** liberación, como su **código de tecla** (sin mayúsculas). `*pressed` se establece si no es `NULL`. Los caracteres escritos se omiten (no son teclas). 0 cuando está vacía. |
| `bool next_event(KeyEvent* e)` | El siguiente evento con todo: `e->key`, `e->symbol`, `e->pressed`, `e->text` (verdadero para un carácter escrito: `key` y `symbol` son entonces su código en la fuente de la BIOS). Falso cuando está vacía. |
| `int glyph(int value)`         | El carácter de la BIOS que **dibujar** para un valor que devolvió `read()`: ASCII y caracteres escritos tal cual, 0 para las teclas sin carácter. Ver [Caracteres escritos](#caracteres-escritos). |
| `int pending()`                | Eventos en espera.                                                                                 |
| `void flush()`                 | Descarta todos los eventos en espera (las teclas mantenidas siguen mantenidas).                    |
| `bool is_down(int key)`        | ¿Está esa tecla mantenida ahora? Toma códigos de tecla: `'a'`, `' '`, `v32::KeyLeft`, ...          |
| `bool shift_down()`, `ctrl_down()`, `alt_down()`, `gui_down()` | Cualquiera de las dos teclas del par mantenida.                      |
| `bool caps_lock_on()`          | Estado del bloqueo de mayúsculas (alternado con cada pulsación de bloqueo de mayúsculas).          |
| `int symbol(int key)`          | El carácter que escribe un código de tecla con el estado *actual* de mayúsculas / bloqueo de mayúsculas (distribución US). |
| `int last_event()`             | Contador de fotogramas en el último evento; -1 antes del primero.                                 |
| `int dropped()`                | Eventos perdidos porque la cola estaba llena.                                                      |
| `bool connected()`, `int port()` | El estado y el id del gamepad.                                                                   |
| `IoDevice* device()`           | El núcleo que hay debajo, para los controles empaquetados en bruto.                               |

`read()` y `read_event()`/`next_event()` toman de la misma cola (64 eventos): usa
un solo estilo por teclado. `is_down()` y las consultas de modificadores
funcionan junto a cualquiera de los dos, y no dependen en absoluto de leer la
cola — un juego que solo quiere WASD puede sondear en cada fotograma y no leer
nunca.

La función libre `v32::key_glyph( value )` es `glyph()` sin un teclado.

**Escribir frente a jugar.** Usa `read()` para texto (nombres, chat, una consola);
`is_down()` para los controles del juego (teclas de movimiento mantenidas);
`next_event()` cuando importan tanto la pulsación como la liberación (un
sintetizador asignado a teclas, por ejemplo). La biblioteca no genera repetición
de teclas: mantener una tecla es una pulsación, y luego una liberación.

**Los códigos de tecla** identifican teclas, no caracteres. Las teclas con un
carácter ASCII informan de él tal como se escribe **sin mayúsculas en una
distribución US**: `'a'`–`'z'`, `'0'`–`'9'`, espacio, y
`` ` - = [ ] \ ; ' , . / `` — compáralas con literales de carácter. Las demás:

| Código | Constante         | Código  | Constante                          |
| ------ | ----------------- | ------- | ---------------------------------- |
| 1      | `v32::KeyUp`      | 11      | `v32::KeyRCtrl`                    |
| 2      | `v32::KeyDown`    | 12      | `v32::KeyLAlt` (opción izquierda)  |
| 3      | `v32::KeyLeft`    | 13      | `v32::KeyEnter`                    |
| 4      | `v32::KeyRight`   | 14–25   | `v32::KeyF1` … `v32::KeyF12`       |
| 5      | `v32::KeyCapsLock`| 26      | `v32::KeyRAlt` (opción derecha)    |
| 6      | `v32::KeyLShift`  | 27      | `v32::KeyEscape`                   |
| 7      | `v32::KeyRShift`  | 28      | `v32::KeyLGui` (comando izquierdo / Windows) |
| 8      | `v32::KeyBackspace` | 29    | `v32::KeyRGui` (comando derecho)   |
| 9      | `v32::KeyTab`     | 127     | `v32::KeyDelete`                   |
| 10     | `v32::KeyLCtrl`   | 30, 31  | `v32::KeyPageHigh`, `v32::KeyPageLow`: prefijos de página de códigos de los caracteres escritos, nunca teclas (ver abajo); el 0 nunca se informa |

El teclado numérico informa de los mismos códigos que el teclado principal. Las
teclas de flecha (1–4) son distintas de `'a'`..`'z'` y de las direcciones propias
del gamepad.

### Caracteres escritos

Los códigos de tecla dicen qué tecla se pulsó, no qué escribió, de modo que no
pueden transportar una letra acentuada procedente de una tecla muerta o de AltGr,
ni un carácter de dibujo de cuadros. Un dispositivo v32io puede enviarlos en su
lugar como **caracteres escritos**, en el juego de caracteres de la fuente de la
BIOS de Vircon32: Windows-1252 (`é ñ ß © € « »` ...) con caracteres de dibujo de
cuadros y sombreados en lugar de los códigos 14 a 31 (`┌ ─ ┐ │ └ ┘ ░ ▒ ▓ █` ...).
`demos/cxx/v32io/glyphs` los muestra todos.

De dónde vienen:

- **El emulador modificado** envía lo que escriban la propia distribución de
  teclado del ordenador, las teclas muertas y la tecla de composición, siempre que
  la fuente de la BIOS tenga el glifo. Nada que aprender: escribe é como siempre.
- **El adaptador** solo sabe qué tecla está pulsada, así que tiene su propia capa
  para un teclado US: **AltGr** (Alt derecho; Option derecha en un Mac) más una
  tecla (AltGr+E = é, AltGr+S = ß, AltGr+C = ©, AltGr+5 = €), **teclas muertas**
  (AltGr+`'` y luego una letra le pone un acento agudo; lo mismo con `` ` `` `^`
  `¨` `~`), y **códigos Alt** (mantén el Alt *izquierdo*, escribe el código de la
  BIOS en el teclado numérico: `Alt 0233` es é, `Alt 021` es ─). Las tablas
  completas están en el repositorio v32io, `docs/v32io/COMPOSE.md`. Una
  combinación de teclas que escribe algo aquí *no* se informa además como una
  tecla.

Cómo los ve un programa:

```cpp
v32::Keyboard keyboard( v32::SecondGamepadPort );
int text[ 64 ];
int length = 0;

keyboard.probe();                           // once on EVERY frame
int key = keyboard.read();
while( key > 0 )
{
    int glyph = keyboard.glyph( key );      // what to draw, 0 for a non-character key
    if( glyph > 0 && length < 63 )
    {
        text[ length ] = glyph;             // ASCII, é, ß, ┌ ... all alike
        length++;
        text[ length ] = 0;
    }
    key = keyboard.read();
}
print_at( 0, 0, text );                     // the BIOS font draws them all
```

- `read()` devuelve un carácter escrito como su **código en la fuente de la
  BIOS** (32–126 y 128–255 tal cual). Un código por debajo de 32 se confundiría
  con un código de tecla (13 es Enter, 27 es Escape, pero 27 es también `├`), así
  que para esos `read()` devuelve el código **más `v32::KeyboardGlyph`** (256).
  **No pruebes tú mismo el valor devuelto: pásalo a `glyph()`**, que da el código
  que hay que dibujar (un carácter) o 0 (una tecla sin carácter: una flecha, F5,
  Escape, Delete...). `key >= 32 && key < 127`, la prueba de «ASCII imprimible»,
  sigue funcionando solo para ASCII.
- `next_event()` establece `e->text` para ellos, y `e->key` y `e->symbol` son
  ambos el código de la BIOS, nunca desplazado. `read_event()` los omite.
- Nunca tocan `is_down()`: las teclas de una combinación no se informan, de modo
  que un juego que lee teclas mantenidas no se ve afectado, y un programa que no
  lee caracteres nunca los ve.
- En el cable un carácter ocupa **dos eventos**, es decir, dos fotogramas: un
  prefijo de página de códigos (`v32::KeyPageHigh`, caracteres de la BIOS 128–255,
  o `v32::KeyPageLow`, 0–127) y una carga con los 7 bits bajos. Ambos son
  *liberaciones* de tecla, por lo que un controlador anterior a ellos ve solo dos
  liberaciones y ninguna pulsación. `probe()` hace todo esto; un prefijo que
  espera más de `v32::KeyboardPageTimeout` (30) fotogramas se olvida. Ver
  PROTOCOLS.md en el repositorio v32io para la descripción completa.
- Un gamepad recién conectado reinicia la decodificación (el estroboscopio y
  cualquier prefijo en espera), igual que con el ratón.

**Caracteres en literales de cadena.** El texto puede contener glifos de dibujo de
cuadros (códigos 14 a 31) como un literal corriente, `"\025\026"`. El compilador de
Vircon32 escribiría esos códigos de control en bruto en su ensamblador, y el
ensamblador los rechaza, así que v32c++ traslada ese literal a un arreglo global
oculto de `char` y usa ese en su lugar (`docs/VIRCON32_QUIRKS.es.md` n.º 28). Nada
que hacer en el código fuente. Todo lo que está desde el código 32 hacia arriba,
letras acentuadas incluidas, está bien en un literal, escrito con un escape: usa
**octal** (`"caf\351e"` es café; como máximo tres dígitos, así que lo que sigue es
seguro), porque C++ hace que un escape `\x` se trague todos los dígitos
hexadecimales que le siguen (`"caf\xE9e"` no es lo que parece; divide el literal,
`"caf\xE9" "e"`). El transpilador pasa cualquiera de los dos al compilador como
`\xE9`.

### `v32::Mouse`

```cpp
v32::Mouse mouse( v32::SecondGamepadPort );
```

El ratón mantiene un puntero: empieza en el centro de la pantalla, se mueve
`pasos × escala` píxeles por sondeo (2 píxeles por paso por defecto) y permanece
dentro de sus límites (toda la pantalla de 640×360 por defecto).

| Miembro                                  | Qué hace                                                                   |
| ---------------------------------------- | -------------------------------------------------------------------------- |
| `bool probe()`                           | Una vez por fotograma. Verdadero si el ratón se movió o cambió un botón.   |
| `int x()`, `int y()`                     | El puntero.                                                                |
| `void position(int* x, int* y)`          | Ambos a la vez (cualquiera puede ser `NULL`).                              |
| `int dx()`, `int dy()`                   | Movimiento en el último sondeo, en píxeles — se informa incluso cuando el puntero está detenido en sus límites, de modo que también sirve para controles de «mirada con ratón». |
| `void delta(int* dx, int* dy)`           | Ambos a la vez (cualquiera puede ser `NULL`).                              |
| `int steps_x()`, `int steps_y()`         | El mismo movimiento en pasos en bruto del dispositivo (−5…+5).             |
| `void set_position(int x, int y)`        | Coloca el puntero (se mantiene dentro de los límites).                     |
| `void set_bounds(int x0, int y0, int x1, int y1)` | Área en la que puede moverse el puntero, inclusiva.               |
| `void set_scale(int pixels)`, `int scale()` | Píxeles por paso (se ignora a menos que sea positivo).                  |
| `bool is_down(int buttons)`              | ¿Alguno de estos botones mantenido? `v32::MouseLeft`, `MouseRight`, `MouseMiddle`, `MouseAny`. |
| `bool pressed(int buttons)`, `released(int buttons)` | ¿Bajó / subió en el último sondeo?                             |
| `int buttons()`                          | Todos los botones mantenidos, como bits de `MouseButton`.                  |
| `int last_event()`                       | Fotograma del último movimiento o cambio de botón; -1 antes de cualquiera. |
| `bool connected()`, `int port()`, `IoDevice* device()` | Como en el teclado.                                          |

El adaptador mantiene cada cambio de botón durante al menos 25 ms, de modo que
incluso un clic muy rápido dura más de un fotograma y `pressed()` lo verá. **No
hay rueda de desplazamiento**: a los 11 controles no les queda sitio para ella.

El movimiento está limitado a 5 pasos por eje y fotograma (el protocolo no puede
distinguir 6 de −6), cosa que el adaptador respeta enviando como máximo un paso
por eje cada 5 ms — unos 200 pasos por segundo, o 400 píxeles por segundo con la
escala por defecto. Para un puntero más rápido, aumenta la escala.

### `v32::IoDevice`

El núcleo, para escribir un controlador propio o ver lo que envía un dispositivo
(ver `demos/cxx/v32io/monitor.cpp`):

| Miembro                  | Qué hace                                                             |
| ------------------------ | -------------------------------------------------------------------- |
| `IoDevice(int port)`     | Lee los controles una vez, de modo que `read()` empieza con el estado actual. |
| `bool probe()`           | Una vez por fotograma; verdadero si cambiaron los controles empaquetados. |
| `int read()`             | Controles empaquetados del último sondeo (`v32::IoLeft` … `v32::IoR`). |
| `int read_previous()`    | … y del sondeo anterior.                                             |
| `int changed()`          | Bits que cambiaron en el último sondeo.                              |
| `bool is_down(int mask)` | ¿Algún control de la máscara activo?                                 |
| `bool connected()`, `bool plugged()` | Conectado; pasó a conectado en el último sondeo.         |
| `int scan()`             | Lee los controles ahora mismo, sin tocar el historial de sondeos.    |
| `int port()`             | El id del gamepad.                                                   |

Las funciones libres `v32::mouse_counter()`, `v32::mouse_steps()` y
`v32::mouse_buttons()` son los pasos de decodificación del ratón por sí solos.

### Costes

Medido en la consola sin pantalla: sondear un teclado o un ratón cuesta bastante
menos del 1 % de la CPU de un fotograma (un programa de dibujo a pantalla completa
gasta ~0,3 % en el controlador del ratón y el resto en dibujar). Memoria: un
`Keyboard` ocupa unas 470 palabras (sobre todo su cola de 64 eventos y 128
banderas de teclas mantenidas), un `Mouse` unas 90, un `IoDevice` 75 (68 de ellas
su rutina de código máquina).

---

## Cómo funcionan los dispositivos

Todo dispositivo v32io se lee de la misma manera. Sus 11 controles se empaquetan
en un solo valor, los **controles empaquetados**, un bit por control en el orden
de puertos INP (el bit *n* es el puerto `0x402 + n`):

| Bit | Puerto  | Control   | Constante    | v32kbd           | v32mouse            |
| --- | ------- | --------- | ------------ | ---------------- | ------------------- |
| 0   | `0x402` | Left      | `v32::IoLeft`  | estroboscopio  | contador X: trit −  |
| 1   | `0x403` | Right     | `v32::IoRight` | estroboscopio  | contador X: trit +  |
| 2   | `0x404` | Up        | `v32::IoUp`    | tecla pulsada  | contador Y: trit −  |
| 3   | `0x405` | Down      | `v32::IoDown`  | tecla liberada | contador Y: trit +  |
| 4   | `0x406` | Start     | `v32::IoStart` | bit 0 del código de tecla | botón central |
| 5   | `0x407` | A         | `v32::IoA`     | bit 1 del código de tecla | botón izquierdo |
| 6   | `0x408` | B         | `v32::IoB`     | bit 2 del código de tecla | botón derecho |
| 7   | `0x409` | X         | `v32::IoX`     | bit 3 del código de tecla | contador X: Gray alto |
| 8   | `0x40A` | Y         | `v32::IoY`     | bit 4 del código de tecla | contador X: Gray bajo |
| 9   | `0x40B` | L         | `v32::IoL`     | bit 5 del código de tecla | contador Y: Gray alto |
| 10  | `0x40C` | R         | `v32::IoR`     | bit 6 del código de tecla | contador Y: Gray bajo |

La consola nunca muestra direcciones opuestas (Izquierda + Derecha, Arriba +
Abajo) a la vez — pulsar una libera la otra — y ambos protocolos están diseñados
en torno a eso.

Leer 11 puertos a través de la API de C supondría 11 llamadas, así que
`IoDevice` escribe en sí mismo una pequeña rutina de código máquina (`IN` de cada
puerto, `IGT` a 0/1, desplazarlo a su sitio, `OR` para incorporarlo; 68 palabras)
y `scan()` la invoca con `CALL` mediante un bloque `asm` de cuatro líneas — la
misma técnica que la biblioteca de C.

**Teclado.** El dispositivo informa como máximo de un evento de tecla por
fotograma, y lo mantiene hasta el siguiente. El **estroboscopio** distingue unos
eventos de otros: el primer evento muestra Left, el siguiente Right, luego Left
otra vez, de modo que ha llegado un evento nuevo siempre que el lado difiere del
último visto (lo que permite que la misma tecla llegue dos veces seguidas). Up
significa pulsación, Down significa liberación, y los siete botones llevan el
código de tecla de 7 bits. Mayúsculas y bloqueo de mayúsculas son teclas
corrientes; `Keyboard` las sigue para producir caracteres. Los caracteres que no
están en un teclado US (é, ß, ┌ ...) llegan como un **par** de eventos, un
prefijo de página de códigos (código de tecla 30 o 31) y una carga, ambos como
liberaciones de tecla: ver [Caracteres escritos](#caracteres-escritos).

**Ratón.** El movimiento no se envía como deltas — un delta mantenido durante un
fotograma no se distingue del mismo delta enviado dos veces — sino como dos
**contadores**, uno por eje, que recorren un ciclo de 12 posiciones. Cada paso
mueve un contador una posición y cambia **exactamente un control**, de modo que la
consola nunca ve un estado intermedio entre posiciones. Un contador es un código
Gray de 2 bits (4 grupos) más un *trit* en un par de direcciones opuestas (3
estados):

| Posición | 0  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 | 11 |
| -------- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- |
| Gray     | 00 | 00 | 00 | 01 | 01 | 01 | 11 | 11 | 11 | 10 | 10 | 10 |
| Trit     | −  | 0  | +  | +  | 0  | −  | −  | 0  | +  | +  | 0  | −  |

X usa Left/Right para su trit y X/Y para su código Gray; Y usa Up/Down y L/R. En
reposo tras el encendido ambos contadores están en la posición 1 (nada pulsado).
Cada sondeo compara las posiciones con las anteriores: se distinguen hasta ±5
pasos por fotograma. Esa disposición usa todos los estados que pueden mostrar los
11 controles (8 combinaciones de botones × 12 × 12 posiciones = 1152), por lo que
no hay sitio para una rueda. Cuando el gamepad acaba de conectarse, los contadores
empiezan de nuevo (de ese fotograma no se toma ningún movimiento).

La descripción completa del protocolo está en `lib/README.md` y `docs/PROTOCOLS.md`
del repositorio v32io.

---

## Port desde la biblioteca de C

| C (`keyboard.h`, `mouse.h`, `v32io.h`)                | C++ (`v32/keyboard.hpp`, `mouse.hpp`, `v32io.hpp`) |
| ----------------------------------------------------- | -------------------------------------------------- |
| `v32kbd *kb = v32kbd_init (SECOND_GAMEPAD_PORT);`     | `v32::Keyboard kb( v32::SecondGamepadPort );`      |
| `v32kbd_free (&kb);`                                  | — (nada que liberar)                               |
| `v32kbd_probe (&kb);`                                 | `kb.probe();`                                      |
| `v32kbd_read (&kb)`                                   | `kb.read()`                                        |
| `v32kbd_readevent (&kb, &pressed)`                    | `kb.read_event( &pressed )`                        |
| `v32kbd_getkey (&kb)` (+ `free()`)                    | `kb.next_event( &event )` (un `v32::KeyEvent`)     |
| `v32kbd_isdown (&kb, V32KEY_LCTRL)`                   | `kb.is_down( v32::KeyLCtrl )` o `kb.ctrl_down()`   |
| `v32kbd_glyph (key)`                                  | `kb.glyph( key )` o `v32::key_glyph( key )`        |
| `V32KBD_GLYPH`, `V32KBD_PAGE_TIMEOUT`                 | `v32::KeyboardGlyph`, `v32::KeyboardPageTimeout`   |
| `V32KEY_PAGE_HIGH`, `V32KEY_PAGE_LOW`                 | `v32::KeyPageHigh`, `v32::KeyPageLow`              |
| `key -> text` (un `v32key`)                           | `event.text` (un `v32::KeyEvent`)                  |
| `v32kbd_symbol (kb, key)`                             | `kb.symbol( key )`                                 |
| `v32kbd_scan (kb)`                                    | `kb.device()->scan()`                              |
| `kb -> capslock`                                      | `kb.caps_lock_on()`                                |
| `v32mouse *m = v32mouse_init (port);`                 | `v32::Mouse m( port );`                            |
| `v32mouse_position (&m, &x, &y)`                      | `m.position( &x, &y )` o `m.x()`, `m.y()`          |
| `v32mouse_delta (&m, &dx, &dy)`, `m -> dx`            | `m.delta( &dx, &dy )`, `m.dx()`                    |
| `m -> stepx`                                          | `m.steps_x()`                                      |
| `v32mouse_setposition / setbounds / setscale (&m, …)` | `m.set_position / set_bounds / set_scale( … )`     |
| `v32mouse_isdown / pressed / released (&m, V32MOUSE_LEFT)` | `m.is_down / pressed / released( v32::MouseLeft )` |
| `v32mouse_counter (…)`, `v32mouse_steps (…)`          | `v32::mouse_counter( … )`, `v32::mouse_steps( … )` |
| `v32io *io = v32io_init (port);` … `v32io_read (&io)` | `v32::IoDevice io( port );` … `io.read()`          |
| `V32IO_A`, `V32KEY_ESCAPE`, `V32MOUSE_ANY`            | `v32::IoA`, `v32::KeyEscape`, `v32::MouseAny`      |

Los nombres de las constantes siguen una sola regla: quita el prefijo de C, pon el
resto en CamelCase y colócalo en `v32::` (`V32KEY_LSHIFT` → `v32::KeyLShift`,
`FOURTH_GAMEPAD_PORT` → `v32::FourthGamepadPort`).

El comportamiento es el mismo, con estas diferencias deliberadas:

- La cola del teclado es un búfer circular fijo de 64 eventos dentro del objeto en
  lugar de una lista reservada con `malloc` (el mismo límite, sin tráfico de
  montón por tecla).
- Un evento que llega con la cola llena sigue actualizando `is_down()` y el
  bloqueo de mayúsculas; `dropped()` lo cuenta.
- La rutina del núcleo se regenera solo una vez, pero la dirección en la que
  guarda su resultado se refresca en cada exploración, de modo que un `IoDevice`
  que se copió sigue funcionando.

---

## Ejemplos

`demos/cxx/v32io/` (ver su [README](../demos/cxx/v32io/README.md)):

| Programa         | Dispositivos                     | Muestra                                                  |
| ---------------- | -------------------------------- | -------------------------------------------------------- |
| `typewriter.cpp` | teclado, Gamepad 2               | `read()`, mayúsculas / bloqueo de mayúsculas, estado de los modificadores |
| `paint.cpp`      | ratón, Gamepad 2                 | puntero, límites, flancos de botones, trazos suaves      |
| `monitor.cpp`    | cualquiera, los cuatro puertos   | `IoDevice` y el protocolo en bruto — una herramienta de configuración/depuración |
| `notepad.cpp`    | teclado Gamepad 2 + ratón Gamepad 3 | dos dispositivos v32io a la vez                       |
| `glyphs.cpp`     | teclado Gamepad 2 (dos de sus tres páginas no necesitan ninguno) | todos los glifos de la fuente de la BIOS, cómo escribirlos, caracteres escritos frente a teclas |

`cd demos/cxx/v32io && make` los construye (`v32c++` y las DevTools en el `PATH`).

---

## Probar sin un dispositivo

Las herramientas sin pantalla de `tools/vircon32/` (`v32peek`, `v32shot`,
`v32prof`) reproducen un guion de entrada de pulsaciones y liberaciones de
gamepad. Un dispositivo v32io *es* un gamepad, así que la entrada de teclado y de
ratón también puede guionizarse: `tools/vircon32/v32io-script.py` codifica
acciones con los protocolos reales.

```text
connect 1 1                  # plug a "gamepad" into port id 1 (Gamepad 2)
connect 2 1                  # ... and one into port id 2 (Gamepad 3)
at 300                       # the standard BIOS hands over around frame 250
kbd 1 type Hello, World!     # shift added as needed; \n is Enter
kbd 1 tap f1
kbd 1 char 233 0x80 20        # typed characters, by BIOS code: é € █ (2 frames each)
kbd 1 press lctrl
kbd 1 tap c
kbd 1 release lctrl
mouse 2 move 50 -20          # device steps, spread over frames (5 max per frame)
mouse 2 click left
wait 10
```

```sh
tools/vircon32/v32io-script.py actions.txt > script.txt
tools/vircon32/bin/v32shot tools/vircon32/bin/StandardBios.v32 game.v32 script.txt shot 400
```

`tests/119sample.cpp` es una prueba con autocomprobación de toda la cabecera
dirigida de esta manera (`tests/119sample.actions` → `tests/119sample.input`);
`make realcheck` la ejecuta. Su guion también se contrastó con los controladores
originales en C, que decodifican a partir de él los mismos 110 eventos de teclas y
posiciones de ratón.

`tests/131sample.cpp` hace lo mismo para los caracteres escritos: decodificación
de prefijo y carga (incluidas cargas iguales a los códigos de prefijo), el tiempo
de espera, un prefijo que se encuentra con una desconexión / reconexión,
`read()` + `glyph()`, y `read_event()` omitiendo caracteres. `kbd <port> char
<code>...` envía caracteres como lo hacen el adaptador y el emulador; `connect
<port> 0` y luego `1` simula una reconexión (el dispositivo nuevo empieza de
cero). La demo `glyphs` tiene su propio guion, `demos/cxx/v32io/glyphs.actions`,
que reproduce una sesión de escritura para poder fotografiar sus tres páginas con
`v32shot` sin un teclado; las capturas de pantalla de `demos/cxx/v32io/images/`
salieron de ahí.

---

## Límites

- **Solo Vircon32.** El núcleo lee los puertos INP de la consola mediante una
  rutina de código máquina; con `--target=standard` la cabecera se transpila pero
  no significa nada.
- **Distribución US.** Los códigos de tecla son teclas físicas informadas al
  estilo US, y `symbol()`/`read()` aplican las reglas de mayúsculas US. Los
  caracteres impresos de otras distribuciones (y AltGr) no se traducen.
- **Los caracteres son lo que la fuente de la BIOS puede dibujar**: los códigos de
  tecla son de 7 bits e identifican teclas; todo lo que va más allá de ASCII
  (acentos, símbolos, dibujo de cuadros) llega como un carácter escrito, en el
  código propio de la fuente (Windows-1252 más dibujo de cuadros). Los caracteres
  que la fuente no tiene (cirílico, CJK, ...) no pueden escribirse. En el
  adaptador necesitan un teclado US y su capa de AltGr / teclas muertas / códigos
  Alt; los caracteres impresos de otras distribuciones no se traducen (el
  emulador usa la distribución del ordenador y sí los traduce).
- Los caracteres escritos ocupan dos fotogramas cada uno (dos eventos), así que la
  escritura rápida con acentos se encola: 30 caracteres por segundo como máximo.
  Los glifos de dibujo de cuadros en un literal de cadena se reescriben en
  arreglos ocultos (ver arriba).
- **Un evento por fotograma.** El protocolo del teclado transporta como máximo 60
  eventos por segundo (30 pulsaciones, ya que pulsar y soltar son eventos
  separados), haga lo que haga quien escribe. No se genera repetición de teclas.
- **Sin rueda de desplazamiento**, y el movimiento del ratón está limitado a 5
  pasos por eje y fotograma (aumenta la escala para un puntero más rápido).
- La cola guarda 64 eventos; los que excedan de ahí se descartan (los cuenta
  `dropped()`) si el programa no lee.
- Los nombres de clase `IoDevice`, `Keyboard` y `Mouse` se emiten sin cualificar en
  el C generado (una limitación general de v32c++ para las clases en un espacio de
  nombres), así que un programa no puede definir una clase propia con uno de esos
  nombres.
