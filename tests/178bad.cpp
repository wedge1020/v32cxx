// Deliberately invalid: two #texture hints with the same name. Each name becomes
// a `#define`, so the later one used to silently replace the earlier one.
#texture HERO "hero.png"
#texture BACKDROP "backdrop.png"
#texture HERO "hero2.png"
int main(void) { return 0; }
