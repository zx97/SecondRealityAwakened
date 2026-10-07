#include "Parts/Common.h"

#include "COMAN_Data.h"

namespace Coman
{
    namespace
    {
        // GPU raymarched heightfield ("voxel space" in the spirit of Comanche).
        // A sinuous liquid surface, raymarched in perspective, tinted by a palette.
        static const char * comanFrag =
            "precision highp float;\n"
            "uniform vec3 uResolution;\n"
            "uniform sampler2D uPalette;\n"
            "uniform float uTime;\n"
            "uniform float uFade;\n"
            "uniform float uRise;\n"
            "uniform float uClimb;\n"
            "\n"
            "float heightfield(float x, float z) {\n"
            "  return 8.0 * sin(x * 0.10 + uTime * 1.5) * cos(z * 0.08 + uTime * 1.2)\n"
            "       + 3.0 * sin(x * 0.22 + z * 0.18 + uTime * 0.8)\n"
            "       + 1.5 * sin(x * 0.40 - uTime * 2.0) * sin(z * 0.35 + uTime * 1.7);\n"
            "}\n"
            "\n"
            "void main() {\n"
            "  vec2 ndc = (gl_FragCoord.xy / uResolution.xy) * 2.0 - 1.0;\n"
            "  ndc.y += uRise;\n"
            "  float aspect = uResolution.x / uResolution.y;\n"
            "\n"
            "  float cap = uTime * 0.15 + 1.2 * sin(uTime * 0.08);\n"
            "  vec3 flightDir = vec3(sin(cap), 0.0, cos(cap));\n"
            "  float dist = uTime * 14.0;\n"
            // uClimb lifts the camera during the exit so the terrain slides down and
            // out of frame, leaving black, which is how the original reveals the
            // scene (References/SecondReality/COMAN/MAIN.C docopy with startrise).
            // Both terms are needed: raising the eye alone changes the view angle,
            // so the pitch has to tilt up with it for the surface to leave frame.
            "  vec3 ro = vec3(0.0, 16.0 + uClimb * 90.0, 0.0) + flightDir * dist;\n"
            "  vec3 fwd = normalize(vec3(flightDir.x, -0.5, flightDir.z));\n"
            "  vec3 rgt = normalize(cross(fwd, vec3(0.0, 1.0, 0.0)));\n"
            "  vec3 upc = cross(rgt, fwd);\n"
            "  float fov = 0.8;\n"
            "  vec3 rd = normalize(fwd + rgt * ndc.x * aspect * fov + upc * ndc.y * fov);\n"
            "\n"
            // Pure black sky. A non-zero value reads as grey haze once uFade
            // scales it during the intro, and the fog mixes distant pixels
            // towards it too.
            "  vec3 bg = vec3(0.0);\n"
            "  vec3 color = bg;\n"
            "  float t = 0.5;\n"
            "  float tPrev = 0.0;\n"
            "  for (int i = 0; i < 128; i++) {\n"
            "    vec3 p = ro + rd * t;\n"
            "    float h = heightfield(p.x, p.z);\n"
            "    if (p.y < h) {\n"
            // The hit distance found by fixed-step marching is quantised to the
            // step, which snaps each pixel into a diamond-shaped cell on the
            // surface. Bisecting the bracketing interval refines the hit far
            // below the step size, so the cell edges become imperceptible.
            "      float lo = tPrev;\n"
            "      float hi = t;\n"
            "      for (int k = 0; k < 24; k++) {\n"
            "        float mid = 0.5 * (lo + hi);\n"
            "        vec3 pm = ro + rd * mid;\n"
            "        if (pm.y < heightfield(pm.x, pm.z)) hi = mid;\n"
            "        else lo = mid;\n"
            "      }\n"
            "      t = 0.5 * (lo + hi);\n"
            "      p = ro + rd * t;\n"
            "      h = heightfield(p.x, p.z);\n"
            // Normal epsilon matches the refined hit scale rather than the march
            // step, so shading no longer facets along the previous step grid.
            "      float e = 0.05;\n"
            "      float hx = heightfield(p.x + e, p.z);\n"
            "      float hz = heightfield(p.x, p.z + e);\n"
            "      vec3 n = normalize(vec3(h - hx, e, h - hz));\n"
            "      float shade = 0.45 + 0.55 * max(dot(n, normalize(vec3(0.4, 0.7, 0.6))), 0.0);\n"
            "      float hNorm = clamp((h + 12.5) / 25.0, 0.0, 1.0);\n"
            "      float pal = hNorm;\n"
            // GL_LINEAR already interpolates the ramp, so one fetch is correct.
            // A second fetch plus fract() here re-quantised the colour into
            // diamond cells, which is worse than the banding it replaced.
            "      color = texture2D(uPalette, vec2(pal, 0.5)).rgb * shade;\n"
            "      float fog = clamp(t / 70.0, 0.0, 1.0);\n"
            "      color = mix(color, bg, fog);\n"
            "      break;\n"
            "    }\n"
            "    tPrev = t;\n"
            "    t += 0.5 + t * 0.02;\n"
            "  }\n"
            "\n"
            // The 6-bit palette quantises the gradient into flat steps no matter
            // how the texture is sampled. Ordered dithering at one 8-bit LSB
            // breaks the visible diamond banding. Same technique as PLZ.
            "  float dith = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
            "  color += (dith - 0.5) * (1.5 / 255.0);\n"
            "\n"
            "  gl_FragColor = vec4(color * uFade, 1.0);\n"
            "}\n";
    }

