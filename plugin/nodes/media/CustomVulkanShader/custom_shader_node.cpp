#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <immat.h>
#include <imgui_helper.h>
#include <TextEditor.h>
#include <ImVulkanShader.h>
#include <imvk_mat_shader.h>
#include "glslang/Public/ShaderLang.h"
#include "CustomShader.h"
#include <algorithm>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>
#include <map>

#define NODE_VERSION    0x01040000
typedef struct _Node_Param
{
    std::string name;
    float      value {0};
    float      min_value {0};
    float      max_value {1};
} Node_Param;

namespace BluePrint
{
struct CustomShaderNode final : Node
{
    BP_NODE_WITH_NAME(CustomShaderNode, "Custom Vulkan Shader", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")

    CustomShaderNode(BP* blueprint): Node(blueprint)
    { 
        m_Name = "Custom Vulkan Shader";
        //glslang::InitializeProcess();
        m_lang = TextEditor::LanguageDefinition::GLSL();
        static const char* const my_keywork[] = { 
            "sfp", "sfpvec2", "sfpvec3", "sfpvec4", "sfpvec8", "eps", "sfpmat3","sfpmat4",
            "afp", "afpvec2", "afpvec3", "afpvec4", "afpvec8", "afpmat3","afpmat4"
        };
        std::string param_string = std::string(SHADER_DEFAULT_PARAM_HEADER);
        param_string += std::string(SHADER_DEFAULT_PARAM_TAIL);
        for (auto& k : my_keywork) m_lang.mKeywords.insert(k);

        static std::map<std::string, std::string> my_identifiers;
        my_identifiers.insert(std::make_pair("sfp", "float type, default is float, fp16 mode is float16_t"));
        my_identifiers.insert(std::make_pair("esp", "float type, default is float(1e-8), fp16 mode is float16_t(1e-4)"));
        my_identifiers.insert(std::make_pair("sfpvec2", "vec type, default is vec2, fp16 mode is f16vec2"));
        my_identifiers.insert(std::make_pair("sfpvec3", "vec type, default is vec3, fp16 mode is f16vec3"));
        my_identifiers.insert(std::make_pair("sfpvec4", "vec type, default is vec4, fp16 mode is f16vec4"));
        my_identifiers.insert(std::make_pair("sfpvec8", "vec type, default is mat2x4, fp16 mode is f16mat2x4"));
        my_identifiers.insert(std::make_pair("sfpmat3", "mat type, default is mat3, fp16 mode is f16mat3"));
        my_identifiers.insert(std::make_pair("sfpmat4", "mat type, default is mat4, fp16 mode is f16mat4"));
        my_identifiers.insert(std::make_pair("load_gray", "sfp load_gray(int x, int y, int w, int cstep, int format, int type, float scale)"));
        my_identifiers.insert(std::make_pair("load_rgb", "sfpvec3 load_rgb(int x, int y, int w, int cstep, int format, int type)"));
        my_identifiers.insert(std::make_pair("load_rgba", "sfpvec4 load_rgba(int x, int y, int w, int cstep, int format, int type)"));
        my_identifiers.insert(std::make_pair("store_gray", "void store_gray(sfp val, int x, int y, int w, int cstep, int format, int type)"));
        my_identifiers.insert(std::make_pair("store_rgb", "void store_rgb(sfpvec3 val, int x, int y, int w, int cstep, int format, int type)"));
        my_identifiers.insert(std::make_pair("store_rgba", "void store_rgba(sfpvec4 val, int x, int y, int w, int cstep, int format, int type)"));
        my_identifiers.insert(std::make_pair("load", "sfpvec4 load(int x, int y) //sample call as load_rgba"));
        my_identifiers.insert(std::make_pair("store", "void store(sfpvec4 val, int x, int y) //sample call as store_rgba"));
        my_identifiers.insert(std::make_pair("p", param_string));
        for (const auto& k : my_identifiers)
        {
            TextEditor::Identifier id;
            id.mDeclaration = k.second;
			m_lang.mIdentifiers.insert(std::make_pair(k.first, id));
        }
        m_editor.SetLanguageDefinition(m_lang);
        m_program_filter_default = std::string(
            "sfpvec4 shader(sfpvec4 rgba)\n"
            "{\n"
                "\tsfpvec4 result = rgba;\n"
                "\treturn result;\n"
            "}\n"
            "void main()\n"
            "{\n"
            "\tint gx = int(gl_GlobalInvocationID.x);\n"
            "\tint gy = int(gl_GlobalInvocationID.y);\n"
            "\tif (gx >= p.out_w || gy >= p.out_h)\n"
                "\t\treturn;\n"
            "\tsfpvec4 rgba = load(gx, gy);\n"
            "\tsfpvec4 result = shader(rgba);\n"
            "\tstore(result, gx, gy);\n"
            "}\n"
        );
        m_editor.SetTabSize(4);
        m_program_filter = m_program_filter_default;
        m_editor.SetText(m_program_filter);

        m_program_start =   std::string(SHADER_HEADER) + 
                            param_string +
                            std::string(SHADER_INPUT_OUTPUT_DATA) +
                            std::string(SHADER_LOAD_RGBA) +
                            std::string(SHADER_STORE_RGBA) +
                            "#define load(x, y) load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type)\n" +
                            "#define store(v, x, y) store_rgba(v, x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type)\n" ;
    }
    ~CustomShaderNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
        //glslang::FinalizeProcess();
    }
    
