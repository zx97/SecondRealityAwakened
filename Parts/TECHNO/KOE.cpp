#include "Parts/Common.h"

#include <algorithm>
#include <cmath>

#include "KOE_Data.h"
#include "TrollData.h"

extern bool g_respectRatio;

namespace KOE
{
    static const char * koeModernFrag =
        "precision highp float;\n"
        "uniform vec3 uResolution;\n"
        "uniform vec4 uViewport;\n"
        "uniform sampler2D uProfile1;\n"
        "uniform sampler2D uProfile2;\n"
        "uniform sampler2D uPalette;\n"
        "uniform vec2 uCircle1Pos;\n"
        "uniform vec2 uCircle2Pos;\n"
        "uniform float uCircle2On;\n"
        "uniform float uSinurot;\n"
        "uniform float uSinuspower;\n"
        "void main(){\n"
        "  float aspect = uResolution.x / uResolution.y;\n"
        "  float nx = gl_FragCoord.x / uResolution.x;\n"
        "  float ny = gl_FragCoord.y / uResolution.y;\n"
        "  float cx1 = 1.0 - uCircle1Pos.x / 320.0;\n"
        "  float cy1 = 1.0 - uCircle1Pos.y / 200.0;\n"
        "  float dx1 = (nx - cx1) * 240.0 * aspect;\n"
        "  float dy1 = (ny - cy1) * 200.0;\n"
        "  float r1 = sqrt(dx1 * dx1 + dy1 * dy1);\n"
        "  float ring1 = texture2D(uProfile1, vec2(r1 / 512.0, 0.5)).r * 255.0;\n"
        "  float ring2 = 0.0;\n"
        "  if (uCircle2On > 0.5) {\n"
        "    float cx2 = 1.0 - uCircle2Pos.x / 320.0;\n"
        "    float cy2 = 1.0 - uCircle2Pos.y / 200.0;\n"
        "    float line = (1.0 - ny) * 200.0;\n"
        "    float si = mod(uSinurot * 0.5 + 9.0 * (line + 1.0), 1024.0);\n"
        "    float powr = sin(6.28318 * si / 1024.0) * 32.0 * uSinuspower / 15.0;\n"
        "    float dx2 = (nx - cx2) * 240.0 * aspect + powr;\n"
        "    float dy2 = (ny - cy2) * 200.0;\n"
        "    float r2 = sqrt(dx2 * dx2 + dy2 * dy2);\n"
        "    ring2 = texture2D(uProfile2, vec2(r2 / 512.0, 0.5)).r * 255.0;\n"
        "  }\n"
        "  float idx = clamp(floor(ring1 + 0.5) + floor(ring2 + 0.5), 0.0, 15.0);\n"
        "  float r = texture2D(uPalette, vec2((idx * 3.0 + 0.5) / 48.0, 0.5)).r;\n"
        "  float g = texture2D(uPalette, vec2((idx * 3.0 + 1.5) / 48.0, 0.5)).r;\n"
        "  float b = texture2D(uPalette, vec2((idx * 3.0 + 2.5) / 48.0, 0.5)).r;\n"
        "  gl_FragColor = vec4(r, g, b, 1.0);\n"
        "}\n";

    static const char * koeBoxFrag =
        "precision highp float;\n"
        "uniform vec3 uResolution;\n"
        "uniform vec4 uQuads[88];\n"
        "uniform float uXShift;\n"
        "float cross2d(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }\n"
        "float inQuad(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d) {\n"
        "  float s1 = cross2d(b - a, p - a);\n"
        "  float s2 = cross2d(c - b, p - b);\n"
        "  float s3 = cross2d(d - c, p - c);\n"
        "  float s4 = cross2d(a - d, p - d);\n"
        "  bool inside = (s1 >= 0.0 && s2 >= 0.0 && s3 >= 0.0 && s4 >= 0.0) || (s1 <= 0.0 && s2 <= 0.0 && s3 <= 0.0 && s4 <= 0.0);\n"
        "  return inside ? 1.0 : 0.0;\n"
        "}\n"
        "void main() {\n"
        "  float aspect = uResolution.x / uResolution.y;\n"
        "  float x = gl_FragCoord.x / uResolution.x * 240.0 * aspect + 160.0 - 120.0 * aspect - uXShift;\n"
        "  float y = (1.0 - gl_FragCoord.y / uResolution.y) * 200.0;\n"
        "  vec2 p = vec2(x, y);\n"
        "  float depth = 0.0;\n"
        "  for (int i = 0; i < 44; i++) {\n"
        "    vec2 a = uQuads[i * 2].xy;\n"
        "    vec2 b = uQuads[i * 2].zw;\n"
        "    vec2 c = uQuads[i * 2 + 1].xy;\n"
        "    vec2 d = uQuads[i * 2 + 1].zw;\n"
        "    depth += inQuad(p, a, b, c, d);\n"
        "  }\n"
        "  vec3 col;\n"
        "  if (depth < 0.5) col = vec3(0.0);\n"
        "  else if (depth < 1.5) col = vec3(0.332, 0.301, 0.395);\n"
        "  else if (depth < 2.5) col = vec3(0.457, 0.395, 0.523);\n"
        "  else if (depth < 3.5) col = vec3(0.602, 0.555, 0.664);\n"
        "  else col = vec3(0.742, 0.695, 0.809);\n"
        "  gl_FragColor = vec4(col, 1.0);\n"
        "}\n";

    constexpr const uint16_t WMINY = 0;
    constexpr const uint16_t WMAXY = SCREEN_HEIGHT - 1;
    constexpr const uint16_t WMINX = 0;
    constexpr const uint16_t WMAXX = SCREEN_WIDTH - 1;

    constexpr const size_t PITCH = (PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT);

    struct XY
    {
        int16_t x{}, y{};
    };

    namespace
    {
        XY clip1{};
        XY clip2{};

        uint16_t polyisides = 0;
        uint16_t polysides = 0;

        XY clipxy2[32] = { 0 };
        XY polyixy[16] = { 0 };
        XY polyxy[16] = { 0 };

        uint16_t clipleft = 0;

        uint16_t framecountA = 0;
        uint16_t palanimcA = 0;
        uint16_t palanimcA2 = 0;
        uint16_t scrnposA = 0;
        uint16_t scrnposAl = 0;
        uint16_t scrnxA = 0;
        uint16_t scrnyA = 0;
        uint16_t scrnrotA = 0;
        uint16_t sinurotA = 0;
        uint16_t overrotA = 211;
        uint16_t overxA = 0;
        uint16_t overyaA = 0;
        uint16_t patdirA = 0;

        uint16_t sizefade = 0;
        uint16_t rotspeed = 0;
        uint16_t palfader = 0;
        uint8_t palfader2 = 255;
        uint8_t zumplane = 0x11;

        uint8_t sinuspower = 0;
        uint8_t powercnt = 0;

        uint8_t palKOEB[32 * 3] = { 0 };

        uint16_t rowsKOEA[200] = { 0 };
        uint16_t blit16t[256] = { 0 };

        uint8_t * circles[8] = { 0 };

        uint16_t framecountB = 0;
        uint32_t palanimcB = 0;
        uint32_t palanimc2B = 0;
        uint16_t scrnposB = 0;
        uint16_t scrnposBl = 0;
        uint16_t scrnx = 0;
        uint16_t scrny = 0;
        uint16_t scrnrot = 0;
        uint16_t sinurot = 0;
        uint16_t overrot = 0;
        uint16_t overx = 0;
        uint16_t overya = 0;
        uint32_t patdir = static_cast<uint32_t>(-3);

        uint8_t vbuf[8192] = { 0 };

        unsigned char planar_vram[4][8][40 * 200] = { 0 };

        char circlemem[16384 * 16] = {};
        char pic[20000 * 4]{};

        char palette[PaletteByteCount]{};
        char palfade[13000]{};

        int pl = 1, plv = 0;

        char currentPal[16 * 16 * 3];

        int koe_curpal = 0;
        int lasta = 0;

        char power0[16 * 256] = { 0 };
        char power1[16 * 256] = { 0 };

        unsigned char koe_tempbuf[PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT] = { 0 };
        unsigned char koe_planar[PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT * 4] = { 0 };

