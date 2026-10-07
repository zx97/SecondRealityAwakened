#include "Parts/Common.h"

#include "WaterBGData.h"
#include "SwordPngData.h"

namespace Water
{
    namespace
    {
        // Full GPU raytracing of the kk.pov scene (water + 2 mirror spheres + sky + fog),
        // with recursive reflections. No precomputed background image.
        static const char * waterReflectFrag =
            "precision highp float;\n"
            "uniform vec3 uResolution;\n"
            "uniform sampler2D uSwordTex;\n"
            "uniform float uScroll;\n"
            "uniform float uTime;\n"
            "uniform float uFadeBlack;\n"
            "uniform float uSwordFade;\n"  // explicit sword visibility (0=hidden, 1=visible)
            "\n"
            "// ---- POV-Ray scene (kk.pov) geometry ----\n"
            "const vec3 CAM_EYE = vec3(15.0, 75.0, -110.0);\n"
            "const vec3 CAM_TGT = vec3(0.0, 0.0, 0.0);\n"
            "\n"
            "const vec3 SPH_A_C = vec3(33.0, 10.0, -33.0);\n"
            "const float SPH_A_R = 20.0;\n"
            "const vec3 SPH_B_C = vec3(22.0, 10.0, 22.0);\n"
            "const float SPH_B_R = 20.0;\n"
            "\n"
            "const vec3 WATER_COLOR  = vec3(0.10, 0.12, 0.45);\n"
            "const vec3 SPHERE_COLOR = vec3(0.10, 0.12, 0.35);\n"
            "const vec3 SKY_COLOR    = vec3(0.08, 0.10, 0.45);\n"
            "const vec3 LIGHT_DIR    = normalize(vec3(-10.0, 60.0, 30.0));\n"
            "const float FOG_DIST    = 250.0;\n"
            "const float SKY_Y       = 140.0;\n"
            "\n"
            "float hitSphere(vec3 ro, vec3 rd, vec3 c, float r, out vec3 n) {\n"
            "  vec3 oc = ro - c;\n"
            "  float b = dot(oc, rd);\n"
            "  float cc = dot(oc, oc) - r * r;\n"
            "  float h = b * b - cc;\n"
            "  if (h < 0.0) return -1.0;\n"
            "  float t = -b - sqrt(h);\n"
            "  if (t < 0.0) return -1.0;\n"
            "  n = (ro + t * rd - c) / r;\n"
            "  return t;\n"
            "}\n"
            "\n"
            "float hitQuad(vec3 ro, vec3 rd, vec3 c, vec3 ua, vec3 va, float hl, float hw, out vec2 q) {\n"
            "  vec3 n = normalize(cross(ua, va));\n"
            "  float denom = dot(rd, n);\n"
            "  if (abs(denom) < 1e-5) return -1.0;\n"
            "  float t = dot(c - ro, n) / denom;\n"
            "  if (t < 0.0) return -1.0;\n"
            "  vec3 p = ro + t * rd - c;\n"
            "  q = vec2(dot(p, ua), dot(p, va));\n"
            "  if (abs(q.x) > hl || abs(q.y) > hw) return -1.0;\n"
            "  return t;\n"
            "}\n"
            "\n"
            "// sword geometry (set in main, used by trace for reflections)\n"
            "vec3 g_swordC;\n"
            "vec3 g_swordUa;\n"
            "vec3 g_swordVa;\n"
            "float g_swordHl;\n"
            "float g_swordHw;\n"
            "// g_swordFade replaced by uniform uSwordFade\n"
            "\n"
            "// Whitted-style recursive raytrace (reflections unrolled, 4 bounces)\n"
            "vec3 trace(vec3 ro, vec3 rd) {\n"
            "  vec3 color = vec3(0.0);\n"
            "  vec3 weight = vec3(1.0);\n"
            "\n"
            "  for (int i = 0; i < 4; i++) {\n"
            "    float tBest = 1e9;\n"
            "    vec3 nBest = vec3(0.0, 1.0, 0.0);\n"
            "    vec3 albedo = vec3(0.0);\n"
            "    float refl = 0.0;\n"
            "    float diffStr = 0.0;\n"
            "    float specStr = 0.0;\n"
            "    int type = -1;\n"
            "    vec3 swordTex = vec3(0.0);\n"
            "\n"
            "    // water plane y=0 (visible from above), with ripples\n"
            "    if (rd.y < -1e-5) {\n"
            "      float t = -ro.y / rd.y;\n"
            "      if (t > 0.0 && t < tBest) {\n"
            "        tBest = t; type = 0;\n"
            "        vec3 wp = ro + rd * t;\n"
            "        float r = length(wp.xz);\n"
            "        float g = 0.30 * cos(r * 0.5 - uTime * 2.0);\n"
            "        nBest = normalize(vec3(-g * wp.x / max(r, 1e-3), 1.0, -g * wp.z / max(r, 1e-3)));\n"
            "        albedo = WATER_COLOR; refl = 0.6; diffStr = 0.6; specStr = 0.7;\n"
            "      }\n"
            "    }\n"
            "    // sky plane y=140 (visible from below)\n"
            "    if (rd.y > 1e-5) {\n"
            "      float t = (SKY_Y - ro.y) / rd.y;\n"
            "      if (t > 0.0 && t < tBest) { tBest = t; type = 2; nBest = vec3(0.0, -1.0, 0.0); albedo = SKY_COLOR; refl = 0.0; }\n"
            "    }\n"
            "\n"
            "    // mirror spheres\n"
            "    vec3 n;\n"
            "    float t = hitSphere(ro, rd, SPH_A_C, SPH_A_R, n);\n"
            "    if (t > 0.0 && t < tBest) { tBest = t; type = 1; nBest = n; albedo = SPHERE_COLOR; refl = 1.0; diffStr = 0.3; specStr = 0.7; }\n"
            "    t = hitSphere(ro, rd, SPH_B_C, SPH_B_R, n);\n"
            "    if (t > 0.0 && t < tBest) { tBest = t; type = 1; nBest = n; albedo = SPHERE_COLOR; refl = 1.0; diffStr = 0.3; specStr = 0.7; }\n"
            "\n"
            "    // sword quad (textured, opaque, transparent black background)\n"
            "    vec2 swordQ;\n"
            "    float tSword = hitQuad(ro, rd, g_swordC, g_swordUa, g_swordVa, g_swordHl, g_swordHw, swordQ);\n"
            "    if (tSword > 0.0 && tSword < tBest) {\n"
            "      vec2 suv = vec2(swordQ.x / (2.0 * g_swordHl) + 0.5, swordQ.y / (2.0 * g_swordHw) + 0.5);\n"
            "      vec3 stex = texture2D(uSwordTex, suv).rgb;\n"
            "      float slum = dot(stex, vec3(0.299, 0.587, 0.114));\n"
            "      if (slum > 0.08) { tBest = tSword; type = 3; swordTex = stex; }\n"
            "    }\n"
            "\n"
            "    if (tBest >= 1e9) break;\n"
            "\n"
            "    vec3 hitP = ro + rd * tBest;\n"
            "    float fog = clamp(tBest / FOG_DIST, 0.0, 1.0);\n"
            "\n"
            "    // sword: opaque, textured\n"
            "    if (type == 3) {\n"
            "      color += weight * swordTex * uSwordFade * (1.0 - fog);\n"
            "      break;\n"
            "    }\n"
            "\n"
            "    // sky: direct color (no specular), attenuated by fog\n"
            "    if (type == 2) {\n"
            "      color += weight * albedo * (1.0 - fog);\n"
            "      break;\n"
            "    }\n"
            "\n"
            "    // diffuse + phong (additive, like POV-Ray), attenuated by fog\n"
            "    float diff = max(dot(nBest, LIGHT_DIR), 0.0);\n"
            "    vec3 halfV = normalize(LIGHT_DIR - rd);\n"
            "    float spec = pow(max(dot(nBest, halfV), 0.0), 60.0);\n"
            "    vec3 local = albedo * diffStr * diff + vec3(1.0) * specStr * spec;\n"
            "    color += weight * local * (1.0 - fog);\n"
            "\n"
            "    if (refl <= 0.0) break;\n"
            "\n"
            "    // reflection (additive), attenuated by fog\n"
            "    rd = reflect(rd, nBest);\n"
            "    ro = hitP + nBest * 1e-3;\n"
            "    weight *= refl * (1.0 - fog * 0.5);\n"
            "  }\n"
            "\n"
            "  return color;\n"
            "}\n"
            "\n"
            "void main() {\n"
            "  vec2 ndc = (gl_FragCoord.xy / uResolution.xy) * 2.0 - 1.0;\n"
            "\n"
            "  // POV-Ray camera: right=+x, up=+y (fixed world axes), direction = target - eye\n"
            "  vec3 fwd = normalize(CAM_TGT - CAM_EYE);\n"
            "  float aspect = uResolution.x / uResolution.y;\n"
            "  float fov = 0.6;\n"
            "  vec3 ro = CAM_EYE;\n"
            "  vec3 rd = normalize(fwd + vec3(aspect * fov, 0.0, 0.0) * ndc.x + vec3(0.0, fov, 0.0) * ndc.y);\n"
            "\n"
            "  // sword geometry (for raytraced reflections)\n"
            "  vec3 d = normalize(vec3(-0.6, 1.0, -0.8));\n"
            "  g_swordUa = d;\n"
            "  g_swordVa = normalize(cross(d, fwd));\n"
            "  g_swordHl = 156.0;\n"
            "  g_swordHw = 18.0;\n"
            "  // Original trajectory: moves through the scene to far away (560).\n"
            "  // Start is pushed back to -200 so the whole quad starts submerged:\n"
            "  // top of quad = 0.7071*s + 156*0.7071 = 0.7071*s + 110.3, which must\n"
            "  // stay below y=0, i.e. s < -156. At s=-100 the tip already poked 39.6\n"
            "  // units above the ripple, so the sword was visible before it moved.\n"
            "  float s = mix(-200.0, 560.0, uScroll);\n"
            "  g_swordC = CAM_TGT + d * s;\n"
            "  // Original fade OUT as it recedes past camera (smoothstep 0.72->0.98)\n"
            "  // uSwordFade = 1.0 - smoothstep(0.72, 0.98, uScroll)\n"
            "\n"
            "  vec3 color = trace(ro, rd);\n"
            "\n"
            "  gl_FragColor = vec4(color * uFadeBlack, 1.0);\n"
            "}\n";
    }

