#pragma once

// Global debug overlay. See DebugOverlay.cpp for the details.
//
// Enabled by SR_DEBUG=1 in the environment, or toggled live with F12.

namespace DebugOverlay
{
    void SetEnabled(bool on);
    bool IsEnabled();

    // Called by the demo when it enters a part.
    void OnPartStart(int index, char command);

    // Called once per vsync, so the counters follow the real frame rate.
    void Tick();

    // Follows the demo pause, so the timecode excludes the paused time.
    void SetPaused(bool on);
    bool IsPaused();

    // Draws the overlay. Does nothing when disabled.
    void Draw();
}
