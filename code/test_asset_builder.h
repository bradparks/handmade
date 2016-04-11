#ifndef TEST_ASSET_BUILDER_H
#define TEST_ASSET_BUILDER_H

struct bitmap_id {
    uint32 Value;
};

struct sound_id {
    uint32 Value;
};

struct asset_bitmap_info {
    char *FileName;
    r32 AlignPercentage[2];
};

struct asset_sound_info {
    char *FileName;
    u32 FirstSampleIndex;
    u32 SampleCount;
    sound_id NextIDToPlay;
};

struct asset {
    u64 DataOffset;
    u32 FirstTagIndex;
    u32 OnePastLastTagIndex;

    union {
        asset_bitmap_info Bitmap;
        asset_sound_info Sound;
    };
};

struct bitmap_asset {
    char *Filename;
    r32 Alignment[2];
};

#define VERY_LARGE_NUMBER 4096

struct game_assets {
    u32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

    u32 AssetTypeCount;
    hha_asset_type AssetTypes[Asset_Count];

    u32 AssetCount;
    asset Assets[VERY_LARGE_NUMBER];

    hha_asset_type *DEBUGAssetType;
    asset *DEBUGAsset;
};

#endif
