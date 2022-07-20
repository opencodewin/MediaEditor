#pragma once
#include <list>
#include "SubtitleTrack.h"
extern "C"
{
    #include "libavformat/avformat.h"
    #include "libavcodec/avcodec.h"
    #include "ass/ass.h"
}

namespace DataLayer
{
    class SubtitleTrack_AssImpl : public SubtitleTrack
    {
    public:
        SubtitleTrack_AssImpl(int64_t id);
        ~SubtitleTrack_AssImpl();

        SubtitleTrack_AssImpl(const SubtitleTrack_AssImpl&) = delete;
        SubtitleTrack_AssImpl(SubtitleTrack_AssImpl&&) = delete;
        SubtitleTrack_AssImpl& operator=(const SubtitleTrack_AssImpl&) = delete;

        bool InitAss();

        bool SetFrameSize(uint32_t width, uint32_t height) override;
        bool SetBackgroundColor(const SubtitleClip::Color& color) override;
        bool SetFont(const std::string& font) override;
        bool SetFontScale(double scale) override;

        SubtitleClipHolder GetClipByTime(int64_t ms) override;
        SubtitleClipHolder GetCurrClip() override;
        SubtitleClipHolder GetPrevClip() override;
        SubtitleClipHolder GetNextClip() override;
        int32_t GetClipIndex(SubtitleClipHolder clip) const override;
        bool SeekToTime(int64_t ms) override;
        bool SeekToIndex(uint32_t index) override;
        uint32_t ClipCount() const override { return m_clips.size(); }
        int64_t Duration() const override { return m_duration; }

        std::string GetError() const override { return m_errMsg; }

        static bool Initialize();
        static void Release();
        static bool SetFontDir(const std::string& path);
        static SubtitleTrackHolder BuildFromFile(int64_t id, const std::string& url);

    private:
        bool ReadFile(const std::string& path);
        void ReleaseFFContext();
        SubtitleImage RenderSubtitleClip(SubtitleClip* clip);

    private:
        Logger::ALogger* m_logger;
        std::string m_errMsg;
        int64_t m_id;
        std::string m_path;
        int64_t m_readPos{0};
        std::list<SubtitleClipHolder> m_clips;
        std::list<SubtitleClipHolder>::iterator m_currIter;
        int64_t m_duration{-1};
        ASS_Track* m_asstrk{nullptr};
        ASS_Renderer* m_assrnd{nullptr};
        uint32_t m_frmW{0}, m_frmH{0};
        SubtitleClip::Color m_bgColor{0, 0, 0, 0};

        AVFormatContext* m_pAvfmtCtx{nullptr};
        AVCodecContext* m_pAvCdcCtx{nullptr};

        static ASS_Library* s_asslib;
    };
}