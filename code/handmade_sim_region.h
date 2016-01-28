#ifndef HANDMADE_SIM_REGION_H
#define HANDMADE_SIM_REGION_H

struct move_spec {
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

enum entity_type {
    EntityType_Null,

    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monstar,
    EntityType_Sword,
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point {
    // TODO: Bake this down into one variable.
    uint8 Flags;
    uint8 FilledAmount;
};

// TODO: Rename sim_entity to entity
struct sim_entity;
union entity_reference {
    sim_entity *Ptr;
    uint32 Index;
};

enum sim_entity_flags {
    // TODO: Does it make more sense to have the flag be for _non_ colliding entities?
    EntityFlag_Collides = (1 << 1),
    EntityFlag_Nonspatial = (1 << 2),

    EntityFlag_Simming = (1 << 30),
};

struct sim_entity {
    uint32 StorageIndex;

    entity_type Type;
    uint32 Flags;

    v2 P;
    v2 dP;

    real32 Z;
    real32 dZ;

    uint32 ChunkZ;

    real32 Width, Height;

    uint32 FacingDirection;
    real32 tBob;

    int32 dAbsTileZ;

    // TODO: Should hitpoints themselves be entities?
    uint32 HitPointMax;
    hit_point HitPoints[16];

    entity_reference Sword;
    real32 DistanceRemaining;

    // TODO: Generation index so we know how "up to date" this entity is.
};

struct sim_entity_hash {
    sim_entity *Ptr;
    uint32 Index;
};

struct sim_region {
    world *World;

    // TODO: Need a hash table here to map stored entity indices
    // to sim entities!
    world_position Origin;
    rectangle2 Bounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity *Entities;

    // TODO: Do I really want a hash for this?
    // NOTE: Must be a power of two!
    sim_entity_hash Hash[4096];
};

#endif
