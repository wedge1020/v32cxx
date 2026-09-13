// Deliberately invalid: a private member operator invoked from outside
// its class. Exercises access-control now being enforced for operator-
// overload uses -- previously a silent gap, since operator resolution
// used to happen at lowering time, after sema's access-control pass had
// already finished, so this was never checked at all.

class Secret {
    public:
        Secret(int v);
    private:
        Secret operator+(Secret other);
        int value;
};

Secret combine(Secret a, Secret b) {
    return a + b;  // ERROR: operator+ is private
}
