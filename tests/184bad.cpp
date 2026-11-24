// tests/184bad.cpp -- `delete[]` of an operand whose element type cannot be told
//
// D has a destructor, so `new D[n]` keeps a count header and `delete[]` must know
// the class to release the block from the right address. A `void *` says nothing
// about the elements: an error that names the way out (a cast).
class D {
public:
    ~D() {}
};

int main(void)
{
    D *p = new D[2];
    void *v = p;
    delete[] v;             // error: cannot tell the element type
    delete[] (D *)v;        // fine: the cast says it
    return 0;
}
