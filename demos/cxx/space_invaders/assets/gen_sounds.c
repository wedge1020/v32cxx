/*
 * gen_sounds.c -- retro Space Invaders sound effects, as .wav files.
 *
 * Build:  cc -o gen_sounds gen_sounds.c -lm
 * Run:    ./gen_sounds
 *
 * Writes 14 WAVs (16-bit PCM mono, 22050 Hz) whose CART ORDER matches
 * si::AssetIds::Sounds exactly:
 *
 *   id  file               what
 *   --  -----------------  ----------------------------------------
 *    0  shoot.wav          player fire: fast downward square sweep
 *    1  alien_death.wav    alien hit: crunchy noise burst
 *    2  player_death.wav   player explodes: long noise + falling tone
 *  3-9  march0..6.wav      7 descending march tones (swarm speeds up)
 *   10  saucer.wav         UFO warble (loopable: it is a steady LFO)
 *   11  saucer_death.wav   UFO destroyed: noise crash
 *   12  extra_life.wav     bonus fanfare: rising sine arpeggio
 *   13  bunker_hit.wav     shield chip: short muffled thud
 *   14  menu_move.wav      title menu: cursor moved (short blip)
 *   15  menu_select.wav    title menu: game started (two-tone confirm)
 *   16  music_title.wav    looping title theme (march-like, 8 notes)
 *   17  music_game.wav     looping gameplay theme (ominous bass ostinato)
 *
 * All synthesis is deterministic (own LCG for noise), so output is
 * reproducible. Keep everything 16-bit MONO -- one word per sample.
 *
 * CART HINT WIRING (macro-name note at the bottom of this comment):
 *
 *   #sound WAV_SHOOT        "sounds/shoot.wav"
 *   #sound WAV_ALIEN_DEATH  "sounds/alien_death.wav"
 *   #sound WAV_PLAYER_DEATH "sounds/player_death.wav"
 *   #sound WAV_MARCH0       "sounds/march0.wav"
 *   #sound WAV_MARCH1       "sounds/march1.wav"
 *   #sound WAV_MARCH2       "sounds/march2.wav"
 *   #sound WAV_MARCH3       "sounds/march3.wav"
 *   #sound WAV_MARCH4       "sounds/march4.wav"
 *   #sound WAV_MARCH5       "sounds/march5.wav"
 *   #sound WAV_MARCH6       "sounds/march6.wav"
 *   #sound WAV_SAUCER       "sounds/saucer.wav"
 *   #sound WAV_SAUCER_DEATH "sounds/saucer_death.wav"
 *   #sound WAV_EXTRA_LIFE   "sounds/extra_life.wav"
 *   #sound WAV_BUNKER_HIT   "sounds/bunker_hit.wav"
 *   #sound WAV_MENU_MOVE    "sounds/menu_move.wav"
 *   #sound WAV_MENU_SELECT  "sounds/menu_select.wav"
 *   #sound WAV_MUSIC_TITLE  "sounds/music_title.wav"
 *   #sound WAV_MUSIC_GAME   "sounds/music_game.wav"
 *
 * Hint order == cart sound id == AssetIds::Sounds value. The hint macro
 * names deliberately do NOT reuse the enum names (WAV_*, not
 * SOUND_*): v32c++ emits `#define NAME id` per hint, and a macro named
 * SOUND_SHOOT would rewrite the enum entry `SOUND_SHOOT = 0` in the
 * generated C into `0 = 0` -- a syntax error.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define RATE 22050
#define PI   3.14159265358979323846

/* ---- little-endian WAV output ------------------------------------------ */

