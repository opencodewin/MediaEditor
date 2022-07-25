#include <sstream>
#include <algorithm>
#include <vector>
#include <cstring>
#include "SubtitleTrack_AssImpl.h"
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/avutil.h"
    #include "libswscale/swscale.h"
}

using namespace std;
using namespace DataLayer;
using namespace Logger;
using std::placeholders::_1;

static void PrintAssStyle(ALogger* logger, ASS_Style* s)
{
    logger->Log(DEBUG) << "------------------- ASS Style --------------------" << endl;
    logger->Log(DEBUG) << '\t' << "Name: " << s->Name << endl;
    logger->Log(DEBUG) << '\t' << "FontName: " << s->FontName << endl;
    logger->Log(DEBUG) << '\t' << "FontSize: " << s->FontSize << endl;
    logger->Log(DEBUG) << '\t' << "PrimaryColor: " << s->PrimaryColour << endl;
    logger->Log(DEBUG) << '\t' << "SecondaryColor: " << s->SecondaryColour << endl;
    logger->Log(DEBUG) << '\t' << "OutlineColor:" << s->OutlineColour << endl;
    logger->Log(DEBUG) << '\t' << "BackColor: " << s->BackColour << endl;
    logger->Log(DEBUG) << '\t' << "Bold: " << s->Bold << endl;
    logger->Log(DEBUG) << '\t' << "Italic: " << s->Italic << endl;
    logger->Log(DEBUG) << '\t' << "Underline: " << s->Underline << endl;
    logger->Log(DEBUG) << '\t' << "StrikeOut: " << s->StrikeOut << endl;
    logger->Log(DEBUG) << '\t' << "ScaleX: " << s->ScaleX << endl;
    logger->Log(DEBUG) << '\t' << "ScaleY: " << s->ScaleY << endl;
    logger->Log(DEBUG) << '\t' << "Spacing: " << s->Spacing << endl;
    logger->Log(DEBUG) << '\t' << "Angle: " << s->Angle << endl;
    logger->Log(DEBUG) << '\t' << "BorderStyle: " << s->BorderStyle << endl;
    logger->Log(DEBUG) << '\t' << "Outline: " << s->Outline << endl;
    logger->Log(DEBUG) << '\t' << "Shadow: " << s->Shadow << endl;
    logger->Log(DEBUG) << '\t' << "Alignment: " << s->Alignment << endl;
    logger->Log(DEBUG) << '\t' << "MarginL: " << s->MarginL << endl;
    logger->Log(DEBUG) << '\t' << "MarginR: " << s->MarginR << endl;
    logger->Log(DEBUG) << '\t' << "MarginV: " << s->MarginV << endl;
    logger->Log(DEBUG) << '\t' << "Encoding: " << s->Encoding << endl;
    logger->Log(DEBUG) << '\t' << "treat_fontname_as_pattern: " << s->treat_fontname_as_pattern << endl;
    logger->Log(DEBUG) << '\t' << "Blur: " << s->Blur << endl;
    logger->Log(DEBUG) << '\t' << "Justify: " << s->Justify << endl;
    logger->Log(DEBUG) << "--------------------------------------------------" << endl;
}

SubtitleTrack_AssImpl::SubtitleTrack_AssImpl(int64_t id)
    : m_id(id)
{
    m_logger = GetSubtitleTrackLogger();
    m_currIter = m_clips.begin();
}

SubtitleTrack_AssImpl::~SubtitleTrack_AssImpl()
{
    if (m_assrnd)
    {
        ass_renderer_done(m_assrnd);
        m_assrnd = nullptr;
    }
    if (m_asstrk)
    {
        ass_free_track(m_asstrk);
        m_asstrk = nullptr;
    }
    ReleaseFFContext();
}

