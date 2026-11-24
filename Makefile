INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
OUT_DIR = out

BIN     = $(BIN_DIR)/v32c++
# v32c: the same program under the name to use for C input (a plain copy)
BINC    = $(BIN_DIR)/v32c
# `make test` runs v32c++ over tests/*, which must not grow Make.sh/Make.bat
TBIN    = $(BIN) --no-build-scripts
# V32CC=path/to/v32cc makes `make test` also push the --line-markers output through v32cc -g
V32CC   ?=

CC      = gcc
CFLAGS  = -Wall -Wextra -g -I$(INC_DIR) -MMD -MP
# -MMD -MP: every object also writes obj/<name>.d listing the headers it
# actually included, pulled in by the `-include` at the bottom of this
# file. Without it make only compared each .o against its own .c, so
# editing a header (inc/v32cxx.h's VERSION, a struct in ast.h, a
# prototype in sema.h) silently left stale objects in place -- `make`
# would report nothing to do and --version kept printing the old string.
BISON   = bison
FLEX    = flex

# The transpiler version: inc/v32cxx.h's VERSION is the single source.
# `make version` shows it and stamps it, with today's date, into the .TH
# header of every manual page (see the target below); `v32c++ --version`
# reads it from the header directly. Portable between GNU and BSD/macOS
# tools: no `sed -i` (BSD sed takes the script as the backup suffix), no
# \t in brackets, no `date +%-d` (the day's leading zero is cut by sed).
VERSION := $(shell sed -n 's/^\#define[[:space:]]*VERSION[[:space:]]*"\(.*\)".*/\1/p' $(INC_DIR)/v32cxx.h)
# The date part (YYYYMMDD) of VERSION names the project archive: `make archive` writes
# ../v32cxx-project-YYYYMMDD.zip ("20261123-dev" -> ../v32cxx-project-20261123.zip)
ARCHDATE := $(shell echo '$(VERSION)' | sed 's/^\([0-9]\{8\}\).*/\1/')
MANDATE := $(shell LC_ALL=C date +"%B %d, %Y" | sed 's/ 0\([1-9]\),/ \1,/')
# The same date in the translated pages' .TH line (man/es, man/fr): month
# names from a table, so the build does not depend on the locales installed.
MAN_D   := $(shell date +%d | sed 's/^0//')
MAN_M   := $(shell date +%m | sed 's/^0//')
MAN_Y   := $(shell date +%Y)
MANDATE_ES := $(MAN_D) de $(word $(MAN_M),enero febrero marzo abril mayo junio julio agosto septiembre octubre noviembre diciembre) de $(MAN_Y)
MANDATE_FR := $(if $(filter 1,$(MAN_D)),1er,$(MAN_D)) $(word $(MAN_M),janvier février mars avril mai juin juillet août septembre octobre novembre décembre) $(MAN_Y)
# The manual pages: man/man1 (v32c++.1) and man/man3 (the headers, in
# section 3v32). A page that is only `.so <other page>` is an alias and
# has no .TH line of its own.
MAN1    := $(wildcard man/man1/*.1)
MAN3    := $(wildcard man/man3/*.3v32)
# The Vircon32 SDK's own pages (the emulator and DevTools commands, and the
# C library headers: video.h(3v32), string.h(3v32), ...), kept in a tree of
# their own, man/vircon32/, because they document someone else's software
# and carry its version, not v32c++'s: `make version` leaves them alone.
# They install beside v32c++'s own, into the same man1 and man3 (no page
# name is shared between the two sets).
VMAN1   := $(wildcard man/vircon32/man1/*.1)
VMAN3   := $(wildcard man/vircon32/man3/*.3v32)
# The translations of v32c++'s own pages: man/es/ and man/fr/, each with its
# man1/ and man3/ (the SDK pages above exist only in English).
ES_MAN1 := $(wildcard man/es/man1/*.1)
ES_MAN3 := $(wildcard man/es/man3/*.3v32)
FR_MAN1 := $(wildcard man/fr/man1/*.1)
FR_MAN3 := $(wildcard man/fr/man3/*.3v32)

# DEST_LANG chooses the documentation language `sysinstall` installs:
#   (unset) / all   everything: the English manual pages and documents, plus
#                   the translations -- the pages in the standard
#                   per-language trees $(MANROOT)/es/man1 ... so `man` picks
#                   one from LANG, the documents side by side
#   en              English only
#   es / fr         that language only: its pages directly in man1 / man3 and
#                   its documents under the plain names (README.md ...)
# The Vircon32 SDK's pages are English only and always installed.
DEST_LANG ?= all
ifeq ($(DEST_LANG),)
DEST_LANG := all
endif
ifeq ($(DEST_LANG),all)
PLAIN_MAN := $(MAN1) $(MAN3) $(VMAN1) $(VMAN3)
LANG_MANS := es fr
else ifeq ($(DEST_LANG),en)
PLAIN_MAN := $(MAN1) $(MAN3) $(VMAN1) $(VMAN3)
LANG_MANS :=
else ifeq ($(DEST_LANG),es)
PLAIN_MAN := $(ES_MAN1) $(ES_MAN3) $(VMAN1) $(VMAN3)
LANG_MANS :=
else ifeq ($(DEST_LANG),fr)
PLAIN_MAN := $(FR_MAN1) $(FR_MAN3) $(VMAN1) $(VMAN3)
LANG_MANS :=
else
$(error DEST_LANG must be all, en, es or fr (not '$(DEST_LANG)'))
endif
# Every page this Makefile may have installed, in any language, for uninstall
ALL_MAN := $(MAN1) $(MAN3) $(VMAN1) $(VMAN3) $(ES_MAN1) $(ES_MAN3) $(FR_MAN1) $(FR_MAN3)
# The installed-header directory, from inc/config.h (same single-source
# idea): `make sysinstall` puts the v32/ veneer headers there.
INCPATH := $(shell sed -n 's/^[[:space:]]*\#[[:space:]]*define[[:space:]]*V32CXX_INCLUDE_PATH[[:space:]]*"\(.*\)".*/\1/p' $(INC_DIR)/config.h)

# PREFIX and DESTDIR for `make sysinstall` / `sysuninstall`, as in every other
# Vircon32 tool Makefile: PREFIX (default /usr/local) is the root of
# bin/, share/man/ and Vircon32/v32tools/ ; DESTDIR is a staging root for
# packaging (nothing is ever written outside it). With the default prefix the
# header directory is the one compiled into v32c++ (inc/config.h). With another
# PREFIX the headers go to $(PREFIX)/Vircon32/v32tools/include/v32c++, which the
# binary finds without a rebuild: it also searches ../Vircon32/v32tools/include/v32c++
# relative to its own directory (see inc/config.h), i.e. PREFIX/bin/..
# (To compile another fixed path in, use the CFLAGS override in inc/config.h.)
PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR   = $(PREFIX)/bin
ifneq ($(PREFIX),/usr/local)
INCPATH := $(PREFIX)/Vircon32/v32tools/include/v32c++
endif

.PHONY: all clean test realcheck install uninstall sysinstall sysuninstall version archive

all: $(BIN_DIR)/v32c++ $(BINC)

# v32c is byte-for-byte v32c++ (the build scripts call whichever name ran).
# A copy, not a link, so it also works where links are unavailable.
$(BINC): $(BIN)
	cp -f $(BIN) $(BINC)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)
# These three rules only ever fire when something actually lists the
# directory as a prerequisite (an order-only one, via `| $(DIR)`, is all
# that's needed -- make doesn't care about the directory's own mtime,
# just that it exists before the recipe runs). $(OBJ_DIR) was already
# wired in everywhere it's needed (every object-compile rule below uses
# `| $(OBJ_DIR)`). $(BIN_DIR) and $(OUT_DIR) weren't -- the v32c++ link
# rule never declared `| $(BIN_DIR)`, and `test` never declared
# `| $(OUT_DIR)`, so `mkdir -p` for either one only ever ran if the
# directory happened to already exist from some OTHER rule needing it,
# or from a previous round's build never having been fully cleaned. That
# masked the gap for bin/ (it's persisted across builds since early
# rounds) but not for out/ (brand new, so it was the first to actually
# expose it, immediately). Both are fixed now -- see the `| $(BIN_DIR)`
# and `| $(OUT_DIR)` prerequisites below.

# Bison: -d also emits parser.tab.h (needed by lexer.l and main.c).
# GLR output still defines yyparse()/yylval/yylloc etc. the same as a
# plain LALR parser, so nothing else in the build changes.
debug_bison:
	$(BISON) -v --output=$(SRC_DIR)/parser.c --defines=$(INC_DIR)/parser.h $(SRC_DIR)/parser.y -t -Wcounterexamples 2> counterexamples.txt

$(SRC_DIR)/parser.c $(INC_DIR)/parser.h: $(SRC_DIR)/parser.y | $(OBJ_DIR)
	$(BISON) -v --output=$(SRC_DIR)/parser.c --defines=$(INC_DIR)/parser.h $(SRC_DIR)/parser.y -t

$(SRC_DIR)/lexer.c: $(SRC_DIR)/lexer.l $(INC_DIR)/parser.h | $(OBJ_DIR)
	$(FLEX) --outfile=$(SRC_DIR)/lexer.c $(SRC_DIR)/lexer.l

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/parser.o: $(SRC_DIR)/parser.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(SRC_DIR)/lexer.o: $(SRC_DIR)/lexer.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(BIN_DIR)/v32c++: $(OBJ_DIR)/parser.o   $(OBJ_DIR)/lexer.o   \
                   $(OBJ_DIR)/ast.o      $(OBJ_DIR)/symtab.o  \
				   $(OBJ_DIR)/prescan.o  $(OBJ_DIR)/sema.o    \
                   $(OBJ_DIR)/lower.o    $(OBJ_DIR)/codegen.o \
				   $(OBJ_DIR)/pathutil.o $(OBJ_DIR)/compat.o $(OBJ_DIR)/generic.o $(OBJ_DIR)/stdstring.o $(OBJ_DIR)/cartxml.o $(OBJ_DIR)/buildscript.o \
				   $(OBJ_DIR)/debugmap.o $(OBJ_DIR)/macro.o    \
				   $(OBJ_DIR)/cmode.o    \
				   $(OBJ_DIR)/link.o     $(OBJ_DIR)/archive.o $(OBJ_DIR)/warn.o \
				   $(OBJ_DIR)/main.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
# If linking fails looking for yywrap/yy_flex_* symbols on your system,
# add -lfl to this link line (some flex installs need it even with
# %option noyywrap; most don't).

