#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <chrono>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb_image.h"

#include "Graphics/Graphics.h"
#include "DebugOverlay.h"
#include "Parts/Common.h"
#include "Parts/ResConfig.h"
#include "Parts/VISU/VISU.h"

extern bool g_wantsToQuit;

int g_partSkipDelta = 0;

unsigned char * demo_loadpng_rgb(const char * path, int * outW, int * outH)
{
    int w = 0, h = 0, comp = 0;
    unsigned char * data = stbi_load(path, &w, &h, &comp, 3);
    if (!data) return nullptr;
    *outW = w;
    *outH = h;
    return data;
}

unsigned char * demo_loadpng_rgb_mem(const unsigned char * png, int len, int * outW, int * outH)
{
    int w = 0, h = 0, comp = 0;
    unsigned char * data = stbi_load_from_memory(png, len, &w, &h, &comp, 3);
    if (!data) return nullptr;
    *outW = w;
    *outH = h;
    return data;
}

void demo_freepng(unsigned char * data)
{
    if (data) stbi_image_free(data);
}

#define FORWARD_PART(X) \
    namespace X         \
    {                   \
        void main();    \
    }

FORWARD_PART(Alku)
FORWARD_PART(U2A)
FORWARD_PART(OUTTA)
FORWARD_PART(Beg)
FORWARD_PART(Glenz)
FORWARD_PART(Tunneli)
FORWARD_PART(KOE)
FORWARD_PART(Shutdown)
FORWARD_PART(Forest)
FORWARD_PART(Lens)
FORWARD_PART(PLZ)
namespace PLZ
{
    extern bool g_cubeOnly;
}
FORWARD_PART(Dots)
FORWARD_PART(Water)
FORWARD_PART(Coman)
FORWARD_PART(JPLogo)
FORWARD_PART(U2E)
FORWARD_PART(End)
FORWARD_PART(Credits)
FORWARD_PART(EndScrl)
FORWARD_PART(DDStars)

bool g_respectRatio = true;
bool g_hires = false;
bool g_pixelPerfect = false;
bool g_smooth = false;
bool g_directScreen = false;
bool g_alkuScrollOnly = false;
// --delay N: open the window and hold it for N seconds before running the
// demo, so an external video capture tool can pick it up.
int g_startDelaySeconds = 0;
int g_windowWidth = 0;
int g_windowHeight = 0;
// Set by the ESC / window-close handler; DemoUpdateExitFade() then fades the
// music and the picture to black before the demo actually quits.
bool g_exitFadeRequest = false;

namespace
{
    enum struct Part
    {
        _00_AlkutekstitI,
        _01_AlkutekstitII,
        _02_AlkutekstitIII,
        _03_Logo,
        _04_Glenz,
        _05_Dottitunneli,
        _06_Techno,
        _07_Panicfake,
        _08_VuoriScrolli,
        _09_Rotazoomer,
        _10_Plasmacube,
        _11_MiniVectorBalls,
        _12_Peilipalloscroll,
        _13_3D_Sinusfield,
        _14_Jellypic,
        _15_VectorPartII,
        _16_Endpictureflash,
        _17_CreditsGreetings,
        _18_EndScrolling,
        ____Empty0,
        _19_HiddenPart,
        ____Empty1,
    };

    struct PartInfo
    {
        Music::Song music;
        unsigned char musicStartOrder;
        unsigned short width;
        unsigned short height;
        void (*part)() = nullptr;

        // Opt-in inheritance of the previous part's background layer. ALKU
        // hands its hires picture to U2A, which carries it into PAM, so those
        // two parts must NOT have the layer cleared when they start. Every
        // other part gets a clean layer: leaving it active paints ALKU's
        // bitmap over later mesh parts (PLZ, U2E) because the cpuPixels
        // composite is skipped while the layer is active. See mainLoop().
        bool keepPreviousBackground = false;
    };

