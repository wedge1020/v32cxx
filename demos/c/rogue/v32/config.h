/*
 * config.h for the Vircon32 build of Rogue.
 *
 * Stands in for the config.h that autoconf generates on Unix. extern.h
 * includes it because rogue.c defines HAVE_CONFIG_H.
 */
#ifndef ROGUE_V32_CONFIG_H
#define ROGUE_V32_CONFIG_H

#define HAVE_CURSES_H   1
#define HAVE_STRING_H   1
#define HAVE_ERASECHAR  1
#define HAVE_KILLCHAR   1

/* the top-ten list (kept on the memory card, see v32/mach_v32.c) */
#define NUMSCORES   10
#define NUMNAME     "Ten"
#define ALLSCORES   1

/* wizard mode needs a password nobody can type */
#define PASSWD      "mTBellIQOsLNA"

#endif