        // Bit-reversal lookup table (matches Win32 ASM flip8)
        static const uint8_t flip8_table[256] = {
            0,128, 64,192, 32,160, 96,224, 16,144, 80,208, 48,176,112,240,
            8,136, 72,200, 40,168,104,232, 24,152, 88,216, 56,184,120,248,
            4,132, 68,196, 36,164,100,228, 20,148, 84,212, 52,180,116,244,
           12,140, 76,204, 44,172,108,236, 28,156, 92,220, 60,188,124,252,
            2,130, 66,194, 34,162, 98,226, 18,146, 82,210, 50,178,114,242,
           10,138, 74,202, 42,170,106,234, 26,154, 90,218, 58,186,122,250,
            6,134, 70,198, 38,166,102,230, 22,150, 86,214, 54,182,118,246,
           14,142, 78,206, 46,174,110,238, 30,158, 94,222, 62,190,126,254,
            1,129, 65,193, 33,161, 97,225, 17,145, 81,209, 49,177,113,241,
            9,137, 73,201, 41,169,105,233, 25,153, 89,217, 57,185,121,249,
            5,133, 69,197, 37,165,101,229, 21,149, 85,213, 53,181,117,245,
           13,141, 77,205, 45,173,109,237, 29,157, 93,221, 61,189,125,253,
            3,131, 67,195, 35,163, 99,227, 19,147, 83,211, 51,179,115,243,
           11,139, 75,203, 43,171,107,235, 27,155, 91,219, 59,187,123,251,
            7,135, 71,199, 39,167,103,231, 23,151, 87,215, 55,183,119,247,
           15,143, 79,207, 47,175,111,239, 31,159, 95,223, 63,191,127,255
        };
    }

    void _initinterferenceKOEA_A(char * memory);
    void _dointerferenceKOEA_A();

    void initinterferenceKOEA(char * memory)
    {
        _initinterferenceKOEA_A(memory);
    }

    void dointerferenceKOEA()
    {
        _dointerferenceKOEA_A();
    }

    static void blitinit()
    {
        uint16_t off = 0;
        for (int y = 0; y < 200; ++y)
        {
            rowsKOEA[y] = off;

            off = (uint16_t)(off + 40);
        }

        for (int al = 0; al < 256; ++al)
        {
            uint8_t dh = 255;
            uint8_t dl = (uint8_t)al;
            uint8_t ah = 0;

            for (int i = 0; i < 8; ++i)
            {
                uint8_t msb = (uint8_t)(dl >> 7);
                dl = (uint8_t)((dl << 1) | msb);
                if (msb) ah ^= dh;
                dh >>= 1;
            }

            uint8_t high = (uint8_t)((ah & 1) ? 0x80 : 0x00);

            blit16t[al] = (uint16_t)((uint16_t)ah | ((uint16_t)high << 8));
        }
    }

    void asmdoit(char * src8, char * vram)
    {
        uint8_t * _blit16t = ((uint8_t *)blit16t);

        const uint8_t * src = (const uint8_t *)src8;
        uint8_t * dst = (uint8_t *)vram;

        for (int line = 0; line < 200; ++line)
        {
            uint8_t dh = 0;

            const uint8_t * s = src;
            uint8_t * d = dst;

            for (int i = 0; i < 20; ++i)
            {
                uint8_t bl = (uint8_t)(s[0] ^ dh);

                const uint8_t * t0 = &_blit16t[((unsigned)bl << 1)];
                uint8_t AL = t0[0];
                uint8_t AH = t0[1];

                bl = (uint8_t)(s[1] ^ AH);

                const uint8_t * t1 = &_blit16t[((unsigned)bl << 1)];
                uint8_t DL = t1[0];

                dh = t1[1];

                d[0] = AL;
                d[1] = DL;

                s += 2;
                d += 2;
            }

            src = s;
            dst = d;
        }
    }

    static void bltline(uint8_t * src, uint8_t * dst, uint8_t /*mask*/, uint8_t plane_count)
    {
        for (int plane = 0; plane < plane_count; plane++)
        {
            memcpy(dst + plane * (PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT), src, 40);
            src += 40;
        }
    }

    static void bltlinerev(uint8_t * src, uint8_t * dst, uint8_t /*mask*/, uint8_t plane_count)
    {
        for (int plane = 0; plane < plane_count; plane++)
        {
            for (int i = 0; i < 40; i++)
                dst[i] = flip8_table[src[39 - i]];
            src += 40;
            dst += PITCH;
        }
    }

    static void mixpal(const uint8_t * src, uint8_t * dst, int count, int fade)
    {
        if (fade <= 256)
        {
            for (int i = 0; i < count; i++)
            {
                dst[i] = static_cast<uint8_t>((src[i] * fade) >> 8);
            }
        }
        else
        {
            int add = fade - 256;
            for (int i = 0; i < count; i++)
            {
                int val = src[i] + add;
                dst[i] = static_cast<uint8_t>((val > 63) ? 63 : val);
            }
        }
    }

    // Radial ring profiles (level vs radius) extracted from the decoded circle
    // bitmaps so the effect shader renders crisp circles at any resolution.
    // The circle is radially symmetric, so the horizontal centre scanline of
    // the mirrored 640x400 bitmap is f(r); the pattern is extrapolated past
    // the 320px edge to fill ultra-wide screens.
    static unsigned char circle1_profile[512] = { 0 };
    static unsigned char circle2_profile[512] = { 0 };
    static unsigned char koe_palette_16[16 * 3] = { 0 };

    static void init_profiles()
    {
        // The centre scanline of the mirrored bitmap is source row 199 read
        // right-to-left (circle1: 3 planes -> 0..7, circle2: 1 plane -> 0/8).
        for (int r = 0; r < 320; ++r)
        {
            const int x = 319 - r;
            const int bi = x >> 3;
            const int bt = 7 - (x & 7);
            int c = 0;
            for (int p = 0; p < 3; ++p)
                c |= ((Data::circle[40 * 199 * 3 + p * 40 + bi] >> bt) & 1) << p;
            circle1_profile[r] = (unsigned char)c;
            circle2_profile[r] = (unsigned char)(((Data::circle2[40 * 199 + bi] >> bt) & 1) << 3);
        }
        // Extrapolate: circle1 rings keep the ~1.33px edge spacing (level
        // cycles 0..7), circle2 mask keeps its 12px period (flips 0/8).
        for (int r = 320; r < 512; ++r)
        {
            circle1_profile[r] = (unsigned char)((200 + (r - 318) * 2 / 3) % 8);
            circle2_profile[r] = (unsigned char)(((r - 311) / 12 % 2) * 8);
        }

        demo_setgputexture("uProfile1", circle1_profile, 512, 1);
        demo_setgputexture("uProfile2", circle2_profile, 512, 1);
    }

    static void upload_palette()
    {
        // Shim::palette is packed 6-bit VGA (b<<2 | g<<10 | r<<18); the first
        // 16 entries are the interference palette (index = ring1 + ring2).
        for (int i = 0; i < 16; ++i)
        {
            const unsigned int p = Shim::palette[i];
            koe_palette_16[i * 3 + 0] = (unsigned char)((p >> 16) & 0xFF);
            koe_palette_16[i * 3 + 1] = (unsigned char)((p >> 8) & 0xFF);
            koe_palette_16[i * 3 + 2] = (unsigned char)(p & 0xFF);
        }
        demo_setgputexture("uPalette", koe_palette_16, 16 * 3, 1);
    }

    static void init_interference()
    {
        // Decode the original 1-bit circle bitmaps into the planar buffer,
        // mirroring horizontally and vertically (same as the original ASM).
        // circle2 = 1 plane (interference mask), circle = 3 planes (rings).
        uint8_t * src = (uint8_t *)Data::circle2;
        uint8_t * dst = koe_tempbuf;
        uint8_t * dst_bottom = koe_tempbuf + PLANAR_WIDTH * 399;

        for (int y = 0; y < 200; y++)
        {
            bltline(src, dst, 0x01, 1);
            bltlinerev(src, dst + 40, 0x01, 1);
            bltline(src, dst_bottom, 0x01, 1);
            bltlinerev(src, dst_bottom + 40, 0x01, 1);

            dst += PLANAR_WIDTH;
            dst_bottom -= PLANAR_WIDTH;
            src += 40;
        }

        src = (uint8_t *)Data::circle;
        dst = koe_planar;
        dst_bottom = koe_planar + PLANAR_WIDTH * 399;

        for (int y = 0; y < 200; y++)
        {
            bltline(src, dst, 0x01, 3);
            bltlinerev(src, dst + 40, 0x01, 3);
            bltline(src, dst_bottom, 0x01, 3);
            bltlinerev(src, dst_bottom + 40, 0x01, 3);

            dst += PLANAR_WIDTH;
            dst_bottom -= PLANAR_WIDTH;
            src += 40 * 3;
        }

        framecountB = 0;
    }