static void put_u32(FILE *f, unsigned long v) {
    fputc((int)(v        & 0xFF), f);
    fputc((int)((v >> 8)  & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

static void put_u16(FILE *f, unsigned v) {
    fputc((int)(v       & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}

static void write_wav(const char *name, int n, const short *samples) {
    FILE *f = fopen(name, "wb");
    if (!f) { perror(name); exit(1); }

    fputs("RIFF", f);
    put_u32(f, 36 + (unsigned long)n * 2);  /* rest of file   */
    fputs("WAVE", f);
    fputs("fmt ", f);
    put_u32(f, 16);            /* PCM fmt chunk size          */
    put_u16(f, 1);             /* format: PCM                 */
    put_u16(f, 1);             /* channels: mono              */
    put_u32(f, RATE);          /* sample rate                 */
    put_u32(f, RATE * 2);      /* byte rate                   */
    put_u16(f, 2);             /* block align                 */
    put_u16(f, 16);            /* bits per sample             */
    fputs("data", f);
    put_u32(f, (unsigned long)n * 2);

    for (int i = 0; i < n; ++i) {
        int s = samples[i];               /* clamp, then write */
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        put_u16(f, (unsigned)(s & 0xFFFF));
    }
    fclose(f);
    printf("wrote %s (%d samples, %.2fs)\n", name, n, (double)n / RATE);
}

/* ---- synthesis helpers -------------------------------------------------- */

static double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

/* deterministic noise in [-1, 1] */
static unsigned g_seed = 0x1234ABCD;
static double noise(void) {
    g_seed = g_seed * 1664525u + 1013904223u;
    return ((g_seed >> 16) & 0x7FFF) / 16384.0 - 1.0;
}

/* square wave from a phase in cycles */
static double square(double phase) {
    double frac = phase - floor(phase);
    return frac < 0.5 ? 1.0 : -1.0;
}

/* one sound = one array built sample by sample */
#define MAX_SAMPLES (RATE * 8)
static short buf[MAX_SAMPLES];

/* ---- tiny note sequencer (used by the music tracks) ---------------------
 * Renders 'count' notes of 'note_dur' seconds each from a frequency
 * table (Hz, 0 = rest) as a square wave with a soft 50/50 duty trim,
 * then returns the sample count. Loop-friendly: the last note's release
 * ends exactly at the loop point, so set_sound_loop seamless-repeats.  */
static int render_melody(const double *freqs, int count, double note_dur)
{
    int n = (int)(note_dur * count * RATE);
    double phase = 0;
    for (int i = 0; i < n; ++i) {
        double t   = (double)i / RATE;
        int   note = (int)(t / note_dur);
        if (note >= count) note = count - 1;
        double tt  = t - note * note_dur;      /* time within the note  */
        double f   = freqs[note];
        double s   = 0;
        if (f > 0) {
            phase += f / RATE;
            s = square(phase);
            /* gentle envelope: quick attack, decay to sustain, clean
             * release at the very end so the loop point is click-free */
            double env = 1.0;
            if (tt < 0.01)  env = tt / 0.01;
            if (tt > note_dur - 0.02) env = (note_dur - tt) / 0.02;
            s *= env;
        }
        buf[i] = (short)(8000 * s);
    }
    return n;
}

int main(void) {
    /* -- 0: shoot -- fast downward square sweep, hard decay ------------- */
    {
        double dur = 0.25;
        int n = (int)(dur * RATE);
        double phase = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / n;
            double freq = 900 - 750 * t;               /* 900 -> 150 Hz */
            phase += freq / RATE;
            double env = 1.0 - t * t;                  /* quick fade    */
            buf[i] = (short)(12000 * env * square(phase));
        }
        write_wav("shoot.wav", n, buf);
    }

    /* -- 1: alien_death -- crunchy noise burst --------------------------- */
    {
        double dur = 0.22;
        int n = (int)(dur * RATE);
        double last = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / n;
            double env = exp(-t * 6.0);
            /* mild lowpass so it reads as a "poof", not static */
            last = last * 0.6 + noise() * 0.4;
            buf[i] = (short)(14000 * env * last);
        }
        write_wav("alien_death.wav", n, buf);
    }

    /* -- 2: player_death -- long noise over a falling tone --------------- */
    {
        double dur = 0.7;
        int n = (int)(dur * RATE);
        double phase = 0;
        double last = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / n;
            double freq = 220 - 180 * t;               /* 220 -> 40 Hz  */
            phase += freq / RATE;
            double env = exp(-t * 3.5);
            last = last * 0.5 + noise() * 0.5;
            buf[i] = (short)(9000 * env * square(phase) + 7000 * env * last);
        }
        write_wav("player_death.wav", n, buf);
    }

    /* -- 3..9: march0..6 -- descending bass square tones -----------------
     * The classic march is a handful of low thumps traded off as the
     * swarm thins out; 7 steps cover SOUND_MARCH_BASE + marchStep().  */
    for (int step = 0; step < 7; ++step) {
        double dur = 0.12;
        int n = (int)(dur * RATE);
        double freq = 140 - step * 10;                 /* 140 -> 80 Hz  */
        double phase = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / n;
            phase += freq / RATE;
            double env = 1.0 - 0.4 * t;                /* blunt, flat-ish */
            buf[i] = (short)(13000 * env * square(phase));
        }
        char name[32];
        sprintf(name, "march%d.wav", step);
        write_wav(name, n, buf);
    }

    /* -- 10: saucer -- warbling sine, steady LFO: loop-friendly ---------- */
    {
        double dur = 2.0;
        int n = (int)(dur * RATE);
        double phase = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / RATE;
            double freq = 620 + 140 * sin(2 * PI * 11 * t); /* vibrato */
            phase += freq / RATE;
            buf[i] = (short)(9000 * sin(2 * PI * phase));
        }
        write_wav("saucer.wav", n, buf);
    }

    /* -- 11: saucer_death -- noise crash --------------------------------- */
    {
        double dur = 0.35;
        int n = (int)(dur * RATE);
        double last = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / n;
            double env = exp(-t * 5.0);
            last = last * 0.3 + noise() * 0.7;         /* brighter noise */
            buf[i] = (short)(13000 * env * last);
        }
        write_wav("saucer_death.wav", n, buf);
    }

    /* -- 12: extra_life -- rising sine arpeggio -------------------------- */
    {
        double dur = 0.6;
        int n = (int)(dur * RATE);
        for (int i = 0; i < n; ++i) {
            double t = (double)i / RATE;
            double freqs[3] = { 660.0, 880.0, 1320.0 };
            int note = (int)(t / 0.2);                 /* 3 x 0.2s      */
            if (note > 2) note = 2;
            double tt = t - note * 0.2;                /* time in note  */
            double env = clamp01(tt * 40) * (1.0 - tt * 1.5);
            if (env < 0) env = 0;
            buf[i] = (short)(10000 * env * sin(2 * PI * freqs[note] * tt));
        }
        write_wav("extra_life.wav", n, buf);
    }

    /* -- 13: bunker_hit -- short muffled thud ----------------------------- */
    {
        double dur = 0.09;
        int n = (int)(dur * RATE);
        double last = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / n;
            double env = 1.0 - t;                      /* fast knock    */
            last = last * 0.75 + noise() * 0.25;       /* heavily damped*/
            buf[i] = (short)(11000 * env * last);
        }
        write_wav("bunker_hit.wav", n, buf);
    }

    /* -- 14: menu_move -- short square blip -------------------------------- */
    {
        double dur = 0.06;
        int n = (int)(dur * RATE);
        double phase = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / n;
            phase += 740.0 / RATE;
            double env = 1.0 - t;
            buf[i] = (short)(11000 * env * square(phase));
        }
        write_wav("menu_move.wav", n, buf);
    }

    /* -- 15: menu_select -- two-tone confirm -------------------------------- */
    {
        double dur = 0.22;
        int n = (int)(dur * RATE);
        double phase = 0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / RATE;
            double freq = (t < 0.1) ? 523.0 : 784.0;   /* C5 -> G5  */
            phase += freq / RATE;
            double env = 1.0 - 0.3 * (t / dur);
            buf[i] = (short)(11000 * env * square(phase));
        }
        write_wav("menu_select.wav", n, buf);
    }

    /* -- 16: music_title -- march-flavored 8-note loop --------------------
     * Two-voice feel from one channel: low square bass with a higher
     * answer every other bar. 8 notes x 0.25s = a clean 2.0s loop.  */
    {
        /* A2 A2 C3 C3 E3 E3 D3 D3 -> answers G3 G3 A3 A3 ... on repeat;
         * rendered as a single 16-note phrase so the loop breathes   */
        double notes[16] = {
            110.0, 110.0, 130.8, 130.8, 164.8, 164.8, 146.8, 146.8,
            196.0, 196.0, 220.0, 220.0, 164.8, 146.8, 130.8, 110.0
        };
        int n = render_melody(notes, 16, 0.25);
        write_wav("music_title.wav", n, buf);
    }

    /* -- 17: music_game -- ominous bass ostinato, 2-bar loop ---------------
     * Semitone-staggered E minor descent (E2 D2 C2 B1), the classic
     * "invaders are coming" downward pressure. 8 x 0.3s = 2.4s loop.  */
    {
        double notes[8] = {
             82.4,  82.4,  73.4,  73.4,
             65.4,  65.4,  61.7,  61.7
        };
        int n = render_melody(notes, 8, 0.30);
        write_wav("music_game.wav", n, buf);
    }

    return 0;
}
