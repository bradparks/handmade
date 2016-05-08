#ifndef HANDMADE_RENDER_GROUP_H
#define HANDMADE_RENDER_GROUP_H

/*

  NOTE:

    1) Everywhere outside the render, Y _always_ goes upward, X to the right.

    2) All bitmps including the render target are assumed to be bottom-up
       (meaing that the first row pointer points to the bottom-most row
        when viewed on screen).

    3) It is mandatory that all inputs to the renderer are in world
       coordinate ("meters"), NOT pixels.  If for some reason something
       absolutely has to be specified in pixels, that will be explicity
       marked in the API, but this should occur exceedingly sparingly.

    4) Z is a special coordinate because it is broken up into discrete slices,
       and the renderer actually understands these slices.  Z sliees are what
       control the _scaling_ of things, whereas Z offsets inside a slice are
       what control Y offering.

       // TODO: ZHANDLING

    5) All color values specified to the renderer as V4's are in
       NON-Premultiplied alpha.
 */

struct loaded_bitmap {
    void *Memory;
    v2 AlignPercentage;
    r32 WidthOverHeight;
    s32 Width;
    s32 Height;
    // TODO: Get rid of pitch!
    s32 Pitch;
};

struct environment_map {
    loaded_bitmap LOD[4];
    real32 Pz;
};

struct render_basis {
    v3 P;
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
    loaded_bitmap *Bitmap;

    v4 Color;
    v2 P;
    v2 Size;
};

struct render_entry_rectangle {
    v4 Color;
    v2 P;
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

    real32 PixelsToMeters; // TODO: Need to store this for lighting!

    environment_map *Top;
    environment_map *Middle;
    environment_map *Bottom;
};
// }

struct render_transform {
    bool32 Orthographic;

    // NOTE: Camera parameters
    real32 FocalLength;
    real32 DistanceAboveTarget;

    real32 MetersToPixels; // NOTE: This translate meters _on the monitor_ into pixels _on the monitor_
    v2 ScreenCenter;

    v3 OffsetP;
    real32 Scale;
};

// TODO: This is dump, this should just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct render_group {
    struct game_assets *Assets;
    real32 GlobalAlpha;

    u32 GenerationID;

    v2 MonitorHalfDimInMeters;

    render_transform Transform;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;

    uint32 MissingResourceCount;
    b32 RendersInBackground;
    b32 InsideRender;
};

void DrawRectangleQuickly(loaded_bitmap *Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
                          loaded_bitmap *Texture, rectangle2i ClipRect, bool32 Even);

#endif
