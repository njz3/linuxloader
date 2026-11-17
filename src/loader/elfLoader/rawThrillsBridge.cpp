// The libraries Raw Thrills' Cruis'n Blast links that no Lindbergh game did:
// - udev, through which it finds its I/O boards (the RIO2 and the wheel's USB H-Bridge, FTDI serial adapters): for
//   now a system without any device, the calls logged;
// - Theora and Ogg, for its videos: streams that never yield a page, so the game finds no video to play;
// - PhysX's cooking library: none, as on a cabinet without PhysX installed.
#if defined(_WIN32) || defined(__MINGW32__)

#include <stdint.h>
#include <string.h>
#include <vector>

#include <glad/gl.h>

#include "rawThrillsBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

// --- PhysX

static void *bridgeNxGetCookingLib(uint32_t sdkVersion)
{
    log_warn("PhysX: no cooking library (SDK version 0x%08X asked)", sdkVersion);
    return nullptr;
}

// --- udev: one context, one enumeration and one monitor object, all empty

struct UdevObject
{
    int refs;
};
static UdevObject s_udev = {1}, s_enumerate = {1}, s_monitor = {1};

static void *bridgeUdevNew()
{
    return &s_udev;
}

static void *bridgeUdevUnref(void *)
{
    return nullptr;
}

static void *bridgeUdevEnumerateNew(void *)
{
    return &s_enumerate;
}

static int bridgeUdevEnumerateAddMatchSubsystem(void *, const char *subsystem)
{
    log_warn("udev: the game looks for %s devices: none", subsystem ? subsystem : "(null)");
    return 0;
}

static int bridgeUdevZero(void *)
{
    return 0;
}

static void *bridgeUdevNull(void *)
{
    return nullptr;
}

static void *bridgeUdevNull2(void *, const void *)
{
    return nullptr;
}

static void *bridgeUdevDeviceNewFromDevnum(void *, char type, uint64_t devnum)
{
    (void)type;
    (void)devnum;
    return nullptr;
}

static void *bridgeUdevDeviceGetParentWithSubsystemDevtype(void *, const char *, const char *)
{
    return nullptr;
}

static void *bridgeUdevMonitorNewFromNetlink(void *, const char *name)
{
    log_info("udev: monitor on %s", name ? name : "(null)");
    return &s_monitor;
}

static int bridgeUdevMonitorFilterAddMatchSubsystemDevtype(void *, const char *subsystem, const char *devtype)
{
    log_info("udev: monitor for %s %s", subsystem ? subsystem : "(null)", devtype ? devtype : "");
    return 0;
}

static int bridgeUdevMonitorGetFd(void *)
{
    return -1;
}

// --- Ogg and Theora: the sync layer takes the file's bytes and gives no page back

static std::vector<char> s_oggBuffer;

static int bridgeOggZero(void *)
{
    return 0;
}

static int bridgeOggZero2(void *, void *)
{
    return 0;
}

static int bridgeOggStreamInit(void *, int)
{
    return 0;
}

static char *bridgeOggSyncBuffer(void *, long size)
{
    if (size < 0)
        return nullptr;
    if ((size_t)size > s_oggBuffer.size())
        s_oggBuffer.resize((size_t)size);
    return s_oggBuffer.data();
}

static int bridgeOggSyncWrote(void *, long)
{
    return 0;
}

static int bridgeOggMinusOne()
{
    return -1;
}

static void bridgeTheoraNothing(void *)
{
}

static int bridgeTheoraBadHeader()
{
    log_info("Theora: no video decoder");
    return -20; // OC_BADHEADER
}

static int64_t bridgeTheoraGranuleFrame(void *, int64_t)
{
    return -1;
}

static double bridgeTheoraGranuleTime(void *, int64_t)
{
    return -1.0;
}

static char *bridgeTheoraCommentQuery(void *, char *, int)
{
    return nullptr;
}

// --- OpenGL 3 functions no Lindbergh game imported: Linux's GL is cdecl, glad's pointers stdcall

static GLboolean __attribute__((cdecl)) rt_glAreTexturesResident(GLsizei n, const GLuint * textures, GLboolean * residences)
{
    return glad_glAreTexturesResident ? glad_glAreTexturesResident(n, textures, residences) : 0;
}

static void __attribute__((cdecl)) rt_glBindAttribLocation(GLuint program, GLuint index, const GLchar * name)
{
    if (glad_glBindAttribLocation)
        glad_glBindAttribLocation(program, index, name);
}

static void __attribute__((cdecl)) rt_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    if (glad_glBindBufferBase)
        glad_glBindBufferBase(target, index, buffer);
}

