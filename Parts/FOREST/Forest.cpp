#include "Parts/Common.h"

#include "Forest_Data.h"
#include "ForestFontIdx.h"
#include "ForestHires.h"
#include "ForestSkyMask.h"

namespace Forest
{
    namespace
    {
        constexpr int W = ForestHires::W; // 1533
        constexpr int H = ForestHires::H; // 954

        // Reference framebuffer: the hi-res AI background plus the accumulated
        // water-reflection glyphs. Never reset per frame (persistence mirrors
        // the original Shim::cpuPixels behaviour).
        unsigned char s_ref[W * H * 4];
        // Output framebuffer: s_ref dimmed by the per-pixel fade factors.
        unsigned char s_out[W * H * 4];

        const float SCALE_X = (float)W / (float)SCREEN_WIDTH;
        const float SCALE_Y = (float)H / (float)SCREEN_HEIGHT;

        void resetRefFromHiresBackground()
        {
            for (int i = 0; i < W * H; ++i)
            {
                s_ref[i * 4 + 0] = ForestHires::RGB[i * 3 + 0];
                s_ref[i * 4 + 1] = ForestHires::RGB[i * 3 + 1];
                s_ref[i * 4 + 2] = ForestHires::RGB[i * 3 + 2];
                s_ref[i * 4 + 3] = 255;
            }
        }

        // Inverse homography mapping a 320x200 screen point (dx,dy) back to
        // font window coordinates (col,row). Fitted from the dest positions:
        // the scroller is a perspective projection of the text plane onto the
        // water surface. Rendering is a backward mapping (one source lookup
        // per hi-res pixel) so the glyphs stay smooth instead of blocky.
        constexpr float H00 =  0.132070607f;
        constexpr float H01 = -0.0971648703f;
        constexpr float H02 =  19.1010054f;
        constexpr float H10 =  0.105926791f;
        constexpr float H11 =  0.216178914f;
        constexpr float H12 = -28.5904546f;
        constexpr float H20 = -0.000493496149f;
        constexpr float H21 =  0.00521577381f;
        constexpr float H22 =  0.204121568f;

        // Stamp the reflection at a single hi-res pixel. ia is the font
        // intensity (0..255). The reflection brightens the hi-res background
        // color toward white by an amount proportional to ia, attenuated by a
        // smooth sky mask so the text stays hidden behind the foliage. This
        // avoids the banding of the original 256-entry palette.
        void stampRefPixel(int px, int py, int ia)
        {
            int d = (py * W + px) * 4;
            int s = (py * W + px) * 3;

            if (ia <= 0)
            {
                s_ref[d + 0] = ForestHires::RGB[s + 0];
                s_ref[d + 1] = ForestHires::RGB[s + 1];
                s_ref[d + 2] = ForestHires::RGB[s + 2];
                s_ref[d + 3] = 255;
                return;
            }

            unsigned char sky = ForestSkyMask::M[py * W + px];
            if (sky == 0)
            {
                s_ref[d + 0] = ForestHires::RGB[s + 0];
                s_ref[d + 1] = ForestHires::RGB[s + 1];
                s_ref[d + 2] = ForestHires::RGB[s + 2];
                s_ref[d + 3] = 255;
                return;
            }

            float t = ia * (1.0f / 255.0f) * (sky * (1.0f / 255.0f)) * 0.45f;
            s_ref[d + 0] = (unsigned char)(ForestHires::RGB[s + 0] + (255.0f - ForestHires::RGB[s + 0]) * t);
            s_ref[d + 1] = (unsigned char)(ForestHires::RGB[s + 1] + (255.0f - ForestHires::RGB[s + 1]) * t);
            s_ref[d + 2] = (unsigned char)(ForestHires::RGB[s + 2] + (255.0f - ForestHires::RGB[s + 2]) * t);
            s_ref[d + 3] = 255;
        }

        // Backward mapping: for every hi-res pixel inside the text bounding
        // box, project it into font space via the inverse homography, sample
        // the bicubic-upscaled font gradient bilinearly, and stamp the
        // reflection (or restore the background). scroll is the subpixel
        // column offset along the scroller.
        void renderText(float scroll)
        {
            int bx0 = 0;
            int bx1 = (int)(270 * SCALE_X) + 1;
            int by0 = (int)(26 * SCALE_Y);
            int by1 = (int)(200 * SCALE_Y) + 1;
            if (bx1 > W) bx1 = W;
            if (by1 > H) by1 = H;

            for (int py = by0; py < by1; ++py)
            {
                float dy = py / SCALE_Y;
                for (int px = bx0; px < bx1; ++px)
                {
                    float dx = px / SCALE_X;
                    float den = H20 * dx + H21 * dy + H22;
                    float col = (H00 * dx + H01 * dy + H02) / den;
                    float row = (H10 * dx + H11 * dy + H12) / den;
                    float colSrc = col + scroll;

                    if (colSrc < 0.0f || colSrc > 639.0f || row < 0.0f || row > 30.0f)
                    {
                        stampRefPixel(px, py, 0);
                        continue;
                    }

                    float fx = colSrc * 9.0f;
                    float fy = row * 9.0f;
                    int x0 = (int)fx;
                    int y0 = (int)fy;
                    float tx = fx - (float)x0;
                    float ty = fy - (float)y0;
                    if (x0 < 0) { x0 = 0; tx = 0.0f; }
                    if (y0 < 0) { y0 = 0; ty = 0.0f; }
                    if (x0 > ForestFontIdx::W - 2) { x0 = ForestFontIdx::W - 2; tx = 1.0f; }
                    if (y0 > ForestFontIdx::H - 2) { y0 = ForestFontIdx::H - 2; ty = 1.0f; }

                    const unsigned char * base = &ForestFontIdx::IDX[y0 * ForestFontIdx::W + x0];
                    float i00 = base[0];
                    float i10 = base[1];
                    float i01 = base[ForestFontIdx::W];
                    float i11 = base[ForestFontIdx::W + 1];
                    float ia = i00 * (1.0f - tx) * (1.0f - ty)
                             + i10 * tx * (1.0f - ty)
                             + i01 * (1.0f - tx) * ty
                             + i11 * tx * ty;
                    stampRefPixel(px, py, (int)(ia + 0.5f));
                }
            }
        }

