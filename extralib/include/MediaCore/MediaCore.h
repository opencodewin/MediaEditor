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
#ifdef _WIN32
#if MEDIACORE_SHARED
#define MEDIACORE_API __declspec( dllexport )
#else
#define MEDIACORE_API
#endif
#else
#define MEDIACORE_API
#endif

#define THREAD_IDLE_TIME    20

#include <cstdint>
#include "immat.h"

namespace MediaCore
{
enum ErrorCode
{
    Ok = 0,
    Failed,
    InvalidArgument,
    Eof,
    NotReady,
    Unsupported,
    UnknownError,
};

MEDIACORE_API void GetVersion(int& major, int& minor, int& patch, int& build);
}
