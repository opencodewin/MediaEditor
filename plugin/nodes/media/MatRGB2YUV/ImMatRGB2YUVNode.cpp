#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#ifdef __cplusplus
}
#endif
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>
#endif

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatRGBA2YUVNode final : Node
{
    BP_NODE_WITH_NAME(MatRGBA2YUVNode, "Color Conv RGBA2YUV", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatRGBA2YUVNode(BP* blueprint): Node(blueprint) { m_Name = "Mat Color Conv RGBA2YUV"; }

    ~MatRGBA2YUVNode()
    {
#if IMGUI_VULKAN_SHADER
        if (m_rgb2yuv) { delete m_rgb2yuv; m_rgb2yuv = nullptr; }
#else
        if (m_img_convert_ctx) { sws_freeContext(m_img_convert_ctx); m_img_convert_ctx = nullptr; }
#endif
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
#if IMGUI_VULKAN_SHADER
        if (m_rgb2yuv) { delete m_rgb2yuv; m_rgb2yuv = nullptr; }
#else
        if (m_img_convert_ctx) { sws_freeContext(m_img_convert_ctx); m_img_convert_ctx = nullptr; }
#endif
    }

    void OnStop(Context& context) override
    {
        // keep last Mat
        //m_mutex.lock();
        //m_MatYUV.SetValue(ImGui::ImMat());
        //m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_rgba = context.GetPinValue<ImGui::ImMat>(m_MatRGBA);
        if (!mat_rgba.empty())
        {
            m_mutex.lock();
#if IMGUI_VULKAN_SHADER
            if (!m_rgb2yuv)
            {
                int gpu = mat_rgba.device == IM_DD_VULKAN ? mat_rgba.device_number : ImGui::get_default_gpu_index();
                m_rgb2yuv = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_rgb2yuv)
                {
                    return {};
                }
            }
            ImGui::VkMat im_YUV; 
            im_YUV.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_rgba.type : m_mat_data_type;
            im_YUV.color_format = m_color_format;
            im_YUV.color_space = m_color_space;
            im_YUV.color_range = m_color_range;
            m_NodeTimeMs = m_rgb2yuv->ConvertColorFormat(mat_rgba, im_YUV);
            im_YUV.time_stamp = mat_rgba.time_stamp;
            im_YUV.rate = mat_rgba.rate;
            im_YUV.flags = mat_rgba.flags;
            m_MatYUV.SetValue(im_YUV);
#else
            int video_depth = m_mat_data_type == IM_DT_INT8 ? 8 : m_mat_data_type == IM_DT_INT16 ? 16 : 8;
            if (!m_img_convert_ctx)
            {
                AVPixelFormat format =  m_color_format == IM_CF_YUV420 ? (video_depth > 8 ? AV_PIX_FMT_YUV420P10 : AV_PIX_FMT_YUV420P) :
                                        m_color_format == IM_CF_YUV422 ? (video_depth > 8 ? AV_PIX_FMT_YUV422P10 : AV_PIX_FMT_YUV422P) :
                                        m_color_format == IM_CF_YUV444 ? (video_depth > 8 ? AV_PIX_FMT_YUV444P10 : AV_PIX_FMT_YUV444P) :
                                        m_color_format == IM_CF_NV12 ? (video_depth > 8 ? AV_PIX_FMT_P010LE : AV_PIX_FMT_NV12) :
                                        AV_PIX_FMT_YUV420P;
                m_img_convert_ctx = sws_getCachedContext(
                                    m_img_convert_ctx,
                                    mat_rgba.w,
                                    mat_rgba.h,
                                    AV_PIX_FMT_RGBA,
                                    mat_rgba.w,
                                    mat_rgba.h,
                                    AV_PIX_FMT_NV12,
                                    SWS_BICUBIC,
                                    NULL, NULL, NULL);
            }
            if (m_img_convert_ctx)
            {
                ImGui::ImMat im_YUV(mat_rgba.w, mat_rgba.h, 4, 1u, 4);
                ImGui::ImMat MatY = im_YUV.channel(0);
                ImGui::ImMat MatU = im_YUV.channel(1);
                ImGui::ImMat MatV = im_YUV.channel(2);
                int data_shift = video_depth > 8 ? 2 : 1;
                uint8_t *src_data[] = { (uint8_t *)mat_rgba.data };
                uint8_t *dst_data[] = { (uint8_t *)MatY.data, (uint8_t *)MatU.data, (uint8_t *)MatV.data };
                int src_linesize[] = { mat_rgba.w * 4 };
                int dst_linesize[] = {MatY.w * data_shift, MatU.w * data_shift, MatV.w * data_shift};
                sws_scale(
                    m_img_convert_ctx,
                    src_data,
                    src_linesize,
                    0, mat_rgba.h,
                    dst_data,
                    dst_linesize
                );
                im_YUV.color_format = m_color_format;
                im_YUV.color_space = m_color_space;
                im_YUV.color_range = m_color_range;
                im_YUV.time_stamp = mat_rgba.time_stamp;
                im_YUV.rate = mat_rgba.rate;
                im_YUV.flags = mat_rgba.flags;
                im_YUV.type = IM_DT_INT8;
                m_MatYUV.SetValue(im_YUV);
            }
#endif
            m_mutex.unlock();
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        int color_format = m_color_format;
        int color_space = m_color_space;
        int color_range = m_color_range;
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        ImGui::Separator();
        ImGui::RadioButton("YUV420",  (int *)&color_format, IM_CF_YUV420);
        ImGui::RadioButton("YUV422",   (int *)&color_format, IM_CF_YUV422);
        ImGui::RadioButton("YUV440",  (int *)&color_format, IM_CF_YUV440);
        ImGui::RadioButton("YUV444",   (int *)&color_format, IM_CF_YUV444);
        ImGui::RadioButton("NV12",   (int *)&color_format, IM_CF_NV12);
        ImGui::Separator();
        ImGui::RadioButton("BT601",  (int *)&color_space, IM_CS_BT601);
        ImGui::RadioButton("BT709",   (int *)&color_space, IM_CS_BT709);
        ImGui::RadioButton("BT2020",  (int *)&color_space, IM_CS_BT2020);
        ImGui::Separator();
        ImGui::RadioButton("Full Range",  (int *)&color_range, IM_CR_FULL_RANGE);
        ImGui::RadioButton("Narrow Range",   (int *)&color_range, IM_CR_NARROW_RANGE);

        if (m_color_format != color_format) { m_color_format = (ImColorFormat)color_format; changed = true; }
        if (m_color_space != color_space) { m_color_space = (ImColorSpace)color_space; changed = true; }
        if (m_color_range != color_range) { m_color_range = (ImColorRange)color_range; changed = true; }
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;
        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("color_format"))
        {
            auto& val = value["color_format"];
            if (val.is_number()) 
                m_color_format = (ImColorFormat)val.get<imgui_json::number>();
        }
        if (value.contains("color_space"))
        {
            auto& val = value["color_space"];
            if (val.is_number()) 
                m_color_space = (ImColorSpace)val.get<imgui_json::number>();
        }
        if (value.contains("color_range"))
        {
            auto& val = value["color_range"];
            if (val.is_number()) 
                m_color_range = (ImColorRange)val.get<imgui_json::number>();
        }
        return ret;
    }
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["color_format"] = imgui_json::number(m_color_format);
        value["color_space"] = imgui_json::number(m_color_space);
        value["color_range"] = imgui_json::number(m_color_range);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatRGBA}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatYUV}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatYUV  = { this, "YUV" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatRGBA };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatYUV };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImColorFormat m_color_format {IM_CF_YUV420};
    ImColorSpace m_color_space {IM_CS_BT709};
    ImColorRange m_color_range {IM_CR_NARROW_RANGE};
#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan * m_rgb2yuv {nullptr};
#else
    struct SwsContext *m_img_convert_ctx {nullptr};
#endif
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatRGBA2YUVNode, "Color Conv RGBA2YUV", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
