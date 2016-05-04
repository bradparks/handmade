struct load_asset_work {
    task_with_memory *Task;
    asset *Asset;

    platform_file_handle *Handle;
    u64 Offset;
    u64 Size;
    void *Destination;

    u32 FinalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork) {
    load_asset_work *Work = (load_asset_work *)Data;

    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);

    CompletePreviousWritesBeforeFutureWrites;

    if (!PlatformNoFileErrors(Work->Handle)) {
        // TODO: Should we actually fill in bogus data here and set to final state anyway?
        ZeroSize((memory_index)Work->Size, Work->Destination);
    }

    Work->Asset->State = Work->FinalState;

    EndTaskWithMemory(Work->Task);
}

inline platform_file_handle *
GetFileHandleFor(game_assets *Assets, u32 FileIndex) {
    Assert(FileIndex < Assets->FileCount);
    platform_file_handle *Result = &Assets->Files[FileIndex].Handle;

    return Result;
}

internal void
RemoveAssetHeaderFromList(asset_memory_header *Header) {
    Header->Prev->Next = Header->Next;
    Header->Next->Prev = Header->Prev;

    Header->Next = Header->Prev = 0;
}

internal asset_memory_block *
FindBlockForSize(game_assets *Assets, memory_index Size) {
    asset_memory_block *Result = 0;

    // TODO: This probably will need to be accelerated in the
    // future
    // TODO: Best match block!
    for (asset_memory_block *Block = Assets->MemorySentinel.Next;
         Block != &Assets->MemorySentinel;
         Block = Block->Next)
    {
        if (!(Block->Flags & AssetMemory_Used)) {
            if (Block->Size >= Size) {
                Result = Block;
                break;
            }
        }
    }

    return Result;
}

internal asset_memory_block *
InsertBlock(asset_memory_block *Prev, u32 Size, void *Memory) {
    Assert(Size > sizeof(asset_memory_block));
    asset_memory_block *Block = (asset_memory_block *)Memory;
    Block->Flags = 0;
    Block->Size = Size - sizeof(asset_memory_block);
    Block->Prev = Prev;
    Block->Next = Prev->Next;
    Block->Prev->Next = Block;
    Block->Next->Prev = Block;

    return Block;
}

internal b32
MergeIfPossible(game_assets *Assets, asset_memory_block *First, asset_memory_block *Second) {
    b32 Result = false;

    if (First != &Assets->MemorySentinel &&
        Second != &Assets->MemorySentinel)
    {
        if (!(First->Flags & AssetMemory_Used) &&
            !(Second->Flags & AssetMemory_Used))
        {
            u8 *ExpectedSecond = (u8 *)First + sizeof(asset_memory_block) + First->Size;
            if ((u8 *)Second == ExpectedSecond) {
                Second->Next->Prev = Second->Prev;
                Second->Prev->Next = Second->Next;
                First->Size += sizeof(asset_memory_block) + Second->Size;

                Result = true;
            }
        }
    }

    return Result;
}

inline void *
AcquireAssetMemory(game_assets *Assets, memory_index Size) {
    void *Result = 0;

    asset_memory_block *Block = FindBlockForSize(Assets, Size);
    for (;;) {
        if (Block && Size <= Block->Size) {
            Block->Flags |= AssetMemory_Used;

            Result = (u8 *)(Block + 1);

            memory_index RemainingSize = Block->Size - Size;
            memory_index BlockSplitThreshold = 4096; // TODO: Set this based on the smallest asset size
            if (RemainingSize > BlockSplitThreshold) {
                Block->Size -= RemainingSize;
                InsertBlock(Block, RemainingSize, (u8 *)Result + Size);
            } else {
                // TODO: Actually record the unused portion of the memory
                // in a block so that we can do the merge on blocks when neighbors
                // are freed
            }

            break;
        } else {
            for (asset_memory_header *Header = Assets->LoadedAssetSentinel.Prev;
                 Header != &Assets->LoadedAssetSentinel;
                 Header = Header->Prev)
            {
                asset *Asset = Assets->Assets + Header->AssetIndex;
                if (GetState(Asset) >= AssetState_Loaded) {
                    u32 AssetIndex = Header->AssetIndex;
                    asset *Asset = Assets->Assets + AssetIndex;

                    Assert(GetState(Asset) == AssetState_Loaded);
                    Assert(!IsLocked(Asset));

                    RemoveAssetHeaderFromList(Header);

                    Block = (asset_memory_block *)Asset->Header - 1;
                    Block->Flags &= ~AssetMemory_Used;
                    if (MergeIfPossible(Assets, Block->Prev, Block)) {
                        Block = Block->Prev;
                    }

                    MergeIfPossible(Assets, Block, Block->Next);

                    Asset->State = AssetState_Unloaded;
                    Asset->Header = 0;

                    break;
                }
            }
        }
    }

    return Result;
}

