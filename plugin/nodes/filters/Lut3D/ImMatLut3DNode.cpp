#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Lut3D_vulkan.h>
#include <ImGuiFileDialog.h>

#define SDR709_HDR2020_HLG_SIZE 33
#define SDR709_HDR2020_HLG_R_SCALE 1.000000
#define SDR709_HDR2020_HLG_G_SCALE 1.000000
#define SDR709_HDR2020_HLG_B_SCALE 1.000000
#define SDR709_HDR2020_HLG_A_SCALE 1.000000
extern const rgbvec sdr709_hdr2020_hlg_lut[];

#define SDR709_HDR2020_PQ_SIZE 33
#define SDR709_HDR2020_PQ_R_SCALE 1.000000
#define SDR709_HDR2020_PQ_G_SCALE 1.000000
#define SDR709_HDR2020_PQ_B_SCALE 1.000000
#define SDR709_HDR2020_PQ_A_SCALE 1.000000
extern const rgbvec sdr709_hdr2020_pq_lut[];

#define HDR2020_HLG_SDR709_SIZE 33
#define HDR2020_HLG_SDR709_R_SCALE 1.000000
#define HDR2020_HLG_SDR709_G_SCALE 1.000000
#define HDR2020_HLG_SDR709_B_SCALE 1.000000
#define HDR2020_HLG_SDR709_A_SCALE 0.000000
extern const rgbvec hdr2020_hlg_sdr709_lut[];

#define HDR2020_PQ_SDR709_SIZE 33
#define HDR2020_PQ_SDR709_R_SCALE 1.000000
#define HDR2020_PQ_SDR709_G_SCALE 1.000000
#define HDR2020_PQ_SDR709_B_SCALE 1.000000
#define HDR2020_PQ_SDR709_A_SCALE 0.000000
extern const rgbvec hdr2020_pq_sdr709_lut[];

#define HDR2020HLG_HDR2020PQ_SIZE 33
#define HDR2020HLG_HDR2020PQ_R_SCALE 1.000000
#define HDR2020HLG_HDR2020PQ_G_SCALE 1.000000
#define HDR2020HLG_HDR2020PQ_B_SCALE 1.000000
#define HDR2020HLG_HDR2020PQ_A_SCALE 0.000000
extern const rgbvec hdr2020hlg_hdr2020pq_lut[];

#define HDR2020PQ_HDR2020HLG_SIZE 33
#define HDR2020PQ_HDR2020HLG_R_SCALE 1.000000
#define HDR2020PQ_HDR2020HLG_G_SCALE 1.000000
#define HDR2020PQ_HDR2020HLG_B_SCALE 1.000000
#define HDR2020PQ_HDR2020HLG_A_SCALE 0.000000
extern const rgbvec hdr2020pq_hdr2020hlg_lut[];

