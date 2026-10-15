// *****************************************************************************
//  tests/132sample.cpp — string literals holding control characters
//
//  The Vircon32 C compiler writes a string literal's control characters
//  (codes 1-31 except \t \n \r, and 127) raw into its assembly, and the
//  assembler rejects them. v32c++ moves such a literal into a hidden global
//  char array and uses its address (lower.c, phase 12b), so ordinary C++
//  like "\031\025\030" (the BIOS font's box drawing glyphs, codes 25 21 24)
//  builds and means what it says. This program must reach the real
//  assembler (check.sh) and then see the right codes at run time.
//
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

char global_border[] = "\031\025\030";     // an array initialized from one
int  global_count    = 0;

class Frame
{
    public:
        char* corners()
        {
            return "\031\030";             // a literal in a method (and a repeat)
        }
};

// do the first characters of text have these codes? (-1: past the end of the
// string, not looked at)
int is_codes( char* text, int a, int b, int c, int d )
{
    int ok = 1;
    if( text[ 0 ] != a ) ok = 0;
    if( b >= 0 && text[ 1 ] != b ) ok = 0;
    if( c >= 0 && text[ 2 ] != c ) ok = 0;
    if( d >= 0 && text[ 3 ] != d ) ok = 0;
    return ok;
}

int main()
{
    int   e = 0;
    Frame frame;
    char  local[] = "\020\021ab";
    char* plain   = "tab\there";
    char* mixed   = "ab\001cd\177e";
    char* hexed   = "\x15\x14";
    char* chosen  = global_count > 0 ? "\031" : "\024";

    // the same literal twice, as an argument and as a value
    if( !is_codes( "\031\025\030", 25, 21, 24, 0 ) ) e++;
    char* again = "\031\025\030";
    if( !is_codes( again, 25, 21, 24, 0 ) ) e++;

    // initialized arrays, global and local
    if( !is_codes( global_border, 25, 21, 24, 0 ) ) e++;
    if( !is_codes( local, 16, 17, 'a', 'b' ) ) e++;
    if( local[ 4 ] != 0 ) e++;

    // control characters in the middle of ordinary text, DEL, hex escapes
    if( mixed[ 0 ] != 'a' || mixed[ 2 ] != 1 || mixed[ 3 ] != 'c' ) e++;
    if( mixed[ 5 ] != 127 || mixed[ 6 ] != 'e' || mixed[ 7 ] != 0 ) e++;
    if( !is_codes( hexed, 21, 20, 0, -1 ) ) e++;

    // in a method, in a conditional
    if( !is_codes( frame.corners(), 25, 24, 0, -1 ) ) e++;
    if( !is_codes( chosen, 20, 0, -1, -1 ) ) e++;

    // what the assembler already carried stays as it was: tab, a high code
    if( plain[ 3 ] != 9 || plain[ 4 ] != 'h' ) e++;
    if( !is_codes( "caf\351", 'c', 'a', 'f', 233 ) ) e++;

    test_errors = e;
    return 0;
}
