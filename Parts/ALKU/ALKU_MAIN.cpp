#include "Parts/Common.h"

#include <vector>
#include <algorithm>
#include "Graphics/VectorFont.h"
#include "ALKU_MAIN_Data.h"
#include "AlkuHzpicData.h"
#include "DolbyLogo.h"

extern bool g_alkuScrollOnly;

namespace Alku
{
    constexpr const int FONT_STRIDE = 1500;
    constexpr const int SCRLF = 9;

    namespace
    {
        const char * alku_fonaorder = "ABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyz0123456789"
                                      "!?,.:\"\"()+-*='\x8f\x99";

        std::vector<char> font;

        unsigned char alku_planar_vram[352 * 500]{};

        Palette palette{};  // pic
        Palette palette2{}; // pic & text
        Palette fuckpal{};
        Palette fade1{}; // black
        Palette fade2{}; // text

        short picin[PaletteByteCount]{};
        short textin[PaletteByteCount]{};
        short textout[PaletteByteCount]{};

        char cfpal[PaletteByteCount * 2]{};

        int alku_fonap[256]{};
        int alku_fonaw[256]{};

        short alku_dtau[30000]{};
        char tbuf[186][352]{};

        int a = 0, p = 0, alku_tptr = 0;

        // SDF title text: stored lines re-emitted every frame via callback.
        struct SdfLine
        {
            char text[128]{};
            int y = 0;
        };
        SdfLine s_sdfLines[8]{};
        int s_sdfLineCount = 0;

        // Dolby logo (SVG) shown in place of the bitmap \x8f/\x99 glyphs.
        bool s_showLogo = false;
        int s_logoCount = 0;

        // Virtual-screen row prtc() handed the logo glyphs; the overlay is
        // placed from it rather than from a hardcoded spot.
        int s_logoY = 0;

        // Screen 3 gate, in module order/row rather than in sync() markers.
        // sync()==3 resolves to order 3 row 0x2f, where the text and the logo
        // led their sound by 2.400 s = 20 rows at Skaven's 125 BPM speed 6, a
        // row being 0.120 s. sync() cannot express a sub-marker row, and delaying
        // the picture instead only leaves the gate already satisfied, which is
        // what put 1.9 s of dead air on screen.
        constexpr unsigned short DOLBY_ORDER = 4;
        constexpr unsigned char DOLBY_ROW = 0x03;
    // Set once the palette fade first leaves black, which is when the old
        // CPU-side picture became visible.
        bool s_bgVisible = false;

        const float IDENT[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
    }