test: all | $(OUT_DIR)
	$(TBIN)  -vvv -c -o out/01partial.c tests/01sample.cpp 1> out/01sample.txt 2>&1
	$(TBIN)  -vvv    -o out/02program.c tests/02sample.cpp 1> out/02sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/03partial.c tests/03sample.cpp 1> out/03sample.txt 2>&1
	-$(TBIN) -vvv -c -o out/04failure.c tests/04sample.cpp 1> out/04sample.txt 2>&1
	-$(TBIN) -vvv -c -o out/05failure.c tests/05sample.cpp 1> out/05sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/06partial.c tests/06sample.cpp 1> out/06sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/07partial.c tests/07sample.cpp 1> out/07sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/08partial.c tests/08sample.cpp 1> out/08sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/09partial.c tests/09sample.cpp 1> out/09sample.txt 2>&1
	-$(TBIN) -vvv -c -o out/10failure.c tests/10sample.cpp 1> out/10sample.txt 2>&1
	-$(TBIN) -vvv -c -o out/11failure.c tests/11sample.cpp 1> out/11sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/12partial.c tests/12sample.cpp 1> out/12sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/13partial.c tests/13sample.cpp 1> out/13sample.txt 2>&1
	$(TBIN)  -vvv    -o out/14program.c tests/14sample.cpp 1> out/14sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/15partial.c tests/15sample.cpp 1> out/15sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/16partial.c tests/16sample.cpp 1> out/16sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/17partial.c tests/17sample.cpp 1> out/17sample.txt 2>&1
	$(TBIN)  -vvv -c -o out/18partial.c tests/18sample.cpp 1> out/18sample.txt 2>&1
	-$(TBIN) -vvv -c -o out/19failure.c tests/19sample.cpp 1> out/19sample.txt 2>&1
	-$(TBIN) -vvv -c -o out/20failure.c tests/20sample.cpp 1> out/20sample.txt 2>&1
	$(TBIN)  -vvv    -o out/21program.c tests/21sample.cpp 1> out/21sample.txt 2>&1
	$(TBIN)  -vvv    -o out/22program.c tests/22sample.cpp 1> out/22sample.txt 2>&1
	$(TBIN)  -vvv    -o out/23program.c tests/23sample.cpp 1> out/23sample.txt 2>&1
	$(TBIN)  -vvv    -o out/24program.c tests/24sample.cpp 1> out/24sample.txt 2>&1
	$(TBIN)  -vvv    -o out/25program.c tests/25sample.cpp 1> out/25sample.txt 2>&1
	$(TBIN)  -vvv    -o out/26program.c tests/26sample.cpp 1> out/26sample.txt 2>&1
	$(TBIN)  -vvv    -o out/27program.c tests/27sample.cpp 1> out/27sample.txt 2>&1
	$(TBIN)  -vvv    -o out/28program.c tests/28sample.cpp 1> out/28sample.txt 2>&1
	$(TBIN)  -vvv    -o out/29program.c tests/29sample.cpp 1> out/29sample.txt 2>&1
	$(TBIN)  -vvv    -o out/30program.c tests/30sample.cpp 1> out/30sample.txt 2>&1
	-$(TBIN) -vvv -c -o out/failureXX.c tests/31sample.cpp 1> out/31sample.txt 2>&1
	$(TBIN)  -vvv    -o out/32program.c tests/32sample.cpp 1> out/32sample.txt 2>&1
	$(TBIN)  -vvv -g -o out/33program.c tests/33sample.cpp 1> out/33sample.txt 2>&1
	$(TBIN)  -vvv -bgo  out/34program.c tests/34sample.cpp 1> out/34sample.txt 2>&1
	$(TBIN)  -vvv    -o out/35program.c tests/35sample.cpp 1> out/35sample.txt 2>&1
	-$(TBIN) -vvv    -o out/36failure.c tests/36sample.cpp 1> out/36sample.txt 2>&1
	$(TBIN)  -vvv    -o out/37program.c tests/37sample.cpp 1> out/37sample.txt 2>&1
	$(TBIN)  -vvv    -o out/38program.c tests/38sample.cpp 1> out/38sample.txt 2>&1
	$(TBIN)  -vvv    -o out/39program.c tests/39sample.cpp 1> out/39sample.txt 2>&1
	$(TBIN)  -vvv    -o out/40program.c tests/40sample.cpp 1> out/40sample.txt 2>&1
	$(TBIN)  -vvv    -o out/41program.c tests/41sample.cpp 1> out/41sample.txt 2>&1
	-$(TBIN) -vvv    -o out/42failure.c tests/42sample.cpp 1> out/42sample.txt 2>&1
	$(TBIN)  -vvv    -o out/43program.c tests/43sample.cpp 1> out/43sample.txt 2>&1
	$(TBIN)  -vvv    -o out/44program.c tests/44sample.cpp 1> out/44sample.txt 2>&1
	-$(TBIN) -vvv    -o out/45failure.c tests/45sample.cpp 1> out/45sample.txt 2>&1
	$(TBIN)  -vvv    -o out/46program.c tests/46sample.cpp 1> out/46sample.txt 2>&1
	$(TBIN)  -vvv    -o out/47program.c tests/47sample.cpp 1> out/47sample.txt 2>&1
	$(TBIN)  -vvv    -o out/48program.c tests/48sample.cpp 1> out/48sample.txt 2>&1
	-$(TBIN) -vvv    -o out/49failure.c tests/49sample.cpp 1> out/49sample.txt 2>&1
	$(TBIN)  -vvv    -o out/50program.c tests/50sample.cpp 1> out/50sample.txt 2>&1
	$(TBIN)  -vvv    -o out/51program.c tests/51sample.cpp 1> out/51sample.txt 2>&1
	$(TBIN)  -vvv    -o out/52program.c tests/52sample.cpp 1> out/52sample.txt 2>&1
	-$(TBIN) -vvv    -o out/53failure.c tests/53sample.cpp 1> out/53sample.txt 2>&1
	$(TBIN)  -vvv    -o out/54program.c tests/54sample.cpp 1> out/54sample.txt 2>&1
	$(TBIN)  -vvv    -o out/55program.c tests/55sample.cpp 1> out/55sample.txt 2>&1
	$(TBIN)  -vvv    -o out/56program.c tests/56sample.cpp 1> out/56sample.txt 2>&1
	$(TBIN)  -vvv    -o out/57program.c tests/57sample.cpp 1> out/57sample.txt 2>&1
	$(TBIN)  -vvv    -o out/58program.c tests/58sample.cpp 1> out/58sample.txt 2>&1
	$(TBIN)  -vvv    -o out/59program.c tests/59sample.cpp 1> out/59sample.txt 2>&1
	$(TBIN)  -vvv    -o out/60program.c tests/60sample.cpp 1> out/60sample.txt 2>&1
	$(TBIN)  -vvv    -o out/61program.c tests/61sample.cpp 1> out/61sample.txt 2>&1
	$(TBIN)  -vvv    -o out/62program.c tests/62sample.cpp 1> out/62sample.txt 2>&1
	$(TBIN)  -vvv    -o out/63program.c tests/63sample.cpp 1> out/63sample.txt 2>&1
	$(TBIN)  -vvv    -o out/64program.c tests/64sample.cpp 1> out/64sample.txt 2>&1
	$(TBIN)  -vvv    -o out/65program.c tests/65sample.cpp 1> out/65sample.txt 2>&1
	$(TBIN)  -vvv    -o out/66program.c tests/66sample.cpp 1> out/66sample.txt 2>&1
	$(TBIN)  -vvv    -o out/67program.c tests/67sample.cpp 1> out/67sample.txt 2>&1
	$(TBIN)  -vvv    -o out/68program.c tests/68sample.cpp 1> out/68sample.txt 2>&1
	$(TBIN)  -vvv    -o out/69program.c tests/69sample.cpp 1> out/69sample.txt 2>&1
	$(TBIN)  -vvv    -o out/70program.c tests/70sample.cpp 1> out/70sample.txt 2>&1
	$(TBIN)  -vvv    -o out/71program.c tests/71sample.cpp 1> out/71sample.txt 2>&1
	$(TBIN)  -vvv    -o out/72program.c tests/72sample.cpp 1> out/72sample.txt 2>&1
	$(TBIN)  -vvv    -o out/73program.c tests/73sample.cpp 1> out/73sample.txt 2>&1
	$(TBIN)  -vvv    -o out/74program.c tests/74sample.cpp 1> out/74sample.txt 2>&1
	$(TBIN)  -vvv    -o out/75program.c tests/75sample.cpp 1> out/75sample.txt 2>&1
	$(TBIN)  -vvv    -o out/76program.c tests/76sample.cpp 1> out/76sample.txt 2>&1
	$(TBIN)  -vvv    -o out/77program.c tests/77sample.cpp 1> out/77sample.txt 2>&1
	$(TBIN)  -vvv    -o out/78program.c tests/78sample.cpp 1> out/78sample.txt 2>&1
	$(TBIN)  -vvv    -o out/79program.c tests/79sample.cpp 1> out/79sample.txt 2>&1
	-$(TBIN) -vvv    -o out/80failure.c tests/80sample.cpp 1> out/80sample.txt 2>&1
	$(TBIN)  -vvv    -o out/81program.c tests/81sample.cpp 1> out/81sample.txt 2>&1
	$(TBIN)  -vvv    -o out/82program.c tests/82sample.cpp 1> out/82sample.txt 2>&1
	$(TBIN)  -vvv    -o out/83program.c tests/83sample.cpp 1> out/83sample.txt 2>&1
	$(TBIN)  -vvv    -o out/84program.c tests/84sample.cpp 1> out/84sample.txt 2>&1
	$(TBIN)  -vvv    -o out/85program.c tests/85sample.cpp 1> out/85sample.txt 2>&1
	$(TBIN)  -vvv    -o out/86program.c tests/86sample.cpp 1> out/86sample.txt 2>&1
	$(TBIN)  -vvv    -o out/87program.c tests/87sample.cpp 1> out/87sample.txt 2>&1
	$(TBIN)  -vvv    -o out/88program.c tests/88sample.cpp 1> out/88sample.txt 2>&1
	$(TBIN)  -vvv    -o out/89program.c tests/89sample.cpp 1> out/89sample.txt 2>&1
	$(TBIN)  -vvv    -o out/90program.c tests/90sample.cpp 1> out/90sample.txt 2>&1
	V32CXX_SDK_INCLUDE=tests/sdkstub $(TBIN) -vvv -o out/90sdk.c tests/90sample.cpp 1> out/90sdk.txt 2>&1
	grep -q 'SDK header search: tests/sdkstub' out/90sdk.txt
	-$(TBIN) -vvv    -o out/91failure.c tests/91sample.cpp 1> out/91sample.txt 2>&1
	$(TBIN)  -vvv    -o out/92program.c tests/92sample.cpp 1> out/92sample.txt 2>&1
	-$(TBIN) -vvv    -o out/93failure.c tests/93sample.cpp 1> out/93sample.txt 2>&1
	$(TBIN)  -vvv -I . -o out/94program.c tests/94sample.cpp 1> out/94sample.txt 2>&1
	$(TBIN)  -vvv -D FROM_CMDLINE=7 -o out/95program.c tests/95sample.cpp 1> out/95sample.txt 2>&1
	-$(TBIN) -vvv    -o out/96failure.c tests/96sample.cpp 1> out/96sample.txt 2>&1
	$(TBIN)  -vvv    -o out/97program.c tests/97sample.cpp 1> out/97sample.txt 2>&1
	@if [ -f tools/vircon32/bin/include/video.h ]; then \
	    echo "V32CXX_SDK_INCLUDE=tools/vircon32/bin/include $(TBIN) -vvv -o out/98program.c tests/98sample.cpp"; \
	    V32CXX_SDK_INCLUDE=tools/vircon32/bin/include $(TBIN) -vvv -o out/98program.c tests/98sample.cpp 1> out/98sample.txt 2>&1; \
	else echo "skipping sample 98: needs the SDK headers (run tools/vircon32/build-tools.sh)"; fi
	$(TBIN)  -vvv    -o out/99program.c tests/99sample.cpp 1> out/99sample.txt 2>&1
	$(TBIN)  -vvv    -o out/100program.c tests/100sample.cpp 1> out/100sample.txt 2>&1
	$(TBIN)  -vvv    -o out/101program.c tests/101sample.cpp 1> out/101sample.txt 2>&1
	$(TBIN)  -vvv    -o out/102program.c tests/102sample.cpp 1> out/102sample.txt 2>&1
	$(TBIN)  -vvv    -o out/103program.c tests/103sample.cpp 1> out/103sample.txt 2>&1
	$(TBIN)  -vvv    -o out/104program.c tests/104sample.cpp 1> out/104sample.txt 2>&1
	$(TBIN)  -vvv    -o out/105program.c tests/105sample.cpp 1> out/105sample.txt 2>&1
	$(TBIN)  -vvv    -o out/106program.c tests/106sample.cpp 1> out/106sample.txt 2>&1
	$(TBIN)  -vvv    -o out/107program.c tests/107sample.cpp 1> out/107sample.txt 2>&1
	$(TBIN)  -vvv    -o out/108program.c tests/108sample.cpp 1> out/108sample.txt 2>&1
	$(TBIN)  -vvv    -o out/109program.c tests/109sample.cpp 1> out/109sample.txt 2>&1
	$(TBIN)  -vvv    -o out/110program.c tests/110sample.cpp 1> out/110sample.txt 2>&1
	$(TBIN)  -vvv    -o out/111program.c tests/111sample.cpp 1> out/111sample.txt 2>&1
	$(TBIN)  -vvv    -o out/112program.c tests/112sample.cpp 1> out/112sample.txt 2>&1
	$(TBIN)  -vvv    -o out/113program.c tests/113sample.cpp 1> out/113sample.txt 2>&1
	$(TBIN)  -vvv    -o out/114program.c tests/114sample.cpp 1> out/114sample.txt 2>&1
	$(TBIN)  -vvv    -o out/115program.c tests/115sample.cpp 1> out/115sample.txt 2>&1
	$(TBIN)  -vvv    -o out/116program.c tests/116sample.c   1> out/116sample.txt 2>&1
	$(TBIN)  -vvv    -o out/117program.c tests/117sample.c   1> out/117sample.txt 2>&1
	$(TBIN)  -vvv    -o out/118program.c tests/118sample.c   1> out/118sample.txt 2>&1
	$(TBIN)  -vvv -I . -o out/119program.c tests/119sample.cpp 1> out/119sample.txt 2>&1
	$(TBIN)  -vvv    -o out/120program.c tests/120sample.cpp 1> out/120sample.txt 2>&1
	$(TBIN)  -vvv    -o out/121program.c tests/121sample.cpp 1> out/121sample.txt 2>&1
	$(TBIN)  -vvv    -o out/122program.c tests/122sample.cpp 1> out/122sample.txt 2>&1
	$(TBIN)  -vvv    -o out/123program.c tests/123sample.cpp 1> out/123sample.txt 2>&1
	$(TBIN)  -vvv    -o out/124program.c tests/124sample.cpp 1> out/124sample.txt 2>&1
	$(TBIN)  -vvv -I . -o out/125program.c tests/125sample.cpp 1> out/125sample.txt 2>&1
	$(TBIN)  -vvv    -o out/126program.c tests/126sample.cpp 1> out/126sample.txt 2>&1
	$(TBIN)  -vvv    -o out/127program.c tests/127sample.cpp 1> out/127sample.txt 2>&1
	$(TBIN)  -vvv    -o out/128program.c tests/128sample.c   1> out/128sample.txt 2>&1
	$(TBIN)  -vvv    -o out/129program.c tests/129sample.cpp 1> out/129sample.txt 2>&1
	$(TBIN)  -vvv    -o out/130program.c tests/130sample.cpp 1> out/130sample.txt 2>&1
	$(TBIN)  -vvv -I . -o out/131program.c tests/131sample.cpp 1> out/131sample.txt 2>&1
	$(TBIN)  -vvv    -o out/132program.c tests/132sample.cpp 1> out/132sample.txt 2>&1
	$(TBIN)  -vvv    -o out/133program.c tests/133sample.cpp 1> out/133sample.txt 2>&1
	$(TBIN)  -vvv    -o out/134program.c tests/134sample.c   1> out/134sample.txt 2>&1
	$(TBIN)  -vvv    -o out/135program.c tests/135sample.cpp 1> out/135sample.txt 2>&1
	$(TBIN)  -vvv    -o out/136program.c tests/136sample.cpp 1> out/136sample.txt 2>&1
	$(TBIN)  -vvv --no-signed-shift -o out/137program.c tests/137sample.cpp 1> out/137sample.txt 2>&1
	$(TBIN)  -vvv    -o out/138program.c tests/138sample.c   1> out/138sample.txt 2>&1
	$(TBIN)  -vvv    -o out/139program.c tests/139sample.c   1> out/139sample.txt 2>&1
	$(TBIN)  -vvv    -o out/140program.c tests/140sample.cpp 1> out/140sample.txt 2>&1
	$(TBIN)  -vvv    -o out/141program.c tests/141sample.cpp 1> out/141sample.txt 2>&1
	-$(TBIN) -vvv    -o out/142failure.c tests/142sample.cpp 1> out/142sample.txt 2>&1
	$(TBIN)  -vvv    -o out/143program.c tests/143sample.cpp 1> out/143sample.txt 2>&1
	grep -q '<binary path="143program.vbin" />' out/143program.xml
	grep -q '<texture path="ship.vtex" /> <!-- SHIP -->' out/143program.xml
	grep -q '<texture path="extra/ship.vtex" /> <!-- BULLET -->' out/143program.xml
	grep -q '<binary path="143programOpt.vbin" />' out/143programOpt.xml
	grep -q '<texture path="extra/ship.vtex" /> <!-- BULLET -->' out/143programOpt.xml
	cp tests/143sample.cpp out/144src.cpp
	$(TBIN)  -vvv --xml-preserve-paths -o out/144program.c out/144src.cpp 1> out/144sample.txt 2>&1
	grep -q '<binary path="144program.vbin" />' out/144program.xml
	grep -q '<texture path="textures/extra/ship.vtex" />' out/144program.xml
	grep -q '<binary path="144programOpt.vbin" />' out/144programOpt.xml
	grep -q 'title="Paths"' out/143program.xml
	grep -q 'title="Paths \[optimized\]"' out/143programOpt.xml
	$(TBIN)  -vvv -o out/145program.c tests/145sample.cpp 1> out/145sample.txt 2>&1
	grep -q 'title="Title Test"' out/145program.xml
	grep -q 'title="Title Test: Turbo Edition"' out/145programOpt.xml
	$(TBIN)  -vvv --title-opt=Cmdline -o out/145cli.c tests/145sample.cpp 1> out/145cli.txt 2>&1
	grep -q 'title="Cmdline"' out/145cliOpt.xml
	$(TBIN)  -vvv --no-diff-opt-title -o out/145same.c tests/143sample.cpp 1> out/145same.txt 2>&1
	grep -q 'title="Paths"' out/145sameOpt.xml
	rm -rf out/146 && mkdir -p out/146/src out/146/obj
	cp tests/143sample.cpp out/146/src/game.cpp
	cd out/146 && ../../$(BIN) -vvv -I src -DFOO=1 -o obj/game.c src/game.cpp 1> ../146sample.txt 2>&1
	test -x out/146/Make.sh && test -f out/146/Make.bat && test ! -e out/146/src/Make.sh
	bash -n out/146/Make.sh
	grep -q 'v32c++ -I src -DFOO=1 --no-build-scripts -o obj/game.c src/game.cpp' out/146/Make.sh
	grep -q 'png2vircon textures/extra/ship.png -o obj/extra/ship.vtex' out/146/Make.sh
	grep -q 'packrom obj/gameOpt.xml -o bin/gameOpt.v32' out/146/Make.sh
	grep -q 'command -v v32opt' out/146/Make.sh
	grep -q 'where v32opt' out/146/Make.bat
	grep -q 'png2vircon textures\\extra\\ship.png -o obj\\extra\\ship.vtex' out/146/Make.bat
	rm -rf out/147 && mkdir -p out/147
	cp tests/143sample.cpp out/147/game.cpp
	echo "my own script" > out/147/Make.sh
	$(BIN)   -vvv -o out/147/game.c out/147/game.cpp 1> out/147sample.txt 2>&1
	grep -q 'my own script' out/147/Make.sh && grep -q 'not generated by v32c++' out/147sample.txt && test -f out/147/Make.bat
	$(BIN)   -vvv --no-build-scripts -o out/147/other.c out/147/game.cpp 1> out/147b.txt 2>&1
	test ! -e out/147/other.sh
	cp tests/128sample.c out/148in.c
	cd out && ../$(TBIN) 148in.c 1> 148sample.txt 2>&1
	cmp tests/128sample.c out/148in.c
	test -f out/148in-transpiled.c && grep -q 'would overwrite its own input' out/148sample.txt
	! $(TBIN) -o out/148in.c out/148in.c 1> out/148b.txt 2>&1
	cmp tests/128sample.c out/148in.c
	rm -rf out/149 && mkdir -p out/149/src out/149/obj
	cp tests/143sample.cpp out/149/src/game.cpp
	cd out/149 && ../../$(BINC) --xml-preserve-paths -o obj/game.c src/game.cpp 1> ../149sample.txt 2>&1
	grep -q '<binary path="obj/game.vbin" />' out/149/game.xml
	grep -q '<texture path="textures/extra/ship.vtex" />' out/149/game.xml
	grep -q '<binary path="obj/gameOpt.vbin" />' out/149/gameOpt.xml
	test ! -e out/149/src/game.xml && test ! -e out/149/obj/game.xml
	grep -q '^need_tool v32c$$' out/149/Make.sh && grep -q '^v32c --xml-preserve-paths --no-build-scripts' out/149/Make.sh
	grep -q 'png2vircon textures/extra/ship.png -o textures/extra/ship.vtex' out/149/Make.sh
	grep -q 'packrom game.xml -o bin/game.v32' out/149/Make.sh
	$(TBIN)  -vvv -I libc -o out/150program.c tests/150sample.c 1> out/150sample.txt 2>&1
	head -n 1 out/150program.c | grep -q '^#include "math.h"$$'
	! grep -q '^int abs(' out/150program.c
	grep -q '^int v32_abs(int n)$$' out/150program.c
	$(TBIN)  -vvv -I libc -D MATH_H -o out/150define.c tests/150sample.c 1> out/150define.txt 2>&1
	grep -q 'include guard MATH_H is already defined' out/150define.txt || true
	$(TBIN)  -vvv    -o out/151program.c tests/151sample.cpp 1> out/151sample.txt 2>&1
	grep -q '^        "integer 11, 22, 33"$$' out/151program.c
	grep -q '^    total = (total + 40);$$\|^    (total += 40);$$' out/151program.c
	! grep -q '151table.inc\|151body.inc' out/151program.c
	$(TBIN)  -vvv -I c_api -o out/152program.c tests/152sample.cpp 1> out/152sample.txt 2>&1
	grep -q 'b = b->next' out/152program.c
	! grep -q '^struct malloc_block' out/152program.c
	rm -rf out/152odd && mkdir -p out/152odd
	printf 'struct plain { int a; plain *next; float f[2]; };\nstruct hook { int (*fn)(int); int x; };\n' > out/152odd/odd.h
	printf '#include "odd.h"\nvoid main (void) { plain p; p.a = 1; hook *h = 0; p.next = &p; }\n' > out/152odd/odd.cpp
	$(TBIN)  -vvv -o out/152odd/odd.c out/152odd/odd.cpp 1> out/152odd.txt 2>&1
	grep -q 'hook \* h' out/152odd/odd.c
	! grep -q '^struct plain' out/152odd/odd.c
	$(TBIN)  -vvv    -o out/153program.c tests/153sample.cpp 1> out/153sample.txt 2>&1
	! grep -q '^    this;$$' out/153program.c
	$(TBIN)  -vvv --silence-unused-args -o out/153quiet.c tests/153sample.cpp 1> out/153quiet.txt 2>&1
	grep -q '^    this;$$' out/153quiet.c
	grep -q '^    tail;$$' out/153quiet.c
	grep -q '^    ignored;$$' out/153quiet.c
	test "`grep -c '^    this;$$' out/153quiet.c`" = 1
	-V32CXX_SDK_INCLUDE= $(TBIN) -o out/154nosdk.c tests/154sample.c 1> out/154sample.txt 2>&1
	grep -q 'SDK header "misc.h" not found' out/154sample.txt
	rm -rf out/154sdk && mkdir -p out/154sdk/include && printf 'exit 0\n' > out/154sdk/compile && chmod +x out/154sdk/compile && : > out/154sdk/include/video.h && : > out/154sdk/include/misc.h
	-PATH="$$PWD/out/154sdk:$$PATH" V32CXX_SDK_INCLUDE= $(TBIN) -o out/154fake.c tests/154sample.c 1> out/154fake.txt 2>&1
	grep -q 'SDK header "misc.h" not found' out/154fake.txt
	grep -q "has no effect: if 'malloc_block' was meant as a type" out/154sample.txt
	grep -q '^#ifndef INT_MIN' libc/v32libc.h
	! grep -q 'environ' libc/v32libc.c
	$(TBIN)  -vvv    -o out/155program.c tests/155sample.c 1> out/155sample.txt 2>&1
	! grep -q '^    char \[[0-9]*\] __v32_tern_tmp' out/155program.c
	! grep -q '^    int __v32_tern_tmp[0-9]*;' out/155program.c
	grep -q 'char \* __v32_tern_tmp0;' out/155program.c
	$(TBIN)  -vvv    -o out/156program.c tests/156sample.c 1> out/156sample.txt 2>&1
	! grep -q '= {.*\[[^]]*\] = ' out/156program.c
	! grep -q '\.b = ' out/156program.c
	! $(TBIN) -o out/156bad.c tests/156bad.c 1> out/156bad.txt 2>&1
	grep -q "'S' has no member named 'c'" out/156bad.txt
	grep -q 'initializer index 5 is outside the array (length 3)' out/156bad.txt
	$(TBIN) -o out/157g7.c tests/157sample.c 1> out/157sample.txt 2>&1
	! grep -q 'warning' out/157sample.txt
	test "`grep -c '__v32_ptr[0-9] = me();' out/157g7.c`" = 4
	! grep -q '(me()->' out/157g7.c
	$(TBIN)  -vvv    -o out/158program.c tests/158sample.c 1> out/158sample.txt 2>&1
	! grep -q 'not checked' out/158sample.txt
	grep -q 'char \[4\] first_buf' out/158program.c
	! grep -q '__attribute__' out/158program.c
	! grep -q 'static_assert' out/158program.c
	grep -q 'N__e[0-9]' out/158program.c
	grep -q '__v32_cl_tmp' out/158program.c
	! $(TBIN) -o out/158bad.c tests/158bad.c 1> out/158bad.txt 2>&1
	grep -q 'static assertion failed: arithmetic is broken' out/158bad.txt
	$(TBIN)  -vvv    -o out/159program.c tests/159sample.c tests/159lib_a.c tests/159lib_b.c 1> out/159sample.txt 2>&1
	grep -q 'calls__159lib_a' out/159program.c
	grep -q 'helper__159lib_b\|helper__159lib_a' out/159program.c
	! grep -q '__v32_tu_begin_' out/159program.c
	test "`grep -c '^int twice(int v)$$' out/159program.c`" = 1
	! $(TBIN) -o out/159bad.c tests/159bad_dup.c tests/159bad_dup2.c 1> out/159bad_dup.txt 2>&1
	grep -q "multiple definition of 'dup'" out/159bad_dup.txt
	! $(TBIN) -o out/159bad.c tests/159bad_undef.c tests/159bad_undef2.c 1> out/159bad_undef.txt 2>&1
	grep -q "undefined reference to 'missing'" out/159bad_undef.txt
	grep -q "undefined reference to 'nowhere'" out/159bad_undef.txt
	! grep -q "unused_proto" out/159bad_undef.txt
	$(TBIN) --allow-undefined -o out/159bad.c tests/159bad_undef.c tests/159bad_undef2.c 1> out/159bad_undef_ok.txt 2>&1
	grep -q "warning: undefined reference to 'missing'" out/159bad_undef_ok.txt
	$(TBIN) --mklib out/lib160.v32a tests/160lib/add.c tests/160lib/mul.c tests/160lib/deep.c tests/160lib/unused.c
	grep -q '^member .* \.\./tests/160lib/add\.c$$' out/lib160.v32a
	$(TBIN)  -vvv    -o out/160program.c tests/160sample.c out/lib160.v32a 1> out/160sample.txt 2>&1
	grep -q '^int mul3(int a)$$' out/160program.c
	grep -q '^int deep(int a)$$' out/160program.c
	! grep -q 'unused_fn\|unused_tail' out/160program.c
	grep -q 'helper__add' out/160program.c
	$(TBIN) -o out/160second.c tests/160sample.c -L out -l160 1> out/160second.txt 2>&1
	cmp out/160program.c out/160second.c
	cp tests/160lib/unused.c out/160stale.c
	$(TBIN) --mklib out/160stale.v32a out/160stale.c
	echo '/* edited after the index was made */' >> out/160stale.c
	! $(TBIN) -o out/160bad.c tests/160sample.c out/160stale.v32a 1> out/160stale.txt 2>&1
	grep -q 'is out of date' out/160stale.txt
	$(TBIN)  -vvv    -o out/161program.c tests/161sample.c tests/161other.c 1> out/161sample.txt 2>&1
	grep -q '^struct cpu;$$' out/161program.c
	grep -q 'pipe_tag' out/161program.c
	! grep -q '__atomic' out/161program.c
	! grep -q ': error' out/161sample.txt
	test "`grep -c 'typedef int uint;' out/161program.c`" = 1
	! grep -q 'const ' out/161program.c
	$(TBIN)  -vvv    -o out/162program.c tests/162sample.cpp tests/162shape.cpp 1> out/162sample.txt 2>&1
	test "`grep -c '^struct Counter {' out/162program.c`" = 1
	$(TBIN)  -vvv    -o out/163program.c tests/163sample.c tests/163other.c 1> out/163sample.txt 2>&1
	grep -q 'ret_tmp0 = 10;' out/163program.c
	grep -q 'ret_tmp0 = 20;' out/163program.c
	$(TBIN)  -vvv    -o out/164program.c tests/164sample.c 1> out/164sample.txt 2>&1
	grep -q '__v32_cl_g0' out/164program.c
	! grep -q 'noreturn' out/164program.c
	$(TBIN)  -vvv    -o out/165program.c tests/165sample.c 1> out/165sample.txt 2>&1
	grep -q '\[-Wunsigned\]' out/165sample.txt
	rm -f out/165bad.c
	! $(TBIN) --warn-error -o out/165bad.c tests/165sample.c 1> out/165w1.txt 2>&1
	grep -q 'treated as errors' out/165w1.txt
	test ! -e out/165bad.c
	! $(TBIN) --warn-error=unsigned,pragma -o out/165bad.c tests/165sample.c 1> out/165w2.txt 2>&1
	$(TBIN) --warn-error=pragma -o out/165ok.c tests/165sample.c 1> out/165w3.txt 2>&1
	! $(TBIN) --warn-error=nosuch -o out/165bad.c tests/165sample.c 1> out/165w4.txt 2>&1
	grep -q 'not a warning kind' out/165w4.txt
	$(TBIN) --list-warnings | grep -q '^undefined-reference '
	$(TBIN)  -vvv    -o out/166program.c tests/166sample.c 1> out/166sample.txt 2>&1
	grep -q '__v32_frame_address' out/166program.c
	$(TBIN)  -vvv    -o out/167program.c tests/167sample.c 1> out/167sample.txt 2>&1
	grep -q '__v32_udiv' out/167program.c
	$(TBIN)  --no-unsigned-ops -o out/167off.c tests/167sample.c 1> out/167off.txt 2>&1
	! grep -q '__v32_udiv' out/167off.c
	$(TBIN)  -vvv    -o out/168program.c tests/168sample.c 1> out/168sample.txt 2>&1
	grep -q "__v32_a_buf" out/168program.c
	$(TBIN)  -vvv --yield-points=10 -o out/169program.c tests/169sample.c 1> out/169sample.txt 2>&1
	grep -q "get_cycle_counter() > 10" out/169program.c
	$(TBIN)  -vvv    -o out/170program.c tests/170sample.c 1> out/170sample.txt 2>&1
	grep -q "__v32_romd_squares" out/170program.c
	$(TBIN)  -vvv --rename abs2=my_abs -o out/171program.c tests/171sample.c 1> out/171sample.txt 2>&1
	grep -q "my_abs" out/171program.c
	$(TBIN)  -vvv    -o out/172program.c tests/172sample.c 1> out/172sample.txt 2>&1
	! grep -q "(0)" out/172program.c
	! $(TBIN) -o out/173bad.c tests/173bad_redef.cpp 1> out/173bad.txt 2>&1
	grep -q "redefinition of 'Game::init'" out/173bad.txt
	grep -q "redefinition of 'Game::run'" out/173bad.txt
	grep -q "was previously defined here" out/173bad.txt
	grep -q "no matching declaration for out-of-line definition of 'run'" out/173bad.txt
	! grep -q "no matching declaration for out-of-line definition of 'init'" out/173bad.txt
	$(TBIN)  -vvv    -o out/174program.c tests/174sample.cpp 1> out/174sample.txt 2>&1
	! grep -q '__v32_obj' out/174program.c
	grep -q 'Counter__Counter__int((&a\[1\]\[1\]), 4);' out/174program.c
	test "`grep -c 'Counter__Counter__int((&g_arr\[' out/174program.c`" = 2
	$(TBIN)  -vvv    -o out/175program.c tests/175sample.cpp 1> out/175sample.txt 2>&1
	grep -q 'Counter__Counter__int((&g_brace), 5);' out/175program.c
	grep -q 'Counter__Counter__int((&c), 1);' out/175program.c
	grep -q 'Counter__Counter__int((&d), 2);' out/175program.c
	grep -q 'int s1 = 3;' out/175program.c
	grep -q 'int s3 = 0;' out/175program.c
	grep -q 'P p = {1, 2};' out/175program.c
	grep -q 'P z = {0, 0};' out/175program.c
	grep -q 'Shade s7 = SHADE_DARK;' out/175program.c
	grep -q 'score_t s8 = 9;' out/175program.c
	! grep -q 'Counter [a-z_]* = {' out/175program.c
	! $(TBIN) -o out/175bad.c tests/175bad.cpp 1> out/175bad.txt 2>&1
	grep -q 'at most one value in braces' out/175bad.txt
	grep -q 'Counter__Counter__int_int((&t4), 4, 5);' out/175program.c
	grep -q 'Counter__Counter__int((&t7\[1\]), 8);' out/175program.c
	grep -q 'Counter__Counter__int((&t6), 6);' out/175program.c
	! grep -q '[ (]C2\? t[0-9] = {' out/175program.c
	$(TBIN)  -vvv    -o out/176program.c tests/176sample.cpp 1> out/176sample.txt 2>&1
	grep -q 'Counter__Counter__int((&this->m\[1\]), 2);' out/176program.c
	grep -q 'Counter__Counter__int((&this->g\[1\]\[0\]), 3);' out/176program.c
	grep -q 'Counter__dtor__void((&this->g\[__v32_mdtor_i[0-9]*\]\[__v32_mdtor_i[0-9]*\]));' out/176program.c
	grep -q 'Counter__Counter__int((&this->m\[1\]), (a + 1));' out/176program.c
	! grep -q '__v32_obj' out/176program.c
	! $(TBIN) -o out/176bad.c tests/176bad.cpp 1> out/176bad.txt 2>&1
	grep -q "'m' is an array: a list of values can only initialize an array of objects whose class has a constructor" out/176bad.txt
	$(TBIN)  -vvv    -o out/177program.c tests/177sample.cpp 1> out/177sample.txt 2>&1
	grep -q '(v32_exit_destructors = (&__v32_destroy_globals));' out/177program.c
	grep -q 'int __v32_globals_live = 0;' out/177program.c
	grep -q 'D__dtor__void((&g_b));' out/177program.c
	grep -q 'D__dtor__void((&g_a));' out/177program.c
	test "`grep -n 'D__dtor__void((&g_b));' out/177program.c | head -1 | cut -d: -f1`" -lt "`grep -n 'D__dtor__void((&g_a));' out/177program.c | head -1 | cut -d: -f1`"
	sh tests/globals_destroy.sh $(BIN)
	! $(TBIN) -o out/178bad.c tests/178bad.cpp 1> out/178bad.txt 2>&1
	grep -q "178bad.cpp:5: error: the cart hint name 'HERO' is already used: #texture here, and #texture at tests/178bad.cpp:3" out/178bad.txt
	! $(TBIN) -o out/178bad_sound.c tests/178bad_sound.cpp 1> out/178bad_sound.txt 2>&1
	grep -q "178bad_sound.cpp:4: error: the cart hint name 'MUSIC' is already used: #sound here, and #texture at tests/178bad_sound.cpp:3" out/178bad_sound.txt
	$(TBIN)  -vvv    -o out/179program.c tests/179sample.cpp 1> out/179sample.txt 2>&1
	grep -q 'v32_delete_arr_D(arr)\|v32_delete_arr_D(p)' out/179program.c
	grep -q 'hdr\[0\] = n;' out/179program.c
	! grep -q '__v32_alloc' out/179program.c
	sh tests/std_run.sh $(BIN) 179sample
	sh tests/std_run.sh $(BIN) 179sample --alloc=arena
	$(TBIN)  -vvv    -o out/180program.c tests/180sample.cpp 1> out/180sample.txt 2>&1
	grep -q 'D__dtor__void((&c));' out/180program.c
	sh tests/std_run.sh $(BIN) 180sample
	! $(TBIN) -o out/180bad.c tests/180bad.cpp 1> out/180bad.txt 2>&1
	grep -q "180bad.cpp:12: error: goto 'over' jumps past the declaration of 'd'" out/180bad.txt
	grep -q "180bad.cpp:20: error: goto 'inside' jumps past the declaration of 'd'" out/180bad.txt
	$(TBIN)  -vvv --alloc=arena -I . -o out/181program.c tests/181sample.cpp 1> out/181sample.txt 2>&1
	grep -q '__v32_arena_init();' out/181program.c
	grep -q 'v32__arena_release__int' out/181program.c
	grep -q 'vector_int__reserve__int' out/181program.c
	grep -q '__v32_alloc((n \* sizeof(int)))' out/181program.c
	! grep -q 'malloc(n \* sizeof(int))' out/181program.c
	sh tests/std_run.sh $(BIN) 181sample --alloc=arena -I .
	$(TBIN)  -vvv --alloc=arena -I . -o out/181string.c tests/181string.cpp 1> out/181string.txt 2>&1
	sh tests/arena_full.sh $(BIN)
	! $(TBIN) -I . -o out/181bad.c tests/181sample.cpp 1> out/181bad.txt 2>&1
	grep -q 'needs --alloc=arena' out/181bad.txt
	$(TBIN)  -vvv --alloc=arena --heap-words=1000 -o out/181words.c tests/181full.cpp -I . 1> out/181words.txt 2>&1
	grep -q '__v32_arena_size = 1000 \* sizeof(int);' out/181words.c
	! $(TBIN) --alloc=bogus -o out/181opt.c tests/181full.cpp 1> out/181opt.txt 2>&1
	grep -q 'is malloc, arena or gc' out/181opt.txt
	$(TBIN)  -vvv --alloc=gc --heap-words=16384 -I . -o out/182program.c tests/182sample.cpp 1> out/182sample.txt 2>&1
	grep -q '__v32_gc_init();' out/182program.c
	grep -q '__v32_gc_root_a\[0\] = (int)&g_keep;' out/182program.c
	grep -q '__v32_gc_root_a\[3\] = (int)&g_ring\[0\];' out/182program.c
	grep -q '"push R10"' out/182program.c
	grep -q 'mov {stk}, SP' out/182program.c
	grep -q '__v32_gc_stack_top = 4194304;' out/182program.c
	grep -q 'v32__gc_collect__void' out/182program.c
	grep -q '__v32_alloc((n \* sizeof(int)))' out/182program.c
	! grep -q '__v32_alloc_rec\|__v32_arena\|malloc(n \* sizeof' out/182program.c
	sh tests/std_run.sh $(BIN) 182sample --alloc=gc --heap-words=16384 -I .
	sh tests/std_run.sh $(BIN) 182sample --alloc=gc --heap-words=16384 -DGC_NO_THRESHOLD --gc-threshold=100000000 -I .
	sh tests/gc_full.sh $(BIN)
	$(TBIN)  -vvv --alloc=gc --heap-words=1000 --gc-threshold=300 -o out/182words.c tests/182full.cpp -I . 1> out/182words.txt 2>&1
	grep -q '__v32_gc_size = 1000 \* sizeof(int);' out/182words.c
	grep -q '__v32_gc_thresh = 300 \* sizeof(int);' out/182words.c
	! $(TBIN) --alloc=gc -I . -o out/182bad.c tests/181sample.cpp 1> out/182bad.txt 2>&1
	grep -q "no function 'arena_mark' is declared" out/182bad.txt
	! $(TBIN) --alloc=arena -I . -o out/182bad2.c tests/182sample.cpp 1> out/182bad2.txt 2>&1
	grep -q "no function 'gc_collect' is declared" out/182bad2.txt
	! $(TBIN) --gc-threshold=0 -o out/182bad3.c tests/181full.cpp 1> out/182bad3.txt 2>&1
	! grep -q '__v32_gc' out/179program.c out/181program.c
	$(TBIN)  -vvv    -o out/183program.c tests/183sample.cpp 1> out/183sample.txt 2>&1
	grep -q 'Base__Base__int(((Base \*)this), 3);' out/183program.c
	grep -q 'Base__Base__int(((Base \*)this), a);' out/183program.c
	sh tests/std_run.sh $(BIN) 183sample
	$(TBIN)  -vvv    -o out/184program.c tests/184sample.cpp -I . 1> out/184sample.txt 2>&1
	grep -q 'v32_delete_arr_D(pp\[0\]);' out/184program.c
	grep -q 'v32_delete_arr_D(h.arr2\[0\]);' out/184program.c
	grep -q 'v32_delete_arr_D(g_factory());' out/184program.c
	grep -q 'v32_delete_arr_D((rest - 1));' out/184program.c
	grep -q 'v32_delete_arr_D(vec\[0\]);\|v32_delete_arr_D(this->vec.m_data\[0\]);' out/184program.c
	grep -q 'v32_delete(arr);' out/184program.c
	sh tests/std_run.sh $(BIN) 184sample -I .
	sh tests/std_run.sh $(BIN) 184sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 184sample --alloc=gc --heap-words=16384 -I .
	! $(TBIN) -o out/184bad.c tests/184bad.cpp 1> out/184bad.txt 2>&1
	grep -q "184bad.cpp:15: error: delete\[\]: v32c++ cannot tell the element type of this operand" out/184bad.txt
	grep -q 'cast it to a pointer to the element type' out/184bad.txt
	! grep -q ':17: error' out/184bad.txt
	$(TBIN) -o out/184ok.c tests/184ok.cpp 1> out/184ok.txt 2>&1
	$(TBIN)  -vvv --alloc=gc --heap-words=16384 -I . -o out/185program.c tests/185sample.cpp 1> out/185sample.txt 2>&1
	grep -q 'n = n + 1;   /\* one trailing unit' out/185program.c
	sh tests/std_run.sh $(BIN) 185sample --alloc=gc --heap-words=16384 -I .
	sh tests/std_run.sh $(BIN) 185sample --alloc=gc --heap-words=16384 -DGC_NO_THRESHOLD --gc-threshold=100000000 -I .
	! grep -q 'one trailing unit' out/179program.c out/181program.c
	$(TBIN)  -vvv    -o out/186program.c tests/186sample.cpp 1> out/186sample.txt 2>&1
	grep -q 'Counter__Counter__int((&game__arr\[1\]), 4);' out/186program.c
	grep -q 'Counter__Counter__int((&game__inner__deep), 5);' out/186program.c
	grep -q 'Counter__dtor__void((&game__again));' out/186program.c
	test "`grep -n 'Counter__Counter__int((&game__g), 2);' out/186program.c | head -1 | cut -d: -f1`" -lt "`grep -n 'Counter__Counter__int((&t2), 6);' out/186program.c | head -1 | cut -d: -f1`"
	$(TBIN)  -vvv --alloc=gc -I . -o out/186gc.c tests/186gc.cpp 1> out/186gc.txt 2>&1
	grep -q '__v32_gc_root_a\[[0-9]\] = (int)&app__deep__tail;' out/186gc.c
	grep -q '__v32_gc_root_a\[[0-9]\] = (int)&app__owner;' out/186gc.c
	sh tests/std_run.sh $(BIN) 186gc --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/187program.c tests/187sample.cpp 1> out/187sample.txt 2>&1
	grep -q 'v32_new_arr_Item_init(__v32_nl_n[0-9]*, 3)' out/187program.c
	grep -q 'v32_new_arr_Item_rest(' out/187program.c
	! grep -q 'v32_new_arr_.*_init' out/179program.c out/181program.c
	sh tests/std_run.sh $(BIN) 187sample
	sh tests/std_run.sh $(BIN) 187sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 187sample --alloc=gc --heap-words=16384 -I .
	! $(TBIN) -o out/187bad.c tests/187bad.cpp 1> out/187bad.txt 2>&1
	grep -q "187bad.cpp:16: .*class 'NoDefault' has no default constructor" out/187bad.txt
	grep -q "187bad.cpp:17: .*class 'Bare' has no constructor" out/187bad.txt
	grep -q "187bad.cpp:18: .*takes a value, not a list in braces" out/187bad.txt
	$(TBIN)  -vvv    -o out/188program.c tests/188sample.cpp 1> out/188sample.txt 2>&1
	grep -q '^int a__x = 1;' out/188program.c
	grep -q '^int b__x = 2;' out/188program.c
	grep -q '^int x = 100;' out/188program.c
	grep -q '^int a__inner__x = 3;' out/188program.c
	grep -q 'Cell__Cell__int((&a__inner__c), 23);' out/188program.c
	grep -q 'Other__Other__int((&b__c), 4);' out/188program.c
	sh tests/std_run.sh $(BIN) 188sample
	sh tests/std_run.sh $(BIN) 188sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 188sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv --alloc=gc -I . -o out/188gc.c tests/188sample.cpp 1> out/188gc.txt 2>&1
	grep -q '(int)&a__inner__c;\|(int)&a__px;' out/188gc.c
	$(TBIN)  -vvv -c -o out/188extern.c tests/188extern.cpp 1> out/188extern.txt 2>&1
	grep -q '^int c_side;' out/188extern.c
	grep -q '^int \* c_table;' out/188extern.c
	grep -q '^int hw__mine = 2;' out/188extern.c
	grep -q '^int other__mine = 3;' out/188extern.c
	! grep -q 'hw__c_side' out/188extern.c
	$(TBIN)  -vvv    -o out/189program.c tests/189sample.cpp 1> out/189sample.txt 2>&1
	grep -q '^D \*()\* g = (&make__void);' out/189program.c
	grep -q '^int(int)\* h = (&square__int);' out/189program.c
	grep -q '(&inc__int), (&dec__int)' out/189program.c
	grep -q '^int(int)\* n__hp = (&n__tw__int);' out/189program.c
	grep -q '^int(int)\* hs = (&n__tw__int);' out/189program.c
	grep -q '^int(float)\* ho_float = (&over__float);' out/189program.c
	grep -q '^int(int)\* ho_int = (&over__int);' out/189program.c
	test "`grep -n '^int square__int(int x);' out/189program.c | head -1 | cut -d: -f1`" -lt "`grep -n '^D \*()\* g = ' out/189program.c | head -1 | cut -d: -f1`"
	sh tests/std_run.sh $(BIN) 189sample
	sh tests/std_run.sh $(BIN) 189sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 189sample --alloc=gc --heap-words=16384 -I .
	! $(TBIN) -o out/189bad.c tests/189bad.cpp 1> out/189bad.txt 2>&1
	grep -q "189bad.cpp:5: error: 'over' is overloaded and no overload has the signature" out/189bad.txt
	grep -q "189bad.cpp:9: error: 'over' is overloaded and no overload has the signature" out/189bad.txt
	! grep -q "189bad.cpp:6" out/189bad.txt
	$(TBIN)  -vvv    -o out/190program.c tests/190sample.cpp 1> out/190sample.txt 2>&1
	grep -q 'Node \* \* v = ((Node \* \*)v32_new_arr_ptr(n));' out/190program.c
	grep -q '^int \*\*v32_new_arr_ptr(int n)' out/190program.c
	grep -q 'void reset__Node_ptr_ref(Node \* \* p);' out/190program.c
	grep -q 'reset__Node_ptr_ref((&arr\[0\]));' out/190program.c
	grep -q 'reset__Node_ptr_ref((&h.pair\[1\]));' out/190program.c
	grep -q 'Node \* \* rc = (&c);' out/190program.c
	grep -q 'put__Node_ptr_ref_int(p, n);' out/190program.c
	! grep -q 'v32_new_arr_ptr' out/188program.c out/189program.c
	sh tests/std_run.sh $(BIN) 190sample
	sh tests/std_run.sh $(BIN) 190sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 190sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/191program.c tests/191sample.c 1> out/191sample.txt 2>&1
	grep -q '^char \* \* environ;' out/191program.c
	grep -q '^int seed = 5;' out/191program.c
	test "`grep -c '^int seed' out/191program.c`" = 1
	test "`grep -c 'environ;' out/191program.c`" = 1
	! grep -q 'auto' out/191program.c
	sh tests/std_run.sh $(BIN) 191sample
	$(TBIN) -I libc -I demos/c/rogue/v32 -I demos/c/rogue/src -o out/191rogue.c demos/c/rogue/rogue.c 1> out/191rogue.txt 2>&1
	grep -q '^char \* \* environ;' out/191rogue.c
	! grep -q 'error' out/191rogue.txt
	$(TBIN)  -vvv    -o out/192program.c tests/192sample.cpp 1> out/192sample.txt 2>&1
	grep -q '^int g1;' out/192program.c
	grep -q '^int g4;' out/192program.c
	grep -q '^int g5;' out/192program.c
	grep -q '^int ml2;' out/192program.c
	grep -q '^int hw__sub = 5;' out/192program.c
	grep -q '^int c_two;' out/192program.c
	grep -q '^int c_side;' out/192program.c
	grep -q '^int hw__mine = 4;' out/192program.c
	grep -q '^int n__v = 6;' out/192program.c
	grep -q '^int p__q__w = 9;' out/192program.c
	grep -q '^int z;' out/192program.c
	test "`grep -c '^int c_def' out/192program.c`" = 1
	test "`grep -c '^int n__v\|^int v;' out/192program.c`" = 1
	! grep -q 'hw__c_def\|hw__c_two\|hw__c_side\|p__q__z' out/192program.c
	sh tests/std_run.sh $(BIN) 192sample
	sh tests/std_run.sh $(BIN) 192sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 192sample --alloc=gc --heap-words=16384 -I .
	! $(TBIN) -o out/192bad.c tests/192bad.cpp 1> out/192bad.txt 2>&1
	grep -q "192bad.cpp:3: error: syntax error" out/192bad.txt
	$(TBIN)  -vvv    -o out/193program.c tests/193sample.cpp 1> out/193sample.txt 2>&1
	grep -q '^    a__RED,' out/193program.c
	grep -q '^    a__BLUE = (a__GREEN + 1)' out/193program.c
	grep -q '^    b__RED = 7,' out/193program.c
	grep -q '^    a__in__DEEP = (a__in__RED + 1)' out/193program.c
	grep -q '^    TOP_B = 40' out/193program.c
	grep -q '^enum Top {' out/193program.c
	grep -q 'case a__BLUE:' out/193program.c
	grep -q 'a__pick__int(a__GREEN)' out/193program.c
	! grep -q '^    RED\|^    GREEN\|^    BLUE\|[^_a-z]a__E__\|::' out/193program.c
	sh tests/std_run.sh $(BIN) 193sample
	sh tests/std_run.sh $(BIN) 193sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 193sample --alloc=gc --heap-words=16384 -I .
	$(TBIN) -o out/193bad.c tests/193bad.cpp 1> out/193bad.txt 2>&1
	grep -q '^enum a__E {' out/193bad.c
	grep -q '^enum b__E {' out/193bad.c
	$(TBIN)  -vvv    -o out/194program.c tests/194sample.cpp 1> out/194sample.txt 2>&1
	grep -q '^int \*\*v32_new_arr_ptr_init(int n, int k)' out/194program.c
	grep -q '^void v32_new_arr_ptr_rest(int \*\*p, int n, int k)' out/194program.c
	grep -q 'i = k; i < n; i++) p\[i\] = NULL;' out/194program.c
	grep -q '(Node \* \*)v32_new_arr_ptr_init(__v32_nl_n[0-9]*, 2)' out/194program.c
	grep -q 'v32_new_arr_ptr_rest(((int \* \*)__v32_nl_tmp[0-9]*), __v32_nl_n[0-9]*, 2);' out/194program.c
	! grep -q 'v32_new_arr_ptr_init' out/190program.c out/187program.c
	sh tests/std_run.sh $(BIN) 194sample
	sh tests/std_run.sh $(BIN) 194sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 194sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/195program.c tests/195sample.cpp 1> out/195sample.txt 2>&1
	grep -q 'P z = {0, 0};' out/195program.c
	grep -q 'R \* g_rp = NULL;' out/195program.c
	grep -q 'R \[2\] gra = {{1, NULL}, {0, NULL, 0.0}};' out/195program.c
	grep -q 'int \[4\] a = {0, 0, 0, 0};' out/195program.c
	grep -q '((\*p) = NULL);' out/195program.c
	grep -q '^    P zero = {0, 0};' out/195program.c
	sh tests/std_run.sh $(BIN) 195sample
	sh tests/std_run.sh $(BIN) 195sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 195sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/196program.c tests/196sample.cpp 1> out/196sample.txt 2>&1
	grep -q '^    P zero = {0, NULL, 0.0};' out/196program.c
	grep -q 'p\[i\] = zero;' out/196program.c
	sh tests/std_run.sh $(BIN) 196sample
	sh tests/std_run.sh $(BIN) 196sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 196sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/197program.c tests/197sample.cpp 1> out/197sample.txt 2>&1
	grep -q '(gs.u.a = 2);' out/197program.c
	grep -q '(g1.a = 7);' out/197program.c
	grep -q '(ns__nu.a = 9);' out/197program.c
	! grep -q 'U g1 = ' out/197program.c
	sh tests/std_run.sh $(BIN) 197sample
	sh tests/std_run.sh $(BIN) 197sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 197sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/198program.c tests/198sample.cpp 1> out/198sample.txt 2>&1
	grep -q 'Counter__Counter__int((&this->c), 3);' out/198program.c
	grep -q '(this->w = 5);' out/198program.c
	grep -q '(this->ptr = NULL);' out/198program.c
	sh tests/std_run.sh $(BIN) 198sample
	sh tests/std_run.sh $(BIN) 198sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 198sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/199program.c tests/199sample.cpp 1> out/199sample.txt 2>&1
	grep -q 'v32_new_int(5)' out/199program.c
	grep -q 'v32_new_ptr(NULL)' out/199program.c
	! grep -q 'v32_new_Shade' out/199program.c
	sh tests/std_run.sh $(BIN) 199sample
	sh tests/std_run.sh $(BIN) 199sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 199sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/200program.c tests/200sample.cpp 1> out/200sample.txt 2>&1
	grep -q 'Color__Red' out/200program.c
	grep -q 'ns__Mode__On' out/200program.c
	grep -q '^int g__a__E(a__E x);' out/200program.c
	grep -q '^int g__b__E(b__E x);' out/200program.c
	sh tests/std_run.sh $(BIN) 200sample
	$(TBIN)  -vvv    -o out/201program.c tests/201sample.cpp 1> out/201sample.txt 2>&1
	! grep -q '^[a-z_]* \*\? *malloc_first_block;\|^[a-z_]* \*\? *malloc_start_address;' out/201program.c
	grep -q 'malloc_first_block == NULL' out/201program.c
	V32CXX_SDK_INCLUDE=tests/sdkstub $(TBIN) -vvv -o out/201stub.c tests/201stub.cpp 1> out/201stub.txt 2>&1
	! grep -q '^int .*sdk_\|^int \*sdk_' out/201stub.c
	grep -q 'sdk_tick_count + sdk_table\[1\]' out/201stub.c
	grep -q '^int mine = 3;' out/201stub.c
	$(TBIN)  -vvv    -o out/202program.c tests/202sample.cpp 1> out/202sample.txt 2>&1
	grep -q 'v32_new_arr_P_init' out/202program.c
	sh tests/std_run.sh $(BIN) 202sample
	sh tests/std_run.sh $(BIN) 202sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 202sample --alloc=gc --heap-words=16384 -I .
	$(TBIN)  -vvv    -o out/203program.c tests/203sample.cpp 1> out/203sample.txt 2>&1
	grep -q '^int a__f__E_int(E e, int y);' out/203program.c
	grep -q 'a__b__h__a__b__F_int' out/203program.c
	grep -q 'C__C__int' out/203program.c
	! grep -q ' a::' out/203program.c
	sh tests/std_run.sh $(BIN) 203sample
	sh tests/std_run.sh $(BIN) 203sample --alloc=arena -I .
	$(TBIN)  -vvv    -o out/204program.c tests/204sample.cpp 1> out/204sample.txt 2>&1
	test "$$(grep -n '^union V {' out/204program.c | cut -d: -f1)" -gt "$$(grep -n '^struct B {' out/204program.c | cut -d: -f1)"
	test "$$(grep -n '^union Pick {' out/204program.c | cut -d: -f1)" -gt "$$(grep -n '^struct Derived {' out/204program.c | cut -d: -f1)"
	grep -q '^union Late;' out/204program.c
	sh tests/std_run.sh $(BIN) 204sample
	! $(TBIN) -o out/204bad.c tests/204bad.cpp 1> out/204bad.txt 2>&1
	grep -q "204bad.cpp:4: error: 'U' and 'S' hold each other by value" out/204bad.txt
	$(TBIN)  -vvv    -o out/205program.c tests/205sample.cpp 1> out/205sample.txt 2>&1
	grep -q 'Color__Blue = (Color__Red + 5)' out/205program.c
	sh tests/std_run.sh $(BIN) 205sample
	! $(TBIN) -o out/205bad.c tests/205bad.cpp 1> out/205bad.txt 2>&1
	grep -q "205bad.cpp:5: error: 'Red' is a scoped enumerator: write 'Shade::Red' or 'Color::Red'" out/205bad.txt
	grep -q "205bad.cpp:6: error: 'Green' is a scoped enumerator: write 'Color::Green'" out/205bad.txt
	grep -q "205bad.cpp:3: error: 'Two' is a scoped enumerator: write 'a::K::Two'" out/205bad.txt
	$(TBIN)  -vvv    -o out/206program.c tests/206sample.cpp 1> out/206sample.txt 2>&1
	grep -q 'zero.k.a = 0' out/206program.c
	sh tests/std_run.sh $(BIN) 206sample
	sh tests/std_run.sh $(BIN) 206sample --alloc=arena -I .
	sh tests/std_run.sh $(BIN) 206sample --alloc=gc --heap-words=16384 -I .
	! $(TBIN) -o out/207bad.c tests/207bad.cpp 1> out/207bad.txt 2>&1
	grep -q "207bad.cpp:7: no declaration of 'zz' in namespace 'a' matches this definition" out/207bad.txt
	grep -q "207bad.cpp:8: no matching declaration for out-of-line definition of 'nope' in class 'C'" out/207bad.txt
	grep -q "207bad.cpp:9: no declaration of 'f' in namespace 'a' matches this definition" out/207bad.txt
	grep -q "207bad.cpp:12: no declaration of 'g' in namespace 'a::in' matches this definition" out/207bad.txt
	grep -q "207bad.cpp:14: no declaration of 'missing' in namespace 'a::in' matches this definition" out/207bad.txt
	test "$$(grep -c 'matches this definition\|no matching declaration' out/207bad.txt)" = 5
	$(TBIN)  -vvv    -o out/208program.c tests/208sample.cpp 1> out/208sample.txt 2>&1
	for f in a__f__D a__g__D a__h__D_ptr a__lf__D a__in__p2__D b__q__D b__q2__D c__r__D c__r2__D c__r3__D_ptr \
	         C__C__D C__m__D C__n__D C__op_add__D C__op_sub__D; do \
	    test "$$(grep -c "^[a-z]* $$f(.*);$$" out/208program.c)" = 1 && test "$$(grep -c "^[a-z]* $$f(.*)$$" out/208program.c)" = 1 \
	        || { echo "208: prototype/definition differ: $$f"; exit 1; }; \
	done
	! grep -q '__a_D\|a_in_E\|_AD\b' out/208program.c
	sh tests/std_run.sh $(BIN) 208sample
	@if [ -f tools/vircon32/bin/include/video.h ]; then \
	    echo "209bad: a C program defining SDK functions, with --alloc=arena (needs the SDK headers)"; \
	    ! V32CXX_SDK_INCLUDE=tools/vircon32/bin/include $(TBIN) --alloc=arena -o out/209bad.c tests/209bad.c 1> out/209bad.txt 2>&1 \
	    && grep -q "209bad.c:3: error: 'clear_screen' is also defined by the SDK header 'video.h'" out/209bad.txt \
	    && grep -q "209bad.c:4: error: 'malloc' is also defined by the SDK header 'misc.h'" out/209bad.txt \
	    && grep -q "use --rename malloc=NEW_NAME" out/209bad.txt \
	    && ! grep -q "get_cycle_counter" out/209bad.txt \
	    && V32CXX_SDK_INCLUDE=tools/vircon32/bin/include $(TBIN) --alloc=arena -o out/169arena.c tests/169sample.c 1> out/169arena.txt 2>&1 \
	    && ! grep -q '#include "time.h"' out/169arena.c || { echo "209bad/169arena failed"; exit 1; }; \
	else echo "skipping 209bad: needs the SDK headers (run tools/vircon32/build-tools.sh)"; fi
