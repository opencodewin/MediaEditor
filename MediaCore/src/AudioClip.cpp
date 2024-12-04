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

#include <sstream>
#include <functional>
#include <cmath>
#include "AudioClip.h"
#include "MatUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
static int64_t MAX_ALLOWED_MISMATCH_SAMPLES = 200;

///////////////////////////////////////////////////////////////////////////////////////////
// AudioClip
///////////////////////////////////////////////////////////////////////////////////////////
class AudioClip_AudioImpl : public AudioClip
{
public:
    AudioClip_AudioImpl(
        int64_t id, MediaParser::Holder hParser, SharedSettings::Holder hSettings,
        int64_t start, int64_t end, int64_t startOffset, int64_t endOffset, bool exclusiveLogger = false)
        : m_id(id), m_start(start)
    {
        m_hInfo = hParser->GetMediaInfo();
        if (hParser->GetBestAudioStreamIndex() < 0)
            throw invalid_argument("Argument 'hParser' has NO AUDIO stream!");

        string fileName = SysUtils::ExtractFileName(hParser->GetUrl());
        ostringstream loggerNameOss;
        loggerNameOss << id;
        string idstr = loggerNameOss.str();
        if (idstr.size() > 4)
            idstr = idstr.substr(idstr.size()-4);
        loggerNameOss.str(""); loggerNameOss << "AClp-" << fileName.substr(0, 4) << "-" << idstr;
        m_logger = GetLogger(loggerNameOss.str());
        string loggerName = "";
        if (exclusiveLogger)
        {
            auto fileName = SysUtils::ExtractFileName(hParser->GetUrl());
            ostringstream oss;
            oss << "AUD@" << fileName << "";
            loggerName = oss.str();
        }
        m_hSettings = hSettings;
        m_hReader = MediaReader::CreateInstance(loggerName);
        if (!m_hReader->Open(hParser))
            throw runtime_error(m_hReader->GetError());
        if (!m_hReader->ConfigAudioReader(hSettings->AudioOutChannels(), hSettings->AudioOutSampleRate(), hSettings->AudioOutSampleFormatName()))
            throw runtime_error(m_hReader->GetError());
        m_srcDuration = (int64_t)(m_hReader->GetAudioStream()->duration*1000);
        if (startOffset < 0)
            throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
        if (end <= start)
            throw invalid_argument("Invalid arguments 'start/end', clip duration is NOT LARGER than 0!");
        m_startOffset = startOffset;
        m_endOffset = endOffset;
        m_padding = (end-start)+startOffset+endOffset-m_srcDuration;
        m_totalSamples = Duration()*hSettings->AudioOutSampleRate()/1000;
        if (!m_hReader->Start())
            throw runtime_error(m_hReader->GetError());
    }

    ~AudioClip_AudioImpl()
    {
    }

    Holder Clone(SharedSettings::Holder hSettings) const override;

    bool UpdateSettings(SharedSettings::Holder hSettings) override
    {
        m_totalSamples = Duration()*hSettings->AudioOutSampleRate()/1000;
        if (!m_hReader->ChangeAudioOutputFormat(hSettings->AudioOutChannels(), hSettings->AudioOutSampleRate(), hSettings->AudioOutSampleFormatName()))
            throw runtime_error(m_hReader->GetError());
        m_hSettings = hSettings;
        return true;
    }

    MediaParser::Holder GetMediaParser() const override
    {
        return m_hReader->GetMediaParser();
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
        return m_srcDuration+m_padding-m_startOffset-m_endOffset;
    }

    int64_t ReadPos() const override
    {
        return round((double)m_readSamples*1000/m_hReader->GetAudioOutSampleRate())+m_start;
    }

    uint32_t OutChannels() const override
    {
        return m_hReader->GetAudioOutChannels();
    }

    uint32_t OutSampleRate() const override
    {
        return m_hReader->GetAudioOutSampleRate();
    }

    uint32_t LeftSamples() const override
    {
        bool isForward = m_hReader->IsDirectionForward();
        if (isForward)
            return m_totalSamples > m_readSamples ? (uint32_t)(m_totalSamples-m_readSamples) : 0;
        else
            return m_readSamples > m_totalSamples ? 0 : (m_readSamples >= 0 ? (uint32_t)m_readSamples : 0);
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
        const int64_t newTotalSamples = Duration()*m_hReader->GetAudioOutSampleRate()/1000;
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
        m_totalSamples = Duration()*m_hReader->GetAudioOutSampleRate()/1000;
    }

