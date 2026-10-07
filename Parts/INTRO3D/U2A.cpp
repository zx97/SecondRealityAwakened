#define _CRT_SECURE_NO_WARNINGS

#include "Parts/VISU/VISU.h"

#include "U2A_DATA.h"

#include <cstdio>
#include <cstdlib>

namespace U2A
{
    namespace
    {
        const char u2a_scene[64] = { "U2A" };

        int repeat{};

        char bg2[16384 * 4]{};

        // Counts how many frames a stream still holds, without executing it.
        // The byte layout is the one U2A::main() parses, so a local pointer can
        // follow it exactly: each command's length is a function of its pflag, and
        // a frame ends at an 0xff byte followed by a value <= 0x7f. This is the
        // countdown that lets the fade start a fixed lead before the animation
        // ends, so the ships dissolve while still moving instead of parking.
        int count_stream_frames(const unsigned char * p)
        {
            const auto lsbytes = [](int32_t f) {
                switch (f & 3)
                {
                    case 0: return 0;
                    case 1: return 1;
                    case 2: return 2;
                    default: return 4;
                }
            };

            int frames = 0;
            for (;;)
            {
                unsigned char a = *p++;

                if (a == 0xff)
                {
                    a = *p++;
                    if (a <= 0x7f)
                    {
                        ++frames;
                        continue;
                    }
                    if (a == 0xff)
                    {
                        break;
                    }
                }

                if ((a & 0xc0) == 0xc0)
                {
                    a = *p++;
                }

                int32_t pflag = 0;
                switch (a & 0x30)
                {
                    case 0x10: pflag = *p++; break;
                    case 0x20: pflag = p[0] | ((int32_t)p[1] << 8); p += 2; break;
                    case 0x30: pflag = p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16); p += 3; break;
                    default: break;
                }

                p += lsbytes(pflag);
                p += lsbytes(pflag >> 2);
                p += lsbytes(pflag >> 4);

                for (int b = 0; b < 9; ++b)
                    if (pflag & (0x80 << b))
                        p += (pflag & 0x40) ? 2 : 1;
            }

