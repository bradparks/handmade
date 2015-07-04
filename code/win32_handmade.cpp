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
#include <stdio.h>
#include <stdint.h>
// TODO: Implement sine ourselves
#include <math.h>

#define internal static
#define global_variable static
#define local_persist static

#define PI32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#include "handmade.cpp"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO: This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

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

    // TODO: Probably clear this to black

    Buffer->Pitch = Width * BytesPerPixel;
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
            uint32 VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown = ((LParam & (1 << 31)) == 0);

            if (WasDown != IsDown) {
                if (VKCode == 'W') {
                } else if (VKCode == 'A') {
                } else if (VKCode == 'S') {
                } else if (VKCode == 'D') {
                } else if (VKCode == 'Q') {
                } else if (VKCode == 'E') {
                } else if (VKCode == VK_UP) {
                } else if (VKCode == VK_LEFT) {
                } else if (VKCode == VK_RIGHT) {
                } else if (VKCode == VK_DOWN) {
                } else if (VKCode == VK_UP) {
                } else if (VKCode == VK_ESCAPE) {
                } else if (VKCode == VK_SPACE) {
                }
            }

            bool32 AltKeyWasDown = (LParam & (1 << 29));
            if ((VKCode == VK_F4) && AltKeyWasDown) {
                GlobalRunning = false;
            }
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
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state OldState, DWORD ButtonBit,
                                game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nCmdShow)
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32LoadXInput();

    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

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

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            bool32 SoundIsPlaying = false;

            GlobalRunning = true;

            int16 *Samples = (int16 *) VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            game_input Input[2] = {};
            game_input *NewInput = &Input[0];
            game_input *OldInput = &Input[1];


            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);

            uint64 LastCycleCount = __rdtsc();
            while (GlobalRunning) {
                MSG Message;

                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
                    if (Message.message == WM_QUIT) {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                // TODO: Should we poll this more frequently
                int MaxControllerCount = XUSER_MAX_COUNT;
                if (MaxControllerCount > ArrayCount(NewInput.Controllers)) {
                    MaxControllerCount = ArrayCount(NewInput.Controllers);
                }

                for (DWORD ControllerIndex = 0;
                     ControllerIndex < MaxControllerCount;
                     ++ControllerIndex)
                {
                    game_controller_input *OldController = OldInput->Controllers[ControllerIndex];
                    game_controller_input *NewController = NewInput->Controllers[ControllerIndex];

                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                        // NOTE: This controller is plugged in
                        // TODO: See if ControllerState.dwPacketNumber increments too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                        NewController->IsAnalog = true;
                        NewController->StartX = OldController->EndX;
                        NewController->StartY = OldController->EndY;

                        real32 X;
                        if (Pad->sThumbLX < 0) {
                            X = (real32) Pad->sThumbLX / 32768.0f;
                        } else {
                            X = (real32) Pad->sThumbLX / 32767.0f;
                        }

                        NewController->MinX = NewController->MaxX = NewController->EndX = X;

                        real32 Y;
                        if (Pad->sThumbLY < 0) {
                            Y = (real32) Pad->sThumbLY / 32768.0f;
                        } else {
                            Y = (real32) Pad->sThumbLY / 32767.0f;
                        }

                        NewController->MinY = NewController->MaxY = NewController->EndY = Y;

                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                        &OldController->Down,
                                                        XINPUT_GAMEPAD_A,
                                                        &NewController->Down)

                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                        &OldController->Right,
                                                        XINPUT_GAMEPAD_B,
                                                        &NewController->Right)

                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                        &OldController->Left,
                                                        XINPUT_GAMEPAD_X,
                                                        &NewController->Left)

                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                        &OldController->Up,
                                                        XINPUT_GAMEPAD_Y,
                                                        &NewController->Up)

                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                        &OldController->LeftShoulder,
                                                        XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                        &NewController->LeftShoulder)

                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                        &OldController->RightShoulder,
                                                        XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                        &NewController->RightShoulder)

                        //bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        //bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);

                    } else {
                        // NOTE: This controller is not available
                    }
                }

                DWORD PlayCursor = 0;
                DWORD WriteCursor = 0;
                DWORD BytesToLock = 0;
                DWORD BytesToWrite = 0;
                bool32 SoundIsValid = false;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
                    BytesToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                    //DWORD TargetCursor = (PlayCursor + SoundOutput->LatencySampleCount * SoundOutput.BytesPerSample) % SoundOutput->SecondaryBufferSize;
                    if (BytesToLock > PlayCursor) {
                        BytesToWrite = (SoundOutput.SecondaryBufferSize  - BytesToLock);
                        BytesToWrite += PlayCursor;
                    } else {
                        BytesToWrite = PlayCursor - BytesToLock;
                    }

                    SoundIsValid = true;
                }

                game_sound_output_buffer SoundBuffer = {};
                SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                SoundBuffer.Samples = Samples;

                game_offscreen_buffer Buffer = {};
                Buffer.Memory = GlobalBackBuffer.Memory;
                Buffer.Width = GlobalBackBuffer.Width;
                Buffer.Height = GlobalBackBuffer.Height;
                Buffer.Pitch = GlobalBackBuffer.Pitch;

                GameUpdateAndRender(NewInput, &Buffer, &SoundBuffer);

                // NOTE: DirectSound output test
                if (SoundIsValid) {
                    Win32FillSoundBuffer(&SoundOutput, BytesToLock, BytesToWrite, &SoundBuffer);
                }

                if (!SoundIsPlaying) {
                    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
                    SoundIsPlaying = true;
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

                uint64 EndCycleCount = __rdtsc();

                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);

                uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                int32 MSPerFrame = (int32) ((1000 * CounterElapsed) / PerfCountFrequency);
                int32 FPS = PerfCountFrequency / CounterElapsed;
                uint32 MCPF = (uint32) (CyclesElapsed / (1000 * 1000));

                printf("%dms/f, %df/s, %umc/f\n", MSPerFrame, FPS, MCPF);

                LastCounter = EndCounter;
                LastCycleCount = EndCycleCount;

                game_input *Temp = NewInput;
                NewInput = OldInput;
                OldInput = Temp;
            }
        } else {
            // TODO: Logging
        }
    } else {
        // TODO: Logging
    }

    return 0;
}
