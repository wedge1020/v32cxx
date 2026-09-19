// Exercises primitive member-field initializers (": x(val)") -- a later
// round than base-class delegation itself. Widget's own `value` is
// private, same motivating shape as base-class delegation's own test
// (sample35.cpp): a member-initializer list is one of the few places a
// constructor can set a field before its own body runs at all.
//
// This file previously tested the OPPOSITE case -- this exact syntax
// used to be deliberately unsupported, rejected with a clear "not yet
// supported" error. Repurposed now that the feature exists: expected
// value is 7 (the argument passed to `new Widget(7)`), proving `value`
// was actually set through the member-initializer list, not left as
// heap garbage.

class Widget {
    public:
        Widget(int v);
        int get();
    private:
        int value;
};

Widget::Widget(int v) : value(v) {
}

int Widget::get() {
    return value;
}

void main() {
    Widget *w = new Widget(7);
    int got = w->get();
    delete w;
}
