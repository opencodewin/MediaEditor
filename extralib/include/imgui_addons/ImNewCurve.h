#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <list>
#include <functional>
#include <imgui.h>
#include <imgui_json.h>

namespace ImGui
{
namespace ImNewCurve
{

#define IMNEWCURVE_EDITOR_FLAG_NONE                 (0)
#define IMNEWCURVE_EDITOR_FLAG_VALUE_LIMITED        (1)
#define IMNEWCURVE_EDITOR_FLAG_ZOOM_V               (1<<1)
#define IMNEWCURVE_EDITOR_FLAG_SCROLL_V             (1<<2)
#define IMNEWCURVE_EDITOR_FLAG_MOVE_CURVE_V         (1<<3)
#define IMNEWCURVE_EDITOR_FLAG_KEEP_BEGIN_END       (1<<4)
#define IMNEWCURVE_EDITOR_FLAG_DOCK_BEGIN_END       (1<<5)
// #define CURVE_EDIT_FLAG_DRAW_TIMELINE   (1<<5)

    enum CurveType
    {
        UnKnown = -1,
        Hold,
        Step,
        Linear,
        Smooth,
        QuadIn,
        QuadOut,
        QuadInOut,
        CubicIn,
        CubicOut,
        CubicInOut,
        SineIn,
        SineOut,
        SineInOut,
        ExpIn,
        ExpOut,
        ExpInOut,
        CircIn,
        CircOut,
        CircInOut,
        ElasticIn,
        ElasticOut,
        ElasticInOut,
        BackIn,
        BackOut,
        BackInOut,
        BounceIn,
        BounceOut,
        BounceInOut
    };

    const std::vector<std::string>& GetCurveTypeNames();

    enum ValueDimension
    {
        DIM_X = 0,
        DIM_Y,
        DIM_Z,
        DIM_T,
    };

    struct IMGUI_API KeyPoint
    {
        using Holder = std::shared_ptr<KeyPoint>;
        using ValType = ImVec4;
        static Holder CreateInstance();
        static Holder CreateInstance(const ValType& val, CurveType type = UnKnown);

        float &x, &y, &z, &t;
        ValType val {0, 0, 0, 0};
        CurveType type {UnKnown};

        KeyPoint() : x(val.x), y(val.y), z(val.z), t(val.w) {}
        KeyPoint(const KeyPoint& a) : val(a.val), type(a.type), x(val.x), y(val.y), z(val.z), t(val.w) {}
        KeyPoint(const ValType& _val, CurveType _type) : val(_val), type(_type), x(val.x), y(val.y), z(val.z), t(val.w) {}
        KeyPoint& operator=(const KeyPoint& a) { val = a.val; type = a.type; return *this; }

        ImVec2 GetVec2PointValByDim(ValueDimension eDim) const
        {
            if (eDim == DIM_X)
                return ImVec2(t, x);
            else if (eDim == DIM_Y)
                return ImVec2(t, y);
            else
                return ImVec2(t, z);
        }

        ImVec2 GetVec2PointValByDim(ValueDimension eDim1, ValueDimension eDim2) const
        {
            float x_, y_;
            if (eDim1 == DIM_X)
                x_ = x;
            else if (eDim1 == DIM_Y)
                x_ = y;
            else if (eDim1 == DIM_Z)
                x_ = z;
            else
                x_ = t;
            if (eDim2 == DIM_X)
                y_ = x;
            else if (eDim2 == DIM_Y)
                y_ = y;
            else if (eDim2 == DIM_Z)
                y_ = z;
            else
                y_ = t;
            return ImVec2(x_, y_);
        }

        imgui_json::value SaveAsJson() const;
        void LoadFromJson(const imgui_json::value& j);

        static inline float GetDimVal(const ValType& v, ValueDimension eDim)
        {
            if (eDim == DIM_X)
                return v.x;
            else if (eDim == DIM_Y)
                return v.y;
            else if (eDim == DIM_Z)
                return v.z;
            else
                return v.w;
        }

