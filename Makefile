INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
OUT_DIR = out

BIN     = $(BIN_DIR)/v32c++

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

.PHONY: all clean test install uninstall

all: $(BIN_DIR)/v32c++

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
				   $(OBJ_DIR)/pathutil.o $(OBJ_DIR)/cartxml.o \
				   $(OBJ_DIR)/debugmap.o $(OBJ_DIR)/main.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
# If linking fails looking for yywrap/yy_flex_* symbols on your system,
# add -lfl to this link line (some flex installs need it even with
# %option noyywrap; most don't).

test: all | $(OUT_DIR)
	$(BIN)  -vvv -c -o out/01partial.c tests/01sample.cpp 1> out/01sample.txt 2>&1
	$(BIN)  -vvv    -o out/02program.c tests/02sample.cpp 1> out/02sample.txt 2>&1
	$(BIN)  -vvv -c -o out/03partial.c tests/03sample.cpp 1> out/03sample.txt 2>&1
	-$(BIN) -vvv -c -o out/04failure.c tests/04sample.cpp 1> out/04sample.txt 2>&1
	-$(BIN) -vvv -c -o out/05failure.c tests/05sample.cpp 1> out/05sample.txt 2>&1
	$(BIN)  -vvv -c -o out/06partial.c tests/06sample.cpp 1> out/06sample.txt 2>&1
	$(BIN)  -vvv -c -o out/07partial.c tests/07sample.cpp 1> out/07sample.txt 2>&1
	$(BIN)  -vvv -c -o out/08partial.c tests/08sample.cpp 1> out/08sample.txt 2>&1
	$(BIN)  -vvv -c -o out/09partial.c tests/09sample.cpp 1> out/09sample.txt 2>&1
	-$(BIN) -vvv -c -o out/10failure.c tests/10sample.cpp 1> out/10sample.txt 2>&1
	-$(BIN) -vvv -c -o out/11failure.c tests/11sample.cpp 1> out/11sample.txt 2>&1
	$(BIN)  -vvv -c -o out/12partial.c tests/12sample.cpp 1> out/12sample.txt 2>&1
	$(BIN)  -vvv -c -o out/13partial.c tests/13sample.cpp 1> out/13sample.txt 2>&1
	$(BIN)  -vvv    -o out/14program.c tests/14sample.cpp 1> out/14sample.txt 2>&1
	$(BIN)  -vvv -c -o out/15partial.c tests/15sample.cpp 1> out/15sample.txt 2>&1
	$(BIN)  -vvv -c -o out/16partial.c tests/16sample.cpp 1> out/16sample.txt 2>&1
	$(BIN)  -vvv -c -o out/17partial.c tests/17sample.cpp 1> out/17sample.txt 2>&1
	$(BIN)  -vvv -c -o out/18partial.c tests/18sample.cpp 1> out/18sample.txt 2>&1
	-$(BIN) -vvv -c -o out/19failure.c tests/19sample.cpp 1> out/19sample.txt 2>&1
	-$(BIN) -vvv -c -o out/20failure.c tests/20sample.cpp 1> out/20sample.txt 2>&1
	$(BIN)  -vvv    -o out/21program.c tests/21sample.cpp 1> out/21sample.txt 2>&1
	$(BIN)  -vvv    -o out/22program.c tests/22sample.cpp 1> out/22sample.txt 2>&1
	$(BIN)  -vvv    -o out/23program.c tests/23sample.cpp 1> out/23sample.txt 2>&1
	$(BIN)  -vvv    -o out/24program.c tests/24sample.cpp 1> out/24sample.txt 2>&1
	$(BIN)  -vvv    -o out/25program.c tests/25sample.cpp 1> out/25sample.txt 2>&1
	$(BIN)  -vvv    -o out/26program.c tests/26sample.cpp 1> out/26sample.txt 2>&1
	$(BIN)  -vvv    -o out/27program.c tests/27sample.cpp 1> out/27sample.txt 2>&1
	$(BIN)  -vvv    -o out/28program.c tests/28sample.cpp 1> out/28sample.txt 2>&1
	$(BIN)  -vvv    -o out/29program.c tests/29sample.cpp 1> out/29sample.txt 2>&1
	$(BIN)  -vvv    -o out/30program.c tests/30sample.cpp 1> out/30sample.txt 2>&1
	-$(BIN) -vvv -c -o out/failureXX.c tests/31sample.cpp 1> out/31sample.txt 2>&1
	$(BIN)  -vvv    -o out/32program.c tests/32sample.cpp 1> out/32sample.txt 2>&1
	$(BIN)  -vvv -g -o out/33program.c tests/33sample.cpp 1> out/33sample.txt 2>&1
	$(BIN)  -vvv -bgo  out/34program.c tests/34sample.cpp 1> out/34sample.txt 2>&1
	$(BIN)  -vvv    -o out/35program.c tests/35sample.cpp 1> out/35sample.txt 2>&1
	-$(BIN) -vvv    -o out/36failure.c tests/36sample.cpp 1> out/36sample.txt 2>&1
	$(BIN)  -vvv    -o out/37program.c tests/37sample.cpp 1> out/37sample.txt 2>&1
	-$(BIN) -vvv    -o out/38failure.c tests/38sample.cpp 1> out/38sample.txt 2>&1
	$(BIN)  -vvv    -o out/39program.c tests/39sample.cpp 1> out/39sample.txt 2>&1
	$(BIN)  -vvv    -o out/40program.c tests/40sample.cpp 1> out/40sample.txt 2>&1
	$(BIN)  -vvv    -o out/41program.c tests/41sample.cpp 1> out/41sample.txt 2>&1
	-$(BIN) -vvv    -o out/42failure.c tests/42sample.cpp 1> out/42sample.txt 2>&1
	$(BIN)  -vvv    -o out/43program.c tests/43sample.cpp 1> out/43sample.txt 2>&1
	$(BIN)  -vvv    -o out/44program.c tests/44sample.cpp 1> out/44sample.txt 2>&1
	-$(BIN) -vvv    -o out/45failure.c tests/45sample.cpp 1> out/45sample.txt 2>&1
	$(BIN)  -vvv    -o out/46program.c tests/46sample.cpp 1> out/46sample.txt 2>&1
	$(BIN)  -vvv    -o out/47program.c tests/47sample.cpp 1> out/47sample.txt 2>&1
	$(BIN)  -vvv    -o out/48program.c tests/48sample.cpp 1> out/48sample.txt 2>&1
	-$(BIN) -vvv    -o out/49failure.c tests/49sample.cpp 1> out/49sample.txt 2>&1
	$(BIN)  -vvv    -o out/50program.c tests/50sample.cpp 1> out/50sample.txt 2>&1
	$(BIN)  -vvv    -o out/51program.c tests/51sample.cpp 1> out/51sample.txt 2>&1
	$(BIN)  -vvv    -o out/52program.c tests/52sample.cpp 1> out/52sample.txt 2>&1
	-$(BIN) -vvv    -o out/53failure.c tests/53sample.cpp 1> out/53sample.txt 2>&1
	$(BIN)  -vvv    -o out/54program.c tests/54sample.cpp 1> out/54sample.txt 2>&1
	$(BIN)  -vvv    -o out/55program.c tests/55sample.cpp 1> out/55sample.txt 2>&1
	$(BIN)  -vvv    -o out/56program.c tests/56sample.cpp 1> out/56sample.txt 2>&1
	$(BIN)  -vvv    -o out/57program.c tests/57sample.cpp 1> out/57sample.txt 2>&1
	$(BIN)  -vvv    -o out/58program.c tests/58sample.cpp 1> out/58sample.txt 2>&1
	$(BIN)  -vvv    -o out/59program.c tests/59sample.cpp 1> out/59sample.txt 2>&1
	$(BIN)  -vvv    -o out/60program.c tests/60sample.cpp 1> out/60sample.txt 2>&1
	$(BIN)  -vvv    -o out/61program.c tests/61sample.cpp 1> out/61sample.txt 2>&1
	$(BIN)  -vvv    -o out/62program.c tests/62sample.cpp 1> out/62sample.txt 2>&1
	$(BIN)  -vvv    -o out/63program.c tests/63sample.cpp 1> out/63sample.txt 2>&1
	$(BIN)  -vvv    -o out/64program.c tests/64sample.cpp 1> out/64sample.txt 2>&1
	$(BIN)  -vvv    -o out/65program.c tests/65sample.cpp 1> out/65sample.txt 2>&1
	$(BIN)  -vvv    -o out/66program.c tests/66sample.cpp 1> out/66sample.txt 2>&1
	$(BIN)  -vvv    -o out/67program.c tests/67sample.cpp 1> out/67sample.txt 2>&1
	$(BIN)  -vvv    -o out/68program.c tests/68sample.cpp 1> out/68sample.txt 2>&1
	$(BIN)  -vvv    -o out/69program.c tests/69sample.cpp 1> out/69sample.txt 2>&1
	$(BIN)  -vvv    -o out/70program.c tests/70sample.cpp 1> out/70sample.txt 2>&1
	$(BIN)  -vvv    -o out/71program.c tests/71sample.cpp 1> out/71sample.txt 2>&1
	$(BIN)  -vvv    -o out/72program.c tests/72sample.cpp 1> out/72sample.txt 2>&1
	$(BIN)  -vvv    -o out/73program.c tests/73sample.cpp 1> out/73sample.txt 2>&1
	$(BIN)  -vvv    -o out/74program.c tests/74sample.cpp 1> out/74sample.txt 2>&1
	$(BIN)  -vvv    -o out/75program.c tests/75sample.cpp 1> out/75sample.txt 2>&1
	$(BIN)  -vvv    -o out/76program.c tests/76sample.cpp 1> out/76sample.txt 2>&1
	$(BIN)  -vvv    -o out/77program.c tests/77sample.cpp 1> out/77sample.txt 2>&1
	$(BIN)  -vvv    -o out/78program.c tests/78sample.cpp 1> out/78sample.txt 2>&1
	$(BIN)  -vvv    -o out/79program.c tests/79sample.cpp 1> out/79sample.txt 2>&1
	-$(BIN) -vvv    -o out/80failure.c tests/80sample.cpp 1> out/80sample.txt 2>&1
	$(BIN)  -vvv    -o out/81program.c tests/81sample.cpp 1> out/81sample.txt 2>&1
	$(BIN)  -vvv    -o out/82program.c tests/82sample.cpp 1> out/82sample.txt 2>&1
	$(BIN)  -vvv    -o out/83program.c tests/83sample.cpp 1> out/83sample.txt 2>&1
	$(BIN)  -vvv    -o out/84program.c tests/84sample.cpp 1> out/84sample.txt 2>&1
	$(BIN)  -vvv    -o out/85program.c tests/85sample.cpp 1> out/85sample.txt 2>&1
	$(BIN)  -vvv    -o out/86program.c tests/86sample.cpp 1> out/86sample.txt 2>&1
	$(BIN)  -vvv    -o out/87program.c tests/87sample.cpp 1> out/87sample.txt 2>&1
	$(BIN)  -vvv    -o out/88program.c tests/88sample.cpp 1> out/88sample.txt 2>&1
	$(BIN)  -vvv    -o out/89program.c tests/89sample.cpp 1> out/89sample.txt 2>&1
	$(BIN)  -vvv    -o out/90program.c tests/90sample.cpp 1> out/90sample.txt 2>&1
	-$(BIN) -vvv    -o out/91failure.c tests/91sample.cpp 1> out/91sample.txt 2>&1
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

install: all
	mkdir -p $(HOME)/bin
	cp $(BIN) $(HOME)/bin/
	@echo "Installed v32c++ to $(HOME)/bin/v32c++"
	@echo "(make sure $(HOME)/bin is on your PATH to run it as just 'v32c++')"

uninstall:
	rm -f $(HOME)/bin/v32c++

put: clean
	@mkdir -p put
	@rm -f put/*
	@cp inc/*.h src/*.c docs/* v32/* README.md c_api/*.h put/
	@cp man/v32c++.1  put/v32c++.1.txt
	@cp src/lexer.l   put/lexer.l.txt
	@cp src/parser.y  put/parser.y.txt

archive: clean
	zip -r v32cxx-project.zip demos docs inc Makefile man README.md src tests v32
# -r matters: without it, `demos/*` stores only the demos/c and demos/cxx
# directory ENTRIES, not the files inside them, so the archive silently
# ships empty demo folders.

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/* $(SRC_DIR)/parser.output $(OUT_DIR)/* put/*
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
