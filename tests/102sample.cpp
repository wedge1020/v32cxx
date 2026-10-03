// *****************************************************************************
//  tests/102sample.cpp — implicit constructors/destructors and destructor chaining
//
//  A class that declares no constructor still constructs its base and its
//  members; one that declares no destructor still destroys them; and every
//  destructor, written or implicit, finishes by destroying the members
//  (last declared first) and then the base.
//   1. derived class with no constructor/destructor of its own (one and two
//      levels), a class whose only reason to need them is a member, both;
//   2. order: base before members before body on the way in, the exact
//      reverse on the way out;
//   3. the three ways an object dies: leaving scope, delete, and an ARRAY
//      leaving scope (arrays were constructed but never destroyed);
//   4. delete through a base pointer with a virtual destructor, where the
//      derived destructor is the implicit one;
//   5. a `return;` inside a destructor body does not skip the chain;
//   6. a member that only has virtual methods still gets its vtable set.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_log = 0;       // each constructor adds a distinct amount
int g_dtors = 0;     // each destructor adds a distinct amount
int g_order = 0;     // decimal trail of what ran, in order

class Base
{
public:
    int hp;
    Base() { hp = 7; g_log += 1; g_order = g_order * 10 + 1; }
    ~Base() { g_dtors += 1; g_order = g_order * 10 + 1; }
    virtual int kind() { return 1; }
};

class Derived : public Base              // no constructor, no destructor
{
public:
    virtual int kind() { return 2; }
};

class Leaf : public Derived              // two levels down, still none
{
public:
    int extra;
};

class Part
{
public:
    int v;
    Part() { v = 5; g_log += 10; g_order = g_order * 10 + 2; }
    ~Part() { g_dtors += 10; g_order = g_order * 10 + 2; }
};

class Gear
{
public:
    int teeth;
    Gear() { teeth = 12; g_order = g_order * 10 + 3; }
    ~Gear() { g_dtors += 100; g_order = g_order * 10 + 3; }
};

class Holder                             // members only
{
public:
    Part part;
    int other;
};

class Both : public Base                 // base and members, nothing declared
{
public:
    Part part;
    Gear gear;
};

class Written : public Base              // its own constructor and destructor
{
public:
    Part part;
    Gear gear;
    int quiet;
    Written() { quiet = 0; g_order = g_order * 10 + 4; }
    ~Written()
    {
        g_order = g_order * 10 + 4;
        if( quiet ) return;              // must still destroy gear, part, Base
        g_dtors += 1000;
    }
};

class Shape
{
public:
    virtual ~Shape() { g_dtors += 1; }
    virtual int sides() { return 0; }
};

class Square : public Shape              // implicit destructor, virtual like its base's
{
public:
    Part part;
    virtual int sides() { return 4; }
};

class Speaker                            // virtual methods, no constructor at all
{
public:
    virtual int volume() { return 11; }
};

class Room                               // ...held by value
{
public:
    Speaker speaker;
    int seats;
};

struct Plain { int a; int b; };          // nothing to construct or destroy

void main()
{
    int e = 0;

    // 1 + 3 (scope)
    {
        Derived d;
        if( d.hp != 7 || d.kind() != 2 ) e += 1;
        Leaf l;
        if( l.hp != 7 || l.kind() != 2 ) e += 2;
        Holder h;
        if( h.part.v != 5 ) e += 4;
        Both b;
        if( b.hp != 7 || b.part.v != 5 || b.gear.teeth != 12 || b.kind() != 1 ) e += 8;
        if( g_log != 1 + 1 + 10 + 11 ) e += 16;
        Plain p;
        p.a = 1;
    }
    if( g_dtors != 1 + 1 + 10 + 111 ) e += 32;

    // 2
    g_order = 0;
    {
        Both b;
        if( g_order != 123 ) e += 64;            // Base, Part, Gear
        g_order = 0;
    }
    if( g_order != 321 ) e += 128;               // Gear, Part, Base
    g_order = 0;
    {
        Written w;
        if( g_order != 1234 ) e += 256;
        g_order = 0;
    }
    if( g_order != 4321 ) e += 512;

    // 5
    g_order = 0; g_dtors = 0;
    {
        Written w;
        w.quiet = 1;
        g_order = 0;
    }
    if( g_order != 4321 || g_dtors != 111 ) e += 1024;

    // 3 (delete)
    g_log = 0; g_dtors = 0;
    Derived* pd = new Derived;
    if( pd->hp != 7 || pd->kind() != 2 || g_log != 1 ) e += 2048;
    Base* pb = pd;
    if( pb->kind() != 2 ) e += 4096;
    delete pd;
    if( g_dtors != 1 ) e += 8192;
    g_log = 0;
    Holder* ph = new Holder;
    if( ph->part.v != 5 || g_log != 10 ) e += 16384;
    g_dtors = 0;
    delete ph;
    if( g_dtors != 10 ) e += 32768;

    // 3 (arrays)
    g_log = 0; g_dtors = 0;
    {
        Derived arr[ 3 ];
        if( arr[ 2 ].hp != 7 || arr[ 1 ].kind() != 2 || g_log != 3 ) e += 65536;
        Holder hs[ 2 ];
        if( hs[ 1 ].part.v != 5 ) e += 131072;
    }
    if( g_dtors != 3 + 20 ) e += 262144;

    // 4
    g_dtors = 0;
    Shape* sp = new Square;
    if( sp->sides() != 4 ) e += 524288;
    delete sp;                                   // ~Square (implicit), Part, ~Shape
    if( g_dtors != 11 ) e += 1048576;

    // 6
    Room room;
    Room* pr = new Room;
    if( room.speaker.volume() != 11 || pr->speaker.volume() != 11 ) e += 2097152;

    test_errors = e;
}
