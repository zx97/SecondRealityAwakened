#pragma once

namespace Music
{
    enum class Song : unsigned char
    {
        Skaven,
        PurpleMotion,
        COUNT
    };

    void start(Song song_idx, int start_order);
    void end();

    // Already implemented in the module player, just not exposed.
    void setPaused(bool paused);
    bool isPaused();

    // Master gain (0..1) applied to the mixed output, for the exit fade.
    void setMasterGain(float gain);

    int sync();

    void setSync(int value);

    int getPlusFlags();
    int getRow();
    int getOrder();

    void setFrame(int frame);
    int getFrame();
}