/* shared by tests/159sample.c, 159lib_a.c and 159lib_b.c (gap G6, linking) */
#ifndef SHARED_H
#define SHARED_H
struct Counter { int n; };
extern int g_total;                 /* defined in 159lib_a.c */
extern struct Counter g_counter;
extern int table[4];
extern int (*ops[2])(int, int);
int add(int a, int b);
int next_id(void);
int read_total(void);
int a_calls(void);
static inline int twice(int v) { return v * 2; }   /* one copy, used by every file */
#endif