    const PartInfo main_parts[] = {
        /* 00 Alkutekstit I     */ { Music::Song::Skaven, 0x00, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Alku::main },
        /* 01 Alkutekstit II    */ { Music::Song::Skaven, 0x0C, SCREEN_WIDTH, SCREEN_HEIGHT, U2A::main, true },
        /* 02 Alkutekstit III   */ { Music::Song::Skaven, 0x0D, SCREEN_WIDTH, SCREEN_HEIGHT, OUTTA::main, true },
        /* 03 Logo              */ { Music::Song::Skaven, 0x0E, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Beg::main },
        /* 04 Glenz             */ { Music::Song::PurpleMotion, 0x00, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Glenz::main },
        /* 05 Dottitunneli      */ { Music::Song::PurpleMotion, 0x0F, SCREEN_WIDTH, SCREEN_HEIGHT, Tunneli::main },
        /* 06 Techno            */ { Music::Song::PurpleMotion, 0x14, SCREEN_WIDTH, SCREEN_HEIGHT, KOE::main },
        /* 07 Panicfake         */ { Music::Song::PurpleMotion, 0x27, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Shutdown::main },
        /* 08 Vuori-Scrolli     */ { Music::Song::PurpleMotion, 0x2A, SCREEN_WIDTH, SCREEN_HEIGHT, Forest::main },
        // Lens
        /* 09 Rotazoomer        */ { Music::Song::PurpleMotion, 0x2F, SCREEN_WIDTH, SCREEN_HEIGHT, Lens::main },
        // Plasma
        /* 10 Plasmacube        */ { Music::Song::PurpleMotion, 0x3E, SCREEN_WIDTH, SCREEN_HEIGHT, PLZ::main },
        /* 11 MiniVectorBalls   */ { Music::Song::PurpleMotion, 0x4D, SCREEN_WIDTH, SCREEN_HEIGHT, Dots::main },
        /* 12 Peilipalloscroll  */ { Music::Song::PurpleMotion, 0x58, SCREEN_WIDTH, SCREEN_HEIGHT, Water::main },
        /* 13 3D-Sinusfield     */ { Music::Song::PurpleMotion, 0x5E, SCREEN_WIDTH, SCREEN_HEIGHT, Coman::main },
        /* 14 Jellypic          */ { Music::Song::PurpleMotion, 0x62, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, JPLogo::main },
        /* 15 Vector Part II    */ { Music::Song::Skaven, 0x12, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, U2E::main },
        /* 16 Endpictureflash   */ { Music::Song::Skaven, 0x19, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, End::main },
        /* 17 Credits/Greetings */ { Music::Song::Skaven, 0x1C, 2560, 1080, Credits::main },
        /* 18 EndScrolling      */ { Music::Song::Skaven, 0x2B, DOUBlE_SCREEN_WIDTH, 350, EndScrl::main },
        { Music::Song::COUNT, 0x00, 0, 0, nullptr },
        /* 20 HiddenPart        */ { Music::Song::Skaven, 0x46, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, DDStars::main },
        { Music::Song::COUNT, 0x00, 0, 0, nullptr },
    };

    // Command character for each part, declared right next to the table above so
    // the two cannot drift apart. It is deliberately NOT positional: 'f' and 'h'
    // already belong to --fullscreen and --hires, so the letters skip them and
    // part 15 is 'g', not 'f'. 0 marks the sentinels, which have no character.
    // The overlay prints this next to the part number, because "part 15" reads
    // like the key "5" and is not.
    const char kPartCommand[static_cast<int>(Part::____Empty1) + 1] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',   //  0 -  9
        'a', 'b', 'c', 'd', 'e', 'g', 'i', 'j', 'k',        // 10 - 18
        0,                                                  // 19  ____Empty0
        'u',                                                // 20  _19_HiddenPart
        0,                                                  // 21  ____Empty1
    };

    Graphics g_graphics;

    float g_lastVblank = { 0.0f };

    bool g_windowMode = true;

    bool g_looping = false;

    Part g_start = Part::_00_AlkutekstitI;

}

void demo_changemode(int x, int y, float jsss)
{
    g_graphics.ChangeMode(x, y, jsss);
}

void demo_directscreen(bool direct)
{
    g_directScreen = direct;
}

void demo_settextoverlay(void (*fn)(unsigned int * rgba, int w, int h))
{
    g_graphics.SetTextOverlay(fn);
}

void demo_setoverlaysdftexture(const unsigned char * data, int w, int h)
{
    g_graphics.SetOverlaySdfTexture(data, w, h);
}

