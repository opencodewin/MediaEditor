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

#include <thread>
#include <mutex>
#include <sstream>
#include <atomic>
#include <functional>
#include <list>
#include <algorithm>
#include "MediaReader.h"
#include "FFUtils.h"
#include "ThreadUtils.h"
#include "DebugHelper.h"
extern "C"
{
    #include "libavformat/avformat.h"
    #include "libavcodec/avcodec.h"
}

using namespace std;
using namespace Logger;

namespace MediaCore
{
class ImageSequenceReader_Impl : public MediaReader
{
public:
    ImageSequenceReader_Impl(const string& loggerName)
    {
        if (loggerName.empty())
            m_logger = Logger::GetLogger("ImgsqReader");
        else
            m_logger = Logger::GetLogger(loggerName);
        int n;
        Level l = GetVideoLogger()->GetShowLevels(n);
        m_logger->SetShowLevels(l, n);
    }

    virtual ~ImageSequenceReader_Impl()
    {}

    bool Open(const string& url) override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    bool Open(MediaParser::Holder hParser) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!hParser || !hParser->IsOpened())
        {
            m_errMsg = "Argument 'hParser' is nullptr or not opened yet!";
            return false;
        }

        if (IsOpened())
            Close();
        if (!hParser->IsImageSequence())
        {
            m_errMsg = "The parser is NOT opened for an image sequence!";
            return false;
        }
        m_hMediaInfo = hParser->GetMediaInfo();
        if (!m_hMediaInfo)
        {
            ostringstream oss; oss << "FAILED to get MediaInfo for MediaParser! Image sequence dir path is '" << m_hParser->GetUrl() << "'.";
            m_errMsg = oss.str();
            return false;
        }
        m_pVidstm = dynamic_cast<VideoStream*>(m_hMediaInfo->streams[0].get());
        m_frameRate = m_pVidstm->realFrameRate;
        m_vidTimeBase = {m_frameRate.den, m_frameRate.num};
        m_vidDurMts = m_pVidstm->duration*1000;
        m_vidfrmIntvPts = 1;

