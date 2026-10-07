#pragma once

namespace Credits::Data
{
    // One credit picture: png is the base64 of an 8-bit PNG whose samples
    // are the palette indices, pal the matching 256-colour palette in
    // 6-bit VGA components (Common::setpalarea expects 6-bit values).
    struct CreditsPic
    {
        const char * png;
        const unsigned char * pal;
    };

    extern const CreditsPic creditsPics[];
    constexpr int CreditsPicCount = 21;
}