# `-vvv` (this project's own verbosity flag, a later round -- see
# main.c) is passed to every sample specifically so `make test`'s own
# output still captures the full AST/semantic-analysis/lowering dumps
# this suite has always relied on for review -- v32c++ is silent by
# default now (no -v at all), matching how the real Vircon32 C compiler
# and v32lua both behave, so without -vvv these dumps simply wouldn't
# appear anywhere. `-vv`'s own explanatory-comments feature (a still
# later round) is a genuine side effect of this, not something worth
# fighting -- -vvv is cumulative, so every sample's own generated `.c`
# now picks up whatever comments -vv would add too, sample32 (vtables,
# a virtual destructor, `new`/`delete` -- the richest feature mix here)
# included among them, but no longer singled out for it the way it once
# was (see docs/DESIGN_NOTES.md for why the levels swapped, and for
# sample32's own earlier, now-stale special case). `-o out/sampleN.c` is
# passed too, specifically so the now-always-written generated C lands
# in `out/` alongside its own `.txt` dump rather than next to the `.cpp`
# source in `tests/` -- v32c++ writes an output file by default now
# (derived from the input's own name) even with no `-o` given at all, so
# leaving it unset here would otherwise clutter `tests/` with 32
# generated `.c` files never meant to live there.
#
# `-c` (this project's own flag, an earlier round) opts out
# of the "must define main" default main.c added this round -- every
# sample here is a focused unit test of one specific compiler feature,
# not a complete, standalone-compilable program, EXCEPT sample2, 14, 21,
# 22, 23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35, 36, 37, 38, 39, 40,
# 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
# 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, and 71, which genuinely
# do define their own `main` (sample2's predates this round; sample14/21
# were given one specifically so they could also be compiled all the
# way through by the real Vircon32 toolchain, not just transpiled;
# sample22 through 30, 32, 33, and 34 already had one; 35 through 71 --
# base-class constructor delegation, member-field initializers,
# bitwise operators/switch, global variables, struct, C-style casts,
# numeric literal formats/suffixes, C++-style
# casts, ternary, do-while, enum, union, sizeof, goto, function
# pointers (both declarator styles, plain and array), multi-
# dimensional arrays, const, const member functions, reference-
# parameter call-site lowering, and the vircon32-mode ternary-to-
# if/else rewrite, several
# later rounds -- also define their own, even though 36, 38, 42,
# 45, 49, and 53 are themselves deliberately invalid -- see below).
# sample31 is deliberately invalid (break/continue-outside-a-loop) and
# gets `-c` like the other unit-test samples, since it isn't trying to
# be a complete program at all. sample36/38/42/45/49/53 are ALSO
# deliberately invalid (an unknown name, a class-typed field, a field
# named twice, a base with no zero-arg constructor, `continue` inside a
# switch with no enclosing loop, and a class's own still-private-by-
# default member accessed from outside, respectively) but do NOT get
# `-c` -- they already define their own `main`, same as 35, so `-c`
# would be redundant, not wrong, but left off for consistency with how
# 35 itself is invoked. sample57 is NOT in that deliberately-invalid
# list despite producing a warning (dynamic_cast, see
# docs/DESIGN_NOTES.md) -- a warning never causes this project's own
# transpile to fail (see sema_get_warning_count's own doc comment in
# sema.h), so it gets no `-` prefix here either, same as any other
# passing sample. sample37 was ALSO deliberately invalid once
# (the very case sample38 now tests -- a member-initializer list naming
# an ordinary field, "not yet supported" at the time), repurposed into a
# real, passing test once primitive member-field initializers themselves
# became supported; sample38 is its replacement as the "still not
# supported" edge (a class-typed field specifically) member-initializer
# lists now have. sample24/32 needed real, one-line fixes the SAME round
# implicit base-class construction shipped (see docs/DESIGN_NOTES.md) --
# both had a derived constructor that never explicitly delegated to a
# base with no zero-arg constructor of its own, which was always
# invalid real C++, just never caught until this project's own
# check_implicit_base_construction existed to catch it. Without `-c`,
# every other sample would now fail at the
# "no main function found" check before ever reaching lowering/codegen --
# not a bug in that check, just what it's supposed to do by default; this
# project's own test suite is exactly the kind of "library/module
# fragment, not a complete program" case `-c` exists for.
# sample4/sample5 are deliberately-invalid inputs (see their own header
# comments) -- they're SUPPOSED to return nonzero. The leading '-' tells
# make to ignore their exit code and keep going, rather than aborting the
# whole `test` run at the first "expected" failure the way it did before
# (sample4 failing used to silently prevent sample5 from ever running).
# sample1/2/3 are deliberately NOT prefixed with '-': if any of those
# start failing, that's a real regression and `make test` should stop and
# report it, not paper over it the same way.
	sh tests/line_markers.sh $(BIN) $(V32CC)
	sh tests/debug_flow.sh $(BIN)
	sh tests/exe_relative.sh $(BIN)
	@test ! -e tests/Make.sh && test ! -e tests/Make.bat || { echo 'a test wrote Make.sh/Make.bat into tests/ (missing --no-build-scripts)'; exit 1; }

