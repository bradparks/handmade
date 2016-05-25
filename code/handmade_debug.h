#ifndef HANDMADE_DEBUG_H
#define HANDMADE_DEBUG_H

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ## __VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ## __VA_ARGS__)

struct debug_record {
    char *FileName;
    char *FunctionName;

    u32 LineNumber;
    u32 Reserved;

    u64 HitCount_CycleCount;
};

debug_record DebugRecordArray[];

struct timed_block {
    debug_record *Record;
    u64 StartCycles;
    u32 HitCount;

    timed_block(int Counter, char *FileName, int LineNumber, char *FunctionName, u32 HitCountInit = 1) {
        HitCount = HitCountInit;

        Record = DebugRecordArray + Counter;
        Record->FileName = FileName;
        Record->LineNumber = LineNumber;
        Record->FunctionName = FunctionName;

        StartCycles = __rdtsc();
    }

    ~timed_block() {
        u64 Delta = (__rdtsc() - StartCycles) | (((u64)HitCount) << 32);
        AtomicAddU64(&Record->HitCount_CycleCount, Delta);
    }
};

struct debug_counter_snapshot {
    u32 HitCount;
    u32 CycleCount;
};

#define DEBUG_MAX_SNAPSHOT_COUNT 120
struct debug_counter_state {
    char *FileName;
    char *FunctionName;

    u32 LineNumber;

    debug_counter_snapshot Snapshots[DEBUG_MAX_SNAPSHOT_COUNT];
};

struct debug_state {
    u32 SnapshotIndex;
    u32 CounterCount;
    debug_counter_state CounterStates[512];
    debug_frame_end_info FrameEndInfos[DEBUG_MAX_SNAPSHOT_COUNT];
};

// TODO: Fix this for looped live code editing
struct render_group;
struct game_assets;

global_variable render_group *DEBUGRenderGroup;

internal void DEBUGReset(game_assets *Assets, u32 Width, u32 Height);
internal void DEBUGOverlay(game_memory *Memory);

#endif // HANDMADE_DEBUG_H
