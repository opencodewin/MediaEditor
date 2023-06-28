#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include "UI.h"

namespace MEC
{
    struct Event
    {
        using Holder = std::shared_ptr<Event>;
        static bool CheckEventOverlapped(const Event& e, int64_t start, int64_t end, int32_t z);
        static std::function<bool(const Event&,const Event&)> EVENT_ORDER_COMPARATOR;

        virtual int64_t Id() const = 0;
        virtual int64_t Start() const = 0;
        virtual int64_t End() const = 0;
        virtual int64_t Length() const = 0;
        virtual int32_t Z() const = 0;
        virtual bool IsInRange(int64_t pos) const = 0;
        virtual BluePrint::BluePrintUI* GetBp() = 0;
        virtual ImGui::KeyPointEditor* GetKeyPoint() = 0;
        virtual bool ChangeRange(int64_t start, int64_t end) = 0;
        virtual bool Move(int64_t start, int32_t z) = 0;
        virtual std::string GetError() const = 0;
    };
}