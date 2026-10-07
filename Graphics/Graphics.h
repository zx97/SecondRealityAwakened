#include <vector>
#include <string>
#include <map>

#include "Parts/Common.h"

class Graphics
{
 public:
    enum class WindowType
    {
        Fullscreen,
        Windowed,
    };

    Graphics() = default;
    ~Graphics() = default;

    bool Init(WindowType _fullscreen);
    void Update();
    void Close();

    bool WantsToQuit();

    void ChangeMode(int x, int y, float jsss = Default_JSSS);

    void ResetPartState();

    void SetGpuEffect(const char * fragmentShader);
    void ClearGpuEffect();
    void SetGpuTexture(const char * name, const unsigned char * luminance, int w, int h);
    void SetGpuTextureRgb(const char * name, const unsigned char * rgb, int w, int h);
    void SetGpuTextureLa(const char * name, const unsigned char * la, int w, int h);
    void SetUniform1f(const char * name, float v);
    void SetUniform2f(const char * name, float x, float y);
    void SetUniform3f(const char * name, float x, float y, float z);
    void SetUniform1i(const char * name, int v);
    void SetUniform4fv(const char * name, const float * data, int count);
    void DrawPoints(const void * data, int count, int stride, float pointSize, float pointStretch = 0.0f);

    // Additive blending for point effects whose dots should glow where they
    // overlap instead of overwriting each other. The points fragment shader
    // already writes the radial falloff into alpha, so SRC_ALPHA/ONE is all it
    // takes. Caller must turn it off again: the state is global.
    void SetPointsAdditive(bool on);

    // Output fade for point effects, mirroring demo_drawmesh's fade argument.
    // The points shader already multiplies its colour by uFade.
    void SetPointsFade(float fade);

    // Sleeps and pumps events while the demo is paused, so the key that resumes
    // is seen. Lives here because that is where SDL event handling is.
    void WaitWhilePaused();

    // SDL keys and window events, split out of Update() so the pause loop can
    // dispatch the resume key while nothing else is running.
    void PollSdlEvents();
    void DrawMesh(const GpuMeshVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16, const float * model16, float fade = 1.0f);
    void DrawTexMesh(const GpuTexVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16);
    void SdfTexture(const unsigned char * data, int w, int h);
    void DrawSdfText(const GpuTexVertex * quads, int quadCount, const float * mvp16, float r, float g, float b, bool sharp = false, bool overlay = false);
    // perFrame says the caller rewrites the pixels in place every frame while
    // keeping the same pointer. FlushRgbaQuads() otherwise caches the upload on
    // the pointer, which is what lets a still picture (the hires troll) skip
    // glTexImage2D entirely -- but that silently freezes a part that mutates
    // its buffer, so the cache cannot tell the two cases apart on its own.
    void RgbaTexture(const unsigned char * data, int w, int h, bool isRgb = false, bool perFrame = false);
    void DrawRgbaQuad(const GpuTexVertex * quads, int quadCount, const float * mvp16, float blur = 0.0f);

    // Picture drawn behind the Shim::cpuPixels composite. A part cannot get this
    // ordering through DrawRgbaQuad(), whose quads are flushed after the SDF text
    // and would cover it.
    void SetBackgroundLayer(const unsigned char * rgb, int w, int h);
    void SetBackgroundLayerScroll(float u0, float u1, float alpha, float halfW);
    void ClearBackgroundLayer();
    void InitBackgroundLayer();
    void SetRgbaWhite(float white);

    // Procedural plasma shockwave torus. Pure GPU: the part supplies a static
    // torus and this grows it, spins the plasma inside and keeps only the outer
    // surface visible, so no per-frame CPU geometry is needed.
    void DrawExplosion(const GpuTorusVertex * verts, int vertCount, const unsigned short * indices,
                       int indexCount, float grow, float spin, float alpha,
                       bool fire = false, float centerX = 0.0f, float centerY = 0.0f);
    void FlushExplosion();
    void SetForceMeshPass(bool on) { m_forceMeshPass = on; }
    void SetExplosionFlash(float v);

    // Full-screen black fade drawn over the finished frame (0 = off), for the
    // graceful quit.
    void SetExitFade(float v);
    void MeshTexture(const unsigned char * data, int w, int h);
    void MeshPalette(const unsigned char * vga768);
    void MeshTime(float t);
    void MeshWarp(float w);
    void MeshBackground(float r, float g, float b);
    void MeshBackgroundScreen();
    void MeshViewport(int x0, int y0, int x1, int y1);
    float WindowAspect() const;
    float WindowHeight() const;
    float FramebufferAspect() const;
    bool MeshBackgroundScreenActive() const { return m_meshBgScreen; }
    void SetTextOverlay(void (*fn)(unsigned int * rgba, int w, int h));
    void SetSdfTextCallback(void (*fn)());

    // The overlay's own glyph atlas, so a part installing its charset cannot
    // invalidate the UVs the overlay already computed.
    void SetOverlaySdfTexture(const unsigned char * data, int w, int h);

