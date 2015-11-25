/*
 * TODO: THIS IS NOT A FINAL PLATFORM LAYER!!
 *
 *   - Saved game locations
 *   - Getting a handle to our own executable file
 *   - Asset loading path
 *   - Threading (launch a thread)
 *   - Raw Input (support for multiple keyboards)
 *   - Sleep/timeBeginPeriod
 *   - ClipCursor() (multimonitor support)
 *   - Fullscreen support
 *   - WM_SETCURSOR (control cursor visibility)
 *   - QueryCanelAutoplay
 *   - WM_ACTIVATEAPP (for when we are not active application)
 *   - Blit speed improvemetns (BitBlt)
 *   - Hardware acceleration (OpenGL or Direct3D or BOTH??)
 *   - GetKeyboardLayout (for French keybords, international WASD support)
 *
 *   Just a partial list of stuff!!
 */
#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#include "handmade.h"
#include "win32_handmade.h"

// TODO: This is a global for now.
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

// NOTE: XinputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: XinputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
CatStrings(size_t SourceACount, const char *SourceA,
           size_t SourceBCount, const char *SourceB,
           size_t DestCount, char *Dest) {
    for (size_t Index = 0; Index < SourceACount && Index < DestCount; ++Index) {
        *Dest++ = *SourceA++;
    }

    for (size_t Index = 0; Index < SourceBCount && Index < DestCount; ++Index) {
        *Dest++ = *SourceB++;
    }

    *Dest++ = 0;
}

internal void
Win32GetEXEFileName(win32_state *State) {
    // NOTE: Nerver use MAX_PATH in code that is user-facing, because it
    // can be dangerous and lead to bad results.
    DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for (char *Scan = State->EXEFileName; *Scan; ++Scan) {
        if (*Scan == '\\') {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

internal int
StringLength(const char *String) {
    int Count = 0;
    while (*String++) {
        ++Count;
    }
    return Count;
}

internal void
Win32BuildEXEPathFileName(win32_state *State, const char *FileName,
                          int DestCount, char *Dest) {
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,
               StringLength(FileName), FileName,
               DestCount, Dest);
}


DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
    if (Memory) {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize)) {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents) {
                DWORD BytesRead;
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0)
                    && (FileSize32 == BytesRead)) {
                    // NOTE: File read successfully
                    Result.ContentsSize = FileSize32;
                } else {
                    // TODO: Logging
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = 0;
                }
            } else {
                // TODO: Logging
            }
        } else {
            // TODO: Logging
        }

        CloseHandle(FileHandle);
    } else {
        // TODO: Logging
    }

    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)) {
            // NOTE: File write successfully
            Result = BytesWritten == MemorySize;
        } else {
            // TODO: Logging
        }
        CloseHandle(FileHandle);
    } else {
        // TODO: Logging
    }

    return Result;
}

inline FILETIME
Win32GetLastWriteTime(const char *Filename) {
    FILETIME LastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data)) {
        LastWriteTime = Data.ftLastWriteTime;
    }

    return LastWriteTime;
}

