#ifndef HANDMADE_AUDIO_H
#define HANDMADE_AUDIO_H

struct playing_sound {
    v2 CurrentVolume;
    v2 dCurrentVolume;
    v2 TargetVolume;

    r32 dSample;

    sound_id ID;
    real32 SamplesPlayed;

    playing_sound *Next;
};

struct audio_state {
    memory_arena *PermArena;
    playing_sound *FirstPlayingSound;
    playing_sound *FirstFreePlayingSound;

    v2 MasterVolume;
};

#endif
