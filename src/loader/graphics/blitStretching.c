#ifndef __i386__
#define __i386__
#endif
#undef __x86_64__
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>

#include "blitStretching.h"
#include "../config/config.h"
#include "../log/log.h"
#include "../patching/patchResolution.h"
#ifdef _WIN32
#include "../gui/gui.h"
#include "../hardware/lindbergh/forceFeedback.h"
#include "../agent/agent.h"
void pollEvents();             // sdlCalls.h, which brings GL/gl.h with it
void segaapiPause(int paused); // libsegaapi.c
void alsaPause(int paused);    // alsa2sdl.c
#endif
// #include "glInitFunctions.h"

#define CHECK_GL(msg)                                                                                                                      \
    do                                                                                                                                     \
    {                                                                                                                                      \
        GLenum err = glad_glGetError();                                                                                                    \
        if (err != GL_NO_ERROR)                                                                                                            \
        {                                                                                                                                  \
            log_error("OpenGL Error in " msg ": 0x%04X", err);                                                                             \
        }                                                                                                                                  \
    } while (0)

// #ifdef _WIN32
// #include "../elfLoader/glhooks.hpp"
// #endif

extern uint32_t gId;
extern int gGrp;
extern int gWidth;
extern int gHeight;
extern SDL_Window *g_SdlWindow;

int blitWidth = 0;
int blitHeight = 0;
int renderWidth = 0;
int renderHeight = 0;

int gameIsOutrunChihiroMode = 0;

int fboInitialized = false;
GLuint fboId = 0;
GLuint fboTextureId = 0;
static GLuint fboDepthId = 0;

int blitOffscreen = 0;                        // the game draws in fboId instead of the window
static GLint gameDrawFbo = 0, gameReadFbo = 0; // the game's bindings, given back after the swap

int drawableW = 1;
int drawableH = 1;

Dest dest;

static int blitDisplayMode(void)
{
    int mode = getConfig()->displayMode;
    if (mode < 0) // only FULLSCREEN was given
        mode = getConfig()->fullscreen ? DISPLAY_FULLSCREEN : DISPLAY_WINDOW;
    return mode;
}

static bool blitStretched(void)
{
    int mode = blitDisplayMode();
    return mode == DISPLAY_WINDOW_STRETCH || mode == DISPLAY_FULLSCREEN_STRETCH;
}

// The frame's shape, for a game rendering renderW x renderH: the picture keeps its own shape inside it, fills it
// deformed or fills it cut, as the aspect ratio says (blitPresent)
static float blitDisplayAspect(int renderW, int renderH)
{
    switch (getConfig()->aspectRatio)
    {
        case ASPECT_4_3:
        case ASPECT_4_3_LETTERBOX:
        case ASPECT_4_3_STRETCH:
        case ASPECT_4_3_CROP:
            return 4.0f / 3.0f;
        case ASPECT_16_9:
        case ASPECT_16_9_STRETCH:
        case ASPECT_16_9_CROP:
            return 16.0f / 9.0f;
        default:
            if (gameIsOutrunChihiroMode)
                return 640.0f / 480.0f; // post-crop aspect (center 640px of 800-wide game)
            return (float)renderW / (float)renderH;
    }
}

void blitWindowedSize(int renderW, int renderH, int *w, int *h)
{
    *w = renderW;
    *h = renderH;
    int mode = blitDisplayMode();
    if (mode == DISPLAY_WINDOW)
    {
        // Widen or heighten the window to the aspect ratio, never below the render size: the game draws its frame
        // in the window's default framebuffer, which must hold it whole.
        float aspect = blitDisplayAspect(renderW, renderH);
        if (aspect > (float)renderW / (float)renderH)
            *w = (int)((float)renderH * aspect + 0.5f);
        else
            *h = (int)((float)renderW / aspect + 0.5f);
    }
#ifdef _WIN32
    if (mode != DISPLAY_WINDOW && mode != DISPLAY_WINDOW_STRETCH)
        return;
    // A window larger than the screen's usable area is shrunk to it, keeping its shape: the game then draws
    // offscreen, at its own size (see blitStretch)
    SDL_Rect usable;
    SDL_DisplayID display = g_SdlWindow ? SDL_GetDisplayForWindow(g_SdlWindow) : SDL_GetPrimaryDisplay();
    if (display && SDL_GetDisplayUsableBounds(display, &usable))
    {
        int top = 0, left = 0, bottom = 0, right = 0;
        if (g_SdlWindow)
            SDL_GetWindowBordersSize(g_SdlWindow, &top, &left, &bottom, &right);
        int maxW = usable.w - left - right;
        int maxH = usable.h - top - bottom;
        if (maxW > 0 && maxH > 0 && (*w > maxW || *h > maxH))
        {
            float scale = SDL_min((float)maxW / (float)*w, (float)maxH / (float)*h);
            *w = (int)((float)*w * scale);
            *h = (int)((float)*h * scale);
        }
    }
#endif
}