    // Resolve planar graphics to linear framebuffer
    static void resolve_planar()
    {
        uint16_t src_pos = scrnposB;
        uint8_t * dst = (uint8_t *)Shim::cpuPixels;
        int pixels_left = SCREEN_WIDTH * 200;
        int row_pixels = SCREEN_WIDTH;
        uint8_t bit_mask = 0x80;

        while (pixels_left > 0)
        {
            uint8_t color = 0;

            if (koe_planar[src_pos + PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT * 0] & bit_mask) color |= 1;
            if (koe_planar[src_pos + PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT * 1] & bit_mask) color |= 2;
            if (koe_planar[src_pos + PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT * 2] & bit_mask) color |= 4;
            if (koe_planar[src_pos + PLANAR_WIDTH * DOUBlE_SCREEN_HEIGHT * 3] & bit_mask) color |= 8;

            *dst++ = color;

            bit_mask = (bit_mask >> 1) | ((bit_mask & 1) << 7); // Rotate right
            if (bit_mask == 0x80)
            {
                src_pos++;
            }

            row_pixels--;
            if (row_pixels == 0)
            {
                src_pos += 40;
                row_pixels = SCREEN_WIDTH;
            }

            pixels_left--;
        }
    }

    // Main interference effect
    static void do_interference()
    {
        while (1)
        {
            demo_vsync();

            // Set initial palette
            Common::setpalarea((char *)palKOEB, 0, 16);

            // Animate palette
            int32_t anim_pos = palanimcB + patdir;
            if (anim_pos < 0)
            {
                anim_pos = 8 * 3 - 3;
            }
            else if (anim_pos >= 8 * 3)
            {
                anim_pos = 0;
            }
            palanimcB = anim_pos;
            palanimc2B = anim_pos;

            // Fade palette
            palfader += 2;
            if (palfader > 512)
            {
                palfader = 512;
            }

            // Mix palettes
            mixpal(Data::pal0 + palanimcB, palKOEB, 8 * 3, palfader);
            mixpal(Data::pal0 + palanimcB, palKOEB + 8 * 3, 8 * 3, palfader);

            Common::setpalarea((char *)palKOEB, 0, 16);

            // Handle music sync
            if (int row = Music::getRow() & 7; row == 0 || row == 4)
            {
                patdir = static_cast<uint32_t>(-3);
            }

            // Update rotation
            scrnrot = (scrnrot + 5) & 1023;

            // Update size fade
            if (framecountB >= 64)
            {
                rotspeed++;
            }

            // Calculate screen position using sine table
            int sin_idx = scrnrot * 2;
            scrnx = ((Data::koe_sin1024[sin_idx] * sizefade) >> 16) + 160;
            sin_idx = ((sin_idx + 256 * 2) & (1024 * 2 - 1));
            scrny = ((Data::koe_sin1024[sin_idx] * sizefade) >> 16) + 100;

            // Calculate overlay position
            overrot = (overrot + rotspeed) & 1023;
            sin_idx = overrot * 2;
            overx = (((Data::koe_sin1024[sin_idx] >> 2) * sizefade) >> 16) + 160;
            sin_idx = ((sin_idx + 256 * 2) & (1024 * 2 - 1));
            overya = ((((Data::koe_sin1024[sin_idx] >> 2) * sizefade) >> 16) + 100) * 80;

            // Calculate screen buffer position
            scrnposBl = scrnx & 7;
            scrnposB = (scrny * PLANAR_WIDTH) + (scrnx >> 3);

            // Update frame counter
            framecountB++;
            if (framecountB >= 256)
            {
                break;
            }

            // Render the circles analytically via the effect shader.
            upload_palette();
            demo_gpuuniform2f("uCircle1Pos", 160.0f, 100.0f);
            demo_gpuuniform1f("uCircle2On", 0.0f);
            demo_blit();

            if (demo_wantstoquit())
            {
                break;
            }
        }
    }

    int waitborder()
    {
        if (int a = Music::getRow(); a != lasta)
        {
            lasta = a;
            if ((a & 7) == 7) koe_curpal = 15;
        }

        int r = demo_vsync();
        if (r > 10) r = 10;
        if (r < 0) r = 1;

        char * p = currentPal + 16 * 3 * koe_curpal;

        Shim::outp(0x3c8, 0);

        for (int a = 0; a < 16 * 3; a++)
            Shim::outp(0x3c9, *p++);

        if (koe_curpal)
        {
            koe_curpal--;
        }

        return r;
    }

    int * flash(int i)
    {
        static int pal1[16 * 3];

        int pal2[16 * 3];
        int a, j;
        if (i == -2)
            ;
        else if (i == -1)
        {
            Shim::outp(0x3c7, 0);
            for (a = 0; a < 16 * 3; a++)
                pal1[a] = Shim::inp(0x3c9);
        }
        else
        {
            j = 256 - i;
            for (a = 0; a < 16 * 3; a++)
                pal2[a] = (pal1[a] * j + 63 * i) >> 8;

            demo_vsync();

            Shim::outp(0x3c8, 0);

            for (a = 0; a < 16 * 3; a++)
                Shim::outp(0x3c9, pal2[a]);

            demo_blit();
        }

        return (pal1);
    }

    void resolve_16color(int page, int x_shift)
    {
        int idx = 0;
        char * dst = (char *)Shim::cpuPixels;
        int dst_stride = SCREEN_WIDTH - x_shift;
        int src_stride = (x_shift + 7) >> 3;
        for (int y = 0; y < 200; y++)
        {
            memset(dst, 0, x_shift);
            dst += x_shift;
            for (int x = 0; x < dst_stride; x++)
            {
                char bit = (x & 0x7);
                unsigned char color = 0;
                color |= ((planar_vram[0][page][idx]) & (1 << (7 - bit))) ? 1 : 0;
                color |= ((planar_vram[1][page][idx]) & (1 << (7 - bit))) ? 2 : 0;
                color |= ((planar_vram[2][page][idx]) & (1 << (7 - bit))) ? 4 : 0;
                color |= ((planar_vram[3][page][idx]) & (1 << (7 - bit))) ? 8 : 0;
                *(dst++) = color;
                if (bit == 7) idx++;
            }
            idx += src_stride;
        }
    }

    void sidescroll_256color(char * src, int x_shift)
    {
        if (char * dst = (char *)Shim::cpuPixels; x_shift >= 0)
        {
            int dst_stride = SCREEN_WIDTH - x_shift;
            for (int y = 0; y < DOUBlE_SCREEN_HEIGHT; y++)
            {
                memset(dst, 0, x_shift);
                dst += x_shift;
                memcpy(dst, src, dst_stride);
                dst += dst_stride;
                src += SCREEN_WIDTH;
            }
        }
        else
        {
            x_shift = -x_shift;
            int stride = SCREEN_WIDTH - x_shift;
            for (int y = 0; y < DOUBlE_SCREEN_HEIGHT; y++)
            {
                src += x_shift;
                memcpy(dst, src, stride);
                src += stride;
                memset(dst, 0, x_shift);
                dst += SCREEN_WIDTH;
            }
        }
    }

    // Hires troll. Placement reproduces what PrepareTextureForGPU() did with the
    // old CPU frame: full window height, native aspect, centred, black to both
    // sides. u1 is how much of the picture is revealed, anchored on its right
    // edge, which is exactly the slide the old x_shift argument produced.
    void draw_troll_hires(const unsigned char * rgb, int w, int h, float u1)
    {
        if (!rgb || w <= 0 || h <= 0) return;
        if (u1 <= 0.0f) return;

        demo_rgbatexture_rgb(rgb, w, h);

        const float winAspect = demo_windowaspect();
        const float hw = ((float)w / (float)h) / winAspect;
        const float x0 = -hw + 2.0f * hw * (1.0f - u1);
        const float x1 = hw;

        static const float IDENT[16] = {
            1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
        };

        GpuTexVertex q[4];
        q[0].px = x0; q[0].py =  1.f; q[0].pz = 0; q[0].u = 0.f; q[0].v = 0.f;
        q[1].px = x1; q[1].py =  1.f; q[1].pz = 0; q[1].u = u1;  q[1].v = 0.f;
        q[2].px = x0; q[2].py = -1.f; q[2].pz = 0; q[2].u = 0.f; q[2].v = 1.f;
        q[3].px = x1; q[3].py = -1.f; q[3].pz = 0; q[3].u = u1;  q[3].v = 1.f;
        demo_drawrgbaquad(q, 1, IDENT);
    }

