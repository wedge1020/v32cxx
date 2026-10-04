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
| `src/` | Rogue 5.4.4: 30 `.c` files. 29 are byte-identical to upstream; `state.c` has eight one-line changes (below); the three headers gained an include guard and nothing else. |
| `v32/mach_v32.c` | Replaces `mach_dep.c`, `mdport.c` and `xcrypt.c` (users, shell, signals, terminal modes). Same function names and signatures. |
| `v32/title.c` | The title screen, which stands in for a command line (below). |
| `v32/pad.c` | Gamepad to keyboard (below). |
| `v32/config.h` | Stands in for autoconf's `config.h`. |
| `../../../libc/` | Shipped with v32c++: a small C library (`v32libc`), an 80x24 text terminal (`v32term`) and curses on top of it (`v32curses`). |

## Curses on Vircon32

`v32term` keeps an 80x24 grid of 8x15-pixel cells on the 640x360 screen.
Glyphs are regions defined over the BIOS texture (-1) font, located by
reading the BIOS's own region for each character (so any BIOS version
works); the cell background and reverse video are the solid block
(character 20). Only
cells that changed are redrawn, and a repaint pauses for the next frame
when the GPU's per-frame pixel budget runs low. `v32curses` implements
the windows, `move`/`addch`/`mvwinch`/`printw`/`clrtoeol`/`standout`,
`getch`, overlays and refresh that Rogue uses. Rogue is monochrome; a
hook in curses lets `pad.c` colour dungeon symbols without touching
Rogue's drawing code.

## Title screen, saved games and scores

On Unix what Rogue does is decided by its command line. Here a title
screen is that command line: v32c++ calls `v32_main_args()` (in
`v32/title.c`) at the top of `main`, and each menu entry hands Rogue's own
`main` the arguments it stands for.

| Menu entry | Command line | What Rogue does |
|---|---|---|
| New game | `rogue` | plays |
| Resume the saved game | `rogue -r` | `restore()` in `save.c` |
| Hall of fame | `rogue -s` | prints the top ten |
| Instructions | (none) | three pages, drawn by `title.c` |

Saving is Rogue's own `save.c` and `state.c`, writing through `fopen` /
`putc` / `fread` to the memory card (`libc/v32file.c`). **Y**, then *Save
the game and stop*, writes the game and returns to the title screen. As
in Unix Rogue, resuming a saved game deletes it: a save is for taking a
break, not a second chance. The top ten is Rogue's score file, kept open
on the card for the whole session.

The one source change: `state.c` writes an `int` as "4 bytes" and a
`short` as "2". On Vircon32 every scalar is one word and `sizeof(int)` is
1, so those eight calls would read and write past the variable. They now
say `sizeof(c)` / `sizeof(input)` instead of the literal, which is also
correct on every other machine.

On the memory card (262,144 words):

| | Words |
|---|---|
| signature, directory | 230 |
| top ten (`rogue.scr`) | 11,240 |
| one saved game (`rogue.save`) | about 20,000 |

A card that belongs to another game is never written, and without a card
the game still plays; the title screen says which it is.

The banner uses the BIOS font's shade blocks (characters 17 to 20).

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
| Y | Everything-else menu: rest, stairs, repeat, traps, stats, discoveries, help, options, save, quit... |
| START | On-screen keyboard: any key Rogue understands (L shift, R ctrl) |

Prompts are answered in context. `pad.c` reads the message line off the
terminal, as a player would, so Rogue's sources need no hooks:

- `--More--` / "press space": any button
- "which object...?": a menu of the pack, filtered to the kind asked for
- "which direction?": the D-pad
- yes/no (quit, save), left/right hand: a two-entry menu
- free text (naming an item): the on-screen keyboard

In menus: up/down, A picks, B cancels.

## Differences from Unix Rogue

- One saved game, under a fixed name, on the memory card.
- No wizard mode, shell escape or user names (the player is "Rogue").
- `unsigned` is plain signed int on Vircon32 (v32c++ warns once).
