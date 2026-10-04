/*
 * mach_v32.c -- Rogue's machine-dependent layer, for the Vircon32 console.
 *
 * On Unix this was three files: mach_dep.c (start-up checks, the score
 * file, terminal modes, signals), mdport.c (the portability layer: user
 * name, home directory, shell escape, reading a key) and save.c (saving
 * and restoring a game, reading and writing the score file). A console
 * has no users, shell, signals or file system, so most of it has nothing
 * to do here. What remains:
 *
 *   - keys come from the gamepad (v32/pad.c);
 *   - the files are on the memory card (libc/v32file.c): the top-ten
 *     list and up to four saved games, read and written by Rogue's own
 *     save.c and state.c;
 *   - the program starts at a title screen (v32/title.c).
 *
 * Every function keeps the name and signature the rest of Rogue calls.
 */

#include <curses.h>
#include "memcard.h"
#include "rogue.h"
#include "score.h"

unsigned int numscores = NUMSCORES;
char *Numname = NUMNAME;
bool allscore = TRUE;

/* ---- start-up ------------------------------------------------------------- */

void
md_init()
{
    /* the console side is set up before main() starts: see
     * v32_main_args() in title.c */
}

void
init_check()
{
}

void
setup()
{
    raw();
    noecho();
    keypad(stdscr, 1);
    getltchars();
}

void
getltchars()
{
    got_ltc = TRUE;
}

void
resetltchars()
{
}

void
playltchars()
{
}

void
md_normaluser()
{
}

void
md_tstpsignal()
{
}

void
md_tstpresume()
{
}

int
md_hasclreol()
{
    /*
     * FALSE keeps the inventory as an overlay in the corner of the
     * map instead of clearing the whole screen for it.
     */
    return FALSE;
}

/* ---- who is playing ------------------------------------------------------- */

char *
md_getusername()
{
    return "Rogue";
}

char *
md_getrealname(int uid)
{
    NOOP(uid);
    return "Rogue";
}

char *
md_gethomedir()
{
    return "";
}

int
md_getuid()
{
    return 0;
}

/*
 * md_getpid:
 *	Rogue adds this to the time to seed its random numbers. The
 *	console's cycle counter is as unpredictable as a process id.
 */
int
md_getpid()
{
#ifdef V32LIBC_FIXED_TIME
    return 0;		/* a reproducible game, for testing */
#else
    return get_cycle_counter() + get_frame_counter() * 7919;
#endif
}

/*
 * md_getpass, md_crypt:
 *	Only the wizard's password uses these, and nobody can type it.
 */
char *
md_getpass(char *prompt)
{
    NOOP(prompt);
    return "";
}

char *
md_crypt(char *key, char *salt)
{
    NOOP(key);
    NOOP(salt);
    return "";
}

int
md_shellescape()
{
    printf("There is no shell on a Vircon32.\n");
    return 0;
}

/* ---- keyboard ------------------------------------------------------------- */

int
md_readchar()
{
    return getch();
}

int
md_erasechar()
{
    return erasechar();
}

int
md_killchar()
{
    return killchar();
}

void
flush_type()
{
    flushinp();
}

void
md_raw_standout()
{
    v32term_attr(V32TERM_REVERSE);
}

void
md_raw_standend()
{
    v32term_attr(0);
}

/* ---- files: the memory card ------------------------------------------------
 *
 * Rogue keeps two files, and libc keeps files on the memory card
 * (libc/v32file.c): the top-ten list, which Rogue holds open for the whole
 * session, and the saved game. Reading and writing them is Rogue's own
 * code (save.c, state.c). What is left for this layer is what was
 * Unix-specific about it.
 */

#define SCORE_FILE	"rogue.scr"

static bool saving = FALSE;	/* save_file() has started writing */

void
open_score()
{
    if (scoreboard != NULL)
    {
	rewind(scoreboard);
	return;
    }
    scoreboard = fopen(SCORE_FILE, "r+");
    if (scoreboard == NULL && errno == ENOENT)
	scoreboard = fopen(SCORE_FILE, "w+");
}

void
start_score()
{
}

bool
lock_sc()
{
    return TRUE;
}

void
unlock_sc()
{
}

/* ---- saved games: four slots -------------------------------------------------
 *
 * Rogue saves to a file whose name the player types. Here the name is
 * one of four, picked from a menu (v32/pad.c types it for the player, and
 * the title screen passes it to `rogue <file>`). A fifth small file says
 * what is in each slot, so the menus can show more than "used".
 */

static char *save_names[SAVE_SLOTS] = {
    "rogue.sv1", "rogue.sv2", "rogue.sv3", "rogue.sv4"
};

#define SAVE_INDEX	"rogue.idx"
#define NOTE_LEVEL	0	/* 0: nothing known about this slot */
#define NOTE_HP		1
#define NOTE_MAXHP	2
#define NOTE_GOLD	3
#define NOTE_WORDS	4

