#ifndef HANDMADE_INTRINSICS_H
#define HANDMADE_INTRINSICS_H

#include "math.h"

// TODO: Convert all of these to platform-efficient versions
// and remove math.h

inline int32
RoundReal32ToInt32(real32 Real32) {
    int32 Result = (int32) roundf(Real32);
    // TODO: Intrinsic????
    return Result;
}

inline uint32
RoundReal32ToUInt32(real32 Real32) {
    uint32 Result = (uint32) roundf(Real32);
    return Result;
}

inline int32
FloorReal32ToInt32(real32 Real32) {
    int32 Result = (int32) floorf(Real32);
    return Result;
}

inline int32
TruncateReal32ToInt32(real32 Real32) {
    int32 Result = (int32) Real32;
    return Result;
}

inline real32
Sin(real32 Angle) {
    return sinf(Angle);
}

inline real32
Cos(real32 Angle) {
    return cosf(Angle);
}

inline real32
Atan2(real32 Y, real32 X) {
    return atan2f(Y, X);
}

// TODO: Move this into the instrinsics and call the MSVC version
struct bit_scan_result {
    bool32 Found;
    uint32 Index;
};

inline bit_scan_result
FindLeastSignificantSetBit(uint32 Value) {
    bit_scan_result Result = {};

#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long *) &Result.Index, Value);
#else
    for (uint32 Test = 0; Test < 32; ++Test) {
        if (Value & (1 << Test)) {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
#endif

    return Result;
}


#endif