internal win32_game_code
Win32LoadGameCode(const char *SourceDLLName, const char *TempDLLName) {
    win32_game_code Result = {};

    // TODO: Need to get the proper path here!
    // TODO: Automatic determination of when updates are necessary.

    Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

    CopyFile(SourceDLLName, TempDLLName, FALSE);
    Result.GameCodeDLL = LoadLibrary(TempDLLName);

    if (Result.GameCodeDLL) {
        Result.UpdateAndRender = (game_update_and_render *) GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
        Result.GetSoundSamples = (game_get_sound_samples *) GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

        Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
    }

    if (!Result.IsValid) {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }

    return Result;
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode) {
    if (GameCode->GameCodeDLL) {
        FreeLibrary(GameCode->GameCodeDLL);
    }

    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal void
Win32LoadXInput(void) {
    // TODO: Test this on Windows 8
    HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
    if (!XInputLibrary) {
        // TODO: Diagnostic
        XInputLibrary = LoadLibrary("xinput1_3.dll");
    }

    if (XInputLibrary) {
        XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) {
            XInputGetState = XInputGetStateStub;
        }

        XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) {
            XInputSetState = XInputSetStateStub;
        }

        // TODO: Diagnostic
    } else {
        // TODO: Diagnostic
    }
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize) {
    // NOTE: Load the library
    HMODULE DSoundLibrary = LoadLibrary("dsound.dll");

    if (DSoundLibrary) {
        // NOTE: Get a DirectSound Object
        direct_sound_create *DirectSoundCreate = (direct_sound_create *) GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        // TODO: Double-check that this works on XP - DirectSound or 7??
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // NOTE: "Create" a primary buffer
                // TODO: DSBCAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if (SUCCEEDED(Error)) {
                        // NOTE: We have finnaly set the format!
                        printf("Primary buffer format was set.\n");
                    } else {
                        // TODO: Diagnostic
                    }
                } else {
                    // TODO: Diagnostic
                }
            } else {
                // TODO: Diagnostic
            }

            // NOTE: "Create" a secondary buffer
            // TODO: DSBCAPS_GETCURRENTPOSITION2
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if (SUCCEEDED(Error)) {
                printf("Secondary buffer created successfully.\n");
            } else {
            }

            // NOTE: Start it playing
        } else {
            // TODO: Diagnostic
        }
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    // TODO: Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if (Buffer->Memory) {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;

    // NOTE: When the biHeight field is negative, this is the clue to
    // Windows to treat this bitmap as top-down, not bottom-up, meaning that
    // the first tree bytes of the image are the color for the top left pixel
    // in the bitmap, not the bottom left.
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * BytesPerPixel;

    // TODO: Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext,
                           int WindowWidth, int WindowHeight)
{
    int OffsetX = 10;
    int OffsetY = 10;

    PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
    PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);
    PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
    PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);

    // NOTE: For prototyping purposes, we're going to always blit
    // 1-to-1 pixels to make sure we don't introduce artifacts with
    // stretching while we are learning to code the renderer!
    StretchDIBits(DeviceContext,
                  /*
                  X, Y, Width, Height,
                  X, Y, Width, Height,
                  */
                  OffsetX, OffsetY, Buffer->Width, Buffer->Height,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory, &Buffer->Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                   UINT Message,
                   WPARAM WParam,
                   LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message) {
        case WM_CLOSE:
        {
            // TODO: Handle this with a message to the user?
            GlobalRunning = false;
            printf("WM_CLOSE\n");
        } break;

        case WM_ACTIVATEAPP:
        {
            printf("WM_ACTIVATEAPP\n");
#if 0
            if (WParam == TRUE) {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
            } else {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
#endif
        } break;

        case WM_DESTROY:
        {
            // TODO: Handle this as an error - recreate window?
            GlobalRunning = false;
            printf("WM_DESTROY\n");
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            assert(!"Keybord input came in through a non-dispatch message");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

            win32_window_dimension Dimension = Win32GetWindowDimension(Window);

            Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            //printf("default\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        }
    }

    return Result;
}

internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0,
                                              SoundOutput->SecondaryBufferSize,
                                              &Region1, &Region1Size,
                                              &Region2, &Region2Size,
                                              0))) {
        uint8 *DestSample = (uint8 *) Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
            *DestSample++ = 0;
        }

        DestSample = (uint8 *) Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
            *DestSample++ = 0;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD BytesToLock, DWORD BytesToWrite,
                     game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(BytesToLock, BytesToWrite,
                                              &Region1, &Region1Size,
                                              &Region2, &Region2Size,
                                              0))) {
        // TODO: assert that Region1Size/Region2Size is valid
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *) Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *) Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown) {
    if (NewState->EndedDown != IsDown) {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state *OldState, DWORD ButtonBit,
                                game_button_state *NewState) {
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold) {
    real32 Result = 0;

    if (Value < -DeadZoneThreshold) {
        Result = (real32) (Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
    } else if (Value > DeadZoneThreshold){
        Result = (real32) (Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold);
    }
    return Result;
}

internal void
Win32GetInputFileLocation(win32_state *State, bool32 InputStream, int SlotIndex, int DestCount, char *Dest) {
    char Temp[64];
    snprintf(Temp, sizeof(Temp), "loop_edit_%d_%s.hmi",
             SlotIndex, InputStream ? "input" : "state");
    Win32BuildEXEPathFileName(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_state *State, unsigned int Index) {
    Assert(Index < ArrayCount(State->ReplayBuffers));
    win32_replay_buffer *ReplayBuffer = &State->ReplayBuffers[Index];
    return ReplayBuffer;
}

internal void
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex) {
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
    if (ReplayBuffer->MemoryBlock) {
        State->InputRecordingIndex = InputRecordingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, InputRecordingIndex, true,
                                  sizeof(FileName), FileName);
        State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0,
                                             CREATE_ALWAYS, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif

        CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndRecordingInput(win32_state *State) {
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayBack(win32_state *State, int InputPlayingIndex) {
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
    if (ReplayBuffer->MemoryBlock) {
        State->InputPlayingIndex = InputPlayingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputPlayingIndex,
                                  sizeof(FileName), FileName);
        State->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 0, 0,
                                            OPEN_EXISTING, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif

        CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndInputPlayBack(win32_state *State) {
    CloseHandle(State->PlaybackHandle);
    State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *State, game_input *NewInput){
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_state *State, game_input *NewInput) {
    DWORD BytesRead = 0;
    if (ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0)) {
        if (BytesRead == 0) {
            // NOTE: We've hit the end of the stream, go back to the begining
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayBack(State);
            Win32BeginInputPlayBack(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
}

internal void
Win32ProcessPendingMessage(win32_state *State, game_controller_input *KeyboardController) {
  MSG Message;

  while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
    if (Message.message == WM_QUIT) {
      GlobalRunning = false;
    }

    switch (Message.message) {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
          uint32 VKCode = (uint32) Message.wParam;

          // NOTE: Since we are comparing WasDown to IsDown,
          // we MUST use == or != to convert these bit tests to actual
          // 0 or 1 values.
          bool WasDown = ((Message.lParam & (1 << 30)) != 0);
          bool IsDown = ((Message.lParam & (1 << 31)) == 0);

          if (WasDown != IsDown) {
            if (VKCode == 'W') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
            } else if (VKCode == 'A') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
            } else if (VKCode == 'S') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
            } else if (VKCode == 'D') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
            } else if (VKCode == 'Q') {
              Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
            } else if (VKCode == 'E') {
              Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
            } else if (VKCode == VK_UP) {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
            } else if (VKCode == VK_LEFT) {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
            } else if (VKCode == VK_RIGHT) {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
            } else if (VKCode == VK_DOWN) {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
            } else if (VKCode == VK_ESCAPE) {
                Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
            } else if (VKCode == VK_SPACE) {
                Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
            }
#if HANDMADE_INTERNAL
            else if (VKCode == 'P') {
                if (IsDown) {
                    GlobalPause = !GlobalPause;
                }
            } else if (VKCode == 'L') {
                if (IsDown) {
                    if (State->InputPlayingIndex == 0) {
                        if (State->InputRecordingIndex == 0) {
                            Win32BeginRecordingInput(State, 1);
                        } else {
                            Win32EndRecordingInput(State);
                            Win32BeginInputPlayBack(State, 1);
                        }
                    } else {
                        Win32EndInputPlayBack(State);
                    }
                }
            }
#endif
          }

          bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
          if ((VKCode == VK_F4) && AltKeyWasDown) {
            GlobalRunning = false;
          }
        } break;
        default: {
          TranslateMessage(&Message);
          DispatchMessage(&Message);
        }
    }
  }

}

inline LARGE_INTEGER
Win32GetWallClock() {
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
    return ((real32) (End.QuadPart - Start.QuadPart) / (real32) GlobalPerfCountFrequency);
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer,
                       int X, int Top, int Bottom, uint32 Color) {
    if (Top <= 0) {
        Top = 0;
    }

    if (Bottom > BackBuffer->Height) {
        Bottom = BackBuffer->Height;
    }

    if (X >= 0 && X < BackBuffer->Width) {
        uint8 *Pixel = ((uint8 *) BackBuffer->Memory
                        + X * BackBuffer->BytesPerPixel
                        + Top * BackBuffer->Pitch);
        for (int Y = Top; Y < Bottom; ++Y) {
            *(uint32 *) Pixel = Color;
            Pixel += BackBuffer->Pitch;
        }
    }
}

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer,
                           win32_sound_output *SoundOutput,
                           real32 C, int PadX, int Top, int Bottom,
                           DWORD Value, uint32 Color) {
    real32 XReal32 = (C * (real32) Value);
    int X = PadX + (int) XReal32;

    Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

#if 0
internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer,
                      size_t MarkerCount, win32_debug_time_marker *Markers,
                      size_t CurrentMarkerIndex,
                      win32_sound_output *SoundOutput, real32 TargetSecondsPerFramew) {
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    real32 C = (real32) (BackBuffer->Width - 2 * PadX) / (real32) SoundOutput->SecondaryBufferSize;
    for (size_t MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex) {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xFFFF0000;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if (MarkerIndex == CurrentMarkerIndex) {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            int FirstTop = Top;

            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
        }

        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample, PlayWindowColor);
        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
    }
}
#endif

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nCmdShow)
{
    win32_state Win32State = {};

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32GetEXEFileName(&Win32State);

    char SourceGameCodeDLLFullpath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade.dll",
                              sizeof(SourceGameCodeDLLFullpath), SourceGameCodeDLLFullpath);

    char TempGameCodeDLLFullpath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade_temp.dll",
                              sizeof(TempGameCodeDLLFullpath), TempGameCodeDLLFullpath);


    // NOTE: Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

    Win32LoadXInput();

    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) {
        HWND Window = CreateWindowEx(
            0, // WS_EX_TOPMOST | WS_EX_LAYERED,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0
        );

        if (Window) {
            // NOTE: Sound test
            win32_sound_output SoundOutput = {};

            // TOOD: How do we reliably query on this on Windows?
            int MonitorRefreshHz = 60;
            HDC RefreshDC = GetDC(Window);
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if (Win32RefreshRate > 1) {
                MonitorRefreshHz = Win32RefreshRate;
            }
            real32 GameUpdateHz = MonitorRefreshHz / 2;
            real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            // TODO: Actually compute this variance and see
            // what the lowest reasonable value is.
            SoundOutput.SafetyBytes = (int) (((real32) SoundOutput.SamplesPerSecond * (real32) SoundOutput.BytesPerSample / GameUpdateHz) / 3.0f);

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            bool32 SoundIsPlaying = false;

            GlobalRunning = true;

            int16 *Samples = (int16 *) VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID) Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

            // TODO: Handle various memory footprints (USING SYSTEM METRICS)
            // TODO: Use MEM_LARGE_PAGES and call adjust token
            // privileges when not on Windows XP
            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.PermanentStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress,
                                                 (size_t) Win32State.TotalSize,
                                                 MEM_RESERVE | MEM_COMMIT,
                                                 PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = ((uint8 *) GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

            for (size_t ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ++ReplayIndex) {
                win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

                // TODO: Recording system still seems to take too long
                // on record start - find out what Windodws is doing and if
                // we can spped up / defer some of that processing.

                Win32GetInputFileLocation(&Win32State, false, ReplayIndex,
                                          sizeof(ReplayBuffer->FileName),
                                          ReplayBuffer->FileName);

                ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->FileName,
                                                       GENERIC_WRITE | GENERIC_READ,
                                                       0, 0, CREATE_ALWAYS, 0, 0);

                LARGE_INTEGER MaxSize;
                MaxSize.QuadPart = Win32State.TotalSize;
                ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0,
                                                            PAGE_READWRITE,
                                                            MaxSize.HighPart, MaxSize.LowPart,
                                                            0);

                ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap,
                                                          FILE_MAP_ALL_ACCESS,
                                                          0, 0, Win32State.TotalSize);
                if (ReplayBuffer->MemoryBlock) {
                } else {
                    // TODO: Diagnostic
                }
            }

            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();

                size_t DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[30] = {0};

                // TODO: Handle startup specially
                bool32 SoundIsValid = false;

                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullpath,
                                                         TempGameCodeDLLFullpath);

                uint64 LastCycleCount = __rdtsc();
                while (GlobalRunning) {
                    NewInput->dtForFrame = TargetSecondsPerFrame;

                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullpath);
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0) {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullpath, TempGameCodeDLLFullpath);
                    }

                    // TODO: Zeroing macro
                    // TODO: We can't zero everything because the up/down state will
                    // be wrong!!!
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    for (size_t ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex) {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }

                    Win32ProcessPendingMessage(&Win32State, NewKeyboardController);

                    if (!GlobalPause) {
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);
                        NewInput->MouseX = MouseP.x;
                        NewInput->MouseY = MouseP.y;
                        NewInput->MouseZ = 0; // TODO: Support mousewheel?
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0],
                                                    GetKeyState(VK_LBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1],
                                                    GetKeyState(VK_MBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2],
                                                    GetKeyState(VK_RBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3],
                                                    GetKeyState(VK_XBUTTON1) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4],
                                                    GetKeyState(VK_XBUTTON2) & (1 << 15));

                        // TODO: Should we poll this more frequently
                        DWORD MaxControllerCount = XUSER_MAX_COUNT;
                        if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) {
                            MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                        }

                        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex) {
                            DWORD OurControllerIndex = ControllerIndex + 1;
                            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                            XINPUT_STATE ControllerState;
                            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                                // NOTE: This controller is plugged in
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;

                                // TODO: See if ControllerState.dwPacketNumber increments too rapidly
                                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                                NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                                if ((NewController->StickAverageX != 0.0f) ||
                                    (NewController->StickAverageY != 0.0f)) {
                                    NewController->IsAnalog = true;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
                                    NewController->StickAverageY = -1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                                    NewController->StickAverageX = 1.0f;
                                    NewController->IsAnalog = false;
                                }


                                real32 Threshold = 0.5f;
                                Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0,
                                                                &OldController->MoveLeft,
                                                                1,
                                                                &NewController->MoveLeft);

                                Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0,
                                                                &OldController->MoveRight,
                                                                1,
                                                                &NewController->MoveRight);

                                Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0,
                                                                &OldController->MoveDown,
                                                                1,
                                                                &NewController->MoveDown);

                                Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0,
                                                                &OldController->MoveUp,
                                                                1,
                                                                &NewController->MoveUp);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionDown,
                                                                XINPUT_GAMEPAD_A,
                                                                &NewController->ActionDown);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionRight,
                                                                XINPUT_GAMEPAD_B,
                                                                &NewController->ActionRight);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionLeft,
                                                                XINPUT_GAMEPAD_X,
                                                                &NewController->ActionLeft);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionUp,
                                                                XINPUT_GAMEPAD_Y,
                                                                &NewController->ActionUp);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->LeftShoulder,
                                                                XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                                &NewController->LeftShoulder);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->RightShoulder,
                                                                XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                                &NewController->RightShoulder);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Start,
                                                                XINPUT_GAMEPAD_START,
                                                                &NewController->Start);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Back,
                                                                XINPUT_GAMEPAD_START,
                                                                &NewController->Back);

                            } else {
                                // NOTE: This controller is not available
                                NewController->IsConnected = false;
                            }
                        }

                        thread_context Thread = {};

                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackBuffer.Memory;
                        Buffer.Width = GlobalBackBuffer.Width;
                        Buffer.Height = GlobalBackBuffer.Height;
                        Buffer.Pitch = GlobalBackBuffer.Pitch;
                        Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

                        if (Win32State.InputRecordingIndex) {
                            Win32RecordInput(&Win32State, NewInput);
                        }

                        if (Win32State.InputPlayingIndex) {
                            Win32PlayBackInput(&Win32State, NewInput);
                        }
                        if (Game.UpdateAndRender) {
                            Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
                        }

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK) {
                            /* NOTE:

                               Here how sound output computation works.

                               We define a safety value that is the number
                               of samples we think out grame update loop
                               may vary by (let's say up to 2ms).

                               When we wake up to write audio, we will look
                               and see what the play cursor position is and we
                               will forecast ahead where we think the play
                               cursor will be on the next frame boundary.

                               We will then look to see if the write cursor is
                               before that by at least our safe value. If
                               it is, the target fill position is that frame
                               boundary plus one frame. This giving us perfect
                               audio sync in the case of a card that has low
                               enough latency.

                               If the write cursor is _after_ taht safety
                               margin, then we assume we can never sync the
                               audio perfectly, so we will write one frame's
                               worth of audio plus the safety margin's worth
                               of guard samples.
                            */

                            if (!SoundIsValid) {
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            DWORD BytesToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                                                 % SoundOutput.SecondaryBufferSize);

                            DWORD ExpectedSoundBytesPerFrame = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;
                            real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
                            DWORD ExpectedBytesUntilFlip = (DWORD) ((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32) ExpectedSoundBytesPerFrame);
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor) {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;

                            bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency) {
                                TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                            } else {
                                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
                                                SoundOutput.SafetyBytes);
                            }
                            TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

                            DWORD BytesToWrite = 0;
                            if (BytesToLock > TargetCursor) {
                                BytesToWrite = (SoundOutput.SecondaryBufferSize  - BytesToLock);
                                BytesToWrite += TargetCursor;
                            } else {
                                BytesToWrite = TargetCursor - BytesToLock;
                            }

                            game_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            if (Game.GetSoundSamples) {
                                Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
                            }

