// Exercises new/delete-to-runtime-call rewriting (lowering phase 6) --
// deliberately a PLACEHOLDER, not a faithful lowering. See the long
// comment on this phase in lower.c for exactly what's simplified (no
// sizeof, no constructor invocation -- the grammar doesn't even parse
// constructor arguments in a `new` expression yet) and why.

class Widget {
    public:
        Widget();
};

void makeAndDestroy() {
    Widget* w = new Widget;
    delete w;
}
