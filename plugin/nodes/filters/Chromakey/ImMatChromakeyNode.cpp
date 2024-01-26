#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <imgui_node_editor_internal.h>
#include "ChromaKey_vulkan.h"

#define NODE_VERSION    0x01000000

namespace edd = ax::NodeEditor::Detail;
namespace BluePrint
{
struct ChromaKeyNode final : Node
{
    BP_NODE_WITH_NAME(ChromaKeyNode, "Chroma Key", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Matting")
    ChromaKeyNode(BP* blueprint): Node(blueprint) { m_Name = "Chroma Key"; m_HasCustomLayout = true; m_Skippable = true; }

    ~ChromaKeyNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
        ImGui::ClearMouseStraw();
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
                m_filter = new ImGui::ChromaKey_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_lumaMask, m_chromaColor,
                                m_alphaCutoffMin, m_alphaScale, m_alphaExponent,
                                m_alpha_only ? CHROMAKEY_OUTPUT_ALPHA_RGBA : m_despill ? CHROMAKEY_OUTPUT_NORMAL : CHROMAKEY_OUTPUT_NODESPILL);
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
        ImGuiIO& io = ImGui::GetIO();
        bool changed = false;
        ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        bool _alpha_only = m_alpha_only;
        bool _despill = m_despill;
        float _lumaMask = m_lumaMask;
        float _alphaCutoffMin = m_alphaCutoffMin;
        float _alphaScale = m_alphaScale;
        ImPixel _chromaColor = m_chromaColor;
        _chromaColor.a = m_alphaExponent;
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::Checkbox("Alpha Output##ChromaKey",&_alpha_only);
        ImGui::Checkbox("Color De-Spill##ChromaKey",&_despill);
        ImGui::SliderFloat("Luma Mask##ChromaKey", &_lumaMask, 0.f, 20.f, "%.1f", flags);
        ImGui::SliderFloat("Alpha Cutoff Min##ChromaKey", &_alphaCutoffMin, 0.f, 1.f, "%.2f", flags);
        ImGui::SliderFloat("Alpha Scale##ChromaKey", &_alphaScale, 0.f, 100.f, "%.1f", flags);
        ImGui::PopItemWidth();
        ImGui::SetNextItemWidth(200);
        ImGui::ColorPicker4("KeyColor##ChromaKey", (float *)&_chromaColor, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);
        ImGui::SameLine();
        if (ImGui::CheckButton(u8"\ue3b8" "##color_pick##ChromaKey", &m_color_straw, ImVec4(0.5, 0.0, 0.0, 1.0)))
        {
            io.MouseStrawed = m_color_straw;
        }
        ImGui::ShowTooltipOnHover("Color Straw");
        ImVec4 straw_color;
        if (ImGui::GetMouseStraw(straw_color))
        {
            _chromaColor.r = straw_color.x; _chromaColor.g = straw_color.y; _chromaColor.b = straw_color.z; _chromaColor.a = straw_color.w;
        }
        if (_lumaMask != m_lumaMask) { m_lumaMask = _lumaMask; changed = true; }
        if (_alphaCutoffMin != m_alphaCutoffMin) { m_alphaCutoffMin = _alphaCutoffMin; changed = true; }
        if (_alphaScale != m_alphaScale) { m_alphaScale = _alphaScale; changed = true; }
        if (_chromaColor.r != m_chromaColor.r || _chromaColor.g != m_chromaColor.g || _chromaColor.b != m_chromaColor.b) { 
            m_chromaColor = _chromaColor; changed = true; }
        if (_chromaColor.a != m_alphaExponent) { m_alphaExponent = _chromaColor.a; changed = true; }
        if (_alpha_only != m_alpha_only) { m_alpha_only = _alpha_only; changed = true; }
        if (_despill != m_despill) { m_despill = _despill; changed = true; }
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
        if (value.contains("alpha_only"))
        { 
            auto& val = value["alpha_only"];
            if (val.is_boolean())
                m_alpha_only = val.get<imgui_json::boolean>();
        }
        if (value.contains("despill"))
        { 
            auto& val = value["despill"];
            if (val.is_boolean())
                m_despill = val.get<imgui_json::boolean>();
        }
        if (value.contains("lumaMask"))
        {
            auto& val = value["lumaMask"];
            if (val.is_number()) 
                m_lumaMask = val.get<imgui_json::number>();
        }
        if (value.contains("alphaCutoffMin"))
        {
            auto& val = value["alphaCutoffMin"];
            if (val.is_number()) 
                m_alphaCutoffMin = val.get<imgui_json::number>();
        }
        if (value.contains("alphaScale"))
        {
            auto& val = value["alphaScale"];
            if (val.is_number()) 
                m_alphaScale = val.get<imgui_json::number>();
        }
        if (value.contains("alphaExponent"))
        {
            auto& val = value["alphaExponent"];
            if (val.is_number()) 
                m_alphaExponent = val.get<imgui_json::number>();
        }
        if (value.contains("chroma_color"))
        {
            auto& val = value["chroma_color"];
            if (val.is_vec4())
            {
                ImVec4 val4 = val.get<imgui_json::vec4>();
                m_chromaColor = ImPixel(val4.x, val4.y, val4.z, val4.w);
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["alpha_only"] = imgui_json::boolean(m_alpha_only);
        value["despill"] = imgui_json::boolean(m_despill);
        value["lumaMask"] = imgui_json::number(m_lumaMask);
        value["alphaCutoffMin"] = imgui_json::number(m_alphaCutoffMin);
        value["alphaScale"] = imgui_json::number(m_alphaScale);
        value["alphaExponent"] = imgui_json::number(m_alphaExponent);
        value["chroma_color"] = imgui_json::vec4(ImVec4(m_chromaColor.r, m_chromaColor.g, m_chromaColor.b, m_chromaColor.a));
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue2ca"));
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
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type  {IM_DT_UNDEFINED};
    int m_device            {-1};
    ImGui::ChromaKey_vulkan * m_filter {nullptr};
    bool m_color_straw          {false};
    bool  m_alpha_only          {false};
    bool  m_despill             {true};
    float m_lumaMask            {10.0f};
    ImPixel m_chromaColor       {0.0f, 1.0f, 0.0f, 1.0f};
    float m_alphaCutoffMin      {0.05f};
    float m_alphaScale          {50.f};
    float m_alphaExponent       {1.0f};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(ChromaKeyNode, "Chroma Key", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Matting")
