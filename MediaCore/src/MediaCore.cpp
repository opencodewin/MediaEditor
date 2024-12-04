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

#include "MediaCore.h"

void MediaCore::GetVersion(int& major, int& minor, int& patch, int& build)
{
    major = MEDIACORE_VERSION_MAJOR;
    minor = MEDIACORE_VERSION_MINOR;
    patch = MEDIACORE_VERSION_PATCH;
    build = MEDIACORE_VERSION_BUILD;
}