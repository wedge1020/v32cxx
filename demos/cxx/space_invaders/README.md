# SPACE INVADERS++

Space Invaders for the Vircon32 fantasy console, written in C++ and built
with **v32c++**. Everything on screen is drawn with glyphs from the BIOS
font: no custom textures.

The "++" is for what this version adds to the original game: power-ups, a
shield for the cannon, and three difficulty levels.

A formation of aliens marches across the screen and down towards you.
Shoot them all before they reach the bottom, dodge their bombs, and use
the four bunkers for cover while they last.

## Controls

| Input        | On the title screen | In play        | In the pause menu           |
|--------------|---------------------|----------------|-----------------------------|
| Left / Right |                     | Move the cannon | Change the selected volume |
| Up / Down    | Choose a difficulty |                | Choose a row                |
| A            |                     | Fire           | Switch gameplay music on/off |
| B            |                     | Fire           |                             |
| Start        | Start the game      | Pause          | Resume                      |

You can only fire when nothing you fired is still in flight: one volley on
screen at a time, whatever weapon you have.

## The screen

The top line shows, left to right: your **score**, the **high score**, the
**wave** number, and on the right the **weapon** you are carrying (with
the seconds it has left, if it is timed) and the **shield**'s remaining
hits. Your spare cannons are shown at the bottom left.

The high score is the best score of the current game: it starts again from
zero with each new game.

## Scoring

| Target                         | Points      |
|--------------------------------|-------------|
| Squid (top row, magenta `W`)   | 30          |
| Crab (rows 2 and 3, `X`)       | 20          |
| Octopus (rows 4 and 5, `O`)    | 10          |
| Saucer                         | 50 to 120, at random |

You start with three cannons and earn another each time your score passes
a multiple of 1500.

## The saucer

Every so often a red saucer crosses the top of the screen, its rim lights
spinning. It drops nothing on you and is worth a random bonus, and
shooting it **always** releases a power-up capsule.

## Power-ups

A destroyed saucer always drops a capsule; a destroyed alien drops one
about one time in ten. A capsule is a letter between brackets, falling
slowly. Catch it with the cannon to use it; it is lost if it reaches the
ground. No more than four are on screen at once.

| Capsule | Name        | What it does |
|---------|-------------|--------------|
| `[D]`   | Double shot | Two shots side by side. |
| `[T]`   | Triple shot | Three shots, the outer two fanning out to the sides. |
| `[M]`   | Mega shot   | One thick, shimmering bolt that goes **through** what it destroys. It has six hits in it: enough for a whole column of five aliens with one left over for the saucer above them. It burns through bunkers too, and takes about a second to recharge. |
| `[B]`   | Blast       | Destroys the aliens' bottom row, at once. |
| `[R]`   | Repair      | Every bunker back to full strength. |
| `[S]`   | Shield      | A cyan bubble round the cannon that absorbs bomb hits. It flickers when it is down to its last one. |

The three weapons replace each other: catching one swaps out whatever you
were carrying and restarts its timer. A weapon is lost when its time runs
out, or when the cannon is destroyed. The shield is separate, and you can
have a weapon and a shield at once.

## Bunkers

Four bunkers sit between you and the aliens. Each is made of cells with
four hit points, drawn lighter as they wear down. Alien bombs chip them
(how fast depends on the difficulty), and so do your own shots: one hit
point per ordinary shot, and a mega bolt destroys every cell in its path.
The bunkers are rebuilt at the start of each wave.

## Difficulty

Chosen on the title screen. Every wave starts the formation at the same
height; the difficulty changes the pace instead.

|                                   | Easy        | Medium     | Hard       |
|-----------------------------------|-------------|------------|------------|
| Alien march                       | Slowest     | Slower     | Original speed |
| Time between bombs                | Longest     | Medium     | Shortest   |
| Fastest a bomb falls (px/frame)   | 1           | 2          | 3          |
| Bunker damage per bomb (of 4)     | 1           | 2          | 4          |
| Weapon power-up lasts             | Until you are hit | 60 seconds | 30 seconds |
| Shield absorbs                    | 5 hits      | 3 hits     | 1 hit      |

On every setting the march speeds up as aliens are destroyed, and bombs
fall faster in later waves, up to the limit above.

## How a game ends

* A bomb that hits an unshielded cannon destroys it. The game is over when
  you have no cannons left.
* The game is also over, at once, if any alien reaches the cannon's line.
* Destroying every alien clears the wave; after a short pause the next
  one begins. There is no last wave.

After a game ends, Start returns to the title screen.

## The pause menu

Start pauses the game. The menu has three rows:

* **Global volume** and **music volume**: Left and Right change them.
* **Gameplay music**: A switches it on or off. The title theme always
  plays.

Start resumes.

## Building

You need `v32c++` and the Vircon32 development tools (`compile`,
`assemble`, `wav2vircon`, `packrom`) in your `PATH`.

    make            # builds bin/space_invaders.v32
    make debug      # the same, with debug information
    make clean

If `v32opt` is also in your `PATH`, an optimized cartridge,
`bin/space_invadersOpt.v32`, is built alongside the regular one. If it is
not, make says so and builds only the regular cartridge. `make
HAVE_OPTIMIZER=` skips the optimized build even when the optimizer is
installed.

To see every power-up without waiting on luck, build the test variant: one
capsule of each kind falls onto the cannon when a game starts.

    make clean
    make TRANSPILER="v32c++ -I inc -D SI_TEST_POWERUPS"

## Source layout

v32c++ resolves `.hpp` includes itself, so the game is one translation
unit spread over several files:

| File                 | Contents |
|----------------------|----------|
| `space_invaders.cpp` | Cartridge title and sound list, `main()` |
| `inc/assets.hpp`     | Sprite ids, sound ids, and every tuning constant |
| `inc/core.hpp`       | `Random`, `Vec2`, `Rect`, the sine table |
| `inc/platform.hpp`   | `Input`, `Video`, `Sound`: thin wrappers over the SDK |
| `inc/entities.hpp`   | `Entity` and `Player`, `Bullet`, `PowerUp`, the aliens, `Bunker`, `Saucer` |
| `inc/swarm.hpp`      | The 5 x 11 formation and its march |
| `inc/text.hpp`       | Numbers, text, the block-letter title |
| `inc/game.hpp`       | `Game`: owns everything and runs each frame |

Most of what you might want to tune (speeds, odds, capacities, sizes) is
one named constant in `inc/assets.hpp`. The per-difficulty numbers are the
small functions at the top of `Game` in `inc/game.hpp`.
