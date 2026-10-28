#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    int W;
    int H;
    int X;
    int Y;
} Dest;

extern Dest dest;
extern int drawableW;
extern int drawableH;
extern int blitWidth;
extern int blitHeight;
extern int renderWidth;
extern int renderHeight;
extern int fboInitialized;
extern unsigned int fboId;
extern int gameIsOutrunChihiroMode;

void initBlitting();
void blitSetWidthandHeightSize();
int blitInitializeFbo();
void blitStretch();
void blitWindowToRender(float windowX, float windowY, float *renderX, float *renderY);
void blitRenderToWindow(float renderX, float renderY, float *windowX, float *windowY);
// Window size for a game rendering renderW x renderH, in the windowed display mode at the configured aspect ratio
void blitWindowedSize(int renderW, int renderH, int *w, int *h);

// Offscreen rendering (Windows): when the game's picture does not fit the window, the game draws in fboId at its own
// size; glHooks redirect its framebuffer 0 and window buffers there, and the swap gives it its binding back.
extern int blitOffscreen;
unsigned int blitRedirectFramebuffer(unsigned int framebuffer);
unsigned int blitRedirectBuffer(unsigned int mode, int read);
void blitAfterSwap(void);
void blitAgentPresent(void); // the agent's `screen`

#ifdef __cplusplus
}
#endif
