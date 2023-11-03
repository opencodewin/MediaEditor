cmake_minimum_required(VERSION 3.5.1)
project(audiofade_node)
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

add_library(
    AudioFade
    SHARED
    ImMatAudioFadeTransitionNode.cpp
)

add_dependencies(AudioFade BluePrintSDK imgui)
set_property(TARGET AudioFade PROPERTY POSITION_INDEPENDENT_CODE ON)
set (LINK_LIBS
    -L${CMAKE_BINARY_DIR}
    -lBluePrintSDK
    -limgui
)
target_link_libraries(
    AudioFade
    ${LINK_LIBS}
)

set_target_properties(
    AudioFade
    PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/fusions/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/fusions/"
)