// An overloaded name whose signature the pointer's type does not have: an
// error, not a guess.
int over(int x) { return x; }
int over(float x) { return 2; }
int (*bad)(char *) = over;
int (*fine)(int) = over;
int main(void)
{
    int (*loc)(char *) = over;
    return 0;
}
