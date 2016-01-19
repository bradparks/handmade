// TODO: Thinks about what the real safe margin is!
#define world_chunk_SAFE_MARGIN (INT32_MAX/64)
#define world_chunk_UNINTIALIZED INT32_MAX

#define TILES_PER_CHUNK 16

inline bool32
IsCanonical(world *World, real32 TileRel) {
    bool32 Result = TileRel >= -0.5f * World->ChunkSideInMeters &&
                    TileRel <= 0.5f * World->ChunkSideInMeters;
    // TODO: Fix floating point math so this can be exacly?
    return Result;
}

inline bool32
IsCanonical(world *World, v2 Offset) {
    bool32 Result = IsCanonical(World, Offset.X) && IsCanonical(World, Offset.Y);
    // TODO: Fix floating point math so this can be exacly?
    return Result;
}

inline bool32
AreInSameChunk(world *World, world_position *A, world_position *B) {
    Assert(IsCanonical(World, A->Offset_));
    Assert(IsCanonical(World, B->Offset_));

    return (A->ChunkX == B->ChunkX &&
            A->ChunkY == B->ChunkY &&
            A->ChunkZ == B->ChunkZ);
}

inline world_chunk *
GetWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
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
    World->TileSideInMeters = TileSideInMeters;
    World->ChunkSideInMeters = (real32)TILES_PER_CHUNK * TileSideInMeters;
    World->FirstFree = 0;

    for (uint32 ChunkIndex = 0;
         ChunkIndex < ArrayCount(World->ChunkHash);
         ++ChunkIndex) {
        World->ChunkHash[ChunkIndex].ChunkX = world_chunk_UNINTIALIZED;
        World->ChunkHash[ChunkIndex].FirstBlock.EntityCount = 0;
    }
}

inline void
RecanonicalizeCoord(world *World, int32 *Tile, real32 *TileRel) {
    // TODO: Need to do something that doesn't use the divede/multiply method
    // for recanonicalizeing because this can end up rounding buack on to the tile
    // you just came from.

    // NOTE: Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
    // within the safe margin!
    // TODO: Assert that we are nowhere near the edges of the world.

    int32 Offset = RoundReal32ToInt32(*TileRel / World->ChunkSideInMeters);
    *Tile += Offset;
    *TileRel -= Offset * World->ChunkSideInMeters;

    Assert(IsCanonical(World, *TileRel));
}

inline world_position
MapIntoTileSpace(world *World, world_position BasePos, v2 Offset) {
    world_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.ChunkX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.ChunkY, &Result.Offset_.Y);

    return Result;
}

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ) {
    world_position Result = {};

    Result.ChunkX = AbsTileX / TILES_PER_CHUNK;
    Result.ChunkY = AbsTileY / TILES_PER_CHUNK;
    Result.ChunkZ = AbsTileZ / TILES_PER_CHUNK;

    Result.Offset_.X = (AbsTileX - (Result.ChunkX * TILES_PER_CHUNK)) * World->TileSideInMeters;
    Result.Offset_.Y = (AbsTileY - (Result.ChunkY * TILES_PER_CHUNK)) * World->TileSideInMeters;
    // TODO: Move to 3D Z!!!
    return Result;
}

inline world_difference
Subtract(world *World, world_position *A, world_position *B) {
    world_difference Result;

    v2 dTile = {(real32) A->ChunkX - (real32) B->ChunkX,
                (real32) A->ChunkY - (real32) B->ChunkY};
    real32 dTileZ = (real32) A->ChunkZ - (real32) B->ChunkZ;

    Result.dXY = World->ChunkSideInMeters * dTile + (A->Offset_ - B->Offset_);

    // TODO: Think about what we want to do about Z
    Result.dZ = World->ChunkSideInMeters * dTileZ;

    return Result;
}

inline world_position
CenteredChunkPoint(int32 ChunkX, int32 ChunkY, int32 ChunkZ) {
    world_position Result = {};

    Result.ChunkX = ChunkX;
    Result.ChunkY = ChunkY;
    Result.ChunkZ = ChunkZ;

    return Result;
}

inline void
ChangeEntityLocation(memory_arena *Arena, world *World, uint32 LowEntityIndex,
                     world_position *OldP, world_position *NewP) {
    if (OldP && AreInSameChunk(World, OldP, NewP)) {
        // NOTE: Leave entity where it is.
    } else {
        if (OldP) {
            // NOTE: Pull the entity out of its old entity block
            world_chunk *Chunk = GetWorldChunk(World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ);
            Assert(Chunk);
            if (Chunk) {
                world_entity_block *FirstBlock = &Chunk->FirstBlock;
                for (world_entity_block *Block = FirstBlock; Block; Block = Block->Next) {
                    for (uint32 Index = 0; Index < Block->EntityCount; ++Index) {
                        if (Block->LowEntityIndex[Index] == LowEntityIndex) {
                            Assert(FirstBlock->EntityCount > 0);
                            Block->LowEntityIndex[Index] =
                                FirstBlock->LowEntityIndex[FirstBlock->EntityCount--];
                            if (FirstBlock->EntityCount == 0) {
                                if (FirstBlock->Next) {
                                    world_entity_block *NextBlock = FirstBlock->Next;
                                    *FirstBlock = *NextBlock;

                                    NextBlock->Next = World->FirstFree;
                                    World->FirstFree = NextBlock;
                                }
                            }

                            Block = 0;
                            break;
                        }
                    }
                }
            }
        }

        // NOTE: Insert the entity into its new entity block
        world_chunk *Chunk = GetWorldChunk(World, NewP->ChunkX, NewP->ChunkY, NewP->ChunkZ, Arena);
        Assert(Chunk);

        world_entity_block *Block = &Chunk->FirstBlock;
        if (Block->EntityCount == ArrayCount(Block->LowEntityIndex)) {
            // NOTE: We're out of room, get a new block!
            world_entity_block *OldBlock = World->FirstFree;
            if (OldBlock) {
                World->FirstFree = OldBlock->Next;
            } else {
               OldBlock = PushStruct(Arena, world_entity_block);
            }

            *OldBlock = *Block;
            Block->Next = OldBlock;
            Block->EntityCount = 0;
        }

        Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
        Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
    }
}
