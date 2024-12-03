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

if (APPLE)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-no_warn_duplicate_libraries")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-no_warn_duplicate_libraries")
endif(APPLE)

#if we need run on MacOS before 12.0, then uncomment following code, but not guarantee it's working
#if(APPLE)
#set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") 
#endif(APPLE)

option(VKSHADER_VULKAN_BENCHMARK      "Enable Vulkan Shader Benchmark" ON)
option(VKSHADER_VULKAN_PREBUILD       "Enable Vulkan Shader prebuild check" OFF)
option(VKSHADER_VULKAN_FP16           "Enable Vulkan Shader support 16bits float" ON)
option(VKSHADER_STATIC                "Build Vulkan Shader as static library" OFF)
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
    find_library(Glslang_GenericCodeGen_LIBRARY_RELEASE NAMES GenericCodeGen)
    if (NOT "${Glslang_GenericCodeGen_LIBRARY_RELEASE}" STREQUAL "Glslang_GenericCodeGen_LIBRARY_RELEASE-NOTFOUND")
        set(GLSLANG_LIBRARY
            ${GLSLANG_LIBRARY}
            ${Glslang_GenericCodeGen_LIBRARY_RELEASE}
        )
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
#compile_shader(shader/internals/imvk_Cast_shader.h cast_fp16_to_fp32_pack4)
#compile_shader(shader/internals/imvk_Cast_shader.h cast_fp16_to_fp32_pack8)
#compile_shader(shader/internals/imvk_Cast_shader.h cast_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Cast_shader.h cast_fp32_to_fp16_pack4)
#compile_shader(shader/internals/imvk_Cast_shader.h cast_fp32_to_fp16_pack8)
#compile_shader(shader/internals/imvk_Cast_shader.h cast_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack1to4_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack1to4_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack1to4)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack1to8_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack1to8_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack1to8)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4to1_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4to1_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4to1)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4to8_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4to8_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack4to8)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8to1_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8to1_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8to1)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8to4_fp16_to_fp32)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8to4_fp32_to_fp16)
#compile_shader(shader/internals/imvk_Packing_shader.h packing_pack8to4)
#compile_shader(shader/internals/imvk_Packing_shader.h packing)
# general
#compile_shader(shader/internals/AlphaBlending_shader.h AlphaBlending_data)
#compile_shader(shader/internals/AlphaBlending_shader.h AlphaBlending_alpha_data)
#compile_shader(shader/internals/Binary_shader.h Filter_data)
#compile_shader(shader/internals/ColorConvert_shader.h YUV2RGB_data)
#compile_shader(shader/internals/ColorConvert_shader.h Y_U_V2RGB_data)
#compile_shader(shader/internals/ColorConvert_shader.h RGB2YUV_data)
#compile_shader(shader/internals/ColorConvert_shader.h GRAY2RGB_data)
#compile_shader(shader/internals/ColorConvert_shader.h Conv_data)
#compile_shader(shader/internals/ColorConvert_shader.h RGB2LAB_data)
#compile_shader(shader/internals/ColorConvert_shader.h LAB2RGB_data)
#compile_shader(shader/internals/ColorConvert_shader.h RGB2HSL_data)
#compile_shader(shader/internals/ColorConvert_shader.h HSL2RGB_data)
#compile_shader(shader/internals/ColorConvert_shader.h RGB2HSV_data)
#compile_shader(shader/internals/ColorConvert_shader.h HSV2RGB_data)
#compile_shader(shader/internals/Concat_shader.h Shader_data)
#compile_shader(shader/internals/Copy_make_border_shader.h Filter_data)
#compile_shader(shader/internals/CopyTo_shader.h CopyTo_data)
#compile_shader(shader/internals/Crop_shader.h CropShader_data)
#compile_shader(shader/internals/Crop_shader.h CropToShader_data)
#compile_shader(shader/internals/Expand_shader.h ExpandShader_data)
#compile_shader(shader/internals/Flip_shader.h Shader_data)
#compile_shader(shader/internals/OpacityFilter_shader.h MASK_OPACITY_SHADER)
#compile_shader(shader/internals/OpacityFilter_shader.h MASK_OPACITY_INPLACE_SHADER)
#compile_shader(shader/internals/Resize_shader.h Resize_data)
#compile_shader(shader/internals/Substract_mean_normalize_shader.h Filter_data)
#compile_shader(shader/internals/Transpose_shader.h Filter_data)
#compile_shader(shader/internals/warpAffine_shader.h Filter_data)
#compile_shader(shader/internals/warpPerspective_shader.h Filter_data)
#compile_shader(shader/internals/SplitMerge_shader.h Split_data)
#compile_shader(shader/internals/SplitMerge_shader.h Merge_data)
#compile_shader(shader/internals/Blend_shader.h Blend_data)
#compile_shader(shader/internals/Blend_shader.h BlendMaskMerge_data)
# filters
#compile_shader(shader/filters/ALM_shader.h ALM_data)
#compile_shader(shader/filters/BarrelDistortion_shader.h Effect_data)
#compile_shader(shader/filters/Bilateral_shader.h Filter_data)
#compile_shader(shader/filters/Brightness_shader.h Filter_data)
#compile_shader(shader/filters/Canny_shader.h DSobelFilter_data)
#compile_shader(shader/filters/Canny_shader.h NMSFilter_data)
#compile_shader(shader/filters/Canny_shader.h CannyFilter_data)
#compile_shader(shader/filters/CAS_shader.h CAS_data)
#compile_shader(shader/filters/ChromaKey_shader.h Filter_data)
#compile_shader(shader/filters/ChromaKey_shader.h Blur_data)
#compile_shader(shader/filters/ChromaKey_shader.h Sharpen_data)
#compile_shader(shader/filters/ChromaKey_shader.h Despill_data)
#compile_shader(shader/filters/ColorBalance_shader.h Filter_data)
#compile_shader(shader/filters/ColorCurve_shader.h Filter_data)
#compile_shader(shader/filters/ColorInvert_shader.h Filter_data)
#compile_shader(shader/filters/Contrast_shader.h Filter_data)
#compile_shader(shader/filters/DeBand_shader.h DeBand_data)
#compile_shader(shader/filters/DeInterlace_shader.h DeInterlace_data)
#compile_shader(shader/filters/Dilation_shader.h Filter_data)
#compile_shader(shader/filters/Distortion_shader.h Effect_data)
#compile_shader(shader/filters/Emboss_shader.h Filter_data)
#compile_shader(shader/filters/Erosion_shader.h Filter_data)
#compile_shader(shader/filters/Exposure_shader.h Filter_data)
#compile_shader(shader/filters/Filter2D_shader.h Filter_data)
#compile_shader(shader/filters/Filter2DS_shader.h FilterColumn_data)
#compile_shader(shader/filters/Filter2DS_shader.h FilterRow_data)
#compile_shader(shader/filters/Filter2DS_shader.h FilterColumnMono_data)
#compile_shader(shader/filters/Filter2DS_shader.h FilterRowMono_data)
#compile_shader(shader/filters/Gamma_shader.h Filter_data)
#compile_shader(shader/filters/Glass_shader.h Effect_data)
#compile_shader(shader/filters/Guided_shader.h ToMatting_data)
#compile_shader(shader/filters/Guided_shader.h Guided_data)
#compile_shader(shader/filters/Guided_shader.h Matting_data)
#compile_shader(shader/filters/Hatch_shader.h Filter_data)
#compile_shader(shader/filters/HQDN3D_shader.h HQDN3D_data)
#compile_shader(shader/filters/Hue_shader.h Filter_data)
#compile_shader(shader/filters/Kuwahara_shader.h Effect_data)
#compile_shader(shader/filters/Lut3D_shader.h LUT3D_data)
#compile_shader(shader/filters/PixeLate_shader.h Effect_data)
#compile_shader(shader/filters/RadicalBlur_shader.h Effect_data)
#compile_shader(shader/filters/Saturation_shader.h Filter_data)
#compile_shader(shader/filters/Sketch_shader.h Filter_data)
#compile_shader(shader/filters/SmartDenoise_shader.h Filter_data)
#compile_shader(shader/filters/SmudgeBlur_shader.h Effect_data)
#compile_shader(shader/filters/Sobel_shader.h Filter_data)
#compile_shader(shader/filters/USM_shader.h USMFilter_data)
#compile_shader(shader/filters/Vibrance_shader.h Filter_data)
#compile_shader(shader/filters/WhiteBalance_shader.h Filter_data)
# scopes
#compile_shader(shader/scopes/CIE_shader.h CIE_data)
#compile_shader(shader/scopes/CIE_shader.h CIE_merge_data)
#compile_shader(shader/scopes/CIE_shader.h CIE_set_data)
#compile_shader(shader/scopes/Harris_shader.h PrewittFilter_data)
#compile_shader(shader/scopes/Harris_shader.h HarrisFilter_data)
#compile_shader(shader/scopes/Harris_shader.h NMSFilter_data)
#compile_shader(shader/scopes/Histogram_shader.h Histogram_data)
#compile_shader(shader/scopes/Histogram_shader.h Zero_data)
#compile_shader(shader/scopes/Histogram_shader.h ConvInt2Float_data)
#compile_shader(shader/scopes/Vector_shader.h Vector_data)
#compile_shader(shader/scopes/Vector_shader.h Zero_data)
#compile_shader(shader/scopes/Vector_shader.h Vector_merge_data)
#compile_shader(shader/scopes/Waveform_shader.h Waveform_data)
#compile_shader(shader/scopes/Waveform_shader.h Zero_data)
#compile_shader(shader/scopes/Waveform_shader.h ConvInt2Mat_data)
# effects
#compile_shader(shader/effects/Jitter_shader.h Effect_data)
#compile_shader(shader/effects/Lighting_shader.h Effect_data)
#compile_shader(shader/effects/Soul_shader.h Effect_data)
#compile_shader(shader/effects/Star_shader.h Effect_data)
#compile_shader(shader/effects/Sway_shader.h Effect_data)
#compile_shader(shader/effects/WaterRipple_shader.h Effect_data)
# fisheye
#compile_shader(shader/fisheye/equidistance2orthographic_shader.h Equidistance2Orthographic_data)
#compile_shader(shader/fisheye/fish2panorama_shader.h Fish2Panorama_data)
#compile_shader(shader/fisheye/fish2sphere_shader.h Fish2Sphere_data)
#compile_shader(shader/fisheye/orthographic2equidistance_shader.h Orthographic2Equidistance_data)
# transitions
#compile_shader(shader/transitions/BookFlip_shader.h BookFlip_data)
#compile_shader(shader/transitions/Bounce_shader.h Bounce_data)
#compile_shader(shader/transitions/BowTie_shader.h BowTie_data)
#compile_shader(shader/transitions/Burn_shader.h Burn_data)
#compile_shader(shader/transitions/BurnOut_shader.h BurnOut_data)
#compile_shader(shader/transitions/ButterflyWave_shader.h ButterflyWave_data)
#compile_shader(shader/transitions/CannabisLeaf_shader.h CannabisLeaf_data)
#compile_shader(shader/transitions/CircleBlur_shader.h CircleBlur_data)
#compile_shader(shader/transitions/CircleCrop_shader.h CircleCrop_data)
#compile_shader(shader/transitions/ColorPhase_shader.h ColorPhase_data)
#compile_shader(shader/transitions/ColourDistance_shader.h ColourDistance_data)
#compile_shader(shader/transitions/CrazyParametric_shader.h CrazyParametric_data)
#compile_shader(shader/transitions/Crosshatch_shader.h Crosshatch_data)
#compile_shader(shader/transitions/CrossWarp_shader.h CrossWarp_data)
#compile_shader(shader/transitions/CrossZoom_shader.h CrossZoom_data)
#compile_shader(shader/transitions/Cube_shader.h Cube_data)
#compile_shader(shader/transitions/DirectionalScaled_shader.h DirectionalScaled_data)
#compile_shader(shader/transitions/DirectionalWarp_shader.h DirectionalWarp_data)
#compile_shader(shader/transitions/Dissolve_shader.h Dissolve_data)
#compile_shader(shader/transitions/DoomScreen_shader.h DoomScreen_data)
#compile_shader(shader/transitions/Door_shader.h Door_data)
#compile_shader(shader/transitions/Doorway_shader.h Doorway_data)
#compile_shader(shader/transitions/Dreamy_shader.h Dreamy_data)
#compile_shader(shader/transitions/DreamyZoom_shader.h DreamyZoom_data)
#compile_shader(shader/transitions/Edge_shader.h Edge_data)
#compile_shader(shader/transitions/Fade_shader.h Fade_data)
#compile_shader(shader/transitions/Flyeye_shader.h Flyeye_data)
#compile_shader(shader/transitions/GlitchDisplace_shader.h GlitchDisplace_data)
#compile_shader(shader/transitions/GlitchMemories_shader.h GlitchMemories_data)
#compile_shader(shader/transitions/GridFlip_shader.h GridFlip_data)
#compile_shader(shader/transitions/Heart_shader.h Heart_data)
#compile_shader(shader/transitions/Hexagonalize_shader.h Hexagonalize_data)
#compile_shader(shader/transitions/KaleidoScope_shader.h KaleidoScope_data)
#compile_shader(shader/transitions/LinearBlur_shader.h LinearBlur_data)
#compile_shader(shader/transitions/Luma_shader.h Luma_data)
#compile_shader(shader/transitions/LuminanceMelt_shader.h LuminanceMelt_data)
#compile_shader(shader/transitions/Morph_shader.h Morph_data)
#compile_shader(shader/transitions/Mosaic_shader.h Mosaic_data)
#compile_shader(shader/transitions/Move_shader.h Move_data)
#compile_shader(shader/transitions/MultiplyBlend_shader.h MultiplyBlend_data)
#compile_shader(shader/transitions/PageCurl_shader.h PageCurl_data)
#compile_shader(shader/transitions/Perlin_shader.h Perlin_data)
#compile_shader(shader/transitions/Pinwheel_shader.h Pinwheel_data)
#compile_shader(shader/transitions/Pixelize_shader.h Pixelize_data)
#compile_shader(shader/transitions/Polar_shader.h Polar_data)
#compile_shader(shader/transitions/PolkaDots_shader.h PolkaDots_data)
#compile_shader(shader/transitions/Radial_shader.h Radial_data)
#compile_shader(shader/transitions/RandomSquares_shader.h RandomSquares_data)
#compile_shader(shader/transitions/Rectangle_shader.h Rectangle_data)
#compile_shader(shader/transitions/Ripple_shader.h Ripple_data)
#compile_shader(shader/transitions/Rolls_shader.h Rolls_data)
#compile_shader(shader/transitions/RotateScale_shader.h RotateScale_data)
#compile_shader(shader/transitions/RotateScaleVanish_shader.h RotateScaleVanish_data)
#compile_shader(shader/transitions/SimpleZoom_shader.h SimpleZoom_data)
#compile_shader(shader/transitions/SimpleZoomOut_shader.h SimpleZoomOut_data)
#compile_shader(shader/transitions/Slider_shader.h Slider_data)
#compile_shader(shader/transitions/SquaresWire_shader.h SquaresWire_data)
#compile_shader(shader/transitions/Squeeze_shader.h Squeeze_data)
#compile_shader(shader/transitions/StaticWipe_shader.h StaticWipe_data)
#compile_shader(shader/transitions/StereoViewer_shader.h StereoViewer_data)
#compile_shader(shader/transitions/Swap_shader.h Swap_data)
#compile_shader(shader/transitions/Swirl_shader.h Swirl_data)
#compile_shader(shader/transitions/WaterDrop_shader.h WaterDrop_data)
#compile_shader(shader/transitions/Wind_shader.h Wind_data)
#compile_shader(shader/transitions/WindowBlinds_shader.h WindowBlinds_data)
#compile_shader(shader/transitions/WindowSlice_shader.h WindowSlice_data)
#compile_shader(shader/transitions/Wipe_shader.h Wipe_data)
#compile_shader(shader/transitions/ZoomInCircles_shader.h ZoomInCircles_data)
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
    ImVulkanShader.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/imvk_platform.h
    include/imvk_vulkan_header_fix.h
    include/imvk_option.h
    include/imvk_allocator.h
    include/imvk_gpu.h
    include/imvk_command.h
    include/imvk_mat.h
    include/imvk_shader.h
    include/imvk_image_mat.h
    include/imvk_pipeline.h
    include/ImVulkanShader.h
    imvk_pipelinecache.h
)

