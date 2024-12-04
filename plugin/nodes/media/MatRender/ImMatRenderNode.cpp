#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <imgui_helper.h>
#if IMGUI_RENDERING_VULKAN
#include "imgui_impl_vulkan.h"
#endif
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>
#endif
#if IMGUI_ICONS
#include <icons.h>
#define ICON_SAVE_TEXTURE     "\uf03e"
#else
#define ICON_SAVE_TEXTURE      "[S]"
#endif
#include <ImGuiFileDialog.h>

#define NODE_VERSION    0x01000000

namespace ed = ax::NodeEditor;
namespace BluePrint
{
struct MatRenderNode final : Node
{
    BP_NODE_WITH_NAME(MatRenderNode, "Media Render", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MatRenderNode(BP* blueprint): Node(blueprint) { m_Name = "Mat Render"; m_HasCustomLayout = true; }
    ~MatRenderNode()
    {
        ImGui::ImDestroyTexture(&m_textureID);
        m_RenderMat = ImGui::ImMat();
#if IMGUI_VULKAN_SHADER
        if (m_convert) { delete m_convert; m_convert = nullptr; }
        m_RenderVkMat = ImGui::VkMat();
#endif
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        ImGui::ImDestroyTexture(&m_textureID);
        m_RenderMat = ImGui::ImMat();
#if IMGUI_VULKAN_SHADER
        if (m_convert) { delete m_convert; m_convert = nullptr; }
        m_RenderVkMat = ImGui::VkMat();
#endif
        m_image_width = 0;
        m_image_height = 0;
    }

    void OnClose(Context& context) override
    {
        m_mutex.lock();
        Reset(context);
        m_mutex.unlock();
    }

#if IMGUI_VULKAN_SHADER
    void GeneratorTexture(ImGui::VkMat & mat)
    {
        if (!mat.empty())
        {
#if !IMGUI_RENDERING_VULKAN
            ImGui::ImMat cpu_mat;
            ImGui::ImVulkanVkMatToImMat(mat, cpu_mat);
            ImGui::ImGenerateOrUpdateTexture(m_textureID, cpu_mat.w, cpu_mat.h, cpu_mat.c, (const unsigned char *)cpu_mat.data);
#else
            ImGui::ImGenerateOrUpdateTexture(m_textureID, mat.w, mat.h, mat.c, (const unsigned char *)&mat, true);
#endif
        }
    }
#endif

    void GeneratorTexture(ImGui::ImMat & mat)
    {
        if (!mat.empty())
        {
            if (mat.c == 1)
            {
#if IMGUI_VULKAN_SHADER
                ImGui::ImGenerateOrUpdateTexture(m_textureID, mat.w, mat.h, mat.c, (const unsigned char *)mat.data);
#else
                ImGui::ImMat im_RGB(mat.w, mat.h, 4, 1u, 4);
                for (int y = 0; y < mat.h; y++)
                {
                    for (int x = 0; x < mat.w; x++)
                    {
                        unsigned char val = 0;
                        int planer = mat.flags == 0 ? x : x % 2 == 0 ? x / 2 : x / 2 + mat.w / 2;
                        if (mat.depth > 8)
                        {
                            auto val16 = mat.at<unsigned short>(x, y);
                            val = val16 >> 8;
                        }
                        else
                            val = mat.at<unsigned char>(x, y);
                        im_RGB.at<unsigned char>(planer, y, 0) =
                        im_RGB.at<unsigned char>(planer, y, 1) =
                        im_RGB.at<unsigned char>(planer, y, 2) = val;
                        im_RGB.at<unsigned char>(planer, y, 3) = 0xFF;
                    }
                }
                ImGui::ImGenerateOrUpdateTexture(m_textureID, im_RGB.w, im_RGB.h, im_RGB.c, (const unsigned char *)im_RGB.data);
#endif
            }
            else if (mat.c == 3)
            {
                ImGui::ImMat im_RGB(mat.w, mat.h, 4, 1u, 4);
                for (int y = 0; y < mat.h; y++)
                {
                    for (int x = 0; x < mat.w; x++)
                    {
                        im_RGB.at<unsigned char>(x, y, 0) = mat.at<unsigned char>(x, y, 0);
                        im_RGB.at<unsigned char>(x, y, 1) = mat.at<unsigned char>(x, y, 1);
                        im_RGB.at<unsigned char>(x, y, 2) = mat.at<unsigned char>(x, y, 2);
                        im_RGB.at<unsigned char>(x, y, 3) = 0xFF;
                    }
                }
                ImGui::ImGenerateOrUpdateTexture(m_textureID, im_RGB.w, im_RGB.h, im_RGB.c, (const unsigned char *)im_RGB.data);
            }
            else if (mat.c == 4)
            {
                ImGui::ImGenerateOrUpdateTexture(m_textureID, mat.w, mat.h, mat.c, (const unsigned char *)mat.data);
            }
        }
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_Reset.m_ID)
        {
            Reset(context);
            return m_OReset;
        }
        else if (entryPoint.m_ID == m_Enter.m_ID)
        {
            auto mat = context.GetPinValue<ImGui::ImMat>(m_Mat);
            if (!mat.empty() && (mat.flags & IM_MAT_FLAGS_VIDEO_FRAME || mat.flags & IM_MAT_FLAGS_IMAGE_FRAME))
            {
                m_mutex.lock();
#if IMGUI_VULKAN_SHADER
                if (!m_convert)
                {
                    int gpu = mat.device == IM_DD_VULKAN ? mat.device_number : ImGui::get_default_gpu_index();
                    m_convert = new ImGui::ColorConvert_vulkan(gpu);
                    if (!m_convert)
                    {
                        m_mutex.unlock();
                        return {};
                    }
                }
#endif
                m_image_width = mat.w;
                m_image_height = mat.h;
#if IMGUI_VULKAN_SHADER
                if (mat.device == IM_DD_VULKAN)
                {
                    if (mat.c == 1)
                    {
                        int video_depth = mat.type == IM_DT_INT8 ? 8 : mat.type == IM_DT_INT16 || mat.type == IM_DT_INT16_BE ? 16 : 8;
                        int video_shift = mat.depth != 0 ? mat.depth : mat.type == IM_DT_INT8 ? 8 : mat.type == IM_DT_INT16 || mat.type == IM_DT_INT16_BE ? 16 : 8;
                        if (mat.flags & IM_MAT_FLAGS_VIDEO_FRAME_UV)
                            mat.color_format = IM_CF_NV12;
                        ImGui::VkMat in_mat = mat;
                        m_RenderVkMat.type = IM_DT_INT8;
                        m_convert->GRAY2RGBA(in_mat, m_RenderVkMat, mat.color_space, mat.color_range, video_depth, video_shift);
                    }
                    else if (mat.c == 2)
                    {
                        m_RenderVkMat.type = IM_DT_INT8;
                        m_convert->ConvertColorFormat(mat, m_RenderVkMat);
                    }
                    else if (mat.c == 3)
                    {
                        m_RenderVkMat.type = IM_DT_INT8;
                        if (IM_ISYUV(mat.color_format))
                            m_convert->ConvertColorFormat(mat, m_RenderVkMat);
                        else
                        {
                            ImGui::VkMat in_mat = mat;
                            m_convert->Conv(in_mat, m_RenderVkMat);
                        }
                    }
                    else
                    {
                        if (mat.type == IM_DT_INT8)
                            m_RenderVkMat = mat;
                        else
                        {
                            ImGui::VkMat in_mat = mat;
                            m_RenderVkMat.type = IM_DT_INT8;
                            m_convert->Conv(in_mat, m_RenderVkMat);
                        }
                    }
                }
                else
                {
                    m_RenderVkMat.type = IM_DT_INT8;
                    m_convert->ConvertColorFormat(mat, m_RenderVkMat);
                }
#else
                m_RenderMat = mat; // TODO::Dicky Software convert?
#endif
                m_mutex.unlock();
            }
            else if (!mat.empty() && mat.flags & IM_MAT_FLAGS_AUDIO_FRAME)
            {
                m_RenderMat = mat;
            }
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        changed |= ImGui::Checkbox(ICON_IGFD_BOOKMARK " Bookmark", &m_isShowBookmark);
        ImGui::SameLine(0);
        changed |= ImGui::Checkbox(ICON_IGFD_HIDDEN_FILE " ShowHide", &m_isShowHiddenFiles);
        ImGui::Separator();

        // Draw custom layout
        changed |= ImGui::InputInt("Preview Width", &m_preview_width);
        changed |= ImGui::InputInt("Preview Height", &m_preview_height);
        ImGui::Separator();

        // open file dialog
        ImVec2 minSize = ImVec2(400, 300);
		ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
        auto& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = true;
        string file_name;
        auto separator = m_save_file_path.find_last_of('/');
        if (separator != std::string::npos)
            file_name = m_save_file_path.substr(separator + 1);
        else
            file_name = m_save_file_path;
        ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_SaveFile_Default;
        if (!m_isShowBookmark)      vflags &= ~ImGuiFileDialogFlags_ShowBookmark;
        if (m_isShowHiddenFiles)    vflags &= ~ImGuiFileDialogFlags_DontShowHiddenFiles;
        if (m_Blueprint->GetStyleLight())
            ImGuiFileDialog::Instance()->SetLightStyle();
        else
            ImGuiFileDialog::Instance()->SetDarkStyle();
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Capture File Name..."))
        {
            IGFD::FileDialogConfig config;
            config.path = file_name.empty() ? "." : file_name;
            config.countSelectionMax = 1;
            config.userDatas = this;
            config.flags = vflags;
            ImGuiFileDialog::Instance()->OpenDialog("##RenderChooseFileDlgKey", "Choose File", 
                                                    m_filters.c_str(), 
                                                    config);
        }
        if (ImGuiFileDialog::Instance()->Display("##RenderChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
        {
	        // action if OK
            if (ImGuiFileDialog::Instance()->IsOk() == true)
            {
                m_save_file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                changed = true;
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        ImGui::SameLine(0);
        ImGui::TextUnformatted(file_name.c_str());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = false;
        return changed;
    }

    void DrawMenuLayout(ImGuiContext * ctx) override
    {
        ImGui::SetCurrentContext(ctx);
        if (!m_textureID)
            return;

        if (ImGui::MenuItem((std::string(ICON_SAVE_TEXTURE) + " Save Texture to File").c_str(), nullptr, nullptr, true))
        {
            if (!m_save_file_path.empty())
            {
                ImGui::ImTextureToFile(m_textureID, m_save_file_path);
            }
        }
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        m_mutex.lock();
#if IMGUI_VULKAN_SHADER
        if (!m_RenderVkMat.empty())
        {
            GeneratorTexture(m_RenderVkMat);
        }
        else 
#endif
        if (!m_RenderMat.empty())
        {
            if (m_RenderMat.flags & IM_MAT_FLAGS_VIDEO_FRAME)
                GeneratorTexture(m_RenderMat);
        }
        if (m_textureID)
        {
            ImDrawList * draw_list = ImGui::GetWindowDrawList();
            auto PreviewPosSource = ImGui::GetCursorPos();
            auto PreviewSizeSource = ImVec2(m_preview_width,m_preview_height);
            ImGui::ImShowVideoWindow(draw_list, m_textureID, PreviewPosSource, PreviewSizeSource, 360, IM_COL32(0,0,0,0), 1);
        }
        else
        {
            if (!m_RenderMat.empty() && m_RenderMat.flags & IM_MAT_FLAGS_AUDIO_FRAME)
            {
                ImVec2 scope_view_size = ImVec2(m_preview_width, m_preview_height / m_RenderMat.c);
                for (int i = 0; i < m_RenderMat.c; i++)
                {
                    ImGui::ImMat wave_mat;
                    ImGui::ImMat channel = m_RenderMat.channel(i);
                    if (m_RenderMat.type == IM_DT_FLOAT32)
                        wave_mat = channel;
                    else if (m_RenderMat.type == IM_DT_INT16)
                    {
                        wave_mat.create_type(channel.w, channel.h, IM_DT_FLOAT32);
                        for (int i = 0; i < channel.w; i++)
                            wave_mat.at<float>(i, 0) = channel.at<short>(i, 0) / (float)(INT16_MAX);
                    }
                    else if (m_RenderMat.type == IM_DT_INT32)
                    {
                        wave_mat.create_type(channel.w, channel.h, IM_DT_FLOAT32);
                        for (int i = 0; i < channel.w; i++)
                            wave_mat.at<float>(i, 0) = channel.at<int32_t>(i, 0) / (float)(INT32_MAX);
                    }
                    else if (m_RenderMat.type == IM_DT_INT8)
                    {
                        wave_mat.create_type(channel.w, channel.h, IM_DT_FLOAT32);
                        for (int i = 0; i < channel.w; i++)
                            wave_mat.at<float>(i, 0) = channel.at<int8_t>(i, 0) / (float)(INT8_MAX);
                    }
                    if (!wave_mat.empty())
                    {
                        ImGui::PushID(i);
                        ImGui::PlotLinesEx("##wave", (float *)wave_mat.data, wave_mat.w, 0, nullptr, -1.0, 1.0, scope_view_size, 4, false, false);
                        ImGui::PopID();
                    }
                }
            }
            else
                ImGui::Dummy(ImVec2(m_preview_width,m_preview_height));
        }
        m_mutex.unlock();
        return false;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("preview_width"))
        {
            auto& val = value["preview_width"];
            if (val.is_number()) 
                m_preview_width = val.get<imgui_json::number>();
        }
        if (value.contains("preview_height"))
        {
            auto& val = value["preview_height"];
            if (val.is_number()) 
                m_preview_height = val.get<imgui_json::number>();
        }
        if (value.contains("save_path_name"))
        {
            auto& val = value["save_path_name"];
            if (val.is_string()) 
                m_save_file_path = val.get<imgui_json::string>();
        }
        if (value.contains("filter"))
        {
            auto& val = value["filter"];
            if (val.is_string())
            {
                m_filters = val.get<imgui_json::string>();
            }
        }
        if (value.contains("show_bookmark"))
        {
            auto& val = value["show_bookmark"];
            if (val.is_boolean()) m_isShowBookmark = val.get<imgui_json::boolean>();
        }
        if (value.contains("show_hidden"))
        {
            auto& val = value["show_hidden"];
            if (val.is_boolean()) m_isShowHiddenFiles = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["preview_width"] = imgui_json::number(m_preview_width);
        value["preview_height"] = imgui_json::number(m_preview_height);
        value["save_path_name"] = m_save_file_path;
        value["show_bookmark"] = m_isShowBookmark;
        value["show_hidden"] = m_isShowHiddenFiles;
        value["filter"] = m_filters;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_Mat}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Reset   = { this, "Reset" };
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_Mat     = { this, "Mat" };
    Pin* m_InputPins[3] = { &m_Enter, &m_Reset, &m_Mat };
    Pin* m_OutputPins[2] = { &m_Exit, &m_OReset };

    ImTextureID m_textureID {0};
    int32_t     m_preview_width {960};
    int32_t     m_preview_height {540};

#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan *m_convert   {nullptr};
#endif

    int m_image_width   {0};
    int m_image_height  {0};
    std::string m_save_file_path    {"saved_texture.png"};
    std::string m_filters {".png,.PNG,.jpg,.jpeg,.JPG,.JPEG,.bmp,.BMP,.tga,.TGA"};
    bool m_isShowBookmark {false};
    bool m_isShowHiddenFiles {true};
    std::mutex m_mutex;
    ImGui::ImMat m_RenderMat;
#if IMGUI_VULKAN_SHADER
    ImGui::VkMat m_RenderVkMat;
#endif
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(MatRenderNode, "Media Render", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
