#include "handmade.h"

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

inline int32
RoundReal32ToInt32(real32 Real32) {
    int32 Result = (int32) (Real32 + 0.5f);
    // TODO: Intrinsic????
    return Result;
}

inline uint32
RoundReal32ToUInt32(real32 Real32) {
    uint32 Result = (uint32) (Real32 + 0.5f);
    // TODO: Intrinsic????
    return Result;
}

// TODO: HOW TO IMPLEMENT THESE MATH FUNCTIONS!!!!
#include "math.h"
inline int32
FloorReal32ToInt32(real32 Real32) {
    int32 Result = (int32) floorf(Real32);
    return Result;
}

inline int32
TruncateReal32ToInt32(real32 Real32) {
    int32 Result = (int32) Real32;
    return Result;
}

internal void
DrawRectangle(game_offscreen_buffer *Buffer,
              real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
              real32 R, real32 G, real32 B) {
    int MinX = RoundReal32ToInt32(RealMinX);
    int MinY = RoundReal32ToInt32(RealMinY);
    int MaxX = RoundReal32ToInt32(RealMaxX);
    int MaxY = RoundReal32ToInt32(RealMaxY);

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

inline uint32
GetTileMapUnchecked(world *World, tile_map *TileMap, int32 TileX, int32 TileY) {
    Assert(TileMap);
    Assert((TileX >= 0) && (TileX < World->CountX) &&
           (TileY >= 0) && (TileY < World->CountY));

    return TileMap->Tiles[TileY * World->CountX + TileX];
}

inline tile_map *
GetTileMap(world *World, int32 TileMapX, int32 TileMapY) {
    tile_map *TileMap = 0;
    if ((TileMapX >= 0) && (TileMapX < World->TileMapCountX) &&
        (TileMapY >= 0) && (TileMapY < World->TileMapCountY)) {
        TileMap = &World->TileMaps[TileMapY * World->TileMapCountX + TileMapX];
    }
    return TileMap;
}

internal bool32
IsTileMapPointEmpty(world *World, tile_map *TileMap, real32 TestTileX, real32 TestTileY) {
    bool32 Empty = false;

    if (TileMap) {
        if ((TestTileX >= 0) && (TestTileX < World->CountX) &&
            (TestTileY >= 0) && (TestTileY < World->CountY)) {
            uint32 TileMapValue = GetTileMapUnchecked(World, TileMap, TestTileX, TestTileY);
            Empty = (TileMapValue == 0);
        }
    }

    return Empty;
}

inline canonical_position
GetCanonicalPosition(world *World, raw_position Pos) {
    canonical_position Result;

    Result.TileMapX = Pos.TileMapX;
    Result.TileMapY = Pos.TileMapY;

    real32 X = Pos.X - World->UpperLeftX;
    real32 Y = Pos.Y - World->UpperLeftY;
    Result.TileX = FloorReal32ToInt32(X / World->TileWidth);
    Result.TileY = FloorReal32ToInt32(Y / World->TileHeight);

    Result.X = X - Result.TileX * World->TileWidth;
    Result.Y = Y - Result.TileY * World->TileHeight;

    Assert(Result.X >= 0);
    Assert(Result.Y >= 0);
    Assert(Result.X < World->TileWidth);
    Assert(Result.Y < World->TileHeight);

    if (Result.TileX < 0) {
        Result.TileX = World->CountX + Result.TileX;
        --Result.TileMapX;
    }

    if (Result.TileY < 0) {
        Result.TileY = World->CountY + Result.TileY;
        --Result.TileMapY;
    }

    if (Result.TileX >= World->CountX) {
        Result.TileX = Result.TileX - World->CountX;
        ++Result.TileMapX;
    }

    if (Result.TileY >= World->CountY) {
        Result.TileY = Result.TileY - World->CountY;
        ++Result.TileMapY;
    }

    return Result;
}

internal bool32
IsWorldPointEmpty(world *World, raw_position TestPos) {
    bool32 Empty = false;

    canonical_position CanPos = GetCanonicalPosition(World, TestPos);

    tile_map *TileMap = GetTileMap(World, CanPos.TileMapX, CanPos.TileMapY);

    Empty = IsTileMapPointEmpty(World, TileMap, CanPos.TileX, CanPos.TileY);

    return Empty;

}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           ArrayCount(Input->Controllers[0].Buttons));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