# add filter
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    shader/filters/Filter2D_vulkan.cpp
    shader/filters/Filter2DS_vulkan.cpp
    shader/filters/Brightness_vulkan.cpp
    shader/filters/Contrast_vulkan.cpp
    shader/filters/Exposure_vulkan.cpp
    shader/filters/Gamma_vulkan.cpp
    shader/filters/Lut3D_vulkan.cpp
    shader/filters/Saturation_vulkan.cpp
    shader/filters/Hue_vulkan.cpp
    shader/filters/ColorInvert_vulkan.cpp
    shader/filters/Bilateral_vulkan.cpp
    shader/filters/Box_vulkan.cpp
    shader/filters/Gaussian_vulkan.cpp
    shader/filters/Dilation_vulkan.cpp
    shader/filters/Erosion_vulkan.cpp
    shader/filters/USM_vulkan.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    shader/filters/Filter2D_shader.h
    shader/filters/Filter2DS_shader.h
    shader/filters/Brightness_shader.h
    shader/filters/Contrast_shader.h
    shader/filters/Exposure_shader.h
    shader/filters/Gamma_shader.h
    shader/filters/Lut3D_shader.h
    shader/filters/Saturation_shader.h
    shader/filters/Hue_shader.h
    shader/filters/ColorInvert_shader.h
    shader/filters/Bilateral_shader.h
    shader/filters/Dilation_shader.h
    shader/filters/Erosion_shader.h
    shader/filters/USM_shader.h
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/shader/filters/Filter2D_vulkan.h
    include/shader/filters/Filter2DS_vulkan.h
    include/shader/filters/Brightness_vulkan.h
    include/shader/filters/Contrast_vulkan.h
    include/shader/filters/Exposure_vulkan.h
    include/shader/filters/Gamma_vulkan.h
    include/shader/filters/Lut3D_vulkan.h
    include/shader/filters/Saturation_vulkan.h
    include/shader/filters/Hue_vulkan.h
    include/shader/filters/ColorInvert_vulkan.h
    include/shader/filters/Bilateral_vulkan.h
    include/shader/filters/Box_vulkan.h
    include/shader/filters/Gaussian_vulkan.h
    include/shader/filters/Dilation_vulkan.h
    include/shader/filters/Erosion_vulkan.h
    include/shader/filters/USM_vulkan.h
)