    static void alku_draw_background()
    {
        static unsigned char * rgb = nullptr;
        static int w = 0, h = 0;
        static float startMs = -1.0f;

        if (!rgb)
        {
            rgb = demo_loadpng_rgb_mem(Data::hzpic_bg_png, Data::hzpic_bg_png_size, &w, &h);
            if (rgb) demo_backgroundtexture(rgb, w, h);
        }
        if (!rgb || w <= 0 || h <= 0) return;

        if (!s_bgVisible) return;

        // Driven from the clock, not from this callback: the music waits call
        // demo_blit() without vsync, so a per-draw counter runs at CPU speed.
        const float now = get_time_ms_precise();
        if (startMs < 0.0f) startMs = now;
        const float frames = (now - startMs) * 0.07f;

        const float winAspect = demo_windowaspect();
        const float imgAspect = (float)w / (float)h;

        // The layer draws the picture square-on across the window, so when the picture
        // is wider than the window the slice below is what "cover" means; when it
        // is narrower there is nothing to scroll and the whole width shows.
        const bool wide = imgAspect > winAspect;
        const float halfW = wide ? imgAspect / winAspect : 1.0f;
        const float spanU = wide ? winAspect / imgAspect : 1.0f;

        // u0 grows, which walks the view rightwards through the image, so the
        // picture itself travels right to left.
        const float uMax = 1.0f - spanU;

        // Duration of the drift: the credit block runs 42.07 s from here to the
        // teardown, so the picture comes to rest a second before the ships take
        // over and is still moving right up to the handover.
        constexpr float SWEEP_SECONDS = 41.07f;

        // How far it travels, as a fraction of a full sweep.
        //
        // This must stay at 1.0. The window only shows spanU (0.417) of the
        // 9216 px picture, so the right-hand part of the image is on screen only
        // once u0 has travelled the remaining uMax; stopping short parks the
        // view on the left of the picture for the whole credit block. Duration
        // is the knob for the feel, not this.
        constexpr float SWEEP_FRACTION = 1.0f;

        // Distance and duration are independent on purpose: the complaint that
        // fixed the duration was speed, the one that fixed this was distance,
        // and slowing a full sweep down cannot fix either.
        const float travel = uMax * SWEEP_FRACTION;
        const float du = travel / (SWEEP_SECONDS * 70.0f);
        float u0 = frames * du;
        if (u0 > travel) u0 = travel;

        // Copied into the layer shader rather than sampled from a quad here,
        // because this must land under the SDF text. The visible slice is padded
        // on both sides: the pad covers the overflow, which sits off-screen.
        const float pad = (1.0f - spanU) * 0.5f;
        const float uL = u0 - pad;
        const float uR = u0 + spanU + pad;

        // Fade the picture up out of black as the drift starts. The original did
        // this with cop_dofade on the palette; the layer has its own alpha, and
        // without a ramp the whole landscape snapped on at full brightness the
        // frame s_bgVisible went true. frames is 0 on that first frame, so the
        // ramp starts exactly with the scroll rather than with the part.
        constexpr float FADE_SECONDS = 1.0f;
        const float fadeFrames = FADE_SECONDS * 70.0f;
        const float alpha = frames < fadeFrames ? frames / fadeFrames : 1.0f;

        // The image is drawn square-on across the whole window; the slice above is
        // what "cover" means when the picture is wider than the window.
        demo_backgroundscroll(uL, uR, alpha, halfW);
    }

    static void alku_sdf_render()
    {
        alku_draw_background();

        static const char * set = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?,.:'()+- ";
        static bool built = false;
        if (!built)
        {
            int aw = 0, ah = 0, apad = 0;
            const unsigned char * atlas = VectorFont::BuildSdfAtlas(set, 56, aw, ah, apad);
            demo_sdftexture(atlas, aw, ah);
            built = true;
        }

        static GpuTexVertex quads[8 * 128 * 4];
        int quadCount = 0;
        const float aspect = demo_windowaspect();
        const float fontNdc = 0.12f;

        if (s_sdfLineCount > 0)
        {
            // Centre the block as a whole on the window, then honour the row each
            // line asked for relative to the block. Rows used to be divided by the
            // 320x400 buffer height, which put the text off-centre and made it move
            // when the window was resized; the window is the reference now.
            // The logo is placed straight from the row prtc() asked for, so the
            // "in" beside it has to use that same mapping. Centring the line on
            // the window instead pulled it down onto the logo's lower edge.
            float shift = 0.0f;
            if (!s_showLogo)
            {
                float top = 1.0f, bottom = -1.0f;
                for (int i = 0; i < s_sdfLineCount; ++i)
                {
                    top = std::min(top, 1.0f - (float)s_sdfLines[i].y / 200.0f);
                    bottom = std::max(bottom, 1.0f - (float)(s_sdfLines[i].y + 32) / 200.0f);
                }
                shift = -(top + bottom) * 0.5f;
            }

            for (int i = 0; i < s_sdfLineCount && quadCount < 8 * 128; ++i)
            {
                const float baseY = 1.0f - (float)(s_sdfLines[i].y + 16) / 200.0f + shift;
                int n = VectorFont::BuildTextQuads(s_sdfLines[i].text, quads + quadCount * 4, 128, baseY, fontNdc, aspect, 1.25f);
                quadCount += n;
            }

            if (quadCount > 0)
                demo_drawsdftext(quads, quadCount, IDENT, 1.0f, 1.0f, 1.0f);
        }

        // No fade on purpose: the word "in" beside it is an SDF overlay, and
        // demo_drawsdftext() has no alpha, so it pops in at full brightness too.
        // Fading the logo alone is what pulled the two apart.
        if (s_showLogo)
        {
            static bool logoTex = false;
            if (!logoTex)
            {
                static unsigned char rgba[DolbyLogo::W * DolbyLogo::H * 4];
                for (int i = 0; i < DolbyLogo::W * DolbyLogo::H; ++i)
                {
                    rgba[i * 4 + 0] = 255;
                    rgba[i * 4 + 1] = 255;
                    rgba[i * 4 + 2] = 255;
                    rgba[i * 4 + 3] = DolbyLogo::Alpha[i];
                }
                demo_rgbatexture(rgba, DolbyLogo::W, DolbyLogo::H);
                logoTex = true;
            }

            GpuTexVertex lq[4];
            // NDC x and y map to different pixel densities (window aspect), so
            // the quad height must scale by windowAspect to keep the source
            // image's true aspect ratio on screen.
            const float winAspect = demo_windowaspect();
            const float lw = 0.6f;
            const float lh = lw * winAspect * (float)DolbyLogo::H / (float)DolbyLogo::W;
            // Same mapping the SDF text applies to its rows, so the logo centres
            // on the row prtc() asked for.
            const float cy = 1.0f - (float)(s_logoY + 16) / 200.0f;
            GpuTexVertex * q = lq;
            q[0].px = -lw / 2; q[0].py = cy + lh / 2; q[0].pz = 0; q[0].u = 0; q[0].v = 0;
            q[1].px =  lw / 2; q[1].py = cy + lh / 2; q[1].pz = 0; q[1].u = 1; q[1].v = 0;
            q[2].px = -lw / 2; q[2].py = cy - lh / 2; q[2].pz = 0; q[2].u = 0; q[2].v = 1;
            q[3].px =  lw / 2; q[3].py = cy - lh / 2; q[3].pz = 0; q[3].u = 1; q[3].v = 1;
            demo_drawrgbaquad(lq, 1, IDENT);
        }
    }

