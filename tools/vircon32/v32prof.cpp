// *****************************************************************************
//  tools/vircon32/v32prof.cpp — headless CPU profiler for a Vircon32 cartridge
//
//  Plays a cartridge on the emulator's own ConsoleLogic (no video/audio),
//  driven by a small input script, and reports:
//    * per-frame CPU load (cycles until the program's wait-for-frame, as a
//      % of the 250,000-cycle frame budget) and GPU load, overall and per
//      script segment: average, 95th percentile, max, frames at 100%;
//    * where the cycles go: the instruction pointer is sampled EVERY cycle
//      and attributed to the enclosing function (from `assemble -g program`
//      debug info), both over all frames and over "heavy" frames only
//      (CPU load >= the threshold), which is what explains slowdowns.
//
//  usage: v32prof bios.v32 cart.v32 cart.vbin.debug script [heavy%=85] [csv]
//
//  Script: one command per line, `#` comments allowed.
//    <frame> connect <port> <0|1>        plug / unplug a gamepad
//    <frame> press   <port> <control>    control: left right up down start
//    <frame> release <port> <control>             a b x y l r
//    <frame> tap     <port> <control>    press now, release next frame
//    <frame> segment <name>              start a named measurement segment
//    <frame> end                         stop the run
//  The optional csv path receives one line per frame: frame,segment,cpu%,gpu%.
//  V32PROF_SEGTOP=N lists N functions per segment (default 8);
//  V32PROF_RAMDUMP=file writes the final RAM (4M little-endian words).
//
//  Built by build-tools.sh next to v32run.
// *****************************************************************************
#include "V32Console.hpp"
#include "ExternalInterfaces.hpp"
#include <algorithm>
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

struct Function { uint32_t start; std::string name; };

struct Segment
{
    std::string name;
    std::vector<float> cpu, gpu;
    std::vector<uint64_t> hist;
};

static double percentile( std::vector<float> v, double p )
{
    if( v.empty() ) return 0;
    std::sort( v.begin(), v.end() );
    size_t i = (size_t)( p * ( v.size() - 1 ) );
    return v[ i ];
}

static void report_functions( const char* title, const std::vector<uint64_t>& hist,
                              const std::vector<Function>& funcs, uint64_t total )
{
    printf( "\n%s (%llu cycles)\n", title, (unsigned long long)total );
    if( total == 0 ) return;
    std::vector<std::pair<uint64_t, std::string>> rows;
    for( size_t i = 0; i < funcs.size(); i++ )
        if( hist[ i ] ) rows.push_back( { hist[ i ], funcs[ i ].name } );
    std::sort( rows.rbegin(), rows.rend() );
    for( size_t i = 0; i < rows.size() && i < 25; i++ )
        printf( "  %6.2f%%  %s\n", 100.0 * rows[ i ].first / total, rows[ i ].second.c_str() );
}

