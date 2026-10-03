%{
/*
 * lexer.l -- flex scanner.
 *

 * The interesting  part is IDENTIFIER classification  and the COLONCOLON
 * rule  right  below  it:  together  they  implement  the  "lexer  hack"
 * (deciding TYPE_NAME vs IDENTIFIER by consulting the symbol table) plus
 * qualified- name  (`v32::Timer`) support,  entirely in scan  order. See
 * the long comment on symtab_arm_qualifier()  in symtab.h for *why* this
 * lives here and not in a parser reduction action.
 *
 * SIMPLIFICATION:  no real  preprocessor.  Real C++  source needs  macro
 * expansion and conditional compilation handled before this scanner ever
 * sees it -- neither happens here. As an interim measure, though, a line
 * starting  with '#'  is  no longer  silently  discarded: it's  captured
 * verbatim (see g_preprocessor_lines, driver.h) so codegen.c can re-emit
 * it at  the top of the  generated file, ahead of  everything else. This
 * means a `#include "video.h"` a  person actually wrote now survives the
 * round trip  instead of having  to be  manually re-added by  hand every
 * time -- but it is still just pass-through, not preprocessing: no macro
 * expansion,  no  #include  resolution,  no  #ifdef  evaluation,  and  a
 * captured  #ifdef/#endif  pair  doesn't  actually  gate  anything  this
 * scanner sees, since it never interprets what was captured at all.
 *
 * As of  the include  pre-scan (see  prescan.c/prescan.h, run  by main.c
 * before  yyparse),  `#include`  lines  targeting  .hpp/.cpp  files  are
 * resolved  and expanded  into  the source  stream  BEFORE this  scanner
 * ever  sees  them  -- so  this  scanner  only  ever  lays eyes  on  the
 * pass-through  kind (.h/system  includes). The  line markers  that keep
 * g_lex_lineno/g_current_filename  honest   across  that   splicing  are
 * consumed by the dedicated rule below, and everything else about #-line
 * handling here is unchanged.
 *
 * ONE exception to "every `#`-line  is just passed through unread": cart
 * hints  (`#texture  NAME "file.png"`  /  `#sound  NAME "file.wav"`),  a
 * targeted special-case  recognized directly below,  NOT the start  of a
 * general  preprocessor  -- these  two  specific  forms get  parsed  for
 * their NAME/filename and registered (see g_cart_textures/g_cart_sounds,
 * driver.h),  instead of  falling  through to  the generic  pass-through
 * above. Modeled  directly on v32lua's own  `--#texture`/`--#sound` cart
 * hints.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver.h"
#include "parser.h"
#include "generic.h"
#include "prescan.h" /* line-marker consumption -- see the new rule below */

/* Suppresses a warning in flex's OWN generated scanning machinery
 * (yy_get_next_buffer's buffer-resize check compares yy_size_t, an
 * unsigned type, against a plain int) -- not anything this project's
 * own code does; nothing here to fix on this project's side, since the
 * comparison lives entirely inside code flex itself emits below this
 * point, regenerated fresh from the flex skeleton every time and never
 * hand-edited. Confirmed present since the very first working build of
 * this project, mentioned in this file's own header comment as a known,
 * pre-existing, not-our-code issue from day one -- silencing it here
 * finally addresses the actual warning text directly, at the one place
 * that can, rather than continuing to just document it as expected in
 * every build report. No matching `#pragma ... pop` -- this generated
 * file is compiled standalone, never `#include`d elsewhere, so the
 * pragma's effect ending at end-of-file is exactly the intended scope. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"

/* Remembers the Symbol* (if any) resolved for the most recently scanned
 * IDENTIFIER/TYPE_NAME token, so that if the *next* token turns out to be
 * "::", we know what to arm the qualifier context with. Reset to NULL by
 * every other token so stale state can't leak across unrelated tokens
 * (e.g. "foo ; bar :: baz" must not treat foo as bar's qualifier). */
static Symbol *g_last_ident_sym = NULL;

/* Macro-name annotation handoff (see the "@" rule below). The pre-scan
 * writes `@MAX@5` for a use of `#define MAX 5`; the "@" rule stores the
 * name in g_pending_macro, and the very next rule to run -- the literal
 * immediately after it -- finds it moved into g_lit_macro by this macro
 * (which runs before every rule's own action), then takes ownership.
 * Any other rule simply lets it go, so a stale name can never attach to
 * a later, unrelated literal. */
static char *g_pending_macro = NULL;
static char *g_lit_macro = NULL;
static int g_pending_negate = 0;   /* `@NAME@-5`: a negative constant */
static int g_lit_negate = 0;

static char *take_lit_macro(void) {
    char *m = g_lit_macro;
    g_lit_macro = NULL;
    return m;
}

/* The sign a "@NAME@-" annotation folded away, applied to the literal. */
#define LIT_SIGN (g_lit_negate ? -1 : 1)

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = g_lex_lineno; \
    free(g_lit_macro); g_lit_macro = g_pending_macro; g_pending_macro = NULL; \
    g_lit_negate = g_pending_negate; g_pending_negate = 0;

/* Appends one cart resource (a `#texture`/`#sound` hint) to `list`,
 * keeping id == list position exactly the way v32lua's own
 * cart_resource_append does (confirmed from its actual source) -- the
 * resource's id is never stored explicitly anywhere; it's always just
 * "whatever position this entry ends up at in the list," read back out
 * at both #define-emission time (codegen.c) and XML-emission time
 * (cartxml.c). */
static void register_cart_resource(CartResourceList *list, const char *name, const char *filename) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->items = realloc(list->items, sizeof(CartResource) * (size_t)list->capacity);
    }
    list->items[list->count].name = strdup(name);
    list->items[list->count].filename = strdup(filename);
    list->count++;
}
%}

