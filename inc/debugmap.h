#ifndef DEBUGMAP_H
#define DEBUGMAP_H

/*
 * Tracks the C-output-line <-> C++-source-line mapping codegen.c builds
 * up as it writes, for main.c's own -g (debug file) output -- modeled on
 * the format of the example .asm.debug file Matthew provided (a real
 * C-to-assembly debug map from the Vircon32 toolchain itself): a SPARSE
 * table, one entry per point where the mapping actually changes, not an
 * exhaustive per-output-line listing. Each entry may optionally carry a
 * function name, for the specific line a function's own C definition
 * begins on.
 *
 * This module only stores and writes entries -- deciding WHEN a new
 * entry is worth recording (has the current AST node's own source line
 * actually changed since the last one?) is codegen.c's own job, since
 * that's where the AST and the output-line counter both live. Always
 * populated during codegen_run() regardless of whether -g was actually
 * given -- main.c decides whether to call debug_map_write() at all,
 * the same "always track, conditionally write" split this project
 * already uses for g_preprocessor_lines/g_cart_textures/g_cart_sounds.
 */

typedef struct DebugMapEntry {
    int c_line;
    int cpp_line;
    char *function_name; /* NULL unless this entry marks a function's own first line of C output */
} DebugMapEntry;

typedef struct DebugMap {
    DebugMapEntry *items;
    int count;
    int capacity;
} DebugMap;

extern DebugMap g_debug_map;

/* Appends one entry -- always appends, no deduplication of its own;
 * the caller (codegen.c) is the one deciding whether this point is
 * actually worth recording. `function_name` may be NULL; when given,
 * it's copied (strdup'd), not just referenced. */
void debug_map_record(int c_line, int cpp_line, const char *function_name);

/* Frees every entry (including each one's own function_name, if any)
 * and resets g_debug_map to empty. Call once after the map has been
 * written out (or, for a run that never calls debug_map_write() at
 * all -- -g wasn't given -- there's nothing wrong with just leaving it
 * unfreed at process exit either; main.c calls this unconditionally
 * anyway, for the same process hygiene reasons it frees everything
 * else). */
void debug_map_free(void);

/* Writes every recorded entry to `path`, one per line, in the format
 * Matthew's own example .asm.debug file uses:
 *     c_path,c_line,cpp_path,cpp_line[,function_name]
 * `c_path`/`cpp_path` are written EXACTLY as given -- no path
 * normalization, resolution, or validation of any kind happens here.
 * Returns 1 on success; on failure to open `path` for writing, reports
 * to stderr (via perror) and returns 0 without writing anything. */
int debug_map_write(const char *path, const char *c_path, const char *cpp_path);

#endif /* DEBUGMAP_H */
