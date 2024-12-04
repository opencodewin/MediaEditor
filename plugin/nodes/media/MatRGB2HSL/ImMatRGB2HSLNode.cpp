#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatRGBA2HSLNode final : Node
{
    BP_NODE_WITH_NAME(MatRGBA2HSLNode, "Color Conv RGBA2HSL", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatRGBA2HSLNode(BP* blueprint): Node(blueprint) { m_Name = "Mat Color Conv RGBA2HSL"; }

    ~MatRGBA2HSLNode()
    {
        if (m_rgb2hsl) { delete m_rgb2hsl; m_rgb2hsl = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_rgb2hsl) { delete m_rgb2hsl; m_rgb2hsl = nullptr; }
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
            if (!m_rgb2hsl)
            {
                int gpu = mat_rgba.device == IM_DD_VULKAN ? mat_rgba.device_number : ImGui::get_default_gpu_index();
                m_rgb2hsl = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_rgb2hsl)
                {
                    return {};
                }
            }
            ImGui::VkMat im_HSL; 
            im_HSL.type = m_mat_data_type == IM_DT_UNDEFINED ? IM_DT_FLOAT16 : m_mat_data_type;
            im_HSL.color_format = IM_CF_HSL;
            m_NodeTimeMs = m_rgb2hsl->RGB2HSL(mat_rgba, im_HSL);
            im_HSL.time_stamp = mat_rgba.time_stamp;
            im_HSL.rate = mat_rgba.rate;
            im_HSL.flags = mat_rgba.flags;
            m_MatHSL.SetValue(im_HSL);
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
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatRGBA}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatHSL}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatHSL  = { this, "HSL" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatRGBA };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatHSL };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImGui::ColorConvert_vulkan * m_rgb2hsl {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatRGBA2HSLNode, "Color Conv RGBA2HSL", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
