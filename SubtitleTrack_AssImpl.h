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
    class SubtitleTrackStyle_AssImpl : public SubtitleTrack::Style
    {
    public:
        SubtitleTrackStyle_AssImpl() = default;
        SubtitleTrackStyle_AssImpl(const ASS_Style* style);
        SubtitleTrackStyle_AssImpl(const SubtitleTrackStyle_AssImpl& a);
        SubtitleTrackStyle_AssImpl(SubtitleTrackStyle_AssImpl&&) = default;
        SubtitleTrackStyle_AssImpl& operator=(const SubtitleTrackStyle_AssImpl& a);

        std::string Font() const override { return std::string(m_style.FontName); }
        double Scale() const override { return m_scale; }
        double ScaleX() const override { return m_style.ScaleX; }
        double ScaleY() const override { return m_style.ScaleY; }
        double Spacing() const override { return m_style.Spacing; }
        double Angle() const override { return m_style.Angle; }
        double OutlineWidth() const override { return m_style.Outline; }
        int Alignment() const override { return m_style.Alignment; }
        int MarginH() const override { return m_style.MarginL; }
        int MarginV() const override { return /*m_marginV*/ m_style.MarginV; }
        int Italic() const override { return m_style.Italic; }
        int Bold() const override { return m_style.Bold; }
        bool UnderLine() const override { return m_style.Underline != 0; }
        bool StrikeOut() const override { return m_style.StrikeOut != 0; }
        SubtitleClip::Color PrimaryColor() const override { return m_primaryColor; }
        SubtitleClip::Color SecondaryColor() const override { return m_secondaryColor; }
        SubtitleClip::Color OutlineColor() const override { return m_outlineColor; }

        void BuildFromAssStyle(const ASS_Style* assStyle);
        ASS_Style* GetAssStylePtr() { return &m_style; }
        void SetFont(const std::string& font);
        void SetScale(double scale) { m_scale = scale; }
        void SetScaleX(double value) { m_style.ScaleX = value; }
        void SetScaleY(double value) { m_style.ScaleY = value; }
        void SetSpacing(double value) { m_style.Spacing = value; }
        void SetAngle(double value) { m_style.Angle = value; }
        void SetOutlineWidth(double value) { m_style.Outline = value; }
        void SetAlignment(int value) { m_style.Alignment = value; }
        void SetMarginH(int value) { m_style.MarginL = value; }
        void SetMarginV(int value) { /* m_marginV = value; */ m_style.MarginV = value; }
        void SetItalic(int value) { m_style.Italic = value; }
        void SetBold(int value) { m_style.Bold = value; }
        void SetUnderLine(bool enable) { m_style.Underline = enable ? 1 : 0; }
        void SetStrikeOut(bool enable) { m_style.StrikeOut = enable ? 1 : 0; }
        void SetPrimaryColor(const SubtitleClip::Color& color);
        void SetSecondaryColor(const SubtitleClip::Color& color);
        void SetOutlineColor(const SubtitleClip::Color& color);

    private:
        ASS_Style m_style;
        std::unique_ptr<char[]> m_name;
        std::unique_ptr<char[]> m_fontName;
        double m_scale{1};
        int m_marginV{0};
        SubtitleClip::Color m_primaryColor;
        SubtitleClip::Color m_secondaryColor;
        SubtitleClip::Color m_outlineColor;
    };

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
        const Style& GetStyle() const override { return m_overrideStyle; }

        bool SetFrameSize(uint32_t width, uint32_t height) override;
        bool EnableFullSizeOutput(bool enable) override;
        bool SetBackgroundColor(const SubtitleClip::Color& color) override;
        bool SetFont(const std::string& font) override;
        bool SetScale(double value) override;
        bool SetScaleX(double value) override;
        bool SetScaleY(double value) override;
        bool SetSpacing(double value) override;
        bool SetAngle(double value) override;
        bool SetOutlineWidth(double value) override;
        bool SetAlignment(int value) override;
        bool SetMarginH(int value) override;
        bool SetMarginV(int value) override;
        bool SetItalic(int value) override;
        bool SetBold(int value) override;
        bool SetUnderLine(bool enable) override;
        bool SetStrikeOut(bool enable) override;
        bool SetPrimaryColor(const SubtitleClip::Color& color) override;
        bool SetSecondaryColor(const SubtitleClip::Color& color) override;
        bool SetOutlineColor(const SubtitleClip::Color& color) override;
        bool ChangeClipTime(SubtitleClipHolder clip, int64_t startTime, int64_t duration) override;

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

        bool m_useOverrideStyle{false};
        SubtitleTrackStyle_AssImpl m_overrideStyle;

        AVFormatContext* m_pAvfmtCtx{nullptr};
        AVCodecContext* m_pAvCdcCtx{nullptr};

        static ASS_Library* s_asslib;
    };
}