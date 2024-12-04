#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatRGBA2LABNode final : Node
{
    BP_NODE_WITH_NAME(MatRGBA2LABNode, "Color Conv RGBA2LAB", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatRGBA2LABNode(BP* blueprint): Node(blueprint) { m_Name = "Mat Color Conv RGBA2LAB"; }

    ~MatRGBA2LABNode()
    {
        if (m_rgb2lab) { delete m_rgb2lab; m_rgb2lab = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_rgb2lab) { delete m_rgb2lab; m_rgb2lab = nullptr; }
    }

    void OnStop(Context& context) override
    {
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_rgba = context.GetPinValue<ImGui::ImMat>(m_MatRGBA);
        if (!mat_rgba.empty())
        {
            m_mutex.lock();
            if (!m_rgb2lab)
            {
                int gpu = mat_rgba.device == IM_DD_VULKAN ? mat_rgba.device_number : ImGui::get_default_gpu_index();
                m_rgb2lab = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_rgb2lab)
                {
                    return {};
                }
            }
            ImGui::VkMat im_LAB; 
            im_LAB.type = m_mat_data_type == IM_DT_UNDEFINED ? IM_DT_FLOAT16 : m_mat_data_type;
            im_LAB.color_format = IM_CF_LAB;
            m_NodeTimeMs = m_rgb2lab->RGB2LAB(mat_rgba, im_LAB, m_color_system, m_color_white);
            im_LAB.time_stamp = mat_rgba.time_stamp;
            im_LAB.rate = mat_rgba.rate;
            im_LAB.flags = mat_rgba.flags;
            m_MatLAB.SetValue(im_LAB);
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
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatRGBA}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatLAB}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatLAB  = { this, "LAB" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatRGBA };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatLAB };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImColorXYZSystem m_color_system {IM_COLOR_XYZ_SRGB};
    int m_color_white {0}; // 0 = D50 1 = D65
    ImGui::ColorConvert_vulkan * m_rgb2lab {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatRGBA2LABNode, "Color Conv RGBA2LAB", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
