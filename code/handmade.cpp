#include "handmade.h"
#include "handmade_world.cpp"
#include "handmade_random.h"

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    int16 ToneVolumn = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0;
         SampleIndex < SoundBuffer->SampleCount;
         ++SampleIndex)
    {
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolumn);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

#if 0
        GameState->tSine += 2.0f * PI32 * 1.0f / (real32) WavePeriod;
        if (GameState->tSine > 2.0 * PI32) {
            GameState->tSine -= 2.0 * PI32;
        }
#endif
    }
}

internal void
DrawRectangle(game_offscreen_buffer *Buffer, v2 vMin, v2 vMax, real32 R, real32 G, real32 B) {
    int MinX = RoundReal32ToInt32(vMin.X);
    int MinY = RoundReal32ToInt32(vMin.Y);
    int MaxX = RoundReal32ToInt32(vMax.X);
    int MaxY = RoundReal32ToInt32(vMax.Y);

    if (MinX < 0) {
        MinX = 0;
    }

    if (MinY < 0) {
        MinY = 0;
    }

    if (MaxX > Buffer->Width) {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height) {
        MaxY = Buffer->Height;
    }

    // BIT PATTERN: 0x AA RR GG BB
    uint32 Color = (uint32)((RoundReal32ToUInt32(R * 255.0f) << 16) +
                            (RoundReal32ToUInt32(G * 255.0f) << 8) +
                            (RoundReal32ToUInt32(B * 255.0f) << 0));


    uint8 *Row = ((uint8 *) Buffer->Memory +
                    MinX * Buffer->BytesPerPixel +
                    MinY * Buffer->Pitch);

    for (int Y = MinY; Y < MaxY; ++Y) {
        uint32 *Pixel = (uint32 *) Row;
        for (int X = MinX; X < MaxX; ++X) {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

internal void
DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap,
           real32 RealX, real32 RealY, real32 CAlpha = 1.0) {
    int MinX = RoundReal32ToInt32(RealX);
    int MinY = RoundReal32ToInt32(RealY);
    int MaxX = MinX + Bitmap->Width;
    int MaxY = MinY + Bitmap->Height;

    int32 SourceOffsetX = 0;
    if (MinX < 0) {
        SourceOffsetX = -MinX;
        MinX = 0;
    }

    int32 SourceOffsetY = 0;
    if (MinY < 0) {
        SourceOffsetY = -MinY;
        MinY = 0;
    }

    if (MaxX > Buffer->Width) {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height) {
        MaxY = Buffer->Height;
    }

    // TODO: SourceRow needs to be changed based on clipping.
    uint32 *SourceRow = Bitmap->Pixels + Bitmap->Width * (Bitmap->Height - 1);
    SourceRow += -Bitmap->Width * SourceOffsetY + SourceOffsetX;
    uint8 *DestRow = ((uint8 *) Buffer->Memory +
                  MinX * Buffer->BytesPerPixel +
                  MinY * Buffer->Pitch);
    for (int32 Y = MinY; Y < MaxY; ++Y) {
        uint32 *Dest = (uint32 *) DestRow;
        uint32 *Source = SourceRow;
        for (int32 X = MinX; X < MaxX; ++X) {
            real32 A = (real32) ((*Source >> 24) & 0xFF) / 255.0f;
            A *= CAlpha;

            real32 SR = (real32) ((*Source >> 16) & 0xFF);
            real32 SG = (real32) ((*Source >> 8) & 0xFF);
            real32 SB = (real32) ((*Source >> 0) & 0xFF);

            real32 DR = (real32) ((*Dest >> 16) & 0xFF);
            real32 DG = (real32) ((*Dest >> 8) & 0xFF);
            real32 DB = (real32) ((*Dest >> 0) & 0xFF);

            // TODO: Someday, we need to talk about premultiplied alpha!
            // (this not premultiplied alpha)
            real32 R = (1 - A) * DR + A * SR;
            real32 G = (1 - A) * DG + A * SG;
            real32 B = (1 - A) * DB + A * SB;

            *Dest = (((uint32) (R + 0.5f) << 16) |
                     ((uint32) (G + 0.5f) << 8) |
                     ((uint32) (B + 0.5f) << 0));

            ++Dest;
            ++Source;
        }
        DestRow += Buffer->Pitch;
        SourceRow -= Bitmap->Width;
    }
}

#pragma pack(push, 1)
struct bitmap_header {
    uint16 FileType;
    uint32 FileSize;
    uint16 Reserved1;
    uint16 Reserved2;
    uint32 BitmapOffset;
    uint32 Size;
    int32 Width;
    int32 Height;
    uint16 Planes;
    uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile,
             const char *FileName) {
    loaded_bitmap Result = {};

    debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);

    if (ReadResult.ContentsSize != 0) {
        bitmap_header *Header = (bitmap_header *) ReadResult.Contents;
        uint32 *Pixels = (uint32 *) ((uint8 *) ReadResult.Contents + Header->BitmapOffset);
        Result.Pixels = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;

        Assert(Header->Compression == 3);

        // NOTE: If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EIGHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc ... DON"T think this
        // is complete BMP loading code because it isn't!)

        // NOTE: Byte order in memory is determined by the Header itself.
        // So we have to read out the masks and convert the pixels ourselves.
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShift = 16 - (int32) RedScan.Index;
        int32 GreenShift = 8 - (int32) GreenScan.Index;
        int32 BlueShift = 0 - (int32) BlueScan.Index;
        int32 AlphaShift = 24 - (int32) AlphaScan.Index;

        uint32 *SourceDest = Pixels;
        for (int32 Y = 0; Y < Header->Height; ++Y) {
            for (int32 X = 0; X < Header->Width; ++X) {
                uint32 C = *SourceDest;
                *SourceDest++ = (RotateLeft(C & RedMask, RedShift) |
                                 RotateLeft(C & GreenMask, GreenShift) |
                                 RotateLeft(C & BlueMask, BlueShift) |
                                 RotateLeft(C & AlphaMask, AlphaShift));
            }
        }
    }

    return Result;
}

internal void
MakeEntityLowFrequency(game_state *GameState, uint32 LowIndex) {
    low_entity *EntityLow = &GameState->LowEntities[LowIndex];
    uint32 HighIndex = EntityLow->HighEntityIndex;
    if (HighIndex) {
        uint32 LastHighIndex = GameState->HighEntityCount - 1;
        if (HighIndex != LastHighIndex) {
            high_entity *LastEntity = GameState->HighEntities_ + LastHighIndex;
            high_entity *DelEntity = GameState->HighEntities_ + HighIndex;

            *DelEntity = *LastEntity;
            GameState->LowEntities[LastEntity->LowEntityIndex].HighEntityIndex = HighIndex;
        }
        --GameState->HighEntityCount;
        EntityLow->HighEntityIndex = 0;
    }
}

inline v2
GetCameraSpaceP(game_state *GameState, low_entity *EntityLow) {
    // NOTE: Map the entity into camera space
    world_difference Diff = Subtract(GameState->World, &EntityLow->P, &GameState->CameraP);
    v2 Result = Diff.dXY;
    return Result;
}

inline high_entity *
MakeEntityHighFrequency(game_state *GameState, low_entity *EntityLow, uint32 LowIndex, v2 CameraSpaceP) {
    high_entity *EntityHigh = 0;
    Assert(EntityLow->HighEntityIndex == 0);
    if (EntityLow->HighEntityIndex == 0) {
        if (GameState->HighEntityCount < ArrayCount(GameState->HighEntities_)) {
            uint32 HighIndex = GameState->HighEntityCount++;
            EntityHigh = GameState->HighEntities_ + HighIndex;

            EntityHigh->P = CameraSpaceP;
            EntityHigh->dP = V2(0, 0);
            EntityHigh->ChunkZ = EntityLow->P.ChunkZ;
            EntityHigh->FacingDirection = 0;
            EntityHigh->LowEntityIndex = LowIndex;

            EntityLow->HighEntityIndex = HighIndex;
        } else {
            InvalidCodePath;
        }
    }

    return EntityHigh;
}

inline high_entity *
MakeEntityHighFrequency(game_state *GameState, uint32 LowIndex) {
    high_entity *EntityHigh = 0;
    low_entity *EntityLow = GameState->LowEntities + LowIndex;

    if (EntityLow->HighEntityIndex) {
        EntityHigh = GameState->HighEntities_ + EntityLow->HighEntityIndex;
    } else {
        v2 CameraSpaceP = GetCameraSpaceP(GameState, EntityLow);
        EntityHigh = MakeEntityHighFrequency(GameState, EntityLow, LowIndex, CameraSpaceP);
    }

    return EntityHigh;
}

inline low_entity *
GetLowEntity(game_state *GameState, uint32 Index) {
    low_entity *Result = 0;

    if ((Index > 0) && (Index < GameState->LowEntityCount)) {
        Result = GameState->LowEntities + Index;
    }

    return Result;
}

inline entity
ForceEntityIntoHigh(game_state *GameState, uint32 LowIndex) {
    entity Result = {};

    if ((LowIndex > 0) && (LowIndex < GameState->LowEntityCount)) {
        Result.LowIndex = LowIndex;
        Result.Low = GameState->LowEntities + LowIndex;
        Result.High = MakeEntityHighFrequency(GameState, LowIndex);
    }

    return Result;
}

inline bool32
ValidateEntityPairs(game_state *GameState) {
    bool32 Valid = true;
    for (uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount; ++HighEntityIndex) {
        high_entity *High = GameState->HighEntities_ + HighEntityIndex;
        Valid = Valid &&
                GameState->LowEntities[High->LowEntityIndex].HighEntityIndex == HighEntityIndex;
    }
    return Valid;
}

internal void
OffsetAndCheckFrequencyByArea(game_state *GameState, v2 Offset, rectangle2 HighFrequencyBounds) {
    for (uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount;) {
        high_entity *High = GameState->HighEntities_ + HighEntityIndex;
        low_entity *Low = GameState->LowEntities + High->LowEntityIndex;

        High->P += Offset;
        if (IsValid(Low->P) && IsInRectange(HighFrequencyBounds, High->P)) {
            ++HighEntityIndex;
        } else {
            Assert(Low->HighEntityIndex == HighEntityIndex);
            MakeEntityLowFrequency(GameState, High->LowEntityIndex);
        }
    }
}

struct add_low_entity_result {
    low_entity *Low;
    uint32 LowIndex;
};

internal add_low_entity_result
AddLowEntity(game_state *GameState, entity_type Type, world_position *P) {
    Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));
    uint32 EntityIndex = GameState->LowEntityCount++;

    low_entity *EntityLow = GameState->LowEntities + EntityIndex;
    *EntityLow = {};
    EntityLow->Type = Type;

    ChangeEntityLocation(&GameState->WorldArena, GameState->World, EntityIndex, EntityLow, 0, P);

    add_low_entity_result Result;
    Result.Low = EntityLow;
    Result.LowIndex = EntityIndex;

    // TODO: Do we need to have a begin/end papradigm for adding
    // entities so that they can be brought into the high set when they
    // are added and are in the caera region?

    return Result;
}

