#include "graphicsBridge.hpp"
#include "../graphics/x11Bridge.h"
#include "../graphics/glxBridge.h"
#include "../graphics/glutBridge.h"
#include "../graphics/shaderPatches.h"
#include "../graphics/sdl12Bridge.h"
#include "symbolResolver.hpp"
#include "../log/log.h"

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))
#define MAP_WITH_ORIG(name, func, origPtr) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func), reinterpret_cast<void**>(origPtr))

extern "C" {
    extern void* real_cgCreateProgram;
    extern void* real_cgGetProgramString;
}

namespace GraphicsBridge
{

    void initBridges()
    {
        log_info("Initializing Graphics Bridges (OpenGL/GLX/X11)...");

        // ============================================================
        // SDL 1.2 (Namco's Dead Heat)
        // ============================================================
        MAP("SDL_Init", sdl12Init);
        MAP("SDL_Quit", sdl12Quit);
        MAP("SDL_GetError", sdl12GetError);
        MAP("SDL_SetVideoMode", sdl12SetVideoMode);
        MAP("SDL_GL_SetAttribute", sdl12GLSetAttribute);
        MAP("SDL_GL_SwapBuffers", sdl12GLSwapBuffers);
        MAP("SDL_ShowCursor", sdl12ShowCursor);
        MAP("SDL_PollEvent", sdl12PollEvent);
        MAP("SDL_GetKeyState", sdl12GetKeyState);
        MAP("SDL_GetMouseState", sdl12GetMouseState);
        MAP("SDL_NumJoysticks", sdl12NumJoysticks);
        MAP("SDL_JoystickOpen", sdl12JoystickOpen);
        MAP("SDL_JoystickOpened", sdl12JoystickOpened);
        MAP("SDL_JoystickClose", sdl12JoystickClose);
        MAP("SDL_JoystickEventState", sdl12JoystickEventState);
        MAP("SDL_JoystickNumAxes", sdl12JoystickNumAxes);
        MAP("SDL_JoystickNumButtons", sdl12JoystickNumButtons);
        MAP("SDL_JoystickGetAxis", sdl12JoystickGetAxis);
        MAP("SDL_JoystickGetButton", sdl12JoystickGetButton);
        MAP("SDL_PushEvent", sdl12PushEvent);
        // Raw Thrills' Cruis'n Blast
        MAP("SDL_GetVideoInfo", sdl12GetVideoInfo);
        MAP("SDL_GetVideoSurface", sdl12GetVideoSurface);
        MAP("SDL_Flip", sdl12Flip);
        MAP("SDL_FreeSurface", sdl12FreeSurface);
        MAP("SDL_WasInit", sdl12WasInit);
        MAP("SDL_EnableKeyRepeat", sdl12EnableKeyRepeat);
        MAP("SDL_SetError", sdl12SetError);
        MAP("SDL_Error", sdl12Error);
        MAP("SDL_GL_GetAttribute", sdl12GLGetAttribute);
        MAP("SDL_WM_SetCaption", sdl12WMSetCaption);
        MAP("SDL_WM_GrabInput", sdl12WMGrabInput);
        MAP("SDL_WarpMouse", sdl12WarpMouse);
        MAP("SDL_GetRelativeMouseState", sdl12GetRelativeMouseState);
        MAP("SDL_GetTicks", sdl12GetTicks);
        MAP("SDL_Delay", sdl12Delay);
        MAP("SDL_ThreadID", sdl12ThreadID);
        MAP("SDL_OpenAudio", sdl12OpenAudio);
        MAP("SDL_CloseAudio", sdl12AudioNothing);
        MAP("SDL_LockAudio", sdl12AudioNothing);
        MAP("SDL_UnlockAudio", sdl12AudioNothing);
        MAP("SDL_PauseAudio", sdl12PauseAudio);
        MAP("SDL_MixAudio", sdl12MixAudio);
        MAP("SDL_BuildAudioCVT", sdl12Unavailable);
        MAP("SDL_ConvertAudio", sdl12Unavailable);
        MAP("SDL_LoadWAV_RW", sdl12Null);
        MAP("SDL_FreeWAV", sdl12Free);
        MAP("SDL_RWFromFile", sdl12Null);
        MAP("SDL_FreeRW", sdl12Free);
        MAP("SDL_ReadBE16", sdl12ReadZero);
        MAP("SDL_ReadBE32", sdl12ReadZero);
        MAP("SDL_ReadLE32", sdl12ReadZero);
        MAP("SDL_LoadObject", sdl12Null);
        MAP("SDL_LoadFunction", sdl12Null);
        MAP("SDL_UnloadObject", sdl12Free);

        // ============================================================
        // GLUT / GLU
        // ============================================================
        MAP("glutInit", bridgeGlutInit);
        MAP("glutInitDisplayMode", bridgeGlutInitDisplayMode);
        MAP("glutInitWindowSize", bridgeGlutInitWindowSize);
        MAP("glutInitWindowPosition", bridgeGlutInitWindowPosition);
        MAP("glutEnterGameMode", bridgeGlutEnterGameMode);
        MAP("glutFullScreen", bridgeGlutFullScreen);
        MAP("glutLeaveGameMode", bridgeGlutLeaveGameMode);
        MAP("glutCreateWindow", bridgeGlutCreateWindow);
        MAP("glutMainLoop", bridgeGlutMainLoop);
        MAP("glutDisplayFunc", bridgeGlutDisplayFunc);
        MAP("glutIdleFunc", bridgeGlutIdleFunc);
        MAP("glutPostRedisplay", bridgeGlutPostRedisplay);
        MAP("glutSwapBuffers", bridgeGlutSwapBuffers);
        MAP("glutGet", bridgeGlutGet);
        MAP("glutSetCursor", bridgeGlutSetCursor);
        MAP("glutGameModeString", bridgeGlutGameModeString);
        MAP("glutBitmapCharacter", bridgeGlutBitmapCharacter);
        MAP("glutBitmapWidth", bridgeGlutBitmapWidth);
        MAP("glutMainLoopEvent", bridgeGlutMainLoopEvent);
        MAP("glutJoystickFunc", bridgeGlutJoystickFunc);
        MAP("glutPostRedisplay", bridgeGlutPostRedisplay);
        MAP("glutGet", bridgeGlutGet);
        MAP("glutSetCursor", bridgeGlutSetCursor);
        MAP("glutGameModeString", bridgeGlutGameModeString);
        MAP("glutBitmapCharacter", bridgeGlutBitmapCharacter);
        MAP("glutBitmapWidth", bridgeGlutBitmapWidth);
        MAP("glutKeyboardFunc", bridgeGlutKeyboardFunc);
        MAP("glutKeyboardUpFunc", bridgeGlutKeyboardUpFunc);
        MAP("glutSpecialUpFunc", bridgeGlutSpecialUpFunc);
        MAP("glutMotionFunc", bridgeGlutMotionFunc);
        MAP("glutMouseFunc", bridgeGlutMouseFunc);
        MAP("glutPassiveMotionFunc", bridgeGlutPassiveMotionFunc);
        MAP("glutEntryFunc", bridgeGlutEntryFunc);
        MAP("glutJoystickFunc", bridgeGlutJoystickFunc);
        MAP("glutSolidTeapot", bridgeGlutSolidTeapot);
        MAP("glutWireTeapot", bridgeGlutWireTeapot);
        MAP("glutSpecialFunc", bridgeGlutSpecialFunc);
        MAP("glutReshapeFunc", bridgeGlutReshapeFunc);
        MAP("glutSolidSphere", bridgeGlutSolidSphere);
        MAP("glutWireSphere", bridgeGlutWireSphere);
        MAP("glutWireCone", bridgeGlutWireCone);
        MAP("glutSolidCone", bridgeGlutSolidCone);
        MAP("glutWireCube", bridgeGlutWireCube);
        MAP("glutSolidCube", bridgeGlutSolidCube);
        MAP("glutVisibilityFunc", bridgeGlutVisibilityFunc);
        MAP("glutExtensionSupported", bridgeGlutExtensionSupported);
        // MAP("glutGetModifiers", bridgeGlutGetModifiers);

        MAP("gluPerspective", bridgegluPerspective);
        MAP("gluLookAt", bridgegluLookAt);
        MAP("gluOrtho2D", bridgegluOrtho2D);
        MAP("gluErrorString", bridgegluErrorString);
        MAP("gluProject", bridgegluProject);
        MAP("gluUnProject", bridgegluUnProject);
        MAP("gluBuild2DMipmaps", bridgegluBuild2DMipmaps);

        // XF86VidMode
        MAP("XF86VidModeQueryExtension", bridgeXF86VidModeQueryExtension);
        MAP("XF86VidModeGetAllModeLines", bridgeXF86VidModeGetAllModeLines);
        MAP("XF86VidModeGetModeLine", bridgeXF86VidModeGetModeLine);
        MAP("XF86VidModeSwitchToMode", bridgeXF86VidModeSwitchToMode);
        MAP("XF86VidModeSetViewPort", bridgeXF86VidModeSetViewPort);
        MAP("XF86VidModeGetViewPort", bridgeXF86VidModeGetViewPort);
        MAP("XF86VidModeLockModeSwitch", bridgeXF86VidModeLockModeSwitch);
        MAP("XF86VidModeQueryVersion", bridgeXF86VidModeQueryVersion);

        // X11 Misc
        MAP("XOpenDisplay", bridgeXOpenDisplay);
        MAP("XCloseDisplay", bridgeXCloseDisplay);
        MAP("XInitThreads", bridgeXInitThreads);
        MAP("XLockDisplay", bridgeXLockDisplay);
        MAP("XUnlockDisplay", bridgeXUnlockDisplay);
        MAP("XSync", bridgeXSync);
        MAP("XFlush", bridgeXFlush);
        MAP("_XFlush", bridgeXFlush);
        MAP("_XReply", bridgeXReply);
        MAP("XCreateWindow", bridgeXCreateWindow);
        MAP("XDestroyWindow", bridgeXDestroyWindow);
        MAP("XConfigureWindow", bridgeXConfigureWindow);
        MAP("XMapWindow", bridgeXMapWindow);
        MAP("XGetWindowAttributes", bridgeXGetWindowAttributes);
        MAP("XChangeWindowAttributes", bridgeXChangeWindowAttributes);
        MAP("XGetGeometry", bridgeXGetGeometry);
        MAP("XMapRaised", bridgeXMapRaised);
        MAP("XextFindDisplay", bridgeXextFindDisplay);
        MAP("XMoveWindow", bridgeXMoveWindow);
        MAP("XResizeWindow", bridgeXResizeWindow);
        MAP("XPending", bridgeXPending);
        MAP("XNextEvent", bridgeXNextEvent);
        MAP("XIfEvent", bridgeXIfEvent);
        MAP("XTranslateCoordinates", bridgeXTranslateCoordinates);
        MAP("XQueryPointer", bridgeXQueryPointer);
        MAP("XWarpPointer", bridgeXWarpPointer);
        MAP("XGrabPointer", bridgeXGrabPointer);
        MAP("XUngrabPointer", bridgeXUngrabPointer);
        MAP("XLookupString", bridgeXLookupString);
        MAP("XStoreName", bridgeXStoreName);
        MAP("XCreateColormap", bridgeXCreateColormap);
        MAP("XFreeColormap", bridgeXFreeColormap);
        MAP("XSetTransientForHint", bridgeXSetTransientForHint);

        MAP("XDisplayWidth", bridgeXDisplayWidth);
        MAP("XDisplayHeight", bridgeXDisplayHeight);
        MAP("XDisplayWidthMM", bridgeXDisplayWidthMM);
        MAP("XDisplayHeightMM", bridgeXDisplayHeightMM);
        MAP("XSetWMNormalHints", bridgeXSetWMNormalHints);
        MAP("XInternAtom", (Atom (*)(Display *, const char *, bool))bridgeXInternAtom);
        MAP("XSetStandardProperties", bridgeXSetStandardProperties);
        MAP("XRRGetScreenResources", bridgeXRRGetScreenResources);
        MAP("XRRGetCrtcInfo", bridgeXRRGetCrtcInfo);

        MAP("XParseColor", bridgeXParseColor);
        MAP("XFree", bridgeXFree);

        MAP("XSetErrorHandler", bridgeXSetErrorHandler);
        MAP("XGetErrorText", bridgeXGetErrorText);

        MAP("XCreatePixmapCursor", bridgeXCreatePixmapCursor);
        MAP("XCreatePixmapFromBitmapData", bridgeXCreatePixmapFromBitmapData);
        MAP("XFreePixmap", bridgeXFreePixmap);
        MAP("XFreeCursor", bridgeXFreeCursor);
        MAP("XDefineCursor", bridgeXDefineCursor);
        MAP("XUndefineCursor", bridgeXUndefineCursor);

        MAP("XSetInputFocus", bridgeXSetInputFocus);
        MAP("XSetWMProtocols", bridgeXSetWMProtocols);
        MAP("XSetWMProperties", bridgeXSetWMProperties);
        MAP("XStringListToTextProperty", bridgeXStringListToTextProperty);
        MAP("XChangeProperty", bridgeXChangeProperty);
        MAP("XSetCloseDownMode", bridgeXSetCloseDownMode);
        MAP("XAutoRepeatOn", bridgeXAutoRepeatOn);
        MAP("XAutoRepeatOff", bridgeXAutoRepeatOff);
        MAP("XCreateBitmapFromData", bridgeXCreateBitmapFromData);

        MAP("XGrabKeyboard", bridgeXGrabKeyboard);
        MAP("XUngrabKeyboard", bridgeXUngrabKeyboard);
        MAP("XKeysymToKeycode", bridgeXKeysymToKeycode);
        MAP("XSendEvent", bridgeXSendEvent);

        MAP("XAllocWMHints", bridgeXAllocWMHints);
        MAP("XAllocSizeHints", bridgeXAllocSizeHints);

        // Maximum Heat 3D and Dead Heat Riders
        MAP("XCreateImage", bridgeXCreateImage);
        MAP("XPutImage", bridgeXPutImage);
        MAP("XCreatePixmap", bridgeXCreatePixmap);
        MAP("XCreateGC", bridgeXCreateGC);
        MAP("XFreeGC", bridgeXFreeGC);
        MAP("XClearWindow", bridgeXWindowNoOp);
        MAP("XRaiseWindow", bridgeXWindowNoOp);
        MAP("XUnmapWindow", bridgeXWindowNoOp);
        MAP("XMoveResizeWindow", bridgeXMoveResizeWindow);
        MAP("XReparentWindow", bridgeXReparentWindow);
        MAP("XSetWindowBackground", bridgeXSetWindowBackground);
        MAP("XGetScreenSaver", bridgeXGetScreenSaver);
        MAP("XSetScreenSaver", bridgeXSetScreenSaver);
        MAP("DPMSQueryExtension", bridgeDPMSQueryExtension);
        MAP("DPMSDisable", bridgeDPMSDisable);
        MAP("XKeycodeToKeysym", bridgeXKeycodeToKeysym);
        MAP("XStringToKeysym", bridgeXStringToKeysym);
        MAP("XQueryKeymap", bridgeXQueryKeymap);

        // ============================================================
        // GLX Functions
        // ============================================================
        MAP("glXChooseVisual", bridgeGlxChooseVisual);
        MAP("glXCreateContext", bridgeGlxCreateContext);
        MAP("glXMakeCurrent", bridgeGlxMakeCurrent);
        MAP("glXCreateNewContext", bridgeGlxCreateNewContext);
        MAP("glXDestroyPbuffer", bridgeGlxDestroyPbuffer);
        MAP("glXAllocateMemoryNV", bridgeGlxAllocateMemoryNV);
        MAP("glXFreeMemoryNV", bridgeGlxFreeMemoryNV);
        MAP("glXCreatePbuffer", bridgeGlxCreatePbuffer);
        MAP("glXSwapBuffers", bridgeGlxSwapBuffers);
        MAP("glXDestroyContext", bridgeGlxDestroyContext);
        MAP("glXGetCurrentDisplay", bridgeGlxGetCurrentDisplay);
        MAP("glXGetCurrentDrawable", bridgeGlxGetCurrentDrawable);
        MAP("glXGetCurrentContext", bridgeGlxGetCurrentContext);
        MAP("glXChooseFBConfig", bridgeGlxChooseFBConfig);
        MAP("glXGetVisualFromFBConfig", bridgeGlxGetVisualFromFBConfig);
        MAP("glXGetProcAddressARB", bridgeGlxGetProcAddress);
        MAP("glXGetProcAddress", bridgeGlxGetProcAddress);
        MAP("glXQueryExtension", bridgeGlxQueryExtension);
        MAP("glXSwapIntervalSGI", bridgeGlxSwapInterval);
        MAP("glXQueryExtension", bridgeGlxQueryExtension);
        MAP("glXQueryExtensionsString", bridgeGlxQueryExtensionsString);
        MAP("glXQueryServerString", bridgeGlxQueryServerString);
        MAP("glXQueryVersion", bridgeGlxQueryVersion);
        MAP("glXGetClientString", bridgeGlxGetClientString);
        MAP("glXIsDirect", bridgeGlxIsDirect);
        MAP("glXGetConfig", bridgeGlxGetConfig);

        // GLX SGI
        MAP("glXSwapIntervalSGI", bridgeGlxSwapIntervalSGI);
        MAP("glXGetVideoSyncSGI", bridgeGlxGetVideoSyncSGI);
        MAP("glXGetRefreshRateSGI", bridgeGlxGetRefreshRateSGI);

        // GLX SGIX / FBConfig Extensions
        MAP("glXChooseFBConfigSGIX", bridgeGlxChooseFBConfigSGIX);
        MAP("glXGetFBConfigAttribSGIX", bridgeGlxGetFBConfigAttribSGIX);
        MAP("glXCreateContextWithConfigSGIX", bridgeGlxCreateContextWithConfigSGIX);
        MAP("glXCreateGLXPbufferSGIX", bridgeGlxCreateGLXPbufferSGIX);
        MAP("glXDestroyGLXPbufferSGIX", bridgeGlxDestroyGLXPbufferSGIX);
        MAP("glXQueryDrawable", bridgeGlxQueryDrawable);
        MAP("glXQueryGLXPbufferSGIX", bridgeGlxQueryDrawable);

        // CG
        MAP_WITH_ORIG("cgCreateProgram", bridgeCgCreateProgram, &real_cgCreateProgram);
        MAP_WITH_ORIG("cgGetProgramString", bridgeCgGetProgramString, &real_cgGetProgramString);
    }
} // namespace GraphicsBridge
