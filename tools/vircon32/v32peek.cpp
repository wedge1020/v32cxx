// *****************************************************************************
//  tools/vircon32/v32peek.cpp — look inside a running Vircon32 cartridge
//
//  Plays a cartridge on the emulator's own ConsoleLogic, driven by the same
//  input script v32prof and v32shot take, up to a given frame, then prints
//  the CPU's state and the RAM words asked for.
//
//  usage: v32peek bios.v32 cart.v32 script frame address count [address count...]
//
//  For the bugs a screenshot cannot explain. The address of a global is in
//  the assembly the C compiler writes: `%define global_<name> <address>`.
//
//  When a cartridge dies with a BIOS error screen (v32shot shows it, with
//  the instruction pointer), `assemble -g program` writes <cart>.vbin.debug,
//  which maps that address back to an assembly line and a function. The
//  BIOS resets the stack pointer on its way to that screen, but what was on
//  the stack is still in RAM just below its top (address 4194303): peek at
//  a few hundred words there and pick out the values in the cartridge's
//  code range (0x20000000 and up) -- those are the return addresses, i.e.
//  who called whom.
//
//  V32PEEK_CARD=file puts a memory card in the slot (created blank if the
//  file does not exist, written back at the end), as V32SHOT_CARD does.
//
//  Built by build-tools.sh next to v32run, v32prof and v32shot.
// *****************************************************************************
#include "V32Console.hpp"
#include "ExternalInterfaces.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace V32;

static void NoColor( GPUColor ) {}
static void NoQuad( GPUQuad& ) {}
static void NoInt( int ) {}
static void NoLoad( int, void* ) {}
static void NoArgs() {}
static void Log( const std::string& ) {}
static void Throw( const std::string& s ) { throw std::runtime_error( s ); }

struct Command { int frame; std::string op; int port; std::string arg; };

static bool control_from_name( const std::string& n, GamepadControls& c )
{
    static const std::map<std::string, GamepadControls> names = {
        { "left", GamepadControls::Left }, { "right", GamepadControls::Right },
        { "up", GamepadControls::Up }, { "down", GamepadControls::Down },
        { "start", GamepadControls::ButtonStart }, { "a", GamepadControls::ButtonA },
        { "b", GamepadControls::ButtonB }, { "x", GamepadControls::ButtonX },
        { "y", GamepadControls::ButtonY }, { "l", GamepadControls::ButtonL },
        { "r", GamepadControls::ButtonR } };
    auto it = names.find( n );
    if( it == names.end() ) return false;
    c = it->second;
    return true;
}

int main( int argc, char** argv )
{
    if( argc < 7 || ( argc - 5 ) % 2 != 0 )
    {
        fprintf( stderr, "usage: v32peek bios.v32 cart.v32 script frame address count [address count...]\n" );
        return 2;
    }
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

    std::vector<Command> script;
    {
        std::ifstream in( argv[ 3 ] );
        std::string line;
        while( std::getline( in, line ) )
        {
            size_t hash = line.find( '#' );
            if( hash != std::string::npos ) line.erase( hash );
            std::istringstream ss( line );
            Command c{ 0, "", 0, "" };
            if( !( ss >> c.frame >> c.op ) ) continue;
            if( c.op == "segment" ) continue;
            if( c.op != "end" ) ss >> c.port >> c.arg;
            script.push_back( c );
        }
    }
    int last_frame = atoi( argv[ 4 ] );

    std::vector<std::pair<int, GamepadControls>> pending_release;
    try
    {
        V32Console console;
        console.LoadBios( argv[ 1 ] );
        console.LoadCartridge( argv[ 2 ] );
        if( getenv( "V32PEEK_CARD" ) )
        {
            const char* card = getenv( "V32PEEK_CARD" );
            FILE* existing = fopen( card, "rb" );
            if( existing ) fclose( existing );
            else console.CreateMemoryCard( card );
            console.LoadMemoryCard( card );
        }
        console.SetPower( true );
        size_t next = 0;
        int frame = 0;
        for( ; frame <= last_frame && !console.IsCPUHalted(); frame++ )
        {
            for( auto& r : pending_release ) console.SetGamepadControl( r.first, r.second, false );
            pending_release.clear();
            for( ; next < script.size() && script[ next ].frame == frame; next++ )
            {
                const Command& c = script[ next ];
                GamepadControls ctl;
                if( c.op == "connect" ) console.SetGamepadConnection( c.port, c.arg == "1" );
                else if( control_from_name( c.arg, ctl ) )
                {
                    if( c.op == "press" || c.op == "tap" ) console.SetGamepadControl( c.port, ctl, true );
                    if( c.op == "release" ) console.SetGamepadControl( c.port, ctl, false );
                    if( c.op == "tap" ) pending_release.push_back( { c.port, ctl } );
                }
            }
            console.RunNextFrame();
        }
        if( console.HasMemoryCard() ) console.SaveMemoryCard();

        printf( "frames=%d %s ip=0x%08X sp=%d bp=%d\n", frame,
                console.IsCPUHalted() ? "halted" : "running",
                (unsigned)console.CPU.InstructionPointer.AsBinary,
                console.CPU.StackPointer.AsInteger, console.CPU.BasePointer.AsInteger );
        for( int i = 5; i + 1 < argc; i += 2 )
        {
            int address = (int)strtol( argv[ i ], nullptr, 0 ), count = atoi( argv[ i + 1 ] );
            if( address < 0 || count < 1 || (size_t)address + (size_t)count > console.RAM.Memory.size() )
            {
                fprintf( stderr, "v32peek: %d..%d is outside RAM\n", address, address + count - 1 );
                return 2;
            }
            printf( "ram[%d..%d]:", address, address + count - 1 );
            for( int k = 0; k < count; k++ )
            {
                if( k % 8 == 0 ) printf( "\n  %8d:", address + k );
                printf( " %d", console.RAM.Memory[ address + k ].AsInteger );
            }
            printf( "\n" );
        }
    }
    catch( const std::exception& e )
    {
        fprintf( stderr, "v32peek: %s\n", e.what() );
        return 1;
    }
    return 0;
}
