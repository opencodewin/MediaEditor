#ifndef IMGUI_CURVE_H
#define IMGUI_CURVE_H

#include <functional>
#include <vector>
#include <algorithm>
#include <set>
#include <imgui.h>
#include <imgui_json.h>
#include <imgui_helper.h>
#include <imgui_extra_widget.h>

// CurveEdit from https://github.com/CedricGuillemet/ImGuizmo
namespace ImGui
{
struct IMGUI_API ImCurveEdit
{
#define CURVE_EDIT_FLAG_NONE            (0)
#define CURVE_EDIT_FLAG_VALUE_LIMITED   (1)
#define CURVE_EDIT_FLAG_SCROLL_V        (1<<1)
#define CURVE_EDIT_FLAG_MOVE_CURVE      (1<<2)
#define CURVE_EDIT_FLAG_KEEP_BEGIN_END  (1<<3)
#define CURVE_EDIT_FLAG_DOCK_BEGIN_END  (1<<4)
#define CURVE_EDIT_FLAG_DRAW_TIMELINE   (1<<5)

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

    enum ValueDimension
    {
        DIM_X = 0,
        DIM_Y,
        DIM_Z,
        DIM_T,
    };

    struct KeyPoint
    {
        KeyPoint() : x(val.x), y(val.y), z(val.z), t(val.w) {}
        KeyPoint(const KeyPoint& a) : val(a.val), type(a.type), x(val.x), y(val.y), z(val.z), t(val.w) {}
        KeyPoint(const ImVec4& _val, CurveType _type) : val(_val), type(_type), x(val.x), y(val.y), z(val.z), t(val.w) {}
        KeyPoint& operator=(const KeyPoint& a) { val = a.val; type = a.type; return *this; }

        float &x, &y, &z, &t;
        ImVec4 val {0, 0, 0, 0};
        CurveType type {UnKnown};

        ImVec2 GetVec2PointByDim(ValueDimension eDim) const
        {
            if (eDim == DIM_X)
                return ImVec2(t, x);
            else if (eDim == DIM_Y)
                return ImVec2(t, y);
            else
                return ImVec2(t, z);
        }
    };

    struct Curve
    {
        Curve() {}
        Curve(const std::string& _name, CurveType _type, ImU32 _color, bool _visible, const ImVec4& _min, const ImVec4& _max, const ImVec4& _default)
            : type(_type), name(_name), color(_color), visible(_visible), m_min(_min), m_max(_max), m_default(_default)
        {
            const auto diff = _max - _min;
            m_valueRangeAbs.x = fabs(diff.x);
            m_valueRangeAbs.y = fabs(diff.y);
            m_valueRangeAbs.z = fabs(diff.z);
            m_valueRangeAbs.w = fabs(diff.w);
        }

        std::vector<KeyPoint> points;
        CurveType type {Smooth};
        std::string name;
        ImU32 color;
        ImVec4 m_min {0, 0, 0, 0};
        ImVec4 m_max {0, 0, 0, 0};
        ImVec4 m_valueRangeAbs {0, 0, 0, 0};
        ImVec4 m_default {0, 0, 0, 0};
        bool visible {true};
        bool checked {false};
        int64_t m_id {-1};
        int64_t m_sub_id {-1};
    };

    struct SelPoint
    {
        int curveIndex;
        int pointIndex;
        SelPoint(int c, int p)
        {
            curveIndex = c;
            pointIndex = p;
        }
        bool operator <(const SelPoint& other) const
        {
            if (curveIndex < other.curveIndex)
                return true;
            if (curveIndex > other.curveIndex)
                return false;
            if (pointIndex < other.pointIndex)
                return true;
            return false;
        }
    };

    struct Delegate
    {
        bool focused = false;
        bool selectingQuad {false};
        ImVec2 quadSelection {0, 0};
        int overCurve {-1};
        int movingCurve {-1};
        bool scrollingV {false};
        bool overSelectedPoint {false};
        std::set<SelPoint> selectedPoints;
        bool MovingCurrentTime {false};
        bool pointsMoved {false};
        ImVec2 mousePosOrigin;
        std::vector<KeyPoint> originalPoints;
        
