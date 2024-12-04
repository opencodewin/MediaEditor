#
# Cmake for building ImVulkanShader
#
cmake_minimum_required(VERSION 3.12.0)
project(ImVulkanShader)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()
if(POLICY CMP0025)
    # reference from https://cmake.org/cmake/help/latest/policy/CMP0025.html
    cmake_policy(SET CMP0025 NEW)
endif()
if (POLICY CMP0111)
    cmake_policy(SET CMP0111 NEW)
endif()
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(${PROJECT_SOURCE_DIR}/cmake/generater_shader_comp.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/precompile_shader_spv.cmake)

#if we need run on MacOS before 12.0, then uncomment following code, but not guarantee it's working
#if(APPLE)
#set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") 
#endif(APPLE)

option(VKSHADER_VULKAN_BENCHMARK      "Enable Vulkan Shader Benchmark" OFF)
option(VKSHADER_VULKAN_PREBUILD       "Enable Vulkan Shader prebuild check" OFF)
option(VKSHADER_VULKAN_FP16           "Enable Vulkan Shader support 16bits float" ON)
option(VKSHADER_STATIC                "Build Vulkan Shader as static library" OFF)
#option(VKSHADER_BUILD_TESTS           "Build Vulkan Shader Tests" OFF)
option(VKSHADER_SKIP_INSTALL          "Skip Vulkan install" ON)

find_package(PkgConfig)

set(LINK_LIBS)

if(IOS AND CMAKE_OSX_ARCHITECTURES MATCHES "arm")
    message(STATUS "Target arch: arm-ios")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
    set(VKSHADER_VULKAN_BENCHMARK OFF CACHE BOOL "Disable Vulkan Benchmark by Force" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    message(STATUS "Target arch: arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
    set(VKSHADER_VULKAN_BENCHMARK OFF CACHE BOOL "Disable Vulkan Benchmark by Force" FORCE)
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|x86_64|AMD64)")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86")
        message(STATUS "Target arch: x86")
    else()
        message(STATUS "Target arch: x86_64")
    endif()
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

if(NOT VULKAN_FOUND AND PKG_CONFIG_FOUND)
    pkg_search_module(VULKAN REQUIRED vulkan glslang)
endif()

if (VULKAN_FOUND)
    message(STATUS "[ImVulkanShader] vulkan found")
    set(VKSHADER_INC_DIRS
        ${Vulkan_INCLUDE_DIR}
        ${VULKAN_INCLUDE_DIRS}
    )
    if ( NOT "${VULKAN_LDFLAGS}" STREQUAL "")
        message(STATUS "[ImVulkanShader] vulkan link with ${VULKAN_LDFLAGS}")
        set(LINK_LIBS
            ${LINK_LIBS}
            ${VULKAN_LDFLAGS}
        )
    elseif (NOT "${Vulkan_LIBRARY}" STREQUAL "")
        message(STATUS "[ImVulkanShader] vulkan link with ${Vulkan_LIBRARY}")
        set(LINK_LIBS
            ${LINK_LIBS}
            ${Vulkan_LIBRARY}
        )
    endif()

    find_library(Glslang_LIBRARY_RELEASE NAMES glslang)
    if (NOT "${Glslang_LIBRARY_RELEASE}" STREQUAL "Glslang_LIBRARY_RELEASE-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${Glslang_LIBRARY_RELEASE}
        )
    endif()
    find_library(Glslang_OGLCompiler_LIBRARY_RELEASE NAMES OGLCompiler)
    if (NOT "${Glslang_OGLCompiler_LIBRARY_RELEASE}" STREQUAL "Glslang_OGLCompiler_LIBRARY_RELEASE-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${Glslang_OGLCompiler_LIBRARY_RELEASE}
        )
    endif()
    find_library(Glslang_SPIRV_LIBRARY_RELEASE NAMES SPIRV)
    if (NOT "${Glslang_SPIRV_LIBRARY_RELEASE}" STREQUAL "Glslang_SPIRV_LIBRARY_RELEASE-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${Glslang_SPIRV_LIBRARY_RELEASE}
        )
    endif()
    find_library(Glslang_GenericCodeGen_LIBRARY_RELEASE NAMES GenericCodeGen)
    if (NOT "${Glslang_GenericCodeGen_LIBRARY_RELEASE}" STREQUAL "Glslang_GenericCodeGen_LIBRARY_RELEASE-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${Glslang_GenericCodeGen_LIBRARY_RELEASE}
        )
    endif()
    find_library(Glslang_OSDependent_LIBRARY_RELEASE NAMES OSDependent)
    if (NOT "${Glslang_OSDependent_LIBRARY_RELEASE}" STREQUAL "Glslang_OSDependent_LIBRARY_RELEASE-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${Glslang_OSDependent_LIBRARY_RELEASE}
        )
    endif()
    find_library(SpirvTools_LIBRARY NAMES SPIRV-Tools)
    if (NOT "${SpirvTools_LIBRARY}" STREQUAL "SpirvTools_LIBRARY-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${SpirvTools_LIBRARY}
        )
    endif()
    find_library(SpirvTools_Opt_LIBRARY NAMES SPIRV-Tools-opt)
    if (NOT "${SpirvTools_Opt_LIBRARY}" STREQUAL "SpirvTools_Opt_LIBRARY-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${SpirvTools_Opt_LIBRARY}
        )
    endif()
    if(NOT WIN32 AND NOT MINGW)
        find_library(Glslang_MachineIndependent_LIBRARY_RELEASE NAMES MachineIndependent)
        if (NOT "${Glslang_MachineIndependent_LIBRARY_RELEASE}" STREQUAL "Glslang_MachineIndependent_LIBRARY_RELEASE-NOTFOUND")
            set(GLSLANG_LIBRARY
                ${GLSLANG_LIBRARY}
                ${Glslang_MachineIndependent_LIBRARY_RELEASE}
            )
        endif()
    endif()
    message(STATUS "Glslang: " ${GLSLANG_LIBRARY})
