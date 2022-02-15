cmake_minimum_required(VERSION 3.5.1)
project(MediaEditor)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_MACOSX_RPATH 0)
set(CMAKE_INCLUDE_CURRENTDIR ON)
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()

option(BUILD_TEST  "Build Test Application" ON)

# Find the cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(MacroLogFeature)

find_package(PkgConfig REQUIRED)

set(IMGUI_ICONS ON CACHE BOOL "Enable Internal Icons Build by Force" FORCE)
add_subdirectory(imgui)
set_target_properties(
    imgui
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

if(IMGUI_VULKAN AND IMGUI_VULKAN_SHADER)
set_target_properties(
    VkShader
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
endif()

include_directories(
    ${IMGUI_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}
)

set(IMGUI_BP_SDK_MEDIA_NODE ON CACHE BOOL "build media Built-in node" FORCE)
add_definitions(-DIMGUI_BP_SDK_MEDIA_NODE)
add_subdirectory(blueprintsdk)
set_target_properties(
    BluePrintSDK
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

include_directories(
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
)

# Basics
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

pkg_search_module(SDL2 REQUIRED sdl2)
pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image>=2.0.0)
macro_log_feature(SDL2_FOUND "SDL2" "Simple DirectMedia Layer framework" "https://www.libsdl.org" TRUE)
macro_log_feature(SDL2_IMAGE_FOUND "SDL2 Image" "Simple DirectMedia Layer image framework" "https://www.libsdl.org" TRUE)

#
# FFMPEG
#
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
    macro_log_feature(FFMPEG_FOUND "FFMpeg" "Multimedia framework" "https://ffmpeg.org" TRUE)
endif(PKG_CONFIG_FOUND)

macro_display_feature_log()

#
#  Application
#
set(MEDIA_EDITOR_BINARY "me")
set(MEDIA_EDITOR_SRCS
    MediaEditor.cpp
    MediaTimeline.cpp
    MediaOverview.cpp
    MediaSnapshot.cpp
    MediaParser.cpp
    MediaReader.cpp
    AudioRender_Impl_Sdl2.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)

set(MEDIA_EDITOR_INCS
    MediaTimeline.h
    MediaOverview.h
    MediaSnapshot.h
    MediaReader.h
    AudioRender_Impl_Sdl2.hpp
    AudioRender.hpp
    FFUtils.h
)

add_executable(
    ${MEDIA_EDITOR_BINARY}
    ${MEDIA_EDITOR_SRCS}
    ${MEDIA_EDITOR_INCS}
)
target_include_directories(
    ${MEDIA_EDITOR_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_EDITOR_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_BINARY}")
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_BINARY} 
    LINK_PRIVATE
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
    Threads::Threads
)

if(BUILD_TEST)
# MediaEditor Test
set(MEDIA_EDITOR_TEST_BINARY "met")
set(MEDIA_EDITOR_TEST_SRCS
    MediaEditorTest.cpp
    ImSequencer.cpp
    MediaOverview.cpp
    MediaSnapshot.cpp
    MediaParser.cpp
    MediaReader.cpp
    AudioRender_Impl_Sdl2.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)

set(MEDIA_EDITOR_TEST_INCS
    ImSequencer.h
    MediaOverview.h
    MediaSnapshot.h
    MediaReader.h
    AudioRender_Impl_Sdl2.hpp
    AudioRender.hpp
    FFUtils.h
)
add_executable(
    ${MEDIA_EDITOR_TEST_BINARY}
    ${MEDIA_EDITOR_TEST_SRCS}
    ${MEDIA_EDITOR_TEST_INCS}
)
target_include_directories(
    ${MEDIA_EDITOR_TEST_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_EDITOR_TEST_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_TEST_BINARY}")
set_property(TARGET ${MEDIA_EDITOR_TEST_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_EDITOR_TEST_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_TEST_BINARY} 
    LINK_PRIVATE
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
    Threads::Threads
)
# MediaPlayer Test
add_executable(
    media_player_test
    test/MediaPlayerTest.cpp
    test/MediaPlayer.cpp
    test/ImGuiToolkit.cpp
    test/Log.cpp
    AudioRender.hpp
    AudioRender_Impl_Sdl2.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    media_player_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
)
target_include_directories(
    media_player_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# MediaSnapshot Test
add_executable(
    media_snapshot_test
    test/MediaSnapshotTest.cpp
    MediaSnapshot.cpp
    MediaOverview.cpp
    MediaParser.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    media_snapshot_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
)
target_include_directories(
    media_snapshot_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# MediaOverview Test
add_executable(
    media_overview_test
    test/MediaOverviewTest.cpp
    MediaOverview.cpp
    MediaParser.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    media_overview_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
)
target_include_directories(
    media_overview_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# MediaReader Test
add_executable(
    media_reader_test
    test/MediaReaderTest.cpp
    MediaReader.cpp
    MediaParser.cpp
    AudioRender_Impl_Sdl2.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    media_reader_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
)
target_include_directories(
    media_reader_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# MediaEncoder Test
add_executable(
    media_encoder_test
    test/MediaEncoderTest.cpp
    MediaEncoder.cpp
    MediaReader.cpp
    MediaParser.cpp
    FFUtils.cpp
    Logger.cpp
)
target_link_libraries(
    media_encoder_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
)
target_include_directories(
    media_encoder_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
endif(BUILD_TEST)
