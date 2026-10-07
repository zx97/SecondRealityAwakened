#include "Parts/Common.h"

#include <cstdio>
#include <cstdlib>

#include "Graphics/VectorFont.h"
#include "ENDSCRL_MAIN_Data.h"

namespace EndScrl
{
    constexpr const size_t FONAY = 25;
    // Rows of scroll the text is visible in, matching the DOS 350 row mode.
    constexpr int VISIBLE_ROWS = 350;
    // Extra rows a line keeps being drawn for once its own row has left the top,
    // so it can be clipped pixel by pixel instead of popping out whole.
    constexpr int EXIT_ROWS = 40;

    namespace
    {
        const char * fonaorder = "ABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyz0123456789!?,.:"
                                 "\x8F\x8F"
                                 "()+-*='Z"
                                 "\x84\x94"
                                 "Y/&";

        char text[64000] =
        "Second Reality\n"
        "\n"
        "Copyright (C) 1993 by the Future Crew\n"
        "\n"
        "\n"
        "\n"
        "This demo won the Assembly'93\n"
        "annual international demo competition\n"
        "held in Finland, summer 1993.\n"
        "\n"
        "\n"
        "\n"
        "As the PC organizers of Assembly'93,\n"
        "we would like to thank the following\n"
        "companies for sponsoring the event:\n"
        "\n"
        "Advanced Gravis\n"
        "Canada\n"
        "[15\n"
        "Epic MegaGames\n"
        "USA\n"
        "[15\n"
        "Waite Group Press\n"
        "USA\n"
        "[15\n"
        "Terton\n"
        "Finland\n"
        "[15\n"
        "HiCompu\n"
        "Finland\n"
        "[15\n"
        "Toptronics\n"
        "Finland\n"
        "[15\n"
        "Pro Component\n"
        "Finland\n"
        "[15\n"
        "Lan Vision\n"
        "Finland\n"
        "[15\n"
        "Data Fellows\n"
        "Finland\n"
        "\n"
        "\n"
        "Don't forget to attend Assembly'94!\n"
        "\n"
        "\n"
        "\n"
        "If you want to contact us, please\n"
        "read the FCINFO10.TXT file first.\n"
        "The questions you'd like to ask\n"
        "might already be answered there.\n"
        "\n"
        "\n"
        "\n"
        "Future Crew greets the following groups:\n"
        "\n"
        "Sonic PC\n"
        "Access Denied\n"
        "Silents PC\n"
        "Avalanche\n"
        "Xography\n"
        "Witan\n"
        "Triton\n"
        "Epical\n"
        "Electromotive Force\n"
        "Extreme\n"
        "Renaissance\n"
        "Iguana\n"
        "Darkzone\n"
        "Dust\n"
        "Pentagon\n"
        "Surprise! Productions\n"
        "Paranoids\n"
        "Cascada\n"
        "Majic 12\n"
        "iCE\n"
        "Anarchy PC\n"
        "\n"
        "\n"
        "\n"
        "The members of Future Crew send\n"
        "their personal greetings to:\n"
        "\n"
        "\n"
        "Psi:\n"
        "The Silents PC members\n"
        "[12\n"
        "Lord Cyrix / Access Denied\n"
        "[12\n"
        "Daryl / Sun Projects\n"
        "[12\n"
        "Tran & White Shadow\n"
        "[12\n"
        "Dr.Weird / DanTe\n"
        "[12\n"
        "Martti Palonen\n"
        "[12\n"
        "Jussi Markula\n"
        "[12\n"
        "Mikko Vierula\n"
        "[12\n"
        "\n"
        "All FC distribution site SysOps:\n"
        "USA:\n"
        "[10\n"
        "Chris Zimman / Quantum Accelerator\n"
        "[8\n"
        "Daredevil / The Sound Barrier\n"
        "[8\n"
        "Soul Rebel / Eleutheria\n"
        "[8\n"
        "Grid Runner / The Power Grid\n"
        "[8\n"
        "Lion Heart / Red Sector BBS\n"
        "[8\n"
        "Holy Water / The End of Time\n"
        "[8\n"
        "Daniel Potter / Programmer's Oasis\n"
        "England:\n"
        "[10\n"
        "Rob Barth / Sound & Vision\n"
        "Canada:\n"
        "[10\n"
        "Snibble / Spasm-o-Tron\n"
        "Australia:\n"
        "[10\n"
        "Bartender / Tequila Sunrise\n"
        "Belgium:\n"
        "[10\n"
        "Lord Cyrix / Point Break\n"
        "McGarret / Genesis\n"
        "Denmark:\n"
        "[10\n"
        "Executioner / Crack Central\n"
        "Germany:\n"
        "[10\n"
        "BitBlaster / The BitBlaster BBS\n"
        "[10\n"
        "Trojaner / The Continental\n"
        "Holland:\n"
        "[10\n"
        "Preceptor / The Consultation\n"
        "Israel:\n"
        "[10\n"
        "Shachar Cafri / The Bureaucratic\n"
        "Norway:\n"
        "[10\n"
        "Stinger / Romeo November\n"
        "Spain:\n"
        "[10\n"
        "Gvyt / Dracker BBS\n"
        "Sweden:\n"
        "[10\n"
        "ZED / Illusion\n"
        "Switzerland:\n"
        "[10\n"
        "PfUsuUS / Wonderland\n"
        "\n"
        "Thank you for experiencing\n"
        "Second Reality\n";

