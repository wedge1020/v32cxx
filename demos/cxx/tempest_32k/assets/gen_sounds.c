/* ============================================================================
   TEMPEST 32K — sound + music generator.
   Companion to the game source: writes the 13 .wav files that the cart's
   #sound hints expect, into a "sounds/" subdirectory next to this binary.

     build:  gcc -O2 -o gen_sounds gen_sounds.c -lm
     run:    ./gen_sounds

   Everything is synthesized from scratch (square/saw/sine/triangle +
   noise + envelopes, 22050 Hz 16-bit mono). No assets, no dependencies.
   ============================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR( d ) _mkdir( d )
#else
  #include <sys/types.h>
  #include <sys/stat.h>
  #define MKDIR( d ) mkdir( d, 0755 )
#endif

#define SR 22050
#define PI  3.14159265358979

/* ------------------------------------------------ sample accumulation ---- */

static float *buf = NULL;
static long   cap = 0, len = 0;

static void buf_reset( void ) { len = 0; }

static void buf_add( long at, float v )
{
    if( at < 0 ) return;
    if( at >= cap )
    {
        long oldcap = cap;
        long ncap = at + 10 * SR;               /* grow 10 s at a time */
        buf = (float *) realloc( buf, ncap * sizeof( float ) );
        if( !buf ) { fprintf( stderr, "out of memory\n" ); exit( 1 ); }
        memset( buf + oldcap, 0, ( ncap - oldcap ) * sizeof( float ) );
        cap = ncap;
    }
    buf[ at ] += v;
    if( at + 1 > len ) len = at + 1;
}

static void write_wav( const char *path )
{
    long i;
    float peak = 1e-9f;
    FILE *f;
    unsigned int u32;
    unsigned short u16;
    long ds;

    if( len <= 0 ) { fprintf( stderr, "empty buffer for %s\n", path ); exit( 1 ); }
    for( i = 0; i < len; i++ )
    {
        float a = (float) fabs( buf[ i ] );
        if( a > peak ) peak = a;
    }
    f = fopen( path, "wb" );
    if( !f ) { fprintf( stderr, "cannot write %s\n", path ); exit( 1 ); }

    ds = len * 2;
    fwrite( "RIFF", 1, 4, f );
    u32 = (unsigned int)( 36 + ds );  fwrite( &u32, 4, 1, f );
    fwrite( "WAVE", 1, 4, f );
    fwrite( "fmt ", 1, 4, f );
    u32 = 16;                          fwrite( &u32, 4, 1, f );
    u16 = 1;                           fwrite( &u16, 2, 1, f );   /* PCM    */
    u16 = 1;                           fwrite( &u16, 2, 1, f );   /* mono   */
    u32 = SR;                          fwrite( &u32, 4, 1, f );
    u32 = SR * 2;                      fwrite( &u32, 4, 1, f );   /* rate   */
    u16 = 2;                           fwrite( &u16, 2, 1, f );   /* align  */
    u16 = 16;                          fwrite( &u16, 2, 1, f );   /* bits   */
    fwrite( "data", 1, 4, f );
    u32 = (unsigned int) ds;           fwrite( &u32, 4, 1, f );

    {
        float g = 0.86f / peak;                   /* normalize */
        for( i = 0; i < len; i++ )
        {
            float v = buf[ i ] * g;
            short s;
            if( v >  1 ) v =  1;
            if( v < -1 ) v = -1;
            s = (short)( v * 32767 );
            fwrite( &s, 2, 1, f );
        }
    }
    fclose( f );
    printf( "  %-24s %6.1f s  %ld KB\n", path, (double) len / SR, ds / 1024 );
}

/* ----------------------------------------------------------- synthesizer -- */

static double osc( double ph, int wave )
{
    double p = ph - floor( ph );
    switch( wave )
    {
        case 0:  return p < 0.5 ? 1.0 : -1.0;                  /* square   */
        case 1:  return 2.0 * p - 1.0;                         /* saw      */
        case 2:  return sin( 2 * PI * p );                     /* sine     */
        default: { double t = fabs( 2.0 * p - 1.0 );           /* triangle */
                   return 2.0 * t - 1.0; }
    }
}

