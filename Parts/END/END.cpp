#include "Parts/Common.h"

#include "END_PNG_DATA.h"

namespace End
{
    void main()
    {
        int a;

        Shim::clearScreen();

        int w = 0, h = 0;
        unsigned char * img = demo_loadpng_rgb_mem(Data::end_pic_png, Data::end_pic_png_size, &w, &h);

        // ENDPIC/BEG.C fades the palette from white into the picture over 129
        // vblanks. Here the picture is a GPU quad and the only palette entry in
        // play is index 0, which the previous part left white, so the same fade
        // is done on both: index 0 goes white to black while the quad's colour
        // is mixed out of white, and the whole frame resolves together.
        char pal[768];
        for (int i = 0; i < 768; i += 3)
        {
            pal[i + 0] = (char)63;
            pal[i + 1] = (char)63;
            pal[i + 2] = (char)63;
        }

        constexpr int FADE_FRAMES = 129;

        for (a = 0; a < FADE_FRAMES && !demo_wantstoquit(); a++)
        {
            const int k = 63 - (63 * a) / FADE_FRAMES;
            for (int i = 0; i < 768; i += 3)
            {
                pal[i + 0] = (char)k;
                pal[i + 1] = (char)k;
                pal[i + 2] = (char)k;
            }
            Common::setpalarea(pal);

            const float s = 1.0f - (float)a / (float)FADE_FRAMES;
            demo_setrgba_white(s * s * (3.0f - 2.0f * s));

            if (img)
            {
                demo_drawfullimage_rgb(img, w, h);
            }
            demo_vsync(true);
            demo_blit();
        }

        demo_setrgba_white(0.0f);

        for (a = 0; a < 5000 && !demo_wantstoquit(); a++)
        {
            if (img)
            {
                demo_drawfullimage_rgb(img, w, h);
            }
            demo_vsync(true);  // advance audio for music sync
            demo_blit();

            if (Music::getPlusFlags() > -16) break;
        }

        if (img) demo_freepng(img);
    }
}