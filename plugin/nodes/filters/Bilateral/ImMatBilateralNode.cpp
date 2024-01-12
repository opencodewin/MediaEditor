#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Bilateral_vulkan.h>

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct BilateralNode final : Node
{
    BP_NODE_WITH_NAME(BilateralNode, "Bilateral Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Blur")
    BilateralNode(BP* blueprint): Node(blueprint) { m_Name = "Bilateral Blur"; m_HasCustomLayout = true; m_Skippable = true; }

    ~BilateralNode()
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
        if (m_SizeIn.IsLinked()) m_ksize = context.GetPinValue<float>(m_SizeIn);
        if (m_SigmaSpatialIn.IsLinked()) m_sigma_spatial = context.GetPinValue<float>(m_SigmaSpatialIn);
        if (m_SigmaColorIn.IsLinked()) m_sigma_color = context.GetPinValue<float>(m_SigmaColorIn);
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
                m_filter = new ImGui::Bilateral_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_ksize, m_sigma_spatial, m_sigma_color);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_SizeIn.m_ID) m_SizeIn.SetValue(m_ksize);
        if (receiver.m_ID == m_SigmaSpatialIn.m_ID) m_SigmaSpatialIn.SetValue(m_sigma_spatial);
        if (receiver.m_ID == m_SigmaColorIn.m_ID) m_SigmaColorIn.SetValue(m_sigma_color);
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
        ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        int _ksize = m_ksize;
        float _sigma_spatial = m_sigma_spatial;
        float _sigma_color = m_sigma_color;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SizeIn.IsLinked());
        ImGui::SliderInt("Kernel Size##Bilateral", &_ksize, 2, 20, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_size##Bilateral")) { _ksize = 5; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_size##Bilateral", key, ImGui::ImCurveEdit::DIM_X, m_SizeIn.IsLinked(), "size##Bilateral@" + std::to_string(m_ID), 2.f, 20.f, 5.f, m_SizeIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_SigmaSpatialIn.IsLinked());
        ImGui::SliderFloat("Sigma Spatial##Bilateral", &_sigma_spatial, 0.f, 100.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_sigma_spatial##Bilateral")) { _sigma_spatial = 10.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_sigma_spatial##Bilateral", key, ImGui::ImCurveEdit::DIM_X, m_SigmaSpatialIn.IsLinked(), "sigma spatial##Bilateral@" + std::to_string(m_ID), 0.f, 100.f, 10.f, m_SigmaSpatialIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_SigmaColorIn.IsLinked());
        ImGui::SliderFloat("Sigma Color##Bilateral", &_sigma_color, 0.f, 100.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_sigma_color##Bilateral")) { _sigma_color = 10.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_sigma_color##Bilateral", key, ImGui::ImCurveEdit::DIM_X, m_SigmaColorIn.IsLinked(), "sigma color##Bilateral@" + std::to_string(m_ID), 0.f, 100.f, 10.f, m_SigmaColorIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_ksize != m_ksize) { m_ksize = _ksize; changed = true; }
        if (_sigma_spatial != m_sigma_spatial) { m_sigma_spatial = _sigma_spatial; changed = true; }
        if (_sigma_color != m_sigma_color) { m_sigma_color = _sigma_color; changed = true; }
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
        if (value.contains("ksize"))
        {
            auto& val = value["ksize"];
            if (val.is_number()) 
                m_ksize = val.get<imgui_json::number>();
        }
        if (value.contains("sigma_spatial"))
        {
            auto& val = value["sigma_spatial"];
            if (val.is_number()) 
                m_sigma_spatial = val.get<imgui_json::number>();
        }
        if (value.contains("sigma_color"))
        {
            auto& val = value["sigma_color"];
            if (val.is_number()) 
                m_sigma_color = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["ksize"] = imgui_json::number(m_ksize);
        value["sigma_spatial"] = imgui_json::number(m_sigma_spatial);
        value["sigma_color"] = imgui_json::number(m_sigma_color);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue3a3"));
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
    FloatPin  m_SizeIn = { this, "Size"};
    FloatPin  m_SigmaSpatialIn = { this, "Sigma Spatial"};
    FloatPin  m_SigmaColorIn = { this, "Sigma Color"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_SizeIn, &m_SigmaSpatialIn, &m_SigmaColorIn };
    Pin* m_OutputPins[2] = { &m_Exit,&m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    int m_ksize             {5};
    float m_sigma_spatial   {10.f};
    float m_sigma_color     {10.f};
    ImGui::Bilateral_vulkan * m_filter {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(BilateralNode, "Bilateral Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Blur")