    void ascrolltext(unsigned short scrl, const void * text)
    {
        const size_t BASE = 100u * 352u;

        for (const unsigned short * pText = (const unsigned short *)text;;)
        {
            for (int i = 0; i < 20; ++i)
            {
                unsigned short idx = pText[0];
                if (idx == 0xFFFFu)
                {
                    return;
                }

                const size_t offset = BASE + (size_t)scrl + (size_t)idx;

                unsigned short val = pText[1];

                alku_planar_vram[offset - 1] ^= (unsigned char)(val & 0xFF);

                pText += 2;
            }
        }
    }

    void outline(const void * src, void * dest)
    {
        const size_t SRC_STRIDE = 640u;
        const size_t DST_STRIDE = 352u * 2u;
        const size_t LINES = 75u;
        const size_t BLOCK_SHIFT = LINES * SRC_STRIDE;
        const size_t DST_BLOCK = LINES * DST_STRIDE;

        const unsigned char * srcBase = (const unsigned char *)src;
        unsigned char * destBase = (unsigned char *)dest;

        for (int offset = 4; offset >= 1; --offset)
        {
            const unsigned char * currentSrc = srcBase + offset;
            unsigned char * currentDest = destBase + offset;

            for (size_t ccc = 0; ccc < LINES; ++ccc)
            {
                currentDest[ccc * DST_STRIDE] = currentSrc[ccc * SRC_STRIDE];
            }

            currentSrc += BLOCK_SHIFT;

            for (size_t ccc = 0; ccc < LINES; ++ccc)
            {
                currentDest[DST_BLOCK + ccc * DST_STRIDE] = currentSrc[ccc * SRC_STRIDE];
            }
        }
    }

    void alku_simulate_scroll()
    {
        for (int y = 0; y < DOUBlE_SCREEN_HEIGHT; y++)
        {
            memcpy(Shim::cpuPixels + y * SCREEN_WIDTH, alku_planar_vram + Common::cop_start * 4 + Common::cop_scrl + y * 352, SCREEN_WIDTH);
        }
    }