#define TILE_MAP_COUNT_X 16
#define TILE_MAP_COUNT_Y 9
    uint32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1},
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
    };

    uint32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
    };

    uint32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    uint32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };


    tile_map TileMaps[2][2];
    TileMaps[0][0].Tiles = (uint32 *) Tiles00;
    TileMaps[0][1].Tiles = (uint32 *) Tiles10;
    TileMaps[1][0].Tiles = (uint32 *) Tiles01;
    TileMaps[1][1].Tiles = (uint32 *) Tiles11;

    world World;
    World.TileMaps = (tile_map *) TileMaps;
    World.TileMapCountX = 2;
    World.TileMapCountY = 2;
    World.CountX = TILE_MAP_COUNT_X;
    World.CountY = TILE_MAP_COUNT_Y;
    World.UpperLeftX = 0;
    World.UpperLeftY = 0;
    World.TileWidth = 60;
    World.TileHeight = 60;
    real32 PlayerWidth = 0.75 * World.TileWidth;
    real32 PlayerHeight = World.TileHeight;



    game_state *GameState = (game_state *) Memory->PermanentStorage;
    if (!Memory->IsInitialized) {
        Memory->IsInitialized = true;
        GameState->PlayerX = 150;
        GameState->PlayerY = 150;
    }

    tile_map *TileMap = GetTileMap(&World, GameState->PlayerTileMapX, GameState->PlayerTileMapY);
    Assert(TileMap);

    for (size_t ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if (Controller->IsAnalog) {
        } else {
            real32 dPlayerX = 0.0f; // pixels/second
            real32 dPlayerY = 0.0f; // pixels/second

            if (Controller->MoveUp.EndedDown) {
                dPlayerY = -1.0f;
            }

            if (Controller->MoveDown.EndedDown) {
                dPlayerY = 1.0f;
            }

            if (Controller->MoveLeft.EndedDown) {
                dPlayerX = -1.0f;
            }

            if (Controller->MoveRight.EndedDown) {
                dPlayerX = 1.0f;
            }

            dPlayerX *= 64.0f;
            dPlayerY *= 64.0f;

            // TODO: Diagonal will be faster! Fix once we have vectors :)
            real32 NewPlayerX = GameState->PlayerX + Input->dtForFrame * dPlayerX;
            real32 NewPlayerY = GameState->PlayerY + Input->dtForFrame * dPlayerY;

            raw_position PlayerPos = {
                GameState->PlayerTileMapX,
                GameState->PlayerTileMapY,
                NewPlayerX,
                NewPlayerY
            };

            raw_position PlayerLeft = PlayerPos;
            PlayerLeft.X -= 0.5f * PlayerWidth;

            raw_position PlayerRight = PlayerPos;
            PlayerRight.X += 0.5f * PlayerWidth;

            if (IsWorldPointEmpty(&World, PlayerPos) &&
                IsWorldPointEmpty(&World, PlayerLeft) &&
                IsWorldPointEmpty(&World, PlayerRight)) {
                canonical_position CanPos = GetCanonicalPosition(&World, PlayerPos);
                GameState->PlayerTileMapX = CanPos.TileMapX;
                GameState->PlayerTileMapY = CanPos.TileMapY;

                GameState->PlayerX = World.UpperLeftX + World.TileWidth * CanPos.TileX + CanPos.X;
                GameState->PlayerY = World.UpperLeftY + World.TileHeight * CanPos.TileY + CanPos.Y;
            }
        }
    }

    DrawRectangle(Buffer, 0.0f, 0.0f, (real32) Buffer->Width, (real32) Buffer->Height,
                  1.0f, 0.0f, 1.0f);

    for (int Row = 0; Row < 9; ++Row) {
        for (int Column = 0; Column < 16; ++Column) {
            uint32 TileID = GetTileMapUnchecked(&World, TileMap, Column, Row);

            real32 Gray = 0.5f;
            if (TileID == 1) {
                Gray = 1.0f;
            }

            real32 MinX = World.UpperLeftX + ((real32) Column) * World.TileWidth;
            real32 MinY = World.UpperLeftY + ((real32) Row) * World.TileHeight;
            real32 MaxX = MinX + World.TileWidth;
            real32 MaxY = MinY + World.TileHeight;

            DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
        }
    }

    real32 PlayerR = 1.0f;
    real32 PlayerG = 1.0f;
    real32 PlayerB = 0.0f;
    real32 PlayerLeft = GameState->PlayerX - 0.5f * PlayerWidth;
    real32 PlayerTop = GameState->PlayerY - PlayerHeight;
    DrawRectangle(Buffer,
                  PlayerLeft, PlayerTop,
                  PlayerLeft + PlayerWidth,
                  PlayerTop + PlayerHeight,
                  PlayerR, PlayerG, PlayerB);
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
