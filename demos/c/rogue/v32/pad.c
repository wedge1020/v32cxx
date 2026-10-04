/*
 * pad.c -- playing Rogue with a gamepad.
 *
 * Rogue reads its keyboard one character at a time, and uses most of it:
 * some forty commands, a letter to pick an item from the pack, a
 * direction for a wand, y or n, and now and then a line of text. A
 * Vircon32 gamepad has a D-pad and seven buttons. This file is the
 * function Rogue's readchar() ends up in (through curses' getch(), see
 * v32term_key_source), and it answers each request for a character the
 * way that suits what Rogue is asking at that moment:
 *
 *   exploring          D-pad moves (two directions at once: diagonal)
 *                      L + D-pad   run
 *                      R + D-pad   fight to the death in that direction
 *                      A           search -- or take the stairs, when
 *                                  standing on them
 *                      R + A       search ten times
 *                      X           inventory
 *                      B           menu: things to do with an item
 *                      Y           menu: every other command
 *                      START       on-screen keyboard: type any key
 *   --More--           any button continues
 *   "which object..."  a list of what is in the pack to choose from
 *   "which direction"  the D-pad answers
 *   y/n, left/right    a two-line menu
 *   anything else that waits at a prompt, and text: the on-screen keyboard
 *
 * How does it know what Rogue is asking? Rogue's sources are not changed
 * to tell it. It reads the screen, as a player would: the prompt is
 * right there on the message line, with the cursor after it.
 */

#include <curses.h>
#include "rogue.h"

/* ---- characters waiting to be handed to Rogue ---------------------------- */

#define PAD_QUEUE 16
static char pad_queue[PAD_QUEUE];
static int pad_queued = 0;

static void
pad_push(char *keys)
{
    while (*keys != '\0' && pad_queued < PAD_QUEUE)
	pad_queue[pad_queued++] = *keys++;
}

static int
pad_pop()
{
    int i, c;

    c = pad_queue[0];
    pad_queued--;
    for (i = 0; i < pad_queued; i++)
	pad_queue[i] = pad_queue[i + 1];
    return c;
}

/* ---- reading the screen --------------------------------------------------- */

static char pad_line[V32TERM_COLS + 1];

/*
 * pad_read_line:
 *	The text on screen row y, without trailing blanks, in pad_line.
 */
static char *
pad_read_line(int y)
{
    int x, end = 0;

    for (x = 0; x < V32TERM_COLS; x++)
    {
	pad_line[x] = (char) (v32term_get(y, x) & V32TERM_CHAR);
	if (pad_line[x] != ' ')
	    end = x + 1;
    }
    pad_line[end] = '\0';
    return pad_line;
}

/*
 * pad_on_screen:
 *	Is this text anywhere on the screen?
 */
static bool
pad_on_screen(char *text)
{
    return v32term_find(text, 0, V32TERM_ROWS - 1);
}

/* ---- pop-up menus --------------------------------------------------------- */

#define PAD_MENU_MAX	32
#define PAD_LABEL_LEN	72
#define PAD_MENU_ROWS	18	/* entries visible at once */

static char pad_labels[PAD_MENU_MAX][PAD_LABEL_LEN];
static int pad_saved[V32TERM_CELLS];

/*
 * pad_menu:
 *	Show pad_labels[0..count-1] under a title and let the player
 *	pick one. Returns its index, or -1 if cancelled with B.
 */
static int
pad_menu(char *title, int count, int start)
{
    int i, width, height, top, left, first, sel, attr, len;
    int choice = -2;

    width = (int) strlen(title) + 4;
    for (i = 0; i < count; i++)
    {
	len = (int) strlen(pad_labels[i]);
	if (len + 4 > width)
	    width = len + 4;
    }
    if (width > V32TERM_COLS)
	width = V32TERM_COLS;
    height = (count < PAD_MENU_ROWS ? count : PAD_MENU_ROWS) + 2;
    top = (V32TERM_ROWS - height) / 2;
    left = (V32TERM_COLS - width) / 2;
    sel = start;
    first = 0;

    v32term_save(pad_saved);
    while (choice == -2)
    {
	if (sel < first)
	    first = sel;
	if (sel >= first + PAD_MENU_ROWS)
	    first = sel - PAD_MENU_ROWS + 1;
	v32term_box(top, left, height, width, V32TERM_INK(V32INK_CYAN));
	v32term_text(top, left + 2, title, V32TERM_INK(V32INK_YELLOW));
	if (first > 0)
	    v32term_text(top, left + width - 4, "^^", V32TERM_INK(V32INK_YELLOW));
	if (first + PAD_MENU_ROWS < count)
	    v32term_text(top + height - 1, left + width - 4, "vv", V32TERM_INK(V32INK_YELLOW));
	for (i = first; i < count && i < first + PAD_MENU_ROWS; i++)
	{
	    attr = V32TERM_INK(V32INK_WHITE);
	    if (i == sel)
		attr = V32TERM_REVERSE | V32TERM_INK(V32INK_YELLOW);
	    v32term_fill(top + 1 + i - first, left + 1, 1, width - 2, ' ' | attr);
	    v32term_text(top + 1 + i - first, left + 2, pad_labels[i], attr);
	}
	v32pad_poll();
	if (v32pad_hit(V32PAD_UP))
	    sel = (sel + count - 1) % count;
	if (v32pad_hit(V32PAD_DOWN))
	    sel = (sel + 1) % count;
	if (v32pad_hit(V32PAD_A))
	    choice = sel;
	if (v32pad_hit(V32PAD_B))
	    choice = -1;
    }
    v32term_restore(pad_saved);
    v32term_flush();
    return choice;
}