    void alku_init()
    {
        int b, x, y;

        Shim::clearScreen();

        Common::setpalarea(fade1, 0, PaletteColorCount);

        memcpy(palette, Data::hzpic + 16, PaletteByteCount);

        // The picture itself is no longer unpacked here: it comes from the GPU
        // background layer, so leaving it out keeps it from being composited over
        // the hires image. The palette still comes from hzpic, and the credit
        // lines still XOR into the same planar buffer.
        alku_simulate_scroll();

        font.assign(FONT_STRIDE * 31 + 1500 * 5 * 2, 0);

        VectorFont::GenerateAlkuFont(font.data(), (int)font.size());

        for (y = 0; y < 32; y++)
        {
            for (a = 0; a < FONT_STRIDE; a++)
            {
                switch (font[y * FONT_STRIDE + a] & 3)
                {
                    case 0x1:
                        b = 0x40;
                        break;
                    case 0x2:
                        b = 0x80;
                        break;
                    case 0x3:
                        b = 0xc0;
                        break;
                    default:
                        b = 0;
                }

                font[y * FONT_STRIDE + a] = static_cast<char>(b);
            }
        }

        for (y = 0; y < PaletteByteCount; y += 3)
        {
            if (y < 64 * 3)
            {
                palette2[y + 0] = palette[y + 0];
                palette2[y + 1] = palette[y + 1];
                palette2[y + 2] = palette[y + 2];
            }
            else if (y < 128 * 3)
            {
                palette2[y + 0] = ((fade2[y + 0] = palette[0x1 * 3 + 0]) * 63 + palette[y % (64 * 3) + 0] * (63 - palette[0x1 * 3 + 0])) >> 6;
                palette2[y + 1] = ((fade2[y + 1] = palette[0x1 * 3 + 1]) * 63 + palette[y % (64 * 3) + 1] * (63 - palette[0x1 * 3 + 1])) >> 6;
                palette2[y + 2] = ((fade2[y + 2] = palette[0x1 * 3 + 2]) * 63 + palette[y % (64 * 3) + 2] * (63 - palette[0x1 * 3 + 2])) >> 6;
            }
            else if (y < 192 * 3)
            {
                palette2[y + 0] = ((fade2[y + 0] = palette[0x2 * 3 + 0]) * 63 + palette[y % (64 * 3) + 0] * (63 - palette[0x2 * 3 + 0])) >> 6;
                palette2[y + 1] = ((fade2[y + 1] = palette[0x2 * 3 + 1]) * 63 + palette[y % (64 * 3) + 1] * (63 - palette[0x2 * 3 + 1])) >> 6;
                palette2[y + 2] = ((fade2[y + 2] = palette[0x2 * 3 + 2]) * 63 + palette[y % (64 * 3) + 2] * (63 - palette[0x2 * 3 + 2])) >> 6;
            }
            else
            {
                palette2[y + 0] = ((fade2[y + 0] = palette[0x3 * 3 + 0]) * 63 + palette[y % (64 * 3) + 0] * (63 - palette[0x3 * 3 + 0])) >> 6;
                palette2[y + 1] = ((fade2[y + 1] = palette[0x3 * 3 + 1]) * 63 + palette[y % (64 * 3) + 1] * (63 - palette[0x3 * 3 + 1])) >> 6;
                palette2[y + 2] = ((fade2[y + 2] = palette[0x3 * 3 + 2]) * 63 + palette[y % (64 * 3) + 2] * (63 - palette[0x3 * 3 + 2])) >> 6;
            }
        }

        for (a = 192; a < PaletteByteCount; a++)
        {
            palette[a] = palette[a - 192];
        }

        for (x = 0; x < FONT_STRIDE && *alku_fonaorder;)
        {
            while (x < FONT_STRIDE)
            {
                for (y = 0; y < 32; y++)
                    if (font[y * FONT_STRIDE + x]) break;

                if (y != 32) break;

                x++;
            }

            b = x;

            while (x < FONT_STRIDE)
            {
                for (y = 0; y < 32; y++)
                    if (font[y * FONT_STRIDE + x]) break;

                if (y == 32) break;

                x++;
            }

            alku_fonap[(unsigned char)*alku_fonaorder] = b;
            alku_fonaw[(unsigned char)*alku_fonaorder] = x - b;

            alku_fonaorder++;
        }

        alku_fonap[32] = FONT_STRIDE - 20;
        alku_fonaw[32] = 16;

        for (a = 0; a < PaletteByteCount; a++)
        {
            textin[a] = (palette2[a] - palette[a]) * 256 / 64;
            textout[a] = (palette[a] - palette2[a]) * 256 / 64;
            picin[a] = (palette[a] - fade1[a]) * 256 / 128;
        }
    }

