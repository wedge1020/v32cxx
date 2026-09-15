#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debugmap.h"

DebugMap g_debug_map = {NULL, 0, 0};

void debug_map_record(int c_line, int cpp_line, const char *function_name) {
    if (g_debug_map.count == g_debug_map.capacity) {
        g_debug_map.capacity = g_debug_map.capacity ? g_debug_map.capacity * 2 : 32;
        g_debug_map.items = realloc(g_debug_map.items,
                                     sizeof(DebugMapEntry) * (size_t)g_debug_map.capacity);
    }
    g_debug_map.items[g_debug_map.count].c_line = c_line;
    g_debug_map.items[g_debug_map.count].cpp_line = cpp_line;
    g_debug_map.items[g_debug_map.count].function_name =
        (function_name != NULL) ? strdup(function_name) : NULL;
    g_debug_map.count++;
}

void debug_map_free(void) {
    for (int i = 0; i < g_debug_map.count; i++) {
        free(g_debug_map.items[i].function_name);
    }
    free(g_debug_map.items);
    g_debug_map.items = NULL;
    g_debug_map.count = 0;
    g_debug_map.capacity = 0;
}

int debug_map_write(const char *path, const char *c_path, const char *cpp_path) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        perror(path);
        return 0;
    }
    for (int i = 0; i < g_debug_map.count; i++) {
        DebugMapEntry *e = &g_debug_map.items[i];
        if (e->function_name != NULL) {
            fprintf(f, "%s,%d,%s,%d,%s\n", c_path, e->c_line, cpp_path, e->cpp_line, e->function_name);
        } else {
            fprintf(f, "%s,%d,%s,%d\n", c_path, e->c_line, cpp_path, e->cpp_line);
        }
    }
    fclose(f);
    return 1;
}
