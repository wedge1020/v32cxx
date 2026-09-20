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
