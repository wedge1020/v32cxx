#include <stdio.h>
#include <stdlib.h>
#include "cartxml.h"
#include "pathutil.h"

void emit_cart_xml(const char *output_filename) {
    char *xml_filename = replace_extension(output_filename, ".xml");
    char *vbin_path = replace_extension(output_filename, ".vbin");

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
    fprintf(xml, "    <rom type=\"cartridge\" title=\"%s\" version=\"%s\" />\n",
            "Vircon32 Program", "1.0");
    fprintf(xml, "<binary path=\"%s\" />\n", vbin_path);
    fprintf(xml, "<textures />\n");
    fprintf(xml, "<sounds />\n");
    fprintf(xml, "</rom-definition>\n");

    fclose(xml);
    free(xml_filename);
    free(vbin_path);
}
