#pragma once
#include <string>
#include <memory>
#include <imgui_json.h>
#include <ThreadUtils.h>
#include <Logger.h>
#include <SharedSettings.h>

namespace MEC
{
    struct BackgroundTask : public SysUtils::BaseAsyncTask
    {
        using Holder = std::shared_ptr<BackgroundTask>;
        static Holder CreateBackgroundTask(const imgui_json::value& jnTask, MediaCore::SharedSettings::Holder hSettings);

        virtual bool Pause() = 0;
        virtual bool IsPaused() const = 0;
        virtual bool Resume() = 0;
        virtual bool DrawContent(const ImVec2& v2ViewSize) = 0;
        virtual void DrawContentCompact() = 0;
        virtual std::string GetTaskDir() const = 0;
        virtual bool SaveAsJson(imgui_json::value& jnTask) = 0;
        virtual std::string Save(const std::string& strSavePath = "") = 0;

        virtual std::string GetError() const = 0;
        virtual void SetLogLevel(Logger::Level l) = 0;

    };
}
