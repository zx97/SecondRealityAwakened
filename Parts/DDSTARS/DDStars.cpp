#include <cmath>
#include <cstring>
#include "Parts/Common.h"

#include "DDStars_Data.h"
#include "DDStars_Text.h"
#include "Graphics/VectorFont.h"

namespace DDStars
{
    constexpr const int TEX_W = 320;
    constexpr const int TEX_H = 400;
    constexpr const int TEXT_ROW_BYTES = 40;
    constexpr const int TEXT_MAX_ROWS = 99;

    constexpr const int MAX_STARS = 3000;
    constexpr const float STAR_MAX_Z = 30.0f;

    constexpr const int TEXT2_DONE_FRAME = 2700;
    constexpr const int HARD_CAP_FRAME = 6000;
    constexpr const int FADE_FRAMES = 140;

    static const char * backgroundFrag =
        "precision mediump float;\n"
        "uniform vec3 uResolution;\n"
        "uniform vec2 uSourceSize;\n"
        "uniform float uFade;\n"
        "uniform sampler2D uTexOverlay;\n"
        "\n"
        "void main() {\n"
        "  vec2 ouv = gl_FragCoord.xy / uResolution.xy - 0.5;\n"
        "  float winAspect = uResolution.x / uResolution.y;\n"
        "  float imgAspect = 0.8;\n"
        "  if (winAspect > imgAspect) { ouv.x *= winAspect / imgAspect; }\n"
        "  else { ouv.y *= imgAspect / winAspect; }\n"
"    vec3 col = vec3(0.0);\n"
        "  if (abs(ouv.x) <= 0.5 && abs(ouv.y) <= 0.5) {\n"
        "    vec2 tuv = vec2((ouv.x + 0.5) * uSourceSize.x, (0.5 - ouv.y) * uSourceSize.y);\n"
        "    vec3 ov = texture2D(uTexOverlay, tuv).bgr;\n"
        "    float om = max(ov.r, max(ov.g, ov.b));\n"
        "    col = mix(col, ov * 1.2, smoothstep(0.02, 0.45, om));\n"
        "  }\n"
        "  float dith = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "  col += (dith - 0.5) * (1.5 / 255.0);\n"
        "  gl_FragColor = vec4(col * uFade, 1.0);\n"
        "}\n";

    struct Star
    {
        float dx, dy;
        float z;
        float spd;
    };

    struct StarVertex
    {
        float x, y, z;
        float depth;
        float size;
        float colorIdx;
    };

    namespace
    {
        int txtOpen = 0;
        int txtClose = 0;
        int txtPage = 0;

        Star stars[MAX_STARS]{};
        StarVertex vertices[MAX_STARS]{};

        int rand_state = 7;

        int rand_val()
        {
            rand_state = rand_state * 1103515245 + 12345;
            return (unsigned int)(rand_state / 65536) & 32767;
        }

        float rand01()
        {
            return (float)(rand_val() % 1000) / 1000.0f;
        }

        void blitText()
        {
            // The vector text is drawn as SDF quads over the stars. The bitmap
            // (planar) path is replaced: the part is now crisp at any resolution.
            if (txtOpen < 99) ++txtOpen;
            if (txtClose > 0) --txtClose;

            // `use` grows at a quarter of the frame rate so the reveal curtain
            // is slower and reads line-by-line.
            int use = (txtOpen < txtClose ? txtOpen : txtClose) / 4;
            if (use <= 0) return;
            if (use == 1) use = 2;
            if (use > 99) use = 99;

            // Choose the active block from the page offset.
            const char * text = (txtPage >= 100 * 80) ? kTextBlock2 : kTextBlock1;

            static const char * set =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
                "!?.,:'()&+-*= ";
            static bool built = false;
            if (!built)
            {
                int aw = 0, ah = 0, apad = 0;
                const unsigned char * atlas = VectorFont::BuildSdfAtlas(set, 56, aw, ah, apad);
                demo_sdftexture(atlas, aw, ah);
                built = true;
            }

            static const float IDENT[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            };

            const float aspect = demo_windowaspect();
            // Sizing matches the original bitmap block: full width, ~25% height.
            const float fontNdc = 0.075f;
            const float spacing = 0.9f;
            const float lineGap = fontNdc * 1.5f;

            // Collect all lines (the target width must be stable for the whole
            // block so the curtain reveals line-by-line without re-flowing).
            char lines[99][128];
            int totalLines = 0;
            {
                const char * p = text;
                while (*p && totalLines < 99)
                {
                    const char * nl = p;
                    while (*nl && *nl != '\n') ++nl;
                    int len = (int)(nl - p);
                    if (len > 126) len = 126;
                    memcpy(lines[totalLines], p, (size_t)len);
                    lines[totalLines][len] = 0;
                    ++totalLines;
                    p = nl + (*nl ? 1 : 0);
                }
            }
            int lineCount = totalLines;
            if (lineCount > use) lineCount = use;
            if (lineCount == 0) return;

            // Justified block width = width of the longest line, shrunk 20%.
            const float scale = fontNdc / 56.0f;
            float targetWidth = 0;
            for (int i = 0; i < totalLines; ++i)
            {
                float w = 0;
                for (const char * q = lines[i]; *q; ++q)
                    w += (float)VectorFont::GetGlyphAdvance(*q) * scale * spacing;
                if (w > targetWidth) targetWidth = w;
            }
            targetWidth *= 0.7f;

            // Centre the whole block vertically around the middle of the screen.
            float blockTop = 0.5f * (float)(totalLines - 1) * lineGap + 0.5f * fontNdc;

            static GpuTexVertex quads[32 * 96 * 4];
            int quadCount = 0;

            for (int i = 0; i < lineCount && quadCount < 32 * 96; ++i)
            {
                float baseY = blockTop - (float)i * lineGap;
                int n = VectorFont::BuildTextQuadsJustified(
                    lines[i], quads + quadCount * 4, 96,
                    baseY, fontNdc, aspect, targetWidth, spacing);
                quadCount += n;
            }

            if (quadCount > 0)
                demo_drawsdftext(quads, quadCount, IDENT, 1.0f, 1.0f, 1.0f);
        }
    }

