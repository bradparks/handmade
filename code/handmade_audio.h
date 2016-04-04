#ifndef HANDMADE_AUDIO_H
#define HANDMADE_AUDIO_H

struct playing_sound {
    real32 Volume[2];
    sound_id ID;
    uint32 SamplesPlayed;

    playing_sound *Next;
};

struct audio_state {
    memory_arena *PermArena;
    playing_sound *FirstPlayingSound;
    playing_sound *FirstFreePlayingSound;
};

#endif
