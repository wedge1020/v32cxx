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

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/* $(SRC_DIR)/parser.output
