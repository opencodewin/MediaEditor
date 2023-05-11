#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Star_vulkan.h"
#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct StarEffectNode final : Node
{
    BP_NODE_WITH_NAME(StarEffectNode, "Star Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Effect")
    StarEffectNode(BP* blueprint): Node(blueprint) { m_Name = "Star Effect"; }

    ~StarEffectNode()
    {
        if (m_effect) { delete m_effect; m_effect = nullptr; }
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
        if (m_TimeIn.IsLinked()) m_time = context.GetPinValue<float>(m_TimeIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_effect || gpu != m_device)
            {
                if (m_effect) { delete m_effect; m_effect = nullptr; }
                m_effect = new ImGui::Star_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, m_time, m_layers, m_color);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_TimeIn.m_ID)
        {
            m_TimeIn.SetValue(m_time);
        }
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

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        float _time = m_time;
        int _layers = m_layers;
        ImPixel _color = m_color;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp; // ImGuiSliderFlags_NoInput
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_TimeIn.IsLinked());
        ImGui::SliderFloat("Time##Star", &_time, 0.1, 8.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_time##Star")) { _time = 0.f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_time##Star", key, "time##Star", 0.0f, 100.f, 1.f);
        ImGui::EndDisabled();
        ImGui::SliderInt("Layers##Star", &_layers, 1, 20, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_layers##Star")) { _layers = 2; changed = true; }
        ImGui::PopItemWidth();
        if (ImGui::ColorEdit4("Color##Star", (float*)&_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        {
            m_color = _color; changed = true;
        } ImGui::SameLine(); ImGui::TextUnformatted("Shadow Color");
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_color##DStar")) { m_color = {1.0f, 0.1f, 0.9f, 1.0f}; changed = true; }
        if (_time != m_time) { m_time = _time; changed = true; }
        if (_layers != m_layers) { m_layers = _layers; changed = true; }
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
        if (value.contains("time"))
        {
            auto& val = value["time"];
            if (val.is_number()) 
                m_time = val.get<imgui_json::number>();
        }
        if (value.contains("layers"))
        {
            auto& val = value["layers"];
            if (val.is_number()) 
                m_layers = val.get<imgui_json::number>();
        }
        if (value.contains("color"))
        {
            auto& val = value["color"];
            if (val.is_vec4())
            {
                ImVec4 val4 = val.get<imgui_json::vec4>();
                m_color = ImPixel(val4.x, val4.y, val4.z, val4.w);
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["time"] = imgui_json::number(m_time);
        value["layers"] = imgui_json::number(m_layers);
        value["color"] = imgui_json::vec4(ImVec4(m_color.r, m_color.g, m_color.b, m_color.a));
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        float font_size = ImGui::GetFontSize();
        float size_min = size.x > size.y ? size.y : size.x;
        ImGui::SetWindowFontScale((size_min - 16) / font_size);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
#if IMGUI_ICONS
        ImGui::Button((std::string(u8"\ue3ea") + "##" + std::to_string(m_ID)).c_str(), size);
#else
        ImGui::Button((std::string("F") + "##" + std::to_string(m_ID)).c_str(), size);
#endif
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0);
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
    FloatPin  m_TimeIn  = { this, "Time" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_MatIn, &m_TimeIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_time            {0.f};
    int m_layers            {2};
    ImPixel m_color         {1.0f, 0.1f, 0.9f, 1.0f};
    ImGui::Star_vulkan * m_effect   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(StarEffectNode, "Star Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Effect")