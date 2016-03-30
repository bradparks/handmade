#ifndef HANDMADE_ASSET_H
#define HANDMADE_ASSET_H

struct hero_bitmaps {
    loaded_bitmap Head;
    loaded_bitmap Cape;
    loaded_bitmap Torso;
};

enum asset_state {
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked,
};

struct asset_slot {
    asset_state State;
    loaded_bitmap *Bitmap;
};

enum asset_tag_id {
    Tag_None,

    Tag_Smoothness,
    Tag_Flatness,

    Tag_Count,
};

enum asset_type_id {
    Asset_None,

    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,

    Asset_Grass,
    Asset_Tuft,
    Asset_Stone,

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

struct asset_type {
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

struct asset_bitmap_info {
    char *FileName;
    v2 AlignPercentage;
};

struct asset_group {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct game_assets {
    // TODO: Not thrilled about this back-pointer
    struct transient_state *TranState;
    memory_arena Arena;

    uint32 BitmapCount;
    asset_bitmap_info *BitmapInfos;
    asset_slot *Bitmaps;

    uint32 SoundCount;
    asset_slot *Sounds;

    uint32 AssetCount;
    asset *Assets;

    uint32 TagCount;
    asset_tag *Tags;

    asset_type AssetTypes[Asset_Count];

    // NOTE: Structured assets
    hero_bitmaps HeroBitmaps[4];

    // TODO: These should go away once we actually load a asset pack file
    uint32 DEBUGUsedBitmapCount;
    uint32 DEBUGUsedAssetCount;
    asset_type *DEBUGAssetType;
};

struct bitmap_id {
    uint32 Value;
};

struct audio_id {
    uint32 Value;
};


inline loaded_bitmap *
GetBitmap(game_assets *Assets, bitmap_id ID) {
    loaded_bitmap *Result = Assets->Bitmaps[ID.Value].Bitmap;

    return Result;
}
internal void LoadBitmap(game_assets *Assets, bitmap_id ID);
internal void LoadSound(game_assets *Assets, audio_id ID);

#endif