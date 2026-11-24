// *****************************************************************************
//  tests/194sample.cpp -- `new T*[n]{...}`: an array of pointers with a list
//
//  The listed pointers come first and the rest are null (as `new T*[n]{}` in
//  C++11, not the indeterminate values of a plain `new T*[n]`): lowered like
//  `new T[n]{...}` on helpers v32_new_arr_ptr_init/_rest. Nothing is
//  constructed or destroyed: the elements are only pointers (a class with a
//  destructor included), and `delete[]` releases the block. Self-checking,
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

class Bag {
public:
    Node **items;
    int count;
    Bag(Node *a, Node *b, int n) { count = n; items = new Node*[n]{ a, b }; }   // in a constructor
    ~Bag() { delete[] items; }
};

Node **make(Node *a, int n) { return new Node*[n]{ a }; }                       // a return value

int count_null(Node **p, int n)
{
    int c = 0;
    for (int i = 0; i < n; i++) if (p[i] == nullptr) c++;
    return c;
}

int main(void)
{
    int e = 0;
    int n = 5;
    Node a(1), b(2);
    int x = 7, y = 8;

    Node **v = new Node*[n]{ &a, &b };                // rest nullptr
    if (v[0] != &a || v[1] != &b || count_null(v, n) != 3) e += 1;
    if (v[0]->v != 1 || v[1]->v != 2) e += 2;

    int **w = new int*[3]{ &x, nullptr, &y };         // a null in the list, n == k
    if (*w[0] != 7 || w[1] != nullptr || *w[2] != 8) e += 4;

    int **s = new int*[1]{ &x, &y };                  // n < k: the list's length is allocated
    if (*s[0] != 7 || *s[1] != 8) e += 8;

    Node **z = new Node*[4]{};                        // an empty list
    if (count_null(z, 4) != 4) e += 16;               // all null
    z[0] = &a;
    if (z[0] != &a || count_null(z, 4) != 3) e += 16;

    Node **m = make(&b, 3);
    if (m[0] != &b || count_null(m, 3) != 2) e += 32;

    int before = g_dtors;
    Node *heap = new Node(9);
    Node **h = new Node*[2]{ heap };
    if (h[0]->v != 9 || h[1] != nullptr) e += 64;
    delete[] h;                                       // frees the pointer block only
    if (g_dtors != before) e += 128;
    delete heap;
    if (g_dtors != before + 1) e += 256;

    {
        Bag bag(&a, &b, 4);
        if (bag.items[1] != &b || count_null(bag.items, 4) != 2) e += 512;
    }

    delete[] v;
    delete[] w;
    delete[] s;
    delete[] z;
    delete[] m;
    test_errors = e;
    return 0;
}
