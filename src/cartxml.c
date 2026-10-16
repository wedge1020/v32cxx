#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cartxml.h"
#include "driver.h"
#include "pathutil.h"

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
 * has nothing in it (still the ordinary case for now -- this project
 * has cart-hint recognition as of this round, but plenty of programs
 * simply won't use any textures or sounds at all). */
static void emit_resource_list(FILE *xml, const CartResourceList *list,
                                const char *tag, const char *ext,
                                int preserve_paths) {
    if (list->count == 0) {
        fprintf(xml, "<%ss />\n", tag);
        return;
    }
    fprintf(xml, "<%ss>\n", tag);
    for (int i = 0; i < list->count; i++) {
        char *resource = replace_extension(
            preserve_paths ? list->items[i].filename
                           : path_basename(list->items[i].filename), ext);
        if (!preserve_paths) {
            for (int j = 0; j < i; j++) {
                if (strcmp(path_basename(list->items[j].filename),
                           path_basename(list->items[i].filename)) == 0 &&
                    strcmp(list->items[j].filename, list->items[i].filename) != 0) {
                    fprintf(stderr, "---- warning: %s '%s' and '%s' both become '%s' in the XML; "
                                    "use --xml-preserve-paths to keep their directories ----\n",
                            tag, list->items[j].name, list->items[i].name, resource);
                }
            }
        }
        fprintf(xml, "    <%s path=\"%s\" /> <!-- %s -->\n",
                tag, resource, list->items[i].name);
        free(resource);
    }
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

void emit_cart_xml(const char *output_filename, const char *source_filename,
                   int is_bios, int preserve_paths) {
    char *xml_filename = cart_xml_filename(output_filename, source_filename,
                                           preserve_paths);
    char *vbin_path;
    if (preserve_paths) {
        char *xml_dir = path_dirname(xml_filename);
        char *vbin_full = replace_extension(output_filename, ".vbin");
        vbin_path = path_relative_to(xml_dir, vbin_full);
        free(xml_dir);
        free(vbin_full);
    } else {
        char *vbin_full = replace_extension(output_filename, ".vbin");
        vbin_path = strdup(path_basename(vbin_full));
        free(vbin_full);
    }

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