        static inline void SetDimVal(ValType& v, float f, ValueDimension eDim)
        {
            if (eDim == DIM_X)
                v.x = f;
            else if (eDim == DIM_Y)
                v.y = f;
            else if (eDim == DIM_Z)
                v.z = f;
            else
                v.w = f;
        }
    };

    inline KeyPoint::ValType Min(const KeyPoint::ValType& a, const KeyPoint::ValType& b)
    { return KeyPoint::ValType(a.x<b.x?a.x:b.x, a.y<b.y?a.y:b.y, a.z<b.z?a.z:b.z, a.w<b.w?a.w:b.w); }

    inline KeyPoint::ValType Max(const KeyPoint::ValType& a, const KeyPoint::ValType& b)
    { return KeyPoint::ValType(a.x>b.x?a.x:b.x, a.y>b.y?a.y:b.y, a.z>b.z?a.z:b.z, a.w>b.w?a.w:b.w); }

    class IMGUI_API Curve
    {
    public:
        using Holder = std::shared_ptr<Curve>;
        static Holder CreateInstance(const std::string& name, CurveType eCurveType, const std::pair<uint32_t, uint32_t>& tTimeBase,
                const KeyPoint::ValType& minVal, const KeyPoint::ValType& maxVal, const KeyPoint::ValType& defaultVal, bool bAddInitKp = false);
        static Holder CreateInstance(const std::string& name, CurveType eCurveType,
                const KeyPoint::ValType& minVal, const KeyPoint::ValType& maxVal, const KeyPoint::ValType& defaultVal, bool bAddInitKp = false);
        static Holder CreateFromJson(const imgui_json::value& j);

        Curve(const std::string& name, CurveType eCurveType, const std::pair<uint32_t, uint32_t>& tTimeBase,
                const KeyPoint::ValType& minVal, const KeyPoint::ValType& maxVal, const KeyPoint::ValType& defaultVal, bool bAddInitKp = false);
        Curve(const std::string& name, CurveType eCurveType,
                const KeyPoint::ValType& minVal, const KeyPoint::ValType& maxVal, const KeyPoint::ValType& defaultVal, bool bAddInitKp = false)
            : Curve(name, eCurveType, {0, 0}, minVal, maxVal, defaultVal, bAddInitKp) {}
        virtual Holder Clone() const;

        const std::string& GetName() const { return m_strName; }
        CurveType GetCurveType() const { return m_eCurveType; }
        const KeyPoint::ValType& GetMinVal() const { return m_tMinVal; }
        const KeyPoint::ValType& GetMaxVal() const { return m_tMaxVal; }
        const KeyPoint::ValType& GetDefaultVal() const { return m_tDefaultVal; }
        const KeyPoint::ValType& GetValueRange() const { return m_tValRange; }
        const std::pair<uint32_t, uint32_t>& GetTimeBase() const { return m_tTimeBase; }
        size_t GetKeyPointCount() const { return m_aKeyPoints.size(); }
        std::vector<float> GetKeyTimes() const;
        std::vector<float> GetKeyTicks() const;
        const KeyPoint::Holder GetKeyPoint(size_t idx) const;
        int GetKeyPointIndex(const KeyPoint::Holder& hKp) const;
        int GetKeyPointIndexByTick(float fTick) const;
        int GetKeyPointIndex(float t) const;
        KeyPoint::ValType CalcPointVal(float t, bool bAlignTime = true) const;
        void SetTimeBase(const std::pair<uint32_t, uint32_t>& tTimeBase);
        float Tick2Time(float tick) const { if (m_bTimeBaseValid) return tick*1000*m_tTimeBase.first/m_tTimeBase.second; return tick; }
        float Time2Tick(float time) const { if (m_bTimeBaseValid) return time*m_tTimeBase.second/(m_tTimeBase.first*1000); return time; }
        float Time2TickAligned(float time) const { if (m_bTimeBaseValid) return roundf(time*m_tTimeBase.second/(m_tTimeBase.first*1000)); return time; }

