//- Common Code For All Addons needed just to ease inclusion as separate files in user code ----------------------
#include <imgui.h>
#include <imgui_user.h>
#include "imgui_helper.h"
#include <errno.h>
#include <mutex>
#include <thread>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <winerror.h>   // For SUCCEEDED macro
#include <shellapi.h>	// ShellExecuteA(...) - Shell32.lib
#include <objbase.h>    // CoInitializeEx(...)  - ole32.lib
#include <shlobj.h>     // For SHGetFolderPathW and various CSIDL "magic numbers"
#include <stringapiset.h>   // For WideCharToMultiByte
#include <psapi.h> 
#include "mman_win.h"
#if IMGUI_RENDERING_DX11
struct IUnknown;
#include <d3d11.h>
#elif IMGUI_RENDERING_DX9
#include <d3d9.h>
#endif
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#define PATH_SETTINGS "\\AppData\\Roaming\\"
#else //_WIN32
#include <unistd.h>
#include <stdlib.h> // system
#include <pwd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/mman.h>
#endif //_WIN32

#if defined(__APPLE__)
#define PATH_SETTINGS "/Library/Application Support/"
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#define PATH_SETTINGS "/.config/"
#endif

#if defined(__EMSCRIPTEN__)
#define IMGUI_IMPL_OPENGL_ES2               // Emscripten    -> GL ES 2, "#version 100"
#define PATH_SETTINGS "/.config/"
#endif

#include <string>
#include <fstream>
#include <vector>
#include <algorithm>

#if !defined(alloca)
#	if defined(__GLIBC__) || defined(__sun) || defined(__APPLE__) || defined(__NEWLIB__)
#		include <alloca.h>     // alloca (glibc uses <alloca.h>. Note that Cygwin may have _WIN32 defined, so the order matters here)
#	elif defined(_WIN32)
#       include <malloc.h>     // alloca
#       if !defined(alloca)
#           define alloca _alloca  // for clang with MS Codegen
#       endif //alloca
#   elif defined(__GLIBC__) || defined(__sun)
#       include <alloca.h>     // alloca
#   else
#       include <stdlib.h>     // alloca
#   endif
#endif //alloca

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || \
	defined(__WIN64__) || defined(WIN64) || defined(_WIN64) || defined(_MSC_VER)
	#define stat _stat
	#define stricmp _stricmp
	#include <cctype>
	// this option need c++17
	// Modify By Dicky
		#include <Windows.h>
		#include <dirent_portable.h> // directly open the dirent file attached to this lib
	// Modify By Dicky end
	#define PATH_SEP '\\'
	#ifndef PATH_MAX
		#define PATH_MAX 260
	#endif // PATH_MAX
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || \
	defined(__NetBSD__) || defined(__APPLE__) || defined (__EMSCRIPTEN__)
	#define stricmp strcasecmp
	#include <sys/types.h>
	// this option need c++17
	#ifndef USE_STD_FILESYSTEM
		#include <dirent.h> 
	#endif // USE_STD_FILESYSTEM
	#define PATH_SEP '/'
#endif

#if IMGUI_RENDERING_GL3
#include <imgui_impl_opengl3.h>
#elif IMGUI_RENDERING_GL2
#include <imgui_impl_opengl2.h>
#elif IMGUI_RENDERING_VULKAN
#include <imgui_impl_vulkan.h>
#endif

namespace ImGui {
// ImGui Info
void ShowImGuiInfo()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::Text("Dear ImGui %s (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
    ImGui::Text("define: __cplusplus = %d", (int)__cplusplus);
    ImGui::Separator();
#ifdef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_OBSOLETE_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_WIN32_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_WIN32_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_FILE_FUNCTIONS
    ImGui::Text("define: IMGUI_DISABLE_FILE_FUNCTIONS");
#endif
#ifdef IMGUI_DISABLE_DEFAULT_ALLOCATORS
    ImGui::Text("define: IMGUI_DISABLE_DEFAULT_ALLOCATORS");
#endif
#ifdef IMGUI_USE_BGRA_PACKED_COLOR
    ImGui::Text("define: IMGUI_USE_BGRA_PACKED_COLOR");
#endif
#ifdef _WIN32
    ImGui::Text("define: _WIN32");
#endif
#ifdef _WIN64
    ImGui::Text("define: _WIN64");
#endif
#ifdef __linux__
    ImGui::Text("define: __linux__");
#endif
#ifdef __APPLE__
    ImGui::Text("define: __APPLE__");
#endif
#ifdef _MSC_VER
    ImGui::Text("define: _MSC_VER=%d", _MSC_VER);
#endif
#ifdef _MSVC_LANG
    ImGui::Text("define: _MSVC_LANG=%d", (int)_MSVC_LANG);
#endif
#ifdef __MINGW32__
    ImGui::Text("define: __MINGW32__");
#endif
#ifdef __MINGW64__
    ImGui::Text("define: __MINGW64__");
#endif
#ifdef __GNUC__
    ImGui::Text("define: __GNUC__ = %d", (int)__GNUC__);
#endif
#ifdef __clang_version__
    ImGui::Text("define: __clang_version__ = %s", __clang_version__);
#endif
    ImGui::Separator();
    ImGui::Text("Backend Platform Name: %s", io.BackendPlatformName ? io.BackendPlatformName : "NULL");
    ImGui::Text("Backend Renderer Name: %s", io.BackendRendererName ? io.BackendRendererName : "NULL");
#if IMGUI_RENDERING_VULKAN
    ImGui::Text("Backend GPU: %s", ImGui_ImplVulkan_GetDeviceName().c_str());
    ImGui::Text("Backend Vulkan API: %s", ImGui_ImplVulkan_GetApiVersion().c_str());
    ImGui::Text("Backend Vulkan Drv: %s", ImGui_ImplVulkan_GetDrvVersion().c_str());
    ImGui::Separator();
#elif IMGUI_OPENGL
#if IMGUI_RENDERING_GL3
    ImGui::Text("Gl Loader: %s", ImGui_ImplOpenGL3_GLLoaderName().c_str());
    ImGui::Text("GL Version: %s", ImGui_ImplOpenGL3_GetVerion().c_str());
#elif IMGUI_RENDERING_GL2
    ImGui::Text("Gl Loader: %s", ImGui_ImplOpenGL2_GLLoaderName().c_str());
    ImGui::Text("GL Version: %s", ImGui_ImplOpenGL2_GetVerion().c_str());
#endif
#endif
    ImGui::Text("Flash Timer: %.1f", io.ConfigMemoryCompactTimer >= 0.0f ? io.ConfigMemoryCompactTimer : 0);
    ImGui::Separator();
    ImGui::Text("Fonts: %d fonts", io.Fonts->Fonts.Size);
    ImGui::Text("Texure Size: %d x %d", io.Fonts->TexWidth, io.Fonts->TexHeight); 
    ImGui::Text("Display Size: %.2f x %.2f", io.DisplaySize.x, io.DisplaySize.y);
    ImGui::Text("Display Framebuffer Scale: %.2f %.2f", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
}

bool OpenWithDefaultApplication(const char* url,bool exploreModeForWindowsOS)	
{
#       ifdef _WIN32
            //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // Needed ??? Well, let's suppose the user initializes it himself for now"
            return ((size_t)ShellExecuteA( NULL, exploreModeForWindowsOS ? "explore" : "open", url, "", ".", SW_SHOWNORMAL ))>32;
#       else //_WIN32
            if (exploreModeForWindowsOS) exploreModeForWindowsOS = false;   // No warnings
            char tmp[4096];
            const char* openPrograms[]={"xdg-open","gnome-open"};	// Not sure what can I append here for MacOS

            static int openProgramIndex=-2;
            if (openProgramIndex==-2)   {
                openProgramIndex=-1;
                for (size_t i=0,sz=sizeof(openPrograms)/sizeof(openPrograms[0]);i<sz;i++) {
                    strcpy(tmp,"/usr/bin/");	// Well, we should check all the folders inside $PATH... and we ASSUME that /usr/bin IS inside $PATH (see below)
                    strcat(tmp,openPrograms[i]);
                    FILE* fd = (FILE *)ImFileOpen(tmp,"r");
                    if (fd) {
                        fclose(fd);
                        openProgramIndex = (int)i;
                        //printf(stderr,"found %s\n",tmp);
                        break;
                    }
                }
            }

            // Note that here we strip the prefix "/usr/bin" and just use openPrograms[openProgramsIndex].
            // Also note that if nothing has been found we use "xdg-open" (that might still work if it exists in $PATH, but not in /usr/bin).
            strcpy(tmp,openPrograms[openProgramIndex<0?0:openProgramIndex]);

            strcat(tmp," \"");
            strcat(tmp,url);
            strcat(tmp,"\"");
            return system(tmp)==0;
#       endif //_WIN32
}

void CloseAllPopupMenus()   {
    ImGuiContext& g = *GImGui;
    while (g.OpenPopupStack.size() > 0) g.OpenPopupStack.pop_back();
}

// Posted by Omar in one post. It might turn useful...
bool IsItemActiveLastFrame()    {
    ImGuiContext& g = *GImGui;
    if (g.ActiveIdPreviousFrame)
        return g.ActiveIdPreviousFrame== g.LastItemData.ID;
    return false;
}
bool IsItemJustReleased()   {
    return IsItemActiveLastFrame() && !ImGui::IsItemActive();
}
bool IsItemDisabled()    {
    ImGuiContext& g = *GImGui;
    return (g.CurrentItemFlags & ImGuiItemFlags_Disabled) == ImGuiItemFlags_Disabled;
}

void Debug_DrawItemRect(const ImVec4& col)
{
    auto drawList = ImGui::GetWindowDrawList();
    auto itemMin = ImGui::GetItemRectMin();
    auto itemMax = ImGui::GetItemRectMax();
    drawList->AddRect(itemMin, itemMax, ImColor(col));
}

const ImFont *GetFont(int fntIndex) {return (fntIndex>=0 && fntIndex<ImGui::GetIO().Fonts->Fonts.size()) ? ImGui::GetIO().Fonts->Fonts[fntIndex] : NULL;}
void PushFont(int fntIndex)    {
    IM_ASSERT(fntIndex>=0 && fntIndex<ImGui::GetIO().Fonts->Fonts.size());
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[fntIndex]);
}
void TextColoredV(int fntIndex, const ImVec4 &col, const char *fmt, va_list args) {
    ImGui::PushFont(fntIndex);
    ImGui::TextColoredV(col,fmt, args);
    ImGui::PopFont();
}
void TextColored(int fntIndex, const ImVec4 &col, const char *fmt,...)  {
    va_list args;
    va_start(args, fmt);
    TextColoredV(fntIndex,col, fmt, args);
    va_end(args);
}
void TextV(int fntIndex, const char *fmt, va_list args) {
    if (ImGui::GetCurrentWindow()->SkipItems) return;

    ImGuiContext& g = *GImGui;
    const char* text, *text_end;
    ImFormatStringToTempBufferV(&text, &text_end, fmt, args);
    ImGui::PushFont(fntIndex);
    TextUnformatted(text, text_end);
    ImGui::PopFont();
}
void Text(int fntIndex, const char *fmt,...)    {
    va_list args;
    va_start(args, fmt);
    TextV(fntIndex,fmt, args);
    va_end(args);
}

bool GetTexCoordsFromGlyph(unsigned short glyph, ImVec2 &uv0, ImVec2 &uv1)
{
    if (!GImGui->Font) return false;
    const ImFontGlyph* g = GImGui->Font->FindGlyph(glyph);
    if (g)  {
        uv0.x = g->U0; uv0.y = g->V0;
        uv1.x = g->U1; uv1.y = g->V1;
        return true;
    }
    return false;
}

float CalcMainMenuHeight()
{
    // Warning: according to https://github.com/ocornut/imgui/issues/252 this approach can fail [Better call ImGui::GetWindowSize().y from inside the menu and store the result somewhere]
    if (GImGui->FontBaseSize>0) return GImGui->FontBaseSize + GImGui->Style.FramePadding.y * 2.0f;
    else {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        ImFont* font = ImGui::GetFont();
        if (!font) {
            if (io.Fonts->Fonts.size()>0) font = io.Fonts->Fonts[0];
            else return (14)+style.FramePadding.y * 2.0f;
        }
        return (io.FontGlobalScale * font->Scale * font->FontSize) + style.FramePadding.y * 2.0f;
    }
}

void RenderMouseCursor(const char* mouse_cursor, ImVec2 offset, float base_scale, float rotate, ImU32 col_fill, ImU32 col_border, ImU32 col_shadow)
{
    ImGuiViewportP* viewport = (ImGuiViewportP*)ImGui::GetWindowViewport();
    ImDrawList* draw_list = ImGui::GetForegroundDrawList(viewport);
    ImGuiIO& io = ImGui::GetIO();
    const float FontSize = draw_list->_Data->FontSize;
    ImVec2 size(FontSize, FontSize);
    const ImVec2 pos = io.MousePos - offset;
    const float scale = base_scale * viewport->DpiScale;
    if (!viewport->GetMainRect().Overlaps(ImRect(pos, pos + ImVec2(size.x + 2, size.y + 2) * scale)))
        return;

    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    int rotation_start_index = draw_list->VtxBuffer.Size;
    draw_list->AddText(pos + ImVec2(-1, -1), col_border, mouse_cursor);
    draw_list->AddText(pos + ImVec2(1, 1), col_shadow, mouse_cursor);
    draw_list->AddText(pos, col_fill, mouse_cursor);
    if (rotate != 0.f)
    {
        float rad = M_PI / 180 * (90 - rotate);
        ImVec2 l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX); // bounds
        auto& buf = draw_list->VtxBuffer;
        float s = sin(rad), c = cos(rad);
        for (int i = rotation_start_index; i < buf.Size; i++)
            l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);
        ImVec2 center = ImVec2((l.x + u.x) / 2, (l.y + u.y) / 2);
        center = ImRotate(center, s, c) - center;
        
