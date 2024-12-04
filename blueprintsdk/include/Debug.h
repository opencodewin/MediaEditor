#pragma once
#include <imgui.h>
#include <imgui_helper.h>
#include <BluePrint.h>
#include <Node.h>

enum class LogLevel: int32_t
{
    Verbose,
    Info,
    Debug,
    Warning,
    Error,
};

# define LOGV(...)
# define LOGD(...)
# define LOGI(...)
# define LOGW(...)
# define LOGE(...)

namespace BluePrint
{
struct BluePrintUI;
struct DebugOverlay:
    private ContextMonitor
{
    DebugOverlay() {}
    DebugOverlay(BP* blueprint);
    ~DebugOverlay();

    void Init(BP* blueprint);
    void Begin();
    void End();
    void Enable(bool enable) { m_Enable = enable; };

    void DrawNode(BluePrintUI* ui, const Node& node);
    void DrawInputPin(BluePrintUI* ui, const Pin& pin);
    void DrawOutputPin(BluePrintUI* ui, const Pin& pin);

    ContextMonitor* GetContextMonitor();
    
private:
    void OnDone(Context& context) override;
    void OnPause(Context& context) override;
    void OnResume(Context& context) override;
    void OnStepNext(Context& context) override;
    void OnStepCurrent(Context& context) override;

    bool m_Enable {true};
    BP* m_Blueprint {nullptr};
    const Node* m_CurrentNode {nullptr};
    const Node* m_NextNode {nullptr};
    FlowPin m_CurrentFlowPin;
    ImDrawList* m_DrawList {nullptr};
    ImDrawListSplitter m_Splitter;
};

}