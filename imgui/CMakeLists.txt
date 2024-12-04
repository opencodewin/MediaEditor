#
# Cmake for building imgui
#
cmake_minimum_required(VERSION 3.12.0)
project(IMGUI)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()
if(POLICY CMP0025)
    # reference from https://cmake.org/cmake/help/latest/policy/CMP0025.html
    cmake_policy(SET CMP0025 NEW)
endif()
if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

#if we need run on MacOS before 12.0, then uncomment following code, but not guarantee it's working
#if(APPLE)
#set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") 
#endif(APPLE)

macro(_OPTION variable description value)
    set(__value ${value})
    set(__condition "")
    set(__varname "__value")
    foreach(arg ${ARGN})
        if(arg STREQUAL "IF" OR arg STREQUAL "if")
            set(__varname "__condition")
        else()
            list(APPEND ${__varname} ${arg})
        endif()
    endforeach()
    unset(__varname)
    if(__condition STREQUAL "")
        set(__condition 2 GREATER 1)
    endif()

    if(${__condition})
        if(__value MATCHES ";")
            if(${__value})
                option(${variable} "${description}" ON)
            else()
                option(${variable} "${description}" OFF)
            endif()
        elseif(DEFINED ${__value})
            if(${__value})
                option(${variable} "${description}" ON)
            else()
                option(${variable} "${description}" OFF)
            endif()
        else()
            option(${variable} "${description}" ${__value})
        endif()
    else()
        unset(${variable} CACHE)
    endif()
    unset(__condition)
    unset(__value)
endmacro()

if(WIN32) 
    add_definitions(-D_CRT_SECURE_NO_WARNINGS) 
    set(CMAKE_COLOR_MAKEFILE OFF CACHE BOOL "Disable Windows color Makefile by Force" FORCE)
endif() 

SET(SDK_CONFIG_DIR ${PROJECT_SOURCE_DIR}/config/)
SET(SDK_DOC_DIR ${PROJECT_SOURCE_DIR}/docs/)
CONFIGURE_FILE(Config.h.in Config.h)

include_directories( 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}
)

find_package(PkgConfig)

_OPTION(IMGUI_STATIC                "Build ImGui as static library" OFF)
_OPTION(IMGUI_BUILD_EXAMPLE         "Build ImGui example" OFF)
_OPTION(IMGUI_WGPU                  "Build ImGui wgpu backends" OFF)
_OPTION(IMGUI_GL2                   "Build ImGui OpenGL2 backends" ON)
_OPTION(IMGUI_GL3                   "Build ImGui OpenGL3 backends" ON)
_OPTION(IMGUI_SDL2                  "Build ImGui SDL2 backends" ON)
_OPTION(IMGUI_SDL2_RENDERER         "Build ImGui SDL2 Renderer backends" OFF)
_OPTION(IMGUI_GLFW                  "Build ImGui GLFW backends" ON)
_OPTION(IMGUI_GLUT                  "Build ImGui GLUT backends" OFF)
_OPTION(IMGUI_GLEW                  "Build ImGui GLEW backends" ON)
_OPTION(IMGUI_GLES                  "Build ImGui GLES backends" ON)
_OPTION(IMGUI_MATAL                 "Build ImGui Apple Metal backends(Apple only)" ON IF APPLE)
_OPTION(IMGUI_VULKAN                "Build ImGui Vulkan backends" OFF)
_OPTION(IMGUI_VULKAN_SHADER         "Build ImGui With Vulkan compute support" ON)
_OPTION(IMGUI_DX9                   "Build ImGui Direct9 backends(Windows only)" ON IF WIN32)
_OPTION(IMGUI_DX10                  "Build ImGui Direct10 backends(Windows only)" ON IF WIN32)
_OPTION(IMGUI_DX11                  "Build ImGui Direct11 backends(Windows only)" ON IF WIN32)
_OPTION(IMGUI_DX12                  "Build ImGui Direct12 backends(Windows only)" ON IF WIN32)
_OPTION(IMGUI_FREETYPE              "Build ImGui with FreeType support" OFF)
_OPTION(IMGUI_BUILD_DITHER          "Build ImGui with Dither support" OFF)
_OPTION(IMGUI_BUILD_POTRACE         "Build ImGui with Potace support" OFF)
_OPTION(IMGUI_ICONS                 "Internal Icons build in library" ON)
_OPTION(IMGUI_APPS                  "build apps base on imgui" ON)
_OPTION(IMGUI_SKIP_INSTALL          "Skip imgui install" ON)

if(IOS AND CMAKE_OSX_ARCHITECTURES MATCHES "arm")
    message(STATUS "Target arch: arm-ios")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    message(STATUS "Target arch: arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|x86_64|AMD64)")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86")
        message(STATUS "Target arch: x86")
    else()
        message(STATUS "Target arch: x86_64")
    endif()
    if(MSVC OR MSVC_IDE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2 /arch:AVX /arch:FMA /arch:SSE /arch:SSE2 /arch:SSSE3 /arch:SSE4.1 /arch:SSE4.2")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX2 /arch:AVX /arch:FMA /arch:SSE /arch:SSE2 /arch:SSSE3 /arch:SSE4.1 /arch:SSE4.2")
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        ### we disable wasm simd support because safari isn't support it yet
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse -msimd128 -mrelaxed-simd")
        #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse -msimd128 -mrelaxed-simd")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mavx")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -mavx")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
    endif()
endif()


if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
find_package(OpenGL)
find_package(OpenSSL)
find_package(TIFF)

if (OPENGL_FOUND)
    message("imgui found opengl")
endif(OPENGL_FOUND)

if (OpenSSL_FOUND)
    message(STATUS "imgui found openssl")
endif(OpenSSL_FOUND)

if (IMGUI_FREETYPE)
    find_package(Freetype)
    if (FREETYPE_FOUND)
        message("imgui found freetype")
    endif (FREETYPE_FOUND)
endif(IMGUI_FREETYPE)

if (IMGUI_GLUT)
    find_package(GLUT)
    if (GLUT_FOUND)
    message("imgui found glut")
    endif (GLUT_FOUND)
endif (IMGUI_GLUT)

