// TODO: Stop using stdio!
#include <stdio.h>

global_variable r32 LeftEdge;
global_variable r32 AtY;
global_variable r32 FontScale;
global_variable font_id FontID;

u64 Global_DebugEventArrayIndex_DebugEventIndex;
debug_event GlobalDebugEventArray[2][MAX_DEBUG_EVENT_COUNT];

internal void
DEBUGReset(game_assets *Assets, u32 Width, u32 Height) {
    TIMED_BLOCK();

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    MatchVector.E[Tag_FontType] = (r32)FontType_Debug;
    WeightVector.E[Tag_FontType] = 1.0f;
    FontID = GetBestMatchFontFrom(Assets, Asset_Font, &MatchVector, &WeightVector);

    FontScale = 1.0f;
    Orthographic(DEBUGRenderGroup, Width, Height, 1.0f);
    LeftEdge = -0.5f * Width;

    hha_font *Info = GetFontInfo(Assets, FontID);
    AtY = 0.5f * Height - FontScale * GetStartingBaselineY(Info);
}

inline b32
IsHex(char Char) {
    b32 Result = ((Char >= '0' && Char <= '9') ||
                  (Char >= 'A' && Char <= 'F'));
    return Result;
}

inline u32
GetHex(char Char) {
    u32 Result = 0;

    if (Char >= '0' && Char <= '9') {
        Result = Char - '0';
    } else if (Char >= 'A' && Char <= 'F') {
        Result = 0xA + (Char - 'A');
    }

    return Result;
}

internal void
DEBUGTextLine(char *String) {
    if (DEBUGRenderGroup) {
        render_group *RenderGroup = DEBUGRenderGroup;

        loaded_font *Font = PushFont(RenderGroup, FontID);

        if (Font) {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, FontID);
            u32 PrevCodePoint = 0;
            r32 AtX = LeftEdge;

            for (char *At = String; *At; ++At) {
                u32 CodePoint = *At;

                if (
                    At[0] == '\\' &&
                    IsHex(At[1]) && IsHex(At[2]) &&
                    IsHex(At[3]) && IsHex(At[4])
                ) {
                    CodePoint = (GetHex(At[1]) << 12 |
                                 GetHex(At[2]) <<  8 |
                                 GetHex(At[3]) <<  4 |
                                 GetHex(At[4]) <<  0);
                    At += 4;
                }

                r32 AdvanceX = FontScale * GetHorizontalAdvanceForPair(Info, Font, PrevCodePoint, CodePoint);
                AtX += AdvanceX;

                if (CodePoint != ' ') {
                    bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, Info, Font, CodePoint);
                    hha_bitmap *BitmapInfo = GetBitmapInfo(RenderGroup->Assets, BitmapID);

                    PushBitmap(RenderGroup, BitmapID, FontScale * (r32)BitmapInfo->Dim[1], V3(AtX, AtY, 0), V4(1, 1, 1, 1));
                }

                PrevCodePoint = CodePoint;
            }

            AtY -= GetLineAdvanceFor(Info) * FontScale;
        } else {
        }
    }
}


struct debug_statistic {
    r64 Min;
    r64 Avg;
    r64 Max;
    u32 Count;
};

inline void
BeginDebugStatistic(debug_statistic *Stat) {
    Stat->Min = Real32Maximum;
    Stat->Max = -Real32Maximum;
    Stat->Avg = 0.0f;
    Stat->Count = 0;
}

inline void
AccumDebugStatistic(debug_statistic *Stat, r64 Value) {
    ++Stat->Count;
    if (Stat->Min > Value) {
        Stat->Min = Value;
    }

    if (Stat->Max < Value) {
        Stat->Max = Value;
    }

    Stat->Avg += Value;
}

inline void
EndDebugStatistic(debug_statistic *Stat) {
    if (Stat->Count != 0) {
        Stat->Avg /= Stat->Count;
    } else {
        Stat->Min = 0.0f;
        Stat->Max = 0.0f;
    }
}

