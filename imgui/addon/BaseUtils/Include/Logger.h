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
#include <cstdint>
#include <string>
#include <ostream>
#include "BaseUtilsCommon.h"

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

        virtual std::string GetName() const = 0;
        virtual Level GetShowLevels(int& n) const = 0;
    };

    BASEUTILS_API void SetSingleLogMaxSize(uint32_t size);

    BASEUTILS_API bool SetDefaultLoggerType(const std::string& loggerType);
    BASEUTILS_API ALogger* GetDefaultLogger();
    BASEUTILS_API void Log(Level l, const std::string fmt, ...);
    BASEUTILS_API std::ostream& Log(Level l);

    BASEUTILS_API ALogger* GetLogger(const std::string& name);
}