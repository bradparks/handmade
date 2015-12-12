inline tile_chunk *
GetTileChunk(tile_map *TileMap, uint32 TileChunkX, uint32 TileChunkY, uint32 TileChunkZ) {
    tile_chunk *TileChunk = 0;
    if ((TileChunkX < TileMap->TileChunkCountX) &&
        (TileChunkY < TileMap->TileChunkCountY) &&
        (TileChunkZ < TileMap->TileChunkCountZ)) {
        TileChunk = &TileMap->TileChunks[TileChunkZ * TileMap->TileChunkCountY * TileMap->TileChunkCountX +
                                         TileChunkY * TileMap->TileChunkCountX +
                                         TileChunkX];
    }
    return TileChunk;
}

inline uint32
GetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, uint32 TileX, uint32 TileY) {
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    return TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
}

inline void
SetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, uint32 TileX, uint32 TileY, uint32 TileValue) {
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
}

internal bool32
GetTileValue(tile_map *TileMap, tile_chunk *TileChunk, real32 TestTileX, real32 TestTileY) {
    uint32 TileChunkValue = 0;

    if (TileChunk && TileChunk->Tiles) {
        TileChunkValue = GetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
    }

    return TileChunkValue;
}

internal void
SetTileValue(tile_map *TileMap, tile_chunk *TileChunk, real32 TestTileX, real32 TestTileY, uint32 TileValue) {
    if (TileChunk && TileChunk->Tiles) {
        SetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY, TileValue);
    }
}

inline tile_chunk_position
GetChunkPositionFor(tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    tile_chunk_position Result;

    Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
    Result.TileChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & TileMap->ChunkMask;
    Result.RelTileY = AbsTileY & TileMap->ChunkMask;

    return Result;
}

internal uint32
GetTileValue(tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
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
             uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ,
             uint32 TileValue) {
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);

    // TODO: On-demand tile chunk creation
    Assert(TileChunk);

    if (!TileChunk->Tiles) {
        uint32 TileCount = TileMap->ChunkDim * TileMap->ChunkDim;
        TileChunk->Tiles = PushArray(Arena, TileCount, uint32);
        for (uint32 TileIndex = 0; TileIndex < TileCount; ++TileIndex) {
            TileChunk->Tiles[TileIndex] = 1;
        }
    }

    SetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}

internal bool32
IsTileMapPointEmpty(tile_map *TileMap, tile_map_position CanPos) {
    uint32 TileChunkValue = GetTileValue(TileMap, CanPos.AbsTileX, CanPos.AbsTileY, CanPos.AbsTileZ);
    bool32 Empty = (TileChunkValue == 1 ||
                    TileChunkValue == 3 ||
                    TileChunkValue == 4);

    return Empty;

}

//
// TODO: Do there really belong in more of a "positioning" or "geometry" file?
//
inline void
RecanonicalizeCoord(tile_map *TileMap, uint32 *Tile, real32 *TileRel) {
    // TODO: Need to do something that doesn't use the divede/multiply method
    // for recanonicalizeing because this can end up rounding buack on to the tile
    // you just came from.

    // NOTE: TileMap is assumed to be toroidal topology, if you step off one end you
    // coma back on the other!
    int32 Offset = RoundReal32ToInt32(*TileRel / TileMap->TileSideInMeters);
    *Tile += Offset;
    *TileRel -= Offset * TileMap->TileSideInMeters;

    Assert(*TileRel >= -0.5f * TileMap->TileSideInMeters);
    // TODO: Fix floating point math so this can be < ?
    Assert(*TileRel <= 0.5f * TileMap->TileSideInMeters);
}

inline tile_map_position
RecanonicalizePosition(tile_map *TileMap, tile_map_position Pos) {
    tile_map_position Result = Pos;

    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.OffsetX);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.OffsetY);

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

    real32 dTileX = (real32) A->AbsTileX - (real32) B->AbsTileX;
    real32 dTileY = (real32) A->AbsTileY - (real32) B->AbsTileY;
    real32 dTileZ = (real32) A->AbsTileZ - (real32) B->AbsTileZ;

    Result.dX = TileMap->TileSideInMeters * dTileX + (A->OffsetX - B->OffsetX);
    Result.dY = TileMap->TileSideInMeters * dTileY + (A->OffsetY - B->OffsetY);

    // TODO: Think about what we want to do about Z
    Result.dZ = TileMap->TileSideInMeters * dTileZ;

    return Result;
}