struct asset_memory_size {
    u32 Total;
    u32 Data;
    u32 Section;
};

inline void
InsertAssetHeaderAtFront(game_assets *Assets, asset_memory_header *Header) {
    asset_memory_header *Sentinel = &Assets->LoadedAssetSentinel;

    Header->Prev = Sentinel;
    Header->Next = Sentinel->Next;

    Header->Next->Prev = Header;
    Header->Prev->Next = Header;
}

internal void
AddAssetHeaderToList(game_assets *Assets, u32 AssetIndex, asset_memory_size Size) {
    asset_memory_header *Header = Assets->Assets[AssetIndex].Header;
    Header->AssetIndex = AssetIndex;
    Header->TotalSize = Size.Total;
    InsertAssetHeaderAtFront(Assets, Header);
}

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID, b32 Locked) {
    asset *Asset = Assets->Assets + ID.Value;

    if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) ==
         AssetState_Unloaded))
    {

        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {
            hha_bitmap *Info = &Asset->HHA.Bitmap;

            asset_memory_size Size = {};
            u32 Width = Info->Dim[0];
            u32 Height = Info->Dim[1];
            Size.Section = 4 * Width;
            Size.Data = Height * Size.Section;
            Size.Total = Size.Data + sizeof(asset_memory_header);

            Asset->Header = (asset_memory_header *)AcquireAssetMemory(Assets, Size.Total);

            loaded_bitmap *Bitmap = &Asset->Header->Bitmap;

            Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
            Bitmap->WidthOverHeight = (r32)Info->Dim[0] / (r32)Info->Dim[1];
            Bitmap->Width = Info->Dim[0];
            Bitmap->Height = Info->Dim[1];
            Bitmap->Pitch = Size.Section;
            Bitmap->Memory = (Asset->Header + 1);

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Asset = Assets->Assets + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->HHA.DataOffset;
            Work->Size = Size.Data;
            Work->Destination = Bitmap->Memory;
            Work->FinalState = (AssetState_Loaded | (Locked ? AssetState_Lock : 0));

            Asset->State |= AssetState_Lock;

            if (!Locked) {
                AddAssetHeaderToList(Assets, ID.Value, Size);
            }

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        } else {
            Asset->State = AssetState_Unloaded;
        }
    }
}

internal void
LoadSound(game_assets *Assets, sound_id ID) {
    asset *Asset = Assets->Assets + ID.Value;

    if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) ==
         AssetState_Unloaded))
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {
            hha_sound *Info = &Asset->HHA.Sound;

            asset_memory_size Size = {};
            Size.Section = Info->SampleCount * sizeof(int16);
            Size.Data = Info->ChannelCount * Size.Section;
            Size.Total = Size.Data + sizeof(asset_memory_header);

            Asset->Header = (asset_memory_header *)AcquireAssetMemory(Assets, Size.Total);

            loaded_sound *Sound = &Asset->Header->Sound;
            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;

            u32 ChannelSize = Size.Section;

            void *Memory = (Asset->Header + 1);
            u8 *SoundAt = (u8 *)Memory;
            for (u32 ChannelIndex = 0; ChannelIndex < Sound->ChannelCount; ++ChannelIndex) {
                Sound->Samples[ChannelIndex] = (s16 *)SoundAt;
                SoundAt += ChannelSize;
            }

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Asset = Assets->Assets + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->HHA.DataOffset;
            Work->Size = Size.Data;
            Work->Destination = Memory;
            Work->FinalState = AssetState_Loaded;

            AddAssetHeaderToList(Assets, ID.Value, Size);

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        } else {
            Assets->Assets[ID.Value].State = AssetState_Unloaded;
        }
    }
}

