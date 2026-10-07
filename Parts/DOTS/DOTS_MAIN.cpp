#include <cmath>
#include "Parts/Common.h"

namespace Dots
{
    constexpr const int MAX_DOTS = 512;
    constexpr const int FRAME_COUNT = 2450;

    static const char * backgroundFrag =
        "precision mediump float;\n"
        "uniform vec3 uResolution;\n"
        "uniform float uFrame;\n"
        "void main(){\n"
        "  vec2 uv = gl_FragCoord.xy / uResolution.xy;\n"
        "  float floorIn = clamp(uFrame / 500.0, 0.0, 1.0);\n"
        "  vec3 col;\n"
        "  if (uv.y < 0.5) {\n"
        "    float t = uv.y * 2.0;\n"
        "    float g = 0.02 + 0.18 * (1.0 - t) * (1.0 - t);\n"
        "    col = vec3(g) * floorIn;\n"
        "  } else {\n"
        "    col = vec3(0.0);\n"
        "  }\n"
        "  float cs = 0.0;\n"
        "  if (uFrame > 2360.0 && uFrame <= 2400.0) cs = (uFrame - 2360.0) / 32.0;\n"
        "  else if (uFrame > 2400.0) cs = 1.0 + (uFrame - 2400.0) / 32.0;\n"
        "  if (cs > 1.0) col = vec3(2.0 - cs);\n"
        "  else if (cs > 0.0) col = col + (vec3(1.0) - col) * cs;\n"
        "  gl_FragColor = vec4(col, 1.0);\n"
        "}\n";

    namespace
    {
        bool quit = false;

        struct Dot
        {
            float x, y, z;
            float vy;
            int phase;
        };

        Dot dots[MAX_DOTS]{};
    }

    // Raw table values: 1.0 == 256 (8-bit fixed point), like the original.
    static inline float isin_raw(int deg)
    {
        return (float)(Common::Data::sin1024[deg & 1023]);
    }

    static inline float icos_raw(int deg)
    {
        return (float)(Common::Data::sin1024[(deg + 256) & 1023]);
    }

    static int rand_state = 10;
    static int rand_val()
    {
        rand_state = rand_state * 1103515245 + 12345;
        return (unsigned int)(rand_state / 65536) & 32767;
    }

    struct DotVertex
    {
        float x, y, z;
        float depth;
        float size;
        float colorIdx;
    };

