#ifndef HANDMADE_RENDER_GROUP_H
#define HANDMADE_RENDER_GROUP_H

struct render_basis {
    v3 P;
};

struct render_entity_basis {
    render_basis *Basis;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;
};

// NOTE: render_group_entry is a "compact discriminated union"
// TODO: Remove the header!
enum render_group_entry_type {
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_coordinate_system,
};

struct render_group_entry_header {
    render_group_entry_type Type;
};

struct render_entry_clear {
    render_group_entry_header Header;
    v4 Color;
};

struct render_entry_coordinate_system {
    render_group_entry_header Header;
    v2 Origin;
    v2 XAxis;
    v2 YAxis;
    v4 Color;

    v2 Points[16];
};

struct render_entry_bitmap {
    render_group_entry_header Header;
    render_entity_basis EntityBasis;
    loaded_bitmap *Bitmap;
    real32 R, G, B, A;
};

struct render_entry_rectangle {
    render_group_entry_header Header;
    render_entity_basis EntityBasis;
    v2 Dim;
    real32 R, G, B, A;
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
