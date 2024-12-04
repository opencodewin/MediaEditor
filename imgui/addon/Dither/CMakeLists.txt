cmake_minimum_required(VERSION 3.12.0)
PROJECT(Dither)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_MACOSX_RPATH 0)
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()

option(BUILD_DITHER_STATIC          "Set to ON to build Static Library"  ON)

## core library
SET(DITHER_SOURCE_FILES
    src/dither_grid.cpp
    src/dither_errordiff.cpp
    src/dither_ordered.cpp
    src/dither_dotdiff.cpp
    src/dither_varerrdiff.cpp
    src/dither_threshold.cpp
    src/dither_dbs.cpp
    src/dither_kallebach.cpp
    src/dither_riemersma.cpp
    src/dither_pattern.cpp
    src/dither_dotlippens.cpp
    src/libdither.cpp
    src/hash.cpp
    src/random.cpp
    src/queue.cpp
)

SET(DITHER_HEADER_FILES
    src/dither_errordiff_data.h
    src/dither_ordered_data.h
    src/dither_dotdiff_data.h
    src/dither_varerrdiff_data.h
    src/dither_kallebach_data.h
    src/dither_riemersma_data.h
    src/dither_pattern_data.h
    src/dither_dotlippens_data.h
    src/hash.h
    src/random.h
    src/queue.h
    inc/libdither.h
)

set(DITHER_INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

include_directories(${DITHER_INC_DIRS})

if (BUILD_DITHER_STATIC)
set(LIBRARY STATIC)
else(BUILD_DITHER_STATIC)
set(LIBRARY SHARED)
endif(BUILD_DITHER_STATIC)

add_library(dither
    ${LIBRARY}
    ${DITHER_SOURCE_FILES}
    ${DITHER_HEADER_FILES}
)

add_compile_definitions(LIB_VERSION="CodeWin")

if (NOT BUILD_DITHER_STATIC)
set_target_properties(dither PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if (IMGUI_BUILD_EXAMPLE)
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
if (APPLE AND OPENMP_FOUND)
    message(STATUS "VkShader(Apple) Use OpenMP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    include_directories(${OpenMP_CXX_INCLUDE_DIR})
    set(LINK_LIBS ${LINK_LIBS} ${OpenMP_omp_LIBRARY})
elseif(OPENMP_FOUND)
    if(WIN32 OR MINGW)
        if (NOT VKSHADER_STATIC)
            message(STATUS "VkShader(Windows) Use OpenMP")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
            if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                set(LINK_LIBS ${LINK_LIBS} omp pthread)
            else ()
                set(LINK_LIBS ${LINK_LIBS} gomp pthread)
            endif ()
        endif()
    else()
        message(STATUS "VkShader Use OpenMP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(LINK_LIBS ${LINK_LIBS} omp pthread)
        else ()
            set(LINK_LIBS ${LINK_LIBS} gomp pthread)
        endif ()
    endif()
endif()
add_executable(
    dither_test
    test/test.cpp
    test/bmp.cpp
    test/bmp.h
)

target_link_libraries(
    dither_test
    dither
    ${LINK_LIBS}
)
endif(IMGUI_BUILD_EXAMPLE)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
set(DITHER_LIBRARYS dither ${LINK_LIBS} PARENT_SCOPE )
set(DITHER_INCLUDES ${DITHER_HEADER_FILES} PARENT_SCOPE )
set(DITHER_INCLUDE_DIRS ${DITHER_INC_DIRS} PARENT_SCOPE )
endif(hasParent)
