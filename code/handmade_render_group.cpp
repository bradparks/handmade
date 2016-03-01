inline v4
Unpack4x8(uint32 Packed) {
    v4 Result = { (real32)((Packed >> 16) & 0xFF),
                  (real32)((Packed >> 8) & 0xFF),
                  (real32)((Packed >> 0) & 0xFF),
                  (real32)((Packed >> 24) & 0xFF) };
    return Result;
}

inline v4
SRGB255ToLinear1(v4 C) {
    v4 Result;

    real32 Inv255 = 1.0f / 255.0f;

    Result.r = Square(Inv255 * C.r);
    Result.g = Square(Inv255 * C.g);
    Result.b = Square(Inv255 * C.b);
    Result.a = Inv255 * C.a;

    return Result;
}

inline v4
Linear1ToSRGB255(v4 C) {
    v4 Result;

    Result.r = SquareRoot(C.r) * 255.0f;
    Result.g = SquareRoot(C.g) * 255.0f;
    Result.b = SquareRoot(C.b) * 255.0f;
    Result.a = C.a * 255.0f;

    return Result;
}

inline v4
UnscaleAndBiasNormal(v4 Normal) {
    v4 Result;

    real32 Inv255 = 1.0f / 255.0f;

    Result.x = -1.0f + 2.0f * Inv255 * Normal.x;
    Result.y = -1.0f + 2.0f * Inv255 * Normal.y;
    Result.z = -1.0f + 2.0f * Inv255 * Normal.z;
    Result.w = Inv255 * Normal.w;

    return Result;
}

