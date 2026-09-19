// Exercises this-injection (lowering phase 2): every method gets an
// explicit "this" parameter, and every reference to the receiver --
// explicit `this->x`, implicit bare-identifier `x`, and an unqualified
// call to another method -- gets rewritten to the same explicit `->`
// form. A local variable sharing a name with a member must SHADOW it
// (left alone, not rewritten) exactly the way access control's identical
// local-vs-member lookup already does.

class Counter {
    public:
        Counter(int start);
        void increment();
        int getValue();
        void resetAndLog();
    private:
        int value;
};

void Counter::increment() {
    value = value + 1;       // both "value"s are implicit this->value
    this->value = value;      // one explicit, one implicit -- both end up explicit
}

int Counter::getValue() {
    int value = 42;           // local variable SHADOWING the member -- must NOT be rewritten
    return value;
}

void Counter::resetAndLog() {
    value = 0;                // implicit this->value
    increment();               // implicit this->increment() -- an unqualified method call
}