endif(VULKAN_FOUND)

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

find_program(GLSLANGVALIDATOR_EXECUTABLE NAMES glslangValidator PATHS $ENV{VULKAN_SDK}/bin NO_CMAKE_FIND_ROOT_PATH)
message(STATUS "VkShader Found glslangValidator: ${GLSLANGVALIDATOR_EXECUTABLE}")
macro(compile_shader header data)
generater_shader_comp(SHADER_COMP ${CMAKE_CURRENT_SOURCE_DIR}/${header} ${data})
precompile_shader_spv(SHADER_SPV_HEX ${SHADER_COMP})
list(APPEND SHADER_SPV_HEX_FILES ${SHADER_SPV_HEX})
endmacro()

if(VKSHADER_VULKAN_PREBUILD)
set(SHADER_COMP_FILES)
set(SHADER_SPV_HEX_FILES)
#internal shaders
compile_shader(imvk_Cast_shader.h cast_fp16_to_fp32_pack4)
compile_shader(imvk_Cast_shader.h cast_fp16_to_fp32_pack8)
compile_shader(imvk_Cast_shader.h cast_fp16_to_fp32)
compile_shader(imvk_Cast_shader.h cast_fp32_to_fp16_pack4)
compile_shader(imvk_Cast_shader.h cast_fp32_to_fp16_pack8)
compile_shader(imvk_Cast_shader.h cast_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack1to4_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack1to4_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack1to4)
compile_shader(imvk_Packing_shader.h packing_pack1to8_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack1to8_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack1to8)
compile_shader(imvk_Packing_shader.h packing_pack4_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack4_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack4)
compile_shader(imvk_Packing_shader.h packing_pack4to1_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack4to1_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack4to1)
compile_shader(imvk_Packing_shader.h packing_pack4to8_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack4to8_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack4to8)
compile_shader(imvk_Packing_shader.h packing_pack8_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack8_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack8)
compile_shader(imvk_Packing_shader.h packing_pack8to1_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack8to1_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack8to1)
compile_shader(imvk_Packing_shader.h packing_pack8to4_fp16_to_fp32)
compile_shader(imvk_Packing_shader.h packing_pack8to4_fp32_to_fp16)
compile_shader(imvk_Packing_shader.h packing_pack8to4)
compile_shader(imvk_Packing_shader.h packing)
# general
compile_shader(internals/AlphaBlending_shader.h AlphaBlending_data)
compile_shader(internals/AlphaBlending_shader.h AlphaBlending_alpha_data)
compile_shader(internals/ColorConvert_shader.h YUV2RGB_data)
compile_shader(internals/ColorConvert_shader.h RGB2YUV_data)
compile_shader(internals/ColorConvert_shader.h GRAY2RGB_data)
compile_shader(internals/ColorConvert_shader.h Conv_data)
compile_shader(internals/Concat_shader.h Shader_data)
compile_shader(internals/Copy_make_border_shader.h Filter_data)
compile_shader(internals/CopyTo_shader.h CopyTo_data)
compile_shader(internals/Crop_shader.h CropShader_data)
compile_shader(internals/Crop_shader.h CropToShader_data)
compile_shader(internals/Flip_shader.h Shader_data)
compile_shader(internals/Resize_shader.h Resize_data)
compile_shader(internals/Substract_mean_normalize_shader.h Filter_data)
compile_shader(internals/Transpose_shader.h Filter_data)
compile_shader(internals/warpAffine_shader.h Filter_data)
compile_shader(internals/warpPerspective_shader.h Filter_data)
compile_shader(internals/Binary_shader.h Filter_data)
# filters
compile_shader(filters/Bilateral_shader.h Filter_data)
compile_shader(filters/Brightness_shader.h Filter_data)
compile_shader(filters/ColorInvert_shader.h Filter_data)
compile_shader(filters/Contrast_shader.h Filter_data)
compile_shader(filters/Exposure_shader.h Filter_data)
compile_shader(filters/Filter2D_shader.h Filter_data)
compile_shader(filters/Filter2DS_shader.h FilterColumn_data)
compile_shader(filters/Filter2DS_shader.h FilterRow_data)
compile_shader(filters/Filter2DS_shader.h FilterColumnMono_data)
compile_shader(filters/Filter2DS_shader.h FilterRowMono_data)
compile_shader(filters/Gamma_shader.h Filter_data)
compile_shader(filters/Hue_shader.h Filter_data)
compile_shader(filters/Saturation_shader.h Filter_data)
compile_shader(filters/Lut3D_shader.h LUT3D_data)
compile_shader(filters/Dilation_shader.h Filter_data)
compile_shader(filters/Erosion_shader.h Filter_data)
# scopes
compile_shader(scopes/CIE_shader.h CIE_data)
compile_shader(scopes/CIE_shader.h CIE_merge_data)
compile_shader(scopes/CIE_shader.h CIE_set_data)
compile_shader(scopes/Harris_shader.h PrewittFilter_data)
compile_shader(scopes/Harris_shader.h HarrisFilter_data)
compile_shader(scopes/Harris_shader.h NMSFilter_data)
compile_shader(scopes/Histogram_shader.h Histogram_data)
compile_shader(scopes/Histogram_shader.h Zero_data)
compile_shader(scopes/Histogram_shader.h ConvInt2Float_data)
compile_shader(scopes/Vector_shader.h Vector_data)
compile_shader(scopes/Vector_shader.h Zero_data)
compile_shader(scopes/Vector_shader.h Vector_merge_data)
compile_shader(scopes/Waveform_shader.h Waveform_data)
compile_shader(scopes/Waveform_shader.h Zero_data)
compile_shader(scopes/Waveform_shader.h ConvInt2Mat_data)
add_custom_target(generate-comp DEPENDS ${SHADER_SPV_HEX_FILES})

