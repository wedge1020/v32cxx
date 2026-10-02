// *****************************************************************************
//  TEMPEST 32K -- inc/tempest.hpp
//  the one header every module includes: SDK + v32 headers, config, state
// *****************************************************************************
#pragma once

#include <v32/video.hpp>
#include <v32/input.hpp>
#include <v32/time.hpp>
#include <v32/math.hpp>     // sqrt() for segment lengths (hardware pow)
#include "audio.h"         // SPU: stop/assign/play channel, channel states
#include "memcard.h"       // memory card: card_is_connected/read/write data

#include "config.hpp"
#include "state.hpp"
