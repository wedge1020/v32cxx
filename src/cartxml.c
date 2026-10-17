#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cartxml.h"
#include "driver.h"
#include "pathutil.h"

/* Collision warnings are printed once per run, not once per XML written. */
static int g_warn_collisions = 1;

/* Path components of `filename` ('/' or '\' separated), with empty, "."
 * and ".." parts dropped. Returns the count; `parts` points into `buf`
 * (a writable copy the caller owns). */
static int split_components(char *buf, const char **parts, int max_parts) {
    int n = 0;
    for (char *tok = buf; *tok != '\0'; ) {
        char *end = tok;
        while (*end != '\0' && *end != '/' && *end != '\\') end++;
        char saved = *end;
        *end = '\0';
        if (tok[0] != '\0' && strcmp(tok, ".") != 0 && strcmp(tok, "..") != 0 &&
            n < max_parts)
            parts[n++] = tok;
        if (saved == '\0') break;
        tok = end + 1;
    }
    return n;
}

/* The XML path of `filename` (extension swapped to `ext`) built from its
 * last `depth` directory/file components, joined with '/'. `*total`
 * receives how many components the path has, so the caller knows when
 * it has run out. */
static char *name_with_depth(const char *filename, int depth, const char *ext,
                             int *total) {
    char *buf = strdup(filename);
    const char *parts[64];
    int n = split_components(buf, parts, 64);
    if (n == 0) { parts[0] = path_basename(filename); n = 1; }
    if (depth > n) depth = n;
    size_t cap = strlen(filename) + 2;
    char *joined = malloc(cap);
    joined[0] = '\0';
    for (int k = n - depth; k < n; k++) {
        if (k > n - depth) strcat(joined, "/");
        strcat(joined, parts[k]);
    }
    *total = n;
    char *result = replace_extension(joined, ext);
    free(joined);
    free(buf);
    return result;
}

/* Works out the XML path of every entry of `list` (malloc'd array of
 * malloc'd strings; caller frees). With `preserve_paths` it is the
 * path as written. Otherwise it is the bare file name -- unless an
 * EARLIER, different asset already took that name, in which case the
 * later one keeps just enough of its own directories to be unique:
 * "textures/ship.png" then "textures/extra/ship.png" give "ship.vtex"
 * and "extra/ship.vtex" (so packrom wants the second in <xml dir>/extra/).
 * The first asset to claim a name always keeps the short form. */
static char **resolve_names(const CartResourceList *list, const char *tag,
                            const char *ext, int preserve_paths) {
    char **names = calloc((size_t)list->count + 1, sizeof *names);
    for (int i = 0; i < list->count; i++) {
        if (preserve_paths) {
            names[i] = replace_extension(list->items[i].filename, ext);
            continue;
        }
        int depth = 1, total = 1, clash;
        for (;;) {
            free(names[i]);
            names[i] = name_with_depth(list->items[i].filename, depth, ext, &total);
            clash = 0;
            for (int j = 0; j < i; j++) {
                if (strcmp(names[j], names[i]) != 0) continue;
                if (strcmp(list->items[j].filename, list->items[i].filename) == 0)
                    continue;   /* the very same file listed twice */
                clash = 1;
                if (depth >= total && g_warn_collisions)
                    fprintf(stderr, "---- warning: %s '%s' and '%s' both become '%s' in the XML ----\n",
                            tag, list->items[j].name, list->items[i].name, names[i]);
                break;
            }
            if (!clash || depth >= total) break;
            depth++;
        }
    }
    return names;
}

/* Emits one <textures>...</textures> or <sounds>...</sounds> block for
 * `list` (g_cart_textures or g_cart_sounds), in original declaration
 * order -- that order IS the id (see driver.h's own doc comment on
 * CartResourceList), so this loop's own index has to match whatever
 * codegen.c's emit_cart_hint_defines assigned each name's #define to,
 * position for position. Each entry's own filename has its extension
 * swapped to `ext` (".vtex"/".vsnd") here, at XML-emission time -- not
 * stored pre-swapped in the list itself, since the list's only other
 * consumer (codegen.c's #define emission) never needed the swapped
 * form at all. Matches v32lua's own <texture>/<sound> element and
 * attribute names, and its own "<!-- var_name -->" trailing comment,
 * exactly -- confirmed directly from v32lua's source, not
 * approximated. Falls back to the empty, self-closing form when `list`
 * has nothing in it. */
