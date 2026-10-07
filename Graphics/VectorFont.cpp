#include "Graphics/VectorFont.h"
#include "Resources/FontData.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "ThirdParty/stb_truetype.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

#include "Parts/Common.h"

extern float g_width, g_height;

namespace VectorFont
{
static stbtt_fontinfo s_font;
static unsigned char * s_ttfBuffer = nullptr;
static bool s_inited = false;

bool Init()
{
    if (s_inited) return true;
    // The typeface is embedded in the binary, so font rendering does not depend
    // on any file present at run time and works from any working directory,
    // including inside a Flatpak sandbox.
    const long sz = (long)Font::Data::liberation_serif_ttf_size;
    s_ttfBuffer = (unsigned char*)malloc((size_t)sz);
    if (!s_ttfBuffer) return false;
    std::memcpy(s_ttfBuffer, Font::Data::liberation_serif_ttf, (size_t)sz);
    if (!stbtt_InitFont(&s_font, s_ttfBuffer, 0)) { free(s_ttfBuffer); s_ttfBuffer=nullptr; return false; }
    s_inited = true;
    return true;
}

void Shutdown()
{
    if (s_ttfBuffer) { free(s_ttfBuffer); s_ttfBuffer=nullptr; }
    s_inited = false;
}

int RenderGlyph(char c, uint8_t * dest, int destStride, int fontHeight)
{
    if (!s_inited && !Init()) return 0;
    float scale = stbtt_ScaleForPixelHeight(&s_font, (float)fontHeight);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&s_font, (int)(unsigned char)c, scale, scale, &x0, &y0, &x1, &y1);
    int w = x1 - x0;
    int h = y1 - y0;
    if (w <=0 || h<=0) return 0;
    for (int y=0; y<h; ++y) memset(dest + y*destStride, 0, w);
    stbtt_MakeCodepointBitmap(&s_font, dest, w, h, destStride, scale, scale, (int)(unsigned char)c);
    return w;
}

void GenerateAtlas(char * atlas, int stride, int height, const char * charOrder)
{
    if (!s_inited && !Init()) return;
    memset(atlas, 0, stride * height);
    float scale = stbtt_ScaleForPixelHeight(&s_font, (float)height);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&s_font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);
    int x = 0;
    const int padding = 1;
    for (const char * p = charOrder; *p; ++p)
    {
        unsigned char ch = (unsigned char)*p;
        int ax, lsb;
        stbtt_GetCodepointHMetrics(&s_font, ch, &ax, &lsb);
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&s_font, ch, scale, scale, &x0, &y0, &x1, &y1);
        int gw = x1 - x0;
        int gh = y1 - y0;
        if (x + gw + padding >= stride) break;
        if (gw >0 && gh>0)
        {
            std::vector<uint8_t> bmp(gw * gh);
            stbtt_MakeCodepointBitmap(&s_font, bmp.data(), gw, gh, gw, scale, scale, ch);
            for (int yy=0; yy<gh; ++yy)
            {
                int ay = baseline + y0 + yy;
                if (ay <0 || ay >= height) continue;
                for (int xx=0; xx<gw; ++xx)
                {
                    uint8_t v = bmp[yy * gw + xx];
                    char q = 0;
                    if (v > 180) q = 3;
                    else if (v > 100) q = 2;
                    else if (v > 30) q = 1;
                    if (q) atlas[ay * stride + x + xx] = q;
                }
            }
        }
        int adv = (int)(ax * scale);
        if (adv < gw) adv = gw + padding;
        x += adv + padding;
    }
}

void GenerateAlkuFont(char * dest, int destSize)
{
    // Must match the exact glyph order ALKU scans with (alku_fonaorder),
    // otherwise the generated atlas maps characters to the wrong positions.
    // \x8f / \x99 are non-UTF8 bitmap symbols in the original; LiberationSerif
    // cannot render them, so they are left blank (still occupy an atlas slot).
    const char * order = "ABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyz0123456789"
                         "!?,.:\x8f\x8f()+-*='\x8f\x99";
    (void)destSize;
    GenerateAtlas(dest, 1500, 32, order);
}

void GenerateCreditsFont(char dest[][1500])
{
    const char * order = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ";
    char * flat = &dest[0][0];
    GenerateAtlas(flat, 1500, 16, order);
    for (int y = 16; y < 80; ++y) memcpy(dest[y], dest[y % 16], 1500);
}

void GenerateEndscrlFont(char dest[25][1550])
{
    const char * order = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";
    GenerateAtlas(&dest[0][0], 1550, 25, order);
}

void GenerateForestFont(uint8_t * dest, int w, int h)
{
    const char * order = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?,.:;()+-*='\" ";
    std::vector<char> tmp(w * h);
    GenerateAtlas(tmp.data(), w, h, order);
    for (int i = 0; i < w * h; ++i) dest[i] = (tmp[i] ? (uint8_t)(tmp[i] * 85) : 0);
}

