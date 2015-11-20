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
#if 1
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolumn);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        GameState->tSine += 2.0f * PI32 * 1.0f / (real32) WavePeriod;
        if (GameState->tSine > 2.0 * PI32) {
            GameState->tSine -= 2.0 * PI32;
        }
    }
}

internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
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

internal void
RenderPlayer(game_offscreen_buffer *Buffer, int PlayerX, int PlayerY) {
    uint8 *EndOfBuffer = (uint8 *) Buffer->Memory + Buffer->Pitch * Buffer->Height;

    uint32 Color = 0xFFFFFFFF;
    int Top = PlayerY;
    int Bottom = PlayerY + 10;

    for (int X = PlayerX; X < PlayerX + 10; ++X) {
        uint8 *Pixel = ((uint8 *) Buffer->Memory +
                       X * Buffer->BytesPerPixel +
                       Top * Buffer->Pitch);

        for (int Y = Top; Y < Bottom; ++Y) {
            if ((Pixel >= Buffer->Memory) &&
                (Pixel + 4 <= EndOfBuffer)) {
                *(uint32 *) Pixel = Color;
            }

            Pixel += Buffer->Pitch;
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           ArrayCount(Input->Controllers[0].Buttons));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *) Memory->PermanentStorage;
    if (!Memory->IsInitialized) {
        const char *Filename = __FILE__;

        debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(Filename);
        if (File.Contents) {
            Memory->DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
            Memory->DEBUGPlatformFreeFileMemory(File.Contents);
        }

        GameState->ToneHz = 512;
        GameState->tSine = 0.0f;

        GameState->PlayerX = 100;
        GameState->PlayerY = 100;

        // TODO: This may be more appropriate to do in the platform layer
        Memory->IsInitialized = true;
    }

    for (size_t ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if (Controller->IsAnalog) {
            GameState->BlueOffset += (int) (4.0f * Controller->StickAverageX);
            GameState->ToneHz = 512 + (int) (128.0f * Controller->StickAverageY);
        } else {
            if (Controller->MoveLeft.EndedDown) {
                GameState->BlueOffset -= 1;
            }

            if (Controller->MoveRight.EndedDown) {
                GameState->BlueOffset += 1;
            }
        }

        GameState->PlayerX += (int) (4.0f * Controller->StickAverageX);
        GameState->PlayerY -= (int) (4.0f * Controller->StickAverageY);
        if (GameState->tJump > 0) {
            GameState->PlayerY -= (int) (10.0f * sinf(GameState->tJump));
        }
        if (Controller->ActionDown.EndedDown) {
            GameState->tJump = 1.0;
        }
        GameState->tJump -= 0.033f;

    }
    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
    RenderPlayer(Buffer, GameState->PlayerX, GameState->PlayerY);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
    game_state *GameState = (game_state *) Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}
