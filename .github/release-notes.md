**v32cxx** is a C/C++ transpiler for [Vircon32](https://www.vircon32.com), in the spirit of Stroustrup's original `cfront`: you write C++ (or plain C), it writes the Vircon32 C that the official toolchain already knows how to build.

## Highlights

- **A practical subset of C++.** Classes and single inheritance, constructors and destructors, virtual functions, operator and conversion operators, references, namespaces, overloading, `auto`, range-based `for`, `override`/`final`, and objects by value. Templates, exceptions, RTTI and multiple inheritance are out of scope by design.
- **Containers without templates.** `std::string`, `std::array<T, N>` and `std::vector<T>` are expanded at transpile time for each element type you use.
- **Plain C too.** `v32c` takes ordinary C and rewrites it into Vircon32's C dialect, with a small libc and curses so Unix programs build as they are: the demo is Rogue 5.4.4, from its original sources.
- **Linking, done by the transpiler.** Give `v32c` several files (or a `--mklib` library index) and it merges them into one program: prototypes and `extern`s bind across files, file-scope `static` names stay private, each file keeps its own macros, and a duplicate or undefined symbol is reported. Gap reports from porting xv6-riscv are closed: designated initializers (with `[a ... b]`), compound literals, `_Static_assert`, function-typed parameters, GCC atomic builtins, and more. xv6's 20 user programs and its whole kernel (RISC-V assembly aside) now build with the real Vircon32 compiler.
- **Carts without the paperwork.** `#title`, `#texture` and `#sound` hints in your source generate the cart-packing XML and ready-to-run `Make.sh` / `Make.bat` build scripts.
- **Vircon32 API wrappers.** `<v32/video.hpp>` and friends add typed enums, overloads and RAII scope guards on top of the real C functions, plus **keyboard and mouse** support through v32io (needs a v32io device).
- **Output you can read and debug.** `-vv` explains the generated C as it goes, `-g` writes a C-line to C++-line map, and `--target=standard` emits portable C.
- **Documented.** Manual pages for the tool and every header, and the README and reference documents in English, Spanish and French.
- **Checked against the real thing.** `make realcheck` runs the output of 128 test programs through the official Vircon32 compiler and assembler, and boots 59 self-checking programs on a headless console.
- **Demos.** A Tempest-style tube shooter, Space Invaders++, a Star Raiders-style game, Spy vs Spy in plain C, Rogue, and keyboard and mouse examples.

## Downloads

Pick the zip for your system: `v32c++-@VERSION@-<os>-<arch>.zip`.

| System | Architectures |
| --- | --- |
| Windows | x86_64, arm64 |
| macOS | x86_64 (Intel), arm64 (Apple silicon) |
| Linux | x86_64, arm64, armv7 and armv8 (Raspberry Pi) |
| OpenBSD, Haiku | included when their (optional) builds succeeded |

Each zip unpacks straight into your `v32tools` folder (`/usr/local/Vircon32/v32tools`, or `C:\Program Files\Vircon32\v32tools` on Windows) and holds `v32c++` and `v32c`, the C++ headers, the C library, the documents and the manual pages. Check a download against `SHA256SUMS.txt`. You also need the Vircon32 DevTools on your `PATH` to build cartridges.

## Quick start

```sh
v32c++ game.cpp      # writes game.c, game.xml, Make.sh and Make.bat
bash Make.sh         # compile, assemble, convert assets, pack the cartridge
```

See the [README](https://github.com/wedge1020/v32cxx#readme) for the full feature list and the [known limitations](https://github.com/wedge1020/v32cxx#known-limitations). Bug reports and feature requests are welcome in the issues.
