#include <thread>
#include "MultiTrackVideoReader.h"

using namespace std;
using namespace Logger;

class MultiTrackVideoReader_Impl : public MultiTrackVideoReader
{
public:
    static ALogger* s_logger;

    MultiTrackVideoReader_Impl()
    {
        m_logger = GetMultiTrackVideoReaderLogger();
    }

    MultiTrackVideoReader_Impl(const MultiTrackVideoReader_Impl&) = delete;
    MultiTrackVideoReader_Impl(MultiTrackVideoReader_Impl&&) = delete;
    MultiTrackVideoReader_Impl& operator=(const MultiTrackVideoReader_Impl&) = delete;

    virtual ~MultiTrackVideoReader_Impl() {}

    bool Configure(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is already started!";
            return false;
        }

        Close();

        m_outWidth = outWidth;
        m_outHeight = outHeight;
        m_frameRate = frameRate;
        m_readFrames = 0;

        m_configured = true;
        return true;
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
            m_errMsg = "This MultiTrackVideoReader instance is NOT configured yet!";
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

        m_tracks.clear();
        m_outputMats.clear();
        m_configured = false;
        m_started = false;
        m_outWidth = 0;
        m_outHeight = 0;
        m_frameRate = { 0, 0 };
    }

    bool AddTrack() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        TerminateMixingThread();

        VideoTrackHolder hTrack(new VideoTrack(m_outWidth, m_outHeight, m_frameRate));
        m_tracks.push_back(hTrack);
        m_outputMats.clear();

        // ReleaseMixer();
        // if (!CreateMixer())
        //     return false;

        double pos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
        for (auto track : m_tracks)
            track->SeekTo(pos);

        StartMixingThread();
        return true;
    }

    bool RemoveTrack(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        if (index >= m_tracks.size())
        {
            m_errMsg = "Invalid value for argument 'index'!";
            return false;
        }

        TerminateMixingThread();

        auto iter = m_tracks.begin();
        while (index > 0)
        {
            iter++;
            index--;
        }
        m_tracks.erase(iter);

        // ReleaseMixer();
        // if (!m_tracks.empty())
        // {
        //     if (!CreateMixer())
        //         return false;
        // }

        double pos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
        for (auto track : m_tracks)
            track->SeekTo(pos);

        StartMixingThread();
        return true;
    }

    bool SetDirection(bool forward) override
    {
        return false;
    }

    bool SeekTo(double pos) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        TerminateMixingThread();

        m_outputMats.clear();
        m_readFrames = (int64_t)(pos*m_frameRate.num/m_frameRate.den);
        for (auto track : m_tracks)
            track->SeekTo(pos);

        StartMixingThread();
        return true;
    }

    bool ReadVideoFrame(double pos, ImGui::ImMat& vmat) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        if (pos < 0)
        {
            m_errMsg = "Invalid argument value for 'pos'! Can NOT be NEGATIVE.";
            return false;
        }

        uint32_t targetFrmidx = (uint32_t)(pos*m_frameRate.num/m_frameRate.den);
        if (targetFrmidx < m_readFrames && targetFrmidx-m_readFrames >= m_outputMatsMaxCount)
        {
            if (!SeekTo(pos))
                return false;
        }

        return true;
    }

    bool ReadNextVideoFrame(ImGui::ImMat& vmat) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        while (m_outputMats.empty() && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
        {
            m_errMsg = "This 'MultiTrackVideoReader' instance is quit.";
            return false;
        }

        lock_guard<mutex> lk2(m_outputMatsLock);
        vmat = m_outputMats.front();
        m_outputMats.pop_front();
        return true;
    }

    uint32_t TrackCount() const override
    {
        return m_tracks.size();
    }

    list<VideoTrackHolder>::iterator TrackListBegin() override
    {
        return m_tracks.begin();
    }

    list<VideoTrackHolder>::iterator TrackListEnd() override
    {
        return m_tracks.end();
    }

    VideoTrackHolder GetTrack(uint32_t idx) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (idx >= m_tracks.size())
            return nullptr;
        lock_guard<mutex> lk2(m_trackLock);
        auto iter = m_tracks.begin();
        while (idx-- > 0 && iter != m_tracks.end())
            iter++;
        return iter != m_tracks.end() ? *iter : nullptr;
    }

    double Duration() override
    {
        if (m_tracks.empty())
            return 0;
        double dur = 0;
        m_trackLock.lock();
        const list<VideoTrackHolder> tracks(m_tracks);
        m_trackLock.unlock();
        for (auto& track : tracks)
        {
            const double trackDur = track->Duration();
            if (trackDur > dur)
                dur = trackDur;
        }
        return dur;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

private:
    void StartMixingThread()
    {
        m_quit = false;
        m_mixingThread = thread(&MultiTrackVideoReader_Impl::MixingThreadProc, this);
    }

    void TerminateMixingThread()
    {
        if (m_mixingThread.joinable())
        {
            m_quit = true;
            m_mixingThread.join();
        }
    }

    void MixingThreadProc()
    {

    }

private:
    ALogger* m_logger;
    string m_errMsg;
    recursive_mutex m_apiLock;

    thread m_mixingThread;
    list<VideoTrackHolder> m_tracks;
    mutex m_trackLock;

    list<ImGui::ImMat> m_outputMats;
    mutex m_outputMatsLock;
    uint32_t m_outputMatsMaxCount{8};

    uint32_t m_outWidth{0};
    uint32_t m_outHeight{0};
    MediaInfo::Ratio m_frameRate;
    uint32_t m_readFrames{0};

    bool m_configured{false};
    bool m_started{false};
    bool m_quit{false};
};

ALogger* MultiTrackVideoReader_Impl::s_logger;

ALogger* GetMultiTrackVideoReaderLogger()
{
    if (!MultiTrackVideoReader_Impl::s_logger)
        MultiTrackVideoReader_Impl::s_logger = GetLogger("MTVReader");
    return MultiTrackVideoReader_Impl::s_logger;
}

MultiTrackVideoReader* CreateMultiTrackVideoReader()
{
    return new MultiTrackVideoReader_Impl();
}

void ReleaseMultiTrackVideoReader(MultiTrackVideoReader** mreader)
{
    if (mreader == nullptr || *mreader == nullptr)
        return;
    MultiTrackVideoReader_Impl* mtvr = dynamic_cast<MultiTrackVideoReader_Impl*>(*mreader);
    mtvr->Close();
    delete mtvr;
    *mreader = nullptr;
}