/* one tone: freq glide f0 -> f1 across the note, attack fraction `atk`,
   exponential-ish decay exponent `dec` */
static void tone( double t0, double dur, double f0, double f1, int wave,
                  double gain, double atk, double dec )
{
    long n  = (long)( dur * SR );
    long s0 = (long)( t0 * SR );
    double ph = 0.0;
    long i;

    if( n <= 0 ) return;
    if( atk < 0.002 ) atk = 0.002;
    if( atk > 0.9 )   atk = 0.9;

    for( i = 0; i < n; i++ )
    {
        double t = (double) i / (double) n;
        double f = f0 + ( f1 - f0 ) * t;
        double env;
        ph += f / SR;
        if( t < atk ) env = t / atk;
        else env = pow( 1.0 - ( t - atk ) / ( 1.0 - atk ), dec );
        buf_add( s0 + i, (float)( osc( ph, wave ) * env * gain ) );
    }
}

static unsigned long rs = 12345;
static double rnd( void )
{
    rs = rs * 1103515245UL + 12345UL;
    return ( ( rs >> 16 ) & 0x7fff ) / 16384.0 - 1.0;
}

static void noisehit( double t0, double dur, double gain, double dec )
{
    long n  = (long)( dur * SR );
    long s0 = (long)( t0 * SR );
    long i;
    for( i = 0; i < n; i++ )
    {
        double t = (double) i / (double) n;
        buf_add( s0 + i, (float)( rnd() * pow( 1.0 - t, dec ) * gain ) );
    }
}

static double nfreq( int note ) { return 440.0 * pow( 2.0, ( note - 69 ) / 12.0 ); }

/* drums */
static void kick ( double t0 ) { tone( t0, 0.12, 120, 38, 2, 0.55, 0.004, 5 ); }
static void snare( double t0 )
{
    noisehit( t0, 0.10, 0.30, 7 );
    tone( t0, 0.06, 190, 120, 2, 0.15, 0.003, 8 );
}
static void hat( double t0, double g ) { noisehit( t0, 0.035, g, 18 ); }

/* ================================================================== SFX == */

static void sfx_shoot( void )
{
    buf_reset();
    tone( 0.00, 0.09, 950, 260, 0, 0.42, 0.004, 3 );      /* falling square */
    write_wav( "sounds/shootsound.wav" );
}

static void sfx_boom( void )
{
    buf_reset();
    noisehit( 0.00, 0.28, 0.55, 2.2 );
    tone( 0.00, 0.16, 140, 42, 2, 0.60, 0.003, 3 );       /* low thump */
    write_wav( "sounds/boom.wav" );
}

static void sfx_death( void )
{
    buf_reset();
    noisehit( 0.00, 0.70, 0.70, 1.5 );
    tone( 0.00, 0.60, 220, 32, 1, 0.40, 0.010, 2 );       /* falling saw  */
    tone( 0.02, 0.50, 110, 28, 2, 0.50, 0.010, 2 );       /* sub droop    */
    write_wav( "sounds/death.wav" );
}

static void sfx_zap( void )
{
    buf_reset();
    tone( 0.00, 0.10, 180, 1500, 1, 0.45, 0.004, 2 );     /* rising zap   */
    tone( 0.10, 0.14, 1500, 240, 1, 0.40, 0.004, 3 );     /* crack tail   */
    noisehit( 0.02, 0.18, 0.25, 4 );
    write_wav( "sounds/zap.wav" );
}

static void sfx_jump( void )
{
    buf_reset();
    tone( 0.00, 0.12, 220, 740, 0, 0.38, 0.004, 1.5 );    /* rising blip  */
    write_wav( "sounds/jump.wav" );
}

static void sfx_pickup( void )
{
    buf_reset();
    tone( 0.00, 0.07, 660,  660,  0, 0.35, 0.003, 2 );
    tone( 0.07, 0.10, 990,  990,  0, 0.35, 0.003, 2 );
    tone( 0.07, 0.10, 1980, 1980, 2, 0.15, 0.003, 2 );    /* sparkle */
    write_wav( "sounds/pickup.wav" );
}

