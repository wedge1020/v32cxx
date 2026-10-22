/* tests/156bad.c -- bad designators are reported, not guessed at (G2) */
struct S { int a; int b; };
struct S s = { .c = 1 };
int arr[3] = { [5] = 1 };
void main (void) { }