# realcheck: `make test`, then push every generated program through the
# REAL Vircon32 compiler/assembler/packer and boot every self-checking
# sample (one that declares `int test_errors`) on a headless console.
# Needs tools/vircon32/bin: run tools/vircon32/build-tools.sh once first.
realcheck: test
	@test -f tools/vircon32/bin/BUILD-INFO || { echo 'realcheck: no toolchain in tools/vircon32/bin -- building it'; sh tools/vircon32/build-tools.sh; }
	tools/vircon32/check.sh

# Show the version (from inc/v32cxx.h) and stamp it, with today's date,
# into every manual page that belongs to v32c++: a .TH line whose fourth
# field is "v32c++ <anything>" becomes
#     .TH <title> <section> "<Month day, year>" "v32c++ <VERSION>" ...
# (title, section and the fifth field are kept). Alias pages (.so) and any
# page whose .TH names something else are left alone. A page is rewritten
# only when its line actually changes, so its mtime says when it was last
# stamped. Run after changing VERSION, before a release.
# With VERSION=... on the command line the #define in inc/v32cxx.h is rewritten first (the header stays the
# single source; rebuild afterwards so the binary picks it up):  make version VERSION=20261105-dev
version:
ifeq ($(origin VERSION),command line)
	@sed 's/^\(#define[[:space:]]*VERSION[[:space:]]*\)"[^"]*"/\1"$(VERSION)"/' $(INC_DIR)/v32cxx.h > $(INC_DIR)/v32cxx.h.tmp && \
	    if cmp -s $(INC_DIR)/v32cxx.h $(INC_DIR)/v32cxx.h.tmp; then rm -f $(INC_DIR)/v32cxx.h.tmp; \
	    else mv $(INC_DIR)/v32cxx.h.tmp $(INC_DIR)/v32cxx.h; echo "stamped $(INC_DIR)/v32cxx.h"; fi
