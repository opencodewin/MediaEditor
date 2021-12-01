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

# Find the cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(MacroLogFeature)
include(MacroFindGStreamerLibrary)

find_package(PkgConfig REQUIRED)

set(IMGUI_ICONS ON CACHE BOOL "Enable Internal Icons Build by Force" FORCE)
add_subdirectory(imgui)
set_target_properties(
    imgui
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

include_directories(
    ${IMGUI_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}
)

# Basics
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

find_package(GLIB2)
macro_log_feature(GLIB2_FOUND "GLib" "GTK general-purpose utility library" "http://www.gtk.org" TRUE)

find_package(GObject)
macro_log_feature(GOBJECT_FOUND "GObject" "GTK object-oriented framework" "http://www.gtk.org" TRUE)

pkg_search_module(SDL2 REQUIRED sdl2)
pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image>=2.0.0)
#
# GSTREAMER
#

find_package(GStreamer 1.0.0 COMPONENTS base)
macro_log_feature(GSTREAMER_FOUND "GStreamer"
"Open Source Multiplatform Multimedia Framework"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

find_package(GStreamerPluginsBase 1.0.0 COMPONENTS app audio video pbutils gl)
macro_log_feature(GSTREAMER_APP_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer app library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_AUDIO_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer audio library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_VIDEO_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer video library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_PBUTILS_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer pbutils library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_GL_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer opengl library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

# Various preprocessor definitions for GST
add_definitions(-DGST_DISABLE_XML -DGST_DISABLE_LOADSAVE)

macro_display_feature_log()

#
#  Application
#

set(MEDIA_EDITOR_BINARY "me")
set(MEDIA_EDITOR_SRCS
    main.cpp
    ImSequencer.cpp
    ${IMGUI_APP_ENTRY_SRC}
)

set(MEDIA_EDITOR_INCS
    ImSequencer.h
)

add_executable(
    ${MEDIA_EDITOR_BINARY}
    ${MEDIA_EDITOR_SRCS}
    ${MEDIA_EDITOR_INCS}
)
target_include_directories(
    ${MEDIA_EDITOR_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIR}
    ${GSTREAMER_INCLUDE_DIR}
    ${GSTREAMER_AUDIO_INCLUDE_DIR}
    ${GSTREAMER_VIDEO_INCLUDE_DIR}
    ${GSTREAMER_BASE_INCLUDE_DIR}
    ${GSTREAMER_APP_INCLUDE_DIR}
    ${GSTREAMER_PBUTILS_INCLUDE_DIR}
    ${GSTREAMER_GL_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_EDITOR_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_BINARY}")
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_BINARY} 
    LINK_PRIVATE
    ${IMGUI_LIBRARYS}
    ${GOBJECT_LIBRARIES}
    ${GSTREAMER_LIBRARY}
    ${GSTREAMER_BASE_LIBRARY}
    ${GSTREAMER_APP_LIBRARY}
    ${GSTREAMER_AUDIO_LIBRARY}
    ${GSTREAMER_VIDEO_LIBRARY}
    ${GSTREAMER_PBUTILS_LIBRARY}
    ${GSTREAMER_GL_LIBRARY}
    Threads::Threads
)

set(MEDIA_PLAYER_BINARY "mp")
set(MEDIA_PLAYER_SRCS
    player.cpp
    SystemToolkit.cpp
    MediaPlayer.cpp
    BaseToolkit.cpp
    GstToolkit.cpp
    ImGuiToolkit.cpp
    Timeline.cpp
    Log.cpp
    ${IMGUI_APP_ENTRY_SRC}
)

set(MEDIA_PLAYER_INCS
    SystemToolkit.h
    MediaPlayer.h
    BaseToolkit.h
    GstToolkit.h
    ImGuiToolkit.h
    Timeline.h
    Log.h
)

add_executable(
    ${MEDIA_PLAYER_BINARY}
    ${MEDIA_PLAYER_SRCS}
    ${MEDIA_PLAYER_INCS}
)
target_include_directories(
    ${MEDIA_PLAYER_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIR}
    ${GSTREAMER_INCLUDE_DIR}
    ${GSTREAMER_AUDIO_INCLUDE_DIR}
    ${GSTREAMER_VIDEO_INCLUDE_DIR}
    ${GSTREAMER_BASE_INCLUDE_DIR}
    ${GSTREAMER_APP_INCLUDE_DIR}
    ${GSTREAMER_PBUTILS_INCLUDE_DIR}
    ${GSTREAMER_GL_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_PLAYER_BINARY} PUBLIC APP_NAME="${MEDIA_PLAYER_BINARY}")
set_property(TARGET ${MEDIA_PLAYER_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_PLAYER_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_PLAYER_BINARY} 
    LINK_PRIVATE
    ${IMGUI_LIBRARYS}
    ${GOBJECT_LIBRARIES}
    ${GSTREAMER_LIBRARY}
    ${GSTREAMER_BASE_LIBRARY}
    ${GSTREAMER_APP_LIBRARY}
    ${GSTREAMER_AUDIO_LIBRARY}
    ${GSTREAMER_VIDEO_LIBRARY}
    ${GSTREAMER_PBUTILS_LIBRARY}
    ${GSTREAMER_GL_LIBRARY}
    Threads::Threads
)

add_executable(
    mp_ff
    player_ff.cpp
    MediaPlayer_FFImpl.cpp
    SystemToolkit.cpp
    ImGuiToolkit_NoGst.cpp
    Log_NoGst.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    mp_ff
    imgui
    avformat
    avutil
    avcodec
)
target_compile_definitions(mp_ff PUBLIC APP_NAME="mp_ff")