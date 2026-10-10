// *****************************************************************************
//  tests/127sample.cpp — variadic functions (`...`) in C++
//
//  The C form -- `...`, va_list, va_start, va_arg, va_end from <cstdarg> --
//  in free functions, a namespace and methods (called through an object and
//  through a pointer). Ints, chars, bools, enums, pointers and floats pass
//  through `...` (a float keeps its bits). An overload with `...` ranks
//  after one without (pick(1) takes pick(int)). Calls with no extra
//  arguments, a variadic call as an argument of another, and calls in a
//  loop condition and on the right of &&.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

#include <cstdarg>
int test_errors = -1;

int sum(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int total = 0;
    for (int i = 0; i < count; i++) total += va_arg(ap, int);
    va_end(ap);
    return total;
}

float sumf(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    float total = 0.0;
    for (int i = 0; i < count; i++) total += va_arg(ap, float);
    va_end(ap);
    return total;
}

int pick(int a)      { return 1; }
int pick(int a, ...) { return 2; }

namespace text
{
    int length_of_all(const char *first, ...)
    {
        va_list ap;
        va_start(ap, first);
        int n = 0;
        const char *p = first;
        while (p != nullptr) {
            for (int i = 0; p[i] != 0; i++) n++;
            p = va_arg(ap, const char *);
        }
        va_end(ap);
        return n;
    }
}

class Log
{
  public:
    Log() : lines(0), last(0) {}
    void print(const char *fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        int sum = 0;
        for (int i = 0; fmt[i] != 0; i++)
            if (fmt[i] == '%') sum += va_arg(ap, int);
        va_end(ap);
        last = sum;
        lines++;
    }
    int count(int n, ...);
    int lines;
    int last;
};

int Log::count(int n, ...) { return n; }

enum Color { Red = 3, Green = 4 };

int main(void)
{
    int e = 0;
    if (sum(0) != 0) e++;
    if (sum(3, 1, 2, 3) != 6) e++;
    if (sum(2, sum(2, 1, 1), 5) != 7) e++;
    if (sumf(3, 1.5, 2.25, 0.25) != 4.0) e++;
    float f = 0.5;
    if (sumf(2, f, f) != 1.0) e++;
    if (pick(1) != 1) e++;
    if (pick(1, 2) != 2) e++;
    if (text::length_of_all("ab", "cde", nullptr) != 5) e++;
    Log log;
    log.print("%d %d", 10, 20);
    if (log.last != 30 || log.lines != 1) e++;
    Log *lp = &log;
    lp->print("%", Green);
    if (log.last != 4 || log.lines != 2) e++;
    if (log.count(4, 'a', true) != 4) e++;
    int k = 0;
    while (sum(1, k) < 3) k++;
    if (k != 3) e++;
    if (k > 0 && sum(2, k, 1) == 4) {} else e++;
    test_errors = e;
    return 0;
}