        virtual void Clear() = 0;
        virtual size_t GetCurveCount() = 0;
        virtual bool IsVisible(size_t /*curveIndex*/) { return true; }
        virtual CurveType GetCurveType(size_t /*curveIndex*/) const { return Linear; }
        virtual CurveType GetCurvePointType(size_t /*curveIndex*/, size_t /*pointIndex*/) const { return Linear; }
        virtual const ImVec4& GetMin() = 0;
        virtual const ImVec4& GetMax() = 0;
        virtual void SetMin(const ImVec4& vmin, bool dock = false) = 0;
        virtual void SetMax(const ImVec4& vmax, bool dock = false) = 0;
        virtual void MoveTo(float x) = 0;
        virtual void SetTimeRange(float _min, float _max, bool dock = false) = 0;
        virtual size_t GetCurvePointCount(size_t curveIndex) = 0;
        virtual ImU32 GetCurveColor(size_t curveIndex) = 0;
        virtual std::string GetCurveName(size_t curveIndex) = 0;
        virtual int64_t GetCurveID(size_t curveIndex) = 0;
        virtual int64_t GetCurveSubID(size_t curveIndex) = 0;
        virtual KeyPoint* GetPoints(size_t curveIndex) = 0;
        virtual KeyPoint GetPoint(size_t curveIndex, size_t pointIndex) = 0;
        virtual ImVec4 GetPrevPoint(float pos) = 0;
        virtual ImVec4 GetNextPoint(float pos) = 0;
        virtual ImVec4 AlignValue(const ImVec4& value) const = 0;
        virtual ImVec2 AlignValueByDim(const ImVec2& value, ValueDimension eDim) const = 0;
        virtual int EditPoint(size_t curveIndex, size_t pointIndex, const ImVec4& value, CurveType ctype) = 0;
        virtual int EditPointByDim(size_t curveIndex, size_t pointIndex, const ImVec2& value, CurveType ctype, ValueDimension eDim) = 0;
        virtual void AddPoint(size_t curveIndex, const ImVec4& value, CurveType ctype, bool bNeedNormalize) = 0;
        virtual void AddPointByDim(size_t curveIndex, const ImVec2& value, CurveType ctype, ValueDimension eDim, bool bNeedNormalize) = 0;
        virtual ImVec4 GetPointValue(size_t curveIndex, float t) = 0;
        virtual ImVec4 GetValue(size_t curveIndex, float t) = 0;
        virtual float GetValueByDim(size_t curveIndex, float t, ValueDimension eDim) = 0;
        virtual void ClearPoints(size_t curveIndex) = 0;
        virtual void DeletePoint(size_t curveIndex, size_t pointIndex) = 0;
        virtual int AddCurve(const std::string& name, CurveType ctype, ImU32 color, bool visible,
                const ImVec4& _min, const ImVec4& _max, const ImVec4& _default, int64_t _id = -1, int64_t _sub_id = -1) = 0;
        virtual int AddCurveByDim(const std::string& name, CurveType ctype, ImU32 color, bool visible, ValueDimension eDim,
                float _min, float _max, float _default, int64_t _id = -1, int64_t _sub_id = -1) = 0;
        virtual void DeleteCurve(size_t curveIndex) = 0;
        virtual void DeleteCurve(const std::string& name) = 0;
        virtual int GetCurveIndex(const std::string& name) = 0;
        virtual int GetCurveIndex(int64_t id) = 0;
        virtual int GetCurveKeyCount() = 0;
        virtual const Curve* GetCurve(const std::string& name) = 0;
        virtual const Curve* GetCurve(size_t curveIndex) = 0;
        virtual void SetCurveColor(size_t curveIndex, ImU32 color) = 0;
        virtual void SetCurveName(size_t curveIndex, const std::string& name) = 0;
        virtual void SetCurveVisible(size_t curveIndex, bool visible) = 0;
        virtual const ImVec4& GetCurveMin(size_t curveIndex) const = 0;
        virtual const ImVec4& GetCurveMax(size_t curveIndex) const = 0;
        virtual const ImVec4& GetCurveValueRange(size_t curveIndex) const = 0;
        virtual const ImVec4& GetCurveDefault(size_t curveIndex) const = 0;
        virtual const float GetCurveMinByDim(size_t curveIndex, ValueDimension eDim) const = 0;
        virtual const float GetCurveMaxByDim(size_t curveIndex, ValueDimension eDim) const = 0;
        virtual const float GetCurveValueRangeByDim(size_t curveIndex, ValueDimension eDim) const = 0;
        virtual const float GetCurveDefaultByDim(size_t curveIndex, ValueDimension eDim) const = 0;
        virtual void SetCurveMin(size_t curveIndex, const ImVec4& _min) = 0;
        virtual void SetCurveMax(size_t curveIndex, const ImVec4& _max) = 0;
        virtual void SetCurveDefault(size_t curveIndex, const ImVec4& _default) = 0;
        virtual void SetCurveMinByDim(size_t curveIndex, float _min, ValueDimension eDim) = 0;
        virtual void SetCurveMaxByDim(size_t curveIndex, float _max, ValueDimension eDim) = 0;
        virtual void SetCurveDefaultByDim(size_t curveIndex, float _default, ValueDimension eDim) = 0;
        virtual void SetCurvePointDefault(size_t curveIndex, size_t pointIndex) = 0;

