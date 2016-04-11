#include <stdio.h>
#include <stdlib.h>

#include "handmade_platform.h"
#include "handmade_asset_type_id.h"
#include "handmade_file_format.h"
#include "test_asset_builder.h"

FILE *Out = 0;

internal void
BeginAssetType(game_assets *Assets, asset_type_id TypeID) {
    Assert(Assets->DEBUGAssetType == 0);

    Assets->DEBUGAssetType = Assets->AssetTypes + TypeID;
    Assets->DEBUGAssetType->TypeID = TypeID;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets *Assets, char *FileName, r32 AlignPercentageX = 0.5f, r32 AlignPercentageY = 0.5f) {
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));

    bitmap_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset *Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->TagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Bitmap.FileName = FileName;
    Asset->Bitmap.AlignPercentage[0] = AlignPercentageX;
    Asset->Bitmap.AlignPercentage[1] = AlignPercentageX;

    Assets->DEBUGAsset = Asset;

    return Result;
}

internal sound_id
AddSoundAsset(game_assets *Assets, char *FileName, u32 FirstSampleIndex = 0, u32 SampleCount = 0) {
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets));

    sound_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset *Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->TagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Sound.FileName = FileName;
    Asset->Sound.FirstSampleIndex = FirstSampleIndex;
    Asset->Sound.SampleCount = SampleCount;
    Asset->Sound.NextIDToPlay.Value = 0;

    Assets->DEBUGAsset = Asset;

    return Result;
}

internal void
AddTag(game_assets *Assets, asset_tag_id ID, real32 Value) {
    Assert(Assets->DEBUGAsset);
    Assert(Assets->TagCount < Assets->AssetCount);

    ++Assets->DEBUGAsset->OnePastLastTagIndex;
    hha_tag *Tag = Assets->Tags + Assets->TagCount++;

    Tag->ID = ID;
    Tag->Value = Value;
}

internal void
EndAssetType(game_assets *Assets) {
    Assert(Assets->DEBUGAssetType);
    Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->DEBUGAsset = 0;
}

int
main(void) {
    game_assets Assets_;
    game_assets *Assets = &Assets_;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->DEBUGAssetType = 0;
    Assets->DEBUGAsset = 0;

    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/test_hero_shadow.bmp", 0.5f, 0.156682029f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test2/tree00.bmp", 0.5f, 0.156682029f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test2/rock03.bmp", 0.5f, 0.65625f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Grass);
    AddBitmapAsset(Assets, "test2/grass00.bmp");
    AddBitmapAsset(Assets, "test2/grass01.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tuft);
    AddBitmapAsset(Assets, "test2/tuft00.bmp");
    AddBitmapAsset(Assets, "test2/tuft01.bmp");
    AddBitmapAsset(Assets, "test2/tuft02.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Stone);
    AddBitmapAsset(Assets, "test2/ground00.bmp");
    AddBitmapAsset(Assets, "test2/ground01.bmp");
    AddBitmapAsset(Assets, "test2/ground02.bmp");
    AddBitmapAsset(Assets, "test2/ground03.bmp");
    EndAssetType(Assets);

    real32 AngleRight = 0.0f * Tau32;
    real32 AngleBack = 0.25f * Tau32;
    real32 AngleLeft = 0.5f * Tau32;
    real32 AngleFront = 0.75f * Tau32;

    r32 HeroAlign[] = {0.5f, 0.156682029f};

    BeginAssetType(Assets, Asset_Head);
    AddBitmapAsset(Assets, "test/test_hero_right_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Cape);
    AddBitmapAsset(Assets, "test/test_hero_right_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Torso);
    AddBitmapAsset(Assets, "test/test_hero_right_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/test_hero_back_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/test_hero_left_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/test_hero_front_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    //
    //
    //
    BeginAssetType(Assets, Asset_Bloop);
    AddSoundAsset(Assets, "test3/bloop_00.wav");
    AddSoundAsset(Assets, "test3/bloop_01.wav");
    AddSoundAsset(Assets, "test3/bloop_02.wav");
    AddSoundAsset(Assets, "test3/bloop_03.wav");
    AddSoundAsset(Assets, "test3/bloop_04.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Crack);
    AddSoundAsset(Assets, "test3/crack_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Drop);
    AddSoundAsset(Assets, "test3/drop_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Glide);
    AddSoundAsset(Assets, "test3/glide_00.wav");
    EndAssetType(Assets);

    u32 OneMusicChunk = 1 * 48000;
    u32 TotalMusicSampleCount = 7468095;
    BeginAssetType(Assets, Asset_Music);
    sound_id LastMusic = {0};
    for (u32 FirstSampleIndex = 0;
         FirstSampleIndex < TotalMusicSampleCount;
         FirstSampleIndex += OneMusicChunk)
    {
        u32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
        if (SampleCount > OneMusicChunk) {
            SampleCount = OneMusicChunk;
        }

        sound_id ThisMusic = AddSoundAsset(Assets, "test3/music_test.wav", FirstSampleIndex, SampleCount);
        if (LastMusic.Value) {
            Assets->Assets[LastMusic.Value].Sound.NextIDToPlay = ThisMusic;
        }
        LastMusic = ThisMusic;
    }
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Puhp);
    AddSoundAsset(Assets, "test3/puhp_00.wav");
    AddSoundAsset(Assets, "test3/puhp_01.wav");
    EndAssetType(Assets);

    Out = fopen("test.hha", "wb");
    if (Out) {
        hha_header Header = {};
        Header.MagicValue = HHA_MAGIC_VALUE;
        Header.Version = HHA_VERSION;
        Header.TagCount = Assets->TagCount;
        Header.AssetTypeCount = Asset_Count; // TODO: Sparseness!
        Header.AssetCount = Assets->AssetCount;

        u32 TagArraySize = Header.TagCount * sizeof(hha_tag);
        u32 AssetTypeArraySize = Header.AssetTypeCount * sizeof(hha_asset_type);
        u32 AssetArraySize = Header.AssetCount * sizeof(hha_asset);

        Header.Tags = sizeof(Header);
        Header.AssetTypes = Header.Tags  + TagArraySize;
        Header.Assets = Header.AssetTypes + AssetTypeArraySize;

        fwrite(&Header, sizeof(Header), 1, Out);
        fwrite(Assets->Tags, TagArraySize, 1, Out);
        fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);
        //fwrite(AssetArray, AssetArraySize, 1, Out);

        fclose(Out);
    } else {
        printf("ERROR: Couldn't open file :(\n");
    }

}
