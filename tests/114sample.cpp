// *****************************************************************************
//  tests/114sample.cpp — std::string (#include <string>)
//
//  The built-in <string> header (lib/string, embedded in the transpiler): a
//  fixed-capacity std::string that copies by value. See docs/STRING.md.
//
//  Covered here, each group adding its own bit to test_errors on failure:
//      1  construction, assignment, size, concatenation, comparison
//      2  searching, substr, erase/insert/replace
//      4  to_string / stoi / stof, npos
//      8  implicit conversion from a literal: initialization, argument
//         (by value and by const reference), return, constructor argument
//     16  strings as class members, in std::vector and std::array, and
//         copied (a copy is independent of its source)
//     32  operators on a POINTER to a string are pointer operations
//     64  truncation at the capacity
//    256  c_str() handed to an SDK function (which wants a plain pointer)
//    128  `using namespace std;` and bare `string` (in a namespace of its
//         own below, so the directive's effect is what is being tested)
//
//  Self-checking: test_errors is 0 when everything passed.
// *****************************************************************************
#include "string.h"      // the SDK's C string functions: strlen
#include <string>
#include <vector>
#include <array>

int test_errors = -1;

std::string describe(int n)
{
    if (n > 0) return "positive";
    if (n < 0) return "negative";
    return "zero";
}

int length_of(const std::string &s) { return s.size(); }
int length_by_value(std::string s)  { s += "!"; return s.size(); }

class Player
{
    public:
        std::string name;
        int         score;

        Player(const std::string &who, int points) : name(who), score(points) {}
        const std::string &getName() const { return name; }
};

int basics()
{
    std::string a("hello");
    std::string b;
    b = "world";
    std::string c = a + " " + b;
    if (c.size() != 11 || c.length() != 11 || c.empty() || !b.size()) return 1;
    if (!(c == "hello world") || c != "hello world") return 1;
    if (a >= b || !(a < b) || a.compare(b) >= 0) return 1;
    std::string d = b;
    d += '!';
    d += a;
    if (d != "world!hello") return 1;
    if ("x" + a != "xhello" || a + 'x' != "hellox") return 1;
    if (a[0] != 'h' || a.front() != 'h' || a.back() != 'o' || a.at(1) != 'e') return 1;
    a[0] = 'j';
    if (a != "jello") return 1;
    a.clear();
    if (!a.empty() || a.size() != 0) return 1;
    return 0;
}

int editing()
{
    std::string c = "hello world";
    if (c.find("wor") != 6 || c.find('o') != 4 || c.rfind('o') != 7) return 2;
    if (c.substr(6) != "world" || c.substr(0, 5) != "hello") return 2;
    c.erase(5, 6);
    if (c != "hello") return 2;
    c.insert(0, ">> ");
    if (c != ">> hello") return 2;
    c.replace(3, 5, "bye");
    if (c != ">> bye") return 2;
    c.push_back('.');
    c.pop_back();
    c.append("!!");
    if (c != ">> bye!!" || !c.starts_with(">>") || !c.ends_with("!!")) return 2;
    return 0;
}

int numbers()
{
    std::string n = std::to_string(-1234);
    if (n != "-1234" || std::stoi(n) != -1234) return 4;
    if (std::stoi("17") != 17) return 4;
    float f = std::stof("2.5");
    if (f < 2.49 || f > 2.51) return 4;
    std::string c = "abc";
    if (c.find('z') != std::string::npos || c.find("zz") != -1) return 4;
    return 0;
}

int conversions()
{
    std::string a = "hello";
    if (a.size() != 5) return 8;
    std::string b = describe(1);
    if (b != "positive" || describe(-1) != "negative" || describe(0) != "zero") return 8;
    if (length_of("four") != 4 || length_of(a) != 5) return 8;
    if (length_by_value("sixsix") != 7 || length_by_value(a) != 6 || a != "hello") return 8;
    int total = 0;
    for (int i = 0; i < 3; i++) total += length_of("ab");   // unbraced loop body
    if (total != 6) return 8;
    Player p("ann", 3);
    if (p.getName() != "ann" || p.score != 3) return 8;
    return 0;
}

int containers()
{
    std::vector<std::string> names;
    names.push_back("one");
    std::string two = "two";
    names.push_back(two);
    if (names.size() != 2 || names[0] != "one" || names[1] != "two") return 16;
    int total = 0;
    for (const std::string &s : names) total += s.size();
    if (total != 6) return 16;
    int os = 0;
    for (char ch : two) if (ch == 'o') os++;
    if (os != 1) return 16;
    std::array<std::string, 2> pair;
    pair[0] = "x";
    pair[1] = pair[0] + "y";
    if (pair[1] != "xy") return 16;
    std::string copy = two;
    copy += "!";
    if (two != "two" || copy != "two!") return 16;
    return 0;
}

int pointers()
{
    std::string a = "q";
    std::string b = "r";
    std::string both[2];
    std::string *p = &a;
    std::string *q = &b;
    both[0] = a;
    both[1] = q[0];          // q[0] is the string b, not b's first character
    p[0] = both[1];
    if (a != "r" || both[0] != "q") return 32;
    if (p == q || !(p != q)) return 32;      // pointer comparison
    *p = *q;
    q = p;
    if (p != q || p->size() != 1) return 32;
    return 0;
}

int truncation()
{
    std::string s(100, 'x');                 // more than the capacity
    if (s.size() != s.capacity() || s.capacity() != V32_STRING_CAPACITY) return 64;
    s += "more";
    if (s.size() != s.capacity()) return 64;
    return 0;
}

namespace game
{
    using namespace std;

    string banner(const string &who)
    {
        string text = "hi ";
        text += who;
        return text;
    }

    int check()
    {
        string a = banner("bob");
        if (a != "hi bob" || a.find('z') != string::npos) return 128;
        if (to_string(42) != "42" || stoi("7") != 7) return 128;
        return 0;
    }
}

int sdk_text()
{
    std::string s = "SCORE ";
    s += std::to_string(1500);
    const char *text = s.c_str();
    if (strlen(s.c_str()) != 10 || strlen(text) != 10) return 256;
    return 0;
}

int main()
{
    int errors = 0;
    errors += sdk_text();
    errors += basics();
    errors += editing();
    errors += numbers();
    errors += conversions();
    errors += containers();
    errors += pointers();
    errors += truncation();
    errors += game::check();
    test_errors = errors;
    return 0;
}
