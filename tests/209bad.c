/* Deliberately invalid: the program defines functions that the SDK headers v32c++ adds for --alloc also define. */
int test_errors = -1;
void clear_screen(int c) { (void)c; }
void *malloc(int n) { (void)n; return 0; }
int get_cycle_counter(void) { return 1; }
int main(void) { clear_screen(0); test_errors = 0; return 0; }