/* ---- the two command menus ------------------------------------------------ */

struct pad_command {
    char *pc_label;
    char *pc_keys;
};

static struct pad_command pad_item_cmds[] = {
    { "Eat",			"e" },
    { "Quaff a potion",		"q" },
    { "Read a scroll",		"r" },
    { "Wield a weapon",		"w" },
    { "Wear armor",		"W" },
    { "Take armor off",		"T" },
    { "Put a ring on",		"P" },
    { "Remove a ring",		"R" },
    { "Throw",			"t" },
    { "Zap a wand or staff",	"z" },
    { "Drop",			"d" },
    { "Pick up what is here",	"," },
    { "Call an item something",	"c" },
};

static struct pad_command pad_other_cmds[] = {
    { "Rest a turn",			"." },
    { "Search ten times",		"10s" },
    { "Go down the stairs",		">" },
    { "Go up the stairs",		"<" },
    { "Repeat the last command",	"a" },
    { "Move onto, without picking up",	"m" },
    { "Identify a trap",		"^" },
    { "Show what you are wielding",	")" },
    { "Show what you are wearing",	"]" },
    { "Show your rings",		"=" },
    { "Show your stats",		"@" },
    { "Inventory: one item",		"I" },
    { "What has been discovered",	"D" },
    { "Show the last message again",	"\020" },
    { "What is that symbol?",		"/" },
    { "Help",				"?" },
    { "Options",			"o" },
    { "Redraw the screen",		"\022" },
    { "Version",			"v" },
    { "Quit",				"Q" },
};

#define NITEMCMDS	(sizeof pad_item_cmds / sizeof (struct pad_command))
#define NOTHERCMDS	(sizeof pad_other_cmds / sizeof (struct pad_command))

static int pad_item_last = 0;
static int pad_other_last = 0;

/*
 * pad_command_menu:
 *	Show one of the two menus; queue the chosen command's keys.
 *	Returns FALSE if the player backed out.
 */
static bool
pad_command_menu(char *title, struct pad_command *cmds, int count, int *last)
{
    int i, choice;

    for (i = 0; i < count; i++)
	strcpy(pad_labels[i], cmds[i].pc_label);
    choice = pad_menu(title, count, *last);
    if (choice < 0)
	return FALSE;
    *last = choice;
    pad_push(cmds[choice].pc_keys);
    return TRUE;
}

/* ---- picking an item from the pack ---------------------------------------- */

/*
 * pad_item_type:
 *	Which kind of item is Rogue's "which object do you want to..."
 *	prompt after?  0 means any.
 */
static int
pad_item_type(char *prompt)
{
    if (strstr(prompt, "eat") != NULL)
	return FOOD;
    if (strstr(prompt, "quaff") != NULL)
	return POTION;
    if (strstr(prompt, "read") != NULL)
	return SCROLL;
    if (strstr(prompt, "wear") != NULL)
	return ARMOR;
    if (strstr(prompt, "put on") != NULL)
	return RING;
    if (strstr(prompt, "zap") != NULL || strstr(prompt, "charge") != NULL)
	return STICK;
    if (strstr(prompt, "wield") != NULL || strstr(prompt, "throw") != NULL)
	return WEAPON;
    return 0;
}

/*
 * pad_pick_item:
 *	A menu of the pack. Returns the chosen item's pack letter, or
 *	ESCAPE.
 */
