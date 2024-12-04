#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatLAB2RGBANode final : Node
{
    BP_NODE_WITH_NAME(MatLAB2RGBANode, "Color Conv LAB2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatLAB2RGBANode(BP* blueprint): Node(blueprint) { m_Name = "Mat Color Conv LAB2RGBA"; }

    ~MatLAB2RGBANode()
    {
        if (m_lab2rgb) { delete m_lab2rgb; m_lab2rgb = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_lab2rgb) { delete m_lab2rgb; m_lab2rgb = nullptr; }
    }

    void OnStop(Context& context) override
    {
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_lab = context.GetPinValue<ImGui::ImMat>(m_MatLAB);
        if (!mat_lab.empty())
        {
            m_mutex.lock();
            if (!m_lab2rgb)
            {
                int gpu = mat_lab.device == IM_DD_VULKAN ? mat_lab.device_number : ImGui::get_default_gpu_index();
                m_lab2rgb = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_lab2rgb)
                {
                    return {};
                }
            }
            ImGui::VkMat im_RGBA; 
            im_RGBA.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_lab.type : m_mat_data_type;
            im_RGBA.color_format = IM_CF_ABGR;
            m_NodeTimeMs = m_lab2rgb->LAB2RGB(mat_lab, im_RGBA, m_color_system, m_color_white);
            im_RGBA.time_stamp = mat_lab.time_stamp;
            im_RGBA.rate = mat_lab.rate;
            im_RGBA.flags = mat_lab.flags;
            m_MatRGBA.SetValue(im_RGBA);
            m_mutex.unlock();
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        int color_system = m_color_system;
        int color_white = m_color_white;
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        ImGui::Separator();
        ImGui::RadioButton("SRGB",      (int *)&color_system, IM_COLOR_XYZ_SRGB);
        ImGui::RadioButton("ADOBE",     (int *)&color_system, IM_COLOR_XYZ_ADOBE);
        ImGui::RadioButton("APPLE",     (int *)&color_system, IM_COLOR_XYZ_APPLE);
        ImGui::RadioButton("BRUCE",     (int *)&color_system, IM_COLOR_XYZ_BRUCE);
        ImGui::RadioButton("PAL",       (int *)&color_system, IM_COLOR_XYZ_PAL);
        ImGui::RadioButton("NTSC",      (int *)&color_system, IM_COLOR_XYZ_NTSC);
        ImGui::RadioButton("SMPTE",     (int *)&color_system, IM_COLOR_XYZ_SMPTE);
        ImGui::RadioButton("CIE",       (int *)&color_system, IM_COLOR_XYZ_CIE);
        ImGui::Separator();
        ImGui::RadioButton("D50",       (int *)&color_white, 0);
        ImGui::RadioButton("D65",       (int *)&color_white, 1);

        if (m_color_system != color_system) { m_color_system = (ImColorXYZSystem)color_system; changed = true; }
        if (m_color_white != color_white) { m_color_white = color_white; changed = true; }
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
        if (value.contains("color_system"))
        {
            auto& val = value["color_system"];
            if (val.is_number()) 
                m_color_system = (ImColorXYZSystem)val.get<imgui_json::number>();
        }
        if (value.contains("color_white"))
        {
            auto& val = value["color_white"];
            if (val.is_number()) 
                m_color_white = val.get<imgui_json::number>();
        }
        return ret;
    }
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["color_system"] = imgui_json::number(m_color_system);
        value["color_white"] = imgui_json::number(m_color_white);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatLAB}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatRGBA}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatLAB  = { this, "LAB" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatLAB };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatRGBA };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImColorXYZSystem m_color_system {IM_COLOR_XYZ_SRGB};
    int m_color_white {0}; // 0 = D50 1 = D65
    ImGui::ColorConvert_vulkan * m_lab2rgb {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatLAB2RGBANode, "Color Conv LAB2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
