cmake_minimum_required(VERSION 3.5.1)
project(matrgb2yuv_node)
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

set(PLUGIN MatRGB2YUV)
add_library(
    ${PLUGIN}
    SHARED
    ImMatRGB2YUVNode.cpp
)

add_dependencies(${PLUGIN} BluePrintSDK VkShader imgui)
set_property(TARGET ${PLUGIN} PROPERTY POSITION_INDEPENDENT_CODE ON)
set (LINK_LIBS
    -L${CMAKE_BINARY_DIR}
    -lBluePrintSDK
    -lVkShader
    -limgui
)
if (FFMPEG_FOUND)
    add_compile_definitions(SDK_WITH_FFMPEG)
    set(LINK_LIBS
        ${LINK_LIBS}
        PkgConfig::FFMPEG
    )
endif(FFMPEG_FOUND)

target_link_libraries(
    ${PLUGIN}
    ${LINK_LIBS}
)

set_target_properties(
    ${PLUGIN}
    PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/media/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/media/"
)