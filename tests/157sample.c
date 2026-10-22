/* tests/157sample.c -- writes through a pointer returned by a call whose
 * callee has only a prototype in this file (gap report G7): each statement
 * must be lowered to one call, with no warning. Text-checked only: the
 * Vircon32 compiler cannot link the undefined me() (gap G6). */
struct P { int x; int a[3]; };
struct P *me(void);
int n;
void f(int i) { me()->x += n; me()->a[i] = 0; me()->x = 1; (*me()).x++; }
void main(void) { f(1); }
