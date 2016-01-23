// TODO: Thinks about what the real safe margin is!
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

#define TILES_PER_CHUNK 16

inline world_position
NullPosition(void) {
    world_position Result = {};

    Result.ChunkX = TILE_CHUNK_UNINITIALIZED;

    return Result;
}

inline bool32
IsValid(world_position P) {
    return P.ChunkX != TILE_CHUNK_UNINITIALIZED;
}

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
    Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ < TILE_CHUNK_SAFE_MARGIN);

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

        if (Arena && Chunk->ChunkX != TILE_CHUNK_UNINITIALIZED && !Chunk->NextInHash) {
            Chunk->NextInHash = PushStruct(Arena, world_chunk);
            Chunk = Chunk->NextInHash;
            Chunk->ChunkX = TILE_CHUNK_UNINITIALIZED;
        }

        if (Arena && Chunk->ChunkX == TILE_CHUNK_UNINITIALIZED) {
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
        World->ChunkHash[ChunkIndex].ChunkX = TILE_CHUNK_UNINITIALIZED;
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
MapIntoChunkSpace(world *World, world_position BasePos, v2 Offset) {
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

    // TODO: Think this through on the real stream and actually work out the math.
    if (AbsTileX < 0) {
        --Result.ChunkX;
    }
    if (AbsTileY < 0) {
        --Result.ChunkY;
    }
    if (AbsTileZ < 0) {
        --Result.ChunkZ;
    }

    // TODO: DECIDE ON TILE ALIGNMENT IN CHUNKS!
    Result.Offset_.X = ((AbsTileX - TILES_PER_CHUNK / 2) - (Result.ChunkX * TILES_PER_CHUNK)) * World->TileSideInMeters;
    Result.Offset_.Y = ((AbsTileY - TILES_PER_CHUNK / 2) - (Result.ChunkY * TILES_PER_CHUNK)) * World->TileSideInMeters;
    // TODO: Move to 3D Z!!!

    Assert(IsCanonical(World, Result.Offset_));

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
ChangeEntityLocationRaw(memory_arena *Arena, world *World, uint32 LowEntityIndex,
                     world_position *OldP, world_position *NewP) {
    // TODO: If this moves an entity into the camera bounds, should it automatically
    // go into the high set immediately?

    Assert(!OldP || IsValid(*OldP));
    Assert(!NewP || IsValid(*NewP));

    if (OldP && AreInSameChunk(World, OldP, NewP)) {
        // NOTE: Leave entity where it is.
    } else {
        if (OldP) {
            // NOTE: Pull the entity out of its old entity block
            world_chunk *Chunk = GetWorldChunk(World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ);
            Assert(Chunk);
            if (Chunk) {
                bool32 NotFound = true;
                world_entity_block *FirstBlock = &Chunk->FirstBlock;
                for (world_entity_block *Block = FirstBlock; Block && NotFound; Block = Block->Next) {
                    for (uint32 Index = 0; Index < Block->EntityCount && NotFound; ++Index) {
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

                            NotFound = false;
                        }
                    }
                }
            }
        }

        if (NewP) {
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
}

inline void
ChangeEntityLocation(memory_arena *Arena, world *World,
                     uint32 LowEntityIndex, low_entity *LowEntity,
                     world_position *OldP, world_position *NewP) {
    ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldP, NewP);
    if (NewP) {
        LowEntity->P = *NewP;
    } else {
        LowEntity->P = NullPosition();
    }
}
