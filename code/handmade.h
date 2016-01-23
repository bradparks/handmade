#ifndef HANDMADE_H
#define HANDMADE_H

#include "handmade_platform.h"

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

struct memory_arena {
    memory_index Size;
    uint8 *Base;
    memory_index Used;
};

void
InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base) {
    Arena->Size = Size;
    Arena->Base = Base;
    Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *) PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *) PushSize_(Arena, (Count) * sizeof(type))

void *
PushSize_(memory_arena *Arena, memory_index Size) {
    Assert(Arena->Used + Size <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

#include "handmade_math.h"
#include "handmade_intrinsics.h"
#include "handmade_world.h"

struct loaded_bitmap {
    int32 Width;
    int32 Height;
    uint32 *Pixels;
};

struct hero_bitmaps {
    v2 Align;

    loaded_bitmap Head;
    loaded_bitmap Cape;
    loaded_bitmap Torso;
};

struct high_entity {
    v2 P; // NOTE: Relative to the camera!
    v2 dP;
    uint32 ChunkZ;
    uint32 FacingDirection;

    real32 tBob;

    real32 Z;
    real32 dZ;

    uint32 LowEntityIndex;
};

enum entity_type {
    EntityType_Null,

    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monstar,
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point {
    // TODO: Bake this down into one variable.
    uint8 Flags;
    uint8 FilledAmount;
};

struct low_entity {
    entity_type Type;

    world_position P;
    real32 Width, Height;

    // NOTE: This is for "stairs"
    bool32 Collides;
    int32 dAbsTileZ;

    uint32 HighEntityIndex;

    // TODO: Should hitpoints themselves be entities?
    uint32 HitPointMax;
    hit_point HitPoints[16];
};

struct entity {
    uint32 LowIndex;
    low_entity *Low;
    high_entity *High;
};

struct entity_visible_piece {
    loaded_bitmap *Bitmap;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;

    real32 R, G, B, A;
    v2 Dim;
};

struct game_state {
    memory_arena WorldArena;
    world *World;

    // TODO: Should we allow split-screen?
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    uint32 PlayerIndexForController[ArrayCount(((game_input *) 0)->Controllers)];

    uint32 LowEntityCount;
    low_entity LowEntities[100000];

    uint32 HighEntityCount;
    high_entity HighEntities_[256];

    loaded_bitmap Backdrop;
    loaded_bitmap Shadow;
    hero_bitmaps HeroBitmaps[4];

    loaded_bitmap Tree;
    real32 MetersToPixels;
};

// TODO: This is dump, this should just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct entity_visible_piece_group {
    game_state *GameState;
    uint32 PieceCount;
    entity_visible_piece Pieces[32];
};

#endif
