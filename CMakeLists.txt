cmake_minimum_required(VERSION 3.5.1)
project(MediaEditor)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

if(IOS AND CMAKE_OSX_ARCHITECTURES MATCHES "arm")
    message(STATUS "Target arch: arm-ios")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    message(STATUS "Target arch: arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|x86_64|AMD64)")
    message(STATUS "Target arch: x86")
    if(MSVC OR MSVC_IDE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2 /arch:AVX /arch:FMA /arch:SSE /arch:SSE2 /arch:SSSE3 /arch:SSE4.1 /arch:SSE4.2")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX2 /arch:AVX /arch:FMA /arch:SSE /arch:SSE2 /arch:SSSE3 /arch:SSE4.1 /arch:SSE4.2")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mavx")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -mavx")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
    endif()
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

if(IMGUI_VULKAN_SHADER)
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

find_package(Fontconfig REQUIRED)

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

    pkg_check_modules(
        LIBASS REQUIRED IMPORTED_TARGET
        libass
    )
endif(PKG_CONFIG_FOUND)

macro_display_feature_log()

add_compile_options(-Wno-ignored-attributes -Wno-inconsistent-dllimport -Wno-deprecated-declarations)
#
#  Application
#
set(MEDIA_EDITOR_BINARY "mec")
set(MEDIA_EDITOR_SRCS
    MediaEditor.cpp
    MediaTimeline.cpp
    MultiTrackVideoReader.cpp
    VideoTrack.cpp
    VideoClip.cpp
    VideoBlender.cpp
    VideoTransformFilter.cpp
    VideoTransformFilter_FFImpl.cpp
    VideoTransformFilter_VulkanImpl.cpp
    MultiTrackAudioReader.cpp
    AudioTrack.cpp
    AudioClip.cpp
    MediaOverview.cpp
    SnapshotGenerator.cpp
    MediaParser.cpp
    MediaReader.cpp
    MediaEncoder.cpp
    AudioRender_Impl_Sdl2.cpp
    SubtitleClip_AssImpl.cpp
    SubtitleTrack.cpp
    SubtitleTrack_AssImpl.cpp
    FontDescriptor.cpp
    FontManager_Fontconfig.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)

set(MEDIA_EDITOR_INCS
    MediaTimeline.h
    MediaOverview.h
    SnapshotGenerator.h
    MediaReader.h
    MediaEncoder.h
    AudioRender_Impl_Sdl2.hpp
    AudioRender.hpp
    FFUtils.h
)

if(APPLE)
set(MACOSX_BUNDLE_ICON mec_logo.icns)
set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/resources/${MACOSX_BUNDLE_ICON})
# set where in the bundle to put the icns file
set_source_files_properties(${MACOSX_BUNDLE_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
#if we need run on MacOS before 12.0, then uncomment following code, but not guarantee it's working
#set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") 
add_executable(
    ${MEDIA_EDITOR_BINARY}
    MACOSX_BUNDLE
    ${MEDIA_EDITOR_SRCS}
    ${MEDIA_EDITOR_INCS}
    ${MACOSX_BUNDLE_ICON_FILE}
)
target_include_directories(
    ${MEDIA_EDITOR_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_EDITOR_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_BINARY}")
#set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_BINARY} 
    LINK_PRIVATE
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
    PkgConfig::LIBASS
    Fontconfig::Fontconfig
    Threads::Threads
)
# set the Info.plist file
set(MACOSX_BUNDLE_PLIST_FILE ${CMAKE_SOURCE_DIR}/resources/Info.plist)
set_target_properties(${MEDIA_EDITOR_BINARY} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_PLIST_FILE})
set_target_properties(${MEDIA_EDITOR_BINARY} PROPERTIES MACOSX_BUNDLE TRUE MACOSX_FRAMEWORK_IDENTIFIER com.tanlu.mec RESOURCE "${RESOURCE_FILES}")
elseif(WIN32)
add_executable(
    ${MEDIA_EDITOR_BINARY}
    ${MEDIA_EDITOR_SRCS}
    ${MEDIA_EDITOR_INCS}
    ${CMAKE_SOURCE_DIR}/resources/logo.rc
)
target_include_directories(
    ${MEDIA_EDITOR_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_EDITOR_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_BINARY}")
#set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_BINARY} 
    LINK_PRIVATE
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
    PkgConfig::LIBASS
    Fontconfig::Fontconfig
    Threads::Threads
)
else()
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
#set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_BINARY} 
    LINK_PRIVATE
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
    PkgConfig::LIBASS
    Fontconfig::Fontconfig
    Threads::Threads
)
endif(APPLE)

