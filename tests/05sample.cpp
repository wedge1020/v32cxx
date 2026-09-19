// Deliberately invalid, but only *semantically* -- this parses cleanly
// (qname_prefix doesn't require its qualifier to resolve to anything at
// parse time), and exists to exercise sema_run()'s two error paths in a
// single run without crashing or stopping after the first one:
//   1. an out-of-line definition with no matching in-class prototype
//   2. an out-of-line definition qualified by a class that was never
//      declared at all
// Expect exactly two "semantic error" lines on stderr and a non-zero exit
// code from v32c++, but a normal (non-crashing) return.

class Widget {
    public:
        Widget();
};

void Widget::spin() {
    // no matching in-class prototype for "spin" -- Widget only declares
    // a constructor.
}

void Ghost::haunt() {
    // "Ghost" was never declared as a class anywhere.
}