internal add_low_entity_result
AddWall(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position P = ChunkPositionFromTilePosition(GameState->World,
                                                     AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Wall, &P);

    Entity.Low->Height = GameState->World->TileSideInMeters;
    Entity.Low->Width = Entity.Low->Height;
    Entity.Low->Collides = true;

    return Entity;
}

internal void
InitHitPoint(low_entity *EntityLow, uint32 HitPointCount) {
    Assert(HitPointCount < ArrayCount(EntityLow->HitPoints));
    EntityLow->HitPointMax = HitPointCount;
    for (uint32 HitPointIndex = 0; HitPointIndex < EntityLow->HitPointMax; ++HitPointIndex) {
        hit_point *HitPoint = EntityLow->HitPoints + HitPointIndex;
        HitPoint->Flags = 0;
        HitPoint->FilledAmount = HIT_POINT_SUB_COUNT;
    }
}

internal add_low_entity_result
AddSword(game_state *GameState) {
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Sword, 0);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = false;

    return Entity;
}

internal add_low_entity_result
AddPlayer(game_state *GameState) {
    world_position P = GameState->CameraP;
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Hero, &P);

    Entity.Low->Height = 0.5f; //1.4f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = true;

    InitHitPoint(Entity.Low, 3);

    add_low_entity_result Sword = AddSword(GameState);
    Entity.Low->SwordLowIndex = Sword.LowIndex;

    if (GameState->CameraFollowingEntityIndex == 0) {
        GameState->CameraFollowingEntityIndex = Entity.LowIndex;
    }

    return Entity;
}

