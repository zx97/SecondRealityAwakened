#pragma once

#include <cstdint>

struct ResConfig
{
    int32_t width;
    int32_t height;
    const char * label;
};

extern ResConfig g_res;

extern const ResConfig g_availableResolutions[];
extern const int g_resolutionCount;

inline int32_t ScreenWidth() { return g_res.width; }
inline int32_t ScreenHeight() { return g_res.height; }
inline int32_t ScreenSize() { return g_res.width * g_res.height; }

inline int32_t DoubleScreenWidth() { return g_res.width * 2; }
inline int32_t DoubleScreenHeight() { return g_res.height * 2; }
inline int32_t DoubleScreenSize() { return g_res.width * 2 * g_res.height * 2; }

inline int32_t PlanarWidth() { return g_res.width / 4; }
