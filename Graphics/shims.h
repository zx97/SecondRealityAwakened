#pragma once

#include <stddef.h>

#include <vector>

constexpr const float Default_JSSS = 0.80f;

extern bool g_respectRatio;
extern bool g_hires;
extern bool g_pixelPerfect;
extern bool g_smooth;
extern bool g_directScreen;

extern std::vector<unsigned int> g_screen32;

#include "Blob/Blob.h"

void demo_blit();
void demo_changemode(int x, int y, float jsss = Default_JSSS);
void demo_directscreen(bool direct);
void demo_settextoverlay(void (*fn)(unsigned int * rgba, int w, int h));
void demo_setsdftextcallback(void (*fn)());
void demo_sdftexture(const unsigned char * data, int w, int h);
void demo_rgbatexture(const unsigned char * data, int w, int h, bool perFrame);

namespace Shim
{
    constexpr const size_t CPU_PIXELS_X = 2560;
    constexpr const size_t CPU_PIXELS_Y = 1080;

    extern unsigned int palette[];
    extern unsigned int startpixel;

    // One palette index per pixel, written on the CPU by the parts. Not video
    // memory: PrepareTextureForGPU() expands it through palette[] into the RGBA
    // g_screen32, which is what actually reaches the GPU. The original Amiga and
    // PC code wrote straight into chip RAM at 0xA0000, hence the old name.
    extern unsigned char cpuPixels[];

    void clearScreen();

    void setpal(int idx, unsigned char r, unsigned char g, unsigned char b);
    void outp(int reg, unsigned int value);
    unsigned char inp(int reg);
    void setstartpixel(int reg);

    bool isDemoFirstPart();
    void finishedDemoFirstPart();
}