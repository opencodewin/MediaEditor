cmake_minimum_required(VERSION 3.12.0)
project(ImMaskCreator VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(ImMaskCreator_STATIC ON)
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|x86_64|AMD64)")
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set(SIMD_ARCH_X86 ON)
    endif()
endif()

if(NOT DEFINED ImMaskCreator_STATIC)
option(ImMaskCreator_STATIC             "Build ImMaskCreator as static library" OFF)
endif()
if(ImMaskCreator_STATIC)
    message("Build ImMaskCreator as static library")
    set(LIBRARY_TYPE STATIC)
else(ImMaskCreator_STATIC)
    message("Build ImMaskCreator as shared library")
    set(LIBRARY_TYPE SHARED)
endif(ImMaskCreator_STATIC)


set(ImMaskCreator_VERSION_MAJOR 1)
set(ImMaskCreator_VERSION_MINOR 1)
set(ImMaskCreator_VERSION_PATCH 1)
string(TIMESTAMP ImMaskCreator_VERSION_BUILD "%y%m%d")
set(ImMaskCreator_VERSION_STRING ${ImMaskCreator_VERSION_MAJOR}.${ImMaskCreator_VERSION_MINOR}.${ImMaskCreator_VERSION_PATCH})
add_definitions(-DImMaskCreator_VERSION_MAJOR=${ImMaskCreator_VERSION_MAJOR})
add_definitions(-DImMaskCreator_VERSION_MINOR=${ImMaskCreator_VERSION_MINOR})
add_definitions(-DImMaskCreator_VERSION_PATCH=${ImMaskCreator_VERSION_PATCH})
add_definitions(-DImMaskCreator_VERSION_BUILD=${ImMaskCreator_VERSION_BUILD})

set(LIB_SRC_DIR ${PROJECT_SOURCE_DIR}/Src)
add_library(ImMaskCreator ${LIBRARY_TYPE}
    ${LIB_SRC_DIR}/ImMaskCreatorImpl.cpp
    ${LIB_SRC_DIR}/Contour2Mask.cpp
    ${LIB_SRC_DIR}/MatFilter.cpp
    ${LIB_SRC_DIR}/MatIo.cpp
    ${LIB_SRC_DIR}/MatMath.cpp
    ${LIB_SRC_DIR}/Morph.cpp
    ${LIB_SRC_DIR}/BoxFilter.cpp
)
target_include_directories(ImMaskCreator PRIVATE
    ${LIB_SRC_DIR}
    ${IMGUI_INC_DIRS}
)

if(SIMD_ARCH_X86)
target_compile_definitions(ImMaskCreator PRIVATE SIMD_ARCH_X86)
target_compile_options(ImMaskCreator PRIVATE -mavx2 -mavx -msse4.2 -msse4.1 -mssse3 -msse3 -msse2 -msse -mfma -mf16c)
endif(SIMD_ARCH_X86)

# Apple try to find openmp if we using clang
if (APPLE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang\$")
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_C_LIB_NAMES "omp")
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(OpenMP_C_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/opt/homebrew/opt/libomp/lib ${OpenMP_C_LIB_NAMES})
        else()
            set(OpenMP_C_INCLUDE_DIR "/usr/local/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/usr/local/opt/libomp/lib ${OpenMP_C_LIB_NAMES})
        endif()
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang\$")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_LIB_NAMES "omp")
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(OpenMP_CXX_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/opt/homebrew/opt/libomp/lib ${OpenMP_CXX_LIB_NAMES})
        else()
            set(OpenMP_CXX_INCLUDE_DIR "/usr/local/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/usr/local/opt/libomp/lib ${OpenMP_CXX_LIB_NAMES})
        endif()
    endif()
endif()

find_package(OpenMP)
if (OPENMP_FOUND)
    if (APPLE)
        message(STATUS "ImMaskCreator(Apple) Support OpenMP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        include_directories(${OpenMP_CXX_INCLUDE_DIR})
        target_link_libraries(ImMaskCreator PUBLIC ${OpenMP_omp_LIBRARY})
    elseif (WIN32 OR MINGW)
        message(STATUS "ImMaskCreator(Windows) Support OpenMP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        target_link_libraries(ImMaskCreator PUBLIC gmp pthread)
    else ()
        message(STATUS "ImMaskCreator Support OpenMP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_link_libraries(ImMaskCreator PUBLIC omp pthread)
        else ()
            target_link_libraries(ImMaskCreator PUBLIC gomp pthread)
        endif ()
    endif ()
endif (OPENMP_FOUND)

target_include_directories(ImMaskCreator PUBLIC
    ${PROJECT_SOURCE_DIR}/Include
    ${IMGUI_INCLUDE_DIRS}
)

if(NOT ImMaskCreator_STATIC)
set_property(TARGET ImMaskCreator PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(ImMaskCreator PROPERTIES VERSION ${ImMaskCreator_VERSION_STRING} SOVERSION ${ImMaskCreator_VERSION_MAJOR})
endif()

target_link_libraries(ImMaskCreator PUBLIC
    BaseUtils
    imgui
)
