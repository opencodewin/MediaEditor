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
        ImGui::ImDestroyTexture(&m_logo);
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
        ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_OpenFile_Default;
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
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\ue3b7"));
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        if (!m_logo) m_logo = Node::LoadNodeLogo((void *)logo_data, logo_size);
        Node::DrawNodeLogo(m_logo, m_logo_index, logo_cols, logo_rows, size);
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
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 5118;
    const unsigned int logo_data[5120/4] =
{
    0xe0ffd8ff, 0x464a1000, 0x01004649, 0x01000001, 0x00000100, 0x8400dbff, 0x02020300, 0x03020203, 0x04030303, 0x05040303, 0x04050508, 0x070a0504, 
    0x0c080607, 0x0b0c0c0a, 0x0d0b0b0a, 0x0d10120e, 0x0b0e110e, 0x1016100b, 0x15141311, 0x0f0c1515, 0x14161817, 0x15141218, 0x04030114, 0x05040504, 
    0x09050509, 0x0d0b0d14, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 
    0x14141414, 0x14141414, 0xc0ff1414, 0x00081100, 0x03640064, 0x02002201, 0x11030111, 0x01c4ff01, 0x010000a2, 0x01010105, 0x00010101, 0x00000000, 
    0x01000000, 0x05040302, 0x09080706, 0x00100b0a, 0x03030102, 0x05030402, 0x00040405, 0x017d0100, 0x04000302, 0x21120511, 0x13064131, 0x22076151, 
    0x81321471, 0x2308a191, 0x15c1b142, 0x24f0d152, 0x82726233, 0x17160a09, 0x251a1918, 0x29282726, 0x3635342a, 0x3a393837, 0x46454443, 0x4a494847, 
    0x56555453, 0x5a595857, 0x66656463, 0x6a696867, 0x76757473, 0x7a797877, 0x86858483, 0x8a898887, 0x95949392, 0x99989796, 0xa4a3a29a, 0xa8a7a6a5, 
    0xb3b2aaa9, 0xb7b6b5b4, 0xc2bab9b8, 0xc6c5c4c3, 0xcac9c8c7, 0xd5d4d3d2, 0xd9d8d7d6, 0xe3e2e1da, 0xe7e6e5e4, 0xf1eae9e8, 0xf5f4f3f2, 0xf9f8f7f6, 
    0x030001fa, 0x01010101, 0x01010101, 0x00000001, 0x01000000, 0x05040302, 0x09080706, 0x00110b0a, 0x04020102, 0x07040304, 0x00040405, 0x00770201, 
    0x11030201, 0x31210504, 0x51411206, 0x13716107, 0x08813222, 0xa1914214, 0x2309c1b1, 0x15f05233, 0x0ad17262, 0xe1342416, 0x1817f125, 0x27261a19, 
    0x352a2928, 0x39383736, 0x4544433a, 0x49484746, 0x5554534a, 0x59585756, 0x6564635a, 0x69686766, 0x7574736a, 0x79787776, 0x8483827a, 0x88878685, 
    0x93928a89, 0x97969594, 0xa29a9998, 0xa6a5a4a3, 0xaaa9a8a7, 0xb5b4b3b2, 0xb9b8b7b6, 0xc4c3c2ba, 0xc8c7c6c5, 0xd3d2cac9, 0xd7d6d5d4, 0xe2dad9d8, 
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xf4003f00, 0x1b69e2a8, 0xe715b825, 0x27e7171f, 0xdbbdd250, 
    0xbaaea81f, 0x7c48c4fb, 0x038d0655, 0x4b0bdb06, 0x83045621, 0xaef401c6, 0xe66ce31b, 0x4500ffd6, 0x4c2601da, 0xf115f573, 0x1ffa4e0e, 0x990f4e6f, 
    0x9fa2a15c, 0xe993bcf4, 0x8c32445d, 0x0db9e275, 0x20fde31a, 0xe5c773e5, 0x82123c5d, 0xbda86d01, 0xf529e8c9, 0x0d6dd3b1, 0x39012603, 0x552e42c5, 
    0xf5c975d8, 0xe678caa2, 0xb8dc8493, 0xb66b0c18, 0x2fac2cd0, 0x70d60ae0, 0x30baef31, 0x61e8ae49, 0x477c51a5, 0xa9125b9f, 0x48c5b974, 0x67bcfbf8, 
    0x72d5ce9a, 0xc3a91262, 0x8afb2027, 0xe17b8df4, 0x35db52fc, 0xa93c96c6, 0xdc3649d5, 0x8edfc9b2, 0x79a51fa4, 0x2d71bb76, 0xdcdab28c, 0x11f7d644, 
    0x2e323ce4, 0x1e1c7a18, 0xe21e7b70, 0x5109a5a6, 0x57e862df, 0xee5e88a7, 0x1269a1e7, 0xb74eb7fd, 0xa05cc695, 0xe6c62ace, 0x882499b7, 0x078690e1, 
    0x69f4b0de, 0x6f8bb685, 0x608f3831, 0xad623c27, 0x999edfb5, 0x721b0d46, 0xb1067919, 0x2e74af9d, 0x137fcb4d, 0xb4d4b574, 0xda142fcb, 0xb785ad8d, 
    0xcef64cbb, 0x71421f01, 0xfee535f4, 0x8f4df126, 0x753b3c83, 0xfe3b6aad, 0x48000ecb, 0x78ee2cd7, 0x3d991e55, 0xbaea15cf, 0xef348b47, 0x8bda690b, 
    0x4130d7eb, 0x6e4c3ca1, 0x6391ba24, 0xb14cc741, 0xc9a02703, 0x9e5f91e4, 0x95f134be, 0x4bbc8ecf, 0x64c8a979, 0x75a6485b, 0xfe5fedd3, 0xe5bb4158, 
    0x63c4fbe3, 0x2bfcb927, 0x6d3f9da2, 0xfb71deaf, 0x3ebd8747, 0x0e1d7adb, 0xc7cbf1a9, 0xd7fc955a, 0xdef57a16, 0x4cec6c87, 0x62525676, 0x3ef5c738, 
    0xd500ffa7, 0x72e17f55, 0x00ff437c, 0xff55d3a1, 0xc7b5bf00, 0xba6b0b9f, 0x9e163791, 0x28f5d61d, 0x852df3dc, 0xdd8aca8b, 0x613890d4, 0x4f8a7bdc, 
    0x4dfc41f8, 0x674200ff, 0x55f03f8a, 0xa557f82f, 0xb7b4e2ec, 0x797b73e2, 0xbfd86775, 0xba66ae0e, 0x4edd73f1, 0xb1c465fe, 0x35f577b9, 0xaad0f1a1, 
    0xf3265aea, 0x0f64de8f, 0xf857455e, 0xc4631479, 0xf763b449, 0x3c7e574d, 0x5adabf46, 0x2773c610, 0xb7f315f5, 0x0a7de849, 0x465e2afe, 0x49209306, 
    0x1d0327c9, 0x66f66e6b, 0x78b449fb, 0x1940369a, 0xff417083, 0x5811f500, 0x7b623c9a, 0xe73f78fb, 0x7f16a9b4, 0xad3f60de, 0x74105f76, 0x8df1b5d7, 
    0x3902e3dc, 0xe7006d76, 0xff8159fe, 0xa49ab200, 0xd2ca8dfa, 0xd324f2e5, 0x31da8ea1, 0x53cfad2c, 0xdf15e19f, 0x7dce4278, 0x3655e2a6, 0x53ddbfd0, 
    0xedf491c3, 0xa31dd7f8, 0x399686d9, 0x88991ba6, 0x80072592, 0xd31ff147, 0xd3be2bfc, 0x6f2a01f5, 0x58062c19, 0x573cdf91, 0x464b87b5, 0xedb69878, 
    0xaeb67564, 0x1120a0a8, 0x49ceb88e, 0x537cc935, 0xe36d6ff8, 0xd84b2abd, 0xdaaec30b, 0x9a2d485b, 0x21cf1722, 0x209f4849, 0xcfb1bbfc, 0x5aeb9351, 
    0x4781fa96, 0xe739c7da, 0x588f5da5, 0x6249ca36, 0x51183948, 0xedde7ac5, 0x8f2dcb45, 0x92d22905, 0x6f3c779c, 0xfc5d984c, 0x150af33d, 0x56d966dd, 
    0x868e83e0, 0x57d93fb8, 0x7fbcd65d, 0x3fdd7be1, 0x113717ec, 0x239832e9, 0x0c711dd4, 0x57e527c3, 0x4387793f, 0xf58ce38c, 0xf064a1af, 0xf3a2966d, 
    0xb063462b, 0x84a5599d, 0x1233cb39, 0x07489e5b, 0x15d38f3d, 0x033de1a9, 0xa27df845, 0x8f069ae9, 0xb8746c6c, 0x066f4114, 0x8e6a93f7, 0x2789b971, 
    0x52e535f1, 0xd3fedec1, 0x57b1876e, 0xc9de6a30, 0x7f84cf6a, 0x5c1edf69, 0x9b34be6b, 0xdb5bbec1, 0x9f1cbae9, 0xc1b871e9, 0x2068eb9a, 0x831114fd, 
    0x57fa24df, 0xe17ff023, 0x1fc4d7e5, 0x96074517, 0x13d7c656, 0x8017ccdd, 0x7a103bb9, 0xed007292, 0x5f6a4f82, 0x7eecda8f, 0xe3ebf832, 0x4943ec39, 
    0x6da8ce1d, 0x97360ea3, 0x7bc22a31, 0x396cccf1, 0xd5573cf6, 0x1b7e05df, 0xc1e3f0c5, 0xdd385ad0, 0xeef3317f, 0x860b59e4, 0x0070fa94, 0xed746000, 
    0x2874addf, 0x3ab48ac6, 0xbfcaf319, 0xd2b033bd, 0xa3653f3c, 0xc059d0e9, 0x9728da86, 0x0047140b, 0x35f5897f, 0xff56ec73, 0x6fd6f300, 0x9bd3e8fb, 
    0x4e9a2771, 0xea916b7d, 0x308f55ee, 0xc7127df8, 0xdef0e0af, 0x2afcc159, 0xb2cd8f5f, 0x9f6e40eb, 0x0a00ffbf, 0x3fb9e0a7, 0x0783aae2, 0xfd3859fe, 
    0x81f6432b, 0x7ff8d340, 0x1fe93f27, 0xb72d5ee1, 0xb16a873e, 0x324d39f4, 0x566a58e8, 0xb795a717, 0x18668eb8, 0x4f7015fe, 0xff7a0de8, 0xa478c400, 
    0xed49bdb8, 0xd9ad2956, 0x1190cbd7, 0x3c307682, 0x1ae575fe, 0x31cbba65, 0xf600c80d, 0x779c5deb, 0xb483a7d1, 0x8dce23dd, 0x652cb423, 0xc6703869, 
    0xf4731849, 0xc729ad75, 0x8aac5c9a, 0x5e53ebde, 0x8255df83, 0xb33c03c6, 0xc92910f9, 0xc7dd0e79, 0xf39afadc, 0xd4e37b9d, 0xb620fe36, 0x4f9ab1b4, 
    0xcf0b3926, 0x1061e523, 0xe09c0ba6, 0x0700d493, 0x5eeb6139, 0xe217f159, 0xcdaea9be, 0xce06b221, 0x91e2d1c2, 0x97966163, 0x39af0396, 0xf7c12824, 
    0x87bfda23, 0xf175157c, 0x47cff626, 0x2fac59d2, 0x9ebac88d, 0x45de02e0, 0xc36462b9, 0x422c8660, 0x961c0795, 0x7d20853c, 0x68841a7e, 0xd4a2cec5, 
    0x85f8faf9, 0xd903cb37, 0x7c1e9ff4, 0xadbef140, 0x6fe19306, 0xac2c5bb2, 0xd4d53d8f, 0x85b6605e, 0x770e77b7, 0x678cdb38, 0x5f73e0dc, 0xc26af846, 
    0xb4492b5d, 0x5e9c508b, 0x2d126b6a, 0xd0a6c2c5, 0x103700f2, 0x2727e3bc, 0xc153ae19, 0x3ed30b1e, 0xca86691a, 0x4cd352c6, 0x1b49568a, 0x1f003f32, 
    0xfd04ddaf, 0xa2d1ba6b, 0x8ef92e13, 0xd2e338e7, 0x53c25995, 0xcbc511f7, 0xd0dba651, 0x23bed4c5, 0xb317f669, 0x6a0096e9, 0x2b5fbc7a, 0x23b5af40, 
    0xb92b647c, 0x64d871e8, 0x6dfc88d7, 0xe07fd7f8, 0x8b1b1e89, 0x188f7beb, 0xd8ba5b5d, 0x8b8456e9, 0x20483d7d, 0x2c794c5c, 0x9c2a3fc3, 0x06f08c64, 
    0xc600ff6a, 0x0dbcd97b, 0xc100fff1, 0xcd2e45f2, 0xda065f3f, 0xf703c158, 0x8faf7cc0, 0xe967e05c, 0x86f81a5f, 0xd4e468de, 0x930c6fae, 0x3f5d5d5f, 
    0x2eb93c9d, 0x24eeb8cd, 0xe55a9f93, 0xae697293, 0x0ad1999f, 0x73b78251, 0x1f657fd5, 0xf86b7f02, 0xc159b5a5, 0x5f4bdf92, 0x4b0699dd, 0xe700d94c, 
    0x8fb90bb9, 0xc3bec2fb, 0x172ef678, 0xc0c779b5, 0x833f063f, 0x89a607bc, 0x63e90b50, 0x830767f3, 0x1f24c0bd, 0x2bfcc0a0, 0x11bb24d1, 0x43a1b292, 
    0x93e78af9, 0x768d9dbe, 0xbcf26ed8, 0xc33c8a64, 0x3551e3fd, 0xd38e65d4, 0xed93d6c7, 0xd6faeb49, 0x1f4f0d76, 0x8fa16ef0, 0x76c0591b, 0x784af10d, 
    0xd415c0eb, 0x9a817cfc, 0x60e0c3e3, 0x79bde366, 0x427dc31f, 0x692bfeed, 0x55bec9f0, 0xd892d336, 0x5a070e24, 0xee8e2ff6, 0xf886d74d, 0x962adee0, 
    0xd63370c6, 0xae1687bc, 0xd3fd9ebb, 0xcee417ab, 0xd3bf4e4f, 0xd62d35fc, 0xbf29e2a3, 0x789b74b3, 0xf939898c, 0x3b18679a, 0x380b3052, 0x35b723c1, 
    0xf12edee6, 0x7c8cdf2d, 0xd6f6764f, 0x7ec55831, 0xd1aa76a9, 0x5615ee9d, 0x923ae623, 0x17ce981b, 0x784e151d, 0xee786fc5, 0xd4cbb548, 0x86c99db7, 
    0x574466da, 0xa008a33b, 0x4ee1ec64, 0x8ee78059, 0xf38a5347, 0x7fde1daf, 0x9660fc63, 0xd126f518, 0x552412ed, 0xdd561979, 0xe5673123, 0x21f3f253, 
    0x7d4deaeb, 0x4f011b9e, 0x9e564d09, 0x4ebcf9ac, 0xae5aa532, 0x11477611, 0xb38f8be3, 0x91a6baeb, 0x82b9193c, 0x76a4b25f, 0x1849038c, 0xb01594c7, 
    0x3a0a2771, 0x01208611, 0x1afc665f, 0x2efc7af0, 0xeadc5af0, 0xa3785c24, 0x87294158, 0x61448020, 0x08190740, 0x39490ea0, 0x2baee324, 0xe133fcc3, 
    0x87f80e2f, 0xfd5a94e2, 0x3f7ceff7, 0xfe358be1, 0x54a7d4d8, 0xdadd3dfb, 0xfe25edaa, 0xbd00ffee, 0xf99ccd9d, 0xdf35677c, 0x85f822fc, 0xba46fca9, 
    0xa01ef1be, 0xdc0d4192, 0x182ceca5, 0xf2ad2c61, 0xd8512255, 0x3a6241e1, 0xb8d227b1, 0xe59d7831, 0x70619add, 0x7d50f3dc, 0x273db60e, 0x932b5ec6, 
    0xd3c48b56, 0x5ba076e9, 0x1b1bd8b2, 0x1b2e9b69, 0x40ce9d29, 0x19d3675c, 0x0f1ee815, 0xf0b35593, 0x460ca136, 0x3a6db529, 0x9009c5d0, 0x54659637, 
    0x39c3ac6d, 0x0047b9db, 0x73ea1de7, 0xf2d99f7a, 0x45a696b7, 0x38031b5e, 0xade045c5, 0x20de2256, 0x968ca589, 0x1cda66b7, 0x311e1c64, 0x4671cdeb, 
    0x995fe751, 0xfda654e9, 0xb7b6ac94, 0xf1a37ce6, 0x26d65357, 0x24e19df8, 0xa15b64d4, 0xcbb5ed15, 0x5670145e, 0x34964160, 0x38d849de, 0x73bdd7c7, 
    0x3a16fe97, 0xa073c7d7, 0x5143cc06, 0x76e8e691, 0xe377f6a6, 0xfa0040a7, 0x683fe88a, 0xfc387aad, 0xa9056d73, 0x67992641, 0x11b2e325, 0x28c07295, 
    0xcfc0e938, 0x1378df35, 0xed3fa9c1, 0x6a2de30d, 0x1cc14b58, 0xec6e4b70, 0x4633a1a4, 0xe349dbac, 0xef4f4ff8, 0x7c46d57b, 0x15a67eb1, 0x6e3515e3, 
    0x39fca79f, 0xb305d0ed, 0x70208eb7, 0xa6a87714, 0x234a32ba, 0xb7b20c8d, 0x34398a0a, 0x4e723297, 0xe045577a, 0x876419cd, 0xbec7b2ed, 0x65f75b59, 
    0x77143bd7, 0xca5a5f1f, 0x929cbafc, 0x3fccaeb5, 0x1bf543f8, 0x8a38de9f, 0x77837f12, 0x07e13f4a, 0x5ffcfebf, 0x00ffd1f7, 0xb2d5f40a, 0x3e01724c, 
    0x49ec97e6, 0x9af9d1fd, 0x5f0ef5d2, 0xbf878f68, 0xea446d67, 0x769632fe, 0xccb67c90, 0x72127a84, 0x377b953f, 0x692208c7, 0xb925383c, 0xb5f6715b, 
    0x7927e5c6, 0xa6e3ddcf, 0x7ec26b7d, 0xa73a6908, 0x1b00ffc2, 0x966ba2cd, 0xb97921a6, 0x3650e5ed, 0x9ee754b9, 0xbff60afc, 0x5f2329da, 0x4ca9c50d, 
    0x6c5ac46e, 0xb4b1dd72, 0x22945d8d, 0x8fdbdc16, 0x32e818dd, 0x72c5614f, 0x7bde3f7b, 0xf53d753b, 0xb38f6a57, 0xde3c1c3c, 0xb6d4f124, 0xc4dbc456, 
    0xcfe89226, 0x87c80c0b, 0x776c75cc, 0xd0bb5fc0, 0xd8de3567, 0xf8b335fc, 0x8e157193, 0x593469b9, 0x89d7a48c, 0x4493a41a, 0x20615dac, 0xabbc818c, 
    0x2bc01fe7, 0x75113cc9, 0x665a6b2b, 0x0d9a29bb, 0x5924e23a, 0xfd3944a6, 0xba1a2dec, 0x40dac111, 0xe4808e2d, 0xfff61a8c, 0xd7ea0e00, 0xdd90c80c, 
    0x40dcfa34, 0x4b59f3c8, 0x30ce81b1, 0xc27d0449, 0xeb1d773b, 0x4a5514ef, 0x1f7b8b1c, 0xab962a23, 0xc7f778ce, 0xeab1887f, 0xf6f0c1fa, 0x44832296, 
    0x477795b5, 0xa689406a, 0x8113b08d, 0xcf732af3, 0xcb9ee442, 0xddc0975d, 0xcfcede6a, 0x299c39fb, 0xeed9a48d, 0x7e20d63c, 0xf5c12b4c, 0xb70f00ff, 
    0xced9b786, 0x7483a7c1, 0x3220d785, 0x078e5016, 0x0f9e9151, 0x5bed0842, 0x5dfb091e, 0x0467abc6, 0x2c69cb53, 0xd37e0893, 0xa8df581f, 0xe3a7873e, 
    0xa58a195f, 0xe8737527, 0x26545570, 0x4cfbdca5, 0x471037eb, 0xcd12431b, 0xe6361c1b, 0x83411fc1, 0xf8a9b19a, 0xc26b78d8, 0xf6fc8d3a, 0x2d088296, 
    0xe39db8e6, 0x15bb8c91, 0x8c361e4b, 0x7bc633b6, 0xe3844756, 0x2e10c29a, 0xdcee85ae, 0xb910c67c, 0x979a7be0, 0x4bc2a7e2, 0x1e8c00ff, 0x4ecf8a10, 
    0x59d2e2d5, 0xc9e4c91c, 0xf2712911, 0x83d88832, 0xc9dda9f2, 0x61b5e3c1, 0x7def3641, 0x52154bcf, 0x3eb18e11, 0xeeeed333, 0xe27165fc, 0x9be74e3d, 
    0x314b41ad, 0x1e43f72d, 0x0303f600, 0x0ff6b5e8, 0xf88ba8c2, 0x24d44627, 0x695edc00, 0x8e23cdf6, 0x799588a4, 0xdb7c853f, 0x579de06b, 0xadb1c6e1, 
    0xb025fe68, 0xacf4367d, 0xd1fc1c26, 0xf3dd10cf, 0x5c86e3c4, 0x328e7a9c, 0xe0177d05, 0x37bd8bab, 0x8a1e1ec1, 0xb765ddda, 0xe1bb084b, 0x3fee468e, 
    0x2b792b02, 0x8bc9a349, 0xf39aa6e6, 0xbc416d3a, 0xc0939cb8, 0xd66b9e04, 0xb32f1bb4, 0xd38b36e9, 0x749c4bf7, 0x75095ee9, 0xddaf88e2, 0xa0091e6d, 
    0x65abca95, 0x0b660032, 0xf69a7bd4, 0xfbf81d5f, 0xe16ff84a, 0x39a97fc3, 0x04450054, 0x3b493708, 0xee4584e3, 0x2700fc78, 0x296ea9a0, 0x3ecbb1df, 
    0x3ab67b79, 0xe73be85d, 0x7a7aa3f0, 0x15f295fe, 0xf11dedd7, 0xee9d532f, 0xa287466d, 0x8a4948db, 0xb8576bce, 0x9cc55e75, 0xb04f823a, 0x7fe1bfa8, 
    0x00ff52fc, 0xff87a7a0, 0x2757f000, 0xab1d00ff, 0xa9afc8f6, 0xf8a3a14c, 0xfc157f92, 0xe245f83a, 0x5f92a05b, 0x773ccb5c, 0xf89f6313, 0xaa3f9bf6, 
    0x6a3fee8a, 0xffe9487d, 0x55670f00, 0x64e1b22f, 0xdc36f689, 0xf59c1403, 0x71c5ef19, 0x65f0029e, 0x01be86c7, 0xeaca76f8, 0x8dab3bea, 0xc495512f, 
    0xba0af0b1, 0x707a8cc9, 0xd4fed9f5, 0x10f82252, 0x797ffbc6, 0x20b9ec0c, 0xc726c27c, 0x5c57a7f3, 0xa99f7e52, 0x0d6a17a5, 0x3b3e973e, 0xc307b9b3, 
    0x45244e73, 0x15f296b9, 0x21922c8a, 0x24872484, 0x54c1b47f, 0xe973e08e, 0x2ff0bf5e, 0xbc5847c5, 0x6249ca83, 0xc92b71b9, 0xf31bd724, 0x73ea1818, 
    0xfa28b893, 0xbdf16a0f, 0x8b6f2926, 0x6e6918c1, 0xc6ef039d, 0xa40ad970, 0x8064800e, 0x7cdfb53e, 0x976b861c, 0xe73e0f4b, 0x24597647, 0xefdcf282, 
    0x63467693, 0x95ee2364, 0xdb757ade, 0x9387535f, 0xe1912fbf, 0x5968d554, 0x9da6b5f8, 0xec39fe52, 0x49f5adee, 0x3b92b223, 0x15206099, 0x18c97b3e, 
    0xd846ce64, 0xd49153c8, 0xbe86df55, 0xf0b8b91b, 0x01a4d5b1, 0xb553eb5a, 0x9cbc705b, 0xfa48ae2b, 0xab615ef3, 0x3f9eb7ea, 0x3459f3f1, 0xfc1fc5b1, 
    0x46a1fd7b, 0x6263f2c5, 0xe41ecbbb, 0x27e7490e, 0xf7f415d3, 0xbef4b482, 0xb2b74d9f, 0x252d3442, 0x20159372, 0x68b7cf5c, 0xa0aa9e2b, 0xc7937b9e, 
    0x7d5cf94a, 0x2b8e6a6e, 0xb2e97345, 0xcf5c37fa, 0x2a36e9b1, 0x17aa46b0, 0x7a9f2303, 0xcd1b34ed, 0xe9861aab, 0x2fb962b4, 0x2f4ccc2f, 0xb4b4da27, 
    0x912d9fab, 0x1848054f, 0x4e953aaf, 0x4c3d9356, 0x3fe2144c, 0x00f098f6, 0x67f083f8, 0x81487b5a, 0x779b74d7, 0x53d934d4, 0x522389ef, 0x1d7d305a, 
    0xc13d5241, 0x343a58ed, 0xdde1610b, 0x1167dc36, 0x7d80c45a, 0x5ee90f10, 0x974ea3cf, 0x805cbe56, 0x29c3b032, 0xf98a3bee, 0xb75ac65f, 0xab1d3c8a, 
    0x77f616eb, 0xadb55e11, 0xb7b688cc, 0xce6de011, 0x00390250, 0xbdce5fa9, 0xcd28fa7a, 0x994b3b1f, 0x1efb42f3, 0x0e0fe0c5, 0xb36aaf41, 0x4b3ece5d, 
    0x80b14d57, 0x72ce663e, 0x1fae987e, 0x0fb186e3, 0x9164bc88, 0x69d943c8, 0x678cb071, 0x8ee12183, 0xfddcf13f, 0xc1536b7d, 0xfced11bf, 0xdbd6f039, 
    0x3ff4bac6, 0xdbb357d9, 0x8d4983ca, 0xacccbbb7, 0xc41f5540, 0x0dec334e, 0xdd1ab478, 0x9774bff6, 0xf2b689b7, 0x2b9256f2, 0xb873ee71, 0x79feb0e7, 
    0x554e2eaf, 0x00ff6827, 0x84428f5d, 0x3de76454, 0xa1a9df96, 0x52bcc535, 0xcb03e162, 0x814c95d4, 0x7f54f8ed, 0x00ffb16c, 0x0af93f9e, 0xf8160fb8, 
    0xd7b154a3, 0xbcd3b727, 0x0b8bb696, 0xbb25c196, 0xf87facf5, 0xfd47fc4c, 0xfe213fd8, 0x15cbd715, 0xe4f988a3, 0xcff74cf5, 0x5f0df4d9, 0x65d47df6, 
    0x916c1ebb, 0x895121ea, 0x70365039, 0x5f59333f, 0xbaceccb6, 0x0ca1852f, 0xee336f44, 0x2b2ebf1e, 0xff6af657, 0xd5789300, 0xff25ec7f, 0x1febc600, 
    0x00ffdbf6, 0xbf846790, 0x00ffb3eb, 0x00ffe92a, 0x5d7f9898, 0xf0973acf, 0xc678eaa5, 0x0cc2a783, 0xaa448013, 0xc460d8f6, 0x73e59a1f, 0x0fa000ff, 
    0xb5fed6cc, 0x636fcda8, 0x7608e275, 0xeb46aac3, 0xa0a81c6f, 0x2017426c, 0xeff3c40f, 0xfa171a59, 0xdf00ff88, 0xd200ffb4, 0x0fe2bd8a, 0xbf6f12f9, 
    0x923fe9eb, 0xe147d4d7, 0x33f1773c, 0x8097e17b, 0x140f5b34, 0x5f0b93eb, 0x19ef583e, 0xf2ce7923, 0x1fd8708a, 0xbbd7fa89, 0x8040a8d9, 0xc5000600, 
    0x3f805779, 0x07f162e4, 0x00ff7efd, 0xad5eabec, 0x0a3fd16b, 0x3915d7fc, 0xfaee443c, 0x2c2cf4b3, 0xcb4ea152, 0xfe6c36a2, 0x3e00ffe9, 0x1b157295, 
    0x1c418ef3, 0xba3f4bd5, 0x5ca5cf7f, 0x1d6bad87, 0xeed4ab89, 0x4ec53c7c, 0x1aaff401, 0x410ab7f8, 0x55757ef1, 0xe06c6e1b, 0x823c4596, 0x257faedc, 
    0x277b951f, 0xe5f7fe87, 0x87f13f5e, 0x7f024bfe, 0xfa1f84d7, 0x3fbd5713, 0x17e623e0, 0x78ca99f1, 0x1ab74db3, 0x62b4b254, 0xbbda4747, 0x387500f9, 
    0x4847aef4, 0x75d0671f, 0xda7c68fd, 0x8e053084, 0x2360e746, 0xf4fc0338, 0xdfc5dfae, 0xffb803f2, 0xd2bfae00, 0xfe3f9db8, 0xff120f44, 0xff25d700, 
    0xcd6bd000, 0x3d37fcc2, 0x1c9a4e4c, 0x153c8237, 0x090de363, 0x5b464d75, 0x4796baa6, 0x689763dc, 0xde8ae9eb, 0x478500ff, 0xcf00ffa1, 0xfffbdf4b, 
    0xd600ff00, 0xff3ff8a5, 0xbf4d2200, 0xe77f75fd, 0xa876ad5d, 0xc2d1aea6, 0x3ff724e7, 0x0000d9ff, 
};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(Lut3DNode, "Lut 3D", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Color")
