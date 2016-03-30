#pragma pack(push, 1)
struct bitmap_header {
    uint16 FileType;
    uint32 FileSize;
    uint16 Reserved1;
    uint16 Reserved2;
    uint32 BitmapOffset;
    uint32 Size;
    int32 Width;
    int32 Height;
    uint16 Planes;
    uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)

inline v2
TopDownAlign(loaded_bitmap *Bitmap, v2 Align) {
    Align.y = (real32)(Bitmap->Height - 1) - Align.y;

    Align.x = SafeRatio0(Align.x, (real32)Bitmap->Width);
    Align.y = SafeRatio0(Align.y, (real32)Bitmap->Height);

    return Align;
}

internal void
SetTopDownAlign(hero_bitmaps *Bitmap, v2 Align) {
    Align = TopDownAlign(&Bitmap->Head, Align);

    Bitmap->Head.AlignPercentage = Align;
    Bitmap->Cape.AlignPercentage = Align;
    Bitmap->Torso.AlignPercentage = Align;
}

internal loaded_bitmap
DEBUGLoadBMP(const char *FileName, v2 AlignPercentage = V2(0.5f, 0.5f))
{
    loaded_bitmap Result = {};

    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(FileName);

    if (ReadResult.ContentsSize != 0) {
        bitmap_header *Header = (bitmap_header *) ReadResult.Contents;
        uint32 *Pixels = (uint32 *) ((uint8 *) ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;
        Result.AlignPercentage = AlignPercentage;
        Result.WidthOverHeight = SafeRatio0((real32)Result.Width, (real32)Result.Height);

        Assert(Result.Height >= 0);
        Assert(Header->Compression == 3);

        // NOTE: If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EIGHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc ... DON"T think this
        // is complete BMP loading code because it isn't!)

        // NOTE: Byte order in memory is determined by the Header itself.
        // So we have to read out the masks and convert the pixels ourselves.
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShiftDown = (int32) RedScan.Index;
        int32 GreenShiftDown = (int32) GreenScan.Index;
        int32 BlueShiftDown = (int32) BlueScan.Index;
        int32 AlphaShiftDown = (int32) AlphaScan.Index;

        uint32 *SourceDest = Pixels;
        for (int32 Y = 0; Y < Header->Height; ++Y) {
            for (int32 X = 0; X < Header->Width; ++X) {
                uint32 C = *SourceDest;

                v4 Texel = { (real32)((C & RedMask) >> RedShiftDown),
                             (real32)((C & GreenMask) >> GreenShiftDown),
                             (real32)((C & BlueMask) >> BlueShiftDown),
                             (real32)((C & AlphaMask) >> AlphaShiftDown) };

                Texel = SRGB255ToLinear1(Texel);
#if 1
                Texel.rgb *= Texel.a;
#endif
                Texel = Linear1ToSRGB255(Texel);

                *SourceDest++ = (((uint32) (Texel.a + 0.5f) << 24) |
                                 ((uint32) (Texel.r + 0.5f) << 16) |
                                 ((uint32) (Texel.g + 0.5f) << 8) |
                                 ((uint32) (Texel.b + 0.5f) << 0));
            }
        }
    }

    Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;

#if 0
    Result.Pitch = -Result.Width * BITMAP_BYTES_PER_PIXEL;
    Result.Memory = (uint8 *) Result.Memory - Result.Pitch * (Result.Height - 1);
#endif

    return Result;
}

struct load_bitmap_work {
    game_assets *Assets;
    bitmap_id ID;
    loaded_bitmap *Bitmap;
    task_with_memory *Task;

    asset_state FinalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork) {
    load_bitmap_work *Work = (load_bitmap_work *)Data;

    asset_bitmap_info *Info = Work->Assets->BitmapInfos + Work->ID.Value;

    *Work->Bitmap = DEBUGLoadBMP(Info->FileName, Info->AlignPercentage);

    CompletePreviousWritesBeforeFutureWrites;

    Work->Assets->Bitmaps[Work->ID.Value].Bitmap = Work->Bitmap;
    Work->Assets->Bitmaps[Work->ID.Value].State = Work->FinalState;

    EndTaskWithMemory(Work->Task);
}

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID) {
    if (ID.Value &&
        (AtomicCompareExchangeUInt32((uint32 *)&Assets->Bitmaps[ID.Value].State, AssetState_Unloaded, AssetState_Queued) ==
         AssetState_Unloaded))
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if (Task) {
            load_bitmap_work *Work = PushStruct(&Task->Arena, load_bitmap_work);

            Work->Assets = Assets;
            Work->ID = ID;
            Work->Task = Task;
            Work->Bitmap = PushStruct(&Assets->Arena, loaded_bitmap);
            Work->FinalState = AssetState_Loaded;

            PlatformAddEntry(Assets->TranState->LowPriorityQueue, LoadBitmapWork, Work);
        }
    }
}

