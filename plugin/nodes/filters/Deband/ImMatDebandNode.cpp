#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "DeBand_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct DeBandNode final : Node
{
    BP_NODE_WITH_NAME(DeBandNode, "Deband", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Enhance")
    DeBandNode(BP* blueprint): Node(blueprint) { m_Name = "DeBand"; m_HasCustomLayout = true; m_Skippable = true; }
    ~DeBandNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (m_ThresholdIn.IsLinked()) m_threshold = context.GetPinValue<float>(m_ThresholdIn);
        if (m_RangeIn.IsLinked()) m_range = context.GetPinValue<float>(m_RangeIn);
        if (m_DirectionIn.IsLinked()) m_direction = context.GetPinValue<float>(m_DirectionIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device ||
                m_filter->in_width != mat_in.w || 
                m_filter->in_height != mat_in.h ||
                m_filter->in_channels != mat_in.c)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::DeBand_vulkan(mat_in.w, mat_in.h, mat_in.c, gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_range, m_direction * M_PI);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_threshold, m_blur);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_ThresholdIn.m_ID) m_ThresholdIn.SetValue(m_threshold);
        if (receiver.m_ID == m_RangeIn.m_ID) m_RangeIn.SetValue(m_range);
        if (receiver.m_ID == m_DirectionIn.m_ID) m_DirectionIn.SetValue(m_direction);
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        float setting_offset = 320;
        if (!embedded)
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            setting_offset = sub_window_size.x - 80;
        }
        bool changed = false;
        float _threshold = m_threshold;
        int _range = m_range;
        float _direction = m_direction;
        bool _blur = m_blur;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_ThresholdIn.IsLinked());
        ImGui::SliderFloat("Threshold##DeBand", &_threshold, 0, 0.05f, "%.3f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_threshold##DeBand")) { _threshold = 0.05f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_threshold##DeBand", key, ImGui::ImCurveEdit::DIM_X, m_ThresholdIn.IsLinked(), "threshold##DeBand@" + std::to_string(m_ID), 0.f, 0.05f, 0.01f, m_ThresholdIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_RangeIn.IsLinked());
        ImGui::SliderInt("Range##DeBand", &_range, 0, 64, "%.d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_range##DeBand")) { _range = 16.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_range##DeBand", key, ImGui::ImCurveEdit::DIM_X, m_RangeIn.IsLinked(), "range##DeBand@" + std::to_string(m_ID), 0.f, 64.f, 16.f, m_RangeIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_DirectionIn.IsLinked());
        ImGui::SliderFloat("Direction##DeBand", &_direction, 0.f, 4.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_direction##DeBand")) { _direction = 2.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_direction##DeBand", key, ImGui::ImCurveEdit::DIM_X, m_DirectionIn.IsLinked(), "direction##DeBand@" + std::to_string(m_ID), 0.f, 4.f, 2.f, m_DirectionIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::TextUnformatted("Blur:");ImGui::SameLine();
        ImGui::ToggleButton("##Blur##DeBand",&_blur);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_threshold != m_threshold) { m_threshold = _threshold; changed = true; }
        if (_range != m_range) { m_range = _range; changed = true; }
        if (_direction != m_direction) { m_direction = _direction; changed = true; }
        if (_blur != m_blur) { m_blur = _blur; changed = true; }
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
        if (value.contains("threshold"))
        {
            auto& val = value["threshold"];
            if (val.is_number()) 
                m_threshold = val.get<imgui_json::number>();
        }
        if (value.contains("range"))
        {
            auto& val = value["range"];
            if (val.is_number()) 
                m_range = val.get<imgui_json::number>();
        }
        if (value.contains("direction"))
        {
            auto& val = value["direction"];
            if (val.is_number()) 
                m_direction = val.get<imgui_json::number>();
        }
        if (value.contains("blur"))
        {
            auto& val = value["blur"];
            if (val.is_boolean()) 
                m_blur = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["threshold"] = imgui_json::number(m_threshold);
        value["range"] = imgui_json::number(m_range);
        value["direction"] = imgui_json::number(m_direction);
        value["blur"] = m_blur;
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf75b"));
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    FloatPin  m_ThresholdIn = { this, "Threshold"};
    FloatPin  m_RangeIn = { this, "Range"};
    FloatPin  m_DirectionIn = { this, "Direction"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_ThresholdIn, &m_RangeIn, &m_DirectionIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_threshold       {0.01};
    int m_range             {16};
    float m_direction       {2};
    bool m_blur             {false};
    ImGui::DeBand_vulkan *  m_filter {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(DeBandNode, "Deband", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Enhance")
