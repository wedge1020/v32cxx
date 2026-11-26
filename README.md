# v32cxx: C/C++ Transpiler for Vircon32

[Spanish version / en español](README.es.md) | [French version / en français](README.fr.md) <!-- langlinks -->

**A  C/C++-subset-to-C  transpiler,  targeting  the
[Vircon32](https://www.vircon32.com) fantasy console's C compiler.**

The project is **v32cxx**; it builds two programs, `v32c++` (C++ or C input)
and `v32c` (the same program, named for C input).

`v32c++` lets  you write games and  programs for Vircon32 in  a subset of
C++  —  classes,  inheritance, constructors  and  destructors,  virtual
functions, operator overloading, references, `std::string`, `std::vector`
— and  translates them into the  C that the Vircon32  toolchain already
knows  how to  build. It  is  the approach  Bjarne Stroustrup's  original
`cfront` took in the 1980s: rather than compiling C++ straight to machine
code, translate it to C first and  let an existing, trusted C compiler do
the  rest.  It  also  reads  plain C  (old  Unix  C  included),  handling
everything Vircon32's own C dialect does differently.

> **Status: functional.** The whole  pipeline — preprocessing, parsing,
> semantic analysis,  lowering and code  generation — works end  to end
> and is  checked against the  **real** Vircon32 compiler,  assembler and
> console (`make  realcheck`: 241  programs compiled  and assembled,  172 of
> them self-checking  at run time  on a headless console).  Five sizeable
> demos (three C++ games, a port of Rogue and a plain-C Spy vs Spy) build
> with it. See [Known
> limitations](#known-limitations)  for the  honest  picture  of what  is
> missing.

## Why this exists

Four audiences, one project:

0. **A historical exploration.** The first C++ implementation  (`cfront`)
   was a transpiler that operates in similar fashion.  Undertaking such an
   endeavour has been quite interesting,  revealing patterns along with a
   number of considerations that might not otherwise have been exposed.

1. **A teaching  tool.** It started as  course material for a  class that
   teaches game programming on Vircon32,  beginning in plain C and moving
   on to C++ and object-oriented design. Showing students *how* a virtual
   function or a constructor gets parsed, resolved and lowered into plain
   C is as much the point as having a working tool — so the generated C
   is meant to be read (see `-vv`).

2. **A community tool.** The Vircon32  homebrew community writes games in
   C; this  lets people opt into  a small, well-understood subset  of C++
   instead, without the weight of templates, exceptions or a full STL.

3. **A normal C gateway.** The Vircon32 C compiler implements a subset of
   C with various syntactical quirks (array declarations and the function
   pointer declaration  syntax, among  others). Using  `v32c`/`v32c++` as
   the front  end code parser, it  will also translate normal  C into the
   Vircon32 C dialect for compilation,  minimizing the learning curve for
   getting the development process started.

It is **not** an attempt to support  all of C++ — see [Deliberately out
of scope](#deliberately-out-of-scope).

## Contents

- [What it supports](#what-it-supports)
- [C++ headers for the Vircon32 API](#c-headers-for-the-vircon32-api)
- [Memory management](#memory-management)
- [Keyboard and mouse: v32io](#keyboard-and-mouse-v32io)
- [Known limitations](#known-limitations)
- [Requirements](#requirements) · [Building](#building) · [Checking with the real toolchain](#checking-with-the-real-toolchain) · [Debugging with v32sim](#debugging-with-v32sim)
- [Using it](#using-it) · [Cart packaging](#cart-packaging) · [Output dialects](#output-dialects)
- [Demos](#demos) · [Documentation](#documentation) · [Project layout](#project-layout)
- [Installing](#installing) · [Versioning](#versioning) · [Feedback](#feedback)

## What it supports

### Classes and objects

- `class`  and `struct`  (the  only difference  is  the default  access),
  `public`/`protected`/`private`  (enforced,  through  inheritance  too),
  `friend`  classes and  functions,  forward  declarations (`class  X;`),
  classes inside namespaces.

- **Single  inheritance**,  with  derived-to-base pointer  and  reference
  conversions; a typedef of a class names a base too (`typedef Base B; class D : public B { D() : B(3) {} };`).

- **Constructors and  destructors**, in-class or  out-of-line; overloaded
  constructors; copy constructors;  default arguments. Member-initializer
  lists for base  classes (`: Base(args)`), primitive  members (`: x(v)`,
  in *declaration* order,  as C++ requires) and members  that are objects
  (`: mVel(vx, vy)`). In-class default member initializers (`int mLives =
  3;`), also for an array of objects (`Counter m[2] = { {1}, Counter(2) };`,
  or `Holder() : m{ {1}, {2} } {}`: each element is built in place; the
  elements left out are default-constructed). A base  with a  default
  constructor  is constructed  implicitly; destructors chain to members and
  bases (member arrays too, of any number of dimensions, last element first).

- Objects  on  the   stack  (`Shape  s;`,  `Shape   s(7);`),  as  unnamed
  temporaries (`return  Vec(x, y);`, `v.push_back(Enemy(1, 2,  3));`), in
  arrays  (constructed and  destroyed element  by element,  member arrays
  included; `Enemy a[2] =  { Enemy(1), {2} };` builds each element in place,
  as C++17 does, also for several dimensions and at file scope),  at file  scope  (constructed  at the  top  of `main`,  with
  constructor arguments if  given, and destroyed, last first, when `main`
  ends), and on the  heap with `new`/`delete`/`new[]`/`delete[]` (elements built and destroyed; opt-in arena or garbage collector: see *Memory management*).
  Destructors  run at  every  scope exit:  the  closing brace,  `return`,
  `break`, `continue`.

- **`virtual`  functions** and  dynamic  dispatch  through vtables,  pure
  virtual functions (`= 0`), virtual destructors (`delete` through a base
  pointer).

- **Operator overloading**: arithmetic,  comparison, compound assignment,
  unary minus  and `!`,  assignment, `[]`,  function call  `()` (function
  objects:  `f(5)` on  an  object), and  prefix/postfix `++`/`--`  (C++'s
  `operator++()` / `operator++(int)`), as members or free functions, used
  with natural syntax (`a + b`, `list[i]->draw()`, `++it`, `it++`). Also
  `% & | ^ << >>` and `%= &= |= ^= <<= >>=`, unary `+` `-` `~`, unary `*`
  and `->` (a smart pointer; `p->m` is `(p.operator->())->m`, again while
  the result is an object with an `operator->`), `&&` and `||` (both sides
  evaluated, left first, no short-circuit, as in C++) and `,`; stream-style
  chains (`out << a << b`, left to right) work. Unary `&` cannot be
  overloaded (an error says so).

- **Conversion operators**  (`operator int()  const`, `operator  bool()`,
  `operator Vec2  *()`), inline  or out  of line,  inherited too:  called
  wherever the object is  used as a value of that  type — an initializer,
  an assignment, a return, a  condition (`if (handle)`, `!handle`, `&&`),
  a cast, an  argument (also choosing between  overloads), arithmetic and
  comparison, an index. An `operator int()`  alone also serves a float or
  bool target;  with several candidates and  no exact one, it  reports an
  ambiguity.

- **`override` and `final`** on member functions, and `final` on a class,
  checked  as C++  checks  them (an  `override`  that overrides  nothing,
  overriding a  `final` function  and deriving from  a `final`  class are
  errors); they stay ordinary  names elsewhere (`int final;`). `explicit`
  is accepted.

- **Overloading** of  functions and  methods, resolved by  argument count
  and type; it reports an ambiguity instead of guessing.

- Objects **by  value**: a struct  or class larger  than one word  can be
  passed  and returned  by  value;  v32c++ rewrites  it  into the  hidden
  pointers Vircon32 C needs (`docs/VIRCON32_QUIRKS.md` #11).

- **References**  — parameters,  return  values,  locals, data  members
  (`int &r;`, bound  in the constructor's initializer list),  `const T &`
  binding to temporaries, and references to pointers (`void reset(Node*& p)`,
  lowered to `Node**`; `Node*& r = p;`, and a function may return one).

- `const`  (variables, pointers  to const,  `T *  const`, `const`  member
  functions) — accepted and emitted, not enforced.

### The rest of the language

- Namespaces (nested, with their global objects built and destroyed like file-scope ones, qualified calls, namespaced  C names for functions and variables:
  `v32::draw(int,int,int)`    becomes   `v32__draw__int_int_int`, `a::x` becomes `a__x`;
  out-of-line definitions may be written qualified, `int a::f(E e) {...}`, `void a::C::m() {...}`,
  `a::C::C(int x) : v(x) {}`, `a::C::~C() {}`, and mean what they would inside the namespace:
  default arguments from the declaration, its names visible unqualified),    and
  `using`: `using  namespace v32;` at  file scope  or in a  block, `using
  v32::Keyboard;`, and `using  Points = int;`. Functions  found through a
  directive overload with the ones in scope, as in C++.

- `auto`  locals  and  range-based   `for`  (over  arrays,  `std::array`,
  `std::vector`, and any class with `begin()`/`end()`).

- **`std::string`**    (`#include   <string>`),    and   **`std::array<T,
  N>`**   and  **`std::vector<T>`**   (`#include  <array>`,   `<vector>`)
  —   not   templates,  but   fixed   forms   the  transpiler   expands
  per   element   type.    See   [`docs/STRING.md`](docs/STRING.md)   and
  [`docs/GENERICS.md`](docs/GENERICS.md).

- The  whole C  statement  and expression  language:  `switch` with  real
  fall-through, `do`/`while`, `goto`, the ternary  operator and the comma
  operator (both  rewritten for Vircon32  C, which has  neither), bitwise
  operators  at C's  precedence, `sizeof`,  C-style  and C++-style  casts
  (`dynamic_cast` with  a warning:  there is  no RTTI),  `enum`  (also  scoped:  `enum class Color : int { Red }`,  used  as  `Color::Red`),  `union`,
  `typedef`,  function pointers  in standard  **or** Vircon32  declarator
  syntax  (output is  always what  the  target needs; initialised at file or
  namespace scope too, singly or in arrays, with an overloaded name picked by the
  pointer's type),  multi-dimensional
  arrays with  constant-expression sizes, brace  initializers, hex/octal/
  binary   literals    with   suffixes,   `nullptr`,    adjacent   string
  concatenation, multiple declarators  (`int a, *b, c[4];`,  and objects:
  `Point a(1, 2), b(3, 4);`).

- **Variadic functions**: `...` with  `va_list`, `va_start`, `va_arg` and
  `va_end` from `<cstdarg>`,  in free functions, namespaces  and methods,
  as in C (see [`docs/C_INPUT.md`](docs/C_INPUT.md)). Each extra argument
  is one word: an int, char, bool, enum,  pointer or float — an object is
  an error  (pass a pointer, or  a string's `c_str()`). A  `...` overload
  ranks after every other, as in C++.

- `static` locals  (moved to file  scope under unique  names), file-scope
  `static`,   `extern`,  `volatile`,   `inline`,  `register`   (accepted;
  Vircon32 C has none of them). `unsigned`, `signed`, `short`, `long` are
  the one  32-bit `int`  type (`unsigned` with  a warning);  `double` and
  `long  double` are  `float`, Vircon32's  one floating  type. Bit-fields
  are  accepted  as   full  words  with  a  warning,   or  rejected  with
  `--reject-bit-fields`.

- **Inline   assembly**   in   Vircon32's   `asm   {   "..."   }`   form,
  plus  GCC   basic  `asm("...")`  re-emitted   in  that  form   —  see
  [`docs/INLINE_ASSEMBLY_SUPPORT.md`](docs/INLINE_ASSEMBLY_SUPPORT.md).

- **`native Name;`** declares  a type defined by a  pass-through C header
  (`date_info`, `game_signature`)  so it can  be used by pointer  — see
  [`docs/NATIVE_PASSTHROUGH.md`](docs/NATIVE_PASSTHROUGH.md).  Struct and
  typedef  names from  SDK headers  v32c++ can  read are  declared native
  automatically.
  A plain C struct in such a header (ints, floats, bools, pointers, arrays
  of literal length: the SDK's `malloc_block`) is read, not just named, so
  `b->size`, `b->next`, `sizeof(malloc_block)` and by-value locals work on
  it. A struct v32c++ cannot follow stays opaque.

### A C++-side preprocessor

Before parsing, v32c++ does what a C++ preprocessor would:

- `#include`  of  `.hpp`/`.cpp`  files   is  inlined,  recursively,  with
  `#pragma once`.  Quote form looks next  to the including file,  then in
  each `-I` directory, then the current directory; angle form only in the
  `-I` directories. Both  then fall back to `$V32CXX_INCLUDE`  and to the
  installed headers in `/usr/local/Vircon32/v32tools/include/v32c++` (the
  default,  set in  [`inc/config.h`](inc/config.h)). Multi-file  projects
  work the usual way, and errors name the file they are in.
- An `#include` inside a `{ }` body (a function, an `asm { }` block) is
  expanded right where it stands, for a file of any extension, so
  generated tables (`"integer 1, 2, 3"` lines for an `asm` block) or runs
  of statements can live in their own files. An SDK header or an
  unfound file there still passes through, moved to the top of the
  output, and v32c++ says so with a warning.
- `#define`/`#undef`   (object-like   and   function-like,   `#`,   `##`,
  `__VA_ARGS__`),   `-D`/`-U`,   predefined   `__V32CXX__`,   `__FILE__`,
  `__LINE__`;  `#if`/`#ifdef`/`#elif`/...   with  `defined()`;  `#error`,
  `#warning`.
- `.h`  includes   pass  through   to  the   generated  C,   **and**  are
  read   when   v32c++   can  find   them   (`$V32CXX_SDK_INCLUDE`,   the
  `include/`  folder  next to  the  Vircon32  `compile` on  your  `PATH`,
  then  `/usr/local/Vircon32/DevTools/include`),   so  SDK   macros  like
  `screen_width` or  `color_red` work in  array sizes and `#if`,  and are
  typed for overload resolution. `-v` lists the folders searched; `V32CXX_SDK_INCLUDE=` (set but empty) turns the search off, as if the SDK were not installed. A symlinked `compile` is followed. When a `#include` names an SDK header (`misc.h`, `video.h`, ...) that cannot be found at all, v32c++ now warns, since a type it defines (`malloc_block *b;`) would otherwise be read as a multiplication.
- Defines are passed through to the generated C, and named constants keep
  their names  there; macros Vircon32's  own preprocessor can't  take are
  expanded instead.  Line numbers in  diagnostics and `-g` maps  point at
  the original file and line.

### C input

A file  named `.c`  is transpiled  as C: its  own `.h`/`.c`  includes are
expanded,  struct  tags get  their  own  namespace, K&R-style  prototypes
that  say  less  than  the definition  are  unified,  variadic  functions
work,  the  null  pointer  is  0, and  `main(argc,  argv)`  is  accepted.
With  the small  C  library and  curses  in `libc/`,  that  is enough  to
build  the  Unix  game  Rogue from  its  sources  (`demos/c/rogue`).  See
[`docs/C_INPUT.md`](docs/C_INPUT.md).

`v32c` is the same program under a second name, for C: `v32c game.c` and
`v32c++ game.c` do exactly the same today (the file's extension decides),
and `make`/`cmake` build and install both. Build scripts call whichever
name you ran. A C file's default output would be itself (`game.c` →
`game.c`), so without `-o` v32c++ writes `game-transpiled.c` instead, and
`-o` naming the input is an error.

## C++ headers for the Vircon32 API

`v32/` holds  C++ headers in  the `v32::`  namespace, one per  SDK header
—  `video.hpp`,  `input.hpp`,  `string.hpp`,  `time.hpp`,  `audio.hpp`,
`math.hpp`,   `misc.hpp`,   `memcard.hpp`   —   plus   **`v32io.hpp`**,
**`keyboard.hpp`**  and  **`mouse.hpp`**  (keyboard  and  mouse,  below).
Each  SDK  wrapper  passes  its  `.h` through  and  adds  only  what  the
C  API   can't  express   well:  typed   enums  instead   of  `#define`s,
overload   sets   (`v32::minimum`,  `v32::clamp`,   `v32::absolute`   for
int  and  float),   RAII  scope  guards  for   the  console's  "selected"
texture/gamepad/sound/  channel  state  (`TextureScope`,  `GamepadScope`,
`SoundScope`,  `ChannelScope`, `FrameScope`),  and  small handle  classes
(`v32::Channel`,   `v32::MemoryCard`,  `v32::HeapBlock`,   `v32::String`,
`v32::Stopwatch`). Every  hardware access still  goes through the  real C
function, and wrapper  names never reuse a C  function's name (`minimum`,
not `min`), so the raw C API stays callable alongside them.

Use them with  `-I` pointing at the directory that  holds `v32/` (`v32c++
-I path/to/v32c++ game.cpp`, then `#include <v32/video.hpp>`), or install
them (see [Installing](#installing)).

## Memory management

`new`, `new[]` and the `std::vector` buffers use the SDK's `malloc`/`free`
unless you ask otherwise. `--alloc=arena` (off by default; the output is
unchanged without it) takes one block of `--heap-words=N` words (default
262144) from the SDK's `malloc` at the start of `main` -- so a cartridge
restart acquires it again -- and serves every generated allocation from it
by bumping a pointer. Nothing is freed one by one: `delete` runs the
destructor and keeps the space, and `<v32/memory.hpp>` gives it back in bulk:

```cpp
#include <v32/memory.hpp>
int level = v32::arena_mark();       // at the start of a level or a frame
Enemy *boss = new Enemy(3);
std::vector<int> path;               // grows inside the arena
v32::arena_release(level);           // boss's destructor runs; all is back
```

`arena_mark()`, `arena_release(mark)`, `arena_reset()`, `arena_used()` and
`arena_free()` (words). Objects whose class has a destructor are recorded
in the arena, and a release runs the destructors of those still alive, newest
first; an object you already `delete`d is not destroyed twice (nor is a
`delete[]`d array). A growing `std::vector` allocates anew and copies, the old
buffer coming back only on release; `std::string` keeps its characters in the
object and allocates nothing. Running out of space prints "OUT OF MEMORY
(arena full)" on the screen and halts. Your own calls to `malloc` and
`v32::HeapBlock` are not rerouted, and including `<v32/memory.hpp>` without
`--alloc=arena` is a compile-time error.

### `--alloc=gc`: a garbage collector

`--alloc=gc` serves the same allocations (the same hook, `__v32_alloc` /
`__v32_release`, acquired at the start of `main`) from one `--heap-words=N`
heap that is garbage collected: a conservative mark-sweep collector, so
nothing has to be `delete`d. `delete` stays legal, runs the destructor and
frees at once; an object that is *collected* gets **no** destructor.

```cpp
#include <v32/memory.hpp>
Enemy *boss = new Enemy(3);          // no delete needed
...
v32::gc_collect();                   // once per frame, at a quiet point
```

The roots are the file-scope variables that can hold a pointer (pointers,
objects, `std::vector`s, arrays of them: v32c++ emits the table), and the
machine stack from the stack pointer to its top plus the registers. Any word
that points into a block, to its start or anywhere inside it (`&obj->member`,
a pointer into the middle of a `new int[10]`, a vector's buffer), keeps the
block and what it points to alive; the heap itself is scanned conservatively
too. A collection runs when an allocation finds no room (then the allocation
is retried), after `--gc-threshold=N` words were allocated since the last one
(default: a quarter of the heap), and when you call `v32::gc_collect()`;
`gc_used()`, `gc_free()` (words) and `gc_collections()` report the state. A full
collection is a pause: call `gc_collect()` once per frame at a quiet point.
Out of space after a collection prints "OUT OF MEMORY (gc heap full)" and
halts. Limits: an integer that looks like a heap address keeps a dead block
alive; a pointer hidden from the collector (xor-ed) keeps nothing alive (a pointer one past the end of an array, `p + n` or an `end()`, is fine: every block has one trailing word, so it still lies inside its own block); raw `malloc`/`v32::HeapBlock` memory is not
managed; there are no finalizers, and the heap is scanned conservatively
(precise per-class pointer maps are future work). The `arena_*` functions
do not exist with `--alloc=gc`, nor the `gc_*` ones with `--alloc=arena`.

## Keyboard and mouse: v32io

Three headers give programs a  real **keyboard** and **mouse**, split the
same  way as  the [v32io](https://github.com/wedge1020/v32io)  Vircon32 C
drivers they port, protocol-for-protocol:

| Header | Class | The C library's |
| ------ | ----- | --------------- |
| `v32/v32io.hpp` | `v32::IoDevice`: a gamepad's 11 controls read as data (the core) | `v32io.h` |
| `v32/keyboard.hpp` | `v32::Keyboard`: key events, typed characters (shift and caps lock, and accented and box drawing characters sent by the device), held keys | `keyboard.h` |
| `v32/mouse.hpp` | `v32::Mouse`: a bounded pointer, movement, three buttons | `mouse.h` |

A program includes the driver(s) it needs; each includes the core.

```cpp
#include <v32/keyboard.hpp>
#include <v32/mouse.hpp>

v32::Keyboard keyboard( v32::SecondGamepadPort );   // "Gamepad 2"
v32::Mouse    mouse( v32::ThirdGamepadPort );       // "Gamepad 3"
...
keyboard.probe();  mouse.probe();                   // once EVERY frame
for( int key = keyboard.read(); key > 0; key = keyboard.read() )
    if( keyboard.glyph( key ) > 0 ) text.add( keyboard.glyph( key ) );  // é, ß, ┌ ...
if( mouse.pressed( v32::MouseLeft ) ) click( mouse.x(), mouse.y() );
```

> **⚠  This REQUIRES  a v32io  device.**  The console  has no  keyboard
> or  mouse port:  a  v32io  device is  a  gamepad  whose controls  carry
> keyboard  or  mouse  data,  and  something  outside  the  console  must
> produce  it  —  **either**  the **v32io  hardware  adaptor**  (a  USB
> keyboard  or  mouse in  the  v32io-pico  board,  which works  with  any
> Vircon32  emulator),  **or**  a **modified  Vircon32  DesktopEmulator**
> with   the   `v32kbd`/`v32mouse`   gamepad   devices:   the   fork   at
> <https://github.com/wedge1020/ComputerSoftware>  (**`v32io`  branch**),
> or the stock emulator with  the v32io repository's patches applied. The
> stock emulator's own *Keyboard* device is **not** a v32io keyboard.

Full  documentation  — setup,  the  API,  the protocols,  porting  from
the  C  library, typing  accented  and  box drawing  characters,  testing
without  a device  — is  in **[`docs/V32IO.md`](docs/V32IO.md)**.  Five
ready-to-build examples  (a typewriter, a  paint program, a  port monitor
for  checking  a  setup,  a  notepad  using  both  devices,  and  a  tour
of  every  glyph  in   the  BIOS  font  and  how  to   type  it)  are  in
[`demos/cxx/v32io`](demos/cxx/v32io/README.md).

## Known limitations

Checked against  the current  transpiler and  the real  Vircon32 compiler
(November 2026), not carried over from older notes.

**Not supported** (a clear error unless noted):

- `static` class members.

- `constexpr`,  lambdas, `mutable`,  nested  classes  and
  class-scope enums.

- A two-dimensional array parameter (`void f(int g[4][4])`).
- Templates, exceptions, RTTI and multiple inheritance: never (see below)

**Supported with caveats:**

- **Array `new`** (`new T[n]`) default-constructs every element, in order;
  for a class with a destructor one header word in front of the elements
  keeps `n`, so `delete[]` destroys them last first and frees the block. `n`
  may be zero or computed at run time, and `delete[]` of null does nothing.
  `delete[]` through a base pointer to a derived array is undefined (as in C++). `delete[]` has to know the element class to release an array of a class with a destructor from its header: v32c++ works the type out through typedefs, members, indexing, pointer arithmetic, calls (function pointers too), casts, `auto` and `std::vector<T *>`/`std::array<T *, N>`; if it still cannot (a `void *`) and the program has a class with a destructor, that is an error asking for a cast (`delete[] (T *)p`). **`new T[n]{a, b}`** builds the first elements from the list (`{x, y}` calls a constructor, `T(x)` or a plain value a converting or copy one) and default-constructs the rest (zero for a scalar or a class without a constructor); the list may be shorter than `n` but not (in C++ it throws `std::bad_array_new_length`) longer: with a smaller run-time `n` v32c++ allocates the list's length. It works in any expression (it is lowered to a comma sequence on a function-level temporary) and with the count header and `--alloc=arena`/`gc`. A class with constructors but no default one needs `n` to be a constant not above the list's length.
- **`goto`** out of a scope destroys the class objects (and arrays of them)
  that are live there and not at the label, last built first, as `break` and
  `return` do. A `goto` that would jump past the declaration of an object
  with a destructor (forward in one block, or into a block) is an error, as
  in C++. A `goto` to a missing label is caught by the Vircon32 compiler,
  not by v32c++.
- **`>>` of a signed value costs a few instructions.** Vircon32's shift is
  logical, so v32c++ rewrites `x >> n` and `x >>= n` into an arithmetic shift
  (`-8 >> 1` is `-4`, as in C). A variable or member is written out in place
  (about five instructions instead of one), anything else goes through a
  small helper. `unsigned` values, constants, and shifts masked by a small
  positive constant (`(x >> 4) & 15`) stay a single instruction;
  `--no-signed-shift` turns the rewrite off. `<`, `>`, `/` and `%` of an
  `unsigned` value are done as unsigned operations too (`--no-unsigned-ops`
  turns that off); only a conversion to float above `INT_MAX` still reads it as negative.
  See `docs/VIRCON32_QUIRKS.md` #14.
- **`const` is not enforced** — violations are left to the C compiler. C input
  drops `const` from the output altogether (the Vircon32 compiler enforces it
  where C does not).
- **Brace lists are written out in full.** The Vircon32 compiler writes only the values
  listed, so a local `P z{};`, `int a[4] = {0};` or `P p{1};` would keep stack garbage
  and an omitted pointer would not be `NULL`: v32c++ completes every list of an array
  or of a struct without a constructor with the zero of each missing slot, and
  `new P()`, `new P[n]{}` and `: m{}` zero what they make (a pointer member, nested
  ones and arrays of them included, reads `NULL`, which is -1 here, not 0). A union cannot be
  brace-initialized by the compiler: v32c++ declares it and assigns the first member
  (`U u = {1};` is `U u; u.a = 1;`, a file-scope one at the start of `main`; `{}` zeroes every
  word), also inside structs and arrays. List-initialization works wherever a constructor
  call does: `P m{1, 2};` and `int w{5};` as in-class defaults, `: w{5}` / `: p{}` in a
  mem-initializer list, `new P{1, 2}`, `new Counter{3}`. `new int`, `new int(5)`,
  `new float{2.5f}` and `new Node*` allocate one word (through `--alloc` too) and `delete` frees it.
  `new T[n]()` and `new int[n]()` (value-initialized array new) are the same as `new T[n]{}`. See `docs/VIRCON32_QUIRKS.md` #31-#33.
- **Definitions and type spellings.** A qualified definition needs a declaration in that namespace
  (`int a::zz() {}` with no `zz` in `a`, or `int a::f(float)` when `a` only has `f(int)`, is an error, as in C++).
  Every spelling of a parameter type (`D` inside the namespace or by `using`, `a::D`, a typedef) is one type for
  matching and for the C name. Members of a namespace-qualified
  class, union or enum type are zeroed like any other (`new H`, `H h{}`, `new H[n]{}`).
- **`new` of a union or an enum, aggregates in `new T[n]{...}`.** `new U`, `new U()`, `new U{}` (the whole union zeroed, word by word),
  `new U{5}` (the first member), `new U[n]`, `new U[n]{ {1}, {2} }`, `new n::U`, `new E`, `new E(v)`, `new E[n]`, `new E[n]{a, b}`,
  `new E[n]()` (plain or `enum class`, in a namespace or not) and `delete` / `delete[]` of them go through the allocation hook. In `new T[n]{ T(), T(1, 2) }`
  / `{ {1, 2}, {} }` of a class with no constructor the listed elements are built from the list (zero, pointers NULL, for what it leaves out).
- **A leading `::`** (`::x`, `::f()`, `::a::D`, `::a::f()`, `: public ::B`) reaches the file-scope name where a namespace or a local declares
  one of its own. Not supported: `::new`, `::delete`, `::operator` (an error says so), and `a::D()` / `::a::D()` as an expression.
- **Out-of-line operators of a namespaced class** may take a bare type of that namespace first (`a::C a::C::operator-(D d)`, `operator()`,
  `operator[]`, `operator==`, `operator a::D()`).
- **A C program cannot define what the SDK headers define.** `misc.h` (added for `new` and `--alloc`) and
  `video.h` (added for `--alloc`) hold function bodies: a C function of the same name (`malloc`, `clear_screen`) is an error
  naming the header; rename it or use `--rename`. The allocator runtime does not include `time.h`, so
  `get_cycle_counter` is free. See `docs/VIRCON32_QUIRKS.md` #38.
- **Control codes in string literals are rewritten**:  the Vircon32 toolchain
  can't assemble a string holding the BIOS font's codes 14–31 (`┌ ─ ┐ │ ░ ▒ ▓ █`
  ...)  or other control codes,  `"\025"`.  v32c++ moves such a literal into a
  hidden global `char` array (`__v32_strN`); `sizeof` and inline asm are left
  alone.  See `docs/VIRCON32_QUIRKS.md` #28.
- **Classes in  a namespace  keep their bare  C name**  (`v32::String` is
  `String` in  the generated C),  so a program  can't have its  own class
  with the same name as one in a header it includes (`String`, `Channel`,
  `Keyboard`, `Mouse`, ...). Free functions and variables are namespaced properly:
  a variable declared in a namespace is `a__x` in the generated C (`a::inner::x`
  is `a__inner__x`; a file-scope one keeps its name), so `a::x`, `b::x` and a
  file-scope `x` are three variables, as in C++. An `extern` declaration in a
  namespace names something defined elsewhere (the SDK, a C file) and keeps its
  own name. `extern` is read per declaration (`extern int a; int b;` on one line
  is one of each, through a macro too). Vircon32 C has no `extern` and no linker,
  so an `extern` variable is printed as a plain declaration, once: it is the
  variable itself when the program defines it nowhere, and it disappears into the
  definition when it does (`namespace hw { extern int v; } int v = 3;` is one `v`).
  A variable the SDK headers define (`malloc_first_block` in `misc.h`),
  redeclared `extern` to name it, is left out of the output: the header's definition is the
  variable. Enumerators of a namespace's enum are namespaced the same
  way (`a::RED` is `a__RED`; `a::E::RED`, `using namespace`, `using a::RED`,
  `case` labels, array sizes and default arguments all resolve; file-scope enums
  are unchanged). The enum's *type* name is qualified too (`a::E` is `a__E`,
  also in the mangled name of a function that takes it), so two enums of one name in
  different namespaces are two types. A scoped enum's enumerators are `Color__Red`
  (`ns__Color__Red`); a bare `Red` is an error that says what to write (`'Red' is a scoped enumerator: write
  'Color::Red'`) unless the name means something else (a variable, a function...), and the conversion to `int`
  is allowed without a cast (more permissive than C++).
- **Overloaded name as a function pointer's value**: `int (*p)(float) = over;`
  picks the overload with the pointer's signature; with none, or more than one,
  that fits it is an error, never a guess. Member functions as pointer values
  are not supported.
- **`new T*[n]`** (an array of pointers) is plain pointer-sized storage: no
  constructors, no count header, `delete[]` releases it (the pointed-to objects
  stay the caller's); `new T*[n]{p, q}` sets the listed pointers and nulls the rest.
- **Overloads and pass-through  values**: v32c++ can't see the  type of a
  C  API  call's result.  An  overloaded  call  still resolves  when  its
  other arguments  settle it  (`v32::minimum(rand(), 10)`); when  none do
  (`v32::absolute(rand())`) it is an error — store the value in a typed
  local first.
- **`native` types** can be pointed to  but not created; the storage must
  come from the C side.
- **Unions** larger  than one word can't  be passed or returned  by value
  (v32c++ doesn't warn; the Vircon32 compiler reports it). Structs, classes and unions are
  written in dependency order (a type after the ones it holds by value, so
  `union V { int a; B b; };` is fine wherever `B` was declared); two that hold each
  other by value are an error, and every union is forward-declared for pointers.
- **File-scope objects** (and arrays of them) are constructed at the top of
  `main` and destroyed when it ends (after its locals, in reverse order),
  also from `exit()`/`abort()` of v32libc, whose `v32_exit` runs them first
  (once). Objects declared inside a namespace (nested or reopened ones too) are built and destroyed with them, in source order; a program that does not define `main` constructs none. `static` class data members are not supported (an error).
- An  `.h`  include  that  can't  be found  anywhere  is  passed  through
  each  time  it  appears  (the  C compiler  then  reports  it  missing).
  `docs/DESIGN_NOTES.md`  has  the  more  detailed  development  history,
  including the gaps that have been closed and how.
- **`NULL` in the SDK is `-1`.** v32c++ prints a `0` used as a null pointer
  as `NULL`, which is right for every pointer, SDK ones included
  (`malloc_first_block` is `NULL` before the first `malloc`). Only an
  integer you compare yourself (`(int)p == 0`) sees the difference.
- **The Vircon32 compiler drops functions nothing calls.** Data placed in
  an `asm { }` block of a function that is never called vanishes with it;
  call the function (or take its address) from somewhere that stays.

- **C input, linking several files** (`docs/C_INPUT.md`): the files are
  merged into one program, so a library is source, re-read at every link.
  C++ files link too, with only a light test so far; the undefined-reference
  check is for C. The library step starts a second copy of the program
  with `popen` (`_popen` on Windows, not yet tried there).
- **Code for another CPU does not translate.** `asm` written for RISC-V (xv6's
  `wfi`, CSR accessors) is passed on as it is; such a function must be
  written again for Vircon32.

## Deliberately out of scope

To keep  the project finishable,  these are permanent  design boundaries,
not "not yet":

- Templates (the containers above are a transpiler-level substitute)
- Exceptions and RTTI
- Multiple inheritance
- The full STL

## Requirements

Minimally:

- A C compiler (gcc or clang) and `make`.

Optionally:

- **flex**
- **bison**, version  3.x. (On macOS  the system `/usr/bin/bison`  is GNU
  bison 2.3, frozen  for licensing reasons: `brew install  bison` and put
  it first on your `PATH`.)

Built and  tested on Linux  and macOS; the release  workflow (see [Releases
and automated builds](#releases-and-automated-builds)) also builds and smoke-
tests it on Windows (x86_64 and arm64), Raspberry Pi, and, as optional extras,
OpenBSD and Haiku. For  Windows (MinGW /  MSYS2), the
few  POSIX  calls the  source  needs  (`realpath`, `getline`,  `strndup`,
`tmpfile`, the `PATH`  separator) go through `src/compat.c`,  which has a
Windows branch for each.

To  build cartridges  you  also need  the  Vircon32 DevTools  (`compile`,
`assemble`, `packrom`, plus `png2vircon`/`wav2vircon` for assets) on your
`PATH`.

## Building

```sh
git clone https://github.com/wedge1020/v32cxx v32c++
cd v32c++
make            # builds bin/v32c++
make test       # transpiles every sample in tests/ into out/
```

`make  test`   shows  that  every   transpile  succeeded  (or,   for  the
deliberately invalid samples, failed cleanly).

### With CMake

For those who prefer it, `CMakeLists.txt` builds the same transpiler from
the  same  sources (the  Makefile  stays  the  primary build,  and  `make
realcheck` is still the real-toolchain check):

```sh
cmake -S . -B build              # Release by default
cmake --build build              # build/v32c++
ctest --test-dir build           # transpile every sample (read from the Makefile's test list)
sudo cmake --install build       # see "Installing"
```

flex  and bison  are  used  when found  (`-DV32CXX_REGENERATE_PARSER=OFF`
skips    them);   otherwise    the   already-generated    `src/parser.c`,
`inc/parser.h` and  `src/lexer.c` are  compiled as they  are, so  a plain
build needs only a C compiler and CMake 3.10+. On **Windows**, build with
MinGW-w64  (for example  from  an MSYS2  MINGW64 shell,  `cmake  -S .  -B
build  -G "MinGW  Makefiles"`);  MSVC isn't  supported  (the sources  use
`getopt_long` and `<unistd.h>`). `cpack --config build/CPackConfig.cmake`
makes a `.tar.gz` (plus `.deb` / `.rpm` where `dpkg-deb` / `rpmbuild` are
available), or a `.zip` on Windows.

## Checking with the real toolchain

`make realcheck`  goes further:  it compiles,  assembles and  packs every
generated program with the **official**  Vircon32 tools, then boots every
self-checking sample on  a headless console with the standard  BIOS and a
fresh memory  card. Build those tools  once (about a minute;  needs `git`
and a C++17 `g++`, no SDL):

```sh
tools/vircon32/build-tools.sh
make realcheck
```

A  self-checking  sample declares  `int  test_errors  = -1;`  and  stores
its  error  count  there  before  halting;  the  check  requires  0  (see
`tests/94sample.cpp`). A sample with  a `tests/NNsample.input` next to it
is played  with that  scripted gamepad input  (`tests/119sample.cpp`, the
v32io test, uses it for keyboard and mouse traffic).

`tools/vircon32/build-tools.sh [COMPUTERSOFTWARE]` builds the stock `compile`,
`assemble` and `packrom` into `tools/vircon32/bin/` (through v32sim's
`tools/oracle/build.sh`) and stamps `bin/BUILD-INFO`: the ComputerSoftware path it
used, that checkout's git `HEAD`, the v32sim path and the compiler's version.
It first smoke-tests the new compiler from a scratch directory and stops with its
message if it can't find its SDK headers (on macOS the whole check needs a v32sim with the macOS fixes: oracle
shim `SDL_GetBasePath`, no `sed -i`).
`make realcheck` runs `build-tools.sh` first when `tools/vircon32/bin/BUILD-INFO` is missing, and `make clean` removes `tools/vircon32/bin`.
`tools/vircon32/check.sh` (what `make realcheck` runs after `make test`) prints
the compiler it uses, **refuses a `bin/` without `BUILD-INFO`** -- one built by an
older `build-tools.sh` -- telling you to rebuild it, and warns when
the ComputerSoftware checkout has moved since the build. Today it compiles and
assembles 241 programs (4 are not packed: their `#texture`/`#sound` assets are not in
`tests/`) and runs 172 self-checking cartridges, among them the `--alloc=arena` and
`--alloc=gc` builds listed in `tests/real_variants.txt` (`name source flags`: add a
sample's variants there when it allocates). `tools/prune-release.sh [--apply]` is the
release cleaner: a dry run by default, it lists (and with `--apply` removes, with
`git rm` for tracked files) stale clones, private copies of v32sim's tools, build
scripts, bison reports and editor leftovers.

Running, profiling and screenshots of carts belong to **v32sim**, which is
the one place for them: `build-tools.sh` and `check.sh` only call its
`tools/oracle/build.sh` (the stock `compile`, `assemble`, `packrom`) and its
`tools/v32sim-selfcheck.sh`. Point `V32SIM_HOME` at a v32sim checkout (default:
`../v32sim` beside this project, then `../v32sim.git`). The ComputerSoftware sources are found the same way (`../ComputerSoftware`, then `../ComputerSoftware.git`, or a path given to `build-tools.sh`); with neither, one is cloned to `../ComputerSoftware` at the verified commit, never inside this project. `V32_ORACLE=1 make realcheck` runs the
self-checking samples on the stock console logic instead, as a second opinion.
The per-frame load, cycles per function, screenshots and RAM peeks that
`v32prof`, `v32shot` and `v32peek` gave are v32sim's `--frame-stats`,
`--profile-*`, `--dump` and batch commands (`docs/V32IO.md` has an example).

The toolchain's `string.h` is the project's own `c_api/string.h`:
`build-tools.sh` installs it over the pinned DevTools one, which predates
the `ftoa()` rounding-carry fix that `tests/130sample.cpp` checks.

v32sim's `tools/v32io-script.py`  writes such  scripts from  keyboard and
mouse actions  (`docs/V32IO.md`). `wav2vircon` is  built too when  a real
SDL2 is installed (`sdl2-config`), for profiling demos that pack sounds.

## Using it

```sh
v32c++ game.cpp                       # writes game.c and game.xml, silently
v32c++ -I path/to/v32c++ -o obj/game.c game.cpp
```

| Option | Meaning |
| ------ | ------- |
| `-o file.c` | output path (default: the input's name with `.c`; `name-transpiled.c` when that would overwrite the input, as for a `.c` file) |
| `-c` | don't require a `main` (a library/module fragment) |
| `-I dir`, `--include=dir` | add a directory to the `.hpp`/`.cpp` search path (repeatable) |
| `-D name[=value]`, `-U name` | define / undefine a macro |
| `-v`, `-vv`, `-vvv` | progress; plus explanatory comments in the generated C; plus AST, semantic and lowering dumps and the lowering-notes log |
| `-x`, `--no-xml` | don't write the cart XML |
| `--xml-preserve-paths` | keep `#texture`/`#sound` paths in the XML as written, and write the XML beside the source file |
| `--no-build-scripts` | don't write `Make.sh` / `Make.bat` (see below) |
| `--title-opt=TEXT` | title of the optimized cart, like a `#titleopt` hint (and overriding it) |
| `--no-diff-opt-title` | give the optimized cart the regular cart's title, without ` [optimized]` |
| `-b` | transpile a BIOS rather than a cartridge |
| `-g` | write a C-line/C++-line debug map, `<output>.c.debug` |
| `--compiler=CMD` | the C compiler the build scripts call (default `compile`, or `$V32CXX_COMPILER`); `v32cc` gets `-S --target-compat=v32cxx` and, with `-g`, `-g` |
| `--compiler-style=stock\|v32cc` | force how `--compiler` is called when its name does not say |
| `--line-markers` | write `#line N "file.cpp"` before each function and statement of the C, for v32cc (see [Debugging with v32sim](#debugging-with-v32sim)) |
| `--target=vircon32`/`v32` | Vircon32 C (the default) |
| `--target=standard`/`std` | portable standard C instead |
| `--reject-bit-fields` | make a bit-field an error instead of a warning |
| `--no-signed-shift` | print `>>` / `>>=` as written (Vircon32's shift is logical; by default a signed shift is rewritten to be arithmetic) |
| `--no-unsigned-ops` | print `unsigned` `<`, `>`, `/`, `%` as written (signed in Vircon32) |
| `--warn-error[=kind,...]` | make v32c++'s own warnings errors (no output file, exit 1); `--list-warnings` lists the kinds |
| `--yield-points[=N]` | insert `if (get_cycle_counter() > N) yield();` at function entry, loops and labels (`--yield-func=`, `--yield-counter=`; `__attribute__((no_yield))` opts out) |
| `--alloc=malloc\|arena\|gc`, `--heap-words=N`, `--gc-threshold=N` | where `new`/`new[]`/`std::vector` memory comes from: the SDK's malloc (default), an arena released in bulk, or a conservative mark-sweep garbage collector (`<v32/memory.hpp>`); the heap's size in words, and the words allocated between collections |
| `--rename old=new` | rename a function/variable program-wide (SDK name clashes) |
| `--assembler=CMD`, `--assembler-style=stock\|v32as` | assembler named in the generated Make.sh / Make.bat (default: `$V32CXX_ASSEMBLER`, else config.h `V32CXX_ASSEMBLER`, else `assemble`); a name starting with `v32as` is driven with `-B -c` / `-B -b` |
| `--mklib lib.v32a` | write a library index over the source files given (see `docs/C_INPUT.md`, *Libraries*) |
| `-L dir`, `-lname`, `lib.v32a` | link with a library: only the members the program needs are taken in |
| `--allow-undefined` | with several input files, make a used-but-never-defined function or `extern` variable a warning instead of a link error |
| `--silence-unused-args` | start every function that ignores a parameter (`this` of a method that touches no member, a callback's argument) with a bare `name;`, which the Vircon32 compiler counts as a use: no `argument "x" is not used` warning (it stops after 10). A little code per parameter |
| `--no-inline-containers` | keep `std::array`/`std::vector` accessors as calls |
| `--version` | print the version and exit |

It is  silent on success, like  the Vircon32 compiler and  v32lua; errors
and warnings go  to standard error with  the file and line,  and the exit
status is non-zero on any error. The manual page (`man v32c++`) describes
every option in full.

**`-vv`** sprinkles comments  into the generated `.c`  where the C++-to-C
translation  is least  obvious  — vtables,  the  explicit `this`,  what
`new`/`delete` become,  destructors called  at scope exit,  virtual calls
— so the  output can be read on its  own (try `tests/32sample.cpp`). It
never changes the  code itself. **`-vvv`** adds the internal  dumps and a
**lowering-notes  log**:  one  line  per place  v32c++  rewrote  code  to
work  around  a  Vircon32  quirk (a  ternary  hoisted  into  `if`/`else`,
an  implicit  `&function`,  a  const-discarding receiver  cast)  —  try
`tests/71sample.cpp`.

**`-g`**      writes     `<output>.c.debug`,      a     sparse      table
(`c_path,c_line,cpp_path,cpp_line[,function_name]`)  mapping generated  C
lines back to the source, modeled on Vircon32's own debug maps.

**`-b`** transpiles  a BIOS:  the XML's  `<rom>` gets  `type="bios"`, and
v32c++ checks  for exactly  one `#texture`,  at most  one `#sound`  and a
`void error_handler()`, reporting every violation at once.

## Debugging with v32sim

The stock Vircon32 compiler rejects `#line`, so for it `-g` only gives the C-line/C++-line map above (and its
`<output>.c.debug` joins with the compiler's own asm/C map). `v32cc` honours `#line`, so with
`--line-markers` (implied by `-g --compiler=v32cc`) the C++ file and line carry through to the assembly (`;@src game.cpp:42`), to
`v32as -g`, and to **v32sim**, which then steps, breaks (`break game.cpp:42`), lists source and shows
backtraces by the C++ lines, and shows locals, since `v32cc -g` writes `;@var` for every C local it sees:

One command does all of it: `v32c++ -g --compiler=v32cc --assembler=v32as game.cpp` writes a Make.sh that builds with
debug files. By hand:

```sh
v32c++ --line-markers game.cpp -o game.c
v32cc -g --target-compat=v32cxx -S game.c -o game.asm      # ;@src game.cpp:N, ;@var, ;@scope
v32as -g game.asm                                           # then assemble/pack as usual
v32sim --source -L src game.v32
```

A `#line` is written only where the C compiler's own line count would otherwise differ, so most statements
need none; the generated code is unchanged (without the option the output is byte-identical to before).
Code the transpiler writes with no C++ counterpart (allocators `v32_new_*`, a destructor call at scope
exit, the epilogue of a function) takes the line of the nearest statement; a function's closing `}` shows
the line after its last statement.

**Names a debugger shows.** Locals and parameters keep their C++ names. The transpiler also writes:

| Name | What it is |
| ---- | ---------- |
| `this` | the explicit first parameter of every method |
| `Class__method__sig`, `Class__Class__void`, `Class__dtor__void` | a method, constructor (`sig` is the parameter types, `void` for none) and destructor, as C functions |
| `__v32_ret_tmpN`, `__v32_tern_tmpN`, `__v32_ref_tmpN`, `__v32_sret_tmpN` | temporaries for a return value (kept alive across destructors), a hoisted `?:`, a reference argument |
| `__v32_ctor_arr_iN`, `__v32_mctor_iN` | loop counters that construct array elements / array members |
| `__v32_destroy_globals`, `__v32_globals_live`, `__v32_gret_tmp` | the function that destroys the file-scope objects (called when `main` ends and from `v32_exit`), its once-only flag, and a `return` value held while it runs |
| `__v32_byval_x` | a class-type parameter passed by pointer: the function copies it to a local `x` on entry |
| `v32_new_Class`, `v32_delete_Class`, `v32_new_arr_T` | `new`, `delete` and `new[]` helpers |

Names starting with `__v32_` are the transpiler's, not the program's. v32sim and v32as skip `__` names in
`locals`.

## Cart packaging

A cart-packing  XML is written next  to the generated C  (`game.xml`), so
`packrom` needs  no hand-written file. Five  hints in the source  fill it
in, modeled on v32lua's:

```cpp
#title   "My Game"
#titleopt "My Game (fast)"
#version 1.0
#texture Background "background.png"
#sound   jump_sfx   "jump.wav"
```

A name can be used  once: two hints with the same name  (two textures, two
sounds, or a texture and a sound) are an error naming both lines.
Each  `#texture`/`#sound`  name  becomes  a   `#define`  of  its  id  (0,
1,  2…  in  order,  textures and  sounds  counted  separately),  usable
anywhere an  integer is (`select_texture(  Background )`), and  fills the
XML's `<textures>`/`<sounds>`  in that  order with the  extension swapped
to  `.vtex`/`.vsnd`.  `#title`/`#version`   set  the  `<rom>`  attributes
(otherwise "Vircon32  Program" / "1.0"). `#titleopt`  (optional) sets the
title in the  optimized cart's XML, `gameOpt.xml`; without  it that title
is the  cart's with ` [optimized]`  appended (`--no-diff-opt-title` keeps
them equal). The XML's `<binary path>` is the `-o` path with `.vbin`.

`packrom` reads every path  in the XML relative to the  XML itself, so by
default  the  XML  holds  bare  file names  only:  with  `-o  obj/game.c`
and   `#texture   Ship   "textures/ship.png"`  you   get   `obj/game.xml`
containing `<binary  path="game.vbin" />` and  `<texture path="ship.vtex"
/>`,  i.e.  the `.vbin`/`.vtex`/`.vsnd`  files  are  expected beside  the
XML.  If  two  assets  end  up  with  the  same  name,  the  first  keeps
it  and  the   later  one  keeps  just  enough  of   its  directories  to
differ: `textures/ship.png`  followed by  `textures/extra/ship.png` gives
`ship.vtex`  and `extra/ship.vtex`  (so  the second  `.vtex` is  expected
in  `obj/extra/`).  A  twin  XML  for  the  optimized  build  is  written
alongside  (`obj/gameOpt.xml`,   whose  `<binary>`   is  `gameOpt.vbin`).
`--xml-preserve-paths`  instead   keeps  each  hint's  path   as  written
(`textures/ship.vtex`), writes the XML into the *source file's* directory
— or its parent, when the source  is in a `src/` directory whose parent
has (or, via `-o`,  gets) an `obj/`, the same place  the build scripts go
— and makes `<binary>` the `.vbin` path relative to it.

### Build scripts

Whenever  the  XML  is  written,  v32c++  also  writes  `Make.sh`  (bash)
and  `Make.bat`   (Windows),  in  the   style  of  the   Vircon32  sample
projects, from the  same information: transpile (v32c++  again, with your
`-I`/`-D`/`-U`/`-b`/`-g`  options  and `--no-build-scripts`),  `compile`,
`assemble`,  `png2vircon`  for  each `#texture`,  `wav2vircon`  for  each
`#sound`, and  `packrom`. They  sit next  to the source  file, or  in its
parent when the source is in a  `src/` directory and that parent has (or,
via `-o`, gets) an `obj/` — and  every path in them is relative to that
folder, so run  them from there. Each script checks  first that the tools
it uses are  in `PATH` (a missing `v32c++`, `compile`...  stops the build
with "X is needed to build"). If  `v32opt` is found when the script runs,
the  optimized build  —  `v32opt  -O3`, `gameOpt.vbin`,  `gameOpt.xml`,
`bin/gameOpt.v32` — is made too; otherwise it is skipped with a note. A
`Make.sh`/`Make.bat` that  v32c++ did not  write is never  overwritten (a
warning is  printed). `--no-build-scripts`  turns all  this off,  as does
`-x`.

## Output dialects

`--target=vircon32`  (the default)  emits  Vircon32 C,  with every  quirk
of  that  dialect  handled:  no  `struct`  keyword  on  type  references,
reversed  array  and  function-pointer  declarators,  `void  main(void)`,
`NULL`,   no  ternary   or  comma   operator,  one-word   parameters  and
returns,   `&`  on   function   names,  and   the   rest  catalogued   in
[`docs/VIRCON32_QUIRKS.md`](docs/VIRCON32_QUIRKS.md). `--target=standard`
emits portable C  instead (no XML or  debug map), for using  v32c++ as an
ordinary C++-to-C translator or checking  output with `gcc`; `main` keeps
its declared return type there.

## Demos

| Demo | Language | What it is |
| ---- | -------- | ---------- |
| [`demos/cxx/tempest_32k`](demos/cxx/tempest_32k/README.md) | C++ | a TEMPEST-style tube shooter, multi-file |
| [`demos/cxx/space_invaders`](demos/cxx/space_invaders/README.md) | C++ | "SPACE INVADERS++", with power-ups, multi-file |
| `demos/cxx/star_raiders` | C++ | a Star Raiders-style space game |
| [`demos/cxx/v32io`](demos/cxx/v32io/README.md) | C++ | keyboard and mouse examples (need a v32io device) |
| [`demos/c/rogue`](demos/c/rogue/README.md) | C | Unix Rogue 5.4.4, transpiled from its sources |
| [`demos/c/spyvsspy`](demos/c/spyvsspy/README.md) | C | a Spy vs Spy game in plain C, and a C coverage probe |

`make  -C demos`  builds  every  demo that  has  a  Makefile (each  needs
`v32c++` and the  DevTools on the `PATH`; the C++  games use `v32opt` too
when it is installed).

## Documentation

| Document | What it covers |
| -------- | -------------- |
| [`man/man1/v32c++.1`](man/man1/v32c++.1) | the manual page: every option, the language, the preprocessor, packaging |
| [`man/man3/`](man/man3) | section 3v32 pages for the headers: `v32c++-headers(3v32)` to start, then `video.hpp`, ..., `v32c++-string`, `v32libc`, `v32curses`, ... |
| [`docs/V32IO.md`](docs/V32IO.md) | keyboard and mouse support (`v32/keyboard.hpp`, `v32/mouse.hpp`) |
| [`docs/C_INPUT.md`](docs/C_INPUT.md) | transpiling C, and the `libc/` C library |
| [`docs/STRING.md`](docs/STRING.md) | the built-in `std::string` |
| [`docs/GENERICS.md`](docs/GENERICS.md) | `std::array` and `std::vector` |
| [`docs/NATIVE_PASSTHROUGH.md`](docs/NATIVE_PASSTHROUGH.md) | `native` types for C-header structs |
| [`docs/INLINE_ASSEMBLY_SUPPORT.md`](docs/INLINE_ASSEMBLY_SUPPORT.md) | `asm` statements |
| [`docs/VIRCON32_QUIRKS.md`](docs/VIRCON32_QUIRKS.md) | every way Vircon32 C differs from standard C, and how each is handled |
| [`docs/DESIGN_NOTES.md`](docs/DESIGN_NOTES.md) | internals and the full development history |
| [`docs/TRANSPILER_REPORT.md`](docs/TRANSPILER_REPORT.md) | the gap report from the first Space Invaders port, with current status |

The README  and the  reference documents  (all but  `DESIGN_NOTES.md` and
`TRANSPILER_REPORT.md`)  are also  available  in  Spanish (`.es.md`)  and
French (`.fr.md`), and the manual pages in `man/es/` and `man/fr/`.

## Project layout

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
docs/AGENTS.md  notes for AI coding agents working on the project
tools/
  embed-header.sh   regenerates src/stdstring.c from lib/string
  prune-release.sh  removes stale, duplicate and generated files (dry run;
                    --apply uses git rm for tracked ones)
  vircon32/         build-tools.sh (builds the real Vircon32 toolchain
                    into bin/ through v32sim, finding ../v32sim(.git) and
                    ../ComputerSoftware(.git), and writes bin/BUILD-INFO)
                    and check.sh (`make realcheck`; refuses a bin/ without
                    BUILD-INFO); every runner, profiler and debugger is
                    v32sim's
```

## Installing

A system-wide install puts everything where  a Vircon32 setup expects it.
The binary and  the man page go  where commands and manual  pages go; the
headers, the C library and the documents go under `v32tools/`, the folder
the  community  tools share  (v32opt's  files,  and v32lua's  headers  in
`v32tools/include/v32lua`), next to the official `DevTools/`:

| | Linux / macOS | Windows (CMake) |
| --- | --- | --- |
| transpiler (`v32c` is a copy of it) | `/usr/local/bin/v32c++`, `/usr/local/bin/v32c` | `C:\Program Files\Vircon32\v32tools\v32c++.exe`, `v32c.exe` |
| man pages (v32c++'s, and the Vircon32 SDK's from `man/vircon32/`, side by side) | `/usr/local/share/man/man1/*.1.gz`, `/usr/local/share/man/man3/*.3v32.gz` (gzipped; aliases are symbolic links) | `...\v32tools\docs\v32c++\man1`, `...\man3` |
| C++ headers (`v32/`) | `/usr/local/Vircon32/v32tools/include/v32c++/v32` | `...\v32tools\include\v32c++\v32` |
| C library (`libc/`) | `/usr/local/Vircon32/v32tools/include/v32c++/libc` | `...\v32tools\include\v32c++\libc` |
| documentation | `/usr/local/Vircon32/v32tools/docs/v32c++` | `...\v32tools\docs\v32c++` |

Either build does it:

```sh
sudo make sysinstall                          # Linux / macOS
sudo make sysuninstall

sudo cmake --install build                    # any system (prefix: /usr/local,
sudo cmake --build build --target uninstall   #   or C:/Program Files/Vircon32)
```

The  header  directory   is  compiled  into  the   transpiler,  so  after
installing,  `#include  <v32/video.hpp>` (or  `<v32/keyboard.hpp>`,  ...)
works from  any directory without `-I`,  and C input finds  the installed
`libc` the same  way. With CMake, pick a different  location at configure
time (`cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/vircon32`) and the
transpiler  will   look  there;   `-DV32CXX_INSTALL_INCLUDEDIR=...`  (and
`BINDIR`, `MANDIR`, `DOCDIR`)  move one part alone.  Uninstalling removes
only  v32c++'s own  files, and  then `v32tools/`  (and on  Linux /  macOS
`Vircon32/`)  only  if nothing  else  is  left  in  it. On  Windows,  add
`C:\Program Files\Vircon32\v32tools` to your `PATH`, as for the DevTools.

**Documentation   language.**  By   default  both   builds  install   the
documents in every language (`README.md`, `README.es.md`, `README.fr.md`,
`docs/V32IO.es.md`,  ...)   and  the   English  manual  pages   plus  the
translated ones  in the  standard per-language  trees (`man/es/`  goes to
`/usr/local/share/man/es/man1` and `man3`, `man/fr/` to `.../fr/man1` and
`man3`,  so `man`  picks  the language  from  `LANG`). `DEST_LANG=es`  or
`DEST_LANG=fr`  (`sudo  make sysinstall  DEST_LANG=es`,  or  `cmake -S  .
-B  build  -DDEST_LANG=es`)  installs  only that  language  instead:  the
translated README and  documents under those plain names  (with the links
between them adjusted), next to the  documents that exist only in English
(`docs/DESIGN_NOTES.md`,  ...), and  the  manual pages  in that  language
directly in `man1`  and `man3`. `DEST_LANG=en` installs  only the English
documents and  manual pages.  The Vircon32 SDK's  pages (`man/vircon32/`)
exist only in English and are always installed.

`make  install`  / `make  uninstall`  instead  copy  just the  binary  to
`~/bin`.  Installation  defaults  for  the  Makefile  build  (the  header
directory,  the  environment  variable  names,  a  few  limits)  live  in
[`inc/config.h`](inc/config.h); change one there and rebuild, or override
it when building:

```sh
make CFLAGS="-Wall -Wextra -g -Iinc -MMD -MP -DV32CXX_INCLUDE_PATH='\"/opt/v32tools/include/v32c++\"'"
```

## Versioning

The   version   is   `YYYYMMDD-status`  (`-dev`   or   `-release`),   the
same  scheme   as  v32lua,   and  lives  in   one  place:   `VERSION`  in
[`inc/v32cxx.h`](inc/v32cxx.h),  which  `v32c++   --version`  prints.  To
change it,  edit it there and  run `make version`, which  stamps it (with
today's date)  into the `.TH` header  of every manual page  in `man/man1`
and `man/man3` that names v32c++, and in those of `man/es/` and `man/fr/`
(with the  date in each one's  language). Alias pages and  pages of other
projects (the Vircon32  SDK's, in `man/vircon32/`) are left  alone, and a
page already up to date is not rewritten.

## Releases and automated builds

[`.github/workflows/release-builds.yml`](.github/workflows/release-builds.yml)
(modeled on v32lua's) runs only when started by hand, from the Actions
tab, as a `test` or a `release` run (nothing runs on push). The jobs:

| Job | What it does |
| --- | --- |
| Read Version | reads `VERSION` from `inc/v32cxx.h`; checks that `make version` has stamped it into the manual pages (a warning on a `test` run, an error on a `release` run), that `man/vircon32/` and `man/` share no page name, and that every alias page leads somewhere |
| Test Against the Vircon32 Toolchain | `make realcheck` (above), with the toolchain cached between runs |
| CMake Build, Test and Install | configure, build, `ctest`, then `cmake --install` into a scratch directory, checking the binary, headers and every manual page, gzipped, aliases as links (Linux and macOS) |
| Build | Windows (x86_64, arm64), macOS (x86_64, arm64), Linux (x86_64, arm64, Raspberry Pi), and, optionally, OpenBSD and Haiku, each smoke-tested (`scripts/smoke_test.sh`) and zipped (`scripts/stage_package.sh`) |
| Release | for a `release` run: a GitHub release `v<VERSION>` for the commit that was built, with every zip, a `SHA256SUMS.txt`, and the text of [`.github/release-notes.md`](.github/release-notes.md). It is left as a draft to review and publish by hand, unless the run was started with `publish_release` ticked, which publishes it at once; a `-dev` version is marked a pre-release |

A     release      zip     unpacks     straight      into     `v32tools/`
(`/usr/local/Vircon32/v32tools` or  `<Program Files>/Vircon32/v32tools`):
the  binary, `include/v32c++/`  (`v32/` and  `libc/`) and  `docs/v32c++/`
with the documents and the manual  pages in `man1/` and `man3/`, v32c++'s
and the Vircon32 SDK's together  (`man -M docs/v32c++ 3v32 string.h`). To
release:  set  `VERSION`,  run  `make version`,  update  the  text  in
`.github/release-notes.md`  (`@VERSION@`  in it  becomes  the  version),
commit, and start the workflow with `release`; then review the draft on
the Releases page and press **Publish release** (or tick `publish_release`
when starting the workflow, to skip that step).

## AI Collaboration

This tool was developed in heavy  collaboration with AI. I am hesitant to
call  it "vibe-coded",  as  I  have some  knowledge  and experience  with
the  basics. But,  I  certainly would  not have  achieved  this level  of
functionality in  so short a  time had AI  not been actively  involved. I
feel like I  took more of a "project manager"  role, dictating direction,
troubleshooting problems, debugging where needed.

While I may  not know every inch of  code as well as if  I had personally
wrote it  entirely myself, I  do appreciate  the leverage it  provides in
establishing  new tools  I  can  use in  the  classroom,  share with  the
community, and realize fruition of ideas more quickly.

I know  this may be or  cross a line of  dubious fuzziness/acceptableness
for some. But, as a niche tool in  a niche environment, may it be just as
easy to ignore.

## Feedback

This is  built in  the open  as a  teaching project  and shared  with the
Vircon32 community  — if something  breaks, transpiles wrong,  or you'd
like  a particular  C++  feature  prioritized, please  open  an issue  at
<https://github.com/wedge1020/v32cxx>.
