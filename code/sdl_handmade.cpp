#include "handmade_platform.h"

#include <SDL2/SDL.h>
#include <unistd.h>
#include <libproc.h>

#include "sdl_handmade.h"

global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable sdl_offscreen_buffer GlobalBackBuffer;
//global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;
global_variable bool32 DEBUGGlobalShowCursor;

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

internal void
SDLResizeDIBSection(sdl_offscreen_buffer *Buffer, int Width, int Height) {
    if (Buffer->Memory) {
    }
}

int main(int argc, char *argv[]) {
    sdl_state SDLState = {};

    GlobalPerfCountFrequency = SDL_GetPerformanceCounter();

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

#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    return 0;
}
