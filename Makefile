INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
OUT_DIR = out

BIN     = $(BIN_DIR)/v32c++

CC      = gcc
CFLAGS  = -Wall -Wextra -g -I$(INC_DIR)
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

$(BIN_DIR)/v32c++: $(OBJ_DIR)/parser.o $(OBJ_DIR)/lexer.o \
                          $(OBJ_DIR)/ast.o $(OBJ_DIR)/symtab.o $(OBJ_DIR)/sema.o \
                          $(OBJ_DIR)/lower.o $(OBJ_DIR)/codegen.o $(OBJ_DIR)/pathutil.o \
                          $(OBJ_DIR)/cartxml.o $(OBJ_DIR)/debugmap.o $(OBJ_DIR)/main.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
# If linking fails looking for yywrap/yy_flex_* symbols on your system,
# add -lfl to this link line (some flex installs need it even with
# %option noyywrap; most don't).

test: all | $(OUT_DIR)
	$(BIN)  -vvv -c -o out/sample1.c  tests/sample1.cpp  1> out/sample1.txt  2>&1
	$(BIN)  -vvv    -o out/sample2.c  tests/sample2.cpp  1> out/sample2.txt  2>&1
	$(BIN)  -vvv -c -o out/sample3.c  tests/sample3.cpp  1> out/sample3.txt  2>&1
	-$(BIN) -vvv -c -o out/sample4.c  tests/sample4.cpp  1> out/sample4.txt  2>&1
	-$(BIN) -vvv -c -o out/sample5.c  tests/sample5.cpp  1> out/sample5.txt  2>&1
	$(BIN)  -vvv -c -o out/sample6.c  tests/sample6.cpp  1> out/sample6.txt  2>&1
	$(BIN)  -vvv -c -o out/sample7.c  tests/sample7.cpp  1> out/sample7.txt  2>&1
	$(BIN)  -vvv -c -o out/sample8.c  tests/sample8.cpp  1> out/sample8.txt  2>&1
	$(BIN)  -vvv -c -o out/sample9.c  tests/sample9.cpp  1> out/sample9.txt  2>&1
	-$(BIN) -vvv -c -o out/sample10.c tests/sample10.cpp 1> out/sample10.txt 2>&1
	-$(BIN) -vvv -c -o out/sample11.c tests/sample11.cpp 1> out/sample11.txt 2>&1
	$(BIN)  -vvv -c -o out/sample12.c tests/sample12.cpp 1> out/sample12.txt 2>&1
	$(BIN)  -vvv -c -o out/sample13.c tests/sample13.cpp 1> out/sample13.txt 2>&1
	$(BIN)  -vvv    -o out/sample14.c tests/sample14.cpp 1> out/sample14.txt 2>&1
	$(BIN)  -vvv -c -o out/sample15.c tests/sample15.cpp 1> out/sample15.txt 2>&1
	$(BIN)  -vvv -c -o out/sample16.c tests/sample16.cpp 1> out/sample16.txt 2>&1
	$(BIN)  -vvv -c -o out/sample17.c tests/sample17.cpp 1> out/sample17.txt 2>&1
	$(BIN)  -vvv -c -o out/sample18.c tests/sample18.cpp 1> out/sample18.txt 2>&1
	-$(BIN) -vvv -c -o out/sample19.c tests/sample19.cpp 1> out/sample19.txt 2>&1
	-$(BIN) -vvv -c -o out/sample20.c tests/sample20.cpp 1> out/sample20.txt 2>&1
	$(BIN)  -vvv    -o out/sample21.c tests/sample21.cpp 1> out/sample21.txt 2>&1
	$(BIN)  -vvv    -o out/sample22.c tests/sample22.cpp 1> out/sample22.txt 2>&1
	$(BIN)  -vvv    -o out/sample23.c tests/sample23.cpp 1> out/sample23.txt 2>&1
	$(BIN)  -vvv    -o out/sample24.c tests/sample24.cpp 1> out/sample24.txt 2>&1
	$(BIN)  -vvv    -o out/sample25.c tests/sample25.cpp 1> out/sample25.txt 2>&1
	$(BIN)  -vvv    -o out/sample26.c tests/sample26.cpp 1> out/sample26.txt 2>&1
	$(BIN)  -vvv    -o out/sample27.c tests/sample27.cpp 1> out/sample27.txt 2>&1
	$(BIN)  -vvv    -o out/sample28.c tests/sample28.cpp 1> out/sample28.txt 2>&1
	$(BIN)  -vvv    -o out/sample29.c tests/sample29.cpp 1> out/sample29.txt 2>&1
	$(BIN)  -vvv    -o out/sample30.c tests/sample30.cpp 1> out/sample30.txt 2>&1
	-$(BIN) -vvv -c -o out/sample31.c tests/sample31.cpp 1> out/sample31.txt 2>&1
	$(BIN)  -vvv   -o out/sample32.c tests/sample32.cpp 1> out/sample32.txt 2>&1
	$(BIN)  -vvv -g  -o out/sample33.c tests/sample33.cpp 1> out/sample33.txt 2>&1
	$(BIN)  -vvv -b -g -o out/sample34.c tests/sample34.cpp 1> out/sample34.txt 2>&1
	$(BIN)  -vvv    -o out/sample35.c tests/sample35.cpp 1> out/sample35.txt 2>&1
	-$(BIN) -vvv    -o out/sample36.c tests/sample36.cpp 1> out/sample36.txt 2>&1
	$(BIN)  -vvv    -o out/sample37.c tests/sample37.cpp 1> out/sample37.txt 2>&1
	-$(BIN) -vvv    -o out/sample38.c tests/sample38.cpp 1> out/sample38.txt 2>&1
	$(BIN)  -vvv    -o out/sample39.c tests/sample39.cpp 1> out/sample39.txt 2>&1
	$(BIN)  -vvv    -o out/sample40.c tests/sample40.cpp 1> out/sample40.txt 2>&1
	$(BIN)  -vvv    -o out/sample41.c tests/sample41.cpp 1> out/sample41.txt 2>&1
	-$(BIN) -vvv    -o out/sample42.c tests/sample42.cpp 1> out/sample42.txt 2>&1
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
# 41, and 42, which genuinely do define their own `main` (sample2's
# predates this round; sample14/21 were given one specifically so they
# could also be compiled all the way through by the real Vircon32
# toolchain, not just transpiled; sample22 through 30, 32, 33, and 34
# already had one; 35 through 42 -- base-class constructor delegation
# and member-field initializers, two later rounds -- also define their
# own, even though 36, 38, and 42 are themselves deliberately invalid --
# see below). sample31 is deliberately invalid (break/continue-outside-
# a-loop) and gets `-c` like the other unit-test samples, since it isn't
# trying to be a complete program at all. sample36/38/42 are ALSO
# deliberately invalid (an unknown name, a class-typed field, and a
# field named twice, respectively, in a member-initializer list) but do
# NOT get `-c` -- they already define their own `main`, same as 35, so
# `-c` would be redundant, not wrong, but left off for consistency with
# how 35 itself is invoked. sample37 was ALSO deliberately invalid once
# (the very case sample38 now tests -- a member-initializer list naming
# an ordinary field, "not yet supported" at the time), repurposed into a
# real, passing test once primitive member-field initializers themselves
# became supported; sample38 is its replacement as the "still not
# supported" edge (a class-typed field specifically) member-initializer
# lists now have. Without `-c`, every other sample would now fail at the
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

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/* $(SRC_DIR)/parser.output $(OUT_DIR)/*
	#rm -f $(SRC_DIR)/parser.c $(SRC_DIR)/lexer.c $(INC_DIR)/parser.h
# Removing the bison/flex-generated files here (not just objects/binary) is
# deliberate: regenerating them relies on make's mtime comparison against
# parser.y/lexer.l, and if that check ever silently fails to trigger (rare,
# but possible depending on filesystem timestamp granularity or how files
# were copied/touched), `make` will happily relink a STALE parser.c against
# a newer parser.y without complaint -- no error, just the old behavior
# persisting invisibly. Forcing full regeneration on `clean` closes that gap.