    // Composite backdrop: the letterbox around the CPU bitmap is painted with
    // this colour instead of black, and the bitmap is scaled about the window
    // centre. Defaults (black, 1.0) leave every existing part untouched; reset
    // per part. Used by the white logo handover in part 03.
    void SetBackgroundColor(float r, float g, float b);
    void SetCompositeScale(float s);

 private:
    void PrepareTextureForGPU();
    void UpdateMeshPass();
    void FlushSdfQuads();
    void FlushRgbaQuads();
    bool m_meshBgScreen = false;

    struct BackgroundLayer
    {
        unsigned int prog = 0;
        int locAlpha = -1;
        int locTex = -1;
        int locPos = -1;
        int locUV = -1;
        unsigned int tex = 0;
        int w = 0;
        int h = 0;
        const unsigned char * data = nullptr;
        float u0 = 0.0f;
        float u1 = 1.0f;
        float alpha = 0.0f;
        float halfW = 1.0f;
    };
    BackgroundLayer m_bgLayer;

    struct MeshCmd
    {
        std::vector<float> vb;
        std::vector<unsigned short> ib;
        float mvp[16]{};
        float model[16]{};
        float fade = 1.0f;
    };

    struct TexMeshCmd
    {
        std::vector<float> vb;
        std::vector<unsigned short> ib;
        float mvp[16]{};
    };

    struct ExplosionCmd
    {
        std::vector<float> vb;
        std::vector<unsigned short> ib;
        float grow = 0.0f;
        float spin = 0.0f;
        float alpha = 1.0f;
        bool fire = false;          // fireball sprite instead of a torus
        float centerX = 0.0f;       // explosion point, NDC
        float centerY = 0.0f;
    };

    bool m_forceMeshPass = false;
    std::vector<ExplosionCmd> m_explosions;
    std::vector<MeshCmd> m_meshes;
    std::vector<TexMeshCmd> m_texMeshes;
    const unsigned char * m_texData = nullptr;
    int m_texW = 0;
    int m_texH = 0;
    float m_texTime = 0.0f;
    float m_meshWarp = 1.0f;

    struct SdfCmd
    {
        std::vector<float> vb;
        std::vector<unsigned short> ib;
        float mvp[16]{};
        float color[3]{};
        // Per command, not global: the overlay wants hard edges because at its
        // size the antialiased ramp reads as grey blur, while the parts keep the
        // smooth default. Keying this off "is an overlay atlas loaded" leaked the
        // hard edges into every part's text.
        bool sharp = false;
        bool useOverlay = false;
    };
    std::vector<SdfCmd> m_sdfQuads;
    const unsigned char * m_sdfData = nullptr;
    int m_sdfW = 0;
    int m_sdfH = 0;

    struct RgbaCmd
    {
        std::vector<float> vb;
        std::vector<unsigned short> ib;
        float mvp[16]{};
        float blur = 0.0f;
    };
    std::vector<RgbaCmd> m_rgbaQuads;
    const unsigned char * m_rgbaData = nullptr;
    int m_rgbaW = 0;
    int m_rgbaH = 0;
    bool m_rgbaIsRgb = false;

    float m_meshClear[3]{};
    bool m_meshScissorOn = false;
    int m_meshScissor[4]{};
    void (*m_textOverlay)(unsigned int * rgba, int w, int h) = nullptr;
    void (*m_sdfTextCb)() = nullptr;

    unsigned int m_effectProg = 0;
    unsigned int m_pointsProg = 0;
    unsigned int m_pointsVBO = 0;
    int m_effectLocResolution = -1;
    int m_effectLocViewport = -1;
    int m_effectLocSourceSize = -1;
    int m_effectLocTime = -1;
    int m_effectLocFrame = -1;
    int m_effectLocOverlay = -1;
    int m_pointsLocResolution = -1;
    int m_pointsLocFade = -1;
    float m_pointsFade = 1.0f;
    int m_pointsLocTime = -1;
    int m_pointsLocFrame = -1;
    int m_pointsLocPalette = -1;
    int m_pointsLocStretch = -1;
    // 0 = plain circular discs; > 0 elongates each disc along the screen-space
    // direction of travel (radial from the vanishing point). Part 05 TUNNELI
    // sets this; DOTS and DDSTARS leave it at 0 and are unchanged.
    float m_pointsStretch = 0.0f;
    int m_pointsCount = 0;
    const unsigned char * m_overlaySdfData = nullptr;
    int m_overlaySdfW = 0;
    int m_overlaySdfH = 0;
    int m_pointsStride = 0;
    float m_pointSize = 1.0f;

    struct EffectTexture
    {
        unsigned int tex = 0;
        int w = 0;
        int h = 0;
    };
    std::map<std::string, EffectTexture> m_effectTextures;

    // Appended, never inserted: ClearGpuEffect() and the part-change path
    // depend on the layout above staying put.
    bool m_rgbaPerFrame = false;
};