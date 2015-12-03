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

struct tile_chunk_position {
    uint32 TileChunkX;
    uint32 TileChunkY;

    uint32 RelTileX;
    uint32 RelTileY;
};

struct world_position {
    /* TODO:

       Take the tile map x and y
       and the tile x and y

       and pack them into single 32-bit values for x and y
       where there is some low bits for the tile index
       and the high bits are the tile "page"

       (NOTE we can eliminate the need for floor!)
     */
    uint32 AbsTileX;
    uint32 AbsTileY;

    // TODO: Should these be from the center of a tile?
    // TODO: Rename to offset x and y
    real32 TileRelX;
    real32 TileRelY;
};

struct tile_chunk {
    uint32 *Tiles;
};

struct world {
    uint32 ChunkShift;
    uint32 ChunkMask;
    uint32 ChunkDim;

    real32 TileSideInMeters;
    int32 TileSideInPixels;
    real32 MetersToPixels;

    // TODO: Beginner's sparseness
    int32 TileChunkCountX;
    int32 TileChunkCountY;

    tile_chunk *TileChunks;
};

struct game_state {
    world_position PlayerP;
};

#endif
