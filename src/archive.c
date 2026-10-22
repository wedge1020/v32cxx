/* archive.c -- libraries for the linker. See archive.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "archive.h"
#include "pathutil.h"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#define QUIET_STDERR " 2>nul"
#else
#define QUIET_STDERR " 2>/dev/null"
#endif

int path_is_archive(const char *path) {
    size_t n = strlen(path);
    return n > 5 && strcmp(path + n - 5, ".v32a") == 0;
}

/* ---- small string sets ---------------------------------------------------- */

typedef struct Names { char **v; int n, cap; } Names;

static int names_has(const Names *s, const char *x) {
    for (int i = 0; i < s->n; i++) if (strcmp(s->v[i], x) == 0) return 1;
    return 0;
}

static void names_add(Names *s, const char *x) {
    if (names_has(s, x)) return;
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 16; s->v = realloc(s->v, (size_t)s->cap * sizeof *s->v); }
    s->v[s->n++] = strdup(x);
}

/* what one translation unit defines and uses */
typedef struct Unit { Names defs, refs; } Unit;

/* ---- running this program again to list symbols ----------------------------- */

static void quote_arg(char *dst, size_t cap, const char *a) {
    size_t k = strlen(dst);
#ifdef _WIN32
    if (k + strlen(a) + 3 >= cap) return;
    dst[k++] = '"';
    for (; *a; a++) { if (*a == '"') dst[k++] = '\\'; dst[k++] = *a; }
    dst[k++] = '"';
#else
    if (k + 4 * strlen(a) + 3 >= cap) return;
    dst[k++] = '\'';
    for (; *a; a++) {
        if (*a == '\'') { memcpy(dst + k, "'\\''", 4); k += 4; }
        else dst[k++] = *a;
    }
    dst[k++] = '\'';
#endif
    dst[k] = '\0';
}

/* `self --list-symbols -c <fwd> <files>`: one Unit per file; NULL on failure */
static Unit *scan_files(const char *self, const char *const *fwd, int nfwd,
                        const char *const *files, int nfiles, int quiet) {
    size_t cap = 4096;
    for (int i = 0; i < nfwd; i++) cap += strlen(fwd[i]) * 4 + 4;
    for (int i = 0; i < nfiles; i++) cap += strlen(files[i]) * 4 + 4;
    char *cmd = calloc(cap + strlen(self) * 4, 1);
    quote_arg(cmd, cap + strlen(self) * 4, self);
    strcat(cmd, " --list-symbols -c --no-build-scripts");
    for (int i = 0; i < nfwd; i++) { strcat(cmd, " "); quote_arg(cmd, cap + strlen(self) * 4, fwd[i]); }
    for (int i = 0; i < nfiles; i++) { strcat(cmd, " "); quote_arg(cmd, cap + strlen(self) * 4, files[i]); }
    if (quiet) strcat(cmd, QUIET_STDERR);
    FILE *p = popen(cmd, "r");
    free(cmd);
    if (p == NULL) { perror("popen"); return NULL; }
    Unit *u = calloc((size_t)nfiles + 1, sizeof *u);
    int cur = -1;
    char line[1024];
    while (fgets(line, sizeof line, p) != NULL) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (strncmp(line, "tu ", 3) == 0) { cur = atoi(line + 3); continue; }
        if (cur < 0 || cur >= nfiles || line[0] == '\0' || line[1] != ' ') continue;
        if (line[0] == 'd') names_add(&u[cur].defs, line + 2);
        else if (line[0] == 'r') names_add(&u[cur].refs, line + 2);
    }
    int status = pclose(p);
    if (status != 0 || cur < 0) {
        free(u);
        return NULL;
    }
    return u;
}

/* ---- making an index ---------------------------------------------------------- */

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

int archive_make(const char *archive_path, const char *self, const char *const *fwd, int nfwd,
                 const char *const *members, int nmembers) {
    for (int i = 0; i < nmembers; i++) {
        if (file_size(members[i]) < 0) {
            fprintf(stderr, "---- error: cannot open library member '%s' ----\n", members[i]);
            return 1;
        }
    }
    Unit *u = scan_files(self, fwd, nfwd, members, nmembers, 0);
    if (u == NULL) {
        fprintf(stderr, "---- error: could not read the library's members (see above) ----\n");
        return 1;
    }
    FILE *out = fopen(archive_path, "w");
    if (out == NULL) {
        fprintf(stderr, "---- error: cannot write '%s' ----\n", archive_path);
        return 1;
    }
    char *dir = path_dirname(archive_path);
    fprintf(out, "v32a 1\n");
    for (int i = 0; i < nmembers; i++) {
        for (int j = 0; j < i; j++)
            for (int k = 0; k < u[i].defs.n; k++)
                if (names_has(&u[j].defs, u[i].defs.v[k]))
                    fprintf(stderr, "warning: '%s' is defined in both %s and %s\n",
                            u[i].defs.v[k], members[j], members[i]);
        char *rel = path_relative_to(dir, members[i]);
        fprintf(out, "member %ld %s\n", file_size(members[i]), rel);
        free(rel);
        for (int k = 0; k < u[i].defs.n; k++) fprintf(out, "d %s\n", u[i].defs.v[k]);
        for (int k = 0; k < u[i].refs.n; k++) fprintf(out, "r %s\n", u[i].refs.v[k]);
    }
    free(dir);
    fclose(out);
    return 0;
}

