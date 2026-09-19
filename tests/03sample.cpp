// Out-of-line definition of a method on a class nested in a namespace.
// This currently works, but only because sema.c's class registry is flat
// (keyed by bare name) -- if some OTHER class named "Timer" existed
// outside v32, this out-of-line definition would resolve against
// whichever one got registered, not necessarily this one. See the TODO in
// sema.c's attach_out_of_line() and the registry doc comment above it.

namespace v32 {
    class Timer {
        public:
            Timer(int freq);
            int getTicks();
        private:
            int frequency;
            int ticks;
    };
}

int v32::Timer::getTicks() {
    return ticks;
}
