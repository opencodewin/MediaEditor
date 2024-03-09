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
struct MatYUV2RGBANode final : Node
{
    BP_NODE_WITH_NAME(MatYUV2RGBANode, "Color Conv YUV2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatYUV2RGBANode(BP* blueprint): Node(blueprint) 
    {
#if !IMGUI_VULKAN_SHADER
        m_mat_device_type = -1;
#endif
        m_Name = "Mat Color Conv YUV2RGBA";
    }

    ~MatYUV2RGBANode()
    {
#if IMGUI_VULKAN_SHADER
        if (m_yuv2rgb) { delete m_yuv2rgb; m_yuv2rgb = nullptr; }
#else
        if (m_img_convert_ctx) { sws_freeContext(m_img_convert_ctx); m_img_convert_ctx = nullptr; }
#endif
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
#if IMGUI_VULKAN_SHADER
        if (m_yuv2rgb) { delete m_yuv2rgb; m_yuv2rgb = nullptr; }
#else
        if (m_img_convert_ctx) { sws_freeContext(m_img_convert_ctx); m_img_convert_ctx = nullptr; }
#endif
    }

    void OnStop(Context& context) override
    {
        // keep last Mat
        //m_mutex.lock();
        //m_MatRGBA.SetValue(ImGui::ImMat());
        //m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_yuv = context.GetPinValue<ImGui::ImMat>(m_MatYUV);
        if (!mat_yuv.empty())
        {
            int video_depth = mat_yuv.type == IM_DT_INT8 ? 8 : mat_yuv.type == IM_DT_INT16 ? 16 : 8;
            int video_shift = mat_yuv.depth != 0 ? mat_yuv.depth : mat_yuv.type == IM_DT_INT8 ? 8 : mat_yuv.type == IM_DT_INT16 ? 16 : 8;
#if IMGUI_VULKAN_SHADER
            if (!m_yuv2rgb)
            {
                int gpu = mat_yuv.device == IM_DD_VULKAN ? mat_yuv.device_number : ImGui::get_default_gpu_index();
                m_yuv2rgb = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_yuv2rgb)
                {
                    return {};
                }
            }
            
            ImGui::ImMat im_RGB;
            im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_yuv.type : m_mat_data_type;
            im_RGB.color_format = IM_CF_ABGR;
            im_RGB.w = mat_yuv.w * m_scale;
            im_RGB.h = mat_yuv.h * m_scale;
            if (m_mat_device_type == 0)
            {
                im_RGB.device = IM_DD_VULKAN;
            }
            m_NodeTimeMs = m_yuv2rgb->ConvertColorFormat(mat_yuv, im_RGB, m_interpolation_mode);
            im_RGB.time_stamp = mat_yuv.time_stamp;
            im_RGB.rate = mat_yuv.rate;
            im_RGB.flags = mat_yuv.flags;
            m_MatRGBA.SetValue(im_RGB);
#else
            // ffmpeg swscale
            if (!m_img_convert_ctx)
            {
                AVPixelFormat format =  mat_yuv.color_format == IM_CF_YUV420 ? (video_depth > 8 ? AV_PIX_FMT_YUV420P10 : AV_PIX_FMT_YUV420P) :
                                        mat_yuv.color_format == IM_CF_YUV422 ? (video_depth > 8 ? AV_PIX_FMT_YUV422P10 : AV_PIX_FMT_YUV422P) :
                                        mat_yuv.color_format == IM_CF_YUV444 ? (video_depth > 8 ? AV_PIX_FMT_YUV444P10 : AV_PIX_FMT_YUV444P) :
                                        mat_yuv.color_format == IM_CF_NV12 ? (video_depth > 8 ? AV_PIX_FMT_P010LE : AV_PIX_FMT_NV12) :
                                        AV_PIX_FMT_YUV420P;
                m_img_convert_ctx = sws_getCachedContext(
                                    m_img_convert_ctx,
                                    mat_yuv.w,
                                    mat_yuv.h,
                                    format,
                                    mat_yuv.w,
                                    mat_yuv.h,
                                    AV_PIX_FMT_RGBA,
                                    SWS_BICUBIC,
                                    NULL, NULL, NULL);
            }
            if (m_img_convert_ctx)
            {
                ImGui::ImMat MatY = mat_yuv.channel(0);
                ImGui::ImMat MatU = mat_yuv.channel(1);
                ImGui::ImMat MatV = mat_yuv.channel(2);
                int mat_u_width = MatU.w >> (mat_yuv.color_format == IM_CF_YUV420 ? 1 : 0);
                int mat_v_width = MatV.w >> (mat_yuv.color_format == IM_CF_YUV420 ? 1 : 0);
                int data_shift = video_depth > 8 ? 2 : 1;
                ImGui::ImMat im_RGB(mat_yuv.w, mat_yuv.h, 4, 1u);
                uint8_t *src_data[] = { (uint8_t *)MatY.data, (uint8_t *)MatU.data, (uint8_t *)MatV.data };
                int src_linesize[] = {MatY.w * data_shift, mat_u_width * data_shift, mat_v_width * data_shift}; // how many for 16 bits?
                uint8_t *dst_data[] = { (uint8_t *)im_RGB.data };
                int dst_linesize[] = { mat_yuv.w * 4 }; // how many for 16 bits?
                sws_scale(
                    m_img_convert_ctx,
                    src_data,
                    src_linesize,
                    0, mat_yuv.h,
                    dst_data,
                    dst_linesize
                );
                im_RGB.time_stamp = mat_yuv.time_stamp;
                im_RGB.rate = mat_yuv.rate;
                im_RGB.flags = mat_yuv.flags;
                im_RGB.type = IM_DT_INT8;
                m_MatRGBA.SetValue(im_RGB);
            }
#endif
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
#if IMGUI_VULKAN_SHADER
        ImGui::Separator();
        changed |= ImGui::RadioButton("GPU", (int *)&m_mat_device_type, 0); ImGui::SameLine();
        changed |= ImGui::RadioButton("CPU", (int *)&m_mat_device_type, -1);
        ImGui::Separator();
        ImGui::DragFloat("Scale", &m_scale, 0.01, 0.1, 2.0, "%.1f");
        ImGui::Separator();
        changed |= ImGui::RadioButton("Nearest",       (int *)&m_interpolation_mode, IM_INTERPOLATE_NEAREST); ImGui::SameLine();
        changed |= ImGui::RadioButton("Bilinear",      (int *)&m_interpolation_mode, IM_INTERPOLATE_BILINEAR); ImGui::SameLine();
        changed |= ImGui::RadioButton("Bicubic",       (int *)&m_interpolation_mode, IM_INTERPOLATE_BICUBIC); ImGui::SameLine();
        changed |= ImGui::RadioButton("Area",          (int *)&m_interpolation_mode, IM_INTERPOLATE_AREA);
#else
        m_mat_device_type = -1;
#endif
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
        if (value.contains("mat_device_type"))
        {
            auto& val = value["mat_device_type"];
            if (val.is_number()) 
                m_mat_device_type = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("mat_scale"))
        {
            auto& val = value["mat_scale"];
            if (val.is_number()) 
                m_scale = val.get<imgui_json::number>();
        }
#if IMGUI_VULKAN_SHADER
        if (value.contains("interpolation"))
        {
            auto& val = value["interpolation"];
            if (val.is_number()) 
                m_interpolation_mode = (ImInterpolateMode)val.get<imgui_json::number>();
        }
#endif
#if !IMGUI_VULKAN_SHADER
        m_mat_device_type = -1;
#endif
        return ret;
    }
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["mat_device_type"] = imgui_json::number(m_mat_device_type);
        value["mat_scale"] = imgui_json::number(m_scale);
#if IMGUI_VULKAN_SHADER
        value["interpolation"] = imgui_json::number(m_interpolation_mode);
#endif
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatYUV}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatRGBA}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatYUV  = { this, "YUV" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatYUV };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatRGBA };

private:
    int m_mat_device_type {0};                      // 0 = gpu -1 = cpu
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    float m_scale   {1.0};
#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan * m_yuv2rgb {nullptr};
    ImInterpolateMode m_interpolation_mode {IM_INTERPOLATE_BILINEAR};
#else
    struct SwsContext *m_img_convert_ctx {nullptr};
#endif
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatYUV2RGBANode, "Color Conv YUV2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
