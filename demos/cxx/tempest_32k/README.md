# TEMPEST 32K

A Tempest  2000-style tunnel  shooter for  the Vircon32  fantasy console,
written in C++  and built with **v32c++**. Everything on  screen is drawn
with scaled  ASCII glyphs from the  BIOS font: no custom  textures, no 3D
hardware.

You pilot a  claw around the near  rim of a wireframe  web. Enemies climb
the lanes from the far end towards  you. Shoot them before they reach the
rim, clear the level's quota, and fly down the web into the next one.

## Controls

| Input        | In play                              | In menus            |
|--------------|--------------------------------------|---------------------|
| Left / Right | Move the claw around the rim         | Change a setting    |
| A            | Fire (hold for continuous fire)      | Select              |
| B            | Superzapper: destroys what is on the web (limited charges) | Back |
| Y            | Jump off the rim                     |                     |
| Start        | Pause                                | Select              |

The pause menu also adjusts the volume and the music track.

## The title menu

* **PLAY**: start a game.
* **PLAYERS**: one or two claws (see *Two players* below).
* **HIGH SCORES**: the top five, with initials.
* **LEVEL SELECT**: start on a later level.
* **DIFFICULTY**: EASY, MEDIUM or HARD.

## The webs

There are 32 levels over 16 web shapes. Some webs are closed loops, where
you can  circle the rim forever.  Others are **open**: the  rim has gaps.
You cannot walk across  a gap, so **jump (Y)** to  leap it. Power-up pods
on open webs always appear on the stretch of rim you are standing on.

## Enemies

| Looks like          | Name       | What it does |
|---------------------|------------|--------------|
| `W` with a `*` core | Flipper    | Climbs its lane, and near the top starts flipping from lane to lane to hunt you. |
| `H` with a `#` core | Tanker     | Climbs slowly; when shot it splits into smaller enemies. |
| `M` with a `v` tail | Spiker     | Rides up and down one lane, leaving a **spike** behind it. |
| Red tumbling `X`    | Rim walker | Patrols the rim itself. Jump over it or shoot it side-on. |
| Chain of beads      | Inchworm   | Stretches and bunches its way up a lane. Takes one hit per segment. |

### Inchworms

Each inchworm spawns with a random  **3 to 8 segments**. Every hit knocks
off the  front segment and the  rest keeps coming. The  body colour tells
you how  much is left:  **cyan** for four  or more, **green**  for three,
**amber** for two, **red** for the last one.

### Spikes

Spikes stay  on the web after  the spiker is  gone. Shots trim a  spike a
little at a  time. They do no  harm during the level, but  they matter at
the end: when the level is cleared  you fly *down* the web, and any spike
left in your lane  is in your way. Move to a clean  lane, shoot the spike
down as you go, or jump at the right moment.

If you hit a spike on the way out, the camera **rebounds** back up to the
rim and you replay the same level:

| Difficulty    | One player                         | Two players |
|---------------|------------------------------------|-------------|
| EASY          | Rebound, replay the level, no life lost | Same |
| MEDIUM / HARD | Lose a life, rebound, replay the level  | The struck player loses a life; the team carries on to the next level |

## Power-ups

Glowing `O` pods drift up the web. Catch one at the rim:

| Message              | Effect |
|----------------------|--------|
| SUPERZAP RECHARGED!  | One more superzapper charge (maximum four). |
| AI BUDDY ONLINE!     | A drone fights beside you for a while (longer on EASY). |
| RAPID BLASTER!       | Faster fire. Lasts the whole game on EASY, a limited time otherwise. |
| SUPER LASER!         | A piercing beam for a short time. |
| EXTRA LIFE!          | One more life. Not offered on HARD. |
| BONUS 250!           | Given instead when the pod's effect would be wasted. |

## Scoring

* Enemies are worth between 50 and 150 points depending on type.
* Each inchworm segment is 50; the last one is 100.
* Trimming a spike is 5 per hit.
* Clearing a level pays 1000 + 250 × level.

High scores are  saved to the memory  card if one is  inserted. Without a
card the game still runs, and scores last until power-off.

## Difficulty

EASY gives  three superzapper charges  per level, slower  spawning, fewer
enemies on the  web at once and longer power-ups.  HARD gives one charge,
faster spawning,  more enemies at  once and  no extra lives.  MEDIUM sits
between.

## Two players

Set PLAYERS to 2 for co-op: both claws share the web and each has its own
score and lives.  If a gamepad port  is empty, that claw is  flown by the
CPU, so  one person can play  co-op with a computer  partner. Plugging or
unplugging a pad hands control over on the next frame.

## Building

With `v32c++` and the Vircon32 DevTools on your `PATH`:

```
make            # builds bin/tempest_32k.v32
make debug      # same, with debug information
make clean
```

The project is split across several files to exercise v32c++'s `#include`
handling: `tempest_32k.cpp` pools the modules  in `src/`, and the headers
live in  `inc/`. Tunables  (lane count,  inchworm segment  range, rebound
speed and so on) are `#define`s in `inc/config.hpp`.

To transpile by hand:

```
v32c++ -I ../../.. -I inc -o obj/tempest_32k.c tempest_32k.cpp
```

Add `-D PROFILE` for the profiling build, in which lives never run out so
a scripted CPU-vs-CPU run can be measured with `tools/vircon32/v32prof`.
