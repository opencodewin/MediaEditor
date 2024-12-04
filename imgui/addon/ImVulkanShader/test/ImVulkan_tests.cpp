#include "ImVulkanShader.h"
#include <iostream>
#include <unistd.h>
#include <ALM_vulkan.h>
#include <Bilateral_vulkan.h>
#include <Box.h>
#include <Brightness_vulkan.h>
#include <Canny_vulkan.h>
#include <CAS_vulkan.h>
#include <ChromaKey_vulkan.h>
#include <ColorBalance_vulkan.h>
#include <ColorCurve_vulkan.h>

#include <AlphaBlending_vulkan.h>
#include <BookFlip_vulkan.h>
#include <Bounce_vulkan.h>
#include <BowTie_vulkan.h>

#include <CIE_vulkan.h>
#include <Harris_vulkan.h>
#include <Histogram_vulkan.h>
#include <Vector_vulkan.h>
#include <Waveform_vulkan.h>

#include <ColorConvert_vulkan.h>

#define TEST_WIDTH  1920
#define TEST_HEIGHT 1080
#define TEST_GPU    0
#define TEST_TIMES  10
static ImGui::ImMat mat1, mat2, mat3, dstmat;

static void test_filters(int gpu, int times)
{
    if (mat1.empty())
        return;
    std::cout << "Filters:" << std::endl;
    // test ALM
    ImGui::ALM_vulkan* alm = new ImGui::ALM_vulkan(gpu);
    if (alm)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = alm->filter(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (ALM)" << std::endl;
        delete alm;
    }

    // test Bilateral
    ImGui::Bilateral_vulkan* bilateral = new ImGui::Bilateral_vulkan(gpu);
    if (bilateral)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = bilateral->filter(mat1, dstmat, 5, 10.f, 10.f);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Bilateral)" << std::endl;
        delete bilateral;
    }

    // test Box
    ImGui::BoxBlur_vulkan* box = new ImGui::BoxBlur_vulkan(gpu);
    if (box)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = box->filter(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (BoxBlur)" << std::endl;
        delete box;
    }

    // test Brightness
    ImGui::Brightness_vulkan* bright = new ImGui::Brightness_vulkan(gpu);
    if (bright)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = bright->filter(mat1, dstmat, 0.5);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Brightness)" << std::endl;
        delete bright;
    }

    // test Canny
    ImGui::Canny_vulkan* canny = new ImGui::Canny_vulkan(gpu);
    if (canny)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = canny->filter(mat1, dstmat, 3, 0.1, 0.45);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Canny)" << std::endl;
        delete canny;
    }

    // test Cas
    ImGui::CAS_vulkan* cas = new ImGui::CAS_vulkan(gpu);
    if (cas)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = cas->filter(mat1, dstmat, 0.95);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (CAS)" << std::endl;
        delete cas;
    }

    // test ChromaKey
    ImGui::ChromaKey_vulkan* chromakey = new ImGui::ChromaKey_vulkan(gpu);
    if (chromakey)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = chromakey->filter(mat1, dstmat, 10.f, {0.0f, 1.0f, 0.0f, 1.0f}, 0.05f, 50.f, 1.0f, CHROMAKEY_OUTPUT_NORMAL);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (ChromaKey)" << std::endl;
        delete chromakey;
    }

    // test ColorBalance
    ImGui::ColorBalance_vulkan* color_balance = new ImGui::ColorBalance_vulkan(gpu);
    if (color_balance)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = color_balance->filter(mat1, dstmat, {0.5, 0.5, 0.5, 0.5}, {0.5, 0.5, 0.5, 0.5}, {0.5, 0.5, 0.5, 0.5}, true);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (ColorBalance)" << std::endl;
        delete color_balance;
    }

    // test ColorCurve
    ImGui::ColorCurve_vulkan* color_curve = new ImGui::ColorCurve_vulkan(gpu);
    if (color_curve)
    {
        ImGui::ImMat mat_curve;
        mat_curve.create_type(1024, 1, 4, IM_DT_FLOAT32);
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = color_curve->filter(mat1, dstmat, mat_curve);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (ColorCurve)" << std::endl;
        delete color_curve;
    }

    // TODO::Dicky add more filter test
}

