// extern "C" is not part of the grammar (it never was); the prescan must not
// mistake it for an `extern` declaration of whatever follows on the line.
extern "C" int c_fun(int a);
int main(void) { return 0; }
