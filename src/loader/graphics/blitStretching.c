#ifndef __i386__
#define __i386__
#endif
#undef __x86_64__
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <stdbool.h>

#include "blitStretching.h"
#include "../config/config.h"
#include "../log/log.h"
#include "../patching/patchResolution.h"
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

// The shape the picture is shown with, for a game rendering renderW x renderH
static float blitDisplayAspect(int renderW, int renderH)
{
    switch (getConfig()->aspectRatio)
    {
        case ASPECT_4_3:
        case ASPECT_4_3_LETTERBOX: // the frame; the picture keeps its shape inside it (blitPresent)
            return 4.0f / 3.0f;
        case ASPECT_16_9:
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

    // 4:3 with black bars: the 4:3 frame is placed above, fitted or stretched as the mode wants; the picture takes
    // its share of it at its own shape. Shares of the frame rather than pixels, so that a frame stretched onto a 4:3
    // screen (a 16:9 signal on a 4:3 monitor) shows the picture at its true shape.
    if (getConfig()->aspectRatio == ASPECT_4_3_LETTERBOX)
    {
        float frameAspect = 4.0f / 3.0f;
        float pictureAspect = gameIsOutrunChihiroMode ? 640.0f / 480.0f : (float)blitWidth / (float)blitHeight;
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

    if (gameIsOutrunChihiroMode)
    {
        // Crop center 640px from 800-wide FBO: remove 80px left, 80px right
        glad_glBlitFramebuffer(80, 0, 720, blitHeight, dest.X, dest.Y, dest.X + dest.W, dest.Y + dest.H, GL_COLOR_BUFFER_BIT,
                               GL_LINEAR);
    }
    else
    {
        glad_glBlitFramebuffer(0, 0, blitWidth, blitHeight, dest.X, dest.Y, dest.X + dest.W, dest.Y + dest.H, GL_COLOR_BUFFER_BIT,
                               GL_LINEAR);
    }
    CHECK_GL("second blit (GL_LINEAR scaled) to default framebuffer");
}

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
        if (!blitOffscreen && (gameIsOutrunChihiroMode || drawableW < renderWidth || drawableH < renderHeight))
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


        blitPresent();

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