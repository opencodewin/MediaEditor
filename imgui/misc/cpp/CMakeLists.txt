cmake_minimum_required(VERSION 3.12.0)
project(MediaAIProject)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

if(IOS AND CMAKE_OSX_ARCHITECTURES MATCHES "arm")
    message(STATUS "Target arch: arm-ios")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    message(STATUS "Target arch: arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|x86_64|AMD64)")
    message(STATUS "Target arch: x86")
    if(MSVC OR MSVC_IDE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2 /arch:AVX /arch:FMA /arch:SSE /arch:SSE2 /arch:SSSE3 /arch:SSE4.1 /arch:SSE4.2")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX2 /arch:AVX /arch:FMA /arch:SSE /arch:SSE2 /arch:SSSE3 /arch:SSE4.1 /arch:SSE4.2")
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        ### we disable wasm simd support because safari isn't support it yet
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse -mrelaxed-simd")
        #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse -mrelaxed-simd")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mavx")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -mavx")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
    endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2  -s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -Wno-unused-command-line-argument")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2  -s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -Wno-unused-command-line-argument")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pthreads-mem-growth -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pthreads-mem-growth -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")

if(APPLE)
#add_link_options(-ld_classic)
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath -Wl,/usr/local/lib -Wl,-no_warn_duplicate_libraries")
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath -Wl,/usr/local/lib -Wl,-no_warn_duplicate_libraries")
endif(APPLE)

set(APP_TYPE MediaEditor CACHE STRING "Build Application Type")
set_property(CACHE APP_TYPE PROPERTY STRINGS 
    MediaUpscale MediaEditor MediaInpaint
)

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

include_directories(
    ${IMGUI_INCLUDE_DIRS}
)

