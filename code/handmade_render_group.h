#ifndef HANDMADE_RENDER_GROUP_H
#define HANDMADE_RENDER_GROUP_H

struct loaded_bitmap {
    int32 Width;
    int32 Height;
    int32 Pitch;
    void *Memory;
};

struct environment_map {
    loaded_bitmap LOD[4];
    real32 Pz;
};

struct render_basis {
    v3 P;
};

struct render_entity_basis {
    render_basis *Basis;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;
};

enum render_group_entry_type {
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_coordinate_system,
    RenderGroupEntryType_render_entry_saturation,
};

struct render_group_entry_header {
    render_group_entry_type Type;
};

struct render_entry_clear {
    v4 Color;
};

struct render_entry_saturation {
    real32 Level;
};

struct render_entry_coordinate_system {
    v2 Origin;
    v2 XAxis;
    v2 YAxis;
    v4 Color;
    loaded_bitmap *Texture;
    loaded_bitmap *NormalMap;

    environment_map *Top;
    environment_map *Middle;
    environment_map *Bottom;
};

struct render_entry_bitmap {
    render_entity_basis EntityBasis;
    loaded_bitmap *Bitmap;
    v4 Color;
};

struct render_entry_rectangle {
    render_entity_basis EntityBasis;
    v4 Color;
    v2 Dim;
};

// TODO: This is dump, this should just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct render_group {
    render_basis *DefaultBasis;
    real32 MetersToPixels;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;
};

#endif