    void main()
    {
        demo_changemode(SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT);
        demo_setgputeffect(backgroundFrag);

        // The overlay texture is native-res; the source (vram) occupies only
        // srcW/srcH of it top-left. Tell the shader the source fraction so the
        // text is not sampled from a tiny corner.
        demo_gpuuniform2f("uSourceSize",
                          (float)SCREEN_WIDTH / (float)VIRTUAL_SCREEN_WIDTH,
                          (float)DOUBlE_SCREEN_HEIGHT / (float)VIRTUAL_SCREEN_HEIGHT);

        Shim::startpixel = 0;
        Shim::setpal(0, 0, 0, 0);
        Shim::setpal(15, 63, 63, 63);

        for (int i = 0; i < MAX_STARS; i++)
        {
            stars[i].dx = rand01() * 2.0f - 1.0f;
            stars[i].dy = rand01() * 2.0f - 1.0f;
            stars[i].z = rand01() * STAR_MAX_Z;
            stars[i].spd = 0.15f + rand01() * 0.25f;
        }

        txtOpen = -9999;
        txtClose = 10000;
        txtPage = 0;

        int frame = 0;
        int fadeLeft = -1;
        int lastOrder = -1;
        int lastRow = -1;
        int frozenCount = 0;

        while (!demo_wantstoquit())
        {
            demo_vsync(true);  // advance audio for music sync

            if (frame == 350)
            {
                txtPage = 80;
                txtOpen = -256;
                txtClose = 1200;
            }
            if (frame == 1400)
            {
                txtPage = 101 * 80;
                txtOpen = -256;
                txtClose = 1200;
            }

            float density = (float)frame / 2800.0f;
            if (density > 1.0f) density = 1.0f;
            int active = (int)(density * (float)MAX_STARS);

            int visibleCount = 0;
            for (int i = 0; i < MAX_STARS; i++)
            {
                Star &s = stars[i];
                s.z -= s.spd;
                if (s.z < 0.2f) s.z = STAR_MAX_Z;
                if (i >= active) continue;

                float closeness = 1.0f - s.z / STAR_MAX_Z;
                float rad = 0.03f + closeness * closeness * 1.4f;
                float sx = s.dx * rad;
                float sy = s.dy * rad;
                if (sx < -1.1f || sx > 1.1f || sy < -1.1f || sy > 1.1f) continue;

                vertices[visibleCount].x = sx;
                vertices[visibleCount].y = sy;
                vertices[visibleCount].z = 0.0f;
                vertices[visibleCount].depth = 8800.0f + (s.z / STAR_MAX_Z) * 400.0f;
                vertices[visibleCount].size = 1.0f + 5.0f * closeness * closeness;
                vertices[visibleCount].colorIdx = 8.0f;
                ++visibleCount;
            }

            float fade = 1.0f;
            if (fadeLeft >= 0)
            {
                fade = (float)fadeLeft / (float)FADE_FRAMES;
                if (fadeLeft == 0) break;
                --fadeLeft;
            }
            else
            {
                int order = Music::getOrder();
                int row = Music::getRow();
                if (order == lastOrder && row == lastRow)
                    ++frozenCount;
                else
                    frozenCount = 0;
                lastOrder = order;
                lastRow = row;

                bool textDone = frame >= TEXT2_DONE_FRAME;
                bool musicDone = frame >= TEXT2_DONE_FRAME && frozenCount > 100;
                if ((textDone && musicDone) || frame >= HARD_CAP_FRAME)
                    fadeLeft = FADE_FRAMES;
            }
            demo_gpuuniform1f("uFade", fade);

            blitText();
            demo_drawpoints(vertices, visibleCount, sizeof(StarVertex), 1.0f);
            demo_blit();

            frame++;
        }

        demo_cleargputeffect();
    }
}