    // Rebuild the indexed 320x400 frame that part 07's CRT collapse reads back
    // out of Shim::cpuPixels. The hires texture path leaves that buffer empty,
    // so the collapse ran on index 0, which setpal(0,63,63,63) paints white: a
    // full-screen flash with nothing left to contract. 4f7dc94 dropped both the
    // blit and the palette that gave those indices their colour.
    //
    // Median cut to 256 colours, the count the original 320x400 256-colour blit
    // could show. Palette entries are 6-bit: fadepals blends against 63 and
    // setpalarea feeds outp(0x3c9), which takes the VGA range.
    void blit_troll_indexed(const unsigned char * rgb, int srcW, int srcH)
    {
        struct Sample { uint16_t c; int32_t n; };
        struct Box { int lo, hi, pop; };
        constexpr int CUBE = 32768;
        static int hist[CUBE];
        static Sample smp[CUBE];
        static Box box[256];
        static uint8_t lut[CUBE];

        memset(hist, 0, sizeof(hist));

        for (int y = 0; y < DOUBlE_SCREEN_HEIGHT; ++y)
        {
            const unsigned char * srow =
                rgb + (size_t)((long long)y * srcH / DOUBlE_SCREEN_HEIGHT) * srcW * 3;
            for (int x = 0; x < SCREEN_WIDTH; ++x)
            {
                const unsigned char * p =
                    srow + (size_t)((long long)x * srcW / SCREEN_WIDTH) * 3;
                ++hist[(p[0] >> 3) * 1024 + (p[1] >> 3) * 32 + (p[2] >> 3)];
            }
        }

        int n = 0;
        for (int i = 0; i < CUBE; ++i)
            if (hist[i]) { smp[n].c = (uint16_t)i; smp[n].n = hist[i]; ++n; }

        box[0] = { 0, n, 0 };
        for (int i = 0; i < n; ++i) box[0].pop += smp[i].n;
        int nb = 1;

        while (nb < 256)
        {
            int pick = -1;
            for (int i = 0; i < nb; ++i)
                if (box[i].hi - box[i].lo > 1 && (pick < 0 || box[i].pop > box[pick].pop))
                    pick = i;
            if (pick < 0) break;

            const int lo = box[pick].lo, hi = box[pick].hi;
            int axis = 0, widest = -1;
            for (int a = 0; a < 3; ++a)
            {
                int mn = 31, mx = 0;
                for (int i = lo; i < hi; ++i)
                {
                    const int v = (smp[i].c >> (10 - a * 5)) & 31;
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                }
                if (mx - mn > widest) { widest = mx - mn; axis = a; }
            }

            std::sort(smp + lo, smp + hi, [axis](const Sample & a, const Sample & b) {
                return ((a.c >> (10 - axis * 5)) & 31) < ((b.c >> (10 - axis * 5)) & 31);
            });

            int acc = 0, mid = lo;
            for (int i = lo; i < hi - 1; ++i)
            {
                acc += smp[i].n;
                if (acc * 2 >= box[pick].pop) { mid = i + 1; break; }
            }
            if (mid <= lo) mid = lo + 1;
            if (mid > hi - 1) mid = hi - 1;

            const int loPop = box[pick].pop;
            int leftPop = 0;
            for (int i = lo; i < mid; ++i) leftPop += smp[i].n;
            const int rightPop = loPop - leftPop;

            box[pick].hi = mid;
            box[pick].pop = leftPop;
            box[nb] = { mid, hi, rightPop };
            ++nb;
        }

        for (int i = 0; i < nb; ++i)
        {
            long long r = 0, g = 0, b = 0, w = 0;
            for (int k = box[i].lo; k < box[i].hi; ++k)
            {
                r += (long long)((smp[k].c >> 10) & 31) * smp[k].n;
                g += (long long)((smp[k].c >> 5) & 31) * smp[k].n;
                b += (long long)(smp[k].c & 31) * smp[k].n;
                w += smp[k].n;
            }
            palette[i * 3 + 0] = (char)((r * 63 / w + 15) / 31);
            palette[i * 3 + 1] = (char)((g * 63 / w + 15) / 31);
            palette[i * 3 + 2] = (char)((b * 63 / w + 15) / 31);
            for (int k = box[i].lo; k < box[i].hi; ++k) lut[smp[k].c] = (uint8_t)i;
        }

        for (int y = 0; y < DOUBlE_SCREEN_HEIGHT; ++y)
        {
            const unsigned char * srow =
                rgb + (size_t)((long long)y * srcH / DOUBlE_SCREEN_HEIGHT) * srcW * 3;
            unsigned char * drow = Shim::cpuPixels + (size_t)y * SCREEN_WIDTH;
            for (int x = 0; x < SCREEN_WIDTH; ++x)
            {
                const unsigned char * p =
                    srow + (size_t)((long long)x * srcW / SCREEN_WIDTH) * 3;
                drow[x] = lut[(p[0] >> 3) * 1024 + (p[1] >> 3) * 32 + (p[2] >> 3)];
            }
        }

        Common::setpalarea(palette, 0, 256);
    }

    static inline void outpal(const uint8_t * pal, uint8_t start, uint8_t count)
    {
        Common::setpalarea((char *)pal, (int)start, (int)count);
    }

    // Copy one 40-byte scanline (mono)
    static inline void bltline_mono(const uint8_t * src, uint8_t * dst)
    {
        memcpy(dst, src, 40);
    }

    static inline void bltlinerev_mono(const uint8_t * src, uint8_t * dst)
    {
        for (int i = 0; i < 40; i++)
            dst[i] = flip8_table[src[39 - i]];
    }

    // Copy N planes from packed src (each 40 bytes) to planar dst slice (each PITCH)
    static inline void bltline_planes(const uint8_t * src, uint8_t * dst, int planes)
    {
        for (int p = 0; p < planes; ++p)
            memcpy(dst + p * PITCH, src + p * 40, 40);
    }

    static void bltlinerev_planes(const uint8_t * src, uint8_t * dst, int planes)
    {
        for (int p = 0; p < planes; ++p)
        {
            const uint8_t * s = src + p * 40;
            uint8_t * d = dst + p * PITCH;
            for (int i = 0; i < 40; i++)
                d[i] = flip8_table[s[39 - i]];
        }
    }

    // Rotate a 32,000-byte page right by 1 bit with carry propagated across the stream
    static void rotate1_page_rcr(const uint8_t * src, uint8_t * dst)
    {
        const int N = 32000;
        uint8_t cf = 0; // initial CF; ASM is unspecified -> choose 0
        for (int i = 0; i < N; ++i)
        {
            uint8_t b = src[i];
            uint8_t new_cf = (uint8_t)(b & 1); // old bit0 becomes next CF
            dst[i] = (uint8_t)((b >> 1) | (cf << 7));
            cf = new_cf;
        }
    }

    void resolve_planar_KOEA()
    {
        uint32_t esi = (uint16_t)scrnposA;
        uint8_t * edi = Shim::cpuPixels;
        uint8_t bl = (uint8_t)(0x80u >> (scrnposAl & 7));
        uint32_t ecx = 320u * 200u;
        uint32_t edx = 320u;

        while (ecx--)
        {
            uint8_t al = 0;
            if (koe_planar[esi + 0 * PITCH] & bl) al |= 1;
            if (koe_planar[esi + 1 * PITCH] & bl) al |= 2;
            if (koe_planar[esi + 2 * PITCH] & bl) al |= 4;
            if (koe_planar[esi + 3 * PITCH] & bl) al |= 8;
            *edi++ = al;

            bl = (uint8_t)((bl >> 1) | (bl << 7)); // ror bl,1
            if (bl & 0x80) ++esi;                  // advance byte when bit wrapped

            if (--edx == 0)
            {
                esi += 40;
                edx = SCREEN_WIDTH;
            }
        }
    }

