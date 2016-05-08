#ifndef HANDMADE_H
#define HANDMADE_H

/*
 * TODO:
 *
 * - Flush all thread queues before we reload game DLL
 *
 * - Debug code
 *   - Fonts
 *   - Logging
 *   - Diagramming
 *   - (A LITTLE GUI, but only a little!) Switches / sliders / etc.
 *   - Draw tile chunks so we can verify things are aligned / in the chunks we wangt them to be in / etc.
 *   - Thread visualization
 *
 * - Audio
 *   - FIX CLICKING BUG AT END OF SAMPLES!!!
 *
 * - Particle systems
 *
 * - Rendering
 *   - Straighten out all coordinate systems!
 *     - Screen
 *     - World
 *     - Texture
 *   - Lighting
 *   - Optimization
 *
 * ARCHITECTURE EXPLORATION
 * - Z!
 *   - Need to make a solid concept of ground levels so the camera can
 *     be freely placed in Z and have multiple ground levels in one
 *     sim region
 *   - Concept of ground in the collision loop so it can handle
 *   - Make sure flying things can go over low walls
 *   - How is this rendered?
 *     "Frinstances"!
 *     ZFudge!
 * - Collision detection?
 *   - Fix sword collisions!
 *   - Clean up predicate proliferation! Can we make a nice clean
 *     set of flags/rules so that it's easy to understand how
 *     things work in terms of special handling? This mya involve
 *     makeing the iteration handle everything instead of handling
 *     overlap outside and so on.
 *   - Transient collision rules! Clear based on flags.
 *     - Allow non-transient rules to override transient ones.
 *     - Entry/exit?
 *   - What's the plan for robustness / shape definition?
 *   - (Implement reprojection to handle interpenetration)
 *   - "Things pushing other things"
 * - Implement multiple sim regions per frame
 *   - Per-entity clocking
 *   - Sim region merging? For multiple players?
 *
 * - Metagame / save game?
 *   - How do you enter "save slot"?
 *   - Persistent unlocks/etc.
 *   - Do we allow saved games? Probably yes, just only for "pausing".
 *   * Continuous save for or crash recovery?
 * - Rudimentary world gen (no quality, just "what sorts of things" we do)
 *   - Placement of background things
 *   - Connectivity?
 *   - Non-overlapping?
 *   - Map display
 *     - Magnets - how they work???
 * - AI
 *   - Rudimentary monstar behavior example
 *   * Pathfinding
 *   - AI "storage"
 *
 * * Animation, Should probably lead into rendering
 *   - Skeletal animation
 *
 * PRODUCTION
 * -> GAME
 *   - Entity system
 *   - World generation
 */

#include "handmade_platform.h"

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

struct memory_arena {
    memory_index Size;
    uint8 *Base;
    memory_index Used;

    int32 TempCount;
};

struct temporary_memory {
    memory_arena *Arena;
    memory_index Used;
};

inline void
InitializeArena(memory_arena *Arena, memory_index Size, void *Base) {
    Arena->Size = Size;
    Arena->Base = (uint8 *) Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

inline memory_index
GetAlignmentOffset(memory_arena *Arena, memory_index Alignment = 4) {
    memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
    memory_index AlignmentOffset = 0;

    memory_index AlignmentMask = Alignment - 1;
    if (ResultPointer & AlignmentMask) {
        AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
    }

    return AlignmentOffset;
}

inline memory_index
GetArenaSizeRemaining(memory_arena *Arena, memory_index Alignment = 4) {
    memory_index Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Alignment));

    return Result;
}

#define PushStruct(Arena, type, ...) (type *)PushSize_(Arena, sizeof(type), ##__VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type *)PushSize_(Arena, (Count) * sizeof(type), ##__VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, (Size), ##__VA_ARGS__)

inline void *
PushSize_(memory_arena *Arena, memory_index SizeInit, memory_index Alignment = 4) {
    memory_index Size = SizeInit;

    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    Size += AlignmentOffset;

    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;

    Assert(Size >= SizeInit);

    return Result;
}

// NOTE: This is generally not for production use, this is probably
// only really something we need during testing, but who knows
inline char *
PushString(memory_arena *Arena, char *Source) {
    u32 Size = 1;
    for (char *At = Source; *At; ++At) {
        ++Size;
    }

    char *Dest = (char *)PushSize_(Arena, Size);
    for (u32 CharIndex = 0; CharIndex < Size; ++CharIndex) {
        Dest[CharIndex] = Source[CharIndex];
    }

    return Dest;
}


inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena) {
    temporary_memory Result;

    Result.Arena = Arena;
    Result.Used = Arena->Used;

    ++Arena->TempCount;

    return Result;
}

