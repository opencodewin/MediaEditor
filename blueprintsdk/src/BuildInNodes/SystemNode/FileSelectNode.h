#pragma once
#include <imgui.h>
#if IMGUI_ICONS
#include <icons.h>
#endif
#include <ImGuiFileDialog.h>
namespace BluePrint
{
struct FileSelectNode final : Node
{
    enum FILESELECT_FLAGS : int32_t
    {
        FILESELECT_PATH      =      0,
        FILESELECT_FOLDER    = 1 << 0,
        FILESELECT_NAME      = 1 << 1,
        FILESELECT_SUFFIX    = 1 << 2,
    };
    BP_NODE(FileSelectNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "System")
    FileSelectNode(BP* blueprint): Node(blueprint) { m_Name = "FileSelect"; m_HasCustomLayout = true; }
    
    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (m_needReload)
        {
            m_needReload = false;
            context.PushReturnPoint(entryPoint);
            return m_Set;
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw default setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        // Draw Custom setting
        changed |= ImGui::Checkbox(ICON_IGFD_BOOKMARK " Bookmark", &m_isShowBookmark);
        ImGui::SameLine(0);
        changed |= ImGui::Checkbox(ICON_IGFD_HIDDEN_FILE " ShowHide", &m_isShowHiddenFiles);
        ImGui::SameLine(0);
        // file filter setting
        if (ImGui::InputText("Filters", (char*)m_filters.data(), m_filters.size() + 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                auto& stringValue = *static_cast<string*>(data->UserData);
                ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                IM_ASSERT(stringValue.data() == data->Buf);
                stringValue.resize(data->BufSize);
                data->Buf = (char*)stringValue.data();
            }
            else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
            {
                auto& stringValue = *static_cast<string*>(data->UserData);
                stringValue = std::string(data->Buf);
            }
            return 0;
        }, &m_filters))
        {
            m_filters.resize(strlen(m_filters.c_str()));
            changed = true;
        }
        ImGui::Separator();
        bool flag_folder        = m_out_flags & FILESELECT_FOLDER;
        bool flag_name          = m_out_flags & FILESELECT_NAME;
        bool flag_suffix        = m_out_flags & FILESELECT_SUFFIX;
        ImGui::TextUnformatted("  Folder Name"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_path", &flag_folder);
        ImGui::TextUnformatted("    File Name"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_name", &flag_name);
        ImGui::TextUnformatted("  File Suffix"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_suffix", &flag_suffix);
        m_out_flags = 0;
        if (flag_folder)    m_out_flags |= FILESELECT_FOLDER;
        if (flag_name)      m_out_flags |= FILESELECT_NAME;
        if (flag_suffix)    m_out_flags |= FILESELECT_SUFFIX;
        BuildOutputPin();

        ImGui::Separator();
        // open file dialog
        ImVec2 minSize = ImVec2(400, 300);
		ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
        auto& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = true;
        ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_OpenFile_Default;
        if (!m_isShowBookmark)      vflags &= ~ImGuiFileDialogFlags_ShowBookmark;
        if (m_isShowHiddenFiles)    vflags &= ~ImGuiFileDialogFlags_DontShowHiddenFiles;
        if (m_Blueprint->GetStyleLight())
            ImGuiFileDialog::Instance()->SetLightStyle();
        else
            ImGuiFileDialog::Instance()->SetDarkStyle();
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose File"))
        {
            IGFD::FileDialogConfig config;
            config.path = m_file_path.empty() ? "." : m_file_path;
            config.countSelectionMax = 1;
            config.userDatas = this;
            config.flags = vflags;
            ImGuiFileDialog::Instance()->OpenDialog("##NodeChooseFileDlgKey", "Choose File", 
                                                    m_filters.c_str(), 
                                                    config);
        }
        if (ImGuiFileDialog::Instance()->Display("##NodeChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
        {
	        // action if OK
            if (ImGuiFileDialog::Instance()->IsOk() == true)
            {
                m_file_path_name = ImGuiFileDialog::Instance()->GetFilePathName();
                m_file_path = ImGuiFileDialog::Instance()->GetCurrentPath();
                m_file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                auto found = m_file_name.find_last_of(".");
                if (found != std::string::npos)
                    m_file_suffix = m_file_name.substr(found + 1);
                else
                    m_file_suffix = "";
                m_FileSuffix.SetValue(m_file_suffix);
                m_FileName.SetValue(m_file_name);
                m_FilePath.SetValue(m_file_path);
                m_FullPath.SetValue(m_file_path_name);
                m_needReload = true;
                changed = true;
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        ImGui::SameLine(0);
        ImGui::TextUnformatted(m_file_name.c_str());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = false;
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        ImGui::Dummy(ImVec2(0, 32));
        ImGui::Text("%s", m_file_name.c_str());
        return false;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if (!value.contains("out_flags"))
            return BP_ERR_NODE_LOAD;

        auto& flags = value["out_flags"];
        if (!flags.is_number())
            return BP_ERR_NODE_LOAD;

        m_out_flags = flags.get<imgui_json::number>();

        BuildOutputPin();

        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("file_path_name"))
        {
            auto& val = value["file_path_name"];
            if (val.is_string())
            {
                m_file_path_name = val.get<imgui_json::string>();
                m_FullPath.SetValue(m_file_path_name);
            }
        }
        if (value.contains("file_path"))
        {
            auto& val = value["file_path"];
            if (val.is_string())
            {
                m_file_path = val.get<imgui_json::string>();
                m_FilePath.SetValue(m_file_path);
            }
        }
        if (value.contains("file_name"))
        {
            auto& val = value["file_name"];
            if (val.is_string())
            {
                m_file_name = val.get<imgui_json::string>();
                m_FileName.SetValue(m_file_name);
            }
        }
        if (value.contains("file_suffix"))
        {
            auto& val = value["file_suffix"];
            if (val.is_string())
            {
                m_file_suffix = val.get<imgui_json::string>();
                m_FileSuffix.SetValue(m_file_suffix);
            }
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

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        value["out_flags"] = imgui_json::number(m_out_flags);
        value["file_path_name"] = m_file_path_name;
        value["file_path"] = m_file_path;
        value["file_name"] = m_file_name;
        value["file_suffix"] = m_file_suffix;
        value["show_bookmark"] = m_isShowBookmark;
        value["show_hidden"] = m_isShowHiddenFiles;
        value["filter"] = m_filters;
    }

    void BuildOutputPin()
    {
        m_OutputPins.clear();
        m_OutputPins.push_back(&m_Exit);
        m_OutputPins.push_back(&m_Set);
        m_OutputPins.push_back(&m_FullPath);
        if (m_out_flags & FILESELECT_FOLDER)    { m_OutputPins.push_back(&m_FilePath); }
        if (m_out_flags & FILESELECT_NAME)      { m_OutputPins.push_back(&m_FileName); }
        if (m_out_flags & FILESELECT_SUFFIX)    { m_OutputPins.push_back(&m_FileSuffix); }
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override
    { 
        if (m_OutputPins.size() == 0) 
            BuildOutputPin();
        return m_OutputPins; 
    }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_FullPath}; }

    FlowPin   m_Enter       = { this, "Enter" };
    FlowPin   m_Exit        = { this, "Exit" };
    FlowPin   m_Set         = { this, "Set" };
    StringPin m_FullPath    = { this, "PathName", "" };
    StringPin m_FilePath    = { this, "Path", "" };
    StringPin m_FileName    = { this, "Name", "" };
    StringPin m_FileSuffix  = { this, "Suffix", "" };
    Pin* m_InputPins[1] = { &m_Enter };
    string m_filters {".*"};
    string m_file_path_name;
    string m_file_path;
    string m_file_name;
    string m_file_suffix;
    bool m_isShowBookmark {false};
    bool m_isShowHiddenFiles {false};
    bool m_needReload {false};

    std::vector<Pin *> m_OutputPins;
    int32_t m_out_flags = 0;
};
} // namespace BluePrint
