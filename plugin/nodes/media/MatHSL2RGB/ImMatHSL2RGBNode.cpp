#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatHSL2RGBANode final : Node
{
    BP_NODE_WITH_NAME(MatHSL2RGBANode, "Color Conv HSL2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatHSL2RGBANode(BP* blueprint): Node(blueprint) { m_Name = "Mat Color Conv HSL2RGBA"; }

    ~MatHSL2RGBANode()
    {
        if (m_hsl2rgb) { delete m_hsl2rgb; m_hsl2rgb = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_hsl2rgb) { delete m_hsl2rgb; m_hsl2rgb = nullptr; }
    }

    void OnStop(Context& context) override
    {
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_hsl = context.GetPinValue<ImGui::ImMat>(m_MatHSL);
        if (!mat_hsl.empty())
        {
            m_mutex.lock();
            if (!m_hsl2rgb)
            {
                int gpu = mat_hsl.device == IM_DD_VULKAN ? mat_hsl.device_number : ImGui::get_default_gpu_index();
                m_hsl2rgb = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_hsl2rgb)
                {
                    return {};
                }
            }
            ImGui::VkMat im_RGBA; 
            im_RGBA.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_hsl.type : m_mat_data_type;
            im_RGBA.color_format = IM_CF_ABGR;
            m_NodeTimeMs = m_hsl2rgb->HSL2RGB(mat_hsl, im_RGBA);
            im_RGBA.time_stamp = mat_hsl.time_stamp;
            im_RGBA.rate = mat_hsl.rate;
            im_RGBA.flags = mat_hsl.flags;
            m_MatRGBA.SetValue(im_RGBA);
            m_mutex.unlock();
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
        return ret;
    }
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatHSL}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatRGBA}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatHSL  = { this, "HSL" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatHSL };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatRGBA };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImGui::ColorConvert_vulkan * m_hsl2rgb {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatHSL2RGBANode, "Color Conv HSL2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
