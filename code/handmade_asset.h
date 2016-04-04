#ifndef HANDMADE_ASSET_H
#define HANDMADE_ASSET_H

struct bitmap_id {
    uint32 Value;
};

struct sound_id {
    uint32 Value;
};

struct loaded_sound {
    uint32 SampleCount;
    uint32 ChannelCount;
    int16 *Samples[2];
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

enum asset_tag_id {
    Tag_Smoothness,
    Tag_Flatness,
    Tag_FacingDirection, // NOTE: Angle in radians off of due right

    Tag_Count,
};

enum asset_type_id {
    Asset_None,

    //
    // NOTE: Bitmaps!
    //

    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,

    Asset_Grass,
    Asset_Tuft,
    Asset_Stone,

    Asset_Head,
    Asset_Cape,
    Asset_Torso,

    //
    // NOTE: Sounds!
    //

    Asset_Bloop,
    Asset_Crack,
    Asset_Drop,
    Asset_Glide,
    Asset_Music,
    Asset_Puhp,

    //
    //
    //

    Asset_Count,
};

struct asset_tag {
    uint32 ID; // NOTE: Tag ID
    real32 Value;
};

struct asset {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
    uint32 SlotID;
};

struct asset_vector {
    real32 E[Tag_Count];
};

struct asset_type {
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
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

struct asset_group {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct game_assets {
    // TODO: Not thrilled about this back-pointer
    struct transient_state *TranState;
    memory_arena Arena;

    real32 TagRange[Tag_Count];

    uint32 BitmapCount;
    asset_bitmap_info *BitmapInfos;
    asset_slot *Bitmaps;

    uint32 SoundCount;
    asset_sound_info *SoundInfos;
    asset_slot *Sounds;

    uint32 AssetCount;
    asset *Assets;

    uint32 TagCount;
    asset_tag *Tags;

    asset_type AssetTypes[Asset_Count];

    // TODO: These should go away once we actually load a asset pack file
    uint32 DEBUGUsedBitmapCount;
    uint32 DEBUGUsedSoundCount;
    uint32 DEBUGUsedAssetCount;
    uint32 DEBUGUsedTagCount;
    asset_type *DEBUGAssetType;
    asset *DEBUGAsset;
};


inline loaded_bitmap *
GetBitmap(game_assets *Assets, bitmap_id ID) {
    Assert(ID.Value <= Assets->BitmapCount);

    loaded_bitmap *Result = Assets->Bitmaps[ID.Value].Bitmap;

    return Result;
}

inline loaded_sound *
GetSound(game_assets *Assets, sound_id ID) {
    Assert(ID.Value <= Assets->SoundCount);
    loaded_sound *Result = Assets->Sounds[ID.Value].Sound;

    return Result;
}

inline asset_sound_info *
GetSoundInfo(game_assets *Assets, sound_id ID) {
    Assert(ID.Value <= Assets->SoundCount);

    asset_sound_info *Result = Assets->SoundInfos + ID.Value;

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

#endif
