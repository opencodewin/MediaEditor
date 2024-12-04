cmake_minimum_required(VERSION 3.12.0)
PROJECT(Potrace)
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

option(BUILD_POTRACE_STATIC          "Set to ON to build Static Library"  ON)

## core library
set(POTRACE_SOURCE_FILES
    src/backend_dxf.cpp
    src/backend_geojson.cpp
    src/backend_pgm.cpp
    src/backend_svg.cpp
    src/backend_mem.cpp
    src/bbox.cpp
    src/curve.cpp
    src/decompose.cpp
    src/greymap.cpp
    src/potracelib.cpp
    src/render.cpp
    src/trace.cpp
    src/trans.cpp
)

set(POTRACE_INCS
    inc/potracelib.h
    inc/auxiliary.h
    inc/backend.h
    inc/trans.h
    inc/bitmap.h
    inc/greymap.h
    src/bbox.h
    src/bitops.h
    src/curve.h
    src/decompose.h
    src/lists.h
    src/progress.h
    src/render.h
    src/trace.h
)

set(POTRACE_INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

include_directories(${POTRACE_INC_DIRS})

if (BUILD_POTRACE_STATIC)
set(LIBRARY STATIC)
else(BUILD_POTRACE_STATIC)
set(LIBRARY SHARED)
endif(BUILD_POTRACE_STATIC)

add_library(potrace
    ${LIBRARY}
    ${POTRACE_SOURCE_FILES}
    ${POTRACE_HEADER_FILES}
)

if (NOT BUILD_POTRACE_STATIC)
set_target_properties(potrace PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if (IMGUI_BUILD_EXAMPLE)
add_executable(
    potrace_test
    test/main.cpp
    test/bitmap_io.cpp
    test/bitmap_io.h
)
target_link_libraries(
    potrace_test
    potrace
)
endif(IMGUI_BUILD_EXAMPLE)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
set(POTRACE_LIBRARYS potrace ${LINK_LIBS} PARENT_SCOPE )
set(POTRACE_INCLUDES ${POTRACE_HEADER_FILES} PARENT_SCOPE )
set(POTRACE_INCLUDE_DIRS ${POTRACE_INC_DIRS} PARENT_SCOPE )
endif(hasParent)
