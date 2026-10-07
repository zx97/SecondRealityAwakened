#include <math.h>
#include <stdlib.h>

#include "Parts/Common.h"

#include "Tunneli_Data.h"

namespace Tunneli
{
    constexpr const size_t FRAME_COUNT_TO_EXIT_VEKE = 1060;

    // TUNNELI/TUN10.PAS: 101 live rings, a 16 frame colour cycle, 1060 frames,
    // and the last 102 frames spawning invisible rings so the tunnel empties out.
    //
    // Everything below is expressed in half-heights: one unit is half the window
    // height, so a position is already its own NDC coordinate and nothing
    // depends on the window size. The previous renderer mapped a 320x200 DOS box
    // onto the window instead, which pinned every coordinate to a
    // height/200 grid (5.4 px steps at 1080p) and made the integer signal tables
    // read as drifting clouds of points rather than motion. Leaving the box
    // behind is what removes that quantisation.
    constexpr int RINGS = 101;
    constexpr int FIRST_DRAWN = 0;
    constexpr int LAST_DRAWN = 100;
    constexpr int REFERENCE_RING = 5;
    constexpr int FADE_TAIL = 102;
    constexpr int DOTS_PER_RING = 256;

    // Radius law from sade[x] := trunc(16384/(x*7+95)) plus the generator's
    // z := x + 10. Kept in double: the original truncated twice, and this is
    // the last truncation standing between the radius law and a smooth ramp.
    inline double ringZ(int r) { return 16384.0 / (7.0 * r + 95.0) + 10.0; }

    // The original stamps 8 white then 8 grey rings on a 16 frame cycle. The
    // stamp goes in at one slot and travels down the ring array, so the bands
    // march outward, and that is what actually reads as forward travel: the
    // ring offsets themselves only wobble by a couple dozen units.
    constexpr int WHITE_PALETTE_IDX = 255;
    constexpr int GREY_PALETTE_IDX = 128;
    constexpr int GREY_LEVEL = 115;

    // The original's signal tables grow their amplitude with the table index
    // (sinit[i] = sin(pi*i/128) * (i*3/128)), so the tunnel starts almost still
    // and swells. Those indices wrap at 4095 and 2047 though, which makes that
    // growth a fast sawtooth rather than the slow build the original had. This
    // envelope restores the build across the whole effect: it reaches ENVELOPE_END
    // at the end and is 1 throughout the tail, so the finale keeps its amplitude.
    constexpr double ENVELOPE_START = 0.28;
    constexpr double ENVELOPE_END = 0.88;
    constexpr int ENVELOPE_RAMP = 820;   // frames; the effect runs 1060

    // Per-ring offset from the original signal, in half-heights per signal unit.
    // This was pinned tiny while the signal tables were integers, to keep their
    // quantisation under a pixel. They are evaluated in double now, so there is
    // no staircase left to hide and the gain can be real: neighbouring rings
    // separate by tens of pixels instead of sitting on top of each other.
    constexpr double OFFSET_GAIN = 0.0100;

    // Camera motion, in half-heights, driven by a phase locked to the module
    // frame counter so the swing runs at the song's own tempo. The vertical
    // swing uses a 1.5x period so the two axes never fall into lockstep.
    constexpr double SWING_X = 0.55;
    constexpr double SWING_Y = 0.40;
    constexpr double SWING_PERIOD = 2.0;   // seconds per left-right swing
    constexpr double Y_PERIOD_RATIO = 1.5;
    // Radial pulse on the same phase: the tunnel breathes in and out on the
    // beat, which is what sells forward travel far better than drift does.
    constexpr double PULSE = 0.10;
    // Rings outside the original's 101. On 4:3 the widest original ring already
    // covers the frame, but on 21:9 and wider the corners fall outside it, so a
    // few extra rings are generated outward to fill the frame at any aspect.
    // They are drawn in the dim grey and are purely a filling device.
    constexpr int OUTER_RINGS = 3;
    constexpr double OUTER_GROWTH = 0.32;

    // Dot size as a fraction of the window height. The wide near rings get big
    // readable discs, the ones bunched around the vanishing point stay small;
    // one flat size made the far half stack into a grey slab.
    constexpr double DOT_NEAR = 0.0060;
    constexpr double DOT_FAR = 0.0024;

    static const char * placeholderFrag =
        "precision mediump float;\n"
        "void main(){ gl_FragColor = vec4(0.0); }\n";

