#pragma once
#include <list>
#include "Event.h"
#include "VideoClip.h"
#include "imgui_curve.h"
#include "imgui_json.h"
#include "Logger.h"

namespace MEC
{
    struct EventStack;

    struct VideoEvent : public Event
    {
        virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) = 0;
        virtual EventStack* GetOwner() = 0;
    };

    struct EventStack
    {
        virtual Event::Holder GetEvent(int64_t id) = 0;
        virtual Event::Holder AddNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) = 0;
        virtual void RemoveEvent(int64_t id) = 0;
        virtual bool ChangeEventRange(int64_t id, int64_t start, int64_t end) = 0;
        virtual bool MoveEvent(int64_t id, int64_t start, int32_t z) = 0;
        virtual bool SetEditingEvent(int64_t id) = 0;
        virtual Event::Holder GetEditingEvent() = 0;
        virtual void SetTimelineHandle(void* handle) = 0;
        virtual void* GetTimelineHandle() const = 0;
        virtual std::string GetError() const = 0;
        virtual void SetLogLevel(Logger::Level l) = 0;
    };

    struct VideoEventStackFilter : public MediaCore::VideoFilter, EventStack
    {
        static MediaCore::VideoFilter::Holder CreateInstance(const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        static MediaCore::VideoFilter::Holder LoadFromJson(const imgui_json::value& json, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        virtual imgui_json::value SaveAsJson() const = 0;
    };
}