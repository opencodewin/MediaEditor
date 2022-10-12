#include "AudioClip.h"

using namespace std;

namespace DataLayer
{
    ///////////////////////////////////////////////////////////////////////////////////////////
    // AudioClip
    ///////////////////////////////////////////////////////////////////////////////////////////
    class AudioClip_AudioImpl : public AudioClip
    {
    public:
        AudioClip_AudioImpl(
            int64_t id, MediaParserHolder hParser,
            uint32_t outChannels, uint32_t outSampleRate,
            int64_t start, int64_t startOffset, int64_t endOffset)
            : m_id(id), m_start(start)
        {
            m_hInfo = hParser->GetMediaInfo();
            if (hParser->GetBestAudioStreamIndex() < 0)
                throw invalid_argument("Argument 'hParser' has NO AUDIO stream!");
            m_srcReader = CreateMediaReader();
            if (!m_srcReader->Open(hParser))
                throw runtime_error(m_srcReader->GetError());
            if (!m_srcReader->ConfigAudioReader(outChannels, outSampleRate))
                throw runtime_error(m_srcReader->GetError());
            m_srcDuration = (int64_t)(m_srcReader->GetAudioStream()->duration*1000);
            if (startOffset < 0)
                throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
            if (endOffset < 0)
                throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
            if (startOffset+endOffset >= m_srcDuration)
                throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
            m_start = start;
            m_startOffset = startOffset;
            m_endOffset = endOffset;
            m_totalSamples = Duration()*outSampleRate/1000;
            if (!m_srcReader->Start())
                throw runtime_error(m_srcReader->GetError());
        }

        ~AudioClip_AudioImpl()
        {
            ReleaseMediaReader(&m_srcReader);
        }

        AudioClipHolder Clone(uint32_t outChannels, uint32_t outSampleRate) const override
        {
            AudioClipHolder newInstance = AudioClipHolder(new AudioClip_AudioImpl(
                m_id, m_srcReader->GetMediaParser(), outChannels, outSampleRate, m_start, m_startOffset, m_endOffset));
            return newInstance;
        }

        MediaParserHolder GetMediaParser() const override
        {
            return m_srcReader->GetMediaParser();
        }

        int64_t Id() const override
        {
            return m_id;
        }

        int64_t TrackId() const override
        {
            return m_trackId;
        }

        int64_t Start() const override
        {
            return m_start;
        }

        int64_t End() const override
        {
            return m_start+Duration();
        }

        int64_t StartOffset() const override
        {
            return m_startOffset;
        }

        int64_t EndOffset() const override
        {
            return m_endOffset;
        }

        int64_t Duration() const override
        {
            return m_srcDuration-m_startOffset-m_endOffset;
        }

        int64_t ReadPos() const override
        {
            return m_readSamples*1000/m_srcReader->GetAudioOutSampleRate()+m_start;
        }

        uint32_t OutChannels() const override
        {
            return m_srcReader->GetAudioOutChannels();
        }

        uint32_t OutSampleRate() const override
        {
            return m_srcReader->GetAudioOutSampleRate();
        }

        uint32_t LeftSamples() const override
        {
            return m_totalSamples > m_readSamples ? (uint32_t)(m_totalSamples-m_readSamples) : 0;
        }

        void SetTrackId(int64_t trackId) override
        {
            m_trackId = trackId;
        }

        void SetStart(int64_t start) override
        {
            int64_t bias = start-m_start;
            m_start = start;
        }

        void ChangeStartOffset(int64_t startOffset) override
        {
            if (startOffset == m_startOffset)
                return;
            if (startOffset < 0)
                throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
            if (startOffset+m_endOffset >= m_srcDuration)
                throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
            m_startOffset = startOffset;
            const int64_t newTotalSamples = Duration()*m_srcReader->GetAudioOutSampleRate()/1000;
            m_readSamples += newTotalSamples-m_totalSamples;
            m_totalSamples = newTotalSamples;
        }

