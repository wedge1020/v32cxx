/* ****************************************************************************
 *  libc/v32file.c -- files, on the memory card. Included once, by v32libc.c.
 *
 *  A Vircon32 console has no file system; what it has that outlives a
 *  session is the memory card, 262144 words. This gives a C program the
 *  stdio it expects on top of it: fopen / fread / fwrite / getc / putc /
 *  fclose / rewind / remove, and stat for "does it exist".
 *
 *  The card:
 *
 *      words 0..19    the game signature, as every Vircon32 game writes
 *                     one: v32file_signature, zero-padded
 *      20, 21         a magic number and a format version
 *      22..           the directory: V32FILE_MAX_FILES entries of a name
 *                     (V32FILE_NAME_LEN words), a start and a size
 *      then           the files, each one contiguous run of words
 *
 *  An open file lives in RAM: fopen reads the whole file in (or starts an
 *  empty one), and it is written back to the card when it is closed,
 *  flushed or rewound after a change. Space on the card is found then,
 *  first fit, so a file can grow or shrink freely from one save to the
 *  next and deleting one leaves its room to the others.
 *
 *  Everything is counted in WORDS. sizeof(char) is 1 and so is
 *  sizeof(int): fwrite(&n, sizeof n, 1, f) writes one word, and a
 *  "byte" written with putc occupies a word and comes back whole.
 *
 *  A card that is missing, or that belongs to another game (a different
 *  signature), is never written: fopen for writing fails with errno set.
 *  A blank card is formatted on first write.
 * ****************************************************************************/
#ifndef V32FILE_C
#define V32FILE_C

#include "v32libc.h"
#include "memcard.h"

#define V32FILE_CARD_WORDS  262144
#define V32FILE_SIGNATURE   20
#define V32FILE_MAGIC       0x56333246      /* "V32F" */
#define V32FILE_VERSION     1
#define V32FILE_ENTRY_WORDS (V32FILE_NAME_LEN + 2)
#define V32FILE_DIR_START   (V32FILE_SIGNATURE + 2)
#define V32FILE_DATA_START  (V32FILE_DIR_START + V32FILE_MAX_FILES * V32FILE_ENTRY_WORDS)
#define V32FILE_MAX_OPEN    4

char *v32file_signature = "V32LIBC FILES";

struct v32file_entry {
    char name[V32FILE_NAME_LEN];        /* name[0] == 0: a free entry */
    int start;
    int size;
};

static struct v32file_entry v32file_dir[V32FILE_MAX_FILES];
static FILE v32file_pool[V32FILE_MAX_OPEN];

/* ---- the card -------------------------------------------------------------- */

int v32file_card(void)
{
    char on_card[V32FILE_SIGNATURE];
    int header[2];
    int i, ours = 1, blank = 1, ended = 0;

    if (!card_is_connected())
        return V32FILE_NO_CARD;
    card_read_data(on_card, 0, V32FILE_SIGNATURE);
    for (i = 0; i < V32FILE_SIGNATURE; i++) {
        int expected = 0;

        if (!ended) {
            expected = v32file_signature[i];
            if (expected == 0)
                ended = 1;
        }
        if (on_card[i] != 0)
            blank = 0;
        if (on_card[i] != expected)
            ours = 0;
    }
    if (blank)
        return V32FILE_BLANK;
    if (!ours)
        return V32FILE_FOREIGN;
    card_read_data(header, V32FILE_SIGNATURE, 2);
    if (header[0] != V32FILE_MAGIC || header[1] != V32FILE_VERSION)
        return V32FILE_BLANK;           /* ours, but not in this format: start over */
    card_read_data(v32file_dir, V32FILE_DIR_START, V32FILE_MAX_FILES * V32FILE_ENTRY_WORDS);
    return V32FILE_READY;
}

static void v32file_write_dir(void)
{
    card_write_data(v32file_dir, V32FILE_DIR_START, V32FILE_MAX_FILES * V32FILE_ENTRY_WORDS);
}

void v32file_format(void)
{
    char signature[V32FILE_SIGNATURE];
    int header[2];
    int i, ended = 0;

    for (i = 0; i < V32FILE_SIGNATURE; i++) {
        signature[i] = 0;
        if (!ended) {
            signature[i] = v32file_signature[i];
            if (signature[i] == 0)
                ended = 1;
        }
    }
    memset(v32file_dir, 0, V32FILE_MAX_FILES * V32FILE_ENTRY_WORDS);
    header[0] = V32FILE_MAGIC;
    header[1] = V32FILE_VERSION;
    card_write_data(signature, 0, V32FILE_SIGNATURE);
    card_write_data(header, V32FILE_SIGNATURE, 2);
    v32file_write_dir();
}

