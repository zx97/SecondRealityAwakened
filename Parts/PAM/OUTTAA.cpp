#include "Parts/Common.h"

#include "OUTTAA_Data.h"
#include "Parts/ALKU/AlkuHzpicData.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace OUTTA
{
    namespace Data
    {
        const unsigned char basePalette[] = {
            0,  0,  0,  2,  3,  5,  5,  5,  7,  6,  6,  7,  7,  7,  8,  8,  8,  10, 10, 10, 11, 11, 11, 13, 12, 12, 14, 13, 13, 15, 15, 15, 17, 16, 16, 18, 13,
            15, 20, 11, 13, 18, 9,  11, 16, 8,  9,  14, 7,  8,  12, 5,  6,  10, 4,  4,  8,  3,  3,  6,  2,  2,  5,  1,  1,  3,  10, 0,  8,  9,  0,  8,  8,  0,
            8,  7,  0,  8,  6,  0,  8,  6,  0,  7,  5,  0,  7,  4,  0,  7,  3,  0,  6,  2,  0,  6,  2,  0,  5,  1,  0,  5,  1,  0,  4,  0,  0,  4,  0,  0,  3,
            6,  3,  6,  7,  4,  7,  9,  6,  9,  11, 8,  11, 13, 11, 13, 0,  63, 0,  63, 63, 63, 21, 21, 21, 3,  1,  1,  7,  2,  1,  7,  3,  10, 9,  4,  3,  9,
            5,  12, 9,  6,  7,  9,  8,  16, 9,  11, 18, 11, 7,  4,  11, 8,  9,  11, 12, 21, 11, 13, 16, 12, 4,  1,  12, 9,  17, 13, 10, 11, 13, 14, 23, 14, 6,
            3,  14, 8,  6,  14, 11, 20, 14, 17, 22, 14, 17, 26, 16, 6,  0,  16, 10, 8,  16, 11, 12, 16, 15, 20, 16, 19, 24, 16, 19, 28, 17, 8,  3,  17, 13, 15,
            17, 14, 23, 17, 16, 27, 17, 18, 21, 17, 22, 30, 18, 14, 12, 18, 21, 26, 19, 9,  6,  19, 11, 9,  19, 18, 29, 19, 22, 33, 20, 8,  1,  20, 15, 15, 20,
            17, 18, 20, 18, 23, 20, 23, 29, 20, 25, 34, 21, 13, 11, 21, 20, 31, 21, 21, 24, 22, 10, 3,  22, 12, 6,  22, 16, 12, 22, 18, 15, 22, 25, 31, 22, 26,
            37, 23, 15, 9,  23, 18, 19, 23, 22, 21, 23, 22, 27, 23, 27, 34, 23, 29, 39, 24, 23, 24, 25, 10, 1,  25, 13, 5,  25, 16, 14, 25, 19, 16, 25, 20, 23,
            25, 24, 29, 25, 30, 42, 26, 14, 8,  26, 17, 10, 26, 20, 19, 26, 25, 32, 26, 26, 26, 26, 28, 36, 27, 19, 13, 27, 22, 25, 27, 24, 22, 27, 32, 44, 28,
            12, 0,  28, 14, 3,  28, 21, 16, 28, 23, 19, 28, 26, 29, 28, 29, 31, 28, 31, 39, 28, 34, 47, 29, 15, 7,  29, 16, 11, 29, 25, 25, 29, 29, 35, 30, 19,
            10, 30, 21, 13, 30, 25, 21, 30, 28, 27, 30, 33, 43, 30, 36, 49, 31, 13, 1,  31, 15, 4,  31, 20, 17, 31, 22, 20, 31, 27, 30, 31, 31, 30, 31, 31, 37,
            32, 17, 7,  32, 23, 16, 32, 27, 24, 32, 29, 33, 32, 33, 34, 32, 33, 40, 33, 20, 11, 33, 24, 22, 33, 26, 27, 33, 34, 37, 33, 36, 45, 34, 14, 0,  34,
            16, 3,  34, 21, 14, 34, 25, 18, 34, 29, 29, 34, 32, 31, 35, 18, 8,  35, 22, 9,  35, 27, 22, 35, 29, 25, 35, 31, 34, 35, 35, 34, 35, 36, 39, 36, 23,
            12, 36, 32, 28, 36, 38, 47, 37, 23, 16, 37, 25, 20, 37, 28, 28, 37, 33, 32, 37, 33, 36, 37, 37, 36, 37, 37, 42, 38, 19, 6,  38, 26, 15, 38, 27, 24,
            38, 28, 19, 38, 31, 25, 39, 17, 2,  39, 21, 9,  39, 31, 29, 39, 35, 34, 39, 38, 39, 39, 40, 43, 40, 23, 12, 40, 29, 22, 40, 34, 31, 40, 40, 47, 41,
            27, 16, 41, 32, 26, 41, 36, 37, 41, 41, 40, 42, 21, 6,  42, 26, 12, 42, 29, 19, 42, 42, 44, 43, 23, 9,  43, 30, 24, 43, 33, 29, 43, 35, 32, 43, 39,
            38, 44, 31, 21, 44, 33, 25, 44, 37, 35, 44, 40, 41, 44, 43, 47, 44, 45, 50, 45, 25, 11, 45, 27, 15, 45, 30, 17, 45, 36, 27, 45, 43, 42, 46, 36, 31,
            46, 40, 35, 46, 45, 45, 47, 32, 19, 47, 32, 23, 47, 39, 38, 47, 48, 50, 48, 27, 12, 48, 29, 15, 48, 35, 24, 48, 35, 28, 48, 39, 32, 48, 42, 37, 48,
            42, 41, 48, 47, 47, 49, 34, 21, 49, 43, 44, 50, 32, 17, 50, 37, 30, 50, 49, 49, 51, 29, 13, 51, 36, 25, 51, 39, 34, 51, 46, 44, 52, 42, 36, 53, 38,
            29, 53, 51, 50, 54, 46, 39, 54, 47, 43, 54, 53, 53, 55, 41, 34, 56, 56, 56, 57, 44, 34, 57, 55, 53, 59, 58, 56, 255
        };
    }

    namespace
    {
        char pam_pal[COUNT(Data::basePalette) + PaletteByteCount * 64];

        // GLSL's smoothstep, which C++ does not provide.
        float smoothstep01(float edge0, float edge1, float x)
        {
            const float t = (x - edge0) / (edge1 - edge0);
            const float c = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
            return c * c * (3.0f - 2.0f * c);
        }
    }

    // Unit torus, generated once: major radius 1 in the XY plane, tube radius
    // TUBE_R around it. tubeDir points from the tube centre out through the
    // surface so the shader can drop the inner wall, and theta/phi are kept per
    // vertex to drive the plasma pattern without any per-frame CPU work.
    constexpr float TUBE_R = 0.30f;
    // Density is tied to the vertex displacement in Graphics.cpp: a coarser grid
    // aliases the relief into noise. 15360 vertices stays inside unsigned short.
    constexpr int RING_SEGS = 320;
    constexpr int TUBE_SEGS = 48;

    void build_torus(std::vector<GpuTorusVertex> & vb, std::vector<unsigned short> & ib)
    {
        vb.clear();
        ib.clear();

        for (int i = 0; i <= RING_SEGS; ++i)
        {
            const float th = (float)(i % RING_SEGS) * (2.0f * 3.14159265f / (float)RING_SEGS);
            const float ct = std::cos(th), st = std::sin(th);
            for (int j = 0; j < TUBE_SEGS; ++j)
            {
                const float ph = (float)j * (2.0f * 3.14159265f / (float)TUBE_SEGS);
                const float cp = std::cos(ph), sp = std::sin(ph);

                GpuTorusVertex v{};
                // The ring lies in the XZ plane and the tube is measured along Y,
                // so the camera sees it edge-on as a horizontal band. Laying it
                // in XY instead made it read as a donut seen face-on from above.
                // Ring centre at (ct, 0, st); the tube grows radially and upward.
                v.px = ct * (1.0f + TUBE_R * cp);
                v.py = TUBE_R * sp;
                v.pz = st * (1.0f + TUBE_R * cp);
                // Outward surface normal: radial*cos(phi) + up*sin(phi).
                v.nx = cp * ct;
                v.ny = sp;
                v.nz = cp * st;
                // Radial direction from the ring axis (now horizontal), NOT the
                // tube-to-surface direction: dot(normal, tubeDir) is 1
                // everywhere, which made the inner-wall discard dead code.
                // dot(normal, radial) is cos(phi): +1 outer skin, -1 inner wall.
                v.tx = ct;
                v.ty = 0.0f;
                v.tz = st;
                v.theta = th;
                v.phi = ph;
                vb.push_back(v);
            }
        }

        for (int i = 0; i < RING_SEGS; ++i)
        {
            for (int j = 0; j < TUBE_SEGS; ++j)
            {
                const int j2 = (j + 1) % TUBE_SEGS;
                const unsigned short a = (unsigned short)(i * TUBE_SEGS + j);
                const unsigned short b = (unsigned short)(i * TUBE_SEGS + j2);
                const unsigned short c = (unsigned short)((i + 1) * TUBE_SEGS + j);
                const unsigned short d = (unsigned short)((i + 1) * TUBE_SEGS + j2);
                ib.push_back(a); ib.push_back(c); ib.push_back(b);
                ib.push_back(b); ib.push_back(c); ib.push_back(d);
            }
        }
    }

    // Fireball: a single quad drawn in sprite mode. aPos holds the local corner
    // and aPlasma repeats it, so the fragment shader can measure the distance
    // from the centre and shape the ball. The explosion point itself arrives as
    // a uniform, so the same quad can be reused anywhere.
    void build_fireball(std::vector<GpuTorusVertex> & vb, std::vector<unsigned short> & ib)
    {
        static const float corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
        vb.clear();
        ib.clear();
        for (int i = 0; i < 4; ++i)
        {
            const float lx = corners[i][0], ly = corners[i][1];
            GpuTorusVertex v{};
            v.px = lx; v.py = ly; v.pz = 0.0f;
            v.nx = lx; v.ny = ly; v.nz = 1.0f;
            v.tx = 0.0f; v.ty = 0.0f; v.tz = 0.0f;
            v.theta = lx;   // local x, reused as the radial coordinate
            v.phi = ly;     // local y
            vb.push_back(v);
        }
        ib.push_back(0); ib.push_back(1); ib.push_back(2);
        ib.push_back(0); ib.push_back(2); ib.push_back(3);
    }

    void main()
    {
        CLEAR(pam_pal);

        memcpy(pam_pal, Data::basePalette, sizeof(Data::basePalette));

        int f{};
        unsigned b{};

        if (!Shim::isDemoFirstPart())
        {
            while (Music::sync() < 10 && !demo_wantstoquit())
            {
                AudioPlayer::Update(true);
            }
        }

        Shim::clearScreen();

        for (int a = 1; a < 64; a++)
        {
            for (b = 0; b < PaletteByteCount; b++)
            {
                pam_pal[a * PaletteByteCount + b] = static_cast<char>((63 * a + (64 - a) * pam_pal[b]) / 64);
            }
        }

        // The hires picture stops scrolling at the end of the previous part, but
        // g_screen32 still holds that last frame, so asking for the screen
        // composite here keeps it behind the shockwave. main.cpp clears the flag
        // for every part, so without this the explosion plays on bare black.
        // 4:3 is refused too: it would pillarbox the picture while the ring
        // itself fills the whole window.
        // Test mode. Launched on its own, part 02 has no background at all: the
        // hires picture lives in the background layer, which only the previous
        // part fills, and MeshBackgroundScreen is skipped while that layer is
        // inactive. The ring only misbehaves over a real background, so SR_BGTEST
        // installs the same layer ALKU does, through the same calls, with the same
        // panorama. That also reproduces the alpha a full run would leave behind.
        if (std::getenv("SR_BGTEST"))
        {
            static unsigned char * rgb = nullptr;
            static int w = 0, h = 0;
            if (!rgb)
            {
                rgb = demo_loadpng_rgb_mem(Alku::Data::hzpic_bg_png, (int)Alku::Data::hzpic_bg_png_size, &w, &h);
                if (rgb) demo_backgroundtexture(rgb, w, h);
            }
            if (rgb && w > 0 && h > 0)
            {
                const float imgAspect = (float)w / (float)h;
                const float winAspect = demo_windowaspect();
                const bool wide = imgAspect > winAspect;
                const float spanU = wide ? winAspect / imgAspect : 1.0f;
                demo_backgroundscroll(0.5f - spanU * 0.5f, 0.5f + spanU * 0.5f, 1.0f,
                                     wide ? imgAspect / winAspect : 1.0f);
            }
        }

        g_respectRatio = false;
        demo_meshbackgroundscreen();

        // No geometry of its own, so the mesh pass would be skipped and the
        // shockwave never flushed.
        demo_forcemeshpass(true);

        // The shockwave replaces the old RLE frames: a static torus built once,
        // then only uniforms change per frame. Fireball first, then the ring
        // opening out of it and coming at the camera, then the white flash that
        // hands over to the next part.
        static std::vector<GpuTorusVertex> tverts, fverts;
        static std::vector<unsigned short> tidx, fidx;
        static bool tbuilt = false;
        if (!tbuilt)
        {
            build_torus(tverts, tidx);
            build_fireball(fverts, fidx);
            tbuilt = true;
        }

        // The ship faded out near the vanishing point, so the fireball opens
        // there: small and far, then the ring takes over from the same spot.
        constexpr float FIRE_X = 0.0f;
        constexpr float FIRE_Y = 0.06f;

        constexpr int FIRE_FRAMES = 26;   // expanding fireball
        constexpr int RING_FRAMES = 118;  // shockwave travelling to the camera
        constexpr int FLASH_FRAMES = 10;  // white blowout
        constexpr int TOTAL = FIRE_FRAMES + RING_FRAMES + FLASH_FRAMES;

        float grow = 0.0f;
        float spin = 0.0f;
        float alpha = 1.0f;
        float white = 0.0f;

        while (!demo_wantstoquit() && f++ < TOTAL)
        {
            const int fr = f - 1;

            if (fr < FIRE_FRAMES)
            {
                // Fireball at the vanishing point: swells quickly, then holds
                // bright while the shockwave forms inside it.
                const float t = (float)fr / (float)FIRE_FRAMES;
                grow = 0.030f + 0.115f * t;
                spin = (float)fr * 0.34f;
                alpha = 0.30f + 0.70f * t;
                if (!fidx.empty() && alpha > 0.01f)
                    demo_drawexplosion(fverts.data(), (int)fverts.size(), fidx.data(), (int)fidx.size(),
                                       grow, spin, alpha, true, FIRE_X, FIRE_Y);
            }
            else if (fr < FIRE_FRAMES + RING_FRAMES)
            {
                // Shockwave: opens slowly at first, then accelerates as it nears.
                const float t = (float)(fr - FIRE_FRAMES) / (float)RING_FRAMES;
                // The band is TUBE_R thick either side, so it covers the window
                // height once TUBE_R * grow reaches 1. Carried past that so the
                // coverage, not an abrupt cut, is what ends the shockwave.
                grow = 0.130f + 4.37f * t * t;
                spin = 6.0f + (float)(fr - FIRE_FRAMES) * 0.16f;
                alpha = 1.0f;

                // The fireball stays alive under the opening ring and is queued
                // first, so the torus grows out of the blast instead of cutting
                // to it: queued second, its near half wins the depth test and
                // covers the ball while the far half leaves it glowing through.
                // Dropping it dead at FIRE_FRAMES read as the explosion being
                // wiped out by the background the instant the ring appeared.
                const float ballFade = 1.0f - smoothstep01(0.0f, 0.40f, t);
                if (!fidx.empty() && ballFade > 0.01f)
                    demo_drawexplosion(fverts.data(), (int)fverts.size(), fidx.data(), (int)fidx.size(),
                                       0.145f, spin * 0.5f, ballFade, true, FIRE_X, FIRE_Y);

                if (!tidx.empty())
                    demo_drawexplosion(tverts.data(), (int)tverts.size(), tidx.data(), (int)tidx.size(),
                                       grow, spin, alpha, false, FIRE_X, FIRE_Y);
            }
            else
            {
                // Contact with the camera: everything blows out to white.
                const float t = (float)(fr - FIRE_FRAMES - RING_FRAMES) / (float)(FLASH_FRAMES - 1);
                grow = 4.50f;
                spin += 0.16f;

                // Hold the ring at full alpha until the flash is nearly opaque.
                // Fading it linearly alongside white left a window where neither
                // covered the screen and the hires picture showed through between
                // the end of the ring and the end of the blowout. The beam has
                // already filled the tube at this grow, so holding it only closes
                // the gap, it does not add a visible edge.
                alpha = 1.0f - smoothstep01(0.80f, 1.0f, t);
                if (!tidx.empty() && alpha > 0.01f)
                    demo_drawexplosion(tverts.data(), (int)tverts.size(), tidx.data(), (int)tidx.size(),
                                       grow, spin, alpha, false, FIRE_X, FIRE_Y);
            }

            // Whiten once the shockwave has reached the camera: keyed on the
            // ring's own grow value (the ring ramp runs 0.13 -> 4.5), not on a
            // frame number, so it follows the torus coordinates. User-validated
            // window: full white between grow 2.4 and 3.8.
            constexpr float WHITE_GROW_START = 2.40f;
            constexpr float WHITE_GROW_END = 3.80f;
            {
                float u = (grow - WHITE_GROW_START) / (WHITE_GROW_END - WHITE_GROW_START);
                if (u < 0.0f) u = 0.0f;
                if (u > 1.0f) u = 1.0f;
                white = u * u * (3.0f - 2.0f * u);
            }

            // Flash over everything, including the hires picture behind.
            demo_explosionflash(white);

            // No copper2()/copper3() here. They animate the VGA palette, and this
            // part no longer has any CPU content for that to act on: every frame
            // is drawn by the GPU and the hires picture behind is a texture. The
            // only thing the fade did was recolour the background, cycling cop_pal
            // through the banks of pam_pal via wfade[f % 100]. On a populated
            // picture that reads as a tint; on a part launched on its own, where
            // the framebuffer holds a single palette index, it drives the whole
            // screen to black and white in turn, which is the flashing.
            demo_vsync(true);
            demo_blit();
        }

        demo_explosionflash(0.0f);
    }
}