static void test_transitions(int gpu, int times)
{
    if (mat1.empty() || mat2.empty())
        return;

    std::cout << "Transitions:" << std::endl;
    ImPixel color = ImPixel(0.0f, 0.0f, 0.0f, 0.6f);
    // test AlphaBlending
    ImGui::AlphaBlending_vulkan *alphablending = new ImGui::AlphaBlending_vulkan(gpu);
    if (alphablending)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = alphablending->blend(mat1, mat2, dstmat, 0.5f);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (AlphaBlending)" << std::endl;
        delete alphablending;
    }

    // test BookFlip
    ImGui::BookFlip_vulkan * boolflip = new ImGui::BookFlip_vulkan(gpu);
    if (boolflip)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = boolflip->transition(mat1, mat2, dstmat, 0.5f);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (BookFlip)" << std::endl;
        delete boolflip;
    }

    // test ounce
    ImGui::Bounce_vulkan * bounce = new ImGui::Bounce_vulkan(gpu);
    if (bounce)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = bounce->transition(mat1, mat2, dstmat, 0.5f, color, 0.075f, 3.f);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Bounce)" << std::endl;
        delete bounce;
    }

    // test BowTie
    ImGui::BowTieHorizontal_vulkan * bowtie = new ImGui::BowTieHorizontal_vulkan(gpu);
    if (bowtie)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = bowtie->transition(mat1, mat2, dstmat, 0.5f, 0);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (BowTie)" << std::endl;
        delete bowtie;
    }

    // TODO::Dicky add more transition test
}

static void test_scopes(int gpu, int times)
{
    if (mat1.empty())
        return;
    std::cout << "Scopes:" << std::endl;
    // test CIE
    ImGui::CIE_vulkan* cie = new ImGui::CIE_vulkan(gpu);
    if (cie)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = cie->scope(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (CIE)" << std::endl;
        delete cie;
    }
    
    // test Harris
    ImGui::Harris_vulkan* harris = new ImGui::Harris_vulkan(gpu);
    if (harris)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = harris->scope(mat1, dstmat, 3, 2.f, 0.1f, 0.04f, 5.f);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Harris)" << std::endl;
        delete harris;
    }

    // test Histogram
    ImGui::Histogram_vulkan* histogram = new ImGui::Histogram_vulkan(gpu);
    if (histogram)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = histogram->scope(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Histogram)" << std::endl;
        delete histogram;
    }

    // test Vector
    ImGui::Vector_vulkan* vector = new ImGui::Vector_vulkan(gpu);
    if (vector)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = vector->scope(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Vector)" << std::endl;
        delete vector;
    }

    // test Waveform
    ImGui::Waveform_vulkan* wave = new ImGui::Waveform_vulkan(gpu);
    if (wave)
    {
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = wave->scope(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Waveform)" << std::endl;
        delete wave;
    }
}