        int fonap[256]{};
        int fonaw[256]{};

        static char vectorEndscrlFont[25][1550]{};
        static bool vectorEndscrlReady = false;

        char * tptr = text;
        int tstart = 0, chars = 0;

        bool textDone = false;
        bool lineFresh = false;

        char textline[100]{};
        char scanbuf[640]{};

        // SDF capture: ring of recently-scrolled lines (each with its vram row).
        struct SdfLine
        {
            char text[200]{};
            // Age in scroll rows since this line was emitted. The previous code
            // stored yscrl, the scroll buffer position, and compared it against
            // Shim::startpixel/640, which is a second cyclic counter. The two
            // drift apart, so travelled could come out negative and the old
            // "+= 401" fallback made an old line look young again, bringing it
            // back from the bottom on top of newer text. Age only ever counts
            // up, so a line can only leave.
            int age = 0;
        };
        SdfLine sdfHistory[420]{};
        int sdfHistoryCount = 0;
        int sdfNext = 0;
    }

    void setstart(int y)
    {
        Shim::setstartpixel(y);
    }

    void setrgbpalette(int p, int r, int g, int b)
    {
        Shim::setpal(p, static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b));
    }

    void do_scroll()
    {
        static int yscrl = 0;
        static int line = 0;

        int a, b, x;

        if (line == 0)
        {
            lineFresh = false;

            // '%' is the source's end-of-scroll marker and '\0' is the end of
            // the text: both mean the credits are over, not that they replay.
            if (*tptr == '%' || *tptr == '\0')
            {
                textDone = true;
            }
            else
            {
                lineFresh = true;

                for (a = 0, tstart = 0, chars = 0; *tptr != '\n' && *tptr != '\0'; a++, chars++)
                {
                    textline[a] = *tptr;
                    tstart += fonaw[(unsigned char)*tptr++] + 2;
                }

                textline[a] = *tptr;
                if (*tptr != '\0') tptr++;
                tstart = (639 - tstart) / 2;

                if (textline[0] == '[')
                {
                    chars = 0;
                }
            }
        }

        // Capture the line for SDF rendering once per full scroll step, so the
        // vector text is drawn at a single clean position instead of smearing
        // a copy at every intermediate pixel row (which blurred the glyphs).
        if (line == 0 && lineFresh && textline[0] != '[')
        {
            SdfLine & sl = sdfHistory[sdfNext];
            sdfNext = (sdfNext + 1) % 420;
            if (sdfHistoryCount < 420) sdfHistoryCount++;
            sl.age = 0;
            int n = 0;
            for (a = 0; a < chars && n + 1 < static_cast<int>(sizeof(sl.text)); ++a)
            {
                sl.text[n++] = textline[a];
            }
            sl.text[n] = 0;
        }

        // Every live line ages by one scroll row per frame, so a line rises one
        // row and never comes back down. Only the lines young enough to still be
        // on screen need ageing: a capture happens every FONAY frames and the
        // window is VISIBLE_ROWS tall, so that is only a handful of entries.
        for (int i = 0; i < sdfHistoryCount; ++i)
        {
            SdfLine & sl2 = sdfHistory[i];
            if (sl2.text[0] != 0 && sl2.age < VISIBLE_ROWS + EXIT_ROWS) ++sl2.age;
        }

        yscrl = (yscrl + 1) % 401;

        if (textline[0] == '[')
        {
            int height = (textline[1] - '0') * 10 + (textline[2] - '0');
            line = (line + 1) % height;
        }
        else
        {
            line = (line + 1) % FONAY;
        }

        setstart(yscrl * 640);

        // Once the text is exhausted, quit as ESC would -- but only after every
        // captured line has scrolled off the top, so the final message is seen.
        if (textDone)
        {
            bool anyVisible = false;
            for (int i = 0; i < sdfHistoryCount; ++i)
            {
                if (sdfHistory[i].text[0] != 0 && sdfHistory[i].age < VISIBLE_ROWS + EXIT_ROWS)
                {
                    anyVisible = true;
                    break;
                }
            }
            if (!anyVisible) demo_requestexit();
        }
    }

    void drawSdfText()
    {
        static const char * set = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?,.:()+-*='Z/Y& ";
        static bool built = false;
        if (!built)
        {
            int aw = 0, ah = 0, apad = 0;
            const unsigned char * atlas = VectorFont::BuildSdfAtlas(set, 48, aw, ah, apad);
            demo_sdftexture(atlas, aw, ah);
            built = true;
        }

        if (sdfHistoryCount == 0) return;

        // Identity MVP: quads in NDC.
        static const float MVP[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };

        const float aspect = demo_windowaspect();
        // Twice the original size on wide screens, but capped so the longest
        // line still fits: 42 characters at this size need 2.77 NDC, which
        // overflows a 4:3 window (2.67 NDC) at the full double. Beyond 16:10 the
        // text is only using part of the width, so the extra room is spent on
        // glyphs rather than on overflow.
        const float maxW = 0.94f * 2.0f * aspect;   // NDC width available
        const float wanted = 0.11f;                  // the doubled size
        const float needed = 42.0f * 0.6f * wanted;  // widest line at that size
        const float fontNdc = (needed > maxW) ? (maxW / (42.0f * 0.6f)) : wanted;
        const float scale = fontNdc / 48.0f;

        static GpuTexVertex quads[420 * 200 * 4];
        int quadCount = 0;

        // Iterate the ring oldest -> newest, draw every line currently in the
        // visible 350-row window.
        const int oldest = (sdfNext - sdfHistoryCount + 420) % 420;
        for (int k = 0; k < sdfHistoryCount && quadCount < 420 * 200; ++k)
        {
            const SdfLine & sl = sdfHistory[(oldest + k) % 420];
            if (sl.text[0] == 0) continue;

            // Age counts up only, so once a line passes the window height it has
            // scrolled off the top for good and can never come back.
            if (sl.age >= VISIBLE_ROWS + EXIT_ROWS) continue;

            const int screenY = VISIBLE_ROWS - 1 - sl.age;

            const char * text = sl.text;
            float total = 0;
            for (const char * p = text; *p; ++p)
                total += (float)VectorFont::GetGlyphAdvance(*p) * scale;

            float penX = -total * 0.5f / aspect;
            // The DOS mode is 350 rows tall over a 200 row logical screen, so
            // there are two text rows per logical unit and the divisor stays 175.
            // Dividing by the 350 window height instead halved the height of the
            // block and left it stranded in the middle of the screen.
            const float baseY = 1.0f - (float)(screenY + 12) / 175.0f;

            for (const char * p = text; *p; ++p)
            {
                char ch = *p;
                float u0, v0, u1, v1;
                int gw, gh, gx, gy;
                if (!VectorFont::GetGlyphUV(ch, u0, v0, u1, v1, gw, gh, gx, gy))
                {
                    penX += (float)VectorFont::GetGlyphAdvance(ch) * scale / aspect;
                    continue;
                }

                const float x0 = penX + (float)gx * scale / aspect;
                const float x1 = x0 + (float)gw * scale / aspect;
                float yTop = baseY - (float)gy * scale;
                const float yBot = yTop - (float)gh * scale;

                // Clip the part of the glyph above the top edge, moving the top
                // texture coordinate with it so the visible rows keep their size.
                float tv0 = v0;
                if (yTop > 1.0f)
                {
                    const float span = yTop - yBot;
                    if (span <= 0.0f) continue;
                    tv0 = v0 + (v1 - v0) * ((yTop - 1.0f) / span);
                    yTop = 1.0f;
                }
                if (yBot >= 1.0f) continue;

                GpuTexVertex * q = quads + quadCount * 4;
                q[0].px = x0; q[0].py = yTop; q[0].pz = 0; q[0].u = u0; q[0].v = tv0;
                q[1].px = x1; q[1].py = yTop; q[1].pz = 0; q[1].u = u1; q[1].v = tv0;
                q[2].px = x0; q[2].py = yBot; q[2].pz = 0; q[2].u = u0; q[2].v = v1;
                q[3].px = x1; q[3].py = yBot; q[3].pz = 0; q[3].u = u1; q[3].v = v1;
                ++quadCount;

                penX += (float)VectorFont::GetGlyphAdvance(ch) * scale / aspect;
            }
        }

        if (quadCount > 0)
        {
            if (getenv("SR_DEBUG")) std::fprintf(stderr, "[ENDSCRL] draw %d quads, hist=%d start=%d\n", quadCount, sdfHistoryCount, (int)(Shim::startpixel/640));
            demo_drawsdftext(quads, quadCount, MVP, 1.0f, 1.0f, 1.0f);
        }
    }

    void init()
    {
        int x, y, b;

        Blob::Handle a = Blob::open("endscrol.txt");
        Blob::read(text, 60000, 1, a);
        Blob::read(text, 60000, 1, a);

        for (x = 0; x < 1550 && *fonaorder;)
        {
            while (x < 1550)
            {
                for (y = 0; y < static_cast<int>(FONAY); y++)
                    if (Data::endscrl_font[y][x]) break;
                if (y != FONAY) break;
                x++;
            }

            b = x;

            while (x < 1550)
            {
                for (y = 0; y < static_cast<int>(FONAY); y++)
                    if (Data::endscrl_font[y][x]) break;
                if (y == FONAY) break;
                x++;
            }

            fonap[(unsigned char)*fonaorder] = b;
            fonaw[(unsigned char)*fonaorder] = x - b;
            fonaorder++;
        }

        fonap[32] = 1550 - 20;
        fonaw[32] = 16;
    }

    void main()
    {
        Shim::clearScreen();

        g_respectRatio = false;
        demo_meshbackgroundscreen();

        demo_vsync();

        // The text is drawn on palette index 0, which is only black if the
        // previous part happened to leave it that way (CREDITS leaves the last
        // picture's palette, often white). Set it explicitly.
        setrgbpalette(0, 0, 0, 0);
        setrgbpalette(1, 20, 20, 20);
        setrgbpalette(2, 40, 40, 40);
        setrgbpalette(3, 60, 60, 60);
        setrgbpalette(4, 60, 60, 60);
        setrgbpalette(5, 60, 60, 60);
        setrgbpalette(6, 60, 60, 60);
        setrgbpalette(7, 60, 60, 60);
        setrgbpalette(8, 60, 60, 60);
        setrgbpalette(9, 60, 60, 60);
        setrgbpalette(10, 60, 60, 60);
        setrgbpalette(11, 60, 60, 60);
        setrgbpalette(12, 60, 60, 60);
        setrgbpalette(13, 60, 60, 60);
        setrgbpalette(14, 60, 60, 60);
        setrgbpalette(15, 60, 60, 60);

        init();

        while (!demo_wantstoquit())
        {
            demo_vsync();

            do_scroll();
            drawSdfText();

            demo_blit();
        }

        demo_meshbackground(0.0f, 0.0f, 0.0f);
        g_respectRatio = true;
    }
}