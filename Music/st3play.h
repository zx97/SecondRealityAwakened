#pragma once

#include <stdint.h>

#define MIX_BUF_SAMPLES 4096

namespace st3play
{
    bool PlaySong(const unsigned char * moduleData, unsigned int dataLength, bool useInterpolationFlag, unsigned int audioFreq, unsigned int startingOrder);

    // Pause. musicPaused already gates the mixer tick and the sample markers, so
    // this freezes the module exactly where it is. The PauseSong/TogglePause in
    // the .cpp sit in an anonymous namespace and were never exported.
    void setPaused(bool paused);
    bool isPaused();
    void Close();
    void GetOrderRowAndFrame(unsigned short * orderPtr, unsigned short * rowPtr, unsigned int * framePtr);
    unsigned short GetOrder();
    unsigned short GetRow();
    unsigned int GetFrame();
    short GetPlusFlags();

    // Master output gain, 0..1, for a clean exit fade.
    void SetMasterGain(float gain);

    bool FillAudioBuffer(int16_t * audioBuffer, int32_t samples);
}
