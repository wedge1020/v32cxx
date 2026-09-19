// Exercises array declaration + subscript use, in BOTH accepted C++-side
// declarator forms (standard-C length-after-name, and Vircon32-native-
// style length-before-name, offered as an alternate input spelling --
// see parser.y's var_decl). Both should produce IDENTICAL Vircon32
// output regardless of which form the source used -- the AST itself
// (AST_ARRAY_TYPE) carries no memory of which spelling was written.
//
// Deliberately scoped to what this round actually built: local-variable
// arrays, class data-member arrays, and ordinary subscript read/write.
// NOT covered here, and not yet supported at all: function parameters
// of array type (decay-to-pointer is separate, unbuilt work) and array
// initializer lists (`= {1, 2, 3}`, also unbuilt).

class Scoreboard {
    public:
        Scoreboard();
        void setScore(int index, int value);
        int getScore(int index);
    private:
        int scores[4];
};

Scoreboard::Scoreboard() {
    scores[0] = 0;
    scores[1] = 0;
    scores[2] = 0;
    scores[3] = 0;
}

void Scoreboard::setScore(int index, int value) {
    scores[index] = value;
}

int Scoreboard::getScore(int index) {
    return scores[index];
}

void main() {
    int totals[4];   // standard-C style: length after the name
    int [4] backups; // Vircon32-style: length before the name (alternate input form)

    totals[0] = 10;
    backups[0] = totals[0];

    Scoreboard board;
    board.setScore(0, 5);
    board.getScore(0);
}