    void wait(int t)
    {
        for (int i = 0; i < t; i++)
        {
            if (demo_wantstoquit()) break;

            demo_vsync();
            demo_blit();
        }
    }

    void fonapois()
    {
        unsigned char * vmem = Shim::cpuPixels;

        for (unsigned int index = SCREEN_WIDTH * 64; index < 320U * (64 + 256); index++)
        {
            vmem[index] &= 63;
        }

        s_sdfLineCount = 0;
        s_showLogo = false;
        s_logoCount = 0;
        s_logoY = 0;
    }

    void prt(int x, int y, const char * txt)
    {
        (void)x;

        // Dolby logo glyphs (\x8f/\x99) are shown as the SVG logo instead of
        // the (unavailable) bitmap glyphs in the generated font.
        for (const char * s = txt; *s; ++s)
        {
            unsigned char c = (unsigned char)*s;
            if (c == 0x8f || c == 0x99)
            {
                // Both glyphs are halves of one logo, so the first fixes where
                // it goes and the second only advances the count.
                if (!s_logoCount)
                {
                    s_showLogo = true;
                    s_logoY = y;
                }
                ++s_logoCount;
                return;
            }
        }

        // SDF path: store the line; alku_sdf_render() emits quads every frame.
        if (s_sdfLineCount < static_cast<int>(sizeof(s_sdfLines) / sizeof(s_sdfLines[0])))
        {
            SdfLine & line = s_sdfLines[s_sdfLineCount++];
            line.y = y;
            int n = 0;
            for (; txt[n] && n + 1 < static_cast<int>(sizeof(line.text)); ++n)
                line.text[n] = txt[n];
            line.text[n] = 0;
        }
        return;
    }

    void prtc(int x, int y, const char * txt)
    {
        int w = 0;
        const char * t = txt;

        while (*t)
            w += alku_fonaw[(unsigned char)*t++] + 2;

        prt(x - w / 2, y, txt);
    }

    void dofade(char * pal1, char * pal2)
    {
        static Palette pal;

        for (int index = 0; index < 64 && !demo_wantstoquit(); index++)
        {
            for (int b = 0; b < PaletteByteCount; b++)
                pal[b] = static_cast<char>(((pal1[b] * (64 - index) + pal2[b] * index) >> 6));

            Common::cop_pal = pal;

            Common::do_pal = 1;

            Common::copper2();
            Common::copper3();

            demo_vsync();
            demo_blit();
        }
    }

    int fdofade(char * pal1, char * pal2, int lerpValue)
    {
        if (lerpValue < 0 || lerpValue > 64) return 0;

        for (int b = 0; b < PaletteByteCount; b++)
            fuckpal[b] = static_cast<char>((pal1[b] * (64 - lerpValue) + pal2[b] * lerpValue) >> 6);

        Common::cop_pal = fuckpal;

        Common::do_pal = 1;

        return 0;
    }

    void addtext(int tx, int ty, const char * txt)
    {
        int w = 0;
        const char * t = txt;

        while (*t)
            w += alku_fonaw[(unsigned char)*t++] + 2;

        t = txt;
        w /= 2;
        while (*t)
        {
            for (int x = 0; x < alku_fonaw[(unsigned char)*t]; x++)
                for (int y = 0; y < 31; y++)
                    tbuf[y + ty][tx + x - w] = font[y * FONT_STRIDE + alku_fonap[(unsigned char)*t] + x];

            tx += alku_fonaw[(unsigned char)*t++] + 2;
        }
    }

    int alku_do_scroll(int mode)
    {
        if (mode != 0)
        {
            while (Common::frame_count < SCRLF)
            {
                Common::copper2();
                Common::copper3();

                demo_vsync();
                demo_blit();
            }
        }

        if (Common::frame_count < SCRLF) return 0;

        Common::frame_count -= SCRLF;

        if (mode == 1) ascrolltext(static_cast<short>(a), (int *)alku_dtau);

        Common::cop_start = a / 4;
        Common::cop_scrl = (a & 3);

        // No outline() here either: the scrolling picture is the GPU layer's job now,
        // and unpacking the old bitmap here would paint over it.

        alku_simulate_scroll();

        a += 1;
        p ^= 1;

        return 1;
    }