internal add_low_entity_result
AddMonstar(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position P = ChunkPositionFromTilePosition(GameState->World,
                                                     AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Monstar, &P);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = true;

    InitHitPoint(Entity.Low, 3);

    return Entity;
}

internal add_low_entity_result
AddFamiliar(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ) {
    world_position P = ChunkPositionFromTilePosition(GameState->World,
                                                     AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Familiar, &P);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = false;

    return Entity;
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

struct move_spec {
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

inline move_spec
DefaultMoveSpec(void) {
    move_spec Result;

    Result.UnitMaxAccelVector = false;
    Result.Speed = 1.0f;
    Result.Drag = 0.0f;

    return Result;
}

internal void
MoveEntity(game_state *GameState, entity Entity, real32 dt, move_spec *MoveSpec, v2 ddP) {
    world *World = GameState->World;

    if (MoveSpec->UnitMaxAccelVector) {
        real32 ddPLength = LengthSq(ddP);
        if (ddPLength > 1.0f) {
            ddP *= (1.0f / SquareRoot(ddPLength));
        }
    }

    ddP *= MoveSpec->Speed;

    // TODO: ODE here!
    ddP += -MoveSpec->Drag * Entity.High->dP;

    //v2 OldPlayerP = Entity.High->P;
    v2 PlayerDelta = (0.5f * ddP * Square(dt) + Entity.High->dP * dt);
    Entity.High->dP = ddP * dt + Entity.High->dP;
    //v2 NewPlayerP = OldPlayerP + PlayerDelta;

    /*
    uint32 MinTileX = Minimum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
    uint32 MinTileY = Minimum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);
    uint32 MaxTileX = Maximum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX) + 1;
    uint32 MaxTileY = Maximum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY) + 1;

    uint32 EntityTileWidth = CeilReal32ToInt32(Entity.High->Width / World->TileSideInMeters);
    uint32 EntityTileHeight = CeilReal32ToInt32(Entity.High->Height / World->TileSideInMeters);

    MinTileX -= EntityTileWidth;
    MinTileY -= EntityTileHeight;
    MaxTileX += EntityTileWidth;
    MaxTileY += EntityTileHeight;

    uint32 AbsTileZ = Entity.High->P.AbsTileZ;
    */

    for (uint32 Iteration = 0; Iteration < 4; ++Iteration) {
        real32 tMin = 1.0;
        v2 WallNormal = {};
        uint32 HitHighEntityIndex = 0;

        v2 DesiredPosition = Entity.High->P + PlayerDelta;

        if (Entity.Low->Collides) {
            for (uint32 TestHighEntityIndex = 1; TestHighEntityIndex < GameState->HighEntityCount; TestHighEntityIndex++) {
                if (TestHighEntityIndex != Entity.Low->HighEntityIndex) {
                    entity TestEntity;
                    TestEntity.High = GameState->HighEntities_ + TestHighEntityIndex;
                    TestEntity.LowIndex = TestEntity.High->LowEntityIndex;
                    TestEntity.Low = GameState->LowEntities + TestEntity.LowIndex;
                    if (TestEntity.Low->Collides) {
                        real32 DiameterW = TestEntity.Low->Width + Entity.Low->Width;
                        real32 DiameterH = TestEntity.Low->Height + Entity.Low->Height;

                        v2 MinCorner = -0.5 * V2(DiameterW, DiameterH);
                        v2 MaxCorner = 0.5 * V2(DiameterW, DiameterH);

                        v2 Rel = Entity.High->P - TestEntity.High->P;

                        if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin,
                                    MinCorner.Y, MaxCorner.Y)) {
                            WallNormal = V2(-1, 0);
                            HitHighEntityIndex = TestHighEntityIndex;
                        }

                        if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin,
                                    MinCorner.Y, MaxCorner.Y)) {
                            WallNormal = V2(1, 0);
                            HitHighEntityIndex = TestHighEntityIndex;
                        }

                        if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin,
                                    MinCorner.X, MaxCorner.X)) {
                            WallNormal = V2(0, -1);
                            HitHighEntityIndex = TestHighEntityIndex;
                        }

                        if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin,
                                    MinCorner.X, MaxCorner.X)) {
                            WallNormal = V2(0, 1);
                            HitHighEntityIndex = TestHighEntityIndex;
                        }
                    }
                }
            }
        }

        Entity.High->P += tMin * PlayerDelta;

        if (HitHighEntityIndex) {
            Entity.High->dP = Entity.High->dP - 1 * Inner(Entity.High->dP, WallNormal) * WallNormal;
            PlayerDelta = DesiredPosition - Entity.High->P;
            PlayerDelta = PlayerDelta - 1 * Inner(PlayerDelta, WallNormal) * WallNormal;

            high_entity *HitHigh = GameState->HighEntities_ + HitHighEntityIndex;
            low_entity *HitLow = GameState->LowEntities + HitHigh->LowEntityIndex;
            // TODO: Stairs
            //Entity.High->AbsTileZ += HitLow->dAbsTileZ;
        } else {
            break;
        }
    }

    // TODO: Change to using the acceleration vector.
    if (Entity.High->dP.X == 0.0f && Entity.High->dP.Y == 0.0f) {
        // NOTE: Leave FacingDirection whatever it was
    } else if (AbsoluteValue(Entity.High->dP.X) > AbsoluteValue(Entity.High->dP.Y)) {
        if (Entity.High->dP.X > 0) {
            Entity.High->FacingDirection = 0;
        } else {
            Entity.High->FacingDirection = 2;
        }
    } else {
        if (Entity.High->dP.Y > 0) {
            Entity.High->FacingDirection = 1;
        } else {
            Entity.High->FacingDirection = 3;
        }
    }

    world_position NewP = MapIntoChunkSpace(GameState->World, GameState->CameraP, Entity.High->P);
    ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity.LowIndex,
                         Entity.Low, &Entity.Low->P, &NewP);
}

