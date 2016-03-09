#ifndef HANDMADE_RENDER_GROUP_H
#define HANDMADE_RENDER_GROUP_H

/*

  NOTE:

    1) Everywhere outside the render, Y _always_ goes upward, X to the right.

    2) All bitmps including the render target are assumed to be bottom-up
       (meaing that the first row pointer points to the bottom-most row
        when viewed on screen).

    3) Unless otherwise specified, all inputs to the renderer are in world
       coordinate ("meters"), NOT pixels.  Anything that is in pixel values
       will be explicity marked as such.

    4) Z is a special coordinate because it is broken up into discrete slices,
       and the renderer actually understands these slices.  Z sliees are what
       control the _scaling_ of things, whereas Z offsets inside a slice are
       what control Y offering.

       // TODO: ZHANDLING

    5) All color values specified to the renderer as V4's are in
       NON-Premultiplied alpha.
 */

struct loaded_bitmap {
    v2 Align;

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
    v3 Offset;
};

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
    v4 Color;
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

// NOTE: This is only for test:
// {
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
// }

// TODO: This is dump, this should just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct render_group {
    real32 GlobalAlpha;

    render_basis *DefaultBasis;
    real32 MetersToPixels;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;
};

// NOTE: Renderer API

#if 0
inline void
PushBitmap(render_group *Group, loaded_bitmap *Bitmap, v2 Offset, real32 OffsetZ,
           v4 Color = V4(1, 1, 1, 1));

inline void
PushRect(render_group *Group, v2 Offset, real32 OffsetZ,
          v2 Dim, v4 Color);

inline void
PushRectOutline(render_group *Group, v2 Offset, real32 OffsetZ, v2 Dim,
                v4 Color = V4(1, 1, 1, 1));

inline void
Clear(render_group *Group, v4 Color);
#endif

#endif
