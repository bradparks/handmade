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
                    DEBUGPlatformFreeFileMemory(Result.Contents);
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

    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = FindFirstFile(Filename, &FindData);
    if (FindHandle != INVALID_HANDLE_VALUE) {
        LastWriteTime = FindData.ftLastWriteTime;
        FindClose(FindHandle);
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
        Result.UpdateAndRender = GameUpdateAndRenderStub;
        Result.GetSoundSamples = GameGetSoundSamplesStub;
    }

    return Result;
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode) {
    if (GameCode->GameCodeDLL) {
        FreeLibrary(GameCode->GameCodeDLL);
    }

    GameCode->IsValid = false;
    GameCode->UpdateAndRender = GameUpdateAndRenderStub;
    GameCode->GetSoundSamples = GameGetSoundSamplesStub;
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
    // TODO: Aspect ratio correction
    StretchDIBits(DeviceContext,
                  /*
                  X, Y, Width, Height,
                  X, Y, Width, Height,
                  */
                  0, 0, WindowWidth, WindowHeight,
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
Win32ProcessKeybordMessage(game_button_state *NewState, bool32 IsDown) {
    assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
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
Win32ProcessPendingMessage(game_controller_input *KeyboardController) {
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
          bool WasDown = ((Message.lParam & (1 << 30)) != 0);
          bool IsDown = ((Message.lParam & (1 << 31)) == 0);

          if (WasDown != IsDown) {
            if (VKCode == 'W') {
                Win32ProcessKeybordMessage(&KeyboardController->MoveUp, IsDown);
            } else if (VKCode == 'A') {
                Win32ProcessKeybordMessage(&KeyboardController->MoveLeft, IsDown);
            } else if (VKCode == 'S') {
                Win32ProcessKeybordMessage(&KeyboardController->MoveDown, IsDown);
            } else if (VKCode == 'D') {
                Win32ProcessKeybordMessage(&KeyboardController->MoveRight, IsDown);
            } else if (VKCode == 'Q') {
              Win32ProcessKeybordMessage(&KeyboardController->LeftShoulder, IsDown);
            } else if (VKCode == 'E') {
              Win32ProcessKeybordMessage(&KeyboardController->RightShoulder, IsDown);
            } else if (VKCode == VK_UP) {
              Win32ProcessKeybordMessage(&KeyboardController->ActionUp, IsDown);
            } else if (VKCode == VK_LEFT) {
              Win32ProcessKeybordMessage(&KeyboardController->ActionLeft, IsDown);
            } else if (VKCode == VK_RIGHT) {
              Win32ProcessKeybordMessage(&KeyboardController->ActionRight, IsDown);
            } else if (VKCode == VK_DOWN) {
              Win32ProcessKeybordMessage(&KeyboardController->ActionDown, IsDown);
            } else if (VKCode == VK_ESCAPE) {
                Win32ProcessKeybordMessage(&KeyboardController->Start, IsDown);
            } else if (VKCode == VK_SPACE) {
                Win32ProcessKeybordMessage(&KeyboardController->Back, IsDown);
            }
#if HANDMADE_INTERNAL
            else if (VKCode == 'P') {
                if (IsDown) {
                    GlobalPause = !GlobalPause;
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

internal void
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount, char *Dest) {
    for (size_t Index = 0; Index < SourceACount && Index < DestCount; ++Index) {
        *Dest++ = *SourceA++;
    }

    for (size_t Index = 0; Index < SourceBCount && Index < DestCount; ++Index) {
        *Dest++ = *SourceB++;
    }

    *Dest++ = 0;
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nCmdShow)
{
    // NOTE: Nerver use MAX_PATH in code that is user-facing, because it
    // can be dangerous and lead to bad results.
    char EXEFilename[MAX_PATH];
    DWORD SizeOfFilename = GetModuleFileNameA(0, EXEFilename, sizeof(EXEFilename));
    char *OnePastLastSlash = EXEFilename;
    for (char *Scan = EXEFilename; *Scan; ++Scan) {
        if (*Scan == '\\') {
            OnePastLastSlash = Scan + 1;
        }
    }

    char SourceGameCodeDLLFilename[] = "handmade.dll";
    char SourceGameCodeDLLFullpath[MAX_PATH];
    CatStrings(OnePastLastSlash - EXEFilename, EXEFilename,
               sizeof(SourceGameCodeDLLFilename) - 1, SourceGameCodeDLLFilename,
               sizeof(SourceGameCodeDLLFullpath), SourceGameCodeDLLFullpath);

    char TempGameCodeDLLFilename[] = "handmade_temp.dll";
    char TempGameCodeDLLFullpath[MAX_PATH];
    CatStrings(OnePastLastSlash - EXEFilename, EXEFilename,
               sizeof(TempGameCodeDLLFilename) - 1, TempGameCodeDLLFilename,
               sizeof(TempGameCodeDLLFullpath), TempGameCodeDLLFullpath);

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    // NOTE: Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

    Win32LoadXInput();

    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    // TOOD: How do we reliably query on this on Windows?
    int FramesOfAudioLatency = 4;
    int MonitorRefreshHz = 60;
    int GameUpdateHz = MonitorRefreshHz / 2;
    real32 TargetSecondsPerFrame = 1.0f / (real32) GameUpdateHz;

    if (RegisterClass(&WindowClass)) {
        HWND Window = CreateWindowEx(
            0,
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
            HDC DeviceContext = GetDC(Window);

            // NOTE: Sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            // TODO: Get rid of LatencySampleCount
            SoundOutput.LatencySampleCount = 3 * (SoundOutput.SamplesPerSecond / GameUpdateHz);
            // TODO: Actually compute this variance and see
            // what the lowest reasonable value is.
            SoundOutput.SafetyBytes = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample / GameUpdateHz) / 3;

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

            uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.PermanentStorageSize;
            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress,
                                                       (size_t) TotalSize,
                                                       MEM_RESERVE | MEM_COMMIT,
                                                       PAGE_READWRITE);
            GameMemory.TransientStorage = ((uint8 *) GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];


                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();

                size_t DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};

                // TODO: Handle startup specially
                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0;
                bool32 SoundIsValid = false;

                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullpath,
                                                         TempGameCodeDLLFullpath);

                uint64 LastCycleCount = __rdtsc();
                while (GlobalRunning) {
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

                    Win32ProcessPendingMessage(NewKeyboardController);

                    if (!GlobalPause) {
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
                                    NewController->IsAnalog = false;
                                    NewController->StickAverageY = -1.0f;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                                    NewController->IsAnalog = false;
                                    NewController->StickAverageX = -1.0f;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                                    NewController->IsAnalog = false;
                                    NewController->StickAverageX = 1.0f;
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

                                //bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                                //bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);

                            } else {
                                // NOTE: This controller is not available
                                NewController->IsConnected = false;
                            }
                        }

                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackBuffer.Memory;
                        Buffer.Width = GlobalBackBuffer.Width;
                        Buffer.Height = GlobalBackBuffer.Height;
                        Buffer.Pitch = GlobalBackBuffer.Pitch;

                        Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);

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
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

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
                            Game.GetSoundSamples(&GameMemory, &SoundBuffer);

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
                            AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                            AudioLatencySeconds =
                                (((real32) AudioLatencyBytes / (real32) SoundOutput.BytesPerSample) /
                                 (real32) SoundOutput.SamplesPerSecond);

                            printf("BTL:%lu TC:%lu BTW:%lu - PC:%lu WC:%lu DELTA:%lu (%fs)\n",
                                   BytesToLock, TargetCursor, BytesToWrite,
                                   PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
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
#if HANDMADE_INTERNAL
                        // TODO: Note, current is wrong on the zero'th index
                        Win32DebugSyncDisplay(&GlobalBackBuffer,
                                              ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                                              DebugTimeMarkerIndex - 1,
                                              &SoundOutput, TargetSecondsPerFrame);
#endif

                        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

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

                        uint64 EndCycleCount = __rdtsc();
                        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        real64 FPS = 0.0f;
                        real64 MCPF = (uint32) (CyclesElapsed / (1000 * 1000));

                        printf("%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);

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
