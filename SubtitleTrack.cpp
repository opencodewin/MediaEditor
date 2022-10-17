#include "SubtitleTrack.h"
#include "SubtitleTrack_AssImpl.h"

using namespace std;
using namespace Logger;

namespace DataLayer
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
    if (!DataLayer::G_SubtilteTrackLogger)
        DataLayer::G_SubtilteTrackLogger = GetLogger("SubtitleTrack");
    return DataLayer::G_SubtilteTrackLogger;
}