#pragma once
// *****************************************************************************
//  text.hpp — numbers, text, and the big block-letter title
// *****************************************************************************
#include "core.hpp"
#include "platform.hpp"

namespace si {

void drawText(Video& video, const char* text, int x, int y) {
    video.tint(color_white);
    for (int i = 0; text[i] != 0; ++i)
        video.blit(text[i], x + i * SPRITE_W, y);
}

void drawNumber(Video& video, int value, int x, int y) {
    char digits[12];
    int n = 0;
    do {
        digits[n] = '0' + value % 10;
        ++n;
        value /= 10;
    } while (value > 0 && n < 11);
    video.tint(color_white);
    for (int i = 0; i < n; ++i)
        video.blit(digits[n - 1 - i], x + i * SPRITE_W, y);
}

// ---------------------------------------------------------------------------
// The title font: each letter is a 3 x 5 grid of "pixels", written here as
// 15 characters, five rows of three, '#' = filled. Only the letters of
// SPACE INVADERS and the '+' exist; anything else is blank.
// ---------------------------------------------------------------------------
const char* titleGlyph(char c) {
    switch (c) {
        case 'S': return "####..###..####";
        case 'P': return "##.#.###.#..#..";
        case 'A': return ".#.#.#####.##.#";
        case 'C': return "####..#..#..###";
        case 'E': return "####..##.#..###";
        case 'I': return "###.#..#..#.###";
        case 'N': return "##.#.##.##.##.#";
        case 'V': return "#.##.##.##.#.#.";
        case 'R': return "##.#.###.#.##.#";
        case 'D': return "##.#.##.##.###.";
        case '+': return "....#.###.#....";
        default:  return "...............";
    }
}

// "SPACE INVADERS++" in bunker blocks: each title pixel is one block,
// shaded solid at the top to lightest at the bottom, every letter riding a
// slow sine wave. The "++" -- everything this version adds to the
// original game -- is yellow; the rest is the classic green.
//
// Sixteen letters have to share the 448 px field, so the blocks are drawn
// at 7/8 size (8.75 x 17.5 px): a letter is three blocks wide (26.25 px)
// on a 28 px pitch, and 16 x 28 = 448 exactly.
void drawBlockTitle(Video& video, int tick) {
    const char* title = "SPACE INVADERS++";
    video.setGlyphScale(0.875, 0.875);
    for (int i = 0; title[i] != 0; ++i) {
        int wave = SINE16[(tick / 4 + i) & 15] * 5 / 16;
        const char* glyph = titleGlyph(title[i]);
        video.tint(title[i] == '+' ? color_yellow : color_green);
        for (int row = 0; row < 5; ++row) {
            // rows 0..4 -> blocks 4, 3, 2, 1, 1 (the two bottom rows share
            // the lightest block)
            int shade = (row < 3) ? 3 - row : 0;
            for (int col = 0; col < 3; ++col)
                if (glyph[row * 3 + col] == '#')
                    video.blit(AssetIds::BUNKER_BLOCK_1 + shade,
                               i * 28 + (col * 35) / 4,        // 8.75 px per block
                               26 + (row * 35) / 2 + wave);    // 17.5 px per row
        }
    }
    video.setGlyphScale(1.0, 1.0);
}

} // namespace si
