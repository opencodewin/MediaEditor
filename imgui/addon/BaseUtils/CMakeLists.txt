cmake_minimum_required(VERSION 3.12.0)
project(BaseUtils VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
set(BaseUtils_STATIC ON)
endif()

if(NOT DEFINED BaseUtils_STATIC)
option(BaseUtils_STATIC             "Build BaseUtils as static library" OFF)
endif()
if(BaseUtils_STATIC)
    message("Build BaseUtils as static library")
    set(LIBRARY_TYPE STATIC)
else(BaseUtils_STATIC)
    message("Build BaseUtils as shared library")
    set(LIBRARY_TYPE SHARED)
endif(BaseUtils_STATIC)

set(BaseUtils_VERSION_MAJOR 0)
set(BaseUtils_VERSION_MINOR 1)
set(BaseUtils_VERSION_PATCH 0)
string(TIMESTAMP BaseUtils_VERSION_BUILD "%y%m%d")
set(BaseUtils_VERSION_STRING ${BaseUtils_VERSION_MAJOR}.${BaseUtils_VERSION_MINOR}.${BaseUtils_VERSION_PATCH})
add_definitions(-DBaseUtils_VERSION_MAJOR=${BaseUtils_VERSION_MAJOR})
add_definitions(-DBaseUtils_VERSION_MINOR=${BaseUtils_VERSION_MINOR})
add_definitions(-DBaseUtils_VERSION_PATCH=${BaseUtils_VERSION_PATCH})
add_definitions(-DBaseUtils_VERSION_BUILD=${BaseUtils_VERSION_BUILD})

set(LIB_SRC_DIR ${PROJECT_SOURCE_DIR}/Src)
add_library(BaseUtils ${LIBRARY_TYPE}
    ${LIB_SRC_DIR}/CpuUtils.cpp
    ${LIB_SRC_DIR}/FileSystemUtils.cpp
    ${LIB_SRC_DIR}/Logger.cpp
    ${LIB_SRC_DIR}/SimdOpt.cpp
    ${LIB_SRC_DIR}/ThreadUtils.cpp
    ${LIB_SRC_DIR}/TimeUtils.cpp
)
target_include_directories(BaseUtils PRIVATE
    ${LIB_SRC_DIR}
)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
if(SIMD_ARCH_X86)
target_compile_definitions(BaseUtils PUBLIC SIMD_ARCH_X86)
target_compile_options(BaseUtils PUBLIC -mavx2 -mavx -msse4.2 -msse4.1 -mssse3 -msse3 -msse2 -msse -mfma -mf16c)
endif(SIMD_ARCH_X86)
endif()

if(!BaseUtils_STATIC)
target_compile_definitions(BaseUtils PUBLIC BaseUtils_SHARED)
endif()

target_include_directories(BaseUtils PUBLIC
    ${PROJECT_SOURCE_DIR}/Include
    ${IMGUI_INCLUDE_DIRS}
)

if(NOT BaseUtils_STATIC)
set_property(TARGET BaseUtils PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(BaseUtils PROPERTIES VERSION ${BaseUtils_VERSION_STRING} SOVERSION ${BaseUtils_VERSION_MAJOR})
endif()
