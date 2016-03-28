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

    Asset_Backdrop,
    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,
    Asset_Stairwell,
    Asset_Rock,

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
    v2 AlignPercentage;
    real32 WidthOverHeight;
    int32 Width;
    int32 Height;
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
    asset_slot *Bitmaps;

    uint32 SoundCount;
    asset_slot *Sounds;

    uint32 AssetCount;
    asset *Assets;

    uint32 TagCount;
    asset_tag *Tags;

    asset_type AssetTypes[Asset_Count];

    // NOTE: Array'd assets
    loaded_bitmap Grass[2];
    loaded_bitmap Stone[4];
    loaded_bitmap Tuft[3];

    // NOTE: Structured assets
    hero_bitmaps HeroBitmaps[4];
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
