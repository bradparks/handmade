internal sim_entity_hash *
GetHashFromStorageIndex(sim_region *SimRegion, uint32 StorageIndex) {
    Assert(StorageIndex);

    sim_entity_hash *Result = 0;

    uint32 HashValue = StorageIndex;
    for (uint32 Offset = 0; Offset < ArrayCount(SimRegion->Hash); ++Offset) {
        sim_entity_hash *Entry = SimRegion->Hash +
            ((HashValue + Offset) & (ArrayCount(SimRegion->Hash) - 1));
        if (Entry->Index == 0 || Entry->Index == StorageIndex) {
            Result = Entry->Ptr;
            break;
        }
    }

    return Result;
}

internal void
MapStorageIndexToEntity(sim_region *SimRegion, uint32 StorageIndex, sim_entity *Entity) {
    sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
    Assert(Entry->Index == 0 || Entry->Index == StorageIndex);
    Entry->Index = StorageIndex;
    Entry->Ptr = Entity;
}

inline sim_entity *
GetEntityByStorageIndex(sim_region *SimRegion, uint32 StorageIndex) {
    sim_entity_hash Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
    sim_entity *Result = Entry->Ptr;
    return Result;
}

internal sim_entity *AddEntity(game_state *GameState, sim_region *SimRegion, uin32 StorageIndex, low_entity *Source);
inline void
LoadEntityReference(game_state *GameState, sim_region *SimRegion, entity_reference *Ref) {
    if (Ref->Index) {
        sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
        if (Entry->Ptr == 0) {
            Entry->Index = Ref->Index;
            Entry->Ptr = AddEntity(GameState, SimRegion, Ref->Index, GetLowEntity(GameState, Ref->Index));
        }

        Ref->Ptr = Entry->Ptr;
    }
}

inline void
StoreEntityReference(entity_reference *Ref) {
    if (Ref->Ptr != 0) {
        Ref->Index = Ref->Ptr->StorageIndex;
    }
}

internal sim_entity *
AddEntity(game_state *GameState, sim_region *SimRegion, uin32 StorageIndex, low_entity *Source) {
    Assert(StorageIndex);

    sim_entity *Entity = 0;

    if (SimRegion->EntityCount < SimRegion->MaxEntityCount) {
        Entity = SimRegion->Entities + SimRegion->EntityCount++;

        // TODO: This should really be a decompression step, not
        // a copy!
        if (Source) {
            *Entity = Source->Sim;
            LoadEntityReference(GameState, SimRegion, &Entity->Sword);
        }

        Entity->StorageIndex = StorageIndex;
        MapStorageIndexToEntity(SimRegion, StorageIndex, Entity);
    } else {
        InvalidCodePath;
    }

    return Entity;
}

inline v2
GetSimSpaceP(sim_region *SimRegion, low_entity *Stored) {
    world_difference Diff = Subtract(SimRegion->World, &Stored->P, &SimRegion->Origin);
    v2 Result = Diff.dXY;
    return Result;
}

internal sim_entity *
AddEntity(game_state GameState, sim_region *SimRegion, uin32 StorageIndex, low_entity *Source, v2 *SimP) {
    sim_entity *Dest = AddEntity(GameState, SimRegion, StorageIndex, Source);
    if (Dest) {
        if (SimP) {
            Dest->P = *SimP;
        } else {
            Dest->P = GetRegionpaceP(SimRegion, Source);
        }
    }
}

internal sim_region *
BeginSim(memory_arena *SimArena, game_state *GameState, world *World, world_position Origin, rectangle2 Bounds) {
    // TODO: If entities were stored in the world, wouldn't need the game state here!

    // TODO IMPORTANT: CLEAR THE HASH TABLE!!!
    // TODO IMPORTANT: MOTION OF ACTIVE vs. INACTIVE ENTITIES FOR THE APRON!
    sim_region *SimRegion = PushStruct(SimArena, sim_region);
    SimRegion->World = World;
    SimRegion->Origin = Origin;
    SimRegion->Bounds = Bounds;

    // TODO: Need to be more specific about entity counts.
    SimRegion->MaxEntityCount = 4096;
    SimRegion->EntityCount = 0;
    SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

    world_position MinChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCornor(SimRegion->Bounds));
    world_position MaxChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCornor(SimRegion->Bounds));

    for (int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ++ChunkY) {
        for (int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ++ChunkX) {
            world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, SimRegion->Origin.ChunkZ);
            if (Chunk) {
                for (world_entity_block *Block = &Chunk->FirstBlock; Block; Block = Block->Next) {
                    for (uint32 EntityIndexIndex = 0; EntityIndexIndex < Block->EntityCount; ++EntityIndexIndex) {
                        uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndexIndex];
                        low_entity *Low = GameState->LowEntities + LowEntityIndex;
                        v2 SimSpaceP = GetSimSpaceP(SimRegion, Low);
                        if (IsInRectange(SimRegion->Bounds, SimSpaceP)) {
                            // TODO: Check a second rectange to set the entity
                            // to be "movable" or not!
                            AddEntity(GameState, SimRegion, LowEntityIndex, Low, &SimSpaceP);
                        }
                    }
                }
            }
        }
    }
}