    static void init_interferenceKOEA(uint8_t * memory)
    {
        for (int i = 0; i < 8; ++i)
        {
            circles[i] = memory;
            memory += 2048 * 16;
        }

        // Build mirrored mono into koe_tempbuf from circle2
        const uint8_t * esi = Data::circle2;
        uint8_t * edi_top = koe_tempbuf;
        uint8_t * edi_bot = koe_tempbuf + PLANAR_WIDTH * 399;

        for (int y = 0; y < 200; ++y)
        {
            // Top pair
            bltline_mono(esi, edi_top);
            bltlinerev_mono(esi, edi_top + 40);

            // Bottom pair
            bltline_mono(esi, edi_bot);
            bltlinerev_mono(esi, edi_bot + 40);

            edi_top += PLANAR_WIDTH;
            edi_bot -= PLANAR_WIDTH;
            esi += 40;
        }

        // Copy 32k to circles[0], then build 7 rotated pages
        memcpy(circles[0], koe_tempbuf, sizeof(koe_tempbuf));
        for (int i = 0; i < 7; ++i)
            rotate1_page_rcr(circles[i], circles[i + 1]);

        // Build 3 planes into _koe_planar (planes 0..2), mirrored from circle
        const uint8_t * sc = Data::circle;
        uint8_t * dt = koe_planar;                      // top forward
        uint8_t * db = koe_planar + PLANAR_WIDTH * 399; // bottom backward

        for (int y = 0; y < SCREEN_HEIGHT; ++y)
        {
            // Top pair
            bltline_planes(sc, dt, 3); // ecx=0103h -> start plane 1, copy 3
            bltlinerev_planes(sc, dt + 40, 3);

            // Bottom pair
            bltline_planes(sc, db, 3);
            bltlinerev_planes(sc, db + 40, 3);

            dt += PLANAR_WIDTH;
            db -= PLANAR_WIDTH;
            sc += 40 * 3;
        }

        framecountA = 0;
    }

    void _initinterferenceKOEA_A(char * memory)
    {
        init_interferenceKOEA((uint8_t *)memory);
    }

    static inline int32_t sar_div8(int32_t x)
    {
        if (x >= 0) return x >> 3;

        // arithmetic divide by 8 rounding toward -inf

        return -(((-x) + 7) >> 3);
    }

    static inline int16_t sin_by_byte_offset(uint16_t byte_off)
    {
        const uint8_t * p = (const uint8_t *)Data::koe_sin1024;

        return (int16_t)*(const int16_t *)(p + (byte_off & 2046u));
    }

    static void do_interferenceKOEA()
    {
        for (;;)
        {
            demo_vsync();

            // --- palette animation & upload ---
            int16_t si = (int16_t)palanimcA + (int16_t)patdirA;
            if (si < 0) si = (int16_t)(8 * 3 - 3);
            if (si >= 8 * 3) si = 0;
            palanimcA = (uint16_t)si;
            palanimcA2 = (uint16_t)si;

            outpal(Data::pal1_KOEA + palanimcA, 0, 8 * 3);
            outpal(Data::pal2_KOEA + palanimcA, 8, 8 * 3);

            uint32_t esi_base = 0;
            uint8_t * edi = koe_planar + 3 * PITCH + (uint16_t)scrnposA;

            uint16_t ebp = (uint16_t)sinurotA;
            ebp = (uint16_t)((ebp + 7 * 2) & 2047);
            sinurotA = ebp;

            for (int lines = 200; lines--;)
            {
                ebp = (uint16_t)((ebp + 9 * 2) & 2047);

                int16_t s = sin_by_byte_offset(ebp);
                int32_t ebx = (int32_t)s;
                ebx >>= 3;
                uint16_t idx = (uint16_t)(((uint16_t)sinuspower << 8) | ((uint8_t)ebx));

                int32_t eax = (int8_t)power0[idx];
                eax += (uint16_t)overxA;
                eax -= (uint16_t)scrnposAl;

                // page = 7 - (eax & 7) ; src_base = circles[page]
                uint8_t page = (uint8_t)(7u - (eax & 7));
                const uint8_t * src = circles[page];

                int32_t byte_off = sar_div8(eax);
                src += (uint16_t)overyaA;
                src += (int32_t)byte_off;
                src += esi_base;

                for (int i = 0; i < 44; i += 4)
                    *(uint32_t *)(edi + i) = *(const uint32_t *)(src + i);

                edi += PLANAR_WIDTH;
                esi_base += PLANAR_WIDTH;
            }

            // Render circles analytically (positions from the previous
            // frame's motion update; overyaA is stored *80).
            upload_palette();
            demo_gpuuniform2f("uCircle1Pos", (float)scrnxA, (float)scrnyA);
            demo_gpuuniform2f("uCircle2Pos", (float)overxA, (float)(overyaA / 80u));
            demo_gpuuniform1f("uCircle2On", 1.0f);
            demo_gpuuniform1f("uSinurot", (float)sinurotA);
            demo_gpuuniform1f("uSinuspower", (float)sinuspower);
            demo_blit();

            // --- pattern dir from music row ---
            uint16_t row = Music::getRow() & 7;
            if (row == 0 || row == 4) patdirA = (uint16_t)-3;

            // --- screen motion (scrnxA/scrnyA) ---
            uint16_t bx = (uint16_t)((scrnrotA + 5) & 1023);
            scrnrotA = bx;

            int16_t v = Data::koe_sin1024[bx];
            v >>= 2;
            v += 160;
            scrnxA = (uint16_t)v;

            uint16_t bx2 = (uint16_t)((bx + 256) & 1023);
            v = Data::koe_sin1024[bx2];
            v >>= 2;
            v += 100;
            scrnyA = (uint16_t)v;

            // --- overlay motion (overxA/overyaA) ---
            bx = (uint16_t)((overrotA + 7) & 1023);
            overrotA = bx;

            v = Data::koe_sin1024[bx];
            v >>= 2;
            v += 160;
            overxA = (uint16_t)v;

            bx2 = (uint16_t)((bx + 256) & 1023);
            v = Data::koe_sin1024[bx2];
            v >>= 2;
            v += 100;
            overyaA = (uint16_t)((uint16_t)v * 80u); // MUL 80 -> AX

            // --- compute resolve start (scrnposAl/scrnposA) ---
            {
                uint16_t ax = scrnxA;
                uint16_t bxv = ax;
                scrnposAl = (uint16_t)(ax & 7);
                uint16_t row_bytes = (uint16_t)(80u * scrnyA);
                scrnposA = (uint16_t)(row_bytes + (bxv >> 3)); // bx is 0..319, >>3 is fine
            }

            // --- power ramp after ~5s ---
            if (framecountA >= 70u * 5u)
            {
                if (++powercnt >= 16)
                {
                    powercnt = 0;
                    if (sinuspower < 15) ++sinuspower;
                }
            }

            ++framecountA;

            if (Music::getFrame() >= 925) break;

            if (demo_wantstoquit() != 0) break;
        }
    }

    void _dointerferenceKOEA_A()
    {
        do_interferenceKOEA();
    }

    static inline void clip_intersect(int16_t * v1, int16_t v2, int16_t * w1, int16_t w2, int16_t wl)
    {
        int32_t dw = (int32_t)w2 - (int32_t)(*w1);
        if (dw != 0)
        {
            int32_t num = ((int32_t)wl - (int32_t)(*w1)) * ((int32_t)v2 - (int32_t)(*v1));

            int32_t dv = num / dw;

            *v1 = (int16_t)((int32_t)(*v1) + dv);
        }
        *w1 = wl;
    }

    static inline uint8_t outcode_x(int16_t x)
    {
        uint8_t c = 0;
        if (x < WMINX) c |= 1;
        if (x > WMAXX) c |= 2;
        return c;
    }

    static inline uint8_t outcode_y(int16_t y)
    {
        uint8_t c = 0;
        if (y < WMINY) c |= 4;
        if (y > WMAXY) c |= 8;
        return c;
    }

    static void drawline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
    {
        if (y1 > y2)
        {
            uint16_t tx = x1;
            x1 = x2;
            x2 = tx;

            uint16_t ty = y1;
            y1 = y2;
            y2 = ty;
        }

        uint16_t dy = (uint16_t)(y2 - y1);

        uint32_t si = rowsKOEA[y1];

        // Pre-fill using clipleft: toggle bit 0x80 at x=0 moving +/- 40 bytes per row
        if (clipleft != 0)
        {
            int16_t cnt = clipleft;
            uint32_t tesi = si;
            if (cnt < 0)
            { // go downwards
                for (; cnt != 0; ++cnt)
                {
                    tesi += 40;
                    vbuf[tesi] ^= 0x80;
                }
            }
            else
            { // go upwards
                for (; cnt != 0; --cnt)
                {
                    tesi -= 40;
                    vbuf[tesi] ^= 0x80;
                }
            }
        }

        if (dy == 0) return;

        // Advance to starting byte & bit
        si += (x1 >> 3);
        uint8_t mask = (uint8_t)(0x80u >> (x1 & 7));

        // Set up deltas & error term exactly like ASM
        int32_t dx = (int32_t)x2 - (int32_t)x1;
        int32_t bx = -((int32_t)dy >> 1); // counter starts at -(dy/2)

        // Choose left or right stepping
        if (x1 >= x2)
        {
            dx = (int32_t)x1 - (int32_t)x2; // magnitude
            uint16_t cx = dy;               // remaining vertical steps

            while (cx--)
            {
                vbuf[si] ^= mask; // plot (XOR)
                si += 40;         // next scanline
                bx += dx;

                // move in X while accumulated error >= 0
                while (bx >= 0)
                {
                    bx -= dy;
                    // shift mask left; cross byte when overflow
                    mask <<= 1;
                    if (mask == 0)
                    {
                        mask = 0x01;
                        --si;
                    }
                }
            }
        }
        else
        {
            dx = (int32_t)x2 - (int32_t)x1; // magnitude
            uint16_t cx = dy;

            while (cx--)
            {
                vbuf[si] ^= mask; // plot (XOR)
                si += 40;         // next scanline
                bx += dx;

                // move in X while accumulated error >= 0
                while (bx >= 0)
                {
                    bx -= dy;
                    // shift mask right; cross byte when underflow
                    mask >>= 1;
                    if (mask == 0)
                    {
                        mask = 0x80;
                        ++si;
                    }
                }
            }
        }
    }