    void main()
    {
        // The original builds a 256-entry palette with integer maths (References/
        // SecondReality/COMAN/MAIN.C:140-170): /3, *9/6 and min(63) truncation.
        // Across the range the terrain actually spans that leaves only 91
        // distinct colours, one channel with 14 levels, and a run of 38 entries
        // that are bit-identical -- hence the flat contour bands.
        //
        // Same hue trajectory, evaluated in continuous 16-bit floats instead of
        // quantised integers: every step of the ramp is now a distinct colour.
        // The three control colours are the ramp's own landmarks, read off the
        // original table: deep blue at the troughs, magenta mid-height, bright
        // cyan at the crests.
        constexpr int RAMP_STEPS = 2048;
        static unsigned char rampTex[RAMP_STEPS * 3];

        // Anchors from trough to crest: near-black, magenta in the crevasses, then
        // the original's blues, and cyan at the peaks. The values follow the
        // original 6-bit table sampled at 8/48/88/128/168/208 and scaled to 8 bit,
        // so the ramp keeps its luminance: the original is far darker in the low
        // and mid range than a naive red-to-blue interpolation produces.
        constexpr float ANCHORS[][3] = {
            { 0.015f, 0.000f, 0.020f },   // f=0.00  trough, near black
            { 0.360f, 0.020f, 0.240f },   // f=0.12  magenta, peaks here
            { 0.150f, 0.060f, 0.330f },   // f=0.22  magenta fading to blue
            { 0.020f, 0.180f, 0.560f },   // f=0.45  deep blue
            { 0.000f, 0.300f, 0.800f },   // f=0.65  blue
            { 0.000f, 0.520f, 0.950f },   // f=0.85  bright blue
            { 0.100f, 0.860f, 0.960f },   // f=1.00  crest, cyan
        };
        constexpr int SEGMENTS = 6;
        constexpr float BREAKS[] = { 0.12f, 0.22f, 0.45f, 0.65f, 0.85f, 1.00f };

        for (int i = 0; i < RAMP_STEPS; i++)
        {
            const float f = static_cast<float>(i) / (RAMP_STEPS - 1);

            int seg = 0;
            while (seg < SEGMENTS - 1 && f > BREAKS[seg]) seg++;

            const float lo = (seg == 0) ? 0.0f : BREAKS[seg - 1];
            const float k = (f - lo) / (BREAKS[seg] - lo);
            // Smoothstep so the joins between anchors show no kink.
            const float ks = k * k * (3.0f - 2.0f * k);

            const float *a = ANCHORS[seg];
            const float *b = ANCHORS[seg + 1];
            const float r = a[0] + (b[0] - a[0]) * ks;
            const float g = a[1] + (b[1] - a[1]) * ks;
            const float bl = a[2] + (b[2] - a[2]) * ks;

            rampTex[i * 3 + 0] = static_cast<unsigned char>(r * 255.0f);
            rampTex[i * 3 + 1] = static_cast<unsigned char>(g * 255.0f);
            rampTex[i * 3 + 2] = static_cast<unsigned char>(bl * 255.0f);
        }

        demo_setgputeffect(comanFrag);
        demo_setgputexture_rgb("uPalette", rampTex, RAMP_STEPS, 1);

        if (!Shim::isDemoFirstPart())
        {
            // Keep producing frames while waiting for the music: a wait with no
            // vsync in it leaves the last image on screen, which reads as a hang
            // and stops the debug overlay.
            while (!demo_wantstoquit() && Music::getPlusFlags() < 0)
            {
                demo_vsync(true);   // advances the audio, one frame of budget
                demo_blit();
            }
        }

        demo_gpuuniform1f("uFade", 0.0f);
        demo_gpuuniform1f("uRise", 1.0f);
        demo_gpuuniform1f("uClimb", 0.0f);

        float startTime = get_time_ms_precise();
        int frame = 0;
        const int FADE_IN_FRAMES = 48;
        const int EXIT_FRAMES = 40;
        bool exiting = false;
        int exitFrame = 0;
        while (!demo_wantstoquit())
        {
            demo_vsync(true);  // advance audio for music sync

            int flags = Music::getPlusFlags();
            if (!exiting && flags > -8 && flags < 0)
            {
                exiting = true;
                exitFrame = 0;
            }

            float fade = 1.0f;
            if (frame < FADE_IN_FRAMES)
            {
                fade = (float)frame / (float)FADE_IN_FRAMES;
                ++frame;
            }

            // Exit: the camera climbs and pitches up so the terrain sinks out of
            // frame, revealing black. The original reveals the scene the same way,
            // by copying the framebuffer from an increasing row offset.
            float climb = 0.0f;
            if (exiting)
            {
                const float e = (float)exitFrame / (float)EXIT_FRAMES;
                const float s = e * e * (3.0f - 2.0f * e);
                climb = s;
                if (exitFrame < EXIT_FRAMES)
                    ++exitFrame;
                else
                    break;
            }

            float t = (get_time_ms_precise() - startTime) / 1000.0f;
            demo_gpuuniform1f("uTime", t);
            demo_gpuuniform1f("uFade", fade);
            demo_gpuuniform1f("uRise", 1.0f - fade + climb * 1.4f);
            demo_gpuuniform1f("uClimb", climb);
            demo_blit();
        }
    }
}