internal void
EndSim(sim_region *Region, game_state *GameState) {
    // TODO: Maybe don't takt a game state here, low entities should be stored
    // in the world??
    sim_entity *Entity = Region->Entities;
    for (uint32 EntityIndex = 0; EntityIndex < Region->EntityCount; ++EntityIndex) {
        low_entity *Stored = GameState->LowEntities + Entity->StorageIndex;
        Stored->Sim = *Entity;
        StoreEntityReference(&Stored->Sim.Sword);

        // TODO: Save state back to the stored entity, once high entities
        // do state decompression, etc.

        world_position NewP = MapIntoChunkSpace(GameState->World, Region->Origin, Entity->P);
        ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity->StorageIndex,
                             Stored, &Stored->P, &NewP);

        if (Entity->StorageIndex == GameState->CameraFollowingEntityIndex) {
            world_position NewCameraP = GameState->CameraP;

            NewCameraP.ChunkZ = Stored->P.ChunkZ;

#if 0
            if (CameraFollowingEntity->P.X > 9.0f * World->TileSideInMeters) {
                NewCameraP.AbsTileX += 17;
            }

            if (CameraFollowingEntity->P.X < -9.0f * World->TileSideInMeters) {
                NewCameraP.AbsTileX -= 17;
            }

            if (CameraFollowingEntity->P.Y > 5.0f * World->TileSideInMeters) {
                NewCameraP.AbsTileY += 9;
            }

            if (CameraFollowingEntity->P.Y < -5.0f * World->TileSideInMeters) {
                NewCameraP.AbsTileY -= 9;
            }
#else
            NewCameraP = Stored->P;
#endif
        }

    }
}

internal bool32
TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY,
         real32 *tMin, real32 MinY, real32 MaxY) {
    bool32 Hit = false;

    real32 tEpsilon = 0.001f;
    if (PlayerDeltaX != 0.0f) {
        real32 tResult = (WallX - RelX) / PlayerDeltaX;
        real32 Y = RelY + tResult * PlayerDeltaY;
        if (tResult >= 0.0f && *tMin > tResult) {
            if (Y >= MinY && Y <= MaxY) {
                *tMin = Maximum(0.0f, tResult - tEpsilon);
                Hit = true;
            }
        }
    }

    return Hit;
}

internal void
MoveEntity(sim_region *SimRegion, sim_entity *Entity, real32 dt, move_spec *MoveSpec, v2 ddP) {
    world *World = GameState->World;

    if (MoveSpec->UnitMaxAccelVector) {
        real32 ddPLength = LengthSq(ddP);
        if (ddPLength > 1.0f) {
            ddP *= (1.0f / SquareRoot(ddPLength));
        }
    }

    ddP *= MoveSpec->Speed;

    // TODO: ODE here!
    ddP += -MoveSpec->Drag * Entity->dP;

    //v2 OldPlayerP = Entity->P;
    v2 PlayerDelta = (0.5f * ddP * Square(dt) + Entity->dP * dt);
    Entity->dP = ddP * dt + Entity->dP;
    //v2 NewPlayerP = OldPlayerP + PlayerDelta;

    for (uint32 Iteration = 0; Iteration < 4; ++Iteration) {
        real32 tMin = 1.0;
        v2 WallNormal = {};
        sim_entity *HitEntity = 0;

        v2 DesiredPosition = Entity->P + PlayerDelta;

        if (Entity->Collides) {
            // TODO: Spatial partition here!
            for (uint32 TestHighEntityIndex = 1; TestHighEntityIndex < SimRegion->EntityCount; TestHighEntityIndex++) {
                sim_entity *TestEntity = SimRegion->Entities + TestHighEntityIndex;
                if (Entity != TestEntity) {
                    if (TestEntity->Collides) {
                        real32 DiameterW = TestEntity->Width + Entity->Width;
                        real32 DiameterH = TestEntity->Height + Entity->Height;

                        v2 MinCorner = -0.5 * V2(DiameterW, DiameterH);
                        v2 MaxCorner = 0.5 * V2(DiameterW, DiameterH);

                        v2 Rel = Entity->P - TestEntity->P;

                        if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin,
                                    MinCorner.Y, MaxCorner.Y)) {
                            WallNormal = V2(-1, 0);
                            HitEntity = TestEntity;
                        }

                        if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin,
                                    MinCorner.Y, MaxCorner.Y)) {
                            WallNormal = V2(1, 0);
                            HitEntity = TestEntity;
                        }

                        if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin,
                                    MinCorner.X, MaxCorner.X)) {
                            WallNormal = V2(0, -1);
                            HitEntity = TestEntity;
                        }

                        if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin,
                                    MinCorner.X, MaxCorner.X)) {
                            WallNormal = V2(0, 1);
                            HitEntity = TestEntity;
                        }
                    }
                }
            }
        }

        Entity->P += tMin * PlayerDelta;

        if (HitEntity) {
            Entity->dP = Entity->dP - 1 * Inner(Entity->dP, WallNormal) * WallNormal;
            PlayerDelta = DesiredPosition - Entity->P;
            PlayerDelta = PlayerDelta - 1 * Inner(PlayerDelta, WallNormal) * WallNormal;

            // TODO: Stairs
            //Entity->AbsTileZ += HitLow->Sim.dAbsTileZ;
        } else {
            break;
        }
    }

    // TODO: Change to using the acceleration vector.
    if (Entity->dP.X == 0.0f && Entity->dP.Y == 0.0f) {
        // NOTE: Leave FacingDirection whatever it was
    } else if (AbsoluteValue(Entity->dP.X) > AbsoluteValue(Entity->dP.Y)) {
        if (Entity->dP.X > 0) {
            Entity->FacingDirection = 0;
        } else {
            Entity->FacingDirection = 2;
        }
    } else {
        if (Entity->dP.Y > 0) {
            Entity->FacingDirection = 1;
        } else {
            Entity->FacingDirection = 3;
        }
    }
}

