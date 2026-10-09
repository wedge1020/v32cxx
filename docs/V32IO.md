# v32io: keyboard and mouse input

`v32/v32io.hpp` gives v32c++ programs a real **keyboard** and **mouse** on
the Vircon32 console. It is a C++ port of the
[v32io](https://github.com/wedge1020/v32io) Vircon32 C drivers (`lib/v32io.h`,
`lib/keyboard.h`, `lib/mouse.h`): the same protocols and the same decoding,
wrapped as three classes.

| Class           | Replaces (C library)            | What it is                                        |
| --------------- | ------------------------------- | ------------------------------------------------- |
| `v32::IoDevice` | `v32io` / `v32io_*()`           | the core: a gamepad's 11 controls read as data    |
| `v32::Keyboard` | `v32kbd` / `v32kbd_*()`         | key events, typed characters, held keys           |
| `v32::Mouse`    | `v32mouse` / `v32mouse_*()`     | a pointer, movement and three buttons             |

```cpp
#include <v32/v32io.hpp>

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

## ⚠ Requirement: a v32io device

**This functionality REQUIRES either the v32io hardware adaptor or a
modified Vircon32 DesktopEmulator.** It does not work with the stock
emulator on its own, and nothing about a cartridge can change that.

The Vircon32 console has no keyboard or mouse port. A v32io device reaches
it through a **gamepad port**: the console sees an ordinary gamepad, but its
11 controls (four directions and seven buttons) carry keyboard or mouse
data instead of game presses. Something outside the console must produce
that data:

1. **The v32io hardware adaptor** — custom firmware (`v32io-pico`) on a
   Waveshare RP2350-USB-A board (with its R13 resistor removed). Plug a
   standard USB keyboard or mouse into it, and plug it into the computer:
   it presents itself as a joystick named `v32io:kbd` or `v32io:mouse`.
   Because it is "just a gamepad" to the computer, it works with **any**
   Vircon32 emulator, stock or modified, once a joystick profile maps it
   (see [Joystick profile](#joystick-profile-for-the-adaptor)). Firmware
   and build notes: the v32io repository, `firmware/RP2350-USB-A/`.
2. **A modified Vircon32 DesktopEmulator** — no hardware needed; the host
   computer's own keyboard and mouse become v32io devices. Either:
   - the fork at <https://github.com/wedge1020/ComputerSoftware>, **`v32io`
     branch**, which has the `v32kbd` and `v32mouse` devices built in; or
   - the stock DesktopEmulator sources with the two patches from the v32io
     repository applied (`emulator/v32kbd.patch`, `emulator/v32mouse.patch`).

> The stock emulator's own **Keyboard** gamepad device is *not* a v32io
> keyboard: it maps a few keys onto gamepad buttons. A `v32::Keyboard`
> reading it sees nonsense. Likewise, an ordinary gamepad on the port a
> program expects a v32io device on produces random key events and mouse
> movement.

Without a device, a program using `v32io.hpp` still builds and runs: the
port is simply not connected (`connected()` is false) or shows nothing, and
no events arrive. The examples print a "no device" message in that case.

---

## Setup

### Gamepad numbers

The emulator's menus count gamepads from 1; the console (and this header)
counts port ids from 0:

| Emulator menu | Port id | Constant                   |
| ------------- | ------- | -------------------------- |
| Gamepad 1     | 0       | `v32::FirstGamepadPort`    |
| Gamepad 2     | 1       | `v32::SecondGamepadPort`   |
| Gamepad 3     | 2       | `v32::ThirdGamepadPort`    |
| Gamepad 4     | 3       | `v32::FourthGamepadPort`   |

The convention used by the examples is **Gamepad 2** for the keyboard (and
for the mouse when it is alone), **Gamepad 3** for the mouse when both are
used, leaving Gamepad 1 for an ordinary gamepad. Any port works; the
program decides.

### Modified DesktopEmulator

Open the *Gamepads* menu, pick the gamepad your program expects, and select
**`v32kbd`** or **`v32mouse`**.

- `v32kbd` can't be selected while any gamepad uses the stock *Keyboard*
  device, and vice versa: both need the host keyboard.
- Emulator hotkeys (Esc, F2, F4, F5 and the Ctrl shortcuts) keep working,
  and are also sent to the program.
- With `v32mouse`, focusing the emulator window **captures the pointer**.
  Press **left Ctrl + left Alt** to release it (the *left* Alt, to stay
  clear of AltGr layouts).

### Joystick profile for the adaptor

The adaptor appears as a joystick, `v32io:kbd` or `v32io:mouse` depending
on what is plugged into it, and each needs a joystick profile in the
emulator (made with *EditControls*; the adaptor has a setup mode for
this, described in the v32io firmware README). Both profiles map the same
way — **button *n* to control *n***:

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

The GUID and exact name depend on the computer, so take them from
*EditControls* with the adaptor plugged in. Then assign the joystick to the
gamepad the program expects.

**Check a setup with `demos/cxx/v32io/monitor`.** It shows, for all four
ports, the 11 controls exactly as the console sees them and what they mean
to each driver. A control that never lights, or two that always move
together, is a profile mapping mistake.

---

## Using `v32io.hpp`

Add the repository root (the directory holding `v32/`) with `-I`, or
install the headers with `sudo make sysinstall`:

```sh
v32c++ -I path/to/v32c++ -o obj/game.c game.cpp
```

Like every `v32/` header, it is C++ source inlined into your program. It is
self-contained: the v32io **C** library is not needed to build a v32c++
program (and the two are not meant to be mixed in one program). It
includes the SDK's `input.h`, `time.h` and `misc.h`, which pass through to
the generated C.

### The one rule: probe every frame

Call `probe()` **once on every frame** for **every** device, before
`end_frame()`. The console updates gamepads only between frames, and a
keyboard reports at most one key event per frame: a frame without a probe
can lose a key, or misread mouse movement. Extra `probe()` calls in the same
frame do nothing, so it is safe to probe from more than one place.

Programs with several screens (a title, a menu, the game) should probe in
every one of them — or keep one probe at the top of a single main loop.

### Objects

A `Keyboard` or `Mouse` is an ordinary object: a global, or a local in
`main`. There is nothing to free. Make one per device, each with its own
port; a keyboard and a mouse can be used at the same time. Copying one is
harmless but pointless (the copy has its own queue and state); pass a
pointer or reference instead.

### `v32::Keyboard`

```cpp
v32::Keyboard keyboard( v32::SecondGamepadPort );
```

The constructor takes the device's current state as the starting point
(the adaptor keeps its last event across console resets, so an old event
is not read as a new one).

| Member                         | What it does                                                                                     |
| ------------------------------ | ------------------------------------------------------------------------------------------------ |
| `bool probe()`                 | Once per frame. Queues the new key event, if any; true when one arrived.                          |
| `int read()`                   | Next key **press** as its typed character (shift and caps lock applied, BIOS-font compatible). Keys without a character return their key code (below 32, or 127). Releases are skipped. 0 when empty. |
| `int read_event(bool* pressed)` | Next event, press **or** release, as its **key code** (no shift). `*pressed` is set if not `NULL`. 0 when empty. |
| `bool next_event(KeyEvent* e)` | Next event with everything: `e->key`, `e->symbol`, `e->pressed`. False when empty.                |
| `int pending()`                | Events waiting.                                                                                    |
| `void flush()`                 | Throw every waiting event away (held keys stay held).                                              |
| `bool is_down(int key)`        | Is that key held now? Takes key codes: `'a'`, `' '`, `v32::KeyLeft`, ...                           |
| `bool shift_down()`, `ctrl_down()`, `alt_down()`, `gui_down()` | Either key of the pair held.                                         |
| `bool caps_lock_on()`          | Caps lock state (toggled by each caps lock press).                                                 |
| `int symbol(int key)`          | The character a key code types under the *current* shift / caps lock state (US layout).           |
| `int last_event()`             | Frame counter at the latest event; -1 before the first.                                           |
| `int dropped()`                | Events lost because the queue was full.                                                            |
| `bool connected()`, `int port()` | The gamepad's state and id.                                                                      |
| `IoDevice* device()`           | The core underneath, for the raw packed controls.                                                 |

`read()` and `read_event()`/`next_event()` take from the same queue (64
events): use one style per keyboard. `is_down()` and the modifier queries
work alongside either, and don't depend on reading the queue at all — a
game that only wants WASD can probe every frame and never read.

**Typing vs. playing.** Use `read()` for text (names, chat, a console);
`is_down()` for game controls (movement keys held down); `next_event()`
when both press and release matter (a key-mapped synthesizer, say). The
library generates no key repeat: holding a key is one press, then one
release.

**Key codes** identify keys, not characters. Keys with an ASCII character
report it as typed **unshifted on a US layout**: `'a'`–`'z'`, `'0'`–`'9'`,
space, and `` ` - = [ ] \ ; ' , . / `` — compare those with character
literals. The others:

| Code | Constant          | Code    | Constant                           |
| ---- | ----------------- | ------- | ---------------------------------- |
| 1    | `v32::KeyUp`      | 11      | `v32::KeyRCtrl`                    |
| 2    | `v32::KeyDown`    | 12      | `v32::KeyLAlt` (left option)       |
| 3    | `v32::KeyLeft`    | 13      | `v32::KeyEnter`                    |
| 4    | `v32::KeyRight`   | 14–25   | `v32::KeyF1` … `v32::KeyF12`       |
| 5    | `v32::KeyCapsLock`| 26      | `v32::KeyRAlt` (right option)      |
| 6    | `v32::KeyLShift`  | 27      | `v32::KeyEscape`                   |
| 7    | `v32::KeyRShift`  | 28      | `v32::KeyLGui` (left command / Windows) |
| 8    | `v32::KeyBackspace` | 29    | `v32::KeyRGui` (right command)     |
| 9    | `v32::KeyTab`     | 127     | `v32::KeyDelete`                   |
| 10   | `v32::KeyLCtrl`   | 30, 31  | unused; 0 is never reported        |

The numeric keypad reports the same codes as the main keyboard. The arrow
keys (1–4) are distinct from `'a'`..`'z'` and from the gamepad's own
directions.

### `v32::Mouse`