internal void
SetCamera(game_state *GameState, world_position NewCameraP) {
    world *World = GameState->World;

    Assert(ValidateEntityPairs(GameState));

    world_difference dCameraP = Subtract(World, &NewCameraP, &GameState->CameraP);
    GameState->CameraP = NewCameraP;

    // TODO: I am totally picking these numbers randomly!
    uint32 TileSpanX = 17 * 3;
    uint32 TileSpanY = 9 * 3;
    rectangle2 CameraBounds = RectCenterHalfDim(V2(0, 0),
                                                World->TileSideInMeters * V2((real32) TileSpanX, (real32) TileSpanY));
    v2 EntityOffsetForFrame = -dCameraP.dXY;
    OffsetAndCheckFrequencyByArea(GameState, EntityOffsetForFrame, CameraBounds);

    // TODO: Do this in terms of tile chunks!
    world_position MinChunkP = MapIntoChunkSpace(World, NewCameraP, GetMinCornor(CameraBounds));
    world_position MaxChunkP = MapIntoChunkSpace(World, NewCameraP, GetMaxCornor(CameraBounds));

    for (int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ++ChunkY) {
        for (int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ++ChunkX) {
            world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, NewCameraP.ChunkZ);
            if (Chunk) {
                for (world_entity_block *Block = &Chunk->FirstBlock; Block; Block = Block->Next) {
                    for (uint32 EntityIndexIndex = 0; EntityIndexIndex < Block->EntityCount; ++EntityIndexIndex) {
                        uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndexIndex];
                        low_entity *Low = GameState->LowEntities + LowEntityIndex;
                        if (Low->HighEntityIndex == 0) {
                            v2 CameraSpaceP = GetCameraSpaceP(GameState, Low);

                            if (IsInRectange(CameraBounds, CameraSpaceP)) {
                                MakeEntityHighFrequency(GameState, Low, LowEntityIndex, CameraSpaceP);
                            }
                        }
                    }
                }
            }
        }
    }

    Assert(ValidateEntityPairs(GameState));
}

