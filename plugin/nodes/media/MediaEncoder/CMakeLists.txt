cmake_minimum_required(VERSION 3.12.0)
project(media_encoder_node)
set(CMAKE_CXX_STANDARD 17)
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

set (LINK_LIBS
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
)
set(PLUGIN MediaEncoder)

add_library(
    ${PLUGIN}
    SHARED
    ffmpeg_encoder_node.cpp
    ffmedia/ffmedia_sink.cpp
    ffmedia/ffmedia_stream.cpp
    ffmedia/ffmedia_utils.cpp
    ffmedia/fps_filter.cpp
    ffmedia/ffmedia_common.h
    ffmedia/ffmedia_define.h
    ffmedia/ffmedia_queue.h
    ffmedia/ffmedia_utils.h
    ffmedia/ffmedia.h
    ffmedia/fps_filter.h
)

set_property(TARGET ${PLUGIN} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(
    ${PLUGIN}
    ${LINK_LIBS}
)

set_target_properties(
    ${PLUGIN}
    PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_FOLDER}/media/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_FOLDER}/media/"
)
