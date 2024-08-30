#pragma once
#include <cstdint>
#include "MediaCore.h"
#include "AudioRender.h"
#include "immat.h"

namespace MatUtils
{
    MEDIACORE_API void CopyAudioMatSamples(ImGui::ImMat& dstMat, const ImGui::ImMat& srcMat, uint32_t dstOffSmpCnt, uint32_t srcOffSmpCnt, uint32_t copySmpCnt = 0);
    MEDIACORE_API ImDataType PcmFormat2ImDataType(MediaCore::AudioRender::PcmFormat pcmFormat);
    MEDIACORE_API MediaCore::AudioRender::PcmFormat ImDataType2PcmFormat(ImDataType dataType);
}