```cpp
v32::Mouse mouse( v32::SecondGamepadPort );
```

The mouse keeps a pointer: it starts at the center of the screen, moves
`steps × scale` pixels per probe (2 pixels per step by default) and stays
inside its bounds (the whole 640×360 screen by default).

| Member                                   | What it does                                                               |
| ---------------------------------------- | -------------------------------------------------------------------------- |
| `bool probe()`                           | Once per frame. True if the mouse moved or a button changed.               |
| `int x()`, `int y()`                     | The pointer.                                                               |
| `void position(int* x, int* y)`          | Both at once (either may be `NULL`).                                       |
| `int dx()`, `int dy()`                   | Movement in the latest probe, in pixels — reported even when the pointer is stopped at its bounds, so it also suits "mouse look" controls. |
| `void delta(int* dx, int* dy)`           | Both at once (either may be `NULL`).                                       |
| `int steps_x()`, `int steps_y()`         | The same movement in raw device steps (−5…+5).                             |
| `void set_position(int x, int y)`        | Place the pointer (kept within bounds).                                    |
| `void set_bounds(int x0, int y0, int x1, int y1)` | Area the pointer can move in, inclusive.                          |
| `void set_scale(int pixels)`, `int scale()` | Pixels per step (ignored unless positive).                              |
| `bool is_down(int buttons)`              | Any of these buttons held? `v32::MouseLeft`, `MouseRight`, `MouseMiddle`, `MouseAny`. |
| `bool pressed(int buttons)`, `released(int buttons)` | Went down / up in the latest probe?                            |
| `int buttons()`                          | Every button held, as `MouseButton` bits.                                  |
| `int last_event()`                       | Frame of the latest movement or button change; -1 before any.            |
| `bool connected()`, `int port()`, `IoDevice* device()` | As for the keyboard.                                         |

