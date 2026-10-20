# v32c++ library manual pages (English)

Section  3  manual  pages  for   the  headers  that  come  with  `v32c++`
(see `v32c++ --version`): the `v32/` C++  headers (including the `v32io` keyboard
and mouse libraries), the  built-in `<string>`, `<array>` and `<vector>`,
and the C-mode library in  `libc/`. They complement `man/man1/v32c++.1`. Every
example was run through v32c++, the real Vircon32 compiler and assembler;
the memory-card, terminal and printf ones were also run on the emulator's
console core.

## Contents

| Page | Covers |
| --- | --- |
| `v32c++-headers.3v32` | overview: the three groups, search paths, conventions |
| `v32-video.hpp.3v32` | `BlendMode`, `TextureScope`, `Point`, draw helpers |
| `v32-input.hpp.3v32` | bool readers, `Button`, `GamepadScope` |
| `v32-audio.hpp.3v32` | `Channel`, `SoundScope`, `ChannelScope` |
| `v32-time.hpp.3v32` | `Date`, `TimeOfDay`, `Stopwatch`, `FrameScope` |
| `v32-math.hpp.3v32` | `minimum`, `maximum`, `absolute`, `clamp`, ... |
| `v32-string.hpp.3v32` | `v32::String` |
| `v32-misc.hpp.3v32` | random ranges, word helpers, `HeapBlock` |
| `v32-memcard.hpp.3v32` | `MemoryCard` |
| `v32-v32io.hpp.3v32` | `IoDevice`, the v32io core |
| `v32-keyboard.hpp.3v32` | `Keyboard`, key codes, the v32kbd protocol |
| `v32-mouse.hpp.3v32` | `Mouse`, the v32mouse protocol |
| `v32c++-string.3v32` | `std::string` |
| `v32c++-array.3v32` | `std::array` |
| `v32c++-vector.3v32` | `std::vector` |
| `v32libc.3v32` | stdio, stdlib, string, ctype, errno, signal, time for C input |
| `v32file.3v32` | stdio files on the memory card |
| `v32term.3v32` | the 80x24 terminal, gamepad polling, on-screen keyboard |
| `v32curses.3v32` | curses |

Plus  alias   pages  (`.so`)  for   every  `v32::`  class   and  function
(`man  v32::Keyboard`, `man  v32::clamp`) and  every `v32_`,  `v32term_`,
`v32pad_`, `v32osk_`  and `v32file_`  name. Standard  C and  curses names
(`printf`, `fopen`, `initscr`,  ...) have no alias, so  they never shadow
the system's pages.

The  section is  `3v32`, shared  with the  Vircon32 SDK  pages, which
these pages refer to (`vircon32(3v32)`, `video.h(3v32)`, ...) and which
live in [`vircon32/`](vircon32/README.md): the emulator and DevTools
commands (section 1) and the C library headers (section 3v32). They are
kept apart because they document the SDK, not v32c++, and carry its
version (`make version` does not stamp them). They are installed with
these. No page name is used by both sets.

## Installing

    sudo install -d /usr/local/share/man/man3
    sudo install -m 644 man3/*.3v32 /usr/local/share/man/man3/
    sudo mandb -q            # Linux (man-db); macOS needs nothing

`make sysinstall` and `cmake --install` install both sets (gzipped, mode
0644, aliases as symbolic links) into the same `man1` and `man3`.

## Translations (Spanish and French)

The v32c++ pages (`man1/v32c++.1` and every section 3v32 page above) are also
translated: [`es/`](es/) (Spanish) and [`fr/`](fr/) (French), each with the same
`man1` and `man3` layout. The alias pages are copied as they are (a `.so
man3/<page>` resolves against the manual root of the tree it is in). The
Vircon32 SDK pages in `vircon32/` stay in English and are installed with every
language. `make version` stamps the translated pages too, with the date written
in their language.

`make sysinstall` and `cmake --install` take `DEST_LANG=all|en|es|fr` (default
`all`). `all` installs the English pages in `man1` and `man3` and the
translations in `$MANROOT/es/man{1,3}` and `$MANROOT/fr/man{1,3}`, which
`man` picks from `LANG` (`LANG=es_ES.UTF-8 man v32c++`). `en` installs only the
English pages. `es` or `fr` installs only that language's pages, directly in
`man1` and `man3`.

    sudo make sysinstall DEST_LANG=fr
    cmake -S . -B build -DDEST_LANG=es && cmake --install build

Without installing, read one in place: `man -l es/man1/v32c++.1`.
