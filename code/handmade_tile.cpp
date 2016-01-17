// TODO: Thinks about what the real safe margin is!
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINTIALIZED INT32_MAX

inline tile_chunk *
GetTileChunk(tile_map *TileMap, int32 TileChunkX, int32 TileChunkY, int32 TileChunkZ,
             memory_arena *Arena = 0) {
    Assert(TileChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ < TILE_CHUNK_SAFE_MARGIN);

    // TODO: BETTER HASH FUNCTION!!!!
    uint32 HashValue = 19 * TileChunkX + 7 * TileChunkY + 3 * TileChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(TileMap->TileChunkHash) - 1);
    Assert(HashSlot < ArrayCount(TileMap->TileChunkHash));

    tile_chunk *Chunk = TileMap->TileChunkHash + HashSlot;
    do {
        if (TileChunkX == Chunk->TileChunkX && TileChunkY == Chunk->TileChunkY &&
            TileChunkZ == Chunk->TileChunkZ) {
            break;
        }

        if (Arena && Chunk->TileChunkX != TILE_CHUNK_UNINTIALIZED && !Chunk->NextInHash) {
            Chunk->NextInHash = PushStruct(Arena, tile_chunk);
            Chunk = Chunk->NextInHash;
            Chunk->TileChunkX = TILE_CHUNK_UNINTIALIZED;
        }

        if (Arena && Chunk->TileChunkX == TILE_CHUNK_UNINTIALIZED) {
            uint32 TileCount = TileMap->ChunkDim * TileMap->ChunkDim;
            Chunk->TileChunkX = TileChunkX;
            Chunk->TileChunkY = TileChunkY;
            Chunk->TileChunkZ = TileChunkZ;
            Chunk->Tiles = PushArray(Arena, TileCount, uint32);

            // TODO: Do we want to always initialize?
            for (uint32 TileIndex = 0; TileIndex < TileCount; ++TileIndex) {
                Chunk->Tiles[TileIndex] = 1;
            }

            Chunk->NextInHash = 0;

            break;
        }

        Chunk = Chunk->NextInHash;
    } while (Chunk);

    return Chunk;
}

inline uint32
GetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, int32 TileX, int32 TileY) {
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    return TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
}

inline void
SetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, int32 TileX, int32 TileY, uint32 TileValue) {
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
}

internal void
SetTileValue(tile_map *TileMap, tile_chunk *TileChunk, real32 TestTileX, real32 TestTileY, uint32 TileValue) {
    if (TileChunk && TileChunk->Tiles) {
        SetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY, TileValue);
    }
}

inline tile_chunk_position
GetChunkPositionFor(tile_map *TileMap, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ) {
    tile_chunk_position Result;

    Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
    Result.TileChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & TileMap->ChunkMask;
    Result.RelTileY = AbsTileY & TileMap->ChunkMask;

    return Result;
}

internal bool32
GetTileValue(tile_map *TileMap, tile_chunk *TileChunk, real32 TestTileX, real32 TestTileY) {
    uint32 TileChunkValue = 0;

    if (TileChunk && TileChunk->Tiles) {
        TileChunkValue = GetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
    }

    return TileChunkValue;
}

internal uint32
GetTileValue(tile_map *TileMap, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    return GetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);
}

internal uint32
GetTileValue(tile_map *TileMap, tile_map_position Pos) {
    return GetTileValue(TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);
}

internal void
SetTileValue(memory_arena *Arena, tile_map *TileMap,
             int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ,
             uint32 TileValue) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ, Arena);
    SetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}

internal void
InitializeTileMap(tile_map *TileMap, real32 TileSideInMeters) {
    TileMap->ChunkShift = 4;
    TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
    TileMap->ChunkDim = (1 << TileMap->ChunkShift);
    TileMap->TileSideInMeters = TileSideInMeters;

    for (uint32 TileChunkIndex = 0;
         TileChunkIndex < ArrayCount(TileMap->TileChunkHash);
         ++TileChunkIndex) {
        TileMap->TileChunkHash[TileChunkIndex].TileChunkX = TILE_CHUNK_UNINTIALIZED;
    }
}

internal bool32
IsTileValueEmpty(uint32 TileChunkValue) {
    bool32 Empty = (TileChunkValue == 1 ||
                    TileChunkValue == 3 ||
                    TileChunkValue == 4);
    return Empty;
}

internal bool32
IsTileMapPointEmpty(tile_map *TileMap, tile_map_position CanPos) {
    uint32 TileChunkValue = GetTileValue(TileMap, CanPos.AbsTileX, CanPos.AbsTileY, CanPos.AbsTileZ);
    return IsTileValueEmpty(TileChunkValue);

}

//
// TODO: Do there really belong in more of a "positioning" or "geometry" file?
//
inline void
RecanonicalizeCoord(tile_map *TileMap, int32 *Tile, real32 *TileRel) {
    // TODO: Need to do something that doesn't use the divede/multiply method
    // for recanonicalizeing because this can end up rounding buack on to the tile
    // you just came from.

    // NOTE: TileMap is assumed to be toroidal topology, if you step off one end you
    // coma back on the other!
    int32 Offset = RoundReal32ToInt32(*TileRel / TileMap->TileSideInMeters);
    *Tile += Offset;
    *TileRel -= Offset * TileMap->TileSideInMeters;

    // TODO: Fix floating point math so this can be exacly?
    Assert(*TileRel > -0.5f * TileMap->TileSideInMeters);
    Assert(*TileRel < 0.5f * TileMap->TileSideInMeters);
}

inline tile_map_position
MapIntoTileSpace(tile_map *TileMap, tile_map_position BasePos, v2 Offset) {
    tile_map_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.Offset_.Y);

    return Result;
}


inline bool32
AreOnSameTile(tile_map_position *A, tile_map_position *B) {
    return (A->AbsTileX == B->AbsTileX &&
            A->AbsTileY == B->AbsTileY &&
            A->AbsTileZ == B->AbsTileZ);
}

inline tile_map_difference
Subtract(tile_map *TileMap, tile_map_position *A, tile_map_position *B) {
    tile_map_difference Result;

    v2 dTile = {(real32) A->AbsTileX - (real32) B->AbsTileX,
                (real32) A->AbsTileY - (real32) B->AbsTileY};
    real32 dTileZ = (real32) A->AbsTileZ - (real32) B->AbsTileZ;

    Result.dXY = TileMap->TileSideInMeters * dTile + (A->Offset_ - B->Offset_);

    // TODO: Think about what we want to do about Z
    Result.dZ = TileMap->TileSideInMeters * dTileZ;

    return Result;
}

inline tile_map_position
CenteredTilePoint(int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ) {
    tile_map_position Result = {};

    Result.AbsTileX = AbsTileX;
    Result.AbsTileY = AbsTileY;
    Result.AbsTileZ = AbsTileZ;

    return Result;
}
