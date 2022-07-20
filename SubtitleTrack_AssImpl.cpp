#include <sstream>
#include <algorithm>
#include <vector>
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

bool SubtitleTrack_AssImpl::SetBackgroundColor(const SubtitleClip::Color& color)
{
    m_bgColor = color;
    for (SubtitleClipHolder clip : m_clips)
        clip->SetBackgroundColor(color);
    return true;
}

bool SubtitleTrack_AssImpl::SetFont(const std::string& font)
{
    m_logger->Log(DEBUG) << "Set default font as '" << font << "'." << endl;
    ass_set_fonts(m_assrnd, font.c_str(), NULL, ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
    for (auto clip : m_clips)
        clip->InvalidateImage();
    return true;
}

bool SubtitleTrack_AssImpl::SetFontScale(double scale)
{
    ass_set_font_scale(m_assrnd, scale);
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

ASS_Library* SubtitleTrack_AssImpl::s_asslib = nullptr;

static void ass_log(int ass_level, const char *fmt, va_list args, void *ctx)
{
    Logger::Level level = VERBOSE;
    if (ass_level < 2)
        level = Error;
    else if (ass_level < 4)
        level = WARN;
    else if (ass_level < 6)
        level = INFO;
    else if (ass_level < 7)
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
            m_logger->Log(DEBUG) << "[" << MillisecToString(start_time) << "(+" << duration << ")] ";
            for (auto i = 0; i < avsub.num_rects; i++)
            {
                char *ass_line = avsub.rects[i]->ass;
                if (!ass_line)
                    break;
                m_logger->Log(DEBUG) << "<" << i << ">: '" << avsub.rects[i]->ass << "'; ";
            }
            m_logger->Log(DEBUG) << endl;
            for (auto i = 0; i < avsub.num_rects; i++)
            {
                char *ass_line = avsub.rects[i]->ass;
                if (!ass_line)
                    break;
                ass_process_chunk(m_asstrk, ass_line, strlen(ass_line), start_time, duration);
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
        for (int i = 0; i < m_asstrk->n_events; i++)
        {
            ASS_Event* e = m_asstrk->events+i;
            SubtitleClipHolder hSubClip(new SubtitleClip(ASS, e->Start, e->Duration, e->Text));
            hSubClip->SetRenderCallback(bind(&SubtitleTrack_AssImpl::RenderSubtitleClip, this, _1));
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
    ASS_Image* assImage = ass_render_frame(m_assrnd, m_asstrk, clip->StartTime(), &detectChange);
    m_logger->Log(DEBUG) << "Render subtitle '" << clip->Text() << "', ASS_Image ptr=" << assImage << ", detectChanged=" << detectChange << "." << endl;
    if (!assImage)
        return SubtitleImage();
    ImGui::ImMat vmat;
    vmat.create_type((int)m_frmW, (int)m_frmH, 4, IM_DT_INT8);
    vmat.color_format = IM_CF_RGBA;
    uint32_t color;
    const SubtitleClip::Color bgColor = clip->BackgroundColor();
    color = ((uint32_t)(bgColor.a*255)<<24) | ((uint32_t)(bgColor.b*255)<<16) | ((uint32_t)(bgColor.g*255)<<8) | (uint32_t)(bgColor.r*255);
    WrapperAlloc<uint32_t> wrapperAlloc((uint32_t*)vmat.data);
    vector<uint32_t, WrapperAlloc<uint32_t>> mapary(wrapperAlloc);
    mapary.resize(vmat.total()/4);
    fill(mapary.begin(), mapary.end(), color);
    const SubtitleClip::Color textColor = clip->TextColor();
    color = ((uint32_t)(textColor.r*255)<<24) | ((uint32_t)(textColor.g*255)<<16) | ((uint32_t)(textColor.b*255)<<8) | (uint32_t)(textColor.a*255);
    uint32_t* linePtr = (uint32_t*)(vmat.data)+assImage->dst_y*m_frmW+assImage->dst_x;
    unsigned char* assPtr = assImage->bitmap;
    for (int i = 0; i < assImage->h; i++)
    {
        for (int j = 0; j < assImage->w; j++)
        {
            if (assPtr[j] > 0)
                linePtr[j] = color;
        }
        linePtr += m_frmW;
        assPtr += assImage->stride;
    }
    return SubtitleImage(vmat, {assImage->dst_x, assImage->dst_y, assImage->w, assImage->h});
}

static SubtitleType GetSubtitleType(AVSubtitleType subtype)
{
    SubtitleType t;
    switch (subtype)
    {
    case SUBTITLE_BITMAP:
        t = DataLayer::BITMAP;
        break;
    case SUBTITLE_TEXT:
        t = DataLayer::TEXT;
        break;
    case SUBTITLE_ASS:
        t = DataLayer::ASS;
        break;
    default:
        t = DataLayer::UNKNOWN;
        break;
    }
    return t;
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
