/* tests/166sample.c -- __builtin_frame_address(n) and
 * __builtin_return_address(n): the frame of the function that asks, and of
 * its callers, and the place a call returns to. Self-checking. */
int test_errors = -1;

int fa_outer, fa_inner0, fa_inner1, ra_inner;
int fa_deep2, fa_outer_up;

void inner (void)
{
    fa_inner0 = (int) __builtin_frame_address (0);
    fa_inner1 = (int) __builtin_frame_address (1);       /* outer's frame */
    fa_deep2  = (int) __builtin_frame_address (2);       /* the one above */
    ra_inner  = (int) __builtin_return_address (0);      /* inside outer */
}

void outer (void)
{
    fa_outer    = (int) __builtin_frame_address (0);
    fa_outer_up = (int) __builtin_frame_address (1);
    inner ();
}

void main (void)
{
    int e = 0;
    int where = (int) outer;

    outer ();
    if (fa_inner1 != fa_outer) e |= 1;                   /* caller's frame */
    if (fa_deep2 != fa_outer_up) e |= 2;                 /* and its caller's */
    if (fa_inner0 >= fa_outer) e |= 4;                   /* stack grows down */
    if (ra_inner <= where || ra_inner > where + 400) e |= 8;
    if (* (int *) fa_inner0 != fa_outer) e |= 16;        /* [BP] is the caller's BP */
    test_errors = e;
}
