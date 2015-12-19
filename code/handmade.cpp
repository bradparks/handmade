#include "handmade.h"
#include "handmade_tile.cpp"
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
           real32 RealX, real32 RealY,
           int32 AlignX = 0, int32 AlignY = 0) {
    RealX -= (real32) AlignX;
    RealY -= (real32) AlignY;

    int MinX = RoundReal32ToInt32(RealX);
    int MinY = RoundReal32ToInt32(RealY);
    int MaxX = RoundReal32ToInt32(RealX + (real32) Bitmap->Width);
    int MaxY = RoundReal32ToInt32(RealY + (real32) Bitmap->Height);

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

inline entity *
GetEntity(game_state *GameState, uint32 Index) {
    entity *Entity = 0;


    if ((Index > 0) && (Index < ArrayCount(GameState->Entities))) {
        Entity = &GameState->Entities[Index];
    }

    return Entity;
}

internal void
InitializePlayer(game_state *GameState, uint32 EntityIndex) {
    entity *Entity = GetEntity(GameState, EntityIndex);

    Entity->Exists = true;
    Entity->P.AbsTileX = 1;
    Entity->P.AbsTileY = 3;
    Entity->P.Offset.X = 5.0f;
    Entity->P.Offset.Y = 5.0f;
    Entity->Height = 1.4f;
    Entity->Width = 0.75f * Entity->Height;

    if (!GetEntity(GameState, GameState->CameraFollowingEntityIndex)) {
        GameState->CameraFollowingEntityIndex = EntityIndex;
    }
}

internal uint32
AddEntity(game_state *GameState) {
    uint32 EntityIndex = GameState->EntityCount++;
    Assert(GameState->EntityCount < ArrayCount(GameState->Entities));
    entity *Entity = &GameState->Entities[EntityIndex];
    *Entity = {};
    return EntityIndex;
}

internal void
MovePlayer(game_state *GameState, entity *Entity, real32 dt, v2 ddP) {
    tile_map *TileMap = GameState->World->TileMap;

    if (ddP.X != 0.0f && ddP.Y != 0.0f) {
        ddP.X *= 0.707106781187f;
        ddP.Y *= 0.707106781187f;
    }

    real32 PlayerSpeed = 50.0f; // m/s^2

    ddP *= PlayerSpeed;

    // TODO: ODE here!
    ddP += -8.0f * Entity->dP;

    tile_map_position OldPlayerP = Entity->P;
    tile_map_position NewPlayerP = OldPlayerP;
    v2 PlayerDelta= (0.5f * ddP * Square(dt) +
                     Entity->dP * dt);
    NewPlayerP.Offset += PlayerDelta;
    Entity->dP = ddP * dt + Entity->dP;
    NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);
    // TODO: Delta function that auto-recanonicalizes
#if 1
    tile_map_position PlayerLeft = NewPlayerP;
    PlayerLeft.Offset.X -= 0.5f * Entity->Width;
    PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

    tile_map_position PlayerRight = NewPlayerP;
    PlayerRight.Offset.X += 0.5f * Entity->Width;
    PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

    bool32 Collided = false;
    tile_map_position ColP = {};

    if (!IsTileMapPointEmpty(TileMap, NewPlayerP)) {
        ColP = NewPlayerP;
        Collided = true;
    }

    if (!IsTileMapPointEmpty(TileMap, PlayerLeft)) {
        ColP = PlayerLeft;
        Collided = true;
    }

    if (!IsTileMapPointEmpty(TileMap, PlayerRight)) {
        ColP = PlayerRight;
        Collided = true;
    }

    if (Collided) {
        v2 r = {1, 0};

        if (ColP.AbsTileX < Entity->P.AbsTileX) {
            r = v2{1, 0};
        }

        if (ColP.AbsTileX > Entity->P.AbsTileX) {
            r = v2{-1, 0};
        }

        if (ColP.AbsTileY < Entity->P.AbsTileY) {
            r = v2{0, 1};
        }

        if (ColP.AbsTileY > Entity->P.AbsTileY) {
            r = v2{0, -1};
        }

        Entity->dP = Entity->dP - 1 * Inner(Entity->dP, r) * r;
    } else {
        Entity->P = NewPlayerP;
    }
#else
    uint32 MinTileX = 0;
    uint32 MinTileY = 0;
    uint32 OnePastMaxTileX = 0;
    uint32 OnePastMaxTileY = 0;
    uint32 AbsTileZ = Entity->P.AbsTileZ;
    v2 BestPoint = Entity->P;
    real32 BestDistanceSq = LengthSq(PlayerDelta);
    for (uint32 AbsTileY = MinTileY; AbsTileY != OnePastMaxTileY; ++AbsTileY) {
        for (uint32 AbsTileX = MinTileX; AbsTileX != OnePastMaxTileX; ++AbsTileX) {
            uint32 TileValue = GetTileValue(TileMap, AbsTileX, AbsTileY, AbsTileZ);
            tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
            if (IsTileValueEmpty(TileValue)) {
                v2 MinCorner = -0.5 * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
                v2 MaxCorner = 0.5 * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};

                tile_map_difference RelNewPlayerP = Subtract(TileMap, &TestTileP, &NewPlayerP);
                v2 TestP = ClosestPointInRectangle(MinCorner, MaxCorner, RelNewPlayerP);
                TestDistanceSq = ;
                if (BestDistanceSq > TestDistanceSq) {
                    BestPlayerP = ;
                    BestDistanceSq = ;
                }
            }
        }
    }