inline void
PushPiece(entity_visible_piece_group *Group, loaded_bitmap *Bitmap,
          v2 Offset, real32 OffsetZ, v2 Align, v2 Dim, v4 Color, real32 EntityZC) {
    Assert(Group->PieceCount < ArrayCount(Group->Pieces));
    entity_visible_piece *Piece = Group->Pieces + Group->PieceCount++;
    Piece->Bitmap = Bitmap;
    Piece->Offset = Group->GameState->MetersToPixels * V2(Offset.X, -Offset.Y) - Align;
    Piece->OffsetZ = Group->GameState->MetersToPixels * OffsetZ;
    Piece->EntityZC = EntityZC;
    Piece->R = Color.R;
    Piece->G = Color.G;
    Piece->B = Color.B;
    Piece->A = Color.A;
    Piece->Dim = Dim;
}

inline void
PushBitmap(entity_visible_piece_group *Group, loaded_bitmap *Bitmap,
          v2 Offset, real32 OffsetZ, v2 Align, real32 Alpha = 1.0f, real32 EntityZC = 1.0f) {
    PushPiece(Group, Bitmap, Offset, OffsetZ, Align, V2(0, 0), V4(1.0f, 1.0f, 1.0f, Alpha), EntityZC);
}

inline void
PushRect(entity_visible_piece_group *Group, v2 Offset, real32 OffsetZ,
          v2 Dim, v4 Color, real32 EntityZC = 1.0f) {
    PushPiece(Group, 0, Offset, OffsetZ, V2(0, 0), Dim, Color, EntityZC);
}

inline entity
EntityFromHighIndex(game_state *GameState, uint32 HighEntityIndex) {
    entity Result = {};

    if (HighEntityIndex) {
        Assert(HighEntityIndex < ArrayCount(GameState->HighEntities_));
        Result.High = GameState->HighEntities_ + HighEntityIndex;
        Result.LowIndex = Result.High->LowEntityIndex;
        Result.Low = GameState->LowEntities + Result.LowIndex;
    }

    return Result;
}

inline void
UpdateFamiliar(game_state *GameState, entity Entity, real32 dt) {
    entity ClosestHero = {};
    real32 ClosestHeroDSq = Square(10.0f); // NOTE: Ten meter maximu search!
    for (uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount;
         ++HighEntityIndex) {
        entity TestEntity = EntityFromHighIndex(GameState, HighEntityIndex);

        if (TestEntity.Low->Type == EntityType_Hero) {
            real32 TestDSq = LengthSq(TestEntity.High->P - Entity.High->P);
            if (ClosestHeroDSq > TestDSq) {
                ClosestHero = TestEntity;
                ClosestHeroDSq = TestDSq;
            }
        }
    }

    v2 ddP = {};
    if (ClosestHero.High && ClosestHeroDSq > Square(3.0f)) {
        // TODO: PULL SPEED OUT OF MOVE ENTITY
        real32 Acceleration = 0.5f;
        real32 OneOverLength = Acceleration / SquareRoot(ClosestHeroDSq);
        ddP = OneOverLength * (ClosestHero.High->P - Entity.High->P);
    }


    move_spec MoveSpec = DefaultMoveSpec();
    MoveSpec.UnitMaxAccelVector = true;
    MoveSpec.Speed = 50.0f;
    MoveSpec.Drag = 8.0f;
    MoveEntity(GameState, Entity, dt, &MoveSpec, ddP);
}

inline void
UpdateMonstar(game_state *GameState, entity Entity, real32 dt) {
}

inline void
UpdateSword(game_state *GameState, entity Entity, real32 dt) {
    move_spec MoveSpec = DefaultMoveSpec();
    MoveSpec.UnitMaxAccelVector = false;
    MoveSpec.Speed = 0.0f;
    MoveSpec.Drag = 0.0f;

    v2 OldP = Entity.High->P;
    MoveEntity(GameState, Entity, dt, &MoveSpec, V2(0, 0));
    real32 DistanceTraveled = Length(Entity.High->P - OldP);

    Entity.Low->DistanceRemaining -= DistanceTraveled;
    if (Entity.Low->DistanceRemaining < 0.0f) {
        ChangeEntityLocation(&GameState->WorldArena, GameState->World,
                             Entity.LowIndex, Entity.Low, &Entity.Low->P, 0);
    }
}

