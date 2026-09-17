// Exercises a `struct` WITH a constructor and a method -- confirms
// struct shares every piece of this project's own class machinery
// identically to class (constructor invocation via `new`, method
// calls), not just the default-access difference sample51.cpp
// exercises. Expected: result is 5, proving the constructor actually
// ran through the same `new`-lowering/allocator path a `class` would.

struct Widget {
    Widget(int startX);
    int getX();
    int x;
};

Widget::Widget(int startX) {
    this->x = startX;
}

int Widget::getX() {
    return x;
}

void main() {
    Widget *w = new Widget(5);
    int result = w->getX();
    delete w;
}