        enum {
            FLAGS_NO_CLIP = 0,
            FLAGS_CLIP_MIN = 1,
            FLAGS_CLIP_MAX = 2,
            FLAGS_CLIP_MINMAX = FLAGS_CLIP_MIN | FLAGS_CLIP_MAX,
        };
        void SetClipKeyPointTime(uint8_t u8Flags, bool bClipKeyPoints = false);
        bool IsClipKeyPointTIme(uint8_t u8TestFlags) const { return (m_u8ClipKpTimeFlags&u8TestFlags) == u8TestFlags; }
        void SetClipKeyPointValue(uint8_t u8Flags, bool bClipKeyPoints = false);
        bool IsClipKeyPointValue(uint8_t u8TestFlags) const { return (m_u8ClipKpValueFlags&u8TestFlags) == u8TestFlags; }
        void SetClipOutputValue(uint8_t u8Flags) { m_u8ClipOutValueFlags = u8Flags; }
        bool IsClipOutputValue(uint8_t u8TestFlags) const { return (m_u8ClipOutValueFlags&u8TestFlags) == u8TestFlags; }

        void SetMinVal(const KeyPoint::ValType& minVal);  // only set the minimum key point value (i.e., min value for DIM X,Y,Z), do not change time range start point
        void SetMaxVal(const KeyPoint::ValType& maxVal);  // only set the maximum key point value (i.e., max value for DIM X,Y,Z), do not change time range end point
        virtual int AddPoint(KeyPoint::Holder hKp, bool bOverwriteIfExists = true);
        virtual int AddPointByDim(ValueDimension eDim, const ImVec2& v2DimVal, CurveType eCurveType = UnKnown, bool bOverwriteIfExists = true);
        virtual int EditPoint(size_t idx, const KeyPoint::ValType& tKpVal, CurveType eCurveType = UnKnown);
        virtual int EditPointByDim(ValueDimension eDim, size_t idx, const ImVec2& v2DimVal, CurveType eCurveType = UnKnown);
        virtual int ChangeKeyPointCurveType(size_t idx, CurveType eCurveType);
        virtual KeyPoint::Holder RemovePoint(size_t idx);
        virtual KeyPoint::Holder RemovePoint(float t);
        virtual void ClearAll();
        virtual float MoveVerticallyByDim(ValueDimension eDim, const ImVec2& v2SyncPoint);
        virtual bool SetTimeRange(const ImVec2& v2TimeRange);
        virtual bool ScaleKeyPoints(const KeyPoint::ValType& tScale, const KeyPoint::ValType& tOrigin = KeyPoint::ValType(0, 0, 0, 0));
        virtual bool PanKeyPoints(const KeyPoint::ValType& tOffset);

        std::string PrintKeyPointsByDim(ValueDimension eDim) const;
        virtual imgui_json::value SaveAsJson() const;
        virtual void LoadFromJson(const imgui_json::value& j);

        std::list<KeyPoint::Holder>::iterator GetKpIter(size_t idx);

        struct Callbacks
        {
            virtual void OnKeyPointAdded(size_t szKpIdx, KeyPoint::Holder hKp) = 0;
            virtual void OnKeyPointRemoved(size_t szKpIdx, KeyPoint::Holder hKp) = 0;
            virtual void OnKeyPointChanged(size_t szKpIdx, KeyPoint::Holder hKp) = 0;
            virtual void OnPanKeyPoints(const KeyPoint::ValType& tOffset) = 0;
            virtual void OnContourNeedUpdate(size_t szKpIdx) = 0;
        };
        void RigisterCallbacks(Callbacks* pCb);
        void UnrigsterCallbacks(Callbacks* pCb);