internal void
LoadSound(game_assets *Assets, uint32 ID) {
}

internal bitmap_id
RandomAssetFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series) {
    bitmap_id Result = {};

    asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        uint32 Count = (Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
        uint32 Choice = RandomChoice(Series, Count);

        asset *Asset = Assets->Assets + Type->FirstAssetIndex + Choice;
        Result.Value = Asset->SlotID;
    }

    return Result;
}

internal bitmap_id
GetFirstBitmapID(game_assets *Assets, asset_type_id TypeID) {
    bitmap_id Result = {};

    asset_type *Type = Assets->AssetTypes + TypeID;
    if (Type->FirstAssetIndex != Type->OnePastLastAssetIndex) {
        asset *Asset = Assets->Assets + Type->FirstAssetIndex;
        Result.Value = Asset->SlotID;
    }

    return Result;
}

internal bitmap_id
DEBUGAddBitmapInfo(game_assets *Assets, char *FileName, v2 AlignPercentage) {
    Assert(Assets->DEBUGUsedBitmapCount < Assets->BitmapCount);

    bitmap_id ID = {Assets->DEBUGUsedBitmapCount++};

    asset_bitmap_info *Info = Assets->BitmapInfos + ID.Value;
    Info->AlignPercentage = AlignPercentage;
    Info->FileName = FileName;

    return ID;
}

internal void
BeginAssetType(game_assets *Assets, asset_type_id TypeID) {
    Assert(Assets->DEBUGAssetType == 0);
    Assets->DEBUGAssetType = Assets->AssetTypes + TypeID;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->DEBUGUsedAssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal void
AddBitmapAsset(game_assets *Assets, char *FileName, v2 AlignPercentage = V2(0.5f, 0.5f)) {
    Assert(Assets->DEBUGAssetType);

    asset *Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
    Asset->FirstTagIndex = 0;
    Asset->OnePastLastTagIndex = 0;
    Asset->SlotID = DEBUGAddBitmapInfo(Assets, FileName, AlignPercentage).Value;
}

internal void
EndAssetType(game_assets *Assets) {
    Assert(Assets->DEBUGAssetType);
    Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
}

internal game_assets *
AllocateGameAssets(memory_arena *Arena, memory_index Size, transient_state *TranState) {
    game_assets *Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->TranState = TranState;

    Assets->BitmapCount = 256 * Asset_Count;
    Assets->BitmapInfos = PushArray(Arena, Assets->BitmapCount, asset_bitmap_info);
    Assets->Bitmaps = PushArray(Arena, Assets->BitmapCount, asset_slot);

    Assets->SoundCount = 1;
    Assets->Sounds = PushArray(Arena, Assets->SoundCount, asset_slot);

    Assets->TagCount = 0;
    Assets->Tags = 0;

    Assets->AssetCount = Assets->SoundCount + Assets->BitmapCount;
    Assets->Assets = PushArray(Arena, Assets->BitmapCount, asset);

    Assets->DEBUGUsedBitmapCount = 1;
    Assets->DEBUGUsedAssetCount = 1;

    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/test_hero_shadow.bmp", V2(0.5f, 0.156682029f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test2/tree00.bmp", V2(0.5f, 0.156682029f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test2/rock03.bmp", V2(0.5f, 0.65625f));
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

    hero_bitmaps *Bitmap;

    Bitmap = Assets->HeroBitmaps;
    Bitmap->Head = DEBUGLoadBMP("test/test_hero_right_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_right_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_right_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));
    Bitmap++;

    Bitmap->Head = DEBUGLoadBMP("test/test_hero_back_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_back_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_back_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));
    Bitmap++;

    Bitmap->Head = DEBUGLoadBMP("test/test_hero_left_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_left_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_left_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));
    Bitmap++;

    Bitmap->Head = DEBUGLoadBMP("test/test_hero_front_head.bmp");
    Bitmap->Cape = DEBUGLoadBMP("test/test_hero_front_cape.bmp");
    Bitmap->Torso = DEBUGLoadBMP("test/test_hero_front_torso.bmp");
    SetTopDownAlign(Bitmap, V2(72, 182));

    return Assets;
}
