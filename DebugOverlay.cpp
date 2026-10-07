// Debug overlay: live sync facts for timing a part against the music.
//
// Enable either way:
//   SR_DEBUG=1 ./secondreality     from the start
//   F12                            toggled live
//
// The point is to make reports checkable. "The drift starts around 0:42" can be
// confirmed against the timecode and the module position instead of being
// guessed at from a screenshot.
//
// Drawn through the same SDF text path as the end scroll, so it stays crisp at
// any resolution and needs no font of its own. The overlay is opt-in and costs
// nothing when disabled: the callback is not even installed.

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "Parts/Common.h"
#include "Graphics/VectorFont.h"

namespace DebugOverlay
{
    namespace
    {
        bool enabled = false;
        int frame = 0;
        int partIndex = -1;
        // Command character that starts this part, printed beside the number so
        // "part 15 [g]" cannot be misread as the key "5". 0 = none.
        char partCommand = 0;

        // Wall clock, not a frame counter. Accumulating 1000/70 per vsync froze
        // the timecode at zero through the whole intro, because the intro waits
        // for audio to hand over before it starts producing frames, and that is
        // exactly when a timecode is most useful.
        using Clock = std::chrono::steady_clock;
        Clock::time_point start = Clock::now();
        Clock::time_point partStart = start;
        // Time spent paused is excluded, so the timecode keeps meaning "where we
        // are in the demo" rather than "how long the process has been alive".
        // Without this, pausing for five seconds would skip the readout forward
        // while the music stayed put, which is exactly what a sync check must not
        // do.
        std::chrono::milliseconds pausedFor{ 0 };
        Clock::time_point pauseStart{};
        bool paused = false;

