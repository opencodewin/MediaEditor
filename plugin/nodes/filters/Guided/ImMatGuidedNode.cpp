#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <UI.h>
#include <ImVulkanShader.h>
#include "Guided_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct GuidedNode final : Node
{
    BP_NODE_WITH_NAME(GuidedNode, "Guided Filter", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Matting")
    GuidedNode(BP* blueprint): Node(blueprint) { m_Name = "Guided Filter"; }
    ~GuidedNode()
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
        if (m_EPSIn.IsLinked()) m_eps = context.GetPinValue<float>(m_EPSIn);
        if (m_RangeIn.IsLinked()) m_range = context.GetPinValue<float>(m_RangeIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::Guided_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_range, m_eps);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_EPSIn.m_ID) m_EPSIn.SetValue(m_eps);
        if (receiver.m_ID == m_RangeIn.m_ID) m_RangeIn.SetValue(m_range);
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        float _eps = m_eps;
        int _range = m_range;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp; // ImGuiSliderFlags_NoInput
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_EPSIn.IsLinked());
        ImGui::SliderFloat("EPS##GuidedFilter", &_eps, 0.000001, 0.001f, "%.6f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_eps##GuidedFilter")) { _eps = 0.0001; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_eps##GuidedFilter", key, "eps##GuidedFilter", 0.000001f, 0.001f, 0.0001f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_RangeIn.IsLinked());
        ImGui::SliderInt("Range##GuidedFilter", &_range, 0, 30, "%.d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_range##GuidedFilter")) { _range = 4; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_range##GuidedFilter", key, "range##GuidedFilter", 0.f, 30.f, 4.f);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_eps != m_eps) { m_eps = _eps; changed = true; }
        if (_range != m_range) { m_range = _range; changed = true; }
        
        return m_Enabled ? changed : false;
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
        if (value.contains("eps"))
        {
            auto& val = value["eps"];
            if (val.is_number()) 
                m_eps = val.get<imgui_json::number>();
        }
        if (value.contains("range"))
        {
            auto& val = value["range"];
            if (val.is_number()) 
                m_range = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["eps"] = imgui_json::number(m_eps);
        value["range"] = imgui_json::number(m_range);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf2ab"));
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
    FloatPin  m_EPSIn   = { this, "EPS" };
    FloatPin  m_RangeIn = { this, "Range" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatIn, &m_EPSIn, &m_RangeIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float   m_eps           {1e-4};
    int     m_range         {4};
    ImGui::Guided_vulkan * m_filter   {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(GuidedNode, "Guided Filter", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Matting")