The adaptor holds every button change for at least 25 ms, so even a very
quick click lasts more than a frame and `pressed()` will see it. There is
**no scroll wheel**: the 11 controls have no room left for it.

Movement is limited to 5 steps per axis per frame (the protocol can't tell
6 from −6), which the adaptor respects by sending at most one step per
axis every 5 ms — about 200 steps per second, or 400 pixels per second at
the default scale. For a faster pointer, raise the scale.

### `v32::IoDevice`

The core, for writing a driver of your own or looking at what a device
sends (see `demos/cxx/v32io/monitor.cpp`):

| Member                   | What it does                                                         |
| ------------------------ | -------------------------------------------------------------------- |
| `IoDevice(int port)`     | Reads the controls once, so `read()` starts with the current state. |
| `bool probe()`           | Once per frame; true if the packed controls changed.                 |
| `int read()`             | Packed controls of the latest probe (`v32::IoLeft` … `v32::IoR`).    |
| `int read_previous()`    | … and of the probe before it.                                        |
| `int changed()`          | Bits that changed in the latest probe.                               |
| `bool is_down(int mask)` | Any control in the mask on?                                          |
| `bool connected()`, `bool plugged()` | Connected; became connected in the latest probe.         |
| `int scan()`             | Read the controls right now, without touching the probe history.    |
| `int port()`             | The gamepad id.                                                      |

Free functions `v32::mouse_counter()`, `v32::mouse_steps()` and
`v32::mouse_buttons()` are the mouse decoding steps on their own.

### Costs

Measured on the headless console: probing a keyboard or a mouse costs well
under 1% of a frame's CPU (a full-screen paint program spends ~0.3% in the
mouse driver and the rest drawing). Memory: a `Keyboard` is about 400
words (mostly its 64-event queue and 128 held-key flags), a `Mouse` about
90, an `IoDevice` 75 (68 of them its machine-code routine).

---

## How the devices work

Every v32io device is read the same way. Its 11 controls are packed into
one value, the **packed controls**, one bit per control in INP port order
(bit *n* is port `0x402 + n`):

| Bit | Port    | Control   | Constant     | v32kbd           | v32mouse            |
| --- | ------- | --------- | ------------ | ---------------- | ------------------- |
| 0   | `0x402` | Left      | `v32::IoLeft`  | strobe         | X counter: trit −   |
| 1   | `0x403` | Right     | `v32::IoRight` | strobe         | X counter: trit +   |
| 2   | `0x404` | Up        | `v32::IoUp`    | key pressed    | Y counter: trit −   |
| 3   | `0x405` | Down      | `v32::IoDown`  | key released   | Y counter: trit +   |
| 4   | `0x406` | Start     | `v32::IoStart` | key code bit 0 | middle button       |
| 5   | `0x407` | A         | `v32::IoA`     | key code bit 1 | left button         |
| 6   | `0x408` | B         | `v32::IoB`     | key code bit 2 | right button        |
| 7   | `0x409` | X         | `v32::IoX`     | key code bit 3 | X counter: Gray high|
| 8   | `0x40A` | Y         | `v32::IoY`     | key code bit 4 | X counter: Gray low |
| 9   | `0x40B` | L         | `v32::IoL`     | key code bit 5 | Y counter: Gray high|
| 10  | `0x40C` | R         | `v32::IoR`     | key code bit 6 | Y counter: Gray low |

The console never shows opposite directions (Left + Right, Up + Down) on at
once — pressing one releases the other — and both protocols are designed
around that.

