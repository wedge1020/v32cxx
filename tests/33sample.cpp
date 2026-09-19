// Exercises cart hints (driver.h's g_cart_textures/g_cart_sounds,
// lexer.l's #texture/#sound recognition, codegen.c's
// emit_cart_hint_defines, cartxml.c's populated <textures>/<sounds>) --
// modeled directly on v32lua's own --#texture/--#sound cart hints.
//
// Mixed-case names on purpose (Background, player, EXPLOSION) -- cart
// hint names are never forced to any particular case. Expected: each
// name becomes a #define mapping to its declaration-order id (textures
// 0/1, sounds 0/1, independently -- each category starts its own count
// at 0), and the generated .xml lists all four resources, in this same
// order, with their extensions swapped to .vtex/.vsnd.

#include "video.h"

#texture Background "background.png"
#texture player "player.png"
#sound EXPLOSION "explosion.wav"
#sound jump_sfx "jump.wav"

void main() {
    select_texture(Background);
    select_texture(player);
    play_sound(EXPLOSION);
    play_sound(jump_sfx);
}
