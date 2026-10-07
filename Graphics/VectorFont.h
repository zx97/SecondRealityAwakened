#pragma once
#include <cstdint>

// Forward-declared at global scope: the real definition lives in Parts/Common.h.
struct GpuTexVertex;

namespace VectorFont
{
bool Init();
void Shutdown();
int RenderGlyph(char c, uint8_t * dest, int destStride, int fontHeight);
void GenerateAtlas(char * atlas, int stride, int height, const char * charOrder);
void GenerateAlkuFont(char * dest, int destSize);
void GenerateCreditsFont(char dest[][1500]);
void GenerateEndscrlFont(char dest[25][1550]);
void GenerateForestFont(uint8_t * dest, int w, int h);
void GenerateWaterFont(uint8_t * dest, int w, int h);

// --- Signed Distance Field (SDF) API for resolution-independent vector text ---

// Builds a single SDF texture atlas for the given character set at `fontSize`
// pixels. Returns the atlas buffer (owned by VectorFont) or nullptr on failure.
// `outAtlasW/H` receive the atlas size; `outPad` the SDF padding in texels.
// Glyph UV rectangles are retrievable via GetGlyphUV().
const unsigned char * BuildSdfAtlas(const char * charSet, int fontSize, int & outAtlasW, int & outAtlasH, int & outPad);

// The debug overlay owns a second glyph table so a part rebuilding the demo one
// cannot change the overlay's typeface mid-run.
const unsigned char * BuildOverlaySdfAtlas(const char * charSet, int fontSize,
                                           int & outAtlasW, int & outAtlasH, int & outPad);
bool GetOverlayGlyphUV(char c, float & u0, float & v0, float & u1, float & v1,
                       int & w, int & h, int & xOff, int & yOff);
int GetOverlayGlyphAdvance(char c);

// Returns the UV rect (u0,v0,u1,v1) of `c` in the last built SDF atlas,
// and the glyph's on-screen size (w,h) and offset (xOff,yOff) in pixels.
bool GetGlyphUV(char c, float & u0, float & v0, float & u1, float & v1,
                int & w, int & h, int & xOff, int & yOff);
int GetGlyphAdvance(char c);

// Renders `text` into `rgba` (RGBA8, alpha=coverage) at `outW`x`outH` using
// the SDF atlas. Returns the rendered text width, or 0. `scale` multiplies
// glyph size; `vPos` is the baseline as a fraction of height (0..1).
int RenderTextSdf(const char * text, uint32_t * rgba, int outW, int outH, float scale, float vPos);

// Builds per-glyph quads for `text` centred horizontally in NDC (x=0) with its
// baseline at `baseY` NDC (+1 top .. -1 bottom). Glyph height is `fontNdc`
// (fraction of window height); X is divided by `aspect` for wide screens.
// Writes up to `maxQuads` quads into `quads`, returns the count.
int BuildTextQuads(const char * text, GpuTexVertex * quads, int maxQuads,
                   float baseY, float fontNdc, float aspect, float spacing = 1.0f);

// Like BuildTextQuads but justifies the line to `targetWidth` NDC by spreading
// the space glyph advances (and any leftover across all glyphs) evenly, so
// every line of a paragraph forms a uniform block.
int BuildTextQuadsJustified(const char * text, GpuTexVertex * quads, int maxQuads,
                            float baseY, float fontNdc, float aspect,
                            float targetWidth, float spacing = 1.0f);

} // namespace VectorFont
