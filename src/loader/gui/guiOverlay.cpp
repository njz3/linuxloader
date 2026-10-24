// The Tab settings window over a running game.

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include <string>

#include "../../imgui/imgui.h"

#include "gui.h"
#include "guiCommon.h"
#include "../config/config.h"
#include "../mainShared.h"

namespace
{
bool g_ready = false;
bool g_open = false;
std::string g_configFile;

void setOpen(bool open)
{
    g_open = open;
    // The game may hide the pointer; the settings need it
    if (open)
        SDL_ShowCursor();
    else if (getConfig()->hideCursor)
        SDL_HideCursor();
}
} // namespace

extern "C" void guiOverlayInit(SDL_Window *window)
{
    g_ready = gui::initContext(window, getConfig()->language);
    const char *used = getConfigFilePath();
    g_configFile = used[0] ? used : (configPath && configPath[0]) ? configPath : "linuxloader.ini";
    gui::setControlsFile((controlsPath && controlsPath[0]) ? controlsPath : "controls.ini");
}

extern "C" int guiOverlayProcessEvent(const SDL_Event *event)
{
    if (!g_ready)
        return 0;
    // Tab, unless [Launcher] SETTINGS_MENU = false (a public cabinet): then the key goes to the game
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_TAB && getConfig()->settingsMenu)
    {
        if (!event->key.repeat)
            setOpen(!g_open);
        return 1;
    }
    if (!g_open)
        return 0;

    if (gui::controlsCaptureEvent(event)) // an input being bound: not for the game
        return 1;
    gui::processEvent(event);
    switch (event->type)
    {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            return 1;
        default:
            return 0;
    }
}

extern "C" void guiOverlayRender(int width, int height)
{
    if (!g_ready || !g_open)
        return;

    // The OpenGL2 renderer sets only the state it uses, and a game leaves anything behind: fog, alpha test, a texture
    // matrix, other texture units, a blend equation, shaders and buffer objects. Any of them can hide the window. All
    // the fixed function state is pushed and switched off; programs and buffer objects, outside the attribute stacks,
    // are saved by hand. Its client arrays need no buffer object, no program and texture unit 0.
    GLint program = 0, arrayBuffer = 0, elementBuffer = 0, activeTexture = GL_TEXTURE0, clientTexture = GL_TEXTURE0;
    glad_glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glad_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glad_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer);
    glad_glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glad_glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &clientTexture);
    GLboolean vertexProgram = glad_glIsEnabled(GL_VERTEX_PROGRAM_ARB);
    GLboolean fragmentProgram = glad_glIsEnabled(GL_FRAGMENT_PROGRAM_ARB);

    if (glad_glUseProgram)
        glad_glUseProgram(0);
    glad_glDisable(GL_VERTEX_PROGRAM_ARB);
    glad_glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
    glad_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glad_glActiveTexture(GL_TEXTURE0);
    glad_glClientActiveTexture(GL_TEXTURE0);

    glad_glPushAttrib(GL_ALL_ATTRIB_BITS);
    glad_glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    static const GLenum capabilities[] = {
        GL_FOG, GL_ALPHA_TEST, GL_LIGHTING, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_COLOR_LOGIC_OP, GL_COLOR_SUM,
        GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_ALPHA_TO_ONE, GL_FRAMEBUFFER_SRGB, GL_POLYGON_STIPPLE, GL_CLIP_PLANE0,
        GL_CLIP_PLANE1, GL_CLIP_PLANE2, GL_CLIP_PLANE3, GL_CLIP_PLANE4, GL_CLIP_PLANE5};
    for (GLenum capability : capabilities)
        glad_glDisable(capability);
    // Every texture unit: unit 0 keeps only the font atlas's 2D texture, the others would modulate it
    GLint units = 1;
    glad_glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
    for (GLint unit = SDL_min(units, 8) - 1; unit >= 0; unit--)
    {
        glad_glActiveTexture(GL_TEXTURE0 + unit);
        static const GLenum targets[] = {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP,
                                         GL_TEXTURE_RECTANGLE, GL_TEXTURE_GEN_S, GL_TEXTURE_GEN_T, GL_TEXTURE_GEN_R,
                                         GL_TEXTURE_GEN_Q};
        for (GLenum target : targets)
            glad_glDisable(target);
    }
    glad_glMatrixMode(GL_TEXTURE);
    glad_glPushMatrix();
    glad_glLoadIdentity();
    glad_glBlendEquation(GL_FUNC_ADD);
    glad_glDisableClientState(GL_NORMAL_ARRAY);
    glad_glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
    glad_glDisableClientState(GL_FOG_COORD_ARRAY);
    // An enabled generic attribute 0 would take the place of the vertex positions
    bool attribEnabled[16] = {};
    GLint attribs = 0;
    glad_glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attribs);
    for (GLint i = 0; i < SDL_min(attribs, 16); i++)
    {
        GLint enabled = 0;
        glad_glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        attribEnabled[i] = enabled != 0;
        if (enabled)
            glad_glDisableVertexAttribArray(i);
    }

    gui::newFrame();
    bool open = g_open;
    gui::drawSettings(&open, g_configFile, true);
    const char *hint = gui::tr("Game paused - Tab: close the settings");
    // Behind the windows: a page over the whole window (a device's bindings) hides it
    ImGui::GetBackgroundDrawList()->AddText(ImVec2(8.0f, 8.0f), IM_COL32(220, 225, 235, 255), hint);
    gui::renderFrame(width, height);
    if (!open)
        setOpen(false);

    for (GLint i = 0; i < 16; i++)
        if (attribEnabled[i])
            glad_glEnableVertexAttribArray(i);
    glad_glMatrixMode(GL_TEXTURE);
    glad_glPopMatrix();
    glad_glPopClientAttrib();
    glad_glPopAttrib();
    while (glad_glGetError() != GL_NO_ERROR) // a capability this driver lacks
        ;

    glad_glClientActiveTexture(clientTexture);
    glad_glActiveTexture(activeTexture);
    glad_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glad_glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
    if (fragmentProgram)
        glad_glEnable(GL_FRAGMENT_PROGRAM_ARB);
    if (vertexProgram)
        glad_glEnable(GL_VERTEX_PROGRAM_ARB);
    if (glad_glUseProgram)
        glad_glUseProgram(program);
}

extern "C" int guiOverlayVisible(void)
{
    return g_open;
}