    static int cliplinex(XY * a, XY * b)
    {
        uint8_t ca = outcode_x(a->x);
        uint8_t cb = outcode_x(b->x);

        if ((ca & cb) != 0) return 1; // trivial reject on same side

        if (ca & 1) clip_intersect(&a->y, b->y, &a->x, b->x, WMINX);
        if (ca & 2) clip_intersect(&a->y, b->y, &a->x, b->x, WMAXX);
        if (cb & 1) clip_intersect(&b->y, a->y, &b->x, a->x, WMINX);
        if (cb & 2) clip_intersect(&b->y, a->y, &b->x, a->x, WMAXX);
        return 0;
    }

    static int clipliney(XY * a, XY * b)
    {
        uint8_t ca = outcode_y(a->y);
        uint8_t cb = outcode_y(b->y);

        if ((ca & cb) != 0) return 1;

        if (ca & 4) clip_intersect(&a->x, b->x, &a->y, b->y, WMINY);
        if (ca & 8) clip_intersect(&a->x, b->x, &a->y, b->y, WMAXY);
        if (cb & 4) clip_intersect(&b->x, a->x, &b->y, a->y, WMINY);
        if (cb & 8) clip_intersect(&b->x, a->x, &b->y, a->y, WMAXY);

        return 0;
    }

    // Clip the input polygon first by Y, then by X, building polyxy and polysides.
    static void clipanypoly()
    {
        uint16_t n = polyisides;
        if (n == 0)
        {
            polysides = 0;
            return;
        }

        if (n == 1)
        {
            // Dot case
            XY p = polyixy[0];
            if (p.x < WMINX || p.x > WMAXX || p.y < WMINY || p.y > WMAXY)
            {
                polysides = 0;
                return;
            }
            polyxy[0] = p;
            polysides = 1;
            return;
        }

        if (n == 2)
        {
            // Line case: clip both axes
            XY a = polyixy[0], b = polyixy[1];
            if (clipliney(&a, &b))
            {
                polysides = 0;
                return;
            }
            if (cliplinex(&a, &b))
            {
                polysides = 0;
                return;
            }
            polyxy[0] = a;
            polyxy[1] = b;
            polysides = (a.x == b.x && a.y == b.y) ? 1 : 2;
            return;
        }

        // ---------- Polygon: first clip by Y into clipxy2 ----------
        uint16_t outc = 0;
        XY last = polyixy[n - 1];
        uint32_t last_dw = ((uint32_t)(uint16_t)last.y << 16) | (uint16_t)last.x;

        for (uint16_t i = 0; i < n; ++i)
        {
            XY cur = polyixy[i];
            XY a = last, b = cur;
            last = cur;

            if (!clipliney(&a, &b))
            {
                uint32_t da = ((uint32_t)(uint16_t)a.y << 16) | (uint16_t)a.x;
                if (da != last_dw) clipxy2[outc++] = a, last_dw = da;
                uint32_t db = ((uint32_t)(uint16_t)b.y << 16) | (uint16_t)b.x;
                if (db != last_dw) clipxy2[outc++] = b, last_dw = db;
            }
        }

        // Remove duplicated first/last
        uint16_t m = outc;
        if (m && m >= 2)
        {
            if (clipxy2[0].x == clipxy2[m - 1].x && clipxy2[0].y == clipxy2[m - 1].y) --m;
        }
        if (m <= 2)
        {
            if (m == 0)
            {
                polysides = 0;
                return;
            }
            // Re-run the remaining segment through X-clip to normalize
            XY a = clipxy2[0], b = (m == 2 ? clipxy2[1] : clipxy2[0]);
            if (cliplinex(&a, &b))
            {
                polysides = 0;
                return;
            }
            polyxy[0] = a;
            polyxy[1] = b;
            polysides = (m == 2 ? ((a.x == b.x && a.y == b.y) ? 1 : 2) : 1);
            return;
        }

        // ---------- Then clip by X into polyxy ----------
        outc = 0;
        last = clipxy2[m - 1];
        last_dw = ((uint32_t)(uint16_t)last.y << 16) | (uint16_t)last.x;

        for (uint16_t i = 0; i < m; ++i)
        {
            XY cur = clipxy2[i];
            XY a = last, b = cur;
            last = cur;

            if (!cliplinex(&a, &b))
            {
                uint32_t da = ((uint32_t)(uint16_t)a.y << 16) | (uint16_t)a.x;
                if (da != last_dw) polyxy[outc++] = a, last_dw = da;
                uint32_t db = ((uint32_t)(uint16_t)b.y << 16) | (uint16_t)b.x;
                if (db != last_dw) polyxy[outc++] = b, last_dw = db;
            }
        }

        // Remove duplicated first/last
        polysides = outc;
        if (polysides && polyxy[0].x == polyxy[polysides - 1].x && polyxy[0].y == polyxy[polysides - 1].y)
        {
            --polysides;
        }
    }

    static float box[25][8][12] = { 0 };
    static int box_slice = 0;

    static void change_plane()
    {
        for (int i = 24; i > 0; --i)
            memcpy(box[i], box[i - 1], sizeof(float) * 8 * 12);
        box_slice = 0;
    }

    static void draw_boxes_gpu(float x_shift)
    {
        static float quad_data[88][4];
        int q = 0;
        for (int t = 0; t < 4; ++t)
        {
            const int frame = (t == 0) ? 24 : (t == 1) ? 16 : (t == 2) ? 8 : 0;
            for (int s = 0; s < 11; ++s)
            {
                quad_data[q][0] = box[frame][0][s];
                quad_data[q][1] = box[frame][1][s];
                quad_data[q][2] = box[frame][2][s];
                quad_data[q][3] = box[frame][3][s];
                quad_data[q + 1][0] = box[frame][4][s];
                quad_data[q + 1][1] = box[frame][5][s];
                quad_data[q + 1][2] = box[frame][6][s];
                quad_data[q + 1][3] = box[frame][7][s];
                q += 2;
            }
        }
        demo_gpuuniform1f("uXShift", x_shift);
        demo_gpuuniform4fv("uQuads", &quad_data[0][0], 88);
        demo_blit();
    }