bool SubtitleTrack_AssImpl::InitAss()
{
    if (!s_asslib)
    {
        m_errMsg = "ASS library has NOT been INITIALIZED!";
        return false;
    }
    m_assrnd = ass_renderer_init(s_asslib);
    if (!m_assrnd)
    {
        m_errMsg = "FAILED to initialize ASS renderer!";
        return false;
    }
    ass_set_fonts(m_assrnd, NULL, NULL, ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
    m_asstrk = ass_new_track(s_asslib);
    if (!m_asstrk)
    {
        m_errMsg = "FAILED to create a new ASS track!";
        return false;
    }
    return true;
}

bool SubtitleTrack_AssImpl::SetFrameSize(uint32_t width, uint32_t height)
{
    ass_set_frame_size(m_assrnd, width, height);
    m_frmW = width;
    m_frmH = height;
    return true;
}

bool SubtitleTrack_AssImpl::EnableFullSizeOutput(bool enable)
{
    if (m_outputFullSize == enable)
        return true;
    m_outputFullSize = enable;
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetBackgroundColor(const SubtitleClip::Color& color)
{
    m_bgColor = color;
    for (SubtitleClipHolder clip : m_clips)
        clip->SetBackgroundColor(color);
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetFont(const std::string& font)
{
    m_logger->Log(DEBUG) << "Set font '" << font << "'" << endl;
    m_overrideStyle.SetFont(font);
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetScale(double scale)
{
    m_logger->Log(DEBUG) << "Set font scale " << scale << endl;
    ass_set_font_scale(m_assrnd, scale);
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetScaleX(double value)
{
    m_logger->Log(DEBUG) << "Set scaleX '" << value << "'" << endl;
    m_overrideStyle.m_style.ScaleX = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetScaleY(double value)
{
    m_logger->Log(DEBUG) << "Set scaleY '" << value << "'" << endl;
    m_overrideStyle.m_style.ScaleY = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetSpacing(double value)
{
    m_logger->Log(DEBUG) << "Set spacing '" << value << "'" << endl;
    m_overrideStyle.m_style.Spacing = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetAngle(double value)
{
    m_logger->Log(DEBUG) << "Set angle '" << value << "'" << endl;
    m_overrideStyle.m_style.Angle = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetOutline(double value)
{
    m_logger->Log(DEBUG) << "Set outline '" << value << "'" << endl;
    m_overrideStyle.m_style.Outline = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetAlignment(int value)
{
    m_logger->Log(DEBUG) << "Set alignment '" << value << "'" << endl;
    m_overrideStyle.m_style.Alignment = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetMarginL(int value)
{
    m_logger->Log(DEBUG) << "Set marginL '" << value << "'" << endl;
    m_overrideStyle.m_style.MarginL = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetMarginR(int value)
{
    m_logger->Log(DEBUG) << "Set marginR '" << value << "'" << endl;
    m_overrideStyle.m_style.MarginR = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetMarginV(int value)
{
    m_logger->Log(DEBUG) << "Set marginV '" << value << "'" << endl;
    m_overrideStyle.m_style.MarginV = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetItalic(int value)
{
    m_logger->Log(DEBUG) << "Set italic '" << value << "'" << endl;
    m_overrideStyle.m_style.Italic = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetBold(int value)
{
    m_logger->Log(DEBUG) << "Set bold '" << value << "'" << endl;
    m_overrideStyle.m_style.Bold = value;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetUnderLine(bool enable)
{
    m_logger->Log(DEBUG) << "Set underline '" << enable << "'" << endl;
    m_overrideStyle.m_style.Underline = enable ? 1 : 0;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetStrikeOut(bool enable)
{
    m_logger->Log(DEBUG) << "Set strikeout '" << enable << "'" << endl;
    m_overrideStyle.m_style.StrikeOut = enable ? 1 : 0;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetPrimaryColor(const SubtitleClip::Color& color)
{
    uint32_t c = ((uint32_t)(color.r*255)<<24) | ((uint32_t)(color.g*255)<<16) | ((uint32_t)(color.b*255)<<8) | (uint32_t)((1-color.a)*255);
    m_logger->Log(DEBUG) << "Set primary color as " << c << "(" << hex << c << ")" << endl;
    m_overrideStyle.m_style.PrimaryColour = c;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetSecondaryColor(const SubtitleClip::Color& color)
{
    uint32_t c = ((uint32_t)(color.r*255)<<24) | ((uint32_t)(color.g*255)<<16) | ((uint32_t)(color.b*255)<<8) | (uint32_t)((1-color.a)*255);
    m_logger->Log(DEBUG) << "Set secondary color as " << c << "(" << hex << c << ")" << endl;
    m_overrideStyle.m_style.SecondaryColour = c;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

bool SubtitleTrack_AssImpl::SetOutlineColor(const SubtitleClip::Color& color)
{
    uint32_t c = ((uint32_t)(color.r*255)<<24) | ((uint32_t)(color.g*255)<<16) | ((uint32_t)(color.b*255)<<8) | (uint32_t)((1-color.a)*255);
    m_logger->Log(DEBUG) << "Set outline color as " << c << "(" << hex << c << ")" << endl;
    m_overrideStyle.m_style.OutlineColour = c;
    ass_set_selective_style_override(m_assrnd, m_overrideStyle.GetAssStylePtr());
    if (!m_useOverrideStyle)
        ToggleOverrideStyle();
    ClearRenderCache();
    return true;
}

SubtitleClipHolder SubtitleTrack_AssImpl::GetClipByTime(int64_t ms)
{
    if (m_clips.size() <= 0)
        return nullptr;
    if (m_currIter != m_clips.end() && (*m_currIter)->StartTime() <= ms && (*m_currIter)->EndTime() > ms)
        return *m_currIter;

    if (m_currIter == m_clips.end() || (*m_currIter)->StartTime() > ms)
    {
        auto iter = m_currIter;
        while (iter != m_clips.begin())
        {
            iter--;
            if ((*iter)->EndTime() <= ms)
            {
                iter++;
                break;
            }
            else if ((*iter)->StartTime() <= ms)
            {
                break;
            }
        }
        m_currIter = iter;
    }
    else
    {
        auto iter = m_currIter;
        iter++;
        while (iter != m_clips.end())
        {
            if ((*iter)->EndTime() > ms)
                break;
            iter++;
        }
        m_currIter = iter;
    }

    if (m_currIter == m_clips.end())
        return nullptr;
    if ((*m_currIter)->StartTime() <= ms && (*m_currIter)->EndTime() > ms)
        return *m_currIter;
    return nullptr;
}

SubtitleClipHolder SubtitleTrack_AssImpl::GetCurrClip()
{
    if (m_currIter == m_clips.end())
        return nullptr;
    return *m_currIter;
}

SubtitleClipHolder SubtitleTrack_AssImpl::GetPrevClip()
{
    if (m_currIter == m_clips.begin())
        return nullptr;
    m_currIter--;
    return *m_currIter;
}

SubtitleClipHolder SubtitleTrack_AssImpl::GetNextClip()
{
    if (m_currIter == m_clips.end())
        return nullptr;
    m_currIter++;
    if (m_currIter == m_clips.end())
        return nullptr;
    return *m_currIter;
}

int32_t SubtitleTrack_AssImpl::GetClipIndex(SubtitleClipHolder clip) const
{
    if (!clip)
        return -1;
    int32_t idx = 0;
    auto iter = m_clips.begin();
    while (iter != m_clips.end())
    {
        if (*iter == clip)
            break;
        idx++;
        iter++;
    }
    if (iter == m_clips.end())
        return -1;
    return idx;
}

bool SubtitleTrack_AssImpl::SeekToTime(int64_t ms)
{
    m_readPos = ms;
    auto iter = find_if(m_clips.begin(), m_clips.end(), [ms] (SubtitleClipHolder clip) {
        return clip->StartTime() > ms;
    });
    m_currIter = iter;
    if (iter != m_clips.begin())
    {
        iter--;
        if ((*iter)->EndTime() > ms)
            m_currIter = iter;
    }

    return true;
}

bool SubtitleTrack_AssImpl::SeekToIndex(uint32_t index)
{
    if (index >= m_clips.size())
    {
        ostringstream oss;
        oss << "Index (" << index << ") exceeds the number of subtitle clips (" << m_clips.size() << ")!";
        m_errMsg = oss.str();
        return false;
    }
    uint32_t loopCnt = index;
    auto iter = m_clips.begin();
    while (loopCnt > 0 && iter != m_clips.end())
    {
        iter++;
        loopCnt--;
    }
    m_currIter = iter;
    m_readPos = (*iter)->StartTime();
    return true;
}

SubtitleClipHolder SubtitleTrack_AssImpl::NewClip(int64_t startTime, int64_t duration)
{
    SubtitleClipHolder hNewClip(new SubtitleClip(DataLayer::ASS, 0, startTime, duration, ""));
    m_clips.push_back(hNewClip);
    return hNewClip;
}

bool SubtitleTrack_AssImpl::ChangeText(uint32_t clipIndex, const string& text)
{
    if (clipIndex >= m_clips.size())
        return false;

    auto iter = m_clips.begin();
    uint32_t i = clipIndex;
    while (iter != m_clips.end() && i > 0)
    {
        iter++;
        i--;
    }
    if (iter == m_clips.end())
        return false;

    return ChangeText(*iter, text);
}

bool SubtitleTrack_AssImpl::ChangeText(SubtitleClipHolder clip, const string& text)
{
    ASS_Event* target = nullptr;
    for (int i = 0; i < m_asstrk->n_events; i++)
    {
        ASS_Event* e = m_asstrk->events+i;
        if (e->ReadOrder == clip->ReadOrder())
        {
            target = e;
            break;
        }
    }
    if (!target)
        return false;

    if (target->Text)
        free(target->Text);
    int len = text.size();
    target->Text = (char*)malloc(len+1);
    memcpy(target->Text, text.c_str(), len);
    target->Text[len] = 0;
    clip->SetText(text);
    clip->InvalidateImage();

    return true;
}

ASS_Library* SubtitleTrack_AssImpl::s_asslib = nullptr;

static void ass_log(int ass_level, const char *fmt, va_list args, void *ctx)
{
    Logger::Level level = VERBOSE;
    if (ass_level < 2)
        level = Error;
    else if (ass_level < 4)
        level = WARN;
    else if (ass_level < 5)
        level = INFO;
    else if (ass_level < 6)
        level = DEBUG;
    char buf[2048]={0};
    va_list vl;
    va_copy(vl, args);
    vsnprintf(buf, sizeof(buf)-1, fmt, vl);
    va_end(vl);
    GetSubtitleTrackLogger()->Log(level) << "[ASSLOG] " << buf << endl;
}

bool SubtitleTrack_AssImpl::Initialize()
{
    if (s_asslib)
    {
        // already initialized
        return true;
    }
    s_asslib = ass_library_init();
    if (!s_asslib)
        return false;
    ass_set_message_cb(s_asslib, ass_log, NULL);
    return true;
}

void SubtitleTrack_AssImpl::Release()
{
    if (s_asslib)
    {
        ass_library_done(s_asslib);
        s_asslib = nullptr;
    }
}

bool SubtitleTrack_AssImpl::SetFontDir(const string& path)
{
    if (!s_asslib)
        return false;
    ass_set_fonts_dir(s_asslib, path.c_str());
    return true;
}

bool SubtitleTrack_AssImpl::ReadFile(const string& path)
{
    ReleaseFFContext();

    int fferr;
    fferr = avformat_open_input(&m_pAvfmtCtx, path.c_str(), nullptr, nullptr);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "avformat_open_input() FAILED! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    fferr = avformat_find_stream_info(m_pAvfmtCtx, nullptr);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "avformat_find_stream_info() FAILED! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    int subStmIdx = -1;
    for (int i = 0; i < m_pAvfmtCtx->nb_streams; i++)
    {
        if (m_pAvfmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            subStmIdx = i;
            break;
        }
    }
    if (subStmIdx < 0)
    {
        ostringstream oss;
        oss << "CANNOT find any subtitle track in the file '" << path << "'!";
        m_errMsg = oss.str();
        return false;
    }

    AVStream* pAvStream = m_pAvfmtCtx->streams[subStmIdx];
    AVCodecPtr pAvCdc = avcodec_find_decoder(pAvStream->codecpar->codec_id);
    AVCodecContext* pAvCdcCtx = avcodec_alloc_context3(pAvCdc);
    fferr = avcodec_parameters_to_context(pAvCdcCtx, pAvStream->codecpar);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "avcodec_parameters_to_context() FAILED! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    pAvCdcCtx->pkt_timebase = pAvStream->time_base;
    fferr = avcodec_open2(pAvCdcCtx, pAvCdc, nullptr);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "avcodec_open2() FAILED! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }

    if (pAvCdcCtx->subtitle_header && pAvCdcCtx->subtitle_header_size > 0)
    {
        ass_process_codec_private(m_asstrk, (char*)pAvCdcCtx->subtitle_header, pAvCdcCtx->subtitle_header_size);
        if (m_asstrk->styles && m_asstrk->n_styles > 0)
        {
            m_logger->Log(DEBUG) << m_asstrk->n_styles << " style(s) are found:" << endl;
            for (int i = 0; i < m_asstrk->n_styles; i++)
                PrintAssStyle(m_logger, m_asstrk->styles+i);
        }
    }

    m_errMsg.clear();
    bool demuxEof = false;
    bool decodeEof = false;
    while (!decodeEof)
    {
        AVPacket avpkt{0};
        if (!demuxEof)
        {
            fferr = av_read_frame(m_pAvfmtCtx, &avpkt);
            if (fferr == AVERROR_EOF)
            {
                demuxEof = true;
            }
            else if (fferr < 0)
            {
                ostringstream oss;
                oss << "av_read_frame() FAILED! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                break;
            }
        }

        AVSubtitle avsub{0};
        int gotSubPtr = 0;
        fferr = avcodec_decode_subtitle2(pAvCdcCtx, &avsub, &gotSubPtr, &avpkt);
        if (fferr == AVERROR_EOF)
        {
            decodeEof = true;
        }
        else if (fferr < 0)
        {
            ostringstream oss;
            oss << "avcodec_decode_subtitle2() FAILED! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            break;
        }
        av_packet_unref(&avpkt);

        if (gotSubPtr)
        {
            const int64_t start_time = av_rescale_q(avsub.pts, AV_TIME_BASE_Q, av_make_q(1, 1000));
            const int64_t duration   = avsub.end_display_time;
            m_logger->Log(VERBOSE) << "[" << MillisecToString(start_time) << "(+" << duration << ")] ";
            for (auto i = 0; i < avsub.num_rects; i++)
            {
                char *ass_line = avsub.rects[i]->ass;
                if (!ass_line)
                    break;
                m_logger->Log(VERBOSE) << "<" << i << ">: '" << avsub.rects[i]->ass << "'; ";
            }
            m_logger->Log(VERBOSE) << endl;
            for (auto i = 0; i < avsub.num_rects; i++)
            {
                char *ass_line = avsub.rects[i]->ass;
                if (!ass_line)
                    break;
#if LIBAVCODEC_VERSION_MAJOR >= 59
                ass_process_chunk(m_asstrk, ass_line, strlen(ass_line), start_time, duration);
#else
                ass_process_data(m_asstrk, ass_line, strlen(ass_line));
#endif
            }
            avsubtitle_free(&avsub);
        }
        else if (demuxEof)
        {
            decodeEof = true;
        }
    }

    bool success = m_errMsg.empty();
    if (success)
    {
        if (m_asstrk->n_styles > 0)
        {
            m_overrideStyle = AssStyleWrapper(m_asstrk->styles+m_asstrk->n_styles-1);
        }

        for (int i = 0; i < m_asstrk->n_events; i++)
        {
            ASS_Event* e = m_asstrk->events+i;
            SubtitleClipHolder hSubClip(new SubtitleClip(DataLayer::ASS, e->ReadOrder, e->Start, e->Duration, e->Text));
            hSubClip->SetRenderCallback(bind(&SubtitleTrack_AssImpl::RenderSubtitleClip, this, _1));
            uint32_t primaryColor = m_asstrk->styles[e->Style].PrimaryColour;
            hSubClip->SetBackgroundColor(m_bgColor);
            m_clips.push_back(hSubClip);
        }
        if (!m_clips.empty())
        {
            auto last = m_clips.back();
            m_duration = last->EndTime();
        }
    }
    return success;
}

void SubtitleTrack_AssImpl::ReleaseFFContext()
{
    if (m_pAvCdcCtx)
    {
        avcodec_free_context(&m_pAvCdcCtx);
        m_pAvCdcCtx = nullptr;
    }
    if (m_pAvfmtCtx)
    {
        avformat_close_input(&m_pAvfmtCtx);
        m_pAvfmtCtx = nullptr;
    }
}

template <typename T>
class WrapperAlloc: public allocator<T>
{
public:
    WrapperAlloc(void* buf=0) throw(): allocator<T>(), m_buf(buf) {}
    WrapperAlloc(const WrapperAlloc& a) throw(): allocator<T>(a) { m_buf = a.m_buf; }
    ~WrapperAlloc() {}

    typedef size_t size_type;
    typedef T* pointer;
    typedef const T* const_pointer;

    template<typename _Tp1>
    struct rebind
    {
        typedef WrapperAlloc<_Tp1> other;
    };

    pointer allocate(size_type n, const void* hint=0)
    {
        char* p = new (m_buf) char[n*sizeof(T)];
        return (T*)p;
    }

    void deallocate(pointer p, size_type n) {}

private:
    void* m_buf;
};

SubtitleImage SubtitleTrack_AssImpl::RenderSubtitleClip(SubtitleClip* clip)
{
    int detectChange = 0;
    ASS_Image* renderRes = ass_render_frame(m_assrnd, m_asstrk, clip->StartTime(), &detectChange);
    m_logger->Log(DEBUG) << "Render subtitle '" << clip->Text() << "', ASS_Image ptr=" << renderRes << ", detectChanged=" << detectChange << "." << endl;
    ImGui::ImMat vmat;
    if (!renderRes)
        return SubtitleImage(vmat, {0});

    // calculate the containing box
    ASS_Image* assImage = renderRes;
    SubtitleImage::Rect containBox{assImage->dst_x, assImage->dst_y, assImage->w, assImage->h};
    assImage = assImage->next;
    while (assImage)
    {
        if (assImage->dst_x < containBox.x)
        {
            containBox.w += containBox.x-assImage->dst_x;
            containBox.x = assImage->dst_x;
        }
        if (assImage->dst_x+assImage->w > containBox.x+containBox.w)
        {
            containBox.w = assImage->dst_x+assImage->w-containBox.x;
        }
        if (assImage->dst_y < containBox.y)
        {
            containBox.h += containBox.y-assImage->dst_y;
            containBox.y = assImage->dst_y;
        }
        if (assImage->dst_y+assImage->h > containBox.y+containBox.h)
        {
            containBox.h = assImage->dst_y+assImage->h-containBox.y;
        }
        assImage = assImage->next;
    }

    int frmW = (int)m_frmW;
    int frmH = (int)m_frmH;
    if (!m_outputFullSize)
    {
        frmW = containBox.w;
        frmH = containBox.h;
    }
    vmat.create_type((int)frmW, (int)frmH, 4, IM_DT_INT8);
    vmat.color_format = IM_CF_RGBA;

    uint32_t color;
    // fill the image with background color
    const SubtitleClip::Color bgColor = clip->BackgroundColor();
    color = ((uint32_t)(bgColor.a*255)<<24) | ((uint32_t)(bgColor.b*255)<<16) | ((uint32_t)(bgColor.g*255)<<8) | (uint32_t)(bgColor.r*255);
    WrapperAlloc<uint32_t> wrapperAlloc((uint32_t*)vmat.data);
    vector<uint32_t, WrapperAlloc<uint32_t>> mapary(wrapperAlloc);
    mapary.resize(vmat.total()/4);
    fill(mapary.begin(), mapary.end(), color);

    // draw ASS_Image list
    assImage = renderRes;
    while (assImage)
    {
        color = assImage->color;
        float baseAlpha = (float)(255-(color&0xff))/255;
        color = ((color&0xff00)<<8) | ((color>>8)&0xff00) | ((color>>24)&0xff);
        uint32_t* linePtr;
        if (m_outputFullSize)
            linePtr = (uint32_t*)(vmat.data)+assImage->dst_y*frmW+assImage->dst_x;
        else
            linePtr = (uint32_t*)(vmat.data)+(assImage->dst_y-containBox.y)*frmW+(assImage->dst_x-containBox.x);
        unsigned char* assPtr = assImage->bitmap;
        for (int i = 0; i < assImage->h; i++)
        {
            for (int j = 0; j < assImage->w; j++)
            {
                const unsigned char b = assPtr[j];
                if (b > 0)
                {
                    uint32_t alpha = (uint32_t)(baseAlpha*b);
                    linePtr[j] = color | (alpha<<24);
                }
            }
            linePtr += frmW;
            assPtr += assImage->stride;
        }
        assImage = assImage->next;
    }

    return SubtitleImage(vmat, containBox);
}

void SubtitleTrack_AssImpl::ClearRenderCache()
{
    for (auto clip : m_clips)
        clip->InvalidateImage();
}

void SubtitleTrack_AssImpl::ToggleOverrideStyle()
{
    int bit = ASS_OVERRIDE_DEFAULT;
    m_useOverrideStyle = !m_useOverrideStyle;
    if (m_useOverrideStyle)
        bit = ASS_OVERRIDE_FULL_STYLE;
    ass_set_selective_style_override_enabled(m_assrnd, bit);
}

SubtitleTrackHolder SubtitleTrack_AssImpl::BuildFromFile(int64_t id, const string& url)
{
    ALogger* logger = GetSubtitleTrackLogger();
    SubtitleTrack_AssImpl* asssubtrk = new SubtitleTrack_AssImpl(id);
    SubtitleTrackHolder hSubTrk = SubtitleTrackHolder(asssubtrk);
    if (!asssubtrk->InitAss())
    {
        logger->Log(Error) << hSubTrk->GetError() << endl;
        return nullptr;
    }

    bool success = asssubtrk->ReadFile(url);
    asssubtrk->ReleaseFFContext();

    if (!success)
    {
        logger->Log(Error) << asssubtrk->GetError() << endl;
        return nullptr;
    }
    return hSubTrk;
}

SubtitleTrack_AssImpl::AssStyleWrapper::AssStyleWrapper(ASS_Style* style)
{
    memcpy(&m_style, style, sizeof(m_style));
    int l = strlen(style->Name);
    m_name = unique_ptr<char[]>(new char[l+1]);
    snprintf(m_name.get(), l+1, "%s", style->Name);
    m_style.Name = m_name.get();
    l = strlen(style->FontName);
    m_fontName = unique_ptr<char[]>(new char[l+1]);
    snprintf(m_fontName.get(), l+1, "%s", style->FontName);
    m_style.FontName = m_fontName.get();
}

SubtitleTrack_AssImpl::AssStyleWrapper::AssStyleWrapper(const SubtitleTrack_AssImpl::AssStyleWrapper& a)
{
    memcpy(&m_style, &a.m_style, sizeof(m_style));
    int l = strlen(a.m_style.Name);
    m_name = unique_ptr<char[]>(new char[l+1]);
    snprintf(m_name.get(), l+1, "%s", a.m_style.Name);
    m_style.Name = m_name.get();
    l = strlen(a.m_style.FontName);
    m_fontName = unique_ptr<char[]>(new char[l+1]);
    snprintf(m_fontName.get(), l+1, "%s", a.m_style.FontName);
    m_style.FontName = m_fontName.get();
}

SubtitleTrack_AssImpl::AssStyleWrapper&
SubtitleTrack_AssImpl::AssStyleWrapper::operator=(const SubtitleTrack_AssImpl::AssStyleWrapper& a)
{
    memcpy(&m_style, &a.m_style, sizeof(m_style));
    int l = strlen(a.m_style.Name);
    m_name = unique_ptr<char[]>(new char[l+1]);
    snprintf(m_name.get(), l+1, "%s", a.m_style.Name);
    m_style.Name = m_name.get();
    l = strlen(a.m_style.FontName);
    m_fontName = unique_ptr<char[]>(new char[l+1]);
    snprintf(m_fontName.get(), l+1, "%s", a.m_style.FontName);
    m_style.FontName = m_fontName.get();
    return *this;
}

void SubtitleTrack_AssImpl::AssStyleWrapper::SetFont(const string& font)
{
    int l = font.size();
    unique_ptr<char[]> newfont(new char[l+1]);
    snprintf(newfont.get(), l+1, "%s", font.c_str());
    m_fontName = move(newfont);
    m_style.FontName = m_fontName.get();
}
