// Exercises `enum` -- top-level declarations, auto-incrementing values
// (starting at 0), explicit values with auto-increment resuming from
// them, and using an enum name as an ordinary variable/parameter type
// with an enumerator as an ordinary value. This project never computes
// an omitted enumerator's own value itself -- it's passed straight
// through as literal C enum syntax (see AST_ENUM_DECL's own doc
// comment in ast.h), so MID's value (11) below is resolved by the
// downstream C compiler, the same as real C++ would resolve it, not by
// anything in this project's own pipeline.
//
// Expected: colorValue = 1 (GREEN, the second auto-incremented value,
// 0-based); lvl assigned MID, whose own value (11) is never computed
// here at all -- confirmed by reading the generated C directly, not by
// anything this test can check from inside itself.

enum Color {
    RED,
    GREEN,
    BLUE
};

enum Level {
    LOW = 10,
    MID,
    HIGH = 20
};

int getColorValue(Color c) {
    return c;
}

void main() {
    Color currentColor = GREEN;
    int colorValue = getColorValue(currentColor);
    Level lvl = MID;
}