    struct TunnelVertex
    {
        float x, y, z;
        float depth;
        float size;
        float colorIdx;
    };

    namespace
    {
        bool quit = false;
        float elapsed = 0.0f;

        // The shipped 4097+2049 word tables are nothing but these two formulas
        // rounded to integers: checked against the embedded data, the mean error
        // is 0.39 and the max 1.37, which is exactly integer rounding. Evaluating
        // them in double therefore reproduces the original trajectory while
        // removing the quantisation that made the tunnel read as clouds of
        // points. The index wraps like the original's and 2047 lookups.
        inline double sinitAt(int i)
        {
            i &= 4095;
            return sin(3.14159265358979323846 * i / 128.0) * (i * 3.0 / 128.0);
        }

        inline double cositAt(int i)
        {
            i &= 2047;
            return cos(3.14159265358979323846 * i / 128.0) * (i * 4.0 / 64.0);
        }

        // True circles: the 1.7 stretch the DOS original baked into its ellipse
        // generator deformed them into flat ovals on a modern display, and the
        // shipped ellipse table is no longer consulted.
        inline void ellipseAt(double z, double ang, double & u, double & v)
        {
            u = sin(ang) * z;
            v = -(cos(ang) * z);
        }

            // Drives the camera from the module frame counter, so the swing runs at
        // the track's tempo instead of a guessed frequency. The module frame
        // rate depends on the song tempo, so it is measured over the first
        // second rather than hardcoded; until then the phase runs off elapsed
        // time so there is no dead start, and it hands over without a jump.
        struct MusicClock
        {
            // A real 70 Hz step is 1. Anything past this is a skip or a counter
            // wrap, not a frame of music.
            static constexpr int MAX_FRAME_STEP = 8;

            int lastFrame = -1;
            double phase = 0.0;
            double framesPerSwing = 0.0;
            int calibFrames = 0;
            float calibElapsed = 0.0f;

            // Frames elapsed on the module's own counter, or -1 before it is known.
            int musicFrame() const { return lastFrame; }

            double advance(float elapsedDelta)
            {
                const int mf = Music::getFrame();

                if (framesPerSwing > 0.0)
                {
                    // A part skip restarts the song, so the module frame counter
                    // can jump forwards by a huge amount or wrap backwards. Either
                    // way the delta is not a real time step, and accumulating it
                    // would fling the camera phase to an arbitrary angle and leave
                    // it there: the swing would then run at full amplitude forever.
                    if (lastFrame >= 0 && mf >= lastFrame)
                    {
                        const int delta = mf - lastFrame;
                        if (delta > 0 && delta <= MAX_FRAME_STEP)
                        {
                            phase += delta * (6.283185307179586 / framesPerSwing);
                        }
                        else if (delta != 0)
                        {
                            // Resynchronise without moving the phase.
                            framesPerSwing = 0.0;
                            calibFrames = 0;
                            calibElapsed = 0.0f;
                        }
                    }
                    else if (lastFrame >= 0)
                    {
                        // The counter went backwards: the song restarted.
                        framesPerSwing = 0.0;
                        calibFrames = 0;
                        calibElapsed = 0.0f;
                    }
                }
                else
                {
                    phase += elapsedDelta * (6.283185307179586 / SWING_PERIOD);
                    if (lastFrame >= 0 && mf > lastFrame)
                    {
                        calibFrames += mf - lastFrame;
                    }
                    calibElapsed += elapsedDelta;
                    if (calibElapsed >= 1.0f)
                    {
                        const double hz = calibElapsed > 0.0f
                            ? static_cast<double>(calibFrames) / calibElapsed
                            : 0.0;
                        if (hz > 0.0)
                        {
                            framesPerSwing = hz * SWING_PERIOD;
                        }
                        calibElapsed = 0.0f;
                        calibFrames = 0;
                    }
                }

                if (mf >= 0) lastFrame = mf;
                return phase;
            }
        };

    void buildPalette(char * pal)
        {
            for (int i = 0; i < 256; i++)
            {
                pal[i * 3 + 0] = 0;
                pal[i * 3 + 1] = 0;
                pal[i * 3 + 2] = 0;
            }
            for (int c = 0; c < 3; c++)
            {
                pal[WHITE_PALETTE_IDX * 3 + c] = static_cast<char>(255);
                pal[GREY_PALETTE_IDX * 3 + c] = static_cast<char>(GREY_LEVEL);
            }
        }
    }