void GenerateWaterFont(uint8_t * dest, int w, int h)
{
    const char * order = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?:,.\"()+- ";
    std::vector<char> tmp(w * h);
    GenerateAtlas(tmp.data(), w, h, order);
    for (int i = 0; i < w * h; ++i) dest[i] = (tmp[i] ? (uint8_t)(tmp[i] * 85) : 0);
}

// --- Signed Distance Field (SDF) ---

namespace
{
    constexpr const int SDF_PADDING = 8;
    constexpr const int SDF_CELL = 64;      // per-glyph SDF resolution
    constexpr const int SDF_GRID = 16;      // glyphs per atlas row/column

    struct SdfGlyph
    {
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
        int w = 0, h = 0, xOff = 0, yOff = 0, adv = 0;
        bool valid = false;
    };

    // One glyph table and one atlas. The demo and the debug overlay each own one:
    // sharing a single table meant that any part calling BuildSdfAtlas replaced
    // the UVs the overlay had already computed, so the overlay silently changed
    // typeface depending on which part was running.
    struct SdfContext
    {
        std::vector<unsigned char> atlas;
        int w = 0, h = 0;
        int pad = SDF_PADDING;
        SdfGlyph glyphs[256];
    };

    SdfContext s_demoCtx;
    SdfContext s_overlayCtx;
}

static const unsigned char * BuildSdfAtlasInto(SdfContext & ctx, const char * charSet,
                                              int fontSize, int & outAtlasW, int & outAtlasH, int & outPad)
{
    ctx.pad = SDF_PADDING;

    // Pack glyphs in a grid; each cell is SDF_CELL + 2*padding.
    const int cell = SDF_CELL + 2 * SDF_PADDING;
    ctx.w = SDF_GRID * cell;
    ctx.h = SDF_GRID * cell;
    ctx.atlas.assign((size_t)ctx.w * ctx.h, 0);

    float scale = stbtt_ScaleForPixelHeight(&s_font, (float)fontSize);

    for (int i = 0; charSet[i]; ++i)
    {
        unsigned char ch = (unsigned char)charSet[i];
        int glyph = stbtt_FindGlyphIndex(&s_font, ch);
        int ax = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&s_font, ch, &ax, &lsb);

        int gw = 0, gh = 0, gx0 = 0, gy0 = 0;
        unsigned char * sdf = stbtt_GetGlyphSDF(
            &s_font, scale, glyph,
            SDF_PADDING, 128, 10.0f,
            &gw, &gh, &gx0, &gy0);

        // Space / no-contour glyphs have an advance but no SDF bitmap.
        SdfGlyph & g = ctx.glyphs[ch];
        g.adv = (int)(ax * scale);
        g.valid = true;
        if (!sdf) continue;

        int col = i % SDF_GRID;
        int row = i / SDF_GRID;
        int px = col * cell;
        int py = row * cell;

        for (int y = 0; y < gh && py + y < ctx.h; ++y)
            for (int x = 0; x < gw && px + x < ctx.w; ++x)
                ctx.atlas[(size_t)(py + y) * ctx.w + (px + x)] = sdf[y * gw + x];

        stbtt_FreeSDF(sdf, nullptr);

        g.u0 = (float)px / (float)ctx.w;
        g.v0 = (float)py / (float)ctx.h;
        g.u1 = (float)(px + gw) / (float)ctx.w;
        g.v1 = (float)(py + gh) / (float)ctx.h;
        g.w = gw;
        g.h = gh;
        g.xOff = gx0;
        g.yOff = gy0;
    }

    outAtlasW = ctx.w;
    outAtlasH = ctx.h;
    outPad = SDF_PADDING;
    return ctx.atlas.data();
}

const unsigned char * BuildSdfAtlas(const char * charSet, int fontSize, int & w, int & h, int & pad)
{
    if (!s_inited && !Init()) return nullptr;
    return BuildSdfAtlasInto(s_demoCtx, charSet, fontSize, w, h, pad);
}

const unsigned char * BuildOverlaySdfAtlas(const char * charSet, int fontSize,
                                           int & w, int & h, int & pad)
{
    if (!s_inited && !Init()) return nullptr;
    return BuildSdfAtlasInto(s_overlayCtx, charSet, fontSize, w, h, pad);
}

static const SdfGlyph * glyphOf(SdfContext & ctx, char c)
{
    const SdfGlyph & g = ctx.glyphs[(unsigned char)c];
    return g.valid ? &g : nullptr;
}

