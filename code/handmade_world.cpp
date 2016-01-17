// TODO: Thinks about what the real safe margin is!
#define world_chunk_SAFE_MARGIN (INT32_MAX/64)
#define world_chunk_UNINTIALIZED INT32_MAX

inline world_chunk *
GetChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
             memory_arena *Arena = 0) {
    Assert(ChunkX > -world_chunk_SAFE_MARGIN);
    Assert(ChunkY > -world_chunk_SAFE_MARGIN);
    Assert(ChunkZ > -world_chunk_SAFE_MARGIN);
    Assert(ChunkX < world_chunk_SAFE_MARGIN);
    Assert(ChunkY < world_chunk_SAFE_MARGIN);
    Assert(ChunkZ < world_chunk_SAFE_MARGIN);

    // TODO: BETTER HASH FUNCTION!!!!
    uint32 HashValue = 19 * ChunkX + 7 * ChunkY + 3 * ChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
    Assert(HashSlot < ArrayCount(World->ChunkHash));

    world_chunk *Chunk = World->ChunkHash + HashSlot;
    do {
        if (ChunkX == Chunk->ChunkX && ChunkY == Chunk->ChunkY &&
            ChunkZ == Chunk->ChunkZ) {
            break;
        }

        if (Arena && Chunk->ChunkX != world_chunk_UNINTIALIZED && !Chunk->NextInHash) {
            Chunk->NextInHash = PushStruct(Arena, world_chunk);
            Chunk = Chunk->NextInHash;
            Chunk->ChunkX = world_chunk_UNINTIALIZED;
        }

        if (Arena && Chunk->ChunkX == world_chunk_UNINTIALIZED) {
            uint32 WorldCount = World->ChunkDim * World->ChunkDim;
            Chunk->ChunkX = ChunkX;
            Chunk->ChunkY = ChunkY;
            Chunk->ChunkZ = ChunkZ;

            Chunk->NextInHash = 0;

            break;
        }

        Chunk = Chunk->NextInHash;
    } while (Chunk);

    return Chunk;
}

#if 0
inline world_chunk_position
GetChunkPositionFor(world *World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ) {
    world_chunk_position Result;

    Result.ChunkX = AbsTileX >> World->ChunkShift;
    Result.ChunkY = AbsTileY >> World->ChunkShift;
    Result.ChunkZ = AbsTileZ;
    Result.RelWorldX = AbsTileX & World->ChunkMask;
    Result.RelWorldY = AbsTileY & World->ChunkMask;

    return Result;
}
#endif

internal void
InitializeWorld(world *World, real32 TileSideInMeters) {
    World->ChunkShift = 4;
    World->ChunkMask = (1 << World->ChunkShift) - 1;
    World->ChunkDim = (1 << World->ChunkShift);
    World->TileSideInMeters = TileSideInMeters;

    for (uint32 ChunkIndex = 0;
         ChunkIndex < ArrayCount(World->ChunkHash);
         ++ChunkIndex) {
        World->ChunkHash[ChunkIndex].ChunkX = world_chunk_UNINTIALIZED;
    }
}

//
// TODO: Do there really belong in more of a "positioning" or "geometry" file?
//
inline void
RecanonicalizeCoord(world *World, int32 *Tile, real32 *TileRel) {
    // TODO: Need to do something that doesn't use the divede/multiply method
    // for recanonicalizeing because this can end up rounding buack on to the tile
    // you just came from.

    // NOTE: World is assumed to be toroidal topology, if you step off one end you
    // coma back on the other!
    int32 Offset = RoundReal32ToInt32(*TileRel / World->TileSideInMeters);
    *Tile += Offset;
    *TileRel -= Offset * World->TileSideInMeters;

    // TODO: Fix floating point math so this can be exacly?
    Assert(*TileRel > -0.5f * World->TileSideInMeters);
    Assert(*TileRel < 0.5f * World->TileSideInMeters);
}

inline world_position
MapIntoTileSpace(world *World, world_position BasePos, v2 Offset) {
    world_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.AbsTileY, &Result.Offset_.Y);

    return Result;
}


inline bool32
AreOnSameTile(world_position *A, world_position *B) {
    return (A->AbsTileX == B->AbsTileX &&
            A->AbsTileY == B->AbsTileY &&
            A->AbsTileZ == B->AbsTileZ);
}

inline world_difference
Subtract(world *World, world_position *A, world_position *B) {
    world_difference Result;

    v2 dTile = {(real32) A->AbsTileX - (real32) B->AbsTileX,
                (real32) A->AbsTileY - (real32) B->AbsTileY};
    real32 dTileZ = (real32) A->AbsTileZ - (real32) B->AbsTileZ;

    Result.dXY = World->TileSideInMeters * dTile + (A->Offset_ - B->Offset_);

    // TODO: Think about what we want to do about Z
    Result.dZ = World->TileSideInMeters * dTileZ;

    return Result;
}

inline world_position
CenteredTilePoint(int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ) {
    world_position Result = {};

    Result.AbsTileX = AbsTileX;
    Result.AbsTileY = AbsTileY;
    Result.AbsTileZ = AbsTileZ;

    return Result;
}