// Called from the graphics layer's key handling, which cannot know about the
// overlay. Only visibility changes here: Tick runs from demo_vsync and OnPartStart
// runs per part regardless, so the counters never need a reset when unhiding.
void DemoToggleDebugOverlay()
{
    DebugOverlay::SetEnabled(!DebugOverlay::IsEnabled());
    std::fprintf(stderr, "[debug] overlay %s (F12)\n",
                 DebugOverlay::IsEnabled() ? "on" : "off");
}

void demo_setsdftextcallback(void (*fn)())
{
    g_graphics.SetSdfTextCallback(fn);
}

void demo_sdftexture(const unsigned char * data, int w, int h)
{
    g_graphics.SdfTexture(data, w, h);
}

void demo_rgbatexture(const unsigned char * data, int w, int h, bool perFrame)
{
    g_graphics.RgbaTexture(data, w, h, false, perFrame);
}

void demo_rgbatexture_rgb(const unsigned char * data, int w, int h)
{
    g_graphics.RgbaTexture(data, w, h, true);
}

void demo_backgroundtexture(const unsigned char * rgb, int w, int h)
{
    g_graphics.SetBackgroundLayer(rgb, w, h);
}

void demo_backgroundscroll(float u0, float u1, float alpha, float halfW)
{
    g_graphics.SetBackgroundLayerScroll(u0, u1, alpha, halfW);
}

void demo_clearbackground()
{
    g_graphics.ClearBackgroundLayer();
}

void demo_backgroundcolor(float r, float g, float b)
{
    g_graphics.SetBackgroundColor(r, g, b);
}

void demo_compositescale(float s)
{
    g_graphics.SetCompositeScale(s);
}

void demo_drawsdftext(const GpuTexVertex * quads, int quadCount, const float * mvp16, float r, float g, float b, bool sharp, bool overlay)
{
    g_graphics.DrawSdfText(quads, quadCount, mvp16, r, g, b, sharp, overlay);
}

void demo_forcemeshpass(bool on)
{
    g_graphics.SetForceMeshPass(on);
}

void demo_explosionflash(float white)
{
    g_graphics.SetExplosionFlash(white);
}

void demo_drawexplosion(const GpuTorusVertex * verts, int vertCount, const unsigned short * indices,
                        int indexCount, float grow, float spin, float alpha,
                        bool fire, float centerX, float centerY)
{
    g_graphics.DrawExplosion(verts, vertCount, indices, indexCount, grow, spin, alpha, fire, centerX, centerY);
}

void demo_drawrgbaquad(const GpuTexVertex * quads, int quadCount, const float * mvp16, float blur)
{
    g_graphics.DrawRgbaQuad(quads, quadCount, mvp16, blur);
}

void demo_drawfullimage_rgb(const unsigned char * data, int w, int h)
{
    if (!data || w <= 0 || h <= 0) return;

    demo_rgbatexture_rgb(data, w, h);

    static const float IDENT[16] = {
        1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
    };

    const float winAspect = demo_windowaspect();
    const float imgAspect = (float)w / (float)h;
    const float hw = imgAspect / winAspect;

    GpuTexVertex q[4];
    q[0].px = -hw; q[0].py =  1.f; q[0].pz = 0; q[0].u = 0; q[0].v = 0;
    q[1].px =  hw; q[1].py =  1.f; q[1].pz = 0; q[1].u = 1; q[1].v = 0;
    q[2].px = -hw; q[2].py = -1.f; q[2].pz = 0; q[2].u = 0; q[2].v = 1;
    q[3].px =  hw; q[3].py = -1.f; q[3].pz = 0; q[3].u = 1; q[3].v = 1;
    demo_drawrgbaquad(q, 1, IDENT);
}

namespace
{
    bool s_exitFadeActive = false;
    float s_exitFadeStartMs = 0.0f;

    // Graceful quit: once requested, fade the music out and the picture to
    // black over at most EXIT_FADE_MS, then let the parts unwind. Called every
    // frame from demo_blit(), so whichever part is running drives the fade.
    void DemoUpdateExitFade()
    {
        if (g_exitFadeRequest && !s_exitFadeActive)
        {
            s_exitFadeActive = true;
            s_exitFadeStartMs = get_time_ms_precise();
        }
        if (!s_exitFadeActive) return;

        constexpr float EXIT_FADE_MS = 2000.0f;
        float t = (get_time_ms_precise() - s_exitFadeStartMs) / EXIT_FADE_MS;
        if (t > 1.0f) t = 1.0f;

        Music::setMasterGain(1.0f - t);
        g_graphics.SetExitFade(t);

        if (t >= 1.0f)
        {
            g_wantsToQuit = true;
        }
    }
}

