#include "Parts/Common.h"

#include <cstdlib>

#include "LENS_MAIN_Data.h"
#include "LensFaceBgData.h"
#include "LensFaceTileData.h"

namespace Lens
{
    namespace
    {
        short * pathdata1 = nullptr;
        short * pathdata2 = nullptr;

        Palette palette{};
        char lensexb[64784 + 4096]{};

        // Hi-res face textures (upscayl-standard). Loaded once in main().
        unsigned char * faceBgRgb = nullptr;
        int faceBgW = 0, faceBgH = 0;
        unsigned char * faceTileRgb = nullptr;
        int faceTileW = 0, faceTileH = 0;

        // Lens radius, read from lensex0 (lenswid) in main().
        float lensRadius = 0.0f;

        // --- GPU firfade reveal: two diagonal fronts sweeping from center ---
        // Procedural approach (from sr-port): discrete diagonal bands sweep from center outwards.
        // uTime goes 0.0 -> 1.0 over the reveal duration.
        static const char * firfadeFrag =
            "precision highp float;\n"
            "uniform vec3 uResolution;\n"
            "uniform vec4 uViewport;\n"
            "uniform sampler2D uFace;\n"
            "uniform float uTime;\n"
            "void main() {\n"
            "  vec2 p = (gl_FragCoord.xy - uViewport.xy) / uViewport.zw;\n"
            "  if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) {\n"
            "    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    return;\n"
            "  }\n"
            "  const float xoff = 0.25;\n"
            "  float yinv = 1.0 - p.y;\n"
            "  float xlen = 1.0 - 2.0 * xoff;\n"
            "  float ybase = 0.2 * floor(yinv * 5.0);\n"
            "  float xbase = xoff + ybase * xlen;\n"
            "  float xpos = xbase + (1.0 - xbase) * uTime;\n"
            "  float xx = xpos + (yinv - ybase) * xlen;\n"
            "  if (p.x > xx || p.x < xx - uTime) {\n"
            "    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "  } else {\n"
            "    vec2 uv = p; uv.y = yinv;\n"
            "    gl_FragColor = texture2D(uFace, uv);\n"
            "  }\n"
            "}\n";

        // --- GPU lens: hemispherical glass dome over the face bitmap ---
        // Light comes from the top-right, so the specular highlight sits
        // toward the upper-right of the lens.
        // Y-FLIP: stb_image loads top-down, glTexImage2D uploads as-is,
        // so texture v=0 = image top. gl_FragCoord.y=0 = screen bottom.
        // Fix: uv.y = 1.0 - p.y and center.y = 1.0 - uLensCenter.y
        static const char * lensFrag =
            "precision highp float;\n"
            "uniform vec3 uResolution;\n"
            "uniform vec4 uViewport;\n"   // 4:3 letterbox rect in window px
            "uniform sampler2D uFace;\n"  // upscaled face bitmap
            "uniform vec2 uLensCenter;\n" // lens centre in [0,1] UV
            "uniform float uLensRadius;\n" // lens radius as a fraction of viewport width
            "uniform float uDebugTile;\n"   // 1.0 = red tile grid debug view
            "void main() {\n"
            "  vec2 p = (gl_FragCoord.xy - uViewport.xy) / uViewport.zw;\n"
            "  if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) {\n"
            "    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    return;\n"
            "  }\n"
            "  if (uDebugTile > 0.5) {\n"
            "    // Square debug cells in SCREEN space: cellsX across, cellsY down,\n"
            "    // with cellsY/cellsX = viewport aspect so each cell is a square.\n"
            "    // Comparing this with the rotozoom grid exposes aspect drift.\n"
            "    float aspect = uViewport.z / uViewport.w;\n"
            "    float cellsX = 6.0;\n"
            "    float cellsY = floor(cellsX * aspect + 0.5);\n"
            "    vec2 uv = p; uv.y = 1.0 - uv.y;\n"
            "    vec2 cell = fract(uv * vec2(cellsX, cellsY));\n"
            "    vec3 dbg = vec3(1.0, 0.0, 0.0);\n"
            "    vec2 g = abs(cell - 0.5);\n"
            "    if (g.x > 0.492 || g.y > 0.492) dbg = vec3(1.0, 1.0, 0.0);\n"
            "    gl_FragColor = vec4(dbg, 1.0);\n"
            "    return;\n"
            "  }\n"
            "  vec2 centerPx = uViewport.xy + vec2(uLensCenter.x, 1.0 - uLensCenter.y) * uViewport.zw;\n"
            "  float radiusPx = uLensRadius * uViewport.z;\n"
            "  vec2 deltaPx = gl_FragCoord.xy - centerPx;\n"
            "  vec2 d = deltaPx / radiusPx;\n"
            "  float dist = length(d);\n"
            "  vec3 col;\n"
            "  if (dist < 1.0) {\n"
            "    float h = sqrt(max(0.0, 1.0 - dist * dist));\n"
            "    float zoom = 1.0 + 0.38 * h;\n"
            "    vec2 samplePx = centerPx + deltaPx / zoom;\n"
            "    vec2 uv = (samplePx - uViewport.xy) / uViewport.zw;\n"
            "    uv.y = 1.0 - uv.y;\n"
            "    col = texture2D(uFace, uv).rgb;\n"
            "    vec3 N = normalize(vec3(d.x, d.y, h));\n"
            "    vec3 L = normalize(vec3(0.55, 0.60, 0.60));\n"
            "    float spec = pow(max(dot(N, L), 0.0), 50.0);\n"
            "    col += vec3(spec) * 0.75;\n"
            "    float rim = smoothstep(0.80, 1.0, dist);\n"
            "    col = mix(col, col * 0.82, rim);\n"
            "  } else {\n"
            "    vec2 uv = p; uv.y = 1.0 - uv.y;\n"
            "    col = texture2D(uFace, uv).rgb;\n"
            "  }\n"
            "  gl_FragColor = vec4(col, 1.0);\n"
            "}\n";

        // --- GPU rotozoom: face tile wrapped (checkerboard) + rotation/zoom ---
        // Y-FLIP: same texture orientation issue as lensFrag
        // EXACT SAME LOGIC AS LENS: window pixel -> viewport [0,1] -> logical 320x200.
        // Difference: NO CLIPPING, so pattern tiles across FULL SCREEN with SAME RATIO as lens.
        static const char * rotozoomFrag =
            "precision highp float;\n"
            "uniform vec3 uResolution;\n"   // xy = window size, z = window aspect
            "uniform vec4 uViewport;\n"     // 4:3 reference rect (defines the tile size)
            "uniform sampler2D uTile;\n"   // square face tile
            "uniform vec2 uRotPos;\n"      // (x, y) from pathdata2
            "uniform vec2 uRotGrad;\n"     // (xa, ya) from pathdata2
            "uniform float uDebugTile;\n"   // 1.0 = red grid, 2.0 = 1:1 tile view
            "uniform float uFade;\n"      // 0..1 blend towards the white flash
            "void main() {\n"
            // p must NOT be clamped: it scales the tile, and letting it grow
            // outside the 4:3 rect makes fract() extend the checkerboard to the
            // window edges with the tile size and ratio unchanged. Re-adding a
            // bounds check here reverts the pattern to a 4:3 square.
            "  vec2 p = (gl_FragCoord.xy - uViewport.xy) / uViewport.zw;\n"
"  if (uDebugTile >= 2.0) {\n"
            "    // 1:1 tile view: native 1:1 aspect, centered on screen\n"
            "    // Use screen coordinates directly (full window), maintain 1:1 aspect\n"
            "    float winAspect = uResolution.x / uResolution.y;\n"
            "    vec2 uv;\n"
            "    if (winAspect >= 1.0) {\n"
            "      // landscape: pillarbox (fit height)\n"
            "      float scale = 1.0 / uResolution.x * uResolution.y; // 1/aspect\n"
            "      float offset = (1.0 - 1.0 / (uResolution.x / uResolution.y)) * 0.5;\n"
            "      if (gl_FragCoord.x / uResolution.x < (1.0 - 1.0 / (uResolution.x / uResolution.y)) * 0.5 ||\n"
            "          gl_FragCoord.x / uResolution.x > 1.0 - (1.0 - 1.0 / (uResolution.x / uResolution.y)) * 0.5) {\n"
            "        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "        return;\n"
            "      }\n"
            "      float u = (gl_FragCoord.x / uResolution.x - (1.0 - 1.0 / (uResolution.x / uResolution.y)) * 0.5) * (uResolution.x / uResolution.y);\n"
            "      float v = 1.0 - gl_FragCoord.y / uResolution.y;\n"
            "      gl_FragColor = texture2D(uTile, vec2(u, v));\n"
            "      return;\n"
            "    } else {\n"
            "      // portrait: letterbox\n"
            "      float scale = uResolution.x / uResolution.y;\n"
            "      float offset = (1.0 - uResolution.x / uResolution.y) * 0.5;\n"
            "      if (gl_FragCoord.y / uResolution.y < (1.0 - uResolution.x / uResolution.y) * 0.5 ||\n"
            "          gl_FragCoord.y / uResolution.y > 1.0 - (1.0 - uResolution.x / uResolution.y) * 0.5) {\n"
            "        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "        return;\n"
            "      }\n"
            "      float u = gl_FragCoord.x / uResolution.x;\n"
            "      float v = (1.0 - gl_FragCoord.y / uResolution.y - (1.0 - uResolution.x / uResolution.y) * 0.5) / (uResolution.x / uResolution.y);\n"
            "      gl_FragColor = texture2D(uTile, vec2(u, v));\n"
            "      return;\n"
            "    }\n"
            "  }\n"
            "  if (uDebugTile > 0.5) {\n"
            "    // Red grid debug\n"
            "    float aspect = uViewport.z / uViewport.w;\n"
            "    float cellsX = 6.0;\n"
            "    float cellsY = floor(cellsX * (uViewport.w / uViewport.z) + 0.5);\n"
            "    vec2 cell = fract(p * vec2(cellsX, cellsY));\n"
            "    vec3 dbg = vec3(1.0, 0.0, 0.0);\n"
            "    vec2 g = abs(cell - 0.5);\n"
            "    if (g.x > 0.492 || g.y > 0.492) dbg = vec3(1.0, 1.0, 0.0);\n"
            "    gl_FragColor = vec4(dbg, 1.0);\n"
            "    return;\n"
            "  }\n"
            "  // Normal rotozoom\n"
            "  float px = p.x * 160.0;\n"
            "  float py = p.y * 100.0;\n"
            "  \n"
            "  float Xadd = uRotGrad.x * 64.0;\n"
            "  float Yadd = uRotGrad.y * 64.0;\n"
            "  float sc = 307.0 / 256.0;\n"
            "  float X = uRotPos.x + px * (Yadd / 65536.0) + py * (Xadd * 307.0 / 256.0 / 65536.0);\n"
            "  float Y = uRotPos.y - px * (Xadd / 65536.0) + py * (Yadd * sc / 65536.0);\n"
            "  vec2 uv = fract(vec2(X, Y) / 256.0);\n"
            "  uv.y = 1.0 - uv.y;\n"
            "  vec3 col = texture2D(uTile, uv).rgb;\n"
            "  gl_FragColor = vec4(mix(col, vec3(1.0), uFade), 1.0);\n"
            "}\n";
    }