        virtual ImU32 GetBackgroundColor() { return 0xFF101010; }
        virtual ImU32 GetGraticuleColor() { return 0xFF202020; }
        virtual void SetBackgroundColor(ImU32 color) = 0;
        virtual void SetGraticuleColor(ImU32 color) = 0;
        // TODO::Dicky handle undo/redo thru this functions
        virtual void BeginEdit(int /*index*/) {}
        virtual void EndEdit() {}

        virtual ~Delegate() = default;
    };

    static const ImVec4 ZERO_POINT;

private:
    static int DrawPoint(ImDrawList* draw_list, ImVec2 pos, const ImVec2 size, const ImVec2 offset, bool edited);
public:
    static int GetCurveTypeName(char**& list);
    static float smoothstep(float edge0, float edge1, float t, CurveType ctype);
    static float distance(float x1, float y1, float x2, float y2);
    static float distance(float x, float y, float x1, float y1, float x2, float y2);
    static bool Edit(ImDrawList* draw_list, Delegate* delegate, const ImVec2& size, unsigned int id, bool editable, float& cursor_pos,
                    unsigned int flags = CURVE_EDIT_FLAG_NONE, const ImRect* clippingRect = NULL, bool* changed = nullptr, ValueDimension eValDim = DIM_X);
    static bool Edit(ImDrawList* draw_list, Delegate* delegate, const ImVec2& size, unsigned int id, bool editable, float& cursor_pos, float firstTime, float lastTime,
                    unsigned int flags = CURVE_EDIT_FLAG_NONE, const ImRect* clippingRect = NULL, bool* changed = nullptr, ValueDimension eValDim = DIM_X);
    static float GetDimVal(const ImVec4& v, ValueDimension eDim);
    static void SetDimVal(ImVec4& v, float f, ValueDimension eDim);
};

struct IMGUI_API KeyPointEditor : public ImCurveEdit::Delegate
{
    KeyPointEditor() {}
    KeyPointEditor(ImU32 bg_color, ImU32 gr_color) 
        : BackgroundColor(bg_color), GraticuleColor(gr_color)
    {}
    ~KeyPointEditor() { mCurves.clear(); }

    void Clear() override { mCurves.clear(); }

