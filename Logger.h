#pragma once
#include <string>
#include <ostream>

namespace Logger
{
    enum Level
    {
        VERBOSE = 0,
        DEBUG,
        INFO,
        WARN,
        Error,
    };

    struct ALogger
    {
        virtual void Log(Level l, const std::string fmt, ...) = 0;
        virtual std::ostream& Log(Level l) = 0;
        virtual ALogger* SetShowLoggerName(bool show) = 0;
        virtual ALogger* SetShowLevels(Level l , int n = 1) = 0;
        virtual ALogger* SetShowLevelName(bool show) = 0;
        virtual ALogger* SetShowTime(bool show) = 0;
    };

    void SetSingleLogMaxSize(uint32_t size);

    bool SetDefaultLoggerType(const std::string& loggerType);
    ALogger* GetDefaultLogger();
    void Log(Level l, const std::string fmt, ...);
    std::ostream& Log(Level l);

    ALogger* GetLogger(const std::string& name);
}