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

# >>> include MediaCore as dependency
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
set(BUILD_DEPENDENCY_IMGUI OFF)
option(BUILD_MEDIACORE_TEST              "Build MediaCore test" OFF)
if (BUILD_MEDIACORE_TEST)
set(IMGUI_SRC_PATH ${CMAKE_SOURCE_DIR}/imgui)
endif()
add_subdirectory(MediaCore)
set_target_properties(
    MediaCore
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
# <<<

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

add_compile_options(-Wno-ignored-attributes -Wno-inconsistent-dllimport -Wno-deprecated-declarations)
#
#  Application
#
set(MEDIA_EDITOR_BINARY "mec")
set(MEDIA_EDITOR_CLI_BINARY "mec_cli")
set(MEDIA_EDITOR_SRCS
    MediaEditor.cpp
    MediaTimeline.cpp
    ${IMGUI_APP_ENTRY_SRC}
)

set(MEDIA_EDITOR_INCS
    MediaTimeline.h
)

set(MEDIAEDITOR_VERSION_MAJOR 0)
set(MEDIAEDITOR_VERSION_MINOR 9)
set(MEDIAEDITOR_VERSION_PATCH 6)
string(TIMESTAMP MEDIAEDITOR_VERSION_BUILD "%y%m%d")
set(MEDIAEDITOR_VERSION_STRING ${MEDIAEDITOR_VERSION_MAJOR}.${MEDIAEDITOR_VERSION_MINOR}.${MEDIAEDITOR_VERSION_PATCH})
add_definitions(-DMEDIAEDITOR_VERSION_MAJOR=${MEDIAEDITOR_VERSION_MAJOR})
add_definitions(-DMEDIAEDITOR_VERSION_MINOR=${MEDIAEDITOR_VERSION_MINOR})
add_definitions(-DMEDIAEDITOR_VERSION_PATCH=${MEDIAEDITOR_VERSION_PATCH})
add_definitions(-DMEDIAEDITOR_VERSION_BUILD=${MEDIAEDITOR_VERSION_BUILD})

if (IMGUI_APPS)
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
    ${MEDIACORE_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_EDITOR_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_BINARY}")

set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_BINARY} 
    LINK_PRIVATE
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    ${MEDIACORE_LIBRARYS}
    Threads::Threads
)

# build non-bundled for debug
add_executable(
    ${MEDIA_EDITOR_CLI_BINARY}
    ${MEDIA_EDITOR_SRCS}
    ${MEDIA_EDITOR_INCS}
)
target_include_directories(
    ${MEDIA_EDITOR_CLI_BINARY} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_BLUEPRINT_INCLUDE_DIRS}
    ${MEDIACORE_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)
target_compile_definitions(${MEDIA_EDITOR_CLI_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_BINARY}")
set_property(TARGET ${MEDIA_EDITOR_CLI_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_CLI_BINARY} 
    LINK_PRIVATE
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    ${MEDIACORE_LIBRARYS}
    Threads::Threads
)
set_target_properties(
    ${MEDIA_EDITOR_CLI_BINARY} 
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

# set the Info.plist file
set(MACOSX_BUNDLE_PLIST_FILE ${CMAKE_SOURCE_DIR}/resources/Info.plist)
set_target_properties(${MEDIA_EDITOR_BINARY} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_PLIST_FILE})
set_target_properties(${MEDIA_EDITOR_BINARY} PROPERTIES MACOSX_BUNDLE TRUE MACOSX_FRAMEWORK_IDENTIFIER com.Code-Win.mec RESOURCE "${RESOURCE_FILES}")
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
    ${MEDIACORE_INCLUDE_DIRS}
)
target_compile_definitions(${MEDIA_EDITOR_BINARY} PUBLIC APP_NAME="${MEDIA_EDITOR_BINARY}")
#set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${MEDIA_EDITOR_BINARY} PROPERTY C_STANDARD 11)
target_link_libraries(
    ${MEDIA_EDITOR_BINARY} 
    LINK_PRIVATE
    ${MEDIACORE_LIBRARYS}
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
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
    MediaCore
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
    Threads::Threads
)
endif(APPLE)