internal void
DEBUGOverlay(game_memory *Memory) {
    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    if (DebugState && DEBUGRenderGroup) {
        render_group *RenderGroup = DEBUGRenderGroup;
        loaded_font *Font = PushFont(RenderGroup, FontID);

        // TODO: Layout / cached font info / etc. for real debug display
        if (Font) {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, FontID);
            for (u32 CounterIndex = 0; CounterIndex < DebugState->CounterCount; ++CounterIndex) {
                debug_counter_state *Counter = DebugState->CounterStates + CounterIndex;

                debug_statistic HitCount, CycleCount, CycleOverHit;
                BeginDebugStatistic(&HitCount);
                BeginDebugStatistic(&CycleCount);
                BeginDebugStatistic(&CycleOverHit);

                for (u32 SnapshotIndex = 0; SnapshotIndex < DEBUG_MAX_SNAPSHOT_COUNT; ++SnapshotIndex) {
                    AccumDebugStatistic(&HitCount, Counter->Snapshots[SnapshotIndex].HitCount);
                    AccumDebugStatistic(&CycleCount, (r64)Counter->Snapshots[SnapshotIndex].CycleCount);

                    r64 HOC = 0.0f;
                    if (Counter->Snapshots[SnapshotIndex].HitCount) {
                        HOC = ((r64)Counter->Snapshots[SnapshotIndex].CycleCount /
                               (r64)Counter->Snapshots[SnapshotIndex].HitCount);
                    }
                    AccumDebugStatistic(&CycleOverHit, HOC);
                }

                EndDebugStatistic(&HitCount);
                EndDebugStatistic(&CycleCount);
                EndDebugStatistic(&CycleOverHit);

                if (Counter->FunctionName) {
                    if (CycleCount.Max > 0.0f) {
                        r32 BarWidth = 4.0f;
                        r32 ChartLeft = 0.0f;
                        r32 ChartMinY = AtY;
                        r32 ChartHeight = Info->AscenderHeight * FontScale;
                        r32 Scale = 1.0f / (r32)CycleCount.Max;
                        for (u32 SnapshotIndex = 0; SnapshotIndex < DEBUG_MAX_SNAPSHOT_COUNT; ++SnapshotIndex) {
                            r32 ThisProportion = Scale * (r32)Counter->Snapshots[SnapshotIndex].CycleCount;
                            r32 ThisHeight = ChartHeight * ThisProportion;
                            PushRect(RenderGroup, V3(ChartLeft + BarWidth * (r32)SnapshotIndex + 0.5f * BarWidth, ChartMinY + 0.5f * ThisHeight, 0.0f), V2(BarWidth, ThisHeight), V4(ThisProportion, 1, 0.0f, 1));
                        }
                    }
#if 1
                    char buf[512];
                    snprintf(buf, 512, "%32s(%4u): %10ucy %8uh %10ucy/h\n",
                             Counter->FunctionName,
                             Counter->LineNumber,
                             (u32)CycleCount.Avg,
                             (u32)HitCount.Avg,
                             (u32)(CycleOverHit.Avg));
                    DEBUGTextLine(buf);
#else
                    DEBUGTextLine(Counter->FileName);
#endif
                }
            }

            r32 BarWidth = 8.0f;
            r32 BarSpacing = 10.0f;
            r32 ChartLeft = LeftEdge + 10.0f;
            r32 ChartHeight = 300.0f;
            r32 ChartWidth = BarSpacing * (r32)DEBUG_MAX_SNAPSHOT_COUNT;
            r32 ChartMinY = AtY - (ChartHeight + 80.0f);
            r32 Scale = 1.0f / 0.03333f;

            v3 Colors[] = {
                {1, 0, 0},
                {0, 1, 0},
                {0, 0, 1},
                {1, 1, 0},
                {0, 1, 1},
                {1, 0, 1},
                {1, 0.5f, 0},
                {1, 0, 0.5f},
                {0.5f, 1, 0},
                {0, 1, 0.5f},
                {0.5f, 0, 1},
                {0, 0.5f, 1},
            };

            for (u32 SnapshotIndex = 0; SnapshotIndex < DEBUG_MAX_SNAPSHOT_COUNT; ++SnapshotIndex) {
                debug_frame_end_info *FrameEndInfo = DebugState->FrameEndInfos + SnapshotIndex;
                r32 StackY = ChartMinY;
                r32 PrevTimestampSeconds = 0.0f;
                for (u32 TimestampIndex = 0; TimestampIndex < FrameEndInfo->TimestampCount; ++TimestampIndex) {
                    debug_frame_timestamp *Timestamp = FrameEndInfo->Timestamps + TimestampIndex;
                    r32 ThisSecondsElapsed = Timestamp->Seconds - PrevTimestampSeconds;
                    PrevTimestampSeconds = Timestamp->Seconds;

                    v3 Color = Colors[TimestampIndex % ArrayCount(Colors)];
                    r32 ThisProportion = Scale * ThisSecondsElapsed;
                    r32 ThisHeight = ChartHeight * ThisProportion;
                    PushRect(RenderGroup, V3(ChartLeft + BarSpacing * (r32)SnapshotIndex + 0.5f * BarWidth,
                                             StackY + 0.5f * ThisHeight, 0.0f),
                             V2(BarWidth, ThisHeight), V4(Color, 1));
                    StackY += ThisHeight;
                }
            }
            PushRect(RenderGroup, V3(ChartLeft + 0.5f * ChartWidth, ChartMinY + ChartHeight, 0.0f),
                     V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));

        }

        //DEBUGTextLine("\\5C0F\\8033\\6728\\514E");
        //DEBUGTextLine("AVA WA Ta");
    }
}

