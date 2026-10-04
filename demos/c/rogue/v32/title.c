/*
 * title.c -- the title screen.
 *
 * On Unix Rogue is started from a shell, and what it does is decided by
 * its command line: `rogue` plays, `rogue -r` restores the saved game,
 * `rogue -s` prints the top ten. A console has no command line, so this
 * is one: v32c++ calls v32_main_args() at the top of main(), the player
 * picks from a menu, and main() gets the argv that choice stands for.
 * Rogue's main() does the rest, exactly as it would on Unix.
 *
 * The banner is drawn with the BIOS font's shade blocks (characters 17 to
 * 20: three dither patterns and a solid block).
 */

#include <curses.h>
#include "rogue.h"

void rogue_exit(int status);

#define TITLE_NEW	0
#define TITLE_RESUME	1
#define TITLE_SCORES	2
#define TITLE_HELP	3
#define TITLE_ITEMS	4

static char *title_items[TITLE_ITEMS] = {
    "New game",
    "Resume the saved game",
    "Hall of fame",
    "Instructions",
};

/* ---- the banner ------------------------------------------------------------ */

#define BANNER_ROWS	6
#define BANNER_TOP	3
#define BANNER_LEFT	15

static char *title_banner[BANNER_ROWS] = {
    "#######    ######    ######   ##    ##  ########",
    "##    ##  ##    ##  ##        ##    ##  ##      ",
    "##    ##  ##    ##  ##  ####  ##    ##  ######  ",
    "#######   ##    ##  ##    ##  ##    ##  ##      ",
    "##  ##    ##    ##  ##    ##  ##    ##  ##      ",
    "##   ###   ######    ######    ######   ########",
};

/*
 * title_band:
 *	One row of a shade block across the screen.
 */
static void
title_band(int y, int shade, int ink)
{
    v32term_fill(y, 0, 1, V32TERM_COLS, shade | V32TERM_INK(ink));
}

static void
title_draw_banner()
{
    int y, x;
    char *row;

    title_band(0, V32TERM_SHADE_DARK, V32INK_BROWN);
    title_band(1, V32TERM_SHADE_HALF, V32INK_BROWN);
    title_band(2, V32TERM_SHADE_LIGHT, V32INK_BROWN);
    title_band(BANNER_TOP + BANNER_ROWS + 1, V32TERM_SHADE_LIGHT, V32INK_BROWN);
    title_band(BANNER_TOP + BANNER_ROWS + 2, V32TERM_SHADE_HALF, V32INK_BROWN);

    /* the letters' shadow first, one cell down and right, then the letters */
    for (y = 0; y < BANNER_ROWS; y++)
    {
	row = title_banner[y];
	for (x = 0; row[x] != '\0'; x++)
	    if (row[x] == '#')
		v32term_set(BANNER_TOP + y + 1, BANNER_LEFT + x + 1,
		    V32TERM_SHADE_HALF | V32TERM_INK(V32INK_ORANGE));
    }
    for (y = 0; y < BANNER_ROWS; y++)
    {
	row = title_banner[y];
	for (x = 0; row[x] != '\0'; x++)
	    if (row[x] == '#')
		v32term_set(BANNER_TOP + y, BANNER_LEFT + x,
		    V32TERM_SHADE_SOLID | V32TERM_INK(V32INK_YELLOW));
    }
}

/*
 * title_center:
 *	Text in the middle of a row.
 */
static void
title_center(int y, char *text, int attr)
{
    v32term_text(y, (V32TERM_COLS - (int) strlen(text)) / 2, text, attr);
}

/* ---- the memory card --------------------------------------------------------- */

/*
 * The first version of this port kept the top ten on the card as raw
 * data under this signature. Such a card is this game's own, with
 * nothing on it but that list: it is turned into a card with files.
 */
static char title_old_signature[20] = "ROGUE 5.4.4 SCORES";

static void
title_adopt_old_card()
{
    char on_card[20];
    int i;

    if (!card_is_connected())
	return;
    card_read_data(on_card, 0, 20);
    for (i = 0; i < 20; i++)
	if (on_card[i] != title_old_signature[i])
	    return;
    v32file_format();
}