endif
	@echo "v32c++ $(VERSION) ($(MANDATE))"
	@for f in $(MAN1) $(MAN3) $(ES_MAN1) $(ES_MAN3) $(FR_MAN1) $(FR_MAN3); do \
	    grep -q '^\.TH .* "v32c++ [^"]*"' "$$f" || continue; \
	    case "$$f" in man/es/*) d='$(MANDATE_ES)' ;; man/fr/*) d='$(MANDATE_FR)' ;; *) d='$(MANDATE)' ;; esac; \
	    sed "s/^\(\.TH [^ ]* [^ ]*\) \"[^\"]*\" \"v32c++ [^\"]*\"/\1 \"$$d\" \"v32c++ $(VERSION)\"/" \
	        "$$f" > "$$f.tmp" || exit 1; \
	    if cmp -s "$$f" "$$f.tmp"; then rm -f "$$f.tmp"; \
	    else mv "$$f.tmp" "$$f"; echo "stamped $$f"; fi; \
	done
	@grep -h '^\.TH' $(MAN1) $(MAN3) $(ES_MAN1) $(ES_MAN3) $(FR_MAN1) $(FR_MAN3) | sed 's/^\.TH \([^ ]*\) \([^ ]*\) \("[^"]*" "[^"]*"\).*/  \1(\2) \3/'

