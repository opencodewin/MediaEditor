#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>

#define NODE_VERSION    0x01030000

namespace BluePrint
{
struct MatRGBA2HSVNode final : Node
{
    BP_NODE_WITH_NAME(MatRGBA2HSVNode, "Color Conv RGBA2HSV", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatRGBA2HSVNode(BP* blueprint): Node(blueprint) { m_Name = "Mat Color Conv RGBA2HSV"; }

    ~MatRGBA2HSVNode()
    {
        if (m_rgb2hsv) { delete m_rgb2hsv; m_rgb2hsv = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_rgb2hsv) { delete m_rgb2hsv; m_rgb2hsv = nullptr; }
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
            if (!m_rgb2hsv)
            {
                int gpu = mat_rgba.device == IM_DD_VULKAN ? mat_rgba.device_number : ImGui::get_default_gpu_index();
                m_rgb2hsv = new ImGui::ColorConvert_vulkan(gpu);
                if (!m_rgb2hsv)
                {
                    return {};
                }
            }
            ImGui::VkMat im_HSV; 
            im_HSV.type = m_mat_data_type == IM_DT_UNDEFINED ? IM_DT_FLOAT16 : m_mat_data_type;
            im_HSV.color_format = IM_CF_HSV;
            m_NodeTimeMs = m_rgb2hsv->RGB2HSV(mat_rgba, im_HSV);
            im_HSV.time_stamp = mat_rgba.time_stamp;
            im_HSV.rate = mat_rgba.rate;
            im_HSV.flags = mat_rgba.flags;
            m_MatHSV.SetValue(im_HSV);
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
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatHSV}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };

    MatPin    m_MatHSV  = { this, "HSV" };
    MatPin    m_MatRGBA = { this, "RGBA" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatRGBA };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatHSV };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImGui::ColorConvert_vulkan * m_rgb2hsv {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatRGBA2HSVNode, "Color Conv RGBA2HSV", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
