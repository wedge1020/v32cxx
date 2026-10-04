# Rogue 5.4.4 for Vircon32

The Unix dungeon crawl, built into a Vircon32 cartridge by `v32c++`.
The point of the demo: Rogue's game sources are **as written for Unix** —
K&R-era C, curses, `stdio`, `unsigned`, `struct` tags, comma operators,
standard array and function-pointer declarators — and `v32c++` turns
them into Vircon32 C that the stock `compile` accepts with no errors.

## Building

    make            # transpile, compile, assemble, pack -> bin/rogue.v32
    make debug
    make SEED=3     # fixed random seed, for reproducible runs
    make clean

Needs `v32c++` and the Vircon32 DevTools (`compile`, `assemble`,
`packrom`) on PATH. No textures or sounds: everything is drawn from the
BIOS texture.

## Layout

| Path | What it is |
|------|------------|
| `rogue.c` | The one file that is compiled. Pools everything below with `#include`. |
| `src/` | Rogue 5.4.4. The 28 `.c` files are byte-identical to upstream; the three headers gained an include guard and nothing else. |
| `v32/mach_v32.c` | Replaces `mach_dep.c`, `mdport.c`, `save.c`, `state.c`, `xcrypt.c` (users, shell, signals, files). Same function names and signatures. |
| `v32/pad.c` | Gamepad to keyboard (below). |
| `v32/config.h` | Stands in for autoconf's `config.h`. |
| `../../../libc/` | Shipped with v32c++: a small C library (`v32libc`), an 80x24 text terminal (`v32term`) and curses on top of it (`v32curses`). |

## Curses on Vircon32

`v32term` keeps an 80x24 grid of 8x15-pixel cells on the 640x360 screen.
Glyphs are regions defined over the BIOS texture (-1) font; the cell
background and reverse video are the solid block (character 20). Only
cells that changed are redrawn, and a repaint pauses for the next frame
when the GPU's per-frame pixel budget runs low. `v32curses` implements
the windows, `move`/`addch`/`mvwinch`/`printw`/`clrtoeol`/`standout`,
`getch`, overlays and refresh that Rogue uses. Rogue is monochrome; a
hook in curses lets `pad.c` colour dungeon symbols without touching
Rogue's drawing code.

## Controls

Rogue has about forty single-key commands. The gamepad covers the
frequent ones directly and reaches the rest through menus:

| Input | Action |
|-------|--------|
| D-pad | Move (two directions together: diagonal) |
| L + D-pad | Run |
| R + D-pad | Fight to the death in that direction |
| A | Search; on a staircase, take the stairs |
| R + A | Search ten times |
| X | Inventory |
| B | Item menu: eat, quaff, read, wield, wear, take off, rings, throw, zap, drop, pick up, call |
| Y | Everything-else menu: rest, stairs, repeat, traps, stats, discoveries, help, options, quit... |
| START | On-screen keyboard: any key Rogue understands (L shift, R ctrl) |

Prompts are answered in context. `pad.c` reads the message line off the
terminal, as a player would, so Rogue's sources need no hooks:

- `--More--` / "press space": any button
- "which object...?": a menu of the pack, filtered to the kind asked for
- "which direction?": the D-pad
- yes/no, left/right hand: a two-entry menu
- free text (naming an item): the on-screen keyboard

In menus: up/down, A picks, B cancels.

## Differences from Unix Rogue

- No saved games. The top-ten list is kept on the memory card.
- No wizard mode, shell escape or user names (the player is "Rogue").
- `unsigned` is plain signed int on Vircon32 (v32c++ warns once).