if(PKG_CONFIG_FOUND)
    pkg_search_module(ZLIB REQUIRED zlib)
    if(ZLIB_FOUND)
        message("imgui zlib found")
    endif(ZLIB_FOUND)
    if(IMGUI_GLEW)
        pkg_search_module(GLEW REQUIRED glew)
    endif(IMGUI_GLEW)
    if(IMGUI_GLFW)
        pkg_search_module(GLFW REQUIRED glfw3)
    endif(IMGUI_GLFW)
    if(IMGUI_GLUT AND NOT GLUT_FOUND)
        pkg_search_module(GLUT REQUIRED glut)
        if (GLUT_FOUND)
            message("imgui glut found")
        endif (GLUT_FOUND)
    endif(IMGUI_GLUT AND NOT GLUT_FOUND)
    if(IMGUI_VULKAN)
        pkg_search_module(VULKAN REQUIRED vulkan)
        if (VULKAN_FOUND)
            message("imgui vulkan found")
        endif(VULKAN_FOUND)
    endif(IMGUI_VULKAN)
    if(NOT OPENGL_FOUND)
        pkg_search_module(OPENGL REQUIRED gl)
        if (OPENGL_FOUND)
            message("imgui opengl found")
        endif()
    endif()
    if(NOT FREETYPE_FOUND)
        pkg_search_module(FREETYPE REQUIRED freetype2)
        if (FREETYPE_FOUND)
            message("imgui freetype found")
        endif()
    endif()
    pkg_search_module(SDL2 REQUIRED sdl2)
    if(SDL2_FOUND)
        message("imgui sdl2 found")
    endif()
    if (WIN32)
        find_package(glm CONFIG REQUIRED)
        set(GLM_FOUND ON)
    else()
        pkg_search_module(GLM REQUIRED glm)
    endif(WIN32)
    if (GLM_FOUND)
        message("imgui glm found")
    endif(GLM_FOUND)
endif(PKG_CONFIG_FOUND)
else()
set(IMGUI_STATIC ON CACHE BOOL "Enable Static ImGui Build by Force" FORCE)
set(IMGUI_OPENGL ON CACHE BOOL "Enable ImGui OpenGL by Force" FORCE)
#set(IMGUI_SDL2 ON CACHE BOOL "Enable ImGui SDL2 backends by Force" FORCE)
set(IMGUI_GL2 OFF CACHE BOOL "Build ImGui OpenGL2 backends by Force" FORCE)
set(IMGUI_GL3 ON CACHE BOOL "Build ImGui OpenGL3 backends by Force" FORCE)
set(IMGUI_WGPU ON CACHE BOOL "Build ImGui wgpu backends by Force" FORCE)
set(IMGUI_VULKAN OFF CACHE BOOL "Disable ImGui Vulkan backends" FORCE)
set(IMGUI_VULKAN_SHADER OFF CACHE BOOL "Disable ImGui With Vulkan compute support" FORCE)
#set(IMGUI_GLFW ON CACHE BOOL "Enable ImGui GLFW support by Force" FORCE)
set(IMGUI_GLEW OFF CACHE BOOL "Disable ImGui GLEW support" FORCE)
set(IMGUI_GLUT OFF CACHE BOOL "Disable ImGui GLUT support" FORCE)
set(IMGUI_MATAL OFF CACHE BOOL "Disable ImGui Apple Matal support" FORCE)
set(IMGUI_DX9 OFF CACHE BOOL "Disable ImGui Windows DX9 support" FORCE)
set(IMGUI_DX10 OFF CACHE BOOL "Disable ImGui Windows DX10 support" FORCE)
set(IMGUI_DX11 OFF CACHE BOOL "Disable ImGui Windows DX11 support" FORCE)
set(IMGUI_DX12 OFF CACHE BOOL "Disable ImGui Windows DX12 support" FORCE)
set(IMGUI_FREETYPE OFF CACHE BOOL "Disable ImGui with FreeType support" FORCE)
set(IMGUI_APPS ON CACHE BOOL "Enable apps base on imgui" FORCE)
set(IMGUI_EMSCRIPTEN ON CACHE BOOL "Set Emscripten on by force" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2  -s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -Wno-unused-command-line-argument")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2  -s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -Wno-unused-command-line-argument")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pthreads-mem-growth -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pthreads-mem-growth -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
set(OPENGL_FOUND ON)
if (IMGUI_SDL2)
set(SDL2_FOUND ON)
endif (IMGUI_SDL2)
if (IMGUI_GLFW)
set(GLFW_FOUND ON)
endif (IMGUI_GLFW)
endif()

