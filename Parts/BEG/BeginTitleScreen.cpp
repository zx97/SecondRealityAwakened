#include "Parts/Common.h"

#include "BeginTitleScreen_Data.h"

namespace Beg
{
    namespace
    {
        Palette pal2{};
        Palette palette{};
    }

    void main()
    {
        Shim::clearScreen();

        for (int index = 0; index < 32; index++)
        {
            demo_vsync(true);
        }

        // The shockwave whites the screen out; keep that white as the backdrop
        // so the 4:3 composite's pillarbox is white instead of black.
        demo_backgroundcolor(1.0f, 1.0f, 1.0f);

        // Set the palette: indices 0..254 are white, index 255 is black. The
        // black index is what zoomer2() (the Glenz "damier") draws its closing
        // bars with, so the title must leave it black before handing over.
        Shim::outp(0x3c8, 0);

        for (int index = 0; index < 255; index++)
        {
            Shim::outp(0x3c9, 63);
            Shim::outp(0x3c9, 63);
            Shim::outp(0x3c9, 63);
        }

        Shim::outp(0x3c9, 0);
        Shim::outp(0x3c9, 0);
        Shim::outp(0x3c9, 0);

        Common::readp(palette, -1, (char *)Data::TitleScreenData);

        for (int y = 0; y < DOUBlE_SCREEN_HEIGHT; y++)
        {
            Common::readp((char *)(Shim::cpuPixels + (unsigned)y * 320U), y, (char *)Data::TitleScreenData);
        }

        // Grow the title in from a small scale while its palette fades up out of
        // white, so the white edges close onto the picture instead of snapping.
        constexpr int FADE_FRAMES = 128;
        constexpr int GROW_FRAMES = 60;
        constexpr float SCALE_START = 0.18f;

        for (int c = 0; c <= FADE_FRAMES; c++)
        {
            const float gt = (c < GROW_FRAMES) ? (float)c / (float)GROW_FRAMES : 1.0f;
            const float ease = gt * gt * (3.0f - 2.0f * gt);
            demo_compositescale(SCALE_START + (1.0f - SCALE_START) * ease);

            for (int index = 0; index < PaletteByteCount - 3; index++)
            {
                pal2[index] = static_cast<char>(((FADE_FRAMES - c) * 63 + palette[index] * c) / FADE_FRAMES);
            }

            demo_vsync();

            Common::setpalarea(pal2, 0, 254);
            demo_blit();
        }

        demo_compositescale(1.0f);
        Common::setpalarea(palette, 0, 254);

        // Black bars close in from the screen edges, replacing the white
        // backdrop and leaving only the picture, before handing over to part 04
        // (whose screen is black). Without this the white side bars snapped to
        // black at the part change.
        {
            static const float IDENT[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            };
            static unsigned char blackTex[4] = { 0, 0, 0, 255 };
            demo_rgbatexture(blackTex, 1, 1);

            const float aspect = demo_windowaspect();
            const float boxHalfW = (4.0f / 3.0f) / aspect; // 4:3 composite half-width, NDC
            constexpr int BAR_FRAMES = 36;

            for (int f = 0; f <= BAR_FRAMES; f++)
            {
                const float t = (float)f / (float)BAR_FRAMES;
                const float inner = -1.0f + t * (1.0f - boxHalfW);

                GpuTexVertex q[8];
                q[0].px = -1.0f; q[0].py = -1.0f; q[0].pz = 0.0f; q[0].u = 0.0f; q[0].v = 0.0f;
                q[1].px = inner; q[1].py = -1.0f; q[1].pz = 0.0f; q[1].u = 1.0f; q[1].v = 0.0f;
                q[2].px = -1.0f; q[2].py = 1.0f;  q[2].pz = 0.0f; q[2].u = 0.0f; q[2].v = 1.0f;
                q[3].px = inner; q[3].py = 1.0f;  q[3].pz = 0.0f; q[3].u = 1.0f; q[3].v = 1.0f;
                q[4].px = -inner; q[4].py = -1.0f; q[4].pz = 0.0f; q[4].u = 0.0f; q[4].v = 0.0f;
                q[5].px = 1.0f;   q[5].py = -1.0f; q[5].pz = 0.0f; q[5].u = 1.0f; q[5].v = 0.0f;
                q[6].px = -inner; q[6].py = 1.0f;  q[6].pz = 0.0f; q[6].u = 0.0f; q[6].v = 1.0f;
                q[7].px = 1.0f;   q[7].py = 1.0f;  q[7].pz = 0.0f; q[7].u = 1.0f; q[7].v = 1.0f;

                demo_drawrgbaquad(q, 2, IDENT);
                demo_vsync();
                demo_blit();
            }
        }
    }
}