        void ChangeEndOffset(int64_t endOffset) override
        {
            if (endOffset == m_endOffset)
                return;
            if (endOffset < 0)
                throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
            if (m_startOffset+endOffset >= m_srcDuration)
                throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
            m_endOffset = endOffset;
            m_totalSamples = Duration()*m_srcReader->GetAudioOutSampleRate()/1000;
        }

        void SeekTo(int64_t pos) override
        {
            if (pos < 0)
                pos = 0;
            else if (pos > Duration())
                pos = Duration()-1;
            if (!m_srcReader->SeekTo((double)(pos+m_startOffset)/1000))
                throw runtime_error(m_srcReader->GetError());
            m_readSamples = pos*m_srcReader->GetAudioOutSampleRate()/1000;
            m_eof = false;
        }

        ImGui::ImMat ReadAudioSamples(uint32_t& readSamples, bool& eof) override
        {
            if (m_eof || m_totalSamples <= m_readSamples)
            {
                readSamples = 0;
                m_eof = eof = true;
                return ImGui::ImMat();
            }

            uint32_t sampleRate = m_srcReader->GetAudioOutSampleRate();
            if (m_pcmFrameSize == 0)
            {
                m_pcmFrameSize = m_srcReader->GetAudioOutFrameSize();
                m_pcmSizePerSec = sampleRate*m_pcmFrameSize;
            }

            const uint32_t leftSamples = m_totalSamples-m_readSamples;
            if (readSamples > leftSamples)
                readSamples = leftSamples;
            uint32_t bufSize = readSamples*m_pcmFrameSize;
            int channels = m_srcReader->GetAudioOutChannels();
            ImGui::ImMat amat;
            amat.create((int)readSamples, (int)1, channels, (size_t)(m_pcmFrameSize/channels));
            if (!amat.data)
                throw runtime_error("FAILED to allocate buffer for 'amat'!");

            uint32_t readSize = bufSize;
            double srcpos;
            bool srceof{false};
            if (!m_srcReader->ReadAudioSamples((uint8_t*)amat.data, readSize, srcpos, srceof))
                throw runtime_error(m_srcReader->GetError());
            if (readSize < bufSize)
                memset((uint8_t*)amat.data+readSize, 0, bufSize-readSize);
            amat.time_stamp = (double)m_readSamples/sampleRate+(double)m_start/1000.;
            amat.elempack = m_srcReader->IsPlanar() ? 1 : channels;
            amat.rate.num = sampleRate;
            amat.rate.den = 1;
            amat.flags &= IM_MAT_FLAGS_AUDIO_FRAME;
            m_readSamples += readSamples;
            if (m_readSamples >= m_totalSamples)
                m_eof = eof = true;

            if (m_filter)
                amat = m_filter->FilterPcm(amat, (int64_t)(amat.time_stamp*1000));
            return amat;
        }

        void SetDirection(bool forward) override
        {
            m_srcReader->SetDirection(forward);
        }

        void SetFilter(AudioFilterHolder filter) override
        {
            if (filter)
            {
                filter->ApplyTo(this);
                m_filter = filter;
            }
            else
            {
                m_filter = nullptr;
            }
        }

        AudioFilterHolder GetFilter() const override
        {
            return m_filter;
        }


    private:
        int64_t m_id;
        int64_t m_trackId{-1};
        MediaInfo::InfoHolder m_hInfo;
        MediaReader* m_srcReader;
        AudioFilterHolder m_filter;
        int64_t m_srcDuration;
        int64_t m_start;
        int64_t m_startOffset;
        int64_t m_endOffset;
        uint32_t m_pcmSizePerSec{0};
        uint32_t m_pcmFrameSize{0};
        int64_t m_readSamples{0};
        int64_t m_totalSamples;
        bool m_eof{false};
    };