static int
pad_pick_item(char *prompt)
{
    static char letters[PAD_MENU_MAX];
    static char title[PAD_LABEL_LEN];
    THING *obj;
    int type, count, choice;
    char *name;

    strncpy(title, prompt, PAD_LABEL_LEN - 1);
    title[PAD_LABEL_LEN - 1] = '\0';
    name = strstr(title, "? (*");
    if (name != NULL)
	name[1] = '\0';

    type = pad_item_type(prompt);
    for (;;)
    {
	count = 0;
	for (obj = pack; obj != NULL && count < PAD_MENU_MAX - 1; obj = next(obj))
	{
	    if (type != 0 && obj->o_type != type)
		continue;
	    name = inv_name(obj, FALSE);
	    pad_labels[count][0] = obj->o_packch;
	    pad_labels[count][1] = ')';
	    pad_labels[count][2] = ' ';
	    strncpy(&pad_labels[count][3], name, PAD_LABEL_LEN - 4);
	    pad_labels[count][PAD_LABEL_LEN - 1] = '\0';
	    letters[count] = obj->o_packch;
	    count++;
	}
	if (type != 0 && count == 0)
	{
	    /* nothing of the usual kind: show the whole pack instead */
	    type = 0;
	    continue;
	}
	if (type != 0)
	{
	    /* anything can be wielded or thrown; let the player see it all */
	    strcpy(pad_labels[count], "(show everything)");
	    letters[count] = '\0';
	    count++;
	}
	if (count == 0)
	    return ESCAPE;
	choice = pad_menu(title, count, 0);
	if (choice < 0)
	    return ESCAPE;
	if (letters[choice] != '\0')
	    return letters[choice];
	type = 0;
    }
}

/* ---- directions ----------------------------------------------------------- */

#define PAD_CHORD_FRAMES 3	/* how long a second direction may lag the first */

/*
 * pad_direction:
 *	If the D-pad was just pressed (or is repeating), the Rogue
 *	movement key for where it points; 0 otherwise. Two directions
 *	held together make a diagonal; a fresh press waits a few frames
 *	for the second one to arrive.
 */
static int
pad_direction()
{
    static char keys[3][3] = {
	{ 'y', 'k', 'u' },
	{ 'h',  0 , 'l' },
	{ 'b', 'j', 'n' }
    };
    int dx, dy, i;
    bool fresh = FALSE;
    bool hit = FALSE;

    for (i = V32PAD_LEFT; i <= V32PAD_DOWN; i++)
	if (v32pad_hit(i))
	{
	    hit = TRUE;
	    if (v32pad_held_frames(i) == 1)
		fresh = TRUE;
	}
    if (!hit)
	return 0;
    if (fresh)
	for (i = 0; i < PAD_CHORD_FRAMES; i++)
	    v32pad_poll();
    dx = v32pad_down(V32PAD_RIGHT) - v32pad_down(V32PAD_LEFT);
    dy = v32pad_down(V32PAD_DOWN) - v32pad_down(V32PAD_UP);
    return keys[dy + 1][dx + 1];
}

/* ---- waiting for any button ------------------------------------------------ */

static void
pad_wait_button()
{
    for (;;)
    {
	v32pad_poll();
	if (v32pad_any_hit() || pad_direction() != 0)
	    return;
    }
}

/* ---- the key source ------------------------------------------------------- */

/*
 * pad_explore:
 *	The player is walking around the dungeon: wait for a command.
 */
static int
pad_explore()
{
    int key;

    for (;;)
    {
	if (pad_queued > 0)
	    return pad_pop();
	v32pad_poll();
	key = pad_direction();
	if (key != 0)
	{
	    if (v32pad_down(V32PAD_L))
		return toupper(key);		/* run */
	    if (v32pad_down(V32PAD_R))
	    {
		pad_queue[0] = (char) key;	/* fight: F, then the direction */
		pad_queued = 1;
		return 'F';
	    }
	    return key;
	}
	if (v32pad_hit(V32PAD_A))
	{
	    if (v32pad_down(V32PAD_R))
		pad_push("10s");
	    else if (chat(hero.y, hero.x) == STAIRS)
		return amulet ? '<' : '>';
	    else
		return 's';
	}
	if (v32pad_hit(V32PAD_X))
	    return 'i';
	if (v32pad_hit(V32PAD_B))
	    pad_command_menu("Items", pad_item_cmds, NITEMCMDS, &pad_item_last);
	if (v32pad_hit(V32PAD_Y))
	    pad_command_menu("Commands", pad_other_cmds, NOTHERCMDS, &pad_other_last);
	if (v32pad_hit(V32PAD_START))
	    return v32osk_getkey_titled("Type a command");
    }
}

/*
 * rogue_pad_getkey:
 *	Rogue wants a character.
 */
