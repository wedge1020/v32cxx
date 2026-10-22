/* tests/159bad_dup.c -- a function defined in two files is a link error */
int dup(void) { return 1; }
void main (void) { }
