#ifndef HANDMADE_ASSET_H
#define HANDMADE_ASSET_H

struct loaded_sound {
    // TODO: This could be shruck to 12 bytes if the loaded_bitmap
    // ever got down that small!
    int16 *Samples[2];
    u32 SampleCount;
    u32 ChannelCount;
};

// TODO: Stramling this, by using header pointer as an indicator of unloaded status?
enum asset_state {
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Operating,
};

struct asset_memory_header {
    asset_memory_header *Next;
    asset_memory_header *Prev;

    u32 AssetIndex;
    u32 TotalSize;
    u32 GenerationID;
    union {
        loaded_bitmap Bitmap;
        loaded_sound Sound;
    };
};

struct asset {
    u32 State;
    asset_memory_header *Header;

    hha_asset HHA;
    u32 FileIndex;
};

struct asset_vector {
    real32 E[Tag_Count];
};

struct asset_type {
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

struct asset_file {
    platform_file_handle Handle;

    // TODO: If we ever do thread stacks,
    // AssetTypeArray doesn't need to be kept here probably.
    hha_header Header;
    hha_asset_type *AssetTypeArray;

    u32 TagBase;
};

enum asset_memory_block_flags {
    AssetMemory_Used = 0x1,
};

struct asset_memory_block {
    asset_memory_block *Prev;
    asset_memory_block *Next;
    u64 Flags;
    memory_index Size;
};

struct game_assets {
    // TODO: Not thrilled about this back-pointer
    struct transient_state *TranState;

    asset_memory_block MemorySentinel;

    asset_memory_header LoadedAssetSentinel;

    real32 TagRange[Tag_Count];

    u32 FileCount;
    asset_file *Files;

    uint32 AssetCount;
    asset *Assets;

    uint32 TagCount;
    hha_tag *Tags;

    asset_type AssetTypes[Asset_Count];
};

internal void MoveHeaderToFront(game_assets *Assets, asset *Asset);
inline asset_memory_header *GetAsset(game_assets *Assets, u32 ID) {
    Assert(ID <= Assets->AssetCount);
    asset *Asset = Assets->Assets + ID;

    asset_memory_header *Result = 0;
    for (;;) {
        u32 State = Asset->State;
        if (State == AssetState_Loaded) {
            if (AtomicCompareExchangeUInt32(&Asset->State, AssetState_Operating, State) ==
                State)
            {
                Result = Asset->Header;
                MoveHeaderToFront(Assets, Asset);

#if 0
                if (Asset->Header->GenerationID < GenerationID) {
                    Asset->Header->GenerationID = GenerationID;
                }
#endif

                CompletePreviousReadsBeforeFutureReads;

                Asset->State = State;

                break;
            }
        } else if (State != AssetState_Operating) {
            break;
        }
    }

    return Result;
}

inline loaded_bitmap *
GetBitmap(game_assets *Assets, bitmap_id ID) {
    asset_memory_header *Header = GetAsset(Assets, ID.Value);

    loaded_bitmap *Result = Header ? &Header->Bitmap : 0;

    return Result;
}

inline loaded_sound *
GetSound(game_assets *Assets, sound_id ID) {
    asset_memory_header *Header = GetAsset(Assets, ID.Value);

    loaded_sound *Result = Header ? &Header->Sound : 0;

    return Result;
}

inline hha_sound *
GetSoundInfo(game_assets *Assets, sound_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    hha_sound *Result = &Assets->Assets[ID.Value].HHA.Sound;

    return Result;
}

inline bool32
IsValid(bitmap_id ID) {
    bool32 Result = ID.Value != 0;

    return Result;
}

inline bool32
IsValid(sound_id ID) {
    bool32 Result = ID.Value != 0;

    return Result;
}

internal void LoadBitmap(game_assets *Assets, bitmap_id ID);
inline void PrefetchBitmap(game_assets *Assets, bitmap_id ID) { LoadBitmap(Assets, ID); }
internal void LoadSound(game_assets *Assets, sound_id ID);
inline void PrefetchSound(game_assets *Assets, sound_id ID) { LoadSound(Assets, ID); }

inline sound_id
GetNextSoundInChain(game_assets *Assets, sound_id ID) {
    sound_id Result = {};
    hha_sound *Info = GetSoundInfo(Assets, ID);
    switch (Info->Chain) {
        case HHASoundChain_None: {
        } break;

        case HHASoundChain_Loop: {
            Result = ID;
        } break;

        case HHASoundChain_Advance: {
            Result.Value = ID.Value + 1;
        } break;

        default: InvalidCodePath;
    }

    return Result;
}

#endif
