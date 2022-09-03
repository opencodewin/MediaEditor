#include <thread>
#include <algorithm>
#include <atomic>
#include <sstream>
#include "MultiTrackVideoReader.h"
#include "FFUtils.h"

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
        m_frameInterval = (double)m_frameRate.den/m_frameRate.num;

        if (!m_mixBlender.Init("rgba", outWidth, outHeight, outWidth, outHeight, 0, 0, false))
        {
            ostringstream oss;
            oss << "Mixer blender initialization FAILED! Error message: '" << m_mixBlender.GetError() << "'.";
            m_errMsg = oss.str();
            return false;
        }
        if (!m_subBlender.Init())
        {
            ostringstream oss;
            oss << "Subtitle blender initialization FAILED! Error message: '" << m_subBlender.GetError() << "'.";
            m_errMsg = oss.str();
            return false;
        }

        m_configured = true;
        return true;
    }

    MultiTrackVideoReader* CloneAndConfigure(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        MultiTrackVideoReader_Impl* newInstance = new MultiTrackVideoReader_Impl();
        if (!newInstance->Configure(outWidth, outHeight, frameRate))
        {
            m_errMsg = newInstance->GetError();
            newInstance->Close(); delete newInstance;
            return nullptr;
        }

        // clone all the video tracks
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            for (auto track : m_tracks)
            {
                newInstance->m_tracks.push_back(track->Clone(outWidth, outHeight, frameRate));
            }
        }
        newInstance->UpdateDuration();
        // seek to 0
        newInstance->m_outputCache.clear();
        for (auto track : newInstance->m_tracks)
            track->SeekTo(0);

        // clone all the subtitle tracks
        {
            lock_guard<mutex> lk2(m_subtrkLock);
            for (auto subtrk : m_subtrks)
            {
                if (!subtrk->IsVisible())
                    continue;
                newInstance->m_subtrks.push_back(subtrk->Clone(outWidth, outHeight));
            }
        }

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
        m_outputCache.clear();
        m_configured = false;
        m_started = false;
        m_outWidth = 0;
        m_outHeight = 0;
        m_frameRate = { 0, 0 };
        m_frameInterval = 0;
    }

    VideoTrackHolder AddTrack(int64_t trackId, int64_t insertAfterId = INT64_MAX) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return nullptr;
        }

        TerminateMixingThread();

        VideoTrackHolder hNewTrack(new VideoTrack(trackId, m_outWidth, m_outHeight, m_frameRate));
        hNewTrack->SetDirection(m_readForward);
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            if (insertAfterId == INT64_MAX)
            {
                m_tracks.push_back(hNewTrack);
            }
            else
            {
                auto iter = m_tracks.begin();
                if (insertAfterId != INT64_MIN)
                {
                    iter = find_if(m_tracks.begin(), m_tracks.end(), [insertAfterId] (auto trk) {
                        return trk->Id() == insertAfterId;
                    });
                    if (iter == m_tracks.end())
                    {
                        ostringstream oss;
                        oss << "CANNOT find the video track specified by argument 'insertAfterId' " << insertAfterId << "!";
                        m_errMsg = oss.str();
                        return nullptr;
                    }
                }
                m_tracks.insert(iter, hNewTrack);
            }
            UpdateDuration();
            for (auto track : m_tracks)
                track->SeekTo(ReadPos());
            m_outputCache.clear();
        }

        StartMixingThread();
        return hNewTrack;
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

        VideoTrackHolder delTrack;
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            auto iter = m_tracks.begin();
            while (index > 0 && iter != m_tracks.end())
            {
                iter++;
                index--;
            }
            if (iter != m_tracks.end())
            {
                delTrack = *iter;
                m_tracks.erase(iter);
                UpdateDuration();
                for (auto track : m_tracks)
                    track->SeekTo(ReadPos());
                m_outputCache.clear();
            }
        }

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

        TerminateMixingThread();

        VideoTrackHolder delTrack;
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            auto iter = find_if(m_tracks.begin(), m_tracks.end(), [trackId] (const VideoTrackHolder& track) {
                return track->Id() == trackId;
            });
            if (iter != m_tracks.end())
            {
                delTrack = *iter;
                m_tracks.erase(iter);
                UpdateDuration();
                for (auto track : m_tracks)
                    track->SeekTo(ReadPos());
                m_outputCache.clear();
            }
        }

        StartMixingThread();
        return delTrack;
    }

    bool ChangeTrackViewOrder(int64_t targetId, int64_t insertAfterId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (targetId == insertAfterId)
        {
            m_errMsg = "INVALID arguments! 'targetId' must NOT be the SAME as 'insertAfterId'!";
            return false;
        }

        lock_guard<recursive_mutex> lk2(m_trackLock);
        auto targetTrackIter = find_if(m_tracks.begin(), m_tracks.end(), [targetId] (auto trk) {
            return trk->Id() == targetId;
        });
        if (targetTrackIter == m_tracks.end())
        {
            ostringstream oss;
            oss << "CANNOT find the video track specified by argument 'targetId' " << targetId << "!";
            m_errMsg = oss.str();
            return false;
        }
        if (insertAfterId == INT64_MAX)
        {
            auto moveTrack = *targetTrackIter;
            m_tracks.erase(targetTrackIter);
            m_tracks.push_back(moveTrack);
        }
        else
        {
            auto insertAfterIter = m_tracks.begin();
            if (insertAfterId != INT64_MIN)
            {
                insertAfterIter = find_if(m_tracks.begin(), m_tracks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertAfterIter == m_tracks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the video track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return false;
                }
            }
            auto moveTrack = *targetTrackIter;
            m_tracks.erase(targetTrackIter);
            m_tracks.insert(insertAfterIter, moveTrack);
        }
        return true;
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
        m_outputCache.clear();

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

    bool ReadVideoFrameEx(int64_t pos, std::vector<CorrelativeFrame>& frames, bool seeking) override
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
        if (m_tracks.empty())
            return false;

        {
            lock_guard<mutex> lk2(m_outputCacheLock);
            if (seeking && !m_outputCache.empty())
                frames = m_outputCache.front();
        }
        if (seeking && frames.empty() && !m_seekingFlash.empty())
            frames = m_seekingFlash;

        uint32_t targetFrmidx = (int64_t)(ceil((double)pos*m_frameRate.num/(m_frameRate.den*1000)));
        if ((m_readForward && (targetFrmidx < m_readFrameIdx || targetFrmidx-m_readFrameIdx >= m_outputCacheSize)) ||
            (!m_readForward && (targetFrmidx > m_readFrameIdx || m_readFrameIdx-targetFrmidx >= m_outputCacheSize)))
        {
            if (!SeekTo(pos, seeking))
                return false;
        }

        if (seeking)
        {
            if (!m_subtrks.empty() && !frames.empty())
                frames[0].frame = BlendSubtitle(frames[0].frame);
            return true;
        }

        // the frame queue may not be filled with the target frame, wait for the mixing thread to fill it
        bool lockAquaired = false;
        while (!m_quit)
        {
            m_outputCacheLock.lock();
            lockAquaired = true;
            if ((m_readForward && targetFrmidx < m_outputCache.size()+m_readFrameIdx) ||
                (!m_readForward && m_readFrameIdx < m_outputCache.size()+targetFrmidx))
                break;
            m_outputCacheLock.unlock();
            lockAquaired = false;
            this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_quit)
        {
            if (lockAquaired) m_outputCacheLock.unlock();
            m_errMsg = "This 'MultiTrackVideoReader' instance is quit.";
            return false;
        }

        lock_guard<mutex> lk2(m_outputCacheLock, adopt_lock);
        if ((m_readForward && targetFrmidx > m_readFrameIdx) || (!m_readForward && m_readFrameIdx > targetFrmidx))
        {
            uint32_t popCnt = m_readForward ? targetFrmidx-m_readFrameIdx : m_readFrameIdx-targetFrmidx;
            while (popCnt-- > 0)
            {
                m_outputCache.pop_front();
                if (m_readForward)
                    m_readFrameIdx++;
                else
                    m_readFrameIdx--;
            }
        }
        if (m_outputCache.empty())
        {
            m_logger->Log(Error) << "No AVAILABLE frame to read!" << endl;
            return false;
        }
        frames = m_outputCache.front();
        auto& vmat = frames[0].frame;
        const double timestamp = (double)pos/1000;
        if (vmat.time_stamp > timestamp+m_frameInterval || vmat.time_stamp < timestamp-m_frameInterval)
            m_logger->Log(Error) << "WRONG image time stamp!! Required 'pos' is " << timestamp
                << ", output vmat time stamp is " << vmat.time_stamp << "." << endl;

        if (!m_subtrks.empty())
            vmat = BlendSubtitle(vmat);
        return true;
    }

    bool ReadVideoFrame(int64_t pos, ImGui::ImMat& vmat, bool seeking) override
    {
        vector<CorrelativeFrame> frames;
        bool success = ReadVideoFrameEx(pos, frames, seeking);
        if (!success)
            return false;
        vmat = frames[0].frame;
        return true;
    }

    bool ReadNextVideoFrameEx(vector<CorrelativeFrame>& frames) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        if (m_tracks.empty())
            return false;

        bool lockAquaired = false;
        while (!m_quit)
        {
            m_outputCacheLock.lock();
            lockAquaired = true;
            if (m_outputCache.size() > 1)
                break;
            m_outputCacheLock.unlock();
            lockAquaired = false;
            this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_quit)
        {
            if (lockAquaired) m_outputCacheLock.unlock();
            m_errMsg = "This 'MultiTrackVideoReader' instance is quit.";
            return false;
        }

        lock_guard<mutex> lk2(m_outputCacheLock, adopt_lock);
        if (m_readForward)
        {
            m_outputCache.pop_front();
            m_readFrameIdx++;
        }
        else if (m_readFrameIdx > 0)
        {
            m_outputCache.pop_front();
            m_readFrameIdx--;
        }
        frames = m_outputCache.front();
        if (!m_subtrks.empty())
            frames[0].frame = BlendSubtitle(frames[0].frame);
        return true;
    }

    bool ReadNextVideoFrame(ImGui::ImMat& vmat) override
    {
        vector<CorrelativeFrame> frames;
        bool success = ReadNextVideoFrameEx(frames);
        if (!success)
            return false;
        vmat = frames[0].frame;
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
        if (!clip)
        {
            ostringstream oss;
            oss << "CANNOT find clip with id " << clipId << "!";
            m_errMsg = oss.str();
        }
        return clip;
    }

    VideoOverlapHolder GetOverlapById(int64_t ovlpId) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        VideoOverlapHolder ovlp;
        for (auto& track : m_tracks)
        {
            ovlp = track->GetOverlapById(ovlpId);
            if (ovlp)
                break;
        }
        return ovlp;
    }

    int64_t Duration() const override
    {
        return m_duration;
    }

    int64_t ReadPos() const override
    {
        return (int64_t)((double)m_readFrameIdx*1000*m_frameRate.den/m_frameRate.num);
    }

    SubtitleTrackHolder BuildSubtitleTrackFromFile(int64_t id, const string& url, int64_t insertAfterId = INT64_MAX) override
    {
        SubtitleTrackHolder newSubTrack = SubtitleTrack::BuildFromFile(id, url);
        newSubTrack->SetFrameSize(m_outWidth, m_outHeight);
        newSubTrack->EnableFullSizeOutput(false);
        lock_guard<mutex> lk(m_subtrkLock);
        if (insertAfterId == INT64_MAX)
        {
            m_subtrks.push_back(newSubTrack);
        }
        else
        {
            auto insertAfterIter = m_subtrks.begin();
            if (insertAfterId != INT64_MIN)
            {
                insertAfterIter = find_if(m_subtrks.begin(), m_subtrks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertAfterIter == m_subtrks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the subtitle track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return nullptr;
                }
            }
            m_subtrks.insert(insertAfterIter, newSubTrack);
        }
        return newSubTrack;
    }

    SubtitleTrackHolder NewEmptySubtitleTrack(int64_t id, int64_t insertAfterId = INT64_MAX) override
    {
        SubtitleTrackHolder newSubTrack = SubtitleTrack::NewEmptyTrack(id);
        newSubTrack->SetFrameSize(m_outWidth, m_outHeight);
        newSubTrack->EnableFullSizeOutput(false);
        lock_guard<mutex> lk(m_subtrkLock);
        if (insertAfterId == INT64_MAX)
        {
            m_subtrks.push_back(newSubTrack);
        }
        else
        {
            auto insertAfterIter = m_subtrks.begin();
            if (insertAfterId != INT64_MIN)
            {
                insertAfterIter = find_if(m_subtrks.begin(), m_subtrks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertAfterIter == m_subtrks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the subtitle track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return nullptr;
                }
            }
            m_subtrks.insert(insertAfterIter, newSubTrack);
        }
        return newSubTrack;
    }

    SubtitleTrackHolder GetSubtitleTrackById(int64_t trackId) override
    {
        lock_guard<mutex> lk(m_subtrkLock);
        auto iter = find_if(m_subtrks.begin(), m_subtrks.end(), [trackId] (SubtitleTrackHolder& hTrk) {
            return hTrk->Id() == trackId;
        });
        if (iter == m_subtrks.end())
            return nullptr;
        return *iter;
    }

    SubtitleTrackHolder RemoveSubtitleTrackById(int64_t trackId) override
    {
        lock_guard<mutex> lk(m_subtrkLock);
        auto iter = find_if(m_subtrks.begin(), m_subtrks.end(), [trackId] (SubtitleTrackHolder& hTrk) {
            return hTrk->Id() == trackId;
        });
        if (iter == m_subtrks.end())
            return nullptr;
        SubtitleTrackHolder hTrk = *iter;
        m_subtrks.erase(iter);
        return hTrk;
    }

    bool ChangeSubtitleTrackViewOrder(int64_t targetId, int64_t insertAfterId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (targetId == insertAfterId)
        {
            m_errMsg = "INVALID arguments! 'targetId' must NOT be the SAME as 'insertAfterId'!";
            return false;
        }

        lock_guard<mutex> lk2(m_subtrkLock);
        auto targetTrackIter = find_if(m_subtrks.begin(), m_subtrks.end(), [targetId] (auto trk) {
            return trk->Id() == targetId;
        });
        if (targetTrackIter == m_subtrks.end())
        {
            ostringstream oss;
            oss << "CANNOT find the video track specified by argument 'targetId' " << targetId << "!";
            m_errMsg = oss.str();
            return false;
        }
        if (insertAfterId == INT64_MAX)
        {
            auto moveTrack = *targetTrackIter;
            m_subtrks.erase(targetTrackIter);
            m_subtrks.push_back(moveTrack);
        }
        else
        {
            auto insertAfterIter = m_subtrks.begin();
            if (insertAfterId != INT64_MIN)
            {
                insertAfterIter = find_if(m_subtrks.begin(), m_subtrks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertAfterIter == m_subtrks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the video track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return false;
                }
            }
            auto moveTrack = *targetTrackIter;
            m_subtrks.erase(targetTrackIter);
            m_subtrks.insert(insertAfterIter, moveTrack);
        }
        return true;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

private:
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

        bool afterSeek = false;
        while (!m_quit)
        {
            bool idleLoop = true;

            if (m_seeking.exchange(false))
            {
                const int64_t seekPos = m_seekPos;
                m_readFrameIdx = (int64_t)(ceil((double)seekPos*m_frameRate.num/(m_frameRate.den*1000)));
                for (auto track : m_tracks)
                    track->SeekTo(seekPos);
                {
                    lock_guard<mutex> lk(m_outputCacheLock);
                    if (!m_outputCache.empty())
                        m_seekingFlash = m_outputCache.front();
                    m_outputCache.clear();
                }
                afterSeek = true;
            }

            if (m_outputCache.size() < m_outputCacheSize)
            {
                ImGui::ImMat mixedFrame;
                vector<CorrelativeFrame> frames;
                frames.reserve(m_tracks.size()*7);
                frames.push_back({CorrelativeFrame::PHASE_AFTER_MIXING, 0, 0, mixedFrame});
                double timestamp = 0;
                {
                    lock_guard<recursive_mutex> trackLk(m_trackLock);
                    auto trackIter = m_tracks.begin();
                    while (trackIter != m_tracks.end())
                    {
                        ImGui::ImMat vmat;
                        (*trackIter)->ReadVideoFrame(frames, vmat);
                        if (!vmat.empty())
                        {
                            if (mixedFrame.empty())
                                mixedFrame = vmat;
                            else
                                mixedFrame = m_mixBlender.Blend(vmat, mixedFrame);
                        }
                        if (trackIter == m_tracks.begin())
                            timestamp = vmat.time_stamp;
                        else if (timestamp != vmat.time_stamp)
                            m_logger->Log(WARN) << "'vmat' got from non-1st track has DIFFERENT TIMESTAMP against the 1st track! "
                                << timestamp << " != " << vmat.time_stamp << "." << endl;
                        trackIter++;
                    }
                }
                if (mixedFrame.empty())
                {
                    mixedFrame.create_type(m_outWidth, m_outHeight, 4, IM_DT_INT8);
                    memset(mixedFrame.data, 0, mixedFrame.total()*mixedFrame.elemsize);
                    mixedFrame.time_stamp = timestamp;
                }

                if (afterSeek)
                {
                    int64_t frameIdx = (int64_t)(round(timestamp*m_frameRate.num/m_frameRate.den));
                    if ((m_readForward && frameIdx >= m_readFrameIdx) || (!m_readForward && frameIdx <= m_readFrameIdx))
                    {
                        m_readFrameIdx = frameIdx;
                        afterSeek = false;
                    }
                }

                if (!afterSeek)
                {
                    lock_guard<mutex> lk(m_outputCacheLock);
                    frames[0].frame = mixedFrame;
                    m_outputCache.push_back(frames);
                    m_seekingFlash.clear();
                    idleLoop = false;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        m_logger->Log(DEBUG) << "Leave MixingThreadProc(VIDEO)." << endl;
    }

    ImGui::ImMat BlendSubtitle(ImGui::ImMat& vmat)
    {
        if (m_subtrks.empty())
            return vmat;

        ImGui::ImMat res = vmat;
        bool cloned = false;
        lock_guard<mutex> lk(m_subtrkLock);
        for (auto& hSubTrack : m_subtrks)
        {
            if (!hSubTrack->IsVisible())
                continue;

            auto hSubClip = hSubTrack->GetClipByTime((int64_t)(vmat.time_stamp*1000));
            if (hSubClip)
            {
                auto subImg = hSubClip->Image();
                if (subImg.Valid())
                {
                    // blend subtitle-image
                    SubtitleImage::Rect dispRect = subImg.Area();
                    ImGui::ImMat submat = subImg.Vmat();
                    res = m_subBlender.Blend(res, submat, dispRect.x, dispRect.y, dispRect.w, dispRect.y);
                    if (res.empty())
                    {
                        m_logger->Log(Error) << "FAILED to blend subtitle on the output image! Error message is '" << m_subBlender.GetError() << "'." << endl;
                    }
                }
                else
                {
                    m_logger->Log(Error) << "Invalid 'SubtitleImage' at " << MillisecToString((int64_t)(vmat.time_stamp*1000)) << "." << endl;
                }
            }
        }
        return res;
    }

private:
    ALogger* m_logger;
    string m_errMsg;
    recursive_mutex m_apiLock;

    thread m_mixingThread;
    list<VideoTrackHolder> m_tracks;
    recursive_mutex m_trackLock;
    FFOverlayBlender m_mixBlender;

    list<vector<CorrelativeFrame>> m_outputCache;
    mutex m_outputCacheLock;
    uint32_t m_outputCacheSize{8};

    uint32_t m_outWidth{0};
    uint32_t m_outHeight{0};
    MediaInfo::Ratio m_frameRate;
    double m_frameInterval{0};
    int64_t m_duration{0};
    uint32_t m_readFrameIdx{0};
    bool m_readForward{true};

    int64_t m_seekPos{0};
    atomic_bool m_seeking{false};
    vector<CorrelativeFrame> m_seekingFlash;

    list<SubtitleTrackHolder> m_subtrks;
    mutex m_subtrkLock;
    FFOverlayBlender m_subBlender;

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
