// TODO: Stop using stdio!
#include <stdio.h>

internal void RestartCollation(debug_state *DebugState, u32 InvalidEventArrayIndex);

inline debug_state *
DEBUGGetState(game_memory *Memory) {
    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    Assert(DebugState->Initialized);

    return DebugState;
}

inline debug_state *
DEBUGGetState(void) {
    debug_state *Result = DEBUGGetState(DebugGlobalMemory);

    return Result;
}

internal void
DEBUGStart(game_assets *Assets, u32 Width, u32 Height) {
    TIMED_FUNCTION();

    debug_state *DebugState = (debug_state *)DebugGlobalMemory->DebugStorage;
    if (DebugState) {
        if (!DebugState->Initialized) {
            DebugState->HighPriorityQueue = DebugGlobalMemory->HighPriorityQueue;

            InitializeArena(&DebugState->DebugArena, DebugGlobalMemory->DebugStorageSize - sizeof(debug_state), DebugState + 1);
            DebugState->RenderGroup = AllocateRenderGroup(Assets, &DebugState->DebugArena, Megabytes(16), false);

            DebugState->Paused = false;
            DebugState->ScopeToRecord = 0;

            DebugState->Initialized = true;

            SubArena(&DebugState->CollateArena, &DebugState->DebugArena, Megabytes(32), 4);
            DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);

            RestartCollation(DebugState, 0);
        }

        BeginRender(DebugState->RenderGroup);

        DebugState->Width = (r32)Width;
        DebugState->Height = (r32)Height;

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_FontType] = (r32)FontType_Debug;
        WeightVector.E[Tag_FontType] = 1.0f;
        DebugState->FontID = GetBestMatchFontFrom(Assets, Asset_Font, &MatchVector, &WeightVector);

        DebugState->FontScale = 1.0f;
        Orthographic(DebugState->RenderGroup, Width, Height, 1.0f);
        DebugState->LeftEdge = -0.5f * Width;

        hha_font *Info = GetFontInfo(Assets, DebugState->FontID);
        DebugState->AtY = 0.5f * Height - DebugState->FontScale * GetStartingBaselineY(Info);
    }
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
DEBUGTextOutAt(v2 P, char *String) {
    debug_state *DebugState = DEBUGGetState();
    if (DebugState) {
        render_group *RenderGroup = DebugState->RenderGroup;

        loaded_font *Font = PushFont(RenderGroup, DebugState->FontID);

        if (Font) {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, DebugState->FontID);
            u32 PrevCodePoint = 0;

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

                r32 AdvanceX = DebugState->FontScale * GetHorizontalAdvanceForPair(Info, Font, PrevCodePoint, CodePoint);
                P.x += AdvanceX;

                if (CodePoint != ' ') {
                    bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, Info, Font, CodePoint);
                    hha_bitmap *BitmapInfo = GetBitmapInfo(RenderGroup->Assets, BitmapID);

                    PushBitmap(RenderGroup, BitmapID, DebugState->FontScale * (r32)BitmapInfo->Dim[1], V3(P.x, P.y, 0), V4(1, 1, 1, 1));
                }

                PrevCodePoint = CodePoint;
            }
        } else {
        }
    }
}

