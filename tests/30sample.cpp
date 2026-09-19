// Exercises break/continue (new this round) specifically alongside
// destructor invocation at scope exit (lower.c's phase 9, redesigned
// this round to treat break/continue as a THIRD early-exit path,
// alongside fall-through and return). Logger (constructor + destructor,
// no virtual methods) mirrors tests/sample27.cpp's own pattern,
// specifically to exercise the same machinery in a loop context rather
// than only a function-return context.
//
// countUpTo constructs a fresh Logger every iteration and exercises all
// three exit paths a single loop body can take: `break` (once i reaches
// limit), `continue` (skipping the count on i == 5, without skipping
// the loop's own increment), and ordinary fall-through (every other
// iteration). Each path needs `marker` destroyed before it executes --
// the loop body's own fall-through destructor sequence was already
// exercised by earlier tests; break and continue are what's new here.

class Logger {
    public:
        Logger();
        ~Logger();
    private:
        int id;
};

Logger::Logger() {
    this->id = 0;
}

Logger::~Logger() {
    this->id = -1;
}

int countUpTo(int limit) {
    int count = 0;
    int i = 0;
    while (i < 100) {
        Logger marker;
        if (i >= limit) {
            break;
        }
        if (i == 5) {
            i = i + 1;
            continue;
        }
        count = count + 1;
        i = i + 1;
    }
    return count;
}

void main() {
    countUpTo(10);
}