    void SeekTo(int64_t pos) override
    {
        if (pos < 0 || pos > Duration())
        {
            m_logger->Log(WARN) << "!! INVALID seek, pos=" << pos << " is out of the valid range [0, " << Duration() << "] !!" << endl;
            return;
        }
        int64_t targetReadSamples = pos*m_hReader->GetAudioOutSampleRate()/1000;
        if (targetReadSamples == m_readSamples)
            return;

        auto seekPos = pos+m_startOffset;
        if (seekPos > m_srcDuration) seekPos = m_srcDuration;
        m_logger->Log(DEBUG) << "-> AudClip.SeekTo(" << seekPos << ")" << endl;
        if (!m_hReader->SeekTo(seekPos))
            throw runtime_error(m_hReader->GetError());
        m_readSamples = targetReadSamples;
        m_eof = false;
    }

    ImGui::ImMat ReadAudioSamples(uint32_t& readSamples, bool& eof) override
    {
        const uint32_t leftSamples = LeftSamples();
        if (m_eof || leftSamples == 0)
        {
            readSamples = 0;
            m_eof = eof = true;
            return ImGui::ImMat();
        }
        if (readSamples > leftSamples)
            readSamples = leftSamples;

        uint32_t sampleRate = m_hReader->GetAudioOutSampleRate();
        int channels = m_hReader->GetAudioOutChannels();
        if (m_pcmFrameSize == 0)
            m_pcmFrameSize = m_hReader->GetAudioOutFrameSize();

        ImGui::ImMat amat;
        int64_t expectedReadPos = (int64_t)((double)m_readSamples/sampleRate*1000)+m_startOffset;
        if (!m_initSeek)
        {
            if (expectedReadPos > 1000)
                m_hReader->SeekTo(expectedReadPos);
            m_initSeek = true;
        }
        int64_t sourceReadPos = m_hReader->GetReadPos();
        bool readForward = m_hReader->IsDirectionForward();
        // if expected read position does not match the real read position, use silence or skip samples to compensate
        bool skip = false;
        uint32_t diffSamples = (uint32_t)(abs(sourceReadPos-expectedReadPos)*sampleRate/1000);
        if (diffSamples > MAX_ALLOWED_MISMATCH_SAMPLES)
        {
            m_logger->Log(DEBUG) << "! expectedReadPos(" << expectedReadPos << ") != sourceReadPos(" << sourceReadPos << "), diffSamples=" << diffSamples << " !" << endl;
            skip = expectedReadPos > sourceReadPos ? readForward : !readForward;
        }
        else
        {
            diffSamples = 0;
        }
        bool srcEof{false};
        uint32_t silenceSamples = 0;
        if (diffSamples > 0)
        {
            if (skip)
            {
                ImGui::ImMat skipMat;
                uint32_t skipSamples = diffSamples;
                if (skipSamples > sampleRate)
                    m_logger->Log(WARN) << "! Skip sample count " << skipSamples << " is TOO LARGE !" << endl;
                if (!m_hReader->ReadAudioSamples(skipMat, skipSamples, srcEof))
                    throw runtime_error(m_hReader->GetError());
                m_logger->Log(DEBUG) << "! Try to skip " << diffSamples << " samples, skipped " << skipSamples << " samples, srcEof=" << srcEof << " !" << endl;
            }
            else
            {
                silenceSamples = diffSamples > readSamples ? readSamples : diffSamples;
                size_t elemSize = m_pcmFrameSize/channels;
                ImGui::ImMat silenceMat;
                silenceMat.create((int)readSamples, 1, channels, elemSize);
                memset(silenceMat.data, 0, silenceMat.total()*silenceMat.elemsize);
                silenceMat.rate.num = m_hReader->GetAudioOutSampleRate();
                silenceMat.rate.den = 1;
                silenceMat.elempack = m_hReader->IsPlanar() ? 1 : m_hReader->GetAudioOutChannels();
                silenceMat.flags |= IM_MAT_FLAGS_AUDIO_FRAME;
                amat = silenceMat;
                m_logger->Log(DEBUG) << "! silenceSamples = " << silenceSamples << " !" << endl;
                amat.time_stamp = (double)(expectedReadPos-m_startOffset+m_start)/1000;
            }
        }

        // read from source
        if (!m_eof && amat.empty())
        {
            if (silenceSamples == 0)
            {
                uint32_t toReadSamples = readSamples;
                if (!m_hReader->ReadAudioSamples(amat, toReadSamples, srcEof))
                    throw runtime_error(m_hReader->GetError());
            }
            else if (silenceSamples < readSamples)
            {
                uint32_t toReadSamples = readSamples-silenceSamples;
                ImGui::ImMat remainMat;
                if (!m_hReader->ReadAudioSamples(remainMat, toReadSamples, srcEof))
                    throw runtime_error(m_hReader->GetError());
                m_logger->Log(WARN) << "--> Merge silence mat and read mat, silence samples=" << silenceSamples << ", src samples=" << remainMat.w << "." << endl;
                MatUtils::CopyAudioMatSamples(amat, remainMat, silenceSamples, 0);
            }
            if (amat.w > 0)
            {
                int64_t actualReadPos = (int64_t)(amat.time_stamp*1000);
                diffSamples = abs(actualReadPos-expectedReadPos)*sampleRate/1000;
                if (diffSamples > MAX_ALLOWED_MISMATCH_SAMPLES)
                    m_logger->Log(DEBUG) << "! expectedReadPos(" << expectedReadPos << ") != actualReadPos(" << actualReadPos << "), diffSamples=" << diffSamples << " !" << endl;
            }
            amat.time_stamp = (double)(expectedReadPos-m_startOffset+m_start)/1000;
        }

        // state update
        if (!amat.empty())
        {
            readSamples = amat.w;
            m_readSamples += readForward ? (int64_t)readSamples : -(int64_t)readSamples;
        }
        else
        {
            readSamples = 0;
        }
        const uint32_t leftSamples2 = LeftSamples();
        if (leftSamples2 == 0 || srcEof)
            m_eof = eof = true;

        if (m_hFilter && readSamples > 0)
            amat = m_hFilter->FilterPcm(amat, (int64_t)(amat.time_stamp*1000)-m_start, Duration());
        return amat;
    }