    void faddtext(int tx, int ty, const char * txt)
    {
        int w = 0;
        const char * t = txt;

        while (*t)
            w += alku_fonaw[(unsigned char)*t++] + 2;

        t = txt;
        w /= 2;

        while (*t)
        {
            for (int x = 0; x < alku_fonaw[(unsigned char)*t]; x++)
                for (int y = 0; y < 32; y++)
                    tbuf[y + ty][tx + x - w] = font[y * FONT_STRIDE + alku_fonap[(unsigned char)*t] + x];

            alku_do_scroll(0);
            tx += alku_fonaw[(unsigned char)*t++] + 2;
        }
    }

    void fmaketext(int scrl)
    {
        unsigned char * vvmem = alku_planar_vram;
        short * p1 = alku_dtau;

        for (int y = 1; y < 184; y++)
            for (int x = SCREEN_WIDTH; x > 0; x--)
                if (tbuf[y][x] != tbuf[y][x - 1])
                {
                    *p1++ = static_cast<short>(x + y * 352);
                    *p1++ = tbuf[y][x] ^ tbuf[y][x - 1];
                }

        *p1++ = -1;
        *p1++ = -1;

        for (int x = 0; x < SCREEN_WIDTH; x++)
        {
            for (int y = 1; y < 184; y++)
            {
                vvmem[y * 352 + 352 * 100 + (x + scrl)] ^= tbuf[y][x];
            }
        }

        while (a <= scrl && !demo_wantstoquit())
        {
            Common::copper2();
            Common::copper3();

            alku_do_scroll(0);

            demo_vsync();
            demo_blit();
        }
    }

    void ffonapois()
    {
        unsigned int * vvmem = (unsigned int *)alku_planar_vram;

        for (unsigned int index = 80 * 64; index < 80U * (64 + 256 + 10); index++)
        {
            vvmem[index] = vvmem[index] & 0x3f3f3f3f;
        }

        alku_do_scroll(0);
    }

    void fffade(char * pal1, char * pal2, int frames)
    {
        for (int index = 0; index < PaletteByteCount; index++)
        {
            cfpal[index] = pal1[index];
            cfpal[index + PaletteByteCount] = static_cast<char>((pal2[index] - pal1[index]) * 256 / frames);
        }
    }