        unsigned elapsedMs()
        {
            const auto now = Clock::now();
            const auto frozen = paused ? pauseStart : now;
            const auto effective = frozen - pausedFor;
            return static_cast<unsigned>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    effective - start).count());
        }

        void line(const char * text, int lineIndex, float aspect)
        {
            static GpuTexVertex quads[160 * 4];
            // NDC spans -1..1 on both axes, so the left margin is a fraction of
            // that and not of the aspect: -aspect put the whole line off screen.
            // Glyph height in NDC is what is being set here, and the atlas glyphs
            // are about 24 units tall at fontSize 32, hence the division by 24
            // rather than by the font size.
            constexpr float glyphHeightNdc = 0.021f;
            constexpr float glyphHeightAtlas = 24.0f;
            const float scale = glyphHeightNdc / glyphHeightAtlas;

            // Liberation Serif is a wide face, and a debug line is mostly digits
            // and lower case, so the block ran across a third of an ultrawide
            // screen. Squashing it horizontally keeps the glyphs legible while
            // making the lines much easier to scan. The atlas is square, so this
            // has to be applied at layout time rather than by picking another
            // font, which would change the end scroll too.
            constexpr float widthScale = 0.68f;
            // Must clear the tallest glyph box (46 atlas units, about 32 px at
            // this scale), otherwise the lines overlap into an unreadable block.
            const float lineHeight = glyphHeightNdc * 2.35f;

            // Margins in pixels converted to NDC, because one NDC unit is not the
            // same distance on both axes: at 2072x5120 it spans 1036 px
            // vertically against 2560 px horizontally, so a single NDC margin
            // would hug the top while sitting far from the left edge.
            const float halfH = demo_windowheight() * 0.5f;
            // Tallest ascender in this charset measured from the atlas: capital
            // letters reach gy = -37 and gh = 46, so the box above the baseline is
            // 37 atlas units, not the 24 the font size suggests.
            const float ascenderPx = 37.0f * scale * halfH;
            const float halfW = halfH * aspect;
            // Clears the tallest ascender, since the margin is measured to the
            // baseline and gy is negative above it.
            const float marginY = (ascenderPx + 14.0f) / halfH;
            const float marginX = 26.0f / halfW;

            const float top = 1.0f - marginY - lineHeight * static_cast<float>(lineIndex);
            const float left = -1.0f + marginX;
            int quadCount = 0;
            float pen = left;

            for (const char * p = text; *p && quadCount < 160; ++p)
            {
                float u0, v0, u1, v1;
                int gw, gh, gx, gy;
                if (!VectorFont::GetOverlayGlyphUV(*p, u0, v0, u1, v1, gw, gh, gx, gy)) continue;

                const float x0 = pen + static_cast<float>(gx) * scale * widthScale;
                const float x1 = x0 + static_cast<float>(gw) * scale * widthScale;
                // `top` is the baseline. gy is negative above it and gh is the
                // box height, so the box spans [top - gy*scale, top + (gh+gy)*scale]
                // in NDC y, which is upward-positive. Getting this backwards put the
                // first line hard against the top edge regardless of the margin.
                const float yHigh = top - static_cast<float>(gy) * scale;
                const float yLow = yHigh - static_cast<float>(gh) * scale;

                GpuTexVertex * q = quads + quadCount * 4;
                q[0].px = x0; q[0].py = yHigh; q[0].pz = 0; q[0].u = u0; q[0].v = v0;
                q[1].px = x1; q[1].py = yHigh; q[1].pz = 0; q[1].u = u1; q[1].v = v0;
                q[2].px = x0; q[2].py = yLow; q[2].pz = 0; q[2].u = u0; q[2].v = v1;
                q[3].px = x1; q[3].py = yLow; q[3].pz = 0; q[3].u = u1; q[3].v = v1;
                ++quadCount;

                pen += static_cast<float>(VectorFont::GetOverlayGlyphAdvance(*p)) * scale * widthScale;
            }

            static const float MVP[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            };
            if (quadCount > 0)
            {
                // Amber on black: readable over any effect without hiding it.
                demo_drawsdftext(quads, quadCount, MVP, 1.0f, 0.78f, 0.15f, true, true);
            }
        }
    }

    void SetEnabled(bool on)
    {
        enabled = on;
    }

    bool IsEnabled()
    {
        return enabled;
    }

    // Called by the demo when it enters a part, so the overlay can show which
    // part is running and how long it has been there.
    void OnPartStart(int index, char command)
    {
        partIndex = index;
        partCommand = command;
        // Relative to the paused-free timeline, so a pause cannot skew it.
        partStart = Clock::now() - pausedFor;
        frame = 0;
    }

    // Called when the demo pauses or resumes. Keeps the timecode consistent with
    // a frozen music position.
    void SetPaused(bool on)
    {
        if (on == paused) return;
        if (on)
        {
            pauseStart = Clock::now();
            paused = true;
        }
        else
        {
            pausedFor += std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - pauseStart);
            paused = false;
        }
    }

    bool IsPaused() { return paused; }

    // Called once per vsync from demo_vsync. Only counts frames: the timecode
    // comes from the wall clock so it keeps running when frames are not.
    void Tick()
    {
        ++frame;
    }

    void Draw()
    {
        if (!enabled) return;

        static const char * set = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                                  "0123456789 .,:-+=/()[]%";
        static bool built = false;
        if (!built)
        {
            int aw = 0, ah = 0, apad = 0;
            const unsigned char * atlas = VectorFont::BuildOverlaySdfAtlas(set, 32, aw, ah, apad);
            demo_setoverlaysdftexture(atlas, aw, ah);
            built = true;
        }

        const float aspect = demo_windowaspect();
        const unsigned ms = elapsedMs();
        const unsigned partMs = static_cast<unsigned>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                (paused ? pauseStart : Clock::now()) - partStart).count());
        char buf[192];

        // Timecode is the primary reading: quote it when reporting a problem.
        std::snprintf(buf, sizeof(buf),
                      "timecode %u:%02u.%03u   part %d [%c]   frame %d",
                      ms / 60000u, (ms / 1000u) % 60u, ms % 1000u, partIndex,
                      partCommand ? partCommand : '-', frame);
        line(buf, 0, aspect);

        // The module's own position, next to our frame counter. Audio drift shows
        // up as these two stopping advancing together.
        std::snprintf(buf, sizeof(buf),
                      "music order %d row %d frame %d",
                      Music::getOrder(), Music::getRow(), Music::getFrame());
        line(buf, 1, aspect);

        std::snprintf(buf, sizeof(buf),
                      "part elapsed %u ms   framebuffer %dx%d   aspect %.3f%s",
                      partMs, (int)demo_windowheight(),
                      (int)(demo_windowheight() * aspect), aspect,
                      paused ? "   [PAUSED]" : "");
        line(buf, 2, aspect);
    }
}