enum default_lut : int {
    SDR709_HDRHLG = 0,
    SDR709_HDRPQ,
    HDRHLG_SDR709,
    HDRPQ_SDR709,
    HDRHLG_HDRPQ,
    HDRPQ_HDRHLG,
    NO_DEFAULT,
};

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct Lut3DNode final : Node
{
    BP_NODE_WITH_NAME(Lut3DNode, "Lut 3D", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Color")
    Lut3DNode(BP* blueprint): Node(blueprint) { m_Name = "Lut 3D"; m_HasCustomLayout = true; m_Skippable = true; }
    ~Lut3DNode()
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

    rgbvec * get_default_lut_param(int model, int& lutsize, float& scale_r, float& scale_g, float& scale_b, float& scale_a)
    {
        rgbvec* lut = nullptr;
        switch (model)
        {
            case SDR709_HDRHLG:
                lutsize = SDR709_HDR2020_HLG_SIZE;
                lut = (rgbvec*)sdr709_hdr2020_hlg_lut;
                scale_r = SDR709_HDR2020_HLG_R_SCALE;
                scale_g = SDR709_HDR2020_HLG_G_SCALE;
                scale_b = SDR709_HDR2020_HLG_B_SCALE;
                scale_a = SDR709_HDR2020_HLG_A_SCALE;
                break;
            case SDR709_HDRPQ:
                lutsize = SDR709_HDR2020_PQ_SIZE;
                lut = (rgbvec*)sdr709_hdr2020_pq_lut;
                scale_r = SDR709_HDR2020_PQ_R_SCALE;
                scale_g = SDR709_HDR2020_PQ_G_SCALE;
                scale_b = SDR709_HDR2020_PQ_B_SCALE;
                scale_a = SDR709_HDR2020_PQ_A_SCALE;
                break;
            case HDRHLG_SDR709:
                lutsize = HDR2020_HLG_SDR709_SIZE;
                lut = (rgbvec*)hdr2020_hlg_sdr709_lut;
                scale_r = HDR2020_HLG_SDR709_R_SCALE;
                scale_g = HDR2020_HLG_SDR709_G_SCALE;
                scale_b = HDR2020_HLG_SDR709_B_SCALE;
                scale_a = HDR2020_HLG_SDR709_A_SCALE;
                break;
            case HDRPQ_SDR709:
                lutsize = HDR2020_PQ_SDR709_SIZE;
                lut = (rgbvec*)hdr2020_pq_sdr709_lut;
                scale_r = HDR2020_PQ_SDR709_R_SCALE;
                scale_g = HDR2020_PQ_SDR709_G_SCALE;
                scale_b = HDR2020_PQ_SDR709_B_SCALE;
                scale_a = HDR2020_PQ_SDR709_A_SCALE;
                break;
            case HDRHLG_HDRPQ:
                lutsize = HDR2020HLG_HDR2020PQ_SIZE;
                lut = (rgbvec*)hdr2020hlg_hdr2020pq_lut;
                scale_r = HDR2020HLG_HDR2020PQ_R_SCALE;
                scale_g = HDR2020HLG_HDR2020PQ_G_SCALE;
                scale_b = HDR2020HLG_HDR2020PQ_B_SCALE;
                scale_a = HDR2020HLG_HDR2020PQ_A_SCALE;
                break;
            case HDRPQ_HDRHLG:
                lutsize = HDR2020PQ_HDR2020HLG_SIZE;
                lut = (rgbvec*)hdr2020pq_hdr2020hlg_lut;
                scale_r = HDR2020PQ_HDR2020HLG_R_SCALE;
                scale_g = HDR2020PQ_HDR2020HLG_G_SCALE;
                scale_b = HDR2020PQ_HDR2020HLG_B_SCALE;
                scale_a = HDR2020PQ_HDR2020HLG_A_SCALE;
                break;
            default:
                break;
        }
        return lut;
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
            if (!m_filter || gpu != m_device || m_setting_changed)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                if (m_lut_mode != NO_DEFAULT)
                {
                    int size = 0;
                    float scale_r, scale_g, scale_b,scale_a;
                    rgbvec * lut_data = get_default_lut_param(m_lut_mode, size, scale_r, scale_g, scale_b, scale_a);
                    m_filter = new ImGui::LUT3D_vulkan((void *)lut_data, size, scale_r, scale_g, scale_b, scale_a, m_interpolation_mode, gpu);
                }
                else if (!m_path.empty())
                    m_filter = new ImGui::LUT3D_vulkan(m_path, m_interpolation_mode, gpu);
                m_setting_changed = false;
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            bool is_hdr_pq = (m_lut_mode == SDR709_HDRPQ) || (m_lut_mode == HDRHLG_HDRPQ);
            bool is_hdr_hlg =(m_lut_mode == SDR709_HDRHLG) || (m_lut_mode == HDRPQ_HDRHLG);
            bool is_sdr_709 =(m_lut_mode == HDRHLG_SDR709) || (m_lut_mode == HDRPQ_SDR709);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            im_RGB.color_space = (is_hdr_pq || is_hdr_hlg) ? IM_CS_BT2020 : 
                                is_sdr_709 ? IM_CS_BT709 : IM_CS_SRGB; // 601?
            if (is_hdr_pq) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_FRAME | IM_MAT_FLAGS_VIDEO_HDR_PQ;
            if (is_hdr_hlg) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_FRAME | IM_MAT_FLAGS_VIDEO_HDR_HLG;
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
        // Draw custom layout
        int lut_mode = m_lut_mode;
        bool changed = false;
        ImGui::TextUnformatted("Lut Mode:");
        ImGui::RadioButton("SDR->HLG",  (int *)&lut_mode, SDR709_HDRHLG);
        ImGui::RadioButton("SDR->PQ",   (int *)&lut_mode, SDR709_HDRPQ);
        ImGui::RadioButton("HLG->SDR",  (int *)&lut_mode, HDRHLG_SDR709);
        ImGui::RadioButton("PQ->SDR",   (int *)&lut_mode, HDRPQ_SDR709);
        ImGui::RadioButton("HLG->PQ",   (int *)&lut_mode, HDRHLG_HDRPQ);
        ImGui::RadioButton("PQ->HLG",   (int *)&lut_mode, HDRPQ_HDRHLG);
        ImGui::RadioButton("File",      (int *)&lut_mode, NO_DEFAULT);
        if (!embedded) ImGui::Separator();
        ImGui::TextUnformatted("Interpolation:");
        ImGui::RadioButton("Nearest",       (int *)&m_interpolation_mode, IM_INTERPOLATE_NEAREST);
        ImGui::RadioButton("Trilinear",     (int *)&m_interpolation_mode, IM_INTERPOLATE_TRILINEAR);
        ImGui::RadioButton("Teteahedral",   (int *)&m_interpolation_mode, IM_INTERPOLATE_TETRAHEDRAL);
        if (!embedded) ImGui::Separator();
        if (m_lut_mode != lut_mode)
        {
            m_lut_mode = lut_mode;
            m_setting_changed = true;
            changed |= m_setting_changed;
        }
        // open from file
        ImVec2 minSize = ImVec2(400, 300);
		ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
        if (m_lut_mode == NO_DEFAULT) ImGui::BeginDisabled(false);  else ImGui::BeginDisabled(true);
        static string filters = ".cube";
        ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_DontShowHiddenFiles | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_Modal;
        vflags |= ImGuiFileDialogFlags_DontShowHiddenFiles;
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose File ## "))
        {
            IGFD::FileDialogConfig config;
            config.path = m_path.empty() ? "." : m_path;
            config.countSelectionMax = 1;
            config.userDatas = this;
            config.flags = vflags;
            ImGuiFileDialog::Instance()->OpenDialog("##NodeChooseLutFileDlgKey", "Choose File", 
                                                    filters.c_str(), 
                                                    config);
        }
        if (embedded) ed::Suspend();
        if (ImGuiFileDialog::Instance()->Display("##NodeChooseLutFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
        {
	        // action if OK
            if (ImGuiFileDialog::Instance()->IsOk() == true)
            {
                m_path = ImGuiFileDialog::Instance()->GetFilePathName();
                m_file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                m_setting_changed = true;
                changed |= m_setting_changed;
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        if (embedded)  ed::Resume();
        if (m_lut_mode != NO_DEFAULT)
        {
            m_path.clear();
            m_file_name.clear();
        }
        ImGui::SameLine(0);
        if (!m_file_name.empty())
            ImGui::TextUnformatted(m_file_name.c_str());
        else
            ImGui::TextUnformatted("Noselected file");
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
        if (value.contains("lut_mode"))
        {
            auto& val = value["lut_mode"];
            if (val.is_number()) 
                m_lut_mode = val.get<imgui_json::number>();
        }
        if (value.contains("interpolation"))
        {
            auto& val = value["interpolation"];
            if (val.is_number()) 
                m_interpolation_mode = val.get<imgui_json::number>();
        }
        if (value.contains("lut_file_path"))
        {
            auto& val = value["lut_file_path"];
            if (val.is_string())
            {
                m_path = val.get<imgui_json::string>();
            }
        }
        if (value.contains("lut_file_name"))
        {
            auto& val = value["lut_file_name"];
            if (val.is_string())
            {
                m_file_name = val.get<imgui_json::string>();
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["lut_mode"] = imgui_json::number(m_lut_mode);
        value["interpolation"] = imgui_json::number(m_interpolation_mode);
        value["lut_file_path"] = m_path;
        value["lut_file_name"] = m_file_name;
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue3b7"));
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
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    int m_lut_mode {SDR709_HDRHLG};
    int m_interpolation_mode {IM_INTERPOLATE_TRILINEAR};
    string  m_path;
    string m_file_name;
    ImGui::LUT3D_vulkan * m_filter {nullptr};
    bool  m_setting_changed {false};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(Lut3DNode, "Lut 3D", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Color")
