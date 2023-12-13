cmake_minimum_required(VERSION 3.5.1)
project(custom_shader_node)
set(CMAKE_CXX_STANDARD 17)
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

set(PLUGIN custom_shader)
add_library(
    ${PLUGIN}
    SHARED
    custom_shader_node.cpp
    CustomShader.cpp
    CustomShader.h
)

add_dependencies(${PLUGIN} BluePrintSDK VkShader imgui)
set_property(TARGET ${PLUGIN} PROPERTY POSITION_INDEPENDENT_CODE ON)
set (LINK_LIBS
    -L${CMAKE_BINARY_DIR}
    -lBluePrintSDK
    -lVkShader
    -limgui
)

target_link_libraries(
    ${PLUGIN}
    ${LINK_LIBS}
)

set_target_properties(
    ${PLUGIN}
    PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/media/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/media/"
)