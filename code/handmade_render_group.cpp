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
DrawRectangle(loaded_bitmap *Buffer, v2 vMin, v2 vMax, v4 Color) {
    real32 R = Color.r;
    real32 G = Color.g;
    real32 B = Color.b;
    real32 A = Color.a;

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
    uint32 Color32 = (uint32)((RoundReal32ToUInt32(A * 255.0f) << 24) |
                              (RoundReal32ToUInt32(R * 255.0f) << 16) |
                              (RoundReal32ToUInt32(G * 255.0f) << 8) |
                              (RoundReal32ToUInt32(B * 255.0f) << 0));


    uint8 *Row = ((uint8 *) Buffer->Memory +
                    MinX * BITMAP_BYTES_PER_PIXEL +
                    MinY * Buffer->Pitch);

    for (int Y = MinY; Y < MaxY; ++Y) {
        uint32 *Pixel = (uint32 *) Row;
        for (int X = MinX; X < MaxX; ++X) {
            *Pixel++ = Color32;
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
SampleEnvironmentMap(v2 ScreenSpaceUV, v3 SampleDirection, real32 Roughness, environment_map *Map,
                     real32 DistanceFromMapInZ)
{
    /* NOTE:
     *
     * ScreenSpaceUV tells us where the ray is being cast _from_ in
     * normalized screen coordinates.
     *
     * SampleDirection tells us what direction the cast is going -
     * it is does not have to be normalized.
     *
     * Roughness says which LODs of Map we sample from.
     *
     * DistanceFromMapInZ says how far the map is from the sample point in Z, given
     * in meters.
     */

    // NOTE: Pick which LOD to sample from
    uint32 LODIndex = (uint32)(Roughness * (real32)(ArrayCount(Map->LOD) - 1) + 0.5f);
    Assert(LODIndex < ArrayCount(Map->LOD));

    loaded_bitmap *LOD = Map->LOD + LODIndex;

    // NOTE: Compute the distance to the map and the scaling
    // factor for meters-to-UVs
    real32 UVsPerMeters = 0.1f; // TODO: Parameterize this, and should be different for X and Y
    real32 C = UVsPerMeters * DistanceFromMapInZ / SampleDirection.y;
    v2 Offset = C * V2(SampleDirection.x, SampleDirection.z);

    // NOTE: Find the intersection point
    v2 UV = ScreenSpaceUV + Offset;

    // NOTE: Clamp to the valid range
    UV = Clamp01(UV);

    // NOTE: Bilinear sample
    // TODO: Formalize texture boundaries!!!
    real32 tX = (UV.x * (LOD->Width - 2));
    real32 tY = (UV.y * (LOD->Height - 2));

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
    BEGIN_TIMED_BLOCK(DrawRectangleSlowly);

    // TODO: Remove this
    real32 PixelsToMeters = 42.0f;

    // NOTE: Premultiply color up front
    Color.rgb *= Color.a;

    real32 XAxisLength = Length(XAxis);
    real32 YAxisLength = Length(YAxis);

    v2 NXAxis = (YAxisLength / XAxisLength) * XAxis;
    v2 NYAxis = (XAxisLength / YAxisLength) * YAxis;

    // NOTE: NzScale could be a parameter if we want people to
    // have control over the amount of scaling in the Z direction
    // that the normals appear to have.
    real32 NzScale = 0.5f * (XAxisLength + YAxisLength);

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

    // TODO: This will need to be specified separately!!!
    real32 OriginZ = 0.0f;
    real32 OriginY = (Origin + 0.5f * XAxis + 0.5f * YAxis).y;
    real32 FixedCastY = InvHeightMax * OriginY;

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
            BEGIN_TIMED_BLOCK(TestPixel);

            v2 PixelP = V2i(X, Y);
            v2 d = PixelP - Origin;

            // TODO: PerpInner
            // TODO: Simpler origin
            real32 Edge0 = Inner(d, -Perp(XAxis));
            real32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
            real32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
            real32 Edge3 = Inner(d - YAxis, Perp(YAxis));

            if (Edge0 < 0 && Edge1 < 0 && Edge2 < 0 && Edge3 < 0) {
                BEGIN_TIMED_BLOCK(FillPixel);
#if 1
                v2 ScreenSpaceUV = { InvWidthMax * X, FixedCastY };
                real32 ZDiff = PixelsToMeters * ((real32)Y - OriginY);
#else
                v2 ScreenSpaceUV = { InvWidthMax * X, InvHeightMax * (real32)Y };
                real32 ZDiff = 0.0f;
#endif

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

#if 0
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
                    Normal.xy = Normal.x * NXAxis + Normal.y * NYAxis;
                    Normal.z *= NzScale;
                    Normal.xyz = Normalize(Normal.xyz);

                    // NOTE: The eye vector is always assumed to be { 0, 0, 1 }
                    // This is just the simplify version of the reflection -e + 2e^T N N
                    v3 BounceDirection = 2.0f * Normal.z * Normal.xyz;
                    BounceDirection.z -= 1.0f;

                    // TODO: Eventually we need to support two mappings.
                    // one for top-down view (which we don't do now) and one
                    // for sideways, which is what's happening here.
                    BounceDirection.z = -BounceDirection.z;

                    environment_map *FarMap = 0;
                    real32 Pz = OriginZ + ZDiff;
                    real32 MapZ = 2.0f;
                    real32 tEnvMap = BounceDirection.y;
                    real32 tFarMap = 0.0f;
                    if (tEnvMap < -0.5f) {
                        // TODO: This path seems PARTICULARLY broken!
                        FarMap = Bottom;
                        tFarMap = -1.0f - 2.0f * tEnvMap;
                    } else if (tEnvMap > 0.5f) {
                        FarMap = Top;
                        tFarMap = 2.0f * (tEnvMap - 0.5f);
                    }

                    tFarMap *= tFarMap;
                    tFarMap *= tFarMap;

                    v3 LightColor = { 0, 0, 0 }; // TODO: How do we sample from middle map???
                    if (FarMap) {
                        real32 DistanceFromMapInZ = FarMap->Pz - Pz;
                        v3 FarMapColor = SampleEnvironmentMap(ScreenSpaceUV, BounceDirection, Normal.w, FarMap,
                                                              DistanceFromMapInZ);
                        LightColor = Lerp(LightColor, tFarMap, FarMapColor);
                    }

                    // TODO: ? Actually do a lighting model computation here
                    Texel.rgb = Texel.rgb + Texel.a * LightColor;

#if 0
                    // NOTE: Draws the bounce direction
                    Texel.rgb = V3(0.5f, 0.5f, 0.5f) + 0.5f * BounceDirection;
                    Texel.rgb *= Texel.a;
#endif
                }
#endif

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

                END_TIMED_BLOCK(FillPixel);
            }

            ++Pixel;

            END_TIMED_BLOCK(TestPixel);
        }
        Row += Buffer->Pitch;
    }

    END_TIMED_BLOCK(DrawRectangleSlowly);
}

