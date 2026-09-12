INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

CC      = gcc
CFLAGS  = -Wall -Wextra -g -I$(INC_DIR)
BISON   = bison
FLEX    = flex

.PHONY: all clean test

all: $(BIN_DIR)/v32c++

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

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
                          $(OBJ_DIR)/ast.o $(OBJ_DIR)/symtab.o $(OBJ_DIR)/sema.o $(OBJ_DIR)/main.o
	$(CC) $(CFLAGS) -o $@ $^
# If linking fails looking for yywrap/yy_flex_* symbols on your system,
# add -lfl to this link line (some flex installs need it even with
# %option noyywrap; most don't).

test: all
	./$(BIN_DIR)/v32c++ tests/sample1.cpp
	./$(BIN_DIR)/v32c++ tests/sample2.cpp
	./$(BIN_DIR)/v32c++ tests/sample3.cpp
	-./$(BIN_DIR)/v32c++ tests/sample4.cpp
	-./$(BIN_DIR)/v32c++ tests/sample5.cpp
	./$(BIN_DIR)/v32c++ tests/sample6.cpp
	./$(BIN_DIR)/v32c++ tests/sample7.cpp
	./$(BIN_DIR)/v32c++ tests/sample8.cpp
	./$(BIN_DIR)/v32c++ tests/sample9.cpp
	-./$(BIN_DIR)/v32c++ tests/sample10.cpp
# sample4/sample5 are deliberately-invalid inputs (see their own header
# comments) -- they're SUPPOSED to return nonzero. The leading '-' tells
# make to ignore their exit code and keep going, rather than aborting the
# whole `test` run at the first "expected" failure the way it did before
# (sample4 failing used to silently prevent sample5 from ever running).
# sample1/2/3 are deliberately NOT prefixed with '-': if any of those
# start failing, that's a real regression and `make test` should stop and
# report it, not paper over it the same way.

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/* $(SRC_DIR)/parser.output
	rm -f $(SRC_DIR)/parser.c $(SRC_DIR)/lexer.c $(INC_DIR)/parser.h
# Removing the bison/flex-generated files here (not just objects/binary) is
# deliberate: regenerating them relies on make's mtime comparison against
# parser.y/lexer.l, and if that check ever silently fails to trigger (rare,
# but possible depending on filesystem timestamp granularity or how files
# were copied/touched), `make` will happily relink a STALE parser.c against
# a newer parser.y without complaint -- no error, just the old behavior
# persisting invisibly. Forcing full regeneration on `clean` closes that gap.
