// Exercises -b (BIOS transpilation), plus #title/#version hints
// together with the #texture/#sound hints from sample33 -- a BIOS
// build's own constraints: exactly one #texture, at most one #sound,
// and a `void error_handler()` function alongside `main`. Run this one
// with `-b` specifically (see the Makefile's own test target) --
// without it, this is just an ordinary, unremarkable program.
//
// Expected XML: <rom type="bios" title="Vircon32 BIOS Test"
// version="0.9" />, with exactly one <texture> and one <sound>.

#include "video.h"

#title "Vircon32 BIOS Test"
#version 0.9

#texture bios_font "font.png"
#sound   startup_chime "chime.wav"

void error_handler() {
    select_texture(bios_font);
}

void main() {
    play_sound(startup_chime);
}