if(BUILD_TEST)
# MediaPlayer Test
add_executable(
    media_player_test
    test/MediaPlayerTest.cpp
    test/Log.cpp
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    media_player_test
    ${MEDIACORE_LIBRARYS}
    ${IMGUI_LIBRARYS}
)
target_include_directories(
    media_player_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

#if(IMGUI_VULKAN_SHADER)
#add_executable(
#    transition_make
#    test/TransitionMake.cpp
#    ${IMGUI_APP_ENTRY_SRC}
#)
#target_link_libraries(
#    transition_make
#    ${IMGUI_LIBRARYS}
#    ${SDL2_STATIC_LDFLAGS}
#    ${SDL2_IMAGE_LIBRARY}
#)
#endif(IMGUI_VULKAN_SHADER)
endif(BUILD_TEST)
endif(IMGUI_APPS)

## build plugins
if(IMGUI_VULKAN_SHADER)
#filers
add_subdirectory(plugin/nodes/filters/ALM)
#fusions
add_subdirectory(plugin/nodes/fusions/Alpha)
add_subdirectory(plugin/nodes/fusions/BookFlip)
add_subdirectory(plugin/nodes/fusions/Blur)
add_subdirectory(plugin/nodes/fusions/Bounce)
add_subdirectory(plugin/nodes/fusions/BowTie)
add_subdirectory(plugin/nodes/fusions/Burn)
add_subdirectory(plugin/nodes/fusions/BurnOut)
add_subdirectory(plugin/nodes/fusions/ButterflyWave)
add_subdirectory(plugin/nodes/fusions/CannabisLeaf)
add_subdirectory(plugin/nodes/fusions/CircleBlur)
add_subdirectory(plugin/nodes/fusions/CircleCrop)
add_subdirectory(plugin/nodes/fusions/ColorPhase)
add_subdirectory(plugin/nodes/fusions/ColourDistance)
add_subdirectory(plugin/nodes/fusions/CrazyParametric)
add_subdirectory(plugin/nodes/fusions/Crosshatch)
add_subdirectory(plugin/nodes/fusions/CrossWarp)
add_subdirectory(plugin/nodes/fusions/CrossZoom)
add_subdirectory(plugin/nodes/fusions/Cube)
#effects
add_subdirectory(plugin/nodes/effects/Lighting)
add_subdirectory(plugin/nodes/effects/Star)
add_subdirectory(plugin/nodes/effects/Sway)
endif(IMGUI_VULKAN_SHADER)

### DEFINE THE PACKAGING (OS specific)
set(CPACK_PACKAGE_NAME  "mec")
if (IMGUI_VULKAN)
    if (IMGUI_SDL2)
        set(CPACK_PACKAGE_NAME  "MEC_SDL2_Vulkan")
    elseif (IMGUI_GLFW)
        set(CPACK_PACKAGE_NAME  "MEC_GLFW_Vulkan")
    endif()
elseif (IMGUI_GL3)
    if (IMGUI_SDL2)
        set(CPACK_PACKAGE_NAME  "MEC_SDL2_OpenGL3")
    elseif (IMGUI_GLFW)
        set(CPACK_PACKAGE_NAME  "MEC_GLFW_OpenGL3")
    endif()
elseif (IMGUI_GL2)
    if (IMGUI_SDL2)
        set(CPACK_PACKAGE_NAME  "MEC_SDL2_OpenGL2")
    elseif (IMGUI_GLFW)
        set(CPACK_PACKAGE_NAME  "MEC_GLFW_OpenGL2")
    endif()
endif()
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MEC installation.")
set(CPACK_PACKAGE_VENDOR "CodeWin")
set(CPACK_PACKAGE_VERSION ${MEDIAEDITOR_VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${MEDIAEDITOR_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${MEDIAEDITOR_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${MEDIAEDITOR_VERSION_PATCH})

if(IMGUI_APPS)
if(APPLE)
    set(CPACK_SYSTEM_NAME "macos")
    set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})
    # Bundle target
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_BINARY_DRAGNDROP ON)
    # OSX cpack info
    install(TARGETS mec
            CONFIGURATIONS Release MinSizeRel RelWithDebInfo Debug
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )
    set(bindirs mec.app/Contents/MacOS)
    set(frameworks_dir mec.app/Contents/Frameworks/)
    set(resources_dir mec.app/Contents/Resources/)
    set(contents_dir mec.app/Contents/)

    # Install Resource File
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/languages"
            DESTINATION "${resources_dir}" COMPONENT Runtime)
    install(FILES  "${PROJECT_SOURCE_DIR}/resources/mec_logo.png"
            DESTINATION "${resources_dir}/" COMPONENT Runtime)

    # Install Plugins
    install(DIRECTORY  "${PROJECT_BINARY_DIR}/plugins"
            DESTINATION "${contents_dir}" COMPONENT Runtime)

    INSTALL(CODE [[
        set(plugins_dir "mec.app/Contents/plugins")
        file(GLOB_RECURSE PLUGINS ${plugins_dir} "*.node")
        set(APPS "${CMAKE_INSTALL_PREFIX}/mec.app")
        set(DIRS ${FRAMEWORKS_PATH})
        list (APPEND DIRS "/usr/local/lib/")
        list (APPEND DIRS "${CMAKE_INSTALL_PREFIX}")
        include(InstallRequiredSystemLibraries)
        include(BundleUtilities)
        fixup_bundle("${APPS}" "${PLUGINS}" "${DIRS}")
    ]])

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
    set(CPACK_SYSTEM_NAME "linux")
    install(CODE "
        include(../cmake/appimage.cmake)
        make_appimage(
            PROJECT_DIR \"${PROJECT_SOURCE_DIR}\"
            EXE \"mec\"
            NAME \"Media Editor Community\"
            OUTPUT_NAME \"${CPACK_PACKAGE_NAME}-${CPACK_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CPACK_PACKAGE_VERSION}\"
            DESKTOP_SRC \"${PROJECT_SOURCE_DIR}/resources/mec.desktop\"
            ICON_SRC \"${PROJECT_SOURCE_DIR}/resources/mec_logo.png\"
            RESOURCE_FILES \"${PROJECT_SOURCE_DIR}/languages\"
            PLUGINS \"${PROJECT_BINARY_DIR}/plugins\"
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

    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")
    install( CODE "set(APPS \"${CMAKE_BINARY_DIR}/mec.exe\")" )
    install( CODE [[
        file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")
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

    install(TARGETS mec
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION bin
            COMPONENT applications)

    # Install Resource File
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/languages/"
            DESTINATION languages COMPONENT Resource)
    install(FILES  "${PROJECT_SOURCE_DIR}/resources/mec_logo.png"
            DESTINATION resources COMPONENT Resource)
    # Install Plugins
    install(DIRECTORY  "${PROJECT_BINARY_DIR}/plugins"
            DESTINATION plugins COMPONENT Runtime)

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
endif(IMGUI_APPS)

# Package full name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CPACK_PACKAGE_VERSION}")
# To Create a package, run "cpack"
include(CPack)
endif (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")