#ifndef MACRO_H
#define MACRO_H

/*
 * C++-side macro support for the include pre-scan (prescan.c).
 *
 * v32c++ used to carry every #define through to the generated C without
 * reading it, so a #define'd name was invisible to its own parser -- an
 * array size could only be an integer literal, a float #define was an
 * unknown-typed name to sema, and #ifdef gated nothing. This module gives
 * the pre-scan a real macro table and expander, so the parser sees source
 * that has already been macro-expanded, exactly as a C++ compiler would:
 *
 *   - object-like (#define N 5) and function-like (#define SQ(x) ((x)*(x)))
 *     macros, # stringizing, ## pasting, variadic (...)/__VA_ARGS__ (with
 *     GNU `, ## __VA_ARGS__` comma swallowing), #undef;
 *   - rescanning with standard self-reference protection (a macro is never
 *     re-expanded inside its own expansion);
 *   - __FILE__, __LINE__ and __V32CXX__ predefined;
 *   - #if/#elif expression evaluation (integer arithmetic, defined(...)).
 *
 * Every #define is ALSO passed through to the generated C whenever the
 * Vircon32 C preprocessor can accept it (see macro_passthrough_ok), so the
 * names stay available downstream -- to pass-through .h headers and anyone
 * reading the output -- even though no use of them is left in the code.
 */

typedef struct Macro {
    char *name;
    int is_function;     /* written NAME( with no space before the '(' */
    int param_count;     /* named parameters, NOT counting a trailing ... */
    char **params;
    int is_variadic;     /* last parameter is ... (__VA_ARGS__) */
    char *body;          /* replacement text, comments stripped, trimmed */
    const char *file;    /* where it was defined (interned), for diagnostics */
    int line;
    int is_builtin;      /* predefined (__V32CXX__): never passed through */
} Macro;

/* Status codes for macro_expand_text. */
#define MACRO_OK         0
#define MACRO_NEED_MORE  1   /* a function-like macro's argument list (or
                                its '(') may continue on the next line */
#define MACRO_ERROR     -1   /* message already printed */

/* Parses the text after "#define" (`rest`) and records the macro.
 * Returns the new Macro, or NULL on a malformed definition (message
 * already printed, mentioning file:line). */
Macro *macro_define(const char *rest, const char *file, int line);

/* -D NAME / -D NAME=VALUE from the command line. Returns the Macro or
 * NULL if NAME isn't an identifier. */
Macro *macro_define_cmdline(const char *spec);

/* #undef NAME / -U NAME. Unknown names are silently ignored, as in C. */
void macro_undef(const char *name);

Macro *macro_lookup(const char *name);
int macro_count(void);

/* 1 when the Vircon32 C preprocessor can accept this definition verbatim:
 * no # or ## in the body, not variadic, and (object-like) no direct
 * self-reference -- each of which is a hard error there. */
int macro_passthrough_ok(const Macro *m);

/* Writes "#define NAME[(params)] body" for `m` into a malloc'd string. */
char *macro_format_define(const Macro *m);

/* Expands every macro invocation in `text` (which may span several lines
 * and may start inside a block comment, per `starts_in_comment`).
 * Comments, string and character literals are copied untouched. On
 * MACRO_OK, *out is a malloc'd expanded copy. MACRO_NEED_MORE (only when
 * `may_need_more` is set) means the text ends where a function-like
 * invocation might still continue: append the next line and call again.
 * `file`/`line` feed __FILE__/__LINE__ and diagnostics. */
int macro_expand_text(const char *text, int starts_in_comment, int may_need_more,
                      const char *file, int line, char **out);

/* Evaluates a #if/#elif expression (the text after the directive word).
 * Sets *value and returns 0, or returns -1 with a message printed. */
int macro_eval_condition(const char *expr, const char *file, int line, long *value);

/* 1 unless `name` was ever #undef'd or redefined differently -- i.e. the
 * passed-through #define block at the top of the generated C gives it
 * the same value everywhere, so codegen may print the name instead of
 * its value. */
int macro_name_is_stable(const char *name);

/* Identifier helpers shared with prescan.c. */
int macro_is_ident_start(int c);
int macro_is_ident_char(int c);

#endif /* MACRO_H */
