// Deliberately invalid: the same out-of-line method is defined twice (the
// way it happens when a .cpp is pasted into its own header, or both are
// #included into one translation unit).  v32c++ must say "redefinition"
// and point at the first definition -- not "no matching declaration", which
// sends the reader looking for a signature mismatch that is not there.
class Game
{
    public:
        Game ();
        void init ();
        void run (int times);
};

Game :: Game () { }
void Game :: init () { }
void Game :: run (int times) { }

void Game :: init () { }          // second definition of Game::init
void Game :: run (int times) { }  // second definition of Game::run
void Game :: run (char c) { }     // genuinely no such declaration

int main ()
{
    return 0;
}