    void asmbox(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
    {
        box[0][0][box_slice] = (float)x0;
        box[0][1][box_slice] = (float)y0;
        box[0][2][box_slice] = (float)x1;
        box[0][3][box_slice] = (float)y1;
        box[0][4][box_slice] = (float)x2;
        box[0][5][box_slice] = (float)y2;
        box[0][6][box_slice] = (float)x3;
        box[0][7][box_slice] = (float)y3;
        ++box_slice;
    }

    int doit1(int count)
    {
        int rot = 45;
        int c{}, x1{}, y1{}, x2{}, y2{}, x3{}, y3{}, x4{}, y4{}, hx{}, hy{}, vx{}, vy{}, cx{}, cy{};
        int vma{}, vm{};
        vm = 50;
        vma = 0;
        waitborder();
        plv = 0;
        pl = 1;
        while (!demo_wantstoquit() && count > 0)
        {
            count -= waitborder();
            memset(vbuf, 0, 8000);
            {
                hx = Data::koe_sin1024[(rot + 0) & 1023] * 16 * 6 / 5;
                hy = Data::koe_sin1024[(rot + 256) & 1023] * 16;
                vx = Data::koe_sin1024[(rot + 256) & 1023] * 6 / 5;
                vy = Data::koe_sin1024[(rot + 512) & 1023];
                vx = vx * vm / 100;
                vy = vy * vm / 100;
                for (c = -10; c < 11; c += 2)
                {
                    cx = vx * c * 2;
                    cy = vy * c * 2;
                    x1 = (-hx - vx + cx) / 16 + 160;
                    y1 = (-hy - vy + cy) / 16 + 100;
                    x2 = (-hx + vx + cx) / 16 + 160;
                    y2 = (-hy + vy + cy) / 16 + 100;
                    x3 = (+hx + vx + cx) / 16 + 160;
                    y3 = (+hy + vy + cy) / 16 + 100;
                    x4 = (+hx - vx + cx) / 16 + 160;
                    y4 = (+hy - vy + cy) / 16 + 100;
                    asmbox(x1, y1, x2, y2, x3, y3, x4, y4);
                }
                rot += 2;
                vm += vma;
                if (vm < 25)
                {
                    vm -= vma;
                    vma = -vma;
                }
                vma--;
            }

            draw_boxes_gpu(0.0f);
            change_plane();
        }
        return 0;
    }

    int doit2(int count)
    {
        int rot = 50, rota = 10;
        int c, x1, y1, x2, y2, x3, y3, x4, y4, hx, hy, vx, vy, cx, cy;
        int vma, vm;
        vm = 100 * 64;
        vma = 0;
        waitborder();
        plv = 0;
        pl = 1;
        while (!demo_wantstoquit() && count > 0)
        {
            count -= waitborder();

            memset(vbuf, 0, 8000);
            {
                hx = Data::koe_sin1024[(rot + 0) & 1023] * 16 * 6 / 5;
                hy = Data::koe_sin1024[(rot + 256) & 1023] * 16;
                vx = Data::koe_sin1024[(rot + 256) & 1023] * 6 / 5;
                vy = Data::koe_sin1024[(rot + 512) & 1023];
                vx = vx * (vm / 64) / 100;
                vy = vy * (vm / 64) / 100;
                for (c = -10; c < 11; c += 2)
                {
                    cx = vx * c * 2;
                    cy = vy * c * 2;
                    x1 = (-hx - vx + cx) / 16 + 160;
                    y1 = (-hy - vy + cy) / 16 + 100;
                    x2 = (-hx + vx + cx) / 16 + 160;
                    y2 = (-hy + vy + cy) / 16 + 100;
                    x3 = (+hx + vx + cx) / 16 + 160;
                    y3 = (+hy + vy + cy) / 16 + 100;
                    x4 = (+hx - vx + cx) / 16 + 160;
                    y4 = (+hy - vy + cy) / 16 + 100;
                    asmbox(x1, y1, x2, y2, x3, y3, x4, y4);
                }
                rot += rota / 10;
                vm += vma;
                if (vm < 0)
                {
                    vm -= vma;
                    vma = -vma;
                }
                vma--;
                rota++;
            }
            draw_boxes_gpu(0.0f);
            change_plane();
        }
        return 0;
    }

    int doit3(int count)
    {
        int rot = 45, rota = 10, rot2 = 0;
        int x, y, c, x1, y1, x2, y2, x3, y3, x4, y4, a, b, hx, hy, vx, vy, cx, cy, wx, wy;
        int vma, vm, xpos = SCREEN_WIDTH, xposa = 0;
        int ripple, ripplep, repeat = 1;
        char * p;
        vm = 100 * 64;
        vma = 0;
        waitborder();
        Shim::outp(0x3c8, 0);
        for (a = 0; a < 16 * 3; a++)
            Shim::outp(0x3c9, 0);

        memset(Shim::cpuPixels, 0, 32768);
        memset(Shim::cpuPixels + 32768, 0, 32768);

        plv = 0;
        pl = 1;
        while (!demo_wantstoquit() && count > 0)
        {
            a = Music::getFrame();

            if (count < 333)
            {
                while (repeat--)
                {
                    xpos -= xposa / 16;
                    if (xpos < 0)
                        xpos = 0;
                    else
                        xposa++;
                }
                if (xpos == 0) break;
            }
            if (rot2 < 32)
            {
                wx = Data::koe_sin1024[(rot2 + 0) & 1023] * rot2 / 8 + 160;
                wy = Data::koe_sin1024[(rot2 + 256) & 1023] * rot2 / 8 + 100;
            }
            else
            {
                wx = Data::koe_sin1024[(rot2 + 0) & 1023] / 4 + 160;
                wy = Data::koe_sin1024[(rot2 + 256) & 1023] / 4 + 100;
            }
            rot2 += 17;
            a = xpos / 8;

            count -= (repeat = waitborder());
            a = xpos & 7;

            memset(vbuf, 0, 8000);
            {
                hx = Data::koe_sin1024[(rot + 0) & 1023] * 16 * 6 / 5;
                hy = Data::koe_sin1024[(rot + 256) & 1023] * 16;
                vx = Data::koe_sin1024[(rot + 256) & 1023] * 6 / 5;
                vy = Data::koe_sin1024[(rot + 512) & 1023];
                vx = vx * (vm / 64) / 100;
                vy = vy * (vm / 64) / 100;
                for (c = -10; c < 11; c += 2)
                {
                    cx = vx * c * 2;
                    cy = vy * c * 2;
                    x1 = (-hx - vx + cx) / 16 + wx;
                    y1 = (-hy - vy + cy) / 16 + wy;
                    x2 = (-hx + vx + cx) / 16 + wx;
                    y2 = (-hy + vy + cy) / 16 + wy;
                    x3 = (+hx + vx + cx) / 16 + wx;
                    y3 = (+hy + vy + cy) / 16 + wy;
                    x4 = (+hx - vx + cx) / 16 + wx;
                    y4 = (+hy - vy + cy) / 16 + wy;
                    asmbox(x1, y1, x2, y2, x3, y3, x4, y4);
                }
                rot += rota / 10;
                {
                    vm += vma;
                    if (vm < 0)
                    {
                        vm -= vma;
                        vma = -vma;
                    }
                    vma--;
                }
                rota++;
            }

            draw_boxes_gpu((float)(SCREEN_WIDTH - xpos));
            change_plane();
        }

        demo_cleargputeffect();

        // The troll is a 320x400 bitmap; keep its native 4:3 ratio (the
        // column-fall transition above set g_respectRatio = false).
        g_respectRatio = true;

        demo_changemode(SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT);
        memset(Shim::cpuPixels, 0, SCREEN_WIDTH * DOUBlE_SCREEN_HEIGHT);
        demo_blit();

        if (demo_wantstoquit()) return 0;

        // The troll used to be a 320x400 256-colour bitmap decoded on the CPU and
        // blitted through Shim::cpuPixels, which capped it at 320x400 whatever
        // source art existed. It is now the 4x upscale as a texture, so the slide,
        // the ripple and the wipe still run on the very same xpos counter, just as
        // a horizontal reveal of the picture instead of a CPU blit. The palette
        // fade that rode along with those blits now has nothing to tint.
        int trollW = 0;
        int trollH = 0;
        unsigned char * trollRgb = demo_loadpng_rgb_mem(Data::troll_png, Data::troll_png_size, &trollW, &trollH);

        demo_vsync();

        // The CPU framebuffer holds nothing but black now that the troll is a
        // texture, so the palette has to be black too. It used to be the troll's
        // own palette, and palfade used to brighten its top 16 scanlines during
        // the ripple. With nothing 256-colour left on screen both are dead, and a
        // palette left over from earlier in the part tints the background grey.
        CLEAR(palette);

        demo_vsync();

        demo_blit();
        Common::setpalarea(palette, 0, 256);

        while (!demo_wantstoquit())
        {
            b = Music::getRow();
            a = Music::getOrder();
            demo_blit();
            if (a > 35 || (a == 35 && b > 48)) break;
        }

        count = 300;
        xposa = 0;
        xpos = 0;
        while (!demo_wantstoquit() && count > 0)
        {
            if (xpos == SCREEN_WIDTH) break;
            xpos += xposa / 4;
            if (xpos > SCREEN_WIDTH)
                xpos = SCREEN_WIDTH;
            else
                xposa++;
            a = xpos / 4;
            count -= demo_vsync();
            a = (xpos & 3) * 2;
            draw_troll_hires(trollRgb, trollW, trollH, (float)xpos / (float)SCREEN_WIDTH);
            demo_blit();
        }

        count = 50;
        c = 0;
        ripple = 0;
        ripplep = 8;

        while (!demo_wantstoquit() && count > 0)
        {
            if (ripplep > 1023)
                ripplep = 1024;
            else
                ripplep = ripplep * 5 / 4;
            xpos = SCREEN_WIDTH + Data::koe_sin1024[ripple & 1023] / ripplep;
            ripple += ripplep + 100;
            a = xpos / 4;

            count -= demo_vsync();

            a = (xpos & 3) * 2;

            draw_troll_hires(trollRgb, trollW, trollH, (float)xpos / (float)SCREEN_WIDTH);
            demo_blit();
        }

        Common::setpalarea(palette, 0, 256);
        count = 420;
        xpos = SCREEN_WIDTH;

        while (!demo_wantstoquit() && count > 0)
        {
            a = Music::getPlusFlags();
            if (a > -6 && a < 16) break;
            a = xpos / 4;

            count -= demo_vsync();

            a = (xpos & 3) * 2;

            draw_troll_hires(trollRgb, trollW, trollH, (float)xpos / (float)SCREEN_WIDTH);
            demo_blit();
        }

        // Last thing doit3 did was draw the troll full width into the texture,
        // leaving Shim::cpuPixels as it was memset at the top: empty. Part 07
        // seeds its collapse from that buffer, so hand it the same picture in
        // the indexed form it expects, at the same x_shift of 0 the original
        // blit ended on.
        blit_troll_indexed(trollRgb, trollW, trollH);

        return 0;
    }

    void reset()
    {
        clip1 = {};
        clip2 = {};

        polyisides = 0;
        polysides = 0;

        memset(&clipxy2, 0, sizeof(clipxy2));
        memset(&polyixy, 0, sizeof(polyixy));
        memset(&polyxy, 0, sizeof(polyxy));

        clipleft = 0;

        framecountA = 0;
        palanimcA = 0;
        palanimcA2 = 0;
        scrnposA = 0;
        scrnposAl = 0;
        scrnxA = 0;
        scrnyA = 0;
        scrnrotA = 0;
        sinurotA = 0;
        overrotA = 211;
        overxA = 0;
        overyaA = 0;
        patdirA = 0;

        sizefade = 0;
        rotspeed = 0;
        palfader = 0;
        palfader2 = 255;
        zumplane = 0x11;

        sinuspower = 0;
        powercnt = 0;

        CLEAR(palKOEB);

        CLEAR(rowsKOEA);
        CLEAR(blit16t);

        CLEAR(circles);

        framecountB = 0;
        palanimcB = 0;
        palanimc2B = 0;
        scrnposB = 0;
        scrnposBl = 0;
        scrnx = 0;
        scrny = 0;
        scrnrot = 0;
        sinurot = 0;
        overrot = 0;
        overx = 0;
        overya = 0;
        patdir = static_cast<uint32_t>(-3);

        CLEAR(vbuf);

        CLEAR(planar_vram);

        CLEAR(circlemem);
        CLEAR(pic);

        CLEAR(palette);
        CLEAR(palfade);

        pl = 1;
        plv = 0;

        CLEAR(currentPal);

        koe_curpal = 0;
        lasta = 0;

        CLEAR(power0);
        CLEAR(power1);

        CLEAR(koe_tempbuf);
        CLEAR(koe_planar);
    }

    void main()
    {
        reset();

        demo_setgputeffect(koeModernFrag);
        init_profiles();

        int x{}, y{}, b{}, c{}, a{};
        char ch{};
        int * ip{};
        char * p{};

        Shim::clearScreen();

        for (c = 0; c < 16; c++)
        {
            p = currentPal + 3 * 16 * c;
            for (a = 0; a < 16; a++)
            {
                x = 0;
                if (a & 1) x++;
                if (a & 2) x++;
                if (a & 4) x++;
                if (a & 8) x++;
                switch (x)
                {
                    case 0:
                        *(p++) = 0;
                        *(p++) = 0;
                        *(p++) = 0;
                        break;
                    case 1:
                        *(p++) = 38 * 64 / 111;
                        *(p++) = 33 * 64 / 111;
                        *(p++) = 44 * 64 / 111;
                        break;
                    case 2:
                        *(p++) = 52 * 64 / 111;
                        *(p++) = 45 * 64 / 111;
                        *(p++) = 58 * 64 / 111;
                        break;
                    case 3:
                        *(p++) = 67 * 64 / 111;
                        *(p++) = 61 * 64 / 111;
                        *(p++) = 73 * 64 / 111;
                        break;
                    case 4:
                        *(p++) = 83 * 64 / 111;
                        *(p++) = 77 * 64 / 111;
                        *(p++) = 89 * 64 / 111;
                        break;
                }
                p -= 3;
                *p = static_cast<char>(*p * (10 + c * 9 / 9) / 10);
                if (*p > 63) *p = 63;
                p++;
                *p = static_cast<char>(*p * (10 + c * 7 / 9) / 10);
                if (*p > 63) *p = 63;
                p++;
                *p = static_cast<char>(*p * (10 + c * 5 / 9) / 10);
                if (*p > 63) *p = 63;
                p++;
            }
        }

        blitinit();

        p = power0;
        for (b = 0; b < 16; b++)
        {
            for (c = 0; c < 256; c++)
            {
                if (b == 15)
                {
                    ch = (char)c;
                    *(p++) = ch;
                }
                else
                {
                    if (c > 127)
                        a = c - 256;
                    else
                        a = c;
                    ch = (char)(a * b / 15);
                    *(p++) = ch;
                }
            }
        }

        p = power1;
        for (b = 0; b < 16; b++)
        {
            for (c = 0; c < 256; c++)
            {
                if (b == 15)
                {
                    ch = (char)c;
                    *(p++) = ch;
                }
                else
                {
                    if (c > 127)
                        a = c - 256;
                    else
                        a = c;
                    ch = (char)(a * b / 15);
                    *(p++) = ch;
                }
            }
        }

        if (!Shim::isDemoFirstPart())
        {
            while (!demo_wantstoquit() && Music::getPlusFlags() < -4)
            {
                AudioPlayer::Update(true);
            }
        }

        Music::setFrame(0);

        init_interference();
        do_interference();

        initinterferenceKOEA(circlemem);

        if (!demo_wantstoquit())
        {
            dointerferenceKOEA();

            // The analytical circle shader is only for the interference
            // phases; the flash/column-fall/box effects below render via the
            // CPU framebuffer (vram), so clear the effect shader to switch
            // back to the normal upscale path.
            demo_cleargputeffect();

            // The flash/column-fall transition fills the whole window (no
            // 4:3 letterbox); the circles/boxes use their own shaders and are
            // unaffected by g_respectRatio.
            g_respectRatio = false;

            flash(-1);
            flash(32);
            flash(64);
            flash(192);
            flash(256);

            memset(Shim::cpuPixels, 0, SCREEN_SIZE);

            memset(Shim::cpuPixels, 15, 8000 * 8);

            ip = flash(-2);
            for (a = 0; a < 16; a++)
            {
                ip[a * 3 + 0] = a * 6 / 2;
                ip[a * 3 + 1] = a * 7 / 2;
                ip[a * 3 + 2] = a * 8 / 2;
            }

            while (!demo_wantstoquit() && Music::getPlusFlags() < -3)
            {
                AudioPlayer::Update(true);
            }

            while (!demo_wantstoquit())
            {
                if (a = Music::getRow() & 7; a == 7) break;

                AudioPlayer::Update(true);
            }

            for (b = 0; b < 4 && !demo_wantstoquit(); b++)
            {
                int zly, zy, zya;
                zy = 0;
                zya = 0;
                zly = 0;
                for (a = 256; a > -400; a -= 32)
                {
                    zly = zy;
                    zya++;
                    zy += zya;
                    char * v = (char *)(Shim::cpuPixels + zly * SCREEN_WIDTH);
                    for (y = zly; y < zy; y++)
                    {
                        memset(v + b * PLANAR_WIDTH, 0, PLANAR_WIDTH);
                        v += SCREEN_WIDTH;
                    }
                    if (a >= 0)
                        flash(a);
                    else
                        flash(0);
                }

                while (!demo_wantstoquit())
                {
                    if (a = Music::getRow() & 7; a == 7) break;

                    AudioPlayer::Update(true);
                }

                {
                    flash(-1);
                    flash(32);
                    flash(64);
                    flash(192);
                    flash(256);
                }
            }
        }

        Shim::clearScreen();

        demo_setgputeffect(koeBoxFrag);
        doit1(70 * 6);
        doit2(70 * 12);
        doit3(70 * 14);
        demo_cleargputeffect();
    }

}