void initBlitting()
{
    blitSetWidthandHeightSize();
    blitInitializeFbo();
}

void blitSetWidthandHeightSize()
{
    if (gGrp == GROUP_ID4_EXP || gGrp == GROUP_ID4_JAP || gGrp == GROUP_ID5)
    {
        if (isTestMode())
        {
            blitWidth = 1360;
            blitHeight = 768;
        }
        else
        {
            blitWidth = gWidth;
            blitHeight = gHeight;
        }
    }
    else if (gId == GHOST_SQUAD_EVOLUTION_SBNJ || gGrp == GROUP_VT3_TEST)
    {
        blitWidth = 640;
        blitHeight = 480;
    }
    else if (gId == QUIZ_AXA_SBMS || gId == QUIZ_AXA_SBUR_LIVE || gId == MJ4_SBPN_REVG || gId == MJ4_EVO_SBTA)
    {
        blitWidth = 1024;
        blitHeight = 768;
    }
    else
    {
        blitWidth = gWidth;
        blitHeight = gHeight;
    }

    renderWidth = blitWidth;
    renderHeight = blitHeight;
}

int blitInitializeFbo()
{
    fboId = 0; // Will be lazily initialized in blitStretch on the correct context

    glad_glGenTextures(1, &fboTextureId);
    if (fboTextureId == 0)
    {
        log_error("Failed to generate FBO texture.\n");
        return 0;
    }

    GLint maxTexSize = 0;
    glad_glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if (blitWidth > maxTexSize || blitHeight > maxTexSize)
    {
        log_error("FBO texture size %dx%d exceeds GL_MAX_TEXTURE_SIZE %d! SSAA may be silently clamped.",
                  blitWidth, blitHeight, maxTexSize);
    }

    glad_glBindTexture(GL_TEXTURE_2D, fboTextureId);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, blitWidth, blitHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glad_glBindTexture(GL_TEXTURE_2D, 0);

    fboInitialized = true;
    return 1;
}

// CROP_BARREL: the cropped aspect ratios keep the frame's middle (BARREL_CENTER of it) as the crop shows it, and bend
// the parts the crop would cut onto the frame's edges, as on a cylinder seen from the front: a point at the arc s
// from the middle's edge shows at sin(s / R), R the cylinder's radius (BARREL_RADIUS), scaled so that the picture's
// edge lands on the frame's; a quarter turn at most. Along the cropped axis, in frames: the crop keeps a share k of
// the picture, which is 1 / k frames long; d, a point of the frame from 0 to 1, shows the picture at barrelSource(d)
typedef struct
{
    double k;          // the share of the picture the crop keeps
    double edge;       // each edge's width, in frames
    double startLeft;  // where the middle starts in the picture, from 0 to 1 (left or top)
    double startRight; // where it ends
    double arcLeft;    // the picture beyond the middle on each side, in frames
    double arcRight;
    double angleLeft;  // the turn of the cylinder each side's picture takes, in radians
    double angleRight;
} Barrel;