internal void
DrawHitPoints(low_entity *LowEntity, entity_visible_piece_group *PieceGroup) {
    if (LowEntity->HitPointMax >= 1) {
        v2 HealthDim = {0.2f, 0.2f};
        real32 SpacingX = 1.5f * HealthDim.X;
        v2 HitP = {-0.5f * (LowEntity->HitPointMax - 1) * SpacingX, -0.25f};
        v2 dHitP = {SpacingX, 0.0f};

        for (uint32 HealthIndex = 0; HealthIndex < LowEntity->HitPointMax; ++HealthIndex) {
            hit_point *HitPoint = LowEntity->HitPoints + HealthIndex;
            v4 Color = {1.0f, 0.0f, 0.0f, 1.0f};
            if (HitPoint->FilledAmount == 0) {
                Color = {0.2f, 0.2f, 0.2f, 1.0f};
            }
            PushRect(PieceGroup, HitP, 0, HealthDim, Color, 0.0f);
            HitP += dHitP;
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           ArrayCount(Input->Controllers[0].Buttons));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *) Memory->PermanentStorage;
    if (!Memory->IsInitialized) {
        // NOTE: Reserve entity slot 0 for null entity
        AddLowEntity(GameState, EntityType_Null, 0);
        GameState->HighEntityCount = 1;

        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
        GameState->Shadow =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_shadow.bmp");
        GameState->Tree =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/tree00.bmp");
        GameState->Sword =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test2/rock03.bmp");

        hero_bitmaps *Bitmap;

        Bitmap = GameState->HeroBitmaps;
        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
        Bitmap->Align = V2(72, 182);
        Bitmap++;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
        Bitmap->Align = V2(72, 182);
        Bitmap++;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
        Bitmap->Align = V2(72, 182);
        Bitmap++;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
        Bitmap->Align = V2(72, 182);

        uint32 RandomNumberIndex = 0;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *) Memory->PermanentStorage + sizeof(game_state));

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;
        InitializeWorld(World, 1.4f);

        int32 TileSideInPixels = 60;
        GameState->MetersToPixels = (real32) TileSideInPixels / (real32) World->TileSideInMeters;

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;

        uint32 ScreenBaseX = 0;
        uint32 ScreenBaseY = 0;
        uint32 ScreenBaseZ = 0;
        uint32 ScreenX = ScreenBaseX;
        uint32 ScreenY = ScreenBaseY;
        uint32 AbsTileZ = ScreenBaseZ;

        // TODO: Replace all this with real world generation!
        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        for (uint32 ScreenIndex = 0; ScreenIndex < 1000; ++ScreenIndex) {
            // TODO: Random number generator!
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
            uint32 RandomChoice;
//            if (DoorUp || DoorDown) {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
#if 0
            } else {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
            }