#if HANDMADE_INTERNAL
                            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation = BytesToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if (WriteCursor < PlayCursor) {
                                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
#if 0
                            printf("BTL:%lu TC:%lu BTW:%lu - PC:%lu WC:%lu DELTA:%lu (%fs)\n",
                                   BytesToLock, TargetCursor, BytesToWrite,
                                   PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
#endif

#endif
                            Win32FillSoundBuffer(&SoundOutput, BytesToLock, BytesToWrite, &SoundBuffer);
                        } else {
                            SoundIsValid = false;
                        }

                        if (!SoundIsPlaying) {
                            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
                            SoundIsPlaying = true;
                        }

                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

                        // TODO: NOT TESTED YET! PROBABLY BUGGY!!!
                        real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                            if (SleepIsGranular) {
                                DWORD SleepMS = (DWORD) (1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                                if (SleepMS > 0) {
                                    Sleep(SleepMS);
                                }
                            }
                            real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                                       Win32GetWallClock());
                            if (TestSecondsElapsedForFrame < TargetSecondsPerFrame) {
                                // TODO: LOG MISSED SLEEP HERE
                            }

                            while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                                Win32GetWallClock());
                            }
                        } else {
                            // TODO: MISSED FRAME RATE!
                            // TODO: Logging
                        }

                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        real64 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                        LastCounter = EndCounter;

                        win32_window_dimension Dimension = Win32GetWindowDimension(Window);

                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                        ReleaseDC(Window, DeviceContext);

                        FlipWallClock = Win32GetWallClock();
#if HANDMADE_INTERNAL
                        {
                            DWORD PlayCursor, WriteCursor;
                            if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK) {
                                Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
                                win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
                                Marker->FlipPlayCursor = PlayCursor;
                                Marker->FlipWriteCursor = WriteCursor;
                            }
                        }
#endif

                        game_input *Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;

#if 0
                        uint64 EndCycleCount = __rdtsc();
                        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        real64 FPS = 0.0f;
                        real64 MCPF = (uint32) (CyclesElapsed / (1000 * 1000));

                        printf("%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
#endif

#if HANDMADE_INTERNAL
                        ++DebugTimeMarkerIndex;
                        if (DebugTimeMarkerIndex >= ArrayCount(DebugTimeMarkers)) {
                            DebugTimeMarkerIndex = 0;
                        }
#endif
                    }
                }
            } else {
                // TODO: Logging
            }
        } else {
            // TODO: Logging
        }
    } else {
        // TODO: Logging
    }

    return 0;
}
