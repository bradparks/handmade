#include "handmade_platform.h"

#include <SDL2/SDL.h>
#include <unistd.h>
#include <libproc.h>
#include <sys/mman.h>

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
                    DEBUGPlatformFreeFileMemory(Result.Contents);
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


inline time_t
SDLGetLastWriteTime(const char *Filename) {
    time_t LastWriteTime = {};
    struct stat Stat;

    if (stat(Filename, &Stat) == 0) {
        LastWriteTime = Stat.st_mtime;
    }

    return LastWriteTime;
}

internal void
SDLCopyFile(const char *SourceFileName, const char *DestFileName) {
    SDL_RWops *Source = SDL_RWFromFile(SourceFileName, "rb");
    if (Source) {
        SDL_RWops *Dest = SDL_RWFromFile(DestFileName, "wb");
        if (Dest) {
            char Buf[4096];
            size_t BytesRead;
            while ((BytesRead = SDL_RWread(Source, Buf, sizeof(*Buf), ArrayCount(Buf)))) {
                SDL_RWwrite(Dest, Buf, sizeof(*Buf), BytesRead);
            }

            SDL_RWclose(Dest);
            SDL_RWclose(Source);
        } else {
            printf("Can't open file %s for writing: %s\n", DestFileName, SDL_GetError());
            SDL_RWclose(Source);
        }
    } else {
        printf("Can't open file %s for reading: %s\n", SourceFileName, SDL_GetError());
    }
}

