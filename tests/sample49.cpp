// Deliberately invalid: `continue` inside a switch that has NO
// enclosing loop at all -- real C rejects this (continue only ever
// targets a loop, never a switch, even though `break` alone inside a
// switch IS valid with no loop around it at all -- see sample47.cpp,
// which has no enclosing loop either and is perfectly valid). Should
// produce exactly one semantic error, confirming this project's own
// g_sema_loop_depth/g_sema_switch_depth split actually distinguishes
// the two rather than conflating them.

void main() {
    int x = 1;
    switch (x) {
        case 1:
            continue;
        default:
            break;
    }
}
