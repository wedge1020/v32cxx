#pragma once
// *****************************************************************************
//  v32/audio.hpp — thin C++ veneer over Vircon32's "audio.h"
//
//  Same consumption model as the other v32/ headers: inlined into the
//  user's translation unit by v32c++'s own include resolution (-I); the
//  "#include" below is a pass-through line, resolved by the downstream
//  Vircon32 C compiler. Every SPU access goes through the C API's own asm
//  wrappers; nothing here touches ports directly.
//
//  What this layer adds:
//   - Typed constants for the channel states and limits (#defines in C).
//   - SoundScope / ChannelScope: RAII guards for the SPU's two pieces of
//     implicit selection state, the same pattern as video.hpp's
//     TextureScope. The C API has two kinds of setters: ones that take a
//     channel id and select it themselves (play_channel, stop_channel,
//     ...) and ones that apply to "the currently selected" sound or
//     channel (set_sound_loop, set_channel_volume, ...). The second kind
//     is the footgun: calling it changes, or depends on, whatever the
//     rest of the program last selected.
//   - Channel: a handle object whose every method acts on its own channel
//     and leaves the SPU selection exactly as it found it.
//
//  One-word rule: Channel holds one int, and every method takes and
//  returns one-word values. Channel and the scope guards are meant to be
//  stack locals; pass a Channel* rather than a Channel.
//
//  Note: the C API's "query" functions (get_channel_speed/position/state)
//  and its play/pause/stop functions select the channel they are given
//  and do NOT restore the previous selection. Channel's methods wrap
//  those in a ChannelScope too, so a Channel never leaves a side effect.
// *****************************************************************************

#include "audio.h"

namespace v32
{
    // -------------------------------------------------------------------------
    //  Typed constants (the C header's #defines)
    // -------------------------------------------------------------------------

    enum ChannelState
    {
        ChannelStopped = 0x40,
        ChannelPaused  = 0x41,
        ChannelPlaying = 0x42
    };

    enum AudioLimits
    {
        SoundChannels = 16
    };

    // what play_any() returns when all 16 channels are busy
    enum PlayResult
    {
        NoFreeChannel = -1
    };

    // -------------------------------------------------------------------------
    //  RAII guards for the SPU's implicit selection state
    // -------------------------------------------------------------------------

    // selects a sound for the sound-configuration calls
    // (set_sound_loop, set_sound_loop_start, set_sound_loop_end),
    // restoring the previous selection at the closing brace
    class SoundScope
    {
        int previous_sound;

        public:
            SoundScope( int sound_id )
            {
                previous_sound = get_selected_sound();
                select_sound( sound_id );
            }

            ~SoundScope()
            {
                select_sound( previous_sound );
            }
    };

    // selects a channel for the channel-configuration calls
    // (set_channel_volume, set_channel_speed, ...), restoring the
    // previous selection at the closing brace
    class ChannelScope
    {
        int previous_channel;

        public:
            ChannelScope( int channel_id )
            {
                previous_channel = get_selected_channel();
                select_channel( channel_id );
            }

            ~ChannelScope()
            {
                select_channel( previous_channel );
            }
    };

    // -------------------------------------------------------------------------
    //  Sound configuration without touching the caller's selection.
    //  Loop points are in samples from the start of the sound.
    // -------------------------------------------------------------------------

    void set_sound_looping( int sound_id, bool enabled )
    {
        SoundScope scope( sound_id );
        set_sound_loop( enabled );
    }

    void set_sound_loop_points( int sound_id, int start_sample, int end_sample )
    {
        SoundScope scope( sound_id );
        set_sound_loop_start( start_sample );
        set_sound_loop_end( end_sample );
    }

    // -------------------------------------------------------------------------
    //  Channel — one of the SPU's 16 playback channels
    // -------------------------------------------------------------------------

    class Channel
    {
        int id;

        public:
            Channel( int channel_id )
            {
                id = channel_id;
            }

            int number()
            {
                return id;
            }

            // ---- playback --------------------------------------------------

            // assign a sound without starting it
            void assign( int sound_id )
            {
                ChannelScope scope( id );
                assign_channel_sound( id, sound_id );
            }

            // (re)start whatever sound is assigned
            void play()
            {
                ChannelScope scope( id );
                play_channel( id );
            }

            // assign and start in one call
            void play( int sound_id )
            {
                ChannelScope scope( id );
                play_sound_in_channel( sound_id, id );
            }

            void pause()
            {
                ChannelScope scope( id );
                pause_channel( id );
            }

            void stop()
            {
                ChannelScope scope( id );
                stop_channel( id );
            }

            // ---- configuration (C API applies these to the selection) ------

            void set_volume( float volume )
            {
                ChannelScope scope( id );
                set_channel_volume( volume );
            }

            void set_speed( float speed )
            {
                ChannelScope scope( id );
                set_channel_speed( speed );
            }

            // position in samples from the start of the assigned sound
            void set_position( int position )
            {
                ChannelScope scope( id );
                set_channel_position( position );
            }

            void set_looping( bool enabled )
            {
                ChannelScope scope( id );
                set_channel_loop( enabled );
            }

            // ---- queries ------------------------------------------------------

            float speed()
            {
                ChannelScope scope( id );
                return get_channel_speed( id );
            }

            int position()
            {
                ChannelScope scope( id );
                return get_channel_position( id );
            }

            // one of ChannelStopped / ChannelPaused / ChannelPlaying
            int state()
            {
                ChannelScope scope( id );
                return get_channel_state( id );
            }

            bool is_playing()
            {
                return state() == ChannelPlaying;
            }

            bool is_paused()
            {
                return state() == ChannelPaused;
            }

            bool is_stopped()
            {
                return state() == ChannelStopped;
            }
    };

    // -------------------------------------------------------------------------
    //  Whole-SPU operations
    // -------------------------------------------------------------------------

    // play a sound in the first free channel; returns that channel's
    // number, or NoFreeChannel if all channels are busy. (The C
    // play_sound() leaves the chosen channel selected; this restores.)
    int play_any( int sound_id )
    {
        ChannelScope scope( get_selected_channel() );
        return play_sound( sound_id );
    }

    void pause_all()
    {
        pause_all_channels();
    }

    void resume_all()
    {
        resume_all_channels();
    }

    void stop_all()
    {
        stop_all_channels();
    }

    // master volume, range 0.0 to 2.0 (1.0 is neutral)
    void set_master_volume( float volume )
    {
        set_global_volume( volume );
    }

    float master_volume()
    {
        return get_global_volume();
    }
}

// usage:
//
//     v32::Channel music( 0 );
//     music.set_looping( true );
//     music.play( SongTheme );          // SongTheme from a #sound hint
//
//     int ch = v32::play_any( SfxJump );
//     if( ch != v32::NoFreeChannel ) { ... }
//
// -----------------------------------------------------------------------------