static void test_others(int gpu, int times)
{
    // color convert
    if (mat1.empty())
        return;
    std::cout << "Others:" << std::endl;

    ImGui::ColorConvert_vulkan* convert = new ImGui::ColorConvert_vulkan(gpu);
    if (convert)
    {
        mat1.color_format = IM_CF_NV12;
        dstmat.color_format = IM_CF_ABGR;
        double avg_time = 0, total_time = 0, max_time = -DBL_MAX, min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert NV12->RGBA)" << std::endl;
        
        mat1.color_format = IM_CF_YUV420;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert YUV420->RGBA)" << std::endl;

        mat1.color_format = IM_CF_YUV422;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert YUV422->RGBA)" << std::endl;

        mat1.color_format = IM_CF_YUV444;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert YUV444->RGBA)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_NV12;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert RGBA->NV12)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_YUV420;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert RGBA->YUV420)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_YUV422;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert RGBA->YUV422)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_YUV444;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->ConvertColorFormat(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (Convert RGBA->YUV444)" << std::endl;

        mat1.color_format = IM_CF_NV12;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA NV12)" << std::endl;

        mat1.color_format = IM_CF_YUV420;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA YUV420)" << std::endl;

        mat1.color_format = IM_CF_YUV422;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA YUV422)" << std::endl;

        mat1.color_format = IM_CF_YUV444;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA YUV444)" << std::endl;

        mat1.color_format = IM_CF_NV12;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, mat2, mat3, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA Planar NV12)" << std::endl;

        mat1.color_format = IM_CF_YUV420;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, mat2, mat3, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA Planar YUV420)" << std::endl;

        mat1.color_format = IM_CF_YUV422;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, mat2, mat3, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA Planar YUV422)" << std::endl;

        mat1.color_format = IM_CF_YUV444;
        dstmat.color_format = IM_CF_ABGR;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->YUV2RGBA(mat1, mat2, mat3, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (YUV2RGBA Planar YUV444)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_NV12;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->RGBA2YUV(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (RGBA2YUV NV12)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_YUV420;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->RGBA2YUV(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (RGBA2YUV YUV420)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_YUV422;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->RGBA2YUV(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (RGBA2YUV YUV422)" << std::endl;

        mat1.color_format = IM_CF_ABGR;
        dstmat.color_format = IM_CF_YUV444;
        avg_time = 0; total_time = 0; max_time = -DBL_MAX; min_time = DBL_MAX; 
        for (int i = 0; i < times; i++)
        {
            auto shader_time = convert->RGBA2YUV(mat1, dstmat);
            if (shader_time > max_time) max_time = shader_time;
            if (shader_time < min_time) min_time = shader_time;
            total_time += shader_time;
        }
        avg_time = total_time / times;
        std::cout << "min:" << std::to_string(min_time) << "\tmax:" << std::to_string(max_time) << "\tavg:" << std::to_string(avg_time) << " (RGBA2YUV YUV444)" << std::endl;

        delete convert;
    }
    // TODO::Dicky add others test
}

int main(int argc, char ** argv)
{
    int test_flags = argc > 1 ? 0 : 0xFFFFFFFF;
    int test_gpu = TEST_GPU;
    int test_loop = TEST_TIMES;
    int o = -1;
    const char *option_str = "aftsog:n:";
    while ((o = getopt(argc, argv, option_str)) != -1)
    {
        switch (o)
        {
            case 'a': test_flags = 0xFFFFFFFF; break;
            case 'f': test_flags |= 0x00000001; break;
            case 't': test_flags |= 0x00000002; break;
            case 's': test_flags |= 0x00000004; break;
            case 'o': test_flags |= 0x00000008; break;
            case 'g': test_gpu = atoi(optarg); break;
            case 'n': test_loop = atoi(optarg); break;
            default: break;
        }
    }

    ImGui::ImVulkanShaderInit();
    ImGui::VulkanDevice* vkdev = ImGui::get_gpu_device(test_gpu);
    std::string device_name = vkdev->info.device_name();

    mat1.create_type(TEST_WIDTH, TEST_HEIGHT, 4, IM_DT_INT8);
    mat1.randn(128.f, 128.f);
    
    // test ImMat to VkMat
    std::cout << "Test GPU:" << test_gpu << "\t(" << device_name << ")" << std::endl;
    std::cout << "Test Loop:" << test_loop << std::endl;
    std::cout << "Global:" << std::endl;
    ImGui::VkMat gmat;
    gmat.device_number = TEST_GPU;
    ImGui::ImVulkanImMatToVkMat(mat1, gmat);
    if (gmat.empty())
    {
        std::cout << "ImMat to VkMat failed!!!" << std::endl;
    }
    else
    {
        std::cout << "ImMat to VkMat passed" << std::endl;
        // test VkMat to ImMat
        ImGui::ImVulkanVkMatToImMat(gmat, mat2);
        if (mat2.empty())
            std::cout << "VkMat to ImMat failed!!!" << std::endl;
        else
            std::cout << "VkMat to ImMat passed" << std::endl;
        gmat.release();
    }

    if (mat2.empty())
        mat2.create_type(TEST_WIDTH, TEST_HEIGHT, 4, IM_DT_INT8);

    if (mat3.empty())
        mat3.create_type(TEST_WIDTH, TEST_HEIGHT, 4, IM_DT_INT8);
    
    if (test_flags & 0x00000001) test_filters(test_gpu, test_loop);
    if (test_flags & 0x00000002) test_transitions(test_gpu, test_loop);
    if (test_flags & 0x00000004) test_scopes(test_gpu, test_loop);
    if (test_flags & 0x00000008) test_others(test_gpu, test_loop);

    ImGui::ImVulkanShaderClear();
    return 0;
}