    void runOriginal()
    {
        quit = false;
        elapsed = 0.0f;

        g_respectRatio = false;

        demo_changemode(SCREEN_WIDTH, SCREEN_HEIGHT);
        demo_setgputeffect(placeholderFrag);

        // Dots that overlap should add into each other rather than overwrite,
        // which is what makes the tunnel glow instead of reading as a flat
        // grey mass where the far rings pile up.
        demo_pointsadditive(true);

        char pal[768];
        buildPalette(pal);
        Common::setpalarea(pal, 0, 256);

        static TunnelVertex vertices[(LAST_DRAWN - FIRST_DRAWN + 1 + OUTER_RINGS) * DOTS_PER_RING];

        MusicClock clock;

        int frame = 0;

        while (!quit && !demo_wantstoquit())
        {
            // Read every frame, not once before the loop: the cached values went
            // stale the moment the window was resized, which is why enlarging it
            // on a 21:9 screen stretched the rings back into ellipses. Taken from
            // the framebuffer rather than from demo_windowaspect(), which follows
            // the desktop mode while fullscreen and disagreed with the viewport.
            const float winH = demo_windowheight();
            const double aspect = demo_framebufferaspect();

            // Rings are circles, so the radius that just contains the screen
            // corners is simply the diagonal. The tunnel fills the frame at any
            // aspect with no crop to guess and no letterbox.
            const double nearRadius = sqrt(aspect * aspect + 1.0);

            const int sync = demo_vsync();
            frame += sync;
            const float delta = sync * (1.0f / 70.0f);
            elapsed += delta;

            // The ring's own signal, and the signal of the reference ring it was
            // 95 frames ahead of. Both are pure functions of the frame counter,
            // so the delay line the original kept is no longer needed: a ring's
            // position is recomputed, never integrated.
            // The reference ring is itself born 95 frames after the oldest ring
            // still alive, so for the first second it does not exist. Evaluating
            // the signal at a negative index produced a plausible but meaningless
            // value, and the switch to the real one at frame 95 showed up as a
            // jump. Hold the reference at the origin until it is actually born.
            const int refBirth = frame - (RINGS - 1 - REFERENCE_RING);
            double refX = 0.0;
            double refY = 0.0;
            if (refBirth >= 0)
            {
                refX = -sinitAt(refBirth * 3);
                refY = sinitAt(refBirth * 2) - cositAt(refBirth) + sinitAt(refBirth);
            }

            // Camera runs on a phase locked to the module frame counter, so the
            // swing is at the track's tempo rather than a guessed frequency.
            const double phase = clock.advance(delta);

            // The swell follows the music, not our own frame counter. After a part
            // skip the song resumes mid-track while our counter restarts at zero,
            // so keying the envelope to the counter would replay the opening swell
            // at full swing and then hold it: the motion never comes back down.
            const int ramp = clock.musicFrame();
            double envelope = ENVELOPE_END;
            if (ramp >= 0 && ramp < ENVELOPE_RAMP)
            {
                const double t = static_cast<double>(ramp) / static_cast<double>(ENVELOPE_RAMP);
                envelope = ENVELOPE_START + (ENVELOPE_END - ENVELOPE_START) * t;
            }
            const double pulse = 1.0 + PULSE * sin(phase * 2.0);
            const double radiusScale = pulse * nearRadius / ringZ(FIRST_DRAWN);

            const double camX = SWING_X * envelope * sin(phase);
            const double camY = SWING_Y * envelope * sin(phase * Y_PERIOD_RATIO + 1.2);

            int count = 0;

            // Appends one circle of dots. z is the radius in half-heights.
            auto emit = [&](double z, float colorIdx, float size,
                            double ox, double oy, int step, float depth)
            {
                for (int a = 0; a < DOTS_PER_RING; a += step)
                {
                    double u = 0.0;
                    double v = 0.0;
                    const double ang =
                        static_cast<double>(a) / static_cast<double>(DOTS_PER_RING) * 6.283185307179586;
                    ellipseAt(z, ang, u, v);

                    // Keep the lateral sway the same fraction of the screen at
                    // any ratio: on 4:3 the half-height offset covered a large
                    // part of the width, on a wide screen it shrank. Gain the
                    // offset by aspect/(4/3) so it matches the 4:3 rendering.
                    const double lateralGain = aspect / (4.0 / 3.0);
                    const float nx = static_cast<float>((u + (camX + ox) * lateralGain) / aspect);
                    const float ny = static_cast<float>(v + camY + oy);

                    // Rings sweep well past the frame; skipping the off-screen
                    // dots keeps the vertex count down and matches what the
                    // reference remaster does with its screen-box reject.
                    if (nx < -1.1f || nx > 1.1f || ny < -1.1f || ny > 1.1f) continue;

                    // x is divided by the aspect because NDC spans the window
                    // width in x and its height in y: one NDC unit is W/2 pixels
                    // horizontally but H/2 vertically, so leaving x alone
                    // stretched every circle by the aspect ratio and only a
                    // square window looked round.
                    vertices[count].x = nx;
                    vertices[count].y = ny;
                    vertices[count].z = 0.0f;
                    vertices[count].depth = depth;
                    vertices[count].size = size;
                    vertices[count].colorIdx = colorIdx;
                    count++;
                }
            };

            // Filler circles beyond the original's 101 so ultrawide corners are
            // never bare. Dim, and drawn sparsely since they are far off screen.
            for (int m = OUTER_RINGS; m >= 1; m--)
            {
                emit(nearRadius * pulse * (1.0 + OUTER_GROWTH * m),
                     static_cast<float>(GREY_PALETTE_IDX),
                     static_cast<float>(winH * DOT_FAR),
                     0.0, 0.0, 1, 8800.0f);
            }

            for (int r = LAST_DRAWN; r >= FIRST_DRAWN; r--)
            {
                const int birth = frame - (RINGS - 1) + r;
                if (birth < 0) continue;

                // The 16 frame colour cycle is stamped on the ring's birth frame,
                // which is why the bands march outward along the tunnel.
                int band = ((birth & 15) > 7) ? 128 : 64;
                if (birth >= static_cast<int>(FRAME_COUNT_TO_EXIT_VEKE) - FADE_TAIL) band = 0;
                if (band == 0) continue;

                const double rNorm = static_cast<double>(r) / static_cast<double>(LAST_DRAWN);

                const double px = -sinitAt(birth * 3);
                const double py = sinitAt(birth * 2) - cositAt(birth) + sinitAt(birth);

                const double oxTable = (px - refX) * OFFSET_GAIN * envelope;
                const double oyTable = -(py - refY) * OFFSET_GAIN * envelope;

                const double z = ringZ(r) * radiusScale;
                const double shade = 1.0 - rNorm;

                const double ox = oxTable;
                const double oy = oyTable;

                const float ringSize = static_cast<float>(
                    winH * (DOT_FAR + (DOT_NEAR - DOT_FAR) * shade * shade));

                // Past roughly 40 rings the circles crowd into the same few
                // pixels, so every extra ring lands on the previous one and the
                // far field turns into a solid band.
                int step = 1;

                // The shared points shader already shades by depth through
                // clamp((9200 - vDepth)/400, 0.2, 1.0); it was being fed a flat
                // zero, so every dot came out at full brightness. Feeding it the
                // ramp restores the near-bright / far-dim falloff, and keeps the
                // grey bands at three quarters of the white ones.
                const double depthFade = r / 1.3;
                double brightness = (band == 64) ? 1.0 : 0.75;
                brightness *= (1.0 - depthFade / 96.0);
                if (brightness < 0.0) brightness = 0.0;
                const float depth = static_cast<float>(9200.0 - 400.0 * brightness);

                emit(z, band == 64 ? static_cast<float>(WHITE_PALETTE_IDX)
                                   : static_cast<float>(GREY_PALETTE_IDX),
                     ringSize, ox, oy, step, depth);
            }

            // Rings become valid one per frame, so the tunnel visibly grows from
            // two rings to a hundred. Ramping the output in hides the ramp.
            const int onset = frame - 20;
            demo_setpointsfade(onset <= 0 ? 0.0f
                                          : (onset >= 50 ? 1.0f : onset / 50.0f));

            demo_drawpoints(vertices, count, sizeof(TunnelVertex), 1.0f, 0.55f);
            demo_blit();

            if (frame >= static_cast<int>(FRAME_COUNT_TO_EXIT_VEKE))
            {
                quit = true;
            }
        }

        demo_setpointsfade(1.0f);
        demo_pointsadditive(false);
        demo_cleargputeffect();
        g_respectRatio = true;
    }

