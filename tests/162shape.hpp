#ifndef V32_162SHAPE_HPP
#define V32_162SHAPE_HPP
class Counter {
public:
    Counter(int start);
    int next();
    int peek() const { return value; }
private:
    int value;
};
int twice(int v);
extern int g_total;
#endif
