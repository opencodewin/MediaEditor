cmake_minimum_required(VERSION 3.5.1)
project(mediasource_node)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_MACOSX_RPATH 0)
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()

find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(
        FFMPEG IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswresample
        libavfilter
        libswscale
        libavdevice
    )
endif(PKG_CONFIG_FOUND)

add_library(
    MediaSource
    SHARED
    ImMediaSourceNode.cpp
)

set_property(TARGET MediaSource PROPERTY POSITION_INDEPENDENT_CODE ON)
set (LINK_LIBS
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
)

if (FFMPEG_FOUND)
    add_compile_definitions(SDK_WITH_FFMPEG)
    set(LINK_LIBS
        ${LINK_LIBS}
        PkgConfig::FFMPEG
    )
endif(FFMPEG_FOUND)

target_link_libraries(
    MediaSource
    ${LINK_LIBS}
)

set_target_properties(
    MediaSource
    PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/media/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/media/"
)