# C input

`v32c++ program.c` transpiles **C**, not C++. The file's extension decides:
a name ending in `.c` turns on C mode for the whole run (`g_c_mode`,
`inc/driver.h`). Everything in this document applies to C input only; a
`.cpp` file is read exactly as before, and its output is unchanged.

v32c++ always read most of C, as a subset of C++. C mode is the rest:
the places where C and C++ disagree, where old C says less than C++
requires, and where Vircon32 C differs from both. Each item below was
found by building a real program, Rogue 5.4.4 (`demos/c/rogue`), whose
28 source files are transpiled byte-for-byte as they were written for
Unix. `tests/116sample.c` and `tests/117sample.c` are the self-checking
samples; both are also valid for a native C compiler.

## What a C program can use

| C construct | What v32c++ does with it | Where |
|---|---|---|
| `#include "rogue.h"`, `#include "src/move.c"` | The program's own `.h` and `.c` files are expanded and transpiled. The Vircon32 SDK's headers (`video.h`, `string.h`, ...) still pass through to the compiler. | `prescan.c` |
| `#include <stdarg.h>` | No file is read; it provides `va_list`. | `prescan.c` |
| `#define when break;case`, function-like macros | Every use is expanded, and only constants are passed through to the generated C (`#define MAXSTR 1024`, `#define STATLINE (NUMLINES - 1)`). An octal constant is never passed through: Vircon32 C would read it as decimal. | `macro.c` |
| `struct rdes { ... } rdes[9];` | Tags have their own namespace. A tag that is also an ordinary name is renamed `rdes_tag` in the output. | `parser.y`, `cmode.c` |
| a struct, union or enum defined inside a declaration, with or without a tag | The definition moves to file scope, ahead of the declaration; unnamed ones are called `__v32_anonN`. | `parser.y` |
| `void fatal();` then `void fatal(char *s) { }`; `void leave(int);`; a prototype inside a function | One function: the first declaration is rewritten to say what the definition says. No name mangling. A function declared and never defined is dropped. | `cmode.c`, `sema.c` |
| `extern char prbuf[];` and, elsewhere, `char prbuf[2*MAXSTR];` | One variable; the declaration with the size wins. | `ast.c` |
| `main(int argc, char **argv, char **envp)`, no return type | `void main(void)` whose first statements declare `argc` (1), `argv` (`{"program", NULL}`) and `envp` (`{NULL}`). | `parser.y`, `cmode.c` |
| `int printf(char *fmt, ...)`, `va_start`, `va_arg`, `va_end` | See *Variadic functions* below. | `parser.y`, `cmode.c` |
| `char **argv`, `char **a, *b;` | Pointers to pointers (up to three levels). | `parser.y` |
| `void (*func)()` as a parameter, `(void (*)())fn` | Function pointer parameters and casts. | `parser.y` |
| `(*d_func)(arg)` through a pointer declared `void (*d_func)();` | The pointer is cast to the type the call implies: `((void(int)*)d_func)(arg)`. | `lower.c` phase 12 |
| a function's name as a value: an argument, a table entry, `f == g` | `&name` | `lower.c` phase 12 |
| `NULL`, `p = 0`, `if (p)`, `!p`, `p && p->x` | See *The null pointer* below. | `lower.c` phase 12, `codegen.c` |
| `char buf[sizeof table / sizeof table[0]]`, `sizeof "text"` | `sizeof` is evaluated in array sizes (sizes are in words). | `parser.y` |
| `"\033[2J"`, `"\b \b"` | A string with a control character becomes a file-scope char array. | `lower.c` phase 12 |
| `struct stats s = { 16, "1x4", 12 };` with `char s_dmg[13]` | The string becomes the member's characters, zero-padded. | `lower.c` phase 12 |
| `class`, `new`, `this`, `delete`, `virtual`, ... as names | Ordinary identifiers, written with a trailing underscore (`this_`). | `lexer.l` |
| `auto`, `register`, `(void) f();` | Accepted and dropped. | `lexer.l`, `lower.c` |

## Variadic functions

Vircon32 C has no `...`. The extra arguments travel as one more
argument: a pointer to an array of words.

```c
int sum(int n, ...) {                    int sum(int n, int *__v32_va) {
    va_list ap;                              va_list ap;      /* int * */
    va_start(ap, n);                         ap = __v32_va;
    x = va_arg(ap, int);                     x = *((int *)((ap += 1) - 1));
    va_end(ap);                              ap = ((void *)0);
```

and each call fills an array of its own first:

```c
msg("%s hits %d", name, dmg);            int [2] __v32_va_tmp0;   /* top of the function */
                                         ...
                                         __v32_va_tmp0[0] = (int)name;
                                         __v32_va_tmp0[1] = (int)dmg;
                                         msg("%s hits %d", __v32_va_tmp0);
```

A call with no extra arguments passes a null pointer. A `va_list` can be
handed on to another function (`vsprintf(buf, fmt, ap)`).

Where the assignments go matters:

- in a `while` or `for` condition, they must run every time round, so
  the loop becomes `while (true) { <assignments> if (!(cond)) break; ... }`;
- on the right of `&&` or `||`, they must run only if that side is
  evaluated, so the expression becomes a flag set by `if` statements
  ahead of the statement;
- **not supported**, and reported as an error: a variadic call with extra
  arguments in a `do ... while` condition, in the step of a `for`, or in
  a branch of `?:`.

Every extra argument is stored as one word (`(int)` cast), so a struct
passed by value through `...` is not supported.

## The null pointer

In C the null pointer is 0, and programs depend on it: zeroed memory,
uninitialized statics, `if (p)`. Vircon32 C's `NULL` is -1, it accepts no
`int` where a pointer is expected, and no pointer where a condition is.
For C input:

- `NULL`, and a literal `0` assigned to, passed as, or initializing a
  pointer, are written `((void *)0)`;
- `p == NULL` and `p != NULL` become `((int)p) == 0`;
- a pointer used as a condition (`if`, `while`, `for`, `!`, `&&`, `||`)
  becomes `((int)p)`.

The SDK's own functions still speak -1 (`malloc` returns it on failure).
`libc/` wraps the ones a C program calls; do not hand a C null pointer to
an SDK function directly.

## The C library

`libc/` holds the slice of the standard library Rogue needed, plus
curses, written in C and transpiled with the program:

    v32c++ -I <v32c++>/libc ... program.c

with `#include <v32libc.c>` (and `<v32curses.c>`) once in the program's
one file. See `libc/v32libc.h`.

## Limits

- One translation unit, as always: `static` at file scope means nothing,
  and two files' `static` functions of the same name collide.
- `unsigned` is `int` (warned once); `char`, `short` and `long` are one
  32-bit word, so `sizeof(char) == sizeof(int) == 1`.
- A union cannot be brace-initialized (Vircon32 C).
- K&R parameter declarations (`f(a, b) int a; char *b; { }`) are not
  parsed; prototype-style definitions are.
- Bit-fields: as for C++ (a warning, or `--reject-bit-fields`).
- Variadic functions are C input only; `...` in a `.cpp` file is an error.
