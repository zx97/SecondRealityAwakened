#include "Parts/Common.h"

#include "GLENZ_DATA.h"
#include "GLENZ_FC_DATA.h"

namespace Glenz
{
    // Full-screen GPU renderer for the glenz polyhedron. The CPU keeps doing the
    // 3D math (rotate/project) and the fc logo stays in the framebuffer; this
    // shader samples that framebuffer as the background and rasterizes the
    // projected triangles (point-in-triangle) on top, replicating the two-pass
    // glenz blend: transparent faces are added, opaque faces overwrite.
    static const char * glenz3DFrag =
        "precision highp float;\n"
        "uniform vec3 uResolution;\n"
        "uniform sampler2D uFcTex;\n"
        "uniform vec4 uTris[144];\n"
        "uniform float uFade;\n"
        "uniform float uFcFade;\n"
        "uniform float uFcTop;\n"
        "uniform float uFcBottom;\n"
        "\n"
        "bool inTri(vec2 p, vec2 a, vec2 b, vec2 c){\n"
        "  float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);\n"
        "  float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);\n"
        "  float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);\n"
        "  bool neg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);\n"
        "  bool pos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);\n"
        "  return !(neg && pos);\n"
        "}\n"
        "\n"
        "void main(){\n"
        "  float aspect = uResolution.x / uResolution.y;\n"
        "  float x = gl_FragCoord.x / uResolution.x * 240.0 * aspect + 160.0 - 120.0 * aspect;\n"
        "  float y = (1.0 - gl_FragCoord.y / uResolution.y) * 200.0;\n"
        "  vec2 p = vec2(x, y);\n"
        "  vec3 color = vec3(0.0);\n"
        "  if (x >= 0.0 && x <= 320.0 && uFcBottom > uFcTop && y >= uFcTop && y <= uFcBottom) {\n"
        "    vec2 uv = vec2(x / 320.0, (y - uFcTop) / (uFcBottom - uFcTop));\n"
        "    color = texture2D(uFcTex, uv).rgb * uFcFade;\n"
        "  }\n"
        "  for (int i = 0; i < 48; i++) {\n"
        "    vec4 d0 = uTris[i * 3 + 0];\n"
        "    vec4 d1 = uTris[i * 3 + 1];\n"
        "    vec4 d2 = uTris[i * 3 + 2];\n"
        "    if (d2.y < 0.5) continue;\n"
        "    if (inTri(p, d0.xy, d0.zw, d1.xy)) color += vec3(d1.z, d1.w, d2.x) * 0.5;\n"
        "  }\n"
        "  for (int i = 0; i < 48; i++) {\n"
        "    vec4 d0 = uTris[i * 3 + 0];\n"
        "    vec4 d1 = uTris[i * 3 + 1];\n"
        "    vec4 d2 = uTris[i * 3 + 2];\n"
        "    if (d2.y >= 0.5) continue;\n"
        "    if (inTri(p, d0.xy, d0.zw, d1.xy)) color = color * 0.5 + vec3(d1.z, d1.w, d2.x) * 0.5;\n"
        "  }\n"
        "  gl_FragColor = vec4(color * uFade, 1.0);\n"
        "}\n";

    // Maximum number of projected triangles (24 main polyhedron faces).
    constexpr int MAX_TRI = 48;
    constexpr int TRI_FLOATS = 12; // 3x vec4: (x0,y0,x1,y1) (x2,y2,r,g) (b,glenz,0,0)

    namespace
    {
        unsigned char backpal[16 * 3] = {
            16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
            16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        };

        char lightshift = 0;

        int32_t projxmul = 0;
        int32_t projymul = 0;
        uint16_t projxadd = 0;
        uint16_t projyadd = 0;
        int32_t projminz = 0;
        uint16_t projminzshr = 0;

        int16_t wminx = 0;
        int16_t wminy = 0;
        int16_t wmaxx = 100;
        int16_t wmaxy = 100;

        uint16_t count = 0;

        uint16_t pointsoff = 0;

        char pal1[PaletteByteCount];
        char pal2[PaletteByteCount];

