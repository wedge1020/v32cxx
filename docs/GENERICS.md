# Transpiler-level generics: `std::array<T, N>`

v32c++ does not do templates. What it offers instead is a small set of
*built-in* generic containers with STL-compatible spelling, starting with
`std::array<T, N>`. `std::vector<T>` is planned next and will reuse the
same machinery.

The technique is the one C++ itself used before templates existed
(cfront's `<generic.h>`): for every distinct `array<T, N>` a program
names, the transpiler writes out one ordinary class with `T` and `N`
filled in, then treats that class exactly like one typed by hand.

```cpp
#include <array>

std::array<Enemy, 8> enemies;     // becomes:  array_Enemy_8 enemies;
enemies[0].hp = 3;
for (int i = 0; i < enemies.size(); i++) ...
```

## What is supported

| Member | Notes |
| --- | --- |
| `operator[]`, `at(i)` | return `T &`; `at` does not bounds-check (no exceptions) |
| `front()`, `back()` | return `T &` |
| `size()`, `max_size()`, `empty()` | `int` / `bool` |
| `data()`, `begin()`, `end()` | plain `T *` |
| `fill(value)` | takes `const T &` |
| `= {1, 2, 3}` | aggregate initialization, local or global |
| `b = a` | whole-array copy (plain struct assignment) |

Element types: `int`, `float`, `bool`, `char`, any struct/class/union/enum
name (qualified or not), a pointer to any of those, or another
`std::array`. Elements of a class type are constructed when the array is
and destroyed when it is (`tests/105sample.cpp` covers the member-array
machinery this relies on). The length is an integer literal, a `#define`d constant, or
a named constant such as an enum constant.

`using namespace std;` is accepted and allows the bare `array<T, N>`
spelling. A program's own variable called `array` is unaffected.

## How it works

1. **`src/prescan.c`** consumes `#include <array>` (no such file exists,
   and the line must not reach the generated C) and switches the feature
   on.
2. **`src/lexer.l`** returns one token, `STD_ARRAY`, for `std::array`.
3. **`src/parser.y`** has one extra `type_spec` alternative,
   `STD_ARRAY '<' type ',' length '>'`. Like the four cast keywords, it
   is a fixed form, not a general `name<args>` rule, so `<` and `>` never
   compete with the comparison operators. Its action asks `generic.c`
   for the class name and uses that as the type.
4. **`src/generic.c`** holds the class as C++ source text with `@T@`,
   `@N@` and `@NAME@` placeholders. After the main parse it writes one
   copy per instantiation, runs the same parser over that text, and
   inserts each class just ahead of the first top-level declaration that
   uses it.

From there on nothing knows the class was generated. To see exactly what
a program got, read `ARRAY_TEMPLATE` in `src/generic.c`, or look at the
`array_*` structs and functions in the generated C.

Names: `array_<T>_<N>`, with `::` and `*` spelled `_` and `_ptr`
(`std::array<Enemy *, 2>` is `array_Enemy_ptr_2`). A pointer element type
also gets a typedef, `array_Enemy_ptr_2_elem`, because this grammar has
no `T *&` declarator.

## Vircon32-specific details

- `= {1, 2, 3}` is emitted as `{{1, 2, 3}}`. Vircon32 C does not allow
  the inner braces to be left out ("too many values to assign to
  structure").
- `size()` is written with `sizeof` rather than `return N;`. The
  Vircon32 C compiler warns about every unused argument, and a `size()`
  that ignores `this` produces three such warnings per instantiation,
  which soon reaches the compiler's warning limit.
- `f()[i].member` is emitted as `(*(f() + i)).member`. The Vircon32 C
  compiler stops with "cannot emit memory placement when an expression
  has none" on the direct form. This applies to any program, not only to
  `data()[i]`.

## Known gaps

- **Every access is a function call.** `a[i]` calls
  `array_T_N__op_index__int`. Lowering `a[i]` straight to
  `a.m_data[i]`, and `size()` to a constant, is planned and matters for
  CPU-bound loops.
- **Unnamed class objects.** `f(7)` into a `const int &` parameter now
  gets a temporary (`tests/106sample.cpp`), but `Enemy(1, 2, 3)` written
  as an expression does not parse at all, so `fill(Enemy(1, 2, 3))` is
  out: build the object in a named variable first.
- **Multi-dimensional member arrays of class objects** (`Counter
  grid[2][2];` as a member) are still not constructed. A
  `std::array` of `std::array` is fine: each level is its own class.
- **Named lengths are compared by name.** `std::array<int, MAX>` and
  `std::array<int, 8>` are different types even when `MAX` is 8. A
  `#define` is not affected (it is expanded before parsing).
- No range-based `for`, no `auto`, no `std::array<T, N>::iterator`.