static void sfx_clear( void )
{
    int i;
    static const int notes[ 4 ] = { 69, 72, 76, 81 };     /* A C E A */
    buf_reset();
    for( i = 0; i < 4; i++ )
        tone( i * 0.09, 0.12, nfreq( notes[ i ] ), nfreq( notes[ i ] ), 0,
              0.35, 0.004, 1.8 );
    tone( 0.36, 0.35, nfreq( 69 ), nfreq( 69 ), 2, 0.20, 0.01, 1.2 );
    tone( 0.36, 0.35, nfreq( 81 ), nfreq( 81 ), 2, 0.15, 0.01, 1.2 );
    write_wav( "sounds/clear.wav" );
}

static void sfx_blip( void )
{
    buf_reset();
    tone( 0.00, 0.035, 800, 800, 0, 0.30, 0.002, 3 );
    write_wav( "sounds/blip.wav" );
}

/* ================================================================ MUSIC ===
   A 16th-note step grid. Each track builds 8 bars; gameplay tracks end on
   a bar boundary so the succession (game rolls to the next WAV when the
   channel reports stopped) feels seamless.                                  */

/* --- M_TITLE: "NEON GRID", 90 BPM, A minor, slow pad + arp + melody ---- */
static void music_title( void )
{
    static const int chords[ 4 ][ 3 ] = {
        { 57, 60, 64 },    /* Am : A3 C4 E4 */
        { 53, 57, 60 },    /* F  : F3 A3 C4 */
        { 60, 64, 67 },    /* C  : C4 E4 G4 */
        { 55, 59, 62 }     /* G  : G3 B3 D4 */
    };
    /* 2-bar melody phrases over each chord pair (step grid, -99 = rest) */
    static const int mel[ 4 ][ 32 ] = {
        { 76,-99,-99,-99, 72,-99,-99,-99, 69,-99,-99,-99, 72,-99,-99,-99,
          74,-99,-99,-99, 72,-99,-99,-99, 69,-99, 67,-99, 69,-99,-99,-99 },
        { 72,-99,-99,-99, 69,-99,-99,-99, 65,-99,-99,-99, 69,-99,-99,-99,
          67,-99,-99,-99, 64,-99,-99,-99, 62,-99,-99,-99, 64,-99,-99,-99 },
        { 76,-99,-99,-99, 72,-99,-99,-99, 67,-99,-99,-99, 72,-99,-99,-99,
          74,-99,-99,-99, 76,-99,-99,-99, 79,-99,-99,-99, 76,-99,-99,-99 },
        { 74,-99,-99,-99, 71,-99,-99,-99, 67,-99,-99,-99, 62,-99,-99,-99,
          64,-99,-99,-99, 67,-99,-99,-99, 69,-99,-99,-99, 69,-99,-99,-99 }
    };
    double step = 60.0 / 90.0 / 4.0;      /* 0.1667 s */
    int c, i;

    buf_reset();
    for( c = 0; c < 4; c++ )               /* 4 chord pairs = 8 bars */
    {
        double t0 = c * 32 * step;
        for( i = 0; i < 3; i++ )
            tone( t0, 32 * step * 0.95, nfreq( chords[ c ][ i ] ),
                  nfreq( chords[ c ][ i ] ), 3, 0.09, 0.40, 0.8 );   /* pad */
        for( i = 0; i < 32; i += 2 )       /* 8th-note arp over the chord */
        {
            int n = chords[ c ][ ( i / 2 ) % 3 ] + 12;
            tone( t0 + i * step, step * 1.6, nfreq( n ), nfreq( n ), 2,
                  0.10, 0.05, 2 );
        }
        for( i = 0; i < 32; i += 4 )       /* quarter hats */
            hat( t0 + i * step, 0.05 );
        for( i = 0; i < 32; i++ )          /* melody */
            if( mel[ c ][ i ] > -90 )
                tone( t0 + i * step, step * 3.6, nfreq( mel[ c ][ i ] ),
                      nfreq( mel[ c ][ i ] ), 2, 0.16, 0.10, 1.5 );
    }
    write_wav( "sounds/m_title.wav" );
}

