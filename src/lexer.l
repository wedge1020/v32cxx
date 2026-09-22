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

    /* Line marker emitted by prescan.c (see prescan.h): `# N "file"`.
     * Consumed HERE, never passed through, because it is this
     * project's own bookkeeping, not the person's source. Sets
     * g_lex_lineno directly to N rather than N-1 because this
     * pattern swallows the marker's own newline -- the \n rule's
     * increment never runs for this line, so the next content line
     * is already counted as N. g_current_filename is re-pointed at
     * an interned copy (see prescan_intern_filename) so the storage
     * stays valid for the rest of the run. */
^"#"[ \t]+{DIGIT}+[ \t]+\"[^\"]*\"[ \t]*\n  {
    int line_no;
    char fname[512];
    if (sscanf(yytext, "# %d \"%511[^\"]\"", &line_no, fname) == 2) {
        g_lex_lineno = line_no;
        g_current_filename = prescan_intern_filename(fname);
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
"enum"       { g_last_ident_sym = NULL; return ENUM; }
"union"      { g_last_ident_sym = NULL; return UNION; }
"public"     { g_last_ident_sym = NULL; return PUBLIC; }
"private"    { g_last_ident_sym = NULL; return PRIVATE; }
"protected"  { g_last_ident_sym = NULL; return PROTECTED; }
"namespace"  { g_last_ident_sym = NULL; return NAMESPACE; }
"typedef"    { g_last_ident_sym = NULL; return TYPEDEF; }
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
"volatile"         { g_last_ident_sym = NULL; return VOLATILE; }
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
    yylval.fval = atof(yytext);
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
    yylval.ival = (int)strtol(yytext, NULL, 16);
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
    yylval.ival = (int)strtol(yytext + 2, NULL, 2);
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
    yylval.ival = (int)strtol(yytext, NULL, 8);
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
     * '[',']',';',',','.',':','?'). Returning the raw character code lets
     * parser.y reference them directly as e.g. '+' in grammar rules. */
    g_last_ident_sym = NULL;
    return yytext[0];
}

%%
