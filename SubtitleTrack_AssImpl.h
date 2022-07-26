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

        int64_t Id() const override { return m_id; }
        uint32_t ClipCount() const override { return m_clips.size(); }
        int64_t Duration() const override { return m_duration; }

        bool SetFrameSize(uint32_t width, uint32_t height) override;
        bool EnableFullSizeOutput(bool enable) override;
        bool SetBackgroundColor(const SubtitleClip::Color& color) override;
        bool SetFont(const std::string& font) override;
        bool SetScale(double value) override;
        bool SetScaleX(double value) override;
        bool SetScaleY(double value) override;
        bool SetSpacing(double value) override;
        bool SetAngle(double value) override;
        bool SetOutline(double value) override;
        bool SetAlignment(int value) override;
        bool SetMarginL(int value) override;
        bool SetMarginR(int value) override;
        bool SetMarginV(int value) override;
        bool SetItalic(int value) override;
        bool SetBold(int value) override;
        bool SetUnderLine(bool enable) override;
        bool SetStrikeOut(bool enable) override;
        bool SetPrimaryColor(const SubtitleClip::Color& color) override;
        bool SetSecondaryColor(const SubtitleClip::Color& color) override;
        bool SetOutlineColor(const SubtitleClip::Color& color) override;

        SubtitleClipHolder NewClip(int64_t startTime, int64_t duration) override;
        SubtitleClipHolder GetClipByTime(int64_t ms) override;
        SubtitleClipHolder GetCurrClip() override;
        SubtitleClipHolder GetPrevClip() override;
        SubtitleClipHolder GetNextClip() override;
        int32_t GetClipIndex(SubtitleClipHolder clip) const override;
        bool SeekToTime(int64_t ms) override;
        bool SeekToIndex(uint32_t index) override;

        bool ChangeText(uint32_t clipIndex, const std::string& text) override;
        bool ChangeText(SubtitleClipHolder clip, const std::string& text) override;

        std::string GetError() const override { return m_errMsg; }

        static bool Initialize();
        static void Release();
        static bool SetFontDir(const std::string& path);
        static SubtitleTrackHolder BuildFromFile(int64_t id, const std::string& url);
        static SubtitleTrackHolder NewEmptyTrack(int64_t id);

    private:
        bool ReadFile(const std::string& path);
        void ReleaseFFContext();
        SubtitleImage RenderSubtitleClip(SubtitleClip* clip);
        void ClearRenderCache();
        void ToggleOverrideStyle();
        void ResetClipListReadOrder();

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
        bool m_outputFullSize{true};
        SubtitleClip::Color m_bgColor{0, 0, 0, 0};

        class AssStyleWrapper
        {
        public:
            AssStyleWrapper() = default;
            AssStyleWrapper(AssStyleWrapper&&) = default;
            AssStyleWrapper(const AssStyleWrapper& a);
            AssStyleWrapper& operator=(const AssStyleWrapper& a);

            AssStyleWrapper(ASS_Style* style);
            ASS_Style* GetAssStylePtr() { return &m_style; }
            void SetFont(const std::string& font);

            ASS_Style m_style{0};
        private:
            std::unique_ptr<char[]> m_name;
            std::unique_ptr<char[]> m_fontName;
        };
        AssStyleWrapper m_overrideStyle;
        bool m_useOverrideStyle{false};

        AVFormatContext* m_pAvfmtCtx{nullptr};
        AVCodecContext* m_pAvCdcCtx{nullptr};

        static ASS_Library* s_asslib;
    };
}