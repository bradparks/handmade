#ifndef HANDMADE_FILE_FORMAT_H
#define HANDMADE_FILE_FORMAT_H

#define HHA_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))

#pragma pack(push, 1)
struct hha_header {
#define HHA_MAGIC_VALUE HHA_CODE('h', 'h', 'a', 'f')
    u32 MagicValue;

#define HHA_VERSION 0
    u32 Version;

    u32 TagCount;
    u32 AssetTypeCount;
    u32 AssetCount;

    u64 Tags; // hha_tag[TagCount]
    u64 AssetTypes; // hha_asset_type[AssetTypeCount]
    u64 Assets; // hha_asset[AssetCount]
};

struct hha_tag {
    u32 ID;
    r32 Value;
};

struct hha_asset_type {
    u32 TypeID;
    u32 FirstAssetIndex;
    u32 OnePastLastAssetIndex;
};

struct hha_bitmap {
    u32 Dim[2];
    r32 AlignPercentage[2];
};

struct hha_sound {
    u32 FirstSampleIndex;
    u32 SampleCount;
    u32 NextIDToPlay;
};

struct hha_asset {
    u64 DataOffset;
    u32 FirstTagIndex;
    u32 OnePastLastTagIndex;
    union {
        hha_bitmap Bitmap;
        hha_sound Sound;
    };
};

#pragma pack(pop)

#endif
