// Exercises the remaining array-completion pieces from this round:
// initializer lists (`= {...}`), array function parameters (decaying to
// a pointer, matching real C/C++ semantics exactly), and new[]/delete[]
// (heap-allocated arrays -- allocation only this round, deliberately no
// per-element construction; see docs/DESIGN_NOTES.md).

int sum(int values[], int count) {
    int total = 0;
    int i = 0;
    while (i < count) {
        total = total + values[i];
        i = i + 1;
    }
    return total;
}

void main() {
    int fixed[4] = {10, 20, 30, 40};
    sum(fixed, 4);

    int *heap = new int[5];
    heap[0] = 1;
    delete[] heap;
}