    void update_program()
    {
        std::string param_string = std::string(SHADER_DEFAULT_PARAM_HEADER);
        for (int i = 0; i < m_params.size(); i++)
        {
            param_string += "\tfloat param_" + std::to_string(i + 1) + ";\n";
        }
        param_string += std::string(SHADER_DEFAULT_PARAM_TAIL);
        m_program_start =   std::string(SHADER_HEADER) + 
                            param_string +
                            std::string(SHADER_INPUT_OUTPUT_DATA) +
                            std::string(SHADER_LOAD_RGBA) +
                            std::string(SHADER_STORE_RGBA) +
                            "#define load(x, y) load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type)\n" +
                            "#define store(v, x, y) store_rgba(v, x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type)\n" ;
        // need earse old pair ? 
        for (auto& ident : m_lang.mIdentifiers)
        {
            if (ident.first == "p")
            {
                TextEditor::Identifier id;
                id.mDeclaration = param_string;
                ident.second = id;
                break;
            }
        }
        m_editor.SetLanguageDefinition(m_lang);
        m_compile_succeed = false;
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void OnStop(Context& context) override
    {
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_IReset.m_ID)
        {
            Reset(context);
            return m_OReset;
        }
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter && m_compile_succeed)
            {
                int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
                m_program_filter = m_editor.GetText();
                std::string shader_program = m_program_start + m_program_filter;
                m_filter = new CustomShader(shader_program, gpu, m_fp16);
            }
            if (!m_filter)
            {
                return {};
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            std::vector<float> params;
            for (auto param : m_params)
                params.push_back(param.value);
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, params);
            im_RGB.time_stamp = mat_in.time_stamp;
            im_RGB.rate = mat_in.rate;
            im_RGB.flags = mat_in.flags;
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    int GetLineNumber(std::string& str)
    {
        int ret = 0;
        size_t start = 0;
        size_t end;
        while (1) 
        {
            string this_line;
            if ((end = str.find("\n", start)) == string::npos)
            {
                if (!(this_line = str.substr(start)).empty()) 
                {
                    ret ++;
                }
                break;
            }

            this_line = str.substr(start, end - start);
            ret ++;
            start = end + 1;
        }
        return ret;
    }

    size_t GetLines(std::string& str, std::vector<std::string>& lines)
    {
        size_t start = 0;
        size_t end;
        lines.clear();
        while (1) 
        {
            string this_line;
            if ((end = str.find("\n", start)) == string::npos)
            {
                if (!(this_line = str.substr(start)).empty()) 
                {
                    lines.push_back(this_line);
                }
                break;
            }

            this_line = str.substr(start, end - start);
            lines.push_back(this_line);
            start = end + 1;
        }
        return lines.size();
    }