    void main()
    {
        a = {};
        p = {};
        alku_tptr = {};

        g_respectRatio = true;
        // Back in place: removing it did not remove the black flash (it was still
        // there afterwards), so it was never the cause and there was nothing to win.
        demo_meshbackgroundscreen();
        demo_setsdftextcallback(alku_sdf_render);

        alku_init();

        if (g_alkuScrollOnly)
        {
            Music::setSync(4);
            g_alkuScrollOnly = false;
            goto scrollStart;
        }

        while (Music::sync() < 1 && !demo_wantstoquit())
        {
            demo_vsync();
            demo_blit();
        }

        if (demo_wantstoquit())
        {
            return;
        }

        prtc(160, 120, "A");
        prtc(160, 160, "Future Crew");
        prtc(160, 200, "Production");
        dofade(fade1, fade2);
        wait(300);
        dofade(fade2, fade1);
        fonapois();

        while (Music::sync() < 2 && !demo_wantstoquit())
        {
            demo_vsync();
            demo_blit();
        }

        if (demo_wantstoquit())
        {
            return;
        }

        prtc(160, 160, "First Presented");
        prtc(160, 200, "at Assembly 93");
        dofade(fade1, fade2);
        wait(300);
        dofade(fade2, fade1);
        fonapois();

        // Gates on order/row, not sync(): see DOLBY_ORDER / DOLBY_ROW above. Both the
        // text and the logo are armed by this one gate, so they cannot separate.
        while (!demo_wantstoquit())
        {
            const unsigned short ord = Music::getOrder();
            if (ord > DOLBY_ORDER || (ord == DOLBY_ORDER && Music::getRow() >= DOLBY_ROW)) break;

            demo_blit();
        }

        if (demo_wantstoquit())
        {
            return;
        }

        prtc(160, 120, "in");
        prtc(160, 160, "\x8f");
        prtc(160, 179, "\x99");
        dofade(fade1, fade2);
        wait(300);
        dofade(fade2, fade1);
        fonapois();

        while (Music::sync() < 4 && !demo_wantstoquit())
        {
            demo_vsync();
            demo_blit();
        }

        if (demo_wantstoquit())
        {
            return;
        }

        scrollStart:
        // The picture belongs to the credit scroll, not to the opening screens:
        // the original faded it in with cop_dofade here, after screen 3.
        s_bgVisible = true;
        memcpy(Common::fadepal, fade1, PaletteByteCount);
        Common::cop_fadepal = picin;
        Common::cop_dofade = 128;

        int aa{}, f{};

        for (a = 1, p = 1, f = 0, Common::frame_count = 0; Common::cop_dofade != 0 && !demo_wantstoquit();)
        {
            Common::copper2();
            Common::copper3();

            alku_do_scroll(2);

            demo_vsync();
            demo_blit();
        }

        for (f = 60; a < SCREEN_WIDTH && !demo_wantstoquit();)
        {
            if (f == 0)
            {
                Common::cop_fadepal = textin;
                Common::cop_dofade = 64;
                f += 20;
            }
            else if (f == 50)
            {
                Common::cop_fadepal = textout;
                Common::cop_dofade = 64;
                f++;
            }
            else if (f > 50 && Common::cop_dofade == 0)
            {
                Common::cop_pal = palette;
                Common::do_pal = 1;
                f++;
                memset(tbuf, 0, 186 * SCREEN_WIDTH);
                s_sdfLineCount = 0;

                // The scroll asks for one page every ~58 rows, which over the
                // 320-row block is six advances, but only five pages are defined.
                // The sixth fell through to the author's own timing-error
                // diagnostic and printed "BUG BUG BUG / Timing error" on screen.
                // Case 4 is the blank beat before the ships, so the block ends
                // there: stop advancing rather than run off the end. The
                // diagnostic stays for a genuinely out-of-range index.
                constexpr int CREDIT_PAGES = 5;
                if (alku_tptr >= CREDIT_PAGES)
                {
                    ffonapois();
                }
                else
                switch (alku_tptr++)
                {
                    case 0:
                        prt(0, 50, "Graphics");
                        prt(0, 90, "Marvel");
                        prt(0, 130, "Pixel");
                        ffonapois();
                        break;
                    case 1:
                        prt(0, 50, "Music");
                        prt(0, 90, "Purple Motion");
                        prt(0, 130, "Skaven");
                        ffonapois();
                        break;
                    case 2:
                        prt(0, 30, "Code");
                        prt(0, 70, "Psi");
                        prt(0, 110, "Trug");
                        prt(0, 148, "Wildfire");
                        ffonapois();
                        break;
                    case 3:
                        prt(0, 50, "Additional Design");
                        prt(0, 90, "Abyss");
                        prt(0, 130, "Gore");
                        ffonapois();
                        break;
                    case 4:
                        ffonapois();
                        break;
                    default:
                        prt(0, 80, "BUG BUG BUG");
                        prt(0, 130, "Timing error");
                        ffonapois();
                        break;
                }

                while (((a & 1) || Music::sync() < 4 + alku_tptr) && !demo_wantstoquit() && a < 319)
                {
                    Common::copper2();
                    Common::copper3();

                    alku_do_scroll(0);

                    demo_vsync();
                    demo_blit();
                }

                aa = a;
                if (aa < SCREEN_WIDTH - 12) fmaketext(aa + 16);
                f = 0;
            }
            else
                f++;

            Common::copper2();
            Common::copper3();

            alku_do_scroll(1);
            demo_vsync();
            demo_blit();
        }

        alku_do_scroll(1);
        demo_blit();

        if (f > 63 / SCRLF)
        {
            dofade(palette2, palette);
        }

        fonapois();

        demo_setsdftextcallback(nullptr);
        // The hires picture deliberately outlives this part: ALKU's teardown used
        // to drop the background layer and then paint an opaque black mesh
        // background over it, which is the black screen U2A used to start on.
        // ResetPartState() clears m_meshBgScreen for the next part anyway, so
        // nothing leaks - U2A keeps the picture and draws the ships over it.
        g_respectRatio = true;
    }
}