/* --- M_TRACK1: "WEB CRAWLER", 118 BPM, A minor, driving ---------------- */
static void music_track1( void )
{
    static const int roots [ 8 ] = { 45, 45, 45, 45, 41, 41, 43, 43 };
    static const int bass8[ 8 ]  = { 0, 0, 12, 0, 0, 12, 0, 12 };
    static const int riff [ 32 ] = {
          0,-99,  3,-99,   5,-99,  7,-99,
          5,  3,  0,-99,   3,-99,  5,-99,
          7,-99, 10,-99,   7,-99,  5,-99,
          3,  5,  7,  5,   3,  0,-99,-99 };
    double step = 60.0 / 118.0 / 4.0;
    int bar, e, i, rep;

    buf_reset();
    for( bar = 0; bar < 8; bar++ )
    {
        double t0 = bar * 16 * step;
        kick ( t0 +  0 * step );
        kick ( t0 +  8 * step );
        if( bar % 2 == 1 ) kick( t0 + 11 * step );
        snare( t0 +  4 * step );
        snare( t0 + 12 * step );
        for( i = 0; i < 16; i += 2 ) hat( t0 + i * step, 0.07 );
        for( e = 0; e < 8; e++ )          /* bass 8ths, octave bounce */
        {
            int n = roots[ bar ] + bass8[ e ];
            tone( t0 + e * 2 * step, step * 1.8, nfreq( n ), nfreq( n ),
                  1, 0.34, 0.010, 1.6 );
        }
        for( i = 0; i < 16; i++ )         /* 16th arp A-C-E-A */
        {
            static const int arp[ 4 ] = { 0, 3, 7, 12 };
            int n = 57 + arp[ i % 4 ];
            tone( t0 + i * step, step * 0.9, nfreq( n ), nfreq( n ),
                  3, 0.09, 0.02, 2 );
        }
    }
    for( rep = 0; rep < 4; rep++ )         /* lead riff, 2 bars x 4 */
        for( i = 0; i < 32; i++ )
            if( riff[ i ] > -90 )
                tone( rep * 32 * step + i * step, step * 1.9,
                      nfreq( 69 + riff[ i ] ), nfreq( 69 + riff[ i ] ),
                      0, 0.24, 0.008, 1.6 );
    write_wav( "sounds/m_track1.wav" );
}

/* --- M_TRACK2: "SPIKE SURFER", 126 BPM, E minor, 16th-hat drive -------- */
static void music_track2( void )
{
    static const int roots [ 8 ] = { 40, 40, 43, 45, 40, 40, 48, 47 };
    static const int bass8[ 8 ]  = { 0, 12, 0, 0, 12, 0, 7, 12 };
    static const int riff [ 32 ] = {
          0,-99,-99,  3,   5,-99,  3,-99,
          7,-99,  5,-99,   3,-99,  2,-99,
          0,-99,-99,  3,   5,-99,  7,-99,
         10,-99,  8,-99,   7,-99,  5,-99 };
    double step = 60.0 / 126.0 / 4.0;
    int bar, e, i, rep;

    buf_reset();
    for( bar = 0; bar < 8; bar++ )
    {
        double t0 = bar * 16 * step;
        kick ( t0 +  0 * step );
        kick ( t0 +  8 * step );
        kick ( t0 + 14 * step );
        snare( t0 +  4 * step );
        snare( t0 + 12 * step );
        for( i = 0; i < 16; i++ ) hat( t0 + i * step, 0.05 );   /* 16ths */
        for( e = 0; e < 8; e++ )
        {
            int n = roots[ bar ] + bass8[ e ];
            tone( t0 + e * 2 * step, step * 1.7, nfreq( n ), nfreq( n ),
                  1, 0.32, 0.010, 1.5 );
        }
        for( i = 0; i < 16; i += 2 )      /* offbeat stab */
        {
            int n = 64 + ( ( i % 4 ) ? 7 : 0 );
            tone( t0 + i * step + step, step * 0.8, nfreq( n ), nfreq( n ),
                  3, 0.08, 0.03, 2.5 );
        }
    }
    for( rep = 0; rep < 4; rep++ )
        for( i = 0; i < 32; i++ )
            if( riff[ i ] > -90 )
                tone( rep * 32 * step + i * step, step * 1.9,
                      nfreq( 64 + riff[ i ] ), nfreq( 64 + riff[ i ] ),
                      0, 0.24, 0.008, 1.6 );
    write_wav( "sounds/m_track2.wav" );
}