    void SetErrorPoint(int start_lines, int filter_lines)
    {
        if (!m_compile_log.empty())
        {
            int row = -1;
            int col = -1;
            TextEditor::ErrorMarkers markers;
            std::vector<std::string> logs;
            GetLines(m_compile_log, logs);
            for (int i = 0; i < logs.size() - 2; i++)
            {
                string msg = logs[i];
                size_t start = 0;
                size_t end;
                string this_word;
                if ((end = logs[i].find(":", start)) != string::npos)
                {
                    this_word = logs[i].substr(start, end - start);
                    if (this_word.compare("ERROR") == 0)
                    {
                        start = end + 1;
                        if ((end = logs[i].find(":", start)) != string::npos)
                        {
                            this_word = logs[i].substr(start, end - start);
                            row = std::atoi(this_word.c_str());
                            start = end + 1;
                            if ((end = logs[i].find(":", start)) != string::npos)
                            {
                                this_word = logs[i].substr(start, end - start);
                                col = std::atoi(this_word.c_str());
                                if (col > start_lines && col <= start_lines + filter_lines)
                                {
                                    markers.insert(std::make_pair<int, std::string>(m_show_all_code ? col : col - start_lines, msg.c_str()));
                                }
                            }
                        }
                    }
                }
            }
            m_editor.SetErrorMarkers(markers);
        }
    }

    void DrawShaderEditor()
    {        
        auto cpos = m_editor.GetCursorPosition();
        ImGui::BeginChild("Vulkan Shader Editor", ImVec2(800, 400), false);
        ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | ", cpos.mLine + 1, cpos.mColumn + 1, m_editor.GetTotalLines(),
                    m_editor.IsOverwrite() ? "Ovr" : "Ins",
                    m_editor.CanUndo() ? "*" : " ",
                    m_editor.GetLanguageDefinition().mName.c_str());
        ImGui::SameLine();
        if (m_compile_succeed)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            ImGui::TextUnformatted("Compiled");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            ImGui::TextUnformatted("Need Compile");
            ImGui::PopStyleColor();
        }

