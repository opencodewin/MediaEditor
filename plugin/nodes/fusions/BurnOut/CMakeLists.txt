cmake_minimum_required(VERSION 3.5.1)
project(burnout_node)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_MACOSX_RPATH 0)
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()

add_library(
    BurnOut
    SHARED
    ImMatBurnOutFusionNode.cpp
    BurnOut_vulkan.cpp
    BurnOut_vulkan.h
    BurnOut_shader.h
)

set_property(TARGET BurnOut PROPERTY POSITION_INDEPENDENT_CODE ON)
set (LINK_LIBS
    ${IMGUI_BLUEPRINT_SDK_LIBRARYS}
    ${IMGUI_LIBRARYS}
)
target_link_libraries(
    BurnOut
    ${LINK_LIBS}
)

set_target_properties(
    BurnOut
    PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/fusions/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/fusions/"
)