#pragma once

// Dear ImGui interface of the Windows loader: the game picker shown when no game is given, and the Tab settings
// window drawn over a running game. Its layout follows sm2-emu's interface (https://github.com/dmanlfc/sm2-emu).

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // In-game overlay: call after the game's GL context is created.
    void guiOverlayInit(SDL_Window *window);
    // Non-zero when the event belongs to the overlay (Tab, or input while it is shown) and must not reach the game.
    int guiOverlayProcessEvent(const SDL_Event *event);
    // Draws the overlay into the bound framebuffer, of width x height pixels. Call after the frame is presented
    // to the window's back buffer, before the swap.
    void guiOverlayRender(int width, int height);
    int guiOverlayVisible(void);
    // The agent's "menu outputs on|off", as the settings' box: the network outputs started or stopped at once and the
    // choice saved. status: their state and where it was saved, on one line. Non-zero when the server is as asked
    int guiOutputsSwitch(int on, char *status, size_t size);

    // The game picker, run instead of a game when linuxloader starts without one. Returns the exit code.
    int guiRunPicker(const char *programPath);

    // Applies ASPECT_RATIO and DISPLAY_MODE to the running game's window (sdlCalls.c).
    void sdlApplyDisplaySettings(void);

#ifdef __cplusplus
}
#endif