install: all
	mkdir -p $(HOME)/bin
	cp $(BIN) $(BINC) $(HOME)/bin/
	@echo "Installed v32c++ and v32c to $(HOME)/bin"
	@echo "(make sure $(HOME)/bin is on your PATH to run it as just 'v32c++')"

uninstall:
	rm -f $(HOME)/bin/v32c++ $(HOME)/bin/v32c

# System-wide install (under PREFIX, default /usr/local; DESTDIR stages it): the
# binary to PREFIX/bin, the manual pages to
# PREFIX/share/man/man1 (v32c++.1, plus the Vircon32 commands' pages from
# man/vircon32/) and man3 (the 3v32 header pages and their aliases, plus the
# Vircon32 C library's from man/vircon32/), and, under V32CXX_INCLUDE_PATH (inc/config.h;
# /usr/local/Vircon32/v32tools/include/v32c++ by default), the v32/ C++
# headers -- so <v32/math.hpp> resolves from any directory with no -I --
# and libc/, the C library C input finds there with no -I either. The
# community tools share v32tools/ (v32lua's includes go in
# v32tools/include/v32lua). The same layout `cmake --install` produces
# with its default prefix (see CMakeLists.txt), documents included. May
# need sudo. `make sysuninstall` removes it again.
#
# DEST_LANG=es|fr|en|all (above) picks the documentation language: by default
# everything is installed, `sudo make sysinstall DEST_LANG=es` only Spanish.
#
# The alias pages v32__<name>.3v32 (a file name cannot hold "::" on Windows) are
# installed under their real names, v32::<name>.3v32, so `man 3v32 v32::clamp`
# works; see INSTALL_MAN below.
#
# Manual pages are installed gzip -9'd (page.3v32.gz), mode 0644. An alias
# page (a one-line `.so man3/<page>`) becomes a symbolic link to its
# target's .gz instead -- what Debian's dh_installman does -- since a .so
# inside a compressed page is not followed by every man(1). Permissions are
# set explicitly (umask 022, chmod), never left to the umask sudo runs
# with. Earlier installs' uncompressed pages, and the v32-<name>.hpp page
# names used before the rename to <name>.hpp, are removed on the way.
LIBCDIR := $(INCPATH)/libc
MANROOT := $(PREFIX)/share/man
# The documents (README, docs/, LICENSE): next to the headers, in the
# directory the CMake build also uses (v32tools/docs/v32c++)
DOCDIR  := $(abspath $(INCPATH)/../../docs/v32c++)
# Vircon32/, v32tools/ and include/ above the header directory: each gets
# go+rx (added, never taken away) so that ordinary users can reach the
# headers whatever umask created them.
INCPARENTS := $(addprefix $(DESTDIR),$(patsubst %/,%,$(dir $(INCPATH))) \
              $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(INCPATH))))) \
              $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(INCPATH))))))))