/* The card, ready to be written: formats a blank one. 0 if it cannot be. */
static int v32file_writable_card(void)
{
    int state = v32file_card();

    if (state == V32FILE_BLANK) {
        v32file_format();
        return 1;
    }
    if (state == V32FILE_READY)
        return 1;
    errno = (state == V32FILE_NO_CARD) ? ENODEV : EACCES;
    return 0;
}

static int v32file_same_name(char *a, char *b)
{
    int i;

    for (i = 0; i < V32FILE_NAME_LEN - 1; i++) {
        if (a[i] != b[i])
            return 0;
        if (a[i] == '\0')
            return 1;
    }
    return 1;
}

/* The directory must have been read (v32file_card() == V32FILE_READY). */
static int v32file_lookup(char *name)
{
    int i;

    for (i = 0; i < V32FILE_MAX_FILES; i++)
        if (v32file_dir[i].name[0] != '\0' && v32file_same_name(v32file_dir[i].name, name))
            return i;
    return -1;
}

/* Where `size` words fit, not counting entry `skip`'s own room. -1: nowhere. */
static int v32file_find_room(int size, int skip)
{
    int start = V32FILE_DATA_START;
    int i, moved;

    do {
        moved = 0;
        for (i = 0; i < V32FILE_MAX_FILES; i++) {
            struct v32file_entry *e = &v32file_dir[i];

            if (i == skip || e->name[0] == '\0' || e->size == 0)
                continue;
            if (start < e->start + e->size && e->start < start + size) {
                start = e->start + e->size;
                moved = 1;
            }
        }
    } while (moved);
    if (start + size > V32FILE_CARD_WORDS)
        return -1;
    return start;
}

int v32file_free_words(void)
{
    int i, used = 0;

    if (v32file_card() != V32FILE_READY)
        return V32FILE_CARD_WORDS - V32FILE_DATA_START;
    for (i = 0; i < V32FILE_MAX_FILES; i++)
        if (v32file_dir[i].name[0] != '\0')
            used += v32file_dir[i].size;
    return V32FILE_CARD_WORDS - V32FILE_DATA_START - used;
}

/* ---- open files -------------------------------------------------------------- */

static int v32file_is_card_file(FILE *f)
{
    return f != NULL && f->fd == 3 && f->open;
}

static int v32file_reserve(FILE *f, int words)
{
    int capacity = f->capacity;
    int *data;

    if (words <= capacity)
        return 1;
    if (capacity < 256)
        capacity = 256;
    while (capacity < words)
        capacity *= 2;
    data = (int *)realloc(f->data, capacity);
    if (data == NULL) {
        f->error = 1;
        errno = ENOMEM;
        return 0;
    }
    f->data = data;
    f->capacity = capacity;
    return 1;
}

/* Writes a changed file back to the card. 0, or EOF. */
static int v32file_commit(FILE *f)
{
    int index, start, i;

    if (!f->dirty)
        return 0;
    if (!v32file_writable_card()) {
        f->error = 1;
        return EOF;
    }
    index = v32file_lookup(f->name);
    if (index < 0) {
        for (i = 0; i < V32FILE_MAX_FILES && index < 0; i++)
            if (v32file_dir[i].name[0] == '\0')
                index = i;
    }
    start = (index < 0) ? -1 : v32file_find_room(f->size, index);
    if (start < 0) {
        f->error = 1;
        errno = ENOSPC;
        return EOF;
    }
    if (f->size > 0)
        card_write_data(f->data, start, f->size);
    for (i = 0; i < V32FILE_NAME_LEN; i++)
        v32file_dir[index].name[i] = f->name[i];
    v32file_dir[index].start = start;
    v32file_dir[index].size = f->size;
    v32file_write_dir();
    f->dirty = 0;
    return 0;
}

FILE *fopen(char *name, char *mode)
{
    FILE *f = NULL;
    int i, index, state;
    int reading = (mode[0] == 'r');
    int update = (mode[1] == '+' || (mode[1] != '\0' && mode[2] == '+'));

    if (name == NULL || name[0] == '\0') {
        errno = ENOENT;
        return NULL;
    }
    state = v32file_card();
    index = (state == V32FILE_READY) ? v32file_lookup(name) : -1;
    if (reading && index < 0) {
        errno = (state == V32FILE_NO_CARD) ? ENODEV : ENOENT;
        return NULL;
    }
    if (!reading && !v32file_writable_card())
        return NULL;
    for (i = 0; i < V32FILE_MAX_OPEN && f == NULL; i++)
        if (!v32file_pool[i].open)
            f = &v32file_pool[i];
    if (f == NULL) {
        errno = EMFILE;
        return NULL;
    }
    memset(f, 0, sizeof (FILE));
    f->fd = 3;
    f->open = 1;
    f->readable = reading || update;
    f->writable = !reading || update;
    for (i = 0; i < V32FILE_NAME_LEN - 1 && name[i] != '\0'; i++)
        f->name[i] = name[i];
    if (index >= 0 && mode[0] != 'w') {
        /* "r", "r+", "a": what is on the card */
        int size = v32file_dir[index].size;

        if (!v32file_reserve(f, size)) {
            f->open = 0;
            return NULL;
        }
        if (size > 0)
            card_read_data(f->data, v32file_dir[index].start, size);
        f->size = size;
        if (mode[0] == 'a')
            f->pos = size;
    } else {
        f->dirty = 1;                   /* "w": it exists, empty, once closed */
    }
    return f;
}

