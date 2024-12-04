cmake_minimum_required(VERSION 3.12.0)
project(BluePrintSDK)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

option(IMGUI_BP_SDK_STATIC              "Build BluePrint as static library" OFF)

find_package(PkgConfig REQUIRED)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
set(IMGUI_BP_SDK_STATIC ON CACHE BOOL "Enable Static Build by Force" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2  -s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -Wno-unused-command-line-argument")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2  -s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -Wno-unused-command-line-argument")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pthreads-mem-growth -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pthreads-mem-growth -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4")
endif()

set(IMGUI_BP_SDK_SRC
    src/BluePrint.cpp
    src/Context.cpp
    src/Pin.cpp
    src/Node.cpp
    src/Icon.cpp
    src/Debug.cpp
    src/Utils.cpp
    src/Document.cpp
    src/UI.cpp
)

set(IMGUI_BP_SDK_INC
    include/BluePrint.h
    include/Pin.h
    include/Node.h
    include/Icon.h
    include/Debug.h
    include/Utils.h
    include/Document.h
    include/UI.h
    include/variant.hpp
    include/span.hpp
)

set(IMGUI_BP_SDK_INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/MediaNode
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/MediaNode/VkShader
)

# Generate all buildin node files into one header file
message(STATUS "Generate Build In Nodes")
file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/BuildInNodes.h)
list(APPEND incls "#pragma once\n")
list(APPEND incls "// Genetated By cmake, don't change it\n")
file(GLOB common_header_files "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/CommonNode/*.h")
list(FILTER common_header_files EXCLUDE REGEX "._")
foreach(f ${common_header_files})
    file(RELATIVE_PATH fr "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes" ${f})
    list(APPEND incls "#include \"${fr}\"\n")
endforeach()

file(GLOB system_header_files "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/SystemNode/*.h")
list(FILTER system_header_files EXCLUDE REGEX "._")
foreach(f ${system_header_files})
    file(RELATIVE_PATH fr "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes" ${f})
    list(APPEND incls "#include \"${fr}\"\n")
endforeach()

if(IMGUI_VULKAN_SHADER)
file(GLOB media_header_files "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/MediaNode/VkShader/*.h")
list(FILTER media_header_files EXCLUDE REGEX "._")
foreach(f ${media_header_files})
    file(RELATIVE_PATH fr "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes" ${f})
    list(APPEND incls "#include \"${fr}\"\n")
endforeach()
endif()

file(GLOB media_header_files "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/MediaNode/*.h")
list(FILTER media_header_files EXCLUDE REGEX "._")
foreach(f ${media_header_files})
    file(RELATIVE_PATH fr "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes" ${f})
    list(APPEND incls "#include \"${fr}\"\n")
endforeach()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildInNodes.h ${incls})

include_directories(${IMGUI_BP_SDK_INC_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

if(IMGUI_BP_SDK_STATIC)
set(LIBRARY STATIC)
else(IMGUI_BP_SDK_STATIC)
set(LIBRARY SHARED)
endif(IMGUI_BP_SDK_STATIC)

add_library(
    BluePrintSDK
    ${LIBRARY}
    ${IMGUI_BP_SDK_SRC}
    ${IMGUI_BP_NODE_LIST}
    ${IMGUI_BP_SDK_INC}
)
set_property(TARGET BluePrintSDK PROPERTY POSITION_INDEPENDENT_CODE ON)


set(IMGUI_BP_SDK_VERSION_MAJOR 1)
set(IMGUI_BP_SDK_VERSION_MINOR 23)
set(IMGUI_BP_SDK_VERSION_PATCH 2)
string(TIMESTAMP IMGUI_BP_SDK_VERSION_BUILD "%y%m%d")
set(IMGUI_BP_SDK_VERSION_STRING ${IMGUI_BP_SDK_VERSION_MAJOR}.${IMGUI_BP_SDK_VERSION_MINOR}.${IMGUI_BP_SDK_VERSION_PATCH})
SET(VERSION_MAJOR ${IMGUI_BP_SDK_VERSION_MAJOR})
SET(VERSION_MINOR ${IMGUI_BP_SDK_VERSION_MINOR})
SET(VERSION_PATCH ${IMGUI_BP_SDK_VERSION_PATCH})
SET(VERSION_BUILD ${IMGUI_BP_SDK_VERSION_BUILD})
set(IMGUI_BP_SDK_API_VERSION_MAJOR 1)
set(IMGUI_BP_SDK_API_VERSION_MINOR 2)
set(IMGUI_BP_SDK_API_VERSION_PATCH 1)
SET(API_VERSION_MAJOR ${IMGUI_BP_SDK_API_VERSION_MAJOR})
SET(API_VERSION_MINOR ${IMGUI_BP_SDK_API_VERSION_MINOR})
SET(API_VERSION_PATCH ${IMGUI_BP_SDK_API_VERSION_PATCH})
CONFIGURE_FILE(version.h.in version.h)

if(NOT IMGUI_BP_SDK_STATIC)
target_link_libraries(BluePrintSDK imgui ${LINK_LIBS})
set_target_properties(BluePrintSDK PROPERTIES VERSION ${IMGUI_BP_SDK_VERSION_STRING} SOVERSION ${IMGUI_BP_SDK_VERSION_MAJOR})
endif(NOT IMGUI_BP_SDK_STATIC)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(IMGUI_BLUEPRINT_SDK_LIBRARYS BluePrintSDK PARENT_SCOPE )
    set(IMGUI_BLUEPRINT_INCLUDES ${IMGUI_BP_SDK_INC} PARENT_SCOPE )
    set(IMGUI_BLUEPRINT_INCLUDE_DIRS ${IMGUI_BP_SDK_INC_DIRS} ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE )
endif()

if (IMGUI_BUILD_EXAMPLE AND IMGUI_APPS)
# build sdk test
add_executable(
    test_blueprint
    test/test.cpp
    ../${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    test_blueprint
    BluePrintSDK
    ${IMGUI_LIBRARYS}
)
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(WEB_DIR ${PROJECT_BINARY_DIR}/web/)
    set(SHARED_FILE ${CMAKE_CURRENT_SOURCE_DIR}/test/Blueprints_datetime.json)
    set_target_properties(
        test_blueprint
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${WEB_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${WEB_DIR}
        OUTPUT_NAME "index"
        PREFIX ""
        SUFFIX ".html"
        LINK_FLAGS "--preload-file ${SHARED_FILE}@tmp/test.json"
        #LINK_FLAGS "--shell-file ${IMGUI_SHELL_FILE}"
    )
endif()
endif()
