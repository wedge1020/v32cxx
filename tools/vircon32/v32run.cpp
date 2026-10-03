// Headless Vircon32 runner for v32c++ testing.
// usage: v32run <bios.v32> <cart.v32> <memcard.v32|-> <ram_word_address> [max_frames]
// Runs until the CPU halts (hlt) or max_frames elapse, then prints the RAM
// word at the given address. Video/audio output is discarded.
#include "V32Console.hpp"
#include "ExternalInterfaces.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
using namespace V32;

static void NoColor( GPUColor ) {}
static void NoQuad( GPUQuad& ) {}
static void NoInt( int ) {}
static void NoLoad( int, void* ) {}
static void NoArgs() {}
static void Log( const std::string& s ) { fprintf( stderr, "[console] %s\n", s.c_str() ); }
static void Throw( const std::string& s ) { throw std::runtime_error( s ); }

int main( int argc, char** argv )
{
    if( argc < 5 ) { fprintf( stderr, "usage: v32run bios cart memcard|- address [frames]\n" ); return 2; }
    Callbacks::ClearScreen = NoColor;
    Callbacks::DrawQuad = NoQuad;
    Callbacks::SetMultiplyColor = NoColor;
    Callbacks::SetBlendingMode = NoInt;
    Callbacks::SelectTexture = NoInt;
    Callbacks::LoadTexture = NoLoad;
    Callbacks::UnloadCartridgeTextures = NoArgs;
    Callbacks::UnloadBiosTexture = NoArgs;
    Callbacks::LogLine = Log;
    Callbacks::ThrowException = Throw;

    int address = (int)strtol( argv[4], nullptr, 0 );
    int max_frames = ( argc > 5 ) ? atoi( argv[5] ) : 600;
    try
    {
        V32Console console;
        console.LoadBios( argv[1] );
        console.LoadCartridge( argv[2] );
        if( std::string( argv[3] ) != "-" )
        {
            FILE* existing = fopen( argv[3], "rb" );
            if( existing ) fclose( existing );
            else console.CreateMemoryCard( argv[3] );   // blank card, file only
            console.LoadMemoryCard( argv[3] );          // ...and insert it
        }
        console.SetGamepadConnection( 0, true );
        console.SetPower( true );
        int frame = 0;
        for( ; frame < max_frames && !console.IsCPUHalted(); frame++ )
          console.RunNextFrame();
        if( console.HasMemoryCard() ) console.SaveMemoryCard();
        printf( "frames=%d halted=%d ram[%d]=%d\n", frame, (int)console.IsCPUHalted(),
                address, console.RAM.Memory[ address ].AsInteger );
        return console.IsCPUHalted() ? 0 : 1;
    }
    catch( std::exception& e ) { fprintf( stderr, "v32run: %s\n", e.what() ); return 3; }
}