        for (int i = rotation_start_index; i < buf.Size; i++)
            buf[i].pos = ImRotate(buf[i].pos, s, c) - center;
    }
}

// These two methods are inspired by imguidock.cpp
void PutInBackground(const char* optionalRootWindowName)
{
    ImGuiWindow* w = optionalRootWindowName ? FindWindowByName(optionalRootWindowName) : GetCurrentWindow();
    if (!w) return;
    ImGuiContext& g = *GImGui;
    if (g.Windows[0] == w) return;
    const int isz = g.Windows.Size;
    for (int i = 0; i < isz; i++)
    {
        if (g.Windows[i] == w)
        {
            for (int j = i; j > 0; --j) g.Windows[j] = g.Windows[j-1];  // shifts [0,j-1] to [1,j]
            g.Windows[0] = w;
            break;
        }
    }
}

void PutInForeground(const char* optionalRootWindowName)
{
    ImGuiWindow* w = optionalRootWindowName ? FindWindowByName(optionalRootWindowName) : GetCurrentWindow();
    if (!w) return;
    ImGuiContext& g = *GImGui;
    const int iszMinusOne = g.Windows.Size - 1;
    if (iszMinusOne<0 || g.Windows[iszMinusOne] == w) return;
    for (int i = iszMinusOne; i >= 0; --i)
    {
        if (g.Windows[i] == w)
        {
            for (int j = i; j < iszMinusOne; j++) g.Windows[j] = g.Windows[j+1];  // shifts [i+1,iszMinusOne] to [i,iszMinusOne-1]
            g.Windows[iszMinusOne] = w;
            break;
        }
    }
}

ScopedItemWidth::ScopedItemWidth(float width)
{
    ImGui::PushItemWidth(width);
}

ScopedItemWidth::~ScopedItemWidth()
{
    Release();
}

void ScopedItemWidth::Release()
{
    if (m_IsDone)
        return;

    ImGui::PopItemWidth();

    m_IsDone = true;
}

ScopedDisableItem::ScopedDisableItem(bool disable, float disabledAlpha)
    : m_Disable(disable)
{
    if (!m_Disable)
        return;

    ImGuiContext& g = *GImGui;
    auto wasDisabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) == ImGuiItemFlags_Disabled;

    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

    auto& stale = ImGui::GetStyle();
    m_LastAlpha = stale.Alpha;

    // Don't override alpha if we're already in disabled context.
    if (!wasDisabled)
        stale.Alpha = disabledAlpha;
}

ScopedDisableItem::~ScopedDisableItem()
{
    Release();
}

void ScopedDisableItem::Release()
{
    if (!m_Disable)
        return;

    auto& stale = ImGui::GetStyle();
    stale.Alpha = m_LastAlpha;

    ImGui::PopItemFlag();

    m_Disable = false;
}

ScopedSuspendLayout::ScopedSuspendLayout()
{
    m_Window = ImGui::GetCurrentWindow();
    m_CursorPos = m_Window->DC.CursorPos;
    m_CursorPosPrevLine = m_Window->DC.CursorPosPrevLine;
    m_CursorMaxPos = m_Window->DC.CursorMaxPos;
    m_CurrLineSize = m_Window->DC.CurrLineSize;
    m_PrevLineSize = m_Window->DC.PrevLineSize;
    m_CurrLineTextBaseOffset = m_Window->DC.CurrLineTextBaseOffset;
    m_PrevLineTextBaseOffset = m_Window->DC.PrevLineTextBaseOffset;
}

ScopedSuspendLayout::~ScopedSuspendLayout()
{
    Release();
}

void ScopedSuspendLayout::Release()
{
    if (m_Window == nullptr)
        return;

    m_Window->DC.CursorPos = m_CursorPos;
    m_Window->DC.CursorPosPrevLine = m_CursorPosPrevLine;
    m_Window->DC.CursorMaxPos = m_CursorMaxPos;
    m_Window->DC.CurrLineSize = m_CurrLineSize;
    m_Window->DC.PrevLineSize = m_PrevLineSize;
    m_Window->DC.CurrLineTextBaseOffset = m_CurrLineTextBaseOffset;
    m_Window->DC.PrevLineTextBaseOffset = m_PrevLineTextBaseOffset;

    m_Window = nullptr;
}

ItemBackgroundRenderer::ItemBackgroundRenderer(OnDrawCallback onDrawBackground)
    : m_OnDrawBackground(std::move(onDrawBackground))
{
    m_DrawList = ImGui::GetWindowDrawList();
    m_Splitter.Split(m_DrawList, 2);
    m_Splitter.SetCurrentChannel(m_DrawList, 1);
}

ItemBackgroundRenderer::~ItemBackgroundRenderer()
{
    Commit();
}

void ItemBackgroundRenderer::Commit()
{
    if (m_Splitter._Current == 0)
        return;

    m_Splitter.SetCurrentChannel(m_DrawList, 0);

    if (m_OnDrawBackground)
        m_OnDrawBackground(m_DrawList);

    m_Splitter.Merge(m_DrawList);
}

void ItemBackgroundRenderer::Discard()
{
    if (m_Splitter._Current == 1)
        m_Splitter.Merge(m_DrawList);
}

StorageHandler<MostRecentlyUsedList::Settings> MostRecentlyUsedList::s_Storage;


void MostRecentlyUsedList::Install(ImGuiContext* context)
{
    context->SettingsHandlers.push_back(s_Storage.MakeHandler("MostRecentlyUsedList"));

    s_Storage.ReadLine = [](ImGuiContext*, Settings* entry, const char* line)
    {
        const char* lineEnd = line + strlen(line);

        auto parseListEntry = [lineEnd](const char* line, int& index) -> const char*
        {
            char* indexEnd = nullptr;
            errno = 0;
            index = strtol(line, &indexEnd, 10);
            if (errno == ERANGE)
                return nullptr;
            if (indexEnd >= lineEnd)
                return nullptr;
            if (*indexEnd != '=')
                return nullptr;
            return indexEnd + 1;
        };


        int index = 0;
        if (auto path = parseListEntry(line, index))
        {
            if (static_cast<int>(entry->m_List.size()) <= index)
                entry->m_List.resize(index + 1);
            entry->m_List[index] = path;
        }
    };

    s_Storage.WriteAll = [](ImGuiContext*, ImGuiTextBuffer* out_buf, const StorageHandler<Settings>::Storage& storage)
    {
        for (auto& entry : storage)
        {
            out_buf->appendf("[%s][##%s]\n", "MostRecentlyUsedList", entry.first.c_str());
            int index = 0;
            for (auto& value : entry.second->m_List)
                out_buf->appendf("%d=%s\n", index++, value.c_str());
            out_buf->append("\n");
        }
    };
}

MostRecentlyUsedList::MostRecentlyUsedList(const char* id, int capacity /*= 10*/)
    : m_ID(id)
    , m_Capacity(capacity)
    , m_List(s_Storage.FindOrCreate(id)->m_List)
{
}

void MostRecentlyUsedList::Add(const std::string& item)
{
    Add(item.c_str());
}

void MostRecentlyUsedList::Add(const char* item)
{
    auto itemIt = std::find(m_List.begin(), m_List.end(), item);
    if (itemIt != m_List.end())
    {
        // Item is already on the list. Rotate list to move it to the
        // first place.
        std::rotate(m_List.begin(), itemIt, itemIt + 1);
    }
    else
    {
        // Push new item to the back, rotate list to move it to the front,
        // pop back last element if we're over capacity.
        m_List.push_back(item);
        std::rotate(m_List.begin(), m_List.end() - 1, m_List.end());
        if (static_cast<int>(m_List.size()) > m_Capacity)
            m_List.pop_back();
    }

    PushToStorage();

    ImGui::MarkIniSettingsDirty();
}

void MostRecentlyUsedList::Clear()
{
    if (m_List.empty())
        return;

    m_List.resize(0);

    PushToStorage();

    ImGui::MarkIniSettingsDirty();
}

const std::vector<std::string>& MostRecentlyUsedList::GetList() const
{
    return m_List;
}

int MostRecentlyUsedList::Size() const
{
    return static_cast<int>(m_List.size());
}

void MostRecentlyUsedList::PullFromStorage()
{
    if (auto settings = s_Storage.Find(m_ID.c_str()))
        m_List = settings->m_List;
}

void MostRecentlyUsedList::PushToStorage()
{
    auto settings = s_Storage.FindOrCreate(m_ID.c_str());
    settings->m_List = m_List;
}

void Grid::Begin(const char* id, int columns, float width)
{
    Begin(ImGui::GetID(id), columns, width);
}

void Grid::Begin(ImU32 id, int columns, float width)
{
    m_CursorPos = ImGui::GetCursorScreenPos();

    ImGui::PushID(id);
    m_Columns = ImMax(1, columns);
    m_Storage = ImGui::GetStateStorage();

    for (int i = 0; i < columns; ++i)
    {
        ImGui::PushID(ColumnSeed());
        m_Storage->SetFloat(ImGui::GetID("MaximumColumnWidthAcc"), -1.0f);
        ImGui::PopID();
    }

    m_ColumnAlignment = 0.0f;
    m_MinimumWidth = width;

    ImGui::BeginGroup();

    EnterCell(0, 0);
}

void Grid::NextColumn(bool keep_max)
{
    LeaveCell(keep_max);

    int nextColumn = m_Column + 1;
    int nextRow    = 0;
    if (nextColumn > m_Columns)
    {
        nextColumn -= m_Columns;
        nextRow    += 1;
    }

    auto cursorPos = m_CursorPos;
    for (int i = 0; i < nextColumn; ++i)
    {
        ImGui::PushID(ColumnSeed(i));
        auto maximumColumnWidth = m_Storage->GetFloat(ImGui::GetID("MaximumColumnWidth"), -1.0f);
        ImGui::PopID();

        if (maximumColumnWidth > 0.0f)
            cursorPos.x += maximumColumnWidth + ImGui::GetStyle().ItemSpacing.x;
    }

    ImGui::SetCursorScreenPos(cursorPos);

    EnterCell(nextColumn, nextRow);
}

void Grid::NextRow()
{
    LeaveCell();

    auto cursorPos = ImGui::GetCursorScreenPos();
    cursorPos.x = m_CursorPos.x;
    for (int i = 0; i < m_Column; ++i)
    {
        ImGui::PushID(ColumnSeed(i));
        auto maximumColumnWidth = m_Storage->GetFloat(ImGui::GetID("MaximumColumnWidth"), -1.0f);
        ImGui::PopID();

        if (maximumColumnWidth > 0.0f)
            cursorPos.x += maximumColumnWidth + ImGui::GetStyle().ItemSpacing.x;
    }

    ImGui::SetCursorScreenPos(cursorPos);

    EnterCell(m_Column, m_Row + 1);
}