#endif

            bool32 CreatedZDoor = false;
            if (RandomChoice == 2) {
                CreatedZDoor = true;
                if (AbsTileZ == ScreenBaseZ) {
                    DoorUp = true;
                } else {
                    DoorDown = true;
                }
            } else if (RandomChoice == 1) {
                DoorRight = true;
            } else {
                DoorTop = true;
            }

            for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY) {
                for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX) {
                    uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

                    uint32 TileValue = 1;
                    if (TileX == 0 && (TileY != (TilesPerHeight / 2) || !DoorLeft)) {
                        TileValue = 2;
                    }

                    if (TileX == (TilesPerWidth - 1) && (TileY != (TilesPerHeight / 2) || !DoorRight)) {
                        TileValue = 2;
                    }

                    if (TileY == 0 && (TileX != (TilesPerWidth / 2) || !DoorBottom)) {
                        TileValue = 2;
                    }

                    if (TileY == (TilesPerHeight - 1) && (TileX != (TilesPerWidth / 2) || !DoorTop)) {
                        TileValue = 2;
                    }

                    if (TileX == 10 && TileY == 6) {
                        if (DoorUp) {
                            TileValue = 3;
                        }
                        if (DoorDown) {
                            TileValue = 4;
                        }
                    }

                    if (TileValue == 2) {
                        AddWall(GameState, AbsTileX, AbsTileY, AbsTileZ);
                    }
                }
            }

            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            if (CreatedZDoor) {
                DoorDown = !DoorDown;
                DoorUp = !DoorUp;
            } else {
                DoorDown = false;
                DoorUp = false;
            }

            DoorRight = false;
            DoorTop = false;

            if (RandomChoice == 2) {
                if (AbsTileZ == ScreenBaseZ) {
                    AbsTileZ = ScreenBaseZ + 1;
                } else {
                    AbsTileZ = ScreenBaseZ;
                }
            } else if (RandomChoice == 1) {
                ScreenX += 1;
            } else {
                ScreenY += 1;
            }
        }

        world_position NewCameraP = {};
        uint32 CameraTileX = ScreenBaseX * TilesPerWidth + 17 / 2;
        uint32 CameraTileY = ScreenBaseY * TilesPerHeight + 9 / 2;
        uint32 CameraTileZ = ScreenBaseZ;
        NewCameraP = ChunkPositionFromTilePosition(GameState->World,
                                                   CameraTileX,
                                                   CameraTileY,
                                                   CameraTileZ);
        AddMonstar(GameState, CameraTileX + 2, CameraTileY + 2, CameraTileZ);
        for (int FamiliarIndex = 0; FamiliarIndex < 1; ++FamiliarIndex) {
            int32 FamiliarOffsetX = RandomNumberTable[RandomNumberIndex++] % 10 - 7;
            int32 FamiliarOffsetY = RandomNumberTable[RandomNumberIndex++] % 10 - 3;
            if (FamiliarOffsetX != 0 || FamiliarOffsetY != 0) {
                AddFamiliar(GameState, CameraTileX + FamiliarOffsetX, CameraTileY + FamiliarOffsetY, CameraTileZ);
            }
        }
        SetCamera(GameState, NewCameraP);

        Memory->IsInitialized = true;
    }

    world *World = GameState->World;

    real32 MetersToPixels = GameState->MetersToPixels;
    //
    // NOTE:
    //
    for (size_t ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        uint32 LowIndex = GameState->PlayerIndexForController[ControllerIndex];
        if (LowIndex == 0) {
            if (Controller->Start.EndedDown) {
                uint32 EntityIndex = AddPlayer(GameState).LowIndex;
                GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
            }
        } else {
            entity ControllingEntity = ForceEntityIntoHigh(GameState, LowIndex);
            v2 ddP = {};

            if (Controller->IsAnalog) {
                ddP = V2(Controller->StickAverageX, Controller->StickAverageY);
            } else {
                if (Controller->MoveUp.EndedDown) {
                    ddP.Y = 1.0f;
                }
                if (Controller->MoveDown.EndedDown) {
                    ddP.Y = -1.0f;
                }
                if (Controller->MoveLeft.EndedDown) {
                    ddP.X = -1.0f;
                }
                if (Controller->MoveRight.EndedDown) {
                    ddP.X = 1.0f;
                }
            }

            if (Controller->Start.EndedDown) {
                ControllingEntity.High->dZ = 3.0f;
            }

            v2 dSword = {};
            if (Controller->ActionUp.EndedDown) {
                dSword = V2(0.0f, 1.0f);
            }

            if (Controller->ActionDown.EndedDown) {
                dSword = V2(0.0f, -1.0f);
            }

            if (Controller->ActionLeft.EndedDown) {
                dSword = V2(-1.0f, 0.0f);
            }

            if (Controller->ActionRight.EndedDown) {
                dSword = V2(1.0f, 0.0f);
            }

            // TODO: Now that we have some real usage examples, let's solidify
            // the positioning system!

            move_spec MoveSpec = DefaultMoveSpec();
            MoveSpec.UnitMaxAccelVector = true;
            MoveSpec.Speed = 50.0f;
            MoveSpec.Drag = 8.0f;
            MoveEntity(GameState, ControllingEntity, Input->dtForFrame, &MoveSpec, ddP);
            if (dSword.X != 0.0f || dSword.Y != 0.0f) {
                low_entity *LowSword = GetLowEntity(GameState, ControllingEntity.Low->SwordLowIndex);
                if (LowSword && !IsValid(LowSword->P)) {
                    world_position SwordP = ControllingEntity.Low->P;
                    ChangeEntityLocation(&GameState->WorldArena, GameState->World,
                                         ControllingEntity.Low->SwordLowIndex, LowSword,
                                         0, &SwordP);
                    entity Sword = ForceEntityIntoHigh(GameState, ControllingEntity.Low->SwordLowIndex);

                    Sword.Low->DistanceRemaining = 5.0f;
                    Sword.High->dP = 5.0f * dSword;
                }
            }
        }
    }

    //
    // NOTE: Update camera/player Z based on last movement.
    //
    entity CameraFollowingEntity = ForceEntityIntoHigh(GameState, GameState->CameraFollowingEntityIndex);
    if (CameraFollowingEntity.High) {
        world_position NewCameraP = GameState->CameraP;

        NewCameraP.ChunkZ = CameraFollowingEntity.Low->P.ChunkZ;

#if 0
        if (CameraFollowingEntity.High->P.X > 9.0f * World->TileSideInMeters) {
            NewCameraP.AbsTileX += 17;
        }

        if (CameraFollowingEntity.High->P.X < -9.0f * World->TileSideInMeters) {
            NewCameraP.AbsTileX -= 17;
        }

        if (CameraFollowingEntity.High->P.Y > 5.0f * World->TileSideInMeters) {
            NewCameraP.AbsTileY += 9;
        }

        if (CameraFollowingEntity.High->P.Y < -5.0f * World->TileSideInMeters) {
            NewCameraP.AbsTileY -= 9;
        }
#else
         NewCameraP = CameraFollowingEntity.Low->P;
#endif

        // TODO: Map new entities in and old entities out
        // TODO: Mapping tiles and stairs into the entity set!
        SetCamera(GameState, NewCameraP);
    }

    //
    // NOTE: Render
    //
#if 1
    DrawRectangle(Buffer, V2(0.0f, 0.0f), V2(Buffer->Width, Buffer->Height), 0.5f, 0.5f, 0.5f);
#else
    DrawBitmap(Buffer, &GameState->Backdrop, 0, 0);
