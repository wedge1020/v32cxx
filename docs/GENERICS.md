# Transpiler-level generics: `std::array<T, N>` and `std::vector<T>`

v32c++ does not do templates. What it offers instead is a small set of
*built-in* generic containers with STL-compatible spelling:
`std::array<T, N>` and `std::vector<T>`. Both use the same machinery.

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

## `std::vector<T>`

```cpp
#include <vector>

std::vector<Enemy> enemies;               // becomes:  vector_Enemy enemies;
enemies.push_back(Enemy(120, 40, 3));
for (int i = 0; i < enemies.size(); ) {
    if (enemies[i].dead()) enemies.erase(enemies.begin() + i);
    else i++;
}
```

| Member | Notes |
| --- | --- |
| `push_back(value)`, `pop_back()` | capacity doubles (4, 8, 16, ...) |
| `size()`, `capacity()`, `empty()` | `int` / `bool` |
| `operator[]`, `at(i)`, `front()`, `back()` | return `T &`; no bounds check |
| `data()`, `begin()`, `end()` | plain `T *` |
| `reserve(n)`, `clear()` | `clear` keeps the storage |
| `resize(n)`, `resize(n, value)` | new elements zero-filled / copies of `value` |
| `erase(pos)`, `insert(pos, value)` | `pos` is a `T *`: `v.begin() + i` |
| `a = b` | copies the elements |

Things to know (tests/108sample.cpp exercises all of it):

- **Elements are raw memory.** The vector copies elements in with plain
  assignment and never runs an element's constructor or destructor.
  Numbers, pointers, plain structs, and classes whose constructor only
  fills in fields all behave as expected, virtual functions included.
  A class with a *destructor* gets a warning: the vector will not call
  it. Hold pointers (`std::vector<Enemy *>`) for such classes.
- **Copies other than `a = b` are rejected.** `std::vector<T> b = a;`,
  passing one by value and returning one by value are errors: without
  copy constructors, each would leave two vectors owning one buffer.
  Pass `std::vector<T> &`.
- **Storage is `malloc()`/`free()`** from Vircon32's `misc.h`. There are
  no exceptions, so running out of memory is not reported. Call
  `reserve` up front when the size is known: growing copies every
  element.
- **A reference into a vector dies when it grows**, exactly as in C++:
  `Enemy &e = v[0]; v.push_back(x);` leaves `e` dangling.
- **`std::vector<std::vector<int> >`** needs the space between the two
  `>` (`>>` is the shift operator here), and its inner vectors are raw
  memory like any other element: not constructed.
- A file-scope `std::vector` works: globals are now constructed at the
  top of `main()` (tests/109sample.cpp).

## How it works

1. **`src/prescan.c`** consumes `#include <array>` (no such file exists,
   and the line must not reach the generated C) and switches the feature
   on.
2. **`src/lexer.l`** returns one token, `STD_ARRAY`, for `std::array`
   (`STD_VECTOR` for `std::vector`).
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
a program got, read `ARRAY_TEMPLATE` and `VECTOR_TEMPLATE` in `src/generic.c`, or look at the
`array_*` structs and functions in the generated C.

Names: `array_<T>_<N>`, with `::` and `*` spelled `_` and `_ptr`
(`std::array<Enemy *, 2>` is `array_Enemy_ptr_2`; `std::vector<int>` is
`vector_int`). For a pointer element type the generated text uses a
typedef name, because this grammar has no `T *&` declarator; once parsed,
the typedef is replaced by the real pointer type and never reaches the
generated C.

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
- **Multi-dimensional member arrays of class objects** (`Counter
  grid[2][2];` as a member) are still not constructed. A
  `std::array` of `std::array` is fine: each level is its own class.
- **Named lengths are compared by name.** `std::array<int, MAX>` and
  `std::array<int, 8>` are different types even when `MAX` is 8. A
  `#define` is not affected (it is expanded before parsing).
- No range-based `for`, no `auto`, no `::iterator` type names (iterators
  are `T *`).
