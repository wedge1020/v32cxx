// Deliberately invalid: a #sound that reuses the name of a #texture (both would be
// `#define MUSIC ...`).
#texture MUSIC "music.png"
#sound MUSIC "music.wav"
int main(void) { return 0; }