    void main()
    {
        quit = false;

        demo_changemode(SCREEN_WIDTH, SCREEN_HEIGHT);
        demo_setgputeffect(backgroundFrag);

        for (int i = 0; i < MAX_DOTS; i++)
        {
            float angle = (float)i * 2.399963f;
            float radius = 5000.0f + (float)(i % 7) * 1000.0f;
            dots[i].x = cosf(angle) * radius;
            dots[i].y = -16000.0f - (float)(i % 9) * 2500.0f;
            dots[i].z = sinf(angle) * radius;
            dots[i].vy = -100.0f - (float)(i % 4) * 60.0f;
            dots[i].phase = 0;
        }

        int frame = 0;
        int f = 0;
        int rot = 0;
        int rots = 0;
        int rota = -64;
        int gravity = 3;
        const int gravityd = 13;
        float dropper = 22000.0f;
        int dotIdx = 0;

        while (!quit && !demo_wantstoquit() && frame < FRAME_COUNT)
        {
            demo_vsync();

            frame++;
            if (frame == 500) f = 0;

            Dot &d = dots[dotIdx];
            dotIdx = (dotIdx + 1) % MAX_DOTS;

            if (frame < 500)
            {
                d.x = isin_raw(f * 11) * 40.0f;
                d.y = icos_raw(f * 13) * 10.0f - dropper;
                d.z = isin_raw(f * 17) * 40.0f;
                d.vy = 0;
            }
            else if (frame < 900)
            {
                d.x = icos_raw(f * 15) * 55.0f;
                d.y = dropper;
                d.z = isin_raw(f * 15) * 55.0f;
                d.vy = -260.0f;
            }
            else if (frame < 1700)
            {
                float a = isin_raw(frame & 1023) / 8.0f;
                d.x = icos_raw(f * 66) * a;
                d.y = 8000.0f;
                d.z = isin_raw(f * 66) * a;
                d.vy = -300.0f;
            }
            else if (frame < 2360)
            {
                d.x = (float)(rand_val() - 16384);
                d.y = (float)(8000 - rand_val() / 2);
                d.z = (float)(rand_val() - 16384);
                d.vy = 0;
                if (frame > 1900 && !(frame & 31) && gravity > 0) gravity--;
            }

            d.vy += (float)gravity;
            d.y += d.vy;

            if (d.y >= 8105.0f)
            {
                d.vy = -d.vy * ((float)gravityd / 16.0f);
                d.y += d.vy;
            }

            if (dropper > 4000.0f) dropper -= 100.0f;

            if (frame > 1900)
            {
                rot += rota / 64;
                rota--;
            }
            else
            {
                rot = (int)isin_raw(rots);
            }
            rots += 2;
            f++;

            float rotcos = icos_raw(rot) * 64.0f;
            float rotsin = isin_raw(rot) * 64.0f;

            int spawnedIdx = (dotIdx + MAX_DOTS - 1) % MAX_DOTS;
            for (int i = 0; i < MAX_DOTS; i++)
            {
                if (i == spawnedIdx) continue;
                Dot &od = dots[i];
                od.vy += (float)gravity;
                od.y += od.vy;
                if (od.y >= 8105.0f)
                {
                    od.vy = -od.vy * ((float)gravityd / 16.0f);
                    od.y += od.vy;
                }
                if (od.y < -26000.0f)
                {
                    od.y = -26000.0f;
                    float av = od.vy >= 0.0f ? od.vy : -od.vy;
                    od.vy = av * 0.5f;
                    if (od.vy < 50.0f) od.vy = 50.0f;
                }
            }

            DotVertex vertices[MAX_DOTS * 2];
            int visibleCount = 0;

            for (int i = 0; i < MAX_DOTS; i++)
            {
                Dot &dot = dots[i];

                float depth = dot.z * rotcos - dot.x * rotsin;
                float bp = depth / 65536.0f + 9000.0f;
                if (bp < 1.0f) bp = 1.0f;

                float sumX = dot.x * rotcos + dot.z * rotsin;
                float sx = ((sumX / 256.0f + sumX / 2048.0f) / bp) / (float)SCREEN_WIDTH + 0.5f;

                float shy = (8192.0f * 64.0f / bp) / 200.0f + 0.5f;
                if (sx < -0.1f || sx > 1.1f || shy < -0.1f || shy > 1.1f) continue;

                float sizeFactor = 8000.0f / bp;

                vertices[visibleCount].x = (sx * 2.0f - 1.0f) * 0.65f;
                vertices[visibleCount].y = -((shy * 2.0f - 1.0f)) * 0.65f;
                vertices[visibleCount].z = 0.0f;
                vertices[visibleCount].depth = bp;
                vertices[visibleCount].size = sizeFactor * 12.0f * 0.65f * 1.0f;
                vertices[visibleCount].colorIdx = 31.0f;
                visibleCount++;
            }

            for (int i = 0; i < MAX_DOTS; i++)
            {
                Dot &dot = dots[i];

                float depth = dot.z * rotcos - dot.x * rotsin;
                float bp = depth / 65536.0f + 9000.0f;
                if (bp < 1.0f) bp = 1.0f;

                float sumX = dot.x * rotcos + dot.z * rotsin;
                float sx = ((sumX / 256.0f + sumX / 2048.0f) / bp) / (float)SCREEN_WIDTH + 0.5f;

                float sy = (dot.y * 64.0f / bp) / 200.0f + 0.5f;

                if (sx < -0.1f || sx > 1.1f || sy < -0.1f || sy > 1.1f) continue;

                float screenX = (sx * 2.0f - 1.0f) * 0.65f;
                float screenY = -(sy * 2.0f - 1.0f) * 0.65f;

                float sizeFactor = 8000.0f / bp;
                float dotSize = sizeFactor * 12.0f * 0.65f * 1.4f;

                vertices[visibleCount].x = screenX;
                vertices[visibleCount].y = screenY;
                vertices[visibleCount].z = 0.0f;
                vertices[visibleCount].depth = bp;
                vertices[visibleCount].size = dotSize;
                vertices[visibleCount].colorIdx = 20.0f;
                visibleCount++;
            }

            demo_gpuuniform1f("uFrame", (float)frame);
            demo_drawpoints(vertices, visibleCount, sizeof(DotVertex), 1.0f);
            demo_blit();
        }

        demo_cleargputeffect();
    }
}
