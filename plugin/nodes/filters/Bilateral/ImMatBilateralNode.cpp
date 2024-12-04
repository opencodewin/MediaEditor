#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Bilateral_vulkan.h>

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct BilateralNode final : Node
{
    BP_NODE_WITH_NAME(BilateralNode, "Bilateral Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Blur")
    BilateralNode(BP* blueprint): Node(blueprint) { m_Name = "Bilateral Blur"; m_HasCustomLayout = true; m_Skippable = true; }

    ~BilateralNode()
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
        if (m_SizeIn.IsLinked()) m_ksize = context.GetPinValue<float>(m_SizeIn);
        if (m_SigmaSpatialIn.IsLinked()) m_sigma_spatial = context.GetPinValue<float>(m_SigmaSpatialIn);
        if (m_SigmaColorIn.IsLinked()) m_sigma_color = context.GetPinValue<float>(m_SigmaColorIn);
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
                m_filter = new ImGui::Bilateral_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_ksize, m_sigma_spatial, m_sigma_color);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_SizeIn.m_ID) m_SizeIn.SetValue(m_ksize);
        if (receiver.m_ID == m_SigmaSpatialIn.m_ID) m_SigmaSpatialIn.SetValue(m_sigma_spatial);
        if (receiver.m_ID == m_SigmaColorIn.m_ID) m_SigmaColorIn.SetValue(m_sigma_color);
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
        ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        int _ksize = m_ksize;
        float _sigma_spatial = m_sigma_spatial;
        float _sigma_color = m_sigma_color;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SizeIn.IsLinked());
        ImGui::SliderInt("Kernel Size##Bilateral", &_ksize, 2, 20, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_size##Bilateral")) { _ksize = 5; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_size##Bilateral", key, ImGui::ImCurveEdit::DIM_X, m_SizeIn.IsLinked(), "size##Bilateral@" + std::to_string(m_ID), 2.f, 20.f, 5.f, m_SizeIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_SigmaSpatialIn.IsLinked());
        ImGui::SliderFloat("Sigma Spatial##Bilateral", &_sigma_spatial, 0.f, 100.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_sigma_spatial##Bilateral")) { _sigma_spatial = 10.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_sigma_spatial##Bilateral", key, ImGui::ImCurveEdit::DIM_X, m_SigmaSpatialIn.IsLinked(), "sigma spatial##Bilateral@" + std::to_string(m_ID), 0.f, 100.f, 10.f, m_SigmaSpatialIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_SigmaColorIn.IsLinked());
        ImGui::SliderFloat("Sigma Color##Bilateral", &_sigma_color, 0.f, 100.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_sigma_color##Bilateral")) { _sigma_color = 10.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_sigma_color##Bilateral", key, ImGui::ImCurveEdit::DIM_X, m_SigmaColorIn.IsLinked(), "sigma color##Bilateral@" + std::to_string(m_ID), 0.f, 100.f, 10.f, m_SigmaColorIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_ksize != m_ksize) { m_ksize = _ksize; changed = true; }
        if (_sigma_spatial != m_sigma_spatial) { m_sigma_spatial = _sigma_spatial; changed = true; }
        if (_sigma_color != m_sigma_color) { m_sigma_color = _sigma_color; changed = true; }
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
        if (value.contains("ksize"))
        {
            auto& val = value["ksize"];
            if (val.is_number()) 
                m_ksize = val.get<imgui_json::number>();
        }
        if (value.contains("sigma_spatial"))
        {
            auto& val = value["sigma_spatial"];
            if (val.is_number()) 
                m_sigma_spatial = val.get<imgui_json::number>();
        }
        if (value.contains("sigma_color"))
        {
            auto& val = value["sigma_color"];
            if (val.is_number()) 
                m_sigma_color = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["ksize"] = imgui_json::number(m_ksize);
        value["sigma_spatial"] = imgui_json::number(m_sigma_spatial);
        value["sigma_color"] = imgui_json::number(m_sigma_color);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\ue3a3"));
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
    FloatPin  m_SizeIn = { this, "Size"};
    FloatPin  m_SigmaSpatialIn = { this, "Sigma Spatial"};
    FloatPin  m_SigmaColorIn = { this, "Sigma Color"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_SizeIn, &m_SigmaSpatialIn, &m_SigmaColorIn };
    Pin* m_OutputPins[2] = { &m_Exit,&m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    int m_ksize             {5};
    float m_sigma_spatial   {10.f};
    float m_sigma_color     {10.f};
    ImGui::Bilateral_vulkan * m_filter {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 4832;
    const unsigned int logo_data[4832/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xf6003f00, 0xb8776b3b, 0x0a3c7193, 0x16da2ff1, 0x63b3bee0, 
    0xbdd61ff5, 0xbaf15b07, 0x61c200ff, 0x2267891e, 0x7616a23a, 0x1f9c0747, 0xfb695e43, 0x6df64b49, 0xcf0b4f6f, 0xf16bfd2b, 0xaf923bbc, 0x42e9f613, 
    0xa79c6dd1, 0xb7fd5887, 0xf2aec093, 0xc74060de, 0x0ebff34a, 0x957ce2dc, 0xdbda35c9, 0xb64ca8dc, 0x7742af00, 0xa237b3e6, 0xd3b42997, 0xabd213e0, 
    0xc619bd7d, 0xefd24e73, 0x1298b821, 0x26b09781, 0xdac083bd, 0x5bac953e, 0x09cd9a88, 0xfe1c43ee, 0x034fdb95, 0xd6fcaa2a, 0x655ebc3c, 0x2bbf320c, 
    0x3fdc8f67, 0xd6c50a4c, 0x3e4a424e, 0xa88f1cf2, 0x6b3ca1af, 0x8b1b52f0, 0xd168b52f, 0x0c5e66ca, 0xb3594671, 0x397214ee, 0xdeced7fa, 0x06ad5b31, 
    0xd81b6bfa, 0x7754de1e, 0x4da1e641, 0x405249fb, 0x5414e33e, 0xbbc354c2, 0x66c340db, 0x3d747c18, 0xd963afc7, 0xd43827bc, 0x24793b74, 0x35e3c2e5, 
    0x69db05a9, 0xc55cd177, 0x3dce36d6, 0xf0886b7d, 0xf0b1d8a5, 0xd80c37e4, 0x70921c8f, 0xadb66805, 0xf80ea1fd, 0xd1235e5d, 0x627ee490, 0x4af19abc, 
    0x54eb484e, 0x93ceacae, 0xd785bfe3, 0xbf255ac4, 0x260e6d8a, 0xdc4d5a0a, 0x0f0b8013, 0x50df70c5, 0xe57c457d, 0x5a175fe2, 0x97424378, 0xc0dcba52, 
    0x098ad829, 0xabfd7ddc, 0xe20f7de8, 0xbfe09f3e, 0xafe96a0b, 0xbe6912cf, 0xe6093a4b, 0xc8718158, 0x2003b100, 0xaf01fde0, 0xfe8adfcc, 0xf8187f32, 
    0x86b954e3, 0x70ebe94c, 0xd828dac2, 0x71d56332, 0x3dce09c6, 0x5dc65eb1, 0xf6ad7886, 0x31e2cb47, 0xa2722e4f, 0x5f43df95, 0x7ca0fd56, 0x2c7f7565, 
    0xb4805a96, 0x71f727b5, 0x8fb6d518, 0xf84f55c4, 0xff3b1e5f, 0xffc8a000, 0xfe29bf00, 0x079dc315, 0x5402b853, 0x4725afd3, 0x18485719, 0x288e3d82, 
    0x738400ff, 0xa000ff55, 0x03fef755, 0xd757f8bf, 0xac243dac, 0x789c8fa2, 0xc9ddedca, 0x24fc659f, 0xf1beb8be, 0x3237b784, 0xce782719, 0x5d433fe6, 
    0xc132ed27, 0x9f38ed35, 0x89b5fe55, 0xcf188af0, 0xc7681b89, 0x637fcfee, 0x47ed275d, 0xe9adab6c, 0xef9573bc, 0xb9cfaff5, 0xc84ab1a2, 0x925f2dfd, 
    0x3e3ce0cc, 0x3950b8f9, 0xde010027, 0x1e65bdba, 0x373be6ca, 0x669e9220, 0x1fc9f654, 0x04cfb9d2, 0xe9117f85, 0x4a20b7c9, 0x5ff441ae, 0x2a00ff98, 
    0x6822beec, 0x28fee8b3, 0x40a99077, 0x9e838b88, 0x5a9f3ea3, 0xaee4b4d9, 0xe25e9311, 0x2bf434f5, 0x59a3ed58, 0xf49c3b65, 0xf0e55e51, 0xc7ba48a7, 
    0x651fa7da, 0xbfca8f81, 0x1f7945c4, 0x59f5b485, 0x06e09662, 0xf4a0aa6e, 0xf8b8d7fa, 0x671f5a7b, 0x07dcb942, 0x61edb552, 0xf3d974ad, 0x873397d9, 
    0xd1630f24, 0x02058aa6, 0xfc192230, 0x3fe6154f, 0x0bc10fb4, 0x2d5a880f, 0x430bb3e6, 0x046d736f, 0x02d2ca93, 0xbf8d9536, 0xc0efb87a, 0x1e38f4ca, 
    0xf8a6ad95, 0xac7dca9f, 0xeee789c5, 0x25fcbfd6, 0x61798b71, 0x3289b49c, 0xf8172895, 0x5f33c648, 0xadd31951, 0x7cec590e, 0x0d5ba713, 0xf2815451, 
    0x1637cc2d, 0x3b750a5f, 0x5e30947b, 0x22c5a35a, 0xf1ba0a9e, 0xa2ecc75c, 0x87f8259e, 0x5a84fd05, 0x0c6fcf7c, 0x0767528e, 0x47c60604, 0xa8af0f1d, 
    0xa18ef535, 0xf14a77f0, 0x62299e60, 0x2077e931, 0x9809912c, 0xeb002e33, 0xd77a3ace, 0x0fdde071, 0x6c083fc3, 0xb64bd396, 0xf1ddca92, 0xca3b9f24, 
    0x9f816a5b, 0xdbcb9e98, 0x3e0cbcd2, 0xc673195b, 0xfad0d7be, 0x287b4eac, 0xebc372ba, 0x1fba6a2b, 0xb500ff94, 0xefef8ea7, 0x18de5a7c, 0xb5b2a021, 
    0xf1007645, 0x323f3d20, 0xd801fa39, 0x15fc9fd7, 0xf17565f8, 0xc430c513, 0x136713b1, 0x83e32303, 0x4f7ff9ed, 0xa5fdd95a, 0xd7f19535, 0x3b424dc7, 
    0x92914000, 0xdc065924, 0x20658c34, 0x7f197af2, 0xb5fefd4e, 0x16fe9dfd, 0xc10ff8db, 0x8fd148f0, 0xd981ceb5, 0x82615696, 0x721df601, 0xa792d26b, 
    0x4ccba581, 0xb93a61f3, 0x5b6b5f95, 0x1c1e77e6, 0xa0951ef0, 0x71f6f6e8, 0x6aacb1c4, 0x2b3d0838, 0xbd11fe4f, 0xdf7fd03b, 0xe2b6ae02, 0x9eaf3969, 
    0x67777273, 0xac14a1b0, 0xc2eff391, 0x121f5218, 0x59be7e38, 0x776be4c7, 0x751ba9f6, 0xfbf4305d, 0x7335fbbf, 0xcf260e1f, 0x00ff388b, 0x8dfc4d9e, 
    0x54b5bf75, 0xacf4d459, 0xffb300ff, 0x8757b300, 0x8fb4feca, 0xa85c93ae, 0xcc044ff3, 0x4c9ff8b0, 0xc8232067, 0x2e7a2c61, 0xc09f29f5, 0x3ea0afb0, 
    0xc5f6bf2c, 0x15a9b6ad, 0xca3986b8, 0x182e1285, 0x00ffba60, 0x7bbea6ec, 0xf2846cd1, 0x63dc9e91, 0x5b53bdd6, 0xf85fbaf1, 0xbe4dd346, 0x6d75d9b8, 
    0xedec9a37, 0x03970996, 0x1557af23, 0xa5bdecd1, 0xd1aea448, 0xb5e9468d, 0x47d0a353, 0x97be05f1, 0xfc799e01, 0xcd925798, 0xbee555fc, 0x5ea4fd24, 
    0xf42d713d, 0x99c29cf9, 0x7760430e, 0xfff19a03, 0x155f8900, 0x7b45fc6f, 0x60ec5624, 0x0b8889b1, 0xffe370d3, 0xf850af00, 0x9f57c00f, 0x4d4bf513, 
    0x5937c22e, 0x48d370e4, 0x0f8eeea3, 0x57e8fb5f, 0x1d2c78e8, 0x5a1d343c, 0xf1e153ab, 0x3fad93f9, 0xe76e4365, 0xf8151ed0, 0xe20bf197, 0x950ebd3e, 
    0x24e581e1, 0x0cb374a7, 0xe54746e1, 0xf500ffc1, 0x7eb4aff4, 0x6852e81a, 0xe1d3191e, 0xba3725d6, 0xe2a694d7, 0x87c43946, 0x9cd00792, 0xffe5157e, 
    0x1d7e0b00, 0xb4031f69, 0x945b20a9, 0x1bc665fb, 0x83e090cd, 0xfa192fb4, 0xc7bdd61f, 0xa94331c1, 0x48339742, 0xca7019b2, 0xefc1c841, 0x4aad8a51, 
    0xb4a61255, 0xc4d0f83c, 0xe8493938, 0xf823be72, 0x37e8a5f1, 0x5a3a69b2, 0x4aeb687f, 0xd5fd8c38, 0x71fdfced, 0x3fcbd7f8, 0x05ed3fb4, 0x7743a273, 
    0x1d7db1fd, 0x1b73b1ee, 0x6e9f1e43, 0xf060a4e0, 0x38c648ca, 0xa7d391e0, 0x6abf965e, 0xefe193e4, 0x367d44c6, 0xc55c8475, 0xe419d12e, 0x00ff9b7c, 
    0x9bf8baf8, 0xefe6e2c7, 0x4abb9ac5, 0x25cf5cf2, 0x7311c7e3, 0x0871b793, 0x5831b607, 0x9271d4d9, 0x659e2e7a, 0xc5c3513a, 0xd7b72bc5, 0xd98ff5c8, 
    0x7813c157, 0xebb3c7df, 0x41fa877a, 0x76575af3, 0xdc89791f, 0x0f24f1c7, 0x96eebece, 0xaa1ac3dc, 0xfd1baff6, 0xff077c98, 0x828f0800, 0x8f76ae6d, 
    0xa89f613e, 0xe4c49fe9, 0x82bd22fe, 0x6d5454e3, 0x5e5321b3, 0xb5a72a3e, 0xa5c76ea9, 0x12944a87, 0x656440ee, 0x9a27cd38, 0x5d5eb5de, 0x43720646, 
    0x679a7b0c, 0xf2fee1db, 0x65c775fe, 0x67e6ebd8, 0x350f7fcd, 0x62c62f7f, 0x1888973f, 0xd5fea9d7, 0x96daac36, 0x810c8c93, 0xe57c8dfc, 0xeefab2f0, 
    0xa70c885f, 0xcde8df7c, 0x98be26d3, 0x9f47a6fd, 0xf8bbe8ab, 0x577b324e, 0xa3945699, 0xb35b8a88, 0x555469e9, 0x79e67763, 0x6c2e866f, 0xda3c357c, 
    0x6bedba8e, 0x2e060869, 0xcc0ff340, 0x1fb8cf31, 0x7ccf6b86, 0x8f6be367, 0x788ef818, 0x5b5b61ed, 0x24e3c229, 0x045599ac, 0x5564d8f1, 0x3fea18cf, 
    0x169c5adb, 0xab36298f, 0x77278f2a, 0x8f7b46c8, 0x8b9779a5, 0xbcacd422, 0x59dae668, 0x5cc98b21, 0x38135230, 0xfa35e3f6, 0xa3280f6e, 0x551ca781, 
    0xf6f9595d, 0xc4579a61, 0x3b0895d6, 0x23be7224, 0x4dc45359, 0x655ef9a7, 0x22aa3acb, 0x00ff24f4, 0xabfcc9f5, 0xd900ffee, 0xdf56c1f7, 0x9a00bc06, 
    0x47fadf8d, 0x81187589, 0x2f94b4b7, 0x8d67e419, 0x805e3fc3, 0x0f3ecf57, 0xa9917ef0, 0x8ef8d878, 0xd2e708f6, 0x79daa874, 0x4bc0b824, 0x773b1987, 
    0x1d38d27d, 0xf4eac679, 0x113f86cf, 0xe27bfcaf, 0x89fbcbab, 0x5ff8b41d, 0x9f80b3c9, 0xe20fa2dd, 0x72069251, 0x8041c748, 0xf978050e, 0x72dd2f9e, 
    0xe574365b, 0x115b1538, 0x5867521a, 0xaff116de, 0xaf59758d, 0xf445ddb5, 0x180687f5, 0x4991246d, 0x2523b15c, 0xe89591db, 0xbdda7130, 0xaefac08f, 
    0x397ca3b3, 0xe3d3e2d4, 0xaafd65fb, 0x6b5e144b, 0x70a87236, 0x9cd483e4, 0x736aee73, 0x0a0d5bab, 0xf9b01667, 0xd1fe7ec1, 0xf8a3829e, 0x6d44ad67, 
    0xa61441fc, 0xe55fac30, 0x7fb87124, 0x85c2abf5, 0x57a4ca78, 0x5c9d7de2, 0x34a3b11f, 0x3d7cacbc, 0xd7555bf1, 0x5a8a5f75, 0xf9736a62, 0x44d24af2, 
    0x52ed3358, 0xee36caea, 0xbda78e63, 0x12c34772, 0x6d195f6b, 0x38c9e874, 0xb94fcc30, 0x00ffdb44, 0xaf5d118f, 0xd17addc6, 0x0969303e, 0xed08b16e, 
    0x4a6921af, 0x7386d911, 0xc20fab8f, 0xbe82dfba, 0xc5a71a0b, 0xa8abb508, 0xaf66b359, 0x50804397, 0xf73f0395, 0x8000ffc1, 0xdc0aa9d7, 0x3eb61c94, 
    0x2b461753, 0x7417c913, 0x8de943bf, 0x0f3dd61a, 0x88b2b54a, 0x548d21e5, 0xda31ec13, 0xbad77b92, 0x094110b8, 0x08b89db8, 0xf3976906, 0xc0412cec, 
    0x075fe915, 0xd51d15bc, 0xd49df6b7, 0x3fa56902, 0xed3c9221, 0xf315d91e, 0x2baa2af5, 0x66cd3dda, 0x2fc299d5, 0xf57aadc2, 0xc21acf44, 0xd233f85b, 
    0xfe14fe97, 0x9fbfeab7, 0xbe5e00ff, 0x050a4d9e, 0xf5833650, 0x90d8bf14, 0x2a7f717f, 0xf6c4597f, 0xa9ed9947, 0xbf243ff7, 0xedcf3b66, 0x2bcf175f, 
    0x79c47690, 0x00ffb1c7, 0x7e4a5f11, 0xbaa070d3, 0xe5db99d1, 0xa31ebb51, 0xe093f99a, 0x7ca9936e, 0xd664f130, 0xd1b6b5ba, 0xf1554e24, 0xe9ab7d80, 
    0x00b1da2f, 0x72bde2d2, 0x236bc142, 0x18e1c075, 0xda0a7ff2, 0xe8f62fb5, 0xe1d4c742, 0xfbc216b1, 0x259f7959, 0xf8af9fbf, 0x3b20adcd, 0x98a53216, 
    0xff6a2377, 0x9f5e3300, 0x1187efe0, 0x5b50eb78, 0x0c634a4d, 0xb867ba49, 0xda80c5ea, 0x80d1733a, 0x3cc52bfc, 0x1e49a91b, 0x9997bab6, 0xe020c7e5, 
    0x1589d493, 0x57a07ef0, 0xac3a68bd, 0x2d9f8e7a, 0x82c0a58d, 0x6506c1f1, 0x06c388e4, 0x3b509fe7, 0x8290ebd7, 0xa27484ad, 0x49f3e3fd, 0x7bc45156, 
    0x26cf6c59, 0x06f1f5f8, 0xbc89b529, 0x0e11a237, 0x6c79138f, 0xddbf71b1, 0x67e0dcb1, 0xd603e33c, 0x1e805fba, 0x55b0b625, 0x9cc295b5, 0x20df7f8c, 
    0xdff04a7f, 0x68b2e816, 0x6ecc9dda, 0x19992bef, 0xe67d9284, 0xea7df2e4, 0xeeda81ef, 0x38da5a6d, 0x304b68a4, 0x2b40c705, 0x1976dcf3, 0xdea09cca, 
    0x38e5d5c7, 0x8d8961cf, 0xdfe75e5b, 0xdd92f0bf, 0xda866c41, 0xc0369e25, 0xaedaf36c, 0x32e283f8, 0x17c04bf8, 0x69692df7, 0xcadb2367, 0x250052e6, 
    0xbe3eb6bc, 0xe16baed9, 0x966933a4, 0x17e62eca, 0x373f086a, 0xaa2bc23f, 0x3ff099f8, 0xe1c9f854, 0x9dccfe74, 0xd216a32d, 0x80d16dc6, 0xc898dc66, 
    0x4cdf3052, 0xfe7cad8f, 0xcd9e2b12, 0xe299799f, 0x5c0e0a61, 0x329e9fba, 0xa3f8d3ea, 0x46b5d555, 0x972bcddc, 0x8342dd42, 0xeb7dfe91, 0xf682afee, 
    0x0d257c3b, 0x294b00f1, 0x0f3b7295, 0x62fe8831, 0xfed64dbe, 0x00ff6a15, 0x2cef6e0a, 0xb172cbb5, 0xce513cbb, 0x186e9411, 0x152030ca, 0xd883736c, 
    0x5f7364e4, 0x551ff851, 0x1b7cd2bf, 0xe2d69660, 0x0fde744b, 0xffb621a9, 0xbda6ec00, 0x95d35a7c, 0x0a0f3e77, 0xd4cfa3dc, 0x8f4c75ed, 0xc0bc9f94, 
    0xb4be667e, 0x8ba379f8, 0x11d8095f, 0xfadde4c6, 0xd392f89a, 0xebab89c6, 0x968c75da, 0xccbd23d2, 0xc6388661, 0xea2bfd73, 0xbf8800ff, 0x839ff419, 
    0x2f2c0d1f, 0x0d6e702f, 0xed36799a, 0x72b463c1, 0x3e7fb27d, 0x46c4c62b, 0xb35b502a, 0xfda48e2a, 0x1ea61ecb, 0xfe9c3120, 0x5f64be14, 0x76e657e4, 
    0xfc6efbad, 0x09b5d44e, 0x3bd62cae, 0xe563563b, 0x8514eec4, 0x3f5ec0ed, 0x7faaf533, 0x00ffd9f0, 0xe700ff15, 0xfbfb1fe6, 0x57d800ff, 0xc84e2d4f, 
    0x3012eac7, 0x6f56fd34, 0xc323f813, 0x2581a39a, 0xce1dd1dc, 0x46e42779, 0x86da00ff, 0xd5f643bd, 0x1cbc78b8, 0x51e544b0, 0xb0596eed, 0xc1229033, 
    0xff70ad1f, 0x1ebc0900, 0xb40b7ff6, 0x99659e54, 0xa95c46e0, 0xfe4438cf, 0xed6f5d49, 0x5ff679b9, 0x1b63da06, 0xe745dede, 0xff7ca2d1, 0xba22e800, 
    0x9df96ef1, 0x0eadbf24, 0x97ad0cdc, 0x687b52d4, 0x78652f7c, 0x465ad061, 0xdfedbc95, 0xa0fb0622, 0x3599fe15, 0x75041feb, 0x0a6ab5ef, 0x29da099c, 
    0x080e0c01, 0x16e335c8, 0x1b714911, 0xca59aa46, 0xaff638e7, 0x0c67f845, 0x5e5af26a, 0x84b2962c, 0x82536662, 0x07402a17, 0x7ea53fd7, 0x8d9b8287, 
    0xf858ba44, 0x42ebcaaa, 0x5bc600ff, 0x15b52e58, 0x921cad36, 0xc3c8332b, 0xc718a98e, 0x7facdcb8, 0x9fe1931a, 0x42972785, 0xc145424d, 0xc7a96292, 
    0xf08f6024, 0xf5cfebfc, 0xc5eb464d, 0x1efb33be, 0xfb2a0cf1, 0x6e03a718, 0xafe96b72, 0xbe317a87, 0x82b6b0b8, 0x925d9a32, 0x798c5c82, 0x00ffd48d, 
    0xc557f821, 0x4d9b1867, 0x7daee8a8, 0xf68445c6, 0xbd92e6d2, 0x74d933bf, 0x6fc7fe08, 0xa6e3b20a, 0xec15b87e, 0x36d601be, 0x206fb22d, 0x113f4665, 
    0x79b14e5e, 0x45bd7c91, 0xd416be74, 0x29d1bc4d, 0xfccb2a38, 0x2a557cc5, 0xcc5cb2bc, 0xa0300bfd, 0x477652aa, 0x7c58fb77, 0x8df8b327, 0xe2efb9f0, 
    0x18fc5a81, 0x1f7465ed, 0x58b0ab33, 0x85a4fb9c, 0x5cba7f0e, 0x9ee13574, 0x7cec8512, 0x32c7a61b, 0x5a963f91, 0xb1983d45, 0xf635f527, 0x8fe4864f, 
    0xb9197ec4, 0x67ac72b0, 0xeceea284, 0xe04ff9d8, 0xfc0a7f70, 0xe2b1f8f0, 0x1ff8121f, 0xb4b0fac5, 0x13665426, 0x68e3b9a5, 0xb7a39261, 0x0a7fb90d, 
    0xcee9eafa, 0x6347399d, 0xd05c2af3, 0xa3cfe294, 0x3efc04be, 0x8877f1b1, 0xb66fd4a4, 0x2a165b22, 0x1f00be87, 0x5ee937fb, 0x785afb5b, 0xfc172fbe, 
    0x6021fb42, 0x84f1746c, 0x3090f14c, 0xfc991fa3, 0x6ff8a16b, 0xc1db56f1, 0xf1a70b1f, 0xda3ab556, 0x58184bdf, 0x9de014ad, 0x47dc86d1, 0xb9472ca8, 
    0xfb7ef95a, 0x309e37c5, 0xaed427d7, 0xe5ee8994, 0xa8471c2f, 0x853fd353, 0x5438f57c, 0x92948aaf, 0xd47a68d1, 0x54ef215c, 0xb33ca1ed, 0x2a91235a, 
    0xa5c3fe82, 0x1f16ed33, 0x822bfddc, 0x2e8a2ff1, 0xa179b5ec, 0x4e63d8b7, 0xcfacf539, 0x2fb54cf8, 0x445f67fa, 0xa3da46a8, 0x5fb95896, 0xfb757d46, 
    0xab47db26, 0x5b8e1f7c, 0x993fe7b1, 0x57cf2d1f, 0x22fb1fb8, 0x7ecf55fe, 0xd3b853de, 0x7cc348b4, 0x3f82028d, 0x8d7f12e0, 0xdfb1df74, 0xff9891fc, 
    0x7fa4eb00, 0xb9ee32fa, 0xfecfdb6f, 0xfa2fb43c, 0x03fabfe2, 0x00ff55d6, 0xe44863e4, 0x55b87fc2, 0xedd0563e, 0xc2518d63, 0xfd1fc0fc, 0x8d00ff00, 
    0x87a49a74, 0x4ba2d644, 0x4c64914f, 0xc1833cb0, 0x47e200ff, 0x371a58e5, 0x00ff95fa, 0xe800ff74, 0x557cb706, 0xeb2100ff, 0x3fe6fa6f, 0x06fd6af6, 
    0x7c34453a, 0x6c26fe94, 0xb6f028fc, 0x3be2739b, 0xc4ad89cb, 0x2d1b92b2, 0xbef435cf, 0xd176048f, 0x358244a2, 0x8151051c, 0x227c3f5f, 0xc59000ff, 
    0x425fe1df, 0xbf16bd69, 0x4deacd17, 0xea9b26e3, 0x5391e37e, 0x6953c182, 0x30cd3a74, 0xe2ecb566, 0x30903a51, 0x649d3f72, 0x69b175e9, 0xa27eb9fe, 
    0xf5d063b8, 0x714f5d31, 0xa13367f8, 0x5fed314c, 0xd6d27e2a, 0x6afc5b91, 0xb1413492, 0xf741609a, 0x91f37f93, 0xa3fa3abf, 0xfc54afe1, 0xdadfe52b, 
    0x7b4bfe77, 0xf32fbd7f, 0xff78fb4a, 0x961fba00, 0x8ced5f54, 0x1b00fff2, 0x1bdac4d8, 0xcb93ee3b, 0x95cd8afb, 0x07aea05f, 0x89165145, 0x7f206aab, 
    0x238446a5, 0xd51ee11f, 0xff36bee8, 0x6a7e9100, 0xfe27bdf3, 0xfd6bfd44, 0x76795cd3, 0xf3f4d4b0, 0xec92241d, 0xa5978e79, 0x1b5bacc3, 0x34efa2bb, 
    0xd8104b8e, 0x8bf09fab, 0x7600ffd8, 0xd1ecfb4f, 0x04f97fe1, 0xfccdfb27, 0x7bbd5aeb, 0xd9ff119e, 
};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(BilateralNode, "Bilateral Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Blur")