endif(VKSHADER_VULKAN_PREBUILD)

if(VKSHADER_VULKAN_BENCHMARK)
    add_definitions(-DVULKAN_SHADER_BENCHMARK)
    message(STATUS "VkShader Support benchmark")
endif(VKSHADER_VULKAN_BENCHMARK)

if(VKSHADER_VULKAN_FP16)
    add_definitions(-DVULKAN_SHADER_FP16)
    message(STATUS "VkShader Support 16bits float")
endif(VKSHADER_VULKAN_FP16)

set(VKSHADER_SRCS
    imvk_option.cpp
    imvk_allocator.cpp
    imvk_gpu.cpp
    imvk_command.cpp
    imvk_pipeline.cpp
    imvk_pipelinecache.cpp
    imvk_Cast_vulkan.cpp
    imvk_Packing_vulkan.cpp
    ImVulkanShader.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    imvk_platform.h
    imvk_vulkan_header_fix.h
    imvk_option.h
    imvk_allocator.h
    imvk_gpu.h
    imvk_command.h
    imvk_mat.h
    imvk_mat_shader.h
    imvk_image_mat.h
    imvk_pipeline.h
    imvk_pipelinecache.h
    imvk_Cast_shader.h
    imvk_Cast_vulkan.h
    imvk_Packing_shader.h
    imvk_Packing_vulkan.h
    ImVulkanShader.h
)

