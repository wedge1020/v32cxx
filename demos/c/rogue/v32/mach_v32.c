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
 *   - the top-ten list lives on the memory card;
 *   - saving a game is not supported.
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
    v32term_init();
    v32term_key_source = rogue_pad_getkey;
    curses_ink_hook = rogue_ink;
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

/* ---- the top-ten list, on the memory card ---------------------------------
 *
 * A memory card starts with a 20-word signature saying which game its
 * data belongs to; the scores follow it. A card that is missing, or that
 * holds another game's data, is left alone: the list is then just this
 * session's score.
 */

#define SCORE_SIGNATURE_WORDS 20
static char score_signature[SCORE_SIGNATURE_WORDS] = "ROGUE 5.4.4 SCORES";
static char card_signature[SCORE_SIGNATURE_WORDS];

/*
 * score_card:
 *	Can the scores be kept on the card in the slot?  0 = no,
 *	1 = yes and it has a list, 2 = yes and it is blank.
 */
static int
score_card()
{
    int i;
    bool blank = TRUE;
    bool ours = TRUE;

    if (!card_is_connected())
	return 0;
    card_read_data(card_signature, 0, SCORE_SIGNATURE_WORDS);
    for (i = 0; i < SCORE_SIGNATURE_WORDS; i++)
    {
	if (card_signature[i] != 0)
	    blank = FALSE;
	if (card_signature[i] != score_signature[i])
	    ours = FALSE;
    }
    if (ours)
	return 1;
    if (blank)
	return 2;
    return 0;
}

void
open_score()
{
    scoreboard = NULL;
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

void
rd_score(SCORE *top_ten)
{
    if (score_card() == 1)
	card_read_data(top_ten, SCORE_SIGNATURE_WORDS, numscores * sizeof (SCORE));
}

void
wr_score(SCORE *top_ten)
{
    if (score_card() == 0)
	return;
    card_write_data(score_signature, 0, SCORE_SIGNATURE_WORDS);
    card_write_data(top_ten, SCORE_SIGNATURE_WORDS, numscores * sizeof (SCORE));
}

/* ---- saving --------------------------------------------------------------- */

void
save_game()
{
    msg("a game cannot be saved on this console");
    after = FALSE;
}

bool
restore(char *file, char **envp)
{
    NOOP(file);
    NOOP(envp);
    return FALSE;
}
