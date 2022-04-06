#include <thread>
#include <algorithm>
#include <atomic>
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
        m_readFrameIdx = 0;

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

        for (auto track : m_tracks)
            track->SeekTo(ReadPos());
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

        for (auto track : m_tracks)
            track->SeekTo(ReadPos());
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

        for (auto track : m_tracks)
            track->SeekTo(ReadPos());
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

        for (auto track : m_tracks)
            track->SeekTo(ReadPos());
        m_outputMats.clear();

        StartMixingThread();
        return true;
    }

    bool SeekTo(int64_t pos, bool async) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        m_seekPos = pos;
        m_seeking = true;

        if (!async)
        {
            while (m_seeking && !m_quit)
                this_thread::sleep_for(chrono::milliseconds(5));
            if (m_quit)
                return false;
        }
        return true;
    }

    bool ReadVideoFrame(int64_t pos, ImGui::ImMat& vmat, bool seeking) override
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
        vmat.release();

        {
            lock_guard<mutex> lk2(m_outputMatsLock);
            if (seeking && !m_outputMats.empty())
                vmat = m_outputMats.front();
        }
        if (seeking && vmat.empty())
            vmat = m_seekingFlash;

        uint32_t targetFrmidx = (int64_t)((double)pos*m_frameRate.num/(m_frameRate.den*1000));
        if (m_readForward && (targetFrmidx < m_readFrameIdx || targetFrmidx-m_readFrameIdx >= m_outputMatsMaxCount) ||
            !m_readForward && (targetFrmidx > m_readFrameIdx || m_readFrameIdx-targetFrmidx >= m_outputMatsMaxCount))
        {
            if (!SeekTo(pos, seeking))
                return false;
        }

        if (seeking)
            return true;

        // the frame queue may not be filled with the target frame, wait for the mixing thread to fill it
        bool lockAquaired = false;
        while (!m_quit)
        {
            m_outputMatsLock.lock();
            lockAquaired = true;
            if (m_readForward && targetFrmidx-m_readFrameIdx < m_outputMats.size() ||
                !m_readForward && m_readFrameIdx-targetFrmidx < m_outputMats.size())
                break;
            m_outputMatsLock.unlock();
            lockAquaired = false;
            this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_quit)
        {
            if (lockAquaired) m_outputMatsLock.unlock();
            m_errMsg = "This 'MultiTrackVideoReader' instance is quit.";
            return false;
        }

        lock_guard<mutex> lk2(m_outputMatsLock, adopt_lock);
        uint32_t popCnt = m_readForward ? targetFrmidx-m_readFrameIdx : m_readFrameIdx-targetFrmidx;
        while (popCnt-- > 0)
        {
            m_outputMats.pop_front();
            if (m_readForward)
                m_readFrameIdx++;
            else
                m_readFrameIdx--;
        }
        if (m_outputMats.empty())
        {
            m_logger->Log(Error) << "No AVAILABLE frame to read!" << endl;
            return false;
        }
        vmat = m_outputMats.front();
        const double timestamp = (double)pos/1000;
        if (timestamp < vmat.time_stamp || timestamp > vmat.time_stamp+(double)m_frameRate.den/m_frameRate.num)
            m_logger->Log(Error) << "WRONG image time stamp!! Required 'pos' is " << timestamp
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

        bool lockAquaired = false;
        while (!m_quit)
        {
            m_outputMatsLock.lock();
            lockAquaired = true;
            if (m_outputMats.size() > 1)
                break;
            m_outputMatsLock.unlock();
            lockAquaired = false;
            this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_quit)
        {
            if (lockAquaired) m_outputMatsLock.unlock();
            m_errMsg = "This 'MultiTrackVideoReader' instance is quit.";
            return false;
        }

        lock_guard<mutex> lk2(m_outputMatsLock, adopt_lock);
        if (m_readForward)
        {
            m_outputMats.pop_front();
            m_readFrameIdx++;
        }
        else if (m_readFrameIdx > 0)
        {
            m_outputMats.pop_front();
            m_readFrameIdx--;
        }
        vmat = m_outputMats.front();
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

        SeekTo(ReadPos(), false);
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

    VideoClipHolder GetClipById(int64_t clipId) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        VideoClipHolder clip;
        for (auto& track : m_tracks)
        {
            clip = track->GetClipById(clipId);
            if (clip)
                break;
        }
        return clip;
    }

    int64_t Duration() override
    {
        if (m_tracks.empty())
            return 0;
        int64_t dur = 0;
        m_trackLock.lock();
        const list<VideoTrackHolder> tracks(m_tracks);
        m_trackLock.unlock();
        for (auto& track : tracks)
        {
            const int64_t trackDur = track->Duration();
            if (trackDur > dur)
                dur = trackDur;
        }
        return dur;
    }

    int64_t ReadPos() const override
    {
        return (int64_t)((double)m_readFrameIdx*1000*m_frameRate.den/m_frameRate.num);
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

            if (m_seeking.exchange(false))
            {
                const int64_t seekPos = m_seekPos;
                m_readFrameIdx = (int64_t)((double)seekPos*m_frameRate.num/(m_frameRate.den*1000));
                for (auto track : m_tracks)
                    track->SeekTo(seekPos);
                {
                    lock_guard<mutex> lk(m_outputMatsLock);
                    if (!m_outputMats.empty())
                        m_seekingFlash = m_outputMats.front();
                    m_outputMats.clear();
                }
            }

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

                {
                    lock_guard<mutex> lk(m_outputMatsLock);
                    m_outputMats.push_back(mixedFrame);
                    m_seekingFlash.release();
                }
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
    uint32_t m_readFrameIdx{0};
    bool m_readForward{true};

    int64_t m_seekPos{0};
    atomic_bool m_seeking{false};
    ImGui::ImMat m_seekingFlash;

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