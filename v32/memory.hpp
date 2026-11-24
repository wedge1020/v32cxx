#pragma once
// *****************************************************************************
//  v32/memory.hpp -- the arena allocator of `--alloc=arena`, and the
//  garbage collector of `--alloc=gc`
//
//  Only available when the program is transpiled with --alloc=arena or
//  --alloc=gc (v32c++ then defines __V32_ARENA__ or __V32_GC__); without one,
//  including this header is a compile-time error ("#error"), not a silent
//  no-op, so that a game that depends on mark/release or collect cannot be
//  built by mistake with plain malloc. The arena_* functions exist only with
//  --alloc=arena and the gc_* ones only with --alloc=gc: calling one in the
//  other mode does not compile.
//
//  ---- --alloc=arena ----
//
//  With --alloc=arena every `new`, `new[]` and std::vector buffer comes from
//  one block (--heap-words=N, default 262144 words) taken from the SDK's malloc at
//  the start of main(). Nothing is freed one by one: `delete` runs the
//  destructor and leaves the space, and the space comes back all at once:
//
//      int level = v32::arena_mark();    // level start
//      ...                               // new, vectors, ...
//      v32::arena_release(level);        // level end: everything since
//                                        // the mark is gone, and the
//                                        // destructors of the objects not
//                                        // `delete`d run, newest first
//
//  Typical use: a mark at the start of a level (or a frame, for scratch
//  data) and a release at its end. A pointer into released space must not be
//  used again; releasing an older mark invalidates the newer ones.
//
//  Out of space: the screen shows "OUT OF MEMORY (arena full)" and the cart
//  stops (standard C: a message on stderr and exit(1)).
//
//  Units: words (Vircon32's sizeof unit). Raw malloc() calls in your own
//  code are not routed to the arena.
//
//  ---- --alloc=gc ----
//
//  Every `new`, `new[]` and std::vector buffer comes from one heap
//  (--heap-words=N, default 262144 words) that is garbage collected: a
//  conservative mark-sweep collector, so nothing has to be `delete`d (but
//  `delete` is still legal and frees at once, after running the destructor).
//  The roots are the file-scope variables that can hold a pointer and the
//  whole machine stack plus the registers; any word that points into a live
//  block, to its start or anywhere inside it, keeps the block alive. A
//  collection runs when an allocation finds no room, after
//  --gc-threshold=N words were allocated since the last one (default a
//  quarter of the heap), and when you call gc_collect(). A game should call
//  it once per frame at a quiet point: a full collection in the middle of a
//  frame is a pause. Objects that are collected do NOT get their destructor
//  run. An integer that happens to look like a heap address keeps a dead
//  block alive, and a pointer hidden from the collector (stored xor-ed, or
//  only past the end of its block) does not keep anything alive. Memory from a
//  raw malloc() is not managed. Out of space after a collection: the screen
//  shows "OUT OF MEMORY (gc heap full)" and the cart stops.
// *****************************************************************************

#if !defined(__V32_ARENA__) && !defined(__V32_GC__)
#error "<v32/memory.hpp> needs --alloc=arena or --alloc=gc (v32c++ --alloc=gc ...)"
#endif

namespace v32
{
#ifdef __V32_ARENA__
    // An opaque mark of the arena's current top, for arena_release().
    int arena_mark()
    {
        return __v32_arena_mark();
    }

    // Frees everything allocated since `mark` (from arena_mark()), after
    // running the destructors of the objects still alive there, newest first.
    void arena_release( int mark )
    {
        __v32_arena_release( mark );
    }

    // Frees everything: arena_release of the very first mark.
    void arena_reset()
    {
        __v32_arena_reset();
    }

    // Words in use, and words still free.
    int arena_used()
    {
        return __v32_arena_used();
    }

    int arena_free()
    {
        return __v32_arena_free();
    }
#endif

#ifdef __V32_GC__
    // Runs a full collection now: every block no root reaches is freed.
    void gc_collect()
    {
        __v32_gc_collect();
    }

    // Words in use (block headers included), and words not in use.
    int gc_used()
    {
        return __v32_gc_get_used();
    }

    int gc_free()
    {
        return __v32_gc_get_free();
    }

    // How many collections have run since the start of main(), whether
    // asked for, triggered by the threshold or by a full heap.
    int gc_collections()
    {
        return __v32_gc_get_collections();
    }
#endif
}
