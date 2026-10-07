#include "Graphics.h"

#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl3.h>
#include <SDL.h>
#include <SDL_opengles2.h>
#include "Parts/VISU/VISU.h"

extern int g_partSkipDelta;

#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

extern "C"
{
    int is_firefox(void);
}

#endif // __EMSCRIPTEN__

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include "Music/audioPlayer.h"

#include "ThirdParty/xbrz/xbrz.h"

struct Float4
{
    float x{}, y{}, z{}, w{};
};

struct Float3
{
    float x{}, y{}, z{};
};

struct Float2
{
    float u{}, v{};
};

struct Vertex
{
    Float3 Pos{};
    Float2 Tex{};
};

struct CBChangesEveryFrame
{
    Float4 ratio{};
    Float4 JSSS{};
};

float g_width = 640.0f;
float g_height = 400.0f;
float g_sourceWidth = 1.0f;
float g_sourceHeight = 1.0f;
float g_jsss = 0.80f;

extern int g_windowWidth;
extern int g_windowHeight;

int g_demoScreenWidth = SCREEN_WIDTH;
int g_demoScreenHeight = SCREEN_HEIGHT;

int g_virtualWidth = 640;
int g_virtualHeight = 400;
std::vector<unsigned int> g_screen32(640 * 400, 0);

#ifdef _DEBUG
const float ClearColor[4] = { 48.0f / 255.0f, 48.0f / 255.0f, 48.0f / 255.0f, 1.0f };
#else
const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
#endif // _DEBUG

// Per-part composite backdrop, set through Graphics::SetBackgroundColor() /
// SetCompositeScale() and reset by ResetPartState(). File scope because
// Render() is a free function.
float g_bgClear[3] = { 0.0f, 0.0f, 0.0f };
float g_compositeScale = 1.0f;

bool g_wantsToQuit{};

extern bool g_respectRatio;
extern bool g_hires;
// Defined in main.cpp: ESC / window close requests the graceful fade there.
extern bool g_exitFadeRequest;

namespace
{
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_WebGL_context = 0;
#endif

    SDL_Window * g_windows = nullptr;
    SDL_GLContext g_context = nullptr;

    GLuint g_prog = 0;
    GLint g_Loc_uResolution = -1;
    GLint g_Loc_uTex0 = -1;
    GLint g_Loc_uTex1 = -1;
    GLuint g_VBO = 0;
    GLuint g_texture = 0;
    GLuint g_overlayTex = 0;
    GLuint g_meshProg = 0;
    GLuint g_meshVBO = 0;
    GLuint g_meshIBO = 0;
    GLuint g_meshPalTex = 0;
    GLint g_meshLocPos = -1;
    GLint g_meshLocNormal = -1;
    GLint g_meshLocColorIdx = -1;
    GLint g_meshLocShadeShift = -1;
    GLint g_meshLocMVP = -1;
    GLint g_meshLocModel = -1;
    GLint g_meshLocPalette = -1;
    GLint g_meshLocFade = -1;
    GLuint g_texProg = 0;
    GLuint g_bgScreenProg = 0;
    int g_bgScreenLocTex = -1;
    GLuint g_sdfProg = 0;
GLint g_sdfLocSharp = -1;
    GLuint g_sdfVBO = 0;
    GLuint g_sdfIBO = 0;
    GLuint g_sdfTex = 0;
GLuint g_sdfTexOverlay = 0;
GLint g_sdfLocSdfOverlay = -1;
GLint g_sdfLocUseOverlay = -1;
    GLint g_sdfLocPos = -1;
    GLint g_sdfLocUV = -1;
    GLint g_sdfLocMVP = -1;
    GLint g_sdfLocSdf = -1;
    GLint g_sdfLocColor = -1;
    GLuint g_rgbaProg = 0;
    GLuint g_torusProg = 0;
    GLuint g_torusVBO = 0;
    GLuint g_torusIBO = 0;
    GLint g_torusLocPos = -1;
    GLint g_torusLocNrm = -1;
    GLint g_torusLocTube = -1;
    GLint g_torusLocPlasma = -1;
    GLint g_torusLocMVP = -1;
    GLint g_torusLocGrow = -1;
    GLint g_torusLocSpin = -1;
    GLint g_torusLocAlpha = -1;
    GLint g_torusLocFlashOn = -1;
    GLint g_torusLocFlash = -1;
    GLint g_torusLocFireOn = -1;
    GLint g_torusLocAspect = -1;
    // Frames since the shockwave started, for time-driven uniforms.
    static int g_explosionFrame = 0;
    GLint g_torusLocCenter = -1;
    GLint g_torusLocPlasmaDebug = -1;
    GLint g_torusLocPlasmaScale = -1;
    GLint g_torusLocPlasmaMotion = -1;
    GLint g_torusLocPlasmaIntensity = -1;
    GLint g_torusLocPlasmaOffset = -1;
    GLint g_torusLocPlasmaHeat = -1;
    GLint g_torusLocPlasmaBoil = -1;
    GLint g_torusLocPlasmaSwirl = -1;
    GLint g_torusLocPlasmaFlow = -1;
    GLint g_torusLocPlasmaClock = -1;
    GLint g_torusLocPlasmaRelief = -1;
    float m_explosionFlash = 0.0f;
    // Dedicated white-out quad. The torus program's own full-screen flash quad
    // never produced pixels on screen, so the blowout uses this trivial program
    // (solid white times uAlpha) drawn over the finished explosion.
    GLuint g_flashProg = 0;
    GLint g_flashLocAlpha = -1;
    GLint g_flashLocPos = -1;
    GLint g_flashLocColor = -1;
    // Full-screen exit fade drawn over the finished frame (0 = off).
    float g_exitFade = 0.0f;

    // One trivial program draws every full-screen colour wash: the blast
    // white-out and the black exit fade.
    static void DrawFullscreenQuad(float r, float g, float b, float a)
    {
        if (a <= 0.0f || g_flashProg == 0) return;
        glUseProgram(g_flashProg);
        glUniform3f(g_flashLocColor, r, g, b);
        glUniform1f(g_flashLocAlpha, a);
        // Full window, alpha-blended: the callers may have left a scissor, a
        // letterbox viewport or blending off.
        glViewport(0, 0, (GLint)g_width, (GLint)g_height);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        // Must unbind: with a VBO still bound, glVertexAttribPointer treats the
        // client-array pointer as an offset into it and reads garbage.
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        static const float POS[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
        glEnableVertexAttribArray(g_flashLocPos);
        glVertexAttribPointer(g_flashLocPos, 2, GL_FLOAT, GL_FALSE, 0, POS);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(g_flashLocPos);
        glUseProgram(0);
    }
    GLuint g_rgbaVBO = 0;
    GLuint g_rgbaIBO = 0;
    GLuint g_rgbaTex = 0;
    GLint g_rgbaLocPos = -1;
    GLint g_rgbaLocUV = -1;
    GLint g_rgbaLocMVP = -1;
    GLint g_rgbaLocTex = -1;
    GLint g_rgbaLocTexSize = -1;
    GLint g_rgbaLocBlur = -1;
    GLint g_rgbaLocWhite = -1;
    float g_rgbaWhite = 0.0f;
    GLuint g_texVBO = 0;
    GLuint g_texIBO = 0;
    GLuint g_texIdxTex = 0;
    GLint g_texLocPos = -1;
    GLint g_texLocUV = -1;
    GLint g_texLocMVP = -1;
    GLint g_texLocIdx = -1;
    GLint g_texLocPal = -1;
    GLint g_texLocWarpT = -1;
    unsigned char g_meshPalRGBA[256 * 4];
    GLint g_Loc_aPos = -1;
    GLint g_Loc_aUV = -1;
    GLuint g_linearSampling = 0;
    GLuint g_nearestSampling = 0;

    // xBRz bitmap upscaler (applied to the CPU framebuffer before upload).
// xBRz per-frame upscaling of the whole framebuffer was a CPU killer
// (upscaling 640x400/640x800 at 60fps saturated the core). Disabled by
// default; the GPU does the final upscale in the blit shader instead.
bool g_xbrzEnabled = true;
size_t g_xbrzFactor = 4;
    std::vector<unsigned int> g_xbrzBuffer;
    std::vector<unsigned int> g_xbrzLastSource;
}

static void die(const char * msg)
{
    std::fprintf(stderr, "FATAL: %s\n", msg);
    std::exit(1);
}

int g_captureRemaining = 0;
static int g_captureIndex = 0;

const char * g_capturePath = "/tmp/secondreality-cap";
static bool g_capturePathRead = false;
static int g_captureStride = 1;
static int g_captureFrom = 0;

static void CaptureFrame(const char * base)
{
    int w = (int)g_width;
    int h = (int)g_height;
    if (w <= 0 || h <= 0) return;

    if (!g_capturePathRead)
    {
        g_capturePathRead = true;
        if (const char * env = getenv("SR_CAPTURE_PATH")) g_capturePath = env;
        // SR_CAPTURE_STRIDE writes one frame in N. glReadPixels plus a 6MB PPM
        // per frame is the whole cost of a capture and it throttles the run, so
        // a long sequence is only reachable at a stride. The counter still runs
        // every frame: the index in the filename is the real frame number, so a
        // stride of 3 leaves gaps and nothing else.
        if (const char * env = getenv("SR_CAPTURE_STRIDE"))
        {
            g_captureStride = std::atoi(env);
            if (g_captureStride < 1) g_captureStride = 1;
        }
        // SR_CAPTURE_FROM begins writing at this frame. Writing PPMs is the
        // whole cost of a capture and it is what pushes the run out of sync, so
        // a run that writes nothing costs nothing. Point this at the frame the
        // sequence of interest starts near and leave --capture generous.
        if (const char * env = getenv("SR_CAPTURE_FROM"))
        {
            g_captureFrom = std::atoi(env);
            if (g_captureFrom < 0) g_captureFrom = 0;
        }
    }

    const int index = g_captureIndex++;
    if (index < g_captureFrom) return;
    if ((index - g_captureFrom) % g_captureStride != 0) return;

    std::vector<unsigned char> px((size_t)(w * h * 4));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    char path[256];
    std::snprintf(path, sizeof(path), "%s_%06d.ppm", base, index);

    FILE * f = std::fopen(path, "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y)
    {
        for (int x = 0; x < w; ++x)
        {
            const unsigned char * p = &px[((size_t)y * w + x) * 4];
            std::fputc(p[0], f);
            std::fputc(p[1], f);
            std::fputc(p[2], f);
        }
    }
    std::fclose(f);
}

// Optional frame pipe, off unless SR_FRAMES_PIPE is set. Every rendered frame is
// written as a PPM to that file, or to stdout for "-". Feeding ffmpeg through
// this keeps the demo's own frame timing instead of the compositor's sampling.
static int g_framesPipeFd = -2;

static void WriteFramesPipe()
{
    if (g_framesPipeFd == -2)
    {
        const char * path = std::getenv("SR_FRAMES_PIPE");
        if (!path || !*path)
            g_framesPipeFd = -1;
        else if (std::strcmp(path, "-") == 0)
            g_framesPipeFd = 1;
        else
        {
            g_framesPipeFd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (g_framesPipeFd < 0)
                std::fprintf(stderr, "[pipe] SR_FRAMES_PIPE: cannot open '%s'\n", path);
        }
    }
    if (g_framesPipeFd < 0) return;

    const int w = (int)g_width, h = (int)g_height;
    if (w <= 0 || h <= 0) return;

    static bool sizeAnnounced = false;
    if (!sizeAnnounced) { std::fprintf(stderr, "[pipe] %dx%d rgb24\n", w, h); sizeAnnounced = true; }

    static std::vector<unsigned char> px;
    static std::vector<unsigned char> out;
    px.resize((size_t)w * h * 4);
    out.resize((size_t)w * h * 3);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    unsigned char * dst = out.data();
    for (int y = h - 1; y >= 0; --y)
    {
        for (int x = 0; x < w; ++x)
        {
            const unsigned char * s = &px[((size_t)y * w + x) * 4];
            *dst++ = s[0]; *dst++ = s[1]; *dst++ = s[2];
        }
    }

    const size_t total = (size_t)w * h * 3;
    size_t off = 0;
    while (off < total)
    {
        const ssize_t k = ::write(g_framesPipeFd, out.data() + off, total - off);
        if (k <= 0) { g_framesPipeFd = -1; return; }
        off += (size_t)k;
    }
}

static GLuint compileShader(GLenum type, const char * src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::fprintf(stderr, "Shader compile error:\n%s\n", log.c_str());
        die("compileShader failed");
    }
    return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);

    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aUV");

    glLinkProgram(p);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::fprintf(stderr, "Program link error:\n%s\n", log.c_str());
        die("linkProgram failed");
    }

    return p;
}

GLuint makeTextureRGBA8(int w, int h)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    return tex;
}

