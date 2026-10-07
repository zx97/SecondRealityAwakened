#include "Parts/Common.h"

#include <vector>

#include "Graphics/VectorFont.h"
#include "CREDITS_MAIN_Data.h"
#include "CreditsPicsData.h"

namespace Credits
{
    constexpr const size_t FONAY = 32;

    // Part framebuffer: height fixed at 1080p, width taken from the window
    // aspect at part start so the frame is never stretched on ultrawide. The
    // picture stays 4:3 in the top half (720x540), leaving the bottom half for
    // the text. 1080p is plenty: the source shots come from a 320x200 demo.
    constexpr int FB_H = 1080;
    constexpr int PIC_W = 720;
    constexpr int PIC_H = 540;

    constexpr int TEXT_LINE_START = 16;
    constexpr int LINE_STEP = 42;

    // Palette index reserved by embed_credits.py as black, used for the backdrop
    // the picture is composited onto.
    constexpr unsigned char BG = 255;

    namespace
    {
        int g_fbW = 1920;
        int g_picX = (1920 - PIC_W) / 2;
        std::vector<unsigned char> s_indices;

        // Vector (SDF) text capture: each line stores the centred text.
        struct TextLine
        {
            char text[128]{};
            int y = 0;
        };
        TextLine s_textLines[32]{};
        int s_textLineCount = 0;
    }

    static void sdfBuildQuads()
    {
        static const char * set = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/?!:,.\"()+- ";
        static bool built = false;
        if (!built)
        {
            int aw = 0, ah = 0, apad = 0;
            const unsigned char * atlas = VectorFont::BuildSdfAtlas(set, 56, aw, ah, apad);
            demo_sdftexture(atlas, aw, ah);
            built = true;
        }

        if (s_textLineCount == 0) return;

        // Identity MVP: quads are emitted directly in NDC.
        static const float MVP[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };

        // Text size as a fraction of the window height.
        const float windowAspect = demo_windowaspect();
        const float fontNdc = 0.18f;
        const float scale = fontNdc / 56.0f;     // atlas is built at 56px
        static GpuTexVertex quads[64 * 4];
        int quadCount = 0;

        for (int li = 0; li < s_textLineCount && quadCount < 64; ++li)
        {
            const char * text = s_textLines[li].text;

            // Measure advance (in atlas px) to centre the line.
            float total = 0;
            for (const char * p = text; *p; ++p)
                total += (float)VectorFont::GetGlyphAdvance(*p) * scale;

            // penX in NDC: centred (divide by aspect for wide screens).
            float penX = -total * 0.5f / windowAspect;
            // Baseline in NDC, independent of the framebuffer resolution: the
            // text strip is the bottom half (NDC -1..0), as in the original.
            const float baseY = -1.0f + (float)(s_textLines[li].y + static_cast<int>(FONAY) / 2) / 200.0f;

            for (const char * p = text; *p; ++p)
            {
                unsigned char ch = (unsigned char)*p;
                float u0, v0, u1, v1;
                int gw, gh, gx, gy;
                if (!VectorFont::GetGlyphUV((char)ch, u0, v0, u1, v1, gw, gh, gx, gy))
                {
                    penX += (float)VectorFont::GetGlyphAdvance((char)ch) * scale / windowAspect;
                    continue;
                }

                const float x0 = penX + (float)gx * scale / windowAspect;
                const float x1 = x0 + (float)gw * scale / windowAspect;
                const float yTop = baseY - (float)gy * scale;
                const float yBot = yTop - (float)gh * scale;

                GpuTexVertex * q = quads + quadCount * 4;
                q[0].px = x0; q[0].py = yTop; q[0].pz = 0; q[0].u = u0; q[0].v = v0;
                q[1].px = x1; q[1].py = yTop; q[1].pz = 0; q[1].u = u1; q[1].v = v0;
                q[2].px = x0; q[2].py = yBot; q[2].pz = 0; q[2].u = u0; q[2].v = v1;
                q[3].px = x1; q[3].py = yBot; q[3].pz = 0; q[3].u = u1; q[3].v = v1;
                ++quadCount;

                penX += (float)VectorFont::GetGlyphAdvance((char)ch) * scale / windowAspect;
            }
        }

        demo_drawsdftext(quads, quadCount, MVP, 1.0f, 1.0f, 1.0f);
    }