void Grid::EnterCell(int column, int row)
{
    m_Column = column;
    m_Row    = row;

    ImGui::PushID(ColumnSeed());
    m_MaximumColumnWidthAcc = m_Storage->GetFloat(ImGui::GetID("MaximumColumnWidthAcc"), -1.0f);
    auto maximumColumnWidth = m_Storage->GetFloat(ImGui::GetID("MaximumColumnWidth"), -1.0f);
    ImGui::PopID();

    ImGui::PushID(Seed());
    auto lastCellWidth = m_Storage->GetFloat(ImGui::GetID("LastCellWidth"), -1.0f);

    if (maximumColumnWidth >= 0.0f && lastCellWidth >= 0.0f)
    {
        auto freeSpace = maximumColumnWidth - lastCellWidth;

        auto offset = ImFloor(m_ColumnAlignment * freeSpace);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::Dummy(ImVec2(offset, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PopStyleVar();
    }

    ImGui::BeginGroup();
}

void Grid::LeaveCell(bool keep_max)
{
    ImGui::EndGroup();

    auto itemSize = ImGui::GetItemRectSize();
    m_Storage->SetFloat(ImGui::GetID("LastCellWidth"), itemSize.x);
    ImGui::PopID();

    m_MaximumColumnWidthAcc = keep_max ? ImMax(m_MaximumColumnWidthAcc, itemSize.x) : itemSize.x;
    ImGui::PushID(ColumnSeed());
    m_Storage->SetFloat(ImGui::GetID("MaximumColumnWidthAcc"), m_MaximumColumnWidthAcc);
    ImGui::PopID();
}

void Grid::SetColumnAlignment(float alignment)
{
    alignment = ImClamp(alignment, 0.0f, 1.0f);
    m_ColumnAlignment = alignment;
}

void Grid::End()
{
    LeaveCell();

    ImGui::EndGroup();

    float totalWidth = 0.0f;
    for (int i = 0; i < m_Columns; ++i)
    {
        ImGui::PushID(ColumnSeed(i));
        auto currentMaxCellWidth = m_Storage->GetFloat(ImGui::GetID("MaximumColumnWidthAcc"), -1.0f);
        totalWidth += currentMaxCellWidth;
        m_Storage->SetFloat(ImGui::GetID("MaximumColumnWidth"), currentMaxCellWidth);
        ImGui::PopID();
    }

    if (totalWidth < m_MinimumWidth)
    {
        auto spaceToDivide = m_MinimumWidth - totalWidth;
        auto spacePerColumn = ImCeil(spaceToDivide / m_Columns);

        for (int i = 0; i < m_Columns; ++i)
        {
            ImGui::PushID(ColumnSeed(i));
            auto columnWidth = m_Storage->GetFloat(ImGui::GetID("MaximumColumnWidth"), -1.0f);
            columnWidth += spacePerColumn;
            m_Storage->SetFloat(ImGui::GetID("MaximumColumnWidth"), columnWidth);
            ImGui::PopID();

            spaceToDivide -= spacePerColumn;
            if (spaceToDivide < 0)
                spacePerColumn += spaceToDivide;
        }
    }

    ImGui::PopID();
}
} // namespace Imgui

#include <stdio.h>  // FILE
namespace ImGuiHelper
{
static const char* FieldTypeNames[ImGui::FT_COUNT+1] = {"INT","UNSIGNED","FLOAT","DOUBLE","STRING","ENUM","BOOL","COLOR","TEXTLINE","CUSTOM","COUNT"};
static const char* FieldTypeFormats[ImGui::FT_COUNT]={"%d","%u","%f","%f","%s","%d","%d","%f","%s","%s"};
static const char* FieldTypeFormatsWithCustomPrecision[ImGui::FT_COUNT]={"%.*d","%*u","%.*f","%.*f","%*s","%*d","%*d","%.*f","%*s","%*s"};

void Deserializer::clear()
{
    if (f_data) ImGui::MemFree(f_data);
    f_data = NULL;f_size=0;
}

bool Deserializer::loadFromFile(const char *filename)
{
    clear();
    if (!filename) return false;
    FILE* f;
    if ((f = (FILE *)ImFileOpen(filename, "rt")) == NULL) return false;
    if (fseek(f, 0, SEEK_END))
    {
        fclose(f);
        return false;
    }
    const long f_size_signed = ftell(f);
    if (f_size_signed == -1)
    {
        fclose(f);
        return false;
    }
    f_size = (size_t)f_size_signed;
    if (fseek(f, 0, SEEK_SET))
    {
        fclose(f);
        return false;
    }
    f_data = (char*)ImGui::MemAlloc(f_size+1);
    f_size = fread(f_data, 1, f_size, f); // Text conversion alter read size so let's not be fussy about return value
    fclose(f);
    if (f_size == 0)
    {
        clear();
        return false;
    }
    f_data[f_size] = 0;
    ++f_size;
    return true;
}
bool Deserializer::allocate(size_t sizeToAllocate, const char *optionalTextToCopy, size_t optionalTextToCopySize)
{
    clear();
    if (sizeToAllocate==0) return false;
    f_size = sizeToAllocate;
    f_data = (char*)ImGui::MemAlloc(f_size);
    if (!f_data) {clear();return false;}
    if (optionalTextToCopy && optionalTextToCopySize>0) memcpy(f_data,optionalTextToCopy,optionalTextToCopySize>f_size ? f_size:optionalTextToCopySize);
    return true;
}
Deserializer::Deserializer(const char *filename) : f_data(NULL),f_size(0)
{
    if (filename) loadFromFile(filename);
}
Deserializer::Deserializer(const char *text, size_t textSizeInBytes) : f_data(NULL),f_size(0)
{
    allocate(textSizeInBytes,text,textSizeInBytes);
}

const char* Deserializer::parse(Deserializer::ParseCallback cb, void *userPtr, const char *optionalBufferStart) const
{
    if (!cb || !f_data || f_size==0) return NULL;
    //------------------------------------------------
    // Parse file in memory
    char name[128];name[0]='\0';
    char typeName[32];char format[32]="";bool quitParsing = false;
    char charBuffer[sizeof(double)*10];void* voidBuffer = (void*) &charBuffer[0];
    static char textBuffer[2050];
    const char* varName = NULL;int numArrayElements = 0;FieldType ft = ImGui::FT_COUNT;
    const char* buf_end = f_data + f_size-1;
    for (const char* line_start = optionalBufferStart ? optionalBufferStart : f_data; line_start < buf_end; )
    {
        const char* line_end = line_start;
        while (line_end < buf_end && *line_end != '\n' && *line_end != '\r') line_end++;

        if (name[0]=='\0' && line_start[0] == '[' && line_end > line_start && line_end[-1] == ']')
        {
            ImFormatString(name, IM_ARRAYSIZE(name), "%.*s", (int)(line_end-line_start-2), line_start+1);
            //fprintf(stderr,"name: %s\n",name);  // dbg

            // Here we have something like: FLOAT-4:VariableName
            // We have to split into FLOAT 4 VariableName
            varName = NULL;numArrayElements = 0;ft = ImGui::FT_COUNT;format[0]='\0';
            const char* colonCh = strchr(name,':');
            const char* minusCh = strchr(name,'-');
            if (!colonCh)
            {
                fprintf(stderr,"ImGuiHelper::Deserializer::parse(...) warning (skipping line with no semicolon). name: %s\n",name);  // dbg
                name[0]='\0';
            }
            else
            {
                ptrdiff_t diff = 0,diff2 = 0;
                if (!minusCh || (minusCh-colonCh)>0)  {diff = (colonCh-name);numArrayElements=1;}
                else
                {
                    diff = (minusCh-name);
                    diff2 = colonCh-minusCh;
                    if (diff2>1 && diff2<7)
                    {
                        static char buff[8];
                        strncpy(&buff[0],(const char*) &minusCh[1],diff2);buff[diff2-1]='\0';
                        sscanf(buff,"%d",&numArrayElements);
                        //fprintf(stderr,"WARN: %s\n",buff);
                    }
                    else if (diff>0) numArrayElements = ((char)name[diff+1]-(char)'0');  // TODO: FT_STRING needs multibytes -> rewrite!
                }
                if (diff>0)
                {
                    const size_t len = (size_t)(diff>31?31:diff);
                    strncpy(typeName,name,len);typeName[len]='\0';

                    for (int t=0;t<=ImGui::FT_COUNT;t++)
                    {
                        if (strcmp(typeName,FieldTypeNames[t])==0)
                        {
                            ft = (FieldType) t;break;
                        }
                    }
                    varName = ++colonCh;

                    const bool isTextOrCustomType = ft==ImGui::FT_STRING || ft==ImGui::FT_TEXTLINE  || ft==ImGui::FT_CUSTOM;
                    if (ft==ImGui::FT_COUNT || numArrayElements<1 || (numArrayElements>4 && !isTextOrCustomType))
                    {
                        fprintf(stderr,"ImGuiHelper::Deserializer::parse(...) Error (wrong type detected): line:%s type:%d numArrayElements:%d varName:%s typeName:%s\n",name,(int)ft,numArrayElements,varName,typeName);
                        varName=NULL;
                    }
                    else
                    {
                        if (ft==ImGui::FT_STRING && varName && varName[0]!='\0')
                        {
                            if (numArrayElements==1 && (!minusCh || (minusCh-colonCh)>0))
                            {
                                numArrayElements=0;   // NEW! To handle blank strings ""
                            }
                            //Process soon here, as the string can be multiline
                            line_start = ++line_end;
                            //--------------------------------------------------------
                            int cnt = 0;
                            while (line_end < buf_end && cnt++ < numArrayElements-1) ++line_end;
                            textBuffer[0]=textBuffer[2049]='\0';
                            const int maxLen = numArrayElements>0 ? (cnt>2049?2049:cnt) : 0;
                            strncpy(textBuffer,line_start,maxLen+1);
                            textBuffer[maxLen]='\0';
                            quitParsing = cb(ft,numArrayElements,(void*)textBuffer,varName,userPtr);
                            //fprintf(stderr,"Deserializer::parse(...) value:\"%s\" to type:%d numArrayElements:%d varName:%s maxLen:%d\n",textBuffer,(int)ft,numArrayElements,varName,maxLen);  // dbg


                            //else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
                            //--------------------------------------------------------
                            ft = ImGui::FT_COUNT;name[0]='\0';varName=NULL; // mandatory                            

                        }
                        else if (!isTextOrCustomType)
                        {
                            format[0]='\0';
                            for (int t=0;t<numArrayElements;t++)
                            {
                                if (t>0) strcat(format," ");
                                strcat(format,FieldTypeFormats[ft]);
                            }
                            // DBG:
                            //fprintf(stderr,"Deserializer::parse(...) DBG: line:%s type:%d numArrayElements:%d varName:%s format:%s\n",name,(int)ft,numArrayElements,varName,format);  // dbg
                        }
                    }
                }
            }
        }
        else if (varName && varName[0]!='\0')
        {
            switch (ft)
            {
            case ImGui::FT_FLOAT:
            case ImGui::FT_COLOR:
            {
                float* p = (float*) voidBuffer;
                if ((numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                    (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                    (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                    (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                    quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_DOUBLE:
            {
                double* p = (double*) voidBuffer;
                if ((numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                    (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                    (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                    (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                    quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_INT:
            case ImGui::FT_ENUM:
            {
                int* p = (int*) voidBuffer;
                if ((numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                    (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                    (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                    (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                    quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_BOOL:
            {
                bool* p = (bool*) voidBuffer;
                int tmp[4];
                if ((numArrayElements==1 && sscanf(line_start, format, &tmp[0])==numArrayElements) ||
                    (numArrayElements==2 && sscanf(line_start, format, &tmp[0],&tmp[1])==numArrayElements) ||
                    (numArrayElements==3 && sscanf(line_start, format, &tmp[0],&tmp[1],&tmp[2])==numArrayElements) ||
                    (numArrayElements==4 && sscanf(line_start, format, &tmp[0],&tmp[1],&tmp[2],&tmp[3])==numArrayElements))    {
                    for (int i=0;i<numArrayElements;i++) p[i] = tmp[i];
                    quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                }
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_UNSIGNED:
            {
                unsigned* p = (unsigned*) voidBuffer;
                if ((numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                    (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                    (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                    (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                    quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_CUSTOM:
            case ImGui::FT_TEXTLINE:
            {
                // A similiar code can be used to parse "numArrayElements" line of text
                for (int i=0;i<numArrayElements;i++)
                {
                    textBuffer[0]=textBuffer[2049]='\0';
                    const int maxLen = (line_end-line_start)>2049?2049:(line_end-line_start);
                    if (maxLen<=0) break;
                    strncpy(textBuffer,line_start,maxLen+1);textBuffer[maxLen]='\0';
                    quitParsing = cb(ft,i,(void*)textBuffer,varName,userPtr);

                    //fprintf(stderr,"%d) \"%s\"\n",i,textBuffer);  // Dbg

                    if (quitParsing) break;
                    line_start = line_end+1;
                    line_end = line_start;
                    if (line_end == buf_end) break;
                    while (line_end < buf_end && *line_end != '\n' && *line_end != '\r') line_end++;
                }
            }
            break;
            default:
            fprintf(stderr,"Deserializer::parse(...) Warning missing value type:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            break;
            }
            //---------------------------------------------------------------------------------
            name[0]='\0';varName=NULL; // mandatory
        }

        line_start = line_end+1;

        if (quitParsing) return line_start;
    }

    //------------------------------------------------
    return buf_end;
}

bool GetFileContent(const char *filePath, ImVector<char> &contentOut, bool clearContentOutBeforeUsage, const char *modes, bool appendTrailingZeroIfModesIsNotBinary)
{
    ImVector<char>& f_data = contentOut;
    if (clearContentOutBeforeUsage) f_data.clear();
//----------------------------------------------------
    if (!filePath) return false;
    const bool appendTrailingZero = appendTrailingZeroIfModesIsNotBinary && modes && strlen(modes)>0 && modes[strlen(modes)-1]!='b';
    FILE* f;
    if ((f = (FILE *)ImFileOpen(filePath, modes)) == NULL) return false;
    if (fseek(f, 0, SEEK_END))
    {
        fclose(f);
        return false;
    }
    const long f_size_signed = ftell(f);
    if (f_size_signed == -1)
    {
        fclose(f);
        return false;
    }
    size_t f_size = (size_t)f_size_signed;
    if (fseek(f, 0, SEEK_SET))
    {
        fclose(f);
        return false;
    }
    f_data.resize(f_size+(appendTrailingZero?1:0));
    const size_t f_size_read = f_size>0 ? fread(&f_data[0], 1, f_size, f) : 0;
    fclose(f);
    if (f_size_read == 0 || f_size_read!=f_size)    return false;
    if (appendTrailingZero) f_data[f_size] = '\0';
//----------------------------------------------------
    return true;
}

bool SetFileContent(const char *filePath, const unsigned char* content, int contentSize,const char* modes)
{
    if (!filePath || !content) return false;
    FILE* f;
    if ((f = (FILE *)ImFileOpen(filePath, modes)) == NULL) return false;
    fwrite(content, contentSize, 1, f);
    fclose(f);f=NULL;
    return true;
}

class ISerializable
{
public:
    ISerializable() {}
    virtual ~ISerializable() {}
    virtual void close()=0;
    virtual bool isValid() const=0;
    virtual int print(const char* fmt, ...)=0;
    virtual int getTypeID() const=0;
};
class SerializeToFile : public ISerializable
{
public:
    SerializeToFile(const char* filename) : f(NULL)
    {
        saveToFile(filename);
    }
    SerializeToFile() : f(NULL) {}
    ~SerializeToFile() {close();}
    bool saveToFile(const char* filename)
    {
        close();
        f = (FILE *)ImFileOpen(filename,"w");
        return (f);
    }
    void close() {if (f) fclose(f);f=NULL;}
    bool isValid() const {return (f);}
    int print(const char* fmt, ...)
    {
        va_list args;va_start(args, fmt);
        const int rv = vfprintf(f,fmt,args);
        va_end(args);
        return rv;
    }
    int getTypeID() const {return 0;}
protected:
    FILE* f;
};
class SerializeToBuffer : public ISerializable
{
public:
    SerializeToBuffer(int initialCapacity=2048) {b.reserve(initialCapacity);b.resize(1);b[0]='\0';}
    ~SerializeToBuffer() {close();}
    bool saveToFile(const char* filename)
    {
        if (!isValid()) return false;
        return SetFileContent(filename,(unsigned char*)&b[0],b.size(),"w");
    }
    void close() {b.clear();ImVector<char> o;b.swap(o);b.resize(1);b[0]='\0';}
    bool isValid() const {return b.size()>0;}
    int print(const char* fmt, ...)
    {
        va_list args,args2;
        va_start(args, fmt);
        va_copy(args2,args);                                    // since C99 (MANDATORY! otherwise we must reuse va_start(args2,fmt): slow)
        const int additionalSize = vsnprintf(NULL,0,fmt,args);  // since C99
        va_end(args);
        //IM_ASSERT(additionalSize>0);

        const int startSz = b.size();
        b.resize(startSz+additionalSize);
        const int rv = vsnprintf(&b[startSz-1],additionalSize,fmt,args2);
        va_end(args2);
        //IM_ASSERT(additionalSize==rv);
        //IM_ASSERT(v[startSz+additionalSize-1]=='\0');

        return rv;
    }
    inline const char* getBuffer() const {return b.size()>0 ? &b[0] : NULL;}
    inline int getBufferSize() const {return b.size();}
    int getTypeID() const {return 1;}
protected:
    ImVector<char> b;
};
const char* Serializer::getBuffer() const
{
    return (f && f->getTypeID()==1 && f->isValid()) ? static_cast<SerializeToBuffer*>(f)->getBuffer() : NULL;
}
int Serializer::getBufferSize() const
{
    return (f && f->getTypeID()==1 && f->isValid()) ? static_cast<SerializeToBuffer*>(f)->getBufferSize() : 0;
}
bool Serializer::WriteBufferToFile(const char* filename,const char* buffer,int bufferSize)
{
    if (!buffer) return false;
    FILE* f = (FILE *)ImFileOpen(filename,"w");
    if (!f) return false;
    fwrite((void*) buffer,bufferSize,1,f);
    fclose(f);
    return true;
}

void Serializer::clear() {if (f) {f->close();}}
Serializer::Serializer(const char *filename)
{
    f=(SerializeToFile*) ImGui::MemAlloc(sizeof(SerializeToFile));
    IM_PLACEMENT_NEW((SerializeToFile*)f) SerializeToFile(filename);
}
Serializer::Serializer(int memoryBufferCapacity)
{
    f=(SerializeToBuffer*) ImGui::MemAlloc(sizeof(SerializeToBuffer));
    IM_PLACEMENT_NEW((SerializeToBuffer*)f) SerializeToBuffer(memoryBufferCapacity);
}
Serializer::~Serializer()
{
    if (f)
    {
        f->~ISerializable();
        ImGui::MemFree(f);
        f=NULL;
    }
}
template <typename T> inline static bool SaveTemplate(ISerializable* f,FieldType ft, const T* pValue, const char* name, int numArrayElements=1, int prec=-1)
{
    if (!f || ft==ImGui::FT_COUNT  || ft==ImGui::FT_CUSTOM || numArrayElements<0 || numArrayElements>4 || !pValue || !name || name[0]=='\0') return false;
    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);
    // value
    const char* precision = FieldTypeFormatsWithCustomPrecision[ft];
    for (int t=0;t<numArrayElements;t++)
    {
        if (t>0) f->print(" ");
        f->print(precision,prec,pValue[t]);
    }
    f->print("\n\n");
    return true;
}
bool Serializer::save(FieldType ft, const float* pValue, const char* name, int numArrayElements,  int prec)
{
    IM_ASSERT(ft==ImGui::FT_FLOAT || ft==ImGui::FT_COLOR);
    return SaveTemplate<float>(f,ft,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const double* pValue,const char* name,int numArrayElements, int prec)
{
    return SaveTemplate<double>(f,ImGui::FT_DOUBLE,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const bool* pValue,const char* name,int numArrayElements)
{
    if (!pValue || numArrayElements<0 || numArrayElements>4) return false;
    static int tmp[4];
    for (int i=0;i<numArrayElements;i++) tmp[i] = pValue[i] ? 1 : 0;
    return SaveTemplate<int>(f,ImGui::FT_BOOL,tmp,name,numArrayElements);
}
bool Serializer::save(FieldType ft,const int* pValue,const char* name,int numArrayElements, int prec)
{
    IM_ASSERT(ft==ImGui::FT_INT || ft==ImGui::FT_BOOL || ft==ImGui::FT_ENUM);
    if (prec==0) prec=-1;
    return SaveTemplate<int>(f,ft,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const unsigned* pValue,const char* name,int numArrayElements, int prec)
{
    if (prec==0) prec=-1;
    return SaveTemplate<unsigned>(f,ImGui::FT_UNSIGNED,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const char* pValue,const char* name,int pValueSize)
{
    FieldType ft = ImGui::FT_STRING;
    int numArrayElements = pValueSize;
    if (!f || ft==ImGui::FT_COUNT || !pValue || !name || name[0]=='\0') return false;
    numArrayElements = pValueSize;
    pValueSize=(int)strlen(pValue);if (numArrayElements>pValueSize || numArrayElements<=0) numArrayElements=pValueSize;
    if (numArrayElements<0) numArrayElements=0;

    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);
    // value
    f->print("%s\n\n",pValue);
    return true;
}
bool Serializer::saveTextLines(const char* pValue,const char* name)
{
    FieldType ft = ImGui::FT_TEXTLINE;
    if (!f || ft==ImGui::FT_COUNT || !pValue || !name || name[0]=='\0') return false;
    const char *tmp;const char *start = pValue;
    int left = strlen(pValue);int numArrayElements =0;  // numLines
    bool endsWithNewLine = pValue[left-1]=='\n';
    while ((tmp=strchr(start, '\n')))
    {
        ++numArrayElements;
        left-=tmp-start-1;
        start = ++tmp;  // to skip '\n'
    }
    if (left>0) ++numArrayElements;
    if (numArrayElements==0) return false;

    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);
    // value
    f->print("%s",pValue);
    if (!endsWithNewLine)  f->print("\n");
    f->print("\n");
    return true;
}
bool Serializer::saveTextLines(int numValues,bool (*items_getter)(void* data, int idx, const char** out_text),void* data,const char* name)
{
    FieldType ft = ImGui::FT_TEXTLINE;
    if (!items_getter || !f || ft==ImGui::FT_COUNT || numValues<=0 || !name || name[0]=='\0') return false;
    int numArrayElements =numValues;  // numLines

    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);

    // value
    const char* text=NULL;int len=0;
    for (int i=0;i<numArrayElements;i++)
    {
        if (items_getter(data,i,&text))
        {
            f->print("%s",text);
            if (len<=0 || text[len-1]!='\n')  f->print("\n");
        }
        else f->print("\n");
    }
    f->print("\n");
    return true;
}
bool Serializer::saveCustomFieldTypeHeader(const char* name, int numTextLines)
{
    // name
    f->print( "[%s",FieldTypeNames[ImGui::FT_CUSTOM]);
    if (numTextLines==0) numTextLines=1;
    if (numTextLines>1) f->print( "-%d",numTextLines);
    f->print( ":%s]\n",name);
    return true;
}

void StringSet(char *&destText, const char *text, bool allowNullDestText)
{
    if (destText) {ImGui::MemFree(destText);destText=NULL;}
    const char e = '\0';
    if (!text && !allowNullDestText) text=&e;
    if (text)
    {
        const int sz = strlen(text);
        destText = (char*) ImGui::MemAlloc(sz+1);strcpy(destText,text);
    }
}
void StringAppend(char *&destText, const char *textToAppend, bool allowNullDestText, bool prependLineFeedIfDestTextIsNotEmpty, bool mustAppendLineFeed)
{
    const int textToAppendSz = textToAppend ? strlen(textToAppend) : 0;
    if (textToAppendSz==0)
    {
        if (!destText && !allowNullDestText) {destText = (char*) ImGui::MemAlloc(1);strcpy(destText,"");}
        return;
    }
    const int destTextSz = destText ? strlen(destText) : 0;
    const bool mustPrependLF = prependLineFeedIfDestTextIsNotEmpty && (destTextSz>0);
    const bool mustAppendLF = mustAppendLineFeed;// && (destText);
    const int totalTextSz = textToAppendSz + destTextSz + (mustPrependLF?1:0) + (mustAppendLF?1:0);
    ImVector<char> totalText;totalText.resize(totalTextSz+1);
    totalText[0]='\0';
    if (destText)
    {
        strcpy(&totalText[0],destText);
        ImGui::MemFree(destText);destText=NULL;
    }
    if (mustPrependLF) strcat(&totalText[0],"\n");
    strcat(&totalText[0],textToAppend);
    if (mustAppendLF) strcat(&totalText[0],"\n");
    destText = (char*) ImGui::MemAlloc(totalTextSz+1);strcpy(destText,&totalText[0]);
}

int StringAppend(ImVector<char>& v,const char* fmt, ...)
{
    IM_ASSERT(v.size()>0 && v[v.size()-1]=='\0');
    va_list args,args2;

    va_start(args, fmt);
    va_copy(args2,args);                                    // since C99 (MANDATORY! otherwise we must reuse va_start(args2,fmt): slow)
    const int additionalSize = vsnprintf(NULL,0,fmt,args);  // since C99
    va_end(args);

    const int startSz = v.size();
    v.resize(startSz+additionalSize);
    const int rv = vsnprintf(&v[startSz-1],additionalSize,fmt,args2);
    va_end(args2);

    return rv;
}

// ImGui Theme Editor
static ImVec4 base = ImVec4(0.502f, 0.075f, 0.256f, 1.0f);
static ImVec4 bg   = ImVec4(0.200f, 0.220f, 0.270f, 1.0f);
static ImVec4 text = ImVec4(0.860f, 0.930f, 0.890f, 1.0f);
static float high_val = 0.8f;
static float mid_val = 0.5f;
static float low_val = 0.3f;
static float window_offset = -0.2f;
inline ImVec4 make_high(float alpha) 
{
    ImVec4 res(0, 0, 0, alpha);
    ImGui::ColorConvertRGBtoHSV(base.x, base.y, base.z, res.x, res.y, res.z);
    res.z = high_val;
    ImGui::ColorConvertHSVtoRGB(res.x, res.y, res.z, res.x, res.y, res.z);
    return res;
}

inline ImVec4 make_mid(float alpha) 
{
    ImVec4 res(0, 0, 0, alpha);
    ImGui::ColorConvertRGBtoHSV(base.x, base.y, base.z, res.x, res.y, res.z);
    res.z = mid_val;
    ImGui::ColorConvertHSVtoRGB(res.x, res.y, res.z, res.x, res.y, res.z);
    return res;
}

inline ImVec4 make_low(float alpha) 
{
    ImVec4 res(0, 0, 0, alpha);
    ImGui::ColorConvertRGBtoHSV(base.x, base.y, base.z, res.x, res.y, res.z);
    res.z = low_val;
    ImGui::ColorConvertHSVtoRGB(res.x, res.y, res.z, res.x, res.y, res.z);
    return res;
}

inline ImVec4 make_bg(float alpha, float offset = 0.f) 
{
    ImVec4 res(0, 0, 0, alpha);
    ImGui::ColorConvertRGBtoHSV(bg.x, bg.y, bg.z, res.x, res.y, res.z);
    res.z += offset;
    ImGui::ColorConvertHSVtoRGB(res.x, res.y, res.z, res.x, res.y, res.z);
    return res;
}

inline ImVec4 make_text(float alpha) 
{
    return ImVec4(text.x, text.y, text.z, alpha);
}

void ThemeGenerator(const char* name, bool* p_open, ImGuiWindowFlags flags) 
{
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin(name, p_open, flags);
    ImGui::ColorEdit3("base", (float*) &base, ImGuiColorEditFlags_PickerHueWheel);
    ImGui::ColorEdit3("bg", (float*) &bg, ImGuiColorEditFlags_PickerHueWheel);
    ImGui::ColorEdit3("text", (float*) &text, ImGuiColorEditFlags_PickerHueWheel);
    ImGui::SliderFloat("high", &high_val, 0, 1);
    ImGui::SliderFloat("mid", &mid_val, 0, 1);
    ImGui::SliderFloat("low", &low_val, 0, 1);
    ImGui::SliderFloat("window", &window_offset, -0.4f, 0.4f);

    ImGuiStyle &style = ImGui::GetStyle();

    style.Colors[ImGuiCol_Text]                  = make_text(0.78f);
    style.Colors[ImGuiCol_TextDisabled]          = make_text(0.28f);
    style.Colors[ImGuiCol_WindowBg]              = make_bg(1.00f, window_offset);
    style.Colors[ImGuiCol_ChildBg]               = make_bg(0.58f);
    style.Colors[ImGuiCol_PopupBg]               = make_bg(0.9f);
    style.Colors[ImGuiCol_Border]                = make_bg(0.6f, -0.05f);
    style.Colors[ImGuiCol_BorderShadow]          = make_bg(0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg]               = make_bg(1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = make_mid(0.78f);
    style.Colors[ImGuiCol_FrameBgActive]         = make_mid(1.00f);
    style.Colors[ImGuiCol_TitleBg]               = make_low(1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = make_high(1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = make_bg(0.75f);
    style.Colors[ImGuiCol_MenuBarBg]             = make_bg(0.47f);
    style.Colors[ImGuiCol_ScrollbarBg]           = make_bg(1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = make_low(1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = make_mid(0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = make_mid(1.00f);
    style.Colors[ImGuiCol_CheckMark]             = make_high(1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = make_bg(1.0f, .1f);
    style.Colors[ImGuiCol_SliderGrabActive]      = make_high(1.0f);
    style.Colors[ImGuiCol_Button]                = make_bg(1.0f, .2f);
    style.Colors[ImGuiCol_ButtonHovered]         = make_mid(1.00f);
    style.Colors[ImGuiCol_ButtonActive]          = make_high(1.00f);
    style.Colors[ImGuiCol_Header]                = make_mid(0.76f);
    style.Colors[ImGuiCol_HeaderHovered]         = make_mid(0.86f);
    style.Colors[ImGuiCol_HeaderActive]          = make_high(1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = make_mid(0.78f);
    style.Colors[ImGuiCol_ResizeGripActive]      = make_mid(1.00f);
    style.Colors[ImGuiCol_PlotLines]             = make_text(0.63f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = make_mid(1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = make_text(0.63f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = make_mid(1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = make_mid(0.43f);
    style.Colors[ImGuiCol_ModalWindowDimBg]      = make_bg(0.73f);
    style.Colors[ImGuiCol_Tab]                   = make_bg(0.40f);
    style.Colors[ImGuiCol_TabHovered]            = make_high(1.00f);
    style.Colors[ImGuiCol_TabActive]             = make_mid(1.00f);
    style.Colors[ImGuiCol_TabUnfocused]          = make_bg(0.40f);
    style.Colors[ImGuiCol_TabUnfocusedActive]    = make_bg(0.70f);
    //style.Colors[ImGuiCol_PlotLines]             = 
    //style.Colors[ImGuiCol_PlotLinesHovered]      = 
    //style.Colors[ImGuiCol_PlotHistogram]         = 
    //style.Colors[ImGuiCol_PlotHistogramHovered]  = 
    //style.Colors[ImGuiCol_TableHeaderBg]         = 
    //style.Colors[ImGuiCol_TableBorderStrong]     = 
    //style.Colors[ImGuiCol_TableBorderLight]      = 
    //style.Colors[ImGuiCol_TableRowBg]            = 
    //style.Colors[ImGuiCol_TableRowBgAlt]         = 
    //style.Colors[ImGuiCol_TextSelectedBg]        = 
    //style.Colors[ImGuiCol_DragDropTarget]        = 
    //style.Colors[ImGuiCol_NavHighlight]          = 
    //style.Colors[ImGuiCol_NavWindowingHighlight] =
    //style.Colors[ImGuiCol_NavWindowingDimBg]     = 
    //style.Colors[ImGuiCol_ModalWindowDimBg]      = 

    if (ImGui::Button("Export")) 
    {
        ImGui::LogToTTY();
        ImGui::LogText("ImVec4* colors = ImGui::GetStyle().Colors;\n");
        for (int i = 0; i < ImGuiCol_COUNT; i++) 
        {
            const ImVec4& col = style.Colors[i];
            const char* name = ImGui::GetStyleColorName(i);
            ImGui::LogText("colors[ImGuiCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, %.2ff);\n",
                            name, 23 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
        }
        ImGui::LogFinish();
    }
    ImGui::End();
}

// System Toolkit
bool file_exists(const std::string& path)
{
    if (path.empty())
        return false;

    return access(path.c_str(), R_OK) == 0;
}

bool is_file(const std::string& path)
{
    if (path.empty()) return false;
    bool isFile = false;
    auto fd = open(path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        struct stat st;
        if (fstat(fd, &st) == 0)
            isFile = S_ISREG(st.st_mode);
        close(fd);
    }
    return isFile;
}

bool is_folder(const std::string& path)
{
    if (path.empty()) return false;
    bool isDir = false;
    auto fd = open(path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        struct stat st;
        if (fstat(fd, &st) == 0)
            isDir = S_ISDIR(st.st_mode);
        close(fd);
    }
    return isDir;
}

bool is_link(const std::string& path)
{
    if (path.empty()) return false;
    bool isLink = false;
    auto fd = open(path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        struct stat st;
        if (fstat(fd, &st) == 0)
            isLink = S_ISLNK(st.st_mode);
        close(fd);
    }
    return isLink;
}

std::string path_url(const std::string& path)
{
    auto pos = path.find_last_of(PATH_SEP);
    if (pos != std::string::npos)
        return path.substr(0, pos + 1);
    return "";
}

std::string path_parent(const std::string& path)
{
    auto pos = path.find_last_of(PATH_SEP);
    if (pos != std::string::npos)
    {
        auto sub_path = path.substr(0, pos);
        pos = sub_path.find_last_of(PATH_SEP);
        if (pos != std::string::npos)
            return sub_path.substr(0, pos + 1);
    }
    return "";
}

std::string path_filename(const std::string& path)
{
    auto pos = path.find_last_of(PATH_SEP);
    if (pos != std::string::npos)
        return path.substr(pos + 1);
    return path;
}

std::string path_filename_prefix(const std::string& path)
{
    auto filename = path_filename(path);
    if (!filename.empty())
    {
        auto pos = filename.find_last_of('.');
        if (pos != std::string::npos)
            return filename.substr(0, pos);
    }
    return "";
}

std::string path_filename_suffix(const std::string& path)
{
    auto filename = path_filename(path);
    if (!filename.empty())
    {
        auto pos = filename.find_last_of('.');
        if (pos != std::string::npos)
            return filename.substr(pos);
    }
    return "";
}

std::string path_current(const std::string& path)
{
    auto pos = path.rfind(PATH_SEP);
    if (pos != std::string::npos) return path.substr(pos + 1, path.length() - pos - 1);
    return "";
}

std::string date_time_string()
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t t = std::chrono::system_clock::to_time_t(now);
    tm* datetime = localtime(&t);

    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << std::to_string(datetime->tm_year + 1900);
    oss << std::setw(2) << std::setfill('0') << std::to_string(datetime->tm_mon + 1);
    oss << std::setw(2) << std::setfill('0') << std::to_string(datetime->tm_mday );
    oss << std::setw(2) << std::setfill('0') << std::to_string(datetime->tm_hour );
    oss << std::setw(2) << std::setfill('0') << std::to_string(datetime->tm_min );
    oss << std::setw(2) << std::setfill('0') << std::to_string(datetime->tm_sec );
    oss << std::setw(3) << std::setfill('0') << std::to_string(millis);

    // fixed length string (17 chars) YYYYMMDDHHmmssiii
    return oss.str();
}

std::string username()
{
    std::string userName;
#ifdef _WIN32
    CHAR cUserNameBuffer[256];
    DWORD dwUserNameSize = 256;
    if(GetUserName(cUserNameBuffer, &dwUserNameSize))
    {
        userName = std::string(cUserNameBuffer);
    }
#else
    // try the system user info
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)
        userName = std::string(pwd->pw_name);
    else
        // try the $USER environment variable
        userName = std::string(getenv("USER"));
#endif
    return userName;
}

std::string home_path()
{
    std::string homePath;
#ifdef _WIN32
    std::string homeDrive = std::string(getenv("HOMEDRIVE"));
    homePath = homeDrive + std::string(getenv("HOMEPATH"));
#else
    // try the system user info
    // NB: avoids depending on changes of the $HOME env. variable
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)
        homePath = std::string(pwd->pw_dir);
    else
        // try the $HOME environment variable
        homePath = std::string(getenv("HOME"));
#endif
    return homePath + PATH_SEP;
}

bool create_directory(const std::string& path)
{
#ifdef _WIN32
    return !mkdir(path.c_str()) || errno == EEXIST;
#else
    return !mkdir(path.c_str(), 0755) || errno == EEXIST;
#endif
}

std::string settings_path(std::string app_name)
{
    // start from home folder
    // NB: use the env.variable $HOME to allow system to specify
    // another directory (e.g. inside a snap)
#ifdef _WIN32
    std::string homeDrive = std::string(getenv("HOMEDRIVE"));
    std::string home = homeDrive + std::string(getenv("HOMEPATH"));
#else
    std::string home(getenv("HOME"));
#endif
    // 2. try to access user settings folder
    std::string settingspath = home + PATH_SETTINGS;
    if (file_exists(settingspath))
    {
        // good, we have a place to put the settings file
        // settings should be in 'app_name' subfolder
        settingspath += app_name;

        // 3. create the vmix subfolder in settings folder if not existing already
        if ( !file_exists(settingspath))
        {
            if ( !create_directory(settingspath) )
                // fallback to home if settings path cannot be created
                settingspath = home;
        }
        return settingspath + PATH_SEP;
    }
    else
    {
        // fallback to home if settings path does not exists
        return home + PATH_SEP;
    }
}

std::string temp_path()
{
    std::string temp;
#ifdef _WIN32
    const char *tmpdir = getenv("TEMP");
#else
    const char *tmpdir = getenv("TMPDIR");
#endif
    if (tmpdir)
        temp = std::string(tmpdir);
    else
        temp = std::string( P_tmpdir );

    temp += PATH_SEP;
    return temp;
}

std::string cwd_path()
{
    char mCwdPath[PATH_MAX] = {0};

    if (getcwd(mCwdPath, sizeof(mCwdPath)) != NULL)
        return std::string(mCwdPath) + PATH_SEP;
    else
        return std::string();
}

std::string exec_path()
{
    std::string path = std::string(); 
    // Preallocate PATH_MAX (e.g., 4096) characters and hope the executable path isn't longer (including null byte)
    char exePath[PATH_MAX];
#if defined(__linux__)
    // Return written bytes, indicating if memory was sufficient
    int len = readlink("/proc/self/exe", exePath, PATH_MAX);
    if (len <= 0 || len == PATH_MAX) // memory not sufficient or general error occured
        return path;
    path = path_url(std::string(exePath));
#elif defined(_WIN32)
    // Return written bytes, indicating if memory was sufficient
    unsigned int len = GetModuleFileNameA(GetModuleHandleA(0x0), exePath, MAX_PATH);
    if (len == 0) // memory not sufficient or general error occured
        return path;
    path = path_url(std::string(exePath));
#elif defined(__APPLE__)
    unsigned int len = (unsigned int)PATH_MAX;
    // Obtain executable path to canonical path, return zero on success
    if (_NSGetExecutablePath(exePath, &len) == 0)
    {
        // Convert executable path to canonical path, return null pointer on error
        char * realPath = realpath(exePath, 0x0);
        if (realPath == 0x0)
            return path;
        path = path_url(std::string(realPath));
        free(realPath);
    }
    else // len is initialized with the required number of bytes (including zero byte)
    {
        char * intermediatePath = (char *)malloc(sizeof(char) * len);
        // Convert executable path to canonical path, return null pointer on error
        if (_NSGetExecutablePath(intermediatePath, &len) != 0)
        {
            free(intermediatePath);
            return path;
        }
        char * realPath = realpath(intermediatePath, 0x0);
        free(intermediatePath);
        // Check if conversion to canonical path succeeded
        if (realPath == 0x0)
            return path;
        path = path_url(std::string(realPath));
        free(realPath);
    }
#endif
    return path;
}

void execute(const std::string& command)
{
    int ignored __attribute__((unused));
#ifdef _WIN32
    ShellExecuteA( nullptr, nullptr, command.c_str(), nullptr, nullptr, 0 );
#elif defined(__APPLE__)
    (void) system( command.c_str() );
#else
    ignored = system( command.c_str() );
#endif
}

size_t memory_usage()
{
#if defined(__linux__)
    // Grabbing info directly from the /proc pseudo-filesystem.  Reading from
    // /proc/self/statm gives info on your own process, as one line of
    // numbers that are: virtual mem program size, resident set size,
    // shared pages, text/code, data/stack, library, dirty pages.  The
    // mem sizes should all be multiplied by the page size.
    size_t size = 0;
    FILE *file = fopen("/proc/self/statm", "r");
    if (file)
    {
        unsigned long m = 0;
        int ret = 0, ret2 = 0;
        ret = fscanf (file, "%lu", &m);  // virtual mem program size,
        ret2 = fscanf (file, "%lu", &m);  // resident set size,
        fclose (file);
        if (ret>0 && ret2>0)
            size = (size_t)m * getpagesize();
    }
    return size;

#elif defined(__APPLE__)
    // Inspired from
    // http://miknight.blogspot.com/2005/11/resident-set-size-in-mac-os-x.html
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    task_info(current_task(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
    return t_info.resident_size;

#elif defined(_WIN32)
    // According to MSDN...
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo (GetCurrentProcess(), &counters, sizeof (counters)))
        return counters.PagefileUsage;
    else return 0;

#else
    return 0;
#endif
}

size_t memory_max_usage()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo (GetCurrentProcess(), &counters, sizeof (counters)))
        return counters.PeakPagefileUsage;
    else return 0;
#else
    struct rusage r_usage;
    getrusage(RUSAGE_SELF,&r_usage);
    return 1024 * r_usage.ru_maxrss;
#endif
}

std::string MillisecToString(int64_t millisec, int show_millisec)
{
    std::ostringstream oss;
    if (millisec < 0)
    {
        oss << "-";
        millisec = -millisec;
    }
    uint64_t t = (uint64_t) millisec;
    uint32_t milli = (uint32_t)(t%1000); t /= 1000;
    uint32_t sec = (uint32_t)(t%60); t /= 60;
    uint32_t min = (uint32_t)(t%60); t /= 60;
    uint32_t hour = (uint32_t)t;
    if (hour > 0)
    {
        oss << std::setfill('0') << std::setw(2) << hour << ':'
            << std::setw(2) << min << ':'
            << std::setw(2) << sec;
        if (show_millisec == 3)
            oss << '.' << std::setw(3) << milli;
        else if (show_millisec == 2)
            oss << '.' << std::setw(2) << milli / 10;
        else if (show_millisec == 1)
            oss << '.' << std::setw(1) << milli / 100;
    }
    else
    {
        oss << std::setfill('0') << std::setw(2) << min << ':'
            << std::setw(2) << sec;
        if (show_millisec == 3)
            oss << '.' << std::setw(3) << milli;
        else if (show_millisec == 2)
            oss << '.' << std::setw(2) << milli / 10;
        else if (show_millisec == 1)
            oss << '.' << std::setw(1) << milli / 100;
    }
    return oss.str();
}

#if !defined(__EMSCRIPTEN__)
void ImDecryptFile(const std::string path, const std::string key, std::vector<uint8_t>& data)
{
    data.clear();
    struct stat sb_enc;
    const uint8_t *memblock_enc;
    int fd_enc = open(path.c_str(), O_RDONLY);
    if (fd_enc == -1)
    {
        std::cout << "out file can't open data file" << path << std::endl;
        return;
    }
    fstat(fd_enc, &sb_enc);
    memblock_enc = (const uint8_t *)mmap(NULL, sb_enc.st_size, PROT_READ, MAP_PRIVATE, fd_enc, 0);
    close(fd_enc);
    if (memblock_enc == MAP_FAILED)
    {
        std::cout << "out file can't map data file" << path << std::endl;
        return;
    }
    auto& instance = ImGuiHelper::Encrypt::Instance();
    auto check_len = instance.decrypt(memblock_enc, data, sb_enc.st_size, (uint8_t*)key.c_str());
    std::cout << "decrypt length:" << check_len << std::endl;
    munmap((void *)memblock_enc, sb_enc.st_size);
}
#endif
} //namespace ImGuiHelper

namespace base64 
{
// Decoder here
extern "C"
{
typedef enum {step_a, step_b, step_c, step_d} base64_decodestep;
typedef struct {base64_decodestep step;char plainchar;} base64_decodestate;

inline int base64_decode_value(char value_in)
{
	static const char decoding[] = {62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
	static const char decoding_size = sizeof(decoding);
	value_in -= 43;
	if (value_in < 0 || value_in > decoding_size) return -1;
	return decoding[(int)value_in];
}
inline void base64_init_decodestate(base64_decodestate* state_in)
{
	state_in->step = step_a;
	state_in->plainchar = 0;
}
inline int base64_decode_block(const char* code_in, const int length_in, char* plaintext_out, base64_decodestate* state_in)
{
	const char* codechar = code_in;
	char* plainchar = plaintext_out;
	char fragment;
	
	*plainchar = state_in->plainchar;
	
	switch (state_in->step)
	{
		while (1)
		{
        case step_a:
			do
            {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_a;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar    = (fragment & 0x03f) << 2;
        case step_b:
			do
            {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_b;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x030) >> 4;
			*plainchar    = (fragment & 0x00f) << 4;
        case step_c:
			do
            {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_c;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x03c) >> 2;
			*plainchar    = (fragment & 0x003) << 6;
        case step_d:
			do
            {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_d;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++   |= (fragment & 0x03f);
		}
	}
	/* control should not reach here */
	return plainchar - plaintext_out;
}
}	// extern "C"
struct decoder
{
	base64_decodestate _state;
	int _buffersize;
	
	decoder(int buffersize_in = 4096) : _buffersize(buffersize_in) {}
	int decode(char value_in) {return base64_decode_value(value_in);}
	int decode(const char* code_in, const int length_in, char* plaintext_out)	{return base64_decode_block(code_in, length_in, plaintext_out, &_state);}
};

// Encoder Here
extern "C"
{
typedef enum {step_A, step_B, step_C} base64_encodestep;
typedef struct {base64_encodestep step;char result;int stepcount;} base64_encodestate;

const int CHARS_PER_LINE = 2147483647;//72; // This was hard coded to 72 originally. But here we add '\n' at a later step. So we use MAX_INT here.
inline void base64_init_encodestate(base64_encodestate* state_in)
{
	state_in->step = step_A;
	state_in->result = 0;
	state_in->stepcount = 0;
}
inline char base64_encode_value(char value_in)
{
	static const char* encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	if (value_in > 63) return '=';
	return encoding[(int)value_in];
}
inline int base64_encode_block(const char* plaintext_in, int length_in, char* code_out, base64_encodestate* state_in)
{
	const char* plainchar = plaintext_in;
	const char* const plaintextend = plaintext_in + length_in;
	char* codechar = code_out;
    char result = '\0';
    char fragment = '\0';
	
	result = state_in->result;
	
	switch (state_in->step)
	{
		while (1)
		{
        case step_A:
			if (plainchar == plaintextend)
			{
				state_in->result = result;
				state_in->step = step_A;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			result = (fragment & 0x0fc) >> 2;
			*codechar++ = base64_encode_value(result);
			result = (fragment & 0x003) << 4;
        case step_B:
			if (plainchar == plaintextend)
			{
				state_in->result = result;
				state_in->step = step_B;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			result |= (fragment & 0x0f0) >> 4;
			*codechar++ = base64_encode_value(result);
			result = (fragment & 0x00f) << 2;
        case step_C:
			if (plainchar == plaintextend)
			{
				state_in->result = result;
				state_in->step = step_C;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			result |= (fragment & 0x0c0) >> 6;
			*codechar++ = base64_encode_value(result);
			result  = (fragment & 0x03f) >> 0;
			*codechar++ = base64_encode_value(result);
			
			++(state_in->stepcount);
			if (state_in->stepcount == CHARS_PER_LINE/4)
			{
				*codechar++ = '\n';
				state_in->stepcount = 0;
			}
		}
	}
	/* control should not reach here */
	return codechar - code_out;
}

inline int base64_encode_blockend(char* code_out, base64_encodestate* state_in)
{
	char* codechar = code_out;
	
	switch (state_in->step)
	{
	case step_B:
		*codechar++ = base64_encode_value(state_in->result);
		*codechar++ = '=';
		*codechar++ = '=';
		break;
	case step_C:
		*codechar++ = base64_encode_value(state_in->result);
		*codechar++ = '=';
		break;
	case step_A:
		break;
	}
	*codechar++ = '\n';
	
	return codechar - code_out;
}	
} // extern "C"
struct encoder
{
	base64_encodestate _state;
	int _buffersize;
	
	encoder(int buffersize_in = 4096)
	: _buffersize(buffersize_in)
	{}
	int encode(char value_in)
	{
		return base64_encode_value(value_in);
	}
	int encode(const char* code_in, const int length_in, char* plaintext_out)
	{
		return base64_encode_block(code_in, length_in, plaintext_out, &_state);
	}
	int encode_end(char* plaintext_out)
	{
		return base64_encode_blockend(plaintext_out, &_state);
	}
};
} // namespace base64

namespace ImGui
{
namespace Stringifier
{
template <typename VectorChar> static bool Base64Decode(const char* input,VectorChar& output)
{
    output.clear();if (!input) return false;
    const int N = 4096;
    base64::decoder d(N);
    base64_init_decodestate(&d._state);

    int outputStart=0,outputlength = 0;
    int codelength = strlen(input);
    const char* pIn = input;
    int stepCodeLength = 0;
    do
    {
        output.resize(outputStart+N);
        stepCodeLength = codelength>=N?N:codelength;
        outputlength = d.decode(pIn, stepCodeLength, &output[outputStart]);
        outputStart+=outputlength;
        pIn+=stepCodeLength;
        codelength-=stepCodeLength;
    }
    while (codelength>0);

    output.resize(outputStart);
    //
    base64_init_decodestate(&d._state);
    return true;
}

template <typename VectorChar> static bool Base64Encode(const char* input,int inputSize,VectorChar& output)
{
	output.clear();if (!input || inputSize==0) return false;

    const int N=4096;
    base64::encoder e(N);
    base64_init_encodestate(&e._state);

    int outputStart=0,outputlength = 0;
    int codelength = inputSize;
    const char* pIn = input;
    int stepCodeLength = 0;

    do
    {
        output.resize(outputStart+2*N);
        stepCodeLength = codelength>=N?N:codelength;
        outputlength = e.encode(pIn, stepCodeLength,&output[outputStart]);
        outputStart+=outputlength;
        pIn+=stepCodeLength;
        codelength-=stepCodeLength;
    }
    while (codelength>0);

    output.resize(outputStart+2*N);
    outputlength = e.encode_end(&output[outputStart]);
    outputStart+=outputlength;
    output.resize(outputStart);
    //
    base64_init_encodestate(&e._state);

	return true;
}
inline static unsigned int Decode85Byte(char c)   { return c >= '\\' ? c-36 : c-35; }
static void Decode85(const unsigned char* src, unsigned char* dst)
{
    while (*src)
    {
        unsigned int tmp = Decode85Byte(src[0]) + 85*(Decode85Byte(src[1]) + 85*(Decode85Byte(src[2]) + 85*(Decode85Byte(src[3]) + 85*Decode85Byte(src[4]))));
        dst[0] = ((tmp >> 0) & 0xFF); dst[1] = ((tmp >> 8) & 0xFF); dst[2] = ((tmp >> 16) & 0xFF); dst[3] = ((tmp >> 24) & 0xFF);   // We can't assume little-endianness.
        src += 5;
        dst += 4;
    }
}
template <typename VectorChar> static bool Base85Decode(const char* input,VectorChar& output)
{
	output.clear();if (!input) return false;
	const int outputSize = (((int)strlen(input) + 4) / 5) * 4;
	output.resize(outputSize);
    Decode85((const unsigned char*)input,(unsigned char*)&output[0]);
    return true;
}

inline static char Encode85Byte(unsigned int x)
{
    x = (x % 85) + 35;
    return (x>='\\') ? x+1 : x;
}
template <typename VectorChar> static bool Base85Encode(const char* input,int inputSize,VectorChar& output,bool outputStringifiedMode,int numCharsPerLineInStringifiedMode=112)	
{
    // Adapted from binary_to_compressed_c(...) inside imgui_draw.cpp
    output.clear();if (!input || inputSize==0) return false;
    output.reserve((int)((float)inputSize*1.3f));
    if (numCharsPerLineInStringifiedMode<=12) numCharsPerLineInStringifiedMode = 12;
    if (outputStringifiedMode) output.push_back('"');
    char prev_c = 0;int cnt=0;
    for (int src_i = 0; src_i < inputSize; src_i += 4)
    {
        unsigned int d = *(unsigned int*)(input + src_i);
        for (unsigned int n5 = 0; n5 < 5; n5++, d /= 85)
        {
            char c = Encode85Byte(d);
            if (outputStringifiedMode && c == '?' && prev_c == '?') output.push_back('\\');	// This is made a little more complicated by the fact that ??X sequences are interpreted as trigraphs by old C/C++ compilers. So we need to escape pairs of ??.
            output.push_back(c);
            prev_c = c;
        }
        cnt+=4;
        if (outputStringifiedMode && cnt>=numCharsPerLineInStringifiedMode)
        {
            output.push_back('"');
            output.push_back('	');
            output.push_back('\\');
            //output.push_back(' ');
            output.push_back('\n');
            output.push_back('"');
            cnt=0;
        }
    }
    // End
    if (outputStringifiedMode)
    {
        output.push_back('"');
        output.push_back(';');
        output.push_back('\n');
        output.push_back('\n');
    }
    output.push_back('\0');	// End character

    return true;
}
} // namespace Stringifier

bool Base64Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)
{
    if (!stringifiedMode) return Stringifier::Base64Encode<ImVector<char> >(input,inputSize,output);
    else
    {
        ImVector<char> output1;
        if (!Stringifier::Base64Encode<ImVector<char> >(input,inputSize,output1)) {output.clear();return false;}
        if (output1.size()==0) {output.clear();return false;}
        if (!ImGui::TextStringify(&output1[0],output,numCharsPerLineInStringifiedMode,output1.size()-1)) {output.clear();return false;}
    }
    return true;
}

bool Base64Decode(const char* input,ImVector<char>& output)
{
    return Stringifier::Base64Decode<ImVector<char> >(input,output);
}

bool Base85Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)
{
    return Stringifier::Base85Encode<ImVector<char> >(input,inputSize,output,stringifiedMode,numCharsPerLineInStringifiedMode);
}

bool Base85Decode(const char* input,ImVector<char>& output)
{
    return Stringifier::Base85Decode<ImVector<char> >(input,output);
}

bool BinaryStringify(const char* input, int inputSize, ImVector<char>& output, int numInputBytesPerLineInStringifiedMode,bool serializeUnsignedBytes)
{
    output.clear();
    if (!input || inputSize<=0) return false;
    ImGuiTextBuffer b;
    b.clear();
    b.Buf.reserve(inputSize*7.5f);
    // -----------------------------------------------------------
    if (serializeUnsignedBytes)
    {
        b.appendf("{%d",(int) (*((unsigned char*) &input[0])));  // start byte
        int cnt=1;
        for (int i=1;i<inputSize;i++)
        {
            if (cnt++>=numInputBytesPerLineInStringifiedMode) {cnt=0;b.appendf("\n");}
            b.appendf(",%d",(int) (*((unsigned char*) &input[i])));
        }
    }
    else
    {
        b.appendf("{%d",(int)input[0]);  // start byte
        int cnt=1;
        for (int i=1;i<inputSize;i++)
        {
            if (cnt++>=numInputBytesPerLineInStringifiedMode) {cnt=0;b.appendf("\n");}
            b.appendf(",%d",(int)input[i]);
        }
    }
    b.appendf("};\n");
    //-------------------------------------------------------------
    b.Buf.swap(output);
    return true;
}

bool TextStringify(const char* input, ImVector<char>& output, int numCharsPerLineInStringifiedMode, int inputSize, bool noBackslashAtLineEnds)
{
    output.clear();if (!input) return false;
    if (inputSize<=0) inputSize=strlen(input);
    output.reserve(inputSize*1.25f);
    // --------------------------------------------------------------
    output.push_back('"');
    char c='\n';int cnt=0;bool endFile = false;
    for (int i=0;i<inputSize;i++)
    {
        c = input[i];
        switch (c)
        {
        case '\\':
            output.push_back('\\');
            output.push_back('\\');
            break;
        case '"':
            output.push_back('\\');
            output.push_back('"');
            break;
        case '\r':
        case '\n':
            //---------------------
            output.push_back('\\');
            output.push_back(c=='\n' ? 'n' : 'r');
            if (numCharsPerLineInStringifiedMode<=0)
            {
                // Break at newline to ease reading:
                output.push_back('"');                
                if (i==inputSize-1)
                {
                    endFile = true;
                    if (!noBackslashAtLineEnds) output.push_back(';');
                    output.push_back('\n');
                }
                else
                {
                    if (!noBackslashAtLineEnds)
                    {
                        output.push_back('\t');
                        output.push_back('\\');
                    }
                    output.push_back('\n');
                    output.push_back('"');
                }
                cnt = 0;
                //--------------------
            }
            //--------------------
            break;
        default:
            output.push_back(c);
            if (++cnt>=numCharsPerLineInStringifiedMode && numCharsPerLineInStringifiedMode>0)
            {
                //---------------------
                //output.push_back('\\');
                //output.push_back('n');
                output.push_back('"');

                if (i==inputSize-1)
                {
                    endFile = true;
                    if (!noBackslashAtLineEnds) output.push_back(';');
                    output.push_back('\n');
                }
                else
                {
                    if (!noBackslashAtLineEnds)
                    {
                        output.push_back('\t');
                        output.push_back('\\');
                    }
                    output.push_back('\n');
                    output.push_back('"');
                }
                cnt = 0;
                //--------------------
            }
            break;
        }
    }

    if (!endFile)
    {
        output.push_back('"');
        if (!noBackslashAtLineEnds) output.push_back(';');
        output.push_back('\n');
        //--------------------
    }

    output.push_back('\0');	// End character
    //-------------------------------------------------------------
    return true;
}
} // namespace ImGui

#ifdef IMGUI_USE_ZLIB	// requires linking to library -lZlib
#include <zlib.h>
namespace ImGui
{
bool GzDecompressFromFile(const char* filePath,ImVector<char>& rv,bool clearRvBeforeUsage)
{
    if (clearRvBeforeUsage) rv.clear();
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"rb",false)) return false;
    //----------------------------------------------------
    return GzDecompressFromMemory(&f_data[0],f_data.size(),rv,clearRvBeforeUsage);
    //----------------------------------------------------
}
bool GzBase64DecompressFromFile(const char* filePath,ImVector<char>& rv)
{
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::GzBase64DecompressFromMemory(&f_data[0],rv);
}
bool GzBase85DecompressFromFile(const char* filePath,ImVector<char>& rv)
{
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::GzBase85DecompressFromMemory(&f_data[0],rv);
}

bool GzDecompressFromMemory(const char* memoryBuffer,int memoryBufferSize,ImVector<char>& rv,bool clearRvBeforeUsage)
{
    if (clearRvBeforeUsage) rv.clear();
    const int startRv = rv.size();

    if (memoryBufferSize == 0  || !memoryBuffer) return false;
    const int memoryChunk = memoryBufferSize > (16*1024) ? (16*1024) : memoryBufferSize;
    rv.resize(startRv+memoryChunk);  // we start using the memoryChunk length

    z_stream myZStream;
    myZStream.next_in = (Bytef *) memoryBuffer;
    myZStream.avail_in = memoryBufferSize;
    myZStream.total_out = 0;
    myZStream.zalloc = Z_NULL;
    myZStream.zfree = Z_NULL;

    bool done = false;
    if (inflateInit2(&myZStream, (16+MAX_WBITS)) == Z_OK)
    {
        int err = Z_OK;
        while (!done)
        {
            if (myZStream.total_out >= (uLong)(rv.size()-startRv)) rv.resize(rv.size()+memoryChunk);    // not enough space: we add the memoryChunk each step

            myZStream.next_out = (Bytef *) (&rv[startRv] + myZStream.total_out);
            myZStream.avail_out = rv.size() - startRv - myZStream.total_out;

            if ((err = inflate (&myZStream, Z_SYNC_FLUSH))==Z_STREAM_END) done = true;
            else if (err != Z_OK)  break;
        }
        if ((err=inflateEnd(&myZStream))!= Z_OK) done = false;
    }
    rv.resize(startRv+(done ? myZStream.total_out : 0));

    return done;
}
bool GzCompressFromMemory(const char* memoryBuffer,int memoryBufferSize,ImVector<char>& rv,bool clearRvBeforeUsage) 
{
    if (clearRvBeforeUsage) rv.clear();
    const int startRv = rv.size();

    if (memoryBufferSize == 0  || !memoryBuffer) return false;
    const int memoryChunk = memoryBufferSize/3 > (16*1024) ? (16*1024) : memoryBufferSize/3;
    rv.resize(startRv+memoryChunk);  // we start using the memoryChunk length

    z_stream myZStream;
    myZStream.next_in =  (Bytef *) memoryBuffer;
    myZStream.avail_in = memoryBufferSize;
    myZStream.total_out = 0;
    myZStream.zalloc = Z_NULL;
    myZStream.zfree = Z_NULL;

    bool done = false;
    if (deflateInit2(&myZStream,Z_BEST_COMPRESSION,Z_DEFLATED,(16+MAX_WBITS),8,Z_DEFAULT_STRATEGY) == Z_OK)
    {
        int err = Z_OK;
        while (!done)
        {
            if (myZStream.total_out >= (uLong)(rv.size()-startRv)) rv.resize(rv.size()+memoryChunk);    // not enough space: we add the full memoryChunk each step

            myZStream.next_out = (Bytef *) (&rv[startRv] + myZStream.total_out);
            myZStream.avail_out = rv.size() - startRv - myZStream.total_out;

            if ((err = deflate (&myZStream, Z_FINISH))==Z_STREAM_END) done = true;
            else if (err != Z_OK)  break;
        }
        if ((err=deflateEnd(&myZStream))!= Z_OK) done=false;
    }
    rv.resize(startRv+(done ? myZStream.total_out : 0));

    return done;
}

bool GzBase64DecompressFromMemory(const char* input,ImVector<char>& rv)
{
    rv.clear();ImVector<char> v;
    if (ImGui::Base64Decode(input,v)) return false;
    if (v.size()==0) return false;
    return GzDecompressFromMemory(&v[0],v.size(),rv);
}
bool GzBase85DecompressFromMemory(const char* input,ImVector<char>& rv)
{
    rv.clear();ImVector<char> v;
    if (ImGui::Base85Decode(input,v)) return false;
    if (v.size()==0) return false;
    return GzDecompressFromMemory(&v[0],v.size(),rv);
}
bool GzBase64CompressFromMemory(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)
{
    output.clear();ImVector<char> output1;
    if (!ImGui::GzCompressFromMemory(input,inputSize,output1)) return false;
    return ImGui::Base64Encode(&output1[0],output1.size(),output,stringifiedMode,numCharsPerLineInStringifiedMode);
}
bool GzBase85CompressFromMemory(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)
{
    output.clear();ImVector<char> output1;
    if (!ImGui::GzCompressFromMemory(input,inputSize,output1)) return false;
    return ImGui::Base85Encode(&output1[0],output1.size(),output,stringifiedMode,numCharsPerLineInStringifiedMode);
}
} // namespace ImGui
#endif //IMGUI_USE_ZLIB

namespace ImGui
{
// Two methods that fill rv and return true on success
bool Base64DecodeFromFile(const char* filePath,ImVector<char>& rv)
{
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::Base64Decode(&f_data[0],rv);
}
bool Base85DecodeFromFile(const char* filePath,ImVector<char>& rv)
{
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::Base85Decode(&f_data[0],rv);
}
} // namespace ImGui

namespace ImGui
{
// Generate color
void RandomColor(ImVec4& color, float alpha)
{
    alpha = ImClamp(alpha, 0.0f, 1.0f);
    int r = std::rand() % 255;
    int g = std::rand() % 255;
    int b = std::rand() % 255;
    color = ImVec4(r / 255.0, g / 255.0, b / 255.0, alpha);
}

void RandomColor(ImU32& color, float alpha)
{
    alpha = ImClamp(alpha, 0.0f, 1.0f);
    int r = std::rand() % 255;
    int g = std::rand() % 255;
    int b = std::rand() % 255;
    color = IM_COL32(r, g, b, (int)(alpha * 255.f));
}
} // namespace ImGui

// platform folders
namespace ImGuiHelper
{
// https://github.com/sago007/PlatformFolders
#ifdef _WIN32
class FreeCoTaskMemory {
	LPWSTR pointer = NULL;
public:
	explicit FreeCoTaskMemory(LPWSTR pointer) : pointer(pointer) {};
	~FreeCoTaskMemory() {
		CoTaskMemFree(pointer);
	}
};
static std::string win32_utf16_to_utf8(const wchar_t* wstr) {
	std::string res;
	// If the 6th parameter is 0 then WideCharToMultiByte returns the number of bytes needed to store the result.
	int actualSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
	if (actualSize > 0) {
		//If the converted UTF-8 string could not be in the initial buffer. Allocate one that can hold it.
		std::vector<char> buffer(actualSize);
		actualSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &buffer[0], static_cast<int>(buffer.size()), nullptr, nullptr);
		res = buffer.data();
	}
	if (actualSize == 0) {
		// WideCharToMultiByte return 0 for errors.
		throw std::runtime_error("UTF16 to UTF8 failed with error code: " + std::to_string(GetLastError()));
	}
	return res;
}
static std::string GetKnownWindowsFolder(REFKNOWNFOLDERID folderId, const char* errorMsg) {
	LPWSTR wszPath = NULL;
	HRESULT hr;
	hr = SHGetKnownFolderPath(folderId, KF_FLAG_CREATE, NULL, &wszPath);
	FreeCoTaskMemory scopeBoundMemory(wszPath);

	if (!SUCCEEDED(hr)) {
		throw std::runtime_error(errorMsg);
	}
	return win32_utf16_to_utf8(wszPath);
}
static std::string GetAppData() {
	return GetKnownWindowsFolder(FOLDERID_RoamingAppData, "RoamingAppData could not be found");
}
static std::string GetAppDataCommon() {
	return GetKnownWindowsFolder(FOLDERID_ProgramData, "ProgramData could not be found");
}

static std::string GetAppDataLocal() {
	return GetKnownWindowsFolder(FOLDERID_LocalAppData, "LocalAppData could not be found");
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
static void PlatformFoldersAddFromFile(const std::string& filename, std::map<std::string, std::string>& folders) {
	std::ifstream infile(filename.c_str());
	std::string line;
	while (std::getline(infile, line)) {
		if (line.length() == 0 || line.at(0) == '#' || line.substr(0, 4) != "XDG_" || line.find("_DIR") == std::string::npos) {
			continue;
		}
		try {
			std::size_t splitPos = line.find('=');
			std::string key = line.substr(0, splitPos);
			std::size_t valueStart = line.find('"', splitPos);
			std::size_t valueEnd = line.find('"', valueStart+1);
			std::string value = line.substr(valueStart+1, valueEnd - valueStart - 1);
			folders[key] = value;
		}
		catch (std::exception&  e) {
			std::cerr << "WARNING: Failed to process \"" << line << "\" from \"" << filename << "\". Error: "<< e.what() << "\n";
			continue;
		}
	}
}

static void PlatformFoldersFillData(std::map<std::string, std::string>& folders) {
	folders["XDG_DOCUMENTS_DIR"] = "$HOME/Documents";
	folders["XDG_DESKTOP_DIR"] = "$HOME/Desktop";
	folders["XDG_DOWNLOAD_DIR"] = "$HOME/Downloads";
	folders["XDG_MUSIC_DIR"] = "$HOME/Music";
	folders["XDG_PICTURES_DIR"] = "$HOME/Pictures";
	folders["XDG_PUBLICSHARE_DIR"] = "$HOME/Public";
	folders["XDG_TEMPLATES_DIR"] = "$HOME/.Templates";
	folders["XDG_VIDEOS_DIR"] = "$HOME/Videos";
	PlatformFoldersAddFromFile( ImGuiHelper::getConfigHome()+"/user-dirs.dirs", folders);
	for (std::map<std::string, std::string>::iterator itr = folders.begin() ; itr != folders.end() ; ++itr ) {
		std::string& value = itr->second;
		if (value.compare(0, 5, "$HOME") == 0) {
			value = ImGuiHelper::home_path() + value.substr(6, std::string::npos);
		}
	}
}

static void throwOnRelative(const char* envName, const char* envValue) {
	if (envValue[0] != '/') {
		char buffer[200];
		std::snprintf(buffer, sizeof(buffer), "Environment \"%s\" does not start with an '/'. XDG specifies that the value must be absolute. The current value is: \"%s\"", envName, envValue);
		throw std::runtime_error(buffer);
	}
}

static std::string getLinuxFolderDefault(const char* envName, const char* defaultRelativePath) {
	std::string res;
	const char* tempRes = std::getenv(envName);
	if (tempRes) {
		throwOnRelative(envName, tempRes);
		res = tempRes;
		return res;
	}
	res = ImGuiHelper::home_path() + defaultRelativePath;
	return res;
}

#endif

std::string getDataHome() {
#ifdef _WIN32
	return GetAppData();
#elif defined(__APPLE__)
	return home_path()+"Library/Application Support";
#else
	return getLinuxFolderDefault("XDG_DATA_HOME", ".local/share");
#endif
}

std::string getConfigHome() {
#ifdef _WIN32
	return GetAppData();
#elif defined(__APPLE__)
	return home_path()+"Library/Application Support";
#else
	return getLinuxFolderDefault("XDG_CONFIG_HOME", ".config");
#endif
}

std::string getCacheDir() {
#ifdef _WIN32
	return GetAppDataLocal();
#elif defined(__APPLE__)
	return home_path()+"Library/Caches";
#else
	return getLinuxFolderDefault("XDG_CACHE_HOME", ".cache");
#endif
}

std::string getStateDir() {
#ifdef _WIN32
	return GetAppDataLocal();
#elif defined(__APPLE__)
	return home_path()+"Library/Application Support";
#else
	return getLinuxFolderDefault("XDG_STATE_HOME", ".local/state");
#endif
}

std::string getDocumentsFolder() {
#ifdef _WIN32
	return GetKnownWindowsFolder(FOLDERID_Documents, "Failed to find My Documents folder");
#elif defined(__APPLE__)
	return home_path()+"Documents";
#else
    std::map<std::string, std::string> folders;
    PlatformFoldersFillData(folders);
	return folders["XDG_DOCUMENTS_DIR"];
#endif
}

std::string getDesktopFolder() {
#ifdef _WIN32
	return GetKnownWindowsFolder(FOLDERID_Desktop, "Failed to find Desktop folder");
#elif defined(__APPLE__)
	return home_path()+"Desktop";
#else
    std::map<std::string, std::string> folders;
    PlatformFoldersFillData(folders);
	return folders["XDG_DESKTOP_DIR"];
#endif
}

std::string getPicturesFolder() {
#ifdef _WIN32
	return GetKnownWindowsFolder(FOLDERID_Pictures, "Failed to find My Pictures folder");
#elif defined(__APPLE__)
	return home_path()+"Pictures";
#else
    std::map<std::string, std::string> folders;
    PlatformFoldersFillData(folders);
	return folders["XDG_PICTURES_DIR"];
#endif
}

std::string getPublicFolder() {
#ifdef _WIN32
	return GetKnownWindowsFolder(FOLDERID_Public, "Failed to find the Public folder");
#elif defined(__APPLE__)
	return home_path()+"Public";
#else
    std::map<std::string, std::string> folders;
    PlatformFoldersFillData(folders);
	return folders["XDG_PUBLICSHARE_DIR"];
#endif
}

std::string getDownloadFolder() {
#ifdef _WIN32
	return GetKnownWindowsFolder(FOLDERID_Downloads, "Failed to find My Downloads folder");
#elif defined(__APPLE__)
	return home_path()+"Downloads";
#else
    std::map<std::string, std::string> folders;
    PlatformFoldersFillData(folders);
	return folders["XDG_DOWNLOAD_DIR"];
#endif
}

std::string getMusicFolder() {
#ifdef _WIN32
	return GetKnownWindowsFolder(FOLDERID_Music, "Failed to find My Music folder");
#elif defined(__APPLE__)
	return home_path()+"Music";
#else
    std::map<std::string, std::string> folders;
    PlatformFoldersFillData(folders);
	return folders["XDG_MUSIC_DIR"];
#endif
}

std::string getVideoFolder() {
#ifdef _WIN32
	return GetKnownWindowsFolder(FOLDERID_Videos, "Failed to find My Video folder");
#elif defined(__APPLE__)
	return home_path()+"Movies";
#else
    std::map<std::string, std::string> folders;
    PlatformFoldersFillData(folders);
	return folders["XDG_VIDEOS_DIR"];
#endif
}
} // namespace ImGuiHelper

//-----------------------------------------------------------------------------
// ImVec/ImMat fuctions
void ImVec4::Transform(const ImMat4x4& matrix)
{
    ImVec4 out;

    out.x = x * matrix.m[0][0] + y * matrix.m[1][0] + z * matrix.m[2][0] + w * matrix.m[3][0];
    out.y = x * matrix.m[0][1] + y * matrix.m[1][1] + z * matrix.m[2][1] + w * matrix.m[3][1];
    out.z = x * matrix.m[0][2] + y * matrix.m[1][2] + z * matrix.m[2][2] + w * matrix.m[3][2];
    out.w = x * matrix.m[0][3] + y * matrix.m[1][3] + z * matrix.m[2][3] + w * matrix.m[3][3];

    x = out.x;
    y = out.y;
    z = out.z;
    w = out.w;
}

void ImVec4::TransformPoint(const ImMat4x4& matrix)
{
    ImVec4 out;

    out.x = x * matrix.m[0][0] + y * matrix.m[1][0] + z * matrix.m[2][0] + matrix.m[3][0];
    out.y = x * matrix.m[0][1] + y * matrix.m[1][1] + z * matrix.m[2][1] + matrix.m[3][1];
    out.z = x * matrix.m[0][2] + y * matrix.m[1][2] + z * matrix.m[2][2] + matrix.m[3][2];
    out.w = x * matrix.m[0][3] + y * matrix.m[1][3] + z * matrix.m[2][3] + matrix.m[3][3];

    x = out.x;
    y = out.y;
    z = out.z;
    w = out.w;
}

void ImVec4::TransformVector(const ImMat4x4& matrix)
{
    ImVec4 out;

    out.x = x * matrix.m[0][0] + y * matrix.m[1][0] + z * matrix.m[2][0];
    out.y = x * matrix.m[0][1] + y * matrix.m[1][1] + z * matrix.m[2][1];
    out.z = x * matrix.m[0][2] + y * matrix.m[1][2] + z * matrix.m[2][2];
    out.w = x * matrix.m[0][3] + y * matrix.m[1][3] + z * matrix.m[2][3];

    x = out.x;
    y = out.y;
    z = out.z;
    w = out.w;
}

float ImMat4x4::Inverse(const ImMat4x4 &srcMatrix, bool affine)
{
    float det = 0;

    if (affine)
    {
        det = GetDeterminant();
        float s = 1 / det;
        m[0][0] = (srcMatrix.m[1][1] * srcMatrix.m[2][2] - srcMatrix.m[1][2] * srcMatrix.m[2][1]) * s;
        m[0][1] = (srcMatrix.m[2][1] * srcMatrix.m[0][2] - srcMatrix.m[2][2] * srcMatrix.m[0][1]) * s;
        m[0][2] = (srcMatrix.m[0][1] * srcMatrix.m[1][2] - srcMatrix.m[0][2] * srcMatrix.m[1][1]) * s;
        m[1][0] = (srcMatrix.m[1][2] * srcMatrix.m[2][0] - srcMatrix.m[1][0] * srcMatrix.m[2][2]) * s;
        m[1][1] = (srcMatrix.m[2][2] * srcMatrix.m[0][0] - srcMatrix.m[2][0] * srcMatrix.m[0][2]) * s;
        m[1][2] = (srcMatrix.m[0][2] * srcMatrix.m[1][0] - srcMatrix.m[0][0] * srcMatrix.m[1][2]) * s;
        m[2][0] = (srcMatrix.m[1][0] * srcMatrix.m[2][1] - srcMatrix.m[1][1] * srcMatrix.m[2][0]) * s;
        m[2][1] = (srcMatrix.m[2][0] * srcMatrix.m[0][1] - srcMatrix.m[2][1] * srcMatrix.m[0][0]) * s;
        m[2][2] = (srcMatrix.m[0][0] * srcMatrix.m[1][1] - srcMatrix.m[0][1] * srcMatrix.m[1][0]) * s;
        m[3][0] = -(m[0][0] * srcMatrix.m[3][0] + m[1][0] * srcMatrix.m[3][1] + m[2][0] * srcMatrix.m[3][2]);
        m[3][1] = -(m[0][1] * srcMatrix.m[3][0] + m[1][1] * srcMatrix.m[3][1] + m[2][1] * srcMatrix.m[3][2]);
        m[3][2] = -(m[0][2] * srcMatrix.m[3][0] + m[1][2] * srcMatrix.m[3][1] + m[2][2] * srcMatrix.m[3][2]);
    }
    else
    {
        // transpose matrix
        float src[16];
        for (int i = 0; i < 4; ++i)
        {
            src[i] = srcMatrix.m16[i * 4];
            src[i + 4] = srcMatrix.m16[i * 4 + 1];
            src[i + 8] = srcMatrix.m16[i * 4 + 2];
            src[i + 12] = srcMatrix.m16[i * 4 + 3];
        }

        // calculate pairs for first 8 elements (cofactors)
        float tmp[12]; // temp array for pairs
        tmp[0] = src[10] * src[15];
        tmp[1] = src[11] * src[14];
        tmp[2] = src[9] * src[15];
        tmp[3] = src[11] * src[13];
        tmp[4] = src[9] * src[14];
        tmp[5] = src[10] * src[13];
        tmp[6] = src[8] * src[15];
        tmp[7] = src[11] * src[12];
        tmp[8] = src[8] * src[14];
        tmp[9] = src[10] * src[12];
        tmp[10] = src[8] * src[13];
        tmp[11] = src[9] * src[12];

        // calculate first 8 elements (cofactors)
        m16[0] = (tmp[0] * src[5] + tmp[3] * src[6] + tmp[4] * src[7]) - (tmp[1] * src[5] + tmp[2] * src[6] + tmp[5] * src[7]);
        m16[1] = (tmp[1] * src[4] + tmp[6] * src[6] + tmp[9] * src[7]) - (tmp[0] * src[4] + tmp[7] * src[6] + tmp[8] * src[7]);
        m16[2] = (tmp[2] * src[4] + tmp[7] * src[5] + tmp[10] * src[7]) - (tmp[3] * src[4] + tmp[6] * src[5] + tmp[11] * src[7]);
        m16[3] = (tmp[5] * src[4] + tmp[8] * src[5] + tmp[11] * src[6]) - (tmp[4] * src[4] + tmp[9] * src[5] + tmp[10] * src[6]);
        m16[4] = (tmp[1] * src[1] + tmp[2] * src[2] + tmp[5] * src[3]) - (tmp[0] * src[1] + tmp[3] * src[2] + tmp[4] * src[3]);
        m16[5] = (tmp[0] * src[0] + tmp[7] * src[2] + tmp[8] * src[3]) - (tmp[1] * src[0] + tmp[6] * src[2] + tmp[9] * src[3]);
        m16[6] = (tmp[3] * src[0] + tmp[6] * src[1] + tmp[11] * src[3]) - (tmp[2] * src[0] + tmp[7] * src[1] + tmp[10] * src[3]);
        m16[7] = (tmp[4] * src[0] + tmp[9] * src[1] + tmp[10] * src[2]) - (tmp[5] * src[0] + tmp[8] * src[1] + tmp[11] * src[2]);

        // calculate pairs for second 8 elements (cofactors)
        tmp[0] = src[2] * src[7];
        tmp[1] = src[3] * src[6];
        tmp[2] = src[1] * src[7];
        tmp[3] = src[3] * src[5];
        tmp[4] = src[1] * src[6];
        tmp[5] = src[2] * src[5];
        tmp[6] = src[0] * src[7];
        tmp[7] = src[3] * src[4];
        tmp[8] = src[0] * src[6];
        tmp[9] = src[2] * src[4];
        tmp[10] = src[0] * src[5];
        tmp[11] = src[1] * src[4];

        // calculate second 8 elements (cofactors)
        m16[8] = (tmp[0] * src[13] + tmp[3] * src[14] + tmp[4] * src[15]) - (tmp[1] * src[13] + tmp[2] * src[14] + tmp[5] * src[15]);
        m16[9] = (tmp[1] * src[12] + tmp[6] * src[14] + tmp[9] * src[15]) - (tmp[0] * src[12] + tmp[7] * src[14] + tmp[8] * src[15]);
        m16[10] = (tmp[2] * src[12] + tmp[7] * src[13] + tmp[10] * src[15]) - (tmp[3] * src[12] + tmp[6] * src[13] + tmp[11] * src[15]);
        m16[11] = (tmp[5] * src[12] + tmp[8] * src[13] + tmp[11] * src[14]) - (tmp[4] * src[12] + tmp[9] * src[13] + tmp[10] * src[14]);
        m16[12] = (tmp[2] * src[10] + tmp[5] * src[11] + tmp[1] * src[9]) - (tmp[4] * src[11] + tmp[0] * src[9] + tmp[3] * src[10]);
        m16[13] = (tmp[8] * src[11] + tmp[0] * src[8] + tmp[7] * src[10]) - (tmp[6] * src[10] + tmp[9] * src[11] + tmp[1] * src[8]);
        m16[14] = (tmp[6] * src[9] + tmp[11] * src[11] + tmp[3] * src[8]) - (tmp[10] * src[11] + tmp[2] * src[8] + tmp[7] * src[9]);
        m16[15] = (tmp[10] * src[10] + tmp[4] * src[8] + tmp[9] * src[9]) - (tmp[8] * src[9] + tmp[11] * src[10] + tmp[5] * src[8]);

        // calculate determinant
        det = src[0] * m16[0] + src[1] * m16[1] + src[2] * m16[2] + src[3] * m16[3];

        // calculate matrix inverse
        float invdet = 1 / det;
        for (int j = 0; j < 16; ++j)
        {
            m16[j] *= invdet;
        }
    }

    return det;
}

void ImMat4x4::RotationAxis(const ImVec4 &axis, float angle)
{
    float length2 = axis.LengthSq();
    if (length2 < FLT_EPSILON)
    {
        SetToIdentity();
        return;
    }

    ImVec4 n = axis * (1.f / sqrtf(length2));
    float s = sinf(angle);
    float c = cosf(angle);
    float k = 1.f - c;

    float xx = n.x * n.x * k + c;
    float yy = n.y * n.y * k + c;
    float zz = n.z * n.z * k + c;
    float xy = n.x * n.y * k;
    float yz = n.y * n.z * k;
    float zx = n.z * n.x * k;
    float xs = n.x * s;
    float ys = n.y * s;
    float zs = n.z * s;

    m[0][0] = xx;
    m[0][1] = xy + zs;
    m[0][2] = zx - ys;
    m[0][3] = 0.f;
    m[1][0] = xy - zs;
    m[1][1] = yy;
    m[1][2] = yz + xs;
    m[1][3] = 0.f;
    m[2][0] = zx + ys;
    m[2][1] = yz - xs;
    m[2][2] = zz;
    m[2][3] = 0.f;
    m[3][0] = 0.f;
    m[3][1] = 0.f;
    m[3][2] = 0.f;
    m[3][3] = 1.f;
}
//-----------------------------------------------------------------------------
