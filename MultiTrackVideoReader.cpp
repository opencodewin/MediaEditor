#include <thread>
#include <algorithm>
#include "MultiTrackVideoReader.h"

using namespace std;
using namespace Logger;
using namespace DataLayer;

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

    VideoTrackHolder AddTrack(int64_t trackId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return nullptr;
        }

        TerminateMixingThread();

        VideoTrackHolder hTrack(new VideoTrack(trackId, m_outWidth, m_outHeight, m_frameRate));
        hTrack->SetDirection(m_readForward);
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            m_tracks.push_back(hTrack);
        }

        double pos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
        for (auto track : m_tracks)
            track->SeekTo(pos);
        m_outputMats.clear();

        StartMixingThread();
        return hTrack;
    }

    VideoTrackHolder RemoveTrackByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
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
        }

        double pos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
        for (auto track : m_tracks)
            track->SeekTo(pos);
        m_outputMats.clear();

        StartMixingThread();
        return delTrack;
    }

    VideoTrackHolder RemoveTrackById(int64_t trackId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return nullptr;
        }

        lock_guard<recursive_mutex> lk2(m_trackLock);
        auto iter = find_if(m_tracks.begin(), m_tracks.end(), [trackId] (const VideoTrackHolder& track) {
            return track->Id() == trackId;
        });
        if (iter == m_tracks.end())
            return nullptr;

        TerminateMixingThread();

        VideoTrackHolder delTrack = *iter;
        m_tracks.erase(iter);

        double pos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
        for (auto track : m_tracks)
            track->SeekTo(pos);
        m_outputMats.clear();

        StartMixingThread();
        return delTrack;
    }

    bool SetDirection(bool forward) override
    {
        if (m_readForward == forward)
            return true;

        TerminateMixingThread();

        m_readForward = forward;
        for (auto& track : m_tracks)
            track->SetDirection(forward);

        double pos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
        for (auto track : m_tracks)
            track->SeekTo(pos);
        m_outputMats.clear();

        StartMixingThread();
        return true;
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

        m_readFrames = (int64_t)(pos*m_frameRate.num/m_frameRate.den);
        for (auto track : m_tracks)
            track->SeekTo(pos);
        m_outputMats.clear();

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
        if (m_readForward && (targetFrmidx < m_readFrames || targetFrmidx-m_readFrames >= m_outputMatsMaxCount) ||
            !m_readForward && (targetFrmidx > m_readFrames || m_readFrames-targetFrmidx >= m_outputMatsMaxCount))
        {
            if (!SeekTo(pos))
                return false;
        }

        // the frame queue may not be filled with the target frame, wait for the mixing thread to fill it
        while ((m_readForward && targetFrmidx-m_readFrames >= m_outputMats.size() ||
            !m_readForward && m_readFrames-targetFrmidx >= m_outputMats.size()) && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
        {
            m_errMsg = "This 'MultiTrackVideoReader' instance is quit.";
            return false;
        }

        lock_guard<mutex> lk2(m_outputMatsLock);
        uint32_t popCnt = m_readForward ? targetFrmidx-m_readFrames : m_readFrames-targetFrmidx;
        while (popCnt-- > 0)
        {
            m_outputMats.pop_front();
            if (m_readForward)
                m_readFrames++;
            else
                m_readFrames--;
        }
        vmat = m_outputMats.front();
        if (pos < vmat.time_stamp || pos > vmat.time_stamp+(double)m_frameRate.den/m_frameRate.num)
            m_logger->Log(Error) << "WRONG image time stamp!! Required 'pos' is " << pos
                << ", output vmat time stamp is " << vmat.time_stamp << "." << endl;
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
        if (m_readForward)
            m_readFrames++;
        else
            m_readFrames--;
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

        double pos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
        SeekTo(pos);
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

    VideoTrackHolder GetTrackByIndex(uint32_t idx) override
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

    VideoTrackHolder GetTrackById(int64_t id, bool createIfNotExists) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        auto iter = find_if(m_tracks.begin(), m_tracks.end(), [id] (const VideoTrackHolder& track) {
            return track->Id() == id;
        });
        if (iter != m_tracks.end())
            return *iter;
        if (createIfNotExists)
            return AddTrack(id);
        else
            return nullptr;
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
        m_logger->Log(DEBUG) << "Enter MixingThreadProc(VIDEO)..." << endl;

        while (!m_quit)
        {
            bool idleLoop = true;

            if (m_outputMats.size() < m_outputMatsMaxCount)
            {
                ImGui::ImMat mixedFrame;
                {
                    lock_guard<recursive_mutex> trackLk(m_trackLock);
                    auto trackIter = m_tracks.begin();
                    while (trackIter != m_tracks.end())
                    {
                        ImGui::ImMat vmat;
                        (*trackIter)->ReadVideoFrame(vmat);
                        if (!vmat.empty() && mixedFrame.empty())
                            mixedFrame = vmat;
                        trackIter++;
                    }
                }
                if (mixedFrame.empty())
                {
                    mixedFrame.create_type(m_outWidth, m_outHeight, 4, IM_DT_INT8);
                    memset(mixedFrame.data, 0, mixedFrame.total()*mixedFrame.elemsize);
                }

                lock_guard<mutex> lk(m_outputMatsLock);
                m_outputMats.push_back(mixedFrame);
                idleLoop = false;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        m_logger->Log(DEBUG) << "Leave MixingThreadProc(VIDEO)." << endl;
    }

private:
    ALogger* m_logger;
    string m_errMsg;
    recursive_mutex m_apiLock;

    thread m_mixingThread;
    list<VideoTrackHolder> m_tracks;
    recursive_mutex m_trackLock;

    list<ImGui::ImMat> m_outputMats;
    mutex m_outputMatsLock;
    uint32_t m_outputMatsMaxCount{8};

    uint32_t m_outWidth{0};
    uint32_t m_outHeight{0};
    MediaInfo::Ratio m_frameRate;
    uint32_t m_readFrames{0};
    bool m_readForward{true};

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