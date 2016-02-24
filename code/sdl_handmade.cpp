#include "handmade_platform.h"

#include <SDL2/SDL.h>
#include <unistd.h>
#include <libproc.h>
#include <sys/mman.h>
#include <x86intrin.h>

#include "sdl_handmade.h"

global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable sdl_offscreen_buffer GlobalBackBuffer;
//global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

internal void
CatStrings(size_t SourceACount, const char *SourceA,
           size_t SourceBCount, const char *SourceB,
           size_t DestCount, char *Dest)
{
    for (size_t Index = 0; Index < SourceACount && Index < DestCount; ++Index) {
        *Dest++ = *SourceA++;
    }

    for (size_t Index = 0; Index < SourceBCount && Index < DestCount; ++Index) {
        *Dest++ = *SourceB++;
    }

    *Dest++ = 0;
}

internal void
SDLGetEXEFileName(sdl_state *State) {
    pid_t pid = getpid();
    proc_pidpath(pid, State->EXEFileName, SDL_STATE_FILE_NAME_COUNT);
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for (char *Scan = State->EXEFileName; *Scan; ++Scan) {
        if (*Scan == '/') {
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
SDLBuildEXEPathFileName(sdl_state *State, const char *FileName,
                        int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName,
               State->EXEFileName,
               StringLength(FileName), FileName,
               DestCount, Dest);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
    if (Memory) {
        size_t *Raw = (size_t *)Memory - 1;
        munmap(Raw, *Raw);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
    debug_read_file_result Result = {};

    SDL_RWops *FileHandle = SDL_RWFromFile(Filename, "rb");

    if (FileHandle) {
        Sint64 FileSize = SDL_RWsize(FileHandle);
        if (FileSize != -1) {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize);
            uint32 MemorySize = FileSize32 + sizeof(size_t);
            size_t *Contents = (size_t *)mmap(0, MemorySize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
            if (Contents) {
                // Add the size of memory to the head the of this block
                *Contents++ = MemorySize;
                Result.Contents = Contents;

                size_t BytesRead;
                if ((BytesRead = SDL_RWread(FileHandle, Result.Contents, 1, FileSize32)) &&
                    (BytesRead == FileSize32))
                {
                    Result.ContentsSize = FileSize32;
                } else {
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = 0;
                }
            } else {
                // TODO: Logging
            }
        } else {
            // TODO: Logging
        }

        SDL_RWclose(FileHandle);
    } else {
        // TODO: Logging
    }

    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
    bool32 Result = false;

    SDL_RWops *FileHandle = SDL_RWFromFile(Filename, "wb");
    if (FileHandle) {
        size_t BytesWritten;
        if ((BytesWritten = SDL_RWwrite(FileHandle, Memory, 1, MemorySize))) {
            // NOTE: FileHandle write successfully
            Result = BytesWritten == MemorySize;
        } else {
            // TODO: Logging
        }
        SDL_RWclose(FileHandle);
    } else {
        // TODO: Logging
    }

    return Result;
}

inline time_t
SDLGetLastWriteTime(const char *Filename) {
    time_t LastWriteTime = {};
    struct stat Stat;

    if (stat(Filename, &Stat) == 0) {
        LastWriteTime = Stat.st_mtime;
    }

    return LastWriteTime;
}

internal sdl_game_code
SDLLoadGameCode(const char *SourceDLLName, const char *TempDLLName,
                const char *LockFileName)
{
    sdl_game_code Result = {};

    if (access(LockFileName, F_OK) == -1) {
        Result.DLLLastWriteTime = SDLGetLastWriteTime(SourceDLLName);
    }

#if 0

    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if (!GetFileAttributesEx(LockFileName, GetFileExInfoStandard, &Ignored)) {
        Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

        CopyFile(SourceDLLName, TempDLLName, FALSE);
        Result.GameCodeDLL = LoadLibrary(TempDLLName);

        if (Result.GameCodeDLL) {
            Result.UpdateAndRender = (game_update_and_render *) GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
            Result.GetSoundSamples = (game_get_sound_samples *) GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

            Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
        }
    }

    if (!Result.IsValid) {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }

#endif
    return Result;
}

internal void
SDLResizeDIBSection(SDL_Window *Window, sdl_offscreen_buffer *Buffer,
                    int Width, int Height)
{
    int BytesPerPixel = 4;

    if (!Buffer->Renderer) {
        Buffer->Renderer = SDL_CreateRenderer(Window, -1,
                                              SDL_RENDERER_ACCELERATED |
                                              SDL_RENDERER_PRESENTVSYNC);
    }

    if (Buffer->Memory) {
        SDL_DestroyTexture(Buffer->Texture);
        int BitmapMemorySize = Buffer->Width * Buffer->Height * BytesPerPixel;
        munmap(Buffer->Memory, BitmapMemorySize);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = BytesPerPixel;
    Buffer->Texture = SDL_CreateTexture(Buffer->Renderer,
                                        SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        Buffer->Width, Buffer->Height);

    int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
    Buffer->Memory = mmap(0, BitmapMemorySize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANON, -1, 0);
    Buffer->Pitch = Width * BytesPerPixel;
}

int main(int argc, char *argv[]) {
    sdl_state SDLState = {};

    GlobalPerfCountFrequency = SDL_GetPerformanceFrequency();

    SDLGetEXEFileName(&SDLState);

    char SourceGameCodeDLLFullpath[SDL_STATE_FILE_NAME_COUNT];
    SDLBuildEXEPathFileName(&SDLState, "handmade.dylib",
                            sizeof(SourceGameCodeDLLFullpath), SourceGameCodeDLLFullpath);

    char TempGameCodeDLLFullpath[SDL_STATE_FILE_NAME_COUNT];
    SDLBuildEXEPathFileName(&SDLState, "handmade_temp.dylib",
                            sizeof(TempGameCodeDLLFullpath), TempGameCodeDLLFullpath);

    char GameCodeLockFullpath[SDL_STATE_FILE_NAME_COUNT];
    SDLBuildEXEPathFileName(&SDLState, "lock.tmp",
                            sizeof(GameCodeLockFullpath), GameCodeLockFullpath);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *Window = SDL_CreateWindow("Breakout",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          960, 540,
                                          SDL_WINDOW_OPENGL);
    if (!Window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        return -1;
    }

    SDLResizeDIBSection(Window, &GlobalBackBuffer, 960, 540);

    // TODO: Set GameUpdateHz by monitor refresh HZ
    real32 GameUpdateHz = 60.0f;
    real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

    GlobalRunning = true;

#if HANDMADE_INTERNAL
    void *BaseAddress = (void *)Terabytes(2);
#else
    void *BaseAddress = 0;
#endif

    game_memory GameMemory = {};
    GameMemory.PermanentStorageSize = Megabytes(64);
    GameMemory.TransientStorageSize = Gigabytes(1);
    GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
    GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
    GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

    SDLState.TotalSize = GameMemory.PermanentStorageSize + GameMemory.PermanentStorageSize;

    SDLState.GameMemoryBlock = mmap(BaseAddress,
                                    (size_t) SDLState.TotalSize,
                                    PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANON, -1, 0);
    GameMemory.PermanentStorage = SDLState.GameMemoryBlock;
    GameMemory.TransientStorage = ((uint8 *) GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

    if (!(GameMemory.PermanentStorage && GameMemory.TransientStorage)) {
        printf("Failed to allocate game memory\n");
        return -1;
    }

    // TODO: Add game replay support here

    game_input Input[2] = {};
    game_input *NewInput = &Input[0];
    game_input *OldInput = &Input[1];

    uint64 LastCounter = SDL_GetPerformanceCounter();
    uint64 FlipWallClock = SDL_GetPerformanceCounter();

    sdl_game_code Game = SDLLoadGameCode(SourceGameCodeDLLFullpath,
                                         TempGameCodeDLLFullpath,
                                         GameCodeLockFullpath);


    uint64 LastCycleCount = _rdtsc();

    return 0;
}
