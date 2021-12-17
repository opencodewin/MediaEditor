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
        ERROR,
    };

    struct Logger
    {
        virtual void Log(Level l, const std::string fmt, ...) = 0;
        virtual std::ostream& Log(Level l) = 0;
        virtual void SetShowLevels(Level l , int n = 1) = 0;
        virtual void SetShowLevelName(bool show) = 0;
        virtual void SetShowTime(bool show) = 0;
    };

    void SetSingleLogMaxSize(uint32_t size);

    bool SetDefaultLoggerType(const std::string& loggerType);
    void SetDefaultLoggerLevels(Level l , int n = 1);

    void Log(Level l, const std::string fmt, ...);
    std::ostream& Log(Level l);
}