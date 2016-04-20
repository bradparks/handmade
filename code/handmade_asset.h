#ifndef HANDMADE_ASSET_H
#define HANDMADE_ASSET_H

#include "handmade_asset_type_id.h"

struct loaded_sound {
    uint32 SampleCount;
    uint32 ChannelCount;
    int16 *Samples[2];
};

struct asset_bitmap_info {
    char *FileName;
    v2 AlignPercentage;
};

struct asset_sound_info {
    char *FileName;
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    sound_id NextIDToPlay;
};

enum asset_state {
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked,
};

struct asset_slot {
    asset_state State;
    union {
        loaded_bitmap *Bitmap;
        loaded_sound *Sound;
    };
};

struct asset {
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
    platform_file_handle *Handle;

    // TODO: If we ever do thread stacks,
    // AssetTypeArray doesn't need to be kept here probably.
    hha_header Header;
    hha_asset_type *AssetTypeArray;

    u32 TagBase;
};

struct asset_group {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct game_assets {
    // TODO: Not thrilled about this back-pointer
    struct transient_state *TranState;
    memory_arena Arena;

    real32 TagRange[Tag_Count];

    u32 FileCount;
    asset_file *Files;

    uint32 AssetCount;
    asset *Assets;
    asset_slot *Slots;

    uint32 TagCount;
    hha_tag *Tags;

    asset_type AssetTypes[Asset_Count];

#if 0
    u8 *HHAContents;

    // TODO: These should go away once we actually load a asset pack file
    uint32 DEBUGUsedAssetCount;
    uint32 DEBUGUsedTagCount;
    asset_type *DEBUGAssetType;
    asset *DEBUGAsset;
#endif
};


inline loaded_bitmap *
GetBitmap(game_assets *Assets, bitmap_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    asset_slot *Slot = Assets->Slots + ID.Value;

    loaded_bitmap *Result = 0;
    if (Slot->State >= AssetState_Loaded) {
        CompletePreviousReadsBeforeFutureReads;
        Result = Slot->Bitmap;
    }

    return Result;
}

inline loaded_sound *
GetSound(game_assets *Assets, sound_id ID) {
    Assert(ID.Value <= Assets->AssetCount);
    asset_slot *Slot = Assets->Slots + ID.Value;

    loaded_sound *Result = 0;
    if (Slot->State >= AssetState_Loaded) {
        CompletePreviousReadsBeforeFutureReads;
        Result = Slot->Sound;
    }

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