static bool
title_have_save()
{
    struct stat st;

    return stat("rogue.save", &st) >= 0;
}

static void
title_card_line(int y)
{
    char *text = "";
    int ink = V32INK_GRAY;

    switch (v32file_card())
    {
	case V32FILE_NO_CARD:
	    text = "No memory card: games cannot be saved, scores will not be kept";
	    ink = V32INK_ORANGE;
	    break;
	case V32FILE_FOREIGN:
	    text = "The memory card holds another game's data: it will not be used";
	    ink = V32INK_ORANGE;
	    break;
	default:
	    text = title_have_save() ? "Memory card: one saved game" : "Memory card: no saved game";
    }
    v32term_fill(y, 0, 1, V32TERM_COLS, ' ');
    title_center(y, text, V32TERM_INK(ink));
}

/* ---- instructions -------------------------------------------------------------- */

#define HELP_PAGES	3
#define HELP_LINES	16

static char *title_help[HELP_PAGES][HELP_LINES] = {
    {
	"THE DUNGEONS OF DOOM",
	"",
	"You are @. Find the Amulet of Yendor, somewhere below level 25,",
	"and bring it back up alive. Each level is nine rooms joined by",
	"passages; the stairs (%) lead down.",
	"",
	"Walk into a monster (a letter) to fight it. Walk over a thing",
	"to pick it up:",
	"",
	"    !  potion      ?  scroll      :  food       *  gold",
	"    )  weapon      ]  armor       =  ring       /  wand or staff",
	"    ^  trap        +  door        #  passage    %  stairs",
	"",
	"Potions, scrolls, rings and wands are unknown until tried.",
	"You get hungry: eat. Hit points come back with time.",
	"When you die, that is the end. There is one life.",
    },
    {
	"CONTROLS",
	"",
	"    D-pad            move  (two directions at once: diagonal)",
	"    L + D-pad        run",
	"    R + D-pad        fight to the death in that direction",
	"    A                search for hidden doors and traps;",
	"                     on a staircase: take the stairs",
	"    R + A            search ten times",
	"    X                inventory",
	"    B                things to do with an item: eat, quaff, read,",
	"                     wield, wear, throw, zap, drop, pick up...",
	"    Y                every other command: rest, stairs, save,",
	"                     discoveries, help, options, quit...",
	"    START            keyboard: type any Rogue key directly",
	"",
	"In menus: up and down, A picks, B cancels.",
    },
    {
	"SAVING",
	"",
	"Y, then \"Save the game and stop\", writes the game to the",
	"memory card and returns here.",
	"",
	"\"Resume the saved game\" picks it up where you left it -- and",
	"erases it from the card. A saved game can be resumed once:",
	"saving is for taking a break, not for a second chance.",
	"",
	"The ten best scores are kept on the memory card too.",
	"",
	"",
	"Rogue: Exploring the Dungeons of Doom",
	"Copyright (C) 1980-1983, 1985, 1999",
	"Michael Toy, Ken Arnold and Glenn Wichman",
	"Vircon32 version built with v32c++",
    },
};

static void
title_instructions()
{
    int page = 0, i;
    char footer[V32TERM_COLS + 1];

    for (;;)
    {
	v32term_clear();
	title_band(0, V32TERM_SHADE_HALF, V32INK_BROWN);
	title_band(V32TERM_ROWS - 3, V32TERM_SHADE_LIGHT, V32INK_BROWN);
	for (i = 0; i < HELP_LINES; i++)
	    v32term_text(2 + i, 7, title_help[page][i],
		V32TERM_INK(i == 0 ? V32INK_YELLOW : V32INK_WHITE));
	sprintf(footer, "Page %d of %d      A or right: next      left: back      B: title screen",
	    page + 1, HELP_PAGES);
	title_center(V32TERM_ROWS - 1, footer, V32TERM_INK(V32INK_CYAN));
	for (;;)
	{
	    v32pad_poll();
	    if (v32pad_hit(V32PAD_B) || v32pad_hit(V32PAD_START))
		return;
	    if (v32pad_hit(V32PAD_A) || v32pad_hit(V32PAD_RIGHT))
	    {
		if (page == HELP_PAGES - 1)
		    return;
		page++;
		break;
	    }
	    if (v32pad_hit(V32PAD_LEFT) && page > 0)
	    {
		page--;
		break;
	    }
	}
    }
}