            return frames;
        }
    }

    void u2a_copper2()
    {
        Visu::syncframe++;

        if (Visu::cl[0].ready == 2) Visu::cl[0].ready = 0;
        if (Visu::cl[1].ready == 2) Visu::cl[1].ready = 0;
        if (Visu::cl[2].ready == 2) Visu::cl[2].ready = 0;
        if (Visu::cl[3].ready == 2) Visu::cl[3].ready = 0;

        Visu::deadlock++;
        Visu::coppercnt++;

        if (Visu::copperdelay > 0)
        {
            Visu::copperdelay--;
        }

        if (Visu::copperdelay > 0)
        {
            return;
        }

        Visu::copperdelay = 0;

        if (Visu::cl[Visu::clr].ready)
        {
            Visu::cl[(Visu::clr - 1) & 3].ready = 2;
            Visu::copperdelay = Visu::cl[Visu::clr].frames;
            Visu::clr++;
            Visu::clr &= 3;
        }
        else
        {
            Visu::avgrepeat++;
        }
    }

    void main()
    {
        Visu::reset();

        // Full window, not a 4:3 pillarbox: the hires picture ALKU leaves behind
        // already covers the whole window, so boxing the ships into the middle
        // would strand them in a letterbox strip. drawobject_gpu() reads this
        // too, so the projection and the viewport agree.
        g_respectRatio = false;

        // Needed for the aspect ratio, not for a background. m_meshBgScreen is
        // what makes drawobject_gpu() use 4/3 instead of the window aspect, so the
        // ships keep their own proportions on a 21:9 screen instead of stretching.
        // It does NOT paint over the hires picture: the cpuPixels composite is
        // gated on !bgLayerActive (Graphics.cpp) and the background layer is still
        // active here, so only the 4:3 viewport comes back.
        demo_meshbackgroundscreen();

        repeat = {};

        CLEAR(bg2);

        // The fade runs over the last frames of the animation itself and ends as
        // the stream ends, so the ships dissolve while they are still moving and
        // are never seen parked at the vanishing point. 35 frames is ~0.5 s at
        // 70 fps. It is not keyed on the music: the gap before the blast differs
        // between a full run and a standalone launch, the animation length does not.
        constexpr int SHIP_FADE_FRAMES = 35;

        int a{}, b{}, c{}, e{}, f{}, g{}, x{}, y{};

        // No clearScreen() here: it painted a black frame between the retained
        // hires picture and the first ship. The picture from ALKU is still on
        // screen and the ships draw over it.

        sprintf(Visu::tmpname, "%s.00M", u2a_scene);
        Visu::scene0 = Visu::scenem = Visu::readfile(Visu::tmpname);

        memcpy(Visu::scene0 + 16 + 192 * 3, Data::u2a_bg + 16, 64 * 3);

        unsigned int u = 0;

        for (y = 0; y < SCREEN_HEIGHT; y++)
        {
            for (x = 0; x < SCREEN_WIDTH; x++)
            {
                a = Data::u2a_bg[16 + PaletteByteCount + x + y * SCREEN_WIDTH];

                bg2[u++] = static_cast<char>(a);
            }
        }

        if (Visu::scene0[15] == 'C') Visu::city = 1;
        if (Visu::scene0[15] == 'R') Visu::city = 2;

        short * ip = (short *)(Visu::scene0 + LONGAT(Visu::scene0 + 4));

        int d{};
        Visu::conum = d = *ip++;

        for (f = -1, c = 1; c < d; c++)
        {
            e = *ip++;
            if (e > f)
            {
                f = e;
                sprintf(Visu::tmpname, "%s.%03i", u2a_scene, e);
                Visu::co[c].o = Visu::loadobject(Visu::tmpname);
                memset(Visu::co[c].o->r, 0, sizeof(Visu::rmatrix));
                memset(Visu::co[c].o->r0, 0, sizeof(Visu::rmatrix));
                Visu::co[c].index = e;
                Visu::co[c].on = 0;
            }
            else
            {
                for (g = 0; g < c; g++)
                    if (Visu::co[g].index == e) break;

                memcpy(Visu::co + c, Visu::co + g, sizeof(Visu::s_co));
                Visu::co[c].o = Visu::getNewObject();
                memcpy(Visu::co[c].o, Visu::co[g].o, sizeof(Visu::object));
                Visu::co[c].o->r = Visu::getNewRMatrix();
                Visu::co[c].o->r0 = Visu::getNewRMatrix();
                memset(Visu::co[c].o->r, 0, sizeof(Visu::rmatrix));
                memset(Visu::co[c].o->r0, 0, sizeof(Visu::rmatrix));
                Visu::co[c].on = 0;
            }
        }

        Visu::co[0].o = &Visu::camobject;
        Visu::camobject.r = &Visu::cam;
        Visu::camobject.r0 = &Visu::cam;

        sprintf(Visu::tmpname, "%s.0AA", u2a_scene);
        ip = (short *)Visu::readfile(Visu::tmpname);
        Visu::scl = 0;
        while (*ip)
        {
            a = *ip;
            if (a == -1) break;
            sprintf(Visu::tmpname, "%s.0%c%c", u2a_scene, a / 10 + 'A', a % 10 + 'A');
            Visu::scenelist[Visu::scl].data = Visu::readfile(Visu::tmpname);
            Visu::scl++;
            ip += 2;
        }

        Visu::resetscene();

        if (!Shim::isDemoFirstPart())
        {
            for (;;)
            {
                a = Music::getOrder();
                b = Music::getRow();

                if (a > 10 && b > 46) break;
                if (demo_wantstoquit()) return;

                AudioPlayer::Update(true);
            }
        }
        Visu::init();
        char * cp = (char *)(Visu::scenem + 16);
        Shim::outp(0x3c8, 0);
        for (a = 0; a < static_cast<int>(PaletteByteCount); a++)
            Shim::outp(0x3c9, cp[a]);
        Visu::window(0L, 319L, 25L, 174L, 512L, 9999999L);

        // No clearScreen() here: it painted a black frame between the retained
        // hires picture and the first ship. The picture from ALKU is still on
        // screen and the ships draw over it.

        Visu::xit = 0;
        Visu::currframe = 0;
        Visu::coppercnt = 0;
        Visu::syncframe = 0;
        Visu::avgrepeat = 1;
        Visu::cl[0].ready = 0;
        Visu::cl[1].ready = 0;
        Visu::cl[2].ready = 0;
        Visu::cl[3].ready = 1;
        int fov = 0;

        // Total animation frames, counted once before the loop. Visu::sp still
        // points at the stream start here, so the countdown below is exact.
        const int streamTotalFrames = count_stream_frames(Visu::sp);
        int streamFramesParsed = 0;
        if (std::getenv("SR_TRACE"))
        {
            std::fprintf(stderr, "[u2a] stream frames=%d\n", streamTotalFrames);
        }

        while (!demo_wantstoquit() && !Visu::xit)
        {
            int onum;
            int32_t pflag;
            int32_t dis;
            int32_t l;

            Visu::object * o;
            Visu::rmatrix * r;

            Visu::deadlock = 0;

            // Draw to free frame. Deliberately NOT clearing to bg2: that buffer is
            // U2A's own 320x200 picture (Data::u2a_bg) and painting it every frame
            // is what covered the hires picture ALKU leaves behind. The ships are
            // drawn over the retained picture instead.

            // Field of vision
            Visu::cameraangle(static_cast<Visu::angle>(fov));
            // Countdown to the end of the animation: fully opaque, then fall to 0
            // over the last SHIP_FADE_FRAMES. Same value for every ship, so it is
            // computed once. Guarded on a sane total so a bad pre-scan cannot
            // blank the ships outright.
            float shipFade = 1.0f;
            const int framesRemaining = streamTotalFrames - streamFramesParsed;
            if (streamTotalFrames > SHIP_FADE_FRAMES && framesRemaining < SHIP_FADE_FRAMES)
            {
                float t = (float)(SHIP_FADE_FRAMES - framesRemaining) / (float)SHIP_FADE_FRAMES;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                shipFade = 1.0f - t * t * (3.0f - 2.0f * t);
                if (std::getenv("SR_TRACE"))
                {
                    std::fprintf(stderr, "[u2a] remaining=%d fade=%.3f\n", framesRemaining, shipFade);
                }
            }

            // Calc matrices and add to order list (only enabled objects)
            Visu::ordernum = 0;
            /* start at 1 to skip camera */

            for (a = 1; a < Visu::conum; a++)
                if (Visu::co[a].on)
                {
                    Visu::order[Visu::ordernum++] = a;
                    o = Visu::co[a].o;
                    o->fade = shipFade;

                    memcpy(o->r, o->r0, sizeof(Visu::rmatrix));
                    Visu::calc_applyrmatrix(o->r, &Visu::cam);
                    b = o->pl[0][1]; // center vertex
                    Visu::co[a].dist = Visu::calc_singlez(b, o->v0, o->r);
                }

            // Zsort
            if (Visu::city == 1)
            {
                Visu::co[2].dist = 1000000000L;  // for CITY scene, test
                Visu::co[7].dist = 1000000000L;  // for CITY scene, test
                Visu::co[13].dist = 1000000000L; // for CITY scene, test
            }

            if (Visu::city == 2)
            {
                Visu::co[14].dist = 1000000000L; // for CITY scene, test
            }

            for (a = 0; a < Visu::ordernum; a++)
            {
                dis = Visu::co[c = Visu::order[a]].dist;
                for (b = a - 1; b >= 0 && dis > Visu::co[Visu::order[b]].dist; b--)
                    Visu::order[b + 1] = Visu::order[b];
                Visu::order[b + 1] = c;
            }

            // Draw
            for (a = 0; a < Visu::ordernum; a++)
            {
                o = Visu::co[Visu::order[a]].o;
                Visu::drawobject(o);
            }

            // **** Drawing completed **** //
            // calculate how many frames late of schedule
            Visu::avgrepeat = (Visu::avgrepeat + (Visu::syncframe - Visu::currframe) + 1) / 2;
            repeat = Visu::avgrepeat;
            if (repeat < 1) repeat = 1;
            Visu::cl[Visu::clw].frames = repeat;
            Visu::cl[Visu::clw].ready = 1;
            Visu::clw++;
            Visu::clw &= 3;
            // advance that many frames
            Visu::currframe += repeat;

            while (repeat-- && !Visu::xit)
            {
                // parse animation stream for 1 frame
                onum = 0;
                while (!Visu::xit)
                {
                    a = *Visu::sp++;
                    if (a == 0xff)
                    {
                        a = *Visu::sp++;
                        if (a <= 0x7f)
                        {
                            ++streamFramesParsed;
                            fov = a << 8;
                            break;
                        }
                        else if (a == 0xff)
                        {
                            Visu::resetscene();
                            Visu::xit = 1;
                            continue;
                        }
                    }
                    if ((a & 0xc0) == 0xc0)
                    {
                        onum = ((a & 0x3f) << 4);
                        a = *Visu::sp++;
                    }
                    onum = (onum & 0xff0) | (a & 0xf);
                    b = 0;

                    switch (a & 0xc0)
                    {
                        case 0x80:
                            b = 1;
                            Visu::co[onum].on = 1;
                            break;
                        case 0x40:
                            b = 1;
                            Visu::co[onum].on = 0;
                            break;
                    }

                    if (onum >= Visu::conum)
                    {
                        return;
                    }

                    r = Visu::co[onum].o->r0;

                    pflag = 0;
                    switch (a & 0x30)
                    {
                        case 0x00:
                            break;
                        case 0x10:
                            pflag |= *Visu::sp++;
                            break;
                        case 0x20:
                            pflag |= Visu::sp[0];
                            pflag |= (int32_t)Visu::sp[1] << 8;
                            Visu::sp += 2;
                            break;
                        case 0x30:
                            pflag |= Visu::sp[0];
                            pflag |= (int32_t)Visu::sp[1] << 8;
                            pflag |= (int32_t)Visu::sp[2] << 16;
                            Visu::sp += 3;
                            break;
                    }

                    l = Visu::lsget(static_cast<unsigned char>(pflag));
                    r->x += l;
                    l = Visu::lsget(static_cast<unsigned char>(pflag >> 2));
                    r->y += l;
                    l = Visu::lsget(static_cast<unsigned char>(pflag >> 4));
                    r->z += l;

                    if (pflag & 0x40)
                    { // word matrix
                        for (b = 0; b < 9; b++)
                            if (pflag & (0x80 << b))
                            {
                                r->m[b] += Visu::lsget(2);
                            }
                    }
                    else
                    { // byte matrix
                        for (b = 0; b < 9; b++)
                            if (pflag & (0x80 << b))
                            {
                                r->m[b] += Visu::lsget(1);
                            }
                    }
                }
            }

            demo_blit();
            demo_vsync();
            u2a_copper2();
        }

        if (std::getenv("SR_TRACE"))
        {
            std::fprintf(stderr, "[u2a] stream-end order=%d row=%d sync=%d parsed=%d/%d\n",
                         Music::getOrder(), Music::getRow(), Music::sync(),
                         streamFramesParsed, streamTotalFrames);
        }

        // The countdown above drove the ships to 0 on the last animation frames,
        // so the field is already empty. Present one clean frame with no ship mesh
        // so nothing near-invisible carries into the held background of part 02.
        demo_blit();
        demo_vsync();

        Visu::clearbg((char *)bg2);
    }
}
