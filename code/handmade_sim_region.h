#ifndef HANDMADE_SIM_REGION_H
#define HANDMADE_SIM_REGION_H

struct sim_entity {
    uint3 StorageIndex;

    v2 P;
    uint32 ChunkZ;

    real32 Z;
    real32 dZ;
};

struct sim_region {
    // TODO: Need a hash table here to map stored entity indices
    // to sim entities!
    world_position Origin;
    rectangle2 Bounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity *Entities;
};

#endif
