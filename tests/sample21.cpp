// Exercises register_free_function's dedup fix (sema.c): a free function
// declared with a separate prototype, then defined later -- an entirely
// ordinary, idiomatic C++ pattern -- used to register as TWO separate
// candidates in the free-function registry (nothing paired a
// prototype to its own later definition the way attach_out_of_line does
// for methods), so resolve_call would see two identically-shaped
// candidates for every call and misreport it as ambiguous. Isolated
// here, deliberately apart from tests/sample14.cpp (which also exercises
// this same fix, but alongside a lot of other, unrelated machinery).
//
// square's own declaration/definition split is the actual thing under
// test. addOne exists only so useSquare() has TWO different free
// functions to call, making it obvious in the dump which resolution
// belongs to which if anything regresses.

int square(int x);

int addOne(int x) {
    return x + 1;
}

int useSquare(int n) {
    return square(n) + addOne(n);
}

int square(int x) {
    return x * x;
}

void main() {
    // Deliberately empty, same reasoning as tests/sample14.cpp's own
    // main() -- this test is about the free-function dedup fix, not
    // about doing anything at runtime. Forgotten in this file's first
    // version, which is exactly why sample21.c failed to compile
    // ("function main is not declared") even though the actual fix
    // being tested (no ambiguous-overload error for square()) worked
    // correctly the whole time.
}
