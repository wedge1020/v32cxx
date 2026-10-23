/* second file of tests/161sample.c */
#include "161types.h"

struct pipe;                       /* tag with a function of the same name */
int pipe (int *fds)
{
    if (fds == 0) return -1;       /* `0` against a pointer parameter */
    fds[0] = 3;
    fds[1] = 4;
    return 0;
}

struct pipe *pipe_tag_user (struct pipe *p) { return p; }

char *scan_to_end (char *s, int n)
{
    return s + n;
}