        int32_t points2[256]{};
        int32_t points2b[256]{};
        int points3[256 * 2]{};

        short matrix[9]{};

        char pal[PaletteByteCount]{};
        char tmppal[PaletteByteCount]{};

        int repeat{};
        int frame{};

        // GPU triangle buffer.
        float triData[MAX_TRI * TRI_FLOATS]{};
        int triCount = 0;

        int32_t g_xadd = 0, g_yadd = 0, g_zadd = 0;
        int32_t g_m[9] = { 0 };
    }

    inline uint8_t clamp63(uint32_t v)
    {
        return (uint8_t)(v > 63u ? 63u : v);
    }

    static void reset_triangles()
    {
        triCount = 0;
        for (int i = 0; i < MAX_TRI; i++)
        {
            float * t = &triData[i * TRI_FLOATS];
            // Off-screen non-degenerate triangle never matches an on-screen pixel.
            t[0] = 1000.0f; t[1] = 1000.0f; t[2] = 1001.0f; t[3] = 1000.0f;
            t[4] = 1000.0f; t[5] = 1001.0f; t[6] = 0.0f; t[7] = 0.0f;
            t[8] = 0.0f; t[9] = 0.0f; t[10] = 0.0f; t[11] = 0.0f;
        }
    }

    static void append_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                                float r, float g, float b, float glenz)
    {
        if (triCount >= MAX_TRI) return;
        float * t = &triData[triCount * TRI_FLOATS];
        t[0] = (float)x0; t[1] = (float)y0; t[2] = (float)x1; t[3] = (float)y1;
        t[4] = (float)x2; t[5] = (float)y2; t[6] = r; t[7] = g;
        t[8] = b; t[9] = glenz; t[10] = 0.0f; t[11] = 0.0f;
        triCount++;
    }

    int csetmatrix(short * m9, int32_t X, int32_t Y, int32_t Z)
    {
        g_xadd = X;
        g_yadd = Y;
        g_zadd = Z;

        g_m[0] = (int32_t)m9[0];
        g_m[1] = (int32_t)m9[1];
        g_m[2] = (int32_t)m9[2];
        g_m[3] = (int32_t)m9[3];
        g_m[4] = (int32_t)m9[4];
        g_m[5] = (int32_t)m9[5];
        g_m[6] = (int32_t)m9[6];
        g_m[7] = (int32_t)m9[7];
        g_m[8] = (int32_t)m9[8];

        return 0;
    }

    static inline int32_t dot_q15(int32_t a0, int32_t a1, int32_t a2, int32_t b0, int32_t b1, int32_t b2)
    {
        int64_t s = (int64_t)a0 * (int64_t)b0 + (int64_t)a1 * (int64_t)b1 + (int64_t)a2 * (int64_t)b2;
        return (int32_t)(s >> 15);
    }

    int crotlist(int32_t * dst, const int32_t * src)
    {
        uint16_t src_count = (uint16_t)(src[0] & 0xFFFFu);
        uint32_t dst_hdr = (uint32_t)dst[0];
        uint16_t dst_count = (uint16_t)(dst_hdr & 0xFFFFu);

        uint16_t new_dst_count = (uint16_t)(dst_count + src_count);
        dst[0] = (int32_t)((dst_hdr & 0xFFFF0000u) | new_dst_count);

        int32_t * out = dst + 1 + (3 * (int)dst_count);
        const int32_t * in = src + 1;

        for (uint16_t i = 0; i < src_count; ++i)
        {
            int32_t x = in[0];
            int32_t y = in[1];
            int32_t z = in[2];

            int32_t X = dot_q15(g_m[0], g_m[1], g_m[2], x, y, z) + g_xadd;
            int32_t Row1 = dot_q15(g_m[3], g_m[4], g_m[5], x, y, z);
            int32_t Y = dot_q15(g_m[6], g_m[7], g_m[8], x, y, z) + g_yadd;
            int32_t Z = Row1 + g_zadd;

            out[0] = X;
            out[1] = Y;
            out[2] = Z;

            in += 3;
            out += 3;
        }

        return (int)src_count;
    }

    void cliplist(int32_t * esi)
    {
        uint16_t cx = *(const uint16_t *)((const void *)esi);
        esi = (int32_t *)((uint8_t *)esi + 4);

        while (cx--)
        {
            if (!(esi[1] < 1500))
            {
                esi[1] = 1500;
            }
            esi += 3;
        }
    }

    static void projlist(const int32_t * ESI, uint16_t * EDI)
    {
        uint16_t cx = *(const uint16_t *)((const void *)ESI);
        ESI = (const int32_t *)((const uint8_t *)ESI + 4);
        count = cx;

        uint16_t have = *EDI;
        *EDI = (uint16_t)(have + cx);

        uint16_t ax = have;
        uint16_t bx = have;
        ax = (uint16_t)((ax << 1) + bx);
        ax = (uint16_t)(ax << 2);

        uint8_t * out = (uint8_t *)EDI + ax + 4;

        while (count--)
        {
            int32_t X = ESI[0];
            int32_t Y = ESI[1];
            int32_t Z = ESI[2];

            uint16_t bp = 0;
            *(int32_t *)(out + 8) = Z;

            int32_t Zdiv = Z;
            if (Zdiv < projminz)
            {
                Zdiv = projminz;
                bp |= 16;
            }

            int32_t qY = (int32_t)((int64_t)Y * (int64_t)projymul / (int64_t)Zdiv);
            int16_t y = (int16_t)(qY + (int32_t)projyadd);

            if (y > wmaxy) bp |= 8;
            if (y < wminy) bp |= 4;

            *(int16_t *)(out + 2) = y;

            int32_t qX = (int32_t)((int64_t)X * (int64_t)projxmul / (int64_t)Zdiv);
            int16_t x = (int16_t)(qX + (int32_t)projxadd);

            if (x > wmaxx) bp |= 2;
            if (x < wminx) bp |= 1;

            *(int16_t *)(out + 0) = x;

            *(uint16_t *)(out + 4) = bp;

            ESI += 3;
            out += 12;
        }
    }

    void cprojlist(uint16_t * dst, const int32_t * src)
    {
        projlist(src, dst);
    }

    void init320x200C()
    {
        projxmul = 256;
        projymul = 213;

        projxadd = 160;
        projyadd = 130;

        projminz = 128;
        projminzshr = 7;

        wminx = 0;
        wminy = 0;
        wmaxx = SCREEN_WIDTH - 1;
        wmaxy = SCREEN_HEIGHT - 1;
    }

    static inline int16_t wrap_deg10(int16_t a)
    {
        int32_t x = a;
        while (x >= 3600)
            x -= 3600;
        while (x < 0)
            x += 3600;
        return (int16_t)x;
    }

    static inline int16_t q15_mul_exact(int16_t a, int16_t b)
    {
        int32_t p = (int32_t)a * (int32_t)b;
        uint32_t up = (uint32_t)p;
        uint32_t shr = up >> 15;
        return (int16_t)(shr & 0xFFFF);
    }

    static inline int16_t add16ForMatrix(int16_t acc, int16_t v)
    {
        return (int16_t)((int32_t)acc + (int32_t)v);
    }

    static inline int16_t sub16ForMatrix(int16_t acc, int16_t v)
    {
        return (int16_t)((int32_t)acc - (int32_t)v);
    }

    static inline void trig_deg10(int16_t a, int16_t * s, int16_t * c)
    {
        int16_t w = wrap_deg10(a);
        *s = Data::sintable16[w];
        *c = Data::costable16[w];
    }

    static void calcmatrix(int16_t rx, int16_t ry, int16_t rz, int16_t * d)
    {
        int16_t sx, cx, sy, cy, sz, cz;
        trig_deg10(rx, &sx, &cx);
        trig_deg10(ry, &sy, &cy);
        trig_deg10(rz, &sz, &cz);

        int16_t xs_ys = q15_mul_exact(sx, sy);
        int16_t xs_yc = q15_mul_exact(sx, cy);
        int16_t xc_ys = q15_mul_exact(cx, sy);
        int16_t xc_yc = q15_mul_exact(cx, cy);

        int16_t v0 = q15_mul_exact(cy, cz);
        v0 = sub16ForMatrix(v0, q15_mul_exact(xs_ys, sz));
        d[0] = v0;

        int16_t v2 = q15_mul_exact(xs_ys, cz);
        v2 = add16ForMatrix(v2, q15_mul_exact(cy, sz));
        d[1] = v2;

        d[2] = (int16_t)(-xc_ys);

        d[3] = (int16_t)(-q15_mul_exact(cx, sz));

        d[4] = q15_mul_exact(cx, cz);

        d[5] = sx;

        int16_t v12 = q15_mul_exact(xs_yc, sz);
        v12 = add16ForMatrix(v12, q15_mul_exact(sy, cz));
        d[6] = v12;

        int16_t v14 = q15_mul_exact(sy, sz);
        v14 = sub16ForMatrix(v14, q15_mul_exact(xs_yc, cz));
        d[7] = v14;

        d[8] = xc_yc;
    }

    void cmatrix_yxzC(int16_t rx, int16_t ry, int16_t rz, int16_t * dst)
    {
        calcmatrix(/*rx*/ ry, /*ry*/ rx, /*rz*/ rz, dst);
    }

    // Build projected triangles (points + per-face color) for the GPU shader.
    // `simple` selects the "shadow" object's reduced glenz (colors 0/1).
    static void ceasypolylist_gpu(const unsigned short * polys, const int32_t * ppoints3, bool simple)
    {
        const int32_t * pts = ppoints3 + 1;

        for (;;)
        {
            uint16_t sides = *polys++;
            if (sides == 0) return;

            uint16_t color = *polys++;

            int16_t vx[3];
            int16_t vy[3];
            for (uint16_t k = 0; k < 3 && k < sides; ++k)
            {
                uint16_t idx = *polys++;
                const int32_t * p = pts + (int)idx * 3;
                vx[k] = (int16_t)((uint32_t)p[0] & 0xFFFFu);
                vy[k] = (int16_t)(((uint32_t)p[0] >> 16) & 0xFFFFu);
            }
            // consume any remaining vertices (polygons are triangles here)
            for (uint16_t k = 3; k < sides; ++k)
                ++polys;

            int64_t det = (int64_t)(vx[0] - vx[1]) * (vy[0] - vy[2]) - (int64_t)(vy[0] - vy[1]) * (vx[0] - vx[2]);

            float r, g, b, glenz;
            if (simple)
            {
                if (det < 0)
                {
                    continue;
                }
                int tone = (color >> 1) & 1;
                if (tone)
                {
                    r = 0.682f; // 0xae1100
                    g = 0.067f;
                    b = 0.0f;
                }
                else
                {
                    r = 0.514f; // 0x830c00
                    g = 0.047f;
                    b = 0.0f;
                }
                glenz = 1.0f;
            }
            else if (det < 0)
            {
                continue;
            }
            else
            {
                glenz = 0.0f;
                int inten = (int)(det >> 7);
                if (inten < 0) inten = 0;
                if (inten > 47) inten = 47;
                int rr, gg, bb;
                if (color & 2)
                {
                    rr = 16;
                    gg = inten / 3 + 16;
                    bb = inten + 16;
                }
                else
                {
                    rr = gg = bb = inten + 16;
                }
                r = (float)rr / 63.0f;
                g = (float)gg / 63.0f;
                b = (float)bb / 63.0f;
            }

            append_triangle(vx[0], vy[0], vx[1], vy[1], vx[2], vy[2], r, g, b, glenz);
        }
    }

    void zoomer2()
    {
        int a{}, b{}, c{}, y{};
        char * v{};
        int framez2 = 0;
        int zly{}, zy{}, zya{};
        int zly2{}, zy2{};

        Shim::outp(0x3c7, 0);

        for (a = 0; a < static_cast<int>(PaletteByteCount); a++)
            pal1[a] = Shim::inp(0x3c9);

        zy = 0;
        zya = 0;
        zly = 0;
        zy2 = 0;
        zly2 = 0;
        framez2 = 0;

        while (!demo_wantstoquit())
        {
            if (zy == 260) break;
            zly = zy;
            zya++;
            zy += zya / 4;
            if (zy > 260) zy = 260;
            v = (char *)(Shim::cpuPixels + zly * PLANAR_WIDTH * 4);
            for (y = zly; y <= zy; y++)
            {
                memset(v, 255, PLANAR_WIDTH * 4);
                v += PLANAR_WIDTH * 4;
            }
            zly2 = zy2;
            zy2 = 125 * zy / 260;
            v = (char *)(Shim::cpuPixels + (399 - zy2) * PLANAR_WIDTH * 4);
            for (y = zly2; y <= zy2; y++)
            {
                memset(v, 255, PLANAR_WIDTH * 4);
                v += PLANAR_WIDTH * 4;
            }
            c = framez2;
            if (c > 32) c = 32;
            b = 32 - c;
            for (a = 0; a < 128 * 3; a++)
            {
                pal2[a] = static_cast<char>((pal1[a] * b + 30 * c) >> 5);
            }
            framez2++;
            demo_vsync();
            Common::setpalarea(pal2, 0, 128);
            demo_blit();
        }
        v = (char *)(Shim::cpuPixels + (194) * PLANAR_WIDTH * 4);
        Shim::outp(0x3c8, 0);
        Shim::outp(0x3c9, 0);
        Shim::outp(0x3c9, 0);
        Shim::outp(0x3c9, 0);
        v = (char *)(Shim::cpuPixels + (0) * PLANAR_WIDTH * 4);
        for (y = 0; y <= 399; y++)
        {
            memset(v, 0, PLANAR_WIDTH * 4);
            v += PLANAR_WIDTH * 4;
        }
    }

    void reset()
    {
        for (auto & col : backpal)
        {
            col = 16;
        }

        lightshift = 0;

        projxmul = 0;
        projymul = 0;
        projxadd = 0;
        projyadd = 0;
        projminz = 0;
        projminzshr = 0;

        wminx = 0;
        wminy = 0;
        wmaxx = 100;
        wmaxy = 100;

        count = 0;

        pointsoff = 0;

        CLEAR(pal1);
        CLEAR(pal2);

        CLEAR(points2);
        CLEAR(points2b);
        CLEAR(points3);

        CLEAR(matrix);

        CLEAR(pal);
        CLEAR(tmppal);

        repeat = {};
        frame = {};

        g_xadd = 0;
        g_yadd = 0;
        g_zadd = 0;

        CLEAR(g_m);
    }

    void main()
    {
        reset();

        int a{}, b{}, y{}, rx{}, ry{}, rz{}, r{}, g{}, zpos = 7500, ypos{}, yposa{};
        int ya{}, yy{}, boingm = 6, boingd = 7;
        int jello = 0, jelloa = 0;
        int xscale = 120, yscale = 120, zscale = 120, bscale = 0;
        int oxp = 0, oyp = 0, ozp = 0;
        int oxb = 0, oyb = 0, ozb = 0;
        char *ps{}, *pd{}, *pp{};
        char * fcrow[100];
        char * fcrow2[16];

        if (!Shim::isDemoFirstPart())
        {
            while (!demo_wantstoquit() && Music::getPlusFlags() < -19)
            {
                demo_vsync(true);  // advance audio for music sync
                demo_blit();
            };
        }

        Music::setFrame(0);

        zoomer2();

        memset(Shim::cpuPixels, 0, 65535);
        demo_changemode(SCREEN_WIDTH, SCREEN_HEIGHT);
        init320x200C();

        demo_setgputeffect(glenz3DFrag);

        {
            int fcW = 0, fcH = 0;
            unsigned char * fcRgb = demo_loadpng_rgb_mem(Data::glenz_fc_png, Data::glenz_fc_png_size, &fcW, &fcH);
            if (fcRgb)
            {
                demo_setgputexture_rgb("uFcTex", fcRgb, fcW, fcH);
                demo_freepng(fcRgb);
            }
        }

        demo_gpuuniform1f("uFade", 1.0f);
        demo_gpuuniform1f("uFcFade", 1.0f);

        reset_triangles();
        demo_gpuuniform4fv("uTris", triData, MAX_TRI * 3);

        constexpr float FC_START_Y = 130.0f;
        constexpr float FC_FINAL_TOP = 144.5f;
        constexpr float FC_FINAL_BOTTOM = 200.0f;

        demo_gpuuniform1f("uFcTop", FC_START_Y);
        demo_gpuuniform1f("uFcBottom", FC_START_Y);

        for (a = 0; a < 100; a++)
        {
            fcrow[a] = (char *)(Data::fc + PaletteByteCount + 16 + a * SCREEN_WIDTH);
        }
        for (a = 0; a < 16; a++)
        {
            fcrow2[a] = (char *)(Data::fc + PaletteByteCount + 16 + a * SCREEN_WIDTH + 100 * SCREEN_WIDTH);
        }

        Shim::outp(0x3c8, 0);

        for (a = 0; a < static_cast<int>(PaletteByteCount); a++)
            Shim::outp(0x3c9, 0);

        Shim::outp(0x3c8, 0);

        for (a = 0; a < 16 * 3; a++)
            Shim::outp(0x3c9, Data::fc[a + 16]);

        yy = 0;
        ya = 0;

        while (!demo_wantstoquit())
        {
            ya++;
            yy += ya;
            if (yy > 48 * 16)
            {
                yy -= ya;
                ya = -ya * 2 / 3;
                if (ya > -4 && ya < 4) break;
            }
            float t = (float)yy / (48.0f * 16.0f);
            demo_gpuuniform1f("uFcTop", FC_START_Y + (FC_FINAL_TOP - FC_START_Y) * t);
            demo_gpuuniform1f("uFcBottom", FC_START_Y + (FC_FINAL_BOTTOM - FC_START_Y) * t);
            demo_blit();
            demo_vsync();
        }

        demo_gpuuniform1f("uFcTop", FC_FINAL_TOP);
        demo_gpuuniform1f("uFcBottom", FC_FINAL_BOTTOM);

        while (!demo_wantstoquit() && Music::getFrame() < 300)
        {
            demo_vsync();
            demo_blit();
        }

        for (a = 0; a < 16; a++)
        {
            ps = (char *)(Data::fc + 0x10 + a * 3);
            pd = (char *)(backpal + a * 3);
            pd[0] = ps[0];
            pd[1] = ps[1];
            pd[2] = ps[2];
        }

        pp = tmppal;
        for (a = 0; a < 256; a++)
        {
            if (a < 16)
                b = a;
            else
            {
                b = a & 7;
            }
            r = backpal[b * 3 + 0];
            g = backpal[b * 3 + 1];
            b = backpal[b * 3 + 2];
            if ((a & 8) && a > 15)
            {
                r += 16;
                g += 16;
                b += 16;
            }
            if (r > 63) r = 63;
            if (g > 63) g = 63;
            if (b > 63) b = 63;
            *pp++ = static_cast<char>(r);
            *pp++ = static_cast<char>(g);
            *pp++ = static_cast<char>(b);
        }

        Shim::outp(0x3c8, 0);
        for (a = 0; a < static_cast<int>(PaletteByteCount); a++)
        {
            Shim::outp(0x3c9, tmppal[a]);
        }
        lightshift = 9;
        rx = ry = rz = 0;
        ypos = -9000;
        yposa = 0;
        demo_vsync();

        while (!demo_wantstoquit() && Music::getFrame() < 333)
        {
            demo_vsync();
            demo_blit();
        }

        memcpy(pal, backpal, 16 * 3);

        Shim::outp(0x3c7, 0);
        for (a = 0; a < 16 * 3; a++)
            pal[a] = Shim::inp(0x3c9);

        while (frame < 7000 && !demo_wantstoquit())
        {
            if (!Shim::isDemoFirstPart())
            {
                a = Music::getPlusFlags();
                if (a < 0 && a > -16) break;
            }

            repeat = demo_vsync(true);  // advance audio for music sync
            Shim::outp(0x3c8, 0);
            for (a = 0; a < 16 * 3; a++)
                Shim::outp(0x3c9, pal[a]);

            while (repeat--)
            {
                frame++;
                rx += 32;
                ry += 7;
                rx %= 3 * 3600;
                ry %= 3 * 3600;
                rz %= 3 * 3600;

                if (frame > 900)
                {
                    b = frame - 900;
                    a = frame - 900;
                    b = frame - 900;
                    if (b > 50) b = 50;
                    oxp = Common::Data::sin1024[(a * 3) & 1023] * b / 10;
                    oyp = Common::Data::sin1024[(a * 5) & 1023] * b / 10;
                    ozp = (Common::Data::sin1024[(a * 4) & 1023] / 2 + 128) * b / 16;
                    if (frame > 1800)
                    {
                        a = frame - 1800 + 64;
                        if (a > 1024) a = 1024;
                        oxb = (int32_t)(-Common::Data::sin1024[(a * 6) & 1023]) * (int32_t)a / 40L;
                        oyb = (int32_t)(-Common::Data::sin1024[(a * 7) & 1023]) * (int32_t)a / 40L;
                        ozb = (int32_t)(Common::Data::sin1024[(a * 8) & 1023] + 128) * (int32_t)a / 40L;
                    }
                    else
                    {
                        oxb = -Common::Data::sin1024[(a * 6) & 1023];
                        oyb = -Common::Data::sin1024[(a * 7) & 1023];
                        ozb = Common::Data::sin1024[(a * 8) & 1023] + 128;
                    }
                    b = 1800 - frame;
                    if (b < 0)
                    {
                        if (b < -99) b = -99;
                        oyp -= b * b / 2;
                    }
                }

                if (frame > 800)
                {
                    if (frame > 1220 + 789)
                    {
                        if (xscale > 0) xscale -= 1;
                        if (yscale > 0) yscale -= 1;
                        if (zscale > 0) zscale -= 1;
                        if (bscale > 0) bscale -= 1;
                    }
                    else if (frame > 1400 + 789)
                    {
                        if (bscale > 0) bscale -= 8;
                        if (bscale < 0) bscale = 0;
                    }
                    else
                    {
                        if (bscale < 180)
                            bscale += 2;
                        else
                            bscale = 180;
                    }
                    if (bscale > xscale) lightshift = 10;
                }
                else
                {
                    if (frame < 640 + 70)
                    {
                        yposa += 31;
                        ypos += yposa / 40;
                        if (ypos > -300)
                        {
                            ypos -= yposa / 40;
                            yposa = -yposa * boingm / boingd;
                            boingm += 2;
                            boingd++;
                        }
                        if (ypos > -900 && yposa > 0)
                        {
                            jello = (ypos + 900) * 5 / 3;
                            jelloa = 0;
                        }
                    }
                    else
                    {
                        if (ypos > -2800)
                            ypos -= 16;
                        else if (ypos < -2800)
                            ypos += 16;
                    }
                    yscale = xscale = 120 + jello / 30;
                    zscale = 120 - jello / 30;
                    a = jello;
                    jello += jelloa;
                    if ((a < 0 && jello > 0) || (a > 0 && jello < 0))
                    {
                        jelloa = jelloa * 5 / 6;
                    }
                    a = jello / 20;
                    jelloa -= a;
                }

                if (frame > 1280 + 789)
                {
                    b = 1280 + 789 + 64 - frame;
                    if (b < 0) b = 0;
                    for (a = 0; a < 16 * 3; a++)
                        pal[a] = static_cast<char>(backpal[a] * b / 64);
                }
                else if (frame > 700)
                {
                    if (frame < 765)
                    {
                        b = 764 - frame;
                        if (b < 0) b = 0;
                        for (a = 0; a < 16 * 3; a++)
                            pal[a] = static_cast<char>(backpal[a] * b / 64);
                    }
                    else if (frame < 790)
                    {
                        y = 150 + (frame - 765) * 2;
                        memset(Shim::cpuPixels + y * SCREEN_WIDTH, 0, 640);
                        if (frame > 785)
                            for (a = 0; a < 16; a++)
                            {
                                r = g = b = 0;
                                if (a & 1) r += 10;
                                if (a & 2) r += 30;
                                if (a & 4) r += 20;
                                if (a & 8) { r += 16; g += 16; b += 16; }
                                if (r > 63) r = 63;
                                if (g > 63) g = 63;
                                if (b > 63) b = 63;
                                backpal[a * 3 + 0] = static_cast<char>(r);
                                backpal[a * 3 + 1] = static_cast<char>(g);
                                backpal[a * 3 + 2] = static_cast<char>(b);
                            }
                    }
                    else if (frame < 795)
                    {
                        memcpy(pal, backpal, 16 * 3);
                    }
                }
            }

            // Build projected triangles and hand them to the GPU shader.
            reset_triangles();

            if (xscale > 4)
            {
                cmatrix_yxzC(static_cast<uint16_t>(rx), static_cast<uint16_t>(ry), static_cast<uint16_t>(rz), matrix);
                csetmatrix(matrix, 0, 0, 0);
                points2b[0] = 0;
                crotlist((int32_t *)points2b, (int32_t *)Data::points);
                matrix[0] = static_cast<short>(xscale * 64);
                matrix[1] = 0;
                matrix[2] = 0;
                matrix[3] = 0;
                matrix[4] = static_cast<short>(yscale * 64);
                matrix[5] = 0;
                matrix[6] = 0;
                matrix[7] = 0;
                matrix[8] = static_cast<short>(zscale * 64);
                csetmatrix(matrix, 0 + oxp, ypos + 1500 + oyp, zpos + ozp);
                points2[0] = 0;
                crotlist((int32_t *)points2, (int32_t *)points2b);
                if (frame < 800) cliplist((int32_t *)points2);
                points3[0] = 0;
                cprojlist((uint16_t *)points3, (int32_t *)points2);
                ceasypolylist_gpu(Data::epolys, (const int32_t *)points3, false);
            }

            if (frame > 800 && bscale > 4)
            {
                cmatrix_yxzC(static_cast<uint16_t>(3600 - rx / 3), static_cast<uint16_t>(3600 - ry / 3), static_cast<uint16_t>(3600 - rz / 3), matrix);
                csetmatrix(matrix, 0, 0, 0);
                points2b[0] = 0;
                crotlist((int32_t *)points2b, (int32_t *)Data::pointsb);
                matrix[0] = static_cast<short>(bscale * 64);
                matrix[1] = 0;
                matrix[2] = 0;
                matrix[3] = 0;
                matrix[4] = static_cast<short>(bscale * 64);
                matrix[5] = 0;
                matrix[6] = 0;
                matrix[7] = 0;
                matrix[8] = static_cast<short>(bscale * 64);
                csetmatrix(matrix, 0 + oxb, ypos + 1500 + oyb, zpos + ozb);
                points2[0] = 0;
                crotlist((int32_t *)points2, (int32_t *)points2b);
                points3[0] = 0;
                cprojlist((uint16_t *)points3, (int32_t *)points2);
                ceasypolylist_gpu(Data::epolysb, (const int32_t *)points3, true);
            }

            float fade = 1.0f;
            if (frame > 1280 + 789)
            {
                int fb = 1280 + 789 + 64 - frame;
                if (fb < 0) fb = 0;
                fade = (float)fb / 64.0f;
            }

            float fcFade = 1.0f;
            if (bscale > 0)
            {
                fcFade = 1.0f - (float)bscale / 40.0f;
                if (fcFade < 0.0f) fcFade = 0.0f;
            }

            demo_gpuuniform4fv("uTris", triData, MAX_TRI * 3);
            demo_gpuuniform1f("uFade", fade);
            demo_gpuuniform1f("uFcFade", fcFade);
            demo_blit();

            // End once the fade-out is done, not just on music sync: standalone
            // launches would otherwise run all 7000 frames into a black screen.
            if (frame >= 1280 + 789 + 64) break;
        }

        demo_cleargputeffect();
    }
}