# base include
set(IMGUI_INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# base library
set(LINK_LIBS)

# Apple try to find openmp if we using clang
if (APPLE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang\$")
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_C_LIB_NAMES "omp")
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(OpenMP_C_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/opt/homebrew/opt/libomp/lib ${OpenMP_C_LIB_NAMES})
        else()
            set(OpenMP_C_INCLUDE_DIR "/usr/local/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/usr/local/opt/libomp/lib ${OpenMP_C_LIB_NAMES})
        endif()
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang\$")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_LIB_NAMES "omp")
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(OpenMP_CXX_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/opt/homebrew/opt/libomp/lib ${OpenMP_CXX_LIB_NAMES})
        else()
            set(OpenMP_CXX_INCLUDE_DIR "/usr/local/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/usr/local/opt/libomp/lib ${OpenMP_CXX_LIB_NAMES})
        endif()
    endif()
endif()

find_package(OpenMP)
if (APPLE AND OPENMP_FOUND)
    message(STATUS "imgui(Apple) Use OpenMP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    include_directories(${OpenMP_CXX_INCLUDE_DIR})
    set(LINK_LIBS ${LINK_LIBS} ${OpenMP_omp_LIBRARY})
elseif(OPENMP_FOUND)
    if(WIN32 OR MINGW)
        if (NOT IMGUI_STATIC)
            message(STATUS "imgui(Windows) Use OpenMP")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
            if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                set(LINK_LIBS ${LINK_LIBS} omp pthread)
            else ()
                set(LINK_LIBS ${LINK_LIBS} gomp pthread)
            endif ()
        endif()
    else()
        message(STATUS "imgui Use OpenMP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(LINK_LIBS ${LINK_LIBS} omp pthread)
        else ()
            set(LINK_LIBS ${LINK_LIBS} gomp pthread)
        endif ()
    endif()
endif()

if(OPENGL_FOUND)
    set(IMGUI_OPENGL ON)
    message(STATUS "    [ImGui with OpenGL support]")
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${OPENGL_INCLUDE_DIR}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${OPENGL_LIBRARIES}
    )
endif(OPENGL_FOUND)

if(GLM_FOUND)
    set(IMGUI_GLM ON)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${GLM_INCLUDE_DIRS}
    )
endif(GLM_FOUND)

if (OpenSSL_FOUND)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${OPENSSL_INCLUDE_DIR}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${OPENSSL_SSL_LIBRARY}
        ${OPENSSL_CRYPTO_LIBRARY}
    )
endif(OpenSSL_FOUND)

if(TIFF_FOUND)
    message("imgui found tiff")
    set(IMGUI_TIFF ON)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${TIFF_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${TIFF_LIBRARIES}
    )
endif(TIFF_FOUND)

if(IMGUI_FREETYPE AND FREETYPE_FOUND)
    set(IMGUI_ENABLE_FREETYPE ON)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${FREETYPE_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${FREETYPE_LIBRARIES}
    )
endif(IMGUI_FREETYPE AND FREETYPE_FOUND)

# base sources
set(IMGUI_SRC
    imgui.cpp 
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_tables.cpp
    imgui_texture.cpp
    imgui_helper.cpp
    immat.cpp
    misc/cpp/codewin.cpp
    misc/cpp/imgui_stdlib.cpp
    misc/cpp/dir_iterate.cpp
    misc/cpp/imgui_cpu.cpp
    misc/cpp/imgui_fft.cpp
    misc/cpp/imgui_socket.cpp
    misc/json/imgui_json.cpp
)

set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/json/
)

# base headers
set(IMGUI_INCS
    imgui.h
    imgui_internal.h
    imgui_texture.h
    imgui_helper.h
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
    immat.h
    imgui_user.h
    misc/cpp/imgui_stdlib.h
    misc/cpp/dir_iterate.h
    misc/cpp/imgui_cpu.h
    misc/cpp/imgui_fft.h
    misc/stb_image.h
    misc/stb_image_write.h
    misc/stb_image_resize2.h
    misc/DynObjectLoader.h
    misc/json/imgui_json.h
)

set(IMGUI_FONT_UTF8 WeiHei CACHE STRING "ImGui internal UTF8 font")
set_property(CACHE IMGUI_FONT_UTF8 PROPERTY STRINGS 
    None WeiHei BoldHei RegHei Yuan Song Kai Sans Zpix
)

set(IMGUI_FONT_LATIN MonoNarrow CACHE STRING "ImGui internal Latin font")
set_property(CACHE IMGUI_FONT_LATIN PROPERTY STRINGS 
    None MonoNarrow Inconsolata Tahoma Cousine-Regular DroidSans Karla-Regular Monaco ProggyTiny Roboto-Medium SourceCodePro-Regular
)

if (${IMGUI_FONT_UTF8} MATCHES "None")
    set(IMGUI_FONT_NO_UTF8 ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "Sans")
    set(IMGUI_FONT_SANS ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "WeiHei")
    set(IMGUI_FONT_WEIHEI ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "BoldHei")
    set(IMGUI_FONT_SARASA ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "RegHei")
    set(IMGUI_FONT_HEI ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "Yuan")
    set(IMGUI_FONT_YUAN ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "Song")
    set(IMGUI_FONT_SONG ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "Kai")
    set(IMGUI_FONT_KAI ON)
endif()
if (${IMGUI_FONT_UTF8} MATCHES "Zpix")
    set(IMGUI_FONT_ZPIX ON)
endif()

if (${IMGUI_FONT_LATIN} MATCHES "None")
    set(IMGUI_FONT_NO_LATIN ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "MonoNarrow")
    set(IMGUI_FONT_MONONARROW ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "Inconsolata")
    set(IMGUI_FONT_INCONSOLATA ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "Tahoma")
    set(IMGUI_FONT_TAHOMA ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "Cousine-Regular")
    set(IMGUI_FONT_COUSINE ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "DroidSans")
    set(IMGUI_FONT_DROIDSANS ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "Karla-Regular")
    set(IMGUI_FONT_KARLA ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "Monaco")
    set(IMGUI_FONT_MONACO ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "ProggyTiny")
    set(IMGUI_FONT_PROGGYTINY ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "Roboto-Medium")
    set(IMGUI_FONT_ROBOTO ON)
endif()
if (${IMGUI_FONT_LATIN} MATCHES "SourceCodePro-Regular")
    set(IMGUI_FONT_SOURCECODEPRO ON)
endif()

if(IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        imgui_demo.cpp 
    )
endif(IMGUI_BUILD_EXAMPLE)

if(WIN32)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        misc/cpp/dlfcn_win.cpp
        misc/cpp/mman_win.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        misc/dirent_portable.h
        misc/cpp/dlfcn_win.h
        misc/cpp/mman_win.h
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ws2_32
    )
endif(WIN32)

if(FREETYPE_FOUND AND IMGUI_FREETYPE)
    message(STATUS "    [ImGui with Freetype support]")
    set(IMGUI_SRC
        ${IMGUI_SRC}
        misc/freetype/imgui_freetype.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        misc/freetype/imgui_freetype.h
    )
endif(FREETYPE_FOUND AND IMGUI_FREETYPE)

# GLEW Loader support
if (IMGUI_GLEW AND GLEW_FOUND)
    message(STATUS "    [ImGui with GLEW support]")
    add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GLEW)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${GLEW_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${GLEW_LDFLAGS}
    )
endif(IMGUI_GLEW AND GLEW_FOUND)

# SDL2 support
if(SDL2_FOUND)
    message(STATUS "    [ImGui with SDL2 support]")
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${SDL2_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${SDL2_STATIC_LDFLAGS}
    )
    if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(REMOVE_ITEM LINK_LIBS "-mwindows")
    endif ()
endif(SDL2_FOUND)

# Find Backend Rendering
set(BACKEND_RENDERING NONE)

# Vulkan support
if (VULKAN_FOUND)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${VULKAN_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        #${Vulkan_LIBRARY}
        ${VULKAN_LDFLAGS}
    )
if (IMGUI_VULKAN)
    message(STATUS "    [ImGui backend rendering with Vulkan]")
    set(IMGUI_RENDERING_VULKAN ON)
    set(BACKEND_RENDERING VULKAN)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_vulkan.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_vulkan.h
    )
endif(IMGUI_VULKAN)
endif(VULKAN_FOUND)

# OpenGL
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
if(OPENGL_FOUND AND BACKEND_RENDERING MATCHES NONE)
    if(IMGUI_GL3)
        message(STATUS "    [ImGui backend rendering with OpenGL3]")
        set(IMGUI_RENDERING_GL3 ON)
        set(BACKEND_RENDERING OPENGL3)
        set(IMGUI_SRC
            ${IMGUI_SRC}
            backends/imgui_impl_opengl3.cpp
        )
        set(IMGUI_INCS
            ${IMGUI_INCS}
            backends/imgui_impl_opengl3.h
            backends/imgui_impl_opengl3_loader.h
        )
    elseif(IMGUI_GL2)
        message(STATUS "    [ImGui backend rendering with OpenGL2]")
        set(IMGUI_RENDERING_GL2 ON)
        set(BACKEND_RENDERING OPENGL2)
        set(IMGUI_SRC
            ${IMGUI_SRC}
            backends/imgui_impl_opengl2.cpp
        )
        set(IMGUI_INCS
            ${IMGUI_INCS}
            backends/imgui_impl_opengl2.h
        )
    endif()
