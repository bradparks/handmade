#include <stdio.h>
#include <stdint.h>
// TODO: Implement sine ourselves
#include <math.h>
#include <windows.h>
#include <xinput.h>
#include <dsound.h>

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

struct win32_offscreen_buffer {
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension {
    int Width;
    int Height;
};

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
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *) Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y) {
        uint32 *Pixel = (uint32 *) Row;
        for (int X = 0; X < Buffer->Width; ++X) {
            uint8 Blue = (X + XOffset);
            uint8 Green = (Y + YOffset);
            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
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
    Buffer->BytesPerPixel = 4;

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

    int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    // TODO: Probably clear this to black

    Buffer->Pitch = Width * Buffer->BytesPerPixel;
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

struct win32_sound_output {
    int SamplesPerSecond;
    int ToneHz;
    int ToneVolumn;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
};

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD BytesToLock, DWORD BytesToWrite)
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
        int16 *SimpleOut = (int16 *) Region1;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
            real32 t = 2.0f * PI32 * (real32) SoundOutput->RunningSampleIndex / (real32) SoundOutput->WavePeriod;
            real32 SineValue = sinf(t);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolumn);
            *SimpleOut++ = SampleValue;
            *SimpleOut++ = SampleValue;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        SimpleOut = (int16 *) Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
            real32 t = 2.0f * PI32 * (real32) SoundOutput->RunningSampleIndex / (real32) SoundOutput->WavePeriod;
            real32 SineValue = sinf(t);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolumn);
            *SimpleOut++ = SampleValue;
            *SimpleOut++ = SampleValue;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
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

            // NOTE: Graphics test
            int XOffset = 0;
            int YOffset = 0;

            // NOTE: Sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolumn = 3000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            bool32 SoundIsPlaying = false;

            GlobalRunning = true;

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
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex) {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                        // NOTE: This controller is plugged in
                        // TODO: See if ControllerState.dwPacketNumber increments too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLX;

                        XOffset += StickX >> 12;
                        YOffset += StickY >> 12;
                    } else {
                        // NOTE: This controller is not available
                    }
                }

                RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

                // NOTE: DirectSound output test
                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
                    DWORD BytesToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                    DWORD BytesToWrite;
                    // TODO: We need a more accurate check than BytesToLock == PlayCursor
                    if (BytesToLock > PlayCursor) {
                        BytesToWrite = (SoundOutput.SecondaryBufferSize  - BytesToLock);
                        BytesToWrite += PlayCursor;
                    } else {
                        BytesToWrite = PlayCursor - BytesToLock;
                    }

                    Win32FillSoundBuffer(&SoundOutput, BytesToLock, BytesToWrite);
                }

                if (!SoundIsPlaying) {
                    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
                    SoundIsPlaying = true;
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);

                ++XOffset;

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
            }
        } else {
            // TODO: Logging
        }
    } else {
        // TODO: Logging
    }

    return 0;
}