%option noyywrap
%option nounput
%option noinput

DIGIT   [0-9]
ALPHA   [A-Za-z_]
ALNUM   [A-Za-z0-9_]

%%

"//"[^\n]*                     { /* line comment, discard */ }
"/*"([^*]|\*+[^*/])*\*+"/"      { /* block comment, discard */ }

    /* Cart hints -- `#texture NAME "file.png"` / `#sound NAME "file.wav"`.
     * Matched BEFORE the generic "#"-line pass-through rule below,
     * deliberately: both would otherwise match the same full line (same
     * length), and flex's own "first rule wins on a length tie" behavior
     * means ORDER in this file is what actually decides which one a
     * cart hint gets recognized by -- these have to come first, or every
     * `#texture`/`#sound` line would just fall through to plain,
     * unrecognized pass-through instead. NAME is captured exactly as
     * written -- upper, lower, or mixed case, never forced to any
     * particular one. Does NOT also add the line to
     * g_preprocessor_lines -- a recognized cart hint is translated into
     * a `#define`, not passed through verbatim; only a genuinely
     * unrecognized `#`-line falls through to that. */
^[ \t]*"#texture"[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+\"[^"\n]*\"[^\n]* {
    char name[128] = {0};
    char filename[256] = {0};
    if (sscanf(yytext, "#texture %127s \"%255[^\"]\"", name, filename) == 2) {
        register_cart_resource(&g_cart_textures, name, filename);
    }
}

^[ \t]*"#sound"[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+\"[^"\n]*\"[^\n]* {
    char name[128] = {0};
    char filename[256] = {0};
    if (sscanf(yytext, "#sound %127s \"%255[^\"]\"", name, filename) == 2) {
        register_cart_resource(&g_cart_sounds, name, filename);
    }
}

    /* `#title "..."` / `#version ...` -- same "matched before the
     * generic pass-through, so it's recognized instead of just carried
     * through verbatim" reasoning as #texture/#sound above. title is
     * quoted (may contain spaces); version is a bare token (matching
     * v32lua's own "--#version 1.1", never quoted there either). Frees
     * any PREVIOUS value before overwriting -- a program with more than
     * one #title/#version simply keeps the last one seen, the same
     * "last write wins" behavior v32lua's own title/version hints have
     * (they just overwrite the same fixed-size buffer each time). */
