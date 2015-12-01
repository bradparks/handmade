#ifndef HANDMADE_H
#define HANDMADE_H

/*
  NOTE:

  HANDMADE_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  HANDMADE_SLOW:
    0 - Not slow code allowd!
    1 - Slow code welcome.
 */

#include <assert.h>
#include <stdio.h>
#include "handmade_platform.h"

#define internal static
#define global_variable static
#define local_persist static
#define PI32 3.14159265359f

#if HANDMADE_SLOW
#define Assert(Expression) assert(Expression)
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// TODO: swap, min, max ... macros ???

inline uint32
SafeTruncateUInt64(uint64 Value) {
    // TODO: Defines for maximum values UInt32Max
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32) Value;
    return Result;
}

inline game_controller_input *GetController(game_input *Input, size_t ControllerIndex) {
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return &Input->Controllers[ControllerIndex];
}

// TODO: This can becom "world position" or something similar
struct canonical_position {
    /* TODO:

       Take the tile map x and y
       and the tile x and y

       and pack them into single 32-bit values for x and y
       where there is some low bits for the tile index
       and the high bits are the tile "page"

       (NOTE we can eliminate the need for floor!)
     */
#if 1
    int32 TileMapX;
    int32 TileMapY;

    int32 TileX;
    int32 TileY;
#else
    uint32 _TileX;
    uint32 _TileY;
#endif

    // TODO: Y Should go up!
    // TODO: Should these be from the center of a tile?
    real32 TileRelX;
    real32 TileRelY;
};

struct tile_map {
    uint32 *Tiles;
};

struct world {
    real32 TileSideInMeters;
    int32 TileSideInPixels;
    real32 MetersToPixels;

    int32 CountX;
    int32 CountY;

    real32 UpperLeftX;
    real32 UpperLeftY;

    // TODO: Beginner's sparseness
    int32 TileMapCountX;
    int32 TileMapCountY;

    tile_map *TileMaps;
};

struct game_state {
    canonical_position PlayerP;
};

#endif
