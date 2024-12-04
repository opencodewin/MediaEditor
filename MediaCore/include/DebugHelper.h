/*
    Copyright (c) 2023-2024 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#include <chrono>
#include <string>
#include <memory>
#include <utility>
#include <ostream>
#include "MediaCore.h"
#include "Logger.h"

namespace MediaCore
{
    using SysClock = std::chrono::system_clock;
    using TimePoint = std::chrono::time_point<SysClock>;
    using TimeSpan = std::pair<TimePoint, TimePoint>;

    inline TimePoint GetTimePoint()
    {
        return SysClock::now();
    }

    constexpr int64_t CountElapsedMillisec(const TimePoint& t0, const TimePoint& t1)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    }

    MEDIACORE_API int64_t GetMillisecFromTimePoint(const TimePoint& tp);

    MEDIACORE_API void AddCheckPoint(const std::string& name);
    MEDIACORE_API void LogCheckPointsTimeInfo(Logger::ALogger* logger = nullptr, Logger::Level loglvl = Logger::DEBUG);

    MEDIACORE_API std::ostream& operator<<(std::ostream& os, const TimeSpan& ts);

    struct PerformanceAnalyzer
    {
        using Holder = std::shared_ptr<PerformanceAnalyzer>;
        static MEDIACORE_API Holder CreateInstance(const std::string& name);
        static MEDIACORE_API Holder GetThreadLocalInstance();

        virtual void SetLogInterval(uint32_t millisec) = 0;
        virtual void Start() = 0;
        virtual TimeSpan End() = 0;
        virtual void Reset() = 0;
        virtual void SectionStart(const std::string& name) = 0;
        virtual void SectionEnd() = 0;
        virtual void PushAndSectionStart(const std::string& name) = 0;
        virtual void PopSection() = 0;
        virtual void EnterSleep() = 0;
        virtual void QuitSleep() = 0;
        virtual TimeSpan LogStatisticsOnInterval(Logger::Level l, Logger::ALogger* logger = nullptr) = 0;
        virtual TimeSpan LogAndClearStatistics(Logger::Level l, Logger::ALogger* logger = nullptr) = 0;
    };

    class MEDIACORE_API AutoSection
    {
    public:
        AutoSection(const std::string& name, PerformanceAnalyzer::Holder hPa = nullptr);
        ~AutoSection();

        AutoSection() = delete;
        AutoSection(const AutoSection&) = delete;
        AutoSection(AutoSection&&) = delete;
        AutoSection& operator=(const AutoSection&) = delete;

    private:
        PerformanceAnalyzer::Holder m_hPa;
    };
}