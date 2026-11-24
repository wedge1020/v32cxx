// A variable that an SDK pass-through header defines, redeclared `extern` here to reach it (here with the
// stand-in tests/sdkstub/time.h; tests/201sample.cpp does the same with the real misc.h). The generated C
// must not define it a second time: the header already does.
#include "time.h"

extern int sdk_tick_count;
extern int sdk_table[3];
extern int sdk_a, *sdk_b;
int mine = 3;

int main(void)
{
    return sdk_tick_count + sdk_table[1] + sdk_a + mine + (sdk_b == 0);
}
