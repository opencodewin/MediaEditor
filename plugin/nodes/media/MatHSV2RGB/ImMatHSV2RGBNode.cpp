#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatHSV2RGBANode final : Node
{
    BP_NODE_WITH_NAME(MatHSV2RGBANode, "Color Conv HSV2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatHSV2RGBANode(BP* blueprint): Node(blueprint) { m_Name = "Mat Color Conv HSV2RGBA"; }

    ~MatHSV2RGBANode()
    {
        if (m_hsv2rgb) { delete m_hsv2rgb; m_hsv2rgb = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_hsv2rgb) { delete m_hsv2rgb; m_hsv2rgb = nullptr; }
    }

    void OnStop(Context& context) override
    {
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_hsv = context.GetPinValue<ImGui::ImMat>(m_MatHSV);
        if (!mat_hsv.empty())
        {
            m_mutex.lock();
            if (!m_hsv2rgb)
            {
                int gpu = mat_hsv.device == IM_DD_VULKAN ? mat_hsv.device_number : ImGui::get_default_gpu_index();
                m_hsv2rgb = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_hsv2rgb)
                {
                    return {};
                }
            }
            ImGui::VkMat im_RGBA; 
            im_RGBA.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_hsv.type : m_mat_data_type;
            im_RGBA.color_format = IM_CF_ABGR;
            m_NodeTimeMs = m_hsv2rgb->HSV2RGB(mat_hsv, im_RGBA);
            im_RGBA.time_stamp = mat_hsv.time_stamp;
            im_RGBA.rate = mat_hsv.rate;
            im_RGBA.flags = mat_hsv.flags;
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
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatHSV}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatRGBA}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatHSV  = { this, "HSV" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatHSV };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatRGBA };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImGui::ColorConvert_vulkan * m_hsv2rgb {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatHSV2RGBANode, "Color Conv HSV2RGBA", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