int v32file_close(FILE *f)
{
    int result;

    if (!v32file_is_card_file(f))
        return 0;
    result = v32file_commit(f);
    if (f->data != NULL)
        free(f->data);
    f->data = NULL;
    f->open = 0;
    return result;
}

int v32file_flush(FILE *f)
{
    if (!v32file_is_card_file(f))
        return 0;
    return v32file_commit(f);
}

int v32file_getc(FILE *f)
{
    if (!v32file_is_card_file(f) || !f->readable)
        return EOF;
    if (f->pos >= f->size) {
        f->eof = 1;
        return EOF;
    }
    return f->data[f->pos++];
}

/* Stores the whole word; returns it as a character (never EOF). */
int v32file_putc(int c, FILE *f)
{
    if (!v32file_is_card_file(f) || !f->writable || !v32file_reserve(f, f->pos + 1))
        return EOF;
    f->data[f->pos++] = c;
    if (f->pos > f->size)
        f->size = f->pos;
    f->dirty = 1;
    return c & 255;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *f)
{
    int wanted = size * count;
    int left;

    if (!v32file_is_card_file(f) || !f->readable || wanted <= 0)
        return 0;
    left = f->size - f->pos;
    if (wanted > left) {
        wanted = (left / size) * size;
        f->eof = 1;
    }
    if (wanted > 0) {
        memcpy(ptr, &f->data[f->pos], wanted);
        f->pos += wanted;
    }
    return wanted / size;
}

size_t fwrite(void *ptr, size_t size, size_t count, FILE *f)
{
    int words = size * count;
    int i;

    if (words <= 0)
        return 0;
    if (f == stdout || f == stderr) {
        for (i = 0; i < words; i++)
            v32term_putc(((char *)ptr)[i]);
        return count;
    }
    if (!v32file_is_card_file(f) || !f->writable || !v32file_reserve(f, f->pos + words))
        return 0;
    memcpy(&f->data[f->pos], ptr, words);
    f->pos += words;
    if (f->pos > f->size)
        f->size = f->pos;
    f->dirty = 1;
    return count;
}

int fseek(FILE *f, int offset, int whence)
{
    int pos;

    if (!v32file_is_card_file(f))
        return -1;
    pos = offset;
    if (whence == SEEK_CUR)
        pos = f->pos + offset;
    else if (whence == SEEK_END)
        pos = f->size + offset;
    if (pos < 0 || !v32file_reserve(f, pos))
        return -1;
    f->pos = pos;
    f->eof = 0;
    return 0;
}

int ftell(FILE *f)
{
    return v32file_is_card_file(f) ? f->pos : -1;
}

/* A file a program keeps open for good (a score file) is written back
 * here, since it may never be closed. */
void rewind(FILE *f)
{
    if (!v32file_is_card_file(f))
        return;
    v32file_commit(f);
    f->pos = 0;
    f->eof = 0;
    f->error = 0;
}

int feof(FILE *f)
{
    return v32file_is_card_file(f) && f->eof;
}

int ferror(FILE *f)
{
    return v32file_is_card_file(f) && f->error;
}

int remove(char *name)
{
    int index;

    if (v32file_card() != V32FILE_READY || (index = v32file_lookup(name)) < 0) {
        errno = ENOENT;
        return -1;
    }
    v32file_dir[index].name[0] = '\0';
    v32file_dir[index].size = 0;
    v32file_write_dir();
    return 0;
}

int unlink(char *name)
{
    return remove(name);
}

int stat(char *name, struct stat *st)
{
    int index;

    if (v32file_card() != V32FILE_READY || (index = v32file_lookup(name)) < 0) {
        errno = ENOENT;
        return -1;
    }
    memset(st, 0, sizeof (struct stat));
    st->st_size = v32file_dir[index].size;
    st->st_nlink = 1;
    st->st_mode = 0600;
    return 0;
}

#endif /* V32FILE_C */
