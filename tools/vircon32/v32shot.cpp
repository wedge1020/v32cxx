// *****************************************************************************
//  tools/vircon32/v32shot.cpp — headless screenshots of a Vircon32 cartridge
//
//  Plays a cartridge on the emulator's own ConsoleLogic, driven by the same
//  input script v32prof takes, and writes the screen at chosen frames as
//  PPM images. The console's GPU hands every draw to the host as a textured
//  quad; here a small software rasterizer draws those quads into a 640x360
//  buffer (nearest-neighbour sampling, multiply colour, and the three
//  blending modes), so no window, OpenGL or SDL is involved.
//
//  usage: v32shot bios.v32 cart.v32 script out_prefix frame [frame...]
//         writes out_prefix<frame>.ppm for each listed frame
//
//  Script: as v32prof (connect / press / release / tap / end; `segment`
//  lines are accepted and ignored).
//
//  Good for checking what a change LOOKS like -- a sprite built from
//  several glyphs, a layout, an animation across a few frames -- without
//  a display. Not pixel-exact against the real renderer's filtering.
//
//  Built by build-tools.sh next to v32run and v32prof.
// *****************************************************************************
#include "V32Console.hpp"
#include "ExternalInterfaces.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace V32;

static const int SCREEN_W = 640, SCREEN_H = 360, TEX = Constants::GPUTextureSize;

static std::vector<uint8_t> g_screen( SCREEN_W * SCREEN_H * 3, 0 );
static std::map<int, std::vector<GPUColor>> g_textures;
static int g_texture = -1;
static GPUColor g_multiply = { 255, 255, 255, 255 };
static int g_blend = (int)IOPortValues::GPUBlendingMode_Alpha;

static void ClearScreen( GPUColor c )
{
    for( int i = 0; i < SCREEN_W * SCREEN_H; i++ )
    {
        g_screen[ i * 3 ] = c.R; g_screen[ i * 3 + 1 ] = c.G; g_screen[ i * 3 + 2 ] = c.B;
    }
}

static void LoadTexture( int id, void* pixels )
{
    const GPUColor* p = (const GPUColor*)pixels;
    g_textures[ id ].assign( p, p + (size_t)TEX * TEX );
}

static void DrawQuad( GPUQuad& q )
{
    auto tex = g_textures.find( g_texture );
    if( tex == g_textures.end() ) return;
    const GPUPoint& o = q.Vertices[ 0 ];
    // the quad is a parallelogram: P = V0 + u (V1 - V0) + v (V2 - V0)
    float ax = q.Vertices[ 1 ].x - o.x, ay = q.Vertices[ 1 ].y - o.y;
    float bx = q.Vertices[ 2 ].x - o.x, by = q.Vertices[ 2 ].y - o.y;
    float det = ax * by - ay * bx;
    if( std::fabs( det ) < 1e-6f ) return;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for( int i = 0; i < 4; i++ )
    {
        minx = std::min( minx, q.Vertices[ i ].x ); maxx = std::max( maxx, q.Vertices[ i ].x );
        miny = std::min( miny, q.Vertices[ i ].y ); maxy = std::max( maxy, q.Vertices[ i ].y );
    }
    int x0 = std::max( 0, (int)std::floor( minx ) ), x1 = std::min( SCREEN_W - 1, (int)std::ceil( maxx ) );
    int y0 = std::max( 0, (int)std::floor( miny ) ), y1 = std::min( SCREEN_H - 1, (int)std::ceil( maxy ) );
    float tu0 = o.texture_x, tv0 = o.texture_y;
    float tu1 = q.Vertices[ 1 ].texture_x, tv2 = q.Vertices[ 2 ].texture_y;
    for( int y = y0; y <= y1; y++ )
      for( int x = x0; x <= x1; x++ )
      {
          float px = x + 0.5f - o.x, py = y + 0.5f - o.y;
          float u = ( px * by - py * bx ) / det, v = ( ax * py - ay * px ) / det;
          if( u < 0 || u >= 1 || v < 0 || v >= 1 ) continue;
          int tx = (int)( ( tu0 + u * ( tu1 - tu0 ) ) * TEX ), ty = (int)( ( tv0 + v * ( tv2 - tv0 ) ) * TEX );
          if( tx < 0 || ty < 0 || tx >= TEX || ty >= TEX ) continue;
          GPUColor c = tex->second[ (size_t)ty * TEX + tx ];
          float a = ( c.A / 255.0f ) * ( g_multiply.A / 255.0f );
          float src[ 3 ] = { c.R * g_multiply.R / 255.0f, c.G * g_multiply.G / 255.0f, c.B * g_multiply.B / 255.0f };
          uint8_t* dst = &g_screen[ ( (size_t)y * SCREEN_W + x ) * 3 ];
          for( int k = 0; k < 3; k++ )
          {
              float out;
              if( g_blend == (int)IOPortValues::GPUBlendingMode_Add ) out = dst[ k ] + src[ k ] * a;
              else if( g_blend == (int)IOPortValues::GPUBlendingMode_Subtract ) out = dst[ k ] - src[ k ] * a;
              else out = dst[ k ] * ( 1 - a ) + src[ k ] * a;
              dst[ k ] = (uint8_t)std::max( 0.0f, std::min( 255.0f, out ) );
          }
      }
}

static void SetMultiplyColor( GPUColor c ) { g_multiply = c; }
static void SetBlendingMode( int m ) { g_blend = m; }
static void SelectTexture( int t ) { g_texture = t; }
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
    if( argc < 6 )
    {
        fprintf( stderr, "usage: v32shot bios.v32 cart.v32 script out_prefix frame [frame...]\n" );
        return 2;
    }
    Callbacks::ClearScreen = ClearScreen;
    Callbacks::DrawQuad = DrawQuad;
    Callbacks::SetMultiplyColor = SetMultiplyColor;
    Callbacks::SetBlendingMode = SetBlendingMode;
    Callbacks::SelectTexture = SelectTexture;
    Callbacks::LoadTexture = LoadTexture;
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
    std::set<int> shots;
    for( int i = 5; i < argc; i++ ) shots.insert( atoi( argv[ i ] ) );
    int last_frame = *shots.rbegin();

    std::vector<std::pair<int, GamepadControls>> pending_release;
    try
    {
        V32Console console;
        console.LoadBios( argv[ 1 ] );
        console.LoadCartridge( argv[ 2 ] );
        console.SetPower( true );
        size_t next = 0;
        for( int frame = 0; frame <= last_frame && !console.IsCPUHalted(); frame++ )
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
            if( shots.count( frame ) )
            {
                std::string path = std::string( argv[ 4 ] ) + std::to_string( frame ) + ".ppm";
                FILE* f = fopen( path.c_str(), "wb" );
                if( f == NULL ) { perror( path.c_str() ); return 1; }
                fprintf( f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H );
                fwrite( g_screen.data(), 1, g_screen.size(), f );
                fclose( f );
                printf( "frame %d -> %s\n", frame, path.c_str() );
            }
        }
        if( console.IsCPUHalted() ) printf( "CPU halted\n" );
    }
    catch( const std::exception& e )
    {
        fprintf( stderr, "v32shot: %s\n", e.what() );
        return 1;
    }
    return 0;
}
