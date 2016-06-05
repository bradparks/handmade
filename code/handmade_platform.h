#ifndef HANDMADE_PLATFORM_H
#define HANDMADE_PLATFORM_H

/*
  NOTE:

  HANDMADE_INTERNAL:
  0 - Build for public release
  1 - Build for developer only

  HANDMADE_SLOW:
  0 - Not slow code allowd!
  1 - Slow code welcome.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdio.h>

//
// NOTE: Compilers
//

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO: More compilerz!!!
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SSE/NEON optimizations are not available for this compiler yet!!!
#endif

//
// NOTE: Types
//
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>

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

typedef int8 s8;
typedef int8 s08;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef int32 s32;
typedef bool32 b32;

typedef uint8 u8;
typedef uint8 u08;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef real32 r32;
typedef real64 r64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

#define Real32Maximum FLT_MAX

#if !defined(internal)
#define internal static
#endif
#define global_variable static
#define local_persist static

#define Pi32 3.14159265359f
#define Tau32 6.2831853071795864692f

#if HANDMADE_SLOW
#define Assert(Expression) assert(Expression)
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {Assert(!"InvalidCodePath");} break

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// TODO: swap, min, max ... macros ???

#define AlignPow2(Value, Alignment) ((Value + (Alignment - 1)) & ~(Alignment - 1))
#define Align4(Value) (((Value) + 3) & ~3)
#define Align8(Value) (((Value) + 7) & ~7)
#define Align16(Value) (((Value) + 15) & ~15)

inline uint32
SafeTruncateUInt64(uint64 Value) {
    // TODO: Defines for maximum values UInt32Max
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32) Value;
    return Result;
}

inline u16
SafeTruncateToUInt16(s32 Value) {
    // TODO: Defines for maximum values UInt32Max
    Assert(Value <= 65535);
    Assert(Value >= 0);
    u16 Result = (u16)Value;
    return Result;
}

inline s16
SafeTruncateToInt16(s32 Value) {
    // TODO: Defines for maximum values UInt32Max
    Assert(Value <= 32767);
    Assert(Value >= -32768);
    u16 Result = (s16)Value;
    return Result;
}

/*
 * NOTE: Services that the platform layer provides to the game.
 */
#if HANDMADE_INTERNAL
/* IMPORTANT:

   These are NOT for doing anything in the shipping game - they are
   blocking and the write doesn't protect against lost data!
 */