internal uint32
GetBestMatchAssetFrom(game_assets *Assets, asset_type_id TypeID,
                      asset_vector *MatchVector, asset_vector *WeightVector)
{
    uint32 Result = 0;
    real32 BestDiff = Real32Maximum;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        for (uint32 AssetIndex = Type->FirstAssetIndex;
             AssetIndex < Type->OnePastLastAssetIndex;
             ++AssetIndex)
        {
            asset *Asset = Assets->Assets + AssetIndex;

            real32 TotalWeightedDiff = 0.0f;
            for (uint32 TagIndex = Asset->HHA.FirstTagIndex;
                 TagIndex < Asset->HHA.OnePastLastTagIndex;
                 ++TagIndex)
            {
                hha_tag *Tag = Assets->Tags + TagIndex;

                real32 A = MatchVector->E[Tag->ID];
                real32 B = Tag->Value;
                real32 D0 = AbsoluteValue(A - B);
                real32 D1 = AbsoluteValue((A - Assets->TagRange[Tag->ID] * SignOf(A)) - B);
                real32 Difference = Minimum(D0, D1);

                real32 Weighted = WeightVector->E[Tag->ID] * AbsoluteValue(Difference);
                TotalWeightedDiff += Weighted;
            }

            if (BestDiff > TotalWeightedDiff) {
                BestDiff = TotalWeightedDiff;
                Result = AssetIndex;
            }
        }
    }

    return Result;
}

internal uint32
GetRandomAssetFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series) {
    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        uint32 Count = (Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
        uint32 Choice = RandomChoice(Series, Count);

        Result = Type->FirstAssetIndex + Choice;
    }

    return Result;
}

internal uint32
GetFirstAssetFrom(game_assets *Assets, asset_type_id TypeID) {
    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        Result = Type->FirstAssetIndex;
    }

    return Result;
}

internal bitmap_id
GetBestMatchBitmapFrom(game_assets *Assets, asset_type_id TypeID,
               asset_vector *MatchVector, asset_vector *WeightVector)
{
    bitmap_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};

    return Result;
}

internal bitmap_id
GetFirstBitmapFrom(game_assets *Assets, asset_type_id TypeID) {
    bitmap_id Result = {GetFirstAssetFrom(Assets, TypeID)};

    return Result;
}

internal bitmap_id
GetRandomBitmapFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series) {
    bitmap_id Result = {GetRandomAssetFrom(Assets, TypeID, Series)};

    return Result;
}

internal sound_id
GetBestMatchSoundFrom(game_assets *Assets, asset_type_id TypeID,
               asset_vector *MatchVector, asset_vector *WeightVector)
{
    sound_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};

    return Result;
}

internal sound_id
GetFirstSoundFrom(game_assets *Assets, asset_type_id TypeID) {
    sound_id Result = {GetFirstAssetFrom(Assets, TypeID)};

    return Result;
}

internal sound_id
GetRandomSoundFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series) {
    sound_id Result = {GetRandomAssetFrom(Assets, TypeID, Series)};

    return Result;
}