debug_record DebugRecords_Main[__COUNTER__];

extern u32 GlobalCurrentEventArrayIndex = 0;
extern u32 const DebugRecords_Optimized_Count;
debug_record DebugRecords_Optimized[];

internal void
UpdateDebugRecords(debug_state *DebugState, u32 CounterCount, debug_record *Counters) {
    for (uint32 CounterIndex = 0; CounterIndex < CounterCount; ++CounterIndex) {
        debug_record *Source = Counters + CounterIndex;
        debug_counter_state *Dest = DebugState->CounterStates + DebugState->CounterCount++;

        u64 HitCount_CycleCount = AtomicExchangeU64(&Source->HitCount_CycleCount, 0);
        Dest->FileName = Source->FileName;
        Dest->FunctionName = Source->FunctionName;
        Dest->LineNumber = Source->LineNumber;
        Dest->Snapshots[DebugState->SnapshotIndex].HitCount = (u32)(HitCount_CycleCount >> 32);
        Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = (u32)(HitCount_CycleCount & 0xFFFFFFFF);
    }
}

internal void
CollateDebugRecords(debug_state *DebugState, u32 EventCount, debug_event *Events) {
    DebugState->CounterCount = DebugRecords_Optimized_Count + ArrayCount(DebugRecords_Main);

    for (uint32 CounterIndex = 0; CounterIndex < DebugState->CounterCount; ++CounterIndex) {
        debug_counter_state *Dest = DebugState->CounterStates + CounterIndex;
        Dest->Snapshots[DebugState->SnapshotIndex].HitCount = 0;
        Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = 0;
    }

    debug_counter_state *CounterArray[2] = {
        DebugState->CounterStates,
        DebugState->CounterStates + ArrayCount(DebugRecords_Main),
    };

    debug_record *DebugRecords[2] = {
        DebugRecords_Main,
        DebugRecords_Optimized,
    };

    for (u32 EventIndex = 0; EventIndex < EventCount; ++EventIndex) {
        debug_event *Event = Events + EventIndex;

        debug_counter_state *Dest = CounterArray[Event->DebugRecordArrayIndex] + Event->DebugRecordIndex;

        debug_record *Source = DebugRecords[Event->DebugRecordArrayIndex] + Event->DebugRecordIndex;
        Dest->FileName = Source->FileName;
        Dest->FunctionName = Source->FunctionName;
        Dest->LineNumber = Source->LineNumber;

        if (Event->Type == DebugEvent_BeginBlock) {
            ++Dest->Snapshots[DebugState->SnapshotIndex].HitCount;
            Dest->Snapshots[DebugState->SnapshotIndex].CycleCount -= Event->Clock;
        } else {
            Assert(Event->Type == DebugEvent_EndBlock);
            Dest->Snapshots[DebugState->SnapshotIndex].CycleCount += Event->Clock;
        }
    }
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd) {
    GlobalCurrentEventArrayIndex = !GlobalCurrentEventArrayIndex;
    u64 ArrayIndex_EventIndex = AtomicExchangeU64(&Global_DebugEventArrayIndex_DebugEventIndex,
                                                  (u64)GlobalCurrentEventArrayIndex << 32);

    u32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
    u32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;

    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    if (DebugState) {
        DebugState->CounterCount = 0;
#if 0
        UpdateDebugRecords(DebugState, DebugRecords_Optimized_Count, DebugRecords_Optimized);
        UpdateDebugRecords(DebugState, ArrayCount(DebugRecords_Main), DebugRecords_Main);
#else
        CollateDebugRecords(DebugState, EventCount, GlobalDebugEventArray[EventArrayIndex]);
#endif

        DebugState->FrameEndInfos[DebugState->SnapshotIndex] = *Info;

        ++DebugState->SnapshotIndex;
        if (DebugState->SnapshotIndex >= DEBUG_MAX_SNAPSHOT_COUNT) {
            DebugState->SnapshotIndex = 0;
        }
    }
}