    void main()
    {
        if (!Shim::isDemoFirstPart())
        {
            // Wait for the music without freezing the screen. A wait with no
            // frame in it leaves the last image on screen, which reads as a hang,
            // and the debug overlay stops updating with it. So keep blitting the
            // previous part's last frame while we wait, at the usual 70 Hz.
            while (Music::getPlusFlags() <= 0)
            {
                demo_vsync(true);   // advances the audio, one frame of budget
                demo_blit();
            }
        }

        uint16_t co = (uint16_t)Music::getOrder();

        int hiW = 0, hiH = 0;
        unsigned char * bgRgb = demo_loadpng_rgb_mem(Data::water_bg_png, Data::water_bg_png_size, &hiW, &hiH);
        if (bgRgb)
        {
            demo_setgputexture_rgb("uBgTex", bgRgb, hiW, hiH);
            demo_freepng(bgRgb);
        }

        int swW = 0, swH = 0;
        unsigned char * swRgb = demo_loadpng_rgb_mem(Data::sword_png, Data::sword_png_size, &swW, &swH);
        if (swRgb)
        {
            demo_setgputexture_rgb("uSwordTex", swRgb, swW, swH);
            demo_freepng(swRgb);
        }

        if (hiW > 0)
        {
            demo_setgputeffect(waterReflectFrag);
        }

        // Full visibility from the very first frame (uniform defaults to 0).
        demo_gpuuniform1f("uFadeBlack", 1.0f);

        for (int y = 0; y < 63 * 2; ++y)
        {
            demo_vsync();
            demo_gpuuniform1f("uScroll", 0.0f);
            demo_gpuuniform1f("uTime", get_time_ms_precise() / 1000.0f);
            // Sword hidden during intro fade-in (scroll=0)
            demo_gpuuniform1f("uSwordFade", 0.0f);
            demo_blit();
        }

        while (true)
        {
            demo_gpuuniform1f("uScroll", 0.0f);
            demo_gpuuniform1f("uTime", get_time_ms_precise() / 1000.0f);
            demo_blit();

            if (demo_wantstoquit()) break;

            int order = Music::getOrder();
            int row = Music::getRow();

            if (!(order == (int)co || row < 16)) break;
        }

        float scp = 0.0f;
        int fadeout = 0;
        int fadeFrames = 0;
        const int FADE_FRAMES = 48;
        uint16_t quit = 0;

        while (!demo_wantstoquit() && !quit)
        {
            demo_vsync(true);  // advance audio for music sync

            // Sequence ends when the music hits the end marker; fade to black
            // over FADE_FRAMES so the next part can fade in from black.
            if (Music::getPlusFlags() == -11) fadeout = 1;

            float fadeBlack = 1.0f;
            if (fadeout)
            {
                ++fadeFrames;
                fadeBlack = 1.0f - (float)fadeFrames / (float)FADE_FRAMES;
                if (fadeBlack < 0.0f) fadeBlack = 0.0f;
                if (fadeFrames >= FADE_FRAMES + 1) quit = 1;
            }

            float uScroll = scp / 390.0f;
            
            // Original behavior: sword fully visible at start, then fades OUT
            // as it recedes into distance (smoothstep 0.72 -> 0.98)
            float swordFade = 1.0f;
            if (uScroll > 0.72f) {
                float t = (uScroll - 0.72f) / (0.98f - 0.72f);
                if (t > 1.0f) t = 1.0f;
                // Smoothstep: 3*t^2 - 2*t^3
                float smooth = t * t * (3.0f - 2.0f * t);
                swordFade = 1.0f - smooth;
            }
            
            demo_gpuuniform1f("uScroll", uScroll);
            demo_gpuuniform1f("uTime", get_time_ms_precise() / 1000.0f);
            demo_gpuuniform1f("uFadeBlack", fadeBlack);
            demo_gpuuniform1f("uSwordFade", swordFade);
            
            if (scp < 390.0f) scp += 390.0f / 1080.0f;

            demo_blit();
        }
    }
}
