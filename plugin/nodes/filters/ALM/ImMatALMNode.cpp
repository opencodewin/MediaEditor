#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "ALM_vulkan.h"

#define NODE_VERSION    0x01000100

namespace BluePrint
{
struct AlmNode final : Node
{
    BP_NODE_WITH_NAME(AlmNode, "ALM Enhancement", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Enhance")
    AlmNode(BP* blueprint): Node(blueprint) { m_Name = "ALM Enhancement"; m_HasCustomLayout = true; m_Skippable = true; }
    ~AlmNode()
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
        if (m_StrengthIn.IsLinked()) m_strength = context.GetPinValue<float>(m_StrengthIn);
        if (m_BiasIn.IsLinked()) m_bias = context.GetPinValue<float>(m_BiasIn);
        if (m_GammaIn.IsLinked()) m_gamma = context.GetPinValue<float>(m_GammaIn);
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
                m_filter = new ImGui::ALM_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_strength, m_bias, m_gamma);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_StrengthIn.m_ID) m_StrengthIn.SetValue(m_strength);
        if (receiver.m_ID == m_BiasIn.m_ID) m_BiasIn.SetValue(m_bias);
        if (receiver.m_ID == m_GammaIn.m_ID) m_GammaIn.SetValue(m_gamma);
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve* pCurve, bool embedded) override
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
        float _strength = m_strength;
        float _bias = m_bias;
        float _gamma = m_gamma;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_StrengthIn.IsLinked());
        ImGui::SliderFloat("Strength##ALM", &_strength, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_strength##ALM")) { _strength = 0.5; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (pCurve) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_strength##ALM", pCurve, ImGui::ImCurveEdit::DIM_X, m_StrengthIn.IsLinked(), "strength##ALM" + std::to_string(m_ID), 0.f, 1.f, 0.5f, m_StrengthIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_BiasIn.IsLinked());
        ImGui::SliderFloat("Bias##ALM", &_bias, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_bias##ALM")) { _bias = 0.7; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (pCurve) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_bias##ALM", pCurve, ImGui::ImCurveEdit::DIM_X, m_BiasIn.IsLinked(), "bias##ALM" + std::to_string(m_ID), 0.f, 1.f, 0.7f, m_BiasIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_GammaIn.IsLinked());
        ImGui::SliderFloat("Gamma##ALM", &_gamma, 0, 4.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_gamma##ALM")) { _gamma = 2.2; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (pCurve) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_gamma##ALM", pCurve, ImGui::ImCurveEdit::DIM_X, m_GammaIn.IsLinked(), "gamma##ALM" + std::to_string(m_ID), 0.f, 4.f, 2.2f, m_GammaIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_strength != m_strength) { m_strength = _strength; changed = true; }
        if (_bias != m_bias) { m_bias = _bias; changed = true; }
        if (_gamma != m_gamma) { m_gamma = _gamma; changed = true; }
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
        if (value.contains("strength"))
        {
            auto& val = value["strength"];
            if (val.is_number()) 
                m_strength = val.get<imgui_json::number>();
        }
        if (value.contains("bias"))
        {
            auto& val = value["bias"];
            if (val.is_number()) 
                m_bias = val.get<imgui_json::number>();
        }
        if (value.contains("gamma"))
        {
            auto& val = value["gamma"];
            if (val.is_number()) 
                m_gamma = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["strength"] = imgui_json::number(m_strength);
        value["bias"] = imgui_json::number(m_bias);
        value["gamma"] = imgui_json::number(m_gamma);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\ue42e"));
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
    FloatPin  m_StrengthIn = { this, "Strength"};
    FloatPin  m_BiasIn  = { this, "Bias"};
    FloatPin  m_GammaIn = { this, "Gamma"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_StrengthIn, &m_BiasIn, &m_GammaIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_strength    {0.5};
    float m_bias        {0.7};
    float m_gamma       {2.2};
    ImGui::ALM_vulkan * m_filter {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 4600;
    const unsigned int logo_data[4600/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xf6003f00, 0xafe75215, 0xea78c915, 0x1f9bf45f, 0x0a00ff76, 
    0x97d5b8dc, 0x3e2d6efb, 0x32230e32, 0xa6d35339, 0x117f7305, 0xb46ef265, 0xf87200dc, 0x0ef055fe, 0x3e74baa6, 0xce378df2, 0xc086d4af, 0x959e3f9f, 
    0x36588db2, 0x24f37e32, 0x115be967, 0x07703db8, 0x1757eda9, 0x169b8ead, 0xf3a362b7, 0x4feb3043, 0x6c2491b7, 0x93edc8ed, 0xbd75fa5b, 0x3078c4bd, 
    0xa0727d46, 0x8561f49a, 0x23755595, 0x8d8aadce, 0xcfd9b21f, 0xbc55c539, 0xc1616027, 0x7a75455e, 0x48d2868f, 0x6463968c, 0xb9f2098a, 0xc19dbe21, 
    0x95523bae, 0x95e1dded, 0x0caf520c, 0xa147308c, 0x3a6becc1, 0x574325b4, 0x143174b1, 0xdfdd2bf1, 0x3fd9d4b1, 0xb713699f, 0x5233dade, 0x64b831ee, 
    0x2b722a38, 0x45904f3b, 0x3821c5a7, 0xd424b95d, 0xb99a3fcd, 0x419e0a52, 0x962aeb1d, 0xcd3ed1bd, 0xcbe24bea, 0x066b7541, 0x020717af, 0x63b78739, 
    0x5ce71ff8, 0xdaafbd76, 0xb84663f8, 0xfd77efd4, 0xb10d2c9e, 0x38b1b38c, 0xdc531f55, 0xb69d5df1, 0xd9a167b9, 0x4faac9dd, 0xc26e1a14, 0x51dd7965, 
    0x0ec4e207, 0x92af757a, 0x39e241bc, 0xddae57fc, 0x883b126a, 0x95859123, 0x18cb7fb3, 0x7a9cf273, 0x5f9f8c91, 0xf610d26a, 0xcba95bad, 0xc10bfb19, 
    0xd2d0dbf4, 0xf8243ed5, 0x96be51c7, 0xb95acfe2, 0xfc982dd2, 0x36bb9696, 0x4990fa20, 0x7faaa63e, 0xafe377c2, 0x1f351bfa, 0x00ff87fc, 0x753dd7c4, 
    0x48b2eaa7, 0xd6408724, 0x937115f5, 0x4c8b8525, 0x5241728a, 0x23c7307b, 0xd93f54dc, 0x0afddfba, 0x00ff281e, 0x6bd74bc1, 0x7e8996a6, 0x2df5aa25, 
    0xe45feed3, 0x39e1377d, 0xa6796d5e, 0x22cdb795, 0x35f56631, 0x5551e263, 0xff33d3bc, 0xad3f3d00, 0x9fa8c233, 0x7fb4cbda, 0x27bea901, 0xd26abf46, 
    0x9600fff8, 0x8257d49f, 0xebb1579a, 0x7945e3af, 0x079c6c14, 0x84563c39, 0x9077deee, 0xc9a854c8, 0xacf7f9cf, 0xba2400db, 0xe8f92f8a, 0x02999fea, 
    0x6a4fbcb7, 0x48d3ebd0, 0xe0f6b531, 0xfe91fd67, 0xd55da415, 0xa4ddc4fc, 0x7b72e5a3, 0xde24d648, 0x15f2cfed, 0xc9b068d2, 0xe561ccb7, 0x883fe821, 
    0xc26aa78f, 0x911be0d3, 0x3a9c64dc, 0xcd1ddb01, 0x3bd7b674, 0xa77e7797, 0xd87bbd23, 0x2c3cb375, 0x46e57663, 0xe02844bc, 0xb59e1e05, 0x0a1fe387, 
    0x362be245, 0xead12d9e, 0x4bde4430, 0x38cc0321, 0xb3bd5125, 0x7e35a78f, 0x1e1c9c3b, 0x5e9c0a7f, 0xcf936379, 0xc75ee9b0, 0xb23c52b9, 0xedf5f9d8, 
    0x1a4ecd28, 0x90b38533, 0x3e58785c, 0x24a0bbd2, 0xc7110cab, 0xc05f5c43, 0xc6fb7e4d, 0x9fed0d1a, 0x3499a7d9, 0x6fd99ac9, 0x6c8a4859, 0xefc78a1e, 
    0x9c71e815, 0xf37acd11, 0x4df756e8, 0xb66db222, 0xb8e91294, 0xfc7992e4, 0x7456ebf3, 0xf04e231d, 0xa46395a6, 0x96fdd0e9, 0x3004ddc6, 0x6a9fb9c5, 
    0x492c838e, 0x1aaf893f, 0x6ef20296, 0x6e6dd1ab, 0x3257b7a7, 0x2ba98de5, 0x301ff8b7, 0x4cf16bfc, 0xe58adffa, 0x8b56e0f0, 0x7d58d24e, 0xcb605ca5, 
    0xfc654072, 0xee411014, 0x8b15f449, 0x13c2e3f0, 0x51c4b378, 0x9afd50c6, 0x256e09dc, 0xc21fc0db, 0x38e7d4a3, 0xa54fb01d, 0x534ff143, 0x4ffc105f, 
    0x8ddb13f1, 0xfd5fdcf1, 0xa50d039d, 0x9fb16a8c, 0x9ac328a7, 0xde86eff7, 0xbec18f0c, 0xfcd88a1c, 0x361f72d7, 0x83e540e2, 0x1c387d48, 0xb7d38101, 
    0x61b4b57e, 0x13e4371a, 0x52f12aab, 0xf597b79a, 0x36746cfd, 0x85bd653a, 0xa64470ac, 0x2260d018, 0xe44f0570, 0xfce7fd47, 0x2b9ac7e9, 0x45ee6607, 
    0xe8c22f8e, 0xf5205413, 0xc54771da, 0x89d2262e, 0x2af6fd38, 0xaef3053d, 0x5c00ffc7, 0xdf54fadb, 0xfadee314, 0x38314e4e, 0xa7cd02af, 0xb553e963, 
    0xd618fa68, 0xee9afd73, 0x22f2e0da, 0xec612499, 0x0afa1318, 0x1b537cef, 0xc4b270cb, 0x1b16f1a8, 0x23edd45b, 0x4d5ce7af, 0x20a52194, 0x3174c5f4, 
    0x5aa1ab4e, 0x5855ef58, 0x0c775f32, 0xc93f6609, 0x1c466b85, 0xbe22e5d2, 0xe8458d92, 0xb7236f48, 0x79f2c88c, 0x3f4b8e71, 0xc9f53e19, 0x2114bf6a, 
    0xda825683, 0xac2847da, 0x966f575e, 0xb7e01a31, 0x30504f3d, 0xb51ec939, 0xc7cf78c2, 0x8e8cfab7, 0xadd94f6d, 0x02399a6d, 0xb79ccc91, 0xc7c0c9e3, 
    0xd82bfdd4, 0xc6eb6a78, 0xba695b92, 0xbb609f64, 0x69e20237, 0x60f296c2, 0x4f9b4c2c, 0xc6402c46, 0x0a79927b, 0x1bbe7d45, 0x75100a0d, 0x1f7b2d2a, 
    0x9cba5e3b, 0xeaf76094, 0x261e447a, 0xaf8867f1, 0x85d0b1a1, 0x1ad901b4, 0xde948b6b, 0x9e938820, 0x183ff38d, 0xf1e47bc6, 0x9bf4d5d7, 0x60ec2c79, 
    0xc505b94b, 0x655945e0, 0xf119da55, 0x3db703c9, 0xbec31bab, 0x8df0b31f, 0x62b6b698, 0xdc61d45d, 0x0f809be5, 0x59d115f2, 0xf9b97120, 0x705a5363, 
    0x63c5fd94, 0xcaa9e5cf, 0xf7197adb, 0xb7b6275e, 0xdbd296b8, 0xf2c97717, 0x5561c398, 0x76339cb1, 0x695eed38, 0x74e22ff1, 0xdb9015fe, 0x7853cc3d, 
    0x3606e28e, 0xca6730d6, 0xce635cb4, 0x3f39f794, 0x236e9c2a, 0xfc2d12d0, 0xf065d442, 0x0c5fc5c7, 0x5ab670ed, 0x1379fbc6, 0x52683e38, 0x7e4cbf55, 
    0xadbef355, 0xeaba256b, 0x96ef5c13, 0xfce5eaf2, 0xcc929cd9, 0x49127772, 0xe10706ec, 0x924a7d5e, 0x2dbb5c93, 0xba537ffd, 0x4a301e96, 0x8ddfdefa, 
    0xeec800ff, 0x00ff677f, 0x55abfd09, 0x2c13d217, 0x1eb50936, 0x439a994f, 0xcedd62c0, 0x0abb4f32, 0x689721fa, 0x00ff71c5, 0xde36fc0a, 0x8c9df01b, 
    0xb45ca06e, 0xf7cfe561, 0x0e3f929b, 0x735de107, 0x0a0e07ce, 0xa2e82dd6, 0xd7520efa, 0xd19ca441, 0x4cd3fab8, 0xfc184faa, 0xedc13ce9, 0x1a68d4f9, 
    0x77810e9c, 0x435b138f, 0xc8d1fcf2, 0xd80afa7d, 0x341b9ff8, 0xff18509a, 0xd73a5c00, 0xa92be107, 0x71f6c7db, 0xec17e9b6, 0x38d812f3, 0xf8be2b1d, 
    0xa39f4f96, 0x546e7070, 0x6b3de813, 0x92566ac0, 0x8ffcdf5b, 0x83d6d1a4, 0xb53d66f2, 0xe78daedd, 0x7df2eade, 0xcb18c28e, 0x9196872c, 0x06150e86, 
    0xe6767a4b, 0x546bfdb8, 0x6ed75c97, 0x56acbd8d, 0xebb6798b, 0x26df591e, 0x23b358d5, 0xc98f21a9, 0x79151dd3, 0xe2b115e3, 0x70a71e73, 0x1c73ca90, 
    0x63551552, 0xc9000a94, 0x313fc0db, 0xe26a9f03, 0x347657bc, 0xabce88bf, 0x75cb747c, 0x8e3c548d, 0xc78c1516, 0x971f949f, 0x4deaef97, 0xcb84817e, 
    0x2aa960a9, 0xca6735f5, 0xabd571e2, 0xece07456, 0x77c64b8e, 0x51471d46, 0x9b84a6d3, 0x3afbe488, 0x34c068c7, 0x1194c789, 0x209cc4b1, 0x801846e8, 
    0x870fe92b, 0x0b3e1ebe, 0xf196e8f0, 0xa0d6da84, 0x398f46a2, 0x800b8400, 0x1ea39071, 0xc5f549bd, 0xe1957679, 0xf85923ed, 0xcbc59a8a, 0x0dbae8ab, 
    0xdc9d1aa0, 0xc545beea, 0xdbd8edd2, 0xc365e8fe, 0x4cce71e7, 0x86bfba46, 0xf1be327e, 0xdeae4dc2, 0x77bc2d47, 0xb2b5646c, 0x2cfb6d94, 0x8ec7c239, 
    0x7f0b0ac7, 0x79b54fb4, 0xbe6ecc78, 0xe1c2e8ac, 0xfaa0e439, 0xc000ff2b, 0x3cac483a, 0xf41bad47, 0x87b0a6d6, 0x4d6d45f7, 0x9b1f12ac, 0x63ee5c39, 
    0xea4a5fc7, 0xb7be3634, 0x51ccd5f0, 0x5aa86fb4, 0x903652c0, 0x9cc28512, 0xb99d3312, 0x8e738003, 0x17da3ff5, 0x8d42ceec, 0x35c775c0, 0x51ef8717, 
    0xe842539f, 0x433b1973, 0x71cd830c, 0x4b5251c2, 0x900a3df1, 0x9a9eb297, 0x47ef5b2b, 0xf3f6f884, 0xbec79752, 0xc7ec8a1c, 0xc3dc7134, 0xb0b66e70, 
    0xc81c9465, 0xb393b88b, 0xada73ea6, 0xc7685f65, 0xbaaef852, 0x8b31b042, 0x655f92b7, 0x4cbf5ddc, 0xfeb4c60f, 0x41516b31, 0x225688e2, 0x59ebb481, 
    0x8c94dd2e, 0x136064ab, 0x6b9e81db, 0x86b7f0aa, 0x7830fed5, 0x22925397, 0x3c71acd1, 0x7f253012, 0x03588a7a, 0x7f3bf8d3, 0x84b0de7b, 0xdeb313dc, 
    0xfcb7725f, 0x2ac66b02, 0xfc92ad49, 0x4dcfe1bf, 0x5463088c, 0xe635851d, 0x97aac225, 0x3a50c073, 0xf3fd48d2, 0x90255a5a, 0x77ddf364, 0x91949f3b, 
    0x5bb547f7, 0x79bc2f97, 0x66d76ab2, 0xcc2df67f, 0x420433bf, 0x7f34877f, 0xe4fdcf60, 0x10aaebfc, 0xb47ea62f, 0x2be82b79, 0x27876a5e, 0x7fce47b4, 
    0xa34e6f08, 0x662529e2, 0x4e1b2c1f, 0xfcf1643b, 0x223ed1ab, 0x8e3e1bc5, 0x1f619924, 0x046e5c68, 0x749c3eee, 0x7c98d7fa, 0xfc6fd330, 0x7449e209, 
    0x9f1056bd, 0x2a43cb6a, 0x73aa1c0e, 0x7ff40afa, 0x3a545e8b, 0x37e6db1f, 0x92042b02, 0x06b28de9, 0x124b1160, 0x41c7ed41, 0xf30a7b92, 0xf6f34aa1, 
    0xf4b1eb56, 0x6abcb2ae, 0x368f15bd, 0xe2b126df, 0x9884b5a7, 0x99d30e63, 0xa9f36163, 0x007fcade, 0x734eef3e, 0x0a7e345d, 0x8994f1b5, 0xba5baa6d, 
    0x70cb36e5, 0x8ce415a8, 0x9e33c212, 0xff86dd40, 0x05d78000, 0x9f978be1, 0xcfd5b051, 0xc48d1d99, 0xa84ae649, 0xcac87c72, 0xe5d35306, 0x011d6720, 
    0xedf44acf, 0xc256fa2a, 0x415dc6a4, 0xd9966601, 0x07db91c2, 0x7624fc83, 0x79d2aff4, 0xb33f4c55, 0x2a3ef68e, 0x7b6b3955, 0x2b3ecd43, 0x5a6ac078, 
    0x2af6e892, 0xc85ba58b, 0x020296a5, 0x6ce0a834, 0x9f1372e0, 0x614f16f8, 0x1dc32f5d, 0x415b1b42, 0x860d5864, 0xccdba34d, 0xd7c80f24, 0x335a6b96, 
    0xced66ae8, 0x67084d43, 0xa50c7825, 0xdb012b94, 0x23883f23, 0x6ff868b5, 0x5adf33ed, 0x6f4992c4, 0xfb22ca23, 0xc6467542, 0x3ee8f741, 0x32e6f9b5, 
    0x5b97d394, 0xaa80551f, 0x8cbe28a3, 0x12ea3cfa, 0x45638ac4, 0x960d0f91, 0x49a51fc1, 0xd135e27f, 0x7b793f3c, 0x302c303c, 0xb2d224c1, 0x54b52f31, 
    0x9c1ce3b1, 0xa1a78a67, 0x01d1b624, 0x18e7e23c, 0x7b618cf9, 0x3bbe697d, 0xc4d725f0, 0xadad0f0f, 0x05b6a0ae, 0x4246b158, 0x8db46156, 0x1ce0418c, 
    0xd029fdfb, 0xce3dd5bb, 0x704a5ddc, 0xdaf9c45a, 0x138f6bc2, 0x7179bbb6, 0xe4f23593, 0x1e033db1, 0x0f0cd007, 0xf086bec2, 0x67f83238, 0xfb71bb4d, 
    0x59488beb, 0xc0887ccf, 0x96e395fe, 0xf0bd1d1e, 0xe9aaa78e, 0x59a3cdfa, 0x9517115c, 0x8c25999f, 0xc346f303, 0xd8f10cef, 0x4506328e, 0x2587077b, 
    0x5ac3cf9a, 0xbc124352, 0x3b1e6b2b, 0xb5fe991e, 0x95b23c4a, 0x5eab00ff, 0x6a7e95c7, 0xc43faf49, 0x513bb9d8, 0xec0a7a8e, 0xbb3cf834, 0xc8a75738, 
    0xe479953f, 0x444ec2fa, 0xbb911c2d, 0x912b0810, 0xd7fa03c9, 0x134fe26b, 0x4a4f7859, 0x858f5737, 0x51a24818, 0xc2b19197, 0xab78eea8, 0x6f77116e, 
    0x726a8663, 0xe46b6cf7, 0x4746917f, 0xf3e015f9, 0xadf1567c, 0x233c8df4, 0x72a1d24a, 0x6ecd5a4a, 0x6dafbaf3, 0x27411dce, 0xd6ef71d8, 0x0b00ff99, 
    0xf45fc72f, 0xf0bfd111, 0x00ff275f, 0xd2faa91d, 0xf32fbfe8, 0xa9cffe36, 0xfcc7afd5, 0xf11d3e8a, 0xf030de34, 0x114fc30f, 0xdfd3aac8, 0xea39f382, 
    0x7f485b37, 0xfcd61598, 0x27fbbc6c, 0xe657ee83, 0x6d3c32d9, 0xc68cb1b5, 0x2bfdfbfc, 0x8437f09a, 0xf0357cee, 0xaed3428f, 0xe7f292ee, 0xdf7fb84e, 
    0x0643bbc6, 0x6e07820f, 0x8d5fba1a, 0x89f09f4c, 0xe7e31bcd, 0x3db95d86, 0x9d3f36f6, 0x5a5c5769, 0x99fcd27d, 0x078d5a74, 0x3ebaf616, 0xce9db37a, 
    0x62de2487, 0xc4dcd22c, 0x3e09c6b1, 0xe41c4154, 0x0a88f68f, 0xd2d77147, 0x77e11bbd, 0x65b53f88, 0x4592c688, 0x79778733, 0x1c94dd25, 0x404e3d75, 
    0xf4cdabfc, 0x7ceb96d4, 0xbadc3320, 0xe08cdfee, 0x742028e4, 0x7b006700, 0xdaf059d7, 0xb354a629, 0x09d327f3, 0x455cf10e, 0xa37b3ef3, 0x37c61829, 
    0xf6959e1e, 0x6ebe6938, 0x763e965e, 0x3c16bbae, 0x0b75627f, 0x78da96a5, 0x642d5d6f, 0x2322698e, 0x74d6fb2b, 0xdac8b90c, 0x52eb74bc, 0x9e4823f8, 
    0x07b5097d, 0xadef1900, 0x951d38c4, 0xcdeb2387, 0xa7ae7a70, 0x1a1fe379, 0x14eb209b, 0x705aea7f, 0x8c942f76, 0xea397157, 0x3ecf937b, 0x1b7eee95, 
    0x6c994bb5, 0x3442b5e0, 0x25f905ab, 0x31690223, 0x40d55780, 0xf1e49e27, 0x301fbed2, 0xa5b52ac4, 0x9f2bab08, 0xb450975f, 0x87aee514, 0x80886d63, 
    0x6fcdc018, 0xaafcd3e9, 0x7b58e933, 0xaa1d2f77, 0x4adb94e5, 0x2b30eaa9, 0x564e9596, 0x8aa16793, 0xa702a7a6, 0x4d0bdfc6, 0x9a869fe2, 0x1dd4d3b9, 
    0xeeddc66a, 0x1f1cf7f4, 0x99901435, 0x7eba6ff4, 0x109f1eb5, 0xe2acd2b5, 0x5014121d, 0x77058a3e, 0x45b34e56, 0x1881f986, 0x1baff720, 0xb5fe05f1, 
    0x116adea0, 0xd31d4bdb, 0x86282b43, 0x55c7c065, 0x2b628f00, 0x3c5ac4d7, 0xfadf12b5, 0xe73300ff, 0xe934b968, 0xf34e6ddf, 0x7a5cbac3, 0xc893e485, 
    0x7059583e, 0x73ada7be, 0x636f107f, 0x59f435d5, 0x48f6d630, 0x00ff6476, 0xe99fe019, 0x039dd5f9, 0x78a6e9c5, 0x5ac4df32, 0xf7d91fbc, 0xd8e0c252, 
    0x20636f23, 0xa71e6d24, 0xf5ade08a, 0x54529d1b, 0x2e40b9b8, 0x44ba1c6e, 0x827ebd3e, 0x554a79bc, 0xd70b5a6a, 0x3c0c3df4, 0x3de76439, 0x2ef58b96, 
    0x62ab90bc, 0xe1fb1950, 0x79527b7a, 0xffdcbfb6, 0x936bc700, 0x46bdf5d7, 0x1b9253db, 0x3e851113, 0xb724b75c, 0x00ffce7a, 0xff5d8b84, 0xa53fa600, 
    0xe848be75, 0x9e3c57e2, 0xf09b9ea9, 0xf0a18e9d, 0xf9e7e1b6, 0xa98a6ed9, 0x7f1ba027, 0x697fb3c6, 0xad745939, 0x92703012, 0x5c0f434e, 0x35f8d18a, 
    0xa62400ff, 0x0fbffeeb, 0xfdcb5af3, 0xc3e33fa5, 0xaf00ff40, 0xbbabfc83, 0xfaa962fe, 0x28f3d42f, 0x5e0600ff, 0x7a70e4bf, 0xca639c5d, 0x5a1d0294, 
    0xd671100c, 0x82fecf75, 0xd5d85a3f, 0xb5b7e4ee, 0x0ae3d5b8, 0x0acbc897, 0xab8dab48, 0x3ff303c7, 0x9f69679d, 0xdbfa63ea, 0xeae800ff, 0x00ffaeb5, 
    0xff77b3c8, 0xffd75d00, 0xafaf6500, 0xe7f0c88e, 0x83df32f1, 0x58a6233c, 0x98578bf8, 0x2e4ae741, 0xee985608, 0x810d0828, 0x53afc9e9, 0x7100e3b7, 
    0xf8c2958e, 0x9a43fe6b, 0xff7cfdbf, 0x5dabec00, 0x2df915d4, 0x2a96937a, 0x7fd2dead, 0x1a56fa99, 0xe9e96129, 0x68e457f6, 0x08abf7db, 0xaf4a37a0, 
    0x94d5d46d, 0x0dd9d5fb, 0xce6d8d91, 0x1d274c83, 0xa2f1836b, 0x94385e2c, 0x112c3100, 0xf2dc03bb, 0x5d01f933, 0xb8d29be6, 0xc87f1c4f, 0xdaeb3ff2, 
    0xf4d5e63f, 0x3e02fe13, 0x66c73f5e, 0x1769b936, 0x81421ed8, 0xbbd136d8, 0xd9610590, 0xd54b231f, 0xdccd2175, 0x6e46ed71, 0x15748c76, 0xf2dfebd0, 
    0xfffd930a, 0xec5ce900, 0x6b2af21f, 0x4a7fee3f, 0x5f9b30f3, 0xb1f5d8fa, 0xa71c593a, 0x6b35bc85, 0x35216de2, 0xae9de70b, 0x52dc6624, 0xa5afa34d, 
    0xc200ff6b, 0xbdbfd2ba, 0x7f00ff75, 0x6fc3278d, 0xff6d15f9, 0xea6ade00, 0x239ca22b, 0xb6d62628, 0x2939a738, 0xd9ff9934, 
};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(AlmNode, "ALM Enhancement", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Enhance")