# a newline, to end each expansion of INSTALL_MAN inside a $(foreach)
define NL


endef

# $(call INSTALL_MAN,<pages>,<manual root>): the pages (man/.../manN/page)
# into <manual root>/manN, gzipped; an alias becomes a symlink (see above)
define INSTALL_MAN
	@umask 022; for f in $(1); do \
	    case "$$f" in *.1) sec=man1 ;; *) sec=man3 ;; esac; \
	    install -d "$(2)/$$sec"; chmod go+rx "$(2)" "$(2)/$$sec"; \
	    n=$${f##*/}; case "$$n" in v32__*) n="v32::$${n#v32__}" ;; esac; \
	    dest="$(2)/$$sec/$$n"; \
	    rm -f "$$dest" "$$dest.gz"; \
	    if head -n 1 "$$f" | grep -q '^\.so '; then \
	        target=`sed -n '1s|^\.so [^/]*/||p' "$$f"`; \
	        ln -s "$$target.gz" "$$dest.gz" || exit 1; \
	    else \
	        gzip -9 -n -c "$$f" > "$$dest.gz" || exit 1; \
	        chmod 0644 "$$dest.gz" || exit 1; \
	    fi; \
	done
endef

sysinstall: all
	umask 022; install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANROOT)/man1 $(DESTDIR)$(MANROOT)/man3 $(DESTDIR)$(INCPATH)/v32 $(DESTDIR)$(LIBCDIR)/sys
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/v32c++
	install -m 755 $(BINC) $(DESTDIR)$(BINDIR)/v32c
	install -m 644 v32/*.hpp $(DESTDIR)$(INCPATH)/v32/
	install -m 644 libc/*.h libc/*.c $(DESTDIR)$(LIBCDIR)/
	install -m 644 libc/sys/*.h $(DESTDIR)$(LIBCDIR)/sys/
	chmod -R go+rX $(DESTDIR)$(INCPATH)
	chmod go+rx $(INCPARENTS) $(DESTDIR)$(MANROOT) $(DESTDIR)$(MANROOT)/man1 $(DESTDIR)$(MANROOT)/man3
	rm -f $(DESTDIR)$(MANROOT)/man3/v32-*.hpp.3v32 $(DESTDIR)$(MANROOT)/man3/v32-*.hpp.3v32.gz
	$(call INSTALL_MAN,$(PLAIN_MAN),$(DESTDIR)$(MANROOT))
	$(foreach l,$(LANG_MANS),$(call INSTALL_MAN,$(wildcard man/$(l)/man1/*.1) $(wildcard man/$(l)/man3/*.3v32),$(DESTDIR)$(MANROOT)/$(l))$(NL))
	sh scripts/install_docs.sh $(DEST_LANG) $(DESTDIR)$(DOCDIR)
	chmod -R go+rX $(DESTDIR)$(DOCDIR)
	-@[ -n "$(DESTDIR)" ] || { command -v mandb >/dev/null 2>&1 && mandb -q 2>/dev/null; }; true
	@echo "Installed v32c++ and v32c to $(DESTDIR)$(BINDIR), their manual pages (gzipped; $(DEST_LANG)) to"
	@echo "$(DESTDIR)$(MANROOT)/man1 and man3, the C++ headers to $(DESTDIR)$(INCPATH)/v32,"
	@echo "the C library to $(DESTDIR)$(LIBCDIR) and the documents ($(DEST_LANG)) to $(DESTDIR)$(DOCDIR)"

# Removes exactly what sysinstall put down -- the manual pages as .gz (and
# any uncompressed copy or old v32-<name>.hpp name an earlier install left)
# -- then each directory that leaves empty: v32tools/include, v32tools and
# Vircon32 only if nothing else (v32opt, v32lua, the DevTools) is in them.
sysuninstall:
	rm -f $(DESTDIR)$(BINDIR)/v32c++ $(DESTDIR)$(BINDIR)/v32c
	@for f in $(ALL_MAN); do \
	    case "$$f" in *.1) sec=man1 ;; *) sec=man3 ;; esac; \
	    case "$$f" in man/es/*) root="$(DESTDIR)$(MANROOT)/es"; rootplain="$(DESTDIR)$(MANROOT)" ;; \
	                  man/fr/*) root="$(DESTDIR)$(MANROOT)/fr"; rootplain="$(DESTDIR)$(MANROOT)" ;; \
	                  *) root="$(DESTDIR)$(MANROOT)"; rootplain="" ;; esac; \
	    n=$${f##*/}; m=$$n; case "$$n" in v32__*) m="v32::$${n#v32__}" ;; esac; \
	    for r in $$root $$rootplain; do \
	        rm -f "$$r/$$sec/$$n" "$$r/$$sec/$$n.gz" "$$r/$$sec/$$m" "$$r/$$sec/$$m.gz"; \
	    done; \
	done
	-@for d in $(DESTDIR)$(MANROOT)/es/man1 $(DESTDIR)$(MANROOT)/es/man3 $(DESTDIR)$(MANROOT)/es $(DESTDIR)$(MANROOT)/fr/man1 $(DESTDIR)$(MANROOT)/fr/man3 $(DESTDIR)$(MANROOT)/fr; do \
	    rmdir "$$d" 2>/dev/null && echo "removed empty $$d"; \
	done; true
	sh scripts/install_docs.sh --remove $(DESTDIR)$(DOCDIR)
	rm -f $(addprefix $(DESTDIR)$(INCPATH)/v32/,$(notdir $(wildcard v32/*.hpp)))
	rm -f $(addprefix $(DESTDIR)$(LIBCDIR)/,$(notdir $(wildcard libc/*.h libc/*.c)))
	rm -f $(addprefix $(DESTDIR)$(LIBCDIR)/sys/,$(notdir $(wildcard libc/sys/*.h)))
	-@for d in $(DESTDIR)$(INCPATH)/v32 $(DESTDIR)$(LIBCDIR)/sys $(DESTDIR)$(LIBCDIR) $(DESTDIR)$(INCPATH) $(DESTDIR)$(DOCDIR) $(patsubst %/,%,$(dir $(DESTDIR)$(DOCDIR))) $(INCPARENTS); do \
	    rmdir "$$d" 2>/dev/null && echo "removed empty $$d"; \
	done; true
	-@[ -n "$(DESTDIR)" ] || { command -v mandb >/dev/null 2>&1 && mandb -q 2>/dev/null; }; true

put: clean
	@mkdir -p put
	@rm -f put/*
	@cp inc/*.h src/*.c docs/* v32/* README.md c_api/*.h lib/* libc/* put/
	@cp man/man1/v32c++.1  put/v32c++.1.txt
	@cp src/lexer.l   put/lexer.l.txt
	@cp src/parser.y  put/parser.y.txt

archive: clean
	zip -r ../v32cxx-project-$(ARCHDATE).zip * .github \
	    -x 'tools/vircon32/bin/*' 'tools/vircon32/ComputerSoftware/*' 'build/*' 'out/*'
# out/* too: `clean` only removes the files directly in out/, not the directories a
# `make test` leaves there (out/146, out/147...), and none of it belongs in the archive
# -r matters: without it, `demos/*` stores only the demos/c and demos/cxx
# directory ENTRIES, not the files inside them, so the archive silently
# ships empty demo folders.

# clean also removes the stock toolchain build-tools.sh built in tools/vircon32/bin (make realcheck rebuilds it
# when it is missing), and what older tooling cloned or generated in the tree: build-tools.sh used to clone
# ComputerSoftware into tools/vircon32/ (it is found beside the project now), and a test used to write
# Make.sh/Make.bat into tests/. tools/prune-release.sh removes tracked copies of these.
clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/* $(SRC_DIR)/parser.output $(OUT_DIR)/* put/* counterexamples.txt *.zip *.patch ../v32cxx-project.zip ../v32cxx-project-*.zip
	rm -rf tools/vircon32/bin tools/vircon32/ComputerSoftware tests/Make.sh tests/Make.bat
	$(MAKE) -C demos clean
	#rm -f $(SRC_DIR)/parser.c $(SRC_DIR)/lexer.c $(INC_DIR)/parser.h
# Removing the bison/flex-generated files here (not just objects/binary) is
# deliberate: regenerating them relies on make's mtime comparison against
# parser.y/lexer.l, and if that check ever silently fails to trigger (rare,
# but possible depending on filesystem timestamp granularity or how files
# were copied/touched), `make` will happily relink a STALE parser.c against
# a newer parser.y without complaint -- no error, just the old behavior
# persisting invisibly. Forcing full regeneration on `clean` closes that gap.

# Auto-generated header dependencies (see -MMD -MP on CFLAGS above).
# The leading '-' keeps a fresh checkout (no .d files yet) quiet.
-include $(wildcard $(OBJ_DIR)/*.d)
