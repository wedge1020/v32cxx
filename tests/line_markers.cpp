#include "video.h"
struct P { int x; P() { x = 1; } ~P() { x = 0; } int get() { return x; } };
int g;
void main() { P p; g = p.get(); if (g) { g = g + 1; }
  while (g < 5) g++;
  g = 9; }
