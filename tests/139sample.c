/* *****************************************************************************
 *  tests/139sample.c -- Vircon32-style array of pointers: `T* [N] name;`
 *
 *  The length-before-the-name declarator takes a pointer element type, as a
 *  local, a global and a struct member (`Node* [4] next;`).
 *  Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

int test_errors = -1;

struct Node
{
    int      value;
    Node* [4] next;
};

Node* [3] globals;

void main(void)
{
    int e = 0;
    int a = 1;
    int b = 2;
    int* [2] locals;
    Node n;

    locals[0] = &a;
    locals[1] = &b;
    n.next[2] = &n;
    globals[1] = &n;
    n.value = 7;

    if (*locals[0] + *locals[1] != 3) e += 1;
    if (n.next[2]->value != 7) e += 2;
    if (globals[1]->value != 7) e += 4;

    test_errors = e;
}
