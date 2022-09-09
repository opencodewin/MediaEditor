#include "VideoBlender.h"
#include <imvk_mat.h>
#include <AlphaBlending_vulkan.h>
#include "FFUtils.h"

using namespace std;

namespace DataLayer
{
    class VideoBlender_Impl : public VideoBlender
    {
    public:
        bool Init() override
        {
            bool success = m_ffBlender.Init();
            if (!success)
                m_errMsg = m_ffBlender.GetError();
            return success;
        }

        ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage, int32_t x, int32_t y) override
        {
            ImGui::ImMat res;
            if (m_useVulkanImpl)
            {
                ImGui::VkMat vkmat;
                vkmat.type = IM_DT_INT8;
                m_vulkanBlender.blend(overlayImage, baseImage, vkmat, x, y);
                res = vkmat;
                res.time_stamp = baseImage.time_stamp;
                res.duration = baseImage.time_stamp;
                res.color_space = baseImage.color_space;
                res.color_range = baseImage.color_range;
            }
            else
            {
                res = m_ffBlender.Blend(baseImage, overlayImage, x, y, overlayImage.w, overlayImage.h);
            }
            return res;
        }

        bool Init(const std::string& inputFormat, uint32_t w1, uint32_t h1, uint32_t w2, uint32_t h2, int32_t x, int32_t y) override
        {
            m_ovlyX = x;
            m_ovlyY = y;
            bool success = m_ffBlender.Init(inputFormat, w1, h1, w2, h2, x, y, false);
            if (!success)
                m_errMsg = m_ffBlender.GetError();
            return success;
        }

        ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage) override
        {
            ImGui::ImMat res;
            if (m_useVulkanImpl)
            {
                ImGui::VkMat vkmat;
                vkmat.type = IM_DT_INT8;
                m_vulkanBlender.blend(overlayImage, baseImage, vkmat, m_ovlyX, m_ovlyY);
                res = vkmat;
                res.time_stamp = baseImage.time_stamp;
                res.duration = baseImage.time_stamp;
                res.color_space = baseImage.color_space;
                res.color_range = baseImage.color_range;
            }
            else
            {
                res = m_ffBlender.Blend(baseImage, overlayImage);
            }
            return res;
        }

        std::string GetError() const override
        {
            return m_errMsg;
        }

    private:
        bool m_useVulkanImpl{true};
        int32_t m_ovlyX{0}, m_ovlyY{0};
        ImGui::AlphaBlending_vulkan m_vulkanBlender;
        FFOverlayBlender m_ffBlender;
        string m_errMsg;
    };

    VideoBlenderHolder CreateVideoBlender()
    {
        VideoBlenderHolder hBlender = VideoBlenderHolder(new VideoBlender_Impl(), [] (VideoBlender* ptr) {
            VideoBlender_Impl* blenderImplPtr = dynamic_cast<VideoBlender_Impl*>(ptr);
            if (blenderImplPtr)
                delete blenderImplPtr;
        });
        return hBlender;
    }
}