    void part1()
    {
        int frame = 0;

        if (!Shim::isDemoFirstPart())
        {
            if (Music::getPlusFlags() > -30)
                while (!demo_wantstoquit() && Music::getPlusFlags() < -6)
                {
                    AudioPlayer::Update(true);
                }
        }

        demo_vsync();

        Music::setFrame(0);

        // GPU firfade: procedural diagonal sweep using uTime (0.0 to 1.0)
        demo_setgputeffect(firfadeFrag);
        demo_setgputexture_rgb("uFace", faceBgRgb, faceBgW, faceBgH);

        while (!demo_wantstoquit() && frame < 300)
        {
            float uTime = 0.0f;
            if (frame < 80) {
                // Map frame 0-79 to uTime 0.0-1.0 for the reveal
                uTime = (float)frame / 80.0f;
                if (uTime > 1.0f) uTime = 1.0f;
            } else {
                uTime = 1.0f; // fully revealed
            }
            demo_gpuuniform1f("uTime", uTime);

            demo_blit();

            frame += demo_vsync();
        }
    }

    void part2()
    {
        int x, y;
        int a;
        int frame = 0, uframe = 0;
        uframe = frame = 0;

        demo_setgputeffect(lensFrag);
        demo_setgputexture_rgb("uFace", faceBgRgb, faceBgW, faceBgH);
        demo_gpuuniform1f("uDebugTile", getenv("SR_DEBUG_TILE") ? 1.0f : 0.0f);

        // Original: runs while uframe < 715, one waitb() per frame
        // uframe accumulates waitb() return values (1 per VBL at 70Hz)
        while (!demo_wantstoquit() && uframe < 715)
        {
            // Single vsync with audio update per frame (like original waitb())
            a = demo_vsync(true);

            x = pathdata1[frame * 2 + 0];
            y = pathdata1[frame * 2 + 1];

            demo_gpuuniform2f("uLensCenter", (float)x / 320.0f, (float)y / 200.0f);
            demo_gpuuniform1f("uLensRadius", lensRadius / 320.0f);

            demo_blit();

            uframe += a;
            if (a > 3) a = 3;
            frame += a;
        }

        // Original: extra 5 frames (uframe 715-720) for fade/transition
        while (!demo_wantstoquit() && uframe < 720)
        {
            uframe += demo_vsync(true);
        }
    }

