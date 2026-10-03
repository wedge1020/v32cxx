// *****************************************************************************
//  tests/99sample.cpp — C-style tag typedefs and elaborated type specifiers
//
//  Plain C spells its types `struct Actor` and needs a typedef to drop the
//  keyword; v32c++ takes both spellings and emits one Vircon32 C type:
//   1. `struct X` / `union X` / `enum X` as a type anywhere a type goes;
//   2. forward declarations (`struct Actor;`);
//   3. `typedef struct Actor Actor;` before OR after the definition
//      (emits nothing: the tag already is the type name);
//   4. `typedef struct Tag { ... } Name;`, with Name == Tag or not, and
//      with a pointer declarator;
//   5. `typedef struct { ... } Name;` (anonymous), same for union and enum;
//   6. `friend class X;` where X is already a known type.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

struct Actor;                              // 2
typedef struct Actor Actor;                // 3, before the definition
struct Actor { int x; int y; Actor* next; struct Actor* prev; };
typedef struct Actor Actor;                // 3, after it
typedef struct Actor Hero;
typedef struct Actor* ActorPtr;

union Cell { int i; float f; };
typedef union Cell Cell;
enum Dir { North, South, East };
typedef enum Dir Dir;

typedef struct Vec { int x; int y; } Vec;              // 4, same name
typedef struct Node_ { int v; struct Node_* next; } Node;      // 4, different name
typedef struct Item_ { int id; } *ItemPtr;             // 4, pointer
typedef struct { int a; int b; } Pair;                 // 5
typedef union { int i; float f; } Bits;
typedef enum { Red, Green, Blue } Color;

class Vault;
class Guard { friend class Vault; int key; public: Guard() { key = 7; } };
class Vault { public: int open( Guard* g ) { return g->key; } };   // 6

struct Actor g_actor;
enum Dir g_dir = East;

struct Actor* advance( struct Actor* a, enum Dir d )
{
    if( d == South ) a->y += 1;
    return a;
}

int total( union Cell* c, Pair* p ) { return c->i + p->a + p->b; }

void main()
{
    int e = 0;
    struct Actor a; Actor b; Hero h; ActorPtr p = &a;
    Vec v; Node n; struct Item_ it; ItemPtr ip = &it;
    Pair pr; Bits bt; Color c = Blue; Dir d = South; union Cell ce;
    Guard g; Vault vault;

    a.x = 3; a.y = 0; a.next = &b; a.prev = &h; b.x = 4; h.x = 5;
    v.x = 6; n.v = 7; n.next = &n; it.id = 8;
    pr.a = 9; pr.b = 10; bt.i = 11; ce.i = 12;
    g_actor.x = 13;

    if( p->x != 3 ) e++;
    if( a.next->x != 4 || a.prev->x != 5 ) e++;
    if( advance( &a, d )->y != 1 ) e++;
    if( v.x + n.next->v + ip->id != 21 ) e++;
    if( total( &ce, &pr ) != 31 ) e++;
    if( bt.i != 11 || c != 2 || g_dir != East ) e++;
    if( g_actor.x != 13 ) e++;
    if( sizeof( struct Actor ) != 4 || sizeof( Pair ) != 2 ) e++;
    if( vault.open( &g ) != 7 ) e++;

    test_errors = e;
}
