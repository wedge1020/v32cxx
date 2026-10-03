// Minimal SDL2 stand-in: the Vircon32 dev tools only use SDL to find
// their own executable's directory.
#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <climits>
inline int SDL_Init(unsigned) { return 0; }
inline void SDL_Quit() {}
inline void SDL_free(void* p) { std::free(p); }
inline char* SDL_GetBasePath() {
    char buf[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (n < 0) return strdup("./");
    buf[n] = 0; char* slash = strrchr(buf, '/'); if (slash) slash[1] = 0;
    return strdup(buf);
}