    void part3()
    {
        int x, y, xa, ya;
        int frame = 0;

        // Same 4:3 letterbox viewport as lens (correct aspect ratio)
        // but shader will tile pattern across FULL SCREEN
        ::g_respectRatio = true;

demo_setgputeffect(rotozoomFrag);
        demo_setgputexture_rgb("uTile", faceTileRgb, faceTileW, faceTileH);
        // uDebugTile: 0=off, 1=red grid debug, 2=1:1 tile view (1:1 aspect, no zoom/rotation)
        const float debugTile = getenv("SR_DEBUG_TILE") ? (float)atoi(getenv("SR_DEBUG_TILE")) : 0.0f;
        demo_gpuuniform1f("uDebugTile", debugTile);

        // SR_LENS_PART3=<frame>: jump straight to the rotozoom at that path frame
        const char * jump = getenv("SR_LENS_PART3");
        frame = jump ? atoi(jump) : 0;

        // SR_LENS_FREEZE=<frame>: hold zoom and rotation at that path frame
        // so two renders differ only by the tile asset (e.g. to compare aspect).
        const char * freeze = getenv("SR_LENS_FREEZE");
        const int freezeFrame = freeze ? atoi(freeze) : -1;

        // Original (MAIN.C): the last 128 frames ramp the palette to white
        // (a = (frame - (2000-128)) / 2, clamped to 63) and the part ends on a
        // full-white frame, so the next part can fade in from white.
        constexpr int END_FRAME = 2000;
        constexpr int WHITE_FADE_START = END_FRAME - 128;
        constexpr int WHITE_HOLD_FRAMES = 8;

        // Original: runs while frame < 2000 AND dis_musplus() <= -4
        // Music sync checked INSIDE loop (not before entry)
        while (!demo_wantstoquit() && frame < END_FRAME)
        {
            if (!jump && Music::getPlusFlags() > -4) break;

            const int src = (freezeFrame >= 0) ? freezeFrame : frame;
            x = pathdata2[src * 4 + 0];
            y = pathdata2[src * 4 + 1];
            xa = pathdata2[src * 4 + 2];
            ya = pathdata2[src * 4 + 3];

            demo_gpuuniform2f("uRotPos", (float)x, (float)y);
            demo_gpuuniform2f("uRotGrad", (float)xa, (float)ya);
            demo_gpuuniform1f("uDebugTile", debugTile);

            int a = (frame - WHITE_FADE_START) / 2;
            if (a < 0) a = 0;
            if (a > 63) a = 63;
            demo_gpuuniform1f("uFade", (float)a / 63.0f);

            demo_blit();

            // Single vsync with audio update per frame
            frame += demo_vsync(true);
        }

        for (int i = 0; i < WHITE_HOLD_FRAMES && !demo_wantstoquit(); i++)
        {
            demo_gpuuniform1f("uFade", 1.0f);
            demo_blit();
            demo_vsync(true);
        }
    }

