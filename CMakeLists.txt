cmake_minimum_required(VERSION 3.10)

# Honor CMAKE_MSVC_RUNTIME_LIBRARY (static CRT for the MSVC build)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(linuxloader LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "The MSVC build must target x86 (32-bit). Use the 'msvc-x86' preset or an x86 developer prompt.")
    endif()
    # Static CRT, matching the x86-windows-static vcpkg triplet
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/log)
include_directories(${PROJECT_SOURCE_DIR}/src/elfio)
include_directories(${PROJECT_SOURCE_DIR}/src/glad/include)
include_directories(${PROJECT_SOURCE_DIR}/src/minhook/include)
include_directories(${PROJECT_SOURCE_DIR}/src/SDL3)

# ------------------------------------------------------------------------------
# Common Static Libraries (Built for whichever platform CMake is targeting)
# ------------------------------------------------------------------------------

# 1. MinHook (Static)
file(GLOB_RECURSE MINHOOK_SRC src/minhook/src/*.c)
if(MSVC)
    set(MINHOOK_C_FLAGS)
else()
    set(MINHOOK_C_FLAGS -fPIC -m32)
endif()
add_library(minhook STATIC ${MINHOOK_SRC})
target_include_directories(minhook PUBLIC src/minhook/include)
target_compile_options(minhook PRIVATE ${MINHOOK_C_FLAGS})

# 2. LibXdiff (Static)
if(MSVC)
    # winconfig.h: same as config.h minus <unistd.h>
    set(LIBXDIFF_C_FLAGS -DHAVE_WINCONFIG_H -D_CRT_SECURE_NO_WARNINGS /wd4244 /wd4267)
else()
    set(LIBXDIFF_C_FLAGS -DHAVE_CONFIG_H -fPIC -m32)
endif()
file(GLOB_RECURSE XDIFF_SRC src/libxdiff/xdiff/*.c)
add_library(xdiff STATIC ${XDIFF_SRC})
target_include_directories(xdiff PUBLIC src/libxdiff/xdiff)
if(MSVC)
    target_include_directories(xdiff PRIVATE src/libxdiff)
endif()
target_compile_options(xdiff PRIVATE ${LIBXDIFF_C_FLAGS})

# ------------------------------------------------------------------------------
# Platform-Specific Projects
# ------------------------------------------------------------------------------

if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT MINGW AND NOT CMAKE_C_COMPILER MATCHES "mingw")
    message(STATUS "Configuring Project 1: Linux Build")

    # The linuxloader executable
    set(LINUXLOADER_EXE_C_FLAGS -m32 -DCOMPILING_LINUXLOADER_ELF)
    add_executable(linuxloader_exe
        src/loader/linuxExe.c
        src/loader/mainShared.c
        src/loader/config/iniParser.c
        src/loader/input/sdlInput.c
        src/loader/input/controlIniGen.c
        src/loader/log/log.c
        src/loader/hardware/lindbergh/output.c
        src/loader/hardware/lindbergh/jvs.c
        src/loader/config/config.c
        src/loader/input/evdevInput.c
        src/loader/config/gameData.c
        src/loader/config/configIni.c)

    set_target_properties(linuxloader_exe PROPERTIES LINK_FLAGS "-m32")
    target_link_libraries(linuxloader_exe PRIVATE m SDL3)
    set_target_properties(linuxloader_exe PROPERTIES OUTPUT_NAME "linuxloader")
    target_compile_options(linuxloader_exe PRIVATE ${LINUXLOADER_EXE_C_FLAGS})

    # Common options for shared libraries
    set(LIBS_C_FLAGS -m32 -fPIC -shared)
    set(SHARED_LINKER_FLAGS -m32 -Wl,-z,defs -rdynamic)

    # libkswapapi.so
    add_library(kswapapi SHARED src/loader/extraLibs/lindbergh/libkswapapi/libkswapapi.c)
    target_link_libraries(kswapapi ${SHARED_LINKER_FLAGS})
    target_compile_options(kswapapi PRIVATE ${LIBS_C_FLAGS})

    # libposixtime.so
    add_library(posixtime SHARED src/loader/extraLibs/lindbergh/libposixtime/libposixtime.c)
    target_link_libraries(posixtime ${SHARED_LINKER_FLAGS})
    target_compile_options(posixtime PRIVATE ${LIBS_C_FLAGS})
    add_custom_command(TARGET posixtime POST_BUILD COMMAND ln -s -f libposixtime.so ${CMAKE_BINARY_DIR}/libposixtime.so.1 COMMAND ln -s -f libposixtime.so ${CMAKE_BINARY_DIR}/libposixtime.so.2.4)

    # libsegaapi.so
    add_library(segaapi SHARED src/loader/audio/lindbergh/libsegaapi/libsegaapi.c)
    set(LIBSEGAAPI_C_FLAGS -m32 -fPIC -shared)
    set(LIBSEGAAPI_LINKER_FLAGS -O0 -g -m32 -Wl,-Bstatic -lFAudio -Wl,-Bdynamic -fPIC -shared)
    target_link_libraries(segaapi ${LIBSEGAAPI_LINKER_FLAGS})
    target_compile_options(segaapi PRIVATE ${LIBSEGAAPI_C_FLAGS})

    # linuxloader.so
    set(LINUX_LOADER_SO_LINKER_FLAGS -m32 -Wl,-z,defs -rdynamic -static-libstdc++ -static-libgcc -lc -ldl -lGL -lglut -lX11 -lXcursor -lSDL3 -lSDL3_ttf -lSDL3_image -ludev -lm -lpthread -shared -lstdc++ -nostdlib)
    set(LINUX_LOADER_SO_C_FLAGS -g -fPIC -m32 -Wall -Wno-unused-variable -Wno-unused-function -D_GNU_SOURCE -Wno-format-truncation -Wno-strict-aliasing -Wno-discarded-qualifiers -Wno-stringop-truncation -Wno-format-overflow)
    file(GLOB LINUX_LOADER_OBJS
        src/loader/init.c
        src/loader/mainL.c
        src/loader/mainShared.c
        src/loader/config/*.c
        src/loader/graphics/*.c
        src/loader/hardware/lindbergh/*.c
        src/loader/hardware/namco/*.c
        src/loader/input/*.c
        src/loader/log/*.c
        src/loader/patching/*.c
        src/loader/redirections/*.c
        src/loader/graphics/shaderWork/*.c
        src/loader/agent/*.c
        src/glad/src/gl.c)

    add_library(linuxloader SHARED ${LINUX_LOADER_OBJS})
    set_target_properties(linuxloader PROPERTIES PREFIX "")
    target_link_libraries(linuxloader ${LINUX_LOADER_SO_LINKER_FLAGS} xdiff minhook dl)
    target_compile_options(linuxloader PRIVATE ${LINUX_LOADER_SO_C_FLAGS})

elseif(WIN32 OR MINGW OR CMAKE_C_COMPILER MATCHES "mingw")

    enable_language(RC)

    include_directories(${PROJECT_SOURCE_DIR}/src/elfio)
    include_directories(${PROJECT_SOURCE_DIR}/src/SDL3)
    include_directories(${PROJECT_SOURCE_DIR}/src/FAudio)

    # Gather all source files
    file(GLOB SRC_FILES
        src/loader/init.c
        src/loader/mainW.cpp
        src/loader/mainShared.c
        src/loader/audio/lindbergh/libsegaapi/libsegaapi.c
        src/loader/audio/lindbergh/alsa2sdl/alsa2sdl.c
        src/loader/config/*.c
        src/loader/graphics/*.c
        src/loader/hardware/lindbergh/*.c
        src/loader/hardware/namco/*.c
        src/loader/input/*.c
        src/loader/log/*.c
        src/loader/patching/*.c
        src/loader/redirections/*.c
        src/loader/graphics/shaderWork/*.c
        src/loader/agent/*.c
        src/loader/elfLoader/*.cpp
        src/loader/elfLoader/pthread/*.cpp
        src/glad/src/gl.c
        src/icon.rc
        src/loader/gui/*.cpp
        src/imgui/imgui.cpp
        src/imgui/imgui_draw.cpp
        src/imgui/imgui_tables.cpp
        src/imgui/imgui_widgets.cpp
        src/imgui/backends/imgui_impl_sdl3.cpp
    )
    include_directories(${PROJECT_SOURCE_DIR}/src/imgui)


    # Executable target
    add_executable(linuxloader ${SRC_FILES})

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WIN32_WINNT=0x0600
        WINVER=0x0600
    )

    if(MSVC)
        # Dependencies come from vcpkg (see vcpkg.json, triplet x86-windows-static)
        find_package(SDL3 CONFIG REQUIRED)
        find_package(SDL3_ttf CONFIG REQUIRED)
        find_package(SDL3_image CONFIG REQUIRED)
        find_package(FAudio CONFIG REQUIRED)

        # MSVC lacks the POSIX headers and GCC extensions MinGW provides (unistd.h, dirent.h, ...)
        target_sources(${PROJECT_NAME} PRIVATE src/msvc/msvc_compat.c)
        target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src/msvc)
        set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 11)

        target_compile_definitions(${PROJECT_NAME} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
            _WINSOCK_DEPRECATED_NO_WARNINGS
            _USE_MATH_DEFINES
            _USE_32BIT_TIME_T # 32-bit time_t like MinGW/msvcrt: Linux i386 timespec/timeval/time_t go straight through the bridges
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )

        target_compile_options(${PROJECT_NAME} PRIVATE
            /FI${PROJECT_SOURCE_DIR}/src/msvc/msvc_compat.h
            /W3
            /utf-8
            /Zc:preprocessor
            /wd4100 # unreferenced parameter
            /wd4244 # conversion, possible loss of data
            /wd4267 # size_t conversion
            /wd4996 # deprecated POSIX names
        )

        # Increase stack size to 4MB to prevent stack overflow on startup
        target_link_options(${PROJECT_NAME} PRIVATE
            /STACK:4194304
            /LARGEADDRESSAWARE
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            minhook xdiff
            SDL3::SDL3-static SDL3_ttf::SDL3_ttf-static SDL3_image::SDL3_image-static FAudio::FAudio-static
            gdi32 winmm ole32 ws2_32 psapi usp10 rpcrt4 setupapi version imm32 hid dbghelp iphlpapi opengl32
        )
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall
            -Wextra
            -fpermissive
            -Wno-unused-parameter
            -Wunused-variable
            -Wno-unused-but-set-variable
            -Wimplicit-fallthrough=0
            -Wstrict-aliasing=0
        )

        # Increase stack size to 4MB to prevent stack overflow on startup
        target_link_options(${PROJECT_NAME} PRIVATE
            -s
            -Wl,--stack,4194304
            -Wl,--large-address-aware
        )

        # Link dependencies
        target_link_directories(${PROJECT_NAME} PRIVATE "libs/win32")
        target_link_libraries(${PROJECT_NAME}
            minhook xdiff -Wl,-Bstatic SDL3 SDL3_ttf SDL3_image FAudio freetype harfbuzz winpthread stdc++ gcc gcc_eh -Wl,-Bdynamic gdi32 winmm ole32 ws2_32 psapi usp10 rpcrt4 setupapi version imm32 hid delayimp
        )
    endif()

else()
    message(WARNING "Unknown platform, doing nothing. Please use Linux or MinGW.")
endif()
