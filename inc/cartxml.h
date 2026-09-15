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
 * <textures/> and <sounds/> are ALWAYS emitted empty, deliberately --
 * this project has no CART HINT functionality yet (unlike v32lua's own
 * "--#" comment-based hints, which populate these sections with real
 * resource entries -- texture/sound files, IDs, variable names), so
 * there is nothing to populate them with. Once cart-hint support
 * exists here too, this function will need real revisiting -- porting
 * v32lua's own resource-list walking and ID-consistency checking
 * alongside it, not just extending the empty-element placeholders in
 * place.
 *
 * cart_title/cart_version use v32lua's own documented defaults
 * ("Vircon32 Program" / "1.0") verbatim, for the same "no hints yet"
 * reason -- there's no way to override either from C++ source yet.
 *
 * Writes an error to stderr and returns without creating anything if
 * the XML file can't be opened for writing -- does not abort the
 * overall transpile (the .c output this function runs after has
 * already been written successfully by this point; a failed XML write
 * is a real problem worth reporting, but not one that should undo an
 * otherwise-successful transpile). */
void emit_cart_xml(const char *output_filename);

#endif /* CARTXML_H */