    // Pre-2026-09-30 look: a hand-written tunnel with its own sine-based ring
    // offsets, a faked FOV/wz projection and a swing/twist motion. Not the
    // original algorithm, but kept because it reads well. Enable with
    // SR_TUNNELI_ALTERNATE=1.
    void runAlternate()
    {
        quit = false;
        elapsed = 0.0f;

        g_respectRatio = false;

        demo_changemode(SCREEN_WIDTH, SCREEN_HEIGHT);
        demo_setgputeffect(placeholderFrag);

        // Dots that overlap should add into each other rather than overwrite,
        // which is what makes the tunnel glow instead of reading as a flat
        // grey mass where the far rings pile up.
        demo_pointsadditive(true);

        constexpr int ALT_RINGS = 96;
        constexpr int ALT_PER_RING = 64;
        constexpr int ALT_MAX_PTS = ALT_RINGS * ALT_PER_RING;
        constexpr float CAM = 0.5f;
        constexpr float RADIUS = 1.6f;
        constexpr float FOV = 1.9f;

        static TunnelVertex vertices[ALT_MAX_PTS];

        uint16_t frame = 0;

        while (!quit && !demo_wantstoquit())
        {
            int sync = demo_vsync();
            if (sync > 10) sync = 10;
            if (sync < 1) sync = 1;
            frame += sync;
            elapsed += sync * (1.0f / 70.0f);

            float aspectY = demo_windowaspect();

            demo_gpuuniform1f("uTime", elapsed);
            demo_gpuuniform1f("uFrame", static_cast<float>(frame));

            float advance = elapsed * 14.0f;
            float twist = elapsed * 0.35f;
            float fade = elapsed < 1.5f ? elapsed / 1.5f : 1.0f;
            float swingX = sinf(elapsed * 1.3f) * 0.80f + sinf(elapsed * 2.3f) * 0.30f;
            float swingY = cosf(elapsed * 1.1f) * 0.70f + cosf(elapsed * 1.9f) * 0.25f;

            int count = 0;

            for (int j = 0; j < ALT_RINGS; j++)
            {
                float z = (float)j + advance;
                z = z - floorf(z / (float)ALT_RINGS) * (float)ALT_RINGS;

                float zNorm = z / (float)ALT_RINGS;
                float zReal = z;

                float cx = sinf(zReal * 0.167f + elapsed * 0.55f) * 0.28f + sinf(zReal * 0.367f - elapsed * 0.30f) * 0.15f;
                float cy = cosf(zReal * 0.147f + elapsed * 0.48f) * 0.26f + cosf(zReal * 0.32f + elapsed * 0.28f) * 0.14f;
                float rad = RADIUS * (1.0f + 0.04f * sinf(elapsed * 0.25f + zReal * 0.167f));

                float wz = zReal + CAM;
                if (wz < 0.35f) continue;
                float proj = FOV / wz;
                float shade = 1.0f - zNorm;
                float bp = 8800.0f + zNorm * 250.0f;
                float size = (2.0f + 7.0f * shade * shade) * fade;
                if (size < 0.75f) size = 0.75f;

                for (int k = 0; k < ALT_PER_RING; k++)
                {
                    float a = ((float)k / (float)ALT_PER_RING) * 6.2831853f + twist * (1.0f - zNorm * 0.5f);
                    float ca = cosf(a);
                    float sa = sinf(a);

                    float wx = cx + ca * rad;
                    float wy = cy + sa * rad;

                    float sx = (wx * proj + swingX * zNorm * 0.5f) * ((4.0f / 3.0f) / aspectY);
                    float sy = wy * proj + swingY * zNorm * 0.5f;

                    if (sx < -1.15f || sx > 1.15f || sy < -1.15f || sy > 1.15f) continue;

                    vertices[count].x = sx;
                    vertices[count].y = -sy;
                    vertices[count].z = 0.0f;
                    vertices[count].depth = bp;
                    vertices[count].size = size;
                    vertices[count].colorIdx = 10.0f;
                    count++;
                }
            }

            // Rings become valid one per frame, so the tunnel visibly grows from
            // two rings to a hundred. Ramping the output in hides the ramp.
            const int onset = frame - 20;
            demo_setpointsfade(onset <= 0 ? 0.0f
                                          : (onset >= 50 ? 1.0f : onset / 50.0f));

            demo_drawpoints(vertices, count, sizeof(TunnelVertex), 1.0f);
            demo_blit();

            if (frame >= static_cast<int>(FRAME_COUNT_TO_EXIT_VEKE))
            {
                quit = true;
            }
        }

        demo_cleargputeffect();
        g_respectRatio = true;
    }

    void main()
    {
        if (getenv("SR_TUNNELI_ALTERNATE"))
        {
            runAlternate();
        }
        else
        {
            runOriginal();
        }
    }
}