^[ \t]*"#title"[ \t]+\"[^"\n]*\"[^\n]* {
    char title[256] = {0};
    if (sscanf(yytext, "#title \"%255[^\"]\"", title) == 1) {
        free(g_cart_title);
        g_cart_title = strdup(title);
    }
}

^[ \t]*"#version"[ \t]+[^ \t\n]+[^\n]* {
    char version[64] = {0};
    if (sscanf(yytext, "#version %63s", version) == 1) {
        free(g_cart_version);
        g_cart_version = strdup(version);
    }
}

    /* Line marker emitted by prescan.c (see prescan.h): `# N "file"`.
     * Consumed HERE, never passed through, because it is this
     * project's own bookkeeping, not the person's source. Sets
     * g_lex_lineno directly to N rather than N-1 because this
     * pattern swallows the marker's own newline -- the \n rule's
     * increment never runs for this line, so the next content line
     * is already counted as N. g_current_filename is re-pointed at
     * an interned copy (see prescan_intern_filename) so the storage
     * stays valid for the rest of the run. */
^[ \t]*"#"[ \t]+{DIGIT}+[ \t]+\"[^\"]*\"[ \t]*\n  {
    int line_no;
    char fname[512];
    if (sscanf(yytext, "# %d \"%511[^\"]\"", &line_no, fname) == 2) {
        g_lex_lineno = line_no;
        g_current_filename = prescan_intern_filename(fname);
    }
}

^[ \t]*"#"[^\n]*                     {
    /* Preprocessor pass-through -- see this file's own header comment
     * and driver.h's PreprocessorLines for the full reasoning. Grows
     * g_preprocessor_lines by one and stores this line's text verbatim,
     * in original order; still returns no token, so the parser's own
     * grammar is completely unaffected by this -- codegen.c is the only
     * thing that ever reads this list. */
    if (g_preprocessor_lines.count == g_preprocessor_lines.capacity) {
        g_preprocessor_lines.capacity = g_preprocessor_lines.capacity ? g_preprocessor_lines.capacity * 2 : 8;
        g_preprocessor_lines.lines = realloc(g_preprocessor_lines.lines,
                                              sizeof(char *) * (size_t)g_preprocessor_lines.capacity);
    }
    g_preprocessor_lines.lines[g_preprocessor_lines.count++] = strdup(yytext);
}

\n                              { g_lex_lineno++; }

    /* `@NAME@` -- the pre-scan's annotation in front of a literal that
     * came from the object-like macro NAME (see YY_USER_ACTION above and
     * macro.c's annotate rule). Never a token itself; '@' cannot appear
     * in C++ source outside a string, comment or this annotation. */
"@"{ALPHA}{ALNUM}*"@"           {
    g_pending_macro = strndup(yytext + 1, (size_t)yyleng - 2);
}

    /* `#define LOW -1`: the annotation swallows the minus, and the literal
     * right after it takes the negative value -- so the parser sees ONE
     * literal, -1, named LOW, never `-(LOW)` (which would print back as
     * -LOW, i.e. +1). */
"@"{ALPHA}{ALNUM}*"@-"          {
    g_pending_macro = strndup(yytext + 1, (size_t)yyleng - 3);
    g_pending_negate = 1;
}
[ \t\r]+                        { /* whitespace */ }