static Barrel barrelSetup(double k, double kept)
{
    Barrel b;
    double center = getConfig()->barrelCenter / 100.0, radius = getConfig()->barrelRadius / 100.0;
    double start = (1.0 - k) * kept; // the crop's window in the picture
    b.k = k;
    b.edge = (1.0 - center) / 2.0;
    b.startLeft = start + k * (1.0 - center) / 2.0;
    b.startRight = start + k * (1.0 + center) / 2.0;
    b.arcLeft = b.startLeft / k;
    b.arcRight = (1.0 - b.startRight) / k;
    // An edge with more picture than room bends; with less it only stretches
    b.angleLeft = b.arcLeft > b.edge ? fmin(b.arcLeft / radius, M_PI / 2.0) : 0.0;
    b.angleRight = b.arcRight > b.edge ? fmin(b.arcRight / radius, M_PI / 2.0) : 0.0;
    return b;
}

// x from the middle's edge outward, 0 to the edge's width: the arc of picture shown there
static double barrelArc(double x, double edge, double arc, double angle)
{
    if (edge <= 0.0)
        return 0.0;
    if (angle < 1e-4)
        return arc * x / edge;
    return arc * asin(sin(angle) * x / edge) / angle;
}

static double barrelSource(const Barrel *b, double d)
{
    if (d < b->edge)
        return b->startLeft - b->k * barrelArc(b->edge - d, b->edge, b->arcLeft, b->angleLeft);
    if (d > 1.0 - b->edge)
        return b->startRight + b->k * barrelArc(d - (1.0 - b->edge), b->edge, b->arcRight, b->angleRight);
    return b->startLeft + b->k * (d - b->edge);
}

// The picture drawn in a strip of textured quads, finer on the edges, into dest; the game's GL state kept around it
// (its programs, ARB and NV ones included, would otherwise draw the strip)
static void blitPresentBarrel(const Barrel *b, bool horizontal, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1)
{
    GLint program = 0, activeTexture = GL_TEXTURE0;
    if (glad_glUseProgram)
        glad_glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glad_glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glad_glPushAttrib(GL_ALL_ATTRIB_BITS);
    glad_glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glad_glMatrixMode(GL_TEXTURE);
    glad_glPushMatrix();
    glad_glLoadIdentity();
    glad_glMatrixMode(GL_PROJECTION);
    glad_glPushMatrix();
    glad_glLoadIdentity();
    glad_glOrtho(0.0, drawableW, 0.0, drawableH, -1.0, 1.0);
    glad_glMatrixMode(GL_MODELVIEW);
    glad_glPushMatrix();
    glad_glLoadIdentity();

    if (glad_glUseProgram)
        glad_glUseProgram(0);
    glad_glActiveTexture(GL_TEXTURE0);
    const GLenum off[] = {GL_DEPTH_TEST, GL_STENCIL_TEST, GL_BLEND, GL_ALPHA_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_LIGHTING,
                          GL_FOG, GL_TEXTURE_GEN_S, GL_TEXTURE_GEN_T, GL_TEXTURE_GEN_R, GL_TEXTURE_GEN_Q, GL_COLOR_LOGIC_OP};
    for (size_t i = 0; i < sizeof(off) / sizeof(off[0]); i++)
        glad_glDisable(off[i]);
    if (GLAD_GL_ARB_vertex_program)
        glad_glDisable(GL_VERTEX_PROGRAM_ARB);
    if (GLAD_GL_ARB_fragment_program)
        glad_glDisable(GL_FRAGMENT_PROGRAM_ARB);
    if (GLAD_GL_NV_register_combiners)
        glad_glDisable(GL_REGISTER_COMBINERS_NV);
    if (GLAD_GL_NV_texture_shader)
        glad_glDisable(GL_TEXTURE_SHADER_NV);
    if (GLAD_GL_NV_vertex_program)
        glad_glDisable(GL_VERTEX_PROGRAM_NV);
    glad_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glad_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glad_glViewport(0, 0, drawableW, drawableH);
    glad_glEnable(GL_TEXTURE_2D);
    glad_glBindTexture(GL_TEXTURE_2D, fboTextureId);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glad_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glad_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // The frame's points: each edge in 64 steps, the middle straight
    enum
    {
        STEPS = 64
    };
    double points[2 * (STEPS + 1)];
    int count = 0;
    for (int i = 0; i <= STEPS; i++)
        points[count++] = b->edge * i / STEPS;
    for (int i = 0; i <= STEPS; i++)
        points[count++] = 1.0 - b->edge + b->edge * i / STEPS;
    double u0 = (double)srcX0 / blitWidth, u1 = (double)srcX1 / blitWidth;
    double v0 = (double)srcY0 / blitHeight, v1 = (double)srcY1 / blitHeight;
    glad_glBegin(GL_QUAD_STRIP);
    for (int i = 0; i < count; i++)
    {
        double source = barrelSource(b, points[i]);
        if (horizontal) // left to right
        {
            double u = u0 + source * (u1 - u0), x = dest.X + points[i] * dest.W;
            glad_glTexCoord2d(u, v0);
            glad_glVertex2d(x, dest.Y);
            glad_glTexCoord2d(u, v1);
            glad_glVertex2d(x, dest.Y + dest.H);
        }
        else // top to bottom, the framebuffer's rows bottom up
        {
            double v = v1 - source * (v1 - v0), y = dest.Y + dest.H - points[i] * dest.H;
            glad_glTexCoord2d(u0, v);
            glad_glVertex2d(dest.X, y);
            glad_glTexCoord2d(u1, v);
            glad_glVertex2d(dest.X + dest.W, y);
        }
    }
    glad_glEnd();

    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glad_glMatrixMode(GL_MODELVIEW);
    glad_glPopMatrix();
    glad_glMatrixMode(GL_PROJECTION);
    glad_glPopMatrix();
    glad_glMatrixMode(GL_TEXTURE);
    glad_glPopMatrix();
    glad_glPopClientAttrib();
    glad_glPopAttrib();
    if (glad_glUseProgram)
        glad_glUseProgram(program);
    glad_glActiveTexture(activeTexture);
}

