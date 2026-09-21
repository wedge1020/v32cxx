/* ============================================================================
 *  make_sfx.c -- Star Raiders sound effects generator
 *
 *  A HOST-side tool (compile with your normal desktop C compiler,
 *  NOT for Vircon32):
 *
 *      gcc make_sfx.c -o make_sfx -lm
 *      ./make_sfx
 *
 *  Writes 44100 Hz, 16-bit, mono WAV files into ./sounds/:
 *
 *      missile.wav      -- electric projectile launch (zap)
 *      explosion.wav    -- debris burst (noise boom, decaying)
 *      beep.wav         -- computer feedback (UI toggle)
 *      hyperspace.wav   -- warp jump (rising sweep + whoosh)
 *      engine.wav       -- seamless engine hum loop
 *      alert.wav        -- red alert klaxon (two-tone)
 *      replenish.wav    -- starbase energy/repair chime
 *
 *  Convert each to VSND (Vircon32 sound) with the standard toolchain,
 *  e.g.:  sndconverter sounds/missile.wav sounds/missile.vsnd
 *
 *  The game declares these with CART HINTs:
 *      #sound "sounds/missile"     1
 *      #sound "sounds/explosion"   2
 *      #sound "sounds/beep"        3
 *      #sound "sounds/hyperspace"  4
 *      #sound "sounds/engine"      5
 *      #sound "sounds/alert"       6
 *      #sound "sounds/replenish"   7
 * ==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define RATE 44100

/* ------------------------------------------------------------------ */
/*  tiny portable RNG (so output is reproducible)                     */
/* ------------------------------------------------------------------ */

static unsigned long rng_state = 12345;

static unsigned long rng_next( void )
{
    rng_state = rng_state * 1103515245UL + 12345UL;
    return ( rng_state >> 16 ) & 0x7FFF;
}

static double rng_01( void )
{
    return rng_next() / 32768.0;
}

/* ------------------------------------------------------------------ */
/*  WAV writer: 16-bit mono                                           */
/* ------------------------------------------------------------------ */

static void write_wav( const char* path, float* samples, int count )
{
    FILE* f;
    unsigned int data_bytes;
    unsigned int riff_size;
    unsigned int byte_rate;
    unsigned short fmt = 1;        /* PCM */
    unsigned short channels = 1;
    unsigned int rate = RATE;
    unsigned short bits = 16;
    int i;

    f = fopen( path, "wb" );
    if( !f )
    {
        fprintf( stderr, "cannot write %s\n", path );
        exit( 1 );
    }

    data_bytes = count * 2;
    riff_size = 36 + data_bytes;
    byte_rate = rate * channels * bits / 8;

    fwrite( "RIFF", 1, 4, f );
    fwrite( &riff_size, 4, 1, f );
    fwrite( "WAVE", 1, 4, f );
    fwrite( "fmt ", 1, 4, f );
    {
        unsigned int fmt_size = 16;
        fwrite( &fmt_size, 4, 1, f );
    }
    fwrite( &fmt, 2, 1, f );
    fwrite( &channels, 2, 1, f );
    fwrite( &rate, 4, 1, f );
    fwrite( &byte_rate, 4, 1, f );
    {
        unsigned short block_align = channels * bits / 8;
        fwrite( &block_align, 2, 1, f );
    }
    fwrite( &bits, 2, 1, f );
    fwrite( "data", 1, 4, f );
    fwrite( &data_bytes, 4, 1, f );

    for( i = 0; i < count; i++ )
    {
        float s = samples[ i ];
        short v;
        if( s > 1.0f ) s = 1.0f;
        if( s < -1.0f ) s = -1.0f;
        v = (short)( s * 32000 );
        fwrite( &v, 2, 1, f );
    }
    fclose( f );
    printf( "wrote %s (%.2f s)\n", path, count / (double)RATE );
}

/* ------------------------------------------------------------------ */
/*  helpers                                                           */
/* ------------------------------------------------------------------ */