void demo_blit()
{
    DemoUpdateExitFade();

    // Queued with the rest of the frame, before Update. The overlay carries its
    // own glyph atlas (see SetOverlaySdfTexture), which is what stops parts
    // drawing through the RGBA quads from hiding it and stops their own charset
    // from invalidating the UVs it already computed. Draining it here instead,
    // after Update, put it in the back buffer where the next frame's clear
    // erased it before it was ever presented.
    if (DebugOverlay::IsEnabled()) DebugOverlay::Draw();
    g_graphics.Update();
}

bool demo_wantstoquit()
{
    return g_graphics.WantsToQuit();
}

void demo_requestexit()
{
    g_exitFadeRequest = true;
}

void demo_setgputeffect(const char * fragmentShader)
{
    g_graphics.SetGpuEffect(fragmentShader);
}

void demo_setgputexture(const char * name, const unsigned char * luminance, int w, int h)
{
    g_graphics.SetGpuTexture(name, luminance, w, h);
}

void demo_setgputexture_rgb(const char * name, const unsigned char * rgb, int w, int h)
{
    g_graphics.SetGpuTextureRgb(name, rgb, w, h);
}

void demo_setgputexture_la(const char * name, const unsigned char * la, int w, int h)
{
    g_graphics.SetGpuTextureLa(name, la, w, h);
}

void demo_cleargputeffect()
{
    g_graphics.ClearGpuEffect();
}

void demo_gpuuniform1f(const char * name, float v)
{
    g_graphics.SetUniform1f(name, v);
}
void demo_gpuuniform2f(const char * name, float x, float y)
{
    g_graphics.SetUniform2f(name, x, y);
}

void demo_gpuuniform3f(const char * name, float x, float y, float z)
{
    g_graphics.SetUniform3f(name, x, y, z);
}

void demo_gpuuniform1i(const char * name, int v)
{
    g_graphics.SetUniform1i(name, v);
}

void demo_gpuuniform4fv(const char * name, const float * data, int count)
{
    g_graphics.SetUniform4fv(name, data, count);
}

void demo_setpointsfade(float fade)
{
    g_graphics.SetPointsFade(fade);
}

void demo_pointsadditive(bool on)
{
    g_graphics.SetPointsAdditive(on);
}

void demo_drawpoints(const void * data, int count, int stride, float pointSize, float pointStretch)
{
    g_graphics.DrawPoints(data, count, stride, pointSize, pointStretch);
}

void demo_drawmesh(const GpuMeshVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16, const float * model16, float fade)
{
    g_graphics.DrawMesh(verts, vertCount, indices, indexCount, mvp16, model16, fade);
}

void demo_drawtexmesh(const GpuTexVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16)
{
    g_graphics.DrawTexMesh(verts, vertCount, indices, indexCount, mvp16);
}

void demo_meshtexture(const unsigned char * data, int w, int h)
{
    g_graphics.MeshTexture(data, w, h);
}

void demo_meshpalette(const unsigned char * vga768)
{
    g_graphics.MeshPalette(vga768);
}

void demo_meshtime(float t)
{
    g_graphics.MeshTime(t);
}

void demo_meshwarp(float w)
{
    g_graphics.MeshWarp(w);
}

void demo_meshbackground(float r, float g, float b)
{
    g_graphics.MeshBackground(r, g, b);
}

void demo_meshviewport(int x0, int y0, int x1, int y1)
{
    g_graphics.MeshViewport(x0, y0, x1, y1);
}

void demo_meshbackgroundscreen()
{
    g_graphics.MeshBackgroundScreen();
}

float demo_windowaspect()
{
    return g_graphics.WindowAspect();
}

float demo_windowheight()
{
    return g_graphics.WindowHeight();
}

float demo_framebufferaspect()
{
    return g_graphics.FramebufferAspect();
}

void demo_setrgba_white(float white)
{
    g_graphics.SetRgbaWhite(white);
}

