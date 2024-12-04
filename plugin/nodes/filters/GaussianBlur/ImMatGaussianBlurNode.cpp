#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Gaussian_vulkan.h>

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct GaussianBlurNode final : Node
{
    BP_NODE_WITH_NAME(GaussianBlurNode, "Gaussian Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Blur")
    GaussianBlurNode(BP* blueprint): Node(blueprint) { m_Name = "Gaussian Blur"; m_HasCustomLayout = true; m_Skippable = true; }

    ~GaussianBlurNode()
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

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (m_RadiusIn.IsLinked()) m_blurRadius = context.GetPinValue<float>(m_RadiusIn);
        if (m_SigmaIn.IsLinked()) m_sigma = context.GetPinValue<float>(m_SigmaIn);
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
                m_filter = new ImGui::GaussianBlur_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_blurRadius, m_sigma);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_RadiusIn.m_ID) m_RadiusIn.SetValue(m_blurRadius);
        if (receiver.m_ID == m_SigmaIn.m_ID) m_SigmaIn.SetValue(m_sigma);
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
        float setting_offset = 320;
        if (!embedded)
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            setting_offset = sub_window_size.x - 80;
        }
        bool changed = false;
        float _sigma = m_sigma;
        int _blurRadius = m_blurRadius;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SigmaIn.IsLinked());
        ImGui::SliderFloat("Sigma##GaussianBlur", &_sigma, 0.0, 10.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_sigma##GaussianBlur")) { _sigma = 0; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_sigma##GaussianBlur", key, ImGui::ImCurveEdit::DIM_X, m_SigmaIn.IsLinked(), "sigma##GaussianBlur@" + std::to_string(m_ID), 0.f, 10.f, 0.f, m_SigmaIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_RadiusIn.IsLinked());
        ImGui::SliderInt("Radius##GaussianBlur", &_blurRadius, 0, 20, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_radius##GaussianBlur")) { _blurRadius = 3; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_radius##GaussianBlur", key, ImGui::ImCurveEdit::DIM_X, m_RadiusIn.IsLinked(), "radius##GaussianBlur@" + std::to_string(m_ID), 0.f, 20.f, 3.f, m_RadiusIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_sigma != m_sigma) { m_sigma = _sigma; changed = true; }
        if (_blurRadius != m_blurRadius) { m_blurRadius = _blurRadius; changed = true; }
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
        if (value.contains("radius"))
        {
            auto& val = value["radius"];
            if (val.is_number()) 
                m_blurRadius = val.get<imgui_json::number>();
        }

        if (value.contains("sigma"))
        {
            auto& val = value["sigma"];
            if (val.is_number()) 
                m_sigma = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["radius"] = imgui_json::number(m_blurRadius);
        value["sigma"] = imgui_json::number(m_sigma);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\uf404"));
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
    FloatPin  m_RadiusIn = { this, "Radius"};
    FloatPin  m_SigmaIn = { this, "Sigma"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatIn, &m_RadiusIn, &m_SigmaIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    int m_blurRadius        {3};
    float m_sigma           {0.0f};
    ImGui::GaussianBlur_vulkan * m_filter   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 3947;
    const unsigned int logo_data[3948/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xf6003f00, 0x5b692238, 0x89573c71, 0xb69c78fc, 0x35f46fa9, 
    0x8d873ae8, 0xfb97f810, 0x82d1dd36, 0x836b27a9, 0xffccabfc, 0x7c296800, 0x0fd4528d, 0x0e6ffc4a, 0xfb91ede5, 0xb6eea174, 0xcec34fce, 0x5d33d27e, 
    0x02c3b4dd, 0xb7c3efbc, 0x8a04bc1f, 0x42ee6ded, 0x1fc096c7, 0x7e27f45a, 0x297a336b, 0x6c469b72, 0x6b1fd40c, 0x7429fd0c, 0x25eea8bb, 0xad8f0019, 
    0x7482ef7a, 0x8457373d, 0x3da1db23, 0xc1d376cd, 0x31bfaa4a, 0x28b3e2e5, 0x5bf99561, 0x1c77863c, 0x46f5b162, 0x7590e321, 0x13fa9a53, 0x23065fc6, 
    0x1a37b2b8, 0xa39a3f5a, 0xcf1622ee, 0x77bec27e, 0x3a3784f1, 0x9525a506, 0x880c4fe4, 0x952271e5, 0xe734f027, 0x7787a984, 0xcc8781b6, 0x7bd8f830, 
    0xb0c75e8f, 0x5fe04ef8, 0xfdb3f068, 0x6db1b5e0, 0xec6dda76, 0xe37ec673, 0x852f5c03, 0xa1c77e6f, 0x13363345, 0x2b70921c, 0xb75fb54d, 0x0c27ba45, 
    0xd79cba87, 0x49295a91, 0x954a1dc9, 0xfcd199d5, 0xf8d2f06e, 0xf1b64383, 0x8a04b61d, 0x2b1e90b8, 0x1300ffe6, 0xc2dfbaf8, 0x32974cba, 0xca07cc82, 
    0x425fef00, 0xb2497c59, 0x358677f0, 0x2289f528, 0xd8c41bfb, 0x1954a559, 0x5744fbc7, 0x3fc5c7e6, 0xcfe2c918, 0x0b49df16, 0x9fe66993, 0x0cb71529, 
    0x9cbdda67, 0x3bf10cbf, 0xc4988fec, 0xe55e9e62, 0x36f4da4a, 0xbe8f5f35, 0x4df97b26, 0x01c1e3a5, 0xd2032a3f, 0xfac27faa, 0x04fd7ff1, 0xbe00ffa4, 
    0xc2878eab, 0x62ecbd7a, 0x642eac58, 0x0796d143, 0xff14f423, 0x3542f800, 0xdd06facf, 0x3be000ff, 0x7d8500ff, 0x6949a162, 0xc5eb7c64, 0xccfc6ed7, 
    0x4ee107fb, 0xf8a871a3, 0x878b5b9e, 0x6423232f, 0x1f74a59f, 0xd56d43b4, 0x58e933ad, 0x43e008bf, 0xf00736af, 0x1f5de51f, 0xab6e21ed, 0x769c59aa, 
    0xcfaff6f5, 0x582596ee, 0x925f29fd, 0x00fff3cc, 0xa2d39d0e, 0x8dbaa68e, 0xa2bcbd40, 0x5cebc113, 0x136382f7, 0xff9056eb, 0xae807900, 0x4c3ac7d7, 
    0x5cb47eba, 0x8142daed, 0xbac69e83, 0x845a4e1b, 0x8db8d764, 0x62d5061d, 0x1ecb6355, 0xe1cbbd82, 0x913a9376, 0x8ccba334, 0x8fbc06f4, 0x1e4ab6c2, 
    0xc6192529, 0xb7d70008, 0x48587778, 0x462e90ed, 0x587ba507, 0x7b3ed36d, 0xe5705232, 0x3cbaec89, 0x2e8046b1, 0xaa00ff07, 0xd7e3cfbc, 0x1f3f6bc1, 
    0x766a53e9, 0x08fe12ca, 0xfba87c64, 0xf52321c4, 0xbee969c5, 0x37cef227, 0xb6e6fb1f, 0x3050f1a2, 0x074b4eb4, 0xd80b481b, 0x74c6d3d7, 0x7b96c3ea, 
    0xd6e9061f, 0x205554c3, 0x0479877c, 0xef0e1ff6, 0x27599521, 0x7cc70885, 0x9afdc18a, 0x8d17f160, 0x6ba06f6d, 0x31304c39, 0x820323db, 0xdfcbea2b, 
    0x231ef684, 0x62f14d9a, 0x3c596ed6, 0x51d704e6, 0xc307ede1, 0xa7b40abf, 0xedddca82, 0x0e2761d7, 0x738f67f2, 0x2d1f065e, 0x5be3b98c, 0x4e7de86b, 
    0x79943c27, 0xdbd58725, 0xf6dbf253, 0xfbcdf1aa, 0x0f6f9ef8, 0x6c45d01d, 0x26d3eb70, 0xff822fb8, 0x7c2e0d00, 0x8028e275, 0x60649b8c, 0x6ec5b159, 
    0x8a1ad27e, 0x8ee3e378, 0xc4812ca8, 0xc61b9493, 0x4ff5f5de, 0xc2f0fdec, 0x6278060f, 0xfb351e19, 0x99198a4c, 0xe8156665, 0x52c130ca, 0xb079a6e5, 
    0xafc64c9d, 0x3e76aab5, 0xa571f81e, 0x16443a69, 0x453edda2, 0xff68edfe, 0xa515c200, 0x7fbacf7f, 0x73b435df, 0x3ae03547, 0xd7b3bb8d, 0xb1b4e254, 
    0x38c2f7f3, 0xc3712d56, 0x957fd875, 0xe3d27e6e, 0xf691fd6d, 0xbfb9ca3f, 0xf89fb386, 0xdc1f0849, 0x4f5de56f, 0xeb1e31ed, 0x1ffa1ccb, 0x2b0f5ee5, 
    0xbd3ecafa, 0x6d1e45bb, 0xb3b56be0, 0x339f1ef1, 0x6690c5fd, 0x6ce2f3bd, 0x05697709, 0x6e0c4ae4, 0x377d03ca, 0x3f5fe90f, 0x7cd77e78, 0xc85147c8, 
    0x7cd64eaf, 0x8bf0bf60, 0x204bd9db, 0x8e9b0c53, 0x3f963c4f, 0x2a7535fb, 0x4ba2d25e, 0xd98f1a73, 0x12e83bb5, 0x220d8ef8, 0x509acddf, 0xafb9a38a, 
    0xed07f134, 0xacae2513, 0x7ccad9b0, 0x3c2738b5, 0x45fc8f57, 0xac7ba1f8, 0x39303c5e, 0x00e31048, 0x3efca6f5, 0xc4df15f0, 0xd6de569d, 0x60711bdd, 
    0x2be8c05d, 0x0e162cf4, 0xd21e1a9e, 0x7cf8d4aa, 0x4feb6276, 0x803ed1d9, 0x5fc46ff0, 0xd641fc19, 0xcbd1b3a1, 0x320f8b08, 0x2bf24f4f, 0xba87bfec, 
    0x875f9a54, 0xfea486ac, 0x9400ff6d, 0xbd7f5286, 0xbcd69f81, 0x806fe19f, 0x9a84bfb4, 0x4fa48d79, 0x0c864db5, 0x07ba9fc7, 0xf076aff4, 0x5ba9c17a, 
    0x5891a489, 0xed39083e, 0x4aad8951, 0x8fa61255, 0x1231343e, 0x879e9483, 0x8d4fe22f, 0xd4151ab6, 0x259e6d9a, 0x3d6d1cbf, 0xda5fe5ab, 0xb884f613, 
    0x8eb920d3, 0xf425c16b, 0x8d6255ca, 0xfbbb4ab8, 0x051fdc51, 0x2c4954fc, 0x542e1ba6, 0x6be869e7, 0x4b19afe2, 0x8ef8ab79, 0xf348dce0, 0x1bb8244b, 
    0x36d67b8e, 0x7a921a75, 0xa171ba21, 0xcf6a1b17, 0x3c99fd59, 0x7c8c2719, 0x76af2669, 0xbf0f60be, 0xaf39cf2d, 0xf6c9feb9, 0x60101558, 0xe315c728, 
    0x814fb35f, 0x19e100ff, 0xc9bd9df0, 0x86224b4c, 0x5cb2573c, 0x1c4e216a, 0x4d7cbc62, 0xdd52694f, 0x950e4f8f, 0xc8805828, 0xcd93e254, 0x464d5a35, 
    0x4d91e712, 0xfb87d0fe, 0x57f9b8c2, 0xf998af63, 0xa8a6e1c7, 0x112ce13f, 0xdbee0f82, 0xcff41af9, 0x9ed556da, 0xd41dc3ee, 0xf92a00ff, 0xf505e1fb, 
    0xc339becc, 0x3fe57116, 0xa3afa1af, 0x9e8f683f, 0xe0664eef, 0xcc2bfd01, 0xc4514aab, 0xd2a72e45, 0x46aaa852, 0x0eaff2ec, 0x79f8dadd, 0x6f53e7de, 
    0x905f352e, 0x9fd7787f, 0x52c6cb78, 0x12c483f8, 0x28a26ec5, 0x24875519, 0xebeb171e, 0xa957bc57, 0x38f6421d, 0xa746a404, 0x35b8551b, 0x5b279ee6, 
    0x9f142fcb, 0xb628de29, 0x1f08100c, 0xc14dbf76, 0x547034e5, 0xaca626d6, 0x34c30cf8, 0xe8aa88ad, 0x9923d941, 0x7fa6add6, 0x5081fd10, 0x57fbaeb4, 
    0x3feeeb1d, 0xb60cfe80, 0xa5e067f8, 0x9af998bb, 0x4879ccc5, 0x3e320e98, 0xe76bfd99, 0x63f80a8f, 0x00f1b84e, 0x43a16fd7, 0xd9bb666f, 0xb06b03a5, 
    0xab573cf7, 0xfdf133fc, 0xe9b58be7, 0xd8cf9165, 0xc2b06d63, 0x7aa0281b, 0x3223af76, 0x4baefbc5, 0x4e199d6b, 0xa9ae5605, 0xd0e4ebc8, 0x9de2593c, 
    0x419d6b46, 0x0251acc5, 0xe7971163, 0xf5b17b92, 0x4ef05eaf, 0xbe69aba9, 0x1082bd05, 0x85b5ba25, 0xe4164bd1, 0x7d3d3885, 0x851aa540, 0x2b65d1b8, 
    0x3a665e04, 0x53d03380, 0xa7ab093c, 0x2ac7ac91, 0x861f8c80, 0x83d73bc6, 0xd194f10a, 0x7e3075f6, 0xf9d28cca, 0x587c121f, 0x4f9d35d6, 0xd4a815e2, 
    0xd72f66c8, 0x93f9712c, 0x868f8d5c, 0xa5f85a9b, 0xdbb9666f, 0x153806e6, 0xd6757cd9, 0x44e2df6d, 0x1e236e29, 0x23949c5c, 0x0fbbdad7, 0x4b0cfe83, 
    0xa96f88ef, 0x08f416cb, 0x47a65819, 0x6ba45ee7, 0x3e725172, 0x3f461553, 0xba8be558, 0x1f5ed247, 0x48175db6, 0xc253b6b6, 0x932c1520, 0x933f7d49, 
    0x87647e04, 0xf28a0a8c, 0x710a835d, 0xd32b308e, 0x32f8133e, 0xed45a82b, 0x634722c4, 0x2bce74c1, 0x5255a9c0, 0xcdeed15e, 0x0a47d266, 0x6bf50c9f, 
    0xd99512b5, 0xfe3bb5bb, 0x5fad5615, 0xebe1474c, 0xa53c4075, 0x771af01f, 0xfcbf05f6, 0x2f951ff3, 0x233be2ac, 0xeed0f6cd, 0xccfe4b7e, 0xf15343f7, 
    0x3f3fb234, 0x0a7f6ebb, 0x8cf63ffa, 0x16d91489, 0xfe84006c, 0xc067f395, 0xf8501bad, 0xb5a7e27d, 0x3a2021d4, 0x5ec7d636, 0xfb117d0d, 0x4874e14f, 
    0x2390d8ee, 0x90fc4682, 0xd65be99f, 0x18ddfea5, 0x3e7cfa58, 0xd595fd23, 0x242fca67, 0x5a8abfba, 0x4e015b08, 0xbcd22b72, 0x3c18e02f, 0x6ab17a51, 
    0x66455538, 0xa7e37b2e, 0xf018aff3, 0xb646a2a6, 0xa3f6a5b7, 0x5aeeb51e, 0x0b35ac2e, 0xb8bc1513, 0x4fbd939f, 0x72fd8a6c, 0x236c159c, 0xe6a79ea6, 
    0x88a3acb2, 0x79668ff6, 0x1cafc64f, 0x34393542, 0xac88245d, 0x9d6ded90, 0x0bf8a7eb, 0x1d7b4be2, 0x5338f296, 0x5ee235db, 0xd366d230, 0x66d64935, 
    0x3b799367, 0xe0bf7a8f, 0x5697ba36, 0xa39d48b7, 0x57b801cb, 0x33ecb8e7, 0xf6063795, 0xc529af3e, 0x6a4c0cfb, 0xc4f7b925, 0x5b98205e, 0x6abe90af, 
    0x157e0437, 0x7f3c893f, 0x177c851f, 0x24903479, 0x8dbc3d52, 0x6127a1bb, 0x347ccf35, 0x972d6d49, 0x8ce72fed, 0x8aeebf8e, 0x7c257eea, 0xb5f8bd13, 
    0x74ba70e1, 0xb81006e2, 0x7800ff11, 0x3a7f2094, 0x5224fcf9, 0xef3e9b9d, 0x34c2c433, 0x3f75b91c, 0xa6d5663f, 0x2e8837f1, 0x6796dcef, 0xad9f2093, 
    0x57f0b57d, 0x52f8114e, 0xde8fe8d6, 0xf99a0c3a, 0x53f85663, 0xd52d7cac, 0xadd6b1a5, 0x0a9b1f5a, 0xe96b70e4, 0x576a01bf, 0x6c84677a, 0x88ef3184, 
    0x165faf20, 0xc247e5b4, 0x79949b61, 0xd9ed9dfa, 0x3d297a3e, 0xb3fa0a58, 0x2d90d6e1, 0xd868337c, 0xf81a50c6, 0x59f14ebb, 0x3bcb35d5, 0x8c958566, 
    0xeb3d1e80, 0x1f884fea, 0x00ffb417, 0x8200be84, 0xfe79d879, 0xeee3f248, 0xaec62b4e, 0xd415a5a2, 0x3fa9a3ce, 0xefabc776, 0x1ecd788d, 0xbfa27e64, 
    0xdb9fb535, 0x9a37c743, 0x28daf294, 0xc8b7dc8a, 0x7fa9d2a7, 0x427c63c3, 0x55fef2fe, 0x233fcbd3, 0xc848a80f, 0x7ca87ef0, 0x07ade14d, 0x5c439555, 
    0xef77303a, 0xfdbb6bf7, 0xdfda6eb0, 0x742af1c1, 0xff73796b, 0xb99a7e00, 0x15fe832f, 0x7a873f7b, 0x46e26c74, 0xfac88889, 0x61fb4b57, 0x125e2cdc, 
    0x5ac646b6, 0xfe674cde, 0xe275b5b9, 0xd8a9ccf5, 0xac49c1ed, 0xecc94805, 0x7a63377c, 0x838569d0, 0x2bf0e09d, 0xf80d3ed6, 0x1574cf80, 0x93cb07db, 
    0x7845fd24, 0x3a2f8cd5, 0xd7245514, 0x1e387ca0, 0xb90c8d4c, 0xdcb1c285, 0x43bfa23e, 0x76c6cdc1, 0x55557ce8, 0xe37fa175, 0xea13b40d, 0x6665e065, 
    0xbe57711d, 0x4652f817, 0xd7ed86b4, 0xaf6d9096, 0xb9bebe3d, 0xbe8b17f1, 0x57ecb8cf, 0xbed223c1, 0xa273f897, 0x15d2a634, 0x80d0b642, 0x5f534772, 
    0x6d629c15, 0x73e91827, 0x271c72ee, 0x969c92b5, 0x6278b0c7, 0x445bb23f, 0x217f7a18, 0x1de0d55e, 0x745bcb63, 0x1895810d, 0x247945fc, 0x05288f30, 
    0x85475ded, 0x346d13b5, 0x65084e4a, 0x2abe62fe, 0x6a595e95, 0xf471d0cf, 0x47364a15, 0x7c50fb77, 0x87f8b325, 0x61efe1f0, 0x6fdf7e81, 0xc9515787, 
    0xc5f009af, 0x865ff6a7, 0x7e9cad6d, 0x45da34f2, 0xdca08f7d, 0x6af8ae47, 0xefc3577b, 0xa51f8c8c, 0x577c7d7e, 0x04df35d6, 0xb651c3f8, 0x25d125b7, 
    0x35db8f38, 0x9593d5f5, 0x7e6c4938, 0x94f04a67, 0xf00d7de2, 0x5af6c053, 0xf7f7b6fe, 0x9d5b5441, 0xfff21ac0, 0xc59fda00, 0x162fe289, 0xcd6481fd, 
    0x14caafad, 0xcfad381e, 0x8354fc01, 0xe50e3fc2, 0x1065efd6, 0x5119cbde, 0x8a933c00, 0xf157a7f9, 0x358bcf5d, 0x89e99bbb, 0x00962bdd, 0x53cf57fa, 
    0x97f84a85, 0x5a3db225, 0xa7f7903c, 0xed5d41db, 0x5254b0d1, 0xdddfb679, 0xde6bc115, 0x52db9a24, 0x6dd83892, 0x00ffcf5a, 0xfbebae84, 0x5085bec2, 
    0xaf1cd556, 0x3ea3ef14, 0xa396fdb4, 0xe3c29f1a, 0x1defc725, 0x5c130064, 0x3bb7edf7, 0x20a69fae, 0xf1135338, 0x350000ff, 0x5fc9fed1, 0xfe5f4af2, 
    0x7fe68aba, 0xf4f8df6d, 0xeb00ffd2, 0xe800ff93, 0xffadb306, 0x671a2300, 0xc2fd1726, 0x87c7f2a1, 0x1b0d63ad, 0x2bbae66d, 0xa02bf6b3, 0x8ca4fcf0, 
    0xfa871f56, 0xde56f847, 0x7fb9feab, 0xf46be40f, 0x6315e918, 0x36bba5e4, 0x7678197e, 0x697e7dc6, 0xee7b8b64, 0x2b7d4dce, 0x6d475ba2, 0x104a2c1a, 
    0xf97cb563, 0x43feb3f0, 0xe86bfd32, 0xafee2f7d, 0xcd19bfd2, 0x9a8b27e7, 0x7efba96f, 0x1cac0845, 0xb70e5d1a, 0x5b73e44d, 0x75162816, 0x7fe46020, 
    0x3ad3c73a, 0xfa9fd556, 0xe08afae5, 0xc4d6438f, 0xf871cfec, 0x5185336f, 0xf215e3b8, 0x6923eda7, 0x25167f14, 0x320e8350, 0xf0517d45, 0xcb57a2e3, 
    0xfcafb4bf, 0x57faa695, 0x43f72fdb, 0x00ffc9f2, 0xfccab3bd, 0xd89f6e65, 0xbdda79e5, 0x03573c17, 0x8f168b60, 0xf7a3727d, 0xd207a7a8, 0x7fc60fbd, 
    0xf36a08f2, 0x17f94feb, 0x4df71f35, 0xc4fa6572, 0x1a0db3f4, 0xda76e7b1, 0xbc925a7c, 0x1969eef3, 0x6f6a4e8e, 0xee4f1bec, 0xd14fe7b7, 0xd0e300ff, 
    0x6abe00ff, 0x3cd77af5, 0x00d9ff33, 
};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(GaussianBlurNode, "Gaussian Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Blur")
