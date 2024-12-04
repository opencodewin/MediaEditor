#pragma once
#include <cstdint>
#include <memory>
#include <immat.h>
#include "MediaCore.h"

namespace MediaCore
{
enum class MediaType
{
    UNKNOWN = 0,
    VIDEO,
    AUDIO,
    SUBTITLE,
};

struct Ratio
{
    Ratio() {}
    Ratio(int32_t _num, int32_t _den) : num(_num), den(_den) {}
    Ratio(const std::string& ratstr);

    int32_t num{0};
    int32_t den{0};

    static inline bool IsValid(const Ratio& r)
    { return r.num != 0 && r.den != 0; }

    bool operator==(const Ratio& r)
    { return num*r.den == den*r.num; }
    bool operator!=(const Ratio& r)
    { return !(*this == r); }

    friend MEDIACORE_API std::ostream& operator<<(std::ostream& os, const Ratio& r);
};

struct Value
{
    enum Type
    {
        VT_INT = 0,
        VT_DOUBLE,
        VT_BOOL,
        VT_STRING,
        VT_FLAGS,
        VT_RATIO,
    };

    Value() = default;
    Value(const Value&) = default;
    Value(Value&&) = default;
    Value& operator=(const Value&) = default;
    Value(int64_t val) : type(VT_INT) { numval.i64=val; }
    Value(uint64_t val) : type(VT_INT) { numval.i64=val; }
    Value(int32_t val) : type(VT_INT) { numval.i64=val; }
    Value(uint32_t val) : type(VT_INT) { numval.i64=val; }
    Value(int16_t val) : type(VT_INT) { numval.i64=val; }
    Value(uint16_t val) : type(VT_INT) { numval.i64=val; }
    Value(int8_t val) : type(VT_INT) { numval.i64=val; }
    Value(uint8_t val) : type(VT_INT) { numval.i64=val; }
    Value(double val) : type(VT_DOUBLE) { numval.dbl=val; }
    Value(float val) : type(VT_DOUBLE) { numval.dbl=val; }
    Value(bool val) : type(VT_BOOL) { numval.bln=val; }
    Value(const char* val) : type(VT_STRING) { strval=std::string(val); }
    Value(const std::string& val) : type(VT_STRING) { strval=val; }
    Value(const Ratio& rat) : type(VT_RATIO) { ratval=rat; }

    template <typename T>
    Value(Type _type, const T& val) : type(_type)
    {
        switch (_type)
        {
            case VT_INT:    numval.i64 = static_cast<int64_t>(val); break;
            case VT_DOUBLE: numval.dbl = static_cast<double>(val); break;
            case VT_BOOL:   numval.bln = static_cast<bool>(val); break;
            case VT_STRING: strval = std::string(val); break;
            case VT_FLAGS:  numval.i64 = static_cast<int64_t>(val); break;
            case VT_RATIO:  ratval = Ratio(val); break;
        }
    }

    Type type;
    union
    {
        int64_t i64;
        double dbl;
        bool bln;
    } numval;
    std::string strval;
    Ratio ratval;

    friend MEDIACORE_API std::ostream& operator<<(std::ostream& os, const Value& val);
};

struct VideoFrame
{
    using Holder = std::shared_ptr<VideoFrame>;
    static MEDIACORE_API Holder CreateMatInstance(const ImGui::ImMat& m);

    virtual bool GetMat(ImGui::ImMat& m) = 0;
    virtual int64_t Pos() const = 0;
    virtual int64_t Pts() const = 0;
    virtual int64_t Dur() const = 0;
    virtual float Opacity() const = 0;
    virtual void SetOpacity(float opacity) = 0;
    virtual void SetAutoConvertToMat(bool enable) = 0;
    virtual bool IsReady() const = 0;

    struct NativeData
    {
        enum Type
        {
            UNKNOWN = 0,
            AVFRAME,
            AVFRAME_HOLDER,
            MAT,
        };
        enum Type eType;
        void* pData;
    };
    virtual NativeData GetNativeData() const = 0;
};

struct CorrelativeFrame
{
    using Holder = std::shared_ptr<CorrelativeFrame>;

    enum Phase
    {
        PHASE_SOURCE_FRAME = 0,
        PHASE_AFTER_FILTER,
        PHASE_AFTER_TRANSFORM,
        PHASE_AFTER_AUDIOEFFECT,
        PHASE_AFTER_TRANSITION,
        PHASE_AFTER_MIXING,
    } phase;
    int64_t clipId{0};
    int64_t trackId{0};
    ImGui::ImMat frame;

    CorrelativeFrame() = default;
    CorrelativeFrame(Phase _phase, int64_t _clipId, int64_t _trackId, const ImGui::ImMat& _frame)
        : phase(_phase), clipId(_clipId), trackId(_trackId), frame(_frame)
    {}
};

struct CorrelativeVideoFrame : CorrelativeFrame
{
    using Holder = std::shared_ptr<CorrelativeVideoFrame>;

    VideoFrame::Holder hVfrm;

    CorrelativeVideoFrame() = default;
    CorrelativeVideoFrame(Phase _phase, int64_t _clipId, int64_t _trackId, const VideoFrame::Holder& _hVfrm)
        : CorrelativeFrame(_phase, _clipId, _trackId, ImGui::ImMat()), hVfrm(_hVfrm)
    {}
};

}