#endif

    real32 ScreenCenterX = 0.5f * (real32) Buffer->Width;
    real32 ScreenCenterY = 0.5f * (real32) Buffer->Height;

    entity_visible_piece_group PieceGroup;
    PieceGroup.GameState = GameState;
    for (uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount; ++HighEntityIndex) {
        PieceGroup.PieceCount = 0;

        high_entity *HighEntity = GameState->HighEntities_ + HighEntityIndex;
        low_entity *LowEntity = GameState->LowEntities + HighEntity->LowEntityIndex;

        entity Entity;
        Entity.LowIndex = HighEntity->LowEntityIndex;
        Entity.Low = LowEntity;
        Entity.High = HighEntity;
        //UpdateEntity(GameState, Entity, dt);

        real32 dt = Input->dtForFrame;

        // TODO: This is incorrect, should be computed after update!!!
        real32 ShadowAlpha = 1.0f - 0.5f * HighEntity->Z;
        if (ShadowAlpha < 0) {
            ShadowAlpha = 0.0f;
        }

        hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[HighEntity->FacingDirection];
        switch (LowEntity->Type) {
            case EntityType_Hero: {
                // TODO: Z!!!
                PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
                PushBitmap(&PieceGroup, &HeroBitmaps->Torso, V2(0, 0), 0, HeroBitmaps->Align);
                PushBitmap(&PieceGroup, &HeroBitmaps->Cape, V2(0, 0), 0, HeroBitmaps->Align);
                PushBitmap(&PieceGroup, &HeroBitmaps->Head, V2(0, 0), 0, HeroBitmaps->Align);

                DrawHitPoints(LowEntity, &PieceGroup);
            } break;

            case EntityType_Wall: {
                PushBitmap(&PieceGroup, &GameState->Tree, V2(0, 0), 0, V2(40, 80));
            } break;

            case EntityType_Sword: {
                UpdateSword(GameState, Entity, dt);
                PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
                PushBitmap(&PieceGroup, &GameState->Sword, V2(0, 0), 0, V2(29, 10));
            } break;

            case EntityType_Familiar: {
                UpdateFamiliar(GameState, Entity, dt);
                Entity.High->tBob += dt;
                if (Entity.High->tBob > 2.0f * PI32) {
                    Entity.High->tBob -= 2.0f * PI32;
                }
                real32 BobSin = Sin(2.0f * Entity.High->tBob);
                PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, (0.5f * ShadowAlpha) + 0.2f * BobSin, 0.0f);
                PushBitmap(&PieceGroup, &HeroBitmaps->Head, V2(0, 0), 0.2f * BobSin, HeroBitmaps->Align);
            } break;

            case EntityType_Monstar: {
                UpdateMonstar(GameState, Entity, dt);
                PushBitmap(&PieceGroup, &GameState->Shadow, V2(0, 0), 0, HeroBitmaps->Align, ShadowAlpha, 0.0f);
                PushBitmap(&PieceGroup, &HeroBitmaps->Torso, V2(0, 0), 0, HeroBitmaps->Align);

                DrawHitPoints(LowEntity, &PieceGroup);
            } break;

            default: {
                InvalidCodePath;
            } break;
        }

        real32 ddZ = -9.8;
        HighEntity->Z += 0.5f * ddZ * Square(dt) + HighEntity->dZ * dt;
        HighEntity->dZ = ddZ * dt + HighEntity->dZ;
        if (HighEntity->Z < 0) {
            HighEntity->Z = 0;
        }

        real32 EntityGroundPointX = ScreenCenterX + MetersToPixels * HighEntity->P.X;
        real32 EntityGroundPointY = ScreenCenterY - MetersToPixels * HighEntity->P.Y;
        real32 EntityZ = -MetersToPixels * HighEntity->Z;
#if 0
        v2 PlayerLeftTop = {PlayerGroundPointX - 0.5f * MetersToPixels * LowEntity->Width,
            PlayerGroundPointY - 0.5f * MetersToPixels * LowEntity->Height};
        v2 EntityWidthHeight = {MetersToPixels * LowEntity->Width,
            MetersToPixels * LowEntity->Height};
        DrawRectangle(Buffer, PlayerLeftTop, PlayerLeftTop + 0.9f * EntityWidthHeight,
                1.0f, 1.0f, 0.0f);
#endif
        for (uint32 PieceIndex = 0; PieceIndex < PieceGroup.PieceCount; ++PieceIndex) {
            entity_visible_piece *Piece = PieceGroup.Pieces + PieceIndex;
            v2 Center = {EntityGroundPointX + Piece->Offset.X,
                         EntityGroundPointY + Piece->Offset.Y + Piece->OffsetZ + Piece->EntityZC * EntityZ};
            if (Piece->Bitmap) {
                DrawBitmap(Buffer, Piece->Bitmap, Center.X, Center.Y, Piece->A);
            } else {
                v2 HalfDim = 0.5f * MetersToPixels * Piece->Dim;
                DrawRectangle(Buffer, Center - HalfDim, Center + HalfDim, Piece->R, Piece->G, Piece->B);
            }
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state *GameState = (game_state *) Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}
