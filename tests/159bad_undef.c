/* tests/159bad_undef.c -- used but never defined: a link error */
int missing(int);
extern int nowhere;
int unused_proto(int);
void main (void) { int x = missing(1) + nowhere; (void)x; }