// Second blit: the game's picture, kept in fboId, scaled into the window's back buffer
static void blitPresent(void)
{
    float gameAspect = blitDisplayAspect(blitWidth, blitHeight);
    float windowAspect = (float)drawableW / (float)drawableH;

    // Reset dest each frame to prevent stale offsets after window resize / fullscreen toggle
    dest.X = 0;
    dest.Y = 0;
    dest.W = drawableW;
    dest.H = drawableH;

    if (blitStretched())
    {
        // the picture fills the window
    }
    else if (windowAspect > gameAspect)
    {
        dest.W = (GLsizei)(drawableH * gameAspect);
        dest.X = (drawableW - dest.W) / 2;
    }
    else if (windowAspect < gameAspect)
    {
        dest.H = (GLsizei)(drawableW / gameAspect);
        dest.Y = (drawableH - dest.H) / 2;
    }

    // The picture in the frame (the frame placed above, fitted or stretched as the display mode wants; only the
    // *_STRETCH modes stretch it). 4:3 and 16:9, as in windowsloader: the picture takes its share of the frame at its
    // own shape, with black bars where the shapes differ; shares of the frame rather than pixels, so that a frame
    // stretched onto a 4:3 screen (a 16:9 signal on a 4:3 monitor) shows the picture at its true shape (4:3_LETTERBOX
    // is 4:3). 4:3_STRETCH and 16:9_STRETCH: the picture fills the frame, deformed. 4:3_CROP and 16:9_CROP: it fills
    // the frame at its shape, the part that overflows cut, the part kept placed by CROP_POSITION.
    int aspectRatio = getConfig()->aspectRatio;
    GLint srcX0 = gameIsOutrunChihiroMode ? 80 : 0, srcY0 = 0; // Chihiro's 640 px in the middle of its 800
    GLint srcX1 = gameIsOutrunChihiroMode ? 720 : blitWidth, srcY1 = blitHeight;
    float frameAspect = blitDisplayAspect(blitWidth, blitHeight);
    float pictureAspect = (float)(srcX1 - srcX0) / (float)(srcY1 - srcY0);
    if (aspectRatio == ASPECT_4_3 || aspectRatio == ASPECT_4_3_LETTERBOX || aspectRatio == ASPECT_16_9)
    {
        if (pictureAspect > frameAspect)
        {
            GLsizei h = (GLsizei)((float)dest.H * frameAspect / pictureAspect + 0.5f);
            dest.Y += (dest.H - h) / 2;
            dest.H = h;
        }
        else if (pictureAspect < frameAspect)
        {
            GLsizei w = (GLsizei)((float)dest.W * pictureAspect / frameAspect + 0.5f);
            dest.X += (dest.W - w) / 2;
            dest.W = w;
        }
    }
    bool barrel = false, barrelHorizontal = true;
    Barrel bend;
    if ((aspectRatio == ASPECT_4_3_CROP || aspectRatio == ASPECT_16_9_CROP) && getConfig()->cropBarrel &&
        pictureAspect != frameAspect)
    {
        // The whole picture, its cut parts bent onto the edges (blitPresentBarrel)
        double kept = (getConfig()->cropPosition + 100) / 200.0;
        barrel = true;
        barrelHorizontal = pictureAspect > frameAspect;
        bend = barrelSetup(barrelHorizontal ? frameAspect / pictureAspect : pictureAspect / frameAspect, kept);
    }
    else if (aspectRatio == ASPECT_4_3_CROP || aspectRatio == ASPECT_16_9_CROP)
    {
        float kept = (getConfig()->cropPosition + 100) / 200.0f; // 0 left or top, 1 right or bottom
        if (pictureAspect > frameAspect)                           // too wide: the sides go
        {
            GLint w = (GLint)((float)(srcY1 - srcY0) * frameAspect + 0.5f);
            srcX0 += (GLint)((float)(srcX1 - srcX0 - w) * kept + 0.5f);
            srcX1 = srcX0 + w;
        }
        else if (pictureAspect < frameAspect) // too tall: above and below go (the framebuffer's rows bottom up)
        {
            GLint h = (GLint)((float)(srcX1 - srcX0) / frameAspect + 0.5f);
            srcY0 += (GLint)((float)(srcY1 - srcY0 - h) * (1.0f - kept) + 0.5f);
            srcY1 = srcY0 + h;
        }
    }

    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK_GL("bind default framebuffer");

    glad_glDrawBuffer(GL_BACK); // Default FBO usually draws to GL_BACK
    CHECK_GL("drawbuffer GL_BACK");

    glad_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT);
    CHECK_GL("glClear on default framebuffer");

    // Non-MSAA path: direct scaled blit fboId → FB 0
    glad_glBindFramebuffer(GL_FRAMEBUFFER, fboId);
    CHECK_GL("bind GL_FRAMEBUFFER fboId second blit");
    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // We want to draw to 0
    CHECK_GL("bind DRAW 0 second blit");

    if (barrel)
    {
        blitPresentBarrel(&bend, barrelHorizontal, srcX0, srcY0, srcX1, srcY1);
        CHECK_GL("bent picture to default framebuffer");
        return;
    }
    glad_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dest.X, dest.Y, dest.X + dest.W, dest.Y + dest.H, GL_COLOR_BUFFER_BIT,
                           GL_LINEAR);
    CHECK_GL("second blit (GL_LINEAR scaled) to default framebuffer");
}

