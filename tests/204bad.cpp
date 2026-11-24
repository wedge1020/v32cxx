// Two aggregates that hold each other by value cannot exist in C: an error that names both.
union U;
struct S { U u; int k; };
union U { S s; int i; };
int main() { return 0; }