    void main()
    {
        CLEAR(lensexb);
        CLEAR(palette);

        pathdata1 = nullptr;
        pathdata2 = nullptr;
        lensRadius = 0.0f;

        // Load the hi-res face textures.
        faceBgRgb = demo_loadpng_rgb_mem(Data::face_bg_png, Data::face_bg_png_size, &faceBgW, &faceBgH);
        faceTileRgb = demo_loadpng_rgb_mem(Data::face_tile_png, Data::face_tile_png_size, &faceTileW, &faceTileH);

        memcpy(lensexb, Data::lensexbBase, Data::lensexbBase_size);

        int a;
        Shim::clearScreen();

        Shim::outp(0x3c8, 0);
        for (a = 0; a < static_cast<int>(PaletteByteCount); a++)
            Shim::outp(0x3c9, 0);

        a = *(short *)(Data::lensexp + 2);
        pathdata1 = (short *)(Data::lensexp + 4);
        pathdata2 = (short *)(Data::lensexp + 4 + 2 * a);

        memcpy(palette, lensexb + 16, PaletteByteCount);

        // back = (uint8_t *)(lensexb + 16 + PaletteByteCount); // unused (GPU uses faceBgRgb)

        lensRadius = (float)(*(short *)(Data::lensex0 + 0)) * 0.5f;

        demo_vsync();

        Common::setpalarea(palette, 0, PaletteColorCount);

        // SR_LENS_SKIP: skip sub-parts while debugging.
        //   "1" or unset -> run all parts (firefade -> lens -> rotozoom)
        //   "2" -> skip firefade (part1), start at lens (part2 -> rotozoom)
        const char * skip = getenv("SR_LENS_SKIP");
        const bool skipPart1 = skip && skip[0] == '2';

        if (!skipPart1)
        {
            if (!demo_wantstoquit()) part1();

            while (!demo_wantstoquit() && Music::getPlusFlags() < -20)
            {
                AudioPlayer::Update(true);
            }

            demo_vsync();
        }

        if (!demo_wantstoquit()) part2();

        if (!demo_wantstoquit()) part3();

        demo_freepng(faceBgRgb);
        demo_freepng(faceTileRgb);
        faceBgRgb = nullptr;
        faceTileRgb = nullptr;
    }
}
