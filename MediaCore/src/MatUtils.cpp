#include <cassert>
#include <cstring>
#include "MatUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

namespace MatUtils
{
void CopyAudioMatSamples(ImGui::ImMat& dstMat, const ImGui::ImMat& srcMat, uint32_t dstOffSmpCnt, uint32_t srcOffSmpCnt, uint32_t copySmpCnt)
{
    assert("Argument 'srcMat' must NOT be EMPTY!" && !srcMat.empty());
    if (copySmpCnt == 0)
    {
        assert("Argument 'srcOffCmpCnt' is larger than or equal to 'srcMat.w'!" && srcOffSmpCnt < (uint32_t)srcMat.w);
        copySmpCnt = (uint32_t)srcMat.w-srcOffSmpCnt;
        if (!dstMat.empty())
        {
            assert("Argument 'dstOffSmpCnt' is larger than or equal to 'dstMat.w'!" && dstOffSmpCnt < (uint32_t)dstMat.w);
            uint32_t remainSpace = (uint32_t)dstMat.w-dstOffSmpCnt;
            if (copySmpCnt > remainSpace)
                copySmpCnt = remainSpace;
        }
    }
    else
    {
        assert("Argument 'srcOffCmpCnt' is larger than or equal to 'srcMat.w'!" && srcOffSmpCnt < (uint32_t)srcMat.w);
        uint32_t copySmpCntMax = (uint32_t)srcMat.w-srcOffSmpCnt;
        if (!dstMat.empty())
        {
            assert("Argument 'dstOffSmpCnt' is larger than or equal to 'dstMat.w'!" && dstOffSmpCnt < (uint32_t)dstMat.w);
            uint32_t remainSpace = (uint32_t)dstMat.w-dstOffSmpCnt;
            if (copySmpCntMax > remainSpace)
                copySmpCntMax = remainSpace;
        }
        assert("Argument 'copySmpCnt' is larger than 'copySmpCntMax'!" && copySmpCnt <= copySmpCntMax);
    }
    if (dstMat.empty())
    {
        int dstW = (int)(dstOffSmpCnt+copySmpCnt);
        int dstH = srcMat.h;
        int dstC = srcMat.c;
        dstMat.create_type(dstW, dstH, dstC, srcMat.type);
        assert("Failed to create 'dstMat'!" && !dstMat.empty());
        memset(dstMat.data, 0, dstMat.total()*dstMat.elemsize);
        dstMat.flags |= IM_MAT_FLAGS_AUDIO_FRAME;
        dstMat.rate.num = srcMat.rate.num;
        dstMat.rate.den = srcMat.rate.den;
        dstMat.elempack = srcMat.elempack;
    }
    else
    {
        assert("Audio sample format conversion is NOT SUPPORTED!" && dstMat.type == srcMat.type);
        assert("The height attribute of 'srcMat' and 'dstMat' does NOT MATCH!" && dstMat.h == srcMat.h);
        assert("The channel attribute of 'srcMat' and 'dstMat' does NOT MATCH!" && dstMat.c == srcMat.c);
        assert("The elempack attribute of 'srcMat' and 'dstMat' does NOT MATCH!" && dstMat.elempack == srcMat.elempack);
        assert("The rate attribute of 'srcMat' and 'dstMat' does NOT MATCH!" && dstMat.rate.num == srcMat.rate.num && dstMat.rate.den == srcMat.rate.den);
    }

    const bool isPlanar = srcMat.elempack == 1 || srcMat.c == 1;
    const size_t unitSize = isPlanar ? srcMat.elemsize : srcMat.elemsize*srcMat.c;
    if (isPlanar)
    {
        const size_t srcLineSize = srcMat.w*unitSize;
        const size_t dstLineSize = dstMat.w*unitSize;
        const size_t srcOffset = srcOffSmpCnt*unitSize;
        const size_t dstOffset = dstOffSmpCnt*unitSize;
        const size_t copySize = copySmpCnt*unitSize;
        const uint8_t* srcPtr = (const uint8_t*)srcMat.data+srcOffset;
        uint8_t* dstPtr = (uint8_t*)dstMat.data+dstOffset;
        const int chCnt = srcMat.c;
        int i = 0;
        while (i++ < chCnt)
        {
            memcpy(dstPtr, srcPtr, copySize);
            srcPtr += srcLineSize;
            dstPtr += dstLineSize;
        }
    }
    else
    {
        const size_t srcOffset = srcOffSmpCnt*unitSize;
        const size_t dstOffset = dstOffSmpCnt*unitSize;
        const size_t copySize = copySmpCnt*unitSize;
        const uint8_t* srcPtr = (const uint8_t*)srcMat.data+srcOffset;
        uint8_t* dstPtr = (uint8_t*)dstMat.data+dstOffset;
        memcpy(dstPtr, srcPtr, copySize);
    }
}

ImDataType PcmFormat2ImDataType(MediaCore::AudioRender::PcmFormat pcmFormat)
{
    ImDataType dataType;
    switch (pcmFormat)
    {
    case MediaCore::AudioRender::PcmFormat::FLOAT32:
        dataType = IM_DT_FLOAT32;
        break;
    case MediaCore::AudioRender::PcmFormat::SINT16:
        dataType = IM_DT_INT16;
        break;
    default:
        dataType = IM_DT_UNDEFINED;
    }
    return dataType;
}

MediaCore::AudioRender::PcmFormat ImDataType2PcmFormat(ImDataType dataType)
{
    MediaCore::AudioRender::PcmFormat pcmFormat;
    switch (dataType)
    {
    case IM_DT_FLOAT32:
        pcmFormat = MediaCore::AudioRender::PcmFormat::FLOAT32;
        break;
    case IM_DT_INT16:
        pcmFormat =  MediaCore::AudioRender::PcmFormat::SINT16;
        break;
    default:
        pcmFormat = MediaCore::AudioRender::PcmFormat::UNKNOWN;
    }
    return pcmFormat;
}
}