// Exercises class-to-struct lowering (phase 1): flattened field layout
// across a 3-level single-inheritance hierarchy, with the vtable pointer
// introduced exactly once (at Entity, the first class with any virtual
// methods) and inherited unchanged by Actor and Player rather than
// duplicated, plus a class with no virtual methods anywhere (Point) that
// should get no vtable pointer field at all.
//
// Expected struct layouts:
//   struct Entity { [0] void *vtable; [1] int x; [2] int y; }
//   struct Actor  { [0] void *vtable; [1] int x (inherited);
//                   [2] int y (inherited); [3] int health; }
//   struct Player { [0] void *vtable; [1] int x (inherited);
//                   [2] int y (inherited); [3] int health (inherited);
//                   [4] int score; }
//   struct Point  { [0] int px; [1] int py; }  -- no vtable pointer at all

class Entity {
    public:
        virtual void update();
    protected:
        int x;
        int y;
};

class Actor : public Entity {
    public:
        virtual void act();
    protected:
        int health;
};

class Player : public Actor {
    public:
        void act();   // override, doesn't repeat 'virtual'
    private:
        int score;
};

class Point {
    public:
        Point(int px, int py);
    private:
        int px;
        int py;
};
