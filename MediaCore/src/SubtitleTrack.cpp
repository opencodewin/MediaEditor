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

#include "SubtitleTrack.h"
#include "SubtitleTrack_AssImpl.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
    bool InitializeSubtitleLibrary()
    {
        bool success = SubtitleTrack_AssImpl::Initialize();
        return success;
    }

    void ReleaseSubtitleLibrary()
    {
        SubtitleTrack_AssImpl::Release();
    }

    bool SetFontDir(const string& path)
    {
        return SubtitleTrack_AssImpl::SetFontDir(path);
    }

    SubtitleTrackHolder SubtitleTrack::BuildFromFile(int64_t id, const string& url)
    {
        return SubtitleTrack_AssImpl::BuildFromFile(id, url);
    }

    SubtitleTrackHolder SubtitleTrack::NewEmptyTrack(int64_t id)
    {
        return SubtitleTrack_AssImpl::NewEmptyTrack(id);
    }

    ALogger* G_SubtilteTrackLogger = nullptr;
}

ALogger* GetSubtitleTrackLogger()
{
    if (!MediaCore::G_SubtilteTrackLogger)
        MediaCore::G_SubtilteTrackLogger = GetLogger("SubtitleTrack");
    return MediaCore::G_SubtilteTrackLogger;
}