#include <BluePrint.h>
#include <Pin.h>
#include <Node.h>
#include <inttypes.h>

std::mutex g_Mutex;

namespace BluePrint
{
void Context::SetContextMonitor(ContextMonitor* monitor)
{
    m_Monitor = monitor;
}

ContextMonitor* Context::GetContextMonitor()
{
    return m_Monitor;
}

const ContextMonitor* Context::GetContextMonitor() const
{
    return m_Monitor;
}

void Context::ResetState()
{
    m_Values.clear();
}

StepResult Context::Start(FlowPin& entryPoint, bool bypass_bg_node)
{
    m_Callstack.resize(0);
    m_bypass_bg_node = bypass_bg_node;
    m_CurrentNode = entryPoint.m_Node;
    m_CurrentFlowPin = entryPoint;
    m_StepCount = 0;

    g_Mutex.lock();
    if (m_Monitor)
        m_Monitor->OnStart(*this);
    g_Mutex.unlock();

    if (m_CurrentNode == nullptr || m_CurrentFlowPin.m_ID == 0)
        return SetStepResult(StepResult::Error);

    return SetStepResult(StepResult::Success);
}

StepResult Context::Step(Context * context, bool restep)
{
    bool isthreading = context ? true : false;
    if (!context)
        context = this;
    if (context->m_LastResult != StepResult::Success)
        return context->m_LastResult;

    auto currentFlowPin = context->m_CurrentFlowPin;
    g_Mutex.lock();
    context->m_PrevNode = context->m_CurrentNode;
    context->m_PrevFlowPin = context->m_CurrentFlowPin;
    context->m_CurrentNode = nullptr;
    context->m_CurrentFlowPin = {};
    g_Mutex.unlock();

    if (currentFlowPin.m_ID == 0 && context->m_Callstack.empty())
        return context->SetStepResult(StepResult::Done);

    auto entryPoint = context->GetPinValue(currentFlowPin, isthreading);

    if (entryPoint.GetType() != PinType::Flow)
        return context->SetStepResult(StepResult::Error);

    auto entryPin = entryPoint.As<FlowPin*>();

    g_Mutex.lock();
    context->m_CurrentNode = entryPin->m_Node;
    g_Mutex.unlock();

    ++m_StepCount;

    g_Mutex.lock();
    if (context->m_Monitor)
        context->m_Monitor->OnPreStep(*context);
    g_Mutex.unlock();
    
    if (!entryPin->m_Node)
        return context->SetStepResult(StepResult::Done);

    entryPin->m_Node->m_Hits ++;

    auto start_time = ImGui::get_current_time_usec();
    auto next = entryPin->m_Node->Execute(*context, *entryPin, isthreading);
    auto end_time = ImGui::get_current_time_usec();
    entryPin->m_Node->m_Tick += end_time - start_time;

    entryPin->m_Node->m_HitCount ++;
    entryPin->m_Node->m_CountTimeMs += entryPin->m_Node->m_NodeTimeMs;
    if (entryPin->m_Node->m_HitCount > 100)
    {
        entryPin->m_Node->m_HitCount = 100;
        entryPin->m_Node->m_CountTimeMs -= entryPin->m_Node->m_AvgTimeMs;
    }
    entryPin->m_Node->m_AvgTimeMs = entryPin->m_Node->m_HitCount > 0 ? entryPin->m_Node->m_CountTimeMs / entryPin->m_Node->m_HitCount : 0;

    if (next.m_Node)
    {
        auto bp = next.m_Node->m_Blueprint;
        auto link = next.GetLink(bp);
        while (link && link->IsMappedPin())
        {
            link = link->GetLink(bp);
        }
        if (link && link->m_Type == PinType::Flow)
        {
            g_Mutex.lock();
            context->m_CurrentFlowPin = next;
            g_Mutex.unlock();
        }
        else 
        {
            if (context->m_StepToEnd)
            {
                g_Mutex.lock();
                if (context->m_StepNode) context->m_CurrentNode = context->m_StepNode;
                if (context->m_StepFlowPin) context->m_CurrentFlowPin = *context->m_StepFlowPin;
                context->m_StepNode = nullptr;
                context->m_StepToEnd = false;
                g_Mutex.unlock();
            }
            else if (!context->m_Callstack.empty())
            {
                g_Mutex.lock();
                context->m_CurrentFlowPin = context->m_Callstack.back();
                context->m_Callstack.pop_back();
                g_Mutex.unlock();
            }
        }
    }
    else if (!context->m_Callstack.empty())
    {
        g_Mutex.lock();
        context->m_CurrentFlowPin = context->m_Callstack.back();
        context->m_Callstack.pop_back();
        g_Mutex.unlock();
    }

    g_Mutex.lock();
    if (context->m_Monitor)
        context->m_Monitor->OnPostStep(*context);
    g_Mutex.unlock();

    return context->SetStepResult(StepResult::Success);
}

StepResult Context::Restep(Context * context)
{
    if (context->m_StepCount > 0)
    {
        g_Mutex.lock();
        context->m_CurrentNode = context->m_PrevNode;
        context->m_Callstack.push_back(context->m_CurrentFlowPin);
        context->m_CurrentFlowPin = context->m_PrevFlowPin;
        g_Mutex.unlock();
        context->m_StepCount--;
        return Step(context, true);
    }
    return SetStepResult(StepResult::Success);
}

StepResult Context::StepToEnd(Node* node)
{
    auto result = SetStepResult(StepResult::Error);
    if (!node || m_Executing || m_ThreadRunning)
    {
        return result;
    }
    m_StepToEnd = true;
    auto stepFlowPin = (FlowPin *)node->GetAutoLinkInputFlowPin();
    if (stepFlowPin)
        result = Run(*stepFlowPin);
    m_StepToEnd = false;
    return result;
}

StepResult Context::Run(FlowPin& entryPoint, bool bypass_bg_node)
{
    m_Executing = true;
    m_ThreadRunning = false;
    Start(entryPoint, bypass_bg_node);
    auto result = StepResult::Done;
    while (true)
    {
        result = Step();
        if (result != StepResult::Success)
            break;
    }
    m_Executing = false;
    m_bypass_bg_node = false;
    m_PrevNode = nullptr;
    m_CurrentNode = nullptr;
    m_PrevFlowPin = {};
    m_CurrentFlowPin = {};
    m_Callstack.clear();
    return result;
}

static void RunThread(Context& context, FlowPin& entryPoint, bool bypass_bg_node)
{
    ContextMonitor* monitor = context.m_Monitor;
    BluePrint::StepResult result = BluePrint::StepResult::Done;
    context.SetContextMonitor(nullptr);
    context.Start(entryPoint, bypass_bg_node);
    context.m_Executing = true;
    context.m_ThreadRunning = true;
    context.m_pause_event = false;
    while (context.m_Executing)
    {
        if (context.m_Paused && !context.m_StepToNext && !context.m_StepCurrent && !context.m_StepToEnd)
        {
            context.SetContextMonitor(monitor);
            if (!context.m_pause_event)
            {
                if (monitor) monitor->OnPause(context);
                context.m_pause_event = true;
            }
            ImGui::sleep(0.02f);
            continue;
        }
        else
        {
            context.SetContextMonitor(nullptr);
        }

        if (context.m_Paused && context.m_StepCurrent)
        {
            context.SetContextMonitor(monitor);
            result = context.Restep(&context);
            if (monitor)  monitor->OnStepCurrent(context);
            context.m_StepCurrent = false;
        }
        else if (context.m_Paused && context.m_StepToNext)
        {
            context.SetContextMonitor(monitor);
            result = context.Step(&context);
            if (monitor)  monitor->OnStepNext(context);
            context.m_StepToNext = false;
        }
        else
        {
            result = context.Step(&context);
        }
        if (context.m_StepToNext)
            context.m_StepToNext = false;
        
        if (context.m_CurrentNode && context.m_CurrentNode->m_BreakPoint)
        {
            context.m_Paused = true;
        }
        if (result != BluePrint::StepResult::Success)
            break;
        std::this_thread::yield();
    }
    context.m_Executing = false;
    context.m_Paused = false;
    context.m_ThreadRunning = false;
    context.m_StepToEnd = false;
    context.m_StepFlowPin = nullptr;
    context.m_StepNode = nullptr;
    context.m_PrevNode = nullptr;
    context.m_CurrentNode = nullptr;
    context.m_bypass_bg_node = false;
    context.m_PrevFlowPin = {};
    context.m_CurrentFlowPin = {};
    context.m_Callstack.clear();
    context.SetContextMonitor(monitor);
    LOGI("Execution: Finished at step %" PRIu32, context.StepCount());
    context.SetStepResult(BluePrint::StepResult::Done);
    return;
}

StepResult Context::Execute(FlowPin& entryPoint, bool bypass_bg_node)
{
    StepResult result = StepResult::Done;
    if (m_Executing && m_Paused)
    {
        m_Paused = false;
        m_pause_event = false;
        g_Mutex.lock();
        if (m_Monitor)
            m_Monitor->OnResume(*this);
        g_Mutex.unlock();
        return SetStepResult(StepResult::Success);
    }
    if (m_thread)
    {
        m_Executing = false;
        if (m_thread->joinable())
        {
            m_thread->join();
            delete m_thread;
            m_thread = nullptr;
        }
    }
    m_thread = new std::thread(RunThread, std::ref(*this), std::ref(entryPoint), bypass_bg_node);
    return result;
}

StepResult Context::Stop()
{
    if (m_ThreadRunning && m_Executing && m_thread)
    {
        m_Executing = false;
        if (m_thread->joinable())
        {
            m_thread->join();
            delete m_thread;
            m_thread = nullptr;
        }
        return SetStepResult(StepResult::Success);
    }
    else if (m_thread)
    {
        m_thread = nullptr;
    }

    if (m_LastResult != StepResult::Success)
        return m_LastResult;

    m_PrevNode = nullptr;
    m_CurrentNode = nullptr;
    m_PrevFlowPin = {};
    m_CurrentFlowPin = {};
    m_Callstack.clear();

    return SetStepResult(StepResult::Done);
}

StepResult Context::Pause()
{
    m_Paused = true;
    return SetStepResult(StepResult::Success);
}

StepResult Context::ThreadStep()
{
    if (m_Paused)
        m_StepToNext = true;
    return SetStepResult(StepResult::Success);
}

StepResult Context::ThreadRestep()
{
    if (m_Paused)
        m_StepCurrent = true;
    return SetStepResult(StepResult::Success);
}

StepResult Context::ThreadStepToEnd(Node* node)
{
    if (m_Paused)
    {
        m_StepToEnd = true;
        if (node)
        {
            g_Mutex.lock();
            m_StepNode = node;
            m_CurrentNode = node;
            m_StepFlowPin = (FlowPin *)node->GetAutoLinkInputFlowPin();
            g_Mutex.unlock();
        }
    }
    return SetStepResult(StepResult::Success);
}

void Context::ShowFlow()
{
    if (!m_CurrentNode)
    {
        return;
    }
    ed::PushStyleVar(ed::StyleVar_FlowMarkerDistance, 30.0f);
    ed::PushStyleVar(ed::StyleVar_FlowDuration, 1.0f);
    if (m_PrevNode)
    {
        for (auto pin : m_PrevNode->GetOutputPins())
        {
            if (!pin->m_Link || !pin->m_Node)
                continue;
            auto bp = pin->m_Node->m_Blueprint;
            auto link = pin->GetLink(bp);
            while (link && link->IsMappedPin())
            {
                link = link->GetLink(bp);
            }
            if (!link || link->m_Node != m_CurrentNode)
                continue;
            ed::Flow(pin->m_ID, pin->GetType() == PinType::Flow ? ed::FlowDirection::Forward : ed::FlowDirection::Backward);
            link = pin->GetLink();
            while (link && link->IsMappedPin())
            {
                ed::Flow(link->m_ID, link->GetType() == PinType::Flow ? ed::FlowDirection::Forward : ed::FlowDirection::Backward);
                link = link->GetLink(bp);
            }
        }
    }

    if (m_CurrentNode)
    {
        for (auto pin : m_CurrentNode->GetInputPins())
        {
            if (!pin->m_Link || !pin->m_Node)
                continue;
            auto bp = pin->m_Node->m_Blueprint;
            auto link = pin->GetLink(bp);
            while (link && link->IsMappedPin())
            {
                link = link->GetLink(bp);
            }
            if (!link)
                continue;
            
            ed::Flow(pin->m_ID, pin->GetType() == PinType::Flow ? ed::FlowDirection::Forward : ed::FlowDirection::Backward);
            link = pin->GetLink(bp);
            while (link && link->IsMappedPin())
            {
                ed::Flow(link->m_ID, link->GetType() == PinType::Flow ? ed::FlowDirection::Forward : ed::FlowDirection::Backward);
                link = link->GetLink(bp);
            }
        }
    }
    ed::PopStyleVar(2);
}

Node* Context::CurrentNode()
{
    return m_CurrentNode;
}

const Node* Context::CurrentNode() const
{
    return m_CurrentNode;
}

Node* Context::NextNode()
{
    g_Mutex.lock();
    auto node = m_CurrentFlowPin.m_Node;
    if (m_CurrentFlowPin.m_Link)
    {
        auto bp = node->m_Blueprint;
        auto link = m_CurrentFlowPin.GetLink(bp);
        while (link && !link->IsMappedPin())
        {
            node = link->m_Node;
            link = link->GetLink(bp);
        }
        if (link)
            node = link->m_Node;
    }

    g_Mutex.unlock();
    return node;
}

const Node* Context::NextNode() const
{
    g_Mutex.lock();
    auto node = m_CurrentFlowPin.m_Node;
    if (m_CurrentFlowPin.m_Link)
    {
        auto bp = node->m_Blueprint;
        auto link = m_CurrentFlowPin.GetLink(bp);
        while (link && !link->IsMappedPin())
        {
            node = link->m_Node;
            link = link->GetLink(bp);
        }
        if (link)
            node = link->m_Node;
    }
    g_Mutex.unlock();
    return node;
}

FlowPin Context::CurrentFlowPin() const
{
    return m_CurrentFlowPin;
}

StepResult Context::LastStepResult() const
{
    return m_LastResult;
}

uint32_t Context::StepCount() const
{
    return m_StepCount;
}

void Context::PushReturnPoint(FlowPin& entryPoint)
{
    m_Callstack.push_back(entryPoint);
}

void Context::SetPinValue(const Pin& pin, PinValue value)
{
    m_Values[pin.m_ID] = std::move(value);
}

PinValue Context::GetPinValue(const Pin& pin, bool threading) const
{
    auto valueIt = m_Values.find(pin.m_ID);
    if (valueIt != m_Values.end())
        return valueIt->second;

    if (!pin.m_Node)
        return pin.GetValue();

    PinValue value;
    auto bp = pin.m_Node->m_Blueprint;
    auto link = pin.GetLink(bp);
    if (link)
        value = GetPinValue(*link);
    else if (pin.m_Node)
        value = pin.m_Node->EvaluatePin(*this, pin, threading);
    else
        value = pin.GetValue();

    return std::move(value);
}

StepResult Context::SetStepResult(StepResult result)
{
    m_LastResult = result;
    g_Mutex.lock();
    if (m_Monitor)
    {
        switch (result)
        {
            case StepResult::Done:
                m_Monitor->OnDone(*this);
                break;

            case StepResult::Error:
                m_Monitor->OnError(*this);
                break;
            
            default:
                break;
        }
    }
    g_Mutex.unlock();

    return result;
}
} // namespace BluePrint