"class"      { g_last_ident_sym = NULL; return CLASS; }
"struct"     { g_last_ident_sym = NULL; return STRUCT; }
"enum"       { g_last_ident_sym = NULL; return ENUM; }
"union"      { g_last_ident_sym = NULL; return UNION; }
"public"     { g_last_ident_sym = NULL; return PUBLIC; }
"private"    { g_last_ident_sym = NULL; return PRIVATE; }
"protected"  { g_last_ident_sym = NULL; return PROTECTED; }
"namespace"  { g_last_ident_sym = NULL; return NAMESPACE; }
"typedef"    { g_last_ident_sym = NULL; return TYPEDEF; }
"native"     { g_last_ident_sym = NULL; return NATIVE; }
"return"     { g_last_ident_sym = NULL; return RETURN; }
"if"         { g_last_ident_sym = NULL; return IF; }
"else"       { g_last_ident_sym = NULL; return ELSE; }
"do"         { g_last_ident_sym = NULL; return DO; }
"while"      { g_last_ident_sym = NULL; return WHILE; }
"for"        { g_last_ident_sym = NULL; return FOR; }
"break"      { g_last_ident_sym = NULL; return BREAK; }
"continue"   { g_last_ident_sym = NULL; return CONTINUE; }
"goto"       { g_last_ident_sym = NULL; return GOTO; }
"asm"        { g_last_ident_sym = NULL; return ASM; }
"__asm"      { g_last_ident_sym = NULL; return ASM; }
"__asm__"    { g_last_ident_sym = NULL; return ASM; }
    /* `volatile` is a token only where the grammar uses it -- gcc-style
     * `asm volatile ( ... )` / `volatile asm ( ... )`. As a type qualifier
     * (`volatile int ticks;`) it is accepted and dropped: nothing in a
     * single-threaded Vircon32 cart changes a variable behind the
     * compiler's back, and Vircon32 C has no such keyword. */
"volatile"/[ \t\r\n]*("("|"asm"|"__asm")  { g_last_ident_sym = NULL; return VOLATILE; }
"volatile"         { g_last_ident_sym = NULL; }
"__volatile__"     { g_last_ident_sym = NULL; return VOLATILE; }
"switch"     { g_last_ident_sym = NULL; return SWITCH; }
"case"       { g_last_ident_sym = NULL; return CASE; }
"default"    { g_last_ident_sym = NULL; return DEFAULT; }
"int"        { g_last_ident_sym = NULL; return INT_KW; }
"float"      { g_last_ident_sym = NULL; return FLOAT_KW; }
"void"       { g_last_ident_sym = NULL; return VOID_KW; }
"bool"       { g_last_ident_sym = NULL; return BOOL_KW; }
"char"       { g_last_ident_sym = NULL; return CHAR_KW; }
"new"        { g_last_ident_sym = NULL; return NEW; }
"delete"     { g_last_ident_sym = NULL; return DELETE; }
"static_cast"       { g_last_ident_sym = NULL; return STATIC_CAST; }
"dynamic_cast"      { g_last_ident_sym = NULL; return DYNAMIC_CAST; }
"const_cast"        { g_last_ident_sym = NULL; return CONST_CAST; }
"reinterpret_cast"  { g_last_ident_sym = NULL; return REINTERPRET_CAST; }
"this"       { g_last_ident_sym = NULL; return THIS; }
"virtual"    { g_last_ident_sym = NULL; return VIRTUAL; }
"const"      { g_last_ident_sym = NULL; return CONST; }
"operator"   { g_last_ident_sym = NULL; return OPERATOR; }
"sizeof"     { g_last_ident_sym = NULL; return SIZEOF; }
"true"       { g_last_ident_sym = NULL; return TRUE_KW; }
"false"      { g_last_ident_sym = NULL; return FALSE_KW; }
"nullptr"    { g_last_ident_sym = NULL; return NULLPTR_KW; }
"friend"     { g_last_ident_sym = NULL; return FRIEND; }

    /* `inline` and `register` are pure hints in a single translation unit
     * (v32c++ always transpiles one): accepted and dropped, so code
     * written with them -- headers especially -- doesn't need editing.
     * `static`/`extern` need more care (a static local keeps its value
     * between calls) -- see their own rules just below. */
"inline"     { g_last_ident_sym = NULL; }
"register"   { g_last_ident_sym = NULL; }

    /* `extern` is dropped too: with one translation unit, an `extern int
     * x;` IS the variable unless the file also defines it, and
     * merge_tentative_globals (ast.c) folds the two declarations into
     * one in that case. An extern function declaration is just a
     * prototype. */
"extern"     { g_last_ident_sym = NULL; }

    /* `static` depends on where it stands, which the scope stack knows:
     *   - at file or namespace scope it only limits linkage, and there is
     *     nothing to link against: dropped;
     *   - inside a function body it changes the variable's LIFETIME, so
     *     it reaches the grammar (stmt: STATIC var_decl) and the variable
     *     is hoisted to file scope by hoist_static_locals (ast.c);
     *   - inside a class body it would declare a static member, which
     *     this project doesn't have. */
