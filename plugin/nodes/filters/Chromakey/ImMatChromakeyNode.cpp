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
        ImGui::ImDestroyTexture(&m_logo);
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
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\ue2ca"));
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
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 4247;
    const unsigned int logo_data[4248/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xfc003f00, 0x288aa2aa, 0xeb8aa200, 0x387c193e, 0xe29ff8bf, 
    0x197bf478, 0xbc31d752, 0xac4c5db2, 0x75548963, 0x25190762, 0x3dec9e57, 0xd338276a, 0x929d9c8b, 0x554ea734, 0x5dc1a966, 0xbfa293b3, 0xfc1dfe43, 
    0xe179f804, 0xf0628965, 0x2aabbeed, 0xab95fe81, 0x6e46e262, 0x1f6d04e7, 0xe92b10f0, 0x0cbe87df, 0xa5a9a6f0, 0x9de1111b, 0xf56fc823, 0xce381b91, 
    0x79c5a8ef, 0xa6c6c7b1, 0xb69e8eb0, 0x5cd98a3d, 0xa9e6f1f0, 0xfc5fb92f, 0x2b3ac58f, 0x87e3e3f5, 0xe1f713fc, 0x4dc400ff, 0x0ddf5b16, 0x6ba0c9e8, 
    0x21cc4630, 0x9ba0d0d2, 0x0eaa70ab, 0x548e9358, 0x5ac781e7, 0xd7f8c8fc, 0xf85a6ff0, 0xfd5be331, 0xbcb35313, 0x795c288a, 0xbcdbdc52, 0xa8646b67, 
    0x6d3eca6d, 0x18753ca3, 0x945df13d, 0x5abba9eb, 0x0d3a9bc7, 0xeb0e9e43, 0xf3a85ef1, 0xbaa228ea, 0x8aa2504e, 0x8aa20028, 0x8aa20028, 0x7b4b0228, 
    0x22ee2e79, 0x4b321482, 0x1d44042b, 0x15c08959, 0x2fc36ff7, 0xcff0ed87, 0x19fed6c2, 0xa50681b9, 0xe53d4913, 0x235321c2, 0xcfb912b3, 0x1fcb733f, 
    0x67f91af0, 0xa2f071f6, 0x17e3bff8, 0x50e46c87, 0x9370c7d0, 0x0eae5c3e, 0x1e2354e5, 0x807ec88e, 0xc74fe8d7, 0x3b5d164d, 0x72a9aee2, 0xfc295681, 
    0x3f1b8a90, 0x7f47c1f2, 0xa67c4df7, 0xe1f78873, 0xdc67fe42, 0x7b1de670, 0xdfaeedd5, 0x79fd2f2d, 0xa02dd10d, 0xdc5c80b3, 0x20753748, 0xeeb5ba9f, 
    0x6b74093f, 0xe0faa6d0, 0x5fd2361d, 0x269e71f5, 0xa790eb75, 0x79ea83ee, 0xc9eb1df4, 0x27a62b3c, 0xc8cd059f, 0x9e9523df, 0xa5f014dd, 0xbfb7774e, 
    0x3d47324e, 0x0fb4dc2b, 0x09d07910, 0x46959694, 0x8d5e66f7, 0x4aaf3f9e, 0xe95ac3e3, 0x99a787be, 0x40727273, 0x781e4df6, 0x0456d16d, 0x7127554b, 
    0x71ce7240, 0xbcd693d4, 0xfda9f6af, 0xda6ff49a, 0x8d1ac11b, 0xbcadefc7, 0xdca56555, 0xa82d63c3, 0x1313de5c, 0xb84950f9, 0x8691371e, 0xef88a1ea, 
    0x896f5a5b, 0x76ec120c, 0x827337f3, 0x562c7fbd, 0x9b8db7e5, 0xd859424c, 0x16ca6db0, 0xa00e7630, 0xaff7f473, 0x9d52a7aa, 0x1f7bda58, 0x5aa3d49f, 
    0x7fe0f984, 0xf0247e3e, 0xb585f7dd, 0xe8dbb499, 0xc1842b5e, 0xf546ca50, 0x7bc02a23, 0x51a7cb8f, 0x2afbef5e, 0x209ec6fe, 0x559fa6fd, 0x6cafd59a, 
    0xbe302c2c, 0x92825a7a, 0xb5fa47c8, 0xe1469991, 0x1d0980d4, 0xbea75ff1, 0xfc94fd25, 0xc633f109, 0xd5201e56, 0xb26252f4, 0x16852659, 0x9e6574fa, 
    0xc2212d46, 0x9c5805b1, 0xc6339e9c, 0x80f77a05, 0x6fe13b7c, 0x87169a83, 0xd1065de1, 0x4f1dadb4, 0xbc6dadd9, 0xdcc026c9, 0xccce8258, 0x73524fdd, 
    0x4e6a4ccd, 0xf9665ca4, 0x6bfad25a, 0xd2dcafe6, 0x6a46a571, 0xebe95c50, 0x9d3c7d67, 0xb7feaaad, 0x37be043f, 0x00ff357c, 0x47f13f85, 0xbb71f05f, 
    0x225df29a, 0x2dcbb37f, 0xde90bfc5, 0x6db85160, 0x4802dcdc, 0x1e8c3c07, 0xb957c3f5, 0x34bedb7e, 0x00fff1b6, 0x1ff151ed, 0xd62cb259, 0xc3d26a92, 
    0x256c3a13, 0xc759c462, 0x00ffb3fd, 0xd78657f5, 0x5e49746c, 0x349f14e7, 0xbbbcb59b, 0x54511405, 0x14455148, 0xb2dfea01, 0xc39788f7, 0x3dbc1a1f, 
    0x71b4e07c, 0x8e3ab2cc, 0xa42c1beb, 0xfd9a517e, 0x6da1fd41, 0x56d7bea5, 0x8a4d00ff, 0x964debda, 0x958bfc58, 0x89b8432a, 0x9dee8695, 0xc8aff29b, 
    0x7a6f879f, 0x3fd03b9e, 0x4d6f86b3, 0x3d184bf4, 0x0f02580e, 0xdbe704b1, 0xda70fa35, 0x2f7c72fc, 0x0486e7f0, 0x9906c1f0, 0x97c935e6, 0xd398272b, 
    0xd3e7e0ce, 0x7cc5c96c, 0x53877366, 0xdc9a53c5, 0xa482bcfb, 0xa24f42a3, 0x7f35f976, 0xfe18f1d3, 0x2dd3b620, 0x5fd7c48d, 0xb843b666, 0xbddb29c9, 
    0x7a62c1b1, 0xaf76d813, 0xed27f12d, 0x9af8d034, 0xb94bdfce, 0x2ec35a98, 0x5242daf9, 0xb8e00a03, 0x0039d8ee, 0x1ec98601, 0x137fe3b5, 0x9eea2e3e, 
    0x6973ba2a, 0xdaa783fe, 0x3d2472bc, 0xc9b09c4c, 0x032739cf, 0x21fddc07, 0xfbc34ff0, 0xbdfa8aef, 0x86a3838e, 0xb924d4b5, 0x144e1317, 0x27016f91, 
    0x62f420cc, 0xc7015248, 0x71803c39, 0x152cd8f4, 0x5a1d2c3c, 0x8b335fab, 0xf33a991f, 0x9f7d58f6, 0xd087fe5e, 0x9ff8121e, 0x6fe267f1, 0x3dfced88, 
    0xb6b5b7e0, 0x358d825b, 0x5f42c4d5, 0xb9c540b2, 0x70eeb82d, 0xe439d717, 0x6d5f73e0, 0x1fd13efc, 0xd31f5ec3, 0x3f6ed6e0, 0xb0403cb4, 0x41779746, 
    0x144cab76, 0x653b606f, 0xbce27ab2, 0xc36fe1cf, 0x66e00f5d, 0x9b862591, 0xde5fe675, 0x3c451bc3, 0x32d93cac, 0x19076000, 0xfa24281f, 0xd15eed91, 
    0xbd249be0, 0x6dbcce8f, 0x1e1c39f3, 0x9afa39dd, 0x8c5a15cf, 0x594d25aa, 0xe2687c1e, 0x2da51c5c, 0x7fc4530e, 0xe18706b4, 0x41eb52cd, 0x221ea3d1, 
    0xba3f4cf1, 0x972f8936, 0x9091120c, 0x0041cc92, 0x7b82b2e3, 0xdfca571c, 0x41ed07b5, 0x96307cab, 0x6a10af6b, 0x3c3eb7d6, 0x7478b3ba, 0x2ce90ecf, 
    0x8c8eae45, 0xd8990ba4, 0xb8415e92, 0x7e8e4394, 0xde398063, 0x9ba2a9fd, 0x71ed87e1, 0x699bcbe0, 0xc437ad7c, 0x1a4bb1c8, 0xe5bfbf10, 0x9debf115, 
    0xfc0afbbc, 0xfd7df1bb, 0x8de2a5de, 0x79fae256, 0xeade6e2e, 0x31b32c43, 0x72dc6666, 0x75fe3c49, 0x28ad8330, 0x754bf6ca, 0xaa3a73e7, 0x296e4a38, 
    0x91af4f5e, 0x348d2c95, 0x334bec8c, 0x4d4e921c, 0xd32b8a36, 0x8aa2503c, 0x55a50028, 0x1254c12c, 0xbd03c049, 0x0e7f7425, 0xf151532c, 0x208700ff, 
    0x1b481699, 0xab12b651, 0x652865a0, 0x8207d950, 0x291e7a30, 0xe1726537, 0xae28791e, 0xb37fd4a7, 0x07adc2d7, 0xe2af14e1, 0x3e5d1ebf, 0xc9c8169a, 
    0xb315ae14, 0x5cdec82f, 0x93670551, 0x15249420, 0x07cf3819, 0xf1417c30, 0x2fc4d78d, 0xdb5cdf18, 0xc7a745d8, 0x8b772ca9, 0x70644219, 0x319fb088, 
    0xc6dc90d4, 0x15feb870, 0xb915e339, 0x3fea1ef1, 0x6d69ea10, 0xf6d68c3c, 0xa351bc73, 0x6a2cd190, 0x18cb2717, 0x3f609753, 0x579c9c11, 0x9f437c9f, 
    0x05e317fb, 0x7f5f30c4, 0x1a475a64, 0xac4a6e2c, 0x0c61c631, 0x70f9a976, 0x9ec49f5c, 0x2d43d1f5, 0x1d2b1e86, 0xeda4f788, 0x077e7afe, 0xcdc689ab, 
    0x69e8e049, 0xd97dfe08, 0x8dc9f8c3, 0x69a9bda6, 0xe25ac073, 0xa9cabe19, 0x8b2ec36c, 0xc1d61e81, 0xa1a3f0c3, 0x15006218, 0x3dfb05fa, 0xe0df1a78, 
    0xa941c347, 0x5eedc3ea, 0x451cf137, 0xa7b2eced, 0x090abb1f, 0x28b1dc38, 0x584e4ec6, 0x9ecd579c, 0x865ff001, 0x62f14d7c, 0xa9ea101f, 0xf011de24, 
    0xaf6d9cad, 0x7daa926a, 0x10e5ee9a, 0xd8091960, 0x37906324, 0xbebc2530, 0x3f7a4d32, 0xfa8a2fc2, 0x127fc5cf, 0xd5287eea, 0xa7eded15, 0x4c339d94, 
    0xeb74f2c9, 0x0852c550, 0xa070e081, 0xb2791fb1, 0xf9f3354f, 0x72dd2f9e, 0x9efa375b, 0x55815396, 0x37a531b1, 0x3bf2f67d, 0x880f74a9, 0x35ee123f, 
    0x736dbcf8, 0xb6ac69a5, 0xf64fba0e, 0xaac49d7d, 0x463789d2, 0x480879b9, 0xc6b8bff1, 0x477bad47, 0x7fcdebc1, 0xe247f84b, 0xaef22128, 0x4e03fdf5, 
    0xc5f9da30, 0x8965668c, 0xd97c33ca, 0xca07b9d9, 0x031c1ca4, 0x42ad83de, 0x16960b34, 0x38d199f1, 0x8cdd6893, 0x82df1474, 0x0db5287e, 0x6bda02f1, 
    0xb4f3778b, 0xe6475cc9, 0xc38043ae, 0x9a5fcf18, 0x845738bc, 0x277e45aa, 0xcac1d6d8, 0x8fa21e96, 0x54cb5b4b, 0x8ff11b7c, 0x9fdc1ac4, 0x3631fc16, 
    0xde2f7ab7, 0xf2daf2de, 0x2051425d, 0xd32a30d9, 0x12238d46, 0xffd05363, 0xf2b53e00, 0xb7678d97, 0xbd96f1ba, 0x358ba435, 0xbcf397ac, 0x74dd2752, 
    0x3d523132, 0x197dc588, 0x9e786cfb, 0x6b673c18, 0x91ee69a7, 0x9d45044d, 0x591d60e0, 0xe438389b, 0x08fc281c, 0xab96aff5, 0x743730e9, 0x9f6badb9, 
    0x43729c03, 0xdd41e912, 0x8aa22844, 0x28084ff4, 0x2b008aa2, 0x621df8be, 0xf411df97, 0x895492a5, 0x32299a63, 0x34b1e1e0, 0xafd3717c, 0xd56be05a, 
    0x6be802be, 0xd627fe72, 0x16d35be4, 0x97e7618f, 0xa1588db2, 0xc46dc1e5, 0x3dbbbf00, 0xe6017d32, 0x33f0abb1, 0x5e6509af, 0x0ffdf42d, 0x8aeffd5b, 
    0x67cd20be, 0x2d44de68, 0x7866b73c, 0x1f22dd48, 0x7b6c4532, 0xa0938f2a, 0xa8d77bce, 0x35e15f78, 0x52adc5af, 0xedd74ad7, 0x940fad6e, 0x0ca136cb, 
    0x89663962, 0x69837549, 0xaf723ece, 0xc64f749c, 0xd7c04fbc, 0xd3209e0e, 0x3583bbb5, 0x4b3c9bb6, 0x2911d714, 0x5ad83cde, 0xb4ebe920, 0xbb1db08c, 
    0xf8bed760, 0xcb37c43f, 0xbe627f3c, 0x42126f96, 0xe9dd33ad, 0x8a541e77, 0xc482d949, 0x87adfe11, 0xce91e3f0, 0xc229ea6b, 0xa572f035, 0xe458ef17, 
    0x471c659d, 0xf2cc96b5, 0xe21fda0f, 0xf49a6b72, 0x52d11bbe, 0xda0e3f0b, 0xacad9b4c, 0xb14c8836, 0x0399aa9d, 0x720b08e6, 0x87592e38, 0x7ed57101, 
    0xb5233ecf, 0xcdfea3d3, 0x3cbb4092, 0x00ff9ecd, 0xc90f24bc, 0xfdb5c0ab, 0x4a0f4f0a, 0xf31072f6, 0xaeb7b943, 0x5d16ba42, 0x04e701ae, 0x1d39f47a, 
    0x9fc14fab, 0x6cd6b86b, 0x2d798661, 0x84659225, 0x36d6c75c, 0x4ebf0f3d, 0xccf3eb9d, 0x94ce1576, 0xf5b1b7a9, 0xe1314e99, 0x6bad51f1, 0xa09f7af9, 
    0xcbabe26d, 0xb4a4682b, 0x07e68eb7, 0x260f643b, 0x828153d6, 0xd67a0e06, 0xc500ffcd, 0xc377f056, 0x4df652ad, 0x2511492b, 0x53f7c4a5, 0x8d9b3345, 
    0x72f01981, 0x6cc11680, 0x21577b2e, 0x742ee5f0, 0xbdbe40a8, 0x8f50515d, 0x1382ad99, 0x6bea81f2, 0xfbbff8a6, 0x00ffea3b, 0x2d806fb4, 0x7e6db4ed, 
    0x129241df, 0xd0ac2dd2, 0xca27b790, 0x53675465, 0x19bb83f2, 0x8373f020, 0x1cfcf98a, 0xdcb23e57, 0x13cfdcfb, 0xb95c144e, 0xbc263f35, 0x9f9be27d, 
    0xffa67819, 0x93ba5400, 0x56e29acc, 0xc9788e91, 0x1cf6c039, 0x7305ec01, 0x193fe9d5, 0xc378017e, 0x2f8987e0, 0x68138f34, 0x40b3e9f3, 0x4398c715, 
    0x49b7b1ac, 0x88c47123, 0xa1971b4f, 0x9bd7e0e0, 0x9743df57, 0xfcd8d196, 0x4ece7c66, 0x1485dd52, 0x51405651, 0x57001445, 0x9f1d7cbb, 0x8fe027fb, 
    0x9f516e8e, 0x5ad0f737, 0xe1b61148, 0x82e4da56, 0x63fde779, 0x26bcde71, 0xf5e0c7bd, 0xdfc12fda, 0x57ca5a1a, 0xe9489764, 0x8bf381b8, 0xe7c78079, 
    0x8af5f55c, 0x156647f5, 0xb3df51a5, 0x39eb8cfc, 0x69727894, 0x7217d5bc, 0xc4b1c45d, 0x08a1f239, 0xf1fe93e4, 0x0377a40a, 0xd92ff69a, 0xf2c400ff, 
    0xb8dd1a6a, 0x74c112b7, 0x699657e2, 0x400c326e, 0x4ecea963, 0x2be8a3e0, 0xe6d8b4c5, 0x2b122fbc, 0x60a8c33d, 0x420eced8, 0xb86e0782, 0x0ff45a1f, 
    0x79493083, 0xbc79ba78, 0x75e4d0bb, 0xdb7b4b92, 0xdc24f355, 0x31d99831, 0x3c874af7, 0xbd4fa10e, 0x8e2a987b, 0x63e91235, 0x1dafaccd, 0x6c189f0b, 
    0x1b35d563, 0xed2e6c49, 0xb06269b5, 0x8bab3b92, 0xfb860066, 0x19469244, 0x22a50c09, 0xd5118428, 0xfcac264f, 0xd5adf031, 0xb20e85c7, 0xd5b901ea, 
    0xc0b6e3ac, 0x9271e5e7, 0x55febc3e, 0xa7c6dae6, 0x1bafe379, 0x2896779b, 0x70638bd7, 0x54be9607, 0x3d76287f, 0x4f722049, 0x9bfa9a24, 0x9a92d6e1, 
    0x6b93e6a5, 0x73788460, 0x70e95d44, 0xdcdd42ca, 0x670ada6d, 0x772c28aa, 0xe9789263, 0x629c155f, 0xd1519352, 0x997ff937, 0x110ef9f6, 0x95349756, 
    0xf5d5d3f4, 0xc99e2bf9, 0xc659a2a3, 0x2c051088, 0x03d72309, 0x80177bad, 0x6cab8df5, 0xa600c881, 0x2bea7806, 0x745e04ca, 0xc7552744, 0x0c3fba02, 
    0x58de466a, 0x59198e91, 0x5f113f46, 0x8cab4a13, 0x443f9392, 0xb42a28cc, 0xfedd919d, 0xec061fdc, 0xccfe35fe, 0xb774241e, 0xe1417c59, 0x5d3549fb, 
    0xde5f7532, 0x34732d0f, 0xf1daf540, 0x29d4e3ee, 0xfa855fed, 0x56da848d, 0x706575a1, 0x95b627bb, 0x197d91a1, 0x627e0449, 0x016fa3bf, 0xda6a10ea, 
    0x42eed95f, 0x638a14cd, 0x32e43696, 0xf4088291, 0x47e1d720, 0xdef079ed, 0xc7afe1f7, 0xddd8159f, 0xa126cb7c, 0xda84233b, 0xe6cf0f09, 0xc5d1b70a, 
    0x9a858f7e, 0x68b68de5, 0x4d156ffc, 0x7ff42ba7, 0x5114e399, 0x9c077a45, 0x00455114, 0x3ef8df15, 0x5d045f67, 0x7d8418c4, 0xd7c33d4a, 0xad3f4cf7, 
    0xf8dd1570, 0xf944fe47, 0x25ec00ff, 0x6a8bfe1f, 0x36f57489, 0x463ff1a5, 0x65814e7a, 0x0033b412, 0xf8d68a44, 0xc2ade320, 0x4000ffe7, 0x83ae991f, 
    0xb7a7d45b, 0x04f1b9b4, 0x23d56125, 0x58d26d91, 0x21c42ed4, 0xa73e0672, 0xd0c67a9f, 0xfe47d5bf, 0x94feaff5, 0x1000ff55, 0x7b99c87f, 0x4b5f00ff, 
    0x98bd96fc, 0x490e1de9, 0xfee94c7c, 0x4613f816, 0xb576f1b7, 0x978b5a23, 0x66a4e886, 0x0ed8e7e3, 0x7d9d1f0f, 0x8ca0a525, 0x28b45120, 0x78b56300, 
    0xf96fc36f, 0xff41bc1a, 0xd19f5f00, 0xee34dd6b, 0x6d9ef8d5, 0xd4c67252, 0xddcfec4d, 0x8e702a32, 0x744b9306, 0xc9507a75, 0x3d00ff39, 0x25da562b, 
    0x0e060213, 0x6965cd41, 0xf4f94f5d, 0xf57f8bad, 0x48ae88bf, 0x13478fec, 0x2ff7d0d7, 0x6c3c9386, 0x778cc730, 0xf81fceaf, 0x36a42d2b, 0xa2511b7f, 
    0xdb5d2b8c, 0x71ca5cdb, 0x61277fc9, 0x5721d323, 0xa35f413f, 0xfefa0d7f, 0xffd5f955, 0xff700500, 0x61d79200, 0x165e00ff, 0x5703fadf, 0xe11f98e8, 
    0x7ee8fac3, 0x5669983f, 0x146df0a1, 0xf3e95e51, 0x501445e1, 0x00d9ff07, 
};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(ChromaKeyNode, "Chroma Key", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Matting")
