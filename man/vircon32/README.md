# Vircon32 SDK manual pages (English)

Manual pages for the Vircon32 desktop emulator and development tools
(`vircon32/ComputerSoftware`, DevTools v26.04.24, emulator v26.02.18),
written against the sources and checked with `mandoc -Tlint`. Every
example program was compiled with the real Vircon32 compiler and
assembler, and the ones that do something observable were run on the
emulator's own console core.

## Contents

### Section 1: commands

| Page | Program |
| --- | --- |
| `Vircon32.1` | the desktop emulator (including the `v32kbd`/`v32mouse` devices of the `v32io` branch, marked as such) |
| `EditControls.1` | the keyboard/joystick profile editor |
| `compile.1` | C compiler, with a summary of the Vircon32 C dialect and preprocessor |
| `assemble.1` | assembler, with the assembly language, instruction set and directives |
| `packrom.1` | ROM packer and the ROM definition XML |
| `unpackrom.1` | ROM unpacker |
| `png2vircon.1`, `vircon2png.1` | texture import/export |
| `wav2vircon.1`, `vircon2wav.1` | sound import/export |
| `tiled2vircon.1` | Tiled map layers to `.vmap` |
| `joinpngs.1` | texture atlas builder |
| `disassemble.1` | disassembler |

### Section 3v32: the C library headers

| Page | Header |
| --- | --- |
| `vircon32.3v32` | overview: the headers, programming model, memory map, ports, hardware errors |
| `video.h.3v32` | GPU |
| `input.h.3v32` | gamepads |
| `audio.h.3v32` | SPU |
| `time.h.3v32` | timer |
| `math.h.3v32` | math |
| `string.h.3v32` | characters, strings, number formatting |
| `misc.h.3v32` | memory, allocator, random numbers |
| `memcard.h.3v32` | memory cards |

Plus one alias page (`.so`) per function, struct and typedef, so that
`man draw_region_at` or `man 3v32 card_read_data` opens the header's
page. Names that standard C also uses (`memcpy`, `malloc`, `sin`,
`strlen`, `sleep`, ...) have no alias, so they never shadow the system's
own pages; their header page documents them.

## Why section `3v32`

Several header names (`string.h`, `math.h`, `time.h`) and many function
names collide with the system's C library documentation. A section
suffix, like OpenSSL's `3ssl`, keeps both:

    man 3v32 string.h        # this library
    man 3 string             # the system's
    man vircon32             # the emulator, Vircon32(1)
    man 3v32 vircon32        # the library overview

## Installing

These pages are part of the v32c++ project (`man/vircon32/`, kept apart
from v32c++'s own pages in `man/man1` and `man/man3`), and v32c++'s
`make sysinstall` and `cmake --install` put them into the same
`man1`/`man3` as its own, gzipped like them. To install just these by
hand:

    sudo install -d /usr/local/share/man/man1 /usr/local/share/man/man3
    sudo install -m 644 man1/*.1    /usr/local/share/man/man1/
    sudo install -m 644 man3/*.3v32 /usr/local/share/man/man3/
    sudo mandb -q            # Linux (man-db); macOS needs nothing

(Run from this directory.) Alias pages refer to `man3/<header>.3v32` relative to the manual root,
so keep the `man1`/`man3` layout.

## Bugs found while writing these

Documented in the pages concerned (mostly under BUGS):

- `assemble --debugmode` is listed in `--help` but rejected.
- `compile -Wall` is listed but does nothing.
- `ftoa()` never returns for a value outside the int range: it writes
  `-2147483648.` and then `0` characters without end, past the buffer.
  (Its old rounding-carry bug, 0.999999 printed as `0.1`, is fixed in
  the current `string.h`; the DevTools 26.04.24 sources still have it.)
- `tiled2vircon -o dir` needs a trailing slash (`-o obj` writes
  `objLayer.vmap`); flipped tiles and empty cells are not distinguished.
- `unpackrom` writes `Make.sh` without execute permission.

Several cross-references point at the v32c++ pages (`v32c++-headers`,
`*.hpp`, `v32libc`), which live in `../man1` and `../man3` and are
installed together with these.
