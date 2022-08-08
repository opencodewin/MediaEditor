#pragma once
#include "ass/ass_types.h"
#include "SubtitleClip.h"

namespace DataLayer
{
    class SubtitleClip_AssImpl;
    using AssRenderCallback = std::function<SubtitleImage(SubtitleClip_AssImpl*)>;

    class SubtitleClip_AssImpl : public SubtitleClip
    {
    public:
        SubtitleClip_AssImpl(ASS_Event* assEvent, ASS_Track* assTrack, AssRenderCallback renderCb);

        SubtitleClip_AssImpl(const SubtitleClip_AssImpl&) = delete;
        SubtitleClip_AssImpl(SubtitleClip_AssImpl&&) = delete;
        SubtitleClip_AssImpl& operator=(const SubtitleClip_AssImpl&) = delete;

        SubtitleType Type() const override { return m_type; }
        bool IsUsingTrackStyle() const override { return m_useTrackStyle; }
        std::string TrackStyle() const override { return m_trackStyle; }
        std::string Font() const override { return m_font; }
        // uint32_t FontSize() const override { return m_fontSize; }
        double ScaleX() const override { return m_scaleX; }
        double ScaleY() const override { return m_scaleY; }
        double Spacing() const override { return m_spacing; }
        SubtitleColor PrimaryColor() const override { return m_primaryColor; }
        SubtitleColor SecondaryColor() const override { return m_secondaryColor; }
        SubtitleColor OutlineColor() const override { return m_outlineColor; }
        SubtitleColor BackgroundColor() const override { return m_bgColor; }
        bool Bold() const override { return m_bold; }
        bool Italic() const override { return m_italic; }
        bool Underline() const override { return m_underline; }
        bool StrikeOut() const override { return m_strikeout; }
        uint32_t BorderWidth() const override { return m_borderWidth; }
        // uint32_t ShadowDepth() const override { return m_shadowDepth; }
        bool BlurEdge() const override { return m_blurEdge; }
        double RotationX() const override { return m_rotationX; }
        double RotationY() const override { return m_rotationY; }
        double RotationZ() const override { return m_rotationZ; }
        int32_t OffsetH() const override { return m_offsetH; }
        int32_t OffsetV() const override { return m_offsetV; }
        uint32_t Alignment() const override { return m_alignment; }
        int64_t StartTime() const override { return m_assEvent->Start; }
        int64_t Duration() const override { return m_assEvent->Duration; }
        int64_t EndTime() const override { return m_assEvent->Start+m_assEvent->Duration; }
        std::string Text() const override { return m_text; }
        SubtitleImage Image() override;

        void EnableUsingTrackStyle(bool enable) override;
        void SetTrackStyle(const std::string& name) override;
        void SyncStyle(const SubtitleStyle& style) override;
        void SetFont(const std::string& font) override;
        // void SetFontSize(uint32_t value) override;
        void SetScaleX(double value) override;
        void SetScaleY(double value) override;
        void SetSpacing(double value) override;
        void SetPrimaryColor(const SubtitleColor& color) override;
        void SetSecondaryColor(const SubtitleColor& color) override;
        void SetOutlineColor(const SubtitleColor& color) override;
        void SetBackgroundColor(const SubtitleColor& color) override;
        void SetBold(bool enable) override;
        void SetItalic(bool enable) override;
        void SetUnderline(bool enable) override;
        void SetStrikeOut(bool enable) override;
        void SetBorderWidth(uint32_t value) override;
        // void SetShadowDepth(uint32_t value) override;
        void SetBlurEdge(bool enable) override;
        void SetRotationX(double value) override;
        void SetRotationY(double value) override;
        void SetRotationZ(double value) override;
        void SetOffsetH(int32_t value) override { m_offsetH = value; }
        void SetOffsetV(int32_t value) override { m_offsetV = value; }
        void SetAlignment(uint32_t value) override;
        void SetText(const std::string& text) override;
        void InvalidateImage() override;

        void SetRenderCallback(AssRenderCallback renderCb) { m_renderCb = renderCb; }
        void SetAssEvent(ASS_Event* assEvent) { m_assEvent = assEvent; m_readOrder = assEvent->ReadOrder; }
        int ReadOrder() const { return m_readOrder; }
        void SetStartTime(int64_t startTime);
        void SetDuration(int64_t duration);
        std::string GenerateAssChunk();
        std::string GenerateStyledText();
        std::string GetAssText();
        bool IsAssTextChanged() const { return m_assTextChanged; }
        void UpdateImageAreaX(int32_t bias);
        void UpdateImageAreaY(int32_t bias);

    private:
        SubtitleType m_type;
        bool m_useTrackStyle{true};
        std::string m_trackStyle{"Default"};
        std::string m_font{"Arial"};
        // uint32_t m_fontSize{22};
        double m_scaleX{1};
        double m_scaleY{1};
        double m_spacing{0};
        SubtitleColor m_primaryColor{1,1,1,1};
        SubtitleColor m_secondaryColor{1,1,1,1};
        SubtitleColor m_outlineColor{0,0,0,1};
        SubtitleColor m_bgColor{0,0,0,0};
        bool m_bold{false};
        bool m_italic{false};
        bool m_underline{false};
        bool m_strikeout{false};
        uint32_t m_borderWidth{1};
        uint32_t m_shadowDepth{0};
        bool m_blurEdge{false};
        double m_rotationX{0};
        double m_rotationY{0};
        double m_rotationZ{0};
        int32_t m_offsetH{0};
        int32_t m_offsetV{0};
        uint32_t m_alignment{2};
        std::string m_text;
        std::string m_styledText;
        bool m_assTextChanged{false};
        SubtitleImage m_image;

        ASS_Track* m_assTrack{nullptr};
        ASS_Event* m_assEvent{nullptr};
        int m_readOrder;
        AssRenderCallback m_renderCb;
    };
}
