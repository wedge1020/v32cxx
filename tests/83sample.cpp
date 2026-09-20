class Inner {
public:
    Inner() : mX(7) {}
    int mX;
    int value() { return mX; }
};

class Outer {
public:
    Outer() : mPlain(5) {}     // member-init list covers mPlain only
    int mPlain;
    Inner mInner;              // must be implicitly constructed
    Inner* mPtr;               // must NOT be constructed (pointer)
    int sum() { return mPlain + mInner.value(); }  // 12
};

int main() {
    Outer o;
    Outer* p = new Outer();
    int a = o.sum();      // 12
    int b = p->sum();     // 12
    delete p;
    return a + b - 24;    // 0
}