// The agent's `screen`: the picture placed in the window as the display settings want, in the window's back buffer
void blitAgentPresent(void)
{
    SDL_GetWindowSizeInPixels(g_SdlWindow, &drawableW, &drawableH);
    blitPresent();
}

#ifdef _WIN32
// F10: the game's thread waits here, inside its buffer swap, while the settings are open. The window shows the
// game's last picture with the settings over it; its sound and the wheel's forces stop meanwhile.
static void blitSettingsPause(void)
{
    sdlFfbPause(1);
    segaapiPause(1);
    alsaPause(1);
    while (guiOverlayVisible())
    {
        Uint64 start = SDL_GetTicks();
        SDL_GetWindowSizeInPixels(g_SdlWindow, &drawableW, &drawableH);
        blitPresent();
        guiOverlayRender(drawableW, drawableH);
        SDL_GL_SwapWindow(g_SdlWindow);
        pollEvents();
        // About 60 frames per second, whether the swap waits for the vertical sync or not
        Uint64 spent = SDL_GetTicks() - start;
        if (spent < 16)
            SDL_Delay((Uint32)(16 - spent));
    }
    alsaPause(0);
    segaapiPause(0);
    sdlFfbPause(0);
    // The frame the game is swapping: its own picture again, without the settings
    SDL_GetWindowSizeInPixels(g_SdlWindow, &drawableW, &drawableH);
    blitPresent();
}
#endif