bool GetGlyphUV(char c, float & u0, float & v0, float & u1, float & v1,
                int & w, int & h, int & xOff, int & yOff)
{
    const SdfGlyph * gp = glyphOf(s_demoCtx, c);
    const SdfGlyph & g = gp ? *gp : s_demoCtx.glyphs[(unsigned char)c];
    if (!g.valid) return false;
    u0 = g.u0; v0 = g.v0; u1 = g.u1; v1 = g.v1;
    w = g.w; h = g.h; xOff = g.xOff; yOff = g.yOff;
    return true;
}

int GetGlyphAdvance(char c)
{
    const SdfGlyph & g = s_demoCtx.glyphs[(unsigned char)c];
    return g.valid ? g.adv : 0;
}

bool GetOverlayGlyphUV(char c, float & u0, float & v0, float & u1, float & v1,
                       int & w, int & h, int & xOff, int & yOff)
{
    const SdfGlyph & g = s_overlayCtx.glyphs[(unsigned char)c];
    if (!g.valid) return false;
    u0 = g.u0; v0 = g.v0; u1 = g.u1; v1 = g.v1;
    w = g.w; h = g.h; xOff = g.xOff; yOff = g.yOff;
    return true;
}

int GetOverlayGlyphAdvance(char c)
{
    const SdfGlyph & g = s_overlayCtx.glyphs[(unsigned char)c];
    return g.valid ? g.adv : 0;
}

int RenderTextSdf(const char * text, uint32_t * rgba, int outW, int outH, float scale, float vPos)
{
    if (!rgba || outW <= 0 || outH <= 0 || !text) return 0;

    float total = 0;
    for (const char * p = text; *p; ++p)
        total += (float)GetGlyphAdvance(*p) * scale;

    float penX = ((float)outW - total) * 0.5f;
    const float baseY = (float)outH * vPos;

    for (const char * p = text; *p; ++p)
    {
        unsigned char ch = (unsigned char)*p;
        float u0, v0, u1, v1;
        int gw, gh, gx, gy;
        if (!GetGlyphUV((char)ch, u0, v0, u1, v1, gw, gh, gx, gy))
        {
            penX += (float)GetGlyphAdvance((char)ch) * scale;
            continue;
        }

        const float dw = (float)gw * scale;
        const float dh = (float)gh * scale;
        const float dx = penX + (float)gx * scale;
        const float dy = baseY + (float)gy * scale;

        // Sample the SDF atlas, supersample for quality.
        const int su = 2;
        for (int y = 0; y < (int)dh; ++y)
        {
            for (int x = 0; x < (int)dw; ++x)
            {
                int px = (int)dx + x;
                int py = (int)dy + y;
                if (px < 0 || px >= outW || py < 0 || py >= outH) continue;

                float cov = 0;
                for (int sy = 0; sy < su; ++sy)
                {
                    for (int sx = 0; sx < su; ++sx)
                    {
                        float fx = ((float)x + (float)(sx + 0.5f) / (float)su) / (float)dw;
                        float fy = ((float)y + (float)(sy + 0.5f) / (float)su) / (float)dh;
                        float tx = u0 + fx * (u1 - u0);
                        float ty = v0 + fy * (v1 - v0);
                        int ti = (int)(tx * (float)s_demoCtx.w);
                        int tj = (int)(ty * (float)s_demoCtx.h);
                        if (ti < 0) ti = 0; if (ti >= s_demoCtx.w) ti = s_demoCtx.w - 1;
                        if (tj < 0) tj = 0; if (tj >= s_demoCtx.h) tj = s_demoCtx.h - 1;
                        float d = ((float)s_demoCtx.atlas[(size_t)tj * s_demoCtx.w + ti] / 255.0f) - 0.5f;
                        cov += (d > 0.0f) ? 1.0f : 0.0f;
                    }
                }
                cov /= (float)(su * su);

                uint32_t & dst = rgba[(size_t)py * outW + px];
                const unsigned int a = (unsigned int)(cov * 255.0f);
                // Composite white text over the existing background.
                const unsigned int inv = 255u - a;
                const unsigned int dr = ((dst >> 16) & 0xFFu) * inv / 255u + a;
                const unsigned int dg = ((dst >> 8) & 0xFFu) * inv / 255u + a;
                const unsigned int db = (dst & 0xFFu) * inv / 255u + a;
                dst = (dr << 16) | (dg << 8) | db;
            }
        }

        penX += (float)GetGlyphAdvance((char)ch) * scale;
    }

    return (int)total;
}

