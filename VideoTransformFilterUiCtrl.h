#pragma once
#include <Logger.h>
#include <VideoTransformFilter.h>

namespace MEC
{
class VideoTransformFilterUiCtrl
{
public:
    VideoTransformFilterUiCtrl(MediaCore::VideoTransformFilter::Holder hTransformFilter);

    void SetLogLevel(Logger::Level l)
    { m_pLogger->SetShowLevels(l); }

private:
    Logger::ALogger* m_pLogger;
    MediaCore::VideoTransformFilter::Holder m_hTransformFilter;
};
}