void blitStretch()
{
    if (!fboInitialized)
        return;

    // Lazily initialize FBO on the correct thread/context
    if (fboId == 0 && fboTextureId > 0)
    {
        glad_glGenFramebuffers(1, &fboId);
        if (fboId != 0)
        {
            glad_glBindFramebuffer(GL_FRAMEBUFFER, fboId);
            glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTextureId, 0);
            // Depth and stencil, for the game when it draws here (offscreen rendering)
            glad_glGenRenderbuffers(1, &fboDepthId);
            glad_glBindRenderbuffer(GL_RENDERBUFFER, fboDepthId);
            glad_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, blitWidth, blitHeight);
            glad_glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glad_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fboDepthId);

            GLenum status = glad_glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                log_error("FBO is not complete upon lazy init! Status: 0x%x\n", status);
            }
            glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    if (fboInitialized && fboId > 0 && fboTextureId > 0 && g_SdlWindow)
    {
        while (glad_glGetError() != GL_NO_ERROR)
            ; // clear previous errors

        static int firstTime = 1;
        if (firstTime)
        {
            // For Chihiro mode, don't resize window (hook redirects FB0 to FBO)
            if (!gameIsOutrunChihiroMode)
            {
                int windowW, windowH;
                blitWindowedSize(gWidth, gHeight, &windowW, &windowH);
                SDL_SetWindowSize(g_SdlWindow, windowW, windowH);
            }
            firstTime = 0;
        }
        SDL_GetWindowSizeInPixels(g_SdlWindow, &drawableW, &drawableH);
#ifdef _WIN32
        // The game's picture larger than the window (a window bigger than the screen shrunk to it, a smaller window,
        // OutRun's Chihiro mode): from now on the game draws in fboId at its own size, through the redirections of
        // glHooks, and the window shows it scaled
        if (!blitOffscreen && (gameIsOutrunChihiroMode || drawableW < renderWidth || drawableH < renderHeight || agentHidden()))
        {
            blitOffscreen = 1;
            log_info("Offscreen rendering: game %dx%d, window %dx%d", renderWidth, renderHeight, drawableW, drawableH);
        }
#endif

        // SAVE STATE
        GLint oldScissorTest = 0;
        GLint oldScissorBox[4];
        GLboolean oldColorMask[4];
        GLint oldDrawFbo = 0, oldReadFbo = 0;
        GLfloat oldClearColor[4];
        GLint oldReadBuffer = GL_BACK, oldDrawBuffer = GL_BACK;

        glad_glGetIntegerv(GL_SCISSOR_TEST, &oldScissorTest);
        CHECK_GL("glGetIntegerv GL_SCISSOR_TEST");
        glad_glGetIntegerv(GL_SCISSOR_BOX, oldScissorBox);
        CHECK_GL("glGetIntegerv GL_SCISSOR_BOX");
        glad_glGetBooleanv(GL_COLOR_WRITEMASK, oldColorMask);
        CHECK_GL("glGetBooleanv GL_COLOR_WRITEMASK");
        glad_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFbo);
        CHECK_GL("glGetIntegerv GL_DRAW_FRAMEBUFFER_BINDING");
        glad_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFbo);
        CHECK_GL("glGetIntegerv GL_READ_FRAMEBUFFER_BINDING");
        glad_glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClearColor);
        CHECK_GL("glGetFloatv GL_COLOR_CLEAR_VALUE");

        // FORCE STATE FOR FULLSCREEN BLITS
        glad_glDisable(GL_SCISSOR_TEST);
        CHECK_GL("glDisable GL_SCISSOR_TEST");
        glad_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        CHECK_GL("glColorMask");

        if (glad_glIsFramebuffer && !glad_glIsFramebuffer(fboId))
        {
            log_error("fboId %u is NOT a valid framebuffer! Context changed?", fboId);
            // Optionally force recreation
            fboId = 0;
            glad_glGenFramebuffers(1, &fboId);
            glad_glBindFramebuffer(GL_FRAMEBUFFER, fboId);
            glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTextureId, 0);
            glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // First blit: copy from framebuffer to FBO
        // Offscreen (Chihiro mode included), the game rendered to our FBO through the glHooks redirections, so skip
        if (!blitOffscreen)
        {
                // Non-MSAA path: direct blit from FB0 → fboId
                glad_glBindFramebuffer(GL_FRAMEBUFFER, fboId);
                CHECK_GL("bind GL_FRAMEBUFFER fboId first blit");
                glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                CHECK_GL("bind GL_READ_FRAMEBUFFER 0 first blit");

                glad_glReadBuffer(GL_BACK);
                CHECK_GL("readbuffer GL_BACK");

                // Source: read from FB0 at game render resolution (pre-SSAA).
                // Destination: write to FBO at SSAA resolution (upscale with GL_LINEAR).
                // When SSAA is off, renderWidth==blitWidth and this is a 1:1 copy.
                glad_glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, blitWidth, blitHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                CHECK_GL("first blit to fboId");
        }

        // The agent mode: the frame, drawn in fboId, read and stepped by the agent (agent.c)
        if (agentActive())
            agentFrame(fboId, blitWidth, blitHeight);

        blitPresent();