static void __attribute__((cdecl)) rt_glBindFragDataLocation(GLuint program, GLuint color, const GLchar * name)
{
    if (glad_glBindFragDataLocation)
        glad_glBindFragDataLocation(program, color, name);
}

static void __attribute__((cdecl)) rt_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    if (glad_glBlitFramebuffer)
        glad_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

static void __attribute__((cdecl)) rt_glGenerateMipmap(GLenum target)
{
    if (glad_glGenerateMipmap)
        glad_glGenerateMipmap(target);
}

static void __attribute__((cdecl)) rt_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name)
{
    if (glad_glGetActiveAttrib)
        glad_glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

static void __attribute__((cdecl)) rt_glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei * length, GLchar * uniformBlockName)
{
    if (glad_glGetActiveUniformBlockName)
        glad_glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

static void __attribute__((cdecl)) rt_glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint * params)
{
    if (glad_glGetActiveUniformBlockiv)
        glad_glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

static void __attribute__((cdecl)) rt_glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei * length, GLchar * uniformName)
{
    if (glad_glGetActiveUniformName)
        glad_glGetActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
}

static void __attribute__((cdecl)) rt_glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint * uniformIndices, GLenum pname, GLint * params)
{
    if (glad_glGetActiveUniformsiv)
        glad_glGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

static void __attribute__((cdecl)) rt_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, void * binary)
{
    if (glad_glGetProgramBinary)
        glad_glGetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

static void __attribute__((cdecl)) rt_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint * params)
{
    if (glad_glGetRenderbufferParameteriv)
        glad_glGetRenderbufferParameteriv(target, pname, params);
}

static void __attribute__((cdecl)) rt_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source)
{
    if (glad_glGetShaderSource)
        glad_glGetShaderSource(shader, bufSize, length, source);
}

static GLuint __attribute__((cdecl)) rt_glGetUniformBlockIndex(GLuint program, const GLchar * uniformBlockName)
{
    return glad_glGetUniformBlockIndex ? glad_glGetUniformBlockIndex(program, uniformBlockName) : 0;
}

static void __attribute__((cdecl)) rt_glProgramBinary(GLuint program, GLenum binaryFormat, const void * binary, GLsizei length)
{
    if (glad_glProgramBinary)
        glad_glProgramBinary(program, binaryFormat, binary, length);
}

static void __attribute__((cdecl)) rt_glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
    if (glad_glProgramParameteri)
        glad_glProgramParameteri(program, pname, value);
}

static void __attribute__((cdecl)) rt_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    if (glad_glRenderbufferStorageMultisample)
        glad_glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

static void __attribute__((cdecl)) rt_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels)
{
    if (glad_glTexImage3D)
        glad_glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

static void __attribute__((cdecl)) rt_glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    if (glad_glUniformBlockBinding)
        glad_glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

static void __attribute__((cdecl)) rt_glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value)
{
    if (glad_glUniformMatrix3x4fv)
        glad_glUniformMatrix3x4fv(location, count, transpose, value);
}

static void __attribute__((cdecl)) rt_glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value)
{
    if (glad_glUniformMatrix4x3fv)
        glad_glUniformMatrix4x3fv(location, count, transpose, value);
}

