#ifndef HANDMADE_INTRINSICS_H
#define HANDMADE_INTRINSICS_H

// TODO: Convert all of these to platform-efficient versions
// and remove math.h

#include "math.h"

#if COMPILER_MSVC
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 Expected, uint32 New) {
    uint32 Result = _InterlockedCompareExchange((long *)Value, Expected, New);
    return Result;
}
#elif HANDMADE_SDL
#include <SDL2/SDL.h>
#define CompletePreviousWritesBeforeFutureWrites SDL_CompilerBarrier()
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 Expected, uint32 New) {
    SDL_atomic_t Atomic;
    Atomic.value = *Value;
    if (SDL_AtomicCAS(&Atomic, Expected, New)) {
        return Expected;
    } else {
        return Atomic.value;
    }
}
#else
// TODO: Need GCC/LLVM equiavalents!
#endif

inline int32
SignOf(int32 Value) {
    int32 Result = (Value >= 0) ? 1 : -1;
    return Result;
}

inline real32
SignOf(real32 Value) {
    real32 Result = (Value >= 0.0f) ? 1.0f : -1.0f;
    return Result;
}

inline real32
SquareRoot(real32 Real32) {
    return sqrtf(Real32);
}

inline real32
AbsoluteValue(real32 Real32) {
    real32 Result = fabsf(Real32);
    return Result;
}

inline uint32
RotateLeft(uint32 Value, int32 Amount) {
#if COMPILER_MSVC
    uint32 Result = _rotl(Value, Amount);
#else
    // TODO: Actually port this to other compiler platforms!
    Amount &= 31;
    uint32 Result = ((Value << Amount) | (Value >> (32 - Amount)));
#endif
    return Result;
}

inline uint32
RotateRight(uint32 Value, int32 Amount) {
#if COMPILER_MSVC
    uint32 Result = _rotr(Value, Amount);
#else
    // TODO: Actually port this to other compiler platforms!
    Amount &= 31;
    uint32 Result = ((Value >> Amount) | (Value << (32 - Amount)));
#endif
    return Result;
}

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
CeilReal32ToInt32(real32 Real32) {
    int32 Result = (int32) ceilf(Real32);
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
