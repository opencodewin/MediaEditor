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
#include <Resize_vulkan.h>
#endif

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatResizeNode final : Node
{
    BP_NODE_WITH_NAME(MatResizeNode, "Mat Resize", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatResizeNode(BP* blueprint): Node(blueprint) { m_Name = "Mat Resize"; m_HasCustomLayout = true; m_Skippable = true; }

    ~MatResizeNode()
    {
#if IMGUI_VULKAN_SHADER
        if (m_resize) { delete m_resize; m_resize = nullptr; }
#else
        if (m_img_resize_ctx) { sws_freeContext(m_img_resize_ctx); m_img_resize_ctx = nullptr; }
#endif
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
#if IMGUI_VULKAN_SHADER
        if (m_resize) { delete m_resize; m_resize = nullptr; }
#else
        if (m_img_resize_ctx) { sws_freeContext(m_img_resize_ctx); m_img_resize_ctx = nullptr; }
#endif
    }

    void OnStop(Context& context) override
    {
        // keep last Mat
        //m_mutex.lock();
        //m_MatOut.SetValue(ImGui::ImMat());
        //m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_IReset.m_ID)
        {
            Reset(context);
            return m_OReset;
        }
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
#if IMGUI_VULKAN_SHADER
            if (!m_resize)
            {
                int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
                m_resize = new ImGui::Resize_vulkan(gpu);
                if (!m_resize)
                {
                    return {};
                }
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_resize->Resize(mat_in, im_RGB, m_fx, m_fy, m_interpolation_mode);
            im_RGB.time_stamp = mat_in.time_stamp;
            im_RGB.rate = mat_in.rate;
            im_RGB.flags = mat_in.flags;
            m_MatOut.SetValue(im_RGB);
#else
            int dst_width = Im_AlignSize((m_fx > 0 ? mat_in.w * m_fx : mat_in.w), 4);
            int dst_height = Im_AlignSize((m_fx > 0 ? (m_fy > 0 ? mat_in.h * m_fy : mat_in.h * m_fx) : mat_in.h), 4);

            if (!m_img_resize_ctx)
            {
                int flags = m_interpolation_mode == IM_INTERPOLATE_NEAREST ? SWS_FAST_BILINEAR : 
                            m_interpolation_mode == IM_INTERPOLATE_BILINEAR ? SWS_BILINEAR :
                            m_interpolation_mode == IM_INTERPOLATE_BICUBIC ? SWS_BICUBIC :
                            m_interpolation_mode == IM_INTERPOLATE_AREA ? SWS_AREA : SWS_FAST_BILINEAR;
                m_img_resize_ctx = sws_getCachedContext(
                                    m_img_resize_ctx,
                                    mat_in.w,
                                    mat_in.h,
                                    AV_PIX_FMT_RGBA,
                                    dst_width,
                                    dst_height,
                                    AV_PIX_FMT_RGBA,
                                    flags,
                                    NULL, NULL, NULL);
            }
            if (m_img_resize_ctx)
            {
                ImGui::ImMat im_RGB(dst_width, dst_height, 4, 1u);
                uint8_t *src_data[] = { (uint8_t *)mat_in.data };
                int src_linesize[] = {mat_in.w * 4};
                uint8_t *dst_data[] = { (uint8_t *)im_RGB.data };
                int dst_linesize[] = { im_RGB.w * 4 }; 
                sws_scale(
                    m_img_resize_ctx,
                    src_data,
                    src_linesize,
                    0, mat_in.h,
                    dst_data,
                    dst_linesize
                );
                im_RGB.time_stamp = mat_in.time_stamp;
                im_RGB.rate = mat_in.rate;
                im_RGB.flags = mat_in.flags;
                im_RGB.type = IM_DT_INT8;
                m_MatOut.SetValue(im_RGB);
            }
#endif
        }
        return m_Exit;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        float _fx = m_fx;
        float _fy = m_fy;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderFloat("fx", &_fx, 0.0, 4.f, "%.2f", flags);
        ImGui::SliderFloat("fy", &_fy, 0.0, 4.f, "%.2f", flags);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_fx != m_fx) { m_fx = _fx; changed = true; }
        if (_fy != m_fy) { m_fy = _fy; changed = true; }
        return changed;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::Dummy(ImVec2(200, 20));
        ImGui::PushItemWidth(200);

        changed |= ImGui::RadioButton("Nearest",       (int *)&m_interpolation_mode, IM_INTERPOLATE_NEAREST); ImGui::SameLine();
        changed |= ImGui::RadioButton("Bilinear",      (int *)&m_interpolation_mode, IM_INTERPOLATE_BILINEAR); ImGui::SameLine();
        changed |= ImGui::RadioButton("Bicubic",       (int *)&m_interpolation_mode, IM_INTERPOLATE_BICUBIC); ImGui::SameLine();
        changed |= ImGui::RadioButton("Area",          (int *)&m_interpolation_mode, IM_INTERPOLATE_AREA);
        ImGui::PopItemWidth();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
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
        if (value.contains("fx"))
        {
            auto& val = value["fx"];
            if (val.is_number()) 
                m_fx = val.get<imgui_json::number>();
        }
        if (value.contains("fy"))
        {
            auto& val = value["fy"];
            if (val.is_number()) 
                m_fy = val.get<imgui_json::number>();
        }
        if (value.contains("interpolation"))
        {
            auto& val = value["interpolation"];
            if (val.is_number()) 
                m_interpolation_mode = (ImInterpolateMode)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["fx"] = imgui_json::number(m_fx);
        value["fy"] = imgui_json::number(m_fy);
        value["interpolation"] = imgui_json::number(m_interpolation_mode);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_IReset  = { this, "Reset In" };
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_IReset, &m_MatIn };
    Pin* m_OutputPins[3] = { &m_Exit, &m_OReset, &m_MatOut };

private:
#if IMGUI_VULKAN_SHADER
    ImGui::Resize_vulkan * m_resize {nullptr};
#else
    struct SwsContext *m_img_resize_ctx {nullptr};
#endif
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImInterpolateMode m_interpolation_mode {IM_INTERPOLATE_BILINEAR};
    float m_fx {1.0};
    float m_fy {0.0};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatResizeNode, "Mat Resize", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
