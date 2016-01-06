#ifndef HANDMADE_TILE_H
#define HANDMADE_TILE_H

// TODO: Replace this with a v3 once we get to v3
struct tile_map_difference {
    v2 dXY;
    real32 dZ;
};

struct tile_map_position {
    // NOTE: Thses are fixed point tile locations.  The high
    // bits are the tile chunk index, and the low bits are the tile
    // idex in the chunk.
    uint32 AbsTileX;
    uint32 AbsTileY;
    uint32 AbsTileZ;

    // NOTE: These are the offset from tile center
    v2 Offset_;
};

struct tile_chunk_position {
    uint32 TileChunkX;
    uint32 TileChunkY;
    uint32 TileChunkZ;

    uint32 RelTileX;
    uint32 RelTileY;
};

struct tile_chunk {
    // TODO: Real structure for a tile!
    uint32 *Tiles;
};

struct tile_map {
    uint32 ChunkShift;
    uint32 ChunkMask;
    uint32 ChunkDim;

    real32 TileSideInMeters;

    // TODO: REAL sparseness so anywhere in the world can be
    // represented withou the giant pointer array.
    uint32 TileChunkCountX;
    uint32 TileChunkCountY;
    uint32 TileChunkCountZ;
    tile_chunk *TileChunks;
};

#endif
