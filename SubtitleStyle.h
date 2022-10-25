#pragma once
#include <string>
#include "imgui.h"
#include "imgui_extra_widget.h"
#include "imgui_curve.h"

namespace DataLayer
{
    struct SubtitleColor
    {
        SubtitleColor() = default;
        SubtitleColor(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {}
        ImVec4 ToImVec4() { return ImVec4(r, g, b, a); }
        float r{1};
        float g{1};
        float b{1};
        float a{1};

        bool operator==(const SubtitleColor& c) { return r==c.r && g==c.g && b==c.b && a==c.a; }
    };

    struct SubtitleStyle
    {
        virtual std::string Name() const = 0;
        virtual std::string Font() const = 0;
        virtual double ScaleX() const = 0;
        virtual double ScaleY() const = 0;
        virtual double Spacing() const = 0;
        virtual double Angle() const = 0;
        virtual double OutlineWidth() const = 0;
        virtual double ShadowDepth() const = 0;
        virtual int BorderStyle() const = 0;
        virtual int Alignment() const = 0;  // 1: left; 2: center; 3: right
        virtual int32_t OffsetH() const = 0;
        virtual int32_t OffsetV() const = 0;
        virtual int Italic() const = 0;
        virtual int Bold() const = 0;
        virtual bool UnderLine() const = 0;
        virtual bool StrikeOut() const = 0;
        virtual SubtitleColor PrimaryColor() const = 0;
        virtual SubtitleColor SecondaryColor() const = 0;
        virtual SubtitleColor OutlineColor() const = 0;
        virtual SubtitleColor BackColor() const = 0;
        virtual SubtitleColor BackgroundColor() const = 0;
    };
}