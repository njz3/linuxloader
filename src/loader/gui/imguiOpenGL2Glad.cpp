// Dear ImGui's OpenGL2 renderer built on glad: the same GL entry points as the games and the loader, from the
// OpenGL library the loader picked (Zink's for Primeval Hunt) rather than the system's opengl32.dll.
#include <glad/gl.h>
#ifndef __GL_H__
#define __GL_H__ // the Windows SDK's GL/gl.h: glad declares it all already
#endif
#include "../../imgui/backends/imgui_impl_opengl2.cpp"
