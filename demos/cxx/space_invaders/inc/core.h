namespace si {

// ---------------------------------------------------------------------------
// Random: small deterministic LCG, replaces <cstdlib> rand().
// Plain 'int' state (no 'unsigned' in the subset).
// ---------------------------------------------------------------------------
class Random {
public:
    // thin wrapper over the hardware RNG in misc.h (rand/srand);
    // note srand(0) is ignored by the hardware (0 never set as seed)
    void seed(int s) { srand(s); }

    // returns 0..limit-1
    int next(int limit) {
        int r = rand();          // full 32-bit value, may be negative
        if (r < 0) r = -r;
        return r % limit;
    }

    // returns 0 or 1
    int bit() { return next(2); }
};

// ---------------------------------------------------------------------------
// Vec2: operator overloading used pervasively through the game.
// ONE WORD (a single int): x in the high 16 bits, y in the low 16, so it
// can be passed and returned BY VALUE under Vircon32 C's one-word rule.
// ---------------------------------------------------------------------------
class Vec2 {
public:
    Vec2() : mV(0) {}
    Vec2(int px, int py) {
        mV = ((px & 0xFFFF) << 16) | (py & 0xFFFF);
    }

    int x() const { return mV >> 16; }
    int y() const { return (mV << 16) >> 16; }   // sign-extend low half

    void setX(int px) { mV = (mV & 0xFFFF) | ((px & 0xFFFF) << 16); }
    void setY(int py) { mV = (mV & (0xFFFF << 16)) | (py & 0xFFFF); }

    Vec2  operator+(const Vec2& o) const {
        Vec2 r(x() + o.x(), y() + o.y());
        return r;
    }
    Vec2  operator-(const Vec2& o) const {
        Vec2 r(x() - o.x(), y() - o.y());
        return r;
    }
    Vec2& operator+=(const Vec2& o) {
        setX(x() + o.x());
        setY(y() + o.y());
        return *this;
    }
    Vec2& operator-=(const Vec2& o) {
        setX(x() - o.x());
        setY(y() - o.y());
        return *this;
    }
    Vec2  operator*(int s) const {
        Vec2 r(x() * s, y() * s);
        return r;
    }
    bool  operator==(const Vec2& o) const { return mV == o.mV; }
    bool  operator!=(const Vec2& o) const { return mV != o.mV; }

private:
    int mV;
};

// ---------------------------------------------------------------------------
// Axis-aligned rectangle for collision. Plain int fields (no Vec2 member),
// never passed or returned by value except through out-parameters.
// ---------------------------------------------------------------------------
struct Rect {
    int x, y, w, h;

    Rect() : x(0), y(0), w(0), h(0) {}

    bool intersects(const Rect& o) const {
        return x < o.x + o.w && x + w > o.x &&
               y < o.y + o.h && y + h > o.y;
    }
};

// ---------------------------------------------------------------------------
// Entity lifecycle states (namespace-level: the subset has no class enums)
// ---------------------------------------------------------------------------
enum EntityState {
    STATE_ALIVE,
    STATE_DYING,
    STATE_DEAD
};

// ---------------------------------------------------------------------------
// BombList: minimal freestanding replacement for std::vector<Bullet*>.
// NOTE: intentionally NOT a template -- the target compiler has no template
// support, so this is a concrete class. Defined in si_game.h, AFTER Bullet's
// full definition: the v32c++ grammar has no class forward declarations.
// ---------------------------------------------------------------------------

// Shared RNG (namespace-level global: the subset has no static data members)
Random g_rng;

} // namespace si
