cmake_minimum_required(VERSION 3.12.0)
project(MediaCore)

option(MEDIACORE_STATIC              "Build MediaCore as static library" OFF)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)

# >>> include ffmpeg as dependency
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
# <<<

# >>> include libass as dependency
pkg_check_modules(
    LIBASS REQUIRED IMPORTED_TARGET
    libass
)
# <<<

# >>> include libass as dependency
pkg_search_module(FONTCONFIG REQUIRED fontconfig)
# <<<

# >>> include imgui as dependency (from source code)
if(DEFINED BUILD_DEPENDENCY_IMGUI)
set(BUILD_DEPENDENCY_IMGUI ${BUILD_DEPENDENCY_IMGUI} CACHE BOOL "Choose whether or not to build the dependency project 'IMGUI' from source code.")
else()
set(BUILD_DEPENDENCY_IMGUI ON CACHE BOOL "Choose whether or not to build the dependency project 'IMGUI' from source code.")
endif()

if(BUILD_DEPENDENCY_IMGUI)
set(IMGUI_SRC_PATH "imgui" CACHE PATH "Path to imgui source code.")
add_subdirectory("${IMGUI_SRC_PATH}" imgui EXCLUDE_FROM_ALL)
set_target_properties(
    imgui
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
if(IMGUI_VULKAN_SHADER)
set_target_properties(
    VkShader
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
endif(IMGUI_VULKAN_SHADER)
endif(BUILD_DEPENDENCY_IMGUI)
# <<<

# >>> make MediaCore library
set(LIB_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

if(MEDIACORE_STATIC)
set(LIBRARY STATIC)
else(MEDIACORE_STATIC)
set(LIBRARY SHARED)
add_definitions(-DMEDIACORE_SHARED)
endif(MEDIACORE_STATIC)

set(MEDIACORE_VERSION_MAJOR 1)
set(MEDIACORE_VERSION_MINOR 12)
set(MEDIACORE_VERSION_PATCH 25)
string(TIMESTAMP MEDIACORE_VERSION_BUILD "%y%m%d")
set(MEDIACORE_VERSION_STRING ${MEDIACORE_VERSION_MAJOR}.${MEDIACORE_VERSION_MINOR}.${MEDIACORE_VERSION_PATCH})
add_definitions(-DMEDIACORE_VERSION_MAJOR=${MEDIACORE_VERSION_MAJOR})
add_definitions(-DMEDIACORE_VERSION_MINOR=${MEDIACORE_VERSION_MINOR})
add_definitions(-DMEDIACORE_VERSION_PATCH=${MEDIACORE_VERSION_PATCH})
add_definitions(-DMEDIACORE_VERSION_BUILD=${MEDIACORE_VERSION_BUILD})

add_library(MediaCore ${LIBRARY}
    ${LIB_SRC_DIR}/AudioRender_Impl_Sdl2.cpp
    ${LIB_SRC_DIR}/AudioClip.cpp
    ${LIB_SRC_DIR}/AudioTrack.cpp
    ${LIB_SRC_DIR}/AudioEffectFilter_FFImpl.cpp
    ${LIB_SRC_DIR}/DebugHelper.cpp
    ${LIB_SRC_DIR}/FFUtils.cpp
    ${LIB_SRC_DIR}/FontDescriptor.cpp
    ${LIB_SRC_DIR}/FontManager_Fontconfig.cpp
    ${LIB_SRC_DIR}/HwaccelManager.cpp
    ${LIB_SRC_DIR}/ImageSequenceReader.cpp
    ${LIB_SRC_DIR}/MatUtils.cpp
    ${LIB_SRC_DIR}/MediaCore.cpp
    ${LIB_SRC_DIR}/MediaData.cpp
    ${LIB_SRC_DIR}/MediaEncoder.cpp
    ${LIB_SRC_DIR}/MediaParser.cpp
    ${LIB_SRC_DIR}/MediaReader.cpp
    ${LIB_SRC_DIR}/MultiTrackAudioReader.cpp
    ${LIB_SRC_DIR}/MultiTrackVideoReader.cpp
    ${LIB_SRC_DIR}/Overview.cpp
    ${LIB_SRC_DIR}/SharedSettings.cpp
    ${LIB_SRC_DIR}/SingleTrackVideoReader.cpp
    ${LIB_SRC_DIR}/Snapshot.cpp
    ${LIB_SRC_DIR}/SubtitleClip_AssImpl.cpp
    ${LIB_SRC_DIR}/SubtitleTrack_AssImpl.cpp
    ${LIB_SRC_DIR}/SubtitleTrack.cpp
    ${LIB_SRC_DIR}/TextureManager.cpp
    ${LIB_SRC_DIR}/VideoBlender.cpp
    ${LIB_SRC_DIR}/VideoClip.cpp
    ${LIB_SRC_DIR}/VideoReader.cpp
    ${LIB_SRC_DIR}/VideoTrack.cpp
    ${LIB_SRC_DIR}/VideoTransformFilter_FFImpl.cpp
    ${LIB_SRC_DIR}/VideoTransformFilter_VkImpl.cpp
    ${LIB_SRC_DIR}/VideoTransformFilter.cpp
)

if (NOT MEDIACORE_STATIC)
set_target_properties(MediaCore PROPERTIES VERSION ${MEDIACORE_VERSION_STRING} SOVERSION ${MEDIACORE_VERSION_MAJOR})
endif()

target_include_directories(MediaCore PRIVATE
    ${LIB_SRC_DIR}
)
target_include_directories(MediaCore PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${IMGUI_INCLUDE_DIRS}
)

target_link_libraries(MediaCore PUBLIC
    PkgConfig::FFMPEG
    PkgConfig::LIBASS
    ${FONTCONFIG_LIBRARIES}
    BaseUtils
    imgui
)

if (IM_MASK_CREATOR_STATIC_LIB)
target_link_libraries(MediaCore INTERFACE
    ImMaskCreator
)
else ()
target_link_libraries(MediaCore PUBLIC
    ImMaskCreator
)
endif ()

# <<<
get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(MEDIACORE_LIBRARYS MediaCore PARENT_SCOPE )
    set(MEDIACORE_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include PARENT_SCOPE )
endif()

# >>> make tests
if(DEFINED BUILD_MEDIACORE_TEST)
set(BUILD_MEDIACORE_TEST ${BUILD_MEDIACORE_TEST} CACHE BOOL "Choose whether or not to build the test programs of MediaCore library.")
else()
set(BUILD_MEDIACORE_TEST ON CACHE BOOL "Choose whether or not to build the test programs of MediaCore library.")
endif()

if(BUILD_MEDIACORE_TEST)
set(LIB_TEST_DIR ${PROJECT_SOURCE_DIR}/test)

add_executable(MediaEncoderTest
    ${LIB_TEST_DIR}/MediaEncoderTest.cpp
)
target_link_libraries(MediaEncoderTest MediaCore)
add_custom_command(TARGET MediaEncoderTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:MediaEncoderTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(OverviewTest
    ${LIB_TEST_DIR}/OverviewTest.cpp
    ${IMGUI_SRC_PATH}/../${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(OverviewTest MediaCore)
add_custom_command(TARGET OverviewTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:OverviewTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(MediaReaderTest
    ${LIB_TEST_DIR}/MediaReaderTest.cpp
    ${IMGUI_SRC_PATH}/../${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(MediaReaderTest MediaCore)
add_custom_command(TARGET MediaReaderTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:MediaReaderTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(SnapshotTest
    ${LIB_TEST_DIR}/SnapshotTest.cpp
    ${IMGUI_SRC_PATH}/../${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(SnapshotTest MediaCore)
add_custom_command(TARGET SnapshotTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SnapshotTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(MultiTrackAudioTest
    ${LIB_TEST_DIR}/MultiTrackAudioReaderTest.cpp
    ${IMGUI_SRC_PATH}/../${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(MultiTrackAudioTest MediaCore)
add_custom_command(TARGET MultiTrackAudioTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:MultiTrackAudioTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(MultiTrackVideoTest
    ${LIB_TEST_DIR}/MultiTrackVideoReaderTest.cpp
    ${IMGUI_SRC_PATH}/../${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(MultiTrackVideoTest MediaCore)
add_custom_command(TARGET MultiTrackVideoTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:MultiTrackVideoTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(SubtitleReaderTest
    ${LIB_TEST_DIR}/SubtitleReaderTest.cpp
    ${IMGUI_SRC_PATH}/../${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(SubtitleReaderTest MediaCore)
add_custom_command(TARGET SubtitleReaderTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SubtitleReaderTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(UnitTest
    ${LIB_TEST_DIR}/UnitTest.cpp
)
target_link_libraries(UnitTest MediaCore)
add_custom_command(TARGET UnitTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:UnitTest> $<TARGET_FILE_DIR:MediaCore>)

add_executable(HwaccelManagerTest
    ${LIB_TEST_DIR}/HwaccelManagerTest.cpp
)
target_link_libraries(HwaccelManagerTest MediaCore)
add_custom_command(TARGET HwaccelManagerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:HwaccelManagerTest> $<TARGET_FILE_DIR:MediaCore>)

endif(BUILD_MEDIACORE_TEST)
# <<<
