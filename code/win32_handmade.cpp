/*
  TODO: THIS IS NOT A FINAL PLATFORM LAYER!!

    - Make the right calls so Windows doesn't think we're "still loading" for a bit after we actually start
    - Saved game locations
    - Getting a handle to our own executable file
    - Asset loading path
    - Threading (launch a thread)
    - Raw Input (support for multiple keyboards)
    - ClipCursor() (multimonitor support)
    - QueryCanelAutoplay
    - WM_ACTIVATEAPP (for when we are not active application)
    - Blit speed improvemetns (BitBlt)
    - Hardware acceleration (OpenGL or Direct3D or BOTH??)
    - GetKeyboardLayout (for French keybords, international WASD support)
    - ChangeDisplaySettings option if we detect slow fullscreen blit??

    Just a partial list of stuff!!
 */

#include "handmade_platform.h"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO: This is a global for now.
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };

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

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data)) {
        LastWriteTime = Data.ftLastWriteTime;
    }

    return LastWriteTime;
}

internal win32_game_code
Win32LoadGameCode(const char *SourceDLLName, const char *TempDLLName, const char *LockFileName) {
    win32_game_code Result = {};

    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if (!GetFileAttributesEx(LockFileName, GetFileExInfoStandard, &Ignored)) {
        Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

        CopyFile(SourceDLLName, TempDLLName, FALSE);
        Result.GameCodeDLL = LoadLibrary(TempDLLName);

        if (Result.GameCodeDLL) {
            Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
            Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
            Result.DEBUGFrameEnd = (debug_game_frame_end *)GetProcAddress(Result.GameCodeDLL, "DEBUGGameFrameEnd");

            Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
        }
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

            // TODO: In release mode, should we not specify DSBCAPS_GLOBALFOCUS?

            // NOTE: "Create" a secondary buffer
            // TODO: DSBCAPS_GETCURRENTPOSITION2
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
#if HANDMADE_INTERNAL
            BufferDescription.dwFlags |= DSBCAPS_GLOBALFOCUS;
#endif
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
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    Buffer->Pitch = Align16(Width * BytesPerPixel);
    int BitmapMemorySize = Buffer->Pitch * Buffer->Height;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    // TODO: Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext,
                           int WindowWidth, int WindowHeight)
{
    // TODO: Centering / black bars?
    if ((WindowWidth >= Buffer->Width * 2) &&
        (WindowHeight >= Buffer->Height * 2)) {
        StretchDIBits(DeviceContext,
                      0, 0, 2 * Buffer->Width, 2 * Buffer->Height,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory, &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
    } else {
#if 1
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
#else
        r32 HeightOverWidth = (r32)Buffer->Height / (r32)Buffer->Width;
        s32 Width = WindowWidth;
        s32 Height = (s32)(Width * HeightOverWidth);
        StretchDIBits(DeviceContext,
                      0, 0, Width, Height,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory, &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
#endif
    }
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

        case WM_SETCURSOR: {
            if (DEBUGGlobalShowCursor) {
                Result = DefWindowProc(Window, Message, WParam, LParam);
            } else {
                SetCursor(0);
            }
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
    Assert(Index > 0);
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
ToggleFullscreen(HWND Window) {
    // NOTE: This follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx

    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {

            SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
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
                Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                GlobalRunning = false;
            } else if (VKCode == VK_SPACE) {
                Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
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
            if (IsDown) {
                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if ((VKCode == VK_F4) && AltKeyWasDown) {
                    GlobalRunning = false;
                }

                if ((VKCode == VK_RETURN) && AltKeyWasDown) {
                    if (Message.hwnd) {
                        ToggleFullscreen(Message.hwnd);
                    }
                }
            }
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

#if 0
internal void
HandleDebugCycleCounters(game_memory *Memory) {
#if HANDMADE_INTERNAL
    printf("DEBUG CYCLE COUNTS:\n");
    for (uint32 CounterIndex = 0; CounterIndex < ArrayCount(Memory->Counters); ++CounterIndex) {
        debug_cycle_count *Counter = Memory->Counters + CounterIndex;
        if (Counter->HitCount) {
            printf("  %d: %llucy %uh %llucy/h\n",
                   CounterIndex,
                   Counter->CycleCount,
                   Counter->HitCount,
                   Counter->CycleCount / Counter->HitCount);
        }
        Counter->HitCount = 0;
        Counter->CycleCount = 0;
    }
#endif
}
#endif

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer,
                       int X, int Top, int Bottom, uint32 Color)
{
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

struct platform_work_queue_entry {
    platform_work_queue_callback *Callback;
    void *Data;
};

struct platform_work_queue {
    uint32 volatile CompletionGoal;
    uint32 volatile CompletionCount;

    uint32 volatile NextEntryToWrite;
    uint32 volatile NextEntryToRead;
    HANDLE SemaphoreHandle;

    platform_work_queue_entry Entries[256];
};

internal void
Win32AddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data) {
    // TODO: Switch to InterlockedCompareExchange eventually
    // so that any thread can add?
    uint32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1 ) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
    platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;
    ++Queue->CompletionGoal;
    _WriteBarrier();
    Queue->NextEntryToWrite = NewNextEntryToWrite;
    ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal bool32
Win32DoNextWorkQueueEntry(platform_work_queue *Queue) {
    bool32 WeShouldSleep = false;

    uint32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    uint32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if (OriginalNextEntryToRead != Queue->NextEntryToWrite) {
        uint32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->NextEntryToRead,
                                                  NewNextEntryToRead,
                                                  OriginalNextEntryToRead);
        if (Index == OriginalNextEntryToRead) {
            platform_work_queue_entry Entry = Queue->Entries[Index];
            Entry.Callback(Queue, Entry.Data);
            InterlockedIncrement((LONG volatile *)&Queue->CompletionCount);
        }
    } else {
        WeShouldSleep = true;
    }

    return WeShouldSleep;
}

internal void
Win32CompleteAllWork(platform_work_queue *Queue) {
    while (Queue->CompletionGoal != Queue->CompletionCount) {
        Win32DoNextWorkQueueEntry(Queue);
    }

    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
}

DWORD WINAPI
ThreadProc(LPVOID lpParameter) {
    platform_work_queue *Queue = (platform_work_queue *)lpParameter;

    for (;;) {
        if (Win32DoNextWorkQueueEntry(Queue)) {
            WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);
        }
    }
}

internal PLATFORM_WORK_QUEUE_CALLBACK(DoWorkerWork) {
    char Buffer[256];
    snprintf(Buffer, ArrayCount(Buffer), "Thread %u: %s\n", GetCurrentThreadId(), (char *)Data);
    printf("%s\n", Buffer);
}

internal void
Win32MakeQueue(platform_work_queue *Queue, uint32 ThreadCount) {
    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;

    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;

    uint32 InitialCount = 0;
    Queue->SemaphoreHandle = CreateSemaphoreEx(0, InitialCount, ThreadCount, 0, 0,
                                               SEMAPHORE_ALL_ACCESS);

    for (uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex) {
        DWORD ThreadID;
        HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadID);
        CloseHandle(ThreadHandle);
    }
}

struct win32_platform_file_handle {
    HANDLE Win32Handle;
};

struct win32_platform_file_group {
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
};

internal PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(Win32GetAllFilesOfTypeBegin) {
    platform_file_group Result = {};

    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)VirtualAlloc(
        0, sizeof(win32_platform_file_group),
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
    );
    Result.Platform = Win32FileGroup;

    wchar_t *WildCard = L"*.*";

    switch (Type) {
        case PlatformFileType_AssetFile: {
            WildCard = L"*.hha";
        } break;

        case PlatformFileType_SavedGameFile: {
            WildCard = L"*.hhs";
        } break;

        InvalidDefaultCase;
    }

    Result.FileCount = 0;

    WIN32_FIND_DATAW FindData;
    HANDLE FindHandle = FindFirstFileW(WildCard, &FindData);
    while (FindHandle != INVALID_HANDLE_VALUE) {
        ++Result.FileCount;

        if (!FindNextFileW(FindHandle, &FindData)) {
            break;
        }
    }

    FindClose(FindHandle);

    Win32FileGroup->FindHandle = FindFirstFileW(WildCard, &Win32FileGroup->FindData);

    return Result;
}

internal PLATFORM_GET_ALL_FILES_OF_TYPE_END(Win32GetAllFilesOfTypeEnd) {
    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
    if (Win32FileGroup) {
        FindClose(Win32FileGroup->FindHandle);
        VirtualFree(Win32FileGroup, 0, MEM_RELEASE);
    }
}

internal PLATFORM_OPEN_NEXT_FILE(Win32OpenNextFile) {
    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
    platform_file_handle Result = {};

    if (Win32FileGroup->FindHandle != INVALID_HANDLE_VALUE) {
        // TODO: If we want, someday, make an actual arena used by Win32
        win32_platform_file_handle *Win32Handle = (win32_platform_file_handle *)VirtualAlloc(
            0, sizeof(win32_platform_file_handle),
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
        );
        Result.Platform = Win32Handle;

        if (Win32Handle) {
            wchar_t *FileName = Win32FileGroup->FindData.cFileName;
            Win32Handle->Win32Handle = CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
            Result.NoErrors = Win32Handle->Win32Handle != INVALID_HANDLE_VALUE;
        }

        if (!FindNextFileW(Win32FileGroup->FindHandle, &Win32FileGroup->FindData)) {
            FindClose(Win32FileGroup->FindHandle);
            Win32FileGroup->FindHandle = INVALID_HANDLE_VALUE;
        }
    }

    return Result;
}

internal PLATFORM_FILE_ERROR(Win32FileError) {
#if HANDMADE_INTERNAL
    printf("WIN32 FILE ERROR: %s\n", Message);
#endif
    Handle->NoErrors = false;
}

internal PLATFORM_READ_DATA_FROM_FILE(Win32ReadDataFromFile) {
    if (PlatformNoFileErrors(Source)) {
        win32_platform_file_handle *Handle = (win32_platform_file_handle *)Source->Platform;

        OVERLAPPED Overlapped = {};
        Overlapped.Offset = (u32)((Offset >> 0) & 0xFFFFFFFF);
        Overlapped.OffsetHigh = (u32)((Offset >> 32) & 0xFFFFFFFF);

        u32 FileSize32 = SafeTruncateUInt64(Size);

        DWORD BytesRead;
        if (ReadFile(Handle->Win32Handle, Dest, FileSize32, &BytesRead, &Overlapped) &&
            (FileSize32 == BytesRead)) {
            // NOTE: File read successfully
        } else {
            Win32FileError(Source, "Read file failed.");
        }
    }
}

/*
internal PLATFORM_FILE_ERROR(Win32CloseFile) {
    CloseHandle(FileHandle);
}
*/

PLATFORM_ALLOCATE_MEMORY(Win32AllocateMemory) {
    void *Result = VirtualAlloc(0, Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    return Result;
}

PLATFORM_DEALLOCATE_MEMORY(Win32DeallocateMemory) {
    if (Memory) {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

inline void
Win32RecordTimestamp(debug_frame_end_info *Info, char *Name, r32 Seconds) {
    Assert(Info->TimestampCount < ArrayCount(Info->Timestamps));

    debug_frame_timestamp *Timestamp = Info->Timestamps + Info->TimestampCount++;
    Timestamp->Name = Name;
    Timestamp->Seconds = Seconds;
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nCmdShow)
{
    win32_state Win32State = {};

    platform_work_queue HighPriorityQueue = {};
    Win32MakeQueue(&HighPriorityQueue, 6);

    platform_work_queue LowPriorityQueue = {};
    Win32MakeQueue(&LowPriorityQueue, 2);


#if 0
    Win32AddEntry(&Queue, DoWorkerWork, "String A0");
    Win32AddEntry(&Queue, DoWorkerWork, "String A1");
    Win32AddEntry(&Queue, DoWorkerWork, "String A2");
    Win32AddEntry(&Queue, DoWorkerWork, "String A3");
    Win32AddEntry(&Queue, DoWorkerWork, "String A4");
    Win32AddEntry(&Queue, DoWorkerWork, "String A5");
    Win32AddEntry(&Queue, DoWorkerWork, "String A6");
    Win32AddEntry(&Queue, DoWorkerWork, "String A7");
    Win32AddEntry(&Queue, DoWorkerWork, "String A8");
    Win32AddEntry(&Queue, DoWorkerWork, "String A9");

    Win32AddEntry(&Queue, DoWorkerWork, "String B0");
    Win32AddEntry(&Queue, DoWorkerWork, "String B1");
    Win32AddEntry(&Queue, DoWorkerWork, "String B2");
    Win32AddEntry(&Queue, DoWorkerWork, "String B3");
    Win32AddEntry(&Queue, DoWorkerWork, "String B4");
    Win32AddEntry(&Queue, DoWorkerWork, "String B5");
    Win32AddEntry(&Queue, DoWorkerWork, "String B6");
    Win32AddEntry(&Queue, DoWorkerWork, "String B7");
    Win32AddEntry(&Queue, DoWorkerWork, "String B8");
    Win32AddEntry(&Queue, DoWorkerWork, "String B9");

    Win32CompleteAllWork(&Queue);
#endif

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

    char GameCodeLockFullpath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "lock.tmp",
                              sizeof(GameCodeLockFullpath), GameCodeLockFullpath);


    // NOTE: Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

    Win32LoadXInput();

#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif

    WNDCLASS WindowClass = {};

    /* NOTE: 1080p display mode is 1920x1080 -> Half of that is 960x540
       1920 -> 2048 = 2048 - 1920 -> 128 pixels
       1080 -> 2048 = 2048 - 1080 -> 968 pixels
       1024 + 128 = 1152
     */
    //Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);
    Win32ResizeDIBSection(&GlobalBackBuffer, 1920, 1080);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
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
            real32 GameUpdateHz = (real32)(MonitorRefreshHz / 2.0f);
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

            // TOOD: Remove MaxPossibleOverrun
            u32 MaxPossibleOverrun = 2 * 4 * sizeof(u16);
            int16 *Samples = (int16 *) VirtualAlloc(0, SoundOutput.SecondaryBufferSize + MaxPossibleOverrun,
                                                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID) Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Megabytes(256);
            GameMemory.DebugStorageSize = Megabytes(64);
            GameMemory.HighPriorityQueue = &HighPriorityQueue;
            GameMemory.LowPriorityQueue = &LowPriorityQueue;

            GameMemory.PlatformAPI.AddEntry = Win32AddEntry;
            GameMemory.PlatformAPI.CompleteAllWork = Win32CompleteAllWork;


            GameMemory.PlatformAPI.GetAllFilesOfTypeBegin = Win32GetAllFilesOfTypeBegin;
            GameMemory.PlatformAPI.GetAllFilesOfTypeEnd = Win32GetAllFilesOfTypeEnd;
            GameMemory.PlatformAPI.OpenNextFile = Win32OpenNextFile;
            GameMemory.PlatformAPI.ReadDataFromFile = Win32ReadDataFromFile;
            GameMemory.PlatformAPI.FileError = Win32FileError;

            GameMemory.PlatformAPI.AllocateMemory = Win32AllocateMemory;
            GameMemory.PlatformAPI.DeallocateMemory = Win32DeallocateMemory;

            GameMemory.PlatformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.PlatformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.PlatformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;

            // TODO: Handle various memory footprints (USING SYSTEM METRICS)

            // TODO: Use MEM_LARGE_PAGES and call adjust token
            // privileges when not on Windows XP

            // TODO: TransientStorage needs to be broken up
            // into game transient and cache transient, and only the
            // former need be saved for state playback.
            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize + GameMemory.DebugStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress,
                                                 (size_t)Win32State.TotalSize,
                                                 MEM_RESERVE | MEM_COMMIT,
                                                 PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);
            GameMemory.DebugStorage = ((uint8 *)GameMemory.TransientStorage + GameMemory.TransientStorageSize);

            for (size_t ReplayIndex = 1; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ++ReplayIndex) {
                win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

                // TODO: Recording system still seems to take too long
                // on record start - find out what Windodws is doing and if
                // we can spped up / defer some of that processing.

                Win32GetInputFileLocation(&Win32State, false, (int)ReplayIndex,
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
                                                         TempGameCodeDLLFullpath,
                                                         GameCodeLockFullpath);

                uint64 LastCycleCount = __rdtsc();
                while (GlobalRunning) {
                    debug_frame_end_info FrameEndInfo = {};

                    NewInput->dtForFrame = TargetSecondsPerFrame;

                    NewInput->ExecutableReloaded = false;
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullpath);
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0) {
                        Win32CompleteAllWork(&HighPriorityQueue);
                        Win32CompleteAllWork(&LowPriorityQueue);

                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullpath,
                                                 TempGameCodeDLLFullpath,
                                                 GameCodeLockFullpath);
                        NewInput->ExecutableReloaded = true;
                    }

                    Win32RecordTimestamp(&FrameEndInfo, "ExecutableReady", Win32GetSecondsElapsed(LastCounter, Win32GetWallClock()));

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
                    }

                    Win32RecordTimestamp(&FrameEndInfo, "InputProcessed", Win32GetSecondsElapsed(LastCounter, Win32GetWallClock()));

                    if (!GlobalPause) {
                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackBuffer.Memory;
                        Buffer.Width = GlobalBackBuffer.Width;
                        Buffer.Height = GlobalBackBuffer.Height;
                        Buffer.Pitch = GlobalBackBuffer.Pitch;

                        if (Win32State.InputRecordingIndex) {
                            Win32RecordInput(&Win32State, NewInput);
                        }

                        if (Win32State.InputPlayingIndex) {
                            Win32PlayBackInput(&Win32State, NewInput);
                        }
                        if (Game.UpdateAndRender) {
                            Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
                            //HandleDebugCycleCounters(&GameMemory);
                        }
                    }

                    Win32RecordTimestamp(&FrameEndInfo, "GameUpdated", Win32GetSecondsElapsed(LastCounter, Win32GetWallClock()));

                    if (!GlobalPause) {
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

                            DWORD ExpectedSoundBytesPerFrame = (DWORD) ((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);
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
                            SoundBuffer.SampleCount = Align8(BytesToWrite / SoundOutput.BytesPerSample);
                            BytesToWrite = SoundBuffer.SampleCount * SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            if (Game.GetSoundSamples) {
                                Game.GetSoundSamples(&GameMemory, &SoundBuffer);
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

                    }

                    Win32RecordTimestamp(&FrameEndInfo, "AudioUpdated", Win32GetSecondsElapsed(LastCounter, Win32GetWallClock()));

                    if (!GlobalPause) {
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
                    }

                    Win32RecordTimestamp(&FrameEndInfo, "FramerateWaitComplete", Win32GetSecondsElapsed(LastCounter, Win32GetWallClock()));

                    if (!GlobalPause) {
                        win32_window_dimension Dimension = Win32GetWindowDimension(Window);

                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                        ReleaseDC(Window, DeviceContext);

                        FlipWallClock = Win32GetWallClock();

                        game_input *Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;

                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        LastCounter = EndCounter;

#if HANDMADE_INTERNAL
                        Win32RecordTimestamp(&FrameEndInfo, "EndOfFrame", Win32GetSecondsElapsed(LastCounter, EndCounter));

                        uint64 EndCycleCount = __rdtsc();
                        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        if (Game.DEBUGFrameEnd) {
                            Game.DEBUGFrameEnd(&GameMemory, &FrameEndInfo);
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
