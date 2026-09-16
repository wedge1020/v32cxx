// Exercises a member-initializer argument that references ANOTHER
// member by its bare name (not "this->y") -- confirms this-injection's
// own rewriting reaches member-initializer-list arguments too, not just
// the constructor body (a real bug found and fixed the same round this
// feature's declaration-order semantics were built: without it, the
// generated C would reference a bare, undeclared identifier no C
// compiler accepts).
//
// x is declared before y, and the list writes y(x) -- x's OWN value
// should already be set (via the earlier, declaration-order-correct
// "x(a)" initializer) by the time y's own initializer runs, so this
// also exercises declaration order and cross-member reference together.
// Expected generated assignment order: "this->x = a; this->y = this->x;".

class Thing {
    public:
        Thing(int a);
    private:
        int x;
        int y;
};

Thing::Thing(int a) : x(a), y(x) {
}

void main() {
    Thing *t = new Thing(5);
    delete t;
}