static void emit_resource_list(FILE *xml, const CartResourceList *list,
                                const char *tag, const char *ext,
                                int preserve_paths) {
    if (list->count == 0) {
        fprintf(xml, "<%ss />\n", tag);
        return;
    }
    char **names = resolve_names(list, tag, ext, preserve_paths);
    fprintf(xml, "<%ss>\n", tag);
    for (int i = 0; i < list->count; i++) {
        fprintf(xml, "    <%s path=\"%s\" /> <!-- %s -->\n",
                tag, names[i], list->items[i].name);
        free(names[i]);
    }
    free(names);
    fprintf(xml, "</%ss>\n", tag);
}

char *cart_xml_filename(const char *output_filename, const char *source_filename,
                        int preserve_paths) {
    char *xml_name = replace_extension(output_filename, ".xml");
    if (!preserve_paths) return xml_name;
    /* Same file name as the default, but in the source file's directory. */
    char *dir = path_dirname(source_filename);
    size_t cap = strlen(dir) + strlen(xml_name) + 2;
    char *result = malloc(cap);
    snprintf(result, cap, "%s/%s", dir, path_basename(xml_name));
    free(dir);
    free(xml_name);
    return result;
}

/* Writes one XML. `variant` is "" for the plain build and "Opt" for the
 * optimized one: it goes before the extension of both the XML name and
 * the <binary> it references (game.xml/game.vbin, gameOpt.xml/gameOpt.vbin). */
static void write_cart_xml(const char *output_filename, const char *source_filename,
                           int is_bios, int preserve_paths, const char *variant) {
    char *plain_xml = cart_xml_filename(output_filename, source_filename,
                                        preserve_paths);
    char *variant_ext = malloc(strlen(variant) + 8);
    sprintf(variant_ext, "%s.xml", variant);
    char *xml_filename = replace_extension(plain_xml, variant_ext);
    free(plain_xml);
    sprintf(variant_ext, "%s.vbin", variant);
    char *vbin_path;
    if (preserve_paths) {
        char *xml_dir = path_dirname(xml_filename);
        char *vbin_full = replace_extension(output_filename, variant_ext);
        vbin_path = path_relative_to(xml_dir, vbin_full);
        free(xml_dir);
        free(vbin_full);
    } else {
        char *vbin_full = replace_extension(output_filename, variant_ext);
        vbin_path = strdup(path_basename(vbin_full));
        free(vbin_full);
    }

    free(variant_ext);
    FILE *xml = fopen(xml_filename, "w");
    if (xml == NULL) {
        perror(xml_filename);
        free(xml_filename);
        free(vbin_path);
        return;
    }

    /* Matches v32lua's own emit_cart_xml output exactly -- same
     * element/attribute names, same indentation (or lack of it --
     * v32lua's own <binary>/<textures>/<sounds>/</rom-definition>
     * lines are all emitted at column 0, only the <rom> line and each
     * individual <texture>/<sound> entry get the 4-space indent;
     * reproduced faithfully here rather than "improved," since
     * matching the sibling project's own convention was the point). */
    fprintf(xml, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n");
    fprintf(xml, "<rom-definition version=\"1.0\">\n");
    fprintf(xml, "    <rom type=\"%s\" title=\"%s\" version=\"%s\" />\n",
            is_bios ? "bios" : "cartridge",
            (g_cart_title != NULL) ? g_cart_title : "Vircon32 Program",
            (g_cart_version != NULL) ? g_cart_version : "1.0");
    fprintf(xml, "<binary path=\"%s\" />\n", vbin_path);
    emit_resource_list(xml, &g_cart_textures, "texture", ".vtex", preserve_paths);
    emit_resource_list(xml, &g_cart_sounds, "sound", ".vsnd", preserve_paths);
    fprintf(xml, "</rom-definition>\n");

    fclose(xml);
    free(xml_filename);
    free(vbin_path);
}

void emit_cart_xml(const char *output_filename, const char *source_filename,
                   int is_bios, int preserve_paths) {
    g_warn_collisions = 1;
    write_cart_xml(output_filename, source_filename, is_bios, preserve_paths, "");
    g_warn_collisions = 0;   /* the Opt twin would only repeat the warnings */
    write_cart_xml(output_filename, source_filename, is_bios, preserve_paths, "Opt");
}
