#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Box_vulkan.h>

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct BoxBlurNode final : Node
{
    BP_NODE_WITH_NAME(BoxBlurNode, "Box Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Blur")
    BoxBlurNode(BP* blueprint): Node(blueprint) { m_Name = "Box Blur"; m_HasCustomLayout = true; m_Skippable = true; }

    ~BoxBlurNode()
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
        if (m_SizeIn.IsLinked()) m_Size = context.GetPinValue<float>(m_SizeIn);
        if (m_IterationIn.IsLinked()) m_iteration = context.GetPinValue<float>(m_IterationIn);
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
                m_filter = new ImGui::BoxBlur_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_Size, m_Size);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            double node_time = 0;
            node_time += m_filter->filter(mat_in, im_RGB);
            for (int i = 1; i < m_iteration; i++)
            {
                node_time += m_filter->filter(im_RGB, im_RGB);
            }
            m_NodeTimeMs = node_time;
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_SizeIn.m_ID) m_SizeIn.SetValue(m_Size);
        if (receiver.m_ID == m_IterationIn.m_ID) m_IterationIn.SetValue(m_iteration);
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origi, ImGui::ImCurveEdit::Curve * key, bool embedded) override
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
        int _Size = m_Size;
        int _iteration = m_iteration;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SizeIn.IsLinked());
        ImGui::SliderInt("Size##Box", &_Size, 1, 20, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_size##Box")) { _Size = 3; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_size##Box", key, ImGui::ImCurveEdit::DIM_X, m_SizeIn.IsLinked(), "size##Box@" + std::to_string(m_ID), 1.f, 20.f, 3.f, m_SizeIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_IterationIn.IsLinked());
        ImGui::SliderInt("Iteration##Box", &_iteration, 1, 20, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_iteration##Box")) { _iteration = 1; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_iteration##Box", key, ImGui::ImCurveEdit::DIM_X, m_IterationIn.IsLinked(), "iteration##Box@" + std::to_string(m_ID), 1.f, 20.f, 1.f, m_IterationIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_Size != m_Size) { m_Size = _Size; changed = true; }
        if (_iteration != m_iteration) { m_iteration = _iteration; changed = true; }
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
        if (value.contains("size"))
        {
            auto& val = value["size"];
            if (val.is_number()) 
                m_Size = val.get<imgui_json::number>();
        }

        if (value.contains("iteration"))
        {
            auto& val = value["iteration"];
            if (val.is_number()) 
                m_iteration = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["size"] = imgui_json::number(m_Size);
        value["iteration"] = imgui_json::number(m_iteration);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\ue3ec"));
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
    FloatPin  m_SizeIn  = { this, "Size"};
    FloatPin  m_IterationIn = { this, "Iteration"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter,&m_MatIn, &m_SizeIn, &m_IterationIn };
    Pin* m_OutputPins[2] = { &m_Exit,&m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    int m_Size          {3};
    int m_iteration     {1};
    ImGui::BoxBlur_vulkan * m_filter   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 3896;
    const unsigned int logo_data[3896/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xf6003f00, 0x1b692338, 0x9fe23571, 0xa92de71c, 0xca3fb02f, 
    0xf15003bd, 0xec791da2, 0x2818dd6d, 0xafc1753f, 0x2ff9f835, 0x0746aa93, 0x7ea57ff7, 0xf6728737, 0x50bafdc8, 0x8f9c6db2, 0xa1fd9887, 0xb4dd5d73, 
    0xe71528df, 0xfdb81e7e, 0xac5d93e2, 0x09a82a17, 0xe8357e20, 0x66d6fc4e, 0x36e552f4, 0xa819f88c, 0xc82fe00d, 0x517769a7, 0x823348dc, 0xb75eeb33, 
    0x4e0fade0, 0xf10aa1d5, 0x57e4fa64, 0xaa143c65, 0xf1f288af, 0xca308c59, 0x459eadfc, 0x45808034, 0x566cea63, 0x411de458, 0xe3097dcd, 0x7349831f, 
    0x91d36764, 0x223a8acc, 0x7c85fc13, 0x2e08e3f1, 0xbda93f7c, 0x136ddc9d, 0xd4f995a3, 0xa93a7fae, 0xdd612ae1, 0xf9b0a0ed, 0x0f1b1f86, 0xf5d8eb71, 
    0x5c0900ff, 0x2222dd0b, 0xabad40fd, 0x5fa6c34b, 0x097f3ac5, 0x5af8c115, 0xa9e859f4, 0xd4133633, 0x3ad40a9c, 0x748bbea8, 0x657a196e, 0xadc86b4e, 
    0x8ee4cc15, 0xc7ea4ab5, 0xc3a7f147, 0x9068af2b, 0x07e496eb, 0x8a079651, 0x1ec5b7f9, 0xfac2832f, 0x09c8935c, 0x05800390, 0xc417077d, 0x78084f9b, 
    0x524d5057, 0x1983bc48, 0x539126d8, 0x1511f89f, 0x5ff1bbf9, 0xaf7832c6, 0x03edf7c5, 0x3eccd3e6, 0x1f6ea35a, 0x7e397ba5, 0xd977e219, 0xc588311f, 
    0x95cabd3c, 0xea6be8b5, 0x42fc1edf, 0x699bf2d7, 0xf273b8fc, 0xff534dae, 0xe2f38500, 0xfdfaf98f, 0xf8d0714d, 0x8cbc5747, 0xcc94154b, 0x3911a187, 
    0xffa442fe, 0x5b1f8400, 0x199f00ff, 0xcbef00ff, 0x7d8500ff, 0x2b49a162, 0xf13a1f59, 0x31bfdb75, 0xddc207f6, 0xc4477d42, 0xc5704f13, 0xf4eba99c, 
    0x82f68bae, 0x13916a6c, 0x58e9dffd, 0x43a1097f, 0xfc8149ac, 0x4957f927, 0x85db44fb, 0xfe38a2d4, 0x5f635f1f, 0x2596b69f, 0x5f29fd58, 0xfff3cc92, 
    0x4be60d00, 0x6275d088, 0xc5177505, 0xc799dae1, 0xe73a3f3d, 0x9f180dfc, 0xea8bb55c, 0x3cec4a7f, 0x65cba55d, 0x6ee3197f, 0xd6e7d0d1, 0x96d324ba, 
    0xee3519a1, 0xb54153a3, 0x564fd558, 0x977b053d, 0x642afdc2, 0xaa80342b, 0x1f79c575, 0x34d4ac85, 0x4ecf4b72, 0xe1dd5e33, 0xb6215ddd, 0x07ae5c55, 
    0x6d587ba5, 0xcc9ecfd5, 0x62399c94, 0x2c8f267b, 0x0f06ae51, 0xe3cfbcd6, 0x3f8bc1bf, 0xea92e91c, 0x79abc876, 0xda216f14, 0x3f212439, 0xdf74b5ce, 
    0x1967f913, 0xadc8f73f, 0xaf52fc98, 0x99c5c40b, 0x6da45d64, 0xf4150f1d, 0xb03a9df1, 0xc1c796e5, 0xd5b075ba, 0x211f4815, 0xe9f3766a, 0x82be007e, 
    0x83b86445, 0x58738c2b, 0xaf9db3df, 0xb7607c88, 0xe26c49d1, 0x6788d918, 0xd5579c07, 0x0a00ff17, 0x3347bc6c, 0x12798c97, 0xd67c59b6, 0xa14dd704, 
    0xf81e5e68, 0x8bdca455, 0xc422795b, 0xb443cc4c, 0xd7dce399, 0x8c2d4381, 0xaf6dd5b9, 0x9c38f5a1, 0x95e451f2, 0x3fb55d0d, 0x7f6a7f2e, 0x119f5f1c, 
    0x454b843e, 0x07c8791c, 0x1fb80219, 0x0d00ff82, 0xaf6f3c6e, 0x6db2fbc5, 0x2b4e86d5, 0xbd8cf677, 0x69fc1a4f, 0x1ec886b9, 0x6dbcd34e, 0xfa1a7fea, 
    0xf87ff69f, 0x3c840763, 0x1acf0c39, 0xca41dd89, 0x8c5ecdca, 0x2e150ca3, 0x099b675a, 0xfb6accd4, 0xe15da75a, 0x5a8600ff, 0xc1a4994e, 0x25626d6e, 
    0x5a51a917, 0x6981f03f, 0x0fe9f31f, 0x782bf0fd, 0xd76452e4, 0xee36ea82, 0x569cea7a, 0xf87e3e96, 0xabc5124b, 0xd3ae1bc8, 0xf67babfc, 0xe16f1b90, 
    0xfd1fec3f, 0xf037d704, 0xe97562da, 0xfee9fe00, 0xd2fed455, 0x87bcee31, 0xfd9fb9fe, 0x6781d704, 0x7b7d84f5, 0xdf3cea7e, 0x696b37c0, 0x5f096be2, 
    0xfeb4dd84, 0x10dfee35, 0xd3b8632d, 0x02d0ae2d, 0xe2cf85a4, 0x1f5ee0f5, 0x652c2fb6, 0x8a20f07e, 0x7810cff4, 0x96874fc9, 0x123049de, 0x8f3fd840, 
    0xa452d7f8, 0x2d4954ea, 0x663f2a4c, 0xe9a0efd4, 0x8f06477c, 0x20cbe66f, 0x6b3da350, 0xfb413ccd, 0xaa69c943, 0x61925679, 0xbc061907, 0xc427e283, 
    0xf55abddb, 0xc2ca85a0, 0xd23b5e09, 0x1df03c7c, 0x567dc4df, 0x8e1108da, 0xbfe2dc48, 0xe960c142, 0x2aeda1e1, 0xc7884fad, 0xfdb42e66, 0xfcdf139d, 
    0xc513f119, 0x60d63f9e, 0xc33cd3b5, 0x863b2008, 0xbee2fce3, 0xa57bf8c9, 0xebe19dbe, 0xf71da821, 0x99b4529e, 0x33b03b39, 0xaff04b5e, 0xc2679ac0, 
    0x23473c7d, 0xb032b7c6, 0xa8ee3f39, 0xbcd760fe, 0xd4c83f78, 0x459e59e0, 0x41107c64, 0x9a1815e3, 0x534b9dd4, 0xc4d0f83c, 0x7a520e4a, 0x7e89c71c, 
    0x5368d935, 0x02b18dbd, 0xaff67ae8, 0x4f68bf94, 0x2b5b0eda, 0xec1aa84b, 0x1bc3ccbd, 0x2afcc873, 0x7347ede7, 0x670800ff, 0xb0e419c4, 0x48ea1d9c, 
    0x6b8e73da, 0xcb1b2fe3, 0xa278ad79, 0x33ad3c61, 0x4e6ee3c8, 0x7536d67a, 0x217a921a, 0x8fd038fb, 0xab67b52d, 0x0e7ecdfe, 0x337ec693, 0x42bb579b, 
    0x96dfa4fb, 0x45b9aff4, 0x2381b5a0, 0x00a38041, 0xd99ff10a, 0xc323c1ab, 0xe486169e, 0x0024d9a6, 0xb865aff4, 0x1c9242d4, 0xa9898f57, 0xb15b2aed, 
    0xa5d2e1e9, 0x2017100b, 0x35cc93e2, 0x2d464d5e, 0xf66f9ac7, 0xaede5f8c, 0xead8553e, 0xf8703ee7, 0xfcafa96b, 0x2020b224, 0x57f987fc, 0xd6d17ea7, 
    0xc7e0eea5, 0xf600ff78, 0xfc3e5f53, 0x97b9be22, 0x996513c6, 0x4f7d9687, 0xfe465f63, 0xdd7527d0, 0x1e4137bb, 0x655e437f, 0x228e525a, 0x963e7529, 
    0x37525595, 0x72789467, 0xc4c3dff2, 0x00905fdc, 0x5cebabfb, 0x63fc8b17, 0xc9aeb726, 0x6f09006f, 0xf6242385, 0xd5fbfa5c, 0x266a13bf, 0x0418f6f6, 
    0xf201509b, 0x57f963ab, 0x6f917899, 0x00ff35ad, 0x0a1519dd, 0xb89e118c, 0xccf4eb3d, 0x0547531e, 0x6a6a624d, 0x33cc80cf, 0xae8ad83a, 0x39921d8c, 
    0x69d662bd, 0x119642bc, 0x606469a1, 0xedeb1d07, 0x5e8100ff, 0x75f8b50c, 0x2175b1e0, 0xd34935f3, 0xaf031929, 0xbed69fe5, 0x86c7f07a, 0xbfd564ac, 
    0x8c616fb7, 0xbe2e0047, 0x27fee070, 0x869ff58a, 0xf1ba3bbe, 0x90d9bc3e, 0xb21386fd, 0x3f65c338, 0xcb9cbcc2, 0x2db9ee17, 0x02a78c8e, 0xe45457ab, 
    0x1e68d275, 0xa62ef128, 0xc16077d7, 0xc4954d14, 0x3dc9c88d, 0x5daff5c9, 0xaaad8ef0, 0x9a0bbe69, 0x3c998708, 0x5b0ee82a, 0xa57720b0, 0x74dd425d, 
    0x18c10a59, 0x6700757c, 0x19bca8a0, 0xd4b729ab, 0x43885812, 0x71988f23, 0x2b045e83, 0x671f4da9, 0xa9ec0753, 0x7c141f49, 0x8df5d555, 0x358ac757, 
    0xf1be0025, 0x75fa31c3, 0x371db926, 0xfcadadc3, 0x7281b34a, 0x81136a57, 0xf16b57f8, 0x93b758b3, 0xc01154e2, 0x28cf0323, 0x9ad3914a, 0xf80c3eea, 
    0x5ffc6f35, 0x164ba96d, 0x627edaf5, 0x6b7d709c, 0x4a6e8dd5, 0x4cf94c2e, 0x63fd184d, 0x1fe92e96, 0xd16ef84a, 0xd61a3d74, 0x00817fd9, 0x4b96a4a9, 
    0x63a8bcf9, 0xa1d2e7de, 0x1bcb9bbd, 0x001ce354, 0x147ed32b, 0xa82b32f8, 0x22c4ddc5, 0xa6373d56, 0x55a9c06b, 0xeed15e52, 0x47d266cd, 0xf50d9f0a, 
    0x6112953b, 0xa5803dd7, 0x5f8500ff, 0xb700ffa9, 0x11d357f9, 0x50dd7af8, 0x608f1201, 0x08ecefb4, 0xff90e73f, 0x7d69be00, 0x1ed91167, 0x7387b66f, 
    0x6cf663f2, 0xbfa61ab8, 0xdac9b033, 0xbe06fedc, 0xe1a1fd90, 0x63b31440, 0x00ffb110, 0xd97c4dc7, 0xbe4527f0, 0x64890ff0, 0xe08cbeb7, 0x3d62d8c6, 
    0xfb117d8d, 0x478b304d, 0x07104bfb, 0x3e19dfd9, 0xebadb191, 0x6d00ff52, 0x3e7d2c8c, 0xcafe111f, 0x09e533ea, 0xf1549365, 0x700bc52b, 0x2bbd0298, 
    0x83065ec3, 0x7fd9be5f, 0x2d3412b5, 0x0f7abdb9, 0x63bc0afd, 0xfe911ac3, 0x20a36bd8, 0xc948ccda, 0x5aee35f4, 0x9dbab05e, 0xbbb4258b, 0xa0c73d64, 
    0xa75bbf22, 0x3ac2564d, 0x6b7eea69, 0x8738ca2a, 0x4f79ec51, 0xc31a7fc6, 0x2edaa406, 0xb1764492, 0xd6430965, 0xfe807fba, 0x64b1b725, 0x122bd885, 
    0x5ff19a3a, 0xe953e919, 0x9cc7c5fa, 0xd4cf484b, 0xbd00dfd5, 0xbaad9ad4, 0x7372c5c4, 0xb0639f5f, 0x1fdc54ee, 0x9cf2ea43, 0x46c4b05f, 0x7b9f5ba2, 
    0xf64bafc5, 0x22031075, 0x661432b6, 0x8e37f1a4, 0x83aff0e2, 0x3796a5ae, 0x91b7578a, 0xec1c2381, 0x193eb026, 0xa2ad25c5, 0x80f7a37d, 0xff6c7990, 
    0xa7ae0800, 0x5bc147e2, 0x1a9e8abf, 0x26dac122, 0xab421df2, 0x1c082575, 0xf8f3357e, 0x363b2548, 0x8965de7d, 0x72b9688c, 0x36037cea, 0x3c892fb1, 
    0x49a02e4b, 0x4fe5e763, 0x7c6b5fe3, 0x3e04d314, 0x8f9cb715, 0xeb67649c, 0x556bf98a, 0x7cab51f8, 0xc77ed530, 0x2fb25bab, 0x63e48099, 0x5ed3d73c, 
    0x74eed403, 0xe5c30adf, 0x3de73129, 0xc5d7ab3d, 0xf05139ad, 0x1ee56698, 0x5cdfb977, 0x43e1ae65, 0x5f0152fc, 0xd2397c55, 0xad86bf85, 0x33dc189f, 
    0xfe68195f, 0xc4537d28, 0x17cd3656, 0x8a73e6cd, 0x3fc777fa, 0x147e6c17, 0x9ab63dfc, 0xc9e30256, 0x26dfa9dc, 0x2aea6abc, 0xea4c5d51, 0xb6fb497b, 
    0x1c14703d, 0xc93fb951, 0x6f6dcdaf, 0x85f1caf6, 0x963ca7de, 0xb78038a9, 0x8a6303ca, 0x0d00ffa3, 0xee8fe383, 0x75df7fdc, 0x233fa3d2, 0xcc48a80f, 
    0x3ca236f0, 0x6ad9a34d, 0xb4332012, 0x7a0f4b2a, 0xcb6b7fed, 0xb0c283a6, 0xb23674aa, 0xd586fc9f, 0xf014fccd, 0x4ef85ac3, 0x87f90bc9, 0xadf203c9, 
    0xe1eedacf, 0xeab6c253, 0x9fa44ddd, 0xafab29fa, 0x4e63ae17, 0x4d0a6ec7, 0x33192760, 0xe70bdbe1, 0x0783d3b7, 0x8f93823b, 0x836ff57a, 0xcc3220fe, 
    0x11385652, 0x85f75fbf, 0x2f8cdd78, 0x72157b3c, 0x3ed02b7d, 0x6807621e, 0x2b1c01a5, 0x8a3b9e1c, 0x37070ffd, 0xf1a1db19, 0x86d65555, 0x7b3bc697, 
    0x1e597579, 0xe7e68303, 0x5dcde907, 0x47e159f8, 0xbadcdb31, 0xd7e735f6, 0xf71ab7b7, 0x97cd7e8b, 0x83739228, 0xbfd2d79c, 0x7147740e, 0xa48c186f, 
    0x2b72064a, 0x4d8cb3e2, 0xb9b4e2a4, 0x130e39f7, 0x4b4ec9da, 0x29bcd763, 0xe0ecd817, 0xbf70300c, 0x78b657c8, 0x96f25807, 0x656003dd, 0xc92bea47, 
    0x40798420, 0xeadac628, 0x9ba82f7c, 0x7152a269, 0x2be65f86, 0x7156a9e2, 0x03fdac9e, 0xe88c871f, 0xeddf1dd9, 0xcb9af02f, 0x8606fec7, 0xed1728fa, 
    0xa8cb3091, 0x1fbcd6e7, 0x4a47b6c2, 0xb236ccf0, 0x59c6e601, 0x2b0e7d58, 0x681a5dec, 0x5c0beff5, 0x9a1db6da, 0x717d8023, 0x16df9f5f, 0xc1678df5, 
    0x8abe309e, 0xbbc758dd, 0x7e9faa72, 0x27abeb6b, 0x3f92702a, 0x4a78a534, 0xf8853e70, 0x1d9be02d, 0x853a596b, 0x72f2a0ca, 0x9757fbfc, 0x2b7ed4fe, 
    0xa3c47b4d, 0xd13f594f, 0x8342f9e0, 0x786d05c6, 0x3e94e227, 0x757ff815, 0x56cadca9, 0x1f01d4f1, 0x9bafc843, 0x5c16bf75, 0xbb5eb378, 0xe69394be, 
    0x953e2339, 0x52e1d4f3, 0x6cc925be, 0x248f568f, 0xc688e93d, 0xa28d58dd, 0xf7bf7d93, 0x265ce747, 0x2d49e2b7, 0x56230975, 0xf80f55e0, 0xf5fe254b, 
    0xada00a7d, 0x299e39aa, 0x6b7d46df, 0xf6af2afb, 0x9e2d8197, 0x271899e0, 0x3757f853, 0xebcc66fb, 0x38a0d9a7, 0xc7b0b35f, 0xd235b3fd, 0xf2dfc8fe, 
    0x00ff5b20, 0xcb15feb9, 0x00ffd9fe, 0xff67e7f1, 0xffed5e00, 0x65cda200, 0x1446fe5b, 0xfb2f3cce, 0x6fe5438d, 0xbe475b0e, 0x9364b423, 0x75a02bfd, 
    0x311ecc16, 0x132b9eb4, 0x1f7a9fc3, 0xb995fea9, 0xb57ed175, 0x8a740cfa, 0xdd52f2b1, 0xb40c1f9b, 0x74ad2b1b, 0x9d5b24cb, 0x96be26b9, 0xb462add0, 
    0x0824168d, 0x9fe7ab3d, 0x17f23f85, 0x2ffa4a1f, 0x5fa9fb4b, 0x93f3e68d, 0xd437c9c5, 0x8422bbfd, 0x230d1256, 0x4786d3ad, 0x8162b135, 0x3b385267, 
    0x7dacf387, 0x5be3a737, 0x97eb7f56, 0x799d3fea, 0xd87ae8f1, 0x5fee99ad, 0xdd7f650d, 0xaf78f08c, 0xbb68bf94, 0x56e29748, 0x60160c02, 0x86afea6b, 
    0x96af63bd, 0xef6800ff, 0xfe6b29f9, 0x2fdbd7fb, 0xd9f243f7, 0x9ebd00ff, 0x8118e357, 0xece4b6d2, 0xc573f70b, 0xac513670, 0xcacc3d7a, 0x1a1ce030, 
    0x7f181ff4, 0x00ff30c8, 0xaf055cbb, 0xfe2e80fc, 0x1f30b986, 0x1a66e909, 0x3ece6335, 0xf2ae97cc, 0x633937ca, 0x97fd4b93, 0x96dafd6f, 0x7f5bfdd3, 
    0xf56a6abc, 0xd9ff10cf, 
};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(BoxBlurNode, "Box Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Blur")
