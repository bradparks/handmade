#ifndef HANDMADE_DEBUG_H
#define HANDMADE_DEBUG_H

struct debug_counter_snapshot {
    u32 HitCount;
    u64 CycleCount;
};

struct debug_counter_state {
    char *FileName;
    char *BlockName;

    u32 LineNumber;
};

struct debug_frame_region {
    debug_record *Record;
    u64 CycleCount;
    u16 LaneIndex;
    u16 ColorIndex;
    r32 MinT;
    r32 MaxT;
};

#define MAX_REGIONS_PER_FRAME 4*2048
struct debug_frame {
    u64 BeginClock;
    u64 EndClock;
    r32 WallSecondsElapsed;

    u32 RegionCount;
    debug_frame_region *Regions;
};

struct open_debug_block {
    u32 StartingFrameIndex;
    debug_record *Source;
    debug_event *OpeningEvent;
    open_debug_block *Parent;

    open_debug_block *NextFree;
};

struct debug_thread {
    u32 ID;
    u32 LaneIndex;
    open_debug_block *FirstOpenBlock;
    debug_thread *Next;
};

struct debug_state {
    b32 Initialized;
    b32 Paused;

    debug_record *ScopeToRecord;

    memory_arena CollateArena;
    temporary_memory CollateTemp;

    u32 CollationArrayIndex;
    debug_frame *CollationFrame;

    u32 FrameBarLaneCount;
    r32 FrameBarScale;
    u32 FrameCount;

    debug_frame *Frames;
    debug_thread *FirstThread;
    open_debug_block *FirstFreeBlock;
};

// TODO: Fix this for looped live code editing
struct render_group;
struct game_assets;

global_variable render_group *DEBUGRenderGroup;

internal void DEBUGReset(game_assets *Assets, u32 Width, u32 Height);
internal void DEBUGOverlay(game_memory *Memory, game_input *Input);
internal void RefreshCollation(debug_state *DebugState);

#endif // HANDMADE_DEBUG_H