int BuildTextQuads(const char * text, GpuTexVertex * quads, int maxQuads,
                   float baseY, float fontNdc, float aspect, float spacing)
{
    if (!text || !quads || maxQuads <= 0) return 0;

    // Glyph height in NDC is fontNdc; atlas is built at 56px.
    const float scale = fontNdc / 56.0f;

    // Total advance (in atlas pixels) to centre the line at x=0.
    float total = 0;
    for (const char * p = text; *p; ++p)
        total += (float)GetGlyphAdvance(*p) * scale * spacing;

    // penX is a NDC offset; divide once by aspect so the text is not stretched
    // on wide screens. The per-glyph widths/advances are also scaled by 1/aspect.
    float penX = -total * 0.5f / aspect;
    int quadCount = 0;

    for (const char * p = text; *p && quadCount < maxQuads; ++p)
    {
        char ch = *p;
        float u0, v0, u1, v1;
        int gw, gh, gx, gy;
        if (!GetGlyphUV(ch, u0, v0, u1, v1, gw, gh, gx, gy))
        {
            penX += (float)GetGlyphAdvance(ch) * scale * spacing / aspect;
            continue;
        }

        // Glyph box: gx/gy are offsets from the pen, gw/gh the box size.
        // stb_truetype gy is negative above the baseline, and NDC y is +1 at the
        // top, so the glyph top is baseY - gy*scale (moves upward when gy<0).
        const float x0 = penX + (float)gx * scale / aspect;
        const float x1 = x0 + (float)gw * scale / aspect;
        const float yTop = baseY - (float)gy * scale;
        const float yBot = yTop - (float)gh * scale;

        GpuTexVertex * q = quads + quadCount * 4;
        q[0].px = x0; q[0].py = yTop; q[0].pz = 0; q[0].u = u0; q[0].v = v0;
        q[1].px = x1; q[1].py = yTop; q[1].pz = 0; q[1].u = u1; q[1].v = v0;
        q[2].px = x0; q[2].py = yBot; q[2].pz = 0; q[2].u = u0; q[2].v = v1;
        q[3].px = x1; q[3].py = yBot; q[3].pz = 0; q[3].u = u1; q[3].v = v1;
        ++quadCount;

        penX += (float)GetGlyphAdvance(ch) * scale * spacing / aspect;
    }

    return quadCount;
}

int BuildTextQuadsJustified(const char * text, GpuTexVertex * quads, int maxQuads,
                            float baseY, float fontNdc, float aspect,
                            float targetWidth, float spacing)
{
    if (!text || !quads || maxQuads <= 0) return 0;

    const float scale = fontNdc / 56.0f;

    // Natural width of the line (in atlas px) and count of space glyphs.
    float natural = 0;
    int spaceCount = 0;
    int charCount = 0;
    for (const char * p = text; *p; ++p)
    {
        natural += (float)GetGlyphAdvance(*p) * scale * spacing;
        if (*p == ' ') ++spaceCount;
        ++charCount;
    }
    if (charCount == 0) return 0;

    // Extra NDC width to reach the target (targetWidth is NDC, x not yet /aspect).
    // We add it to each space advance; if no spaces, distribute across all chars.
    const float targetPx = targetWidth * aspect; // back to atlas-px scale
    float extra = targetPx - natural;
    int spreadCount = (spaceCount > 0) ? spaceCount : (charCount - 1 > 0 ? charCount - 1 : 1);
    if (spreadCount < 1) spreadCount = 1;
    const float extraPer = (extra / (float)spreadCount);

    // Centre the justified block.
    float penX = -targetPx * 0.5f / aspect;
    int quadCount = 0;

    for (const char * p = text; *p && quadCount < maxQuads; ++p)
    {
        char ch = *p;
        float u0, v0, u1, v1;
        int gw, gh, gx, gy;
        if (!GetGlyphUV(ch, u0, v0, u1, v1, gw, gh, gx, gy))
        {
            penX += (float)GetGlyphAdvance(ch) * scale * spacing / aspect;
            if (ch == ' ') penX += extraPer / aspect;
            continue;
        }

        const float x0 = penX + (float)gx * scale / aspect;
        const float x1 = x0 + (float)gw * scale / aspect;
        const float yTop = baseY - (float)gy * scale;
        const float yBot = yTop - (float)gh * scale;

        GpuTexVertex * q = quads + quadCount * 4;
        q[0].px = x0; q[0].py = yTop; q[0].pz = 0; q[0].u = u0; q[0].v = v0;
        q[1].px = x1; q[1].py = yTop; q[1].pz = 0; q[1].u = u1; q[1].v = v0;
        q[2].px = x0; q[2].py = yBot; q[2].pz = 0; q[2].u = u0; q[2].v = v1;
        q[3].px = x1; q[3].py = yBot; q[3].pz = 0; q[3].u = u1; q[3].v = v1;
        ++quadCount;

        penX += (float)GetGlyphAdvance(ch) * scale * spacing / aspect;
        if (ch == ' ') penX += extraPer / aspect;
    }

    return quadCount;
}

} // namespace VectorFont
