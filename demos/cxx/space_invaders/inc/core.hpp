#pragma once
// *****************************************************************************
//  core.hpp — the small value types everything else is built from
// *****************************************************************************
#include "misc.h"   // rand() / srand(): the console's hardware RNG

namespace si {

// ---------------------------------------------------------------------------
// Random numbers: a thin wrapper over the hardware RNG.
// ---------------------------------------------------------------------------
class Random {
public:
    void seed(int s) { srand(s); }   // note: the hardware ignores a seed of 0

    // 0 .. limit-1
    int next(int limit = 2) {
        int r = rand();              // a full 32-bit value, may be negative
        if (r < 0) r = -r;
        return r % limit;
    }

    bool coin() { return next() == 1; }
};

// ---------------------------------------------------------------------------
// Vec2: a pair of ints with the usual operators.
//
// Two words, passed and returned BY VALUE. Vircon32 C itself only moves
// one word per parameter or return value; v32c++ rewrites a bigger struct
// to travel through a hidden pointer, so `a + b` can be written as usual.
// (This class used to pack x and y into the two halves of a single int to
// stay inside that one-word limit -- and paid for it with a function call
// on every read of x or y.)
// ---------------------------------------------------------------------------
struct Vec2 {
    int x;
    int y;

    Vec2() : x(0), y(0) {}
    Vec2(int px, int py) : x(px), y(py) {}

    Vec2  operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2  operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2  operator*(int s) const         { return Vec2(x * s, y * s); }
    Vec2& operator+=(const Vec2& o)      { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o)      { x -= o.x; y -= o.y; return *this; }
    bool  operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool  operator!=(const Vec2& o) const { return x != o.x || y != o.y; }
};

// ---------------------------------------------------------------------------
// Axis-aligned rectangle, for collision.
// ---------------------------------------------------------------------------
struct Rect {
    int x, y, w, h;

    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int px, int py, int pw, int ph) : x(px), y(py), w(pw), h(ph) {}

    bool intersects(const Rect& o) const {
        return x < o.x + o.w && x + w > o.x &&
               y < o.y + o.h && y + h > o.y;
    }
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

// Entity lifecycle. (Namespace-level: v32c++ has no class-nested enums.)
enum EntityState {
    STATE_ALIVE,
    STATE_DYING,
    STATE_DEAD
};

// One period of a sine wave in 16 steps, amplitude 16. SINE16[k] is the
// sine of k sixteenths of a turn; SINE16[(k + 4) & 15] is its cosine.
// Used by the title logo's wave and the saucer's rim lights.
const int SINE16[16] = {   0,   6,  11,  15,  16,  15,  11,   6,
                           0,  -6, -11, -15, -16, -15, -11,  -6 };

// The game's one random number generator. (A namespace-level global:
// v32c++ has no static data members.)
Random g_rng;

} // namespace si