internal void
DrawRectangleHopefulQuickly(loaded_bitmap *Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
                            loaded_bitmap *Texture)
{
    BEGIN_TIMED_BLOCK(DrawRectangleHopefulQuickly);

    // TODO: Remove this
    real32 PixelsToMeters = 42.0f;

    // NOTE: Premultiply color up front
    Color.rgb *= Color.a;

    real32 XAxisLength = Length(XAxis);
    real32 YAxisLength = Length(YAxis);

    v2 NXAxis = (YAxisLength / XAxisLength) * XAxis;
    v2 NYAxis = (XAxisLength / YAxisLength) * YAxis;

    // NOTE: NzScale could be a parameter if we want people to
    // have control over the amount of scaling in the Z direction
    // that the normals appear to have.
    real32 NzScale = 0.5f * (XAxisLength + YAxisLength);

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

    // TODO: This will need to be specified separately!!!
    real32 OriginZ = 0.0f;
    real32 OriginY = (Origin + 0.5f * XAxis + 0.5f * YAxis).y;
    real32 FixedCastY = InvHeightMax * OriginY;

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

    v2 nXAxis = InvXAxisLengthSq * XAxis;
    v2 nYAxis = InvYAxisLengthSq * YAxis;

    real32 Inv255 = 1.0f / 255.0f;

    uint8 *Row = ((uint8 *) Buffer->Memory +
                  XMin * BITMAP_BYTES_PER_PIXEL +
                  YMin * Buffer->Pitch);

    for (int Y = YMin; Y <= YMax; ++Y) {
        uint32 *Pixel = (uint32 *) Row;
        for (int X = XMin; X <= XMax; ++X) {
            BEGIN_TIMED_BLOCK(TestPixel);

            v2 PixelP = V2i(X, Y);
            v2 d = PixelP - Origin;

            real32 U = Inner(d, nXAxis);
            real32 V = Inner(d, nYAxis);

            if (U >= 0.0 && U <= 1.0f && V >= 0.0f && V <= 1.0f) {
                BEGIN_TIMED_BLOCK(FillPixel);

                // TODO: Formalize texture boundaries!!!
                real32 tX = (U * (Texture->Width - 2));
                real32 tY = (V * (Texture->Height - 2));

                int32 iX = (int32)tX;
                int32 iY = (int32)tY;

                real32 fX = tX - (real32)iX;
                real32 fY = tY - (real32)iY;

                Assert(iX >= 0 && iX < Texture->Width);
                Assert(iY >= 0 && iY < Texture->Height);

                uint8 *TexelPtr = (uint8 *)Texture->Memory + iY * Texture->Pitch + iX * sizeof(uint32);
                uint32 SampleA = *(uint32 *)(TexelPtr);
                uint32 SampleB = *(uint32 *)(TexelPtr + sizeof(uint32));
                uint32 SampleC = *(uint32 *)(TexelPtr + Texture->Pitch);
                uint32 SampleD = *(uint32 *)(TexelPtr + Texture->Pitch + sizeof(uint32));

                real32 TexelAr = (real32)((SampleA >> 16) & 0xFF);
                real32 TexelAg = (real32)((SampleA >> 8) & 0xFF);
                real32 TexelAb = (real32)((SampleA >> 0) & 0xFF);
                real32 TexelAa = (real32)((SampleA >> 24) & 0xFF);

                real32 TexelBr = (real32)((SampleB >> 16) & 0xFF);
                real32 TexelBg = (real32)((SampleB >> 8) & 0xFF);
                real32 TexelBb = (real32)((SampleB >> 0) & 0xFF);
                real32 TexelBa = (real32)((SampleB >> 24) & 0xFF);

                real32 TexelCr = (real32)((SampleC >> 16) & 0xFF);
                real32 TexelCg = (real32)((SampleC >> 8) & 0xFF);
                real32 TexelCb = (real32)((SampleC >> 0) & 0xFF);
                real32 TexelCa = (real32)((SampleC >> 24) & 0xFF);

                real32 TexelDr = (real32)((SampleD >> 16) & 0xFF);
                real32 TexelDg = (real32)((SampleD >> 8) & 0xFF);
                real32 TexelDb = (real32)((SampleD >> 0) & 0xFF);
                real32 TexelDa = (real32)((SampleD >> 24) & 0xFF);

                // NOTE: Convert texture from sRGB to "linear" brightness space
                TexelAr = Square(Inv255 * TexelAr);
                TexelAg = Square(Inv255 * TexelAg);
                TexelAb = Square(Inv255 * TexelAb);
                TexelAa = Inv255 * TexelAa;

                TexelBr = Square(Inv255 * TexelBr);
                TexelBg = Square(Inv255 * TexelBg);
                TexelBb = Square(Inv255 * TexelBb);
                TexelBa = Inv255 * TexelBa;

                TexelCr = Square(Inv255 * TexelCr);
                TexelCg = Square(Inv255 * TexelCg);
                TexelCb = Square(Inv255 * TexelCb);
                TexelCa = Inv255 * TexelCa;

                TexelDr = Square(Inv255 * TexelDr);
                TexelDg = Square(Inv255 * TexelDg);
                TexelDb = Square(Inv255 * TexelDb);
                TexelDa = Inv255 * TexelDa;

                // NOTE: Bilinear texture blend
                real32 ifX = 1.0f - fX;
                real32 ifY = 1.0f - fY;

                real32 l0 = ifY * ifX;
                real32 l1 = ifY * fX;
                real32 l2 = fY * ifX;
                real32 l3 = fY * fX;

                real32 Texelr = l0 * TexelAr + l1 * TexelBr + l2 * TexelCr + l3 * TexelDr;
                real32 Texelg = l0 * TexelAg + l1 * TexelBg + l2 * TexelCg + l3 * TexelDg;
                real32 Texelb = l0 * TexelAb + l1 * TexelBb + l2 * TexelCb + l3 * TexelDb;
                real32 Texela = l0 * TexelAa + l1 * TexelBa + l2 * TexelCa + l3 * TexelDa;

                // NOTE: Modulate by incoming color
                Texelr = Texelr * Color.r;
                Texelg = Texelg * Color.g;
                Texelb = Texelb * Color.b;
                Texela = Texela * Color.a;

                // NOTE: Clamp colors to valid range
                Texelr = Clamp01(Texelr);
                Texelg = Clamp01(Texelg);
                Texelb = Clamp01(Texelb);

                // NOTE: Load destination
                real32 Destr = (real32)((*Pixel >> 16) & 0xFF);
                real32 Destg = (real32)((*Pixel >> 8) & 0xFF);
                real32 Destb = (real32)((*Pixel >> 0) & 0xFF);
                real32 Desta = (real32)((*Pixel >> 24) & 0xFF);

                // NOTE: Go from sRGB to "linear" brightness space
                Destr = Square(Inv255 * Destr);
                Destg = Square(Inv255 * Destg);
                Destb = Square(Inv255 * Destb);
                Desta = Inv255 * Desta;

                // NOTE: Desination blend
                real32 InvTexelA = (1.0f - Texela);
                real32 Blendedr = InvTexelA * Destr + Texelr;
                real32 Blendedg = InvTexelA * Destg + Texelg;
                real32 Blendedb = InvTexelA * Destb + Texelb;
                real32 Blendeda = InvTexelA * Desta + Texela;

                // NOTE: Go from "linear" brightness space to sRGB
                Blendedr = SquareRoot(Blendedr) * 255.0f;
                Blendedg = SquareRoot(Blendedg) * 255.0f;
                Blendedb = SquareRoot(Blendedb) * 255.0f;
                Blendeda = Blendeda * 255.0f;

                // NOTE: Repack
                *Pixel = (((uint32)(Blendeda + 0.5f) << 24) |
                          ((uint32)(Blendedr + 0.5f) << 16) |
                          ((uint32)(Blendedg + 0.5f) << 8) |
                          ((uint32)(Blendedb + 0.5f) << 0));

                END_TIMED_BLOCK(FillPixel);
            }

            ++Pixel;

            END_TIMED_BLOCK(TestPixel);
        }
        Row += Buffer->Pitch;
    }

    END_TIMED_BLOCK(DrawRectangleHopefulQuickly);
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

internal void
ChangeSaturation(loaded_bitmap *Buffer, real32 Level) {
    uint8 *DestRow = (uint8 *) Buffer->Memory;
    for (int32 Y = 0; Y < Buffer->Height; ++Y) {
        uint32 *Dest = (uint32 *) DestRow;
        for (int32 X = 0; X < Buffer->Width; ++X) {
            v4 D = { (real32)((*Dest >> 16) & 0xFF),
                     (real32)((*Dest >> 8) & 0xFF),
                     (real32)((*Dest >> 0) & 0xFF),
                     (real32)((*Dest >> 24) & 0xFF) };

            D = SRGB255ToLinear1(D);

            real32 Avg = (D.r + D.g + D.b) * (1.0f / 3.0f);
            v3 Delta = V3(D.r - Avg, D.g - Avg, D.b - Avg);

            v4 Result = V4(V3(Avg, Avg, Avg) + Level * Delta, D.a);

            Result = Linear1ToSRGB255(Result);

            *Dest = (((uint32)(Result.a + 0.5f) << 24) |
                     ((uint32)(Result.r + 0.5f) << 16) |
                     ((uint32)(Result.g + 0.5f) << 8) |
                     ((uint32)(Result.b + 0.5f) << 0));

            ++Dest;
        }
        DestRow += Buffer->Pitch;
    }
}

inline void
DrawRectangleOutline(loaded_bitmap *Buffer, v2 vMin, v2 vMax, v3 Color, real32 R = 2.0f) {
    // NOTE: Top and bottom
    DrawRectangle(Buffer, V2(vMin.x - R, vMin.y - R), V2(vMax.x + R, vMin.y + R), V4(Color, 1.0));
    DrawRectangle(Buffer, V2(vMin.x - R, vMax.y - R), V2(vMax.x + R, vMax.y + R), V4(Color, 1.0));

    // NOTE: Left and right
    DrawRectangle(Buffer, V2(vMin.x - R, vMin.y - R), V2(vMin.x + R, vMax.y + R), V4(Color, 1.0));
    DrawRectangle(Buffer, V2(vMax.x - R, vMin.y - R), V2(vMax.x + R, vMax.y + R), V4(Color, 1.0));
}

struct entity_basis_p_result {
    v2 P;
    real32 Scale;
    bool32 Valid;
};

inline entity_basis_p_result
GetRenderEntityBasisP(render_group *RenderGroup, render_entity_basis *EntityBasis,
                      v2 ScreenDim)
{
    v2 ScreenCenter = 0.5f * ScreenDim;

    entity_basis_p_result Result = {};

    v3 EntityBaseP = EntityBasis->Basis->P;
    real32 DistanceToPZ = RenderGroup->RenderCamera.DistanceAboveTarget - EntityBaseP.z;
    real32 NearClipPlane = 0.2f;

    v3 RayXY = V3(EntityBaseP.xy + EntityBasis->Offset.xy, 1.0f);

    if (DistanceToPZ > NearClipPlane) {
        v3 ProjectedXY = (1.0f / DistanceToPZ) * RenderGroup->RenderCamera.FocalLength * RayXY;
        Result.P = ScreenCenter + RenderGroup->MetersToPixels * ProjectedXY.xy;
        Result.Scale = RenderGroup->MetersToPixels * ProjectedXY.z;
        Result.Valid = true;
    }

    return Result;
}

internal void
RenderGroupToOutput(render_group *RenderGroup, loaded_bitmap *OutputTarget) {
    BEGIN_TIMED_BLOCK(RenderGroupToOutput);

    v2 ScreenDim = V2((real32)OutputTarget->Width,
                      (real32)OutputTarget->Height);

    real32 PixelsToMeters = 1.0f / RenderGroup->MetersToPixels;

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
                              Entry->Color);

                BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_entry_bitmap: {
                render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
                entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup,
                                                                    &Entry->EntityBasis,
                                                                    ScreenDim);
                Assert(Entry->Bitmap);

#if 0
                DrawBitmap(OutputTarget, Entry->Bitmap, P.x, P.y, Entry->Color.a);
#else
                DrawRectangleHopefulQuickly(OutputTarget, Basis.P,
                                            Basis.Scale * V2(Entry->Size.x, 0),
                                            Basis.Scale * V2(0, Entry->Size.y),
                                            Entry->Color,
                                            Entry->Bitmap);
#endif

                BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_entry_rectangle: {
                render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
                entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup,
                                                                    &Entry->EntityBasis,
                                                                    ScreenDim);
                DrawRectangle(OutputTarget, Basis.P, Basis.P + Basis.Scale * Entry->Dim, Entry->Color);

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
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

                P = Entry->Origin + Entry->XAxis;
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

                P = Entry->Origin + Entry->YAxis;
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

                P = Entry->Origin + Entry->XAxis + Entry->YAxis;
                DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

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

    END_TIMED_BLOCK(RenderGroupToOutput);
}

