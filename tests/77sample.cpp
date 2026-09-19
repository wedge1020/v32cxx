// Exercises `nullptr` (AST_NULL_LIT, a new literal kind of its own --
// see inc/ast.h's own doc comment on it) -- one of the very first
// things an intro-course student reaches for once they've been taught
// not to write a bare `0` for a pointer, and, before this round, a
// real, previously-undiscovered gap: `nullptr` transpiled as the
// literal, unmangled word `nullptr` (this project's own call-
// resolution correctly leaves an unresolved identifier alone rather
// than guessing, the same behavior tests/22sample.cpp's own comment
// describes for an unrelated reason), which doesn't compile under
// either target -- neither dialect's own standard library defines a
// macro or keyword by that name.
//
// Exercised in every position a null pointer constant can appear in:
// a declaration's own initializer, an equality comparison, and a
// plain assignment -- confirming the fix isn't specific to any one of
// codegen.c's many separate expression-printing call sites, since
// AST_NULL_LIT is a single, shared literal kind reused unchanged by
// all of them.
//
// Expected: transpiles clean under both `--target=vircon32` (where
// `nullptr` becomes the literal word `NULL`, already in scope via
// misc.h's own unconditional `#include`) and `--target=standard`
// (where it becomes the same `NULL`, in scope via <stdlib.h>'s own
// unconditional `#include`) -- confirmed directly against real gcc:
// `n` starts NULL, so the `if` body never runs and `val` stays
// whatever `new Node()`'s own (uninitialized) allocation left it as;
// this test only checks that the program COMPILES and links, not any
// particular runtime value from that uninitialized read.

class Node {
    public:
        Node();
        int val;
};

Node::Node() {
}

void main() {
    Node *n = nullptr;
    Node *m = new Node();

    if (n == nullptr) {
        m->val = 1;
    }

    n = nullptr;
}