    KeyPointEditor& operator=(const KeyPointEditor& keypoint);
    ImVec4 GetPrevPoint(float pos) override;
    ImVec4 GetNextPoint(float pos) override;
    ImCurveEdit::CurveType GetCurvePointType(size_t curveIndex, size_t pointIndex) const override;
    ImCurveEdit::KeyPoint GetPoint(size_t curveIndex, size_t pointIndex) override;
    int EditPoint(size_t curveIndex, size_t pointIndex, const ImVec4& value, ImCurveEdit::CurveType ctype) override;
    int EditPointByDim(size_t curveIndex, size_t pointIndex, const ImVec2& value, ImCurveEdit::CurveType ctype, ImCurveEdit::ValueDimension eDim) override;
    ImVec4 AlignValue(const ImVec4& value) const override;
    ImVec2 AlignValueByDim(const ImVec2& value, ImCurveEdit::ValueDimension eDim) const override;
    void AddPoint(size_t curveIndex, const ImVec4& value, ImCurveEdit::CurveType ctype, bool bNeedNormalize) override;
    void AddPointByDim(size_t curveIndex, const ImVec2& value, ImCurveEdit::CurveType ctype, ImCurveEdit::ValueDimension eDim, bool bNeedNormalize) override;
    void ClearPoints(size_t curveIndex) override;
    void DeletePoint(size_t curveIndex, size_t pointIndex) override;
    int AddCurve(const std::string& name, ImCurveEdit::CurveType ctype, ImU32 color, bool visible,
            const ImVec4& _min, const ImVec4& _max, const ImVec4& _default, int64_t _id = -1, int64_t _sub_id = -1) override;
    int AddCurveByDim(const std::string& name, ImCurveEdit::CurveType ctype, ImU32 color, bool visible, ImCurveEdit::ValueDimension eDim,
            float _min, float _max, float _default, int64_t _id = -1, int64_t _sub_id = -1) override;
    void DeleteCurve(size_t curveIndex) override;
    void DeleteCurve(const std::string& name) override;
    int GetCurveIndex(const std::string& name) override;
    int GetCurveIndex(int64_t id) override;
    int GetCurveKeyCount()  override { return mCurves.size(); }
    const ImCurveEdit::Curve* GetCurve(const std::string& name) override;
    const ImCurveEdit::Curve* GetCurve(size_t curveIndex) override;
    ImVec4 GetPointValue(size_t curveIndex, float t) override;
    ImVec4 GetValue(size_t curveIndex, float t) override;
    float GetValueByDim(size_t curveIndex, float t, ImCurveEdit::ValueDimension eDim) override;
    void SetCurvePointDefault(size_t curveIndex, size_t pointIndex) override;
    void MoveTo(float x) override;
    void SetMin(const ImVec4& vmin, bool dock = false) override;
    void SetMax(const ImVec4& vmax, bool dock = false) override;

