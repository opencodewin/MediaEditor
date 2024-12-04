#pragma once
#include <cstddef>
#include <chrono>
#include "BaseUtilsCommon.h"

namespace SysUtils
{
BASEUTILS_API std::chrono::steady_clock::rep GetTickSinceProcessStart();
BASEUTILS_API std::size_t GetTickHash();
}