// XML paths (not self-checking: its assets aren't shipped). Cart hints: assets in subdirectories, two with the same file name (the second is renamed extra/ship.vtex).
#title "Paths"
#version 0.1
#texture SHIP "textures/ship.png"
#texture BULLET "textures/extra/ship.png"
#sound BOOM "sounds/boom.wav"

int main ()
{
    return SHIP + BULLET + BOOM;
}