inline void
EndTemporaryMemory(temporary_memory TempMem) {
    memory_arena *Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

inline void
CheckArena(memory_arena *Arena) {
    Assert(Arena->TempCount == 0);
}

inline void
SubArena(memory_arena *Result, memory_arena *Arena, memory_index Size, memory_index Alignment = 16) {
    Result->Size = Size;
    Result->Base = (uint8 *)PushSize_(Arena, Size, Alignment);
    Result->Used = 0;
    Result->TempCount = 0;
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
inline void
ZeroSize(memory_index Size, void *Ptr) {
    // TODO: Check this guy for performance
    uint8 *Byte = (uint8 *) Ptr;
    while (Size--) {
        *Byte++ = 0;
    }
}

inline void
Copy(memory_index Size, void *SourceInit, void *DestInit) {
    u8 *Source = (u8 *)SourceInit;
    u8 *Dest = (u8 *)DestInit;

    while (Size--) {
        *Dest++ = *Source++;
    }

}

#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"
#include "handmade_entity.h"
#include "handmade_render_group.h"
#include "handmade_file_format.h"
#include "handmade_asset.h"
#include "handmade_random.h"
#include "handmade_audio.h"

struct low_entity {
    // TODO: It's kind of busted that P's can be invliad here,
    // AND we store whether they would be invliad in the falgs field...
    // Can we do something better there?
    world_position P;
    sim_entity Sim;
};

struct controlled_hero {
    uint32 EntityIndex;

    // NOTE: These are the controller requests for simulation
    v2 ddP;
    v2 dSword;
    real32 dZ;
};

struct pairwise_collision_rule {
    bool32 CanCollide;
    uint32 StorageIndexA;
    uint32 StorageIndexB;

    pairwise_collision_rule *NextInHash;
};
struct game_state;
internal void AddCollisionRule(game_state *GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 ShouldCollide);
internal void ClearCollisionRulesFor(game_state *GameState, uint32 StorageIndex);

struct ground_buffer {
    // NOTE: An invalid P tells us that this ground_buffer has not been filled
    world_position P; // NOTE: This is the center of the bitmap
    loaded_bitmap Bitmap;
};

struct hero_bitmap_ids {
    bitmap_id Head;
    bitmap_id Cape;
    bitmap_id Torso;
};

struct particle_cel {
    real32 Density;
    v3 VelocityTimesDensity;
};

struct particle {
    bitmap_id BitmapID;
    v3 P;
    v3 dP;
    v3 ddP;
    v4 Color;
    v4 dColor;
};

struct game_state {
    bool32 IsInitialized;

    memory_arena MetaArena;
    memory_arena WorldArena;
    world *World;

    real32 TypicalFloorHeight;

    // TODO: Should we allow split-screen?
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    controlled_hero ControlledHeroes[ArrayCount(((game_input *) 0)->Controllers)];

    // TODO: Change the name to "stored entity"
    uint32 LowEntityCount;
    low_entity LowEntities[100000];

    // TODO: Must be power of two
    pairwise_collision_rule *CollisionRuleHash[256];
    pairwise_collision_rule *FirstFreeCollisionRule;

    sim_entity_collision_volume_group *NullCollision;
    sim_entity_collision_volume_group *SwordCollision;
    sim_entity_collision_volume_group *StairCollision;
    sim_entity_collision_volume_group *PlayerCollision;
    sim_entity_collision_volume_group *MonstarCollision;
    sim_entity_collision_volume_group *FamiliarCollision;
    sim_entity_collision_volume_group *WallCollision;
    sim_entity_collision_volume_group *StandardRoomCollision;

    real32 Time;

    loaded_bitmap TestDiffuse; // TODO: Re-fill this guy with gray;
    loaded_bitmap TestNormal;

    random_series EffectsEntropy; // NOTE: This is entropy that doesn't affect the gameplay
    real32 tSine;

    audio_state AudioState;
    playing_sound *Music;

#define PARTICLE_CEL_DIM 32
    u32 NextParticle;
    particle Particles[256];
    particle_cel ParticleCels[PARTICLE_CEL_DIM][PARTICLE_CEL_DIM];
};

struct task_with_memory {
    bool32 BeingUsed;
    memory_arena Arena;

    temporary_memory MemoryFlush;
};

struct transient_state {
    bool32 IsInitialized;
    memory_arena TranArena;

    task_with_memory Tasks[4];

    game_assets *Assets;

    uint32 GroundBufferCount;
    ground_buffer *GroundBuffers;
    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    uint32 EnvMapWidth;
    uint32 EnvMapHeight;
    // NOTE: 0 is bottom, 1 is middle, 2 is top
    environment_map EnvMaps[3];
};

inline low_entity *
GetLowEntity(game_state *GameState, uint32 Index) {
    low_entity *Result = 0;

    if ((Index > 0) && (Index < GameState->LowEntityCount)) {
        Result = GameState->LowEntities + Index;
    }

    return Result;
}

global_variable platform_api Platform;

internal task_with_memory *BeginTaskWithMemory(transient_state *TranState);
internal void EndTaskWithMemory(task_with_memory *Task);

#endif