        m_hParser = hParser;
        m_opened = true;
        return true;
    }

    bool ConfigVideoReader(uint32_t outWidth, uint32_t outHeight, ImColorFormat outClrfmt, ImDataType outDtype, ImInterpolateMode rszInterp, HwaccelManager::Holder hHwaMgr) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This 'ImageSequenceReader' instance is NOT OPENED yet!";
            return false;
        }
        if (m_started)
        {
            m_errMsg = "This 'ImageSequenceReader' instance is ALREADY STARTED!";
            return false;
        }

        m_outWidth = outWidth;
        m_outHeight = outHeight;
        m_useSizeFactor = false;
        m_outClrFmt = outClrfmt;
        m_outDtype = outDtype;
        m_interpMode = rszInterp;
        for (auto i = 0; i < m_decWorkerCount; i++)
            m_decCtxs.push_back(DecodeImageContext::Holder(new DecodeImageContext(this)));

        m_configured = true;
        return true;
    }

    bool ConfigVideoReader(float outWidthFactor, float outHeightFactor, ImColorFormat outClrfmt, ImDataType outDtype, ImInterpolateMode rszInterp, HwaccelManager::Holder hHwaMgr) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "Can NOT configure a 'ImageSequenceReader' until it's been configured!";
            return false;
        }
        if (m_started)
        {
            m_errMsg = "Can NOT configure a 'ImageSequenceReader' after it's already started!";
            return false;
        }

        m_ssWFactor = outWidthFactor;
        m_ssHFactor = outHeightFactor;
        m_useSizeFactor = true;
        m_outClrFmt = outClrfmt;
        m_outDtype = outDtype;
        m_interpMode = rszInterp;
        for (auto i = 0; i < m_decWorkerCount; i++)
            m_decCtxs.push_back(DecodeImageContext::Holder(new DecodeImageContext(this)));

        m_configured = true;
        return true;
    }

    bool ConfigAudioReader(uint32_t outChannels, uint32_t outSampleRate, const string& outPcmFormat, uint32_t audioStreamIndex) override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    bool Start(bool suspend) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_configured)
        {
            m_errMsg = "This 'ImageSequenceReader' instance is NOT CONFIGURED yet!";
            return false;
        }
        if (m_started)
            return true;

        StartAllThreads();
        m_started = true;
        return true;
    }

    bool Stop() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_configured)
        {
            m_errMsg = "This 'ImageSequenceReader' instance is NOT CONFIGURED yet!";
            return false;
        }
        if (!m_started)
            return true;

        WaitAllThreadsQuit();
        FlushAllQueues();

        m_readPts = 0;
        m_prevReadResult = {0., nullptr};
        m_readForward = true;
        m_vidDurMts = 0;

        m_prepared = false;
        m_started = false;
        m_configured = false;
        m_errMsg = "";
        return true;
    }

    void Close() override
    {
        m_close = true;
        lock_guard<recursive_mutex> lk(m_apiLock);
        WaitAllThreadsQuit();
        m_decCtxs.clear();
        FlushAllQueues();

        m_hParser = nullptr;
        m_hMediaInfo = nullptr;
        m_readPts = 0;
        m_prevReadResult = {0., nullptr};
        m_readForward = true;
        m_vidDurMts = 0;
        if (m_pFrmCvt)
        {
            delete m_pFrmCvt;
            m_pFrmCvt = nullptr;
        }

        m_prepared = false;
        m_started = false;
        m_configured = false;
        m_opened = false;
        m_errMsg = "";
    }

    bool SeekTo(int64_t pos, bool bSeekingMode) override
    {
        if (!m_configured)
        {
            m_errMsg = "Can NOT use 'SeekTo' until the 'ImageSequenceReader' instance is configured!";
            return false;
        }
        if (pos < 0 || pos > m_vidDurMts)
        {
            m_errMsg = "INVALID argument 'pos'! Can NOT be negative or exceed the duration.";
            return false;
        }

        m_logger->Log(DEBUG) << "--> Seek[0]: Set seek pos " << pos << endl;
        if (m_bInSeekingMode != bSeekingMode)
        {
            if (!bSeekingMode) m_hSeekingFlash = nullptr;
            m_bInSeekingMode = bSeekingMode;
        }
        int64_t seekPts = CvtMtsToPts(pos);
        UpdateReadPts(seekPts);
        return true;
    }

    void SetDirection(bool forward) override
    {
        if (m_readForward == forward)
            return;
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This 'ImageSequenceReader' instance is NOT OPENED yet!";
            return;
        }
        m_readForward = forward;
    }

    void Suspend() override {}
    void Wakeup() override {}

    bool ReadVideoFrame(int64_t pos, ImGui::ImMat& m, bool& eof, bool wait) override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    VideoFrame::Holder ReadVideoFrame(int64_t pos, bool& eof, bool wait) override
    {
        if (!m_started)
        {
            m_errMsg = "This 'VideoReader' instance is NOT STARTED yet!";
            return nullptr;
        }
        if (!wait && !m_prepared)
        {
            eof = false;
            return nullptr;
        }
        while (!m_quitThread && !m_prepared && wait)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_close || !m_prepared)
        {
            m_errMsg = "This 'VideoReader' instance is NOT READY to read!";
            return nullptr;
        }

        lock_guard<recursive_mutex> lk(m_apiLock);
        eof = false;
        auto prevReadResult = m_prevReadResult;
        if (prevReadResult.second && pos == prevReadResult.first)
        {
            return prevReadResult.second;
        }

        int64_t pts = CvtMtsToPts(pos);
        return ReadVideoFrameByPts(pts, eof, wait);
    }

    VideoFrame::Holder ReadVideoFrameByPts(int64_t pts, bool& eof, bool wait)
    {
        int64_t pos = CvtPtsToMts(pts);
        if (pos < 0 || (pos >= m_vidDurMts))
        {
            m_errMsg = "Invalid argument! 'pos' can NOT be negative or larger than video's duration.";
            eof = true;
            return nullptr;
        }
        UpdateReadPts(pts);
        m_logger->Log(VERBOSE) << ">> TO READ frame: pts=" << pts << ", ts=" << pos << "." << endl;

        auto wait1 = GetTimePoint();
        auto wait0 = wait1;
        const int64_t hungupWarnInternal = 3000;
        const bool zeroCache = m_cacheFrameCount.first == 0 && m_cacheFrameCount.second == 0;
        VideoFrame::Holder hVfrm;
        while (!m_quitThread)
        {
            if (!zeroCache)
            {
                lock_guard<mutex> _lk(m_vfrmQLock);
                auto iter = find_if(m_vfrmQ.begin(), m_vfrmQ.end(), [pts] (auto& vf) {
                    return vf->Pts() > pts;
                });
                if (iter != m_vfrmQ.end())
                {
                    if (iter != m_vfrmQ.begin())
                        hVfrm = *(--iter);
                    else
                    {
                        auto pVf = dynamic_cast<VideoFrame_Impl*>(iter->get());
                        if (pVf->isStartFrame)
                            hVfrm = *iter;
                    }
                }
                else if (!m_vfrmQ.empty())
                {
                    auto pVf = dynamic_cast<VideoFrame_Impl*>(m_vfrmQ.back().get());
                    if (pts >= pVf->pts && pts < pVf->pts+pVf->dur || pVf->isEofFrame)
                        hVfrm = m_vfrmQ.back();
                }
            }
            else
            {
                lock_guard<mutex> _lk(m_vfrmQLock);
                auto iter = find_if(m_vfrmQ.begin(), m_vfrmQ.end(), [pts] (auto& vf) {
                    auto pVf = dynamic_cast<VideoFrame_Impl*>(vf.get());
                    return pts >= pVf->pts && pts < pVf->pts+pVf->dur || pVf->isEofFrame && pts >= pVf->pts;
                });
                if (iter != m_vfrmQ.end())
                    hVfrm = *iter;
            }
            if (hVfrm)
            {
                auto pVf = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
                if (!pVf->decodeStarted)
                    hVfrm = nullptr;
            }
            if (hVfrm || !wait)
                break;

            this_thread::sleep_for(chrono::milliseconds(2));
            auto wait2 = GetTimePoint();
            if (CountElapsedMillisec(wait1, wait2) > 3000)
            {
                wait1 = wait2;
                Log(WARN) << "ReadVideoFrame() Hung UP for " << (double)CountElapsedMillisec(wait0, wait2)/1000 << "seconds!" << endl;
            }
        }
        if (!hVfrm)
        {
            m_errMsg = "No suitable frame!";
            return nullptr;
        }
        auto pVf = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
        if (m_readForward && pVf->isEofFrame)
        {
            eof = true;
        }

        m_prevReadResult = {pos, hVfrm};
        return hVfrm;
    }

    VideoFrame::Holder ReadNextVideoFrame(bool& eof, bool wait) override
    {
        if (!m_started)
        {
            m_errMsg = "This 'VideoReader' instance is NOT STARTED yet!";
            return nullptr;
        }
        if (!wait && !m_prepared)
        {
            eof = false;
            return nullptr;
        }
        while (!m_quitThread && !m_prepared && wait)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_close || !m_prepared)
        {
            m_errMsg = "This 'VideoReader' instance is NOT READY to read!";
            return nullptr;
        }
        eof = false;

        lock_guard<recursive_mutex> lk(m_apiLock);
        int64_t i64CurrFramePts, i64NextFramePts;
        const auto& prevReadResult = m_prevReadResult;
        if (prevReadResult.second)
        {
            const auto pVf = dynamic_cast<VideoFrame_Impl*>(prevReadResult.second.get());
            if (m_readForward && pVf->isEofFrame || !m_readForward && pVf->isStartFrame)
            {
                eof = true;
                return nullptr;
            }
            i64CurrFramePts = pVf->pts;
        }
        else
            i64CurrFramePts = m_readPts;
        bool bFoundNextFrame = false;
        while (!m_quitThread)
        {
            {
                lock_guard<mutex> _lk(m_vfrmQLock);
                if (m_readForward)
                {
                    auto iter = find_if(m_vfrmQ.begin(), m_vfrmQ.end(), [i64CurrFramePts] (auto& vf) {
                        return vf->Pts() > i64CurrFramePts;
                    });
                    if (iter != m_vfrmQ.end())
                    {
                        i64NextFramePts = (*iter)->Pts();
                        bFoundNextFrame = true;
                        break;
                    }
                }
                else
                {
                    auto iter = find_if(m_vfrmQ.rbegin(), m_vfrmQ.rend(), [i64CurrFramePts] (auto& vf) {
                        return vf->Pts() < i64CurrFramePts;
                    });
                    if (iter != m_vfrmQ.rend())
                    {
                        i64NextFramePts = (*iter)->Pts();
                        bFoundNextFrame = true;
                        break;
                    }
                }
            }
            if (!wait)
                break;
            this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (!bFoundNextFrame)
            return nullptr;
        return ReadVideoFrameByPts(i64NextFramePts, eof, wait);
    }

    VideoFrame::Holder GetSeekingFlash() const override
    {
        return m_hSeekingFlash;
    }

    bool ReadAudioSamples(uint8_t* buf, uint32_t& size, int64_t& pos, bool& eof, bool wait) override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    bool ReadAudioSamples(ImGui::ImMat& m, uint32_t readSamples, bool& eof, bool wait) override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    bool IsOpened() const override
    {
        return m_opened;
    }

    bool IsStarted() const override
    {
        return m_started;
    }

    MediaParser::Holder GetMediaParser() const override
    {
        return m_hParser;
    }

    bool IsVideoReader() const override
    {
        return true;
    }

    bool IsDirectionForward() const override
    {
        return m_readForward;
    }

    bool IsSuspended() const override
    {
        return false;
    }

    bool IsPlanar() const override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    int64_t GetReadPos() const override
    {
        return m_readPts;
    }

    bool SetCacheDuration(double forwardDur, double backwardDur) override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    bool SetCacheFrames(bool readForward, uint32_t forwardFrames, uint32_t backwardFrames) override
    {
        m_cacheFrameCount.first = backwardFrames;
        m_cacheFrameCount.second = forwardFrames;
        return true;
    }

    pair<double, double> GetCacheDuration() const override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    bool IsHwAccelEnabled() const override
    {
        return m_vidPreferUseHw;
    }

    void EnableHwAccel(bool enable) override
    {
        m_vidPreferUseHw = enable;
    }

    bool ChangeVideoOutputSize(uint32_t outWidth, uint32_t outHeight, ImInterpolateMode rszInterp) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_prepared && m_pFrmCvt)
        {
            bool bNeedFlushVfrmQ = false;
            if (m_pFrmCvt->GetOutWidth() != outWidth || m_pFrmCvt->GetOutHeight() != outHeight)
            {
                if (!m_pFrmCvt->SetOutSize(outWidth, outHeight))
                {
                    ostringstream oss; oss << "FAILED to set output size to 'AVFrameToImMatConverter'! Error is '" << m_pFrmCvt->GetError() << "'.";
                    m_errMsg = oss.str();
                    return false;
                }
                bNeedFlushVfrmQ = true;
            }
            if (rszInterp == (ImInterpolateMode)-1)
            {
                if (outWidth*outHeight >= m_pVidstm->width*m_pVidstm->height)
                    rszInterp = IM_INTERPOLATE_BICUBIC;
                else
                    rszInterp = IM_INTERPOLATE_AREA;
            }
            if (m_pFrmCvt->GetResizeInterpolateMode() != rszInterp)
            {
                if (!m_pFrmCvt->SetResizeInterpolateMode(rszInterp))
                {
                    ostringstream oss; oss << "FAILED to set resize interp-mode to 'AVFrameToImMatConverter'! Error is '" << m_pFrmCvt->GetError() << "'.";
                    m_errMsg = oss.str();
                    return false;
                }
                bNeedFlushVfrmQ = true;
            }
            if (bNeedFlushVfrmQ)
            {
                lock_guard<mutex> lk2(m_vfrmQLock);
                FlushAllQueues();
            }
        }
        m_outWidth = outWidth;
        m_outHeight = outHeight;
        m_interpMode = rszInterp;
        return true;
    }

    bool ChangeAudioOutputFormat(uint32_t outChannels, uint32_t outSampleRate, const string& outPcmFormat) override
    {
        throw runtime_error("VideoReader does NOT SUPPORT method ChangeAudioOutputFormat()!");
    }

    MediaInfo::Holder GetMediaInfo() const override
    {
        return m_hMediaInfo;
    }

    const VideoStream* GetVideoStream() const override
    {
        return m_pVidstm;
    }

    const AudioStream* GetAudioStream() const override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    uint32_t GetVideoOutWidth() const override
    {
        uint32_t w = m_outWidth;
        if (w > 0)
            return w;
        const VideoStream* vidStream = GetVideoStream();
        if (!vidStream)
            return 0;
        w = vidStream->width;
        return w;
    }

    uint32_t GetVideoOutHeight() const override
    {
        uint32_t h = m_outHeight;
        if (h > 0)
            return h;
        const VideoStream* vidStream = GetVideoStream();
        if (!vidStream)
            return 0;
        h = vidStream->height;
        return h;
    }

    string GetAudioOutPcmFormat() const override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    uint32_t GetAudioOutChannels() const override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    uint32_t GetAudioOutSampleRate() const override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    uint32_t GetAudioOutFrameSize() const override
    {
        throw runtime_error("This interface is NOT SUPPORTED by ImageSequenceReader!");
    }

    void SetLogLevel(Level l) override
    {
        m_logger->SetShowLevels(l);
    }

    string GetError() const override
    {
        return m_errMsg;
    }