# add plugins filters
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    shader/filters/ALM_vulkan.cpp
    shader/filters/BarrelDistortion_vulkan.cpp
    shader/filters/CAS_vulkan.cpp
    shader/filters/Canny_vulkan.cpp
    shader/filters/ChromaKey_vulkan.cpp
    shader/filters/ColorBalance_vulkan.cpp
    shader/filters/ColorCurve_vulkan.cpp
    shader/filters/DeBand_vulkan.cpp
    shader/filters/DeInterlace_vulkan.cpp
    shader/filters/Distortion_vulkan.cpp
    shader/filters/Emboss_vulkan.cpp
    shader/filters/Glass_vulkan.cpp
    shader/filters/Guided_vulkan.cpp
    shader/filters/Hatch_vulkan.cpp
    shader/filters/HQDN3D_vulkan.cpp
    shader/filters/Kuwahara_vulkan.cpp
    shader/filters/Laplacian_vulkan.cpp
    shader/filters/PixeLate_vulkan.cpp
    shader/filters/RadicalBlur_vulkan.cpp
    shader/filters/Sketch_vulkan.cpp
    shader/filters/SmartDenoise_vulkan.cpp
    shader/filters/SmudgeBlur_vulkan.cpp
    shader/filters/Sobel_vulkan.cpp
    shader/filters/Vibrance_vulkan.cpp
    shader/filters/WhiteBalance_vulkan.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    shader/filters/ALM_shader.h
    shader/filters/BarrelDistortion_shader.h
    shader/filters/CAS_shader.h
    shader/filters/Canny_shader.h
    shader/filters/ChromaKey_shader.h
    shader/filters/ColorBalance_shader.h
    shader/filters/ColorCurve_shader.h
    shader/filters/DeBand_shader.h
    shader/filters/DeInterlace_shader.h
    shader/filters/Distortion_shader.h
    shader/filters/Emboss_shader.h
    shader/filters/Glass_shader.h
    shader/filters/Guided_shader.h
    shader/filters/Hatch_shader.h
    shader/filters/HQDN3D_shader.h
    shader/filters/Kuwahara_shader.h
    shader/filters/PixeLate_shader.h
    shader/filters/RadicalBlur_shader.h
    shader/filters/Sketch_shader.h
    shader/filters/SmartDenoise_shader.h
    shader/filters/SmudgeBlur_shader.h
    shader/filters/Sobel_shader.h
    shader/filters/Vibrance_shader.h
    shader/filters/WhiteBalance_shader.h
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/shader/filters/ALM_vulkan.h
    include/shader/filters/BarrelDistortion_vulkan.h
    include/shader/filters/CAS_vulkan.h
    include/shader/filters/Canny_vulkan.h
    include/shader/filters/ChromaKey_vulkan.h
    include/shader/filters/ColorBalance_vulkan.h
    include/shader/filters/ColorCurve_vulkan.h
    include/shader/filters/DeBand_vulkan.h
    include/shader/filters/DeInterlace_vulkan.h
    include/shader/filters/Distortion_vulkan.h
    include/shader/filters/Emboss_vulkan.h
    include/shader/filters/Glass_vulkan.h
    include/shader/filters/Guided_vulkan.h
    include/shader/filters/Hatch_vulkan.h
    include/shader/filters/HQDN3D_vulkan.h
    include/shader/filters/Kuwahara_vulkan.h
    include/shader/filters/Laplacian_vulkan.h
    include/shader/filters/PixeLate_vulkan.h
    include/shader/filters/RadicalBlur_vulkan.h
    include/shader/filters/Sketch_vulkan.h
    include/shader/filters/SmartDenoise_vulkan.h
    include/shader/filters/SmudgeBlur_vulkan.h
    include/shader/filters/Sobel_vulkan.h
    include/shader/filters/Vibrance_vulkan.h
    include/shader/filters/WhiteBalance_vulkan.h
)
set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shader/filters
)

