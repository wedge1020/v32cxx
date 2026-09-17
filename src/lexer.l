%{
/*
 * lexer.l -- flex scanner.
 *
 * The interesting part is IDENTIFIER classification and the COLONCOLON
 * rule right below it: together they implement the "lexer hack" (deciding
 * TYPE_NAME vs IDENTIFIER by consulting the symbol table) plus qualified-
 * name (`v32::Timer`) support, entirely in scan order. See the long
 * comment on symtab_arm_qualifier() in symtab.h for *why* this lives here
 * and not in a parser reduction action.
 *
 * SIMPLIFICATION: no real preprocessor. Real C++ source needs macro
 * expansion and conditional compilation handled before this scanner ever
 * sees it -- neither happens here. As an interim measure, though, a line
 * starting with '#' is no longer silently discarded: it's captured
 * verbatim (see g_preprocessor_lines, driver.h) so codegen.c can re-emit
 * it at the top of the generated file, ahead of everything else. This
 * means a `#include "video.h"` a person actually wrote now survives the
 * round trip instead of having to be manually re-added by hand every
 * time -- but it is still just pass-through, not preprocessing: no
 * macro expansion, no #include resolution, no #ifdef evaluation, and a
 * captured #ifdef/#endif pair doesn't actually gate anything this
 * scanner sees, since it never interprets what was captured at all.
 *
 * ONE exception to "every `#`-line is just passed through unread": cart
 * hints (`#texture NAME "file.png"` / `#sound NAME "file.wav"`), a
 * targeted special-case recognized directly below, NOT the start of a
 * general preprocessor -- these two specific forms get parsed for their
 * NAME/filename and registered (see g_cart_textures/g_cart_sounds,
 * driver.h), instead of falling through to the generic pass-through
 * above. Modeled directly on v32lua's own `--#texture`/`--#sound` cart
 * hints. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver.h"
#include "parser.h"

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

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = g_lex_lineno;

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
^"#texture"[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+\"[^"\n]*\"[^\n]* {
    char name[128] = {0};
    char filename[256] = {0};
    if (sscanf(yytext, "#texture %127s \"%255[^\"]\"", name, filename) == 2) {
        register_cart_resource(&g_cart_textures, name, filename);
    }
}

^"#sound"[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+\"[^"\n]*\"[^\n]* {
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
^"#title"[ \t]+\"[^"\n]*\"[^\n]* {
    char title[256] = {0};
    if (sscanf(yytext, "#title \"%255[^\"]\"", title) == 1) {
        free(g_cart_title);
        g_cart_title = strdup(title);
    }
}

^"#version"[ \t]+[^ \t\n]+[^\n]* {
    char version[64] = {0};
    if (sscanf(yytext, "#version %63s", version) == 1) {
        free(g_cart_version);
        g_cart_version = strdup(version);
    }
}

^"#"[^\n]*                     {
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
[ \t\r]+                        { /* whitespace */ }

"class"      { g_last_ident_sym = NULL; return CLASS; }
"struct"     { g_last_ident_sym = NULL; return STRUCT; }
"public"     { g_last_ident_sym = NULL; return PUBLIC; }
"private"    { g_last_ident_sym = NULL; return PRIVATE; }
"protected"  { g_last_ident_sym = NULL; return PROTECTED; }
"namespace"  { g_last_ident_sym = NULL; return NAMESPACE; }
"typedef"    { g_last_ident_sym = NULL; return TYPEDEF; }
"return"     { g_last_ident_sym = NULL; return RETURN; }
"if"         { g_last_ident_sym = NULL; return IF; }
"else"       { g_last_ident_sym = NULL; return ELSE; }
"while"      { g_last_ident_sym = NULL; return WHILE; }
"for"        { g_last_ident_sym = NULL; return FOR; }
"break"      { g_last_ident_sym = NULL; return BREAK; }
"continue"   { g_last_ident_sym = NULL; return CONTINUE; }
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
"this"       { g_last_ident_sym = NULL; return THIS; }
"virtual"    { g_last_ident_sym = NULL; return VIRTUAL; }
"operator"   { g_last_ident_sym = NULL; return OPERATOR; }
"true"       { g_last_ident_sym = NULL; return TRUE_KW; }
"false"      { g_last_ident_sym = NULL; return FALSE_KW; }

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

{ALPHA}{ALNUM}*  {
    /* The core of the lexer hack: classify this identifier by consulting
     * the symbol table *right now*, using the qualifier context armed by
     * a preceding "::" if any (symtab_lookup_qualified consumes it).
     * Remember what we resolved in case the *next* token is "::". */
    yylval.str = strdup(yytext);
    Symbol *sym = symtab_lookup_qualified(g_symtab, yytext);
    g_last_ident_sym = sym;
    if (sym != NULL && (sym->kind == SYM_CLASS || sym->kind == SYM_TYPEDEF)) {
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
    yylval.fval = atof(yytext);
    return FLOAT_LITERAL;
}

{DIGIT}+  {
    g_last_ident_sym = NULL;
    yylval.ival = atoi(yytext);
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

'([^'\\]|\\.)'  {
    g_last_ident_sym = NULL;
    if (yytext[1] == '\\') {
        switch (yytext[2]) {
            case 'n': yylval.ival = '\n'; break;
            case 't': yylval.ival = '\t'; break;
            case 'r': yylval.ival = '\r'; break;
            case '0': yylval.ival = '\0'; break;
            case '\\': yylval.ival = '\\'; break;
            case '\'': yylval.ival = '\''; break;
            case '"': yylval.ival = '"'; break;
            default: yylval.ival = yytext[2];
        }
    } else {
        yylval.ival = yytext[1];
    }
    return CHAR_LITERAL;
}

.  {
    /* Single-character tokens: operators/punctuation not covered above
     * ('+','-','*','/','%','<','>','=','!','&','|','~','(',')','{','}',
     * '[',']',';',',','.',':'). Returning the raw character code lets
     * parser.y reference them directly as e.g. '+' in grammar rules. */
    g_last_ident_sym = NULL;
    return yytext[0];
}

%%