internal game_assets *
AllocateGameAssets(memory_arena *Arena, memory_index Size, transient_state *TranState) {
    game_assets *Assets = PushStruct(Arena, game_assets);

    Assets->MemorySentinel.Flags = 0;
    Assets->MemorySentinel.Size = 0;
    Assets->MemorySentinel.Prev = &Assets->MemorySentinel;
    Assets->MemorySentinel.Next = &Assets->MemorySentinel;

    InsertBlock(&Assets->MemorySentinel, Size, PushSize(Arena, Size));

    Assets->TranState = TranState;

    Assets->LoadedAssetSentinel.Next = &Assets->LoadedAssetSentinel;
    Assets->LoadedAssetSentinel.Prev = &Assets->LoadedAssetSentinel;

    for (uint32 TagType = 0; TagType < Tag_Count; ++TagType) {
        Assets->TagRange[TagType] = 1000000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = Tau32;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;

    {
        platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin("hha");
        Assets->FileCount = FileGroup.FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
            asset_file *File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(&FileGroup);
            Platform.ReadDataFromFile(&File->Handle, 0, sizeof(File->Header), &File->Header);

            u32 AssetTypeArraySize = File->Header.AssetTypeCount * sizeof(hha_asset_type);
            File->AssetTypeArray = (hha_asset_type *)PushSize(Arena, AssetTypeArraySize);
            Platform.ReadDataFromFile(&File->Handle, File->Header.AssetTypes,
                                      AssetTypeArraySize, File->AssetTypeArray);

            if (File->Header.MagicValue != HHA_MAGIC_VALUE) {
                Platform.FileError(&File->Handle, "HHA file has an invalid magic value.");

            }

            if (File->Header.Version != HHA_VERSION) {
                Platform.FileError(&File->Handle, "HHA file is of a later version.");
            }

            if (PlatformNoFileErrors(&File->Handle)) {
                // NOTE: The first asset and tag slot in every HHA is a null asset (reserved)
                // so we don't count it as something we will need space for!
                Assets->TagCount += (File->Header.TagCount - 1);
                Assets->AssetCount += (File->Header.AssetCount - 1);
            } else {
                // TODO: Eventually, have some way of notifying users of bogus files?
                InvalidCodePath;
            }
        }

        Platform.GetAllFilesOfTypeEnd(&FileGroup);
    }

    // NOTE: Allocate all metadata space
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

    // NOTE: Reserve one null tag at the begining
    ZeroStruct(Assets->Tags[0]);

    // NOTE: Load tags
    for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
        asset_file *File = Assets->Files + FileIndex;
        if (PlatformNoFileErrors(&File->Handle)) {
            // NOTE: Skip the first tag, since it's null
            u32 TagArraySize = sizeof(hha_tag) * (File->Header.TagCount - 1);
            Platform.ReadDataFromFile(&File->Handle, File->Header.Tags + sizeof(hha_tag),
                                      TagArraySize, Assets->Tags + File->TagBase);
        }
    }

    // NOTE: Reserve one null asset at the begining
    u32 AssetCount = 0;
    ZeroStruct(*(Assets->Assets + AssetCount));
    ++AssetCount;

    // TODO: Exercise for the reader - how would you do this in a way
    // that sacled gracefully to hundreds of asset pack files? (or more!)

    for (u32 DestTypeID = 0; DestTypeID < Asset_Count; ++DestTypeID) {
        asset_type *DestType = Assets->AssetTypes + DestTypeID;
        DestType->FirstAssetIndex = AssetCount;

        for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
            asset_file *File = Assets->Files + FileIndex;
            if (PlatformNoFileErrors(&File->Handle)) {
                for (u32 SourceIndex = 0; SourceIndex < File->Header.AssetTypeCount; ++SourceIndex) {
                    hha_asset_type *SourceType = File->AssetTypeArray + SourceIndex;
                    if (SourceType->TypeID == DestTypeID) {
                        u32 AssetCountForType = (SourceType->OnePastLastAssetIndex -
                                                 SourceType->FirstAssetIndex);

                        temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
                        hha_asset *HHAAssetArray = PushArray(&TranState->TranArena,
                                                             AssetCountForType,
                                                             hha_asset);

                        Platform.ReadDataFromFile(&File->Handle,
                                                  File->Header.Assets + SourceType->FirstAssetIndex * sizeof(hha_asset),
                                                  AssetCountForType * sizeof(hha_asset),
                                                  HHAAssetArray);
                        for (u32 AssetIndex = 0; AssetIndex < AssetCountForType; ++AssetIndex) {
                            hha_asset *HHAAsset = HHAAssetArray + AssetIndex;

                            Assert(AssetCount < Assets->AssetCount);
                            asset *Asset = Assets->Assets + AssetCount++;

                            Asset->FileIndex = FileIndex;
                            Asset->HHA = *HHAAsset;
                            if (Asset->HHA.FirstTagIndex == 0) {
                                Asset->HHA.FirstTagIndex = Asset->HHA.OnePastLastTagIndex = 0;
                            } else {
                                Asset->HHA.FirstTagIndex += (File->TagBase - 1);
                                Asset->HHA.OnePastLastTagIndex += (File->TagBase - 1);
                            }
                        }

                        EndTemporaryMemory(TempMem);
                    }
                }
            }
        }

        DestType->OnePastLastAssetIndex = AssetCount;
    }

    Assert(AssetCount == Assets->AssetCount);

    return Assets;
}

internal void
MoveHeaderToFront(game_assets *Assets, asset *Asset) {
    if (!IsLocked(Asset)) {
        asset_memory_header *Header = Asset->Header;

        RemoveAssetHeaderFromList(Header);
        InsertAssetHeaderAtFront(Assets, Header);
    }
}

