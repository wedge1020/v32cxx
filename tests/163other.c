/* second file of tests/163sample.c */
#define N 20
int other_n (void) { return N; }
int other_sees_only_here (void)
{
#ifdef ONLY_HERE
    return 1;
#else
    return 0;
#endif
}
