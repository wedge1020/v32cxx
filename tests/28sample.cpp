// Exercises preprocessor pass-through (driver.h's g_preprocessor_lines,
// lexer.l's capture, codegen.c's emit_preprocessor_passthrough) -- the
// #include below should survive verbatim into the generated C, at the
// very top, ahead of everything else. Every previous sample that needed
// something like video.h's select_texture required the include line to
// be manually re-added by hand after transpiling; this is the first one
// that shouldn't need that at all.

#include "video.h"

void main() {
    select_texture(-1);
}