endif()
else() # Emscripten
    set(IMGUI_RENDERING_GL3 ON)
    set(BACKEND_RENDERING OPENGL3)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_opengl3.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_opengl3.h
    )
endif()

# DirectX support
if(WIN32 AND BACKEND_RENDERING MATCHES NONE)
    if(IMGUI_DX12)
        message(STATUS "    [ImGui backend rendering with DX12]")
        set(IMGUI_RENDERING_DX12 ON)
        set(BACKEND_RENDERING DX12)
        set(LINK_LIBS
            ${LINK_LIBS}
            d3d12
        )
        set(IMGUI_SRC
            ${IMGUI_SRC}
            backends/imgui_impl_dx12.cpp
        )
        set(IMGUI_INCS
            ${IMGUI_INCS}
            backends/imgui_impl_dx12.h
        )
    elseif(IMGUI_DX11)
        message(STATUS "    [ImGui backend rendering with DX11]")
        set(IMGUI_RENDERING_DX11 ON)
        set(BACKEND_RENDERING DX11)
        set(LINK_LIBS
            ${LINK_LIBS}
            d3d11
        )
        set(IMGUI_SRC
            ${IMGUI_SRC}
            backends/imgui_impl_dx11.cpp
        )
        set(IMGUI_INCS
            ${IMGUI_INCS}
            backends/imgui_impl_dx11.h
        )
    elseif(IMGUI_DX10)
        message(STATUS "    [ImGui backend rendering with DX10]")
        set(IMGUI_RENDERING_DX10 ON)
        set(BACKEND_RENDERING DX10)
        set(LINK_LIBS
            ${LINK_LIBS}
            d3d10
        )
        set(IMGUI_SRC
            ${IMGUI_SRC}
            backends/imgui_impl_dx10.cpp
        )
        set(IMGUI_INCS
            ${IMGUI_INCS}
            backends/imgui_impl_dx10.h
        )
    elseif(IMGUI_DX9)
        message(STATUS "    [ImGui backend rendering with DX9]")
        set(IMGUI_RENDERING_DX9 ON)
        set(BACKEND_RENDERING DX9)
        set(LINK_LIBS
            ${LINK_LIBS}
            d3d9
        )
        set(IMGUI_SRC
            ${IMGUI_SRC}
            backends/imgui_impl_dx9.cpp
        )
        set(IMGUI_INCS
            ${IMGUI_INCS}
            backends/imgui_impl_dx9.h
        )
    endif(IMGUI_DX12)
    if(NOT BACKEND_RENDERING MATCHES NONE)
        set(LINK_LIBS
            ${LINK_LIBS}
            d3dcompiler
            dxgi
        )
    endif()
endif()

# Apple metal support
if (APPLE AND IMGUI_MATAL AND BACKEND_RENDERING MATCHES NONE)
    message(STATUS "    [ImGui backend rendering with Matal]")
    set(IMGUI_RENDERING_MATAL ON)
    set(BACKEND_RENDERING MATAL)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_metal.mm
        backends/imgui_impl_osx.mm
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_metal.h
        backends/imgui_impl_osx.h
    )
endif()

# Find Backend platform
# SDL2 Support
if (IMGUI_SDL2 AND SDL2_FOUND)
    message(STATUS "    [ImGui backend platform with SDL2]")
    set(IMGUI_PLATFORM_SDL2 ON)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_sdl2.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_sdl2.h
    )
    if (IMGUI_SDL2_RENDERER)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_sdlrenderer2.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_sdlrenderer2.h
    )
    endif(IMGUI_SDL2_RENDERER)
endif(IMGUI_SDL2 AND SDL2_FOUND)
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(IMGUI_PLATFORM_WGPU ON)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_wgpu.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_wgpu.h
    )
endif()

# GLFW support
if (IMGUI_GLFW AND GLFW_FOUND)
    message(STATUS "    [ImGui backend platform with GLFW]")
    set(IMGUI_PLATFORM_GLFW ON)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${GLFW_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${GLFW_LDFLAGS}
    )
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_glfw.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_glfw.h
    )
endif(IMGUI_GLFW AND GLFW_FOUND)

# GLUT support
if (IMGUI_GLUT AND GLUT_FOUND)
    message(STATUS "    [ImGui backend platform with GLUT]")
    set(IMGUI_PLATFORM_GLUT ON)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${GLUT_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${GLUT_LDFLAGS}
        ${GLUT_LIBRARY}
    )
    if (APPLE)
        set(LINK_LIBS
            ${LINK_LIBS}
            "-framework GLUT"
        )
    endif(APPLE)
    if (NOT APPLE AND NOT LINUX)
        set(LINK_LIBS
            ${LINK_LIBS}
            -lGL
        )
    endif(NOT APPLE AND NOT LINUX)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_glut.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_glut.h
    )
endif(IMGUI_GLUT AND GLUT_FOUND)

# Win32 Support
if(WIN32)
    message(STATUS "    [ImGui backend platform with Win32]")
    set(IMGUI_PLATFORM_WIN32 ON)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        backends/imgui_impl_win32.cpp
    )
    set(IMGUI_INCS
        ${IMGUI_INCS}
        backends/imgui_impl_win32.h
    )
endif(WIN32)

# Add Fonts
if (IMGUI_FONT_MONONARROW)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_narrow.cpp
    )
endif (IMGUI_FONT_MONONARROW)
if (IMGUI_FONT_INCONSOLATA)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/Inconsolata.cpp
    )
endif (IMGUI_FONT_INCONSOLATA)
if (IMGUI_FONT_TAHOMA)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/Tahoma.cpp
    )
endif (IMGUI_FONT_TAHOMA)
if (IMGUI_FONT_COUSINE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/Cousine-Regular.cpp
    )
endif (IMGUI_FONT_COUSINE)
if (IMGUI_FONT_DROIDSANS)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/DroidSans.cpp
    )
endif (IMGUI_FONT_DROIDSANS)
if (IMGUI_FONT_KARLA)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/Karla-Regular.cpp
    )
endif (IMGUI_FONT_KARLA)
if (IMGUI_FONT_MONACO)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/Monaco.cpp
    )
endif (IMGUI_FONT_MONACO)
if (IMGUI_FONT_PROGGYTINY)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/ProggyTiny.cpp
    )
