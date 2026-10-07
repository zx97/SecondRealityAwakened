#include "ResConfig.h"

const ResConfig g_availableResolutions[] = {
    {  320,  200, "320x200  (original DOS)" },
    {  640,  400, "640x400  (2x)" },
    { 1280,  720, "1280x720 (HD)" },
    { 1920, 1080, "1920x1080 (Full HD)" },
    { 2560, 1440, "2560x1440 (QHD)" },
    { 3840, 2160, "3840x2160 (4K)" },
    { 7680, 4320, "7680x4320 (8K)" },
};

const int g_resolutionCount = sizeof(g_availableResolutions) / sizeof(g_availableResolutions[0]);

ResConfig g_res = { 320, 200, "original" };