    protected:
        Curve() : m_bTimeBaseValid(false), m_tTimeBase(0, 0) {}
        void SortKeyPoints();
        KeyPoint::Holder GetKeyPoint_(size_t idx) const;

    protected:
        std::string m_strName;
        std::list<KeyPoint::Holder> m_aKeyPoints;
        CurveType m_eCurveType{Smooth};
        KeyPoint::ValType m_tMinVal;
        KeyPoint::ValType m_tMaxVal;
        KeyPoint::ValType m_tValRange;
        KeyPoint::ValType m_tDefaultVal;
        std::pair<uint32_t, uint32_t> m_tTimeBase;
        bool m_bTimeBaseValid;
        uint8_t m_u8ClipKpTimeFlags{0};
        uint8_t m_u8ClipKpValueFlags{0};
        uint8_t m_u8ClipOutValueFlags{0};
        std::list<Callbacks*> m_aCallbacksArray;
    };

    IMGUI_API bool DrawCurveArraySimpleView(float fViewWidth, const std::vector<Curve::Holder>& aCurves, float& fCurrTime, const ImVec2& v2TimeRange = ImVec2(0,0), ImGuiKey eRemoveKey = ImGuiKey_LeftAlt);

    class IMGUI_API Editor
    {
    public:
        using Holder = std::shared_ptr<Editor>;
        static Holder CreateInstance();

        Editor(const Editor&) = delete;

        bool DrawContent(const char* pcLabel, const ImVec2& v2ViewSize, float fViewScaleX, float fViewOffsetX, uint32_t flags, bool* pCurveUpdated = nullptr, ImDrawList* pDrawList = nullptr);

        bool AddCurve(Curve::Holder hCurve, ValueDimension eDim, ImU32 u32CurveColor);
        void ClearAll() { m_aCurveUiObjs.clear(); }
        size_t GetCurveCount() const { return m_aCurveUiObjs.size(); }
        Curve::Holder GetCurveByIndex(size_t idx) const;

        void SetGraticuleLineCount(size_t szLineCnt) { m_szGraticuleLineCnt = szLineCnt; }
        size_t GetGraticuleLineCount() const { return m_szGraticuleLineCnt; }
        void SetBackgroundColor(ImU32 color) { m_u32BackgroundColor = color; }
        ImU32 GetBackgroundColor() const { return m_u32BackgroundColor; }
        void SetGraticuleColor(ImU32 color) { m_u32GraticuleColor = color; }
        ImU32 GetGraticuleColor() const { return m_u32GraticuleColor; }
        void SetShowValueToolTip(bool bShow) { m_bShowValueToolTip = bShow; }
        bool IsShowValueToolTip() const { return m_bShowValueToolTip; }
        bool SetCurveVisible(size_t idx, bool bVisible);
        bool IsCurveVisible(size_t idx) const;

        inline ImVec2 CvtPoint2Pos(const ImVec2& v2NormedPointVal) const
        { return v2NormedPointVal*m_v2UiScale*m_v2CurveAxisAreaSize; }

        inline ImVec2 CvtPos2Point(const ImVec2& v2Pos) const
        { return v2Pos/(m_v2CurveAxisAreaSize*m_v2UiScale); }

        imgui_json::value SaveStateAsJson() const;
        void RestoreStateFromJson(const imgui_json::value& j);