Reading 11 ports through the C API would mean 11 calls, so `IoDevice`
writes a small machine-code routine into itself (`IN` each port, `IGT` it
into 0/1, shift it into place, `OR` it in; 68 words) and `scan()` `CALL`s
it through a four-line `asm` block — the same technique as the C library.

**Keyboard.** The device reports at most one key event per frame, and holds
it until the next. The **strobe** tells events apart: the first event shows
Left, the next Right, then Left again, so a new event has arrived whenever
the side differs from the last one seen (which is what lets the same key
arrive twice in a row). Up means press, Down means release, and the seven
buttons carry the 7-bit key code. Shift and caps lock are ordinary keys;
`Keyboard` tracks them to produce characters.

**Mouse.** Movement is not sent as deltas — a delta held for a frame can't
be told from the same delta sent twice — but as two **counters**, one per
axis, going around a cycle of 12 positions. Each step moves a counter one
position and changes **exactly one control**, so the console never sees a
state halfway between positions. A counter is a 2-bit Gray code (4 groups)
plus a *trit* on a pair of opposite directions (3 states):

| Position | 0  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 | 11 |
| -------- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- |
| Gray     | 00 | 00 | 00 | 01 | 01 | 01 | 11 | 11 | 11 | 10 | 10 | 10 |
| Trit     | −  | 0  | +  | +  | 0  | −  | −  | 0  | +  | +  | 0  | −  |

X uses Left/Right for its trit and X/Y for its Gray code; Y uses Up/Down
and L/R. At rest after power-on both counters sit at position 1 (nothing
pressed). Each probe compares positions with the previous ones: up to ±5
steps per frame can be told apart. That layout uses every state the 11
controls can show (8 button combinations × 12 × 12 positions = 1152), which
is why there is no room for a wheel. When the gamepad has just been
connected, the counters start over (no movement is taken from that frame).

The full protocol description is the v32io repository's `lib/README.md`
and `docs/PROTOCOLS.md`.

---

## Porting from the C library

| C (`keyboard.h`, `mouse.h`, `v32io.h`)                | C++ (`v32/v32io.hpp`)                              |
| ----------------------------------------------------- | -------------------------------------------------- |
| `v32kbd *kb = v32kbd_init (SECOND_GAMEPAD_PORT);`     | `v32::Keyboard kb( v32::SecondGamepadPort );`      |
| `v32kbd_free (&kb);`                                  | — (nothing to free)                                |
| `v32kbd_probe (&kb);`                                 | `kb.probe();`                                      |
| `v32kbd_read (&kb)`                                   | `kb.read()`                                        |
| `v32kbd_readevent (&kb, &pressed)`                    | `kb.read_event( &pressed )`                        |
| `v32kbd_getkey (&kb)` (+ `free()`)                    | `kb.next_event( &event )` (a `v32::KeyEvent`)      |
| `v32kbd_isdown (&kb, V32KEY_LCTRL)`                   | `kb.is_down( v32::KeyLCtrl )` or `kb.ctrl_down()`  |
| `v32kbd_symbol (kb, key)`                             | `kb.symbol( key )`                                 |
| `v32kbd_scan (kb)`                                    | `kb.device()->scan()`                              |
| `kb -> capslock`                                      | `kb.caps_lock_on()`                                |
| `v32mouse *m = v32mouse_init (port);`                 | `v32::Mouse m( port );`                            |
| `v32mouse_position (&m, &x, &y)`                      | `m.position( &x, &y )` or `m.x()`, `m.y()`         |
| `v32mouse_delta (&m, &dx, &dy)`, `m -> dx`            | `m.delta( &dx, &dy )`, `m.dx()`                    |
| `m -> stepx`                                          | `m.steps_x()`                                      |
| `v32mouse_setposition / setbounds / setscale (&m, …)` | `m.set_position / set_bounds / set_scale( … )`     |
| `v32mouse_isdown / pressed / released (&m, V32MOUSE_LEFT)` | `m.is_down / pressed / released( v32::MouseLeft )` |
| `v32mouse_counter (…)`, `v32mouse_steps (…)`          | `v32::mouse_counter( … )`, `v32::mouse_steps( … )` |
| `v32io *io = v32io_init (port);` … `v32io_read (&io)` | `v32::IoDevice io( port );` … `io.read()`          |
| `V32IO_A`, `V32KEY_ESCAPE`, `V32MOUSE_ANY`            | `v32::IoA`, `v32::KeyEscape`, `v32::MouseAny`      |

