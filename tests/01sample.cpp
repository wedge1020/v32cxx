namespace v32 {
    class Timer {
        public:
            Timer(int freq);
            ~Timer();
            int getTicks();
        private:
            int frequency;
            int ticks;
    };
}

class Entity {
    public:
        Entity(int startX, int startY);
        virtual void update();
        int x;
        int y;
    protected:
        bool active;
};

class Player : public Entity {
    public:
        Player(int startX, int startY);

        virtual void update() {
            if (this->health <= 0) {
                this->active = false;
            }
            for (int i = 0; i < 10; i++) {
                x += i;
            }
        }
    private:
        int health;
        v32::Timer *invincibilityTimer;
};

int clamp(int value, int lo, int hi) {
    if (value < lo) {
        return lo;
    } else if (value > hi) {
        return hi;
    }
    return value;
}