int main( int argc, char** argv )
{
    if( argc < 5 )
    {
        fprintf( stderr, "usage: v32prof bios.v32 cart.v32 cart.vbin.debug script [heavy%%=85] [csv]\n" );
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
    float heavy = ( argc > 5 ) ? (float)atof( argv[ 5 ] ) : 85.0f;
    FILE* csv = ( argc > 6 ) ? fopen( argv[ 6 ], "w" ) : nullptr;

    // function table from `assemble -g program`: addr,file,line[,label]
    std::vector<Function> funcs;
    {
        std::ifstream in( argv[ 3 ] );
        std::string line;
        while( std::getline( in, line ) )
        {
            std::stringstream ss( line );
            std::string addr, file, ln, label;
            std::getline( ss, addr, ',' ); std::getline( ss, file, ',' );
            std::getline( ss, ln, ',' );   std::getline( ss, label, ',' );
            if( label.rfind( "__function_", 0 ) == 0 &&
                label.find( "_return" ) == std::string::npos )
                funcs.push_back( { (uint32_t)strtoul( addr.c_str(), nullptr, 16 ),
                                   label.substr( 11 ) } );
            else if( label == "__global_scope_initialization" )
                funcs.push_back( { (uint32_t)strtoul( addr.c_str(), nullptr, 16 ), "(startup)" } );
        }
        std::sort( funcs.begin(), funcs.end(),
                   []( const Function& a, const Function& b ) { return a.start < b.start; } );
        if( funcs.empty() ) { fprintf( stderr, "v32prof: no function labels in %s\n", argv[ 3 ] ); return 2; }
    }
    funcs.push_back( { 0xFFFFFFFFu, "(BIOS / outside cartridge)" } );
    const size_t OUTSIDE = funcs.size() - 1;

    std::vector<Command> script;
    {
        std::ifstream in( argv[ 4 ] );
        std::string line;
        while( std::getline( in, line ) )
        {
            size_t h = line.find( '#' );
            if( h != std::string::npos ) line = line.substr( 0, h );
            std::stringstream ss( line );
            Command c{ -1, "", 0, "" };
            if( !( ss >> c.frame >> c.op ) ) continue;
            if( c.op == "segment" ) ss >> c.arg;
            else if( c.op != "end" ) ss >> c.port >> c.arg;
            script.push_back( c );
        }
        std::stable_sort( script.begin(), script.end(),
                          []( const Command& a, const Command& b ) { return a.frame < b.frame; } );
    }

    std::vector<uint64_t> hist_all( funcs.size(), 0 ), hist_heavy( funcs.size(), 0 ), frame_hist( funcs.size(), 0 );
    std::vector<Segment> segments{ { "(boot)", {}, {}, std::vector<uint64_t>( funcs.size(), 0 ) } };
    std::vector<std::pair<int, GamepadControls>> pending_release;

    try
    {
        V32Console console;
        console.LoadBios( argv[ 1 ] );
        console.LoadCartridge( argv[ 2 ] );
        console.SetPower( true );
        size_t next = 0;
        int frame = 0, last_frame = script.empty() ? 600 : script.back().frame;
        for( ; frame <= last_frame && !console.IsCPUHalted(); frame++ )
        {
            for( auto& r : pending_release ) console.SetGamepadControl( r.first, r.second, false );
            pending_release.clear();
            bool stop = false;
            for( ; next < script.size() && script[ next ].frame == frame; next++ )
            {
                const Command& c = script[ next ];
                GamepadControls ctl;
                if( c.op == "connect" ) console.SetGamepadConnection( c.port, c.arg == "1" );
                else if( c.op == "segment" ) segments.push_back( { c.arg, {}, {}, std::vector<uint64_t>( funcs.size(), 0 ) } );
                else if( c.op == "end" ) stop = true;
                else if( control_from_name( c.arg, ctl ) )
                {
                    if( c.op == "press" || c.op == "tap" ) console.SetGamepadControl( c.port, ctl, true );
                    if( c.op == "release" ) console.SetGamepadControl( c.port, ctl, false );
                    if( c.op == "tap" ) pending_release.push_back( { c.port, ctl } );
                }
            }
            if( stop ) break;

            // V32Console::RunNextFrame, with the instruction pointer sampled
            // every cycle
            console.Timer.ChangeFrame();
            console.CPU.ChangeFrame();
            console.GPU.ChangeFrame();
            console.SPU.ChangeFrame();
            console.GamepadController.ChangeFrame();
            std::fill( frame_hist.begin(), frame_hist.end(), 0 );
            size_t cached = 0;
            try
            {
                for( int i = 0; i < Constants::CyclesPerFrame; i++ )
                {
                    if( console.CPU.Waiting || console.CPU.Halted ) break;
                    uint32_t ip = (uint32_t)console.CPU.InstructionPointer.AsInteger;
                    size_t f = OUTSIDE;
                    if( ip >= (uint32_t)Constants::CartridgeProgramROMFirstAddress )
                    {
                        uint32_t rel = ip;   // `-g program` addresses are absolute
                        // most cycles stay in the same function: try the cached one first
                        if( !( cached < OUTSIDE && funcs[ cached ].start <= rel && funcs[ cached + 1 ].start > rel ) )
                        {
                            auto it = std::upper_bound( funcs.begin(), funcs.end() - 1, rel,
                                [] ( uint32_t v, const Function& fn ) { return v < fn.start; } );
                            cached = ( it == funcs.begin() ) ? OUTSIDE : (size_t)( it - funcs.begin() ) - 1;
                        }
                        f = cached;
                    }
                    frame_hist[ f ]++;
                    console.Timer.RunNextCycle();
                    console.CPU.RunNextCycle();
                }
            }
            catch( CPUException& ) {}

            float cpu = 100.0f * console.Timer.CycleCounter / Constants::CyclesPerFrame;
            int used = Constants::GPUPixelCapacityPerFrame - std::max( 0, (int)console.GPU.RemainingPixels );
            float gpu = 100.0f * used / Constants::GPUPixelCapacityPerFrame;
            segments.back().cpu.push_back( cpu );
            segments.back().gpu.push_back( gpu );
            for( size_t i = 0; i < funcs.size(); i++ )
            {
                hist_all[ i ] += frame_hist[ i ];
                segments.back().hist[ i ] += frame_hist[ i ];
                if( cpu >= heavy ) hist_heavy[ i ] += frame_hist[ i ];
            }
            if( csv ) fprintf( csv, "%d,%s,%.2f,%.2f\n", frame, segments.back().name.c_str(), cpu, gpu );
        }
        if( getenv( "V32PROF_RAMDUMP" ) )
        {
            // whole RAM (4M words, little-endian) for post-mortem inspection
            FILE* dump = fopen( getenv( "V32PROF_RAMDUMP" ), "wb" );
            if( dump )
            {
                fwrite( &console.RAM.Memory[ 0 ], 4, console.RAM.Memory.size(), dump );
                fclose( dump );
            }
        }
        printf( "frames run: %d\n\n", frame );
        printf( "%-22s %7s %7s %7s %7s %8s %7s %7s\n", "segment", "frames", "cpu avg", "p95", "max",
                ">=100%", "gpu avg", "gpu max" );
        for( auto& s : segments )
        {
            if( s.cpu.empty() ) continue;
            double avg = 0, gavg = 0;
            int over = 0;
            for( float v : s.cpu ) { avg += v; if( v >= 99.99f ) over++; }
            for( float v : s.gpu ) gavg += v;
            printf( "%-22s %7zu %6.1f%% %6.1f%% %6.1f%% %8d %6.1f%% %6.1f%%\n", s.name.c_str(), s.cpu.size(),
                    avg / s.cpu.size(), percentile( s.cpu, 0.95 ), *std::max_element( s.cpu.begin(), s.cpu.end() ),
                    over, gavg / s.gpu.size(), *std::max_element( s.gpu.begin(), s.gpu.end() ) );
        }
        for( auto& s : segments )
        {
            if( s.cpu.empty() ) continue;
            uint64_t t = 0;
            for( uint64_t h : s.hist ) t += h;
            std::string title = "segment " + s.name;
            std::vector<uint64_t> h = s.hist;
            printf( "\n%s: top functions", title.c_str() );
            std::vector<std::pair<uint64_t, std::string>> rows;
            for( size_t i = 0; i < funcs.size(); i++ ) if( h[ i ] ) rows.push_back( { h[ i ], funcs[ i ].name } );
            std::sort( rows.rbegin(), rows.rend() );
            size_t top = getenv( "V32PROF_SEGTOP" ) ? (size_t)atoi( getenv( "V32PROF_SEGTOP" ) ) : 8;
            for( size_t i = 0; i < rows.size() && i < top; i++ )
                printf( "%s %.1f%% %s", i ? "," : "", 100.0 * rows[ i ].first / ( t ? t : 1 ), rows[ i ].second.c_str() );
            printf( "\n" );
        }
        uint64_t total_all = 0, total_heavy = 0;
        for( size_t i = 0; i < funcs.size(); i++ ) { total_all += hist_all[ i ]; total_heavy += hist_heavy[ i ]; }
        report_functions( "CPU time by function, all frames", hist_all, funcs, total_all );
        char title[ 96 ];
        snprintf( title, sizeof title, "CPU time by function, heavy frames (cpu >= %.0f%%)", heavy );
        report_functions( title, hist_heavy, funcs, total_heavy );
        if( csv ) fclose( csv );
        return 0;
    }
    catch( std::exception& e ) { fprintf( stderr, "v32prof: %s\n", e.what() ); return 3; }
}
