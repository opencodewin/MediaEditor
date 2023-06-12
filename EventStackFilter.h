#pragma once

#include <list>
#include "VideoClip.h"
#include "UI.h"
#include "imgui_curve.h"
#include "imgui_json.h"
#include "Logger.h"

namespace MEC
{
    struct EventStackFilter : public MediaCore::VideoFilter
    {
        static MediaCore::VideoFilter::Holder CreateInstance(const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        static MediaCore::VideoFilter::Holder LoadFromJson(const imgui_json::value& json, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
        virtual imgui_json::value SaveAsJson() const = 0;

        struct Event
        {
            static bool CheckEventOverlapped(const Event* e, int64_t start, int64_t end, int32_t z);

            virtual int64_t Id() const = 0;
            virtual int64_t Start() const = 0;
            virtual int64_t End() const = 0;
            virtual int64_t Length() const = 0;
            virtual int32_t Z() const = 0;
            virtual bool IsInRange(int64_t pos) const = 0;
            virtual BluePrint::BluePrintUI* GetBp() = 0;
            virtual ImGui::KeyPointEditor* GetKeyPoint() = 0;
            virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) = 0;
            virtual bool ChangeRange(int64_t start, int64_t end) = 0;
            virtual bool Move(int64_t start, int32_t z) = 0;
            virtual EventStackFilter* GetOwner() = 0;
            virtual std::string GetError() const = 0;
        };

        virtual Event* GetEvent(int64_t id) = 0;
        virtual Event* AddNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) = 0;
        virtual void RemoveEvent(int64_t id) = 0;
        virtual bool ChangeEventRange(int64_t id, int64_t start, int64_t end) = 0;
        virtual bool MoveEvent(int64_t id, int64_t start, int32_t z) = 0;
        virtual bool SetEditingEvent(int64_t id) = 0;
        virtual Event* GetEditingEvent() = 0;

        virtual void SetTimelineHandle(void* handle) = 0;
        virtual void* GetTimelineHandle() const = 0;
        virtual std::string GetError() const = 0;
        virtual void SetLogLevel(Logger::Level l) = 0;
    };
}