private:
    struct VideoFrame_Impl;

    struct DecodeImageContext
    {
        using Holder = shared_ptr<DecodeImageContext>;

        DecodeImageContext(ImageSequenceReader_Impl* _owner) : owner(_owner)
        {
            m_decThread = thread(&DecodeImageContext::DecodeImageProc, this);
        }

        ~DecodeImageContext()
        {
            quit = true;
            if (m_decThread.joinable())
                m_decThread.join();
            ReleaseDecoderContext();
            ReleaseFormatContext();
        }

        ImageSequenceReader_Impl* owner;
        thread m_decThread;
        string m_imagePath;
        AVFormatContext* m_avfmtCtx{nullptr};
        AVCodecContext* m_viddecCtx{nullptr};
        atomic_bool isBusy{false};
        VideoFrame_Impl* m_pVfrm{nullptr};
        mutex m_vfLock;
        VideoFrame::Holder m_hVfrm;
        bool quit{false};

        bool StartDecode(VideoFrame::Holder hVfrm)
        {
            VideoFrame_Impl* pVfrm = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
            if (pVfrm->imageFilePath.empty())
                return false;
            bool testVal = false;
            if (!isBusy.compare_exchange_strong(testVal, true))
                return false;
            m_imagePath = pVfrm->imageFilePath;
            pVfrm->decodeStarted = true;
            {
                lock_guard<mutex> lk(m_vfLock);
                m_pVfrm = pVfrm;
                m_hVfrm = hVfrm;
            }
            return true;
        }

        void ReleaseFormatContext()
        {
            if (m_avfmtCtx)
            {
                avformat_close_input(&m_avfmtCtx);
                m_avfmtCtx = nullptr;
            }
        }

        void ReleaseDecoderContext()
        {
            if (m_viddecCtx)
            {
                avcodec_free_context(&m_viddecCtx);
                m_viddecCtx = nullptr;
            }
        }

        bool DecodeImageFile(const string& filePath)
        {
            int fferr = avformat_open_input(&m_avfmtCtx, filePath.c_str(), nullptr, nullptr);
            if (fferr < 0 || !m_avfmtCtx)
            {
                m_avfmtCtx = nullptr;
                owner->m_logger->Log(Error) << "FAILED to invoke 'avformat_open_input' on file '" << filePath << "'! fferr=" << fferr << "." << endl;
                return false;
            }
            fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
            if (fferr < 0)
            {
                owner->m_logger->Log(Error) << "FAILED to invoke 'avformat_find_stream_info' on file '" << filePath << "'! fferr=" << fferr << "." << endl;
                return false;
            }
            auto vidstmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            if (vidstmIdx < 0)
            {
                owner->m_logger->Log(Error) << "Can not find any video stream in file '" << filePath << "'!";
                return false;
            }
            AVStream* pVidstm = m_avfmtCtx->streams[vidstmIdx];
            if (m_viddecCtx)
            {
                bool recreateViddec = false;
                if (m_viddecCtx->codec_id != pVidstm->codecpar->codec_id)
                {
                    owner->m_logger->Log(WARN) << "Img-sq file '" << filePath << "' has different codec_id (" << pVidstm->codecpar->codec_id
                            << ") against the 1st image's codec_id (" << m_viddecCtx->codec_id << ")!" << endl;
                    recreateViddec = true;
                }
                if (m_viddecCtx->width != pVidstm->codecpar->width || m_viddecCtx->height != pVidstm->codecpar->height)
                {
                    owner->m_logger->Log(WARN) << "Img-sq file '" << filePath << "' has different resolution (" << pVidstm->codecpar->width << "x" << pVidstm->codecpar->height
                            << ") against the 1st image's size (" << m_viddecCtx->width << "x" << m_viddecCtx->height << ")!" << endl;
                    recreateViddec = true;
                }
                if (recreateViddec)
                {
                    avcodec_free_context(&m_viddecCtx);
                    m_viddecCtx = nullptr;
                }
            }
            if (!m_viddecCtx)
            {
                FFUtils::OpenVideoDecoderOptions viddecOpenOpts = owner->m_viddecOpenOpts;
                FFUtils::OpenVideoDecoderResult res;
                if (FFUtils::OpenVideoDecoder(m_avfmtCtx, vidstmIdx, &viddecOpenOpts, &res, false))
                {
                    m_viddecCtx = res.decCtx;
                    AVHWDeviceType hwDevType = res.hwDevType;
                    owner->m_logger->Log(DEBUG) << "Opened video decoder '" << m_viddecCtx->codec->name << "'("
                            << (hwDevType==AV_HWDEVICE_TYPE_NONE ? "SW" : av_hwdevice_get_type_name(hwDevType)) << ")" << " for img-sq file '" << filePath << "'." << endl;
                }
                else
                {
                    owner->m_logger->Log(Error) << "FAILED to open video decoder for img-sq file '" << filePath << "'! Error is '" << res.errMsg << "'." << endl;
                    return false;
                }
            }
            else
            {
                avcodec_flush_buffers(m_viddecCtx);
            }

            SelfFreeAVPacketPtr ptrPkt = AllocSelfFreeAVPacketPtr();
            bool requireAvpkt = true;
            bool avpktReady = false;
            bool demuxEof = false;
            bool nullpktSent = false;
            SelfFreeAVFramePtr ptrFrm = AllocSelfFreeAVFramePtr();
            bool vidfrmReady = false;
            while (!quit)
            {
                if (requireAvpkt && !demuxEof)
                {
                    av_packet_unref(ptrPkt.get());
                    fferr = av_read_frame(m_avfmtCtx, ptrPkt.get());
                    if (fferr == 0)
                    {
                        if (ptrPkt->stream_index == vidstmIdx)
                        {
                            requireAvpkt = false;
                            avpktReady = true;
                        }
                    }
                    else if (fferr == AVERROR_EOF)
                    {
                        demuxEof = true;
                    }
                    else
                    {
                        owner->m_logger->Log(WARN) << "FAILED to invoke 'av_read_frame' on file '" << filePath << "'! fferr=" << fferr << "." << endl;
                        demuxEof = true;
                    }
                }

                if (avpktReady || demuxEof && !nullpktSent)
                {
                    if (!demuxEof)
                        fferr = avcodec_send_packet(m_viddecCtx, ptrPkt.get());
                    else
                    {
                        fferr = avcodec_send_packet(m_viddecCtx, NULL);
                        nullpktSent = true;
                    }
                    if (fferr == 0)
                    {
                        avpktReady = false;
                        requireAvpkt = true;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        owner->m_logger->Log(WARN) << "FAILED to invoke 'avcodec_send_packet' on file '" << filePath << "'! fferr=" << fferr << "." << endl;
                    }
                }

                fferr = avcodec_receive_frame(m_viddecCtx, ptrFrm.get());
                if (fferr == 0)
                {
                    vidfrmReady = true;
                    break;
                }
                else if (fferr != AVERROR(EAGAIN))
                {
                    owner->m_logger->Log(WARN) << "FAILED to invoke 'avcodec_receive_frame' on file '" << filePath << "'! fferr=" << fferr << "." << endl;
                }
            }
            if (!vidfrmReady && !quit)
            {
                owner->m_logger->Log(Error) << "FAILED to decode picture out of img-sq file '" << filePath << "'!" << endl;
                return false;
            }

            if (vidfrmReady)
            {
                lock_guard<mutex> lk(m_vfLock);
                if (m_pVfrm)
                {
                    m_pVfrm->isHwfrm = IsHwFrame(ptrFrm.get());
                    m_pVfrm->frmPtr = ptrFrm;
                }
                else
                    owner->m_logger->Log(DEBUG) << "'pVfrm' is NULL when setting 'frmPtr' to it." << endl;
            }
            return true;
        }

        void UnlinkVideoFrame(VideoFrame_Impl* pVfrm)
        {
            lock_guard<mutex> lk(m_vfLock);
            if (m_pVfrm == pVfrm)
            {
                m_pVfrm = nullptr;
                m_hVfrm = nullptr;
            }
        }

        void DecodeImageProc()
        {
            while (!quit)
            {
                bool idleLoop = true;

                if (!m_imagePath.empty())
                {
                    if (DecodeImageFile(m_imagePath))
                    {
                        owner->m_logger->Log(VERBOSE) << "--> Imgsq decode done. '" << m_imagePath << "'" << endl;
                        idleLoop = false;
                    }
                    else
                    {
                        lock_guard<mutex> lk(m_vfLock);
                        if (m_pVfrm)
                            m_pVfrm->decodeFailed = true;
                    }
                    ReleaseFormatContext();
                    m_imagePath.clear();
                    isBusy = false;
                }

                if (idleLoop)
                    this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
            }
        }
    };

    struct VideoFrame_Impl : public VideoFrame
    {
    public:
        VideoFrame_Impl(ImageSequenceReader_Impl* _owner, int64_t _pos, int64_t _pts, int64_t _dur)
            : owner(_owner), pos(_pos), pts(_pts), dur(_dur)
        {}

        virtual ~VideoFrame_Impl()
        {
            if (hDecCtx)
            {
                hDecCtx->UnlinkVideoFrame(this);
                hDecCtx = nullptr;
            }
        }

        bool GetMat(ImGui::ImMat& m) override
        {
            if (!vmat.empty())
            {
                m = vmat;
                return true;
            }
            if (!frmPtr)
            {
                while (!frmPtr && !decodeFailed && (!discarded || decodeStarted))
                {
                    this_thread::sleep_for(chrono::milliseconds(2));
                }
                if (!frmPtr)
                    return false;
            }

            // acquire the lock of 'frmPtr'
            while (!owner->m_quitThread)
            {
                bool testVal = false;
                if (frmPtrInUse.compare_exchange_strong(testVal, true))
                    break;
                this_thread::sleep_for(chrono::milliseconds(5));
            }
            if (owner->m_quitThread)
                return false;

            // avframe -> ImMat
            double ts = (double)pos/1000;
            if (!owner->m_pFrmCvt->ConvertImage(frmPtr.get(), vmat, ts))
            {
                owner->m_logger->Log(Error) << "AVFrameToImMatConverter::ConvertImage() FAILED at pos " << pos << "(" << pts
                        << ")! Error is '" << owner->m_pFrmCvt->GetError() << "'." << endl;
            }
            frmPtr = nullptr;
            isHwfrm = false;
            frmPtrInUse = false;

            if (vmat.empty())
                return false;
            m = vmat;
            return true;
        }

        int64_t Pos() const override { return pos; }
        int64_t Pts() const override { return pts; }
        int64_t Dur() const override { return dur; }
        float Opacity() const override { return m_fOpacity; }
        void SetOpacity(float opacity) override { m_fOpacity = opacity; }
        void SetAutoConvertToMat(bool enable) override
        { bAutoCvtToMat = enable; }

        void Discard()
        {
            discarded = true;
            hDecCtx = nullptr;
        }

        bool IsReady() const override { return !vmat.empty() || decodeFailed; }

        NativeData GetNativeData() const override
        {
            if (frmPtr)
                return { NativeData::AVFRAME_HOLDER, (void*)&frmPtr };
            else if (!vmat.empty())
                return { NativeData::MAT, (void*)&vmat };
            else
                return { NativeData::UNKNOWN, nullptr };
        }

        ImageSequenceReader_Impl* owner;
        SelfFreeAVFramePtr frmPtr;
        ImGui::ImMat vmat;
        int64_t pos;
        int64_t pts;
        int64_t dur{0};
        float m_fOpacity{1.f};
        bool isHwfrm{false};
        bool isEofFrame{false};
        bool isStartFrame{false};
        atomic_bool frmPtrInUse{false};
        bool bAutoCvtToMat{false};
        DecodeImageContext::Holder hDecCtx;
        bool decodeStarted{false};
        bool decodeFailed{false};
        bool discarded{false};
        string imageFilePath;
    };

    static const function<void (VideoFrame*)> IMGSQ_READER_VIDEO_FRAME_HOLDER_DELETER;

    void UpdateReadPts(int64_t readPts)
    {
        lock_guard<mutex> _lk(m_cacheRangeLock);
        m_readPts = readPts;
        const auto cacheFrameCount = m_bInSeekingMode ? pair<int32_t, int32_t>(0, 0) : m_cacheFrameCount;
        if (m_readForward)
        {
            m_cacheRange.first = readPts-cacheFrameCount.first*m_vidfrmIntvPts;
            m_cacheRange.second = readPts+cacheFrameCount.second*m_vidfrmIntvPts;
        }
        else
        {
            m_cacheRange.first = readPts-cacheFrameCount.second*m_vidfrmIntvPts;
            m_cacheRange.second = readPts+cacheFrameCount.first*m_vidfrmIntvPts;
        }
        if (m_vidfrmIntvPts > 1)
        {
            m_cacheRange.first--;
            m_cacheRange.second++;
        }
    }

    int64_t CvtMtsToPts(int64_t mts)
    {
        return av_rescale_q_rnd(mts, MILLISEC_TIMEBASE, m_vidTimeBase, AV_ROUND_DOWN);
    }

    int64_t CvtPtsToMts(int64_t pts)
    {
        return av_rescale_q_rnd(pts, m_vidTimeBase, MILLISEC_TIMEBASE, AV_ROUND_DOWN);
    }

    void StartAllThreads()
    {
        string fileName = SysUtils::ExtractFileName(m_hParser->GetUrl());
        ostringstream thnOss;
        m_quitThread = false;
        m_rdimgThdRunning = true;
        m_readImageThread = thread(&ImageSequenceReader_Impl::ReadImageThreadProc, this);
        thnOss << "RdimgTh-" << fileName;
        SysUtils::SetThreadName(m_readImageThread, thnOss.str());
        m_cnvThdRunning = true;
        m_cnvMatThread = thread(&ImageSequenceReader_Impl::ConvertMatThreadProc, this);
        thnOss.str(""); thnOss << "VrdrCmt-" << fileName;
        SysUtils::SetThreadName(m_cnvMatThread, thnOss.str());
    }

    void WaitAllThreadsQuit(bool callFromReleaseProc = false)
    {
        m_quitThread = true;
        if (m_readImageThread.joinable())
        {
            m_readImageThread.join();
            m_readImageThread = thread();
        }
        if (m_cnvMatThread.joinable())
        {
            m_cnvMatThread.join();
            m_cnvMatThread = thread();
        }
    }

    void FlushAllQueues()
    {
        for (const auto& hVfrm : m_vfrmQ)
        {
            VideoFrame_Impl* pVfrm = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
            pVfrm->Discard();
        }
        m_vfrmQ.clear();
    }

    bool Prepare()
    {
        bool locked = false;
        do {
            locked = m_apiLock.try_lock();
            if (!locked)
                this_thread::sleep_for(chrono::milliseconds(5));
        } while (!locked && !m_quitThread);
        if (m_quitThread)
        {
            m_logger->Log(WARN) << "Abort 'Prepare' procedure! 'm_quitThread' is set!" << endl;
            return false;
        }

        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        m_hFileIter = m_hParser->GetImageSequenceIterator()->Clone();

        if (!m_pFrmCvt)
        {
            m_pFrmCvt = new AVFrameToImMatConverter();
            if (!m_pFrmCvt)
            {
                m_errMsg = "FAILED to allocate new 'AVFrameToImMatConverter' instance!";
                return false;
            }
            if (m_useSizeFactor)
            {
                auto u32OutWidth = (uint32_t)ceil(m_pVidstm->width*m_ssWFactor);
                if ((u32OutWidth&0x1) == 1)
                    u32OutWidth++;
                auto u32OutHeight = (uint32_t)ceil(m_pVidstm->height*m_ssHFactor);
                if ((u32OutHeight&0x1) == 1)
                    u32OutHeight++;
                m_outWidth = m_outHeight = 0;
                if (!ChangeVideoOutputSize(u32OutWidth, u32OutHeight, m_interpMode))
                    return false;
            }
            if (!m_pFrmCvt->SetOutColorFormat(m_outClrFmt))
            {
                m_errMsg = m_pFrmCvt->GetError();
                return false;
            }
            if (!m_pFrmCvt->SetOutDataType(m_outDtype))
            {
                m_errMsg = m_pFrmCvt->GetError();
                return false;
            }
        }

        m_prepared = true;
        return true;
    }

    void ReadImageThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter ReadImageThreadProc()..." << endl;

        bool prepareFailed = !m_prepared && !Prepare();
        if (prepareFailed)
        {
            m_logger->Log(Error) << "Prepare() FAILED! Error is '" << m_errMsg << "'." << endl;
            return;
        }

        while (!m_quitThread)
        {
            bool idleLoop = true;

            pair<int64_t, int64_t> cacheRange;
            {
                lock_guard<mutex> lk(m_cacheRangeLock);
                cacheRange = m_cacheRange;
            }

            const uint32_t frontFileIndex = cacheRange.first <= 0 ? 0 : (uint32_t)cacheRange.first;
            uint32_t endFileIndex = cacheRange.second <= 0 ? 0 : (uint32_t)cacheRange.second;
            if (endFileIndex >= m_hFileIter->GetValidFileCount()) endFileIndex = m_hFileIter->GetValidFileCount()-1;
            list<VideoFrame::Holder> undecodedFrames;
            const bool readForward = m_readForward;
            {
                lock_guard<mutex> lk(m_vfrmQLock);
                for (const auto& hVfrm : m_vfrmQ)
                {
                    VideoFrame_Impl* pVfrm = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
                    const auto pts = pVfrm->pts;
                    if ((pts < frontFileIndex || pts > endFileIndex) && !pVfrm->discarded && !pVfrm->decodeStarted)
                        undecodedFrames.push_back(hVfrm);
                }
                list<VideoFrame::Holder> inCacheRangeFrames;
                for (uint32_t i = frontFileIndex; i <= endFileIndex; i++)
                {
                    auto iter = find_if(m_vfrmQ.begin(), m_vfrmQ.end(), [i] (auto& hFrm) {
                        return hFrm->Pts() >= i;
                    });
                    if (iter == m_vfrmQ.end() || (*iter)->Pts() > i)
                    {
                        auto pVfrmImpl = new VideoFrame_Impl(this, CvtPtsToMts(i), i, m_vidfrmIntvPts);
                        VideoFrame::Holder hVfrm(pVfrmImpl, IMGSQ_READER_VIDEO_FRAME_HOLDER_DELETER);
                        if (i == 0)
                            pVfrmImpl->isStartFrame = true;
                        if (i == m_hFileIter->GetValidFileCount()-1)
                            pVfrmImpl->isEofFrame = true;
                        m_vfrmQ.insert(iter, hVfrm);
                        if (readForward)
                            inCacheRangeFrames.push_back(hVfrm);
                        else
                            inCacheRangeFrames.push_front(hVfrm);
                    }
                    else
                    {
                        auto hVfrm = *iter;
                        VideoFrame_Impl* pVfrm = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
                        if (!pVfrm->hDecCtx)
                        {
                            if (readForward)
                                inCacheRangeFrames.push_back(hVfrm);
                            else
                                inCacheRangeFrames.push_front(hVfrm);
                        }
                    }
                }
                undecodedFrames.splice(undecodedFrames.begin(), inCacheRangeFrames);
            }

            for (auto& hVfrm : undecodedFrames)
            {
                VideoFrame_Impl* pVfrm = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
                const uint32_t fileIndex = (uint32_t)pVfrm->pts;
                if (pVfrm->imageFilePath.empty())
                {
                    if (!m_hFileIter->SeekToValidFile(fileIndex))
                    {
                        m_logger->Log(Error) << "FAILED to seek to img-sq file index " << fileIndex << "." << endl;
                        continue;
                    }
                    auto filePath = m_hFileIter->GetCurrFilePath();
                    if (filePath.empty())
                    {
                        m_logger->Log(Error) << "FAILED to get the img-sq file path by index " << fileIndex << "." << endl;
                        continue;
                    }
                    pVfrm->imageFilePath = m_hFileIter->JoinBaseDirPath(filePath);
                }
                for (auto& hDecCtx : m_decCtxs)
                {
                    if (!hDecCtx->isBusy)
                    {
                        auto pCurrVfrm = hDecCtx->m_pVfrm;
                        if (pCurrVfrm && !pCurrVfrm->decodeFailed)
                        {
                            m_hSeekingFlash = hDecCtx->m_hVfrm;
                            m_logger->Log(VERBOSE) << "Update seeking flash at " << m_hSeekingFlash->Pos() << endl;
                        }
                        pVfrm->hDecCtx = hDecCtx;
                        m_logger->Log(DEBUG) << "-> StartDecode[idx=" << fileIndex << ", pos=" << pVfrm->pos << "]: '" << pVfrm->imageFilePath << "'" << endl;
                        hDecCtx->StartDecode(hVfrm);
                        idleLoop = false;
                        break;
                    }
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
        }
        m_rdimgThdRunning = false;
        m_logger->Log(DEBUG) << "Leave ReadImageThreadProc()." << endl;
    }

    void ConvertMatThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter ConvertMatThreadProc()..." << endl;
        while (!m_prepared && !m_quitThread)
            this_thread::sleep_for(chrono::milliseconds(5));

        while (!m_quitThread)
        {
            bool idleLoop = true;

            // remove unused frames and find the next frame needed to do the conversion
            VideoFrame::Holder hVfrm;
            {
                lock_guard<mutex> _lk(m_vfrmQLock);
                auto iter = m_vfrmQ.begin();
                while (iter != m_vfrmQ.end())
                {
                    VideoFrame_Impl* pVf = dynamic_cast<VideoFrame_Impl*>(iter->get());
                    bool remove = false;
                    if (pVf->pts+pVf->dur < m_cacheRange.first)
                    {
                        if (m_readForward && (!pVf->isEofFrame || m_vfrmQ.size() > 1))
                        {
                            // m_logger->Log(VERBOSE) << "   --------- Set remove=true : pVf->pts(" << pVf->pts << ")+pVf->dur(" << pVf->dur << ") < cacheRange.first(" << m_cacheRange.first
                            //         << "), readForward=" << m_readForward << ", isEofFrame=" << pVf->isEofFrame << ", vfrmQ.size=" << m_vfrmQ.size() << endl;
                            remove = true;
                        }
                    }
                    else if (pVf->pts > m_cacheRange.second)
                    {
                        // m_logger->Log(VERBOSE) << "   --------- Set remove=true : pVf->pts(" << pVf->pts << ") > cacheRange.second(" << m_cacheRange.second << ")" << endl;
                        remove = true;
                    }
                    if (remove)
                    {
                        m_logger->Log(VERBOSE) << "   --------- Remove video frame: pts=" << pVf->pts << ", pos=" << pVf->pos << "." << endl;
                        pVf->Discard();
                        iter = m_vfrmQ.erase(iter);
                        continue;
                    }
                    if (!hVfrm && (pVf->isHwfrm || (pVf->bAutoCvtToMat && pVf->frmPtr)))
                        hVfrm = *iter;
                    iter++;
                }
            }

            // transfer hardware frame to software frame, to reduce the count of frames referenced from decoder
            if (hVfrm)
            {
                VideoFrame_Impl* pVf = dynamic_cast<VideoFrame_Impl*>(hVfrm.get());
                // acquire the lock of 'frmPtr'
                while (!m_quitThread)
                {
                    bool testVal = false;
                    if (pVf->frmPtrInUse.compare_exchange_strong(testVal, true))
                        break;
                    this_thread::sleep_for(chrono::milliseconds(5));
                }

                if (pVf->isHwfrm)
                {
                    if (!m_quitThread && pVf->frmPtr)
                    {
                        SelfFreeAVFramePtr swfrm = AllocSelfFreeAVFramePtr();
                        if (!TransferHwFrameToSwFrame(swfrm.get(), pVf->frmPtr.get()))
                        {
                            m_logger->Log(Error) << "TransferHwFrameToSwFrame() FAILED at pos " << pVf->pos << "(" << pVf->pts << ")! Discard this frame." << endl;
                            pVf->frmPtr = nullptr;
                            lock_guard<mutex> _lk(m_vfrmQLock);
                            auto iter = find(m_vfrmQ.begin(), m_vfrmQ.end(), hVfrm);
                            if (iter != m_vfrmQ.end())
                            {
                                pVf->Discard();
                                m_vfrmQ.erase(iter);
                            }
                        }
                        else
                        {
                            pVf->frmPtr = swfrm;
                        }
                    }
                    pVf->isHwfrm = false;
                }
                if (pVf->frmPtr && pVf->bAutoCvtToMat)
                {
                    // avframe -> ImMat
                    double ts = (double)pVf->pos/1000;
                    ImGui::ImMat vmat;
                    if (!m_pFrmCvt->ConvertImage(pVf->frmPtr.get(), vmat, ts))
                    {
                        m_logger->Log(Error) << "AVFrameToImMatConverter::ConvertImage() FAILED at pos " << pVf->pos << "(" << pVf->pts
                                << ")! Error is '" << m_pFrmCvt->GetError() << "'." << endl;
                    }
                    else
                    {
                        pVf->vmat = vmat;
                    }
                    pVf->frmPtr = nullptr;
                }

                pVf->frmPtrInUse = false;
                idleLoop = false;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
        }
        m_cnvThdRunning = false;
        m_logger->Log(DEBUG) << "Leave ConvertMatThreadProc()." << endl;
    }

private:
    ALogger* m_logger;
    string m_errMsg;

    recursive_mutex m_apiLock;
    bool m_opened{false};
    bool m_started{false};
    bool m_configured{false};
    bool m_prepared{false};
    bool m_close{false};
    bool m_readForward{true};

    bool m_quitThread{false};
    thread m_readImageThread;
    bool m_rdimgThdRunning{false};
    thread m_cnvMatThread;
    bool m_cnvThdRunning{false};

    MediaParser::Holder m_hParser;
    MediaInfo::Holder m_hMediaInfo;
    VideoStream* m_pVidstm{nullptr};
    int64_t m_vidDurMts{0};
    int64_t m_vidfrmIntvPts{1};
    Ratio m_frameRate;
    AVRational m_vidTimeBase;

    SysUtils::FileIterator::Holder m_hFileIter;
    list<VideoFrame::Holder> m_vfrmQ;
    mutex m_vfrmQLock;
    pair<int64_t, VideoFrame::Holder> m_prevReadResult;
    int64_t m_readPts{0};
    pair<int64_t, int64_t> m_cacheRange;
    pair<int32_t, int32_t> m_cacheFrameCount{0, 1};
    mutex m_cacheRangeLock;
    bool m_bInSeekingMode{false};
    VideoFrame::Holder m_hSeekingFlash;

    list<DecodeImageContext::Holder> m_decCtxs;
    uint8_t m_decWorkerCount{4};
    bool m_vidPreferUseHw{true};
    FFUtils::OpenVideoDecoderOptions m_viddecOpenOpts;
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};

    uint32_t m_outWidth{0}, m_outHeight{0};
    float m_ssWFactor{1.f}, m_ssHFactor{1.f};
    bool m_useSizeFactor{false};
    ImColorFormat m_outClrFmt;
    ImDataType m_outDtype;
    ImInterpolateMode m_interpMode;
    AVFrameToImMatConverter* m_pFrmCvt{nullptr};
};

const function<void (VideoFrame*)> ImageSequenceReader_Impl::IMGSQ_READER_VIDEO_FRAME_HOLDER_DELETER = [] (VideoFrame* p) {
    ImageSequenceReader_Impl::VideoFrame_Impl* ptr = dynamic_cast<ImageSequenceReader_Impl::VideoFrame_Impl*>(p);
    delete ptr;
};

static const auto IMAGE_SEQUENCE_READER_HOLDER_DELETER = [] (MediaReader* p) {
    ImageSequenceReader_Impl* ptr = dynamic_cast<ImageSequenceReader_Impl*>(p);
    ptr->Close();
    delete ptr;
};

MediaReader::Holder MediaReader::CreateImageSequenceInstance(const string& loggerName)
{
    return MediaReader::Holder(new ImageSequenceReader_Impl(loggerName), IMAGE_SEQUENCE_READER_HOLDER_DELETER);
}
}