if(BUILD_TEST)
# MediaPlayer Test
add_executable(
    media_player_test
    test/MediaPlayerTest.cpp
    test/MediaPlayer.cpp
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
    SnapshotGenerator.cpp
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

# MultiTrackAudioReader Test
add_executable(
    multi_track_audio_reader_test
    test/MultiTrackAudioReaderTest.cpp
    MultiTrackAudioReader.cpp
    AudioTrack.cpp
    AudioClip.cpp
    MediaReader.cpp
    MediaParser.cpp
    AudioRender_Impl_Sdl2.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    multi_track_audio_reader_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
)
target_include_directories(
    multi_track_audio_reader_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# MultiTrackVideoReader Test
add_executable(
    multi_track_video_reader_test
    test/MultiTrackVideoReaderTest.cpp
    MultiTrackVideoReader.cpp
    VideoTrack.cpp
    VideoClip.cpp
    VideoBlender.cpp
    VideoTransformFilter.cpp
    VideoTransformFilter_FFImpl.cpp
    VideoTransformFilter_VulkanImpl.cpp
    MediaReader.cpp
    MediaParser.cpp
    SubtitleClip_AssImpl.cpp
    SubtitleTrack.cpp
    SubtitleTrack_AssImpl.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    multi_track_video_reader_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
    PkgConfig::LIBASS
)
target_include_directories(
    multi_track_video_reader_test PRIVATE
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

# SubtitleReaderTest
add_executable(
    subtitle_reader_test
    test/SubtitleReaderTest.cpp
    SubtitleClip_AssImpl.cpp
    SubtitleTrack.cpp
    SubtitleTrack_AssImpl.cpp
    FontDescriptor.cpp
    FontManager_Fontconfig.cpp
    FFUtils.cpp
    Logger.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    subtitle_reader_test
    ${IMGUI_LIBRARYS}
    PkgConfig::FFMPEG
    PkgConfig::LIBASS
    Fontconfig::Fontconfig
)
target_include_directories(
    subtitle_reader_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
endif(BUILD_TEST)

### DEFINE THE PACKAGING (OS specific)
set(CPACK_SYSTEM_NAME  "mec")
if (IMGUI_VULKAN)
    if (IMGUI_SDL2)
        set(CPACK_SYSTEM_NAME  "MEC_SDL2_Vulkan")
    elseif (IMGUI_GLFW)
        set(CPACK_SYSTEM_NAME  "MEC_GLFW_Vulkan")
    endif()
elseif (IMGUI_GL3)
    if (IMGUI_SDL2)
        set(CPACK_SYSTEM_NAME  "MEC_SDL2_OpenGL3")
    elseif (IMGUI_GLFW)
        set(CPACK_SYSTEM_NAME  "MEC_GLFW_OpenGL3")
    endif()
elseif (IMGUI_GL2)
    if (IMGUI_SDL2)
        set(CPACK_SYSTEM_NAME  "MEC_SDL2_OpenGL2")
    elseif (IMGUI_GLFW)
        set(CPACK_SYSTEM_NAME  "MEC_GLFW_OpenGL2")
    endif()
endif()
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MEC installation.")
set(CPACK_PACKAGE_VENDOR "Tanlu")
set(CPACK_PACKAGE_VERSION 1.0.0)
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)

if(APPLE)
    set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})
    # Bundle target
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_BINARY_DRAGNDROP ON)
    # OSX cpack info
    install(TARGETS mec
            CONFIGURATIONS Release RelWithDebInfo
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )
    set(frameworks_dir mec.app/Contents/Frameworks/)
    set(resources_dir mec.app/Contents/Resources/)

    # Install Resource File
    install(FILES  "${PROJECT_SOURCE_DIR}/languages/mec_language.ini"
            DESTINATION "${resources_dir}" COMPONENT Runtime)
    install(FILES  "${PROJECT_SOURCE_DIR}/resources/mec_logo.png"
            DESTINATION "${resources_dir}/" COMPONENT Runtime)

    # package runtime fixup bundle
    set(APPS "\${CMAKE_INSTALL_PREFIX}/mec.app")
    set(DIRS ${FRAMEWORKS_PATH})
    list (APPEND DIRS "/usr/local/lib/")
    INSTALL(CODE "
        include(BundleUtilities)
        fixup_bundle(\"${APPS}\"   \"\"  \"${DIRS}\")
    " COMPONENT Runtime)

    set(APPLE_CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/entitlements.plist")
    set(APPLE_CODESIGN_IDENTITY "" CACHE STRING "")
    string(LENGTH "${APPLE_CODESIGN_IDENTITY}" APPLE_CODESIGN_IDENTITY_LENGHT)
    if( ${APPLE_CODESIGN_IDENTITY_LENGHT} LESS 40 )
        message(STATUS "Not signing bundle. Specify APPLE_CODESIGN_IDENTITY to cmake before running cpack to sign")
    else()
        install(CODE "
                execute_process(COMMAND
                codesign -vvv --deep --force
                --entitlements \"${APPLE_CODESIGN_ENTITLEMENTS}\"
                --sign \"${APPLE_CODESIGN_IDENTITY}\"
                \"${APPS}\" )
                "
                COMPONENT Runtime
        )
    endif()

elseif(UNIX)
    install(CODE "
        include(../cmake/appimage.cmake)
        make_appimage(
            PROJECT_DIR \"${PROJECT_SOURCE_DIR}\"
            EXE \"mec\"
            NAME \"Media Editor Community\"
            OUTPUT_NAME \"${CPACK_SYSTEM_NAME}\"
            DESKTOP_SRC \"${PROJECT_SOURCE_DIR}/resources/mec.desktop\"
            ICON_SRC \"${PROJECT_SOURCE_DIR}/resources/mec_logo.png\"
            RESOURCE_FILES \"${PROJECT_SOURCE_DIR}/languages/mec_language.ini\"
        )
    " COMPONENT Runtime)
elseif(WIN32)
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

    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")
    install(TARGETS mec
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION bin
            COMPONENT applications)

    install( CODE "set(APPS \"${CMAKE_INSTALL_PREFIX}/bin/mec.exe\")" )
    install( CODE [[
        execute_process(COMMAND ldd ${APPS} OUTPUT_VARIABLE ldd_out)
        string (REPLACE "\n" ";" ldd_out_lines "${ldd_out}")
        foreach (line ${ldd_out_lines})
            string (REGEX REPLACE "^.* => | \(.*\)" "" pruned ${line})
            string (STRIP ${pruned} dep_filename)
            if (IS_ABSOLUTE ${dep_filename})
                string (REGEX MATCH "SYSTEM32|System32|WinSxS" system_lib ${dep_filename})
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
            endif()
        endforeach()
    ]])

    # Install Resource File
    install(FILES  "${PROJECT_SOURCE_DIR}/languages/mec_language.ini"
            DESTINATION languages COMPONENT Resource)

    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL OFF)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/mec_logo.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/mec_logo.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME bin\\\\mec.exe)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_NAME mec)
    set(CPACK_NSIS_DISPLAY_NAME mec)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY mec)
    
    prepareNSIS_Link("mec" "mec" "")
endif(APPLE)

# Package full name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SYSTEM_NAME}")
# To Create a package, run "cpack"
include(CPack)