    void SetDirection(bool forward) override
    {
        m_hReader->SetDirection(forward);
    }

    void SetFilter(AudioFilter::Holder filter) override
    {
        if (filter)
        {
            filter->ApplyTo(this);
            m_hFilter = filter;
        }
        else
        {
            m_hFilter = nullptr;
        }
    }

    AudioFilter::Holder GetFilter() const override
    {
        return m_hFilter;
    }

    void SetLogLevel(Level l) override
    {
        m_logger->SetShowLevels(l);
    }

private:
    ALogger* m_logger;
    int64_t m_id;
    int64_t m_trackId{-1};
    SharedSettings::Holder m_hSettings;
    MediaInfo::Holder m_hInfo;
    MediaReader::Holder m_hReader;
    AudioFilter::Holder m_hFilter;
    int64_t m_srcDuration;
    int64_t m_start;
    int64_t m_startOffset;
    int64_t m_endOffset;
    int64_t m_padding;
    uint32_t m_pcmFrameSize{0};
    int64_t m_readSamples{0};
    int64_t m_totalSamples;
    bool m_eof{false};
    bool m_initSeek{false};
};

static const function<void(AudioClip*)> AUDIO_CLIP_HOLDER_DELETER = [] (AudioClip* p) {
    AudioClip_AudioImpl* ptr = dynamic_cast<AudioClip_AudioImpl*>(p);
    delete ptr;
};

AudioClip::Holder AudioClip::CreateInstance(
    int64_t id, MediaParser::Holder hParser, SharedSettings::Holder hSettings,
    int64_t start, int64_t end, int64_t startOffset, int64_t endOffset)
{
    return AudioClip::Holder(new AudioClip_AudioImpl(id, hParser, hSettings, start, end, startOffset, endOffset), AUDIO_CLIP_HOLDER_DELETER);
}

AudioClip::Holder AudioClip_AudioImpl::Clone(SharedSettings::Holder hSettings) const
{
    AudioClip_AudioImpl* newInstance = new AudioClip_AudioImpl(m_id, m_hReader->GetMediaParser(), hSettings, m_start, End(), m_startOffset, m_endOffset);
    if (m_hFilter) newInstance->SetFilter(m_hFilter->Clone());
    return AudioClip::Holder(newInstance, AUDIO_CLIP_HOLDER_DELETER);
}