/* ---- using libraries ------------------------------------------------------------- */

typedef struct Member { char *path; Unit u; int selected; } Member;
typedef struct Lib { char *name; Member *m; int n; } Lib;

static int load_lib(const char *path, Lib *lib) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "---- error: cannot open library '%s' ----\n", path);
        return -1;
    }
    char line[2048];
    if (fgets(line, sizeof line, f) == NULL || strncmp(line, "v32a 1", 6) != 0) {
        fprintf(stderr, "---- error: '%s' is not a v32c++ library index (rebuild it with --mklib) ----\n", path);
        fclose(f);
        return -1;
    }
    char *dir = path_dirname(path);
    lib->name = strdup(path);
    lib->m = NULL;
    lib->n = 0;
    while (fgets(line, sizeof line, f) != NULL) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (strncmp(line, "member ", 7) == 0) {
            long size = strtol(line + 7, NULL, 10);
            const char *rel = strchr(line + 7, ' ');
            if (rel == NULL) continue;
            rel++;
            lib->m = realloc(lib->m, (size_t)(lib->n + 1) * sizeof *lib->m);
            Member *m = &lib->m[lib->n++];
            memset(m, 0, sizeof *m);
            size_t len = strlen(dir) + strlen(rel) + 2;
            m->path = malloc(len);
            snprintf(m->path, len, "%s/%s", dir, rel);
            if (strcmp(dir, ".") == 0) snprintf(m->path, len, "%s", rel);
            long now = file_size(m->path);
            if (now != size) {
                fprintf(stderr, "---- error: library '%s' is out of date: member '%s' %s "
                                "(rebuild it with --mklib) ----\n", path, m->path,
                        now < 0 ? "is missing" : "has changed");
                free(dir);
                fclose(f);
                return -1;
            }
        } else if (lib->n > 0 && line[0] == 'd' && line[1] == ' ') {
            names_add(&lib->m[lib->n - 1].u.defs, line + 2);
        } else if (lib->n > 0 && line[0] == 'r' && line[1] == ' ') {
            names_add(&lib->m[lib->n - 1].u.refs, line + 2);
        }
    }
    free(dir);
    fclose(f);
    return 0;
}

int archive_select(const char *self, const char *const *fwd, int nfwd,
                   const char *const *libs, int nlibs,
                   const char ***inputs, int *nin) {
    Lib *lib = calloc((size_t)nlibs + 1, sizeof *lib);
    for (int i = 0; i < nlibs; i++)
        if (load_lib(libs[i], &lib[i]) != 0) return -1;

    Unit *u = scan_files(self, fwd, nfwd, *inputs, *nin, 1);
    if (u == NULL) return 0;     /* the real parse will report what is wrong */

    Names defined = { NULL, 0, 0 }, needed = { NULL, 0, 0 };
    for (int i = 0; i < *nin; i++)
        for (int k = 0; k < u[i].defs.n; k++) names_add(&defined, u[i].defs.v[k]);
    for (int i = 0; i < *nin; i++)
        for (int k = 0; k < u[i].refs.n; k++)
            if (!names_has(&defined, u[i].refs.v[k])) names_add(&needed, u[i].refs.v[k]);

    /* a member that defines something needed is taken, and what it needs
     * in turn; round and round until nothing more is wanted */
    for (int changed = 1; changed;) {
        changed = 0;
        for (int l = 0; l < nlibs; l++) {
            for (int j = 0; j < lib[l].n; j++) {
                Member *m = &lib[l].m[j];
                if (m->selected) continue;
                int wanted = 0;
                for (int k = 0; k < m->u.defs.n && !wanted; k++)
                    if (names_has(&needed, m->u.defs.v[k]) && !names_has(&defined, m->u.defs.v[k])) wanted = 1;
                if (!wanted) continue;
                m->selected = 1;
                changed = 1;
                for (int k = 0; k < m->u.defs.n; k++) names_add(&defined, m->u.defs.v[k]);
                for (int k = 0; k < m->u.refs.n; k++)
                    if (!names_has(&defined, m->u.refs.v[k])) names_add(&needed, m->u.refs.v[k]);
            }
        }
    }
    for (int l = 0; l < nlibs; l++) {
        for (int j = 0; j < lib[l].n; j++) {
            if (!lib[l].m[j].selected) continue;
            char *key = path_relative_to(".", lib[l].m[j].path);
            int explicit_input = 0;
            for (int i = 0; i < *nin; i++) {
                char *k2 = path_relative_to(".", (*inputs)[i]);
                if (strcmp(key, k2) == 0) explicit_input = 1;
                free(k2);
            }
            free(key);
            if (explicit_input) continue;
            *inputs = realloc((void *)*inputs, (size_t)(*nin + 1) * sizeof **inputs);
            (*inputs)[(*nin)++] = lib[l].m[j].path;
        }
    }
    return 0;
}