# add effects
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    shader/effects/Jitter_vulkan.cpp
    shader/effects/Lighting_vulkan.cpp
    shader/effects/Soul_vulkan.cpp
    shader/effects/Star_vulkan.cpp
    shader/effects/Sway_vulkan.cpp
    shader/effects/WaterRipple_vulkan.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    shader/effects/Jitter_shader.h
    shader/effects/Lighting_shader.h
    shader/effects/Soul_shader.h
    shader/effects/Star_shader.h
    shader/effects/Sway_shader.h
    shader/effects/WaterRipple_shader.h
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/shader/effects/Jitter_vulkan.h
    include/shader/effects/Lighting_vulkan.h
    include/shader/effects/Soul_vulkan.h
    include/shader/effects/Star_vulkan.h
    include/shader/effects/Sway_vulkan.h
    include/shader/effects/WaterRipple_vulkan.h
)
set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shader/effects
)

# add scopes
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    shader/scopes/Harris_vulkan.cpp
    shader/scopes/CIE_vulkan.cpp
    shader/scopes/Histogram_vulkan.cpp
    shader/scopes/Waveform_vulkan.cpp
    shader/scopes/Vector_vulkan.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    shader/scopes/Harris_shader.h
    shader/scopes/CIE_shader.h
    shader/scopes/Histogram_shader.h
    shader/scopes/Waveform_shader.h
    shader/scopes/Vector_shader.h
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/shader/scopes/Harris_vulkan.h
    include/shader/scopes/CIE_vulkan.h
    include/shader/scopes/Histogram_vulkan.h
    include/shader/scopes/Waveform_vulkan.h
    include/shader/scopes/Vector_vulkan.h
)
set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shader/scopes
)

