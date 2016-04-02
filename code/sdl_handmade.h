#ifndef SDL_HANDMADE_H
#define SDL_HANDMADE_H

struct sdl_offscreen_buffer {
    SDL_Renderer *Renderer;
    SDL_Texture *Texture;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct sdl_game_code {
    void *GameCodeDLL;
    time_t DLLLastWriteTime;

    // IMPORTANT: Either of the callbacks can be 0! You must
    // check before calling.
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;

    bool32 IsValid;
};

struct sdl_sound_output {
    int SamplesPerSecond;
    int BytesPerSample;
    uint32 BufferSize;

    // TODO: Should running sample index be in tyeps as well
    // TODO: Math gets simpler if we add a "bytes per second" field?
};

#if 0
struct sdl_window_dimension {
    int Width;
    int Height;
};

struct sdl_debug_time_marker {
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;

    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};

struct sdl_recorded_input {
    int InputCount;
    game_input *InputStream;
};

struct sdl_replay_buffer {
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char FileName[sdl_STATE_FILE_NAME_COUNT];
    void *MemoryBlock;
};
#endif

#define SDL_STATE_FILE_NAME_COUNT PATH_MAX
struct sdl_state {
    memory_index TotalSize;
    void *GameMemoryBlock;

#if 0
    sdl_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlaybackHandle;
    int InputPlayingIndex;
#endif

    char EXEFileName[SDL_STATE_FILE_NAME_COUNT];
    char *OnePastLastEXEFileNameSlash;
};

#endif