# add filter
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    filters/Filter2D_vulkan.cpp
    filters/Filter2DS_vulkan.cpp
    filters/Brightness_vulkan.cpp
    filters/Contrast_vulkan.cpp
    filters/Exposure_vulkan.cpp
    filters/Gamma_vulkan.cpp
    filters/Lut3D_vulkan.cpp
    filters/Saturation_vulkan.cpp
    filters/Hue_vulkan.cpp
    filters/ColorInvert_vulkan.cpp
    filters/Bilateral_vulkan.cpp
    filters/Box_vulkan.cpp
    filters/Gaussian_vulkan.cpp
    filters/Dilation_vulkan.cpp
    filters/Erosion_vulkan.cpp
    filters/USM_vulkan.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    filters/Filter2D_shader.h
    filters/Filter2D_vulkan.h
    filters/Filter2DS_shader.h
    filters/Filter2DS_vulkan.h
    filters/Brightness_shader.h
    filters/Brightness_vulkan.h
    filters/Contrast_shader.h
    filters/Contrast_vulkan.h
    filters/Exposure_shader.h
    filters/Exposure_vulkan.h
    filters/Gamma_shader.h
    filters/Gamma_vulkan.h
    filters/Lut3D_vulkan.h
    filters/Lut3D_shader.h
    filters/Saturation_shader.h
    filters/Saturation_vulkan.h
    filters/Hue_shader.h
    filters/Hue_vulkan.h
    filters/ColorInvert_shader.h
    filters/ColorInvert_vulkan.h
    filters/Bilateral_shader.h
    filters/Bilateral_vulkan.h
    filters/Box_vulkan.h
    filters/Gaussian_vulkan.h
    filters/Dilation_shader.h
    filters/Dilation_vulkan.h
    filters/Erosion_shader.h
    filters/Erosion_vulkan.h
    filters/USM_shader.h
    filters/USM_vulkan.h
)

set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/filters
)

# add scopes
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    scopes/Harris_vulkan.cpp
    scopes/CIE_vulkan.cpp
    scopes/Histogram_vulkan.cpp
    scopes/Waveform_vulkan.cpp
    scopes/Vector_vulkan.cpp
)

set(VKSHADER_INCS
    ${VKSHADER_INCS}
    scopes/Harris_vulkan.h
    scopes/Harris_shader.h
    scopes/CIE_shader.h
    scopes/CIE_vulkan.h
    scopes/Histogram_shader.h
    scopes/Histogram_vulkan.h
    scopes/Waveform_shader.h
    scopes/Waveform_vulkan.h
    scopes/Vector_shader.h
    scopes/Vector_vulkan.h
)

set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/scopes
)

# add internals
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    internals/AlphaBlending_vulkan.cpp
    internals/colorspace_table.cpp
    internals/ColorConvert_vulkan.cpp
    internals/Resize_vulkan.cpp
    internals/CopyTo_vulkan.cpp
    internals/Flip_vulkan.cpp
    internals/Crop_vulkan.cpp
    internals/Transpose_vulkan.cpp
    internals/Concat_vulkan.cpp
    internals/warpPerspective_vulkan.cpp
    internals/warpAffine_vulkan.cpp
    internals/Substract_mean_normalize.cpp
    internals/Copy_make_border.cpp
    internals/Binary_vulkan.cpp
    internals/OpacityFilter_vulkan.cpp
    internals/Expand_vulkan.cpp
)