        if (ImGui::Button("Compile"))
        {
            m_compile_log.clear();
            ImGui::Option _opt;
            _opt.use_fp16_arithmetic = m_fp16;
            _opt.use_fp16_storage = m_fp16;
            std::vector<uint32_t> spirv_data;
            m_program_filter = m_editor.GetText();
            int start_lines = GetLineNumber(m_program_start);
            int filter_lines = GetLineNumber(m_program_filter);
            std::string shader_program = m_program_start + m_program_filter;
            int ret = ImGui::compile_spirv_module(shader_program.data(), _opt, spirv_data, m_compile_log);
            if (ret != 0)
            {
                SetErrorPoint(start_lines, filter_lines);
                m_compile_succeed = false;
                m_compile_log = "Compile Failed!!!";
            }
            else
            {
                TextEditor::ErrorMarkers markers;
                m_editor.SetErrorMarkers(markers);
                m_compile_log = "Compile Succeed!!!";
                m_compile_succeed = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            m_program_filter = m_program_filter_default;
            m_editor.SetText(m_program_filter);
        }
        ImGui::SameLine(); ImGui::Spacing();ImGui::SameLine();
        if (ImGui::Button("Undo"))
        {
            if (m_editor.CanUndo())
                m_editor.Undo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Redo"))
        {
            if (m_editor.CanRedo())
                m_editor.Redo();
        }
        ImGui::SameLine(); ImGui::Spacing();ImGui::SameLine();
        if (ImGui::Button("Copy"))
        {
            if (m_editor.HasSelection())
                m_editor.Copy();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cut"))
        {
            if (m_editor.HasSelection())
                m_editor.Cut();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            if (m_editor.HasSelection())
                m_editor.Delete();
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste"))
        {
            if (ImGui::GetClipboardText() != nullptr)
                m_editor.Paste();
        }
        ImGui::SameLine(); ImGui::Spacing();ImGui::SameLine();
        if (ImGui::Checkbox("Show Space", &m_show_space_tab))
        {
            m_editor.SetShowWhitespaces(m_show_space_tab);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Short TAB", &m_show_short_tab))
        {
            m_editor.SetShowShortTabGlyphs(m_show_short_tab);
        }
        
        // For debug
        /*
        ImGui::SameLine(); ImGui::Spacing();ImGui::SameLine();
        if (ImGui::Checkbox("Show All Code", &m_show_all_code))
        {
            if (m_show_all_code)
            {
                m_program_filter = m_editor.GetText();
                m_editor.SetText(m_program_start + m_program_filter);
            }
            else
                m_editor.SetText(m_program_filter);
        }
        */
        
        ImGui::SameLine(); ImGui::Spacing();ImGui::SameLine();

        int _editor_style = m_editor_style;
        ImGui::SetNextItemWidth(80);
        ImGui::Combo("Style", &_editor_style, "Dark\0Light\0Retro blue\0\0");
        if (_editor_style != m_editor_style)
        {
            m_editor_style = _editor_style;
            m_editor.SetPalette(m_editor_style == 0 ? TextEditor::GetDarkPalette() :
                                m_editor_style == 1 ? TextEditor::GetLightPalette() :
                                TextEditor::GetRetroBluePalette());
        }
        if (m_editor.IsTextChanged())
        {
            TextEditor::ErrorMarkers markers;
            m_editor.SetErrorMarkers(markers);
            m_compile_succeed = false;
        }

        m_editor.Render("VulkanShader");
        ImGui::EndChild();
    }

    void DrawCompileLog()
    {
        ImGui::BeginChild("Vulkan Shader Compile log", ImVec2(800, 80), false);
        ImGui::TextUnformatted(m_compile_log.c_str());
        ImGui::EndChild();
    }

    void ParamSetting(Node_Param& param)
    {
        ImGui::PushItemWidth(200);
        ImGui::TextUnformatted("Param:"); ImGui::SameLine();
        std::string value = param.name;
        if (ImGui::InputText(("##param_name_string_value" + value).c_str(), (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
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
        }, &value))
        {
            value.resize(strlen(value.c_str()));
            if (param.name.compare(value) != 0)
            {
                param.name = value;
            }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Min:"); ImGui::SameLine();
        ImGui::InputFloat(("##param_min_value" + param.name).c_str(), &param.min_value);
        ImGui::SameLine();
        ImGui::TextUnformatted("Max:"); ImGui::SameLine();
        ImGui::InputFloat(("##param_max_value" + param.name).c_str(), &param.max_value);
        ImGui::PopItemWidth();
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        ImGui::SetCurrentContext(ctx); // External Node must set context

        // Draw Set Node Name
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        // Custom Node Setting
        DrawShaderEditor();
        ImGui::Separator();
        DrawCompileLog();
        ImGui::Separator();

        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
        ImGui::Separator();
        bool check = m_fp16;
        ImGui::TextUnformatted("Half Float(16bits):"); ImGui::SameLine();
        if (ImGui::Checkbox("##16bit FLoat:", &check))
        {
            if (check != m_fp16)
            {
                m_fp16 = check; 
                m_compile_succeed = false;
                if (m_filter) { delete m_filter; m_filter = nullptr; }
            }
        }
        ImGui::Separator();
        if (ImGui::Button( ICON_FK_PLUS " Add param"))
        {
            Node_Param param;
            param.name = "param";
            m_params.push_back(param);
            update_program();
        }

        int count = 0;
        for (auto iter = m_params.begin(); iter != m_params.end();)
        {
            ParamSetting(*iter);
            ImGui::SameLine();
            ImGui::PushID(count);
            if (ImGui::Button(ICON_FK_TRASH_O "##delete_param"))
            {
                iter = m_params.erase(iter);
                update_program();
            }
            else
                iter++;
            ImGui::PopID();
            count++;
        }
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        ImGui::PushItemWidth(300);
        if (m_params.empty())
        {
            ImGui::TextUnformatted("No params");
        }
        else
        {
            int id = 0;
            static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
            ImGui::BeginDisabled(!m_Enabled);
            for (auto& param : m_params)
            {
                ImGui::PushID(id);
                float _value = param.value;
                ImGui::SliderFloat(param.name.c_str(), &_value, param.min_value, param.max_value, "%.2f", flags);
                if (_value != param.value) { param.value = _value; changed = true; }
                ImGui::PopID();
                id++;
            }
            ImGui::EndDisabled();
        }
        if (m_compile_succeed)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
            ImGui::TextUnformatted("Shader Compled Passed");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
            ImGui::TextUnformatted("Shader NOT Compled Yet!!!");
            ImGui::PopStyleColor();
        }
        ImGui::PopItemWidth();
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
        if (value.contains("show_space"))
        { 
            auto& val = value["show_space"];
            if (val.is_boolean())
                m_show_space_tab = val.get<imgui_json::boolean>();
        }
        if (value.contains("show_short_tab"))
        { 
            auto& val = value["show_short_tab"];
            if (val.is_boolean())
                m_show_short_tab = val.get<imgui_json::boolean>();
        }
        if (value.contains("fp16"))
        { 
            auto& val = value["fp16"];
            if (val.is_boolean())
                m_fp16 = val.get<imgui_json::boolean>();
        }
        if (value.contains("compiled"))
        { 
            auto& val = value["compiled"];
            if (val.is_boolean())
                m_compile_succeed = val.get<imgui_json::boolean>();
        }
        if (value.contains("editor_style"))
        {
            auto& val = value["editor_style"];
            if (val.is_number()) 
                m_editor_style = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("program"))
        {
            auto& val = value["program"];
            if (val.is_string()) 
            {
                m_program_filter = val.get<imgui_json::string>();
                m_program_filter_default = m_program_filter;
            }
        }
        const imgui_json::array* paramArray = nullptr;
        if (imgui_json::GetPtrTo(value, "params", paramArray))
        {
            for (auto& val : *paramArray)
            {
                Node_Param param;
                if (val.contains("name")) { auto& name = val["name"]; if (name.is_string())         param.name = name.get<imgui_json::string>(); }
                if (val.contains("value")) { auto& fvalue = val["value"]; if (fvalue.is_number())   param.value = fvalue.get<imgui_json::number>(); }
                if (val.contains("min")) { auto& fvalue = val["min"]; if (fvalue.is_number())       param.min_value = fvalue.get<imgui_json::number>(); }
                if (val.contains("max")) { auto& fvalue = val["max"]; if (fvalue.is_number())       param.max_value = fvalue.get<imgui_json::number>(); }
                m_params.push_back(param);
            }
            update_program();
        }
        m_editor.SetPalette(m_editor_style == 0 ? TextEditor::GetDarkPalette() :
                                m_editor_style == 1 ? TextEditor::GetLightPalette() :
                                TextEditor::GetRetroBluePalette());
        m_editor.SetText(m_program_filter);
        m_editor.SetTextChanged(false);
        m_editor.SetShowWhitespaces(m_show_space_tab);
        m_editor.SetShowShortTabGlyphs(m_show_short_tab);
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["fp16"] = imgui_json::boolean(m_fp16);
        value["show_space"] = imgui_json::boolean(m_show_space_tab);
        value["show_short_tab"] = imgui_json::boolean(m_show_short_tab);
        value["compiled"] = imgui_json::boolean(m_compile_succeed);
        value["editor_style"] = imgui_json::number(m_editor_style);
        value["program"] = m_program_filter;

        imgui_json::value params;
        for (auto param : m_params)
        {
            imgui_json::value val;
            val["name"]  = param.name;
            val["value"] = param.value;
            val["min"]   = param.min_value;
            val["max"]   = param.max_value;
            params.push_back(val);
        }
        if (m_params.size() > 0) value["params"] = params;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter           = { this, "Enter" };
    FlowPin   m_IReset          = { this, "Reset In" };
    MatPin    m_MatIn           = { this, "In"    };

    FlowPin   m_Exit            = { this, "Exit" };
    FlowPin   m_OReset          = { this, "Reset Out"};
    MatPin    m_MatOut          = { this, "Out"  };

    Pin*      m_InputPins[3] = { &m_Enter, &m_IReset, &m_MatIn };
    Pin*      m_OutputPins[3] = { &m_Exit, &m_OReset, &m_MatOut };

private:
    std::string m_program_start;
    std::string m_program_filter_default;
    std::string m_program_filter;
    std::string m_compile_log;
    TextEditor m_editor;
    std::vector<Node_Param> m_params;
    int m_editor_style  {0};
    bool m_show_space_tab {false};
    bool m_show_short_tab {false};
    bool m_show_all_code {false};
    bool m_compile_succeed {false};
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    bool m_fp16          {true};
    TextEditor::LanguageDefinition m_lang;
private:
    CustomShader * m_filter {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(CustomShaderNode, "Custom Vulkan Shader", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