    AudioClipHolder AudioClip::CreateAudioInstance(
        int64_t id, MediaParserHolder hParser,
        uint32_t outChannels, uint32_t outSampleRate,
        int64_t start, int64_t startOffset, int64_t endOffset)
    {
        return AudioClipHolder(new AudioClip_AudioImpl(id, hParser, outChannels, outSampleRate, start, startOffset, endOffset));
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // DefaultAudioTransition_Impl
    ///////////////////////////////////////////////////////////////////////////////////////////
    class DefaultAudioTransition_Impl : public AudioTransition
    {
    public:
        void ApplyTo(AudioOverlap* overlap) override
        {
            m_overlapPtr = overlap;
        }

        ImGui::ImMat MixTwoAudioMats(const ImGui::ImMat& amat1, const ImGui::ImMat& amat2, int64_t pos) override
        {
            return amat2;
        }

    private:
        AudioOverlap* m_overlapPtr{nullptr};
    };

    ///////////////////////////////////////////////////////////////////////////////////////////
    // AudioOverlap
    ///////////////////////////////////////////////////////////////////////////////////////////
    AudioOverlap::AudioOverlap(int64_t id, AudioClipHolder hClip1, AudioClipHolder hClip2)
        : m_id(id), m_frontClip(hClip1), m_rearClip(hClip2), m_transition(new DefaultAudioTransition_Impl())
    {
        Update();
        m_transition->ApplyTo(this);
    }

    void AudioOverlap::Update()
    {
        AudioClipHolder hClip1 = m_frontClip;
        AudioClipHolder hClip2 = m_rearClip;
        if (hClip1->Start() <= hClip2->Start())
        {
            m_frontClip = hClip1;
            m_rearClip = hClip2;
        }
        else
        {
            m_frontClip = hClip2;
            m_rearClip = hClip1;
        }
        if (m_frontClip->End() <= m_rearClip->Start())
        {
            m_start = m_end = 0;
        }
        else
        {
            m_start = m_rearClip->Start();
            m_end = m_frontClip->End() <= m_rearClip->End() ? m_frontClip->End() : m_rearClip->End();
        }
    }

    void AudioOverlap::SetTransition(AudioTransitionHolder transition)
    {
        if (transition)
        {
            transition->ApplyTo(this);
            m_transition = transition;
        }
        else
        {
            AudioTransitionHolder defaultTrans(new DefaultAudioTransition_Impl());
            defaultTrans->ApplyTo(this);
            m_transition = defaultTrans;
        }
    }

    void AudioOverlap::SeekTo(int64_t pos)
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
        int64_t pos1 = pos+(Start()-m_frontClip->Start());
        m_frontClip->SeekTo(pos1);
        int64_t pos2 = pos+(Start()-m_rearClip->Start());
        m_rearClip->SeekTo(pos2);
    }

    ImGui::ImMat AudioOverlap::ReadAudioSamples(uint32_t& readSamples, bool& eof)
    {
        const uint32_t leftSamples1 = m_frontClip->LeftSamples();
        if (leftSamples1 < readSamples)
            readSamples = leftSamples1;
        const uint32_t leftSamples2 = m_rearClip->LeftSamples();
        if (leftSamples2 < readSamples)
            readSamples = leftSamples2;
        if (readSamples == 0)
        {
            eof = true;
            return ImGui::ImMat();
        }

        bool eof1{false};
        ImGui::ImMat amat1 = m_frontClip->ReadAudioSamples(readSamples, eof1);
        bool eof2{false};
        ImGui::ImMat amat2 = m_rearClip->ReadAudioSamples(readSamples, eof2);
        AudioTransitionHolder transition = m_transition;
        ImGui::ImMat amat = transition->MixTwoAudioMats(amat1, amat2, (int64_t)(amat1.time_stamp*1000));
        eof = eof1 || eof2;
        return amat;
    }

    std::ostream& operator<<(std::ostream& os, AudioOverlap& overlap)
    {
        os << "{'id':" << overlap.Id() << ", 'start':" << overlap.Start() << ", 'dur':" << overlap.Duration() << "}";
        return os;
    }
}
