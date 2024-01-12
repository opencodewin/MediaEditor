#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "HQDN3D_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct HQDN3DNode final : Node
{
    BP_NODE_WITH_NAME(HQDN3DNode, "HQDN3D Denoise", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Denoise")
    HQDN3DNode(BP* blueprint): Node(blueprint) { m_Name = "HQDN3D Denoise"; m_HasCustomLayout = true; m_Skippable = true; }
    ~HQDN3DNode()
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
        if (m_LumSpatialIn.IsLinked()) m_lum_spac = context.GetPinValue<float>(m_LumSpatialIn);
        if (m_ChromaSpatialIn.IsLinked()) m_chrom_spac = context.GetPinValue<float>(m_ChromaSpatialIn);
        if (m_LumTemporalIn.IsLinked()) m_lum_tmp = context.GetPinValue<float>(m_LumTemporalIn);
        if (m_ChromaTemporalIn.IsLinked()) m_chrom_tmp = context.GetPinValue<float>(m_ChromaTemporalIn);
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
                m_filter = new ImGui::HQDN3D_vulkan(mat_in.w, mat_in.h, mat_in.c, gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_lum_spac, m_chrom_spac, m_lum_tmp, m_chrom_tmp);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_LumSpatialIn.m_ID) m_LumSpatialIn.SetValue(m_lum_spac);
        if (receiver.m_ID == m_ChromaSpatialIn.m_ID) m_ChromaSpatialIn.SetValue(m_chrom_spac);
        if (receiver.m_ID == m_LumTemporalIn.m_ID) m_LumTemporalIn.SetValue(m_lum_tmp);
        if (receiver.m_ID == m_ChromaTemporalIn.m_ID) m_ChromaTemporalIn.SetValue(m_chrom_tmp);
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
        float _lum_spac = m_lum_spac;
        float _chrom_spac = m_chrom_spac;
        float _lum_tmp = m_lum_tmp;
        float _chrom_tmp = m_chrom_tmp;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::Dummy(ImVec2(160, 8));
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_LumSpatialIn.IsLinked());
        ImGui::SliderFloat("Luma Spatial##HQDN3D", &_lum_spac, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_luma_spatial##HQDN3D")) { _lum_spac = 6.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_luma_spatial##HQDN3D", key, ImGui::ImCurveEdit::DIM_X,  m_LumSpatialIn.IsLinked(), "luma spatial##HQDN3D@" + std::to_string(m_ID), 0.f, 50.f, 6.f, m_LumSpatialIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_ChromaSpatialIn.IsLinked());
        ImGui::SliderFloat("Chroma Spatial##HQDN3D", &_chrom_spac, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_chroma_spatial##HQDN3D")) { _chrom_spac = 4.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_chroma_spatial##HQDN3D", key, ImGui::ImCurveEdit::DIM_X, m_ChromaSpatialIn.IsLinked(), "chroma spatial##HQDN3D@" + std::to_string(m_ID), 0.f, 50.f, 4.f, m_ChromaSpatialIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_LumTemporalIn.IsLinked());
        ImGui::SliderFloat("Luma Temporal##HQDN3D", &_lum_tmp, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_luma_temporal##HQDN3D")) { _lum_tmp = 4.5f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_luma_temporal##HQDN3D", key, ImGui::ImCurveEdit::DIM_X, m_LumTemporalIn.IsLinked(), "luma temporal##HQDN3D@" + std::to_string(m_ID), 0.f, 50.f, 4.5f, m_LumTemporalIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_ChromaTemporalIn.IsLinked());
        ImGui::SliderFloat("Chroma Temporal##HQDN3D", &_chrom_tmp, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_chroma_temporal##HQDN3D")) { _chrom_tmp = 3.375f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_chroma_temporal##HQDN3D", key, ImGui::ImCurveEdit::DIM_X, m_ChromaTemporalIn.IsLinked(), "chroma temporal##HQDN3D@" + std::to_string(m_ID), 0.f, 50.f, 3.375f, m_ChromaTemporalIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_lum_spac != m_lum_spac) { m_lum_spac = _lum_spac; changed = true; }
        if (_chrom_spac != m_chrom_spac) { m_chrom_spac = _chrom_spac; changed = true; }
        if (_lum_tmp != m_lum_tmp) { m_lum_tmp = _lum_tmp; changed = true; }
        if (_chrom_tmp != m_chrom_tmp) { m_chrom_tmp = _chrom_tmp; changed = true; }
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
        if (value.contains("lum_spac"))
        {
            auto& val = value["lum_spac"];
            if (val.is_number()) 
                m_lum_spac = val.get<imgui_json::number>();
        }
        if (value.contains("chrom_spac"))
        {
            auto& val = value["chrom_spac"];
            if (val.is_number()) 
                m_chrom_spac = val.get<imgui_json::number>();
        }
        if (value.contains("lum_tmp"))
        {
            auto& val = value["lum_tmp"];
            if (val.is_number()) 
                m_lum_tmp = val.get<imgui_json::number>();
        }
        if (value.contains("chrom_tmp"))
        {
            auto& val = value["chrom_tmp"];
            if (val.is_number()) 
                m_chrom_tmp = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["lum_spac"] = imgui_json::number(m_lum_spac);
        value["chrom_spac"] = imgui_json::number(m_chrom_spac);
        value["lum_tmp"] = imgui_json::number(m_lum_tmp);
        value["chrom_tmp"] = imgui_json::number(m_chrom_tmp);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue3a4"));
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
    FloatPin  m_LumSpatialIn = { this, "Lum spatial"};
    FloatPin  m_ChromaSpatialIn = { this, "Chroma spatial"};
    FloatPin  m_LumTemporalIn = { this, "Lum temporal"};
    FloatPin  m_ChromaTemporalIn = { this, "Chroma temporal"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[6] = { &m_Enter, &m_MatIn, &m_LumSpatialIn, &m_ChromaSpatialIn, &m_LumTemporalIn, &m_ChromaTemporalIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_lum_spac    {6.0};
    float m_chrom_spac  {4.0};
    float m_lum_tmp     {4.5};
    float m_chrom_tmp   {3.375};
    ImGui::HQDN3D_vulkan * m_filter   {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(HQDN3DNode, "HQDN3D Denoise", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Denoise")