int
rogue_pad_getkey()
{
    static char prompt[V32TERM_COLS + 1];
    int key, len;

    if (pad_queued > 0)
	return pad_pop();

    /*
     * Out of curses: Rogue is printing the score list, or asking to
     * press return.
     */
    if (isendwin())
    {
	pad_wait_button();
	return '\n';
    }

    if (v32term_find("--More--", 0, 0))
    {
	for (;;)
	{
	    v32pad_poll();
	    if (v32pad_hit(V32PAD_B) && msg_esc)
		return ESCAPE;
	    if (v32pad_any_hit() || pad_direction() != 0)
		return ' ';
	}
    }
    /*
     * The next two only come up in a window drawn over the map (the
     * inventory, a list of discoveries, the options): no need to
     * search the whole screen for them while the map is showing.
     */
    if (curses_shown != stdscr && pad_on_screen("--Press space to continue--"))
    {
	pad_wait_button();
	return ' ';
    }
    if (v32term_find("[Press return to continue]", V32TERM_ROWS - 1, V32TERM_ROWS - 1))
    {
	pad_wait_button();
	return '\n';
    }
    /* the options screen is a form: every answer is typed */
    if (curses_shown != stdscr && pad_on_screen("Inventory style"))
	return v32osk_getkey_titled("Options: Enter next, - back, Esc done");

    strcpy(prompt, pad_read_line(0));
    len = (int) strlen(prompt);

    if (strstr(prompt, "(* for list)") != NULL)
	return pad_pick_item(prompt);

    if (strstr(prompt, "direction") != NULL)
    {
	for (;;)
	{
	    v32pad_poll();
	    key = pad_direction();
	    if (key != 0)
		return key;
	    if (v32pad_hit(V32PAD_B))
		return ESCAPE;
	    if (v32pad_hit(V32PAD_START))
		return v32osk_getkey_titled(prompt);
	}
    }

    /* (msg() capitalizes a message's first letter: match without it) */
    if (strstr(prompt, "eally quit?") != NULL)
    {
	strcpy(pad_labels[0], "No, keep playing");
	strcpy(pad_labels[1], "Yes, quit");
	return pad_menu("Really quit?", 2, 0) == 1 ? 'y' : 'n';
    }

    if (strstr(prompt, "eft hand or right hand") != NULL
	|| strstr(prompt, "eft or right ring") != NULL)
    {
	strcpy(pad_labels[0], "Left hand");
	strcpy(pad_labels[1], "Right hand");
	key = pad_menu("Which hand?", 2, 0);
	if (key < 0)
	    return ESCAPE;
	return key == 0 ? 'l' : 'r';
    }

    /*
     * Any other question Rogue is waiting on, and any text being
     * typed after one: the keyboard, with the message line as its
     * title. How to tell a question ("what do you want to call it? ")
     * from a remark ("Why would you want to drink that?"): the
     * cursor. Rogue leaves it on the message line when it wants an
     * answer typed there, and puts it back on the hero before it
     * reads a command.
     */
    if (len > 0 && stdscr != NULL && stdscr->_cury == 0)
	return v32osk_getkey_titled(prompt);

    return pad_explore();
}

/* ---- colour --------------------------------------------------------------- */

/*
 * rogue_ink:
 *	Rogue was written for one-colour terminals. curses asks this
 *	function what colour each character of a window should be as it
 *	goes to the screen (curses_ink_hook), so the dungeon can have
 *	some without Rogue's drawing code knowing.
 */
int
rogue_ink(WINDOW *win, int y, int x, int ch)
{
    NOOP(x);
    if (win != stdscr || y < 1)
	return -1;
    /* only while the map is what stdscr shows: the hero is on it */
    if ((stdscr->_cells[hero.y * stdscr->_stride + hero.x] & A_CHARTEXT) != PLAYER)
	return -1;
    if (y == STATLINE)
	return V32INK_CYAN;
    switch (ch)
    {
	case PLAYER:
	    return V32INK_YELLOW;
	case '|':
	case '-':
	    return V32INK_BROWN;
	case DOOR:
	    return V32INK_ORANGE;
	case FLOOR:
	    return V32INK_DKGREEN;
	case PASSAGE:
	    return V32INK_GRAY;
	case STAIRS:
	    return V32INK_GREEN;
	case TRAP:
	    return V32INK_MAGENTA;
	case GOLD:
	case AMULET:
	    return V32INK_YELLOW;
	case POTION:
	case SCROLL:
	case RING:
	case STICK:
	case MAGIC:
	    return V32INK_CYAN;
	case FOOD:
	    return V32INK_RED;
	case WEAPON:
	case ARMOR:
	    return V32INK_BLUE;
    }
    if (isupper(ch))
	return V32INK_WHITE;
    return -1;
}
