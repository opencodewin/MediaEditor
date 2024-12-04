#include "MediaData.h"

using namespace std;

namespace MediaCore
{
Ratio::Ratio(const string& ratstr)
{
    auto pos = ratstr.find('/');
    if (pos == string::npos)
        pos = ratstr.find(':');
    if (pos == string::npos)
    {
        num = atoi(ratstr.c_str());
        den = 1;
    }
    else
    {
        num = atoi(ratstr.substr(0, pos).c_str());
        if (pos == ratstr.size()-1)
            den = 1;
        else
            den = atoi(ratstr.substr(pos+1).c_str());
    }
}

ostream& operator<<(ostream& os, const Ratio& r)
{
    os << "(" << r.num << "/" << r.den << ")";
    return os;
}

ostream& operator<<(ostream& os, const Value& val)
{
    if (val.type == Value::VT_INT)
        os << val.numval.i64;
    else if (val.type == Value::VT_DOUBLE)
        os << val.numval.dbl;
    else if (val.type == Value::VT_BOOL)
        os << val.numval.bln;
    else if (val.type == Value::VT_STRING)
        os << val.strval;
    else if (val.type == Value::VT_FLAGS)
        os << val.numval.i64;
    else if (val.type == Value::VT_RATIO)
    {
        if (val.strval.empty())
            os << "0";
        else
            os << val.strval;
    }
    return os;
}

class VideoFrame_MatImpl : public VideoFrame
{
public:
    VideoFrame_MatImpl(const ImGui::ImMat& vmat) : m_vmat(vmat) {}
    virtual ~VideoFrame_MatImpl() {}

    bool GetMat(ImGui::ImMat& m) override
    {
        m = m_vmat;
        return true;
    }

    int64_t Pos() const override { return (int64_t)(m_vmat.time_stamp*1000); }
    int64_t Pts() const override { return 0; }
    int64_t Dur() const override { return 0; }
    float Opacity() const override { return m_fOpacity; }
    void SetOpacity(float opacity) override { m_fOpacity = opacity; }
    void SetAutoConvertToMat(bool enable) override {}
    bool IsReady() const override { return !m_vmat.empty(); }

    NativeData GetNativeData() const override
    {
        return { NativeData::MAT, (void*)&m_vmat };
    }

private:
    ImGui::ImMat m_vmat;
    float m_fOpacity{1.f};
};

static const auto MEDIA_READER_VIDEO_FRAME_MATIMPL_HOLDER_DELETER = [] (VideoFrame* p) {
    VideoFrame_MatImpl* ptr = dynamic_cast<VideoFrame_MatImpl*>(p);
    delete ptr;
};

VideoFrame::Holder VideoFrame::CreateMatInstance(const ImGui::ImMat& m)
{
    return VideoFrame::Holder(new VideoFrame_MatImpl(m), MEDIA_READER_VIDEO_FRAME_MATIMPL_HOLDER_DELETER);
}
}