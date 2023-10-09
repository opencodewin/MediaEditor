#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include <ostream>
#include "UI.h"
#include "imgui_curve.h"
#include "imgui_json.h"

namespace MEC
{
    struct EventStack;

    struct Event
    {
        using Holder = std::shared_ptr<Event>;
        static bool CheckEventOverlapped(const Event& e, int64_t start, int64_t end, int32_t z);
        static std::function<bool(const Event&,const Event&)> EVENT_ORDER_COMPARATOR;

        virtual EventStack* GetOwner() = 0;
        virtual int64_t Id() const = 0;
        virtual int64_t Start() const = 0;
        virtual int64_t End() const = 0;
        virtual int64_t Length() const = 0;
        virtual int32_t Z() const = 0;
        virtual uint32_t Status() const = 0;
        virtual bool IsInRange(int64_t pos) const = 0;
        virtual BluePrint::BluePrintUI* GetBp() = 0;
        virtual ImGui::KeyPointEditor* GetKeyPoint() = 0;
        virtual bool ChangeRange(int64_t start, int64_t end) = 0;
        virtual void ChangeId(int64_t id) = 0;
        virtual bool Move(int64_t start, int32_t z) = 0;
        virtual void SetStatus(uint32_t status) = 0;
        virtual void SetStatus(int bit, int val) = 0;
        virtual std::string GetError() const = 0;
        virtual imgui_json::value SaveAsJson() const = 0;

        friend std::ostream& operator<<(std::ostream& os, const Event& e);
    };

    struct VideoEvent : virtual Event
    {
        virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) = 0;

        virtual int GetMaskCount() const = 0;
        virtual int GetMaskCount(int64_t nodeId) const = 0;
        virtual bool GetMask(imgui_json::value& j, int index) const = 0;
        virtual bool GetMask(imgui_json::value& j, int64_t nodeId, int index) const = 0;
        virtual bool RemoveMask(int index) = 0;
        virtual bool RemoveMask(int64_t nodeId, int index) = 0;
        virtual bool SaveMask(const imgui_json::value& j, int index = -1) = 0;
        virtual bool SaveMask(int64_t nodeId, const imgui_json::value& j, int index = -1) = 0;
    };

    struct AudioEvent : virtual Event
    {
        virtual ImGui::ImMat FilterPcm(const ImGui::ImMat& amat, int64_t pos, int64_t dur) = 0;
    };
}