"static"     {
    g_last_ident_sym = NULL;
    Scope *sc = g_symtab->current;
    if (sc->is_class_scope) {
        fprintf(stderr, "%s:%d: error: static class members are not supported\n",
                g_current_filename, g_lex_lineno);
        g_parse_errors++;
    } else if (sc->parent != NULL && sc->owner_name == NULL) {
        return STATIC;
    }
}

    /* C's other integer spellings -- unsigned / signed / short / long, in
     * any combination, optionally followed by int or char (`unsigned
     * int`, `long long`, `signed short int`, `unsigned char`). Vircon32
     * has exactly one integer type, a signed 32-bit word, so they are all
     * `int` here. That is exact for signed/short/long as long as values
     * stay in range; for `unsigned` it changes what >>, /, % and
     * comparisons do with values above INT_MAX, hence the warning. The
     * trailing context keeps `long longer` from swallowing half an
     * identifier. */
("unsigned"|"signed"|"short"|"long")([ \t]+("unsigned"|"signed"|"short"|"long"|"int"|"char"))*/[^A-Za-z0-9_]  {
    g_last_ident_sym = NULL;
    if (strstr(yytext, "unsigned") != NULL) {
        static int warned = 0;
        if (!warned) {
            fprintf(stderr, "%s:%d: warning: `unsigned` is treated as plain int "
                    "(Vircon32 has only a signed 32-bit integer): >>, /, %% and "
                    "comparisons behave as signed (reported once)\n",
                    g_current_filename, g_lex_lineno);
            warned = 1;
        }
    }
    return INT_KW;
}

"::"  {
    /* Arm (or clear) the qualifier context for the *next* token, using
     * whatever symbol the previous identifier-like token resolved to.
     * This must happen here, at scan time, not in a parser action --
     * see symtab_arm_qualifier's doc comment for why. */
    symtab_arm_qualifier(g_symtab, g_last_ident_sym);
    g_last_ident_sym = NULL;
    return COLONCOLON;
}

"->"    { g_last_ident_sym = NULL; return ARROW; }
"=="    { g_last_ident_sym = NULL; return EQ; }
"!="    { g_last_ident_sym = NULL; return NE; }
"<="    { g_last_ident_sym = NULL; return LE; }
">="    { g_last_ident_sym = NULL; return GE; }
"&&"    { g_last_ident_sym = NULL; return ANDAND; }
"||"    { g_last_ident_sym = NULL; return OROR; }
"+="    { g_last_ident_sym = NULL; return PLUSEQ; }
"-="    { g_last_ident_sym = NULL; return MINUSEQ; }
"*="    { g_last_ident_sym = NULL; return STAREQ; }
"/="    { g_last_ident_sym = NULL; return SLASHEQ; }
"++"    { g_last_ident_sym = NULL; return INC; }
"--"    { g_last_ident_sym = NULL; return DEC; }
"<<="   { g_last_ident_sym = NULL; return SHLEQ; }
">>="   { g_last_ident_sym = NULL; return SHREQ; }
"<<"    { g_last_ident_sym = NULL; return SHL; }
">>"    { g_last_ident_sym = NULL; return SHR; }
"&="    { g_last_ident_sym = NULL; return ANDEQ; }
"|="    { g_last_ident_sym = NULL; return OREQ; }
"^="    { g_last_ident_sym = NULL; return XOREQ; }
"%="    { g_last_ident_sym = NULL; return MODEQ; }

"std"[ \t]*"::"[ \t]*"array"  {
    /* std::array<T, N> -- the transpiler's own generic (generic.h). Only
     * the type-argument list after it is special; see type_spec. */
    g_last_ident_sym = NULL;
    return STD_ARRAY;
}

"using"[ \t]+"namespace"[ \t]+"std"[ \t]*";"  {
    /* Lets `array<T, N>` be written without the std:: prefix. Nothing
     * else lives in std here, so the directive has no other effect. */
    g_last_ident_sym = NULL;
    g_using_std = 1;
}