    // Blits the picture at horizontal offset `x` (its left edge), clipped to
    // the frame; used to scroll it in from the left and out to the right.
    void reconstitute_split(int x)
    {
        memset(Shim::cpuPixels, BG, g_fbW * FB_H);

        int sx = 0;
        if (x < 0) { sx = -x; x = 0; }
        int x1 = x + (PIC_W - sx);
        if (x1 > g_fbW) x1 = g_fbW;
        const int w = x1 - x;
        if (w <= 0) return;

        for (int i = 0; i < PIC_H; ++i)
            memcpy(Shim::cpuPixels + (size_t)g_fbW * i + x,
                   s_indices.data() + (size_t)PIC_W * i + sx, w);
    }

    static std::vector<unsigned char> base64Decode(const char * s)
    {
        auto value = [](char c) -> int
        {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };

        std::vector<unsigned char> out;
        out.reserve(strlen(s) * 3 / 4);
        int buf = 0, bits = 0;
        for (const char * p = s; *p; ++p)
        {
            const int v = value(*p);
            if (v < 0) continue;
            buf = (buf << 6) | v;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out.push_back((unsigned char)((buf >> bits) & 0xFF));
            }
        }
        return out;
    }

    static void loadPicture(const Data::CreditsPic & pic)
    {
        s_indices.assign((size_t)PIC_W * PIC_H, 0);

        const std::vector<unsigned char> png = base64Decode(pic.png);
        int w = 0, h = 0;
        unsigned char * rgb = demo_loadpng_rgb_mem(png.data(), (int)png.size(), &w, &h);
        if (!rgb)
            return;

        if (w == PIC_W && h == PIC_H)
            for (size_t i = 0; i < s_indices.size(); ++i)
                s_indices[i] = rgb[i * 3];

        demo_freepng(rgb);
    }

    // Captures one NUL-terminated line and returns the next one (or the final
    // terminator, which ends the caller's loop).
    const char * credits_nextline(const char * txt)
    {
        if (s_textLineCount < static_cast<int>(sizeof(s_textLines) / sizeof(s_textLines[0])))
        {
            TextLine & line = s_textLines[s_textLineCount];
            size_t n = 0;
            for (; txt[n] && n + 1 < sizeof(line.text); ++n)
                line.text[n] = txt[n];
            line.text[n] = 0;
            line.y = TEXT_LINE_START + s_textLineCount * LINE_STEP;
            ++s_textLineCount;
        }

        const char * t = txt;
        while (*t) ++t;
        return t + 1;
    }

    void screenin(const Data::CreditsPic & pic, const char * text)
    {
        s_textLineCount = 0;

        memset(Shim::cpuPixels, BG, g_fbW * FB_H);

        demo_vsync();

        Common::setpalarea((char *)pic.pal, 0, PaletteColorCount);

        loadPicture(pic);

        while (*(text = credits_nextline(text)))
            ;

        // Scroll direction and easing match the original: the picture decelerates
        // in from the right edge, holds, then accelerates out to the left.
        for (int h = g_fbW - g_picX; h > 0; h = h * 12 / 13)
        {
            demo_vsync();
            reconstitute_split(g_picX + h);
            sdfBuildQuads();
            demo_blit();
        }

        for (int a = 0; a < 200 && !demo_wantstoquit(); a++)
        {
            demo_vsync();
            sdfBuildQuads();
            demo_blit();
        }

        for (int y = 0, v = 0; y < 80 * (g_picX + PIC_W) && !demo_wantstoquit(); y = y + v, v += 15)
        {
            demo_vsync();
            reconstitute_split(g_picX - y / 80);
            sdfBuildQuads();
            demo_blit();
        }
    }

    void main()
    {
        Shim::clearScreen();

        // Match the window aspect so the frame is not stretched on ultrawide,
        // then fill the whole window: the pictures scroll from edge to edge
        // instead of being confined to a 4:3 letterbox.
        g_fbW = static_cast<int>(FB_H * demo_windowaspect() + 0.5f);
        if (g_fbW > VIRTUAL_SCREEN_WIDTH) g_fbW = VIRTUAL_SCREEN_WIDTH;
        if (g_fbW < PIC_W) g_fbW = PIC_W;
        g_picX = (g_fbW - PIC_W) / 2;
        demo_changemode(g_fbW, FB_H);
        g_respectRatio = false;

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[0], "GRAPHICS - MARVEL\0"
                                            "MUSIC - SKAVEN\0"
                                            "CODE - WILDFIRE\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[1],
                     "GRAPHICS - MARVEL\0"
                     "MUSIC - SKAVEN\0"
                     "CODE - PSI\0"
                     "OBJECTS - WILDFIRE\0" // --- W32 PORT CHANGE (updated from final) ---
            );

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[2], "GRAPHICS - MARVEL\0"
                                            "MUSIC - SKAVEN\0"
                                            "CODE - WILDFIRE\0"
                                            "ANIMATION - TRUG\0");

        if (!demo_wantstoquit()) screenin(Data::creditsPics[3], "\0GRAPHICS - PIXEL\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[4], "GRAPHICS - PIXEL\0"
                                            "MUSIC - PURPLE MOTION\0"
                                            "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[5], "\0MUSIC - PURPLE MOTION\0"
                                            "CODE - TRUG\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[6], "\0MUSIC - PURPLE MOTION\0"
                                            "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[7], "\0MUSIC - PURPLE MOTION\0"
                                            "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[8], "\0GRAPHICS - PIXEL\0"
                                            "MUSIC - PURPLE MOTION\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[9], "GRAPHICS - PIXEL\0"
                                            "MUSIC - PURPLE MOTION\0"
                                            "CODE - TRUG\0"
                                            "RENDERING - TRUG\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[10],
                     "SKETCH - SKAVEN\0" // --- W32 PORT CHANGE (updated from final) ---
                     "GRAPHICS - PIXEL\0"
                     "MUSIC - PURPLE MOTION\0"
                     "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[11],
                     "SKETCH - SKAVEN\0" // --- W32 PORT CHANGE (updated from final) ---
                     "GRAPHICS - PIXEL\0"
                     "MUSIC - PURPLE MOTION\0"
                     "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[12], "\0MUSIC - PURPLE MOTION\0"
                                             "CODE - WILDFIRE\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[13], "\0MUSIC - PURPLE MOTION\0"
                                             "CODE - WILDFIRE\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[14], "\0MUSIC - PURPLE MOTION\0"
                                             "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[15], "GRAPHICS - PIXEL\0"
                                             "MUSIC - PURPLE MOTION\0"
                                             "CODE - TRUG\0"
                                             "RENDERING - TRUG\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[16], "\0MUSIC - PURPLE MOTION\0"
                                             "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[17], "GRAPHICS - MARVEL\0"
                                             "MUSIC - PURPLE MOTION\0"
                                             "CODE - PSI\0");

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[18],
                     "MUSIC - SKAVEN\0"
                     "CODE - PSI\0"
                     "WORLD - TRUG\0" // --- W32 PORT CHANGE (updated from final) ---
            );

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[19],
                     "GRAPHICS - PIXEL\0"
                     "MUSIC - SKAVEN\0" // --- W32 PORT CHANGE (updated from final) ---
            );

        if (!demo_wantstoquit())
            screenin(Data::creditsPics[20],
                     "GRAPHICS - PIXEL\0"
                     "MUSIC - SKAVEN\0" // --- W32 PORT CHANGE (updated from final) ---
                     "CODE - WILDFIRE\0");

        demo_settextoverlay(nullptr);
        demo_meshbackground(0.0f, 0.0f, 0.0f);
        g_respectRatio = true;
    }
}