void UpdateTextureRGBA8(GLuint tex, int w, int h, const void * pixels)
{
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

static void initGL()
{
    const char * vertexShader = "precision mediump float;\n"
                                "attribute vec2 aPos;\n"
                                "attribute vec2 aUV;\n"
                                "uniform   vec3 uResolution;\n"
                                "varying   vec2 vUV;\n"
                                "void main(){\n"
                                "  vec2 pos = (aPos / uResolution.xy) * 2.0 - 1.0;\n"
                                "  pos.y = -pos.y;\n"
                                "  gl_Position = vec4(pos, 0.0, 1.0);\n"
                                "  vUV = aUV;\n"
                                "}\n";

    const char * fragmentShader = "precision mediump float;\n"
                                  "varying vec2 vUV;\n"
                                  "uniform vec3 uResolution;\n"
                                  "uniform sampler2D uTex0;\n"
                                  "uniform sampler2D uTex1;\n"
                                  "void main(){\n"
                                  "  gl_FragColor = vec4(mix(texture2D(uTex0, vUV).bgr, texture2D(uTex1, vUV).bgr, uResolution.z),1);\n"
                                  "}\n";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
    g_prog = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    g_Loc_uResolution = glGetUniformLocation(g_prog, "uResolution");
    g_Loc_uTex0 = glGetUniformLocation(g_prog, "uTex0");
    g_Loc_uTex1 = glGetUniformLocation(g_prog, "uTex1");
    g_Loc_aPos = 0;
    g_Loc_aUV = 1;

    glGenBuffers(1, &g_VBO);

    g_texture = makeTextureRGBA8(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT);
    g_overlayTex = makeTextureRGBA8(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT);
    glBindTexture(GL_TEXTURE_2D, g_overlayTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenSamplers(1, &g_linearSampling);
    glGenSamplers(1, &g_nearestSampling);

    glSamplerParameteri(g_linearSampling, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(g_linearSampling, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(g_nearestSampling, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(g_nearestSampling, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glSamplerParameteri(g_linearSampling, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(g_linearSampling, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(g_nearestSampling, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(g_nearestSampling, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUseProgram(g_prog);
    glUniform1i(g_Loc_uTex0, 0);
    glUniform1i(g_Loc_uTex1, 1);
    glUseProgram(0);

    const char * vsMesh =
        "precision highp float;\n"
        "attribute vec3 aPos;\n"
        "attribute vec3 aNormal;\n"
        "attribute float aColorIdx;\n"
        "attribute float aShadeShift;\n"
        "uniform mat4 uMVP;\n"
        "uniform mat4 uModel;\n"
        "varying float vPalIdx;\n"
        "void main(){\n"
        "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "  vec3 n = normalize((uModel * vec4(aNormal, 0.0)).xyz);\n"
        "  float nl01 = dot(n, normalize(vec3(0.7396, 0.6472, 0.1850))) * 0.5 + 0.5;\n"
        "  float q = 0.0;\n"
        "  if (aShadeShift > 0.5) {\n"
        "    q = floor(nl01 * 255.0 / exp2(aShadeShift));\n"
        "    q = clamp(q, 1.0, 30.0);\n"
        "  }\n"
        "  vPalIdx = aColorIdx + q;\n"
        "}\n";

    const char * fsMesh =
        "precision mediump float;\n"
        "uniform sampler2D uPalette;\n"
        "uniform float uFade;\n"
        "varying float vPalIdx;\n"
        "void main(){\n"
        "  vec3 c = texture2D(uPalette, vec2((vPalIdx + 0.5) / 256.0, 0.5)).bgr;\n"
        "  gl_FragColor = vec4(c, uFade);\n"
        "}\n";

    GLuint vsMeshId = compileShader(GL_VERTEX_SHADER, vsMesh);
    GLuint fsMeshId = compileShader(GL_FRAGMENT_SHADER, fsMesh);
    g_meshProg = linkProgram(vsMeshId, fsMeshId);
    glDeleteShader(vsMeshId);
    glDeleteShader(fsMeshId);

    g_meshLocPos = glGetAttribLocation(g_meshProg, "aPos");
    g_meshLocNormal = glGetAttribLocation(g_meshProg, "aNormal");
    g_meshLocColorIdx = glGetAttribLocation(g_meshProg, "aColorIdx");
    g_meshLocShadeShift = glGetAttribLocation(g_meshProg, "aShadeShift");
    g_meshLocMVP = glGetUniformLocation(g_meshProg, "uMVP");
    g_meshLocModel = glGetUniformLocation(g_meshProg, "uModel");
    g_meshLocPalette = glGetUniformLocation(g_meshProg, "uPalette");
    g_meshLocFade = glGetUniformLocation(g_meshProg, "uFade");

    glGenBuffers(1, &g_meshVBO);
    glGenBuffers(1, &g_meshIBO);

    glGenTextures(1, &g_meshPalTex);
    glBindTexture(GL_TEXTURE_2D, g_meshPalTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    const char * vsTex =
        "precision highp float;\n"
        "attribute vec3 aPos;\n"
        "attribute vec2 aUV;\n"
        "uniform mat4 uMVP;\n"
        "varying vec2 vUV;\n"
        "void main(){\n"
        "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "  vUV = aUV;\n"
        "}\n";

    const char * fsTex =
        "precision highp float;\n"
        "uniform sampler2D uTexIdx;\n"
        "uniform sampler2D uPalette;\n"
        "uniform float uWarpT;   // dd = frames & 63\n"
        "varying vec2 vUV;\n"
        "void main(){\n"
        "  float uPix = vUV.x * 256.0;\n"
        "  float vAtlas = vUV.y * 192.0;\n"
        "  float band = floor(vAtlas / 64.0) * 64.0;\n"
        "  float vPix = vAtlas - band;\n"
        "  float idx = mod(uWarpT, 64.0) + vPix;\n"
        "  float dv = sin(idx * 3.14159265358979 / 32.0) * 127.0 / 3.0;\n"
        "  float du = mod(uPix + floor(dv), 256.0);\n"
        "  vec2 uv = vec2(du / 256.0, (band + vPix) / 192.0);\n"
        "  float texIdx = texture2D(uTexIdx, uv).r * 255.0;\n"
        "  vec3 col = texture2D(uPalette, vec2((texIdx + 0.5) / 256.0, 0.5)).rgb;\n"
        "  gl_FragColor = vec4(col, 1.0);\n"
        "}\n";

    GLuint vsTexId = compileShader(GL_VERTEX_SHADER, vsTex);
    GLuint fsTexId = compileShader(GL_FRAGMENT_SHADER, fsTex);
    // Render-order contract: CPU-screen composite must stay under GPU meshes
    g_texProg = linkProgram(vsTexId, fsTexId);
    glDeleteShader(vsTexId);
    glDeleteShader(fsTexId);

    // The tex-mesh pass (PLZ's cube) needs its own locations, buffers and atlas
    // texture. Without them the attrib locations stay -1 and no element buffer is
    // bound, so glDrawElements reads indices from address 0 and the driver
    // segfaults the moment the cube draws (e.g. `secondreality A`).
    g_texLocPos = glGetAttribLocation(g_texProg, "aPos");
    g_texLocUV = glGetAttribLocation(g_texProg, "aUV");
    g_texLocMVP = glGetUniformLocation(g_texProg, "uMVP");
    g_texLocIdx = glGetUniformLocation(g_texProg, "uTexIdx");
    g_texLocPal = glGetUniformLocation(g_texProg, "uPalette");
    g_texLocWarpT = glGetUniformLocation(g_texProg, "uWarpT");

    glGenBuffers(1, &g_texVBO);
    glGenBuffers(1, &g_texIBO);

    glGenTextures(1, &g_texIdxTex);
    glBindTexture(GL_TEXTURE_2D, g_texIdxTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 192, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    // SDF text pipeline: each glyph is a textured quad, the fragment shader
    // applies smoothstep on the signed distance for perfect anti-aliasing.
    const char * vsSdf =
        "precision highp float;\n"
        "attribute vec3 aPos;\n"
        "attribute vec2 aUV;\n"
        "uniform mat4 uMVP;\n"
        "varying vec2 vUV;\n"
        "void main(){\n"
        "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "  vUV = aUV;\n"
        "}\n";
    const char * fsSdf =
        "precision mediump float;\n"
        "uniform sampler2D uSdf;\n"
        "uniform sampler2D uSdfOverlay;\n"
        "uniform float uUseOverlay;\n"  // 1 = sample the overlay's own atlas
        "uniform vec3 uColor;\n"
        "uniform float uSharp;\n"   // 0 = smooth edges, 1 = hard aliased edges
        "varying vec2 vUV;\n"
        "void main(){\n"
        // The overlay owns a separate glyph table, so its UVs only make sense
        // against its own atlas. Picking one atlas globally made every part's
        // text sample the overlay's texture with its own UVs, which is what
        // turned ':' into the wrong glyph.
        "  float d = mix(texture2D(uSdf, vUV).r, texture2D(uSdfOverlay, vUV).r, uUseOverlay);\n"
        "  // The overlay reads as small grey blur at this size, so it asks for hard\n"
        "  // edges: a one step ramp instead of a smoothstep band. Parts keep the\n"
        "  // antialiased default because their glyphs are much larger.\n"
        "  float aa = mix(0.08, 0.005, uSharp);\n"
        "  float a = smoothstep(0.5 - aa, 0.5 + aa, d);\n"
        "  gl_FragColor = vec4(uColor, a);\n"
        "}\n";
    g_sdfProg = linkProgram(compileShader(GL_VERTEX_SHADER, vsSdf), compileShader(GL_FRAGMENT_SHADER, fsSdf));
    g_sdfLocPos = glGetAttribLocation(g_sdfProg, "aPos");
    g_sdfLocUV = glGetAttribLocation(g_sdfProg, "aUV");
    g_sdfLocMVP = glGetUniformLocation(g_sdfProg, "uMVP");
    g_sdfLocSdf = glGetUniformLocation(g_sdfProg, "uSdf");
    g_sdfLocColor = glGetUniformLocation(g_sdfProg, "uColor");
    g_sdfLocSharp = glGetUniformLocation(g_sdfProg, "uSharp");
    g_sdfLocSdfOverlay = glGetUniformLocation(g_sdfProg, "uSdfOverlay");
    g_sdfLocUseOverlay = glGetUniformLocation(g_sdfProg, "uUseOverlay");

    glGenBuffers(1, &g_sdfVBO);
    glGenBuffers(1, &g_sdfIBO);

    // RGBA image quad pipeline (e.g. vector logo rasterised to a texture).
    const char * vsRgba =
        "precision highp float;\n"
        "attribute vec3 aPos;\n"
        "attribute vec2 aUV;\n"
        "uniform mat4 uMVP;\n"
        "varying vec2 vUV;\n"
        "void main(){\n"
        "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "  vUV = aUV;\n"
        "}\n";
    const char * fsRgba =
        "precision mediump float;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec2 uTexSize;\n"
        "uniform float uBlur;\n"
        "uniform float uWhite;\n"
        "varying vec2 vUV;\n"
        "void main(){\n"
        "  vec4 c = texture2D(uTex, vUV);\n"
        "  vec3 rgb = c.rgb;\n"
        "  if (uBlur > 0.001) {\n"
        "    vec2 px = 1.0 / uTexSize;\n"
        "    float w[5];\n"
        "    w[0] = 0.06136; w[1] = 0.24477; w[2] = 0.38774; w[3] = 0.24477; w[4] = 0.06136;\n"
        "    vec3 acc = vec3(0.0);\n"
        "    for (int j = -2; j <= 2; ++j) {\n"
        "      for (int i = -2; i <= 2; ++i) {\n"
        "        float weight = w[i + 2] * w[j + 2];\n"
        "        acc += texture2D(uTex, vUV + vec2(px.x * float(i), px.y * float(j))).rgb * weight;\n"
        "      }\n"
        "    }\n"
        "    rgb = mix(rgb, acc, uBlur);\n"
        "  }\n"
        "  gl_FragColor = vec4(mix(rgb, vec3(1.0), uWhite), c.a);\n"
        "}\n";
    g_rgbaProg = linkProgram(compileShader(GL_VERTEX_SHADER, vsRgba), compileShader(GL_FRAGMENT_SHADER, fsRgba));
    g_rgbaLocPos = glGetAttribLocation(g_rgbaProg, "aPos");
    g_rgbaLocUV = glGetAttribLocation(g_rgbaProg, "aUV");
    g_rgbaLocMVP = glGetUniformLocation(g_rgbaProg, "uMVP");
    g_rgbaLocTex = glGetUniformLocation(g_rgbaProg, "uTex");
    g_rgbaLocTexSize = glGetUniformLocation(g_rgbaProg, "uTexSize");
    g_rgbaLocBlur = glGetUniformLocation(g_rgbaProg, "uBlur");
    g_rgbaLocWhite = glGetUniformLocation(g_rgbaProg, "uWhite");
    glUseProgram(g_rgbaProg);
    glUniform1f(g_rgbaLocWhite, 0.0f);

    // Shockwave torus. The torus is generated once by the part and never rebuilt:
    // uGrow opens it up, uSpin turns the plasma inside it. Only the outer skin
    // survives (dot(n, tubeDir) > 0), which is what makes it read as a ring rather
    // than a doughnut with its inner wall showing.
    const char * vsTorus =
        "precision highp float;\n"
        "attribute vec3 aPos;\n"
        "attribute vec3 aNrm;\n"
        "attribute vec3 aTube;\n"
        "attribute vec2 aPlasma;\n"   // theta around the ring, phi around the tube
        "uniform mat4 uMVP;\n"
        "uniform float uGrow;\n"
        "uniform float uSpin;\n"
        "uniform float uFlashOn;\n"
        "uniform float uFireOn;\n"
        "uniform float uAspect;\n"
        "uniform vec2 uCenter;\n"
        "uniform float uPlasmaClock;\n"
        "uniform float uPlasmaSwirl;\n"
        "uniform float uPlasmaFlow;\n"
        "uniform float uPlasmaRelief;\n"
        // Height field, identical to the one the fragment stage samples for
        // colour, so the displaced geometry and the shading cannot drift apart.
        "float ringHeight(vec3 wp)\n"
        "{\n"
        "  float tang = atan(wp.z, wp.x) + length(wp.xz) * 3.0;\n"
        // The axis wanders: high frequency along theta, which is screen WIDTH and
        // so cannot produce bars, and a small amplitude so it stays a wobble. The
        // earlier coherent warp at tang * 3 read as a big C instead.
        "  float warp = 0.0;\n"
        "  float across = abs(wp.y + warp);\n"
        "  float h = sin(tang * 13.0 - uPlasmaClock * uPlasmaSwirl + across * 5.0);\n"
        "  h += 0.42 * sin(tang * 21.0 + uPlasmaClock * uPlasmaFlow + across * 9.0);\n"
        "  h += 0.30 * sin(across * 15.0 - uPlasmaClock * uPlasmaSwirl * 0.6);\n"
        "  return h;\n"
        "}\n"
        "\n"
        // Slope of that field along the two surface tangents. Differencing in the
        // tangent frame rather than in world xyz keeps the result meaningful on
        // a surface that is seen exactly edge-on, where a world-space gradient
        // would be dominated by the degenerate depth axis.
        "vec2 ringSlope(vec3 wp, vec3 tang, float eps)\n"
        "{\n"
        "  float h0 = ringHeight(wp);\n"
        "  float ht = ringHeight(wp + tang * eps);\n"
        "  float hb = ringHeight(wp + vec3(0.0, eps, 0.0));\n"
        "  return vec2(ht - h0, hb - h0) / eps;\n"
        "}\n"
        "\n"
        "varying vec3 vNrm;\n"
        "varying vec3 vTube;\n"
        "varying float vTheta;\n"
        "varying float vPhi;\n"
        "varying vec3 vPos;\n"
        "varying vec3 vPert;\n"
        "varying vec2 vNdc;\n"
        "void main(){\n"
        "  vPert = aNrm;\n"
        "  vNdc = vec2(0.0);\n"
        // Flash mode reuses the program for a full-screen quad in NDC, so the
        // blowout covers the hires picture too instead of only the torus.
        "  if (uFlashOn > 0.5) { gl_Position = vec4(aPos, 1.0); }\n"
        // Fireball: one quad, aPos carries the local corner and aTube the centre.
        "  else if (uFireOn > 0.5) { gl_Position = vec4(uCenter + vec2(aPos.x / uAspect, aPos.y) * uGrow, 0.0, 1.0); }\n"
        "  else {\n"
        // Depth comes from the unscaled axis. Scaling z with uGrow too pushed
        // the ring outside [-1,1], which cut it into strips and opened a hole at
        // the near side that grew until it swallowed the band.
        "    vec3 p = aPos * uGrow; p.z = aPos.z * 0.7;\n"
        // Requires the dense grid built in OUTTAA.cpp: a coarse ring leaves no
        // vertices to displace.
        "    if (uPlasmaRelief > 0.001) {\n"
        "      vec3 T = normalize(vec3(-aPos.z, 0.0, aPos.x));\n"
        "      vec3 N = normalize(aNrm);\n"
        "      vec3 B = normalize(cross(N, T));\n"
        "      vec2 g = ringSlope(aPos, T, 0.010);\n"
        "      p += N * ringHeight(aPos) * uPlasmaRelief;\n"
        "      vPert = normalize(N - (T * g.x + B * g.y) * uPlasmaRelief * 1.6);\n"
        "    }\n"
        "    gl_Position = uMVP * vec4(p, 1.0);\n"
        // Same origin as the fireball. Without this the ring sat on the middle
        // of the screen while the blast opened off-centre above it, so the two
        // read as unrelated objects instead of one explosion.
        "    gl_Position.xy += uCenter;\n"
        "  }\n"
        "  vNrm = aNrm;\n"
        "  vTube = aTube;\n"
        "  vTheta = aPlasma.x;\n"
        "  vPhi = aPlasma.y;\n"
        // Unscaled position, which is what the ring plasma samples. aPos, not
        // p: p is aPos * uGrow, and scaling the pattern with the ring made the
        // whole texture slide every frame, which reads as a flicker.
        "  vPos = aPos;\n"
        "  vNdc = gl_Position.xy;\n"
        "}\n";
    const char * fsTorus =
        "precision highp float;\n"
        // Position of a colour on the fire wheel, in 0..1. Needed because the
        // thermal gradient has to slide the existing colour along the wheel
        // rather than recolour it, which fireWheel() alone cannot express.
        "vec3 fireWheel(float t)\n"
        "{\n"
        "  const vec3 s0 = vec3(0.0, 0.0, 0.0);\n"
        "  const vec3 s1 = vec3(0.549, 0.0, 0.0);\n"
        "  const vec3 s2 = vec3(1.0, 0.353, 0.0);\n"
        "  const vec3 s3 = vec3(1.0, 0.863, 0.471);\n"
        "  const vec3 s4 = vec3(1.0, 0.353, 0.0);\n"
        "  const vec3 s5 = vec3(0.549, 0.0, 0.0);\n"
        "  float x = clamp(t, 0.0, 0.99999) * 6.0;\n"
        "  vec3 c = mix(s0, s1, clamp(x, 0.0, 1.0));\n"
        "  c = mix(c, s2, clamp(x - 1.0, 0.0, 1.0));\n"
        "  c = mix(c, s3, clamp(x - 2.0, 0.0, 1.0));\n"
        "  c = mix(c, s4, clamp(x - 3.0, 0.0, 1.0));\n"
        "  c = mix(c, s5, clamp(x - 4.0, 0.0, 1.0));\n"
        "  return mix(c, s0, clamp(x - 5.0, 0.0, 1.0));\n"
        "}\n"
        "\n"
        "float plasmaClassic(vec2 uv, float time, float motion, float scale, float intensity, float aspect)\n"
        "{\n"
        "  float t = time * motion;\n"
        "  vec2 p = vec2(uv.x * aspect, uv.y);\n"
        "  float v = sin(p.x * scale + t)\n"
        "          + sin(p.y * scale * 0.8 + time * motion * 1.3)\n"
        "          + sin((p.x + p.y) * scale * 0.6 + time * motion * 0.7)\n"
        "          + sin(length(p - vec2(aspect * 0.5, 0.5)) * scale * 1.4 - time * motion * 1.1);\n"
        "  v *= 0.25 * intensity;\n"
        "  return clamp(v, -1.0, 1.0) * 0.5 + 0.5;\n"
        "}\n"
        "\n"
        "float hash13(vec3 p)\n"
        "{\n"
        "  p = fract(p * 0.1031);\n"
        "  p += dot(p, p.zyx + 31.32);\n"
        "  return fract((p.x + p.y) * p.z);\n"
        "}\n"
        "\n"
        "float vnoise3(vec3 p)\n"
        "{\n"
        "  vec3 i = floor(p);\n"
        "  vec3 f = fract(p);\n"
        "  f = f * f * (3.0 - 2.0 * f);\n"
        "  float nx00 = mix(mix(hash13(i), hash13(i + vec3(1,0,0)), f.x),\n"
        "                     mix(hash13(i + vec3(0,1,0)), hash13(i + vec3(1,1,0)), f.x), f.y);\n"
        "  float nx01 = mix(mix(hash13(i + vec3(0,0,1)), hash13(i + vec3(1,0,1)), f.x),\n"
        "                     mix(hash13(i + vec3(0,1,1)), hash13(i + vec3(1,1,1)), f.x), f.y);\n"
        "  return mix(nx00, nx01, f.z);\n"
        "}\n"
        "\n"
        "float fbm3(vec3 p)\n"
        "{\n"
        "  float v = 0.0;\n"
        "  float amp = 0.55;\n"
        "  mat2 turn = mat2(0.80, -0.60, 0.60, 0.80);\n"
        "  for (int o = 0; o < 4; o++) {\n"
        "    v += vnoise3(p) * amp;\n"
        "    p.xy = turn * p.xy * 2.07;\n"
        "    p.z *= 1.9;\n"
        "    amp *= 0.48;\n"
        "  }\n"
        "  return v;\n"
        "}\n"
        "\n"

        "vec3 plasmaTrails(vec2 uv, float time, float motion, float scale, float intensity, float aspect, float offset, float heat)\n"
        "{\n"
        "  vec2 drift = vec2(cos(time * motion * 0.31), sin(time * motion * 0.37)) * (0.006 * motion);\n"
        "  vec3 c0 = fireWheel(fract(plasmaClassic(uv, time, motion, scale, intensity, aspect) + offset + heat));\n"
        "  vec3 c1 = fireWheel(fract(plasmaClassic(uv - drift, time - 0.10, motion, scale, intensity, aspect) + offset + heat));\n"
        "  vec3 c2 = fireWheel(fract(plasmaClassic(uv - drift * 2.1, time - 0.22, motion, scale, intensity, aspect) + offset + heat));\n"
        "  vec3 c3 = fireWheel(fract(plasmaClassic(uv - drift * 3.4, time - 0.38, motion, scale, intensity, aspect) + offset + heat));\n"
        "  vec3 col = max(c0, c1 * 0.76);\n"
        "  col = max(col, c2 * 0.53);\n"
        "  col = max(col, c3 * 0.34);\n"
        "  col = max(col, vec3(c1.r, c2.g, c3.b) * 0.68);\n"
        "  col += (c1 + c2 + c3) * 0.035;\n"
        "  return col;\n"
        "}\n"
        "\n"
        "uniform float uSpin;\n"
        "uniform float uAlpha;\n"
        "uniform float uFlashOn;\n"
        "uniform float uFlash;\n"
        "uniform float uFireOn;\n"
        "uniform float uPlasmaDebug;\n"
        "uniform float uPlasmaTiles;\n"
        "uniform float uPlasmaScale;\n"
        "uniform float uPlasmaMotion;\n"
        "uniform float uPlasmaIntensity;\n"
        "uniform float uPlasmaOffset;\n"
        "uniform float uPlasmaHeat;\n"
        "uniform float uPlasmaBoil;\n"
        "uniform float uPlasmaSwirl;\n"
        "uniform float uPlasmaFlow;\n"
        "uniform float uPlasmaClock;\n"
        "uniform float uAspect;\n"
        "varying vec3 vNrm;\n"
        "varying vec3 vTube;\n"
        "varying float vTheta;\n"
        "varying float vPhi;\n"
        "varying vec3 vPos;\n"
        "varying vec3 vPert;\n"
        "varying vec2 vNdc;\n"
        "uniform vec2 uCenter;\n"
        "uniform float uPlasmaRelief;\n"
        "void main(){\n"
        "  if (uFlashOn > 0.5) { gl_FragColor = vec4(1.0, 1.0, 1.0, uFlash); return; }\n"
        // Fireball: intense heat at the core boiling outward. The Plasma Lab
        // "classic" field supplies the turbulence and the fire wheel the colour,
        // but the wheel is cyclic, so on its own it gives hot and cold patches
        // with no thermal gradient. uPlasmaHeat pushes the core up the wheel
        // toward the white-hot stop while the rim stays on the deep reds, which
        // is what reads as a heat source rather than as coloured smoke.
        // uPlasmaBoil speeds the field up as the blast develops: accelerating
        // convection is what makes it look like boiling gas.
        "  if (uFireOn > 0.5) {\n"
        "    vec2 q = vec2(vTheta, vPhi);\n"
        "    float d = length(q);\n"
        // Sprite-local -> 0..1 field coords, x aspect-corrected as upstream.
        "    vec2 uv = q * 0.5 + 0.5;\n"
        // Convection: the field scrolls outward from the core as the blast ages.
        // uSpin only spans 0..8.8 across the 26 frames the fireball lives, so
        // scaling it cannot produce visible motion. A dedicated clock in seconds
        // is what the Plasma Lab source uses (animationTime), and at 60fps this
        // is fr/60, so the field advances 0.11 rad per frame at motion 0.113.
        "    float boil = uPlasmaBoil;\n"
        // Thermal gradient: hot in the core, cooling outward. Applied to the
        // wheel INDEX, before the lookup. Shifting the colour instead would need
        // its position located on a cyclic wheel, where t and 1-t are nearly the
        // same colour, so a nearest-neighbour search snaps between them and
        // collapses the plasma into a smooth gradient.
        "    float heat = uPlasmaHeat * (1.0 - smoothstep(0.05, 0.95, d));\n"
        // Brownian from the centre, compressing against the wall. sqrt() on the
        // radius spreads the cells further out, so they crowd together as they
        // approach the surface and read as gas being squeezed against it.
        // 3D noise on the sprite plane with the third axis advected outward, so
        // the cells appear to travel from the core to the wall. No atan() here:
        // polar coordinates put a singularity at the centre, which is what turned
        // the field into a visible spiral.
        "    float br = min(d, 1.0);\n"
"    vec3 sp = vec3(q * 3.4, br * 2.2 - uPlasmaClock * (0.22 + boil * 0.30));\n"
        "    float fb = fbm3(sp);\n"
        "    float fidx = clamp((fb - 0.42) * 2.8, -1.0, 1.0) * 0.5 + 0.5;\n"
        "    vec3 col = fireWheel(fract(fidx + uPlasmaOffset + heat));\n"
        // Rim falloff keeps the silhouette.
        "    float fall = smoothstep(1.0, 0.10, d);\n"
        "    gl_FragColor = vec4(col * fall, fall * uAlpha);\n"
        "    return;\n"
        "  }\n"
        // Inner wall faces the ring centre, so it is dropped instead of drawn.
        "  float facing = dot(normalize(vNrm), normalize(vTube));\n"
        "  if (facing < 0.0) discard;\n"
        // Ring: grey shockwave. Two things are wanted here and both are read
        // from world position, never from phi or theta: on an edge-on torus phi
        // IS screen height, so a phi term is coherent across the whole width
        // and paints a bar, while theta is screen width and scrolls sideways.
        // Measured cost of getting this wrong, worst coherent dip:
        //   vPhi * 3.0                3 hard dark minima
        //   vPhi shear over theta * 7 streaks refondues en barres
        //   radial K=6 puis K=11      train d anneaux dans la section
        //   radial K=2.2              renflement de 6%, dont 2 bords = 2 lignes
        //   frequencies incommensurables decorelees mais crete de 5,8%
        //   world position            monotone a chaque frame
        "  float rim = pow(facing, 0.7);\n"
        // Grazing term. Seen edge-on the outer equator projects to a horizontal
        // segment, so without this both ends of the band terminated in a
        // measured 1px cut from background to white. N.z is zero exactly at
        // the tips. Cubed, to pack the falloff into the last few percent of the
        // half-width so the band keeps its brightness.
        "  float ndv = abs(normalize(vNrm).z);\n"
        // Feathers only the true grazing tips. ndv*ndv*ndv looked equivalent but
        // on an edge-on ring ndv tracks the same quantity as rim, so the two
        // compounded to cp^3.7 and thinned the whole band to a 0.08 alpha at
        // cp=0.5. Flat above 0.28 keeps the body opaque.
        "  float tip = smoothstep(0.0, 0.28, ndv);\n"
        "  float edge = rim * tip;\n"
        // Gas rotation around the tube: the pattern travels along the ring, so
        // the band reads as material being spun rather than sliding sideways.
        // vPos is unscaled on purpose, see the vertex shader.
        "  vec3 wp = vPos;\n"
        // Unwrapped angle around the ring. atan alone is degenerate on the axis
        // and, fed straight into the sines, aliases into regular vertical
        // striping where the ring is seen end-on; the position is added back so
        // the coordinate stays continuous all the way round.
        "  float tang = atan(wp.z, wp.x) + length(wp.xz) * 3.0;\n"
        "  float height = wp.y;\n"
        // Air deformation: the shock front bends as it passes, so the pattern
        // is warped by a low-frequency term before it is sampled. Modest
        // amplitude, since anything stronger reads as banding again.
        // High frequency along theta, which is screen WIDTH and so cannot produce
        // bars. The coherent warp at tang * 3 was low frequency and read as a C.
        "  float wq = sin(tang * 7.0 + uPlasmaClock * 3.1);\n"
        "  wq += 0.60 * sin(tang * 13.0 - uPlasmaClock * 4.7);\n"
        "  wq += 0.40 * sin(tang * 23.0 + uPlasmaClock * 6.3);\n"
        "  float warp = wq * 0.05;\n"
        // Fold to |height| so the pattern is MIRRORED about the horizontal plane
        // of the ring. Without the fold sin() is antisymmetric in height, so the
        // two halves of the band showed unrelated structure and the whole thing
        // read as a full-width stripe instead of a section through a tube.
        // The fold is also what makes the radial flow symmetric: a wave crossing
        // zero at the mid-plane is the same wave arriving from both walls.
        // Fold the height alone. Folding abs(height + warp) put the mirror line
        // at height = -warp(tang), so the axis drifted along the tube and the
        // band read as a C.
        // Smooth fold. abs() has a corner at height = 0 and the eye reads that
        // corner as a hard crease running along the band; the hyperbolic form
        // keeps the mirror exact while removing the crease.
        "  float across = sqrt(height * height + 0.0004) + warp;\n"
        // Incommensurate rates on purpose: that is what makes the sum read as
        // irregular instead of as a few travelling waves, and rounding them off
        // makes it visibly loop.
        // Rotation: the sample point walks around a circle in the tangent and
        // height plane, so the field genuinely turns about the ring axis. A
        // circle rather than an angle, so there is no seam at tang = 0.
        // Incommensurate rates on purpose: that is what makes the sum read as
        // irregular instead of as a few travelling waves, and rounding them off
        // makes it visibly loop.
        "  float bt = uPlasmaClock;\n"
        "  float b = sin(across * 11.0 - bt * 7.0 + tang * 3.0);\n"
        "  b += 0.55 * sin(across * 13.0 - bt * 11.3 + tang * 7.0);\n"
        "  b += 0.34 * sin(across * 19.0 - bt * 17.9 + tang * 13.0);\n"
        "  b += 0.18 * sin(across * 27.0 - bt * 27.1 + tang * 19.0);\n"
        "  b += 0.18 * sin(tang * 31.0 - bt * 23.7 + across * 5.0);\n"
        // Triangle wave: the radial motion reflects off the tube walls instead
        // of passing through them.
        "  float refl = abs(fract(across * 0.5 + 0.5) * 2.0 - 1.0);\n"
        "  b += 0.20 * sin(refl * 7.0 - bt * 13.0 + tang * 5.0);\n"
        "  float band = 0.5 + 0.5 * clamp(b * 0.62, -1.0, 1.0);\n"
        // Grey shock front: dark troughs, bright crests. Monochrome on purpose,
        // the fireball beside it carries the colour.
        // Relief shading. The light sits at the blast centre in NDC, so crests
        // facing it read bright and the ones tilted away fall into shadow. The
        // gain is high because the ring is seen edge-on, where a true normal
        // gives almost no N.L variation at all.
        // Dominated by the view axis: the ring is seen edge-on, so its normals
        // are nearly perpendicular to the screen and a light lying in the screen
        // plane gives N.L ~ 0 across the whole band, which crushed the peak from
        // 236 to 44. Wrap lighting keeps the far side from going black.
        "  vec3 L = normalize(vec3(uCenter - vNdc, 3.0));\n"
        "  float ndl = dot(normalize(vPert), L);\n"
        "  float wrapped = clamp((ndl + 0.6) / 1.6, 0.0, 1.0);\n"
        "  float lit = mix(1.0, 0.55 + 0.75 * wrapped, clamp(uPlasmaRelief * 3.0, 0.0, 1.0));\n"
        "  float g = mix(0.22, 1.0, band) * rim * lit;\n"
        "  gl_FragColor = vec4(vec3(g), uAlpha * edge);\n"
        "}\n";
    GLuint vsTorusId = compileShader(GL_VERTEX_SHADER, vsTorus);
    GLuint fsTorusId = compileShader(GL_FRAGMENT_SHADER, fsTorus);
    g_torusProg = linkProgram(vsTorusId, fsTorusId);
    glDeleteShader(vsTorusId);
    glDeleteShader(fsTorusId);

    g_torusLocPos = glGetAttribLocation(g_torusProg, "aPos");
    g_torusLocNrm = glGetAttribLocation(g_torusProg, "aNrm");
    g_torusLocTube = glGetAttribLocation(g_torusProg, "aTube");
    g_torusLocPlasma = glGetAttribLocation(g_torusProg, "aPlasma");
    g_torusLocMVP = glGetUniformLocation(g_torusProg, "uMVP");
    g_torusLocGrow = glGetUniformLocation(g_torusProg, "uGrow");
    g_torusLocSpin = glGetUniformLocation(g_torusProg, "uSpin");
    g_torusLocAlpha = glGetUniformLocation(g_torusProg, "uAlpha");
    g_torusLocFlashOn = glGetUniformLocation(g_torusProg, "uFlashOn");
    g_torusLocFlash = glGetUniformLocation(g_torusProg, "uFlash");
    g_torusLocFireOn = glGetUniformLocation(g_torusProg, "uFireOn");
    g_torusLocAspect = glGetUniformLocation(g_torusProg, "uAspect");
    g_torusLocCenter = glGetUniformLocation(g_torusProg, "uCenter");
    g_torusLocPlasmaDebug = glGetUniformLocation(g_torusProg, "uPlasmaDebug");
    g_torusLocPlasmaScale = glGetUniformLocation(g_torusProg, "uPlasmaScale");
    g_torusLocPlasmaMotion = glGetUniformLocation(g_torusProg, "uPlasmaMotion");
    g_torusLocPlasmaIntensity = glGetUniformLocation(g_torusProg, "uPlasmaIntensity");
    g_torusLocPlasmaOffset = glGetUniformLocation(g_torusProg, "uPlasmaOffset");
    g_torusLocPlasmaHeat = glGetUniformLocation(g_torusProg, "uPlasmaHeat");
    g_torusLocPlasmaBoil = glGetUniformLocation(g_torusProg, "uPlasmaBoil");
    g_torusLocPlasmaSwirl = glGetUniformLocation(g_torusProg, "uPlasmaSwirl");
    g_torusLocPlasmaFlow = glGetUniformLocation(g_torusProg, "uPlasmaFlow");
    g_torusLocPlasmaClock = glGetUniformLocation(g_torusProg, "uPlasmaClock");
    g_torusLocPlasmaRelief = glGetUniformLocation(g_torusProg, "uPlasmaRelief");

    g_flashProg = linkProgram(
        compileShader(GL_VERTEX_SHADER,
                      "attribute vec2 aPos;\n"
                      "void main(){ gl_Position = vec4(aPos, 0.0, 1.0); }\n"),
        compileShader(GL_FRAGMENT_SHADER,
                      "precision highp float;\n"
                      "uniform vec3 uColor;\n"
                      "uniform float uAlpha;\n"
                      "void main(){ gl_FragColor = vec4(uColor, uAlpha); }\n"));
    g_flashLocAlpha = glGetUniformLocation(g_flashProg, "uAlpha");
    g_flashLocPos = glGetAttribLocation(g_flashProg, "aPos");
    g_flashLocColor = glGetUniformLocation(g_flashProg, "uColor");

    glGenBuffers(1, &g_torusVBO);
    glGenBuffers(1, &g_torusIBO);

    glGenBuffers(1, &g_rgbaVBO);
    glGenBuffers(1, &g_rgbaIBO);

    glGenTextures(1, &g_rgbaTex);
    glBindTexture(GL_TEXTURE_2D, g_rgbaTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glClearColor(ClearColor[0], ClearColor[1], ClearColor[2], ClearColor[3]);
}

static void Render()
{
    glClearColor(g_bgClear[0], g_bgClear[1], g_bgClear[2], 1.0f);

    glViewport(0, 0, g_width, g_height);

    float w, h;

    if (g_respectRatio)
    {
        constexpr float DOS_4_3 = 4.0f / 3.0f;
        const float windowAspect = g_width / g_height;

        if (windowAspect > DOS_4_3)
        {
            h = g_height;
            w = h * DOS_4_3;
        }
        else
        {
            w = g_width;
            h = w / DOS_4_3;
        }
    }
    else
    {
        w = g_width;
        h = g_height;
    }

    w *= g_compositeScale;
    h *= g_compositeScale;

    const float x = (g_width - w) * 0.5f;
    const float y = (g_height - h) * 0.5f;

    const GLfloat verts[] = {
        x,     y,     0.0f,          0.0f,           // top-left
        x,     y + h, 0.0f,          g_sourceHeight, // bottom-left
        x + w, y,     g_sourceWidth, 0.0f,           // top-right
        x + w, y + h, g_sourceWidth, g_sourceHeight  // bottom-right
    };

    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_prog);

    glUniform3f(g_Loc_uResolution, g_width, g_height, g_jsss);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_texture);

    glBindSampler(0, g_nearestSampling);
    glBindSampler(1, g_nearestSampling);

    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glEnableVertexAttribArray(g_Loc_aPos);
    glEnableVertexAttribArray(g_Loc_aUV);
    glVertexAttribPointer(g_Loc_aPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(0));
    glVertexAttribPointer(g_Loc_aUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(g_Loc_aPos);
    glDisableVertexAttribArray(g_Loc_aUV);
    glUseProgram(0);
}

#ifdef __EMSCRIPTEN__
static void SyncCanvasToCSSAndViewport()
{
    double cssW = 0.0, cssH = 0.0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);

    const double dpr = emscripten_get_device_pixel_ratio();
    const int width = (int)std::lround(cssW * dpr);
    const int height = (int)std::lround(cssH * dpr);

    emscripten_set_canvas_element_size("#canvas", width, height);

    if (g_WebGL_context)
    {
        emscripten_webgl_make_context_current(g_WebGL_context);
    }

    glViewport(0, 0, width, height);

    g_width = static_cast<float>(width);
    g_height = static_cast<float>(height);
}

static EM_BOOL OnResize(int, const EmscriptenUiEvent *, void *)
{
    SyncCanvasToCSSAndViewport();

    emscripten_set_timeout([](void *) { SyncCanvasToCSSAndViewport(); }, 16, nullptr);

    return EM_TRUE;
}

void InitSizesAndLlisteners()
{
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;
    attrs.antialias = false;
    attrs.enableExtensionsByDefault = 1;
    g_WebGL_context = emscripten_webgl_create_context("#canvas", &attrs);
    emscripten_webgl_make_context_current(g_WebGL_context);

    SyncCanvasToCSSAndViewport();
    emscripten_set_timeout([](void *) { SyncCanvasToCSSAndViewport(); }, 0, nullptr);

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, OnResize);
}

// clang-format off
EM_JS(int, is_macos, (), {
    const nav = navigator || {};
    // Prefer UA-CH if present
    if (nav.userAgentData && nav.userAgentData.platform) {
      const isMac = nav.userAgentData.platform === 'macOS';
      // iPadOS may not expose UA-CH; fall through if so.
      if (isMac) return 1;
    }
    const ua = (nav.userAgent || ' ');
    const plt = (nav.platform || ' ');
    const looksMac = /Macintosh|Mac OS X/.test(ua) || /^Mac/.test(plt);
    const isIPadOS = (plt === 'MacIntel' && (nav.maxTouchPoints || 0) > 1);
    return (looksMac && !isIPadOS) ? 1 : 0;
});

EM_JS(void, Wake_StartFallbackVideo, (), {
    if (window.__wlFallback) return;
    try
    {
        const c = document.createElement('canvas');
        c.width = 2;
        c.height = 2;
        const cx = c.getContext('2d');
        cx.fillRect(0, 0, 2, 2);
        let on = false;
        (function tick() {
            on = !on;
            cx.fillStyle = on ? '#001' : '#000';
            cx.fillRect(0, 0, 2, 2);
            setTimeout(() => requestAnimationFrame(tick), 1000);
        })();
        const stream = c.captureStream(1);
        const v = document.createElement('video');
        v.playsInline = true;
        v.muted = true;
        v.autoplay = true;
        v.loop = true;
        v.srcObject = stream;
        v.style.cssText = 'position:fixed;width:1px;height:1px;opacity:0;left:0;top:0;pointer-events:none';
        document.body.appendChild(v);
        v.play().catch(() => {});
        window.__wlFallback = { v, stream };
        console.log('[wake] fallback video started');
    }
    catch (e)
    {
        window.__wl_lastErr = 'fallback:' + (e && e.name || 'err');
    }
});

EM_JS(void, Wake_TryNow_NoAwait, (), {
    if (!('wakeLock' in navigator))
    {
        window.__wl_lastErr = 'unsupported';
        return;
    }
    if (window.__wl && !window.__wl.released) return;

    navigator.wakeLock.request('screen')
        .then(s =>
                  {
                      window.__wl = s;
                      window.__wl_lastErr = ' ';
                      console.log('[wake] acquired');
                      s.addEventListener(
                          'release', () => {
                              console.log('[wake] released');
                              window.__wl = null;
                          });
                  })
        .catch(e => {
            window.__wl_lastErr = (e && e.name) || 'denied';
            console.warn('[wake] denied:', window.__wl_lastErr);
        });
});

EM_JS(void, Wake_InstallLifecycle, (), {
    if (Module.__wl_life) return;
    Module.__wl_life = true;
    const retry = () =>
    {
        if (!window.__wl || window.__wl.released)
        {
            if ('wakeLock' in navigator && document.visibilityState === 'visible')
            {
                navigator.wakeLock.request('screen')
                    .then(s =>
                              {
                                  window.__wl = s;
                                  window.__wl_lastErr = ' ';
                              })
                    .catch(e => { window.__wl_lastErr = (e && e.name) || 'denied'; });
            }
        }
    };
    document.addEventListener('visibilitychange', retry, true);
    window.addEventListener('pageshow', retry, true);
    window.addEventListener('focus', retry, true);
    if (screen.orientation && screen.orientation.addEventListener) screen.orientation.addEventListener('change', retry, true);
});

EM_JS(void, Wake_OnGesture_Begin, (), {
    Wake_TryNow_NoAwait();
    if (!window.__wl || window.__wl.released)
    {
        if (window.__wl_lastErr)
        {
            Wake_StartFallbackVideo();
        }
    }
});

EM_JS(void, Wake_AfterFullscreenChange, (), {
    // Some WebKit builds only allow lock after fullscreen transition.
    if (!window.__wl || window.__wl.released)
    {
        Wake_TryNow_NoAwait();
        if (!window.__wl || window.__wl.released)
        {
            if (window.__wl_lastErr)
            {
                Wake_StartFallbackVideo();
            }
        }
    }
});
// clang-format on

void ToggleFullScreen()
{
    if (static bool isMacOS = is_macos(); isMacOS)
    {
        return;
    }

    EmscriptenFullscreenChangeEvent st;

    if (emscripten_get_fullscreen_status(&st); st.isFullscreen)
    {
        emscripten_exit_fullscreen();
    }
    else
    {
        emscripten_request_fullscreen("#canvas", EM_FALSE);
    }
}

EM_BOOL OnMouseDown(int, const EmscriptenMouseEvent *, void *)
{
    Wake_OnGesture_Begin();

    ToggleFullScreen();

    return EM_TRUE;
}

EM_BOOL OnTouchStart(int, const EmscriptenTouchEvent *, void *)
{
    Wake_OnGesture_Begin();

    ToggleFullScreen();

    return EM_TRUE;
}

void InstallClickAnywhereFullscreen()
{
    emscripten_set_mousedown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, OnMouseDown);
    emscripten_set_touchstart_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, OnTouchStart);
    emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE,
                                             [](int, const EmscriptenFullscreenChangeEvent *, void *) -> EM_BOOL {
                                                 SyncCanvasToCSSAndViewport();
                                                 Wake_AfterFullscreenChange();
                                                 return EM_TRUE;
                                             });
    Wake_InstallLifecycle();
}

#else

bool is_macos()
{
    return false;
}
bool is_firefox()
{
    return false;
}
void InitSizesAndLlisteners() {}
void InstallClickAnywhereFullscreen() {}

#endif // __EMSCRIPTEN__

bool Graphics::Init(WindowType _fullscreen)
{
    SDL_DisableScreenSaver();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);

    InitSizesAndLlisteners();
    InstallClickAnywhereFullscreen();

    const int createW = g_windowWidth > 0 ? g_windowWidth : (int)g_width;
    const int createH = g_windowHeight > 0 ? g_windowHeight : (int)g_height;

    g_windows =
        SDL_CreateWindow("Second Reality: Reawakened", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, createW, createH, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!g_windows)
    {
        die("SDL_CreateWindow failed");
    }

    if (g_context = SDL_GL_CreateContext(g_windows); !g_context)
    {
        die("SDL_GL_CreateContext failed");
    }

    SDL_GL_MakeCurrent(g_windows, g_context);
    SDL_GL_SetSwapInterval(0);

    if (!g_pixelPerfect)
    {
        SDL_MaximizeWindow(g_windows);
    }

    SDL_ShowCursor(SDL_DISABLE);

    if (_fullscreen == WindowType::Fullscreen)
    {
        SDL_SetWindowFullscreen(g_windows, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    // Allocate the native-size framebuffer before allocating GPU textures.
    // The virtual framebuffer stays at the fixed logical resolution
    // (VIRTUAL_SCREEN_WIDTH x VIRTUAL_SCREEN_HEIGHT = 640x400); the window
    // size (g_width/g_height) only controls the on-screen viewport/scaling.
    {
        int dw = 0, dh = 0;
        SDL_GL_GetDrawableSize(g_windows, &dw, &dh);
        if (dw > 0 && dh > 0)
        {
            g_width = static_cast<float>(dw);
            g_height = static_cast<float>(dh);
        }
        g_virtualWidth = VIRTUAL_SCREEN_WIDTH;
        g_virtualHeight = VIRTUAL_SCREEN_HEIGHT;
        g_screen32.assign((size_t)g_virtualWidth * g_virtualHeight, 0);
    }

    initGL();

    return true;
}

#ifndef __EMSCRIPTEN__
// Split out of Update() so the pause loop can dispatch keys as well: while
// paused nothing else runs, and SDL_PumpEvents only fills SDL's queue, so the
// resume key was polled but never handled and the demo could not be resumed.
void Graphics::PollSdlEvents()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if ((ev.type == SDL_QUIT) || (ev.type == SDL_KEYUP && ev.key.keysym.sym == SDLK_ESCAPE))
        {
            // Graceful: the music and the picture fade to black, then the demo
            // unwinds. g_wantsToQuit is raised by DemoUpdateExitFade() when the
            // fade completes.
            g_exitFadeRequest = true;
        }

        if (ev.type == SDL_KEYDOWN && !ev.key.repeat)
        {
            if (ev.key.keysym.sym == SDLK_F11)
            {
                static bool fs = false;
                fs = !fs;
                SDL_SetWindowFullscreen(g_windows, fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            }

            // Only armed when SR_DEBUG is set: pausing is a development aid and
            // has no place in a normal run.
            if (SDL_getenv("SR_DEBUG") && ev.key.keysym.sym == SDLK_SPACE)
            {
                extern void DemoTogglePause();
                DemoTogglePause();
            }

            // Only armed when the overlay was asked for with SR_DEBUG: a
            // development key binding has no place in a normal run.
            if (SDL_getenv("SR_DEBUG") && ev.key.keysym.sym == SDLK_F12)
            {
                // Routed through a hook: the key handling lives in this file, the
                // overlay in main. The toggle must not disturb the counters, which
                // keep running while hidden.
                extern void DemoToggleDebugOverlay();
                DemoToggleDebugOverlay();
            }

            if (g_partSkipDelta == 0)
            {
                if (ev.key.keysym.sym == SDLK_PAGEDOWN)
                {
                    g_partSkipDelta = 1;
                    g_wantsToQuit = true;
                    Visu::xit = 1;
                }
                else if (ev.key.keysym.sym == SDLK_PAGEUP)
                {
                    g_partSkipDelta = -1;
                    g_wantsToQuit = true;
                    Visu::xit = 1;
                }
            }
        }

        if (ev.type == SDL_WINDOWEVENT)
        {
            if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                g_width = ev.window.data1;
                g_height = ev.window.data2;
            }
        }
    }
}
#endif // __EMSCRIPTEN__

void Graphics::Update()
{
#ifndef __EMSCRIPTEN__
    PollSdlEvents();
#endif

#ifndef __EMSCRIPTEN__
    // Re-query every frame: the SIZE_CHANGED handler above can miss resizes
    if (g_windows)
    {
        int dw = 0, dh = 0;
        SDL_GL_GetDrawableSize(g_windows, &dw, &dh);
        if (dw > 0 && dh > 0)
        {
            bool sizeChanged = (dw != (int)g_width || dh != (int)g_height);
            g_width = static_cast<float>(dw);
            g_height = static_cast<float>(dh);
            if (sizeChanged)
            {
                // The virtual framebuffer stays at the fixed logical
                // resolution (640x400); only the viewport adapts to the
                // window size. Resizing g_screen32 to the window would
                // break every part that writes it at 640x400.
                if (g_virtualWidth != VIRTUAL_SCREEN_WIDTH || g_virtualHeight != VIRTUAL_SCREEN_HEIGHT)
                {
                    g_virtualWidth = VIRTUAL_SCREEN_WIDTH;
                    g_virtualHeight = VIRTUAL_SCREEN_HEIGHT;
                    g_screen32.assign(g_virtualWidth * g_virtualHeight, 0);
                    if (g_texture) { glDeleteTextures(1, &g_texture); g_texture = 0; }
                    if (g_overlayTex) { glDeleteTextures(1, &g_overlayTex); g_overlayTex = 0; }
                    g_texture = makeTextureRGBA8(g_virtualWidth, g_virtualHeight);
                    g_overlayTex = makeTextureRGBA8(g_virtualWidth, g_virtualHeight);
                    glBindTexture(GL_TEXTURE_2D, g_overlayTex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }
        }
    }
#endif

    if (m_sdfTextCb)
        m_sdfTextCb();

    // m_forceMeshPass lets a part with no geometry of its own still reach the mesh
    // pass, which is where the shockwave torus is flushed. Deliberately separate
    // from m_meshBgScreen: that one would also re-enable the cpuPixels composite,
    // which OUTTA has no business painting.
    if (!m_meshes.empty() || !m_texMeshes.empty() || m_meshBgScreen || m_forceMeshPass)
    {
        UpdateMeshPass();

        static bool isFirefoxMesh = is_firefox();
        AudioPlayer::Update(!isFirefoxMesh);
        return;
    }

    if (m_effectProg)
    {
        glViewport(0, 0, g_width, g_height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(m_effectProg);
        // z = window aspect ratio (for shaders that need aspect correction like rotozoom)
        glUniform3f(m_effectLocResolution, g_width, g_height, g_width / g_height);

        // Dynamic viewport: letterbox the source into a centered rect that
        // respects the part's native resolution ratio (e.g. 320x200=1.6,
        // 320x400=0.8), not a hard-coded 4:3.
        if (m_effectLocViewport >= 0)
        {
            float vw, vh;
            if (g_respectRatio)
            {
                constexpr float DOS_4_3 = 4.0f / 3.0f;
                const float windowAspect = g_width / g_height;
                if (windowAspect > DOS_4_3)
                {
                    vh = g_height;
                    vw = vh * DOS_4_3;
                }                else
                {
                    vw = g_width;
                    vh = vw / DOS_4_3;
                }
            }
            else
            {
                vw = g_width;
                vh = g_height;
            }
            const float vx = (g_width - vw) * 0.5f;
            const float vy = (g_height - vh) * 0.5f;
            glUniform4f(m_effectLocViewport, vx, vy, vw, vh);
        }

        if (m_effectLocOverlay >= 0)
        {
            PrepareTextureForGPU();
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, g_overlayTex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, g_screen32.data());
            glUniform1i(m_effectLocOverlay, 1);
            glActiveTexture(GL_TEXTURE0);
        }

        if (m_effectLocSourceSize >= 0)
        {
            glUniform2f(m_effectLocSourceSize, g_sourceWidth, g_sourceHeight);
        }

        if (!m_effectTextures.empty())
        {
            glUseProgram(m_effectProg);
            int unit = 2;
            for (auto & kv : m_effectTextures)
            {
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
                glBindTexture(GL_TEXTURE_2D, kv.second.tex);
                glUniform1i(glGetUniformLocation(m_effectProg, kv.first.c_str()), unit);
                ++unit;
            }
            glActiveTexture(GL_TEXTURE0);
        }

        if (m_pointsCount > 0 && m_pointsProg)
        {
            const float fullscreenVerts[] = {
                -1.0f, -1.0f,
                -1.0f,  1.0f,
                 1.0f, -1.0f,
                 1.0f,  1.0f,
            };

            glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenVerts), fullscreenVerts, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(g_Loc_aPos);
            glVertexAttribPointer(g_Loc_aPos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray(g_Loc_aPos);

            glUseProgram(m_pointsProg);
            glUniform3f(m_pointsLocResolution, g_width, g_height, 0.0f);

            if (m_pointsLocStretch < 0) m_pointsLocStretch = glGetUniformLocation(m_pointsProg, "uStretch");
            if (m_pointsLocStretch >= 0) glUniform1f(m_pointsLocStretch, m_pointsStretch);

            {
                GLint locPal = glGetUniformLocation(m_pointsProg, "uPalette");
                if (m_pointsLocPalette < 0) m_pointsLocPalette = locPal;
                if (locPal >= 0)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, g_meshPalTex);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, Shim::palette);
                    glUniform1i(locPal, 1);
                    glActiveTexture(GL_TEXTURE0);
                }
            }

            glBindBuffer(GL_ARRAY_BUFFER, m_pointsVBO);

            GLint locPos = glGetAttribLocation(m_pointsProg, "aPos");
            GLint locDepth = glGetAttribLocation(m_pointsProg, "aDepth");
            GLint locSize = glGetAttribLocation(m_pointsProg, "aSize");
            GLint locColor = glGetAttribLocation(m_pointsProg, "aColorIdx");

            glEnableVertexAttribArray(locPos);
            glVertexAttribPointer(locPos, 3, GL_FLOAT, GL_FALSE, m_pointsStride, nullptr);

            if (locDepth >= 0)
            {
                glEnableVertexAttribArray(locDepth);
                glVertexAttribPointer(locDepth, 1, GL_FLOAT, GL_FALSE, m_pointsStride, (const void *)(3 * sizeof(float)));
            }

            if (locSize >= 0)
            {
                glEnableVertexAttribArray(locSize);
                glVertexAttribPointer(locSize, 1, GL_FLOAT, GL_FALSE, m_pointsStride, (const void *)(4 * sizeof(float)));
            }

            if (locColor >= 0)
            {
                glEnableVertexAttribArray(locColor);
                glVertexAttribPointer(locColor, 1, GL_FLOAT, GL_FALSE, m_pointsStride, (const void *)(5 * sizeof(float)));
            }

            if (m_pointsLocFade >= 0)
            {
                glUniform1f(m_pointsLocFade, m_pointsFade);
            }

            glDrawArrays(GL_POINTS, 0, m_pointsCount);

            glDisableVertexAttribArray(locPos);
            if (locDepth >= 0) glDisableVertexAttribArray(locDepth);
            if (locSize >= 0) glDisableVertexAttribArray(locSize);
            if (locColor >= 0) glDisableVertexAttribArray(locColor);

            m_pointsCount = 0;
            glUseProgram(0);
        }
        else
        {
            const float fullscreenVerts[] = {
                -1.0f, -1.0f,
                -1.0f,  1.0f,
                 1.0f, -1.0f,
                 1.0f,  1.0f,
            };

            glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenVerts), fullscreenVerts, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(g_Loc_aPos);
            glVertexAttribPointer(g_Loc_aPos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray(g_Loc_aPos);
        }

        if (m_effectLocOverlay >= 0)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
        }
        glUseProgram(0);
        FlushSdfQuads();
        FlushRgbaQuads();
        DrawFullscreenQuad(0.0f, 0.0f, 0.0f, g_exitFade);
        WriteFramesPipe();
        SDL_GL_SwapWindow(g_windows);

        if (g_captureRemaining > 0)
        {
            CaptureFrame(g_capturePath);
            // Counts rendered frames, not files written: CaptureFrame decides
            // per SR_CAPTURE_STRIDE whether to write one, and --capture is a
            // frame budget. Debiting only on a written file would make the run
            // three times longer than asked for at a stride of 3.
            --g_captureRemaining;
        }
    }
    else
    {
        PrepareTextureForGPU();
        if (!g_directScreen && !g_xbrzBuffer.empty())
        {
            const int xw = g_demoScreenWidth * (int)g_xbrzFactor;
            const int xh = g_demoScreenHeight * (int)g_xbrzFactor;
            glBindTexture(GL_TEXTURE_2D, g_texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         xw, xh,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, g_xbrzBuffer.data());
        }
        else
        {
            // Full VIRTUAL upload: g_screen32 is VIRTUAL-strided, so a narrower
            // sub-region would need GL_UNPACK_ROW_LENGTH.
            UpdateTextureRGBA8(g_texture, VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, g_screen32.data());
        }
        Render();
        // SDF/RGBA overlays draw on top of the plain bitmap background.
        FlushSdfQuads();
        FlushRgbaQuads();
        DrawFullscreenQuad(0.0f, 0.0f, 0.0f, g_exitFade);
        WriteFramesPipe();
        SDL_GL_SwapWindow(g_windows);

        if (g_captureRemaining > 0)
        {
            CaptureFrame(g_capturePath);
            // Counts rendered frames, not files written: CaptureFrame decides
            // per SR_CAPTURE_STRIDE whether to write one, and --capture is a
            // frame budget. Debiting only on a written file would make the run
            // three times longer than asked for at a stride of 3.
            --g_captureRemaining;
        }
    }

    static bool isFirefox = is_firefox();
    AudioPlayer::Update(!isFirefox);
}

void Graphics::Close()
{
#ifndef __EMSCRIPTEN__

    if (g_linearSampling) glDeleteSamplers(1, &g_linearSampling);
    if (g_nearestSampling) glDeleteSamplers(1, &g_nearestSampling);
    if (g_texture) glDeleteTextures(1, &g_texture);
    if (g_overlayTex) glDeleteTextures(1, &g_overlayTex);
    if (g_meshVBO) glDeleteBuffers(1, &g_meshVBO);
    if (g_meshIBO) glDeleteBuffers(1, &g_meshIBO);
    if (g_meshPalTex) glDeleteTextures(1, &g_meshPalTex);
    if (g_meshProg) glDeleteProgram(g_meshProg);
    if (g_texVBO) glDeleteBuffers(1, &g_texVBO);
    if (g_texIBO) glDeleteBuffers(1, &g_texIBO);
    if (g_texIdxTex) glDeleteTextures(1, &g_texIdxTex);
    if (g_texProg) glDeleteProgram(g_texProg);
    if (g_bgScreenProg) glDeleteProgram(g_bgScreenProg);
    if (g_VBO) glDeleteBuffers(1, &g_VBO);
    if (g_prog) glDeleteProgram(g_prog);

    SDL_GL_DeleteContext(g_context);
    SDL_DestroyWindow(g_windows);
    SDL_Quit();
#endif // __EMSCRIPTEN__

    SDL_EnableScreenSaver();
}

bool Graphics::WantsToQuit()
{
    return g_wantsToQuit;
}

void Graphics::PrepareTextureForGPU()
{
    if (g_directScreen)
    {
        // The part wrote the native-resolution framebuffer directly.
        g_sourceWidth = 1.0f;
        g_sourceHeight = 1.0f;
        return;
    }

    // The CPU still renders in the demo's logical resolution (320x200 or
    // 320x400). We pack that into the TOP-LEFT of the native-resolution
    // framebuffer, then the GPU upscales that sub-region to the full window.
    // This keeps 1:1 CPU writes and lets the GPU do the high-res scaling.
    const int srcW = g_hires ? (g_demoScreenWidth * 2) : g_demoScreenWidth;
    const int srcH = g_demoScreenHeight;
    const int fbW = VIRTUAL_SCREEN_WIDTH;

    unsigned char * src = Shim::cpuPixels + Shim::startpixel;
    unsigned int * dst = g_screen32.data();

    if (g_demoScreenWidth == 640 && g_demoScreenHeight == 350)
    {
        for (int y = 0; y < g_demoScreenHeight; y++)
        {
            for (int x = 0; x < g_demoScreenWidth; x++)
                *dst++ = Shim::palette[*src++];
            dst += fbW - g_demoScreenWidth;
        }
    }
    else if (g_hires)
    {
        for (int y = 0; y < srcH; y++)
        {
            for (int x = 0; x < g_demoScreenWidth; x++)
            {
                unsigned int c = Shim::palette[*src++];
                dst[0] = c; dst[1] = c; dst += 2;
            }
            dst += fbW - srcW;
        }
    }
    else
    {
        for (int y = 0; y < srcH; y++)
        {
            for (int x = 0; x < g_demoScreenWidth; x++)
                *dst++ = Shim::palette[*src++];
            dst += fbW - g_demoScreenWidth;
        }
    }

    g_sourceWidth = static_cast<float>(srcW) / VIRTUAL_SCREEN_WIDTH;
    g_sourceHeight = static_cast<float>(srcH) / VIRTUAL_SCREEN_HEIGHT;

    if (m_textOverlay)
        m_textOverlay(g_screen32.data(), VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT);

    // Upscale the CPU bitmap framebuffer with xBRz so pixel art stays crisp
    // (no GPU bilinear blur). The upload + source UVs below use the 4x buffer.
    // Only the VRAM sub-region is upscaled, and only when it changed since the
    // last frame (static bitmaps are upscaled once and then cached).
    // A framebuffer larger than the fixed 640x400 composite (the 640x800
    // credits mode) is a photo at native size: xBRz would cost 8 Mpixel/frame
    // for no visible gain, so it is skipped and the stale buffer cleared.
    const bool xbrzFits = (g_demoScreenWidth <= DOUBlE_SCREEN_WIDTH) &&
                          (g_demoScreenHeight <= DOUBlE_SCREEN_HEIGHT);
    if (!xbrzFits)
    {
        g_xbrzBuffer.clear();
        g_xbrzLastSource.clear();
    }
    if (g_xbrzEnabled && xbrzFits && VIRTUAL_SCREEN_WIDTH > 0 && VIRTUAL_SCREEN_HEIGHT > 0)
    {
        const size_t srcN = (size_t)srcW * srcH;
        const size_t xw = (size_t)srcW * g_xbrzFactor;
        const size_t xh = (size_t)srcH * g_xbrzFactor;
        if (g_xbrzLastSource.size() != srcN)
            g_xbrzLastSource.assign(srcN, 0xFFFFFFFFu);
        bool changed = false;
        for (size_t y = 0; y < (size_t)srcH && !changed; ++y)
        {
            for (size_t x = 0; x < (size_t)srcW; ++x)
            {
                if (g_screen32[y * (size_t)fbW + x] != g_xbrzLastSource[y * (size_t)srcW + x])
                {
                    changed = true;
                    break;
                }
            }
        }
        if (changed)
        {
            if (g_xbrzBuffer.size() != xw * xh)
                g_xbrzBuffer.assign(xw * xh, 0);
            static std::vector<unsigned int> xbrzIn;
            if (xbrzIn.size() != srcN) xbrzIn.resize(srcN);
            // Palette pixels are (b<<2)|(g<<10)|(r<<18) with 6-bit channels.
            // xBRz wants 0x00RRGGBB with 8-bit channels: expand 6->8 bits.
            // The source is packed top-left in g_screen32 with a 640 stride.
            for (size_t y = 0; y < (size_t)srcH; ++y)
            {
                for (size_t x = 0; x < (size_t)srcW; ++x)
                {
                    unsigned int c = g_screen32[y * (size_t)fbW + x];
                    unsigned int r6 = (c >> 18) & 0x3Fu;
                    unsigned int g6 = (c >> 10) & 0x3Fu;
                    unsigned int b6 = (c >> 2) & 0x3Fu;
                    xbrzIn[y * (size_t)srcW + x] =
                        (r6 * 255 / 63) << 16 | (g6 * 255 / 63) << 8 | (b6 * 255 / 63);
                }
            }
            xbrz::scale(g_xbrzFactor, xbrzIn.data(), g_xbrzBuffer.data(),
                        srcW, srcH, xbrz::ColorFormat::RGB);
            for (size_t y = 0; y < (size_t)srcH; ++y)
                for (size_t x = 0; x < (size_t)srcW; ++x)
                    g_xbrzLastSource[y * (size_t)srcW + x] = g_screen32[y * (size_t)fbW + x];
        }
        g_sourceWidth = 1.0f;
        g_sourceHeight = 1.0f;
    }
}

void Graphics::SetTextOverlay(void (*fn)(unsigned int * rgba, int w, int h))
{
    m_textOverlay = fn;
}

void Graphics::SetSdfTextCallback(void (*fn)())
{
    m_sdfTextCb = fn;
}

void Graphics::ChangeMode(int x, int y, float jsss)
{
    g_demoScreenWidth = x;
    g_demoScreenHeight = y;

    g_jsss = g_smooth ? 1.0f : jsss;

    // The virtual framebuffer always stays at the fixed logical resolution;
    // only the viewport in Render() adapts to the window. The texture is
    // recreated on every part start because an xBRz part reallocates it to its
    // own 4x size, and the next non-xBRz part must get it back at VIRTUAL size.
    g_virtualWidth = VIRTUAL_SCREEN_WIDTH;
    g_virtualHeight = VIRTUAL_SCREEN_HEIGHT;
    if ((int)g_screen32.size() != g_virtualWidth * g_virtualHeight)
    {
        g_screen32.assign(g_virtualWidth * g_virtualHeight, 0);
    }

    if (g_texture) { glDeleteTextures(1, &g_texture); g_texture = 0; }
    if (g_overlayTex) { glDeleteTextures(1, &g_overlayTex); g_overlayTex = 0; }
    g_texture = makeTextureRGBA8(g_virtualWidth, g_virtualHeight);
    g_overlayTex = makeTextureRGBA8(g_virtualWidth, g_virtualHeight);
    glBindTexture(GL_TEXTURE_2D, g_overlayTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    Common::reset();
}

void Graphics::SetGpuEffect(const char * fragmentShader)
{
    if (m_effectProg) glDeleteProgram(m_effectProg);
    if (m_pointsProg) glDeleteProgram(m_pointsProg);
    m_pointsProg = 0;

    // Fullscreen quad vertex shader (2D positions)
    const char * vs =
        "precision mediump float;\n"
        "attribute vec2 aPos;\n"
        "void main(){\n"
        "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    GLuint vsId = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fsId = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
    m_effectProg = linkProgram(vsId, fsId);
    glDeleteShader(vsId);
    glDeleteShader(fsId);

    m_effectLocResolution = glGetUniformLocation(m_effectProg, "uResolution");
    m_effectLocViewport = glGetUniformLocation(m_effectProg, "uViewport");
    m_effectLocSourceSize = glGetUniformLocation(m_effectProg, "uSourceSize");
    m_effectLocTime = glGetUniformLocation(m_effectProg, "uTime");
    m_effectLocFrame = glGetUniformLocation(m_effectProg, "uFrame");
    m_effectLocOverlay = glGetUniformLocation(m_effectProg, "uTexOverlay");

    // Points vertex shader: reads vec3 position + float depth + float size + float colorIdx
    // DotVertex layout: x,y,z (offset 0), depth (offset 12), size (offset 16), colorIdx (offset 20), stride=24
    const char * vsPoints =
        "precision mediump float;\n"
        "attribute vec3 aPos;\n"
        "attribute float aDepth;\n"
        "attribute float aSize;\n"
        "attribute float aColorIdx;\n"
        "varying float vDepth;\n"
        "varying float vColorIdx;\n"
        "varying vec2 vPos;\n"
        "void main(){\n"
        "  gl_Position = vec4(aPos.xy, 0.0, 1.0);\n"
        "  gl_PointSize = aSize;\n"
        "  vDepth = aDepth;\n"
        "  vColorIdx = aColorIdx;\n"
        "  vPos = aPos.xy;\n"
        "}\n";

    // Fragment shader that uses vDepth + vColorIdx for colored dots
    const char * fsPoints =
        "precision mediump float;\n"
        "uniform vec3 uResolution;\n"
        "uniform float uFade;\n"
        "uniform float uTime;\n"
        "uniform float uFrame;\n"
        "uniform float uStretch;\n"
        "uniform sampler2D uPalette;\n"
        "varying float vDepth;\n"
        "varying float vColorIdx;\n"
        "varying vec2 vPos;\n"
        "\n"
        "vec3 hsv2rgb(vec3 c) {\n"
        "  vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n"
        "  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
        "  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
        "}\n"
        "\n"
        "void main() {\n"
        "  vec2 uv = gl_PointCoord * 2.0 - 1.0;\n"
        "  float d;\n"
        "  if (uStretch > 0.0) {\n"
        "    vec2 sp = vec2(vPos.x * (uResolution.x / uResolution.y), vPos.y);\n"
        "    vec2 ax = normalize(sp + vec2(1e-5, 1e-5));\n"
        "    vec2 pp = vec2(-ax.y, ax.x);\n"
        "    float a = dot(uv, ax) / (1.0 + uStretch);\n"
        "    float b = dot(uv, pp) / (1.0 - uStretch * 0.35);\n"
        "    d = a * a + b * b;\n"
        "  } else {\n"
        "    d = dot(uv, uv);\n"
        "  }\n"
        "  if (d > 1.0) discard;\n"
        "\n"
        "  float bright = clamp((9200.0 - vDepth) / 400.0, 0.2, 1.0);\n"
        "  if (vColorIdx > 19.5 && vColorIdx < 40.5) {\n"
        "    float rr = min(d, 1.0);\n"
        "    vec3 nrm = vec3(uv, sqrt(max(0.0, 1.0 - rr)));\n"
        "    vec3 lightDir = normalize(vec3(0.55, 0.60, 0.60));\n"
        "    float diff = max(dot(nrm, lightDir), 0.0);\n"
        "    float spec = pow(max(dot(reflect(-lightDir, nrm), vec3(0.0, 0.0, 1.0)), 0.0), 28.0);\n"
        "    vec3 base = vec3(0.30, 0.52, 1.00) * (0.65 + 0.35 * bright);\n"
        "    float specK = 0.9;\n"
        "    if (vColorIdx > 30.0) { base = vec3(0.10, 0.11, 0.13); specK = 0.0; }\n"
        "    vec3 col = base * (0.55 + 0.75 * diff) + vec3(specK) * spec;\n"
        "    float cs = 0.0;\n"
        "    if (uFrame > 2360.0 && uFrame <= 2400.0) cs = (uFrame - 2360.0) / 32.0;\n"
        "    else if (uFrame > 2400.0) cs = 1.0 + (uFrame - 2400.0) / 32.0;\n"
        "    if (cs > 1.0) col = vec3(2.0 - cs);\n"
        "    else if (cs > 0.0) col = col + (vec3(1.0) - col) * cs;\n"
        "    gl_FragColor = vec4(col, 1.0);\n"
        "    return;\n"
        "  }\n"
        "  vec3 baseColor;\n"
        "  if (vColorIdx >= 64.0) {\n"
        "    baseColor = texture2D(uPalette, vec2((mod(vColorIdx, 256.0) + 0.5) / 256.0, 0.5)).rgb;\n"
        "    baseColor *= bright;\n"
        "  } else if (vColorIdx > 11.5) {\n"
        "    baseColor = vec3(0.17);\n"
        "  } else if (vColorIdx > 10.5) {\n"
        "    float g = 0.75 - (vDepth - 4000.0) / 16000.0;\n"
        "    g = clamp(g, 0.0, 1.0);\n"
        "    baseColor = vec3(g / 3.0, g, g * 1.1);\n"
        "  } else if (vColorIdx > 9.5) {\n"
        "    float zNorm = clamp((vDepth - 8800.0) / 250.0, 0.0, 1.0);\n"
        "    float bank = (mod(floor(zNorm * 8.0 - uTime * 1.5), 2.0) > 0.5) ? 1.0 : 0.75;\n"
        "    float shade = 1.0 - zNorm;\n"
        "    baseColor = vec3(bank * shade * shade) * bright;\n"
        "  } else if (vColorIdx > 8.5) {\n"
        "    baseColor = vec3(0.22, 0.24, 0.30) * bright;\n"
        "  } else if (vColorIdx > 7.5) {\n"
        "    baseColor = vec3(0.85, 0.9, 1.0) * bright;\n"
        "  } else {\n"
        "    float hue = fract(vColorIdx * 0.125);\n"
        "    baseColor = hsv2rgb(vec3(hue, 0.7, 1.0)) * bright;\n"
        "  }\n"
        "\n"
        "  float highlight = 1.0 - d * 0.3;\n"
        "\n"
        "  vec3 col = baseColor * highlight;\n"
        "\n"
        "  float edge = 1.0 - smoothstep(0.5, 1.0, d);\n"
        "\n"
        "  if (vColorIdx > 10.5) {\n"
        "    float cs = 0.0;\n"
        "    if (uFrame > 2360.0 && uFrame <= 2400.0) cs = (uFrame - 2360.0) / 32.0;\n"
        "    else if (uFrame > 2400.0) cs = 1.0 + (uFrame - 2400.0) / 32.0;\n"
        "    if (cs > 1.0) col = vec3(2.0 - cs);\n"
        "    else if (cs > 0.0) col = col + (vec3(1.0) - col) * cs;\n"
        "  }\n"
        "\n"
        "  gl_FragColor = vec4(col * edge * uFade, edge);\n"
        "}\n";

    GLuint vsPtsId = compileShader(GL_VERTEX_SHADER, vsPoints);
    GLuint fsPtsId = compileShader(GL_FRAGMENT_SHADER, fsPoints);
    m_pointsProg = linkProgram(vsPtsId, fsPtsId);
    glDeleteShader(vsPtsId);
    glDeleteShader(fsPtsId);

    m_pointsLocResolution = glGetUniformLocation(m_pointsProg, "uResolution");
    m_pointsLocTime = glGetUniformLocation(m_pointsProg, "uTime");
    m_pointsLocFrame = glGetUniformLocation(m_pointsProg, "uFrame");

    m_pointsLocFade = glGetUniformLocation(m_pointsProg, "uFade");
    if (m_pointsLocFade >= 0)
    {
        glUseProgram(m_pointsProg);
        glUniform1f(m_pointsLocFade, 1.0f);
        glUseProgram(0);
    }
}

float Graphics::FramebufferAspect() const
{
    // The GL viewport is set from g_width/g_height, so geometry that has to come
    // out round must read its aspect from the same place. WindowAspect()
    // deliberately prefers the desktop display mode while fullscreen, because
    // text overlays follow the desktop; mixing that with a drawable height is
    // what stretched the tunnel into ellipses on 21:9.
    return (g_height > 0.0f) ? (g_width / g_height) : (4.0f / 3.0f);
}

float Graphics::WindowHeight() const
{
    if (g_height > 0.0f)
    {
        return g_height;
    }

#ifndef __EMSCRIPTEN__
    if (g_windows)
    {
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(g_windows, &w, &h);
        if (h > 0) return static_cast<float>(h);
    }
#endif
    return static_cast<float>(VIRTUAL_SCREEN_HEIGHT);
}

void Graphics::ClearGpuEffect()
{
    if (m_effectProg)
    {
        glDeleteProgram(m_effectProg);
        m_effectProg = 0;
    }
    if (m_pointsProg)
    {
        glDeleteProgram(m_pointsProg);
        m_pointsProg = 0;
    }
    if (!m_effectTextures.empty())
    {
        for (auto & kv : m_effectTextures)
        {
            if (kv.second.tex) glDeleteTextures(1, &kv.second.tex);
        }
        m_effectTextures.clear();
    }
    m_effectLocResolution = -1;
    m_effectLocTime = -1;
    m_effectLocFrame = -1;
    m_effectLocOverlay = -1;
}

void Graphics::ResetPartState()
{
    ClearGpuEffect();
    m_pointsCount = 0;
    m_meshes.clear();
    m_texMeshes.clear();
    m_meshBgScreen = false;
    m_forceMeshPass = false;
    m_meshScissorOn = false;
    m_meshClear[0] = 0.0f;
    m_meshClear[1] = 0.0f;
    m_meshClear[2] = 0.0f;
    g_bgClear[0] = 0.0f;
    g_bgClear[1] = 0.0f;
    g_bgClear[2] = 0.0f;
    g_compositeScale = 1.0f;
    m_textOverlay = nullptr;
    m_sdfTextCb = nullptr;
    m_sdfData = nullptr;
    m_sdfW = 0;
    m_sdfH = 0;
    m_rgbaData = nullptr;
    m_rgbaW = 0;
    m_rgbaH = 0;
    m_rgbaIsRgb = false;
    m_texData = nullptr;
    m_texW = 0;
    m_texH = 0;
    m_texTime = 0.0f;
    m_meshWarp = 1.0f;
    g_xbrzBuffer.clear();
    g_xbrzLastSource.clear();
    Shim::startpixel = 0;
    if ((int)g_screen32.size() != g_virtualWidth * g_virtualHeight)
        g_screen32.assign((size_t)g_virtualWidth * g_virtualHeight, 0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, (GLsizei)g_width, (GLsizei)g_height);
}

void Graphics::SetUniform1f(const char * name, float v)
{
    if (!m_effectProg) return;
    glUseProgram(m_effectProg);
    glUniform1f(glGetUniformLocation(m_effectProg, name), v);
    if (m_pointsProg)
    {
        GLint loc = glGetUniformLocation(m_pointsProg, name);
        if (loc >= 0)
        {
            glUseProgram(m_pointsProg);
            glUniform1f(loc, v);
            glUseProgram(m_effectProg);
        }
    }
}

void Graphics::SetUniform2f(const char * name, float x, float y)
{
    if (!m_effectProg) return;
    glUseProgram(m_effectProg);
    glUniform2f(glGetUniformLocation(m_effectProg, name), x, y);
}

void Graphics::SetUniform3f(const char * name, float x, float y, float z)
{
    if (!m_effectProg) return;
    glUseProgram(m_effectProg);
    glUniform3f(glGetUniformLocation(m_effectProg, name), x, y, z);
}

void Graphics::SetUniform1i(const char * name, int v)
{
    if (!m_effectProg) return;
    glUseProgram(m_effectProg);
    glUniform1i(glGetUniformLocation(m_effectProg, name), v);
}

void Graphics::SetUniform4fv(const char * name, const float * data, int count)
{
    if (!m_effectProg) return;
    glUseProgram(m_effectProg);
    glUniform4fv(glGetUniformLocation(m_effectProg, name), count, data);
}

void Graphics::SetGpuTexture(const char * name, const unsigned char * luminance, int w, int h)
{
    if (!luminance || w <= 0 || h <= 0) return;

    EffectTexture & et = m_effectTextures[name];

    if (et.tex == 0)
    {
        glGenTextures(1, &et.tex);
        glBindTexture(GL_TEXTURE_2D, et.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, et.tex);
    }

    et.w = w;
    et.h = h;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, luminance);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Graphics::SetGpuTextureLa(const char * name, const unsigned char * la, int w, int h)
{
    if (!la || w <= 0 || h <= 0) return;

    EffectTexture & et = m_effectTextures[name];

    if (et.tex == 0)
    {
        glGenTextures(1, &et.tex);
        glBindTexture(GL_TEXTURE_2D, et.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, et.tex);
    }

    et.w = w;
    et.h = h;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, la);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Graphics::SetGpuTextureRgb(const char * name, const unsigned char * rgb, int w, int h)
{
    if (!rgb || w <= 0 || h <= 0) return;

    EffectTexture & et = m_effectTextures[name];

    if (et.tex == 0)
    {
        glGenTextures(1, &et.tex);
        glBindTexture(GL_TEXTURE_2D, et.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, et.tex);
    }

    et.w = w;
    et.h = h;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Graphics::SetPointsFade(float fade)
{
    m_pointsFade = fade;
    if (m_pointsProg && m_pointsLocFade >= 0)
    {
        glUseProgram(m_pointsProg);
        glUniform1f(m_pointsLocFade, fade);
        glUseProgram(0);
    }
}

void Graphics::WaitWhilePaused()
{
    SDL_Delay(16);
    // Dispatch, do not just pump: SDL_PumpEvents fills SDL's queue but nothing
    // consumes it while paused, so the resume key would sit there unhandled and
    // the demo could never be resumed.
#ifndef __EMSCRIPTEN__
    PollSdlEvents();
#endif
}

void Graphics::SetPointsAdditive(bool on)
{
    if (on)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    else
    {
        glDisable(GL_BLEND);
    }
}

void Graphics::DrawPoints(const void * data, int count, int stride, float /*pointSize*/, float pointStretch)
{
    if (!m_pointsProg || count <= 0) return;

    m_pointsCount = count;
    m_pointsStride = stride;
    m_pointsStretch = pointStretch;

    if (m_pointsVBO == 0) glGenBuffers(1, &m_pointsVBO);

    glBindBuffer(GL_ARRAY_BUFFER, m_pointsVBO);
    glBufferData(GL_ARRAY_BUFFER, count * stride, data, GL_DYNAMIC_DRAW);
}

void Graphics::DrawExplosion(const GpuTorusVertex * verts, int vertCount, const unsigned short * indices,
                             int indexCount, float grow, float spin, float alpha,
                             bool fire, float centerX, float centerY)
{
    if (g_torusProg == 0 || !verts || vertCount <= 0 || !indices || indexCount <= 0) return;

    ExplosionCmd cmd;
    cmd.vb.reserve((size_t)vertCount * 12);
    for (int i = 0; i < vertCount; ++i)
    {
        const GpuTorusVertex &v = verts[i];
        const float src[12] = { v.px, v.py, v.pz, v.nx, v.ny, v.nz, v.tx, v.ty, v.tz, v.theta, v.phi, 0.0f };
        cmd.vb.insert(cmd.vb.end(), src, src + 12);
    }
    cmd.ib.assign(indices, indices + indexCount);
    cmd.grow = grow;
    cmd.spin = spin;
    cmd.alpha = alpha;
    cmd.fire = fire;
    cmd.centerX = centerX;
    cmd.centerY = centerY;
    m_explosions.push_back(std::move(cmd));
}

void Graphics::DrawMesh(const GpuMeshVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16, const float * model16, float fade)
{
    if (g_meshProg == 0 || !verts || vertCount <= 0 || !indices || indexCount <= 0 || !mvp16 || !model16) return;

    MeshCmd cmd;
    cmd.vb.resize((size_t)vertCount * 8);
    memcpy(cmd.vb.data(), verts, (size_t)vertCount * 8 * sizeof(float));
    cmd.ib.assign(indices, indices + indexCount);
    memcpy(cmd.mvp, mvp16, sizeof(cmd.mvp));
    memcpy(cmd.model, model16, sizeof(cmd.model));
    cmd.fade = fade;
    m_meshes.push_back(std::move(cmd));
}

void Graphics::MeshBackground(float r, float g, float b)
{
    m_meshBgScreen = false; // explicit color background overrides screen composite
    m_meshClear[0] = r;
    m_meshClear[1] = g;
    m_meshClear[2] = b;
}

void Graphics::MeshBackgroundScreen()
{
    m_meshBgScreen = true;
}

void Graphics::DrawTexMesh(const GpuTexVertex * verts, int vertCount, const unsigned short * indices, int indexCount, const float * mvp16)
{
    if (g_texProg == 0 || !verts || vertCount <= 0 || !indices || indexCount <= 0 || !mvp16) return;

    TexMeshCmd cmd;
    cmd.vb.resize((size_t)vertCount * 5);
    memcpy(cmd.vb.data(), verts, (size_t)vertCount * 5 * sizeof(float));
    cmd.ib.assign(indices, indices + indexCount);
    memcpy(cmd.mvp, mvp16, sizeof(cmd.mvp));
    m_texMeshes.push_back(std::move(cmd));
}

void Graphics::MeshTexture(const unsigned char * data, int w, int h)
{
    m_texData = data;
    m_texW = w;
    m_texH = h;
}

void Graphics::MeshTime(float t)
{
    m_texTime = t;
}

void Graphics::MeshWarp(float w)
{
    m_meshWarp = w;
}

// Uploads the single channel SDF into RGBA so sampling works on drivers where
// GL_LUMINANCE is deprecated. One texture per context.
static void UploadSdf(GLuint tex, const unsigned char * data, int w, int h)
{
    if (!tex || !data || w <= 0 || h <= 0) return;
    std::vector<unsigned char> rgba((size_t)w * h * 4, 255);
    for (size_t i = 0; i < (size_t)w * h; ++i)
    {
        rgba[i * 4 + 0] = data[i];
        rgba[i * 4 + 1] = data[i];
        rgba[i * 4 + 2] = data[i];
        rgba[i * 4 + 3] = 255;
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Graphics::SdfTexture(const unsigned char * data, int w, int h)
{
    m_sdfData = data;
    m_sdfW = w;
    m_sdfH = h;
    if (g_sdfTex == 0)
    {
        glGenTextures(1, &g_sdfTex);
        // Same trap as the overlay atlas in SetOverlaySdfTexture(): with no
        // explicit filters the default MIN filter is NEAREST_MIPMAP_LINEAR,
        // which needs mipmaps this texture does not have. The texture is then
        // incomplete and every sample comes back black, so a part drawing its
        // own SDF text got nothing on screen while the overlay still worked.
        glBindTexture(GL_TEXTURE_2D, g_sdfTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    UploadSdf(g_sdfTex, data, w, h);
}

void Graphics::SetOverlaySdfTexture(const unsigned char * data, int w, int h)
{
    m_overlaySdfData = data;
    m_overlaySdfW = w;
    m_overlaySdfH = h;
    if (g_sdfTexOverlay == 0)
    {
        glGenTextures(1, &g_sdfTexOverlay);
        // Without explicit filters the default is NEAREST_MIPMAP_LINEAR, which
        // needs mipmaps this texture does not have. That makes it incomplete and
        // every sample comes back black, so the overlay drew nothing at all.
        glBindTexture(GL_TEXTURE_2D, g_sdfTexOverlay);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    UploadSdf(g_sdfTexOverlay, data, w, h);
}

void Graphics::DrawSdfText(const GpuTexVertex * quads, int quadCount, const float * mvp16, float r, float g, float b, bool sharp, bool overlay)
{
    if (g_sdfProg == 0 || !quads || quadCount <= 0 || !mvp16) return;

    SdfCmd cmd;
    cmd.vb.resize((size_t)quadCount * 4 * 5);
    memcpy(cmd.vb.data(), quads, (size_t)quadCount * 4 * 5 * sizeof(float));
    cmd.ib.reserve((size_t)quadCount * 6);
    for (int q = 0; q < quadCount; ++q)
    {
        unsigned short base = (unsigned short)(q * 4);
        cmd.ib.push_back(base + 0); cmd.ib.push_back(base + 1); cmd.ib.push_back(base + 2);
        cmd.ib.push_back(base + 1); cmd.ib.push_back(base + 3); cmd.ib.push_back(base + 2);
    }
    memcpy(cmd.mvp, mvp16, sizeof(cmd.mvp));
    cmd.color[0] = r; cmd.color[1] = g; cmd.color[2] = b;
    cmd.sharp = sharp;
    cmd.useOverlay = overlay;
    m_sdfQuads.push_back(std::move(cmd));
}

void Graphics::RgbaTexture(const unsigned char * data, int w, int h, bool isRgb, bool perFrame)
{
    m_rgbaData = data;
    m_rgbaW = w;
    m_rgbaH = h;
    m_rgbaIsRgb = isRgb;
    m_rgbaPerFrame = perFrame;
}

void Graphics::SetRgbaWhite(float white)
{
    g_rgbaWhite = white < 0.0f ? 0.0f : (white > 1.0f ? 1.0f : white);
}

void Graphics::DrawRgbaQuad(const GpuTexVertex * quads, int quadCount, const float * mvp16, float blur)
{
    if (g_rgbaProg == 0 || !quads || quadCount <= 0 || !mvp16) return;

    RgbaCmd cmd;
    cmd.vb.resize((size_t)quadCount * 4 * 5);
    memcpy(cmd.vb.data(), quads, (size_t)quadCount * 4 * 5 * sizeof(float));
    cmd.ib.reserve((size_t)quadCount * 6);
    for (int q = 0; q < quadCount; ++q)
    {
        unsigned short base = (unsigned short)(q * 4);
        cmd.ib.push_back(base + 0); cmd.ib.push_back(base + 1); cmd.ib.push_back(base + 2);
        cmd.ib.push_back(base + 1); cmd.ib.push_back(base + 3); cmd.ib.push_back(base + 2);
    }
    memcpy(cmd.mvp, mvp16, sizeof(cmd.mvp));
    cmd.blur = blur;
    m_rgbaQuads.push_back(std::move(cmd));
}

void Graphics::MeshPalette(const unsigned char * vga768)
{
    if (!vga768) return;
    for (int i = 0; i < 256; i++)
    {
        g_meshPalRGBA[i * 4 + 0] = (unsigned char)(vga768[i * 3 + 0] * 4);
        g_meshPalRGBA[i * 4 + 1] = (unsigned char)(vga768[i * 3 + 1] * 4);
        g_meshPalRGBA[i * 4 + 2] = (unsigned char)(vga768[i * 3 + 2] * 4);
        g_meshPalRGBA[i * 4 + 3] = 255;
    }
}

void Graphics::MeshViewport(int x0, int y0, int x1, int y1)
{
    m_meshScissorOn = true;
    m_meshScissor[0] = x0;
    m_meshScissor[1] = y0;
    m_meshScissor[2] = x1;
    m_meshScissor[3] = y1;
}

float Graphics::WindowAspect() const
{
    // Vector text/overlays occupy the full window, so they use the real window
    // aspect regardless of the 4:3 letterbox applied to CPU bitmap content.
#ifndef __EMSCRIPTEN__
    if (g_windows)
    {
        const Uint32 wf = SDL_GetWindowFlags(g_windows);
        if (wf & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(g_windows), &dm) == 0 && dm.h > 0)
            {
                const float a = (float)dm.w / (float)dm.h;
                if (SDL_getenv("SR_DEBUG")) std::fprintf(stderr, "[ASPECT] fullscreen desktop %dx%d -> %.4f\n", dm.w, dm.h, a);
                return a;
            }
        }
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(g_windows, &w, &h);
        if (w > 0 && h > 0)
        {
            const float a = (float)w / (float)h;
            if (SDL_getenv("SR_DEBUG")) std::fprintf(stderr, "[ASPECT] drawable %dx%d -> %.4f\n", w, h, a);
            return a;
        }
    }
#endif
    return (g_height > 0.0f) ? (g_width / g_height) : (4.0f / 3.0f);
}

void Graphics::UpdateMeshPass()
{
    float vpX = 0.0f, vpY = 0.0f, vpW = g_width, vpH = g_height;
    if (g_respectRatio)
    {
        // DOS pixel aspect (320x200 shown 4:3) applied to the part's mode, so
        // doubled-height parts (e.g. 320x400 credits) letterbox correctly.
        // Use the logical visible height (200) for the ratio, not the physical
        // double-buffer height (400) which would produce a false portrait ratio.
        const float visH = (g_demoScreenHeight > SCREEN_HEIGHT) ? (float)SCREEN_HEIGHT : (float)g_demoScreenHeight;
        const float partRatio = (float)g_demoScreenWidth / visH;
        const float windowAspect = (g_height > 0.0f) ? (g_width / g_height) : partRatio;
        if (windowAspect > partRatio)
        {
            vpH = g_height;
            vpW = vpH * partRatio;
        }
        else
        {
            vpW = g_width;
            vpH = vpW / partRatio;
        }
        vpX = (g_width - vpW) * 0.5f;
        vpY = (g_height - vpH) * 0.5f;
    }
    glViewport(0, 0, g_width, g_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport((GLint)vpX, (GLint)vpY, (GLint)vpW, (GLint)vpH);

    if (m_meshScissorOn)
    {
        const float sx = vpW / (float)SCREEN_WIDTH;
        const float sy = vpH / (float)SCREEN_HEIGHT;
        GLint rx = (GLint)(vpX + m_meshScissor[0] * sx);
        GLint ry = (GLint)(vpY + (SCREEN_HEIGHT - 1 - m_meshScissor[3]) * sy);
        GLsizei rw = (GLsizei)((m_meshScissor[2] - m_meshScissor[0] + 1) * sx);
        GLsizei rh = (GLsizei)((m_meshScissor[3] - m_meshScissor[1] + 1) * sy);
        glEnable(GL_SCISSOR_TEST);
        glScissor(rx, ry, rw, rh);
    }

    // glClear respects the depth mask, so a false mask left behind by an
    // earlier pass (FlushExplosion) would silently skip the depth clear and
    // leave stale depth that occludes the next part's centre geometry.
    glDepthMask(GL_TRUE);
    glClearColor(m_meshClear[0], m_meshClear[1], m_meshClear[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Background layer, drawn before the screen composite so a part can put a
    // picture behind its cpuPixels content. A part cannot get this ordering from
    // demo_drawrgbaquad(): those quads are flushed after the SDF text, so they
    // land on top of it.
    const bool bgLayerActive = (m_bgLayer.tex != 0 && m_bgLayer.alpha > 0.0f);
    if (bgLayerActive)
    {
        // The mesh scissor rectangle is meant for the part's own composite and
        // can be smaller than the window; the layer covers the window, so the
        // scissor has to come off or the quad is clipped away to nothing.
        const bool hadScissor = m_meshScissorOn;
        if (hadScissor) glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, (GLint)g_width, (GLint)g_height);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        glUseProgram(m_bgLayer.prog);
        glUniform1f(m_bgLayer.locAlpha, m_bgLayer.alpha);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_bgLayer.tex);
        glUniform1i(m_bgLayer.locTex, 0);

        // The part supplies the NDC rectangle, so a picture wider than the window can
        // overflow it instead of being stretched to fit.
        const float POS[8] = {
            -m_bgLayer.halfW, -1.0f, m_bgLayer.halfW, -1.0f, -m_bgLayer.halfW, 1.0f, m_bgLayer.halfW, 1.0f,
        };
        // v runs 0 at the top of the window because NDC y is +1 there, while the
        // image's first row is v = 0, so the top edge takes v = 1.
        const float UV[8] = {
            m_bgLayer.u0, 1.0f, m_bgLayer.u1, 1.0f, m_bgLayer.u0, 0.0f, m_bgLayer.u1, 0.0f,
        };
        glEnableVertexAttribArray(m_bgLayer.locPos);
        glEnableVertexAttribArray(m_bgLayer.locUV);
        glVertexAttribPointer(m_bgLayer.locPos, 2, GL_FLOAT, GL_FALSE, 0, POS);
        glVertexAttribPointer(m_bgLayer.locUV, 2, GL_FLOAT, GL_FALSE, 0, UV);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(m_bgLayer.locPos);
        glDisableVertexAttribArray(m_bgLayer.locUV);

        glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
        if (hadScissor) glEnable(GL_SCISSOR_TEST);
    }

    if (m_meshBgScreen && !bgLayerActive)
    {
        bool hadScissor = m_meshScissorOn;
        if (hadScissor) glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, (GLint)g_width, (GLint)g_height);
        PrepareTextureForGPU();
        if (!g_xbrzBuffer.empty())
        {
            const int xw = g_demoScreenWidth * (int)g_xbrzFactor;
            const int xh = g_demoScreenHeight * (int)g_xbrzFactor;
            glBindTexture(GL_TEXTURE_2D, g_texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         xw, xh,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, g_xbrzBuffer.data());
        }
        else
        {
            UpdateTextureRGBA8(g_texture, VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, g_screen32.data());
        }
        float w, h;
        if (g_respectRatio)
        {
            const float visH = (g_demoScreenHeight > SCREEN_HEIGHT) ? (float)SCREEN_HEIGHT : (float)g_demoScreenHeight;
            const float partRatio = (g_demoScreenWidth * 5.0f) / (6.0f * visH);
            const float windowAspect = (g_height > 0.0f) ? (g_width / g_height) : partRatio;
            if (windowAspect > partRatio)
            {
                h = g_height;
                w = h * partRatio;
            }
            else
            {
                w = g_width;
                h = w / partRatio;
            }
        }
        else
        {
            w = g_width;
            h = g_height;
        }
        const float x = (g_width - w) * 0.5f;
        const float y = (g_height - h) * 0.5f;
        const GLfloat verts[] = {
            x, y, 0.0f, 0.0f,
            x, y + h, 0.0f, g_sourceHeight,
            x + w, y, g_sourceWidth, 0.0f,
            x + w, y + h, g_sourceWidth, g_sourceHeight,
        };
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glUseProgram(g_prog);
        glUniform3f(g_Loc_uResolution, g_width, g_height, g_jsss);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_texture);
        glBindSampler(0, g_nearestSampling);
        glBindSampler(1, g_nearestSampling);
        glUniform1i(g_Loc_uTex0, 0);
        glUniform1i(g_Loc_uTex1, 1);
        glEnableVertexAttribArray(g_Loc_aPos);
        glEnableVertexAttribArray(g_Loc_aUV);
        glVertexAttribPointer(g_Loc_aPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)0);
        glVertexAttribPointer(g_Loc_aUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(g_Loc_aPos);
        glDisableVertexAttribArray(g_Loc_aUV);
        glBindSampler(0, 0);
        glBindSampler(1, 0);
        glUseProgram(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        if (hadScissor)
        {
            const float sx = g_width / (float)SCREEN_WIDTH;
            const float sy = g_height / (float)SCREEN_HEIGHT;
            GLint rx = (GLint)(m_meshScissor[0] * sx);
            GLint ry = (GLint)(g_height - (m_meshScissor[3] + 1) * sy);
            GLsizei rw = (GLsizei)((m_meshScissor[2] - m_meshScissor[0] + 1) * sx);
            GLsizei rh = (GLsizei)((m_meshScissor[3] - m_meshScissor[1] + 1) * sy);
            glEnable(GL_SCISSOR_TEST);
            glScissor(rx, ry, rw, rh);
        }
    }

    // Vector content (3D meshes, tex meshes). When a screen background is
    // active (mesh background screen), the 3D shares the same 4:3 viewport as
    // the CPU bitmap — exactly like the original 320x200 framebuffer where
    // everything lived in one space. Otherwise it renders full-window.
    if (m_meshBgScreen)
    {
        float vpW2 = g_width, vpH2 = g_height;
        if (g_respectRatio)
        {
            const float visH = (g_demoScreenHeight > SCREEN_HEIGHT) ? (float)SCREEN_HEIGHT : (float)g_demoScreenHeight;
            const float partRatio = (g_demoScreenWidth * 5.0f) / (6.0f * visH);
            const float windowAspect = (g_height > 0.0f) ? (g_width / g_height) : partRatio;
            if (windowAspect > partRatio)
            {
                vpH2 = g_height;
                vpW2 = vpH2 * partRatio;
            }
            else
            {
                vpW2 = g_width;
                vpH2 = vpW2 / partRatio;
            }
        }
        const float vpX2 = (g_width - vpW2) * 0.5f;
        const float vpY2 = (g_height - vpH2) * 0.5f;
        glViewport((GLint)vpX2, (GLint)vpY2, (GLint)vpW2, (GLint)vpH2);
    }
    else
    {
        glViewport(0, 0, g_width, g_height);
    }
    if (m_meshScissorOn)
    {
        const float sx = g_width / (float)SCREEN_WIDTH;
        const float sy = g_height / (float)SCREEN_HEIGHT;
        GLint rx = (GLint)(m_meshScissor[0] * sx);
        GLint ry = (GLint)(g_height - (m_meshScissor[3] + 1) * sy);
        GLsizei rw = (GLsizei)((m_meshScissor[2] - m_meshScissor[0] + 1) * sx);
        GLsizei rh = (GLsizei)((m_meshScissor[3] - m_meshScissor[1] + 1) * sy);
        glEnable(GL_SCISSOR_TEST);
        glScissor(rx, ry, rw, rh);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);


    if (!m_texMeshes.empty())
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_texIdxTex);
        if (m_texData && m_texW > 0 && m_texH > 0)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_texW, m_texH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_texData);
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_meshPalTex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, g_meshPalRGBA);
        glActiveTexture(GL_TEXTURE0);

        glUseProgram(g_texProg);
        glUniform1i(g_texLocIdx, 0);
        glUniform1i(g_texLocPal, 1);
        glUniform1f(g_texLocWarpT, m_texTime);

        glBindBuffer(GL_ARRAY_BUFFER, g_texVBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_texIBO);

        const GLsizei texStride = 5 * sizeof(float);

        for (const TexMeshCmd &cmd : m_texMeshes)
        {
            glBufferData(GL_ARRAY_BUFFER, cmd.vb.size() * sizeof(float), cmd.vb.data(), GL_DYNAMIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd.ib.size() * sizeof(unsigned short), cmd.ib.data(), GL_DYNAMIC_DRAW);

            glUniformMatrix4fv(g_texLocMVP, 1, GL_FALSE, cmd.mvp);

            glEnableVertexAttribArray(g_texLocPos);
            glVertexAttribPointer(g_texLocPos, 3, GL_FLOAT, GL_FALSE, texStride, (const void *)0);
            glEnableVertexAttribArray(g_texLocUV);
            glVertexAttribPointer(g_texLocUV, 2, GL_FLOAT, GL_FALSE, texStride, (const void *)(3 * sizeof(float)));

            glDrawElements(GL_TRIANGLES, (GLsizei)cmd.ib.size(), GL_UNSIGNED_SHORT, nullptr);

            glDisableVertexAttribArray(g_texLocPos);
            glDisableVertexAttribArray(g_texLocUV);
        }

        m_texMeshes.clear();

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glUseProgram(0);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_meshPalTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, Shim::palette);

    glUseProgram(g_meshProg);
    glUniform1i(g_meshLocPalette, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_meshVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_meshIBO);

    const GLsizei stride = 8 * sizeof(float);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const MeshCmd &cmd : m_meshes)
    {
        glBufferData(GL_ARRAY_BUFFER, cmd.vb.size() * sizeof(float), cmd.vb.data(), GL_DYNAMIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd.ib.size() * sizeof(unsigned short), cmd.ib.data(), GL_DYNAMIC_DRAW);

        glUniformMatrix4fv(g_meshLocMVP, 1, GL_FALSE, cmd.mvp);
        glUniformMatrix4fv(g_meshLocModel, 1, GL_FALSE, cmd.model);
        glUniform1f(g_meshLocFade, cmd.fade);

        glEnableVertexAttribArray(g_meshLocPos);
        glVertexAttribPointer(g_meshLocPos, 3, GL_FLOAT, GL_FALSE, stride, (const void *)0);
        glEnableVertexAttribArray(g_meshLocNormal);
        glVertexAttribPointer(g_meshLocNormal, 3, GL_FLOAT, GL_FALSE, stride, (const void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(g_meshLocColorIdx);
        glVertexAttribPointer(g_meshLocColorIdx, 1, GL_FLOAT, GL_FALSE, stride, (const void *)(6 * sizeof(float)));
        glEnableVertexAttribArray(g_meshLocShadeShift);
        glVertexAttribPointer(g_meshLocShadeShift, 1, GL_FLOAT, GL_FALSE, stride, (const void *)(7 * sizeof(float)));

        glDrawElements(GL_TRIANGLES, (GLsizei)cmd.ib.size(), GL_UNSIGNED_SHORT, nullptr);

        glDisableVertexAttribArray(g_meshLocPos);
        glDisableVertexAttribArray(g_meshLocNormal);
        glDisableVertexAttribArray(g_meshLocColorIdx);
        glDisableVertexAttribArray(g_meshLocShadeShift);
    }

    m_meshes.clear();

    FlushSdfQuads();
    FlushRgbaQuads();
    FlushExplosion();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_SCISSOR_TEST);
    m_meshScissorOn = false;
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    if (g_captureRemaining > 0)
    {
        CaptureFrame(g_capturePath);
        --g_captureRemaining;
    }

    DrawFullscreenQuad(0.0f, 0.0f, 0.0f, g_exitFade);
    WriteFramesPipe();
    SDL_GL_SwapWindow(g_windows);
}

void Graphics::FlushSdfQuads()
{
    if (m_sdfQuads.empty()) return;

    // SDF quads are emitted in full-window NDC (aspect handled by the caller),
    // so force the full window viewport regardless of any mesh letterbox.
    glViewport(0, 0, g_width, g_height);
    glDisable(GL_SCISSOR_TEST);
    glBindSampler(0, 0);
    glBindSampler(1, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_sdfTex);
    // The overlay carries its own atlas: parts install their own charset and
    // would otherwise invalidate the UVs it computed. One SDF texture is active
    // at a time, so the overlay's is bound just before its flush and the part's
    // is restored for the next frame.
    const unsigned char * sdfData = m_overlaySdfData ? m_overlaySdfData : m_sdfData;
    const int sdfW = m_overlaySdfData ? m_overlaySdfW : m_sdfW;
    const int sdfH = m_overlaySdfData ? m_overlaySdfH : m_sdfH;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_sdfTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_sdfTexOverlay);
    glActiveTexture(GL_TEXTURE0);

    glUseProgram(g_sdfProg);
    glUniform1i(g_sdfLocSdf, 0);
    if (g_sdfLocSdfOverlay >= 0) glUniform1i(g_sdfLocSdfOverlay, 1);

    glBindBuffer(GL_ARRAY_BUFFER, g_sdfVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_sdfIBO);

    const GLsizei sdfStride = 5 * sizeof(float);
    glEnableVertexAttribArray(g_sdfLocPos);
    glVertexAttribPointer(g_sdfLocPos, 3, GL_FLOAT, GL_FALSE, sdfStride, (const void *)0);
    glEnableVertexAttribArray(g_sdfLocUV);
    glVertexAttribPointer(g_sdfLocUV, 2, GL_FLOAT, GL_FALSE, sdfStride, (const void *)(3 * sizeof(float)));

    for (const SdfCmd &cmd : m_sdfQuads)
    {
        glBufferData(GL_ARRAY_BUFFER, cmd.vb.size() * sizeof(float), cmd.vb.data(), GL_DYNAMIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd.ib.size() * sizeof(unsigned short), cmd.ib.data(), GL_DYNAMIC_DRAW);

        glUniformMatrix4fv(g_sdfLocMVP, 1, GL_FALSE, cmd.mvp);
        glUniform3f(g_sdfLocColor, cmd.color[0], cmd.color[1], cmd.color[2]);
        if (g_sdfLocSharp >= 0) glUniform1f(g_sdfLocSharp, cmd.sharp ? 1.0f : 0.0f);
        if (g_sdfLocUseOverlay >= 0) glUniform1f(g_sdfLocUseOverlay, cmd.useOverlay ? 1.0f : 0.0f);
        glDrawElements(GL_TRIANGLES, (GLsizei)cmd.ib.size(), GL_UNSIGNED_SHORT, nullptr);
    }

    glDisableVertexAttribArray(g_sdfLocPos);
    glDisableVertexAttribArray(g_sdfLocUV);
    m_sdfQuads.clear();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void Graphics::InitBackgroundLayer()
{
    const char * vs =
        "precision highp float;\n"
        "attribute vec2 aPos;\n"
        "attribute vec2 aUV;\n"
        "varying vec2 vUV;\n"
        "void main(){ vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
    const char * fs =
        "precision highp float;\n"
        "uniform sampler2D uTex;\n"
        "uniform float uAlpha;\n"
        "varying vec2 vUV;\n"
        "void main(){ gl_FragColor = vec4(texture2D(uTex, vUV).rgb, uAlpha); }\n";
    m_bgLayer.prog = linkProgram(compileShader(GL_VERTEX_SHADER, vs), compileShader(GL_FRAGMENT_SHADER, fs));
    m_bgLayer.locAlpha = glGetUniformLocation(m_bgLayer.prog, "uAlpha");
    m_bgLayer.locTex = glGetUniformLocation(m_bgLayer.prog, "uTex");
    m_bgLayer.locPos = glGetAttribLocation(m_bgLayer.prog, "aPos");
    m_bgLayer.locUV = glGetAttribLocation(m_bgLayer.prog, "aUV");
}

void Graphics::SetBackgroundLayer(const unsigned char * rgb, int w, int h)
{
    if (!rgb || w <= 0 || h <= 0) return;

    // Only re-upload when the source actually changed: this is a per-part
    // lifetime pointer, so a stable pointer means the pixels are already there.
    if (m_bgLayer.tex != 0 && rgb == m_bgLayer.data && w == m_bgLayer.w && h == m_bgLayer.h)
    {
        return;
    }

    m_bgLayer.data = rgb;
    m_bgLayer.w = w;
    m_bgLayer.h = h;

    if (m_bgLayer.tex == 0) glGenTextures(1, &m_bgLayer.tex);
    glBindTexture(GL_TEXTURE_2D, m_bgLayer.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Graphics::SetBackgroundLayerScroll(float u0, float u1, float alpha, float halfW)
{
    m_bgLayer.u0 = u0;
    m_bgLayer.u1 = u1;
    m_bgLayer.alpha = alpha;
    m_bgLayer.halfW = halfW;
}

void Graphics::ClearBackgroundLayer()
{
    m_bgLayer.alpha = 0.0f;
    m_bgLayer.data = nullptr;
}

void Graphics::SetBackgroundColor(float r, float g, float b)
{
    g_bgClear[0] = r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
    g_bgClear[1] = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g);
    g_bgClear[2] = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);
}

void Graphics::SetCompositeScale(float s)
{
    g_compositeScale = s < 0.0f ? 0.0f : s;
}

void Graphics::SetExitFade(float v)
{
    g_exitFade = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void Graphics::SetExplosionFlash(float v)
{
    m_explosionFlash = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void Graphics::FlushExplosion()
{
    ++g_explosionFrame;
    // The white-out flash is drawn by this function too, and a frame can carry
    // the flash with no torus at all (the ring faded to alpha 0 at the very end
    // of the blast). Returning here then skipped the flash and the screen went
    // black before the title's white.
    if (m_explosions.empty() && m_explosionFlash <= 0.0f) return;

    // Full window: the shockwave flies at the camera, so it must not be clipped
    // to a 4:3 strip while the hires picture behind it fills the screen.
    glViewport(0, 0, g_width, g_height);
    glDisable(GL_SCISSOR_TEST);
    // Seen edge-on the ring is a closed volume, so the near side has to hide the
    // far side or the tube shows through itself.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(g_torusProg);
    glBindBuffer(GL_ARRAY_BUFFER, g_torusVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_torusIBO);

    // 12 floats: pos3, nrm3, tube3, plasma2 (+1 pad to keep the stride 16B-friendly).
    const GLsizei stride = 12 * sizeof(float);
    glEnableVertexAttribArray(g_torusLocPos);
    glVertexAttribPointer(g_torusLocPos, 3, GL_FLOAT, GL_FALSE, stride, (const void *)0);
    glEnableVertexAttribArray(g_torusLocNrm);
    glVertexAttribPointer(g_torusLocNrm, 3, GL_FLOAT, GL_FALSE, stride, (const void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(g_torusLocTube);
    glVertexAttribPointer(g_torusLocTube, 3, GL_FLOAT, GL_FALSE, stride, (const void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(g_torusLocPlasma);
    glVertexAttribPointer(g_torusLocPlasma, 2, GL_FLOAT, GL_FALSE, stride, (const void *)(9 * sizeof(float)));

    // Orthographic on purpose, so clip.w is exactly 1. The previous matrix put
    // -0.02 into the w component, and since the torus lies flat (pz ~ 0) that
    // divided by nearly zero. "Toward the camera" is carried by uGrow opening
    // the ring past the window edges, not by a perspective divide.
    const float aspect = (g_height > 0.0f) ? (g_width / (float)g_height) : (4.0f / 3.0f);
    const float mvp[16] = {
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    glUniform1f(g_torusLocAspect, aspect);
    glUniformMatrix4fv(g_torusLocMVP, 1, GL_FALSE, mvp);
    // Must be cleared before the tori: it is left at 1 by a flash frame, and a
    // torus drawn with it on becomes a full-screen quad.
    glUniform1f(g_torusLocFlashOn, 0.0f);
    // Plasma Lab preset: sc=14, motion=2.3, intensity=0.2. uSpin carries the
    // time term and the source scales by 0.25*intensity inside
    // demosceneValue(), so scale and rate carry over.
    // These MUST be set. Left at their default 0 the sine field is flat and the
    // fireball renders as a plain radial gradient with no plasma at all.
    glUniform1f(g_torusLocPlasmaScale, 14.0f);
    // Upstream motionTime() multiplies SECONDS by uMotion. Here uSpin is a frame
    // counter, so the same rate has to be scaled by the nominal frame rate to
    // keep the same perceived speed: the fireball's spin advances 0.34 per
    // frame, and at 60fps that is 20.4 per second, so 2.3 / 20.4 = 0.113.
    // At the previous 0.16 against a raw frame counter the field advanced
    // 0.054 rad per frame, which measures as motion but is far too slow to read
    // as animation.
    // The fireball only lives 26 frames, so the field has to cross a good part
    // of a cycle inside that window. At 0.113 a frame advanced 0.0019 rad, and
    // the crest measured moved under a pixel per frame, which reads as static.
    // The clock advances 1/60 s per frame, so with motion 2.3 the field moved
    // 0.038 rad per frame, which measures as a sub-pixel drift and reads as
    // static. 21 gives ~0.35 rad per frame, about two degrees of phase: enough
    // to carry the crest visibly without strobing.
    glUniform1f(g_torusLocPlasmaMotion, 21.0f);
    glUniform1f(g_torusLocPlasmaIntensity, 1.0f);
    // Palette rotation, 49 entries per second as in the preset (spd=49); with
    // 192 entries on the wheel that is a full turn every ~3.9s. This rotation is
    // what makes the flames move: pinning it to 0 left the plasma structure
    // completely static, which is what was reported.
    // Driven by the frame counter rather than the wall clock so the effect stays
    // reproducible across runs and in captures. g_explosionFrame is incremented
    // once per flush, i.e. once per drawn frame.
    // Palette rotation. Preset spd=49 is 49 entries per second, which on a
    // 192-entry wheel is a full turn every 3.9s. The fireball only lives 26
    // frames, so that rate only advances 21 entries across its whole life, too
    // slow to carry the colour. Rotating through a full wheel over the blast is
    // what makes the flames read as burning rather than as a still gradient.
glUniform1f(g_torusLocPlasmaOffset, (float)g_explosionFrame * (192.0f / 54.0f) / 60.0f);
    // Fireball heat: 0 at ignition, rising so the core climbs the wheel toward
    // white-hot while the rim stays deep red.
    glUniform1f(g_torusLocPlasmaHeat, 0.34f);
    // Convection acceleration: the field speeds up and its detail grows as the
    // blast develops, which is what makes it read as boiling rather than drifting.
    glUniform1f(g_torusLocPlasmaBoil, 0.55f);
    // Ring: gas rotation around the tube.
    glUniform1f(g_torusLocPlasmaSwirl, 2.6f);
    glUniform1f(g_torusLocPlasmaFlow, 1.4f);
    // Plasma clock in seconds, matching Plasma Lab's animationTime. uSpin is a
    // frame counter whose range over the 26-frame fireball is under 9, which is
    // why the plasma read as static there.
    glUniform1f(g_torusLocPlasmaClock, (float)g_explosionFrame / 60.0f);
    // Vertex displacement + relief shading strength. 0 disables both, which is
    // the state the captures that established the plasma were taken in.
    glUniform1f(g_torusLocPlasmaRelief, 0.0f);

    for (const ExplosionCmd &cmd : m_explosions)
    {
        // The fireball is a flat billboard. If it writes depth, its whole
        // footprint kills the far half of the shockwave behind it and shows up
        // as a square (the quad) or a disc (its opaque part) on the ring. The
        // ring itself does need depth writes so its near side hides the far
        // side. The ball still reads as glowing through the ring's blended
        // far half without writing depth.
        glDepthMask(cmd.fire ? GL_FALSE : GL_TRUE);
        glBufferData(GL_ARRAY_BUFFER, cmd.vb.size() * sizeof(float), cmd.vb.data(), GL_DYNAMIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd.ib.size() * sizeof(unsigned short), cmd.ib.data(), GL_DYNAMIC_DRAW);
        glUniform1f(g_torusLocGrow, cmd.grow);
        glUniform1f(g_torusLocSpin, cmd.spin);
        glUniform1f(g_torusLocAlpha, cmd.alpha);
        glUniform1f(g_torusLocFireOn, cmd.fire ? 1.0f : 0.0f);
        // Both modes need it: the fireball positions the ball, the ring opens
        // from the same point. Gating this on cmd.fire left the ring at the
        // default (0,0) whenever the ball was not queued that frame.
        glUniform2f(g_torusLocCenter, cmd.centerX, cmd.centerY);
        glDrawElements(GL_TRIANGLES, (GLsizei)cmd.ib.size(), GL_UNSIGNED_SHORT, nullptr);
    }

    glDisableVertexAttribArray(g_torusLocPos);
    glDisableVertexAttribArray(g_torusLocNrm);
    glDisableVertexAttribArray(g_torusLocTube);
    glDisableVertexAttribArray(g_torusLocPlasma);
    m_explosions.clear();
    // Must not stay disabled: the next part's mesh pass clears depth with
    // glClear, which respects this mask, so a false mask skips the depth clear
    // and stale depth occludes its centre geometry (the PLZ cube, the U2E city).
    glDepthMask(GL_TRUE);

    // White-out last, so it covers the torus and the picture behind it.
    if (m_explosionFlash > 0.0f)
    {
        DrawFullscreenQuad(1.0f, 1.0f, 1.0f, m_explosionFlash);
        m_explosionFlash = 0.0f;
    }
    else
    {
        glUseProgram(g_torusProg);
        glUniform1f(g_torusLocFlashOn, 0.0f);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void Graphics::FlushRgbaQuads()
{
    if (m_rgbaQuads.empty()) return;

    glViewport(0, 0, g_width, g_height);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_rgbaTex);
    // What is already resident in g_rgbaTex, which is created once at init and
    // never deleted. A part that redraws the same picture every frame must not
    // re-upload it every frame: the hires troll is 24MB of RGB and glTexImage2D
    // per frame is 1.4GB/s across its 13 second sequence.
    // Deliberately function-local and not members: this class is sensitive to
    // member layout, and growing it in the middle shifts every later member,
    // which crashes ClearGpuEffect() on the next part change.
    static const unsigned char * upData = nullptr;
    static int upW = 0;
    static int upH = 0;
    static bool upRgb = false;

    // m_rgbaPerFrame says the caller's pixels change under a stable pointer, so
    // the pointer comparison above cannot detect it and would freeze the part.
    const bool mustUpload = m_rgbaPerFrame || upData != m_rgbaData || upW != m_rgbaW
                            || upH != m_rgbaH || upRgb != m_rgbaIsRgb;

    if (m_rgbaData && m_rgbaW > 0 && m_rgbaH > 0 && mustUpload)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, m_rgbaIsRgb ? GL_RGB : GL_RGBA,
                     m_rgbaW, m_rgbaH, 0, m_rgbaIsRgb ? GL_RGB : GL_RGBA,
                     GL_UNSIGNED_BYTE, m_rgbaData);
        upData = m_rgbaData;
        upW = m_rgbaW;
        upH = m_rgbaH;
        upRgb = m_rgbaIsRgb;
    }

    glUseProgram(g_rgbaProg);
    glUniform1i(g_rgbaLocTex, 0);
    glUniform2f(g_rgbaLocTexSize, (float)m_rgbaW, (float)m_rgbaH);
    glUniform1f(g_rgbaLocWhite, g_rgbaWhite);

    glBindBuffer(GL_ARRAY_BUFFER, g_rgbaVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_rgbaIBO);

    const GLsizei rgbaStride = 5 * sizeof(float);
    glEnableVertexAttribArray(g_rgbaLocPos);
    glVertexAttribPointer(g_rgbaLocPos, 3, GL_FLOAT, GL_FALSE, rgbaStride, (const void *)0);
    glEnableVertexAttribArray(g_rgbaLocUV);
    glVertexAttribPointer(g_rgbaLocUV, 2, GL_FLOAT, GL_FALSE, rgbaStride, (const void *)(3 * sizeof(float)));

    for (const RgbaCmd &cmd : m_rgbaQuads)
    {
        glBufferData(GL_ARRAY_BUFFER, cmd.vb.size() * sizeof(float), cmd.vb.data(), GL_DYNAMIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd.ib.size() * sizeof(unsigned short), cmd.ib.data(), GL_DYNAMIC_DRAW);
        glUniformMatrix4fv(g_rgbaLocMVP, 1, GL_FALSE, cmd.mvp);
        glUniform1f(g_rgbaLocBlur, cmd.blur);
        glDrawElements(GL_TRIANGLES, (GLsizei)cmd.ib.size(), GL_UNSIGNED_SHORT, nullptr);
    }

    glDisableVertexAttribArray(g_rgbaLocPos);
    glDisableVertexAttribArray(g_rgbaLocUV);
    m_rgbaQuads.clear();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}