"array"/[ \t]*"<"  {
    /* A bare `array<`: the generic, but only after `#include <array>`
     * and `using namespace std;`, and only if the program has not
     * declared something of its own called `array` (`array < n` on a
     * variable stays a comparison). */
    Symbol *sym = symtab_lookup_qualified(g_symtab, yytext);
    if (sym == NULL && g_generic_array_enabled && g_using_std) {
        g_last_ident_sym = NULL;
        return STD_ARRAY;
    }
    yylval.str = strdup(yytext);
    g_last_ident_sym = sym;
    if (sym != NULL && (sym->kind == SYM_CLASS || sym->kind == SYM_TYPEDEF || sym->kind == SYM_ENUM || sym->kind == SYM_UNION)) {
        return TYPE_NAME;
    }
    return IDENTIFIER;
}

{ALPHA}{ALNUM}*  {
    /* The core of the lexer hack: classify this identifier by consulting
     * the symbol table *right now*, using the qualifier context armed by
     * a preceding "::" if any (symtab_lookup_qualified consumes it).
     * Remember what we resolved in case the *next* token is "::". */
    yylval.str = strdup(yytext);
    Symbol *sym = symtab_lookup_qualified(g_symtab, yytext);
    g_last_ident_sym = sym;
    if (sym != NULL && (sym->kind == SYM_CLASS || sym->kind == SYM_TYPEDEF || sym->kind == SYM_ENUM || sym->kind == SYM_UNION)) {
        return TYPE_NAME;
    }
    return IDENTIFIER;
}

{DIGIT}+"."{DIGIT}+([eE][+-]?{DIGIT}+)?[fF]?  {
    /* Trailing f/F (real C++'s own "this is a float, not a double"
     * suffix) is matched and simply ignored -- atof() itself already
     * stops at the first character it can't parse, so `atof("3.9f")`
     * correctly yields 3.9 with no separate stripping needed. Harmless
     * either way in this project specifically: there is no distinct
     * "double" type here for the suffix to have disambiguated FROM --
     * every floating-point value is already `float` -- so the suffix
     * is accepted purely for compatibility with real C++ source that
     * writes it out of habit, not because it changes anything here. */
    g_last_ident_sym = NULL;
    yylval.lit.fval = LIT_SIGN * atof(yytext);
    yylval.lit.macro = take_lit_macro();
    return FLOAT_LITERAL;
}

0[xX][0-9a-fA-F]+[uUlL]*  {
    /* Hex literal -- 0x1F, 0XFF. strtol() with an explicit base of 16
     * already knows to accept (and skip over) a leading "0x"/"0X"
     * itself, so the full yytext, suffix included, can be handed to it
     * directly -- it stops parsing at the first character that doesn't
     * fit hex, i.e. exactly at the suffix, the same "already stops
     * where it needs to" reasoning the float-suffix rule above relies
     * on for atof(). */
    g_last_ident_sym = NULL;
    yylval.lit.ival = LIT_SIGN * (int)strtol(yytext, NULL, 16);
    yylval.lit.macro = take_lit_macro();
    return INT_LITERAL;
}

0[bB][01]+[uUlL]*  {
    /* Binary literal -- 0b1010, 0B11110000. NOT part of every C++
     * standard -- added in C++14; earlier standards have no binary-
     * literal syntax at all. Accepted here anyway as a convenience,
     * matching real C++14-and-later compilers, but worth knowing if
     * you're deliberately targeting something that predates that (see
     * the README/man page's own note on this). Unlike hex, strtol()
     * does NOT know to skip a "0b"/"0B" prefix itself even with an
     * explicit base of 2 (that leading-prefix convenience is specific
     * to base 16, and base 0's own auto-detection, per the C standard)
     * -- so the prefix is skipped explicitly here (yytext + 2) before
     * handing the rest to strtol(), which then stops at the suffix the
     * same way the hex/decimal rules do. */
    g_last_ident_sym = NULL;
    yylval.lit.ival = LIT_SIGN * (int)strtol(yytext + 2, NULL, 2);
    yylval.lit.macro = take_lit_macro();
    return INT_LITERAL;
}

