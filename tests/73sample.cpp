// sample73.cpp -- function-pointer typedefs, both accepted declarator
// spellings (standard-C and Vircon32-native), matching the same
// dual-acceptance treatment sample64/sample65 already established for
// plain function-pointer VARIABLES. Exercises both declaring a typedef
// name and actually using it (as a variable type, assigned a real
// function, called through).

typedef int (*Callback)(int);        // standard-C style
typedef int(int)* NativeCallback;    // Vircon32-native style

int doubleIt(int x) {
    return x * 2;
}

int tripleIt(int x) {
    return x * 3;
}

int main() {
    Callback cb = doubleIt;
    NativeCallback ncb = tripleIt;

    int a = cb(5);
    int b = ncb(5);

    return a + b;
}