add_subdirectory(imgui_addon)
set_target_properties(
    imgui_addons
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

include_directories(
    ${IMGUI_ADDONS_INCLUDE_DIRS}
)

if(IMGUI_VULKAN_SHADER)
set_target_properties(
    VkShader
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
endif()

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}
)

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

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
option(BUILD_MEDIACORE_TEST              "Build MediaCore test" OFF)
add_subdirectory(MediaCore)
set_target_properties(
    MediaCore
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
set(LINK_LIBS)

## Base librarys
if (${APP_TYPE} MATCHES "MediaUpscale")
if(APPLE OR WIN32)
set(NCNN_SYSTEM_GLSLANG OFF CACHE BOOL "Build with internal glslang by Force" FORCE)
else()
set(NCNN_SYSTEM_GLSLANG ON CACHE BOOL "Build with internal glslang by Force" FORCE)
endif(APPLE OR WIN32)
set(NCNN_SHARED_LIB ON CACHE BOOL "Build ncnn as shared library by Force" FORCE)
set(NCNN_INSTALL_SDK OFF CACHE BOOL "Skip ncnn sdk install by Force" FORCE)
add_subdirectory(ncnn)
set_target_properties(
    ncnn
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
include_directories(${NCNN_INCLUDE_DIRS})

## build ncnn conponents
add_subdirectory(ncnn_components/RealSR)
include_directories(
    ${REALSR_INCLUDE_DIRS}
)

add_subdirectory(ncnn_components/FaceDetection)
include_directories(
    ${FACEDETECTION_INCLUDE_DIRS}
)

add_subdirectory(ncnn_components/CodeFormer)
include_directories(
    ${CODEFORMER_INCLUDE_DIRS}
)
elseif(${APP_TYPE} MATCHES "MediaInpaint")
if(APPLE OR WIN32)
set(NCNN_SYSTEM_GLSLANG OFF CACHE BOOL "Build with internal glslang by Force" FORCE)
else()
set(NCNN_SYSTEM_GLSLANG ON CACHE BOOL "Build with internal glslang by Force" FORCE)
endif(APPLE OR WIN32)
set(NCNN_SHARED_LIB ON CACHE BOOL "Build ncnn as shared library by Force" FORCE)
set(NCNN_INSTALL_SDK OFF CACHE BOOL "Skip ncnn sdk install by Force" FORCE)
add_subdirectory(ncnn)
set_target_properties(
    ncnn
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
include_directories(${NCNN_INCLUDE_DIRS})

add_subdirectory(ncnn_components/SegmentAnything)
include_directories(
    ${SEGMENTANYTHING_INCLUDE_DIRS}
)

add_subdirectory(ncnn_components/Inpainting)
include_directories(
    ${INPAINTING_INCLUDE_DIRS}
)

add_subdirectory(ncnn_components/NanoTrack)
include_directories(
    ${NANOTRACK_INCLUDE_DIRS}
)

set(LINK_LIBS
    ${LINK_LIBS}
    ${INPAINTING_LIBRARYS}
    ${SEGMENTANYTHING_LIBRARYS}
    ${NANOTRACK_LIBRARYS}
)
endif()

# Basics
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

pkg_search_module(SDL2 REQUIRED sdl2)
macro_log_feature(SDL2_FOUND "SDL2" "Simple DirectMedia Layer framework" "https://www.libsdl.org" TRUE)

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

message("**********************************************************\n          MeAI application: " ${APP_TYPE} "\n**********************************************************\n")

option(UI_PERFORMANCE_ANALYSIS "Enable time analysis code to monitor UI actions." OFF)
add_compile_options(-Wno-ignored-attributes -Wno-inconsistent-dllimport -Wno-deprecated-declarations)

#
#  Application
#
if (${APP_TYPE} MATCHES "MediaUpscale")
set(APP_BINARY "MediaUpscale")
set(APP_L "mu")
set(APP_S ".mup")
set(PLUGIN_FOLDER "mu")
set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 0)
set(APP_VERSION_PATCH 1)
elseif(${APP_TYPE} MATCHES "MediaInpaint")
set(APP_BINARY "MediaInpaint")
set(APP_L "mi")
set(APP_S ".mip")
set(PLUGIN_FOLDER "mi")
set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 0)
set(APP_VERSION_PATCH 1)
elseif(${APP_TYPE} MATCHES "MediaEditor")
set(APP_BINARY "MediaEditor")
set(APP_L "me")
set(APP_S ".mep")
set(PLUGIN_FOLDER "me")
set(APP_VERSION_MAJOR 1)
set(APP_VERSION_MINOR 0)
set(APP_VERSION_PATCH 0)
endif()

set(BASE_SRCS
    MediaEngine/Engine.cpp
    MediaEngine/Overlap.cpp
    MediaEngine/Track.cpp
    MediaEngine/Event.cpp
    MediaEngine/EventTrack.cpp
    MediaEngine/EventStack.cpp
    MediaEngine/Clip.cpp
    MediaEngine/Video_Clip.cpp
    MediaEngine/Audio_Clip.cpp
    MediaEngine/Text_Clip.cpp
    MediaEngine/VideoTransition.cpp
    MediaEngine/AudioTransition.cpp
    MediaEngine/EditingClip.cpp
    MediaEngine/EditingOverlap.cpp
    MediaEngine/EditingItem.cpp
    MediaEngine/VideoTransformFilterUiCtrl.cpp
    MediaEngine/BackgroundTask.cpp
    MediaEngine/BgtaskVidstab.cpp
    MediaEngine/BgtaskSceneDetect.cpp
    MediaEngine/Item.cpp
    MediaEngine/Player.cpp
    MediaEngine/Project.cpp
    MediaEngine/MediaUtils.cpp
)

set(BASE_INCS
    MediaEngine/Engine.h
    MediaEngine/Overlap.h
    MediaEngine/Track.h
    MediaEngine/Event.h
    MediaEngine/EventTrack.h
    MediaEngine/EventStack.h
    MediaEngine/Clip.h
    MediaEngine/Video_Clip.h
    MediaEngine/Audio_Clip.h
    MediaEngine/Text_Clip.h
    MediaEngine/VideoTransition.h
    MediaEngine/AudioTransition.h
    MediaEngine/EditingClip.h
    MediaEngine/EditingOverlap.h
    MediaEngine/EditingItem.h
    MediaEngine/VideoTransformFilterUiCtrl.h
    MediaEngine/BackgroundTask.h
    MediaEngine/Item.h
    MediaEngine/Player.h
    MediaEngine/Project.h
    MediaEngine/IDGenerator.h
    MediaEngine/MediaUtils.h
)

if (${APP_TYPE} MATCHES "MediaUpscale")
set(APP_SRCS
    MediaUpscale.cpp
    SingleClipTimeline.cpp
    MediaOutputView.cpp
    Resources.cpp
    ${BASE_SRCS}
    ${IMGUI_APP_ENTRY_SRC}
)
set(APP_INCS
    SingleClipTimeline.h
    MediaOutputView.h
    ${BASE_INCS}
)
elseif (${APP_TYPE} MATCHES "MediaInpaint")
set(APP_SRCS
    MediaInpaint.cpp
    SingleClipTimeline.cpp
    MediaOutputView.cpp
    Resources.cpp
    ${BASE_SRCS}
    ${IMGUI_APP_ENTRY_SRC}
)
set(APP_INCS
    SingleClipTimeline.h
    MediaOutputView.h
    ${BASE_INCS}
)
elseif(${APP_TYPE} MATCHES "MediaEditor")
set(APP_SRCS
    MediaEditor.cpp
    MainTimeline.cpp
    MediaBankView.cpp
    MediaOutputView.cpp
    MediaPreviewView.cpp
    AudioMixingView.cpp
    EditingClipView.cpp
    EditingTransitionView.cpp
    EditingTextView.cpp
    ClipTimeline.cpp
    OverlapTimeLine.cpp
    Resources.cpp
    ${BASE_SRCS}
    ${IMGUI_APP_ENTRY_SRC}
)
set(APP_INCS
    MainTimeline.h
    MediaBankView.h
    MediaOutputView.h
    MediaPreviewView.h
    AudioMixingView.h
    EditingClipView.h
    EditingTransitionView.h
    EditingTextView.h
    ClipTimeline.h
    OverlapTimeLine.h
    ${BASE_INCS}
)
endif()

include_directories(${CMAKE_SOURCE_DIR}/MediaEngine)

if(${APP_TYPE} MATCHES "MediaEditor")
set(APP_VERSION_BUILD "-4")
elseif (${APP_TYPE} MATCHES "MediaUpscale")
string(TIMESTAMP APP_VERSION_BUILD "%y%m%d")
elseif (${APP_TYPE} MATCHES "MediaInpaint")
string(TIMESTAMP APP_VERSION_BUILD "%y%m%d")
endif()

set(APP_VERSION_STRING ${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH})
add_definitions(-DAPP_VERSION_MAJOR=${APP_VERSION_MAJOR})
add_definitions(-DAPP_VERSION_MINOR=${APP_VERSION_MINOR})
add_definitions(-DAPP_VERSION_PATCH=${APP_VERSION_PATCH})
add_definitions(-DAPP_VERSION_BUILD=${APP_VERSION_BUILD})

if (IMGUI_APPS)
if(APPLE)
set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/resources/logo_${APP_L}.icns)
# set where in the bundle to put the icns file
set_source_files_properties(${MACOSX_BUNDLE_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
#if we need run on MacOS before 12.0, then uncomment following code, but not guarantee it's working
#set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") 
add_executable(
    ${APP_BINARY}
    MACOSX_BUNDLE
    ${APP_SRCS}
    ${APP_INCS}
    ${MACOSX_BUNDLE_ICON_FILE}
)
target_include_directories(
    ${APP_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${MEDIACORE_INCLUDE_DIRS}
    ${NCNN_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)

target_compile_definitions(${APP_BINARY} PUBLIC APP_NAME="${APP_BINARY}" APP_SUFFIX="${APP_S}")
if (UI_PERFORMANCE_ANALYSIS)
target_compile_definitions(${APP_BINARY} PRIVATE UI_PERFORMANCE_ANALYSIS=1)
endif(UI_PERFORMANCE_ANALYSIS)

set_property(TARGET ${APP_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${APP_BINARY} 
    LINK_PRIVATE
    ${IMGUI_LIBRARYS}
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${MEDIACORE_LIBRARYS}
    ${LINK_LIBS}
    ${NCNN_LIBRARYS}
    Threads::Threads
)

# set the Info.plist file
set(MACOSX_BUNDLE_PLIST_FILE ${CMAKE_SOURCE_DIR}/resources/Info_${APP_L}.plist)
set_target_properties(${APP_BINARY} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_PLIST_FILE})
set_target_properties(${APP_BINARY} PROPERTIES MACOSX_BUNDLE TRUE MACOSX_FRAMEWORK_IDENTIFIER com.Code-Win.${APP_BINARY} RESOURCE "${RESOURCE_FILES}")

elseif(WIN32)
add_executable(
    ${APP_BINARY}
    ${APP_SRCS}
    ${APP_INCS}
    ${CMAKE_SOURCE_DIR}/resources/logo_${APP_L}.rc
)
target_include_directories(
    ${APP_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${NCNN_INCLUDE_DIRS}
    ${MEDIACORE_INCLUDE_DIRS}
)
target_compile_definitions(${APP_BINARY} PUBLIC APP_NAME="${APP_BINARY}"  APP_SUFFIX="${APP_S}")
if (UI_PERFORMANCE_ANALYSIS)
target_compile_definitions(${APP_BINARY} PRIVATE UI_PERFORMANCE_ANALYSIS=1)
endif(UI_PERFORMANCE_ANALYSIS)
#set_property(TARGET ${APP_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${APP_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${APP_BINARY} 
    LINK_PRIVATE
    ${MEDIACORE_LIBRARYS}
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    ${LINK_LIBS}
    ${NCNN_LIBRARYS}
    Threads::Threads
)

else() # this branch is for linux
add_executable(
    ${APP_BINARY}
    ${APP_SRCS}
    ${APP_INCS}
)
target_include_directories(
    ${APP_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${MEDIACORE_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${NCNN_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${APP_BINARY} PUBLIC APP_NAME="${APP_BINARY}" APP_SUFFIX="${APP_S}")
if (UI_PERFORMANCE_ANALYSIS)
target_compile_definitions(${APP_BINARY} PRIVATE UI_PERFORMANCE_ANALYSIS=1)
endif(UI_PERFORMANCE_ANALYSIS)
#set_property(TARGET ${APP_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${APP_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${APP_BINARY} 
    LINK_PRIVATE
    MediaCore
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    ${LINK_LIBS}
    ${NCNN_LIBRARYS}
    Threads::Threads
)
endif(APPLE)

option(DEV_BACKGROUND_TASK "Developping background task feature" OFF)
if(DEV_BACKGROUND_TASK)
target_compile_definitions(${APP_BINARY} PRIVATE ENABLE_BACKGROUND_TASK)
endif()
endif(IMGUI_APPS)

## build plugins
set(EXTRA_DEPENDENCE_LIBRARYS
    -L${CMAKE_BINARY_DIR}
    -lBluePrintSDK
    -limgui
    -limgui_addons
)

set(EXTRA_DEPENDENCE_PROJECT BluePrintSDK imgui imgui_addons)
if(IMGUI_VULKAN_SHADER)
set(EXTRA_DEPENDENCE_PROJECT
    ${EXTRA_DEPENDENCE_PROJECT}
    VkShader
)
set(EXTRA_DEPENDENCE_LIBRARYS
    ${EXTRA_DEPENDENCE_LIBRARYS}
    -lVkShader
)
endif()

if(VKSHADER_VULKAN_BENCHMARK)
    add_definitions(-DVULKAN_SHADER_BENCHMARK)
endif(VKSHADER_VULKAN_BENCHMARK)
if(IMGUI_VULKAN_SHADER)
#filers common
add_subdirectory(plugin/nodes/filters/ALM)
add_subdirectory(plugin/nodes/filters/Brightness)
add_subdirectory(plugin/nodes/filters/CAS)
add_subdirectory(plugin/nodes/filters/ColorBalance)
add_subdirectory(plugin/nodes/filters/ColorCurve)
add_subdirectory(plugin/nodes/filters/Contrast)
add_subdirectory(plugin/nodes/filters/Deband)
add_subdirectory(plugin/nodes/filters/Deinterlace)
add_subdirectory(plugin/nodes/filters/Exposure)
add_subdirectory(plugin/nodes/filters/Gamma)
add_subdirectory(plugin/nodes/filters/GaussianBlur)
add_subdirectory(plugin/nodes/filters/HQDN3D)
add_subdirectory(plugin/nodes/filters/Hue)
add_subdirectory(plugin/nodes/filters/Lut3D)
add_subdirectory(plugin/nodes/filters/Saturation)
add_subdirectory(plugin/nodes/filters/USM)
add_subdirectory(plugin/nodes/filters/Vibrance)
add_subdirectory(plugin/nodes/filters/Whitebalance)
add_subdirectory(plugin/nodes/filters/SmartDenoise)
endif(IMGUI_VULKAN_SHADER)

if(${APP_TYPE} MATCHES "MediaEditor")
add_subdirectory(plugin/nodes/filters/AudioEqualizer)
add_subdirectory(plugin/nodes/filters/AudioGain)
add_subdirectory(plugin/nodes/filters/AudioAecho)
add_subdirectory(plugin/nodes/transitions/AudioFade)
add_subdirectory(pluginex/nodes/scope/AudioVector)
add_subdirectory(pluginex/nodes/scope/AudioFFT)
add_subdirectory(pluginex/nodes/scope/AudioDecibel)
add_subdirectory(pluginex/nodes/scope/AudioSpectorgram)
if(IMGUI_VULKAN_SHADER)
#filers
add_subdirectory(plugin/nodes/filters/Bilateral)
add_subdirectory(plugin/nodes/filters/Binary)
add_subdirectory(plugin/nodes/filters/Box)
add_subdirectory(plugin/nodes/filters/Canny)
add_subdirectory(plugin/nodes/filters/Chromakey)
add_subdirectory(plugin/nodes/filters/ColorInvert)
add_subdirectory(plugin/nodes/filters/Dilation)
add_subdirectory(plugin/nodes/filters/Erosion)
add_subdirectory(plugin/nodes/filters/Flip)
add_subdirectory(plugin/nodes/filters/Guided)
add_subdirectory(plugin/nodes/filters/Laplacian)
add_subdirectory(plugin/nodes/filters/Sobel)
add_subdirectory(plugin/nodes/filters/Glass)
add_subdirectory(plugin/nodes/filters/SmudgeBlur)
add_subdirectory(plugin/nodes/filters/RadicalBlur)
add_subdirectory(plugin/nodes/filters/Distortion)
add_subdirectory(plugin/nodes/filters/BarrelDistortion)
add_subdirectory(plugin/nodes/filters/PixeLate)
add_subdirectory(plugin/nodes/filters/Kuwahara)
add_subdirectory(plugin/nodes/filters/Sketch)
add_subdirectory(plugin/nodes/filters/Emboss)
add_subdirectory(plugin/nodes/filters/Hatch)
#scope
add_subdirectory(pluginex/nodes/scope/MatHistogram)
add_subdirectory(pluginex/nodes/scope/MatVector)
add_subdirectory(pluginex/nodes/scope/MatWaveform)
add_subdirectory(pluginex/nodes/scope/MatCIE)
add_subdirectory(pluginex/nodes/scope/HarrisCorner)
#fisheye
add_subdirectory(pluginex/nodes/fisheye/fish2sphere)
add_subdirectory(pluginex/nodes/fisheye/fish2panorama)
add_subdirectory(pluginex/nodes/fisheye/equidistance2orthographic)
add_subdirectory(pluginex/nodes/fisheye/orthographic2equidistance)
#media
add_subdirectory(pluginex/nodes/media/CustomVulkanShader)
add_subdirectory(pluginex/nodes/media/SDLAudioRender)
add_subdirectory(pluginex/nodes/media/MatRGB2YUV)
add_subdirectory(pluginex/nodes/media/MatYUV2RGB)
add_subdirectory(pluginex/nodes/media/MatCrop)
add_subdirectory(pluginex/nodes/media/MatResize)
add_subdirectory(pluginex/nodes/media/MatExpand)
add_subdirectory(pluginex/nodes/media/MatRender)
add_subdirectory(pluginex/nodes/media/MediaSource)
add_subdirectory(pluginex/nodes/media/MediaEncoder)
add_subdirectory(pluginex/nodes/media/ImageSource)
add_subdirectory(pluginex/nodes/media/WarpAffine)
add_subdirectory(pluginex/nodes/media/WarpPerspective)
add_subdirectory(pluginex/nodes/media/MatRGB2LAB)
add_subdirectory(pluginex/nodes/media/MatLAB2RGB)
add_subdirectory(pluginex/nodes/media/MatRGB2HSL)
add_subdirectory(pluginex/nodes/media/MatHSL2RGB)
add_subdirectory(pluginex/nodes/media/MatRGB2HSV)
add_subdirectory(pluginex/nodes/media/MatHSV2RGB)
#transitions
add_subdirectory(plugin/nodes/transitions/Alpha)
add_subdirectory(plugin/nodes/transitions/BookFlip)
add_subdirectory(plugin/nodes/transitions/Blur)
add_subdirectory(plugin/nodes/transitions/Bounce)
add_subdirectory(plugin/nodes/transitions/BowTie)
add_subdirectory(plugin/nodes/transitions/Burn)
add_subdirectory(plugin/nodes/transitions/BurnOut)
add_subdirectory(plugin/nodes/transitions/ButterflyWave)
add_subdirectory(plugin/nodes/transitions/CannabisLeaf)
add_subdirectory(plugin/nodes/transitions/CircleBlur)
add_subdirectory(plugin/nodes/transitions/CircleCrop)
add_subdirectory(plugin/nodes/transitions/ColorPhase)
add_subdirectory(plugin/nodes/transitions/ColourDistance)
add_subdirectory(plugin/nodes/transitions/CrazyParametric)
add_subdirectory(plugin/nodes/transitions/Crosshatch)
add_subdirectory(plugin/nodes/transitions/CrossWarp)
add_subdirectory(plugin/nodes/transitions/CrossZoom)
add_subdirectory(plugin/nodes/transitions/Cube)
add_subdirectory(plugin/nodes/transitions/DirectionalScaled)
add_subdirectory(plugin/nodes/transitions/DirectionalWarp)
add_subdirectory(plugin/nodes/transitions/DoomScreen)
add_subdirectory(plugin/nodes/transitions/Door)
add_subdirectory(plugin/nodes/transitions/Doorway)
add_subdirectory(plugin/nodes/transitions/Dreamy)
add_subdirectory(plugin/nodes/transitions/DreamyZoom)
add_subdirectory(plugin/nodes/transitions/Edge)
add_subdirectory(plugin/nodes/transitions/Fade)
add_subdirectory(plugin/nodes/transitions/Flyeye)
add_subdirectory(plugin/nodes/transitions/GlitchDisplace)
add_subdirectory(plugin/nodes/transitions/GlitchMemories)
add_subdirectory(plugin/nodes/transitions/GridFlip)
add_subdirectory(plugin/nodes/transitions/Heart)
add_subdirectory(plugin/nodes/transitions/Hexagonalize)
add_subdirectory(plugin/nodes/transitions/KaleidoScope)
add_subdirectory(plugin/nodes/transitions/LuminanceMelt)
add_subdirectory(plugin/nodes/transitions/Morph)
add_subdirectory(plugin/nodes/transitions/Mosaic)
add_subdirectory(plugin/nodes/transitions/Move)
add_subdirectory(plugin/nodes/transitions/MultiplyBlend)
add_subdirectory(plugin/nodes/transitions/PageCurl)
add_subdirectory(plugin/nodes/transitions/Perlin)
add_subdirectory(plugin/nodes/transitions/Pinwheel)
add_subdirectory(plugin/nodes/transitions/Pixelize)
add_subdirectory(plugin/nodes/transitions/Polar)
add_subdirectory(plugin/nodes/transitions/PolkaDots)
add_subdirectory(plugin/nodes/transitions/Radial)
add_subdirectory(plugin/nodes/transitions/RandomSquares)
add_subdirectory(plugin/nodes/transitions/Rectangle)
add_subdirectory(plugin/nodes/transitions/Ripple)
add_subdirectory(plugin/nodes/transitions/Rolls)
add_subdirectory(plugin/nodes/transitions/RotateScale)
add_subdirectory(plugin/nodes/transitions/RotateScaleVanish)
add_subdirectory(plugin/nodes/transitions/SimpleZoom)
add_subdirectory(plugin/nodes/transitions/SimpleZoomOut)
add_subdirectory(plugin/nodes/transitions/Slider)
add_subdirectory(plugin/nodes/transitions/SquaresWire)
add_subdirectory(plugin/nodes/transitions/Squeeze)
add_subdirectory(plugin/nodes/transitions/StaticWipe)
add_subdirectory(plugin/nodes/transitions/StereoViewer)
add_subdirectory(plugin/nodes/transitions/Swap)
add_subdirectory(plugin/nodes/transitions/Swirl)
add_subdirectory(plugin/nodes/transitions/WaterDrop)
add_subdirectory(plugin/nodes/transitions/Wind)
add_subdirectory(plugin/nodes/transitions/WindowBlinds)
add_subdirectory(plugin/nodes/transitions/WindowSlice)
add_subdirectory(plugin/nodes/transitions/Wipe)
add_subdirectory(plugin/nodes/transitions/ZoomInCircles)
add_subdirectory(plugin/nodes/transitions/Luma)
add_subdirectory(plugin/nodes/transitions/Dissolve)
#effects
add_subdirectory(plugin/nodes/effects/Lighting)
add_subdirectory(plugin/nodes/effects/Jitter)
add_subdirectory(plugin/nodes/effects/Star)
add_subdirectory(plugin/nodes/effects/Soul)
add_subdirectory(plugin/nodes/effects/Sway)
add_subdirectory(plugin/nodes/effects/WaterRipple)
endif(IMGUI_VULKAN_SHADER)
endif(${APP_TYPE} MATCHES "MediaEditor")

if (${APP_TYPE} MATCHES "MediaUpscale")
#upscaling
add_subdirectory(pluginex/nodes/upscale/SimpleFast)
add_subdirectory(pluginex/nodes/upscale/SimpleFaster)
add_subdirectory(pluginex/nodes/upscale/General)
add_subdirectory(pluginex/nodes/upscale/RefineSlower)
add_subdirectory(pluginex/nodes/upscale/AnimeFaster)
add_subdirectory(pluginex/nodes/upscale/AnimeGeneral)
add_subdirectory(pluginex/nodes/upscale/AnimeSlower)
#enhance
add_subdirectory(pluginex/nodes/enhance/denoise)
add_subdirectory(pluginex/nodes/enhance/denoise_iso)
add_subdirectory(pluginex/nodes/enhance/refocus)
add_subdirectory(pluginex/nodes/enhance/over_exposure)
add_subdirectory(pluginex/nodes/enhance/under_exposure)
endif()

### DEFINE THE PACKAGING (OS specific)
set(CPACK_PACKAGE_NAME  ${APP_BINARY})
if (IMGUI_VULKAN)
    if (IMGUI_SDL2)
        set(CPACK_PACKAGE_NAME  "${APP_BINARY}_SDL2_Vulkan")
    elseif (IMGUI_GLFW)
        set(CPACK_PACKAGE_NAME  "${APP_BINARY}_GLFW_Vulkan")
    endif()
elseif (IMGUI_GL3)
    if (IMGUI_SDL2)
        set(CPACK_PACKAGE_NAME  "${APP_BINARY}_SDL2_OpenGL3")
    elseif (IMGUI_GLFW)
        set(CPACK_PACKAGE_NAME  "${APP_BINARY}_GLFW_OpenGL3")
    endif()
elseif (IMGUI_GL2)
    if (IMGUI_SDL2)
        set(CPACK_PACKAGE_NAME  "${APP_BINARY}_SDL2_OpenGL2")
    elseif (IMGUI_GLFW)
        set(CPACK_PACKAGE_NAME  "${APP_BINARY}_GLFW_OpenGL2")
    endif()
endif()
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/resources/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_BINARY} installation.")
set(CPACK_PACKAGE_VENDOR "CodeWin")
set(CPACK_PACKAGE_VERSION ${APP_VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${APP_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${APP_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${APP_VERSION_PATCH})

if(IMGUI_APPS)
if(APPLE)
    set(CPACK_SYSTEM_NAME "macos")
    set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})
    # Bundle target
    #set(CPACK_GENERATOR DragNDrop)
    #set(CPACK_BINARY_DRAGNDROP ON)
    # OSX cpack info
    install(TARGETS ${APP_BINARY}
            CONFIGURATIONS Release MinSizeRel RelWithDebInfo Debug
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )
    set(bindirs ${APP_BINARY}.app/Contents/MacOS)
    set(frameworks_dir ${APP_BINARY}.app/Contents/Frameworks)
    set(resources_dir ${APP_BINARY}.app/Contents/Resources)
    set(contents_dir ${APP_BINARY}.app/Contents)

    # Install Resource File
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/languages/${APP_L}/"
            DESTINATION "${resources_dir}/languages/"
            COMPONENT Runtime
            PATTERN "*.ini"
            )
    install(FILES  "${PROJECT_SOURCE_DIR}/resources/logo_${APP_L}.png"
            DESTINATION "${resources_dir}/" COMPONENT Runtime)

    # Install Plugins
    install(DIRECTORY  "${PROJECT_BINARY_DIR}/plugins/${PLUGIN_FOLDER}/"
            DESTINATION "${contents_dir}/plugins/" COMPONENT Runtime)

    install(CODE "
            set(APP_NAME ${APP_BINARY})
            set(PROJECT_PATH ${PROJECT_BINARY_DIR})
            set(FRAMEWORKS_PATH ${frameworks_dir})
            ")
    install(CODE [[
        set(plugins_dir "${PROJECT_PATH}/${APP_NAME}.app/Contents/plugins")
        file(GLOB_RECURSE PLUGINS "${plugins_dir}/*.node")
        set(APPS "${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")
        set(DIRS ${FRAMEWORKS_PATH})
        list (APPEND DIRS "/usr/local/lib/")
        list (APPEND DIRS "${CMAKE_INSTALL_PREFIX}")
        include(InstallRequiredSystemLibraries)
        include(BundleUtilities)
        fixup_bundle("${APPS}" "${PLUGINS}" "${DIRS}")
    ]])

    set(APPLE_CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/entitlements.plist")
    set(APPLE_CODESIGN_IDENTITY "Apple Development Identity" CACHE STRING "Please Input Apple Development Codesign ID")
    string(LENGTH "${APPLE_CODESIGN_IDENTITY}" APPLE_CODESIGN_IDENTITY_LENGHT)
    if( "${APPLE_CODESIGN_IDENTITY}" STREQUAL "Apple Development Identity" OR APPLE_CODESIGN_IDENTITY_LENGHT LESS 20)
        message(STATUS "Not signing bundle. Specify APPLE_CODESIGN_IDENTITY to cmake before running cpack to sign")
    else()
        message(STATUS "Using Sign ID: " ${APPLE_CODESIGN_IDENTITY})
        install(CODE "
                message(STATUS \"Sign code with: \" \"${APPLE_CODESIGN_IDENTITY}\")
                execute_process(COMMAND
                codesign -vvv --deep --force
                --entitlements \"${APPLE_CODESIGN_ENTITLEMENTS}\"
                --sign \"${APPLE_CODESIGN_IDENTITY}\"
                \"${APP_BINARY}.app\" )
                "
                COMPONENT Runtime
        )
    endif()

elseif(UNIX)
    set(CPACK_SYSTEM_NAME "linux")
    install(CODE "
        include(../cmake/appimage.cmake)
        make_appimage(
            PROJECT_DIR \"${PROJECT_SOURCE_DIR}\"
            EXE \"${APP_BINARY}\"
            NAME \"Media AI\"
            OUTPUT_NAME \"${CPACK_PACKAGE_NAME}-${CPACK_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CPACK_PACKAGE_VERSION}\"
            DESKTOP_SRC \"${PROJECT_SOURCE_DIR}/resources/${APP_L}.desktop\"
            ICON_SRC \"${PROJECT_SOURCE_DIR}/resources/logo_${APP_L}.png\"
            RESOURCE_FILES \"${PROJECT_SOURCE_DIR}/languages\"
            PLUGINS \"${PROJECT_BINARY_DIR}/plugins/${PLUGIN_FOLDER}\"
        )
    " COMPONENT Runtime)
elseif(WIN32)
    set(CPACK_SYSTEM_NAME "win")
    macro(prepareNSIS_Link linkName appName params)
        #prepare start menu links
        LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA "  CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${linkName}.lnk' '$INSTDIR\\\\bin\\\\${appName}.exe' '${params}'")
        LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA "  Delete '$SMPROGRAMS\\\\$START_MENU\\\\${linkName}.lnk'")

        #prepare desktop links
        LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA  "  CreateShortCut '$DESKTOP\\\\${linkName}.lnk' '$INSTDIR\\\\bin\\\\${appName}.exe' '${params}'")
        LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA  "  Delete '$DESKTOP\\\\${linkName}.lnk'")

        #replace new line
        string (REPLACE ";" "\n" CPACK_NSIS_CREATE_ICONS_EXTRA "${CPACK_NSIS_CREATE_ICONS_EXTRA}")
        string (REPLACE ";" "\n" CPACK_NSIS_DELETE_ICONS_EXTRA "${CPACK_NSIS_DELETE_ICONS_EXTRA}")
    endmacro()

    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install_${APP_L}")
    install( CODE "set(APPS \"${CMAKE_BINARY_DIR}/${APP_BINARY}.exe\")" )
    install( CODE "set(APP_NAME \"${APP_BINARY}\")" )
    install( CODE [[
        file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")
        execute_process(COMMAND ldd ${APPS} OUTPUT_VARIABLE ldd_out)
        string (REPLACE "\n" ";" ldd_out_lines "${ldd_out}")
        foreach (line ${ldd_out_lines})
            string (REGEX REPLACE "^.* => | \(.*\)" "" pruned ${line})
            string (STRIP ${pruned} dep_filename)
            if (IS_ABSOLUTE ${dep_filename})
                string (REGEX MATCH "SYSTEM32|System32|system32|WinSxS|MeAI" system_lib ${dep_filename})
                if ("${system_lib}" STREQUAL "")
                    set(dep_path "")
                    string (PREPEND dep_path "C:/msys64" ${dep_filename})
                    message("Link librarys: " ${dep_path})
                    file(INSTALL 
                        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" 
                        FOLLOW_SYMLINK_CHAIN
                        FILES ${dep_path}
                    )
                endif()
                string (REGEX MATCH "SYSTEM32|System32|system32|WinSxS|mingw64" custom_lib ${dep_filename})
                if ("${custom_lib}" STREQUAL "")
                    set(dep_path "")
                    #string (PREPEND dep_path "" ${dep_filename})
                    string (REPLACE "/c/" "C:/" dep_path ${dep_filename})
                    string (REPLACE "/d/" "D:/" dep_path ${dep_filename})
                    message("Link librarys: " ${dep_path})
                    file(INSTALL 
                        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" 
                        FOLLOW_SYMLINK_CHAIN
                        FILES ${dep_path}
                    )
                endif()
            endif()
        endforeach()
    ]])

    install(TARGETS ${APP_BINARY}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION bin
            COMPONENT applications)

    # Install Resource File
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/languages/${APP_L}"
            DESTINATION languages COMPONENT Resource)
    install(FILES  "${PROJECT_SOURCE_DIR}/resources/logo_${APP_L}.png"
            DESTINATION resources COMPONENT Resource)
    # Install Plugins
    install(DIRECTORY  "${PROJECT_BINARY_DIR}/plugins/${PLUGIN_FOLDER}/"
            DESTINATION plugins COMPONENT Runtime)
    
    if(NOT ${APP_TYPE} MATCHES "MediaEditor")
        # Install Plugin dependence
        install(FILES  "${CMAKE_BINARY_DIR}/libncnn.dll"
                DESTINATION bin COMPONENT Runtime)
    endif()

    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL OFF)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo_${APP_L}.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo_${APP_L}.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME bin\\\\${APP_BINARY}.exe)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_NAME ${APP_BINARY})
    set(CPACK_NSIS_DISPLAY_NAME ${APP_BINARY})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY ${APP_BINARY})
    
    prepareNSIS_Link("${APP_BINARY}" "${APP_BINARY}" "")
endif(APPLE)
endif(IMGUI_APPS)

# Package full name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CPACK_PACKAGE_VERSION}")
# To Create a package, run "cpack"
include(CPack)
endif (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")