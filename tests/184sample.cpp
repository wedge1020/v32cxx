// *****************************************************************************
//  tests/184sample.cpp -- `delete[]` finds the element class in every spelling
//
//  A class with a destructor keeps a count header in front of the elements of
//  `new T[n]`, so `delete[]` must name the class (v32_delete_arr_T): a plain
//  release would free the wrong address. The operand's type is worked out
//  through typedef chains, members (`->`, `.`, inherited, `this->`), arrays
//  and pointers of pointers (`tbl[1]`, `pp[0]`, `*pp`), function and method
//  results, a call through a function pointer, pointer arithmetic, casts,
//  `auto`, namespaces and `std::vector<T *>` / `std::array<T *, N>`. A
//  pointer to pointers is an array of pointers: a plain release. test_errors
//  must end at 0; g_live counts the objects alive. (See 184bad.cpp for the
//  operand whose type cannot be told.)
// *****************************************************************************

#include <vector>
#include <array>

int test_errors = -1;
int g_live = 0;

class D {
public:
    int id;
    D() { id = 1; g_live = g_live + 1; }
    ~D() { g_live = g_live - 1; }
};

class Plain { public: int a; Plain() { a = 2; } };     // a constructor, no destructor: no header

typedef D *DP;
typedef DP DPP;
typedef D DT;
typedef DT DT2;
typedef DT2 *DT2P;

namespace ns {
    D *gp;
    D *gtab[2];
    D *mk() { return new D[2]; }
}

class Base {
public:
    D *bd;
    DPP bdp;
    D *get() { return bd; }
    DP take() { D *r = bd; bd = NULL; return r; }
};

class Holder : public Base {
public:
    D *own;
    D **pp;
    DT2P arr2[2];
    std::vector<D *> vec;
    std::array<D *, 2> ar;
    void clear()
    {
        delete[] own;           // a member
        own = new D[2];
        delete[] this->own;     // through this
        bd = new D[2];
        delete[] bd;            // an inherited member
        bdp = new D[2];
        delete[] bdp;           // an inherited member, typedef chain
        bd = new D[2];
        delete[] get();         // an inherited method
        bd = NULL;
        arr2[1] = new D[2];
        delete[] arr2[1];       // an array of typedef'd pointers
        vec.push_back(new D[2]);
        delete[] vec[0];        // a generic vector's element
        vec.pop_back();
        ar[0] = new D[2];
        delete[] ar[0];         // a generic array's element
    }
};

D *id(D *p) { return p; }
D *mkd() { return new D[2]; }
D *(*g_factory)();

int main(void)
{
    int e = 0;
    g_factory = mkd;

    // an array of pointers, and a pointer to its elements
    D *tbl[3];
    D **pp = tbl;
    tbl[0] = new D[2];
    delete[] pp[0];
    tbl[1] = new D[2];
    delete[] *(pp + 1);
    tbl[2] = new D[3];
    delete[] tbl[2];
    if (g_live != 0) e++;

    // typedefs
    DP q = new D[2];
    delete[] q;
    DT *r = new DT2[2];
    delete[] r;
    DT2P x = new D[2];
    delete[] x;
    if (g_live != 0) e++;

    // members of objects, pointers and references
    Holder h;
    Holder *hp = &h;
    Holder &hr = h;
    h.own = new D[2];
    delete[] h.own;
    hp->own = new D[2];
    delete[] hp->own;
    hr.own = new D[3];
    delete[] hr.own;
    h.arr2[0] = new D[2];
    delete[] h.arr2[0];
    h.bd = new D[2];
    delete[] hp->get();
    h.bd = new D[2];
    delete[] h.take();
    h.own = new D[2];
    h.clear();
    h.vec.push_back(new D[2]);
    h.vec.push_back(new D[2]);
    delete[] hp->vec[0];
    delete[] h.vec.back();
    if (g_live != 0) e++;

    // results of functions, a function pointer, namespaces
    delete[] id(new D[2]);
    delete[] ns::mk();
    delete[] g_factory();
    ns::gp = new D[2];
    delete[] ns::gp;
    ns::gtab[1] = new D[2];
    delete[] ns::gtab[1];
    if (g_live != 0) e++;

    // pointer arithmetic, casts, auto, a range-for variable
    D *base = new D[4];
    D *rest = base + 1;
    delete[] (rest - 1);
    auto a = new D[2];
    delete[] a;
    D *c = new D[2];
    delete[] (D *)c;
    delete[] static_cast<D *>(new D[2]);
    std::vector<D *> v;
    v.push_back(new D[2]);
    v.push_back(new D[3]);
    for (D *it : v) delete[] it;
    if (g_live != 0) e++;

    // a pointer to pointers is an array of pointers: a plain release
    {   D **arr = (D **)malloc(3 * sizeof(D *));
        arr[0] = new D;
        delete arr[0];
        delete[] arr;
        Plain *pl = new Plain[3];
        delete[] pl;
        int *ints = new int[4];
        delete[] ints;
    }
    if (g_live != 0) e++;

    test_errors = e;
    return 0;
}
