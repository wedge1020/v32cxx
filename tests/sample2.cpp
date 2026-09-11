// Exercises out-of-line member definitions: a regular method, a
// constructor, and a destructor, all defined outside the class body.
// Also exercises sema_run()'s attach_out_of_line() successfully finding
// and merging each one onto its in-class prototype.

class Counter {
    public:
        Counter(int start);
        ~Counter();
        int getValue();
        void increment();
    private:
        int value;
};

Counter::Counter(int start) {
    value = start;
}

Counter::~Counter() {
    value = 0;
}

int Counter::getValue() {
    return value;
}

void Counter::increment() {
    value += 1;
}

int main() {
    return 0;
}