static int save_notes[SAVE_SLOTS][NOTE_WORDS];

char *
save_slot_name(int slot)
{
    return save_names[slot];
}

/*
 * save_slot_of:
 *	Which slot is this file name?  -1 if none.
 */
int
save_slot_of(char *name)
{
    int slot;

    for (slot = 0; slot < SAVE_SLOTS; slot++)
	if (strcmp(name, save_names[slot]) == 0)
	    return slot;
    return -1;
}

bool
save_slot_used(int slot)
{
    struct stat st;

    return stat(save_names[slot], &st) >= 0;
}

int
save_slots_used()
{
    int slot, used = 0;

    for (slot = 0; slot < SAVE_SLOTS; slot++)
	if (save_slot_used(slot))
	    used++;
    return used;
}

static void
save_notes_read()
{
    FILE *f;

    memset(save_notes, 0, SAVE_SLOTS * NOTE_WORDS);
    if ((f = fopen(SAVE_INDEX, "r")) == NULL)
	return;
    fread(save_notes, NOTE_WORDS, SAVE_SLOTS, f);
    fclose(f);
}

static void
save_notes_write()
{
    FILE *f;

    if ((f = fopen(SAVE_INDEX, "w")) == NULL)
	return;
    fwrite(save_notes, NOTE_WORDS, SAVE_SLOTS, f);
    fclose(f);
}

/*
 * save_slot_label:
 *	One line describing a slot, for a menu.
 */
void
save_slot_label(int slot, char *buf)
{
    int *note;

    save_notes_read();
    note = save_notes[slot];
    if (!save_slot_used(slot))
	sprintf(buf, "Slot %d   (empty)", slot + 1);
    else if (note[NOTE_LEVEL] == 0)
	sprintf(buf, "Slot %d   a saved game", slot + 1);
    else
	sprintf(buf, "Slot %d   Level %d   Hp %d(%d)   Gold %d", slot + 1,
	    note[NOTE_LEVEL], note[NOTE_HP], note[NOTE_MAXHP], note[NOTE_GOLD]);
}

/*
 * save_adopt_old:
 *	The first version with saved games had one, called rogue.save.
 *	It moves into the first free slot.
 */
void
save_adopt_old()
{
    struct stat st;
    int slot;

    if (stat("rogue.save", &st) < 0)
	return;
    for (slot = 0; slot < SAVE_SLOTS; slot++)
	if (!save_slot_used(slot))
	{
	    save_notes_read();
	    save_notes[slot][NOTE_LEVEL] = 0;
	    save_notes_write();
	    rename("rogue.save", save_names[slot]);
	    return;
	}
}

/*
 * md_chmod:
 *	Nothing to change on a memory card. save_file() calls this just
 *	before it writes the game out, which is how rogue_exit() below
 *	knows that leaving means "saved", not "quit" -- and the moment
 *	to note what is going into the slot.
 */
int
md_chmod(char *filename, int mode)
{
    int slot;

    NOOP(mode);
    if (strcmp(filename, file_name) != 0)
	return 0;
    saving = TRUE;
    if ((slot = save_slot_of(filename)) >= 0)
    {
	save_notes_read();
	save_notes[slot][NOTE_LEVEL] = level;
	save_notes[slot][NOTE_HP] = pstats.s_hpt;
	save_notes[slot][NOTE_MAXHP] = max_hp;
	save_notes[slot][NOTE_GOLD] = purse;
	save_notes_write();
    }
    return 0;
}

int
md_unlink(char *file)
{
    return unlink(file);
}

/*
 * md_unlink_open_file:
 *	restore() deletes the saved game once it has been read, so a
 *	game can only ever be resumed once. The open file is in memory
 *	by then, and stays readable.
 */
int
md_unlink_open_file(char *file, FILE *inf)
{
    NOOP(inf);
    return unlink(file);
}

bool
is_symlink(char *sp)
{
    NOOP(sp);
    return FALSE;
}

void
md_ignoreallsignals()
{
}

void
md_tstphold()
{
}

/* ---- leaving ----------------------------------------------------------------- */

/*
 * rogue_exit:
 *	exit() comes here first (v32_exit_hook). After it, libc waits
 *	for START and restarts the cartridge, which brings the title
 *	screen back.
 */
void
rogue_exit(int status)
{
    struct stat st;

    NOOP(status);
    if (!saving)
	return;
    if (stat(file_name, &st) >= 0)
    {
	v32term_attr(V32TERM_INK(V32INK_GREEN));
	printf("\nGame saved on the memory card, in slot %d.\n", save_slot_of(file_name) + 1);
    }
    else
    {
	v32term_attr(V32TERM_INK(V32INK_RED));
	printf("\nThe game could not be saved: %s.\n", strerror(errno));
    }
    v32term_attr(0);
}
