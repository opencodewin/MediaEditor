#include <cstdint>
#include <vector>
#include <thread>
#include <mutex>
#include <iostream>
#include <immat.h>
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>

using namespace std;

static bool G_QUITAPP = false;

void TestColorConvertProc(uint32_t threadIdx, uint32_t loopCount)
{
    uint32_t logInterval = 1000;
    ImGui::ColorConvert_vulkan* pClrCvt = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
    for (uint32_t i = 0; i < loopCount; i++)
    {
        ImGui::ImMat m;
        m.color_format = IM_CF_NV12;
        m.color_space = IM_CS_BT709;
        m.color_range = IM_CR_NARROW_RANGE;
        m.create_type(1920, 1080, 2, IM_DT_INT8);

        ImGui::VkMat rgbMat;
        rgbMat.type = IM_DT_INT8;
        rgbMat.color_format = IM_CF_RGBA;
        rgbMat.color_range = IM_CR_FULL_RANGE;
        rgbMat.color_space = IM_CS_SRGB;
        pClrCvt->ConvertColorFormat(m, rgbMat);

        if (i%logInterval == logInterval-1)
            cout << "[Thread#" << threadIdx << "] " << i+1 << endl;
    }
    delete pClrCvt;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
        return -1;
    uint32_t testThreadNum = atoi(argv[1]);
    uint32_t testLoopCount = atoi(argv[2]);
    vector<thread> testThreads;
    cout << "Start multi-threads ColorConvert test, threads=" << testThreadNum << ", loopcount=" << testLoopCount << " ..." << endl;
    for (uint32_t i = 0; i < testThreadNum; i++)
    {
        testThreads.push_back(thread(TestColorConvertProc, i, testLoopCount));
    }
    for (auto& th : testThreads)
        th.join();
    cout << "Test done." << endl;
    return 0;
}