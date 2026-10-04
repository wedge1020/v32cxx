# std::string

`#include <string>` gives a program `std::string`. There is no such file
on disk: the header is built into the transpiler (its source is
[`lib/string`](../lib/string)) and is an ordinary C++ class, transpiled
with the rest of the program.

```cpp
#include <string>

std::string label(int score)
{
    std::string text = "SCORE ";
    text += std::to_string(score);
    return text;
}

// ...
print(label(1500).c_str());
```

## The one big difference: a fixed capacity

A `std::string` here keeps its characters **inside the object**, in an
array of `V32_STRING_CAPACITY + 1` characters. It never allocates.

That is what makes copying work. v32c++ has no copy constructors: an
object is copied as raw memory. A string that pointed at heap memory
would be shared by its copies and freed twice. With the characters in
the object, a copy is a real, independent copy, so a string can be
assigned, passed, returned, stored in a `std::vector` or a class member,
exactly as in C++.

The price:

- **A string that would grow past the capacity is truncated, silently.**
  `size()` never exceeds `capacity()`.
- The capacity is **63 characters** unless you set it, before the
  include or on the command line:

  ```cpp
  #define V32_STRING_CAPACITY 127
  #include <string>
  ```
  ```sh
  v32c++ -D V32_STRING_CAPACITY=127 game.cpp
  ```
  It is one setting for the whole program.
- On Vircon32 a `char` is a full 32-bit word, so every string occupies
  capacity + 2 words whatever its length, and copying one copies all of
  them. Pass strings as `const std::string &` where you can, and keep
  the capacity no larger than you need if you hold many of them.

## What is there

| | |
|---|---|
| Construction | `string()`, `string("text")`, `string("text", count)`, `string(count, 'c')`, `string(other, pos, count)` |
| Size | `size` `length` `empty` `capacity` `max_size` `clear` `resize` `reserve` `shrink_to_fit` (the last two do nothing) |
| Access | `s[i]` `at` `front` `back` `c_str` `data` `begin` `end` |
| Adding | `push_back` `pop_back` `append` (text, text+count, string, substring, count+char) `+=` (string, text, char) |
| Assigning | `=` (string, text, char), `assign` |
| Editing | `erase(pos, count)` `insert(pos, ...)` `replace(pos, count, ...)` |
| Searching | `find` `rfind` `find_first_of` `find_first_not_of` `find_last_of` `find_last_not_of` `starts_with` `ends_with` |
| Pieces | `substr(pos, count)` |
| Comparing | `compare`, `== != < > <= >=` against a string or text, with the text on either side of `==` and `!=` |
| Joining | `a + b`, `a + "text"`, `a + 'c'`, `"text" + a` |
| Numbers | `std::to_string(int)`, `std::to_string(float)` (six decimals), `std::stoi`, `std::stof` |

A literal converts to a string wherever C++ would convert it:

```cpp
std::string name = "ann";            // initialization
greet("bob");                        // argument, by value or const reference
return "none";                       // return value
Player p("cy", 0);                   // constructor argument
names.push_back("dee");              // into a std::vector<std::string>
```

Range-based `for` walks the characters (`for (char c : name)`), and
`using namespace std;` (or `using std::string;`) lets you write `string`.

## Differences from C++

- Fixed capacity and truncation, above.
- **Sizes and positions are `int`**, and `std::string::npos` is `-1`.
  `if (s.find("x") != std::string::npos)` reads as usual.
- `at()`, `front()`, `back()`, `substr()` and the rest **do not check
  bounds or throw**: there are no exceptions. Out-of-range positions are
  clamped where that is cheap (`substr`, `erase`, `insert`), and
  otherwise it is your index.
- `stoi` and `stof` take just the string (no position or base
  arguments), read as far as the digits go, and return 0 for no digits.
- `std::to_string(float)` always writes six decimals, like C++.
- Iterators are `char *`. There is no `string::iterator` type name, no
  reverse iterators, and no `size_t`.
- No streams (`<<`, `>>`, `getline`), no `string_view`, no wide or UTF
  strings, no `std::swap`, no `+` between two literals (that is not C++
  either).
- A conversion from a literal **in a loop's condition or step**
  (`while (count("x") < n)`) is an error: construct the string in a
  variable before the loop. Everywhere else the conversion object is
  declared just before the statement that needs it.
- `using std::string;` makes every name in `std` visible, not just
  `string`.

## Printing

The SDK's text functions take a plain pointer. `c_str()` and `data()`
return `const char *`, and the transpiler removes the `const` when the
pointer goes to a C function, so this works as written:

```cpp
print(text.c_str());
print_at(10, 40, text.c_str());
```

## Changing it

`lib/string` is the source. After editing it, run
`sh tools/embed-header.sh` from the project root to regenerate
`src/stdstring.c`, then `make`. `tests/114sample.cpp` exercises it.

## What it took in the transpiler

Besides the header itself, std::string needed these, all general:

- **Converting constructors**: a value is converted to a class that can
  be constructed from it, in an initialization, an argument, and a
  return. Any class with a suitable one-argument constructor gets this,
  not only std::string.
- The **implicit copy assignment** survives a class declaring
  `operator=` for other right-hand types.
- Operators on a **pointer** to an object (`p[0]`, `p == q`) are pointer
  operations, even when the class overloads them.
- A free operator with the object on the **right** (`"x" + s`), and free
  operators are only chosen when the operand types fit.
- `char` and `int` stand in for each other in overload matching, and an
  array argument decays to a pointer.
