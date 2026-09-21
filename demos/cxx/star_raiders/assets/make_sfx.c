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
/*  4. hyperspace: rising sweep + noise whoosh, tail fade             */
/* ================================================================== */

static void make_hyperspace( void )
{
    double seconds = 1.5;
    int n = (int)( seconds * RATE );
    float* b = new_buffer( seconds );
    int i;
    double phase = 0;
    lp_state = 0;
    for( i = 0; i < n; i++ )
    {
        double t = (double)i / n;
        double f = 120 + 1400 * t * t;                /* accelerating rise */
        double e = env_ar( i, n, 0.05, 0.3 );
        double tone = sin( phase * 2 * 3.14159265 );
        phase += f / RATE;
        double whoosh = lowpass( rng_01() * 2 - 1, 0.05 + 0.3 * t ) * 2.2;
        b[i] = (float)( ( tone * 0.35 + whoosh * 0.5 ) * e * 0.8 );
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

    printf( "\nAll sounds written to ./sounds/ -- convert to VSND:\n" );
    printf( "  sndconverter sounds/missile.wav    sounds/missile.vsnd\n" );
    printf( "  sndconverter sounds/explosion.wav  sounds/explosion.vsnd\n" );
    printf( "  sndconverter sounds/beep.wav       sounds/beep.vsnd\n" );
    printf( "  sndconverter sounds/hyperspace.wav sounds/hyperspace.vsnd\n" );
    printf( "  sndconverter sounds/engine.wav     sounds/engine.vsnd\n" );
    printf( "  sndconverter sounds/alert.wav      sounds/alert.vsnd\n" );
    printf( "  sndconverter sounds/replenish.wav  sounds/replenish.vsnd\n" );
    return 0;
}
