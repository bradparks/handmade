#ifndef HANDMADE_DEBUG_H
#define HANDMADE_DEBUG_H

struct debug_counter_snapshot {
    u32 HitCount;
    u64 CycleCount;
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
