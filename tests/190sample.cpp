// *****************************************************************************
//  tests/190sample.cpp -- `new T*[n]` and `T*&`
//
//  `new Node*[n]` is an array of pointers: plain pointer-sized words, no
//  constructors and no count header; `delete[]` releases it. `Node*&` is a
//  reference to a pointer, lowered to `Node**` the way `int&` is `int*`:
//  call sites pass the address of a variable, a member or an array element;
//  the function assigns through it, reads it, hands it on to another
//  `Node*&` function; a `Node*&` local binds to a variable. Self-checking,
//  built in the default mode and with --alloc=arena / --alloc=gc.
// *****************************************************************************

int test_errors = -1;
int g_dtors = 0;

class Node {
public:
    int v;
    Node() { v = 0; }
    Node(int n) { v = n; }
    ~Node() { g_dtors = g_dtors + 1; }
};

class Holder {
public:
    Node *slot;
    Node *pair[2];
    Holder() { slot = 0; pair[0] = 0; pair[1] = 0; }
    void fill(int n) { slot = new Node(n); }
    void drop() { drop_ptr(slot); }
    void drop_ptr(Node*& p) { delete p; p = 0; }
};

void reset(Node*& p)                    // assigns through the reference, deletes
{
    delete p;
    p = nullptr;
}

void put(Node*& p, int n)               // assigns a new object, reads the old one
{
    int old = (p != 0) ? p->v : -1;
    delete p;
    p = new Node(n + old);
}

void put_via(Node*& p, int n)           // passes the reference on
{
    put(p, n);
    put(p, 1);
}

void swap_ptrs(Node*& a, Node*& b)
{
    Node *t = a;
    a = b;
    b = t;
}

int value_of(Node*& p) { return p->v; }

Node*& pick(Node*& a, Node*& b, int first)   // returns a reference to one of them
{
    if (first) return a;
    return b;
}

Node *g_node = 0;

int main(void)
{
    int e = 0;

    // new T*[n]: an array of pointers
    int n = 4;
    Node **v = new Node*[n];
    int **ints = new int*[2];
    for (int i = 0; i < n; i++) v[i] = 0;
    if (v[0] != 0 || v[3] != 0) e++;
    v[1] = new Node(11);
    v[2] = new Node(12);
    if (v[1]->v != 11 || v[2]->v != 12) e++;
    int d0 = g_dtors;
    delete v[1];                            // the elements are the caller's
    delete v[2];
    delete[] v;                             // releases the pointer array only
    if (g_dtors != d0 + 2) e++;
    ints[0] = &n;
    ints[1] = &d0;
    if (*ints[0] != 4) e++;
    delete[] ints;

    // T*& with a variable
    Node *a = new Node(5);
    Node *b = new Node(6);
    put(a, 10);                             // 5 + 10
    if (a->v != 15) e++;
    put_via(a, 1);                          // put(1 + 15), then put(1 + 16)
    if (a->v != 17) e++;
    swap_ptrs(a, b);
    if (a->v != 6 || b->v != 17) e++;
    if (value_of(a) != 6) e++;
    int before = g_dtors;
    reset(a);
    if (a != 0 || g_dtors != before + 1) e++;
    reset(a);                               // already null: deleting null is fine
    if (a != 0) e++;

    // a member, an array element, a global
    Holder h;
    h.fill(3);
    put(h.slot, 4);
    if (h.slot->v != 7) e++;
    put(h.pair[1], 8);
    if (h.pair[1]->v != 7 || h.pair[0] != 0) e++;
    Node *arr[3];
    arr[0] = new Node(1);
    arr[1] = 0;
    arr[2] = new Node(2);
    reset(arr[0]);
    swap_ptrs(arr[1], arr[2]);
    if (arr[0] != 0 || arr[2] != 0 || arr[1] == 0 || arr[1]->v != 2) e++;
    reset(arr[1]);
    put(g_node, 9);
    if (g_node->v != 8) e++;
    reset(g_node);
    if (g_node != 0) e++;
    reset(h.pair[1]);
    h.drop();
    if (h.slot != 0) e++;

    // a T*& local bound to a variable
    Node *c = new Node(20);
    Node*& rc = c;
    rc->v = 21;
    if (c->v != 21) e++;
    rc = new Node(30);                      // reseats the VARIABLE, not the reference
    if (c->v != 30) e++;
    put(rc, 1);
    if (c->v != 31) e++;
    Node *dd = new Node(40);
    Node*& pr = pick(c, dd, 0);             // the reference a function returns
    if (pr->v != 40) e++;
    pr = 0;
    if (dd != 0 || c == 0) e++;

    reset(c);
    reset(b);

    test_errors = e;
    return 0;
}