internal render_group *
AllocateRenderGroup(memory_arena *Arena, uint32 MaxPushBufferSize,
                    uint32 ResolutionPixelsX, uint32 ResolutionPixelsY) {
    render_group *Result = PushStruct(Arena, render_group);
    Result->PushBufferBase = (uint8 *) PushSize(Arena, MaxPushBufferSize);

    render_basis DefaultBasis = {0, 0, 0};
    Result->DefaultBasis = PushStruct(Arena, render_basis);
    Result->DefaultBasis->P = V3(0, 0, 0);

    Result->MaxPushBufferSize = MaxPushBufferSize;
    Result->PushBufferSize = 0;

    real32 WidthOfMonitor = 0.635; // NOTE: Horizontal measurement of monitor in meters
    Result->GameCamera.FocalLength = 0.6f; // NOTE: Meters the person is sitting from their monitor
    Result->GameCamera.DistanceAboveTarget = 9.0f;
    Result->RenderCamera = Result->GameCamera;
    //Result->RenderCamera.DistanceAboveTarget = 50.0f;

    Result->GlobalAlpha = 1.0f;

    Result->MetersToPixels = (real32)ResolutionPixelsX * WidthOfMonitor;

    real32 PixelsToMeters = 1.0f / Result->MetersToPixels;
    Result->MonitorHalfDimInMeters = { 0.5f * ResolutionPixelsX * PixelsToMeters,
                                       0.5f * ResolutionPixelsY * PixelsToMeters };


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
PushBitmap(render_group *Group, loaded_bitmap *Bitmap, real32 Height, v3 Offset, v4 Color = V4(1, 1, 1, 1)) {
    render_entry_bitmap *Piece = PushRenderElement(Group, render_entry_bitmap);
    if (Piece) {
        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->Bitmap = Bitmap;
        v2 Size = V2(Height * Bitmap->WidthOverHeight, Height);
        v2 Align = Hadamard(Bitmap->AlignPercentage, Size);
        Piece->EntityBasis.Offset = Offset - V3(Align, 0);
        Piece->Color = Group->GlobalAlpha * Color;
        Piece->Size = Size;
    }
}

inline void
PushRect(render_group *Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1)) {
    render_entry_rectangle *Piece = PushRenderElement(Group, render_entry_rectangle);
    if (Piece) {
        Piece->EntityBasis.Basis = Group->DefaultBasis;
        Piece->EntityBasis.Offset = (Offset - V3(0.5f * Dim, 0));
        Piece->Color = Color;
        Piece->Dim = Dim;
    }
}

inline void
PushRectOutline(render_group *Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1)) {
    real32 Thickness = 0.1f;

    // NOTE: Top and bottom
    PushRect(Group, Offset - V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);
    PushRect(Group, Offset + V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);

    // NOTE: Left and right
    PushRect(Group, Offset - V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
    PushRect(Group, Offset + V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
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

inline v2
Unproject(render_group *RenderGroup, v2 ProjectedXY, real32 AtDistanceFromCamera) {
    v2 WorldXY = AtDistanceFromCamera / RenderGroup->GameCamera.FocalLength * ProjectedXY;
    return WorldXY;
}

inline rectangle2
GetCameraRectangleAtDistance(render_group *Group, real32 DistanceFromCamera) {
    v2 RawXY = Unproject(Group, Group->MonitorHalfDimInMeters, DistanceFromCamera);

    rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);
    return Result;
}

inline rectangle2
GetCameraRectangleAtTarget(render_group *Group) {
    rectangle2 Result = GetCameraRectangleAtDistance(Group, Group->GameCamera.DistanceAboveTarget);
    return Result;
}