typedef struct debug_read_file_result {
    uint32 ContentsSize;
    void *Contents;
} debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(const char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(const char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

// TODO: Actually stoping using this
extern struct game_memory *DebugGlobalMemory;

#endif

/*
 * NOTE: Services that the game provides to the platform layer.
 */

#define BITMAP_BYTES_PER_PIXEL 4
typedef struct game_offscreen_buffer {
    void *Memory;
    int Width;
    int Height;
    int Pitch;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer {
    int SamplesPerSecond;
    int SampleCount;

    // IMPORTANT: Samples must be padded to a multiple of 4 samples!
    int16 *Samples;
} game_sound_output_buffer;

typedef struct game_button_state {
    int HalfTransitionCount;
    bool32 EndedDown;
} game_button_state;

typedef struct game_controller_input {
    bool32 IsConnected;
    bool32 IsAnalog;
    real32 StickAverageX;
    real32 StickAverageY;

    union {
        game_button_state Buttons[12];
        struct {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Back;
            game_button_state Start;

            // NOTE: All buttons must be added above this line
            game_button_state Terminator;
        };
    };
} game_controller_input;

typedef struct game_input {
    game_button_state MouseButtons[5];
    int32 MouseX, MouseY, MouseZ;

    bool32 ExecutableReloaded;
    real32 dtForFrame;

    game_controller_input Controllers[5];
} game_input;

typedef struct platform_file_handle {
    b32 NoErrors;
    void *Platform;
} platform_file_handle;

typedef struct platform_file_group {
    u32 FileCount;
    void *Platform;
} platform_file_group;

typedef enum platform_file_type {
    PlatformFileType_AssetFile,
    PlatformFileType_SavedGameFile,

    PlatformFileType_Count,
} platform_file_type;

#define PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(name) platform_file_group name(platform_file_type Type)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILES_OF_TYPE_END(name) void name(platform_file_group *FileGroup)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_NEXT_FILE(name) platform_file_handle name(platform_file_group *FileGroup)
typedef PLATFORM_OPEN_NEXT_FILE(platform_open_next_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors)

struct platform_work_queue;

#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

#define PLATFORM_ALLOCATE_MEMORY(name) void *name(memory_index Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);

typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data);
typedef void platform_complete_all_work(platform_work_queue *Queue);

typedef struct platform_api {
    platform_add_entry *AddEntry;
    platform_complete_all_work *CompleteAllWork;

    platform_get_all_files_of_type_begin *GetAllFilesOfTypeBegin;
    platform_get_all_files_of_type_end *GetAllFilesOfTypeEnd;
    platform_open_next_file *OpenNextFile;
    platform_read_data_from_file *ReadDataFromFile;
    platform_file_error *FileError;

    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;

    debug_platform_free_file_memory *DEBUGFreeFileMemory;
    debug_platform_read_entire_file *DEBUGReadEntireFile;
    debug_platform_write_entire_file *DEBUGWriteEntireFile;
} platform_api;

typedef struct game_memory {
    memory_index PermanentStorageSize;
    void *PermanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

    memory_index TransientStorageSize;
    void *TransientStorage; // NOTE: REQUIRED to be cleared to zero at startup

    memory_index DebugStorageSize;
    void *DebugStorage; // NOTE: REQUIRED to be cleared to zero at startup

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    platform_api PlatformAPI;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE: At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so.
// TODO: Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected) {
    uint32 Result = _InterlockedCompareExchange((long *)Value, New, Expected);
    return Result;
}
inline u64 AtomicExchangeU64(u64 volatile *Value, u64 New) {
    u64 Result = _InterlockedExchange64((__int64 *)Value, New);
    return Result;
}
inline u64 AtomicAddU64(u64 volatile *Value, u64 Addend) {
    // NOTE: Returns the original value _priori_ to adding
    u64 Result = _InterlockedExchangeAdd64((__int64 *)Value, Addend);
    return Result;
}
inline u32 GetThreadID(void) {
    u8 *ThreadLocalStorage = (u8 *)__readgsqword(0x30);
    u32 ThreadID = *(u32 *)ThreadLocalStorage + 0x48;

    return ThreadID;
}
#elif HANDMADE_SDL
#include <SDL2/SDL.h>
#define CompletePreviousReadsBeforeFutureReads SDL_CompilerBarrier()
#define CompletePreviousWritesBeforeFutureWrites SDL_CompilerBarrier()
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected) {
    if (SDL_AtomicCAS((SDL_atomic_t *)Value, Expected, New)) {
        return Expected;
    } else {
        return *Value;
    }
}
#else
// TODO: Need GCC/LLVM equiavalents!
#endif

struct debug_table;
#define DEBUG_GAME_FRAME_END(name) debug_table *name(game_memory *Memory)
typedef DEBUG_GAME_FRAME_END(debug_game_frame_end);

inline game_controller_input *GetController(game_input *Input, size_t ControllerIndex) {
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return &Input->Controllers[ControllerIndex];
}

struct debug_record {
    char *FileName;
    char *BlockName;

    u32 LineNumber;
    u32 Reserved;
};

enum debug_event_type {
    DebugEvent_FrameMaker,
    DebugEvent_BeginBlock,
    DebugEvent_EndBlock,
};

struct debug_event {
    u64 Clock;
    u16 ThreadID;
    u16 CoreIndex;
    u16 DebugRecordIndex;
    u8 TranslationUnit;
    u8 Type;
};

debug_record DebugRecordArray[];
#define MAX_DEBUG_THREAD_COUNT 256
#define MAX_DEBUG_EVENT_ARRAY_COUNT 64
#define MAX_DEBUG_TRANSLATION_UNITS 3
#define MAX_DEBUG_EVENT_COUNT (16*65536)
#define MAX_DEBUG_RECORD_COUNT (65536)

struct debug_table {
    u32 CurrentEventArrayIndex;
    u64 volatile EventArrayIndex_EventIndex;
    u32 EventCount[MAX_DEBUG_EVENT_ARRAY_COUNT];
    debug_event Events[MAX_DEBUG_EVENT_ARRAY_COUNT][MAX_DEBUG_EVENT_COUNT];

    u32 RecordCount[MAX_DEBUG_TRANSLATION_UNITS];
    debug_record Records[MAX_DEBUG_TRANSLATION_UNITS][MAX_DEBUG_RECORD_COUNT];
};

extern debug_table *GlobalDebugTable;

inline void
RecordDebugEvent(int RecordIndex, debug_event_type EventType) {
    u64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, 1);
    u32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;
    Assert(EventIndex < MAX_DEBUG_EVENT_COUNT);
    debug_event *Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32] + EventIndex;
    Event->Clock = __rdtsc();
    Event->ThreadID = (u16)GetThreadID();
    Event->CoreIndex = 0;
    Event->DebugRecordIndex = (u16)RecordIndex;
    Event->TranslationUnit = TRANSLATION_UNIT_INDEX;
    Event->Type = (u8)EventType;
}

#define FRAME_MARKER() \
    { \
        int Counter = __COUNTER__; \
        RecordDebugEvent(Counter, DebugEvent_FrameMaker); \
        debug_record *Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter; \
        Record->FileName = __FILE__; \
        Record->LineNumber = __LINE__; \
        Record->BlockName = "Frame Marker"; \
    }

#define TIMED_BLOCK__(BlockName, Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, BlockName, ## __VA_ARGS__)
#define TIMED_BLOCK_(BlockName, Number, ...) TIMED_BLOCK__(BlockName, Number, ## __VA_ARGS__)
#define TIMED_BLOCK(BlockName, ...) TIMED_BLOCK_(#BlockName, __LINE__, ## __VA_ARGS__)
#define TIMED_FUNCTION(...) TIMED_BLOCK_(__FUNCTION__, __LINE__, ## __VA_ARGS__)

#define BEGIN_BLOCK_(Counter, FileNameInit, LineNumberInit, BlockNameInit) \
    {debug_record *Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter; \
    Record->FileName = FileNameInit; \
    Record->LineNumber = LineNumberInit; \
    Record->BlockName = BlockNameInit; \
    RecordDebugEvent(Counter, DebugEvent_BeginBlock);}

#define END_BLOCK_(Counter) \
    RecordDebugEvent(Counter, DebugEvent_EndBlock);

#define BEGIN_BLOCK(Name) \
    int Counter_##Name = __COUNTER__; \
    BEGIN_BLOCK_(Counter_##Name, __FILE__, __LINE__, #Name)

#define END_BLOCK(Name) \
    END_BLOCK_(Counter_##Name)

struct timed_block {
    int Counter;

    timed_block(int CounterInit, char *FileName, int LineNumber, char *BlockName, u32 HitCountInit = 1) {
        Counter = CounterInit;
        // TODO: Record the hit count value here?
        BEGIN_BLOCK_(Counter, FileName, LineNumber, BlockName);
    }

    ~timed_block() {
        END_BLOCK_(Counter);
    }
};

#ifdef __cplusplus
}
#endif

#endif