/* --- M_TRACK3: "HYPERSPACE", 138 BPM, D minor, four-on-the-floor ------- */
static void music_track3( void )
{
    static const int roots [ 8 ] = { 38, 38, 45, 43, 38, 38, 41, 45 };
    static const int bass8[ 8 ]  = { 0, 0, 7, 0, 0, 0, 7, 0 };
    static const int run  [ 32 ] = {
          0,  3,  7, 10,  12, 10,  7,  3,
          0,  3,  7, 10,  12, 14, 12, 10,
          7, 10, 14, 10,   7,  5,  3,  2,
          0,  3,  7,  3,   0,-99,-99,-99 };
    double step = 60.0 / 138.0 / 4.0;
    int bar, e, i, rep;

    buf_reset();
    for( bar = 0; bar < 8; bar++ )
    {
        double t0 = bar * 16 * step;
        for( i = 0; i < 16; i += 4 ) kick( t0 + i * step );    /* 4-floor */
        snare( t0 +  4 * step );
        snare( t0 + 12 * step );
        for( i = 2; i < 16; i += 4 ) hat( t0 + i * step, 0.08 );
        for( e = 0; e < 8; e++ )
        {
            int n = roots[ bar ] + bass8[ e ];
            tone( t0 + e * 2 * step, step * 1.7, nfreq( n ), nfreq( n ),
                  1, 0.33, 0.010, 1.5 );
        }
    }
    for( rep = 0; rep < 4; rep++ )
        for( i = 0; i < 32; i++ )
            if( run[ i ] > -90 )
                tone( rep * 32 * step + i * step, step * 0.95,
                      nfreq( 62 + run[ i ] ), nfreq( 62 + run[ i ] ),
                      1, 0.20, 0.006, 1.2 );
    write_wav( "sounds/m_track3.wav" );
}

/* --- M_TRACK4: "FLIPPER STORM", 150 BPM, C minor, aggressive ------------ */
static void music_track4( void )
{
    static const int roots [ 8 ] = { 36, 36, 36, 36, 39, 39, 41, 41 };
    static const int bass8[ 8 ]  = { 0, 12, 0, 12, 0, 12, 0, 12 };
    static const int riff [ 32 ] = {
          0,-99,  3,-99,   0,  0,-99,  3,
          5,-99,  3,-99,   0,-99,-99,-99,
          8,-99,  7,-99,   5,-99,  3,-99,
          2,-99,  3,  5,   3,-99,  0,-99 };
    double step = 60.0 / 150.0 / 4.0;
    int bar, e, i, rep;

    buf_reset();
    for( bar = 0; bar < 8; bar++ )
    {
        double t0 = bar * 16 * step;
        kick ( t0 +  0 * step );
        kick ( t0 + 10 * step );
        snare( t0 +  4 * step );
        snare( t0 + 12 * step );
        for( i = 0; i < 16; i++ ) hat( t0 + i * step, 0.06 );
        for( e = 0; e < 8; e++ )
        {
            int n = roots[ bar ] + bass8[ e ];
            tone( t0 + e * 2 * step, step * 1.6, nfreq( n ), nfreq( n ),
                  1, 0.38, 0.008, 1.4 );
        }
        if( bar % 4 == 0 )                 /* crash-ish accent */
            noisehit( t0, 0.35, 0.14, 3 );
    }
    for( rep = 0; rep < 4; rep++ )
        for( i = 0; i < 32; i++ )
            if( riff[ i ] > -90 )
                tone( rep * 32 * step + i * step, step * 1.9,
                      nfreq( 60 + riff[ i ] ), nfreq( 60 + riff[ i ] ),
                      0, 0.26, 0.008, 1.5 );
    write_wav( "sounds/m_track4.wav" );
}

/* ------------------------------------------------------------------ main - */

int main( void )
{
    MKDIR( "sounds" );
    printf( "TEMPEST 32K — generating sounds/...\n" );

    sfx_shoot(); sfx_boom(); sfx_death(); sfx_zap(); sfx_jump();
    sfx_pickup(); sfx_clear(); sfx_blip();

    music_title(); music_track1(); music_track2(); music_track3(); music_track4();

    printf( "done — 13 files.\n" );
    return 0;
}