#ifdef _WIN32
        if (guiOverlayVisible())
            blitSettingsPause();
#endif

        // RESTORE STATE
        if (oldScissorTest)
            glad_glEnable(GL_SCISSOR_TEST);
        glad_glScissor(oldScissorBox[0], oldScissorBox[1], oldScissorBox[2], oldScissorBox[3]);
        glad_glColorMask(oldColorMask[0], oldColorMask[1], oldColorMask[2], oldColorMask[3]);

        if (blitOffscreen)
        {
            // The window stays bound for the overlays drawn before the swap; blitAfterSwap gives the game its own back
            gameDrawFbo = oldDrawFbo;
            gameReadFbo = oldReadFbo;
            glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        else
        {
            glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFbo);
            if (oldReadFbo == 0)
            {
                glad_glReadBuffer(oldReadBuffer);
            } // Can only restore safely if it's default

            glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFbo);
            if (oldDrawFbo == 0)
            {
                glad_glDrawBuffer(oldDrawBuffer);
            }
        }

        glad_glClearColor(oldClearColor[0], oldClearColor[1], oldClearColor[2], oldClearColor[3]);
    }
}

// After the swap, offscreen: the game draws again in what it had bound, fboId for the window
void blitAfterSwap(void)
{
    if (!blitOffscreen || !fboId)
        return;
    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gameDrawFbo ? (GLuint)gameDrawFbo : fboId);
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, gameReadFbo ? (GLuint)gameReadFbo : fboId);
}

// glBindFramebuffer(EXT) from the game, offscreen: the window (0) is fboId
unsigned int blitRedirectFramebuffer(unsigned int framebuffer)
{
    return (framebuffer == 0 && blitOffscreen && fboId) ? fboId : framebuffer;
}

// glDrawBuffer / glReadBuffer from the game, offscreen: the window's buffers are fboId's colour attachment
unsigned int blitRedirectBuffer(unsigned int mode, int read)
{
    if (!blitOffscreen || !fboId)
        return mode;
    GLint bound = 0;
    glad_glGetIntegerv(read ? GL_READ_FRAMEBUFFER_BINDING : GL_DRAW_FRAMEBUFFER_BINDING, &bound);
    if ((GLuint)bound != fboId)
        return mode;
    switch (mode)
    {
        case GL_FRONT:
        case GL_BACK:
        case GL_LEFT:
        case GL_FRONT_LEFT:
        case GL_BACK_LEFT:
        case GL_FRONT_AND_BACK:
            return GL_COLOR_ATTACHMENT0;
        default:
            return mode;
    }
}