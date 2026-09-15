#ifndef CARTXML_H
#define CARTXML_H

/* Generates a Vircon32 cartridge-packing XML file alongside the
 * generated C output -- the manual, repetitive step Matthew asked to
 * have automated, matching v32lua's own emit_cart_xml format exactly
 * (same defaults, same structure, same element names/attributes) for
 * consistency between the two sibling projects and the wider Vircon32
 * toolchain's own expectations. Derives the XML's own filename, AND
 * the <binary path="..."/> element it references, from
 * `output_filename` -- the GENERATED .c file's own path, not the
 * original .cpp source -- replacing that path's own extension with
 * ".xml" for the XML file itself, and with ".vbin" for the <binary>
 * reference (the binary the Vircon32 C compiler will itself eventually
 * produce from this project's generated .c, the same role a
 * v32lua-generated .vbin plays for that compiler's own assembly
 * output). This mirrors v32lua's own emit_cart_xml exactly: it also
 * derives both paths from ITS OWN output filename (confirmed directly
 * from its source), not the original .lua.
 *
 * <textures>/<sounds> reflect driver.h's g_cart_textures/g_cart_sounds
 * (populated by lexer.l's own #texture/#sound recognition) -- non-empty,
 * in declaration order, when the program used either hint; the previous
 * always-empty <textures />/<sounds /> otherwise, unchanged for a
 * program that doesn't use them.
 *
 * title/version reflect driver.h's g_cart_title/g_cart_version
 * (#title/#version hints) when set, falling back to v32lua's own
 * documented defaults ("Vircon32 Program" / "1.0") otherwise.
 *
 * `is_bios`: when true, the <rom> element's own `type` attribute is
 * "bios" instead of "cartridge" -- main.c's own -b flag; this function
 * itself enforces nothing about what makes a valid BIOS (exactly one
 * texture, at most one sound, an error_handler function) -- that
 * validation is main.c's own job, run BEFORE this is ever called, so by
 * the time emit_cart_xml runs for a -b build, those constraints are
 * already known to hold.
 *
 * Writes an error to stderr and returns without creating anything if
 * the XML file can't be opened for writing -- does not abort the
 * overall transpile (the .c output this function runs after has
 * already been written successfully by this point; a failed XML write
 * is a real problem worth reporting, but not one that should undo an
 * otherwise-successful transpile). */
void emit_cart_xml(const char *output_filename, int is_bios);

#endif /* CARTXML_H */