    private:
        class CurveUiObj : public Curve::Callbacks
        {
        public:
            using Holder = std::shared_ptr<CurveUiObj>;
            CurveUiObj(Editor* pOwner, Curve::Holder hCurve, ValueDimension eDim, ImU32 u32CurveColor);
            ~CurveUiObj();
            void DrawCurve(ImDrawList* pDrawList, const ImVec2& v2OriginPos, bool bIsHovering) const;
            Curve::Holder GetCurve() const { return m_hCurve; }
            int MoveKeyPoint(int iKpIdx, const ImVec2& v2MousePos);
            void MoveCurveVertically(const ImVec2& v2MousePos);
            void UpdateCurveAttributes();
            void UpdateContourPoints(int iKpIdx);
            bool CheckMouseHoverCurve(const ImVec2& v2MousePos) const;
            int CheckMouseHoverPoint(const ImVec2& v2MousePos) const;
            void ScaleContour(const ImVec2& v2Scale);
            void PanContour(const ImVec2& v2Offset);
            void SetVisible(bool bVisible) { m_bVisible = bVisible; }
            bool IsVisible() const { return m_bVisible; }
            void SetRefreshContour() { m_bNeedRefreshContour = true; }
            bool NeedRrefreshContour() const { return m_bNeedRefreshContour; }
            std::string GetCurveName() const { return m_hCurve->GetName(); }
            ValueDimension GetDim() const { return m_eDim; }
            ImVec2 CalcPointValue(const ImVec2& v2MousePos) const;
            void SetCurveColor(ImU32 u32CurveColor);
            ImU32 GetCurveColor() const { return m_u32CurveColor; }

            void OnKeyPointAdded(size_t szKpIdx, KeyPoint::Holder hKp) override;
            void OnKeyPointRemoved(size_t szKpIdx, KeyPoint::Holder hKp) override;
            void OnKeyPointChanged(size_t szKpIdx, KeyPoint::Holder hKp) override;
            void OnPanKeyPoints(const KeyPoint::ValType& tOffset) override;
            void OnContourNeedUpdate(size_t szKpIdx) override;

        private:
            using ContourPointsTable = std::unordered_map<KeyPoint::Holder, std::vector<ImVec2>>;
            ContourPointsTable m_aCpTable;

        private:
            Editor* m_pOwner;
            Curve::Holder m_hCurve;
            ValueDimension m_eDim;
            ImVec2 m_v2DimValRange, m_v2DimMinVal;
            bool m_bVisible{true};
            bool m_bNeedRefreshContour{true};
            ImU32 m_u32CurveColor, m_u32CurveHoverColor;
        };

    private:
        Editor();

    private:
        std::vector<CurveUiObj::Holder> m_aCurveUiObjs;
        ImVec2 m_v2CurveAreaSize, m_v2CurveAxisAreaSize;
        ImVec2 m_v2UiScale{1.f, 1.f};
        float m_fMaxScale{10.f};
        ImVec2 m_v2PanOffset{0.f, 0.f};
        float m_fCheckHoverDistanceThresh{8.f};
        int m_iHoveredCurveUiObjIdx{-1};
        int m_iHoveredKeyPointIdx{-1};
        bool m_bIsDragging{false};
        bool m_bShowValueToolTip{false};
        ImVec2 m_v2ViewPadding{0, 2};
        ImU32 m_u32BackgroundColor{IM_COL32(24, 24, 24, 255)};
        ImU32 m_u32GraticuleColor{IM_COL32(48, 48, 48, 128)};
        size_t m_szGraticuleLineCnt{10};
        float m_fGraticuleLineThickness{1.f};
        float m_fCurveWidth{1.5f}, m_fCurveHoverWidth{2.5f};
        ImU32 m_u32KeyPointColor{IM_COL32(40, 40, 40, 255)};
        ImU32 m_u32KeyPointHoverColor{IM_COL32(240, 240, 240, 255)};
        ImU32 m_u32KeyPointEdgeColor{IM_COL32(255, 128, 0, 255)};
        float m_fKeyPointRadius{3.5f};
        float m_fCurveAreaTopMargin{0.05f}, m_fCurveAreaBottomMargin{0.05f};
        ImU32 m_u32MaxValLineColor{IM_COL32(220, 140, 140, 180)};
        ImU32 m_u32MinValLineColor{IM_COL32(160, 160, 220, 180)};
        float m_fMinMaxLineThickness{1.5f};
    };

#if IMGUI_BUILD_EXAMPLE
    IMGUI_API void ShowDemo();
#endif

}  // ~ namespace ImCurve
}  // ~ namespace ImGui