/* ---- the menu ---------------------------------------------------------------- */

#define MENU_TOP	14
#define MENU_LEFT	28
#define MENU_WIDTH	25

/*
 * title_menu:
 *	Show the title screen and return what the player picked.
 */
static int
title_menu()
{
    int sel, i, attr;
    bool have_save;
    bool redraw = TRUE;

    sel = title_have_save() ? TITLE_RESUME : TITLE_NEW;
    for (;;)
    {
	have_save = title_have_save();
	if (redraw)
	{
	    v32term_clear();
	    title_draw_banner();
	    title_center(12, "Exploring the Dungeons of Doom", V32TERM_INK(V32INK_WHITE));
	    title_center(V32TERM_ROWS - 3, "Rogue 5.4.4  (C) 1980-1985, 1999 M. Toy, K. Arnold, G. Wichman",
		V32TERM_INK(V32INK_GRAY));
	    title_center(V32TERM_ROWS - 1, "Up / down: choose        A or START: select",
		V32TERM_INK(V32INK_CYAN));
	    redraw = FALSE;
	}
	for (i = 0; i < TITLE_ITEMS; i++)
	{
	    attr = V32TERM_INK(V32INK_WHITE);
	    if (i == TITLE_RESUME && !have_save)
		attr = V32TERM_INK(V32INK_DIM);
	    if (i == sel)
		attr = V32TERM_REVERSE | V32TERM_INK(V32INK_YELLOW);
	    v32term_fill(MENU_TOP + i, MENU_LEFT, 1, MENU_WIDTH, ' ' | attr);
	    v32term_text(MENU_TOP + i, MENU_LEFT + 2, title_items[i], attr);
	}
	title_card_line(V32TERM_ROWS - 5);

	v32pad_poll();
	if (v32pad_hit(V32PAD_UP))
	    do
		sel = (sel + TITLE_ITEMS - 1) % TITLE_ITEMS;
	    while (sel == TITLE_RESUME && !have_save);
	if (v32pad_hit(V32PAD_DOWN))
	    do
		sel = (sel + 1) % TITLE_ITEMS;
	    while (sel == TITLE_RESUME && !have_save);
	if (sel == TITLE_RESUME && !have_save)
	    sel = TITLE_NEW;		/* the card was taken out */
	if (v32pad_hit(V32PAD_A) || v32pad_hit(V32PAD_START))
	{
	    if (sel != TITLE_HELP)
		return sel;
	    title_instructions();
	    redraw = TRUE;
	}
    }
}

/*
 * v32_main_args:
 *	Called by main() before anything else (v32c++ arranges that for
 *	a C program that defines this function). Sets the console up,
 *	shows the title screen, and returns the command line the
 *	player's choice stands for.
 */
char **
v32_main_args(int *argc, char **argv)
{
    static char *args[3];
    int choice;

    v32file_signature = "ROGUE 5.4.4";
    v32term_init();
    v32term_key_source = rogue_pad_getkey;
    curses_ink_hook = rogue_ink;
    v32_exit_hook = rogue_exit;
    v32_exit_prompt = "\n[Press START for the title screen]";
    title_adopt_old_card();

    choice = title_menu();
    v32term_clear();
    v32term_goto(0, 0);

    args[0] = argv[0];
    args[1] = NULL;
    args[2] = NULL;
    *argc = 1;
    if (choice == TITLE_RESUME)
    {
	args[1] = "-r";
	*argc = 2;
    }
    else if (choice == TITLE_SCORES)
    {
	args[1] = "-s";
	*argc = 2;
    }
#ifdef ROGUE_TEST_DEATH
    else
    {
	/* for testing the score list: `rogue -d` dies at once, with gold */
	args[1] = "-d";
	*argc = 2;
    }
#endif
    return args;
}