bool demo_meshbgactive()
{
    return g_graphics.MeshBackgroundScreenActive();
}

float get_time_ms_precise()
{
    static std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsedSeconds = std::chrono::high_resolution_clock::now() - startTime;

    return static_cast<float>(elapsedSeconds.count() * 1000.0f);
}

bool g_paused = false;

// Called from the graphics layer's key handling, which cannot know about the
// debug overlay or the player. Only active when SR_DEBUG is set.
void DemoTogglePause()
{
    g_paused = !g_paused;
    Music::setPaused(g_paused);
    DebugOverlay::SetPaused(g_paused);
    std::fprintf(stderr, "[debug] %s\n", g_paused ? "paused" : "resumed");
}

int demo_vsync(bool updateAudio)
{
    // Hold the frame while paused. The audio is stopped separately by
    // Music::setPaused, so image and music freeze on the same instant and the
    // readout next to them stays true.
    while (g_paused && !demo_wantstoquit())
    {
        g_graphics.WaitWhilePaused();
    }

    if (updateAudio)
    {
        AudioPlayer::Update(true);
    }

    // Frame target: 70 fps by default (the original's pace). SR_FPS overrides it,
    // so a recording can match a 60 Hz display and avoid the 70->60 judder.
    static const float targetFps = [] {
        const char * e = getenv("SR_FPS");
        const int v = e ? atoi(e) : 0;
        return (v > 0) ? static_cast<float>(v) : 70.0f;
    }();
    const float cycle_ms = 1000.0f / targetFps;
    float now = get_time_ms_precise();
    float elapsed = now - g_lastVblank;

    while (elapsed < cycle_ms)
    {
        now = get_time_ms_precise();
        elapsed = now - g_lastVblank;
    }

    g_lastVblank = now;

    DebugOverlay::Tick();
    return 1;
}

void mainLoop()
{
    Music::Song lastMusic = Music::Song::COUNT;

    if (getenv("SR_DEBUG"))
    {
        DebugOverlay::SetEnabled(true);
        std::fprintf(stderr, "[debug] overlay on (SR_DEBUG)\n");
    }


    do
    {
        for (int index = static_cast<int>(g_start); main_parts[index].width; index++)
        {
            if (lastMusic != main_parts[index].music)
            {
                lastMusic = main_parts[index].music;

                int startOrder = main_parts[index].musicStartOrder;
                if (index == static_cast<int>(Part::_10_Plasmacube) && PLZ::g_cubeOnly)
                {
                    // Cube-only mode skips the plasma section of the song, so
                    // start the music at the order where the cube begins (the
                    // full demo enters vect() at order 68).
                    startOrder = 68;
                }

                Music::start(main_parts[index].music, startOrder);
            }

            // Reset per-part rendering state so direct part-launch and full-run
            // always behave identically (bitmap sections default to 4:3).
            g_respectRatio = true;
            g_graphics.ResetPartState();

            // The background layer is a shared cross-part resource: ALKU leaves
            // its hires picture for U2A (and PAM) to draw over. ResetPartState()
            // only clears m_meshBgScreen, not the layer, so without this the
            // picture stays active for the whole demo and is drawn over every
            // later mesh part (PLZ's cube, U2E's city). Clear it here unless the
            // part explicitly inherits it.
            if (!main_parts[index].keepPreviousBackground)
            {
                g_graphics.ClearBackgroundLayer();
            }

            demo_changemode(main_parts[index].width, main_parts[index].height);
            g_graphics.InitBackgroundLayer();

            DebugOverlay::OnPartStart(index, kPartCommand[index]);
            main_parts[index].part();

            if (g_partSkipDelta != 0)
            {
                int nextIndex = index + g_partSkipDelta;
                int maxIndex = 0;
                while (main_parts[maxIndex].width) maxIndex++;
                if (nextIndex < 0) nextIndex = 0;
                if (nextIndex >= maxIndex) nextIndex = maxIndex - 1;
                g_start = static_cast<Part>(nextIndex);
                g_wantsToQuit = false;
                g_partSkipDelta = 0;
                Visu::xit = 0;
                lastMusic = Music::Song::COUNT;
                index = nextIndex - 1;
                continue;
            }

            if (demo_wantstoquit())
            {
                break;
            }

            Shim::finishedDemoFirstPart();

            if (index == static_cast<int>(Part::_07_Panicfake))
            {
                // wait for music between the shutdown / forest parts
                while (!demo_wantstoquit() && Music::getPlusFlags() >= 0)
                {
                    demo_vsync(true);  // advance audio for music sync
                    demo_blit();
                }

                while (!demo_wantstoquit() && Music::getPlusFlags() < 0)
                {
                    demo_vsync(true);  // advance audio for music sync
                    demo_blit();
                }
            }

            // loop
            if (g_looping && index == static_cast<int>(Part::_17_CreditsGreetings))
            {
                lastMusic = Music::Song::COUNT;
                index = -1;
            }
        }

        if (demo_wantstoquit())
        {
            break;
        }

    } while (g_looping);
}