0[0-7]+[uUlL]*  {
    /* Octal literal -- 013 (=11 decimal), 0777. Real C++'s own
     * "leading zero means octal" rule -- listed BEFORE the plain
     * decimal rule below deliberately: flex's own longest-match-wins
     * only decides BETWEEN rules of different match length; for two
     * rules that would match the SAME text with the SAME length (e.g.
     * "013" matches both this rule and, if it came first, the decimal
     * one), flex breaks the tie by rule ORDER, first-listed wins -- so
     * this has to come first, or "013" would be silently (and
     * incorrectly) read as decimal 13 rather than octal 11. A leading
     * "0" whose next digit is 8 or 9 (invalid in octal, e.g. "089") is
     * NOT matched by this rule at all (requires at least one digit in
     * [0-7] after the leading 0) -- it falls through to the plain
     * decimal rule below instead, silently treating it as decimal 89,
     * not flagging real C++'s own "invalid digit in octal constant"
     * error. Matches this project's established best-effort philosophy
     * elsewhere -- not pedantically strict, but never silently wrong
     * about the COMMON case. strtol() with an explicit base of 8 treats
     * the whole matched text, leading 0 included, as ordinary base-8
     * digits -- no separate prefix-skipping needed, unlike binary
     * above. */
    g_last_ident_sym = NULL;
    yylval.lit.ival = LIT_SIGN * (int)strtol(yytext, NULL, 8);
    yylval.lit.macro = take_lit_macro();
    return INT_LITERAL;
}

{DIGIT}+[uUlL]*  {
    /* Suffix (u/U, l/L, and any combination -- ul, LL, ...) matched and
     * simply ignored, the exact same "atoi/atof/strtol already stops at
     * the first unparseable character" reasoning the float-literal
     * suffix rule above relies on. Deliberately permissive rather than
     * validating exactly which combinations real C++ allows (`lu` is
     * valid, `uu` is not, etc) -- this project has exactly one integer
     * type, so the suffix changes nothing regardless of which letters
     * appear or in what order; being permissive here costs nothing and
     * avoids rejecting otherwise-valid code over a distinction that
     * has no effect either way. */
    g_last_ident_sym = NULL;
    yylval.lit.ival = LIT_SIGN * (int)strtoul(yytext, NULL, 10); /* strtoul, not
        atoi: 4000000000u must wrap to its 32-bit pattern, not saturate */
    yylval.lit.macro = take_lit_macro();
    return INT_LITERAL;
}

\"([^"\\\n]|\\.)*\"  {
    /* SIMPLIFICATION: escape sequences inside the string are kept as
     * literal backslash-letter pairs rather than being decoded (\n stays
     * as the two characters '\' 'n'). Fine for a parsing skeleton; a real
     * codegen backend needs to decode these into the target C dialect's
     * own escape rules (and Vircon32 C's string/char handling should be
     * double-checked against its docs before assuming C escape parity). */
    g_last_ident_sym = NULL;
    size_t len = strlen(yytext);
    yylval.str = malloc(len - 1);
    memcpy(yylval.str, yytext + 1, len - 2);
    yylval.str[len - 2] = '\0';
    return STRING_LITERAL;
}

'([^'\\]|\\[0-7]{1,3}|\\[xX][0-9a-fA-F]+|\\.)'  {
    g_last_ident_sym = NULL;
    if (yytext[1] == '\\') {
        const char *p = yytext + 2;
        yylval.lit.ival = ast_decode_escape(&p);   /* \n, \x41, \101, ... */
    } else {
        yylval.lit.ival = (unsigned char)yytext[1];
    }
    yylval.lit.macro = take_lit_macro();
    return CHAR_LITERAL;
}

.  {
    /* Single-character tokens: operators/punctuation not covered above
     * ('+','-','*','/','%','<','>','=','!','&','|','~','(',')','{','}',
     * '[',']',';',',','.',':','?'). Returning the raw character code lets
     * parser.y reference them directly as e.g. '+' in grammar rules. */
    g_last_ident_sym = NULL;
    return yytext[0];
}

%%
