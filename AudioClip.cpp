#include "AudioClip.h"

using namespace std;

namespace DataLayer
{
    ///////////////////////////////////////////////////////////////////////////////////////////
    // AudioClip
    ///////////////////////////////////////////////////////////////////////////////////////////
    AudioClip::AudioClip(int64_t id, MediaParserHolder hParser, uint32_t outChannels, uint32_t outSampleRate, int64_t start, int64_t startOffset, int64_t endOffset, int64_t readPos)
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
        m_startOffset = startOffset;
        m_endOffset = endOffset;
        if (readPos < 0 || readPos >= Duration()) readPos = 0;
        if (!m_srcReader->SeekTo((double)(startOffset+readPos)/1000))
            throw runtime_error(m_srcReader->GetError());
        if (!m_srcReader->Start())
            throw runtime_error(m_srcReader->GetError());

        m_start = start;
    }

    AudioClip::~AudioClip()
    {
        ReleaseMediaReader(&m_srcReader);
    }

    bool AudioClip::IsStartOffsetValid(int64_t startOffset)
    {
        if (startOffset < 0 || startOffset+m_endOffset >= m_srcDuration)
            return false;
        return true;
    }

    void AudioClip::ChangeStartOffset(int64_t startOffset)
    {
        if (startOffset == m_startOffset)
            return;
        if (startOffset < 0)
            throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
        if (startOffset+m_endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_startOffset = startOffset;
    }

    void AudioClip::ChangeEndOffset(int64_t endOffset)
    {
        if (endOffset == m_endOffset)
            return;
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
        if (m_startOffset+endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_endOffset = endOffset;
    }

    void AudioClip::SeekTo(int64_t pos)
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
        if (!m_srcReader->SeekTo((double)(pos+m_startOffset)/1000))
            throw runtime_error(m_srcReader->GetError());
        m_readPos = pos;
        m_eof = false;
    }

    void AudioClip::ReadAudioSamples(uint8_t* buf, uint32_t& size, bool& eof)
    {
        if (m_eof)
        {
            eof = true;
            size = 0;
            return;
        }
        uint32_t readSize = size;
        double pos;
        if (!m_srcReader->ReadAudioSamples(buf, readSize, pos, eof))
            throw runtime_error(m_srcReader->GetError());
        if (m_pcmFrameSize == 0)
        {
            m_pcmFrameSize = m_srcReader->GetAudioOutFrameSize();
            m_pcmSizePerSec = m_srcReader->GetAudioOutSampleRate()*m_pcmFrameSize;
        }
        double endpos = pos+(double)size/m_pcmSizePerSec;
        if (endpos >= m_srcDuration-m_endOffset)
        {
            readSize = (uint32_t)((m_srcDuration-m_endOffset-pos)*m_pcmSizePerSec/m_pcmFrameSize)*m_pcmFrameSize;
            m_eof = eof = true;
        }
        size = readSize;
    }

    uint32_t AudioClip::ReadAudioSamples(ImGui::ImMat& amat, uint32_t readSamples, bool& eof)
    {
        amat.release();
        if (m_eof)
        {
            eof = true;
            return 0;
        }
        if (m_pcmFrameSize == 0)
        {
            m_pcmFrameSize = m_srcReader->GetAudioOutFrameSize();
            m_pcmSizePerSec = m_srcReader->GetAudioOutSampleRate()*m_pcmFrameSize;
        }
        uint32_t bufSize = readSamples*m_pcmFrameSize;
        if (bufSize == 0)
            return 0;
        int channels = m_srcReader->GetAudioOutChannels();
        amat.create((int)readSamples, (int)1, channels, (size_t)(m_pcmFrameSize/channels));
        if (!amat.data)
            throw runtime_error("FAILED to allocate buffer for 'amat'!");
        uint32_t readSize = bufSize;
        double pos;
        if (!m_srcReader->ReadAudioSamples((uint8_t*)amat.data, readSize, pos, eof))
            throw runtime_error(m_srcReader->GetError());
        int64_t endpos = (int64_t)((pos+(double)readSize/m_pcmSizePerSec)*1000);
        if (endpos > m_srcDuration-m_endOffset)
        {
            readSize = (uint32_t)(((double)(m_srcDuration-m_endOffset)/1000-pos)*m_pcmSizePerSec/m_pcmFrameSize)*m_pcmFrameSize;
            m_eof = eof = true;
            endpos = m_srcDuration-m_endOffset;
        }
        m_readPos = endpos-m_startOffset;
        if (readSize < bufSize)
            memset((uint8_t*)amat.data+readSize, 0, bufSize-readSize);
        return readSize;
    }

    void AudioClip::SetDirection(bool forward)
    {
        m_srcReader->SetDirection(forward);
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

        void MixTwoPcmBuffers(const uint8_t* src1, const uint8_t* src2, uint8_t* dst, uint32_t size, int64_t pos) override
        {
            memcpy(dst, src2, size);
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

    uint32_t AudioOverlap::ReadAudioSamples(uint8_t* buf, uint32_t size, bool& eof)
    {
        int64_t pos = m_frontClip->ReadPos()+m_frontClip->Start()-m_start;
        bool eof1{false};
        unique_ptr<uint8_t[]> buf1(new uint8_t[size]);
        uint32_t readBytes1 = size;
        m_frontClip->ReadAudioSamples(buf1.get(), readBytes1, eof1);
        if (readBytes1 == 0)
        {
            eof = true;
            return 0;
        }

        bool eof2{false};
        unique_ptr<uint8_t[]> buf2(new uint8_t[readBytes1]);
        uint32_t readBytes2 = readBytes1;
        m_rearClip->ReadAudioSamples(buf2.get(), readBytes2, eof2);
        if (readBytes2 < readBytes1)
        {
            memset(buf2.get()+readBytes2, 0, readBytes1-readBytes2);
            eof2 = true;
        }

        AudioTransitionHolder transition = m_transition;
        transition->MixTwoPcmBuffers(buf1.get(), buf2.get(), buf, readBytes1, pos);
        eof = eof1 || eof2;
        return readBytes1;
    }

    void AudioOverlap::ReadAudioSamples(ImGui::ImMat& amat, uint32_t readSamples, bool& eof)
    {
        bool eof1{false};
        ImGui::ImMat amat1;
        m_frontClip->ReadAudioSamples(amat1, readSamples, eof1);

        bool eof2{false};
        ImGui::ImMat amat2;
        m_rearClip->ReadAudioSamples(amat2, readSamples, eof2);

        AudioTransitionHolder transition = m_transition;
        int64_t pos = m_frontClip->ReadPos()+m_frontClip->Start()-m_start;
        amat = transition->MixTwoAudioMats(amat1, amat2, pos);

        eof = eof1 || eof2;
    }

    std::ostream& operator<<(std::ostream& os, AudioOverlap& overlap)
    {
        os << "{'id':" << overlap.Id() << ", 'start':" << overlap.Start() << ", 'dur':" << overlap.Duration() << "}";
        return os;
    }
}