internal void
DrawRectangle(loaded_bitmap *Buffer, v2 vMin, v2 vMax, real32 R, real32 G, real32 B, real32 A = 1.0f) {
    int MinX = RoundReal32ToInt32(vMin.x);
    int MinY = RoundReal32ToInt32(vMin.y);
    int MaxX = RoundReal32ToInt32(vMax.x);
    int MaxY = RoundReal32ToInt32(vMax.y);

    if (MinX < 0) {
        MinX = 0;
    }

    if (MinY < 0) {
        MinY = 0;
    }

    if (MaxX > Buffer->Width) {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height) {
        MaxY = Buffer->Height;
    }

    // BIT PATTERN: 0x AA RR GG BB
    uint32 Color = (uint32)((RoundReal32ToUInt32(A * 255.0f) << 24) |
                            (RoundReal32ToUInt32(R * 255.0f) << 16) |
                            (RoundReal32ToUInt32(G * 255.0f) << 8) |
                            (RoundReal32ToUInt32(B * 255.0f) << 0));


    uint8 *Row = ((uint8 *) Buffer->Memory +
                    MinX * BITMAP_BYTES_PER_PIXEL +
                    MinY * Buffer->Pitch);

    for (int Y = MinY; Y < MaxY; ++Y) {
        uint32 *Pixel = (uint32 *) Row;
        for (int X = MinX; X < MaxX; ++X) {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

struct bilinear_sample {
    uint32 A, B, C, D;
};

inline bilinear_sample
BilinearSample(loaded_bitmap *Texture, int32 X, int32 Y) {
    bilinear_sample Result;

    uint8 *TexelPtr = (uint8 *)Texture->Memory + Y * Texture->Pitch + X * sizeof(uint32);
    Result.A = *(uint32 *)(TexelPtr);
    Result.B = *(uint32 *)(TexelPtr + sizeof(uint32));
    Result.C = *(uint32 *)(TexelPtr + Texture->Pitch);
    Result.D = *(uint32 *)(TexelPtr + Texture->Pitch + sizeof(uint32));

    return Result;
}

inline v4
SRGBBilinearBlend(bilinear_sample TexelSample, real32 fX, real32 fY) {
    v4 TexelA = Unpack4x8(TexelSample.A);
    v4 TexelB = Unpack4x8(TexelSample.B);
    v4 TexelC = Unpack4x8(TexelSample.C);
    v4 TexelD = Unpack4x8(TexelSample.D);

    // NOTE: Go from sRGB to "linear" brightness space
    TexelA = SRGB255ToLinear1(TexelA);
    TexelB = SRGB255ToLinear1(TexelB);
    TexelC = SRGB255ToLinear1(TexelC);
    TexelD = SRGB255ToLinear1(TexelD);

    v4 Result = Lerp(Lerp(TexelA, fX, TexelB), fY, Lerp(TexelC, fX, TexelD));

    return Result;
}

inline v3
SampleEnvironmentMap(v2 ScreenSpaceUV, v3 Normal, real32 Roughness, environment_map *Map) {
    uint32 LODIndex = (uint32)(Roughness * (real32)(ArrayCount(Map->LOD) - 1) + 0.5f);
    Assert(LODIndex < ArrayCount(Map->LOD));

    loaded_bitmap *LOD = Map->LOD[LODIndex];

    // TODO: Do intersection math to determine where we should be!
    real32 tX = 0.0f;
    real32 tY = 0.0f;

    int32 X = (int32)tX;
    int32 Y = (int32)tY;

    real32 fX = tX - (real32)X;
    real32 fY = tY - (real32)Y;

    Assert(X >= 0 && X < LOD->Width);
    Assert(Y >= 0 && Y < LOD->Height);

    bilinear_sample Sample = BilinearSample(LOD, X, Y);

    v3 Result = SRGBBilinearBlend(Sample, fX, fY).xyz;

    return Result;
}

internal void
DrawRectangleSlowly(loaded_bitmap *Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
                    loaded_bitmap *Texture, loaded_bitmap *NormalMap,
                    environment_map *Top,
                    environment_map *Middle,
                    environment_map *Bottom)
{
    // NOTE: Premultiply color up front
    Color.rgb *= Color.a;

    real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
    real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

    // BIT PATTERN: 0x AA RR GG BB
    uint32 Color32 = (uint32)((RoundReal32ToUInt32(Color.a * 255.0f) << 24) |
                              (RoundReal32ToUInt32(Color.r * 255.0f) << 16) |
                              (RoundReal32ToUInt32(Color.g * 255.0f) << 8) |
                              (RoundReal32ToUInt32(Color.b * 255.0f) << 0));

    int WidthMax = Buffer->Width - 1;
    int HeightMax = Buffer->Height - 1;

    real32 InvWidthMax = 1.0f / (real32)WidthMax;
    real32 InvHeightMax = 1.0f / (real32)HeightMax;

    int XMin = WidthMax;
    int XMax = 0;
    int YMin = HeightMax;
    int YMax = 0;

    v2 P[4] = { Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis };
    for (uint32 PIndex = 0; PIndex < ArrayCount(P); ++PIndex) {
        v2 TestP = P[PIndex];
        int FloorX = FloorReal32ToInt32(TestP.x);
        int CeilX = CeilReal32ToInt32(TestP.x);
        int FloorY = FloorReal32ToInt32(TestP.y);
        int CeilY = CeilReal32ToInt32(TestP.y);

        if (XMin > FloorX) { XMin = FloorX; }
        if (YMin > FloorY) { YMin = FloorY; }
        if (XMax < CeilX) { XMax = CeilX; }
        if (YMax < CeilY) { YMax = CeilY; }
    }

    if (XMin < 0) { XMin = 0; }
    if (YMin < 0) { YMin = 0; }
    if (XMax > WidthMax) { XMax = WidthMax; }
    if (YMax > HeightMax) { YMax = HeightMax; }

    uint8 *Row = ((uint8 *) Buffer->Memory +
                  XMin * BITMAP_BYTES_PER_PIXEL +
                  YMin * Buffer->Pitch);

    for (int Y = YMin; Y <= YMax; ++Y) {
        uint32 *Pixel = (uint32 *) Row;
        for (int X = XMin; X <= XMax; ++X) {
            v2 PixelP = V2i(X, Y);
            v2 d = PixelP - Origin;

            // TODO: PerpInner
            // TODO: Simpler origin
            real32 Edge0 = Inner(d, -Perp(XAxis));
            real32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
            real32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
            real32 Edge3 = Inner(d - YAxis, Perp(YAxis));

            if (Edge0 < 0 && Edge1 < 0 && Edge2 < 0 && Edge3 < 0) {
                v2 ScreenSpaceUV = { InvWidthMax * X, InvHeightMax * Y };

                real32 U = InvXAxisLengthSq * Inner(d, XAxis);
                real32 V = InvYAxisLengthSq * Inner(d, YAxis);
#if 0
                // TODO: SSE clamping.
                Assert(U >= 0.0f && U <= 1.0f);
                Assert(V >= 0.0f && V <= 1.0f);
#endif

                // TODO: Formalize texture boundaries!!!
                real32 tX = (U * (Texture->Width - 2));
                real32 tY = (V * (Texture->Height - 2));

                int32 iX = (int32)tX;
                int32 iY = (int32)tY;

                real32 fX = tX - (real32)iX;
                real32 fY = tY - (real32)iY;

                Assert(iX >= 0 && iX < Texture->Width);
                Assert(iY >= 0 && iY < Texture->Height);

                bilinear_sample TexelSample = BilinearSample(Texture, iX, iY);
                v4 Texel = SRGBBilinearBlend(TexelSample, fX, fY);

                if (NormalMap) {
                    bilinear_sample NormalSample = BilinearSample(NormalMap, iX, iY);

                    v4 NormalA = Unpack4x8(NormalSample.A);
                    v4 NormalB = Unpack4x8(NormalSample.B);
                    v4 NormalC = Unpack4x8(NormalSample.C);
                    v4 NormalD = Unpack4x8(NormalSample.D);

                    v4 Normal = Lerp(Lerp(NormalA, fX, NormalB),
                                     fY,
                                     Lerp(NormalC, fX, NormalD));

                    Normal = UnscaleAndBiasNormal(Normal);
                    // TODO: Do we really need to do this??
                    Normal.xyz = Normalize(Normal.xyz);

                    // TODO: ? Actually compute a bounce based on viewer direction

#if 1
                    environment_map *FarMap = 0;
                    real32 tEnvMap = Normal.y;
                    real32 tFarMap = 0.0f;
                    if (tEnvMap < -0.5f) {
                        FarMap = Bottom;
                        tFarMap = 2.0f * (tEnvMap + 1.0f);
                    } else if (tEnvMap > 0.5f) {
                        FarMap = Top;
                        tFarMap = 2.0f * (tEnvMap - 0.5f);
                    }

                    v3 LightColor = { 0, 0, 0}; //SampleEnvironmentMap(ScreenSpaceUV, Normal.xyz, Normal.w, Middle);
                    if (FarMap) {
                        v3 FarMapColor = SampleEnvironmentMap(ScreenSpaceUV, Normal.xyz, Normal.w, FarMap);
                        LightColor = Lerp(LightColor, tFarMap, FarMapColor);
                    }

                    // TODO: ? Actually do a lighting model computation here
                    Texel.rgb = Texel.rgb + Texel.a * LightColor;
#else
                    Texel.rgb = V3(0.5f, 0.5f, 0.5f) + 0.5f * Normal.rgb;
                    Texel.a = 1.0f;
#endif
                }

                Texel = Hadamard(Texel, Color);
                Texel.r = Clamp01(Texel.r);
                Texel.g = Clamp01(Texel.g);
                Texel.b = Clamp01(Texel.b);

                v4 Dest = { (real32)((*Pixel >> 16) & 0xFF),
                            (real32)((*Pixel >> 8) & 0xFF),
                            (real32)((*Pixel >> 0) & 0xFF),
                            (real32)((*Pixel >> 24) & 0xFF) };

                // NOTE: Go from sRGB to "linear" brightness space
                Dest = SRGB255ToLinear1(Dest);

                // TODO: Check this for math errors
                v4 Blended = (1.0f - Texel.a) * Dest + Texel;

                // NOTE: Go from "linear" brightness space to sRGB
                v4 Blended255 = Linear1ToSRGB255(Blended);

                *Pixel = (((uint32)(Blended255.a + 0.5f) << 24) |
                          ((uint32)(Blended255.r + 0.5f) << 16) |
                          ((uint32)(Blended255.g + 0.5f) << 8) |
                          ((uint32)(Blended255.b + 0.5f) << 0));
            }

            ++Pixel;
        }
        Row += Buffer->Pitch;
    }
}

internal void
DrawBitmap(loaded_bitmap *Buffer, loaded_bitmap *Bitmap,
           real32 RealX, real32 RealY, real32 CAlpha = 1.0)
{
    int MinX = RoundReal32ToInt32(RealX);
    int MinY = RoundReal32ToInt32(RealY);
    int MaxX = MinX + Bitmap->Width;
    int MaxY = MinY + Bitmap->Height;

    int32 SourceOffsetX = 0;
    if (MinX < 0) {
        SourceOffsetX = -MinX;
        MinX = 0;
    }

    int32 SourceOffsetY = 0;
    if (MinY < 0) {
        SourceOffsetY = -MinY;
        MinY = 0;
    }

    if (MaxX > Buffer->Width) {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height) {
        MaxY = Buffer->Height;
    }

    uint8 *SourceRow = (uint8 *)Bitmap->Memory + SourceOffsetY * Bitmap->Pitch + BITMAP_BYTES_PER_PIXEL * SourceOffsetX;
    uint8 *DestRow = ((uint8 *) Buffer->Memory +
                      MinX * BITMAP_BYTES_PER_PIXEL +
                      MinY * Buffer->Pitch);
    for (int32 Y = MinY; Y < MaxY; ++Y) {
        uint32 *Dest = (uint32 *) DestRow;
        uint32 *Source = (uint32 *) SourceRow;
        for (int32 X = MinX; X < MaxX; ++X) {
            v4 Texel = { (real32)((*Source >> 16) & 0xFF),
                         (real32)((*Source >> 8) & 0xFF),
                         (real32)((*Source >> 0) & 0xFF),
                         (real32)((*Source >> 24) & 0xFF) };
            Texel = SRGB255ToLinear1(Texel);
            Texel *= CAlpha;

            v4 D = { (real32)((*Dest >> 16) & 0xFF),
                     (real32)((*Dest >> 8) & 0xFF),
                     (real32)((*Dest >> 0) & 0xFF),
                     (real32)((*Dest >> 24) & 0xFF) };

            D = SRGB255ToLinear1(D);

            v4 Result = (1.0f - Texel.a) * D + Texel;

            Result = Linear1ToSRGB255(Result);

            *Dest = (((uint32)(Result.a + 0.5f) << 24) |
                     ((uint32)(Result.r + 0.5f) << 16) |
                     ((uint32)(Result.g + 0.5f) << 8) |
                     ((uint32)(Result.b + 0.5f) << 0));

            ++Dest;
            ++Source;
        }
        DestRow += Buffer->Pitch;
        SourceRow += Bitmap->Pitch;
    }
}

inline void
DrawRectangleOutline(loaded_bitmap *Buffer, v2 vMin, v2 vMax, v3 Color, real32 R = 2.0f) {
    // NOTE: Top and bottom
    DrawRectangle(Buffer, V2(vMin.x - R, vMin.y - R), V2(vMax.x + R, vMin.y + R), Color.r, Color.g, Color.b);
    DrawRectangle(Buffer, V2(vMin.x - R, vMax.y - R), V2(vMax.x + R, vMax.y + R), Color.r, Color.g, Color.b);

    // NOTE: Left and right
    DrawRectangle(Buffer, V2(vMin.x - R, vMin.y - R), V2(vMin.x + R, vMax.y + R), Color.r, Color.g, Color.b);
    DrawRectangle(Buffer, V2(vMax.x - R, vMin.y - R), V2(vMax.x + R, vMax.y + R), Color.r, Color.g, Color.b);
}

inline v2 GetRenderEntityBasisP(render_group *RenderGroup,
                                render_entity_basis *EntityBasis,
                                v2 ScreenCenter)
{
    v3 EntityBaseP = EntityBasis->Basis->P;
    real32 ZFudge = 1.0f + 0.1f * (EntityBaseP.z + EntityBasis->OffsetZ);

    real32 EntityGroundPointX = ScreenCenter.x + RenderGroup->MetersToPixels * ZFudge * EntityBaseP.x;
    real32 EntityGroundPointY = ScreenCenter.y - RenderGroup->MetersToPixels * ZFudge * EntityBaseP.y;
    real32 EntityZ = -RenderGroup->MetersToPixels * EntityBaseP.z;

    v2 Center = {EntityGroundPointX + EntityBasis->Offset.x,
                 EntityGroundPointY + EntityBasis->Offset.y + EntityBasis->EntityZC * EntityZ};
    return Center;
}

internal void
RenderGroupToOutput(render_group *RenderGroup, loaded_bitmap *OutputTarget) {
    v2 ScreenCenter = V2(0.5f * (real32) OutputTarget->Width,
                         0.5f * (real32) OutputTarget->Height);
    for (uint32 BaseAddress = 0;
         BaseAddress < RenderGroup->PushBufferSize;
         )
    {
        render_group_entry_header *Header = (render_group_entry_header *)
            (RenderGroup->PushBufferBase + BaseAddress);
        BaseAddress += sizeof(*Header);

        void *Data = (Header + 1);
        switch (Header->Type) {
            case RenderGroupEntryType_render_entry_clear: {
                render_entry_clear *Entry = (render_entry_clear *)Data;

                DrawRectangle(OutputTarget, V2(0.0f, 0.0f),
                              V2((real32) OutputTarget->Width,
                                 (real32) OutputTarget->Height),
                              Entry->Color.r,
                              Entry->Color.g,
                              Entry->Color.b,
                              Entry->Color.a);

                BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_entry_bitmap: {
                render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
#if 0

                v2 P = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis,
                                             ScreenCenter);

                Assert(Entry->Bitmap);
                DrawBitmap(OutputTarget, Entry->Bitmap, P.x, P.y, Entry->A);
#endif
                BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_entry_rectangle: {
                render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
                v2 P = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis,
                                             ScreenCenter);
                DrawRectangle(OutputTarget, P, P + Entry->Dim, Entry->R, Entry->G, Entry->B);

                BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_entry_coordinate_system: {
                render_entry_coordinate_system *Entry = (render_entry_coordinate_system *)Data;

                DrawRectangleSlowly(OutputTarget,
                                    Entry->Origin,
                                    Entry->XAxis,
                                    Entry->YAxis,
                                    Entry->Color,
                                    Entry->Texture,
                                    Entry->NormalMap,
                                    Entry->Top,
                                    Entry->Middle,
                                    Entry->Bottom);

                v4 Color = {1, 1, 0, 1};
                v2 Dim = {2, 2};
                v2 P = Entry->Origin;
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color.r, Color.g, Color.b);

                P = Entry->Origin + Entry->XAxis;
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color.r, Color.g, Color.b);

                P = Entry->Origin + Entry->YAxis;
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color.r, Color.g, Color.b);

                P = Entry->Origin + Entry->XAxis + Entry->YAxis;
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color.r, Color.g, Color.b);

#if 0
                for (uint32 PIndex = 0;
                     PIndex < ArrayCount(Entry->Points);
                     ++PIndex)
                {
                    P = Entry->Points[PIndex];
                    P = Entry->Origin + P.x * Entry->XAxis + P.y * Entry->YAxis;
                    DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);
                }
#endif

                BaseAddress += sizeof(*Entry);
            } break;

            InvalidDefaultCase;
        }

    }
}

