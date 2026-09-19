// Deliberately invalid: "Frobnicator" is never declared as a class or
// typedef, so the lexer classifies it as a plain IDENTIFIER rather than
// TYPE_NAME, and "IDENTIFIER IDENTIFIER ;" doesn't match any top_decl
// alternative. Expect a clean bison syntax error and a non-zero exit code
// -- NOT a crash, and not a silent misparse.

Frobnicator widget;