ostream& operator<<(ostream& os, AudioClip::Holder hClip)
{
        os << "{'id':" << hClip->Id() << ", 'start':" << hClip->Start() << ", 'dur':" << hClip->Duration()
            << ", 'soff':" << hClip->StartOffset() << ", 'eoff':" << hClip->EndOffset() << "}";
    return os;
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
class AudioOverlap_Impl : public AudioOverlap
{
public:
    AudioOverlap_Impl(int64_t id, AudioClip::Holder hClip1, AudioClip::Holder hClip2)
        : m_id(id), m_frontClip(hClip1), m_rearClip(hClip2), m_transition(new DefaultAudioTransition_Impl())
    {
        Update();
        m_transition->ApplyTo(this);
    }

    void Update() override
    {
        AudioClip::Holder hClip1 = m_frontClip;
        AudioClip::Holder hClip2 = m_rearClip;
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

    AudioTransition::Holder GetTransition() const override
    {
        return m_transition;
    }

    void SetTransition(AudioTransition::Holder hTrans) override
    {
        if (hTrans)
        {
            hTrans->ApplyTo(this);
            m_transition = hTrans;
        }
        else
        {
            AudioTransition::Holder defaultTrans(new DefaultAudioTransition_Impl());
            defaultTrans->ApplyTo(this);
            m_transition = defaultTrans;
        }
    }

    int64_t Id() const override
    {
        return m_id;
    }

    void SetId(int64_t id) override
    {
        m_id = id;
    }

    int64_t Start() const override
    {
        return m_start;
    }

    int64_t End() const override
    {
        return m_end;
    }

    int64_t Duration() const override
    {
        return m_end-m_start;
    }

    AudioClip::Holder FrontClip() const override
    {
        return m_frontClip;
    }

    AudioClip::Holder RearClip() const override
    {
        return m_rearClip;
    }

    void SeekTo(int64_t pos) override
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

    ImGui::ImMat ReadAudioSamples(uint32_t& readSamples, bool& eof) override
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
        auto toReadSize1 = readSamples;
        ImGui::ImMat amat1 = m_frontClip->ReadAudioSamples(toReadSize1, eof1);
        if (eof1 && toReadSize1 < readSamples)
        {
            ImGui::ImMat temp;
            temp.create_type(readSamples, 1, amat1.c, amat1.type);
            memset(temp.data, 0, temp.total()*temp.elemsize);
            if (!amat1.empty() && amat1.w > 0)
                MatUtils::CopyAudioMatSamples(temp, amat1, 0, 0);
            temp.time_stamp = amat1.time_stamp;
            temp.flags = amat1.flags;
            temp.rate = amat1.rate;
            amat1 = temp;
            toReadSize1 = readSamples;
        }
        bool eof2{false};
        auto toReadSize2 = readSamples;
        ImGui::ImMat amat2 = m_rearClip->ReadAudioSamples(toReadSize2, eof2);
        if (eof2 && toReadSize2 < readSamples)
        {
            ImGui::ImMat temp;
            temp.create_type(readSamples, 1, amat2.c, amat2.type);
            memset(temp.data, 0, temp.total()*temp.elemsize);
            if (!amat2.empty() && amat2.w > 0)
                MatUtils::CopyAudioMatSamples(temp, amat2, 0, 0);
            temp.time_stamp = amat2.time_stamp;
            temp.flags = amat2.flags;
            temp.rate = amat2.rate;
            amat2 = temp;
            toReadSize2 = readSamples;
        }
        assert("Front clip and rear clip returns different read sample count!" && toReadSize1 == toReadSize2);
        AudioTransition::Holder transition = m_transition;
        ImGui::ImMat amat = transition->MixTwoAudioMats(amat1, amat2, (int64_t)(amat1.time_stamp*1000));
        eof = eof1 || eof2;
        return amat;
    }

private:
    int64_t m_id;
    AudioClip::Holder m_frontClip;
    AudioClip::Holder m_rearClip;
    int64_t m_start{0};
    int64_t m_end{0};
    AudioTransition::Holder m_transition;
};

bool AudioOverlap::HasOverlap(AudioClip::Holder hClip1, AudioClip::Holder hClip2)
{
    return (hClip1->Start() >= hClip2->Start() && hClip1->Start() < hClip2->End()) ||
            (hClip1->End() > hClip2->Start() && hClip1->End() <= hClip2->End()) ||
            (hClip1->Start() < hClip2->Start() && hClip1->End() > hClip2->End());
}

static const function<void(AudioOverlap*)> AUDIO_OVERLAP_HOLDER_DELETER = [] (AudioOverlap* p) {
    AudioOverlap_Impl* ptr = dynamic_cast<AudioOverlap_Impl*>(p);
    delete ptr;
};

AudioOverlap::Holder AudioOverlap::CreateInstance(int64_t id, AudioClip::Holder hClip1, AudioClip::Holder hClip2)
{
    return AudioOverlap::Holder(new AudioOverlap_Impl(id, hClip1, hClip2), AUDIO_OVERLAP_HOLDER_DELETER);
}

std::ostream& operator<<(std::ostream& os, const AudioOverlap::Holder& hOverlap)
{
    os << "{'id':" << hOverlap->Id() << ", 'start':" << hOverlap->Start() << ", 'dur':" << hOverlap->Duration() << "}";
    return os;
}
}
