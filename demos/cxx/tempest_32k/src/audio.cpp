// *****************************************************************************
//  TEMPEST 32K -- src/audio.cpp
//  sound effects and the music track succession
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Audio — SFX round-robin over channels 2..13; channel 0 is reserved
//  for music, 1 is spare. Gameplay tracks play one after another (loop
//  OFF); the title theme loops. Channel state register: 0x40 stopped,
//  0x41 paused, 0x42 playing.
// ---------------------------------------------------------------------------
void sfx( G* g, int snd )
{
    if( g->sfx_channel < 2 || g->sfx_channel > 13 ) g->sfx_channel = 2;
    play_sound_in_channel( snd, g->sfx_channel );
    // play_sound_in_channel leaves this channel selected — apply the
    // SFX volume live so pause-menu changes reach every channel
    set_channel_volume( g->sfx_volume );
    g->sfx_channel++;
}

// play gameplay track n (0..3) on the music channel, loop OFF.
// NEVER stack: play_sound_in_channel only assigns + plays — on real
// hardware a Play command on an already-playing channel layers the
// new track over the old one instead of replacing it. So: STOP the
// channel first, then assign and play (note assign_channel_sound
// takes the CHANNEL id first, unlike play_sound_in_channel).
void play_track( G* g, int track )
{
    if( track > 3 ) track = 0;   // wrap: the cycle loops 1->2->3->4->1
    if( track < 0 ) track = 3;
    g->music_index = track;
    stop_channel( 0 );
    select_channel( 0 );
    set_channel_loop( 0 );   // loop OFF (0/1, not bool literals)
    assign_channel_sound( 0, M_TRACK1 + track );
    play_channel( 0 );
    set_channel_volume( g->music_volume );   // play_channel selected 0
}

// the 32 levels are split into 8-level MUSIC BANDS: track 1 plays on
// levels 1-8, track 2 on 9-16, track 3 on 17-24, track 4 on 25-32 —
// and past level 32 the bands wrap (4 -> 1), so an endless run never
// runs out of music. Within a level the tracks still hand off
// 1->2->3->4->1 as each one finishes (update_music); start_level
// re-stamps the band track whenever the level crosses a band edge.
int level_track( G* g )
{
    int t = ( ( g->level - 1 ) / 8 ) % 4;
    if( t < 0 ) t = 0;
    return t;
}

// title theme on the music channel, loop ON (same stop-first rule)
void play_title_music( G* g )
{
    stop_channel( 0 );
    select_channel( 0 );
    set_channel_loop( 1 );   // loop ON
    assign_channel_sound( 0, M_TITLE );
    play_channel( 0 );
    set_channel_volume( g->music_volume );
}

// gameplay track succession: roll to the next track when the current
// one finishes (called only during play/dying/transition/pause)
void update_music( G* g )
{
    if( get_channel_state( 0 ) == channel_stopped )
    {
        int t = g->music_index + 1;
        if( t > 3 ) t = 0;
        play_track( g, t );
    }
}
