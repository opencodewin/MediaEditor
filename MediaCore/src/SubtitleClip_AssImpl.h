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

#pragma once
#include <map>
#include "ass/ass_types.h"
#include "SubtitleClip.h"

namespace MediaCore
{
    class SubtitleClip_AssImpl;
    using AssRenderCallback = std::function<SubtitleImage(SubtitleClip_AssImpl*, int64_t, bool, bool)>;

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
        SubtitleColor BackColor() const override { return m_backColor; }
        SubtitleColor BackgroundColor() const override { return m_bgColor; }
        bool Bold() const override { return m_bold; }
        bool Italic() const override { return m_italic; }
        bool UnderLine() const override { return m_underline; }
        bool StrikeOut() const override { return m_strikeout; }
        double BorderWidth() const override { return m_borderWidth; }
        double ShadowDepth() const override { return m_shadowDepth; }
        bool BlurEdge() const override { return m_blurEdge; }
        double RotationX() const override { return m_rotationX; }
        double RotationY() const override { return m_rotationY; }
        double RotationZ() const override { return m_rotationZ; }
        int32_t OffsetH() const override { return m_offsetH; }
        int32_t OffsetV() const override { return m_offsetV; }
        float OffsetHScale() const override { return m_foffsetH; }
        float OffsetVScale() const override { return m_foffsetV; }
        uint32_t Alignment() const override { return m_alignment; }
        ImGui::KeyPointEditor* GetKeyPoints() override { return &m_keyPoints; }
        int64_t StartTime() const override { return m_assEvent ? m_assEvent->Start : -1; }
        int64_t Duration() const override { return m_assEvent ? m_assEvent->Duration : -1; }
        int64_t EndTime() const override { return m_assEvent ? m_assEvent->Start+m_assEvent->Duration : -1; }
        std::string Text() const override { return m_text; }
        SubtitleImage Image(int64_t timeOffset = 0) override;

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
        void SetBackColor(const SubtitleColor& color) override;
        void SetBackgroundColor(const SubtitleColor& color) override;
        void SetPrimaryColor(const ImVec4& color) override;
        void SetSecondaryColor(const ImVec4& color) override;
        void SetOutlineColor(const ImVec4& color) override;
        void SetBackColor(const ImVec4& color) override;
        void SetBackgroundColor(const ImVec4& color) override;
        void SetBold(bool enable) override;
        void SetItalic(bool enable) override;
        void SetUnderLine(bool enable) override;
        void SetStrikeOut(bool enable) override;
        void SetBorderWidth(double value) override;
        void SetShadowDepth(double value) override;
        void SetBlurEdge(bool enable) override;
        void SetRotationX(double value) override;
        void SetRotationY(double value) override;
        void SetRotationZ(double value) override;
        void SetOffsetH(int32_t value) override;
        void SetOffsetV(int32_t value) override;
        void SetOffsetH(float value) override;
        void SetOffsetV(float value) override;
        void SetAlignment(uint32_t value) override;
        void SetKeyPoints(const ImGui::KeyPointEditor& keyPoints) override;
        void SetText(const std::string& text) override;

        void CloneStyle(SubtitleClipHolder from, double wRatio = 1, double hRatio = 1) override;
        void InvalidateImage() override;

        void SetRenderCallback(AssRenderCallback renderCb) { m_renderCb = renderCb; }
        ASS_Event* AssEventPtr() const { return m_assEvent; }
        void ResyncAssEventPtr(ASS_Event* assEvent);
        void AssEventPtrDecrease() { m_assEvent--; m_readOrder = m_assEvent->ReadOrder; }
        int ReadOrder() const { return m_readOrder; }
        void SetStartTime(int64_t startTime);
        void SetDuration(int64_t duration);
        std::string GenerateAssChunk();
        std::string GenerateStyledText();
        std::string GetAssText();
        bool IsAssTextChanged() const { return m_styledTextNeedUpdate; }
        void UpdateImageAreaX(int32_t bias);
        void UpdateImageAreaY(int32_t bias);
        void InvalidateClip();

    private:
        void _SetScaleX(double value, bool clearCache = true);
        void _SetScaleY(double value, bool clearCache = true);
        void _SetSpacing(double value, bool clearCache = true);
        void _SetBorderWidth(double value, bool clearCache = true);
        void _SetShadowDepth(double value, bool clearCache = true);
        void _SetRotationX(double value, bool clearCache = true);
        void _SetRotationY(double value, bool clearCache = true);
        void _SetRotationZ(double value, bool clearCache = true);
        void _SetOffsetH(int value, bool clearCache = true);
        void _SetOffsetV(int value, bool clearCache = true);
        void _SetOffsetH(float value, bool clearCache = true);
        void _SetOffsetV(float value, bool clearCache = true);

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
        SubtitleColor m_backColor{0,0,0,1};
        SubtitleColor m_bgColor{0,0,0,0};
        bool m_bold{false};
        bool m_italic{false};
        bool m_underline{false};
        bool m_strikeout{false};
        double m_borderWidth{1};
        double m_shadowDepth{0};
        bool m_blurEdge{false};
        double m_rotationX{0};
        double m_rotationY{0};
        double m_rotationZ{0};
        int32_t m_offsetH{0};
        int32_t m_offsetV{0};
        float m_foffsetH{0};
        float m_foffsetV{0};
        uint32_t m_alignment{2};
        ImGui::KeyPointEditor m_keyPoints;
        std::string m_text;
        std::string m_styledText;
        bool m_styledTextNeedUpdate{false};
        std::map<int64_t, SubtitleImage> m_renderedImages;

        ASS_Track* m_assTrack{nullptr};
        ASS_Event* m_assEvent{nullptr};
        int m_readOrder;
        AssRenderCallback m_renderCb;
    };
}
