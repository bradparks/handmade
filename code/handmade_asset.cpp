struct load_asset_work {
    task_with_memory *Task;
    asset_slot *Slot;

    platform_file_handle *Handle;
    u64 Offset;
    u64 Size;
    void *Destination;

    asset_state FinalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork) {
    load_asset_work *Work = (load_asset_work *)Data;

    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);

    CompletePreviousWritesBeforeFutureWrites;

    // TODO: Should we actually fill in bogus data here and set to final state anyway?
    if (PlatformNoFileErrors(Work->Handle)) {
        Work->Slot->State = Work->FinalState;
    }

    EndTaskWithMemory(Work->Task);
}

inline platform_file_handle *
GetFileHandleFor(game_assets *Assets, u32 FileIndex) {
    Assert(FileIndex < Assets->FileCount);
    platform_file_handle *Result = Assets->Files[FileIndex].Handle;

    return Result;
}

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID) {
    asset_slot *Slot = Assets->Slots + ID.Value;

    if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Slot->State, AssetState_Queued, AssetState_Unloaded) ==
         AssetState_Unloaded))
    {

        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {
            asset *Asset = Assets->Assets + ID.Value;
            hha_bitmap *Info = &Asset->HHA.Bitmap;
            loaded_bitmap *Bitmap = &Slot->Bitmap;

            Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
            Bitmap->WidthOverHeight = (r32)Info->Dim[0] / (r32)Info->Dim[1];
            Bitmap->Width = SafeTruncateToUInt16(Info->Dim[0]);
            Bitmap->Height = SafeTruncateToUInt16(Info->Dim[1]);
            Bitmap->Pitch = SafeTruncateToUInt16(4 * Info->Dim[0]);
            u32 MemorySize = Bitmap->Pitch * Bitmap->Height;
            Bitmap->Memory = PushSize(&Assets->Arena, MemorySize);

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Slot = Assets->Slots + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->HHA.DataOffset;
            Work->Size = MemorySize;
            Work->Destination = Bitmap->Memory;
            Work->FinalState = AssetState_Loaded;

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        } else {
            Slot->State = AssetState_Unloaded;
        }
    }
}

internal void
LoadSound(game_assets *Assets, sound_id ID) {
    asset_slot *Slot = Assets->Slots + ID.Value;

    if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Slot->State, AssetState_Queued, AssetState_Unloaded) ==
         AssetState_Unloaded))
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {
            asset *Asset = Assets->Assets + ID.Value;
            hha_sound *Info = &Asset->HHA.Sound;
            loaded_sound *Sound = &Slot->Sound;
            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;
            u32 ChannelSize = Sound->SampleCount * sizeof(s16);
            u32 MemorySize = Sound->ChannelCount * ChannelSize;
            void *Memory = PushSize(&Assets->Arena, MemorySize);

            u8 *SoundAt = (u8 *)Memory;
            for (u32 ChannelIndex = 0; ChannelIndex < Sound->ChannelCount; ++ChannelIndex) {
                Sound->Samples[ChannelIndex] = (s16 *)SoundAt;
                SoundAt += ChannelSize;
            }

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Slot = Assets->Slots + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->HHA.DataOffset;
            Work->Size = MemorySize;
            Work->Destination = Memory;
            Work->FinalState = AssetState_Loaded;

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        } else {
            Assets->Slots[ID.Value].State = AssetState_Unloaded;
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
GetRandomSlotFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series) {
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
GetFirstSlotFrom(game_assets *Assets, asset_type_id TypeID) {
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
    bitmap_id Result = {GetFirstSlotFrom(Assets, TypeID)};

    return Result;
}

internal bitmap_id
GetRandomBitmapFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series) {
    bitmap_id Result = {GetRandomSlotFrom(Assets, TypeID, Series)};

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
    sound_id Result = {GetFirstSlotFrom(Assets, TypeID)};

    return Result;
}

internal sound_id
GetRandomSoundFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series) {
    sound_id Result = {GetRandomSlotFrom(Assets, TypeID, Series)};

    return Result;
}

internal game_assets *
AllocateGameAssets(memory_arena *Arena, memory_index Size, transient_state *TranState) {
    game_assets *Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->TranState = TranState;

    for (uint32 TagType = 0; TagType < Tag_Count; ++TagType) {
        Assets->TagRange[TagType] = 1000000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = Tau32;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;

    {
        platform_file_group *FileGroup = Platform.GetAllFilesOfTypeBegin("hha");
        Assets->FileCount = FileGroup->FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
            asset_file *File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(FileGroup);
            Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);

            u32 AssetTypeArraySize = File->Header.AssetTypeCount * sizeof(hha_asset_type);
            File->AssetTypeArray = (hha_asset_type *)PushSize(Arena, AssetTypeArraySize);
            Platform.ReadDataFromFile(File->Handle, File->Header.AssetTypes,
                                     AssetTypeArraySize, File->AssetTypeArray);

            if (File->Header.MagicValue != HHA_MAGIC_VALUE) {
                Platform.FileError(File->Handle, "HHA file has an invalid magic value.");

            }

            if (File->Header.Version != HHA_VERSION) {
                Platform.FileError(File->Handle, "HHA file is of a later version.");
            }

            if (PlatformNoFileErrors(File->Handle)) {
                // NOTE: The first asset and tag slot in every HHA is a null asset (reserved)
                // so we don't count it as something we will need space for!
                Assets->TagCount += (File->Header.TagCount - 1);
                Assets->AssetCount += (File->Header.AssetCount - 1);
            } else {
                // TODO: Eventually, have some way of notifying users of bogus files?
                InvalidCodePath;
            }
        }

        Platform.GetAllFilesOfTypeEnd(FileGroup);
    }

    // NOTE: Allocate all metadata space
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);
    Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

    // NOTE: Reserve one null tag at the begining
    ZeroStruct(Assets->Tags[0]);

    // NOTE: Load tags
    for (u32 FileIndex = 0; FileIndex < Assets->FileCount; ++FileIndex) {
        asset_file *File = Assets->Files + FileIndex;
        if (PlatformNoFileErrors(File->Handle)) {
            // NOTE: Skip the first tag, since it's null
            u32 TagArraySize = sizeof(hha_tag) * (File->Header.TagCount - 1);
            Platform.ReadDataFromFile(File->Handle, File->Header.Tags + sizeof(hha_tag),
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
            if (PlatformNoFileErrors(File->Handle)) {
                for (u32 SourceIndex = 0; SourceIndex < File->Header.AssetTypeCount; ++SourceIndex) {
                    hha_asset_type *SourceType = File->AssetTypeArray + SourceIndex;
                    if (SourceType->TypeID == DestTypeID) {
                        u32 AssetCountForType = (SourceType->OnePastLastAssetIndex -
                                                 SourceType->FirstAssetIndex);

                        temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
                        hha_asset *HHAAssetArray = PushArray(&TranState->TranArena,
                                                             AssetCountForType,
                                                             hha_asset);

                        Platform.ReadDataFromFile(File->Handle,
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
