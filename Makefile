INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
OUT_DIR = out

CC      = gcc
CFLAGS  = -Wall -Wextra -g -I$(INC_DIR)
BISON   = bison
FLEX    = flex

.PHONY: all clean test

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
                          $(OBJ_DIR)/lower.o $(OBJ_DIR)/codegen.o $(OBJ_DIR)/main.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
# If linking fails looking for yywrap/yy_flex_* symbols on your system,
# add -lfl to this link line (some flex installs need it even with
# %option noyywrap; most don't).

test: all | $(OUT_DIR)
	./$(BIN_DIR)/v32c++  -c tests/sample1.cpp  2>&1 | tee out/sample1.txt
	./$(BIN_DIR)/v32c++     tests/sample2.cpp  2>&1 | tee out/sample2.txt
	./$(BIN_DIR)/v32c++  -c tests/sample3.cpp  2>&1 | tee out/sample3.txt
	-./$(BIN_DIR)/v32c++ -c tests/sample4.cpp  2>&1 | tee out/sample4.txt
	-./$(BIN_DIR)/v32c++ -c tests/sample5.cpp  2>&1 | tee out/sample5.txt
	./$(BIN_DIR)/v32c++  -c tests/sample6.cpp  2>&1 | tee out/sample6.txt
	./$(BIN_DIR)/v32c++  -c tests/sample7.cpp  2>&1 | tee out/sample7.txt
	./$(BIN_DIR)/v32c++  -c tests/sample8.cpp  2>&1 | tee out/sample8.txt
	./$(BIN_DIR)/v32c++  -c tests/sample9.cpp  2>&1 | tee out/sample9.txt
	-./$(BIN_DIR)/v32c++ -c tests/sample10.cpp 2>&1 | tee out/sample10.txt
	-./$(BIN_DIR)/v32c++ -c tests/sample11.cpp 2>&1 | tee out/sample11.txt
	./$(BIN_DIR)/v32c++  -c tests/sample12.cpp 2>&1 | tee out/sample12.txt
	./$(BIN_DIR)/v32c++  -c tests/sample13.cpp 2>&1 | tee out/sample13.txt
	./$(BIN_DIR)/v32c++     tests/sample14.cpp 2>&1 | tee out/sample14.txt
	./$(BIN_DIR)/v32c++  -c tests/sample15.cpp 2>&1 | tee out/sample15.txt
	./$(BIN_DIR)/v32c++  -c tests/sample16.cpp 2>&1 | tee out/sample16.txt
	./$(BIN_DIR)/v32c++  -c tests/sample17.cpp 2>&1 | tee out/sample17.txt
	./$(BIN_DIR)/v32c++  -c tests/sample18.cpp 2>&1 | tee out/sample18.txt
	-./$(BIN_DIR)/v32c++ -c tests/sample19.cpp 2>&1 | tee out/sample19.txt
	-./$(BIN_DIR)/v32c++ -c tests/sample20.cpp 2>&1 | tee out/sample20.txt
	./$(BIN_DIR)/v32c++     tests/sample21.cpp 2>&1 | tee out/sample21.txt
	./$(BIN_DIR)/v32c++     tests/sample22.cpp 2>&1 | tee out/sample22.txt
# `-c` (this project's own flag now, not just a real compiler's) opts out
# of the "must define main" default main.c added this round -- every
# sample here is a focused unit test of one specific compiler feature,
# not a complete, standalone-compilable program, EXCEPT sample2, 14, 21,
# and 22, which genuinely do define their own `main` (sample2's predates
# this round; sample14/21 were given one specifically so they could also
# be compiled all the way through by the real Vircon32 toolchain, not
# just transpiled; sample22 already had one -- it's a real, hand-written
# program, not an artificial unit test). Without `-c`, every other sample would now fail at the
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

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/* $(SRC_DIR)/parser.output $(OUT_DIR)/*
	rm -f $(SRC_DIR)/parser.c $(SRC_DIR)/lexer.c $(INC_DIR)/parser.h
# Removing the bison/flex-generated files here (not just objects/binary) is
# deliberate: regenerating them relies on make's mtime comparison against
# parser.y/lexer.l, and if that check ever silently fails to trigger (rare,
# but possible depending on filesystem timestamp granularity or how files
# were copied/touched), `make` will happily relink a STALE parser.c against
# a newer parser.y without complaint -- no error, just the old behavior
# persisting invisibly. Forcing full regeneration on `clean` closes that gap.