# add fisheye
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    shader/fisheye/equidistance2orthographic_vulkan.cpp
    shader/fisheye/orthographic2equidistance_vulkan.cpp
    shader/fisheye/fish2panorama_vulkan.cpp
    shader/fisheye/fish2sphere_vulkan.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    shader/fisheye/equidistance2orthographic_shader.h
    shader/fisheye/orthographic2equidistance_shader.h
    shader/fisheye/fish2panorama_shader.h
    shader/fisheye/fish2sphere_shader.h
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/shader/fisheye/equidistance2orthographic_vulkan.h
    include/shader/fisheye/orthographic2equidistance_vulkan.h
    include/shader/fisheye/fish2panorama_vulkan.h
    include/shader/fisheye/fish2sphere_vulkan.h
)
set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shader/fisheye
)

# add transitions
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    shader/transitions/BookFlip_vulkan.cpp
    shader/transitions/LinearBlur_vulkan.cpp
    shader/transitions/Bounce_vulkan.cpp
    shader/transitions/BowTie_vulkan.cpp
    shader/transitions/Burn_vulkan.cpp
    shader/transitions/BurnOut_vulkan.cpp
    shader/transitions/ButterflyWave_vulkan.cpp
    shader/transitions/CannabisLeaf_vulkan.cpp
    shader/transitions/CircleBlur_vulkan.cpp
    shader/transitions/CircleCrop_vulkan.cpp
    shader/transitions/ColorPhase_vulkan.cpp
    shader/transitions/ColourDistance_vulkan.cpp
    shader/transitions/CrazyParametric_vulkan.cpp
    shader/transitions/Crosshatch_vulkan.cpp
    shader/transitions/CrossWarp_vulkan.cpp
    shader/transitions/CrossZoom_vulkan.cpp
    shader/transitions/Cube_vulkan.cpp
    shader/transitions/DirectionalScaled_vulkan.cpp
    shader/transitions/DirectionalWarp_vulkan.cpp
    shader/transitions/Dissolve_vulkan.cpp
    shader/transitions/DoomScreen_vulkan.cpp
    shader/transitions/Door_vulkan.cpp
    shader/transitions/Doorway_vulkan.cpp
    shader/transitions/Dreamy_vulkan.cpp
    shader/transitions/DreamyZoom_vulkan.cpp
    shader/transitions/Edge_vulkan.cpp
    shader/transitions/Fade_vulkan.cpp
    shader/transitions/Flyeye_vulkan.cpp
    shader/transitions/GlitchDisplace_vulkan.cpp
    shader/transitions/GlitchMemories_vulkan.cpp
    shader/transitions/GridFlip_vulkan.cpp
    shader/transitions/Heart_vulkan.cpp
    shader/transitions/Hexagonalize_vulkan.cpp
    shader/transitions/KaleidoScope_vulkan.cpp
    shader/transitions/Luma_vulkan.cpp
    shader/transitions/LuminanceMelt_vulkan.cpp
    shader/transitions/Morph_vulkan.cpp
    shader/transitions/Mosaic_vulkan.cpp
    shader/transitions/Move_vulkan.cpp
    shader/transitions/MultiplyBlend_vulkan.cpp
    shader/transitions/PageCurl_vulkan.cpp
    shader/transitions/Perlin_vulkan.cpp
    shader/transitions/Pinwheel_vulkan.cpp
    shader/transitions/Pixelize_vulkan.cpp
    shader/transitions/Polar_vulkan.cpp
    shader/transitions/PolkaDots_vulkan.cpp
    shader/transitions/Radial_vulkan.cpp
    shader/transitions/RandomSquares_vulkan.cpp
    shader/transitions/Rectangle_vulkan.cpp
    shader/transitions/Ripple_vulkan.cpp
    shader/transitions/Rolls_vulkan.cpp
    shader/transitions/RotateScale_vulkan.cpp
    shader/transitions/RotateScaleVanish_vulkan.cpp
    shader/transitions/SimpleZoom_vulkan.cpp
    shader/transitions/SimpleZoomOut_vulkan.cpp
    shader/transitions/Slider_vulkan.cpp
    shader/transitions/SquaresWire_vulkan.cpp
    shader/transitions/Squeeze_vulkan.cpp
    shader/transitions/StaticWipe_vulkan.cpp
    shader/transitions/StereoViewer_vulkan.cpp
    shader/transitions/Swap_vulkan.cpp
    shader/transitions/Swirl_vulkan.cpp
    shader/transitions/WaterDrop_vulkan.cpp
    shader/transitions/Wind_vulkan.cpp
    shader/transitions/WindowBlinds_vulkan.cpp
    shader/transitions/WindowSlice_vulkan.cpp
    shader/transitions/Wipe_vulkan.cpp
    shader/transitions/ZoomInCircles_vulkan.cpp
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    shader/transitions/BookFlip_shader.h
    shader/transitions/LinearBlur_shader.h
    shader/transitions/Bounce_shader.h
    shader/transitions/BowTie_shader.h
    shader/transitions/Burn_shader.h
    shader/transitions/BurnOut_shader.h
    shader/transitions/ButterflyWave_shader.h
    shader/transitions/CannabisLeaf_shader.h
    shader/transitions/CircleBlur_shader.h
    shader/transitions/CircleCrop_shader.h
    shader/transitions/ColorPhase_shader.h
    shader/transitions/ColourDistance_shader.h
    shader/transitions/CrazyParametric_shader.h
    shader/transitions/Crosshatch_shader.h
    shader/transitions/CrossWarp_shader.h
    shader/transitions/CrossZoom_shader.h
    shader/transitions/Cube_shader.h
    shader/transitions/DirectionalScaled_shader.h
    shader/transitions/DirectionalWarp_shader.h
    shader/transitions/Dissolve_shader.h
    shader/transitions/DoomScreen_shader.h
    shader/transitions/Door_shader.h
    shader/transitions/Doorway_shader.h
    shader/transitions/Dreamy_shader.h
    shader/transitions/DreamyZoom_shader.h
    shader/transitions/Edge_shader.h
    shader/transitions/Fade_shader.h
    shader/transitions/Flyeye_shader.h
    shader/transitions/GlitchDisplace_shader.h
    shader/transitions/GlitchMemories_shader.h
    shader/transitions/GridFlip_shader.h
    shader/transitions/Heart_shader.h
    shader/transitions/Hexagonalize_shader.h
    shader/transitions/KaleidoScope_shader.h
    shader/transitions/Luma_shader.h
    shader/transitions/LuminanceMelt_shader.h
    shader/transitions/Morph_shader.h
    shader/transitions/Mosaic_shader.h
    shader/transitions/Move_shader.h
    shader/transitions/MultiplyBlend_shader.h
    shader/transitions/PageCurl_shader.h
    shader/transitions/Perlin_shader.h
    shader/transitions/Pinwheel_shader.h
    shader/transitions/Pixelize_shader.h
    shader/transitions/Polar_shader.h
    shader/transitions/PolkaDots_shader.h
    shader/transitions/Radial_shader.h
    shader/transitions/RandomSquares_shader.h
    shader/transitions/Rectangle_shader.h
    shader/transitions/Ripple_shader.h
    shader/transitions/Rolls_shader.h
    shader/transitions/RotateScale_shader.h
    shader/transitions/RotateScaleVanish_shader.h
    shader/transitions/SimpleZoom_shader.h
    shader/transitions/SimpleZoomOut_shader.h
    shader/transitions/Slider_shader.h
    shader/transitions/SquaresWire_shader.h
    shader/transitions/Squeeze_shader.h
    shader/transitions/StaticWipe_shader.h
    shader/transitions/StereoViewer_shader.h
    shader/transitions/Swap_shader.h
    shader/transitions/Swirl_shader.h
    shader/transitions/WaterDrop_shader.h
    shader/transitions/Wind_shader.h
    shader/transitions/WindowBlinds_shader.h
    shader/transitions/WindowSlice_shader.h
    shader/transitions/Wipe_shader.h
    shader/transitions/ZoomInCircles_shader.h
)
set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/shader/transitions/BookFlip_vulkan.h
    include/shader/transitions/LinearBlur_vulkan.h
    include/shader/transitions/Bounce_vulkan.h
    include/shader/transitions/BowTie_vulkan.h
    include/shader/transitions/Burn_vulkan.h
    include/shader/transitions/BurnOut_vulkan.h
    include/shader/transitions/ButterflyWave_vulkan.h
    include/shader/transitions/CannabisLeaf_vulkan.h
    include/shader/transitions/CircleBlur_vulkan.h
    include/shader/transitions/CircleCrop_vulkan.h
    include/shader/transitions/ColorPhase_vulkan.h
    include/shader/transitions/ColourDistance_vulkan.h
    include/shader/transitions/CrazyParametric_vulkan.h
    include/shader/transitions/Crosshatch_vulkan.h
    include/shader/transitions/CrossWarp_vulkan.h
    include/shader/transitions/CrossZoom_vulkan.h
    include/shader/transitions/Cube_vulkan.h
    include/shader/transitions/DirectionalScaled_vulkan.h
    include/shader/transitions/DirectionalWarp_vulkan.h
    include/shader/transitions/Dissolve_vulkan.h
    include/shader/transitions/DoomScreen_vulkan.h
    include/shader/transitions/Door_vulkan.h
    include/shader/transitions/Doorway_vulkan.h
    include/shader/transitions/Dreamy_vulkan.h
    include/shader/transitions/DreamyZoom_vulkan.h
    include/shader/transitions/Edge_vulkan.h
    include/shader/transitions/Fade_vulkan.h
    include/shader/transitions/Flyeye_vulkan.h
    include/shader/transitions/GlitchDisplace_vulkan.h
    include/shader/transitions/GlitchMemories_vulkan.h
    include/shader/transitions/GridFlip_vulkan.h
    include/shader/transitions/Heart_vulkan.h
    include/shader/transitions/Hexagonalize_vulkan.h
    include/shader/transitions/KaleidoScope_vulkan.h
    include/shader/transitions/Luma_vulkan.h
    include/shader/transitions/LuminanceMelt_vulkan.h
    include/shader/transitions/Morph_vulkan.h
    include/shader/transitions/Mosaic_vulkan.h
    include/shader/transitions/Move_vulkan.h
    include/shader/transitions/MultiplyBlend_vulkan.h
    include/shader/transitions/PageCurl_vulkan.h
    include/shader/transitions/Perlin_vulkan.h
    include/shader/transitions/Pinwheel_vulkan.h
    include/shader/transitions/Pixelize_vulkan.h
    include/shader/transitions/Polar_vulkan.h
    include/shader/transitions/PolkaDots_vulkan.h
    include/shader/transitions/Radial_vulkan.h
    include/shader/transitions/RandomSquares_vulkan.h
    include/shader/transitions/Rectangle_vulkan.h
    include/shader/transitions/Ripple_vulkan.h
    include/shader/transitions/Rolls_vulkan.h
    include/shader/transitions/RotateScale_vulkan.h
    include/shader/transitions/RotateScaleVanish_vulkan.h
    include/shader/transitions/SimpleZoom_vulkan.h
    include/shader/transitions/SimpleZoomOut_vulkan.h
    include/shader/transitions/Slider_vulkan.h
    include/shader/transitions/SquaresWire_vulkan.h
    include/shader/transitions/Squeeze_vulkan.h
    include/shader/transitions/StaticWipe_vulkan.h
    include/shader/transitions/StereoViewer_vulkan.h
    include/shader/transitions/Swap_vulkan.h
    include/shader/transitions/Swirl_vulkan.h
    include/shader/transitions/WaterDrop_vulkan.h
    include/shader/transitions/Wind_vulkan.h
    include/shader/transitions/WindowBlinds_vulkan.h
    include/shader/transitions/WindowSlice_vulkan.h
    include/shader/transitions/Wipe_vulkan.h
    include/shader/transitions/ZoomInCircles_vulkan.h
)
set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shader/transitions
)
# add internals
set(VKSHADER_SRCS
    ${VKSHADER_SRCS}
    shader/internals/AlphaBlending_vulkan.cpp
    shader/internals/colorspace_table.cpp
    shader/internals/ColorConvert_vulkan.cpp
    shader/internals/Resize_vulkan.cpp
    shader/internals/CopyTo_vulkan.cpp
    shader/internals/Flip_vulkan.cpp
    shader/internals/Crop_vulkan.cpp
    shader/internals/Transpose_vulkan.cpp
    shader/internals/Concat_vulkan.cpp
    shader/internals/warpPerspective_vulkan.cpp
    shader/internals/warpAffine_vulkan.cpp
    shader/internals/Substract_mean_normalize.cpp
    shader/internals/Copy_make_border.cpp
    shader/internals/Binary_vulkan.cpp
    shader/internals/OpacityFilter_vulkan.cpp
    shader/internals/Expand_vulkan.cpp
    shader/internals/imvk_Cast_vulkan.cpp
    shader/internals/imvk_Packing_vulkan.cpp
    shader/internals/SplitMerge_vulkan.cpp
    shader/internals/Blend_vulkan.cpp
    shader/internals/Feather_vulkan.cpp
)

