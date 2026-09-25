#pragma once
// *****************************************************************************
//  v32/memcard.hpp — thin C++ veneer over Vircon32's "memcard.h"
//
//  Same consumption model as the other v32/ headers: inlined into the
//  user's translation unit by v32c++'s own include resolution (-I); the
//  "#include" below is a pass-through line, resolved by the downstream
//  Vircon32 C compiler. All card access goes through the C API.
//
//  First consumer of `native` (docs/NATIVE_PASSTHROUGH.md). memcard.h
//  defines `typedef int[20] game_signature;` and its signature functions
//  take game_signature*. v32c++ never parses the C header, so the name is
//  declared native below: usable by pointer, emitted bare, never sized.
//  v32c++ source can't create a game_signature by value, so the API here
//  takes the signature as a plain 20-word int array -- which is exactly
//  what a game_signature is -- and casts at the C boundary:
//
//      int signature[ 20 ] = "MYGAME-SAVE-V1";   // padded with zeroes
//      v32::MemoryCard card( signature );
//
//  Card layout. The card is 262144 words (MemoryCardSize in the console
//  definitions), mapped at one base address. The 20-word signature is the
//  FIRST 20 words of that same range, and memcard.h's card_read_data /
//  card_write_data offsets count from the very start. So a raw
//  card_write_data( buf, 0, n ) overwrites the signature. MemoryCard's
//  load/save offsets instead count from the first word AFTER the
//  signature, so game data can't clobber it.
// *****************************************************************************

#include "memcard.h"

native game_signature;

namespace v32
{
    // -------------------------------------------------------------------------
    //  Card geometry, in words
    // -------------------------------------------------------------------------

    enum MemoryCardLayout
    {
        CardWords      = 262144,   // whole card
        SignatureWords = 20,       // game_signature, at the start of the card
        CardDataWords  = 262124    // CardWords - SignatureWords, for game data
    };

    // -------------------------------------------------------------------------
    //  Free functions: the C API with signatures as int arrays
    // -------------------------------------------------------------------------

    bool card_present()
    {
        return card_is_connected();
    }

    // true when the card's signature is all zeroes (never claimed by a game)
    bool card_blank()
    {
        return card_is_empty();
    }

    // signature must point at SignatureWords (20) ints
    bool card_matches( int* signature )
    {
        return card_signature_matches( (game_signature*)signature );
    }

    void read_card_signature( int* out_signature )
    {
        card_read_signature( (game_signature*)out_signature );
    }

    void write_card_signature( int* signature )
    {
        card_write_signature( (game_signature*)signature );
    }

    // -------------------------------------------------------------------------
    //  MemoryCard — a save slot for one game, identified by its signature
    //
    //  Every operation re-checks that a card is connected (it can be pulled
    //  out at any time) and, for load/save, that it carries this game's
    //  signature -- so a save can never land on another game's card.
    //
    //  The signature array is the caller's (usually a global or a local in
    //  main) and must outlive the MemoryCard; only its pointer is stored.
    //  Offsets and sizes are in words, counted from the start of the game
    //  data area (just past the signature), 0 .. CardDataWords-1.
    // -------------------------------------------------------------------------

    class MemoryCard
    {
        int* signature;

        public:
            MemoryCard( int* game_signature_words )
            {
                signature = game_signature_words;
            }

            bool connected()
            {
                return card_is_connected();
            }

            // connected, and never claimed by any game
            bool is_blank()
            {
                if( !card_is_connected() ) return false;
                return card_is_empty();
            }

            // connected, and claimed by THIS game
            bool is_ours()
            {
                if( !card_is_connected() ) return false;
                return card_signature_matches( (game_signature*)signature );
            }

            // write this game's signature, claiming the card. Refuses (and
            // returns false) unless the card is blank or already ours:
            // overwriting another game's signature would orphan its saves.
            bool claim()
            {
                if( !is_blank() && !is_ours() ) return false;
                card_write_signature( (game_signature*)signature );
                return true;
            }

            // read `words` words of game data starting at `offset`
            bool load( int* destination, int offset, int words )
            {
                if( !is_ours() ) return false;
                if( offset < 0 || words < 0 || offset + words > CardDataWords ) return false;
                card_read_data( destination, SignatureWords + offset, words );
                return true;
            }

            // write `words` words of game data starting at `offset`
            bool save( int* source, int offset, int words )
            {
                if( !is_ours() ) return false;
                if( offset < 0 || words < 0 || offset + words > CardDataWords ) return false;
                card_write_data( source, SignatureWords + offset, words );
                return true;
            }
    };
}

// usage:
//
//     int signature[ 20 ] = "SPACEGAME-SAVE-1";
//     v32::MemoryCard card( signature );
//
//     if( card.is_blank() ) card.claim();
//
//     int progress[ 4 ];
//     if( !card.load( progress, 0, 4 ) )
//       v32::fill_words( progress, 0, 4 );     // misc.hpp
//     ...
//     card.save( progress, 0, 4 );
//
// -----------------------------------------------------------------------------