        void applyFade(int fadeCiel, int fadeFeuilles)
        {
            if (fadeCiel >= 255 && fadeFeuilles >= 255)
            {
                memcpy(s_out, s_ref, (size_t)W * H * 4);
                return;
            }
            for (int i = 0; i < W * H; ++i)
            {
                // Anti-aliased water mask gives a smooth fade instead of a 9px staircase.
                int m = ForestSkyMask::M[i];
                int f = fadeFeuilles + ((fadeCiel - fadeFeuilles) * m) / 255;
                int d = i * 4;
                s_out[d + 0] = (unsigned char)((s_ref[d + 0] * f) >> 8);
                s_out[d + 1] = (unsigned char)((s_ref[d + 1] * f) >> 8);
                s_out[d + 2] = (unsigned char)((s_ref[d + 2] * f) >> 8);
                s_out[d + 3] = 255;
            }
        }

        void drawHiresQuad(float blur)
        {
            static const float IDENT[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            };
            const float aspect = demo_windowaspect();
            constexpr float DOS_4_3 = 4.0f / 3.0f;
            float halfW, halfH;
            if (aspect > DOS_4_3)
            {
                halfH = 1.0f;
                halfW = DOS_4_3 / aspect;
            }
            else
            {
                halfW = 1.0f;
                halfH = aspect / DOS_4_3;
            }
            GpuTexVertex q[4];
            q[0].px = -halfW; q[0].py =  halfH; q[0].pz = 0; q[0].u = 0; q[0].v = 0;
            q[1].px =  halfW; q[1].py =  halfH; q[1].pz = 0; q[1].u = 1; q[1].v = 0;
            q[2].px = -halfW; q[2].py = -halfH; q[2].pz = 0; q[2].u = 0; q[2].v = 1;
            q[3].px =  halfW; q[3].py = -halfH; q[3].pz = 0; q[3].u = 1; q[3].v = 1;
            demo_drawrgbaquad(q, 1, IDENT, blur);
        }
    }

    void main()
    {
        uint8_t fp = 0;
        const uint16_t veke = 2800;
        uint16_t frame = 0;
        int quit = 0;
        int fadeout = 0;

        float scroll = 0.0f;
        int fadeCiel = 0;
        int fadeFeuilles = 0;

        resetRefFromHiresBackground();
        // perFrame: applyFade() rewrites s_out in place below, so the pointer
        // never changes while the pixels do. Without this the upload is cached
        // on the pointer and the part stays black.
        demo_rgbatexture(s_out, W, H, true);

        // Fade-in stage 1: foliage appears, water stays black.
        for (int y = 0; y < 64; ++y)
        {
            demo_vsync();

            fadeFeuilles += 4;
            if (fadeFeuilles > 255) fadeFeuilles = 255;

            applyFade(fadeCiel, fadeFeuilles);
            drawHiresQuad(1.0f);
            demo_blit();
        }

        while (Music::getPlusFlags() > 0)
        {
            demo_vsync(true);  // advance audio for music sync
            applyFade(fadeCiel, fadeFeuilles);
            drawHiresQuad(1.0f);
            demo_blit();
        }

        // Main loop: water and reflection fade in while the text scrolls.
        for (int y = 0; y < 63 * 2; ++y)
        {
            demo_vsync();

            if (y & 1)
            {
                fadeCiel += 4;
                if (fadeCiel > 255) fadeCiel = 255;
            }

            renderText(scroll);
            scroll += 1.0f / 3.0f;
            if (scroll > 640.0f) scroll = 640.0f;

            applyFade(fadeCiel, fadeFeuilles);
            drawHiresQuad(1.0f - (float)fadeCiel / 255.0f);
            demo_blit();
        }

        frame = 0;
        quit = 0;
        fp = 0;
        fadeout = 0;

        do
        {
            demo_vsync();

            if (Music::getPlusFlags() == -11) fadeout = 1;

            if (fadeout)
            {
                if (fp == 64)
                    quit = 1;
                else
                    ++fp;

                if (fadeCiel > 0) fadeCiel -= 4;
                if (fadeFeuilles > 0) fadeFeuilles -= 4;
            }

            renderText(scroll);
            scroll += 1.0f / 3.0f;
            if (scroll > 640.0f) scroll = 640.0f;

            ++frame;

            applyFade(fadeCiel, fadeFeuilles);
            drawHiresQuad(0.0f);
            demo_blit();

        } while (!demo_wantstoquit() && frame != veke && !quit);
    }
}
