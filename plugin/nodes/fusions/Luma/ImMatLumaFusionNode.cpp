#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Luma_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct LumaFusionNode final : Node
{
    BP_NODE_WITH_NAME(LumaFusionNode, "Luma Transform", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Fusion#Video#Move")
    LumaFusionNode(BP* blueprint): Node(blueprint) 
    {
        m_Name = "Luma Transform";
        if (blueprint)
        {
            // load masks
            auto node_path = GetURL();
            if (!node_path.empty())
            {
                std::string masks_path = node_path + "masks";
                std::vector<std::string> masks, mask_names;
                std::vector<std::string> mask_filter = {"png"};
                if (DIR_Iterate(masks_path, masks, mask_names, mask_filter, false) == 0)
                {
                    for (int i = 0; i < masks.size(); i++)
                    {
                        ImGui::ImMat mat;
                        ImGui::ImLoadImageToMat(masks[i].c_str(), mat, true);
                        if (!mat.empty())
                        {
                            m_masks.push_back(mat);
                            ImGui::ImMat mat_snapshot = mat.resize(ImSize(64, 64));
                            m_mask_snapshots.push_back(mat_snapshot);
                            m_mask_name.push_back(mask_names[i]);
                        }
                    }
                }
            }
        }
    }

    ~LumaFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
        m_masks.clear();
        if (m_logo) { ImGui::ImDestroyTexture(m_logo); m_logo = nullptr; }
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
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float progress = context.GetPinValue<float>(m_Pos);

        if (!mat_first.empty() && !mat_second.empty() && !m_masks.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_fusion || m_device != gpu)
            {
                if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
                m_fusion = new ImGui::Luma_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, m_masks[m_mask_index], im_RGB, progress);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        float icon_size = 48;
        float icon_gap = 8;
        ImVec2 draw_icon_size = ImVec2(icon_size, icon_size);
        int icon_number_pre_row = 4;
        int icon_count = 0;
        int _index = m_mask_index;
        
        for (auto item = m_masks.begin(); item != m_masks.end();)
        {
            auto icon_pos = ImGui::GetCursorScreenPos() + ImVec2(0, icon_gap);
            for (int i =0; i < icon_number_pre_row; i++)
            {
                ImGui::PushID(icon_count);
                auto row_icon_pos = icon_pos + ImVec2(i * (icon_size + icon_gap), 0);
                ImGui::SetCursorScreenPos(row_icon_pos);
                //ImTextureID DrawMatTexture = nullptr;
                //ImGui::ImMatToTexture(*item, DrawMatTexture, ImSize(32, 32));
                //ImGui::ImageButton("", DrawMatTexture, ImVec2(icon_size, icon_size));
                if (ImGui::Button("O", draw_icon_size))
                {
                    _index = icon_count;
                }
                if (ImGui::IsItemHovered() && ImGui::BeginTooltip())
                {
                    ImGui::Text("%s", m_mask_name[icon_count].c_str());
                    ImGui::EndTooltip();
                }
                if (icon_count == m_mask_index)
                {
                    ImGui::GetWindowDrawList()->AddRect(row_icon_pos, row_icon_pos + draw_icon_size, IM_COL32(255, 0, 0, 255));
                }
                item++;
                icon_count ++;
                ImGui::PopID();
                if (item == m_masks.end())
                    break;
            }
            if (item == m_masks.end())
                break;
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(0, icon_size));
        }
        if (_index != m_mask_index) { m_mask_index = _index; changed = true; }

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
        if (value.contains("mask_index"))
        {
            auto& val = value["mask_index"];
            if (val.is_number()) 
            {
                m_mask_index = val.get<imgui_json::number>();
                if (m_mask_index < 0 || m_mask_index > m_masks.size()) m_mask_index = 0;
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["mask_index"] = imgui_json::number(m_mask_index);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        // if show icon then we using u8"\ue87d"
        //if (!m_logo) m_logo = Node::LoadNodeLogo((void *)logo_data, logo_size);
        //Node::DrawNodeLogo(m_logo, m_logo_index, logo_cols, logo_rows, size);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond, &m_Pos}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter       = { this, "Enter" };
    FlowPin   m_Exit        = { this, "Exit" };
    MatPin    m_MatInFirst  = { this, "In 1" };
    MatPin    m_MatInSecond = { this, "In 2" };
    FloatPin  m_Pos         = { this, "Pos" };
    MatPin    m_MatOut      = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    ImGui::Luma_vulkan * m_fusion   {nullptr};
    std::vector<ImGui::ImMat> m_masks;
    std::vector<ImGui::ImMat> m_mask_snapshots;
    std::vector<std::string> m_mask_name;
    int m_mask_index {0};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(LumaFusionNode, "Luma Transform", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Fusion#Video#Move")
