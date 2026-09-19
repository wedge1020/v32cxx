// Exercises typedef-transparent type comparison (out-of-line matching +
// mangling) and access-control tracking (member access defaults/labels,
// plus a non-public inheritance access-specifier).

typedef int Meters;

class Box {
    // No leading access-specifier here: these members should default to
    // private, matching real C++'s default for `class` (never public,
    // which is `struct`'s default -- not supported by this project).
    int width;

    public:
        Box(int w);
        void setWidth(Meters w);   // declared using the typedef...
    protected:
        int height;
};

// ...but defined out-of-line using the underlying type directly. If
// typedef resolution works, this still attaches to setWidth's prototype
// above (same signature: Meters IS int) rather than reporting "no
// matching declaration for out-of-line definition of 'setWidth'".
void Box::setWidth(int w) {
    width = w;
}

// Exercises the new PROTECTED (and, implicitly, PRIVATE) opt_base
// alternative -- every earlier test used public inheritance only.
class SecretBox : protected Box {
    public:
        SecretBox(int w);
};