#endif

    if (!AreOnSameTile(&OldPlayerP, &Entity->P)) {
        uint32 NewTileValue = GetTileValue(TileMap, Entity->P);

        if (NewTileValue == 3) {
            ++Entity->P.AbsTileZ;
        } else if (NewTileValue == 4) {
            --Entity->P.AbsTileZ;
        }
    }
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           ArrayCount(Input->Controllers[0].Buttons));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *) Memory->PermanentStorage;
    if (!Memory->IsInitialized) {
        // NOTE: Reserve entity slot 0 for null entity
        AddEntity(GameState);

        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");

        hero_bitmaps *Bitmap;

        Bitmap = GameState->HeroBitmaps;
        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        Bitmap++;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        Bitmap++;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        Bitmap++;

        Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
        Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
        Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;

        GameState->CameraP.AbsTileX = 17 / 2;
        GameState->CameraP.AbsTileY = 9 / 2;

        uint32 RandomNumberIndex = 0;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *) Memory->PermanentStorage + sizeof(game_state));

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;
        World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

        tile_map *TileMap = World->TileMap;

        TileMap->ChunkShift = 4;
        TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim = (1 << TileMap->ChunkShift);

        TileMap->TileChunkCountX = 128;
        TileMap->TileChunkCountY = 128;
        TileMap->TileChunkCountZ = 2;
        TileMap->TileChunks = PushArray(&GameState->WorldArena,
                                        TileMap->TileChunkCountX * TileMap->TileChunkCountY * TileMap->TileChunkCountZ,
                                        tile_chunk);

        TileMap->TileSideInMeters = 1.4f;

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        uint32 ScreenX = 0;
        uint32 ScreenY = 0;
        uint32 AbsTileZ = 0;

        // TODO: Replace all this with real world generation!
        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        for (uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex) {
            // TODO: Random number generator!
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
            uint32 RandomChoice;
            if (DoorUp || DoorDown) {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            } else {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
            }

            bool32 CreatedZDoor = false;
            if (RandomChoice == 2) {
                CreatedZDoor = true;
                if (AbsTileZ == 0) {
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

                    SetTileValue(&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ,
                                 TileValue);
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
                if (AbsTileZ == 0) {
                    AbsTileZ = 1;
                } else {
                    AbsTileZ = 0;
                }
            } else if (RandomChoice == 1) {
                ScreenX += 1;
            } else {
                ScreenY += 1;
            }
        }

        Memory->IsInitialized = true;
    }

    world *World = GameState->World;
    tile_map *TileMap = World->TileMap;

    int32 TileSideInPixels = 60;
    real32 MetersToPixels = (real32) TileSideInPixels / (real32) TileMap->TileSideInMeters;
    real32 LowerLeftX = -(real32) TileSideInPixels / 2.0f;
    real32 LowerLeftY = (real32) Buffer->Height;

    //
    // NOTE:
    //
    for (size_t ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        entity *ControllingEntity = GetEntity(GameState, GameState->PlayerIndexForController[ControllerIndex]);
        if (ControllingEntity) {
            v2 ddP = {};

            if (Controller->IsAnalog) {
                ddP = v2{Controller->StickAverageX, Controller->StickAverageY};
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

            MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddP);
        } else {
            if (Controller->Start.EndedDown) {
                uint32 EntityIndex = AddEntity(GameState);
                InitializePlayer(GameState, EntityIndex);
                GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
            }
        }
    }

    //
    // NOTE: Update camera/player Z based on last movement.
    //
    entity *CameraFollowingEntity = GetEntity(GameState, GameState->CameraFollowingEntityIndex);
    if (CameraFollowingEntity) {
        GameState->CameraP.AbsTileZ = CameraFollowingEntity->P.AbsTileZ;

        tile_map_difference Diff = Subtract(TileMap, &CameraFollowingEntity->P, &GameState->CameraP);
        if (Diff.dXY.X > 9.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileX += 17;
        }

        if (Diff.dXY.X < -9.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileX -= 17;
        }

        if (Diff.dXY.Y > 5.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileY += 9;
        }

        if (Diff.dXY.Y < -5.0f * TileMap->TileSideInMeters) {
            GameState->CameraP.AbsTileY -= 9;
        }
    }

    //
    // NOTE: Render
    //
    DrawBitmap(Buffer, &GameState->Backdrop, 0, 0);

    real32 ScreenCenterX = 0.5f * (real32) Buffer->Width;
    real32 ScreenCenterY = 0.5f * (real32) Buffer->Height;

    for (int32 RelRow = -10; RelRow < 10; ++RelRow) {
        for (int32 RelColumn = -10; RelColumn < 20; ++RelColumn) {
            uint32 Column = GameState->CameraP.AbsTileX + RelColumn;
            uint32 Row = GameState->CameraP.AbsTileY + RelRow;

            uint32 TileID = GetTileValue(TileMap, Column, Row, GameState->CameraP.AbsTileZ);

            if (TileID > 1) {
                real32 Gray = 0.5f;
                if (TileID == 2) {
                    Gray = 1.0f;
                }

                if (TileID > 2) {
                    Gray = 0.25f;
                }

                if (Column == GameState->CameraP.AbsTileX &&
                    Row == GameState->CameraP.AbsTileY) {
                    Gray = 0.0f;
                }

                v2 TileSide = {0.5f * TileSideInPixels, 0.5f * TileSideInPixels};
                v2 Cen = {ScreenCenterX - MetersToPixels * GameState->CameraP.Offset.X + ((real32) RelColumn) * TileSideInPixels,
                          ScreenCenterY + MetersToPixels * GameState->CameraP.Offset.Y - ((real32) RelRow) * TileSideInPixels};
                v2 Min = Cen - TileSide;
                v2 Max = Cen + TileSide;

                DrawRectangle(Buffer, Min, Max, Gray, Gray, Gray);
            }
        }
    }

    entity *Entity = GameState->Entities;
    for (uint32 EntityIndex = 0; EntityIndex < GameState->EntityCount; ++EntityIndex, ++Entity) {
        // TODO: Culling of entities base of Z / camera view
        if (Entity->Exists) {
            tile_map_difference Diff = Subtract(TileMap, &Entity->P, &GameState->CameraP);
            real32 PlayerR = 1.0f;
            real32 PlayerG = 1.0f;
            real32 PlayerB = 0.0f;
            real32 PlayerGroundPointX = ScreenCenterX + MetersToPixels * Diff.dXY.X;
            real32 PlayerGroundPointY = ScreenCenterY - MetersToPixels * Diff.dXY.Y;
            v2 PlayerLeftTop = {PlayerGroundPointX - 0.5f * MetersToPixels * Entity->Width,
                                PlayerGroundPointY - MetersToPixels * Entity->Height};
            v2 EntityWidthHeight = {MetersToPixels * Entity->Width,
                                    MetersToPixels * Entity->Height};
            DrawRectangle(Buffer,
                          PlayerLeftTop,
                          PlayerLeftTop + EntityWidthHeight,
                          PlayerR, PlayerG, PlayerB);

            hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[Entity->FacingDirection];
            DrawBitmap(Buffer, &HeroBitmaps->Torso, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
            DrawBitmap(Buffer, &HeroBitmaps->Cape, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
            DrawBitmap(Buffer, &HeroBitmaps->Head, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state *GameState = (game_state *) Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}

/*
internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset) {
    uint8 *Row = (uint8 *) Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y) {
        uint32 *Pixel = (uint32 *) Row;
        for (int X = 0; X < Buffer->Width; ++X) {
            uint8 Blue = (uint8) (X + BlueOffset);
            uint8 Green = (uint8) (Y + GreenOffset);
            *Pixel++ = ((Green << 16) | Blue);
        }

        Row += Buffer->Pitch;
    }
}
*/
