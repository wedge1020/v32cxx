/*
 * rogue.c -- Rogue 5.4.4 for the Vircon32 console, built with v32c++.
 *
 * Vircon32 C compiles exactly one file and has no linker, so this file
 * pools the whole program into one translation unit: the C library and
 * curses that v32c++ ships (libc/), Rogue's own sources as they were
 * written for Unix (src/), and the two files that stand in for Rogue's
 * machine-dependent layer (v32/).
 *
 *     v32c++ -I ../../../libc -I v32 -I src -o obj/rogue.c rogue.c
 *
 * See README.md for what was and was not changed in Rogue's sources.
 */

#title "Rogue"
#version 5.4.4

/* extern.h reads v32/config.h instead of guessing a Unix */
#define HAVE_CONFIG_H 1

/* The C library, the terminal and curses for Vircon32 */
#include <v32libc.c>
#include <v32curses.c>

/* Rogue, as written */
#include "src/vers.c"
#include "src/extern.c"
#include "src/armor.c"
#include "src/chase.c"
#include "src/command.c"
#include "src/daemon.c"
#include "src/daemons.c"
#include "src/fight.c"
#include "src/init.c"
#include "src/io.c"
#include "src/list.c"
#include "src/misc.c"
#include "src/monsters.c"
#include "src/move.c"
#include "src/new_level.c"
#include "src/options.c"
#include "src/pack.c"
#include "src/passages.c"
#include "src/potions.c"
#include "src/rings.c"
#include "src/rip.c"
#include "src/rooms.c"
#include "src/scrolls.c"
#include "src/sticks.c"
#include "src/things.c"
#include "src/weapons.c"
#include "src/wizard.c"

/* The Vircon32 side: what mach_dep.c, mdport.c and save.c were on Unix */
#include "v32/pad.c"
#include "v32/mach_v32.c"

#include "src/main.c"
