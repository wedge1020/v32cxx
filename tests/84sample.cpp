// Regression test for the Vircon32 C compiler workaround (phase 3c).
// Mirrors the exact game pattern that HALTed: a virtual call whose receiver
// is the result of an overloaded operator[] (a call, post-lowering), with
// the same call textually repeated as the dispatch argument.
//
// EXPECTED GENERATED C (what to eyeball after transpiling):
//   Bullet* v32_temp_0 = BombList__op_index__int((&L), i);
//   v32_temp_0->vtable->Entity__update__void(((Entity*)v32_temp_0));
// i.e. NO textual duplication of the operator[] call across the dispatch.
//
// Also exercises the hazard our compiler repro did NOT cover: a virtual
// call with a SIMPLE receiver but a call-containing ARGUMENT.

class Entity {
public:
    Entity() {}
    virtual ~Entity() {}
    virtual void update() {}
    virtual int  tag(int x) { return x; }
};

class Bullet : public Entity {
public:
    Bullet() : mN(0) {}
    void update() { ++mN; }          // overrides virtual
    int  tag(int x) { return mN + x; }
    int  n() const { return mN; }
private:
    int mN;
};

class BombList {
public:
    BombList() : mSize(0) {}
    int size() const { return mSize; }
    void push(Bullet* b) { if (mSize < 4) mItems[mSize++] = b; }
    Bullet* operator[](int i) { return mItems[i]; }
private:
    Bullet* mItems[4];
    int mSize;
};

int one() { return 1; }

int main()
{
    BombList L;
    for (int i = 0; i < 4; ++i) L.push(new Bullet());

    // HAZARD 1: virtual call, receiver is an operator[] call result.
    // Without the workaround this emits the doubled receiver that the
    // Vircon32 C compiler miscompiles (HALT ~first delete in the game).
    // NOTE: plain subscript on the OBJECT (L[i]), not (*L)[i] -- L is not
    // a pointer, and *L would be ill-formed C++ (deref of a non-pointer).
    // It still lowers to BombList__op_index__int(&L, i): a call result in
    // receiver position, which is the hazard under test.
    for (int i = 0; i < L.size(); ++i)
        L[i]->update();

    // HAZARD 2: virtual call with simple receiver but a call-containing
    // argument -- also clobbers the staged receiver in the buggy compiler.
    int t = L[0]->tag(one());

    int sum = 0;
    for (int i = 0; i < L.size(); ++i)
        sum = sum + L[i]->n();

    // hazard-1 loop updates each bullet once -> n == 1 each; hazard-2
    // adds one() to bullet 0's n -> t == 1 + 1 == 2. sum == 4.
    return (sum == 4 && t == 2) ? 0 : 1;
}