int main(int argc, char * argv[])
{
    const char * helpText =
        "Second Reality: Reawakened - Usage:\n\n"
        "  secondreality [options]\n\n"
        "Start Part:\n"
        "  0    Alkutekstit I\n"
        "  1    Alkutekstit II\n"
        "  2    Alkutekstit III\n"
        "  3    Logo\n"
        "  4    Glenz\n"
        "  5    Dottitunneli\n"
        "  6    Techno (KOE)\n"
        "  7    Panicfake (Shutdown)\n"
        "  8    Vuori Scrolli (Forest)\n"
        "  9    Rotazoomer (Lens)\n"
        "  a    Plasmacube (PLZ)\n"
  "  A    Plasmacube cube only (skip plasma)\n"
        "  b    MiniVectorBalls (Dots)\n"
        "  c    Peilipalloscroll (Water)\n"
        "  d    3D-Sinusfield (Coman)\n"
        "  e    Jellypic (JPLogo)\n"
        "  g    Vector Part II (U2E)\n"
        "  i    Endpictureflash\n"
        "  j    Credits/Greetings\n"
        "  k    End Scrolling\n"
        "  U    Hidden Part (DDStars)\n\n"
        "Display:\n"
        "  W    Window mode (default)\n"
        "  F    Fullscreen mode\n"
        "  H    Hi-res mode (pixel doubling, full 640 width)\n\n"
        "Other:\n"
        "  L    Loop demo\n"
        "  --res NxM   Set render resolution (e.g. --res 1920x1080)\n"
        "  --delay N   Hold the window N seconds before starting (for a capture tool)\n"
        "  --listres   List available resolutions\n"
        "  --help   Show this help\n";

    if (argc > 1)
    {
        bool invalidArg = false;

        for (auto index = 1; index < argc; index++)
        {
            if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0)
            {
                printf("%s", helpText);
                return 0;
            }

            if (strcmp(argv[index], "--listres") == 0)
            {
                printf("Available resolutions:\n");
                for (int i = 0; i < g_resolutionCount; i++)
                {
                    printf("  %s\n", g_availableResolutions[i].label);
                }
                return 0;
            }

            // Accept both "--res 1920x1080" (one argv entry) and
            // "--res" "1920x1080" (two entries, which is how some launchers
            // and shells split the option).
            {
                const char * resSpec = nullptr;
                if (strcmp(argv[index], "--res") == 0)
                {
                    if (index + 1 >= argc)
                    {
                        printf("Missing resolution. Use --res WxH (e.g. --res 1920x1080)\n");
                        return 1;
                    }
                    resSpec = argv[++index];
                }
                else if (strncmp(argv[index], "--res ", 6) == 0)
                {
                    resSpec = argv[index] + 6;
                }

                if (resSpec)
                {
                    int w = 0, h = 0;
                    if (sscanf(resSpec, "%dx%d", &w, &h) == 2 && w > 0 && h > 0)
                    {
                        g_res.width = w;
                        g_res.height = h;
                        g_res.label = "custom";
                        // Size the actual window so --res is honoured: without
                        // this the SDL path maximises the window and g_width /
                        // g_height end up at the desktop size, so --res had no
                        // visible effect.
                        g_windowWidth = w;
                        g_windowHeight = h;
                        g_pixelPerfect = true;
                    }
                    else
                    {
                        printf("Invalid resolution format. Use --res WxH (e.g. --res 1920x1080)\n");
                        return 1;
                    }
                    continue;
                }
            }

            if (strcmp(argv[index], "--capture") == 0)
            {
                if (index + 1 < argc) g_captureRemaining = atoi(argv[index + 1]);
                if (g_captureRemaining < 1) g_captureRemaining = 3;
                index++;
                continue;
            }
            if (strncmp(argv[index], "--capture", 9) == 0)
            {
                g_captureRemaining = atoi(argv[index] + 9);
                if (g_captureRemaining < 1) g_captureRemaining = 3;
                continue;
            }

            // "--delay 10" (or "--delay10"): hold the window open for that many
            // seconds before the demo starts, so a capture tool can be pointed
            // at it.
            {
                const char * delaySpec = nullptr;
                if (strcmp(argv[index], "--delay") == 0)
                {
                    if (index + 1 < argc) delaySpec = argv[++index];
                }
                else if (strncmp(argv[index], "--delay", 7) == 0 && argv[index][7] != '\0')
                {
                    delaySpec = argv[index] + 7;
                }
                if (delaySpec)
                {
                    g_startDelaySeconds = atoi(delaySpec);
                    if (g_startDelaySeconds < 0) g_startDelaySeconds = 0;
                    continue;
                }
            }

            switch (argv[index][0])
            {
                case '0':
                    g_start = Part::_00_AlkutekstitI;
                    break;
                case 'p':
                    g_start = Part::_00_AlkutekstitI;
                    g_alkuScrollOnly = true;
                    break;
                case '1':
                    g_start = Part::_01_AlkutekstitII;
                    break;
                case '2':
                    g_start = Part::_02_AlkutekstitIII;
                    break;
                case '3':
                    g_start = Part::_03_Logo;
                    break;
                case '4':
                    g_start = Part::_04_Glenz;
                    break;
                case '5':
                    g_start = Part::_05_Dottitunneli;
                    break;
                case '6':
                    g_start = Part::_06_Techno;
                    break;
                case '7':
                    g_start = Part::_07_Panicfake;
                    break;
                case '8':
                    g_start = Part::_08_VuoriScrolli;
                    break;
                case '9':
                    g_start = Part::_09_Rotazoomer;
                    break;
                case 'a':
                    g_start = Part::_10_Plasmacube;
                    break;
                case 'A':
                    PLZ::g_cubeOnly = true;
                    g_start = Part::_10_Plasmacube;
                    break;
                case 'b':
                    g_start = Part::_11_MiniVectorBalls;
                    break;
                case 'c':
                    g_start = Part::_12_Peilipalloscroll;
                    break;
                case 'd':
                    g_start = Part::_13_3D_Sinusfield;
                    break;
                case 'e':
                    g_start = Part::_14_Jellypic;
                    break;
                case 'g':
                    g_start = Part::_15_VectorPartII;
                    break;
                case 'i':
                    g_start = Part::_16_Endpictureflash;
                    break;
                case 'j':
                    g_start = Part::_17_CreditsGreetings;
                    break;
                case 'k':
                    g_start = Part::_18_EndScrolling;
                    break;
                case 'u':
                case 'U':
                    g_start = Part::_19_HiddenPart;
                    break;
                case 'l':
                case 'L':
                    g_looping = true;
                    break;
                case 'w':
                case 'W':
                    g_windowMode = true;
                    break;
                case 'f':
                case 'F':
                    g_windowMode = false;
                    break;
                case 'H':
                    g_hires = true;
                    break;
                case 's':
                case 'S':
                    g_smooth = true;
                    break;
                default:
                    invalidArg = true;
                    break;
            }
        }

        if (invalidArg)
        {
            printf("%s", helpText);
            return 0;
        }
    }

    if (!g_graphics.Init(g_windowMode ? Graphics::WindowType::Windowed : Graphics::WindowType::Fullscreen))
    {
        return -3;
    }

    // --delay N: hold the freshly opened (blank) window for N seconds so a
    // capture tool can be pointed at it, then run the demo.
#ifndef __EMSCRIPTEN__
    for (int frame = 0; frame < g_startDelaySeconds * 70; ++frame)
    {
        demo_vsync();
        demo_blit();
    }
#endif

#ifndef __EMSCRIPTEN__
    mainLoop();
#else
    emscripten_set_main_loop(mainLoop, 0, 1);
    emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
#endif

    Music::end();

    g_graphics.Close();

    return 0;
}
