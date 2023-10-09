#pragma once
#include <list>
#include "Event.h"
#include "VideoClip.h"
#include "AudioClip.h"
#include "Logger.h"

namespace MEC
{
    struct EventStack
    {
        virtual Event::Holder GetEvent(int64_t id) = 0;
        virtual Event::Holder AddNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) = 0;
        virtual Event::Holder RestoreEventFromJson(const imgui_json::value& eventJson) = 0;
        virtual void RemoveEvent(int64_t id) = 0;
        virtual bool ChangeEventRange(int64_t id, int64_t start, int64_t end) = 0;
        virtual bool MoveEvent(int64_t id, int64_t start, int32_t z) = 0;
        virtual bool MoveAllEvents(int64_t offset) = 0;
        virtual bool SetEditingEvent(int64_t id) = 0;
        virtual Event::Holder GetEditingEvent() = 0;
        virtual std::list<Event::Holder> GetEventList() const = 0;
        virtual std::list<Event::Holder> GetEventListByZ(int32_t z) const = 0;
        virtual void SetTimelineHandle(void* handle) = 0;
        virtual void* GetTimelineHandle() const = 0;
        virtual std::string GetError() const = 0;
        virtual void SetLogLevel(Logger::Level l) = 0;

        friend std::ostream& operator<<(std::ostream& os, const EventStack& e);
    };

    struct VideoEventStackFilter : MediaCore::VideoFilter, virtual EventStack
    {
        static MediaCore::VideoFilter::Holder CreateInstance(const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        static MediaCore::VideoFilter::Holder LoadFromJson(const imgui_json::value& json, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        virtual imgui_json::value SaveAsJson() const = 0;
        virtual void SetBluePrintCallbacks(const BluePrint::BluePrintCallbackFunctions& bpCallbacks) = 0;
    };

    struct AudioEventStackFilter : MediaCore::AudioFilter, virtual EventStack
    {
        static MediaCore::AudioFilter::Holder CreateInstance(const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        static MediaCore::AudioFilter::Holder LoadFromJson(const imgui_json::value& json, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        virtual imgui_json::value SaveAsJson() const = 0;
        virtual void SetBluePrintCallbacks(const BluePrint::BluePrintCallbackFunctions& bpCallbacks) = 0;
    };

    struct EventStackFilterContext
    {
        void* pFilterPtr{nullptr};
        void* pEventPtr{nullptr};
    };
}