Constant names follow one rule: drop the C prefix, CamelCase the rest, and
put it in `v32::` (`V32KEY_LSHIFT` → `v32::KeyLShift`,
`FOURTH_GAMEPAD_PORT` → `v32::FourthGamepadPort`).

Behavior is the same, with these deliberate differences:

- The keyboard queue is a fixed 64-event ring buffer inside the object
  instead of a `malloc`'d list (same limit, no heap traffic per key).
- An event that arrives with the queue full still updates `is_down()` and
  caps lock; `dropped()` counts it.
- The core's routine is regenerated only once, but the address it stores
  its result at is refreshed on every scan, so an `IoDevice` that was
  copied still works.

---

## Examples

`demos/cxx/v32io/` (see its [README](../demos/cxx/v32io/README.md)):

| Program          | Devices                          | Shows                                                    |
| ---------------- | -------------------------------- | -------------------------------------------------------- |
| `typewriter.cpp` | keyboard, Gamepad 2              | `read()`, shift / caps lock, modifier state              |
| `paint.cpp`      | mouse, Gamepad 2                 | pointer, bounds, button edges, smooth strokes            |
| `monitor.cpp`    | any, all four ports              | `IoDevice` and the raw protocol — a setup/debug tool     |
| `notepad.cpp`    | keyboard Gamepad 2 + mouse Gamepad 3 | two v32io devices at once                            |

`cd demos/cxx/v32io && make` builds them (`v32c++` and the DevTools on the
`PATH`).

---

## Testing without a device

The headless tools in `tools/vircon32/` (`v32peek`, `v32shot`, `v32prof`)
play an input script of gamepad presses and releases. A v32io device *is* a
gamepad, so keyboard and mouse input can be scripted too:
`tools/vircon32/v32io-script.py` encodes actions with the real protocols.

```text
connect 1 1                  # plug a "gamepad" into port id 1 (Gamepad 2)
connect 2 1                  # ... and one into port id 2 (Gamepad 3)
at 300                       # the standard BIOS hands over around frame 250
kbd 1 type Hello, World!     # shift added as needed; \n is Enter
kbd 1 tap f1
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

`tests/119sample.cpp` is a self-checking test of the whole header driven
this way (`tests/119sample.actions` → `tests/119sample.input`); `make
realcheck` runs it. Its script was also cross-checked against the original
C drivers, which decode the same 110 key events and mouse positions from
it.

---

## Limits

- **Vircon32 only.** The core reads the console's INP ports through a
  machine-code routine; under `--target=standard` the header transpiles
  but means nothing.
- **US layout.** Key codes are physical keys reported US-style, and
  `symbol()`/`read()` apply US shift rules. Other layouts' printed
  characters (and AltGr) are not translated.
- **ASCII only**: 7-bit key codes, characters the BIOS font can draw.
- **One event per frame.** The keyboard protocol carries at most 60 events
  a second (30 keystrokes, since press and release are separate events),
  whatever the typist does. No key repeat is generated.
- **No scroll wheel**, and mouse movement is capped at 5 steps per axis
  per frame (raise the scale for a faster pointer).
- The queue holds 64 events; events beyond that are dropped (counted by
  `dropped()`) if the program doesn't read.
- Class names `IoDevice`, `Keyboard` and `Mouse` are emitted bare in the
  generated C (a general v32c++ limitation for classes in a namespace), so
  a program can't define its own class with one of those names.
