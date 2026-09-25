#pragma once
// *****************************************************************************
//  v32/misc.hpp — thin C++ veneer over Vircon32's "misc.h"
//
//  Same consumption model as the other v32/ headers: inlined into the
//  user's translation unit by v32c++'s own include resolution (-I); the
//  "#include" below is a pass-through line, resolved by the downstream
//  Vircon32 C compiler.
//
//  misc.h's allocator (malloc/calloc/realloc/free, with its block
//  splitting and merging) is deliberately NOT re-implemented or wrapped
//  in anything clever: it works, and a bug introduced here would corrupt
//  every program's RAM. The generated C already calls it for new/delete.
//
//  What this layer adds:
//   - Range-shaped random numbers (the C API only has rand()/srand()).
//   - Word-typed memory helpers: misc.h's memset/memcpy/memcmp take void*
//     and a size in WORDS (Vircon32's sizeof unit); the wrappers take int*
//     so the common "array of ints" case needs no casts.
//   - HeapBlock: an RAII owner for one malloc'd block -- free() at the
//     closing brace, including on an early return.
//
//  Units: Vircon32 memory is word-addressed. Every "size" and "count"
//  below is in 32-bit words, the same unit sizeof() returns there.
// *****************************************************************************

#include "misc.h"

namespace v32
{
    // -------------------------------------------------------------------------
    //  Random numbers
    //
    //  rand() reads the console's hardware generator, which only ever
    //  produces values from 1 to 0x7FFFFFFE (checked against the Vircon32
    //  emulator's V32RNG), so the remainder arithmetic below needs no sign
    //  handling. The small modulo bias of "rand() % n" is irrelevant at
    //  game-sized ranges.
    // -------------------------------------------------------------------------

    // 0 .. limit-1 (limit must be positive)
    int random_below( int limit )
    {
        return rand() % limit;
    }

    // low .. high, both included (requires low <= high)
    int random_between( int low, int high )
    {
        return low + rand() % (high - low + 1);
    }

    // true with the given chance, in percent (0 = never, 100 = always)
    bool random_chance( int percent )
    {
        return (rand() % 100) < percent;
    }

    // 0.0 .. 1.0 (1.0 itself is possible: 32-bit floats round the largest
    // generator values up to 2^31)
    float random_unit()
    {
        float value = rand();
        return value / 2147483647.0;
    }

    // Seeding. The generator is C++11's minstd_rand (next = value * 48271
    // mod 0x7FFFFFFF), and the console keeps only the low 31 bits of a
    // seed. Two seeds are therefore bad: 0 (the hardware ignores it) and
    // any seed whose low 31 bits are 0x7FFFFFFF -- that includes -1 --
    // which makes the very next value 0 and every value after it 0 too.
    // Both are replaced with 1, the console's own power-on seed.
    void seed_random( int seed )
    {
        if( (seed & 0x7FFFFFFF) == 0x7FFFFFFF || seed == 0 )
          seed = 1;
        srand( seed );
    }

    // -------------------------------------------------------------------------
    //  Word-array helpers (int* versions of memset / memcpy / memcmp)
    // -------------------------------------------------------------------------

    void fill_words( int* destination, int value, int count )
    {
        memset( destination, value, count );
    }

    void copy_words( int* destination, int* source, int count )
    {
        memcpy( destination, source, count );
    }

    bool same_words( int* first, int* second, int count )
    {
        return memcmp( first, second, count ) == 0;
    }

    // -------------------------------------------------------------------------
    //  HeapBlock — owns one malloc'd block of words, freed automatically
    //
    //  Check ok() before use: malloc returns NULL when the heap has no free
    //  block large enough. By default the heap is the middle half of the
    //  console's 4 MWords of RAM (words 1M to 3M-1; misc.h leaves the first
    //  MWord to globals and the last to the stack).
    //
    //  Stack local only. The generated C copies structs word by word, so a
    //  copied HeapBlock would free the same block twice. Pass a HeapBlock*
    //  or the raw data() pointer instead.
    // -------------------------------------------------------------------------

    class HeapBlock
    {
        int* words;
        int count;

        public:
            HeapBlock( int word_count )
            {
                words = (int*)malloc( word_count );
                count = 0;
                if( words != NULL )
                  count = word_count;
            }

            ~HeapBlock()
            {
                free( words );
            }

            bool ok()
            {
                return words != NULL;
            }

            int* data()
            {
                return words;
            }

            // size in words (0 if the allocation failed)
            int size()
            {
                return count;
            }

            void fill( int value )
            {
                if( words != NULL )
                  memset( words, value, count );
            }

            // grow or shrink in place when possible; returns false (and
            // keeps the old block) if the heap can't satisfy the request
            bool resize( int word_count )
            {
                int* bigger = (int*)realloc( words, word_count );
                if( bigger == NULL )
                  return false;
                words = bigger;
                count = word_count;
                return true;
            }
    };

    // -------------------------------------------------------------------------
    //  Program flow
    // -------------------------------------------------------------------------

    // halts the CPU (misc.h's exit(); Vircon32 programs have no exit codes)
    void halt()
    {
        exit();
    }
}

// usage:
//
//     {
//         v32::HeapBlock enemies( 64 );
//         if( !enemies.ok() ) return;
//         enemies.fill( 0 );
//         int* slot = enemies.data();
//         ...
//     }   // freed here
//
//     int x = v32::random_between( 0, screen_width - 1 );
//
// -----------------------------------------------------------------------------
