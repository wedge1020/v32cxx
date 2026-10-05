/* *****************************************************************************
 *  tests/118sample.c -- C input: a struct, union or enum named WITHOUT its
 *  keyword, as Vircon32 C allows and code written for it does:
 *
 *      struct v32key { int value; v32key *next; };
 *      v32key *v32key_newkey (int keyval);
 *
 *  Standard C has no such thing (the tag needs `struct`, or a typedef), so
 *  this sample is NOT valid for a native C compiler -- unlike 116 and 117.
 *
 *   1. a member that points at its own struct, and at another one;
 *   2. bare tags as return types, parameters (`*` and `**`), locals,
 *      globals, casts and in sizeof;
 *   3. bare and `struct`-keyword spellings of one type, mixed;
 *   4. arrays of a bare tag, written both ways (`node pool[4]` and
 *      Vircon32 C's `node [4] pool`), and a struct held by value;
 *   5. a tag used bare before its struct is defined (`struct later;`);
 *   6. bare union and enum tags;
 *   7. what must still read as before: a tag that is also the name of a
 *      variable, a member, a parameter or a local (`struct rdes rdes[2]`,
 *      `item *item`) -- there the ordinary name wins wherever it is in scope.
 *
 *  Self-checking: test_errors must end at 0.
 * ****************************************************************************/

int test_errors = -1;

/* 1 */
struct node
{
    int   value;
    node *next;
};

struct list
{
    node        *head;
    struct node *tail;
    int          count;
    node         first;         /* by value */
};

/* 5 */
struct later;
later *pending;

/* 6 */
union word
{
    int   i;
    float f;
};

enum shade { DARK, MID, LIGHT };

/* 7 */
struct rdes { int x; } rdes[2];

struct item
{
    int   weight;
    item *item;                 /* a member named like its tag */
};

/* 4 */
node pool[4];
node [2] spare;
int  used = 0;

/* 2 */
node *node_new (int value);
int   list_push (list **where, node *n);
node *list_pop (list **where);

node *node_new (int value)
{
    node *n   = NULL;

    if (used < 4)
    {
        n          = (node *) &pool[used];
        used       = used + 1;
        n -> value = value;
        n -> next  = NULL;
    }

    return (n);
}

int list_push (list **where, node *n)
{
    node *tmp = NULL;

    if ((*where != NULL) && (n != NULL))
    {
        n -> next = NULL;
        if ((*where) -> head == NULL)
        {
            (*where) -> head = n;
        }
        else
        {
            tmp         = (*where) -> tail;
            tmp -> next = n;
        }
        (*where) -> tail  = n;
        (*where) -> count = (*where) -> count + 1;
        return (1);
    }

    return (0);
}

node *list_pop (list **where)
{
    node *n = NULL;

    if ((*where) -> head != NULL)
    {
        n                 = (*where) -> head;
        (*where) -> head  = n -> next;
        (*where) -> count = (*where) -> count - 1;
        n -> next         = NULL;
    }

    return (n);
}

struct later
{
    int  stamp;
    list *owner;
};

later the_later;

int brightness (shade s)
{
    shade other = LIGHT;

    if (s == other) return (2);
    return (s == MID);
}

/* 7: `item` is a parameter here, and a type again afterwards */
int total_weight (struct item *item)
{
    int total = 0;

    while (item != NULL)
    {
        total = total + item -> weight * 2;
        item  = item -> item;
    }

    return (total);
}

int main (void)
{
    int    e = 0;
    list   the_list;
    list  *lp = &the_list;
    node  *n  = NULL;
    word   w;
    item   a;
    item   b;
    struct rdes *rp = rdes;

    /* 1, 2, 3 */
    the_list.head  = NULL;
    the_list.tail  = NULL;
    the_list.count = 0;

    if (!list_push (&lp, node_new (10))) e++;
    if (!list_push (&lp, node_new (20))) e++;
    if (!list_push (&lp, node_new (30))) e++;
    if (list_push (&lp, NULL)) e++;
    if (the_list.count != 3 || the_list.head -> next -> value != 20) e++;
    if (the_list.tail -> value != 30 || the_list.tail -> next != NULL) e++;

    n = list_pop (&lp);
    if (n == NULL || n -> value != 10 || n -> next != NULL) e++;
    n = list_pop (&lp);
    n = list_pop (&lp);
    if (n -> value != 30 || the_list.count != 0) e++;
    if (list_pop (&lp) != NULL) e++;

    if (sizeof (node) != 2 || sizeof (list) != 5) e++;
    if (sizeof (node) * 4 != sizeof pool) e++;

    /* 4 */
    spare[1].value     = 7;
    spare[0].next      = &spare[1];
    the_list.first     = spare[0];
    if (the_list.first.next -> value != 7) e++;
    if ((node *) pool != &pool[0] || used != 3) e++;

    /* 5 */
    the_later.stamp = 99;
    the_later.owner = lp;
    pending         = &the_later;
    if (pending -> stamp != 99 || pending -> owner != &the_list) e++;

    /* 6 */
    w.i = 5;
    if (w.i != 5 || sizeof (word) != 1) e++;
    if (brightness (LIGHT) != 2 || brightness (MID) != 1 || brightness (DARK) != 0) e++;

    /* 7 */
    rdes[0].x = 3;
    rdes[1].x = 4;
    if (rp -> x != 3 || rdes[1].x * rdes[0].x != 12) e++;

    a.weight = 2;
    a.item   = &b;
    b.weight = 5;
    b.item   = NULL;
    if (total_weight (&a) != 14) e++;
    {
        struct item *item = &a;
        int          sum  = 0;

        sum = item -> weight * 3;
        if (sum != 6 || (item) -> item != &b) e++;
    }

    test_errors = e;
    return 0;
}
