#include "VideoTransformFilterUiCtrl.h"

using namespace std;
using namespace MediaCore;
using namespace Logger;

namespace MEC
{
VideoTransformFilterUiCtrl::VideoTransformFilterUiCtrl(VideoTransformFilter::Holder hTransformFilter)
    : m_hTransformFilter(hTransformFilter)
{
    m_pLogger = GetLogger("VTransFilterUiCtrl");
}
}
