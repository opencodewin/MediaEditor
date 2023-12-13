#pragma once
#include <string>
#include <memory>
#include <imgui_json.h>
#include <ThreadUtils.h>
#include <Logger.h>

namespace MEC
{
    struct BackgroundTask : public SysUtils::BaseAsyncTask
    {
        using Holder = std::shared_ptr<BackgroundTask>;
        virtual void DrawContent() = 0;
        virtual void DrawContentCompact() = 0;

        virtual std::string GetError() const = 0;
        virtual void SetLogLevel(Logger::Level l) = 0;

        static Holder CreateBackgroundTask(const imgui_json::value& jnTask);
    };
}