internal sdl_game_code
SDLLoadGameCode(const char *SourceDLLName, const char *TempDLLName,
                const char *LockFileName)
{
    sdl_game_code Result = {};

    if (access(LockFileName, F_OK) == -1) {
        Result.DLLLastWriteTime = SDLGetLastWriteTime(SourceDLLName);
        SDLCopyFile(SourceDLLName, TempDLLName);
        Result.GameCodeDLL = SDL_LoadObject(TempDLLName);

        if (Result.GameCodeDLL) {
            Result.UpdateAndRender = (game_update_and_render *)
                SDL_LoadFunction(Result.GameCodeDLL, "GameUpdateAndRender");
            Result.GetSoundSamples = (game_get_sound_samples *)
                SDL_LoadFunction(Result.GameCodeDLL, "GameGetSoundSamples");

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
SDLUnloadGameCode(sdl_game_code *GameCode) {
    if (GameCode->GameCodeDLL) {
        SDL_UnloadObject(GameCode->GameCodeDLL);
    }

    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal SDL_AudioDeviceID
SDLInitSound(int32 SamplesPerSecond) {
    SDL_AudioSpec spec = {};
    spec.freq = SamplesPerSecond;
    spec.format = AUDIO_S16;
    spec.channels = 2;
    spec.samples = 4096;

    SDL_AudioDeviceID Result = SDL_OpenAudioDevice(NULL, 0, &spec, 0, 0);
    if (Result != 0) {
        SDL_PauseAudioDevice(Result, 0);
    } else {
        printf("Failed to open SDL audio device: %s\n", SDL_GetError());
    }

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
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        Buffer->Width, Buffer->Height);
    Buffer->Pitch = Align16(Width * BytesPerPixel);
    int BitmapMemorySize = Buffer->Pitch * Buffer->Height;
    Buffer->Memory = mmap(0, BitmapMemorySize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANON, -1, 0);
}

internal void
SDLProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown) {
    if (NewState->EndedDown != IsDown) {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

internal void
SDLProcessPendingMessage(sdl_state *State, game_controller_input *KeyboardController) {
    SDL_Event Event;
    while (SDL_PollEvent(&Event)) {
        switch (Event.type) {
            case SDL_QUIT: {
                GlobalRunning = false;
            } break;

            case SDL_KEYUP:
            case SDL_KEYDOWN: {
                SDL_Keycode Key = Event.key.keysym.sym;

                bool IsDown = Event.key.state == SDL_PRESSED;
                bool WasDown = Event.key.state == SDL_RELEASED;

                if (WasDown != IsDown) {
                    if (Key == SDLK_w) {
                        SDLProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    } else if (Key == SDLK_a) {
                        SDLProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    } else if (Key == SDLK_s) {
                        SDLProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    } else if (Key == SDLK_d) {
                        SDLProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    } else if (Key == SDLK_q) {
                        SDLProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    } else if (Key == SDLK_e) {
                        SDLProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    } else if (Key == SDLK_UP) {
                        SDLProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    } else if (Key == SDLK_LEFT) {
                        SDLProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    } else if (Key == SDLK_RIGHT) {
                        SDLProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    } else if (Key == SDLK_DOWN) {
                        SDLProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    } else if (Key == SDLK_ESCAPE) {
                        SDLProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                        GlobalRunning = false;
                    } else if (Key == SDLK_SPACE) {
                        SDLProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                }
            } break;

            default: break;
        }
    }
}

internal void
SDLDisplayBufferInWindow(sdl_offscreen_buffer *Buffer) {
    SDL_UpdateTexture(Buffer->Texture, 0, Buffer->Memory, Buffer->Pitch);
    SDL_RenderCopyEx(Buffer->Renderer, Buffer->Texture, 0, 0, 0, 0, SDL_FLIP_VERTICAL);
    SDL_RenderPresent(Buffer->Renderer);
}

struct platform_work_queue_entry {
    platform_work_queue_callback *Callback;
    void *Data;
};

struct platform_work_queue {
    int32 volatile CompletionGoal;
    SDL_atomic_t CompletionCount;

    SDL_atomic_t NextEntryToWrite;
    SDL_atomic_t NextEntryToRead;

    SDL_sem *Sem;

    platform_work_queue_entry Entries[256];
};

internal void
SDLAddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data) {
    int NewNextEntryToWrite = (Queue->NextEntryToWrite.value + 1 ) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead.value);
    platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite.value;
    Entry->Callback = Callback;
    Entry->Data = Data;
    ++Queue->CompletionGoal;

    SDL_CompilerBarrier();

    Queue->NextEntryToWrite.value = NewNextEntryToWrite;
    SDL_SemPost(Queue->Sem);
}

internal bool32
SDLDoNextWorkQueueEntry(platform_work_queue *Queue) {
    bool32 WeShouldSleep = false;

    int OriginalNextEntryToRead = Queue->NextEntryToRead.value;
    int NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if (OriginalNextEntryToRead != Queue->NextEntryToWrite.value) {
        if (SDL_AtomicCAS(&Queue->NextEntryToRead, OriginalNextEntryToRead, NewNextEntryToRead)) {
            platform_work_queue_entry Entry = Queue->Entries[OriginalNextEntryToRead];
            Entry.Callback(Queue, Entry.Data);
            SDL_AtomicIncRef(&Queue->CompletionCount);
        }
    } else {
        WeShouldSleep = true;
    }

    return WeShouldSleep;
}

internal void
SDLCompleteAllWork(platform_work_queue *Queue) {
    while (Queue->CompletionGoal != Queue->CompletionCount.value) {
        SDLDoNextWorkQueueEntry(Queue);
    }

    Queue->CompletionGoal = 0;
    Queue->CompletionCount.value = 0;
}

internal int
ThreadProc(void *Data) {
    platform_work_queue *Queue = (platform_work_queue *)Data;

    for (;;) {
        if (SDLDoNextWorkQueueEntry(Queue)) {
            SDL_SemWait(Queue->Sem);
        }
    }
}

internal void
SDLMakeQueue(platform_work_queue *Queue, uint32 ThreadCount) {
    Queue->CompletionGoal = 0;
    Queue->CompletionCount.value = 0;

    Queue->NextEntryToWrite.value = 0;
    Queue->NextEntryToRead.value = 0;

    uint32 InitialCount = 0;
    Queue->Sem = SDL_CreateSemaphore(InitialCount);

    for (uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex) {
        SDL_Thread *Thread = SDL_CreateThread(ThreadProc, 0, Queue);
    }
}

int main(int argc, char *argv[]) {
    sdl_state SDLState = {};

    platform_work_queue HighPriorityQueue = {};
    SDLMakeQueue(&HighPriorityQueue, 6);

    platform_work_queue LowPriorityQueue = {};
    SDLMakeQueue(&LowPriorityQueue, 2);


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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *Window = SDL_CreateWindow("Handmade Hero",
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

    sdl_sound_output SoundOutput = {};
    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.BytesPerSample = sizeof(int16) * 2;
    SoundOutput.BufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

    SDL_AudioDeviceID Audio = SDLInitSound(SoundOutput.SamplesPerSecond);

    u32 MaxPossibleOverrun = 2 * 4 * sizeof(u16);
    int16 *Samples = (int16 *)mmap(0, (size_t)(SoundOutput.BufferSize + MaxPossibleOverrun),
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANON, -1, 0);

    GlobalRunning = true;

#if HANDMADE_INTERNAL
    void *BaseAddress = (void *)Terabytes(2);
#else
    void *BaseAddress = 0;
#endif

    game_memory GameMemory = {};
    GameMemory.PermanentStorageSize = Megabytes(64);
    GameMemory.TransientStorageSize = Gigabytes(1);
    GameMemory.HighPriorityQueue = &HighPriorityQueue;
    GameMemory.LowPriorityQueue = &LowPriorityQueue;
    GameMemory.PlatformAddEntry = SDLAddEntry;
    GameMemory.PlatformCompleteAllWork = SDLCompleteAllWork;
    GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
    GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
    GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

    SDLState.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

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

    while (GlobalRunning) {
        NewInput->dtForFrame = TargetSecondsPerFrame;

        NewInput->ExecutableReloaded = false;
        time_t NewDLLWriteTime = SDLGetLastWriteTime(SourceGameCodeDLLFullpath);
        if (difftime(NewDLLWriteTime, Game.DLLLastWriteTime) > 0) {
            SDLCompleteAllWork(&HighPriorityQueue);
            SDLCompleteAllWork(&LowPriorityQueue);

            SDLUnloadGameCode(&Game);
            Game = SDLLoadGameCode(SourceGameCodeDLLFullpath,
                                   TempGameCodeDLLFullpath,
                                   GameCodeLockFullpath);
            NewInput->ExecutableReloaded = true;
        }

        game_controller_input *OldKeyboardController = GetController(OldInput, 0);
        game_controller_input *NewKeyboardController = GetController(NewInput, 0);
        *NewKeyboardController = {};
        NewKeyboardController->IsConnected = true;
        for (size_t ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex) {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
        }

        SDLProcessPendingMessage(&SDLState, NewKeyboardController);

        if (!GlobalPause) {
            Uint32 MouseButtons = SDL_GetMouseState(&NewInput->MouseX, &NewInput->MouseY);
            NewInput->MouseZ = 0;
            SDLProcessKeyboardMessage(&NewInput->MouseButtons[0],
                                      SDL_BUTTON(SDL_BUTTON_LEFT));
            SDLProcessKeyboardMessage(&NewInput->MouseButtons[1],
                                      SDL_BUTTON(SDL_BUTTON_MIDDLE));
            SDLProcessKeyboardMessage(&NewInput->MouseButtons[2],
                                      SDL_BUTTON(SDL_BUTTON_RIGHT));
            SDLProcessKeyboardMessage(&NewInput->MouseButtons[3],
                                      SDL_BUTTON(SDL_BUTTON_X1));
            SDLProcessKeyboardMessage(&NewInput->MouseButtons[4],
                                      SDL_BUTTON(SDL_BUTTON_X2));

            // TODO: Handle Mouse button here

            // TODO: Game controller support here

            game_offscreen_buffer Buffer = {};
            Buffer.Memory = GlobalBackBuffer.Memory;
            Buffer.Width = GlobalBackBuffer.Width;
            Buffer.Height = GlobalBackBuffer.Height;
            Buffer.Pitch = GlobalBackBuffer.Pitch;

            if (Game.UpdateAndRender) {
                Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
                HandleDebugCycleCounters(&GameMemory);
            }

            // TODO: Game audio support here
            game_sound_output_buffer SoundBuffer = {};
            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
            SoundBuffer.SampleCount = Align8((u32)(SoundOutput.SamplesPerSecond * TargetSecondsPerFrame));
            SoundBuffer.Samples = Samples;
            if (Game.GetSoundSamples) {
                Game.GetSoundSamples(&GameMemory, &SoundBuffer);
                SDL_QueueAudio(Audio, SoundBuffer.Samples, SoundBuffer.SampleCount * SoundOutput.BytesPerSample);
            }

            SDLDisplayBufferInWindow(&GlobalBackBuffer);

            game_input *Temp = NewInput;
            NewInput = OldInput;
            OldInput = Temp;
        }
    }

    return 0;
}
