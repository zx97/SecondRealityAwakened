#pragma once

#include <cstring>
#include <stdint.h>

#include "Graphics/shims.h"
#include "Music/Music.h"

#include "Music/audioPlayer.h"

bool demo_wantstoquit();
void demo_requestexit();
int demo_vsync(bool updateAudio = false);
float get_time_ms_precise();

extern int g_captureRemaining;
// Directory prefix for --capture output. Point SR_CAPTURE_PATH at a tmpfs such
// as /dev/shm when capturing many frames: a 1920x1920 PPM is 6 MB per frame, and
// writing that to disk throttles the demo well below 70 fps.
extern const char * g_capturePath;
void demo_setgputeffect(const char * fragmentShader);
void demo_cleargputeffect();
void demo_setgputexture(const char * name, const unsigned char * luminance, int w, int h);
void demo_setgputexture_rgb(const char * name, const unsigned char * rgb, int w, int h);
void demo_setgputexture_la(const char * name, const unsigned char * la, int w, int h);
void demo_gpuuniform1f(const char * name, float v);
void demo_gpuuniform2f(const char * name, float x, float y);
void demo_gpuuniform3f(const char * name, float x, float y, float z);
void demo_gpuuniform1i(const char * name, int v);
void demo_gpuuniform4fv(const char * name, const float * data, int count);
void demo_drawpoints(const void * data, int count, int stride, float pointSize, float pointStretch = 0.0f);
void demo_pointsadditive(bool on);
void demo_setpointsfade(float fade);

struct GpuMeshVertex
{
    float px, py, pz;
    float nx, ny, nz;
    float colorIdx;
    float shadeShift;
};

void demo_drawmesh(const GpuMeshVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16, const float * model16, float fade = 1.0f);
void demo_meshbackground(float r, float g, float b);
void demo_meshviewport(int x0, int y0, int x1, int y1);

// One vertex of the shockwave torus. pos/nrm are the unit torus, radial points
// from the ring axis outwards (dot(nrm, radial) is +1 on the outer skin and -1 on
// the inner wall, which is how the shader keeps only the outside visible), and
// theta/phi locate the point on the ring and on the tube for the plasma pattern.
struct GpuTorusVertex
{
    float px, py, pz;
    float nx, ny, nz;
    float tx, ty, tz;
    float theta, phi;
};

struct GpuTexVertex
{
    float px, py, pz;
    float u, v;
};

void demo_drawtexmesh(const GpuTexVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16);
void demo_meshtexture(const unsigned char * data, int w, int h);
void demo_meshpalette(const unsigned char * vga768);
void demo_meshtime(float t);
void demo_meshwarp(float w);
void demo_sdftexture(const unsigned char * data, int w, int h);
void demo_setoverlaysdftexture(const unsigned char * data, int w, int h);
void demo_rgbatexture(const unsigned char * data, int w, int h, bool perFrame = false);
void demo_explosionflash(float white);
void demo_forcemeshpass(bool on = true);
void demo_drawexplosion(const GpuTorusVertex * verts, int vertCount, const unsigned short * indices,
                        int indexCount, float grow, float spin, float alpha,
                        bool fire = false, float centerX = 0.0f, float centerY = 0.0f);
void demo_rgbatexture_rgb(const unsigned char * data, int w, int h);
// Drawn behind the Shim::cpuPixels composite, so it stays under the SDF text.
void demo_backgroundtexture(const unsigned char * rgb, int w, int h);
void demo_backgroundscroll(float u0, float u1, float alpha, float halfW);
void demo_clearbackground();
void demo_backgroundcolor(float r, float g, float b);
void demo_compositescale(float s);
unsigned char * demo_loadpng_rgb(const char * path, int * outW, int * outH);
unsigned char * demo_loadpng_rgb_mem(const unsigned char * png, int len, int * outW, int * outH);
void demo_freepng(unsigned char * data);
void demo_drawfullimage_rgb(const unsigned char * data, int w, int h);
void demo_setrgba_white(float white);
void demo_setsdftextcallback(void (*fn)());
void demo_drawsdftext(const GpuTexVertex * quads, int quadCount, const float * mvp16, float r, float g, float b, bool sharp = false, bool overlay = false);
void demo_drawrgbaquad(const GpuTexVertex * quads, int quadCount, const float * mvp16, float blur = 0.0f);
void demo_meshbackground(float r, float g, float b);
void demo_meshbackgroundscreen();
void demo_meshviewport(int x0, int y0, int x1, int y1);
float demo_windowaspect();
float demo_windowheight();
float demo_framebufferaspect();
bool demo_meshbgactive();

#define COUNT(X) (sizeof(X) / sizeof(X[0]))
#define CLEAR(X) memset(X, 0, sizeof(X));

constexpr const int PaletteColorCount = 256;
constexpr const int PaletteByteCount = (3 * PaletteColorCount);

extern int g_virtualWidth;
extern int g_virtualHeight;
// The virtual framebuffer always stays at a fixed logical resolution
// regardless of the window size. Parts write g_screen32 at this resolution;
// the window size only controls the on-screen viewport. 2560x1080 covers up to
// a 21:9 credits frame (the largest part); every other part still fills only
// the top-left sub-region it declares.
constexpr const int32_t VIRTUAL_SCREEN_WIDTH = 2560;
constexpr const int32_t VIRTUAL_SCREEN_HEIGHT = 1080;

constexpr const int32_t SCREEN_WIDTH = 320;
constexpr const int32_t SCREEN_HEIGHT = 200;
constexpr const int32_t SCREEN_SIZE = (SCREEN_WIDTH * SCREEN_HEIGHT);

constexpr const int32_t DOUBlE_SCREEN_WIDTH = (2 * SCREEN_WIDTH);
constexpr const int32_t DOUBlE_SCREEN_HEIGHT = (2 * SCREEN_HEIGHT);
constexpr const int32_t DOUBlE_SCREEN_SIZE = (DOUBlE_SCREEN_WIDTH * DOUBlE_SCREEN_HEIGHT);

constexpr const int32_t PLANAR_WIDTH = 80;

using Palette = char[PaletteByteCount];

namespace Common
{
    namespace Data
    {
        extern const short sin1024[];
    }

    extern int frame_count;
    extern char * cop_pal;
    extern int do_pal;
    extern int cop_start;
    extern int cop_scrl;
    extern int cop_dofade;
    extern int cop_drop;
    extern short * cop_fadepal;
    extern Palette fadepal;
    extern short fadepal_short[];

    void reset();

    void copper2();
    void copper3();

    void readp(char * dest, int row, const char * src);

    void setpalarea(char * p, int offset = 0, int count = PaletteColorCount);
    void getpalarea(char * p, int offset = 0, int count = PaletteColorCount);
}
