#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Crop_vulkan.h>

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct CropNode final : Node
{
    BP_NODE_WITH_NAME(CropNode, "Crop", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    CropNode(BP* blueprint): Node(blueprint) { m_Name = "Crop"; m_HasCustomLayout = true; m_Skippable = true; }

    ~CropNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_filter) { delete m_filter; m_filter = nullptr; }
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
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
            if (m_x2 - m_x1 <= 0 || m_y2 - m_y1 <= 0)
            {
                return {};
            }
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_filter)
            {
                m_filter = new ImGui::Crop_vulkan(gpu);
                if (!m_filter)
                    return {};
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            im_RGB.w = mat_in.w;
            im_RGB.h = mat_in.h;
            m_NodeTimeMs = m_filter->cropto(mat_in, im_RGB, m_x1 * im_RGB.w, m_y1 * im_RGB.h, (m_x2 - m_x1) * im_RGB.w, (m_y2 - m_y1) * im_RGB.h, m_xd * im_RGB.w, m_yd * im_RGB.h);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
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
        float _x1 = m_x1;
        float _y1 = m_y1;
        float _x2 = m_x2;
        float _y2 = m_y2;
        float _xd = m_xd;
        float _yd = m_yd;
        // TODO::Hard to get focus and input number
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderFloat("x1##Crop", &_x1, 0.f, 1.f, "%.02f", flags);
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_x1##Crop")) { _x1 = 0.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("y1##Crop", &_y1, 0.f, 1.f, "%.02f", flags);
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_y1##Crop")) { _y1 = 0.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("x2##Crop", &_x2, _x1, 1.0f, "%.02f", flags);
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_x2##Crop")) { _x2 = 1.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("y2##Crop", &_y2, _y1, 1.f, "%.02f", flags);
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_y2##Crop")) { _y2 = 1.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("xd##Crop", &_xd, -1.f, 1.f, "%.02f", flags);
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_xd##Crop")) { _xd = 0.0f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("yd##Crop", &_yd, -1.f, 1.f, "%.02f", flags);
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_yd##Crop")) { _yd = 0.0f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_x1 != m_x1) { m_x1 = _x1; changed = true; }
        if (_y1 != m_y1) { m_y1 = _y1; changed = true; }
        if (_x2 != m_x2) { m_x2 = _x2; changed = true; }
        if (_y2 != m_y2) { m_y2 = _y2; changed = true; }
        if (_xd != m_xd) { m_xd = _xd; changed = true; }
        if (_yd != m_yd) { m_yd = _yd; changed = true; }
        ImGui::EndDisabled();
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
        if (value.contains("x1"))
        {
            auto& val = value["x1"];
            if (val.is_number()) 
                m_x1 = val.get<imgui_json::number>();
        }
        if (value.contains("y1"))
        {
            auto& val = value["y1"];
            if (val.is_number()) 
                m_y1 = val.get<imgui_json::number>();
        }
        if (value.contains("x2"))
        {
            auto& val = value["x2"];
            if (val.is_number()) 
                m_x2 = val.get<imgui_json::number>();
        }
        if (value.contains("y2"))
        {
            auto& val = value["y2"];
            if (val.is_number()) 
                m_y2 = val.get<imgui_json::number>();
        }
        if (value.contains("xd"))
        {
            auto& val = value["xd"];
            if (val.is_number()) 
                m_xd = val.get<imgui_json::number>();
        }
        if (value.contains("yd"))
        {
            auto& val = value["yd"];
            if (val.is_number()) 
                m_yd = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["x1"] = imgui_json::number(m_x1);
        value["y1"] = imgui_json::number(m_y1);
        value["x2"] = imgui_json::number(m_x2);
        value["y2"] = imgui_json::number(m_y2);
        value["xd"] = imgui_json::number(m_xd);
        value["yd"] = imgui_json::number(m_yd);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue434"));
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_IReset  = { this, "Reset In" };
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_IReset, &m_MatIn };
    Pin* m_OutputPins[3] = { &m_Exit, &m_OReset, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImGui::Crop_vulkan * m_filter {nullptr};
    float m_x1 {0};
    float m_y1 {0};
    float m_x2 {1.0};
    float m_y2 {1.0};
    float m_xd {0};
    float m_yd {0};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(CropNode, "Crop", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