internal render_group *
AllocateRenderGroup(memory_arena *Arena, uint32 MaxPushBufferSize, real32 MetersToPixels) {
    render_group *Result = PushStruct(Arena, render_group);
    Result->PushBufferBase = (uint8 *) PushSize(Arena, MaxPushBufferSize);

    render_basis DefaultBasis = {0, 0, 0};
    Result->DefaultBasis = PushStruct(Arena, render_basis);
    Result->DefaultBasis->P = V3(0, 0, 0);
    Result->MetersToPixels = MetersToPixels;

    Result->MaxPushBufferSize = MaxPushBufferSize;
    Result->PushBufferSize = 0;

    return Result;
}

#define PushRenderElement(Group, type) (type *)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type)

inline void *
PushRenderElement_(render_group *Group, uint32 Size, render_group_entry_type Type) {
    void *Result = 0;

    Size += sizeof(render_group_entry_header);

    if (Group->PushBufferSize + Size < Group->MaxPushBufferSize) {
        render_group_entry_header *Header = (render_group_entry_header *)(Group->PushBufferBase + Group->PushBufferSize);
        Header->Type = Type;
        Result = (Header + 1);
        Group->PushBufferSize += Size;
    } else {
        InvalidCodePath;
    }

    return Result;
}

inline void
PushPiece(render_group *Group, loaded_bitmap *Bitmap,
          v2 Offset, real32 OffsetZ, v2 Align, v2 Dim, v4 Color, real32 EntityZC)
{
    render_entry_bitmap *Piece = PushRenderElement(Group, render_entry_bitmap);
    if (Piece) {
        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->EntityBasis.Offset = Group->MetersToPixels * V2(Offset.x, -Offset.y) - Align;
        Piece->EntityBasis.OffsetZ = OffsetZ;
        Piece->EntityBasis.EntityZC = EntityZC;
        Piece->Bitmap = Bitmap;
        Piece->R = Color.r;
        Piece->G = Color.g;
        Piece->B = Color.b;
        Piece->A = Color.a;
    }
}