static float* new_buffer( double seconds )
{
    float* b = (float*)malloc( (int)( seconds * RATE ) * sizeof( float ) );
    memset( b, 0, (int)( seconds * RATE ) * sizeof( float ) );
    return b;
}

/* 1 unit = 44100 samples per second of pitch */
static double env_ar( int i, int n, double attack, double release )
{
    double t = (double)i / n;
    double a = attack;
    double r = release;
    if( t < a && a > 0 )
        return t / a;
    if( t > 1.0 - r && r > 0 )
        return ( 1.0 - t ) / r;
    return 1.0;
}

/* simple one-pole lowpass state */
static double lp_state = 0;

static double lowpass( double x, double cutoff_norm )
{
    double a = 1.0 - exp( -2.0 * 3.14159265 * cutoff_norm );
    lp_state = lp_state + a * ( x - lp_state );
    return lp_state;
}

/* ================================================================== */
/*  1. missile: electric zap -- square-ish tone dropping in pitch,   */
/*     with a crackly noise layer and fast decay                      */
/* ================================================================== */

static void make_missile( void )
{
    double seconds = 0.45;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    double phase = 0;
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / n;
        double f = 1800 - 1500 * t;             /* pitch drop */
        double e = env_ar( i, n, 0.01, 0.55 );  /* fast tail */
        double sq = ( fmod( phase, 1.0 ) < 0.5 ) ? 1.0 : -1.0;
        double crackle = ( rng_01() * 2 - 1 ) * 0.35 * ( 1 - t );
        phase += f / RATE;
        b[i] = (float)( ( sq * 0.5 + crackle ) * e * 0.7 );
    }
    write_wav( "sounds/missile.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  2. explosion: filtered noise boom, long decay + rumble tail       */
/* ================================================================== */

static void make_explosion( void )
{
    double seconds = 1.6;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    lp_state = 0;
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / n;
        double e = pow( 1.0 - t, 2.2 );
        double cutoff = 0.35 * pow( 1.0 - t, 1.5 ) + 0.01;
        double noise = rng_01() * 2 - 1;
        double body = lowpass( noise, cutoff ) * 3.0;
        /* sub-bass thump at the start */
        double thump = 0;
        if( t < 0.15 )
            thump = sin( 2 * 3.14159265 * 55 * t ) * ( 1 - t / 0.15 ) * 0.8;
        b[i] = (float)( ( body * 0.8 + thump ) * e * 0.8 );
    }
    write_wav( "sounds/explosion.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  3. beep: UI feedback -- two quick sine blips                      */
/* ================================================================== */

static void make_beep( void )
{
    double seconds = 0.22;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / n;
        double f = ( t < 0.5 ) ? 880.0 : 1320.0;
        double e = env_ar( i, n, 0.02, 0.1 );
        b[i] = (float)( sin( 2 * 3.14159265 * f * ( t * seconds ) ) * e * 0.5 );
    }
    write_wav( "sounds/beep.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  4. hyperspace: full 8 s jump sequence -- a long accelerating      */
/*     engine rise (matching the ship's ramp-up), then a bright       */
/*     whoosh burst at the moment of transition, tail fade            */
/* ================================================================== */

static void make_hyperspace( void )
{
    double seconds = 8.4;               /* covers the whole 8 s run */
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    double phase = 0;
    lp_state = 0;
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / n;
        double e = env_ar( i, n, 0.02, 0.05 );
        /* accelerating rise across the whole run: 90 -> 900 Hz */
        double f = 90 + 810 * t * t;
        double tone = sin( phase * 2 * 3.14159265 );
        double tone2 = sin( phase * 4 * 3.14159265 ) * 0.3;
        phase += f / RATE;
        /* airy shimmer that thickens as we go faster */
        double shimmer = lowpass( rng_01() * 2 - 1, 0.02 + 0.25 * t ) * 2.0;
        /* the transition burst near the end of the 8 s */
        double burst = 0;
        if( t > 0.72 && t < 0.95 )
        {
            double bt = ( t - 0.72 ) / 0.23;
            burst = lowpass( rng_01() * 2 - 1, 0.5 ) * 3.0
                  * sin( 3.14159265 * bt ) * sin( 3.14159265 * bt );
        }
        b[i] = (float)( ( tone * 0.4 + tone2 * 0.15
                        + shimmer * 0.30 * ( 0.3 + 0.7 * t )
                        + burst * 0.5 ) * e * 0.8 );
    }
    write_wav( "sounds/hyperspace.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  5. engine: seamless loop -- low rumble + hum, loop-periodic       */
/*     (all components use whole numbers of cycles across the loop    */
/*     so the wrap-around is click-free)                              */
/* ================================================================== */

static void make_engine( void )
{
    double seconds = 2.0;                /* loop length */
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    lp_state = 0;
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / RATE;
        double hum = sin( 2 * 3.14159265 * 55 * t );           /* 55 Hz: 110 cycles */
        double hum2 = sin( 2 * 3.14159265 * 110 * t ) * 0.4;   /* 220 cycles */
        double flutter = 0.75 + 0.25 * sin( 2 * 3.14159265 * 2 * t ); /* 2 Hz AM */
        b[i] = (float)( ( hum * 0.55 + hum2 ) * flutter * 0.6 );
    }
    write_wav( "sounds/engine.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  6. alert: red alert klaxon -- alternating two tones               */
/* ================================================================== */

static void make_alert( void )
{
    double seconds = 1.2;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / n;
        double f = ( fmod( t, 0.4 ) < 0.2 ) ? 620.0 : 470.0;
        double e = env_ar( i, n, 0.01, 0.05 );
        /* saw-ish: fundamental + a couple of harmonics */
        double s = sin( 2 * 3.14159265 * f * ( t * seconds ) ) * 0.6
                 + sin( 2 * 3.14159265 * f * 2 * ( t * seconds ) ) * 0.2
                 + sin( 2 * 3.14159265 * f * 3 * ( t * seconds ) ) * 0.1;
        b[i] = (float)( s * e * 0.55 );
    }
    write_wav( "sounds/alert.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  7. replenish: starbase chime -- rising arpeggio, soft sines       */
/* ================================================================== */

static void make_replenish( void )
{
    double seconds = 1.1;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    /* three notes: C5, E5, G5-ish */
    double freqs[3] = { 523.25, 659.25, 783.99 };
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / n;
        int note = (int)( t * 3.999 );           /* 0,1,2 across the clip */
        double nt = fmod( t * 3.999, 1.0 );      /* position within note */
        double f = freqs[note];
        double e = sin( 3.14159265 * nt );       /* smooth per-note swell */
        double s = sin( 2 * 3.14159265 * f * ( t * seconds ) );
        b[i] = (float)( s * e * 0.45 );
    }
    write_wav( "sounds/replenish.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  8. title music: 8 s loop -- heroic minor-key arpeggio over a      */
/*     pulsing bass, all loop-periodic (click-free wrap)              */
/* ================================================================== */

static void make_title_music( void )
{
    double seconds = 8.0;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    /* A minor: A2 bass, arpeggio A3-C4-E4-A4 (110, 130.81, 164.81, 220) */
    double arp[4] = { 110.0 * 2, 130.8127826503 * 2, 164.8137784564 * 2, 220.0 * 2 };
    double bass_f = 55.0;                    /* 440 whole cycles in 8 s */
    double arp_rate = 4.0;                   /* 32 steps across the loop */
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / RATE;
        double step = fmod( t * arp_rate, 4.0 );
        int note = (int)step;
        double nt = fmod( step, 1.0 );
        double f = arp[note];
        /* exact loop-periodic phase: cycles = f * 8 / arp_rate per step,
           which is an integer for these frequencies over this length */
        double cycles = f * ( seconds / arp_rate ) * note;
        double cycles2 = f * ( seconds / arp_rate );
        double arp_s = sin( 2 * 3.14159265 * ( cycles + nt * cycles2 ) );
        double env = sin( 3.14159265 * nt );   /* soft per-note swell */
        double bass = sin( 2 * 3.14159265 * bass_f * t ) * 0.5
                    + sin( 2 * 3.14159265 * bass_f * 2 * t ) * 0.15;
        double pulse = 0.85 + 0.15 * sin( 2 * 3.14159265 * 2 * t );
        b[i] = (float)( ( arp_s * env * 0.35 + bass * 0.30 ) * pulse * 0.7 );
    }
    write_wav( "sounds/title.wav", b, n );
    free( b );
}

/* ================================================================== */
/*  9. gameplay music: 12 s loop -- tense low drone with a slow      */
/*     rotating two-chord pad (Am -> F), sparse blips on top         */
/* ================================================================== */

static void make_gameplay_music( void )
{
    double seconds = 12.0;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    /* two 6 s chords: A (110, 220, 261.63) then F (87.31, 174.61, 220) */
    double root[2] = { 110.0, 87.3070578583 };
    double third[2] = { 130.8127826503, 130.8127826503 };
    double fifth[2] = { 164.8137784564, 174.6141157165 };
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / RATE;
        int ch = ( t < 6.0 ) ? 0 : 1;
        /* pad: chord tones with slow tremolo */
        double pad = sin( 2 * 3.14159265 * root[ch] * t ) * 0.35
                   + sin( 2 * 3.14159265 * third[ch] * 2 * t ) * 0.18
                   + sin( 2 * 3.14159265 * fifth[ch] * 2 * t ) * 0.18;
        double trem = 0.7 + 0.3 * sin( 2 * 3.14159265 * ( 1.0 / 6.0 ) * t );
        /* deep pulse: 45 Hz, 540 whole cycles in 12 s */
        double pulse = sin( 2 * 3.14159265 * 45.0 * t ) * 0.22;
        /* sparse radar blips: 8 per loop, each ~0.05 s */
        double blip = 0;
        double bt = fmod( t, 1.5 );
        if( bt < 0.05 )
            blip = sin( 2 * 3.14159265 * 1560.0 * t )
                 * ( 1.0 - bt / 0.05 ) * 0.10;
        b[i] = (float)( ( pad * trem + pulse + blip ) * 0.65 );
    }
    write_wav( "sounds/gameplay.wav", b, n );
    free( b );
}

/* ================================================================== */

int main( void )
{
    /* the tool creates the output folder if it can */
    system( "mkdir -p sounds" );

    make_missile();
    make_explosion();
    make_beep();
    make_hyperspace();
    make_engine();
    make_alert();
    make_replenish();
    make_title_music();
    make_gameplay_music();

    printf( "\nAll sounds written to ./sounds/ -- convert to VSND:\n" );
    printf( "  sndconverter sounds/missile.wav    sounds/missile.vsnd\n" );
    printf( "  sndconverter sounds/explosion.wav  sounds/explosion.vsnd\n" );
    printf( "  sndconverter sounds/beep.wav       sounds/beep.vsnd\n" );
    printf( "  sndconverter sounds/hyperspace.wav sounds/hyperspace.vsnd\n" );
    printf( "  sndconverter sounds/engine.wav     sounds/engine.vsnd\n" );
    printf( "  sndconverter sounds/alert.wav      sounds/alert.vsnd\n" );
    printf( "  sndconverter sounds/replenish.wav  sounds/replenish.vsnd\n" );
    printf( "  sndconverter sounds/title.wav      sounds/title.vsnd\n" );
    printf( "  sndconverter sounds/gameplay.wav   sounds/gameplay.vsnd\n" );
    return 0;
}
