// tests/184ok.cpp -- no class has a destructor: no array has a count header, so
// `delete[]` of a `void *` (or of anything) is a plain release and no error
class C {
public:
    int a;
    C() { a = 1; }
};

int main(void)
{
    C *p = new C[2];
    void *v = p;
    delete[] v;
    return 0;
}