internal void
DEBUGTextLine(char *String) {
    debug_state *DebugState = DEBUGGetState();
    if (DebugState) {
        render_group *RenderGroup = DebugState->RenderGroup;

        loaded_font *Font = PushFont(RenderGroup, DebugState->FontID);

        if (Font) {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, DebugState->FontID);

            DEBUGTextOutAt(V2(DebugState->LeftEdge, DebugState->AtY), String);

            DebugState->AtY -= GetLineAdvanceFor(Info) * DebugState->FontScale;
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
DEBUGEnd(game_input *Input, loaded_bitmap *DrawBuffer) {
    TIMED_FUNCTION();

    debug_state *DebugState = DEBUGGetState();
    if (DebugState) {
        render_group *RenderGroup = DebugState->RenderGroup;

        debug_record *HotRecord = 0;

        v2 MouseP = V2(Input->MouseX, Input->MouseY);
        if (WasPressed(Input->MouseButtons[PlatformMouseButton_Right])) {
            DebugState->Paused = !DebugState->Paused;
        }

        loaded_font *Font = PushFont(RenderGroup, DebugState->FontID);

        // TODO: Layout / cached font info / etc. for real debug display
        if (Font) {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, DebugState->FontID);

#if 0
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

                if (Counter->BlockName) {
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
                             Counter->BlockName,
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
#endif
            if (DebugState->FrameCount) {
                char buf[512];
                snprintf(buf, 512, "Last frame time: %.02fms",
                         DebugState->Frames[DebugState->FrameCount - 1].WallSecondsElapsed * 1000.0f);
                DEBUGTextLine(buf);
            }

            Orthographic(DebugState->RenderGroup, (s32)DebugState->Width, (s32)DebugState->Height, 1.0f);

            DebugState->ProfileRect = RectMinMax(V2(50.0f, 50.0f), V2(200.0f, 200.0f));
            PushRect(DebugState->RenderGroup, DebugState->ProfileRect, 0.0f, V4(0, 0, 0, 0.25f));

            r32 BarSpacing = 4.0f;
            r32 LaneHeight = 0.0f;
            u32 LaneCount = DebugState->FrameBarLaneCount;

            u32 MaxFrame = DebugState->FrameCount;
            if (MaxFrame > 10) {
                MaxFrame = 10;
            }

            if (LaneCount > 0 && MaxFrame > 0) {
                r32 PixelsPerFramePlusSpacing = GetDim(DebugState->ProfileRect).y / (r32)MaxFrame;
                r32 PixelsPerFrame = PixelsPerFramePlusSpacing - BarSpacing;
                LaneHeight = PixelsPerFrame / (r32)LaneCount;
            }

            r32 BarHeight = LaneHeight * LaneCount;
            r32 BarsPlusSpacing = BarHeight + BarSpacing;
            r32 ChartLeft = DebugState->ProfileRect.Min.x;
            r32 ChartHeight = BarSpacing * (r32)MaxFrame;
            r32 ChartWidth = GetDim(DebugState->ProfileRect).x;
            r32 ChartTop = DebugState->ProfileRect.Max.y;
            r32 Scale = ChartWidth * DebugState->FrameBarScale;

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

#if 1
            for (u32 FrameIndex = 0; FrameIndex < MaxFrame; ++FrameIndex) {
                debug_frame *Frame = DebugState->Frames + DebugState->FrameCount - (FrameIndex + 1);
                r32 StackX = ChartLeft;
                r32 StackY = ChartTop - BarsPlusSpacing * (r32)FrameIndex;
                r32 PrevTimestampSeconds = 0.0f;
                for (u32 RegionIndex = 0; RegionIndex < Frame->RegionCount; ++RegionIndex) {
                    debug_frame_region *Region = Frame->Regions + RegionIndex;

                    // v3 Color = Colors[RegionIndex % ArrayCount(Colors)];
                    v3 Color = Colors[Region->ColorIndex % ArrayCount(Colors)];
                    r32 ThisMinX = StackX + Scale * Region->MinT;
                    r32 ThisMaxX = StackX + Scale * Region->MaxT;

                    rectangle2 RegionRect = RectMinMax(V2(ThisMinX, StackY - LaneHeight * (Region->LaneIndex + 1)),
                                                       V2(ThisMaxX, StackY - LaneHeight * Region->LaneIndex));

                    PushRect(RenderGroup, RegionRect, 0.0f, V4(Color, 1));

                    if (IsInRectangle(RegionRect, MouseP)) {
                        debug_record *Record = Region->Record;
                        char buf[512];
                        snprintf(buf, 512, "%s: %10llucy [%s(%d)]\n",
                                 Record->BlockName,
                                 Region->CycleCount,
                                 Record->FileName,
                                 Record->LineNumber);
                        DEBUGTextOutAt(MouseP + V2(0.0f, 10.0f), buf);

                        HotRecord = Record;
                    }
                }
            }
#endif
#if 0
            PushRect(RenderGroup, V3(ChartLeft + 0.5f * ChartWidth, ChartMinY + ChartHeight, 0.0f),
                     V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));
#endif
        }

        if (WasPressed(Input->MouseButtons[PlatformMouseButton_Left])) {
            if (HotRecord) {
                DebugState->ScopeToRecord = HotRecord;
            } else {
                DebugState->ScopeToRecord = 0;
            }
            RefreshCollation(DebugState);
        }

        TiledRenderGroupToOutput(DebugState->HighPriorityQueue, DebugState->RenderGroup, DrawBuffer);
        EndRender(DebugState->RenderGroup);
    }
}

#define DebugRecords_Main_Count __COUNTER__
extern u32 const DebugRecords_Optimized_Count;

global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;

inline u32 GetLaneFromThreadIndex(debug_state *DebugState, u32 ThreadIndex) {
    u32 Result = 0;

    // TODO

    return Result;
}

internal debug_thread *
GetDebugThread(debug_state *DebugState, u32 ThreadID) {
    debug_thread * Result = 0;
    for (debug_thread *Thread = DebugState->FirstThread; Thread; Thread = Thread->Next) {
        if (Thread->ID == ThreadID) {
            Result = Thread;
            break;
        }
    }

    if (!Result) {
        Result = PushStruct(&DebugState->CollateArena, debug_thread);
        Result->ID = ThreadID;
        Result->LaneIndex = DebugState->FrameBarLaneCount++;
        Result->FirstOpenBlock = 0;
        Result->Next = DebugState->FirstThread;
        DebugState->FirstThread = Result;
    }

    return Result;
}

debug_frame_region *
AddRegion(debug_state *DebugState, debug_frame *CurrentFrame) {
    Assert(CurrentFrame->RegionCount < MAX_REGIONS_PER_FRAME);
    debug_frame_region *Result = CurrentFrame->Regions + CurrentFrame->RegionCount++;

    return Result;
}

inline debug_record *
GetRecordFrom(open_debug_block *Block) {
    debug_record *Result = Block ? Block->Source : 0;

    return Result;
}

internal void
CollateDebugRecords(debug_state *DebugState, u32 InvalidEventArrayIndex) {
    for (; /* empty */; ++DebugState->CollationArrayIndex) {
        if (DebugState->CollationArrayIndex == MAX_DEBUG_EVENT_ARRAY_COUNT) {
            DebugState->CollationArrayIndex = 0;
        }

        u32 EventArrayIndex = DebugState->CollationArrayIndex;
        if (EventArrayIndex == InvalidEventArrayIndex) {
            break;
        }

        for (u32 EventIndex = 0; EventIndex < GlobalDebugTable->EventCount[EventArrayIndex]; ++EventIndex) {
            debug_event *Event = GlobalDebugTable->Events[EventArrayIndex] + EventIndex;
            debug_record *Source = (GlobalDebugTable->Records[Event->TranslationUnit] + Event->DebugRecordIndex);

            if (Event->Type == DebugEvent_FrameMaker) {
                if (DebugState->CollationFrame) {
                    DebugState->CollationFrame->EndClock = Event->Clock;
                    DebugState->CollationFrame->WallSecondsElapsed = Event->SecondsElapsed;
                    ++DebugState->FrameCount;

                    r32 ClockRange = (r32)(DebugState->CollationFrame->EndClock - DebugState->CollationFrame->BeginClock);
#if 0
                    if (ClockRange > 0.0f) {
                        r32 FrameBarScale = 1.0f / ClockRange;
                        if (DebugState->FrameBarScale > FrameBarScale) {
                            DebugState->FrameBarScale = FrameBarScale;
                        }
                    }
#endif
                }

                DebugState->CollationFrame = DebugState->Frames + DebugState->FrameCount;
                DebugState->CollationFrame->BeginClock = Event->Clock;
                DebugState->CollationFrame->EndClock = 0;
                DebugState->CollationFrame->RegionCount = 0;
                DebugState->CollationFrame->Regions = PushArray(&DebugState->CollateArena, MAX_REGIONS_PER_FRAME, debug_frame_region);
                DebugState->CollationFrame->WallSecondsElapsed = 0.0f;
            } else if (DebugState->CollationFrame) {
                u32 FrameIndex = DebugState->FrameCount - 1;
                debug_thread *Thread = GetDebugThread(DebugState, Event->TC.ThreadID);
                u64 RelativeClock = Event->Clock - DebugState->CollationFrame->BeginClock;
                u32 LaneIndex = GetLaneFromThreadIndex(DebugState, Event->TC.ThreadID);
                if (Event->Type == DebugEvent_BeginBlock) {
                    open_debug_block *DebugBlock = DebugState->FirstFreeBlock;
                    if (DebugBlock) {
                        DebugState->FirstFreeBlock = DebugBlock->NextFree;
                    } else {
                        DebugBlock = PushStruct(&DebugState->CollateArena, open_debug_block);
                    }

                    DebugBlock->StartingFrameIndex = FrameIndex;
                    DebugBlock->OpeningEvent = Event;
                    DebugBlock->Parent = Thread->FirstOpenBlock;
                    DebugBlock->Source = Source;
                    Thread->FirstOpenBlock = DebugBlock;
                    DebugBlock->NextFree = 0;
                } else if (Event->Type == DebugEvent_EndBlock) {
                    if (Thread->FirstOpenBlock) {
                        open_debug_block *MatchingBlock = Thread->FirstOpenBlock;
                        debug_event *OpeningEvent = MatchingBlock->OpeningEvent;
                        if (
                            (OpeningEvent->TC.ThreadID == Event->TC.ThreadID) &&
                            (OpeningEvent->DebugRecordIndex == Event->DebugRecordIndex) &&
                            (OpeningEvent->TranslationUnit == Event->TranslationUnit)
                        ) {
                            if (MatchingBlock->StartingFrameIndex == FrameIndex) {
                                if (GetRecordFrom(Thread->FirstOpenBlock->Parent) == DebugState->ScopeToRecord) {
                                    r32 MinT = (r32)(OpeningEvent->Clock - DebugState->CollationFrame->BeginClock);
                                    r32 MaxT = (r32)(Event->Clock - DebugState->CollationFrame->BeginClock);
                                    r32 ThresholdT = 0.01f;
                                    if ((MaxT - MinT) > ThresholdT) {
                                        debug_frame_region *Region = AddRegion(DebugState, DebugState->CollationFrame);
                                        Region->Record = Source;
                                        Region->CycleCount = (Event->Clock - OpeningEvent->Clock);
                                        Region->LaneIndex = (u16)Thread->LaneIndex;
                                        Region->MinT = MinT;
                                        Region->MaxT = MaxT;
                                        Region->ColorIndex = (u16)OpeningEvent->DebugRecordIndex;
                                    }
                                }
                            } else {
                                // TODO: Record all frames in between and begin/end spans!
                            }

                            MatchingBlock->NextFree = DebugState->FirstFreeBlock;
                            DebugState->FirstFreeBlock = MatchingBlock;
                            Thread->FirstOpenBlock = MatchingBlock->Parent;
                        } else {
                            // TODO: Record span that goes to the begining of the frame series?
                        }
                    }
                } else {
                    Assert(!"Invalid event type");
                }
            }
        }
    }
}

internal void
RestartCollation(debug_state *DebugState, u32 InvalidEventArrayIndex) {
    EndTemporaryMemory(DebugState->CollateTemp);
    DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);

    DebugState->FirstThread = 0;
    DebugState->FirstFreeBlock = 0;

    DebugState->Frames = PushArray(&DebugState->CollateArena, MAX_DEBUG_EVENT_ARRAY_COUNT * 4, debug_frame);
    DebugState->FrameBarLaneCount = 0;
    DebugState->FrameBarScale = 1.0f / 60000000.0f;
    DebugState->FrameCount = 0;

    DebugState->CollationArrayIndex = InvalidEventArrayIndex + 1;
    DebugState->CollationFrame = 0;
}

internal void
RefreshCollation(debug_state *DebugState) {
    RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
    CollateDebugRecords(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd) {
    GlobalDebugTable->RecordCount[0] = DebugRecords_Main_Count;
    GlobalDebugTable->RecordCount[1] = DebugRecords_Optimized_Count;

    ++GlobalDebugTable->CurrentEventArrayIndex;
    if (GlobalDebugTable->CurrentEventArrayIndex >= ArrayCount(GlobalDebugTable->Events)) {
        GlobalDebugTable->CurrentEventArrayIndex = 0;
    }
    u64 ArrayIndex_EventIndex = AtomicExchangeU64(&GlobalDebugTable->EventArrayIndex_EventIndex,
                                                  (u64)GlobalDebugTable->CurrentEventArrayIndex << 32);

    u32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
    u32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;
    GlobalDebugTable->EventCount[EventArrayIndex] = EventCount;

    debug_state *DebugState = DEBUGGetState(Memory);
    if (DebugState) {
        if (!DebugState->Paused) {
            if (DebugState->FrameCount >= 4 * MAX_DEBUG_EVENT_ARRAY_COUNT) {
                RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
            }
            CollateDebugRecords(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
        }
    }

    return GlobalDebugTable;
}