endif (IMGUI_FONT_PROGGYTINY)
if (IMGUI_FONT_ROBOTO)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/Roboto-Medium.cpp
    )
endif (IMGUI_FONT_ROBOTO)
if (IMGUI_FONT_SOURCECODEPRO)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/SourceCodePro-Regular.cpp
    )
endif (IMGUI_FONT_SOURCECODEPRO)
if (IMGUI_FONT_HEI)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_hei.cpp
    )
endif (IMGUI_FONT_HEI)
if (IMGUI_FONT_KAI)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_kai.cpp
    )
endif (IMGUI_FONT_KAI)
if (IMGUI_FONT_SONG)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_song.cpp
    )
endif (IMGUI_FONT_SONG)
if (IMGUI_FONT_YUAN)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_yuan.cpp
    )
endif (IMGUI_FONT_YUAN)
if (IMGUI_FONT_SARASA)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_sarasa.cpp
    )
endif (IMGUI_FONT_SARASA)
if (IMGUI_FONT_WEIHEI)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_weihei.cpp
    )
endif(IMGUI_FONT_WEIHEI)
if (IMGUI_FONT_SANS)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_sans.cpp
    )
endif(IMGUI_FONT_SANS)
if (IMGUI_FONT_ZPIX)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        fonts/mono_zpix.cpp
    )
endif(IMGUI_FONT_ZPIX)
message(STATUS "    [ImGui Internal UTF-8 Font] ${IMGUI_FONT_UTF8}")
message(STATUS "    [ImGui Internal Latin Font] ${IMGUI_FONT_LATIN}")

# Add Icons
if (IMGUI_ICONS)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        icons/icons.cpp
        icons/folder.cpp
        icons/fontaudio.cpp
        icons/fa_brands.cpp
        icons/fa_regular.cpp
        icons/fa_solid.cpp
        icons/webfont.cpp
        icons/fork_webfont.cpp
        icons/material_icons.cpp
        icons/kenney.cpp
        icons/StepMath.cpp
        icons/Code.cpp
        icons/lucide.cpp
    )
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/icons
    )
endif(IMGUI_ICONS)

set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/misc
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/freetype
)

include_directories(${IMGUI_INC_DIRS})

# ImVulkanShader
if(IMGUI_VULKAN_SHADER)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    add_subdirectory(addon/ImVulkanShader)
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${VKSHADER_INCLUDE_DIRS}
    )
    set(LINK_LIBS
        ${LINK_LIBS}
        ${VKSHADER_LIBRARYS}
    )
endif(IMGUI_VULKAN_SHADER)

# Potrace
if (IMGUI_BUILD_POTRACE)
add_subdirectory(addon/Potrace)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/Potrace/inc
)
set(LINK_LIBS
    ${LINK_LIBS}
    ${POTRACE_LIBRARYS}
)
endif(IMGUI_BUILD_POTRACE)

# Dither
if (IMGUI_BUILD_DITHER)
add_subdirectory(addon/Dither)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/Dither/inc
)
set(LINK_LIBS
    ${LINK_LIBS}
    ${DITHER_LIBRARYS}
)
endif(IMGUI_BUILD_DITHER)

# ImGuiFileDialog
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/ImGuiFileDialog/ImGuiFileDialog.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/ImGuiFileDialog/ImGuiFileDialog.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiFileDialog
)
if (IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        addon/ImGuiFileDialog/ImGuiFileDialogDemo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# implot
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/implot/implot.cpp
    addon/implot/implot_items.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/implot/implot_internal.h
    addon/implot/implot.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/implot
)
if (IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        addon/implot/implot_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# Colorful Text Edit
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/TextEditor/TextEditor.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/TextEditor/TextEditor.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/TextEditor
)

# Extra Widget
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/imgui_extra_widget/imgui_extra_widget.cpp
    addon/imgui_extra_widget/imgui_extra_spinner.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/imgui_extra_widget/imgui_extra_widget.h
    addon/imgui_extra_widget/imgui_markdown.h
    addon/imgui_extra_widget/imgui_memory_editor.h
    addon/imgui_extra_widget/portable-file-dialogs.h
    addon/imgui_extra_widget/HotKey.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_extra_widget
)
if (IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        addon/imgui_extra_widget/imgui_extra_widget_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# Curve
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/ImCurve/imgui_curve.cpp
    addon/ImCurve/imgui_spline.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/ImCurve/imgui_curve.h
    addon/ImCurve/imgui_spline.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImCurve
)
if (IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        addon/ImCurve/imgui_curve_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# BaseUtils
set(BaseUtils_STATIC OFF)
add_subdirectory(addon/BaseUtils)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/BaseUtils/Include
)

# New Curve
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/ImNewCurve/ImNewCurve.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/ImNewCurve/ImNewCurve.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImNewCurve
)
if (IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        addon/ImNewCurve/ImNewCurveDemo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# ImMaskCreator
set(ImMaskCreator_STATIC OFF)
add_subdirectory(addon/ImMaskCreator)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImMaskCreator/Include
)

