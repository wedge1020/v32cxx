int test_errors = -1;
int seen = 0;
void note(char *p) { if (p == 0) seen++; }
struct ops { void (*fn)(char *); };
void (*table[2])(char *) = { note, note };
void (*global_cb)(char *) = note;
int main(void) {
    void (*cb)(char *) = note;
    struct ops o;
    struct ops *po = &o;
    o.fn = note;
    cb(0);
    o.fn(0);
    po->fn(0);
    table[1](0);
    global_cb(0);
    test_errors = (seen == 5) ? 0 : 1;
    return 0;
}