namespace RawThrillsBridge
{
    void initBridges()
    {
        MAP("glAreTexturesResident", rt_glAreTexturesResident);
        MAP("glBindAttribLocation", rt_glBindAttribLocation);
        MAP("glBindBufferBase", rt_glBindBufferBase);
        MAP("glBindFragDataLocation", rt_glBindFragDataLocation);
        MAP("glBlitFramebuffer", rt_glBlitFramebuffer);
        MAP("glGenerateMipmap", rt_glGenerateMipmap);
        MAP("glGetActiveAttrib", rt_glGetActiveAttrib);
        MAP("glGetActiveUniformBlockName", rt_glGetActiveUniformBlockName);
        MAP("glGetActiveUniformBlockiv", rt_glGetActiveUniformBlockiv);
        MAP("glGetActiveUniformName", rt_glGetActiveUniformName);
        MAP("glGetActiveUniformsiv", rt_glGetActiveUniformsiv);
        MAP("glGetProgramBinary", rt_glGetProgramBinary);
        MAP("glGetRenderbufferParameteriv", rt_glGetRenderbufferParameteriv);
        MAP("glGetShaderSource", rt_glGetShaderSource);
        MAP("glGetUniformBlockIndex", rt_glGetUniformBlockIndex);
        MAP("glProgramBinary", rt_glProgramBinary);
        MAP("glProgramParameteri", rt_glProgramParameteri);
        MAP("glRenderbufferStorageMultisample", rt_glRenderbufferStorageMultisample);
        MAP("glTexImage3D", rt_glTexImage3D);
        MAP("glUniformBlockBinding", rt_glUniformBlockBinding);
        MAP("glUniformMatrix3x4fv", rt_glUniformMatrix3x4fv);
        MAP("glUniformMatrix4x3fv", rt_glUniformMatrix4x3fv);

        MAP("NxGetCookingLib", bridgeNxGetCookingLib);

        MAP("udev_new", bridgeUdevNew);
        MAP("udev_unref", bridgeUdevUnref);
        MAP("udev_enumerate_new", bridgeUdevEnumerateNew);
        MAP("udev_enumerate_add_match_subsystem", bridgeUdevEnumerateAddMatchSubsystem);
        MAP("udev_enumerate_scan_devices", bridgeUdevZero);
        MAP("udev_enumerate_get_list_entry", bridgeUdevNull);
        MAP("udev_enumerate_unref", bridgeUdevNull);
        MAP("udev_list_entry_get_name", bridgeUdevNull);
        MAP("udev_list_entry_get_next", bridgeUdevNull);
        MAP("udev_device_new_from_syspath", bridgeUdevNull2);
        MAP("udev_device_new_from_devnum", bridgeUdevDeviceNewFromDevnum);
        MAP("udev_device_get_parent_with_subsystem_devtype", bridgeUdevDeviceGetParentWithSubsystemDevtype);
        MAP("udev_device_get_sysattr_value", bridgeUdevNull2);
        MAP("udev_device_get_devnode", bridgeUdevNull);
        MAP("udev_device_get_sysname", bridgeUdevNull);
        MAP("udev_device_get_action", bridgeUdevNull);
        MAP("udev_device_unref", bridgeUdevNull);
        MAP("udev_monitor_new_from_netlink", bridgeUdevMonitorNewFromNetlink);
        MAP("udev_monitor_filter_add_match_subsystem_devtype", bridgeUdevMonitorFilterAddMatchSubsystemDevtype);
        MAP("udev_monitor_enable_receiving", bridgeUdevZero);
        MAP("udev_monitor_get_fd", bridgeUdevMonitorGetFd);
        MAP("udev_monitor_receive_device", bridgeUdevNull);
        MAP("udev_monitor_unref", bridgeUdevNull);

        MAP("ogg_sync_init", bridgeOggZero);
        MAP("ogg_sync_clear", bridgeOggZero);
        MAP("ogg_sync_buffer", bridgeOggSyncBuffer);
        MAP("ogg_sync_wrote", bridgeOggSyncWrote);
        MAP("ogg_sync_pageout", bridgeOggZero2); // no page: more data wanted, until the file ends
        MAP("ogg_page_bos", bridgeOggZero);
        MAP("ogg_page_serialno", bridgeOggZero);
        MAP("ogg_stream_init", bridgeOggStreamInit);
        MAP("ogg_stream_clear", bridgeOggZero);
        MAP("ogg_stream_pagein", bridgeOggMinusOne);
        MAP("ogg_stream_packetin", bridgeOggMinusOne);
        MAP("ogg_stream_packetout", bridgeOggZero2);
        MAP("ogg_stream_pageout", bridgeOggZero2);
        MAP("ogg_stream_flush", bridgeOggZero2);

        MAP("theora_info_init", bridgeTheoraNothing);
        MAP("theora_info_clear", bridgeTheoraNothing);
        MAP("theora_comment_init", bridgeTheoraNothing);
        MAP("theora_comment_clear", bridgeTheoraNothing);
        MAP("theora_comment_add", bridgeTheoraNothing);
        MAP("theora_comment_query", bridgeTheoraCommentQuery);
        MAP("theora_clear", bridgeTheoraNothing);
        MAP("theora_decode_header", bridgeTheoraBadHeader);
        MAP("theora_decode_init", bridgeTheoraBadHeader);
        MAP("theora_decode_packetin", bridgeTheoraBadHeader);
        MAP("theora_decode_YUVout", bridgeTheoraBadHeader);
        MAP("theora_encode_init", bridgeTheoraBadHeader);
        MAP("theora_encode_YUVin", bridgeTheoraBadHeader);
        MAP("theora_encode_packetout", bridgeTheoraBadHeader);
        MAP("theora_encode_header", bridgeTheoraBadHeader);
        MAP("theora_encode_comment", bridgeTheoraBadHeader);
        MAP("theora_encode_tables", bridgeTheoraBadHeader);
        MAP("theora_granule_frame", bridgeTheoraGranuleFrame);
        MAP("theora_granule_time", bridgeTheoraGranuleTime);
    }
}
#endif