# Tab Window
set(IMGUI_SRC
    ${IMGUI_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiTabWindow/ImGuiTabWindow.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiTabWindow/ImGuiTabWindow.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiTabWindow
)
if(IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiTabWindow/ImGuiTabWindow_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# Node Editor
set(IMGUI_SRC
    ${IMGUI_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_canvas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_node_editor_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_node_editor.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_bezier_math.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_bezier_math.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_canvas.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_extra_math.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_extra_math.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_node_editor_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_node_editor_internal.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_node_editor.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor
)

if(IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/addon/node_editor/imgui_node_editor_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# ImGuiZmoQuat
set(IMGUI_SRC
    ${IMGUI_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmoQuat/ImGuiZMOquat.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmoQuat/ImGuiZMOquat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmoQuat/vgConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmoQuat/vGizmo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmoQuat/vgMath.h
)

set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmoQuat
)

if(IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmoQuat/ImGuiZMOquat_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# ImGuiZmo
set(IMGUI_SRC
    ${IMGUI_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/ImGuiOrient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/ImGuiZmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/model.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/ImGuiOrient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/ImGuiZmo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/model.h
)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
set(IMGUI_SRC
    ${IMGUI_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/mesh.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/mesh.h
)
endif()


set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo
)

if(IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiZmo/ImGuiZmo_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# imgui toggle
set(IMGUI_SRC
    ${IMGUI_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_palette.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_presets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_renderer.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_offset_rect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_math.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_palette.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_presets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_renderer.h
)

set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle
)

if(IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_toggle/imgui_toggle_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# imgui_tex_inspect
set(IMGUI_SRC
    ${IMGUI_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_tex_inspect/imgui_tex_inspect.cpp
)

set(IMGUI_INCS
    ${IMGUI_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_tex_inspect/imgui_tex_inspect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_tex_inspect/imgui_tex_inspect_internal.h
)

set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_tex_inspect
)

if(IMGUI_BUILD_EXAMPLE)
    set(IMGUI_SRC
        ${IMGUI_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/addon/imgui_tex_inspect/imgui_tex_inspect_demo.cpp
    )
endif(IMGUI_BUILD_EXAMPLE)

# ImCoolbar
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/ImCoolbar/ImCoolbar.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/ImCoolbar/ImCoolbar.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImCoolbar
)

# Style Serializer
set(IMGUI_SRC
    ${IMGUI_SRC}
    addon/ImGuiStyleSerializer/ImGuiStyleSerializer.cpp
)
set(IMGUI_INCS
    ${IMGUI_INCS}
    addon/ImGuiStyleSerializer/ImGuiStyleSerializer.h
)
set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/addon/ImGuiStyleSerializer
)

# ZLib
if(ZLIB_FOUND)
    add_definitions(-DIMGUI_USE_ZLIB)
    set(LINK_LIBS
        ${LINK_LIBS}
        ${ZLIB_LDFLAGS}
    )
endif(ZLIB_FOUND)

# Application Framework
set(IMGUI_APP_SRC)
if (IMGUI_APPS)
    if(BACKEND_RENDERING MATCHES VULKAN AND SDL2_FOUND AND IMGUI_SDL2)
        set(IMGUI_APPLICATION_RENDERING_VULKAN ON)
        set(IMGUI_APPLICATION_PLATFORM_SDL2 ON)
        message(STATUS "    [ImGui application platform with SDL2]")
        message(STATUS "    [ImGui application rendering with Vulkan]")
        set(IMGUI_APP_SRC
            ${IMGUI_APP_SRC}
            apps/application/source/vulkan/entry_sdl2.cpp
        )
    elseif(BACKEND_RENDERING MATCHES VULKAN AND GLFW_FOUND AND IMGUI_GLFW)
        set(IMGUI_APPLICATION_RENDERING_VULKAN ON)
        set(IMGUI_APPLICATION_PLATFORM_GLFW ON)
        message(STATUS "    [ImGui application platform with GLFW]")
        message(STATUS "    [ImGui application rendering with Vulkan]")
        set(IMGUI_APP_SRC
            ${IMGUI_APP_SRC}
            apps/application/source/vulkan/entry_glfw.cpp
        )
    elseif(BACKEND_RENDERING MATCHES VULKAN AND WIN32)
        set(IMGUI_APPLICATION_RENDERING_VULKAN ON)
        set(IMGUI_APPLICATION_PLATFORM_WIN32 ON)
        message(STATUS "    [ImGui application platform with Win32]")
        message(STATUS "    [ImGui application rendering with Vulkan]")
        set(IMGUI_APP_SRC
            ${IMGUI_APP_SRC}
            apps/application/source/vulkan/entry_win32.cpp
        )
    elseif(BACKEND_RENDERING MATCHES DX11)
        set(IMGUI_APPLICATION_RENDERING_DX11 ON)
        set(IMGUI_APPLICATION_PLATFORM_WIN32 ON)
        message(STATUS "    [ImGui application platform with Win32]")
        message(STATUS "    [ImGui application rendering with DX11]")
        set(IMGUI_APP_SRC
            ${IMGUI_APP_SRC}
            apps/application/source/dx11/entry.cpp
        )
    elseif(BACKEND_RENDERING MATCHES DX9)
        set(IMGUI_APPLICATION_RENDERING_DX9 ON)
        set(IMGUI_APPLICATION_PLATFORM_WIN32 ON)
        message(STATUS "    [ImGui application platform with Win32]")
        message(STATUS "    [ImGui application rendering with Dx9]")
        set(IMGUI_APP_SRC
            ${IMGUI_APP_SRC}
            apps/application/source/dx9/entry.cpp
        )
    elseif(BACKEND_RENDERING MATCHES OPENGL3)
        set(IMGUI_APPLICATION_RENDERING_GL3 ON)
        if(IMGUI_SDL2 AND SDL2_FOUND)
            set(IMGUI_APPLICATION_PLATFORM_SDL2 ON)
            message(STATUS "    [ImGui application platform with SDL2]")
            set(IMGUI_APP_SRC
                ${IMGUI_APP_SRC}
                apps/application/source/opengl3/entry_sdl2.cpp
            )
        elseif(IMGUI_GLFW AND GLFW_FOUND)
            set(IMGUI_APPLICATION_PLATFORM_GLFW ON)
            message(STATUS "    [ImGui application platform with GLFW]")
            set(IMGUI_APP_SRC
                ${IMGUI_APP_SRC}
                apps/application/source/opengl3/entry_glfw.cpp
            )
        elseif(WIN32)
            set(IMGUI_APPLICATION_PLATFORM_WIN32 ON)
            message(STATUS "    [ImGui application platform with Win32]")
            set(IMGUI_APP_SRC
                ${IMGUI_APP_SRC}
                apps/application/source/opengl3/entry_win32.cpp
            )
        endif()
        message(STATUS "    [ImGui application rendering with OpenGL3]")
    elseif(BACKEND_RENDERING MATCHES OPENGL2)
        set(IMGUI_APPLICATION_RENDERING_GL2 ON)
        if(SDL2_FOUND AND IMGUI_SDL2)
            set(IMGUI_APPLICATION_PLATFORM_SDL2 ON)
            message(STATUS "    [ImGui application platform with SDL2]")
            set(IMGUI_APP_SRC
                ${IMGUI_APP_SRC}
                apps/application/source/opengl2/entry_sdl2.cpp
            )   
        elseif(IMGUI_GLFW AND GLFW_FOUND)
            set(IMGUI_APPLICATION_PLATFORM_GLFW ON)
            message(STATUS "    [ImGui application platform with GLFW]")
            set(IMGUI_APP_SRC
                ${IMGUI_APP_SRC}
                apps/application/source/opengl2/entry_glfw.cpp
            )
        elseif(WIN32)
            set(IMGUI_APPLICATION_PLATFORM_WIN32 ON)
            message(STATUS "    [ImGui application platform with Win32]")
            set(IMGUI_APP_SRC
                ${IMGUI_APP_SRC}
                apps/application/source/opengl2/entry_win32.cpp
            )
        elseif(IMGUI_GLUT AND GLUT_FOUND)
            set(IMGUI_APPLICATION_PLATFORM_GLUT ON)
            message(STATUS "    [ImGui application platform with GLUT]")
            set(IMGUI_APP_SRC
                ${IMGUI_APP_SRC}
                apps/application/source/opengl2/entry_glut.cpp
            )
        endif()
        message(STATUS "    [ImGui application rendering with OpenGL2]")
    else()
        message(WARNING "No Application Backend Found! IMGUI_APPS will be turned off.")
        set(IMGUI_APPS OFF)
    endif()
    set(IMGUI_INCS
        ${IMGUI_INCS}
        apps/application/include/application.h
    )
    set(IMGUI_INC_DIRS
        ${IMGUI_INC_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/apps/application/include
    )
endif(IMGUI_APPS)

if(NOT IMGUI_STATIC)
set(IMGUI_SHARED_LIBRARY ON)
endif(NOT IMGUI_STATIC)
# Build library
configure_file(imconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/imconfig.h)

set(IMGUI_INC_DIRS
    ${IMGUI_INC_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)
include_directories(${IMGUI_INC_DIRS})

if(IMGUI_STATIC)
set(LIBRARY STATIC)
else(IMGUI_STATIC)
set(LIBRARY SHARED)
endif(IMGUI_STATIC)

set(IMGUI_VERSION_MAJOR 1)
set(IMGUI_VERSION_MINOR 91)
set(IMGUI_VERSION_PATCH 04)
string(TIMESTAMP IMGUI_VERSION_BUILD "%y%m%d")
set(IMGUI_VERSION_STRING ${IMGUI_VERSION_MAJOR}.${IMGUI_VERSION_MINOR}.${IMGUI_VERSION_PATCH})
add_definitions(-DIMGUI_VERSION_MAJOR=${IMGUI_VERSION_MAJOR})
add_definitions(-DIMGUI_VERSION_MINOR=${IMGUI_VERSION_MINOR})
add_definitions(-DIMGUI_VERSION_PATCH=${IMGUI_VERSION_PATCH})
add_definitions(-DIMGUI_VERSION_BUILD=${IMGUI_VERSION_BUILD})

add_library(imgui ${LIBRARY} ${IMGUI_SRC} ${IMGUI_INCS})
set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)
if(NOT IMGUI_STATIC)
target_link_libraries(imgui ${LINK_LIBS})
set_target_properties(imgui PROPERTIES VERSION ${IMGUI_VERSION_STRING} SOVERSION ${IMGUI_VERSION_MAJOR})
endif(NOT IMGUI_STATIC)
# Build Tools
if (APPLE AND IMGUI_MATAL AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(
        imgui 
        PUBLIC "-fobjc-arc"
    )
endif()

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
if (IMGUI_BUILD_EXAMPLE AND SDL2_FOUND AND IMGUI_SDL2)
    find_package(SDL2_image REQUIRED)
    if(SDL2_IMAGE_FOUND)
        message("sdl2 image found")
        add_executable(
            img2c
            misc/tools/img2c.cpp
        )
        target_link_libraries(
            img2c
            ${SDL2_IMAGE_LIBRARY}
            ${LINK_LIBS}
        )
        if(BACKEND_RENDERING MATCHES OPENGL3)
            add_executable(
                example_sdl2_rendering
                test/example_sdl2_rendering.cpp
            )
            target_link_libraries(
                example_sdl2_rendering
                imgui
                ${SDL2_IMAGE_LIBRARY}
                ${LINK_LIBS}
            )
        endif()
    endif(SDL2_IMAGE_FOUND)
    # Build utils
    add_executable(
        binary_to_compressed
        misc/fonts/binary_to_compressed_c.cpp
    )
    target_link_libraries(
        binary_to_compressed
        imgui
    )
    # Build examples
    if(BACKEND_RENDERING MATCHES OPENGL3)
        add_executable(
            example_sdl2_opengl3
            test/example_sdl2_opengl3.cpp
        )
        target_link_libraries(
            example_sdl2_opengl3
            imgui
            ${LINK_LIBS}
        )
    elseif(BACKEND_RENDERING MATCHES OPENGL2)
        add_executable(
            example_sdl2_opengl2
            test/example_sdl2_opengl2.cpp
        )
        target_link_libraries(
            example_sdl2_opengl2
            imgui
            ${LINK_LIBS}
        )
    endif(BACKEND_RENDERING MATCHES OPENGL3)

    # Build Vulkan examples
    if (BACKEND_RENDERING MATCHES VULKAN)
        add_executable(
            example_sdl2_vulkan
            test/example_sdl2_vulkan.cpp
        )
        target_link_libraries(
            example_sdl2_vulkan
            imgui
            ${LINK_LIBS}
        )
    endif(BACKEND_RENDERING MATCHES VULKAN)
    if(IMGUI_SDL2_RENDERER)
        add_executable(
            example_sdl2_sdlrenderer2
            test/example_sdl2_sdlrenderer2.cpp
        )
        target_link_libraries(
            example_sdl2_sdlrenderer2
            imgui
            ${LINK_LIBS}
        )
    endif(IMGUI_SDL2_RENDERER)
endif(IMGUI_BUILD_EXAMPLE AND SDL2_FOUND AND IMGUI_SDL2)

if (IMGUI_BUILD_EXAMPLE AND GLFW_FOUND AND IMGUI_GLFW AND BACKEND_RENDERING MATCHES VULKAN)
    add_executable(
        example_glfw_vulkan
        test/example_glfw_vulkan.cpp
    )
    target_link_libraries(
        example_glfw_vulkan
        imgui
        ${LINK_LIBS}
    )
endif(IMGUI_BUILD_EXAMPLE AND GLFW_FOUND AND IMGUI_GLFW AND BACKEND_RENDERING MATCHES VULKAN)

if (IMGUI_BUILD_EXAMPLE AND IMGUI_GLUT AND GLUT_FOUND AND BACKEND_RENDERING MATCHES OPENGL2)
    add_executable(
        example_glut_opengl2
        test/example_glut_opengl2.cpp
    )
    target_link_libraries(
        example_glut_opengl2
        imgui
        ${LINK_LIBS}
    )
endif(IMGUI_BUILD_EXAMPLE AND IMGUI_GLUT AND GLUT_FOUND AND BACKEND_RENDERING MATCHES OPENGL2)

if (IMGUI_BUILD_EXAMPLE AND GLFW_FOUND AND IMGUI_GLFW AND BACKEND_RENDERING MATCHES OPENGL3)
    add_executable(
        example_glfw_opengl3
        test/example_glfw_opengl3.cpp
    )
    target_link_libraries(
        example_glfw_opengl3
        imgui
        ${LINK_LIBS}
    )
endif(IMGUI_BUILD_EXAMPLE AND GLFW_FOUND AND IMGUI_GLFW AND BACKEND_RENDERING MATCHES OPENGL3)

if (IMGUI_BUILD_EXAMPLE AND APPLE AND IMGUI_MATAL AND BACKEND_RENDERING MATCHES MATAL)
    if (IMGUI_GLFW AND GLFW_FOUND)
        add_executable(
            example_glfw_metal
            test/example_glfw_metal.mm
        )
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(
            example_glfw_metal 
            PUBLIC "-fobjc-arc"
        )
        endif()
        target_link_libraries(
            example_glfw_metal
            imgui
            ${LINK_LIBS}
            "-framework Metal"
            "-framework MetalKit"
            "-framework Cocoa"
            "-framework QuartzCore"
        )
    endif(IMGUI_GLFW AND GLFW_FOUND)
endif(IMGUI_BUILD_EXAMPLE AND APPLE AND IMGUI_MATAL AND BACKEND_RENDERING MATCHES MATAL)

if(IMGUI_BUILD_EXAMPLE AND WIN32)
    if(BACKEND_RENDERING MATCHES DX12)
        add_executable(
            example_win32_directx12
            test/example_win32_directx12.cpp
        )
        target_link_libraries(
            example_win32_directx12
            imgui
            ${LINK_LIBS}
        )
    elseif(BACKEND_RENDERING MATCHES DX11)
        add_executable(
            example_win32_directx11
            test/example_win32_directx11.cpp
        )
        target_link_libraries(
            example_win32_directx11
            imgui
            ${LINK_LIBS}
        )
    elseif(BACKEND_RENDERING MATCHES DX10)
        add_executable(
            example_win32_directx10
            test/example_win32_directx10.cpp
        )
        target_link_libraries(
            example_win32_directx10
            imgui
            ${LINK_LIBS}
        )
    elseif(BACKEND_RENDERING MATCHES DX9)
        add_executable(
            example_win32_directx9
            test/example_win32_directx9.cpp
        )
        target_link_libraries(
            example_win32_directx9
            imgui
            ${LINK_LIBS}
        )
    endif()
endif(IMGUI_BUILD_EXAMPLE AND WIN32)
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")

if (IMGUI_BUILD_EXAMPLE AND IMGUI_APPS)
    add_executable(
        example_application
        test/example_application.cpp
        ${IMGUI_APP_SRC}
    )
    target_link_libraries(
        example_application
        imgui
        ${LINK_LIBS}
    )
    add_executable(
        example_objviewer
        test/example_objviewer.cpp
        ${IMGUI_APP_SRC}
    )
    target_link_libraries(
        example_objviewer
        imgui
        ${LINK_LIBS}
    )
    set (FONT_SRC test/show_fonts.cpp)
    add_executable(
        show_fonts
        ${FONT_SRC}
        ${IMGUI_APP_SRC}
    )
    target_link_libraries(
        show_fonts
        imgui
        ${LINK_LIBS}
    )
    add_executable(ImMaskCreatorTest
        addon/ImMaskCreator/Src/ImMaskCreatorTest.cpp
        ${IMGUI_APP_SRC}
    )
    target_link_libraries(ImMaskCreatorTest PRIVATE
        ImMaskCreator
    )
    if (IMGUI_BUILD_DITHER)
        add_executable(
            DitherTest
            test/DitherTest.cpp
            ${IMGUI_APP_SRC}
        )

        target_link_libraries(
            DitherTest
            imgui
            ${LINK_LIBS}
        )
    endif(IMGUI_BUILD_DITHER)
    if (IMGUI_BUILD_POTRACE)
        add_executable(
            PotraceTest
            test/PotraceTest.cpp
            ${IMGUI_APP_SRC}
        )
        target_link_libraries(
            PotraceTest
            imgui
            ${LINK_LIBS}
        )
    endif(IMGUI_BUILD_POTRACE)
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(IMGUI_SHELL_FILE ${PROJECT_SOURCE_DIR}/test/shell_minimal.html)
    set(IMGUI_WEB_DIR ${PROJECT_BINARY_DIR}/web/)
    set_target_properties(
        example_application
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${IMGUI_WEB_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${IMGUI_WEB_DIR}
        OUTPUT_NAME "index"
        PREFIX ""
        SUFFIX ".html"
        LINK_FLAGS "--shell-file ${IMGUI_SHELL_FILE}"
    )
endif()
endif(IMGUI_BUILD_EXAMPLE AND IMGUI_APPS)

if (IMGUI_BUILD_EXAMPLE AND ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(IMGUI_WEB_WGPU_DIR ${PROJECT_BINARY_DIR}/web_wgpu/)
    SET(IMGUI_SHELL_WGPU_FILE ${PROJECT_SOURCE_DIR}/test/wgpu_index.html)
    add_executable(
        example_glfw_wgpu
        test/example_glfw_wgpu.cpp
    )
    target_link_libraries(
        example_glfw_wgpu
        imgui
    )
    set_target_properties(
        example_glfw_wgpu
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${IMGUI_WEB_WGPU_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${IMGUI_WEB_WGPU_DIR}
        OUTPUT_NAME "index"
        PREFIX ""
        SUFFIX ".html"
        LINK_FLAGS "--shell-file ${IMGUI_SHELL_WGPU_FILE}"
    )
endif()

if (IMGUI_BUILD_EXAMPLE AND IMGUI_VULKAN_SHADER AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
add_executable(
    vulkan_shader_test
    test/vulkan_shader_test.cpp
)
target_link_libraries(
    vulkan_shader_test
    ${VKSHADER_LIBRARYS}
)
endif()

if (IMGUI_BUILD_EXAMPLE AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
add_executable(
    immat_test
    test/immat_test.cpp
)
target_link_libraries(
    immat_test
    imgui
)
add_executable(
    img2cc
    misc/tools/img2cc.cpp
)
target_link_libraries(
    img2cc
    imgui
)
endif()

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(IMGUI_LIBRARYS imgui ${LINK_LIBS} PARENT_SCOPE )
    set(IMGUI_INCLUDES ${IMGUI_INCS} PARENT_SCOPE )
    set(IMGUI_INCLUDE_DIRS ${IMGUI_INC_DIRS} PARENT_SCOPE )
    set(IMGUI_APP_ENTRY_SRC imgui/${IMGUI_APP_SRC} PARENT_SCOPE )
    set(IM_MASK_CREATOR_STATIC_LIB ${ImMaskCreator_STATIC} PARENT_SCOPE )
endif()
if(UNIX AND NOT IMGUI_SKIP_INSTALL)
install(TARGETS imgui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()