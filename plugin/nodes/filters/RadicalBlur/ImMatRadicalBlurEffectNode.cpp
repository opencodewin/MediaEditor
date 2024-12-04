#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "RadicalBlur_vulkan.h"
#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct RadicalBlurEffectNode final : Node
{
    BP_NODE_WITH_NAME(RadicalBlurEffectNode, "Radical Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Blur")
    RadicalBlurEffectNode(BP* blueprint): Node(blueprint) { m_Name = "Radical Blur"; m_HasCustomLayout = true; m_Skippable = true; }

    ~RadicalBlurEffectNode()
    {
        if (m_effect) { delete m_effect; m_effect = nullptr; }
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
        if (m_RadiusIn.IsLinked()) m_radius = context.GetPinValue<float>(m_RadiusIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_effect || gpu != m_device)
            {
                if (m_effect) { delete m_effect; m_effect = nullptr; }
                m_effect = new ImGui::RadicalBlur_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, m_radius, m_dist, m_intensity, m_count);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_RadiusIn.m_ID)
        {
            m_RadiusIn.SetValue(m_radius);
        }
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
        float _radius = m_radius;
        float _dist = m_dist;
        float _intensity = m_intensity;
        float _count = m_count;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_RadiusIn.IsLinked());
        ImGui::SliderFloat("Radius##RadicalBlur", &_radius, 0.0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_radius##RadicalBlur")) { _radius = 0.38f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_radius##RadicalBlur", key, ImGui::ImCurveEdit::DIM_X, m_RadiusIn.IsLinked(), "radius##RadicalBlur@" + std::to_string(m_ID), 0.0f, 1.f, 0.38f, m_RadiusIn.m_ID);
        ImGui::EndDisabled();
        ImGui::SliderFloat("Dist##RadicalBlur", &_dist, 0.0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_dist##RadicalBlur")) { _dist = 0.25f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("Intensity##RadicalBlur", &_intensity, 0.0, 1.f, "%.0f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_intensity##RadicalBlur")) { _intensity = 1.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("Count##RadicalBlur", &_count, 1.f, 40.f, "%.0f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_count##RadicalBlur")) { _count = 40.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_radius != m_radius) { m_radius = _radius; changed = true; }
        if (_dist != m_dist) { m_dist = _dist; changed = true; }
        if (_intensity != m_intensity) { m_intensity = _intensity; changed = true; }
        if (_count != m_count) { m_count = _count; changed = true; }
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
                m_radius = val.get<imgui_json::number>();
        }
        if (value.contains("dist"))
        {
            auto& val = value["dist"];
            if (val.is_number()) 
                m_dist = val.get<imgui_json::number>();
        }
        if (value.contains("intensity"))
        {
            auto& val = value["intensity"];
            if (val.is_number()) 
                m_intensity = val.get<imgui_json::number>();
        }
        if (value.contains("count"))
        {
            auto& val = value["count"];
            if (val.is_number()) 
                m_count = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["radius"] = imgui_json::number(m_radius);
        value["dist"] = imgui_json::number(m_dist);
        value["intensity"] = imgui_json::number(m_intensity);
        value["count"] = imgui_json::number(m_count);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\ue01e"));
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
    FloatPin  m_RadiusIn  = { this, "Radius" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_MatIn, &m_RadiusIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_radius          {0.38f};
    float m_dist            {0.25f};
    float m_intensity       {1.f};
    float m_count           {40.f};
    ImGui::RadicalBlur_vulkan * m_effect   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 3640;
    const unsigned int logo_data[3640/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xe9003f00, 0x6754eba5, 0x765bc538, 0xfbac78dc, 0x7e0dc696, 
    0x36f6135b, 0x563cbb52, 0xd810d335, 0x72a92bad, 0x8dd75831, 0x5c573ccf, 0xa6b3d90c, 0xa32b0831, 0xadd17db6, 0xa154da72, 0x738fe980, 0x0285ce5d, 
    0xc506445d, 0xdb15f73c, 0x951c7318, 0xbb2228aa, 0xa64b0d9e, 0xd0c75030, 0x00ffbdd6, 0x67a1d987, 0x0970c8b6, 0xef4ef0f2, 0x922461a5, 0x408a165a, 
    0x511e9462, 0x2bfc08c6, 0x9c180757, 0x6c6aa44a, 0x9bafd174, 0x9d3f876e, 0x3136d93a, 0x1ac12ca9, 0x8cc74e7c, 0x4199ab66, 0x72450e7c, 0x17c1414b, 
    0xda67b833, 0xe6435aae, 0x221c3b2b, 0x63ab9a8c, 0x95e6361a, 0x96817084, 0x757ee076, 0x9578ae8d, 0x3ba1081c, 0x64e05c62, 0x95de4a7f, 0x56673627, 
    0xdd8686b2, 0x524c88c7, 0x41148894, 0xffa202c0, 0x6f968400, 0x83ebfc56, 0x7e2c5b7d, 0xfd49e56f, 0xf2d7feab, 0x6200ffae, 0x32db3f8f, 0x4faa37bc, 
    0x75f2865a, 0xd45cf3c2, 0xd72db91a, 0x752fa9f4, 0x7356be50, 0x9aaff4c9, 0x579f6951, 0x3f774fcc, 0x7dac79cc, 0xc6bc5342, 0x4e305241, 0x2ddcd26a, 
    0xff71c4d5, 0xf4c27d00, 0xf1a438f5, 0x6bdaa395, 0x56c03073, 0x7f0e80db, 0xad00ff87, 0x8b7a115d, 0x15a4b9c8, 0xdfce3d8a, 0xa35e61fd, 0x693e8de0, 
    0x30209b47, 0x4f75df44, 0xdae923df, 0xb541dbbc, 0x90a54806, 0x46129b65, 0xdc31f0a0, 0x5e78afd7, 0x20ab48f4, 0xc4dc80e5, 0xd4abfd8e, 0x976793a0, 
    0xd1466d8d, 0xf5b4083b, 0x10aafc11, 0x9ad4711e, 0x8157f1c4, 0x33b75fd3, 0x0b35bdc3, 0xe5c5d878, 0x02a4f7e3, 0xeff11b55, 0xd0b5055d, 0x3edf0a46, 
    0x2d57e979, 0xbbd4962f, 0xc8862431, 0xdad17d00, 0x4a58bebd, 0xf3ca6736, 0x8fae84d4, 0x4fe58637, 0x0c90a385, 0x3d381113, 0x2af86145, 0x599bb579, 
    0xcfbe95dd, 0x9fd3706e, 0x07d97fba, 0x4eaff6b9, 0x1a84f0b8, 0x4f6dd3ac, 0xbc90c926, 0x2c39ef91, 0xaed75b4e, 0xe52a7f7d, 0x74adf835, 0xbb34485b, 
    0x0af63605, 0x2511c54a, 0xc949be8f, 0xa3c86bfd, 0x9f6f7384, 0x7cad9e6b, 0xb397607c, 0x59bce0dc, 0x4ff7c0e3, 0x82611667, 0x7a66b703, 0x9d5c539f, 
    0xae7789c7, 0x982b9077, 0xbacc05f1, 0xfc2f899f, 0xec4cb983, 0x4e0790ab, 0x596ba53f, 0x12613ef8, 0xb694a5fd, 0xbfb77632, 0x06bdded3, 0xc47249a1, 
    0x579d8be1, 0xae5322cd, 0x0ed849c8, 0x27b73fda, 0x746135f4, 0x7f1c833d, 0xd8bf54df, 0xb0ce4fdf, 0xbb6c3ae6, 0x489b251c, 0x959eef1b, 0xe3915763, 
    0xcb63d916, 0x7c956d53, 0x6abd3de0, 0x02a0105f, 0xf0eafdd3, 0xf7f4a9ed, 0xdaf4c212, 0x165c5bdf, 0x924ab12c, 0x090cf630, 0x9ee955fe, 0x4992b73b, 
    0x8a746b12, 0xd975666b, 0xed042320, 0x1af8f320, 0x83380bf3, 0x07d373cc, 0xaa0d76b5, 0x4c3ff8a3, 0xe2d87cd2, 0x94b1d08e, 0x8cdbcabf, 0xd3e73092, 
    0x0e0eb50e, 0xcbb74553, 0xa7861669, 0x676e9d06, 0xb7c8e396, 0x71f29253, 0xa1cff6b8, 0xd583ab3d, 0x356800ff, 0xac0df1b3, 0x2dc81316, 0xcc7493e2, 
    0x2526ace4, 0xf7dc61c1, 0x24070303, 0xfc93d77a, 0xa8a1f843, 0x00ff8eeb, 0x4400ff65, 0xdd68b6b3, 0x965fea62, 0x38799ee7, 0x0fd5fa18, 0xff1e7807, 
    0xb1b6e200, 0xfbe8a065, 0xb8e46fad, 0x5238cd13, 0x7c121044, 0x24460fc2, 0x731c2085, 0x1f07c893, 0x42c2826b, 0xbb4a1d84, 0xcc8d279f, 0x2a7b591d, 
    0x3ef77a3d, 0xc48ff08c, 0x111f89af, 0xa16f417c, 0xf0ec5378, 0xb7348a89, 0xfb244613, 0xa79e2d34, 0xbf00cb71, 0x9a03e75e, 0xbac107fb, 0x0d9ae80b, 
    0x38a7be95, 0x947cd6bc, 0xbb809b4b, 0x6eb4cb54, 0xb2f97620, 0xdfbce27a, 0x7dc387e1, 0x948ee013, 0x5e1756f6, 0x31e4fd6d, 0xc34a53b4, 0x062093cd, 
    0x281f7700, 0x5e91fa24, 0x0578e1bf, 0xa76b06cc, 0x8383cdcc, 0x6afd7bd4, 0x4aa95571, 0xa1358da2, 0x87380ac1, 0x38f43607, 0x577c1acf, 0x2c3f3cd1, 
    0x069a2eda, 0xf247aeaf, 0xb56fb435, 0xc3199323, 0xd871e8b9, 0x05aff067, 0x2f8eeff1, 0xb5d92f2c, 0x5a1bb87b, 0xd3213296, 0xadf2c92c, 0x431e8e14, 
    0x0f3cb7d4, 0xed27ebc7, 0x2778e70d, 0xfcf08bf6, 0x58e5ef96, 0x441688eb, 0x7ef30152, 0x71ae6fe8, 0x64f77ccd, 0x96e719f3, 0x994bdee5, 0x49326f66, 
    0x3c5b2c09, 0xc9724d92, 0xdfd624b9, 0x4669bca9, 0xd4b79b8e, 0x89879ff6, 0xa27789f6, 0x8c231c01, 0xce7fac8e, 0xc965be6b, 0xbce78a6c, 0x339da40b, 
    0x402eb741, 0x04e6c695, 0xa73f23f7, 0x2213ba02, 0x06486086, 0xeccac9bc, 0x15a221f4, 0x47f78142, 0xfe854b53, 0xd4fca8e8, 0xf3a5dec7, 0x96d5fb53, 
    0xdb3e9f85, 0x85f6c7de, 0x5643c7f9, 0x8f7041fc, 0x7d1830f6, 0xb52de1ea, 0x7f076752, 0xb5b6c66e, 0xfbcc4b5d, 0xf9f9381f, 0xbfd6fb24, 0x5a477756, 
    0x9a74a6c4, 0x9b86f555, 0xac5a9734, 0x8db265bf, 0x32f52b0b, 0xaec2c130, 0x1d796009, 0x575cefb9, 0xfcc9358c, 0x3cdcac4d, 0x976cc936, 0x540a675b, 
    0x13aa6096, 0x95d761e6, 0x9ec38ee9, 0xe55a7d2a, 0x37120975, 0x80dc4839, 0x31800a0e, 0x35753ace, 0xd6a2e289, 0x15612e9e, 0x4285b1c1, 0x3502b7aa, 
    0x1ee0995d, 0x24fe6c39, 0x3061d1d7, 0x56e9b034, 0x71f0ac9e, 0x5bb5ea78, 0xe6a117a7, 0x876633be, 0xe5d3be56, 0xc9d1b484, 0x1b4875f6, 0x9b805196, 
    0x851f0e4e, 0x80040f1d, 0xfbd57d05, 0xdb2af83e, 0x7fc367e0, 0x7d544ded, 0x21bec6a7, 0x99bdb588, 0x05f2712a, 0x586e9c04, 0x393975d4, 0x3f5f7162, 
    0x3ec11ff8, 0x23fed71f, 0xd16aae43, 0xc36bf8b3, 0xc6facad6, 0x91af32a1, 0x23a83271, 0xe4507e2e, 0x92cd0de4, 0xaec97176, 0xc577e1d7, 0x77e25b4d, 
    0x112f758a, 0xb4e0216a, 0x9bfd5392, 0x4d3e6ea7, 0xab62b085, 0xe1a06314, 0xf13e6241, 0x99c735c9, 0xd606ad62, 0xb0ca34d7, 0x3426be4a, 0xa400ffe6, 
    0x5fe83277, 0x5c253e10, 0xdaf8f16b, 0x59d34ae7, 0xaf745d2c, 0x893bfbec, 0x6e12a515, 0x10f2728d, 0x7177e391, 0xf55a8f8c, 0xfaf785af, 0xa3f095f6, 
    0x2a4fb75c, 0xecd05beb, 0x42e6160c, 0xc42a33c6, 0x6cbe19fb, 0x8083dcec, 0xf7e0c141, 0xb600ffa9, 0xec143ae1, 0xa233e32d, 0x5bd02671, 0x3b2be818, 
    0xad8a87e1, 0xdb1a7fe3, 0xe70d344d, 0x3cdb48d8, 0x23f890c1, 0xbce6f518, 0xa78c5648, 0x619ff815, 0xd0e8085b, 0xd6d2a3a8, 0x1f1fd5f2, 0xd65afbf8, 
    0x68243e6e, 0x9b5caa8f, 0x82b8a3e9, 0x49dceee1, 0xc8202891, 0x50925c88, 0xafd69f1e, 0xdf9268db, 0x4c9f467c, 0x92b70a07, 0xb861c76f, 0x5f2afd96, 
    0x3741738c, 0xd98e228e, 0x5a381b50, 0x90f16866, 0x3a0ef0c4, 0x677a0570, 0x032d3c84, 0x75d74ffc, 0xe4535007, 0x8c233696, 0x0f8ea1c8, 0x67d7f9e7, 
    0xec3bb5b4, 0x9422ee7c, 0x2ec65963, 0x6f877ee9, 0x8528950e, 0xe34a5923, 0xc3d7d3d8, 0xdbb97397, 0xffd8d771, 0x406f8d00, 0x65098290, 0x4b7febfc, 
    0x6f11b7b7, 0xa6278f66, 0x9f57e048, 0x66aa25cd, 0x178e9d52, 0xd38b10fe, 0xeb47dacf, 0x0800ff49, 0xfcfcef3d, 0x37bdd68f, 0xcd07b665, 0xd928e617, 
    0xbfe8fd6d, 0x4e3d4731, 0x1fd13ee6, 0x7097d69e, 0x8e6207ef, 0x4896a02b, 0x856c5b66, 0x7ebf1b50, 0xdbb7986b, 0xfac9381e, 0x6bec567b, 0xf9acc6e7, 
    0x15187901, 0xb349f2f4, 0xb67b3386, 0xef58b558, 0x252e2fb5, 0x973337d8, 0x3f27c71d, 0xb4eb6afd, 0xf85b05ef, 0x2fed68ca, 0xd8744bd2, 0x5ca4cb7c, 
    0x8824a833, 0x07f1fef1, 0x0f0facf0, 0xf164fee9, 0x0c8ef94c, 0x790e9c61, 0xfcb6e218, 0x34b9e243, 0xb0e2488b, 0xa4dcbe95, 0x6b1f6830, 0x3839e70c, 
    0x7aa7e3f4, 0xa09484ef, 0x5b39b7e0, 0xe799b9f7, 0x207e1a7f, 0xf8fc768d, 0xed584877, 0x14794bb4, 0x04b60dc9, 0x051c226d, 0x41c1fbc8, 0x93d83d39, 
    0xf8d90ad0, 0xa6c1ac27, 0x0a66b030, 0xfbdf7617, 0x5a3f90ec, 0x8feccff2, 0xcedb44b0, 0x775b50e7, 0xfcc1c8a8, 0x36c1736b, 0x823a1737, 0xcb857714, 
    0xf0af1284, 0x7fbede93, 0x279da430, 0x297bcf16, 0x88582aad, 0x36fb48d4, 0x5037c4cb, 0x44d0425b, 0x38fcc493, 0x075ec177, 0xbd9a8323, 0x6daaf8af, 
    0x97d7c103, 0xe1e9a7a9, 0x73cb1204, 0x9b377224, 0xfe312bb0, 0x78720b10, 0x327861cd, 0x41206c39, 0x2eece23c, 0xd8013139, 0xe26bd77b, 0x7c53878f, 
    0xdba2f050, 0x9e1d7bc1, 0xf14224c5, 0x54c82ff1, 0x0ef02029, 0xf0e72b7d, 0x7bb3e796, 0x54996b9f, 0x975f698c, 0x0c6de353, 0xea5dadcf, 0xb404d757, 
    0x12bb44d7, 0xa04f4f7d, 0xa1aff2e3, 0x17722d3c, 0x63b8b342, 0xe2add9fb, 0xa8fc7bdc, 0x9157fa17, 0xef2be14f, 0xf6ddfa87, 0x35dbaa93, 0x115399b4, 
    0xcfcb2a27, 0x77c5dd2a, 0xc5b23776, 0x18c5d9a5, 0x4685631b, 0x7aadcf33, 0x3e76b595, 0x73c39736, 0xded5b9a9, 0xe390d81d, 0x5757eb19, 0x410c0dc5, 
    0x4721984c, 0xf5d9f37a, 0x39959016, 0x74fe1c07, 0xf9d6dbdb, 0x05e86380, 0xdb98c867, 0xf1197aee, 0xf37e6063, 0x00ffa4f5, 0xe7bfb184, 0xccebfca1, 
    0x7edcdc4d, 0x3e298ef0, 0xf5fc37d7, 0x423e953f, 0xb61e8f2c, 0x6464a45d, 0xa74bd7fb, 0x04a2195b, 0xf779269c, 0xdaa79115, 0x25667e09, 0x71ad1557, 
    0xaeda907c, 0x9ea00a00, 0xfb5ee978, 0xcd9d8267, 0x8b564b0d, 0x7020b44d, 0x77140141, 0x4ddd5cae, 0x49ce3277, 0x27a95ff3, 0x55fae3b0, 0x539f2e59, 
    0x40782ebc, 0xfcaa1d78, 0x46b36cf0, 0x083da904, 0x9bd29afa, 0xd1b58ab4, 0x2546af89, 0x962d2f8a, 0xb3f6f246, 0x8cc4fb2f, 0xae40608f, 0x86cee1d7, 
    0x91134ac7, 0x167349f3, 0x57185edf, 0xdbeb2e05, 0x0bc793a1, 0xf8b557f8, 0x4b0bd412, 0xa3110334, 0x78cd3d02, 0x245bbd99, 0x1d19f591, 0xcfb63915, 
    0x4510d260, 0xc7710008, 0xf04aaff2, 0x5558e17d, 0xaf007072, 0xe846d339, 0xa75de1cf, 0x9f286486, 0xd0cc574c, 0xd747e693, 0x9b4e10e6, 0x7e8f8f39, 
    0x3ec48f1a, 0x5174921f, 0xde5fbbf6, 0x3d72e0c4, 0xe98fd047, 0x7a692d5e, 0x38114b44, 0x7d0ed478, 0xe21f7d85, 0xa4ac2e62, 0x0d1ee446, 0xabe2d77c, 
    0x5aad3419, 0xb3765257, 0xfa0adc1e, 0x131ff31b, 0xf062556e, 0xdc939c75, 0xe770a90a, 0x7bd56ccc, 0x99afbc7b, 0xa5d2719e, 0x4eaac491, 0xc9486846, 
    0x41e554b1, 0x7abc943e, 0x0367881a, 0xd57c941e, 0x1b759e64, 0xe0a88c94, 0xb31e560c, 0x47102977, 0x38c185b8, 0x5fda5a1f, 0x7385dfba, 0xc8fac7fa, 
    0xb5fedd3f, 0x2e3cf5ec, 0x839a8686, 0x91710c72, 0xcbb71a5b, 0x8f7aa462, 0x4d236be6, 0x15f5e9fb, 0x00ffabaf, 0x477d3cc8, 0xdc7135f3, 0xe61c3bd6, 
    0x99cc8096, 0xbcdceb1d, 0xda86001c, 0xff4a3fbc, 0xee955c00, 0x00ff0f1e, 0x335f6b55, 0xb48fee9c, 0xaf9e3ac8, 0x5dc3fda5, 0xebbe8777, 0xd2902b7d, 
    0xfe75e5fe, 0xf4adfb1f, 0xc4870aaf, 0x7cc7a68f, 0x39578f0c, 0x3cc7eb85, 0xaf1c6d79, 0xec15e536, 0xa7fab75a, 0xc7f122af, 0x5fe99bdf, 0x898f0d43, 
    0xf6e6b9a9, 0x3eb055ae, 0x00317895, 0xfbb7cf9a, 0x5f8d00ff, 0x9c5a13fe, 0x862a2e92, 0x14ecd21c, 0xd469f747, 0xd9ff3fc9, 
};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(RadicalBlurEffectNode, "Radical Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Blur")