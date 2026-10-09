# v32io examples

Four small programs using [`v32/keyboard.hpp`](../../../v32/keyboard.hpp)
and [`v32/mouse.hpp`](../../../v32/mouse.hpp) (both built on
[`v32/v32io.hpp`](../../../v32/v32io.hpp)), the
v32c++ keyboard and mouse support. Full documentation:
[`docs/V32IO.md`](../../../docs/V32IO.md).

> **These need a v32io device.** A v32io keyboard or mouse reaches the
> console through a gamepad port, and something outside the console has
> to produce its data: **either** the v32io hardware adaptor (a USB
> keyboard or mouse plugged into the v32io-pico board, which works with
> any Vircon32 emulator), **or** a modified Vircon32 DesktopEmulator with
> the `v32kbd` / `v32mouse` gamepad devices — the fork at
> <https://github.com/wedge1020/ComputerSoftware/tree/v32io> (its `v32io`
> branch), or the stock emulator patched with the v32io repository's
> `emulator/*.patch`. The stock emulator's own *Keyboard* device is **not**
> a v32io keyboard. Without a device, the programs run but report the
> missing device and do nothing else useful (except `monitor`, which then
> just shows your gamepads).

| Program          | Devices                                   | What it shows                                                                 |
| ---------------- | ----------------------------------------- | ----------------------------------------------------------------------------- |
| `typewriter.cpp` | keyboard on **Gamepad 2**                 | `v32::Keyboard::read()`, shift / caps lock, modifier state                    |
| `paint.cpp`      | mouse on **Gamepad 2**                    | `v32::Mouse`: pointer, bounds, button edges, movement                          |
| `monitor.cpp`    | anything, on all four ports               | `v32::IoDevice`: the raw 11 controls, decoded both ways — a setup/debug tool   |
| `notepad.cpp`    | keyboard on **Gamepad 2**, mouse on **Gamepad 3** | two v32io devices at once                                             |

Gamepad 1 is left for an ordinary gamepad in every example. Each program
names its port(s) in a `#define` near the top (`KEYBOARD_PORT`,
`MOUSE_PORT`) if your setup differs.

## Building

```sh
make            # bin/typewriter.v32, bin/paint.v32, bin/monitor.v32, bin/notepad.v32
make debug      # the same, with -g debug maps
make clean
```

Needs `v32c++` and the Vircon32 DevTools (`compile`, `assemble`,
`packrom`) on the `PATH`. The Makefile passes `-I ../../..`, so the
repository's own `v32/` headers are used without `make sysinstall`. The
top-level `demos/Makefile` builds this directory along with the others.

## Running

**Modified DesktopEmulator:** load the cartridge, then in the *Gamepads*
menu pick *Gamepad 2* and select `v32kbd` (typewriter) or `v32mouse`
(paint); for notepad, `v32kbd` on Gamepad 2 and `v32mouse` on Gamepad 3.
`v32kbd` can't be selected while any gamepad uses the *Keyboard* device
(both need the host keyboard). With `v32mouse`, focusing the window
captures the pointer: **left Ctrl + left Alt** releases it.

**Hardware adaptor:** plug the adaptor (keyboard or mouse in it) into the
computer, and give the `v32io:kbd` / `v32io:mouse` joystick a profile that
maps **button *n* to control *n*** (see docs/V32IO.md, "Joystick profile").
Then assign that joystick to the gamepad the program expects. If something
looks wrong, run `monitor` first: it shows exactly what each port sends.

## Testing without a device

`tools/vircon32/v32io-script.py` turns keyboard and mouse actions into an
input script for the headless tools, which is how these examples were
checked:

```sh
cat > typing.act <<'EOF'
connect 1 1
at 300
kbd 1 type Hello, Vircon32!
EOF
tools/vircon32/v32io-script.py typing.act > typing.txt
tools/vircon32/bin/v32shot tools/vircon32/bin/StandardBios.v32 \
    demos/cxx/v32io/bin/typewriter.v32 typing.txt shot 360
```

(`at 300`: the standard BIOS hands over to the cartridge around frame 250.)