set(VKSHADER_INCS
    ${VKSHADER_INCS}
    shader/internals/AlphaBlending_shader.h
    shader/internals/ColorConvert_shader.h
    shader/internals/Resize_shader.h
    shader/internals/CopyTo_shader.h
    shader/internals/Flip_shader.h
    shader/internals/Crop_shader.h
    shader/internals/Transpose_shader.h
    shader/internals/Concat_shader.h
    shader/internals/warpPerspective_shader.h
    shader/internals/warpAffine_shader.h
    shader/internals/Substract_mean_normalize_shader.h
    shader/internals/Copy_make_border_shader.h
    shader/internals/Binary_shader.h
    shader/internals/Expand_shader.h
    shader/internals/imvk_Cast_shader.h
    shader/internals/imvk_Packing_shader.h
    shader/internals/SplitMerge_shader.h
    shader/internals/Blend_shader.h
)

set(VKSHADER_INCS
    ${VKSHADER_INCS}
    include/shader/internals/AlphaBlending_vulkan.h
    include/shader/internals/ColorConvert_vulkan.h
    include/shader/internals/Resize_vulkan.h
    include/shader/internals/CopyTo_vulkan.h
    include/shader/internals/Flip_vulkan.h
    include/shader/internals/Crop_vulkan.h
    include/shader/internals/Transpose_vulkan.h
    include/shader/internals/Concat_vulkan.h
    include/shader/internals/warpPerspective_vulkan.h
    include/shader/internals/warpAffine_vulkan.h
    include/shader/internals/Substract_mean_normalize.h
    include/shader/internals/Copy_make_border.h
    include/shader/internals/Binary_vulkan.h
    include/shader/internals/Expand_vulkan.h
    include/shader/internals/imvk_Cast_vulkan.h
    include/shader/internals/imvk_Packing_vulkan.h
    include/shader/internals/SplitMerge_vulkan.h
    include/shader/internals/Blend_vulkan.h
    include/shader/internals/Feather_vulkan.h
)

set(VKSHADER_INC_DIRS
    ${VKSHADER_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shader
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shader/internals
)

include_directories(${VKSHADER_INC_DIRS})

if(VKSHADER_STATIC)
    set(LIBRARY STATIC)
    add_definitions(-DVKSHADER_STATIC_LIBRARY)
else(VKSHADER_STATIC)
    set(LIBRARY SHARED)
    add_definitions(-DVKSHADER_SHARED_LIBRARY)
endif(VKSHADER_STATIC)

include_directories("/usr/local/include")

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
set(VKSHADER_VERSION_MINOR 15)
set(VKSHADER_VERSION_PATCH 5)
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
target_link_libraries(VkShader PUBLIC ${GLSLANG_LIBRARY} ${LINK_LIBS} )

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