set(VKSHADER_INCS
    ${VKSHADER_INCS}
    internals/AlphaBlending_shader.h
    internals/AlphaBlending_vulkan.h
    internals/ColorConvert_shader.h
    internals/ColorConvert_vulkan.h
    internals/Resize_shader.h
    internals/Resize_vulkan.h
    internals/CopyTo_shader.h
    internals/CopyTo_vulkan.h
    internals/Flip_shader.h
    internals/Flip_vulkan.h
    internals/Crop_shader.h
    internals/Crop_vulkan.h
    internals/Transpose_shader.h
    internals/Transpose_vulkan.h
    internals/Concat_shader.h
    internals/Concat_vulkan.h
    internals/warpPerspective_shader.h
    internals/warpPerspective_vulkan.h
    internals/warpAffine_shader.h
    internals/warpAffine_vulkan.h
    internals/Substract_mean_normalize_shader.h
    internals/Substract_mean_normalize.h
    internals/Copy_make_border_shader.h
    internals/Copy_make_border.h
    internals/Binary_shader.h
    internals/Binary_vulkan.h
    internals/Expand_shader.h
    internals/Expand_vulkan.h
)

set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/internals
)

include_directories(${VKSHADER_INC_DIRS})

if(VKSHADER_STATIC)
    set(LIBRARY STATIC)
    add_definitions(-DVKSHADER_STATIC_LIBRARY)
else(VKSHADER_STATIC)
    set(LIBRARY SHARED)
    add_definitions(-DVKSHADER_SHARED_LIBRARY)
endif(VKSHADER_STATIC)

add_library(
    VkShader
    ${LIBRARY}
    ${VKSHADER_SRCS}
    ${VKSHADER_INCS}
)

if(VKSHADER_VULKAN_PREBUILD)
add_dependencies(VkShader generate-comp)
endif(VKSHADER_VULKAN_PREBUILD)

set(VKSHADER_VERSION_MAJOR 1)
set(VKSHADER_VERSION_MINOR 13)
set(VKSHADER_VERSION_PATCH 6)
string(TIMESTAMP VKSHADER_VERSION_BUILD "%y%m%d")
set(VKSHADER_VERSION_STRING ${VKSHADER_VERSION_MAJOR}.${VKSHADER_VERSION_MINOR}.${VKSHADER_VERSION_PATCH})
add_definitions(-DVKSHADER_VERSION_MAJOR=${VKSHADER_VERSION_MAJOR})
add_definitions(-DVKSHADER_VERSION_MINOR=${VKSHADER_VERSION_MINOR})
add_definitions(-DVKSHADER_VERSION_PATCH=${VKSHADER_VERSION_PATCH})
add_definitions(-DVKSHADER_VERSION_BUILD=${VKSHADER_VERSION_BUILD})

if(NOT VKSHADER_STATIC)
set_property(TARGET VkShader PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(VkShader PROPERTIES VERSION ${VKSHADER_VERSION_STRING} SOVERSION ${VKSHADER_VERSION_MAJOR})
endif()
target_link_libraries(VkShader ${LINK_LIBS} ${GLSLANG_LIBRARY})

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(VKSHADER_LIBRARYS VkShader ${LINK_LIBS} PARENT_SCOPE )
    set(VKSHADER_INCLUDES ${VKSHADER_INCS} PARENT_SCOPE )
    set(VKSHADER_INCLUDE_DIRS ${VKSHADER_INC_DIRS} PARENT_SCOPE )
endif()
if(UNIX AND NOT VKSHADER_SKIP_INSTALL)
install(TARGETS VkShader
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

#if(VKSHADER_BUILD_TESTS)
#    add_executable(
#        ImVulkan_tests
#        test/ImVulkan_tests.cpp
#    )
#    target_link_libraries(
#        ImVulkan_tests
#        VkShader
#        ${LINK_LIBS}
#    )
#endif(VKSHADER_BUILD_TESTS)