    ImU32 GetBackgroundColor() override { return BackgroundColor; }
    ImU32 GetGraticuleColor() override { return GraticuleColor; }
    void SetBackgroundColor(ImU32 color) override { BackgroundColor = color; }
    void SetGraticuleColor(ImU32 color) override { GraticuleColor = color; }
    size_t GetCurveCount() override { return mCurves.size(); }
    std::string GetCurveName(size_t curveIndex) override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].name; return ""; }
    int64_t GetCurveID(size_t curveIndex) override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].m_id; return -1; }
    int64_t GetCurveSubID(size_t curveIndex) override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].m_sub_id; return -1; }
    size_t GetCurvePointCount(size_t curveIndex) override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].points.size(); return 0; }
    ImU32 GetCurveColor(size_t curveIndex) override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].color; return 0; }
    ImCurveEdit::CurveType GetCurveType(size_t curveIndex) const override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].type; return ImCurveEdit::Linear; }
    void SetCurveColor(size_t curveIndex, ImU32 color) override { if (curveIndex < mCurves.size()) mCurves[curveIndex].color = color; }
    void SetCurveName(size_t curveIndex, const std::string& name) override { if (curveIndex < mCurves.size()) mCurves[curveIndex].name = name; }
    void SetCurveVisible(size_t curveIndex, bool visible) override { if (curveIndex < mCurves.size()) mCurves[curveIndex].visible = visible; }
    ImCurveEdit::KeyPoint* GetPoints(size_t curveIndex) override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].points.data(); return nullptr; }
    const ImVec4& GetMax() override { return mMax; }
    const ImVec4& GetMin() override { return mMin; }
    const ImVec4& GetCurveMin(size_t curveIndex) const override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].m_min; return ImCurveEdit::ZERO_POINT; }
    const ImVec4& GetCurveMax(size_t curveIndex) const override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].m_max; return ImCurveEdit::ZERO_POINT; }
    const ImVec4& GetCurveValueRange(size_t curveIndex) const override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].m_valueRangeAbs; return ImCurveEdit::ZERO_POINT; }
    const ImVec4& GetCurveDefault(size_t curveIndex) const override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].m_default; return ImCurveEdit::ZERO_POINT; }
    const float GetCurveMinByDim(size_t curveIndex, ImCurveEdit::ValueDimension eDim) const override { return ImCurveEdit::GetDimVal(GetCurveMin(curveIndex), eDim); }
    const float GetCurveMaxByDim(size_t curveIndex, ImCurveEdit::ValueDimension eDim) const override  { return ImCurveEdit::GetDimVal(GetCurveMax(curveIndex), eDim); }
    const float GetCurveValueRangeByDim(size_t curveIndex, ImCurveEdit::ValueDimension eDim) const override  { return ImCurveEdit::GetDimVal(GetCurveValueRange(curveIndex), eDim); }
    const float GetCurveDefaultByDim(size_t curveIndex, ImCurveEdit::ValueDimension eDim) const override  { return ImCurveEdit::GetDimVal(GetCurveDefault(curveIndex), eDim); }
    void SetCurveMin(size_t curveIndex, const ImVec4& _min) override;
    void SetCurveMax(size_t curveIndex, const ImVec4& _max) override;
    void SetCurveDefault(size_t curveIndex, const ImVec4& _default) override;
    void SetCurveMinByDim(size_t curveIndex, float _min, ImCurveEdit::ValueDimension eDim) override;
    void SetCurveMaxByDim(size_t curveIndex, float _max, ImCurveEdit::ValueDimension eDim) override;
    void SetCurveDefaultByDim(size_t curveIndex, float _default, ImCurveEdit::ValueDimension eDim) override;
    void SetCurveAlign(const ImVec2& align, ImCurveEdit::ValueDimension eDim);
    void SetTimeRange(float _min, float _max, bool dock) override { SetMin(ImVec4(0.f, 0.f, 0.f, _min), dock); SetMax(ImVec4(1.f, 1.f, 1.f, _max), dock); }
    bool IsVisible(size_t curveIndex) override { if (curveIndex < mCurves.size()) return mCurves[curveIndex].visible; return false; }

    void Load(const imgui_json::value& value);
    void Save(imgui_json::value& value);

private:
    std::vector<ImCurveEdit::Curve> mCurves;
    ImVec4 mMin {-1.f, -1.f, -1.f, -1.f};
    ImVec4 mMax {-1.f, -1.f, -1.f, -1.f};
    ImVec2 mDimAlign[4];
    ImU32 BackgroundColor {IM_COL32(24, 24, 24, 255)};
    ImU32 GraticuleColor {IM_COL32(48, 48, 48, 128)};

private:
    void SortValues(size_t curveIndex);
};

IMGUI_API bool ImCurveEditKeyByDim(const std::string& label, ImCurveEdit::Curve* pCurve, ImCurveEdit::ValueDimension eDim, const std::string& name, float _min, float _max, float _default, float space = 0);
IMGUI_API bool ImCurveCheckEditKeyByDim(const std::string& label, ImCurveEdit::Curve* pCurve, ImCurveEdit::ValueDimension eDim, bool &check, const std::string& name, float _min, float _max, float _default, float space = 0);
IMGUI_API bool ImCurveCheckEditKeyWithIDByDim(const std::string& label, ImCurveEdit::Curve* pCurve, ImCurveEdit::ValueDimension eDim, bool check, const std::string& name, float _min, float _max, float _default, int64_t subid = -1, float space = 0);

} // namespace ImGui

#if IMGUI_BUILD_EXAMPLE
namespace ImGui
{
    IMGUI_API void ShowCurveDemo();
} //namespace ImGui
#endif
#endif /* IMGUI_CURVE_H */