inline void
PushBitmap(render_group *Group, loaded_bitmap *Bitmap,
           v2 Offset, real32 OffsetZ, v2 Align,
           real32 Alpha = 1.0f, real32 EntityZC = 1.0f)
{
    PushPiece(Group, Bitmap, Offset, OffsetZ, Align, V2(0, 0), V4(1.0f, 1.0f, 1.0f, Alpha), EntityZC);
}

inline void
PushRect(render_group *Group, v2 Offset, real32 OffsetZ,
          v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
    render_entry_rectangle *Piece = PushRenderElement(Group, render_entry_rectangle);
    if (Piece) {
        v2 HalfDim = 0.5f * Group->MetersToPixels * Dim;

        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->EntityBasis.Offset = Group->MetersToPixels * V2(Offset.x, -Offset.y) - HalfDim;
        Piece->EntityBasis.OffsetZ = OffsetZ;
        Piece->EntityBasis.EntityZC = EntityZC;
        Piece->R = Color.r;
        Piece->G = Color.g;
        Piece->B = Color.b;
        Piece->A = Color.a;
        Piece->Dim = Group->MetersToPixels * Dim;
    }
}

inline void
PushRectOutline(render_group *Group, v2 Offset, real32 OffsetZ,
                v2 Dim, v4 Color, real32 EntityZC = 1.0f)
{
    real32 Thickness = 0.1f;

    // NOTE: Top and bottom
    PushRect(Group, Offset - V2(0, 0.5f * Dim.y), OffsetZ, V2(Dim.x, Thickness), Color, EntityZC);
    PushRect(Group, Offset + V2(0, 0.5f * Dim.y), OffsetZ, V2(Dim.x, Thickness), Color, EntityZC);

    // NOTE: Left and right
    PushRect(Group, Offset - V2(0.5f * Dim.x, 0), OffsetZ, V2(Thickness, Dim.y), Color, EntityZC);
    PushRect(Group, Offset + V2(0.5f * Dim.x, 0), OffsetZ, V2(Thickness, Dim.y), Color, EntityZC);
}

inline void
Clear(render_group *Group, v4 Color) {
    render_entry_clear *Entry = PushRenderElement(Group, render_entry_clear);
    if (Entry) {
        Entry->Color = Color;
    }
}

inline render_entry_coordinate_system *
CoordinateSystem(render_group *Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
                 loaded_bitmap *Texture, loaded_bitmap *NormalMap,
                 environment_map *Top, environment_map *Middle, environment_map *Bottom)
{
    render_entry_coordinate_system *Entry = PushRenderElement(Group, render_entry_coordinate_system);
    if (Entry) {
        Entry->Origin = Origin;
        Entry->XAxis = XAxis;
        Entry->YAxis = YAxis;
        Entry->Color = Color;
        Entry->Texture = Texture;
        Entry->NormalMap = NormalMap;
        Entry->Top = Top;
        Entry->Middle = Middle;
        Entry->Bottom = Bottom;
    }

    return Entry;
}
