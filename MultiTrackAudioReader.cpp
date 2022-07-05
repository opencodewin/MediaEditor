#include <sstream>
#include <thread>
#include <mutex>
#include <list>
#include <algorithm>
#include "AudioTrack.h"
#include "MultiTrackAudioReader.h"
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/avutil.h"
    #include "libavutil/avstring.h"
    #include "libavutil/pixdesc.h"
    #include "libavformat/avformat.h"
    #include "libavcodec/avcodec.h"
    #include "libavdevice/avdevice.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"
}

using namespace std;
using namespace Logger;
using namespace DataLayer;

class MultiTrackAudioReader_Impl : public MultiTrackAudioReader
{
public:
    static ALogger* s_logger;

    MultiTrackAudioReader_Impl()
    {
        m_logger = GetMultiTrackAudioReaderLogger();
    }

    MultiTrackAudioReader_Impl(const MultiTrackAudioReader_Impl&) = delete;
    MultiTrackAudioReader_Impl(MultiTrackAudioReader_Impl&&) = delete;
    MultiTrackAudioReader_Impl& operator=(const MultiTrackAudioReader_Impl&) = delete;

    virtual ~MultiTrackAudioReader_Impl() {}

    bool Configure(uint32_t outChannels, uint32_t outSampleRate, uint32_t outSamplesPerFrame) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            m_errMsg = "This MultiTrackAudioReader instance is already started!";
            return false;
        }

        Close();

        m_outChannels = outChannels;
        m_outSampleRate = outSampleRate;
        m_outChannelLayout = av_get_default_channel_layout(outChannels);
        m_outSamplesPerFrame = outSamplesPerFrame;
        m_samplePos = 0;
        m_readPos = 0;
        m_blockSize = outChannels*4;  // for now, output sample format only supports float32 data type, thus 4 bytes per sample.

        m_configured = true;
        return true;
    }

    MultiTrackAudioReader* CloneAndConfigure(uint32_t outChannels, uint32_t outSampleRate, uint32_t outSamplesPerFrame) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        MultiTrackAudioReader_Impl* newInstance = new MultiTrackAudioReader_Impl();
        if (!newInstance->Configure(outChannels, outSampleRate, outSamplesPerFrame))
        {
            m_errMsg = newInstance->GetError();
            newInstance->Close(); delete newInstance;
            return nullptr;
        }

        lock_guard<recursive_mutex> lk2(m_trackLock);
        // clone all the tracks
        for (auto track : m_tracks)
        {
            newInstance->m_tracks.push_back(track->Clone(outChannels, outSampleRate));
        }
        newInstance->UpdateDuration();
        // create mixer in the new instance
        if (!newInstance->CreateMixer())
        {
            m_errMsg = newInstance->GetError();
            newInstance->Close(); delete newInstance;
            return nullptr;
        }

        // seek to 0
        newInstance->m_outputMats.clear();
        newInstance->m_samplePos = 0;
        newInstance->m_readPos = 0;
        for (auto track : newInstance->m_tracks)
            track->SeekTo(0);

        // start new instance
        if (!newInstance->Start())
        {
            m_errMsg = newInstance->GetError();
            newInstance->Close(); delete newInstance;
            return nullptr;
        }
        return newInstance;
    }

    bool Start() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            return true;
        }
        if (!m_configured)
        {
            m_errMsg = "This MultiTrackAudioReader instance is NOT configured yet!";
            return false;
        }

        StartMixingThread();

        m_started = true;
        return true;
    }

    void Close() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        TerminateMixingThread();

        ReleaseMixer();
        m_tracks.clear();
        m_outputMats.clear();
        m_configured = false;
        m_started = false;
        m_outChannels = 0;
        m_outSampleRate = 0;
        m_outSamplesPerFrame = 1024;
    }

    AudioTrackHolder AddTrack(int64_t trackId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackAudioReader instance is NOT started yet!";
            return nullptr;
        }

        TerminateMixingThread();

        AudioTrackHolder hTrack(new AudioTrack(trackId, m_outChannels, m_outSampleRate));
        hTrack->SetDirection(m_readForward);
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            m_tracks.push_back(hTrack);
            UpdateDuration();
        }
        m_outputMats.clear();

        ReleaseMixer();
        if (!CreateMixer())
            return nullptr;

        int64_t pos = m_samplePos*1000/m_outSampleRate;
        for (auto track : m_tracks)
            track->SeekTo(pos);

        StartMixingThread();
        return hTrack;
    }

    AudioTrackHolder RemoveTrackByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackAudioReader instance is NOT started yet!";
            return nullptr;
        }
        if (index >= m_tracks.size())
        {
            m_errMsg = "Invalid value for argument 'index'!";
            return nullptr;
        }

        TerminateMixingThread();

        auto iter = m_tracks.begin();
        while (index > 0)
        {
            iter++;
            index--;
        }
        auto delTrack = *iter;
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            m_tracks.erase(iter);
            UpdateDuration();
        }
        m_outputMats.clear();

        ReleaseMixer();
        if (!m_tracks.empty())
        {
            if (!CreateMixer())
                return nullptr;
        }

        for (auto track : m_tracks)
            track->SeekTo(ReadPos());

        StartMixingThread();
        return delTrack;
    }

    AudioTrackHolder RemoveTrackById(int64_t trackId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return nullptr;
        }

        lock_guard<recursive_mutex> lk2(m_trackLock);
        auto iter = find_if(m_tracks.begin(), m_tracks.end(), [trackId] (const AudioTrackHolder& track) {
            return track->Id() == trackId;
        });
        if (iter == m_tracks.end())
            return nullptr;

        TerminateMixingThread();

        auto delTrack = *iter;
        m_tracks.erase(iter);
        UpdateDuration();
        m_outputMats.clear();

        ReleaseMixer();
        if (!m_tracks.empty())
        {
            if (!CreateMixer())
                return nullptr;
        }

        for (auto track : m_tracks)
            track->SeekTo(ReadPos());

        StartMixingThread();
        return delTrack;
    }

    bool SetDirection(bool forward) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_readForward == forward)
            return true;

        TerminateMixingThread();

        m_readForward = forward;
        for (auto& track : m_tracks)
            track->SetDirection(forward);

        int64_t readPos = ReadPos();
        for (auto track : m_tracks)
            track->SeekTo(readPos);
        m_samplePos = readPos*m_outSampleRate/1000;

        m_outputMats.clear();
        ReleaseMixer();
        if (!m_tracks.empty())
        {
            if (!CreateMixer())
                return false;
        }

        StartMixingThread();
        return true;
    }

    bool SeekTo(int64_t pos) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackAudioReader instance is NOT started yet!";
            return false;
        }
        if (pos < 0)
        {
            m_errMsg = "INVALID argument! 'pos' must in the range of [0, Duration()].";
            return false;
        }

        TerminateMixingThread();

        m_outputMats.clear();
        m_samplePos = pos*m_outSampleRate/1000;
        m_readPos = pos;
        for (auto track : m_tracks)
            track->SeekTo(pos);

        StartMixingThread();
        return true;
    }

    bool ReadAudioSamples(ImGui::ImMat& amat, bool& eof) override
    {
        eof = false;
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackAudioReader instance is NOT started yet!";
            return false;
        }
        amat.release();
        if (m_tracks.empty())
            return false;

        m_outputMatsLock.lock();
        while (m_outputMats.empty() && !m_eof && !m_quit)
        {
            m_outputMatsLock.unlock();
            this_thread::sleep_for(chrono::milliseconds(5));
            m_outputMatsLock.lock();
        }
        lock_guard<mutex> lk2(m_outputMatsLock, adopt_lock);
        if (m_quit)
        {
            m_errMsg = "This 'MultiTrackAudioReader' instance is quit.";
            return false;
        }
        if (m_outputMats.empty() && m_eof)
        {
            eof = true;
            return false;
        }

        amat = m_outputMats.front();
        m_outputMats.pop_front();
        m_readPos += (int64_t)amat.w*1000/m_outSampleRate;
        return true;
    }

    bool Refresh() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            UpdateDuration();
        }

        SeekTo(ReadPos());
        return true;
    }

    int64_t SizeToDuration(uint32_t sizeInByte) override
    {
        if (!m_configured)
            return -1;
        uint32_t sampleCnt = sizeInByte/m_blockSize;
        return av_rescale_q(sampleCnt, {1, (int)m_outSampleRate}, MILLISEC_TIMEBASE);
    }

    uint32_t TrackCount() const override
    {
        return m_tracks.size();
    }

    list<AudioTrackHolder>::iterator TrackListBegin() override
    {
        return m_tracks.begin();
    }

    list<AudioTrackHolder>::iterator TrackListEnd() override
    {
        return m_tracks.end();
    }

    AudioTrackHolder GetTrackByIndex(uint32_t idx) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (idx >= m_tracks.size())
            return nullptr;
        lock_guard<recursive_mutex> lk2(m_trackLock);
        auto iter = m_tracks.begin();
        while (idx-- > 0 && iter != m_tracks.end())
            iter++;
        return iter != m_tracks.end() ? *iter : nullptr;
    }

    AudioTrackHolder GetTrackById(int64_t id, bool createIfNotExists) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        auto iter = find_if(m_tracks.begin(), m_tracks.end(), [id] (const AudioTrackHolder& track) {
            return track->Id() == id;
        });
        if (iter != m_tracks.end())
            return *iter;
        if (createIfNotExists)
            return AddTrack(id);
        else
            return nullptr;
    }

    AudioClipHolder GetClipById(int64_t clipId) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        AudioClipHolder clip;
        for (auto& track : m_tracks)
        {
            clip = track->GetClipById(clipId);
            if (clip)
                break;
        }
        return clip;
    }

    int64_t Duration() const override
    {
        return m_duration;
    }

    int64_t ReadPos() const override
    {
        return m_readPos;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

private:
    double ConvertSampleCountToTs(int64_t sampleCount)
    {
        return (double)sampleCount/m_outSampleRate;
    }

    double ConvertPtsToTs(int64_t pts)
    {
        return (double)pts/m_outSampleRate;
    }

    void UpdateDuration()
    {
        int64_t dur = 0;
        for (auto& track : m_tracks)
        {
            const int64_t trackDur = track->Duration();
            if (trackDur > dur)
                dur = trackDur;
        }
        m_duration = dur;
    }

    void StartMixingThread()
    {
        m_quit = false;
        m_mixingThread = thread(&MultiTrackAudioReader_Impl::MixingThreadProc, this);
    }

    void TerminateMixingThread()
    {
        if (m_mixingThread.joinable())
        {
            m_quit = true;
            m_mixingThread.join();
        }
    }

    bool CreateMixer()
    {
        const AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
        const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");

        m_filterGraph = avfilter_graph_alloc();
        if (!m_filterGraph)
        {
            m_errMsg = "FAILED to allocate new 'AVFilterGraph'!";
            return false;
        }

        ostringstream oss;
        oss << "time_base=1/" << m_outSampleRate << ":sample_rate=" << m_outSampleRate
            << ":sample_fmt=" << av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT)
            << ":channel_layout=" << av_get_default_channel_layout(m_outChannels);
        string bufsrcArgs = oss.str(); oss.str("");
        int fferr;

        AVFilterInOut* prevFiltInOutPtr = nullptr;
        for (uint32_t i = 0; i < m_tracks.size(); i++)
        {
            oss << "in_" << i;
            // oss << "in";
            string filtName = oss.str(); oss.str("");
            m_logger->Log(DEBUG) << "buffersrc name '" << filtName << "'." << endl;

            AVFilterContext* bufSrcCtx = nullptr;
            fferr = avfilter_graph_create_filter(&bufSrcCtx, abuffersrc, filtName.c_str(), bufsrcArgs.c_str(), nullptr, m_filterGraph);
            if (fferr < 0)
            {
                oss << "FAILED when invoking 'avfilter_graph_create_filter' for INPUTs! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }

            AVFilterInOut* filtInOutPtr = avfilter_inout_alloc();
            if (!filtInOutPtr)
            {
                m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
                return false;
            }
            filtInOutPtr->name       = av_strdup(filtName.c_str());
            filtInOutPtr->filter_ctx = bufSrcCtx;
            filtInOutPtr->pad_idx    = 0;
            filtInOutPtr->next       = nullptr;
            if (prevFiltInOutPtr)
                prevFiltInOutPtr->next = filtInOutPtr;
            else
                m_filterOutputs = filtInOutPtr;
            prevFiltInOutPtr = filtInOutPtr;

            m_bufSrcCtxs.push_back(bufSrcCtx);
        }

        {
            string filtName = "out";

            AVFilterContext* bufSinkCtx = nullptr;
            fferr = avfilter_graph_create_filter(&bufSinkCtx, abuffersink, filtName.c_str(), nullptr, nullptr, m_filterGraph);
            if (fferr < 0)
            {
                oss << "FAILED when invoking 'avfilter_graph_create_filter' for OUTPUTS! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }

            const AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_FLT, (AVSampleFormat)-1 };
            fferr = av_opt_set_int_list(bufSinkCtx, "sample_fmts", out_sample_fmts, -1, AV_OPT_SEARCH_CHILDREN);
            if (fferr < 0)
            {
                oss << "FAILED when invoking 'av_opt_set_int_list' for OUTPUTS! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }

            AVFilterInOut* filtInOutPtr = avfilter_inout_alloc();
            if (!filtInOutPtr)
            {
                m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
                return false;
            }
            filtInOutPtr->name        = av_strdup(filtName.c_str());
            filtInOutPtr->filter_ctx  = bufSinkCtx;
            filtInOutPtr->pad_idx     = 0;
            filtInOutPtr->next        = nullptr;
            m_filterInputs = filtInOutPtr;

            m_bufSinkCtxs.push_back(bufSinkCtx);
        }

        for (uint32_t i = 0; i < m_tracks.size(); i++)
            oss << "[in_" << i << "]";
        oss << "amix=inputs=" << m_tracks.size();
        string filtArgs = oss.str(); oss.str("");
        m_logger->Log(DEBUG) << "'MultiTrackAudioReader' mixer filter args: '" << filtArgs << "'." << endl;
        fferr = avfilter_graph_parse_ptr(m_filterGraph, filtArgs.c_str(), &m_filterInputs, &m_filterOutputs, nullptr);
        if (fferr < 0)
        {
            oss << "FAILED to invoke 'avfilter_graph_parse_ptr'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

        fferr = avfilter_graph_config(m_filterGraph, nullptr);
        if (fferr < 0)
        {
            oss << "FAILED to invoke 'avfilter_graph_config'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

        FreeAVFilterInOutPtrs();
        return true;
    }

    void ReleaseMixer()
    {
        if (m_filterGraph)
        {
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
        }
        m_bufSrcCtxs.clear();
        m_bufSinkCtxs.clear();

        FreeAVFilterInOutPtrs();
    }

    void FreeAVFilterInOutPtrs()
    {
        if (m_filterOutputs)
            avfilter_inout_free(&m_filterOutputs);
        if (m_filterInputs)
            avfilter_inout_free(&m_filterInputs);
    }

    void MixingThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter MixingThreadProc(AUDIO)..." << endl;

        SelfFreeAVFramePtr outfrm = AllocSelfFreeAVFramePtr();
        while (!m_quit)
        {
            bool idleLoop = true;
            int fferr;

            int64_t mixingPos = m_samplePos*1000/m_outSampleRate;
            m_eof = m_readForward ? mixingPos >= Duration() : mixingPos <= 0;
            if (m_outputMats.size() < m_outputMatsMaxCount && !m_eof)
            {
                if (!m_tracks.empty())
                {
                    {
                        lock_guard<recursive_mutex> lk(m_trackLock);
                        uint32_t i = 0;
                        for (auto iter = m_tracks.begin(); iter != m_tracks.end(); iter++, i++)
                        {
                            SelfFreeAVFramePtr avfrm = AllocSelfFreeAVFramePtr();
                            avfrm->format = AV_SAMPLE_FMT_FLT;
                            avfrm->channels = m_outChannels;
                            avfrm->channel_layout = m_outChannelLayout;
                            avfrm->sample_rate = m_outSampleRate;
                            avfrm->nb_samples = m_outSamplesPerFrame;
                            avfrm->pts = m_samplePos;
                            fferr = av_frame_get_buffer(avfrm.get(), 0);
                            if (fferr < 0)
                            {
                                m_logger->Log(Error) << "FAILED to invoke 'av_frame_get_buffer'(In MixingThreadProc)! fferr=" << fferr << "." << endl;
                                break;
                            }
                            uint32_t toRead = avfrm->linesize[0];
                            auto& track = *iter;
                            double pos;
                            track->ReadAudioSamples(avfrm->data[0], toRead, pos);
                            if (toRead < avfrm->linesize[0])
                                memset(avfrm->data[0]+toRead, 0, avfrm->linesize[0]-toRead);

                            fferr = av_buffersrc_add_frame(m_bufSrcCtxs[i], avfrm.get());
                            if (fferr < 0)
                            {
                                m_logger->Log(Error) << "FAILED to invoke 'av_buffersrc_add_frame'(In MixingThreadProc)! fferr=" << fferr << "." << endl;
                                break;
                            }
                        }
                        if (m_readForward)
                            m_samplePos += m_outSamplesPerFrame;
                        else
                            m_samplePos -= m_outSamplesPerFrame;
                    }

                    fferr = av_buffersink_get_frame(m_bufSinkCtxs[0], outfrm.get());
                    if (fferr >= 0)
                    {
                        ImGui::ImMat amat;
                        amat.create((int)m_outSamplesPerFrame, 1, (int)m_outChannels, (size_t)4);
                        if (amat.total()*4 == outfrm->linesize[0])
                        {
                            memcpy(amat.data, outfrm->data[0], outfrm->linesize[0]);
                            amat.time_stamp = ConvertPtsToTs(outfrm->pts);
                            lock_guard<mutex> lk(m_outputMatsLock);
                            m_outputMats.push_back(amat);
                            idleLoop = false;
                        }
                        else
                            m_logger->Log(Error) << "Audio frame linesize(" << outfrm->linesize[0] << ") is ABNORMAL!" << endl;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        m_logger->Log(Error) << "FAILED to invoke 'av_buffersink_get_frame'(In MixingThreadProc)! fferr=" << fferr << "." << endl;
                    }
                }
                else
                {
                    ImGui::ImMat amat;
                    amat.create((int)m_outSamplesPerFrame, 1, (int)m_outChannels, (size_t)4);
                    memset(amat.data, 0, amat.total()*amat.elemsize);
                    amat.time_stamp = (double)m_samplePos/m_outSampleRate;
                    if (m_readForward)
                        m_samplePos += m_outSamplesPerFrame;
                    else
                        m_samplePos -= m_outSamplesPerFrame;
                    lock_guard<mutex> lk(m_outputMatsLock);
                    m_outputMats.push_back(amat);
                    idleLoop = false;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        m_logger->Log(DEBUG) << "Leave MixingThreadProc(AUDIO)." << endl;
    }

private:
    ALogger* m_logger;
    string m_errMsg;
    recursive_mutex m_apiLock;
    thread m_mixingThread;

    list<AudioTrackHolder> m_tracks;
    recursive_mutex m_trackLock;
    int64_t m_duration{0};
    int64_t m_samplePos{0};
    uint32_t m_outChannels{0};
    uint32_t m_outSampleRate{0};
    int64_t m_outChannelLayout{0};
    uint32_t m_blockSize{0};
    uint32_t m_outSamplesPerFrame{1024};
    int64_t m_readPos{0};
    bool m_readForward{true};
    bool m_eof{false};

    list<ImGui::ImMat> m_outputMats;
    mutex m_outputMatsLock;
    uint32_t m_outputMatsMaxCount{32};

    bool m_configured{false};
    bool m_started{false};
    bool m_quit{false};

    AVFilterGraph* m_filterGraph{nullptr};
    AVFilterInOut* m_filterOutputs{nullptr};
    AVFilterInOut* m_filterInputs{nullptr};
    vector<AVFilterContext*> m_bufSrcCtxs;
    vector<AVFilterContext*> m_bufSinkCtxs;
};

ALogger* MultiTrackAudioReader_Impl::s_logger;

ALogger* GetMultiTrackAudioReaderLogger()
{
    if (!MultiTrackAudioReader_Impl::s_logger)
        MultiTrackAudioReader_Impl::s_logger = GetLogger("MTAReader");
    return MultiTrackAudioReader_Impl::s_logger;
}

MultiTrackAudioReader* CreateMultiTrackAudioReader()
{
    return new MultiTrackAudioReader_Impl();
}

void ReleaseMultiTrackAudioReader(MultiTrackAudioReader** mreader)
{
    if (mreader == nullptr || *mreader == nullptr)
        return;
    MultiTrackAudioReader_Impl* mtar = dynamic_cast<MultiTrackAudioReader_Impl*>(*mreader);
    mtar->Close();
    delete mtar;
    *mreader = nullptr;
}