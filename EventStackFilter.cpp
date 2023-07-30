#include <algorithm>
#include <functional>
#include <sstream>
#include "EventStackFilter.h"

using namespace std;
using namespace MediaCore;
using namespace Logger;

namespace MEC
{
class VideoEventStackFilter_Impl : public VideoEventStackFilter
{
public:
    VideoEventStackFilter_Impl(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
        : m_bpCallbacks(bpCallbacks)
    {
        m_logger = GetLogger("EventStackFilter");
    }

    virtual ~VideoEventStackFilter_Impl()
    {
        m_pClip = nullptr;
        m_eventList.clear();
    }

    const string GetFilterName() const override
    {
        return "EventStackFilter";
    }

    Holder Clone() override
    {
        imgui_json::value filterJson = SaveAsJson();
        BluePrint::BluePrintCallbackFunctions bpCallbacks;
        return LoadFromJson(filterJson, bpCallbacks);
    }

    void ApplyTo(VideoClip* clip) override
    {
        m_pClip = clip;
        auto clipId = clip->Id();
        ostringstream tagOss; tagOss << clipId;
        auto idstr = tagOss.str();
        if (idstr.size() > 4)
            idstr = idstr.substr(idstr.size()-4);
        tagOss.str(""); tagOss << "ESF#" << idstr;
        auto loggerName = tagOss.str();
        m_logger = GetLogger(loggerName);
    }

    void UpdateClipRange() override
    {
        // TODO: handle clip range changed
    }

    ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override
    {
        list<Event::Holder> effectiveEvents;
        for (auto e : m_eventList)
        {
            if (e->IsInRange(pos))
                effectiveEvents.push_back(e);
        }
        ImGui::ImMat outM = vmat;
        for (auto& e : effectiveEvents)
        {
            VideoEvent_Impl* pEvtImpl = dynamic_cast<VideoEvent_Impl*>(e.get());
            outM = pEvtImpl->FilterImage(outM, pos-pEvtImpl->Start());
        }
        return outM;
    }

    const VideoClip* GetVideoClip() const override { return m_pClip; }

    Event::Holder GetEvent(int64_t id) override
    {
        auto iter = find_if(m_eventList.begin(), m_eventList.end(), [id] (auto e) {
            return e->Id() == id;
        });
        if (iter == m_eventList.end())
        {
            ostringstream oss; oss << "CANNOT find event with id '" << id << "'!";
            m_errMsg = oss.str();
            return nullptr;
        }
        return *iter;
    }

    Event::Holder AddNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) override
    {
        if (start == end)
        {
            m_errMsg = "IVALID arguments! 'start' and 'end' CANNOT be IDENTICAL.";
            return nullptr;
        }
        auto hDupEvt = GetEvent(id);
        if (hDupEvt)
        {
            ostringstream oss; oss << "IVALID arguments! Event with id '" << id << "' already exists.";
            m_errMsg = oss.str();
            return nullptr;
        }
        if (end < start)
        {
            auto tmp = end; end = start; start = tmp;
        }
        bool hasOverlap = false;
        for (auto& e : m_eventList)
        {
            if (Event::CheckEventOverlapped(*e, start, end, z))
            {
                hasOverlap = true;
                break;
            }
        }
        if (hasOverlap)
        {
            m_errMsg = "INVALID arguments! Event range has overlap with the existing ones.";
            return nullptr;
        }

        auto pEvtImpl = new VideoEvent_Impl(this, id, start, end, z, m_bpCallbacks);
        Event::Holder hEvt(pEvtImpl, VIDEO_EVENT_DELETER);
        m_eventList.push_back(hEvt);
        m_eventList.sort(EVENTLIST_COMPARATOR);
        return hEvt;
    }

    Event::Holder RestoreEventFromJson(const imgui_json::value& eventJson) override
    {
        auto hEvent = VideoEvent_Impl::LoadFromJson(this, eventJson, m_bpCallbacks);
        if (!hEvent)
            return nullptr;
        if (!EnrollEvent(hEvent))
            return nullptr;
        return hEvent;
    }

    void RemoveEvent(int64_t id) override
    {
        auto iter = find_if(m_eventList.begin(), m_eventList.end(), [id] (auto e) {
            return e->Id() == id;
        });
        if (iter != m_eventList.end())
        {
            m_eventList.erase(iter);
        }
    }

    bool ChangeEventRange(int64_t id, int64_t start, int64_t end) override
    {
        if (start == end)
        {
            m_errMsg = "IVALID arguments! 'start' and 'end' CANNOT be IDENTICAL.";
            return false;
        }
        if (end < start)
        {
            auto tmp = end; end = start; start = tmp;
        }
        auto hEvt = GetEvent(id);
        if (!hEvt)
            return false;
        VideoEvent_Impl* pEvtImpl = dynamic_cast<VideoEvent_Impl*>(hEvt.get());
        auto z = pEvtImpl->Z();
        bool hasOverlap = false;
        for (auto& e : m_eventList)
        {
            if (e->Id() == id)
                continue;
            if (Event::CheckEventOverlapped(*e, start, end, z))
            {
                hasOverlap = true;
                break;
            }
        }
        if (hasOverlap)
        {
            m_errMsg = "INVALID arguments! Event range has overlap with the existing ones.";
            return false;
        }
        pEvtImpl->SetStart(start);
        pEvtImpl->SetEnd(end);
        pEvtImpl->UpdateKeyPointRange();
        m_eventList.sort(EVENTLIST_COMPARATOR);
        return true;
    }

    bool MoveEvent(int64_t id, int64_t start, int32_t z) override
    {
        auto hEvt = GetEvent(id);
        if (!hEvt)
            return false;
        VideoEvent_Impl* pEvtImpl = dynamic_cast<VideoEvent_Impl*>(hEvt.get());
        auto end = pEvtImpl->End()+(start-pEvtImpl->Start());
        bool hasOverlap = false;
        for (auto e : m_eventList)
        {
            if (e->Id() == id)
                continue;
            if (Event::CheckEventOverlapped(*e, start, end, z))
            {
                hasOverlap = true;
                break;
            }
        }
        if (hasOverlap)
        {
            m_errMsg = "INVALID arguments! Event range has overlap with the existing ones.";
            return false;
        }
        pEvtImpl->SetStart(start);
        pEvtImpl->SetEnd(end);
        pEvtImpl->SetZ(z);
        m_eventList.sort(EVENTLIST_COMPARATOR);
        return true;
    }

    bool MoveAllEvents(int64_t offset) override
    {
        for (auto e : m_eventList)
        {
            VideoEvent_Impl* pEvtImpl = dynamic_cast<VideoEvent_Impl*>(e.get());
            auto newStart = pEvtImpl->Start()+offset;
            auto newEnd = pEvtImpl->End()+offset;
            pEvtImpl->SetStart(newStart);
            pEvtImpl->SetEnd(newEnd);
        }
        return true;
    }

    bool SetEditingEvent(int64_t id) override
    {
        if (id == -1)
        {
            m_editingEventId = -1;
            return true;
        }
        auto pEvent = GetEvent(id);
        if (!pEvent)
            return false;
        m_editingEventId = id;
        return true;
    }

    Event::Holder GetEditingEvent() override
    {
        return GetEvent(m_editingEventId);
    }

    list<Event::Holder> GetEventList() const override
    {
        list<Event::Holder> eventList(m_eventList);
        return eventList;
    }

    list<Event::Holder> GetEventListByZ(int32_t z) const override
    {
        list<Event::Holder> eventList;
        for (auto& e : m_eventList)
        {
            if (e->Z() == z)
                eventList.push_back(e);
        }
        return eventList;
    }

    imgui_json::value SaveAsJson() const override
    {
        imgui_json::value json;
        json["name"] = imgui_json::string(GetFilterName());
        imgui_json::array eventJsonAry;
        for (auto& e : m_eventList)
        {
            VideoEvent_Impl* pEvtImpl = dynamic_cast<VideoEvent_Impl*>(e.get());
            eventJsonAry.push_back(pEvtImpl->SaveAsJson());
        }
        json["events"] = eventJsonAry;
        m_logger->Log(DEBUG) << "Save filter-json : " << json.dump() << std::endl;
        return std::move(json);
    }

    void SetBluePrintCallbacks(const BluePrint::BluePrintCallbackFunctions& bpCallbacks) override
    {
        for (auto& hEvent : m_eventList)
        {
            auto pEvt = dynamic_cast<VideoEvent_Impl*>(hEvent.get());
            pEvt->SetBluePrintCallbacks(bpCallbacks);
        }
        m_bpCallbacks = bpCallbacks;
    }

    void SetTimelineHandle(void* handle) override
    {
        m_tlHandle = handle;
    }

    void* GetTimelineHandle() const override
    {
        return m_tlHandle;
    }

    string GetError() const override { return m_errMsg; }
    void SetLogLevel(Level l) override { m_logger->SetShowLevels(l); }

public:
    class VideoEvent_Impl : public VideoEvent
    {
    public:
        VideoEvent_Impl(VideoEventStackFilter_Impl* owner, int64_t id, int64_t start, int64_t end, int32_t z,
            const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
            : m_owner(owner), m_id(id), m_start(start), m_end(end), m_z(z)
        {
            m_pKp = new ImGui::KeyPointEditor();
            m_pKp->SetRangeX(0, end-start, true);

            m_pBp = new BluePrint::BluePrintUI();
            m_pBp->Initialize();
            m_pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(static_cast<MediaCore::VideoFilter*>(owner)));
            imgui_json::value emptyJson;
            m_pBp->File_New_Filter(emptyJson, "VideoFilter", "Video");
        }

        virtual ~VideoEvent_Impl()
        {
            if (m_pBp) 
            {
                m_pBp->Finalize(); 
                delete m_pBp;
                m_pBp = nullptr;
            }
            if (m_pKp)
            {
                delete m_pKp;
                m_pKp = nullptr;
            }
        }

        static Event::Holder LoadFromJson(VideoEventStackFilter_Impl* owner, const imgui_json::value& bpJson, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);

        int64_t Id() const override { return m_id; }
        int64_t Start() const override { return m_start; }
        int64_t End() const override { return m_end; }
        int64_t Length() const override { return m_end-m_start; }
        int32_t Z() const override { return m_z; }
        uint32_t Status() const override { return m_status; }
        bool IsInRange(int64_t pos) const override { return pos >= m_start && pos < m_end; }
        BluePrint::BluePrintUI* GetBp() override { return m_pBp; }
        ImGui::KeyPointEditor* GetKeyPoint() override { return m_pKp; }

        void SetStart(int64_t start) { m_start = start; }
        void SetEnd(int64_t end) { m_end = end; }
        void SetZ(int32_t z) { m_z = z; }
        void UpdateKeyPointRange() { m_pKp->SetRangeX(0, m_end-m_start, true); }

        ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override
        {
            ImGui::ImMat outMat(vmat);
            if (m_pBp->Blueprint_IsExecutable())
            {
                // setup bp input curve
                for (int i = 0; i < m_pKp->GetCurveCount(); i++)
                {
                    auto name = m_pKp->GetCurveName(i);
                    auto value = m_pKp->GetValue(i, pos);
                    m_pBp->Blueprint_SetFilter(name, value);
                }
                ImGui::ImMat inMat(vmat);
                m_pBp->Blueprint_RunFilter(inMat, outMat, pos, Length());
            }
            return outMat;
        }

        bool ChangeRange(int64_t start, int64_t end) override
        {
            return m_owner->ChangeEventRange(m_id, start, end);
        }

        bool Move(int64_t start, int32_t z) override
        {
            return m_owner->MoveEvent(m_id, start, z);
        }

        EventStack* GetOwner() override
        {
            return static_cast<EventStack*>(m_owner);
        }

        void SetStatus(uint32_t status) override
        {
            m_status = status;
        }

        void SetStatus(int bit, int val) override
        {
            m_status = (m_status & ~(1UL << bit)) | (val << bit);
        }

        void SetBluePrintCallbacks(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
        {
            m_pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(static_cast<MediaCore::VideoFilter*>(m_owner)));
        }

        string GetError() const override
        {
            return m_owner->GetError();
        }        

        imgui_json::value SaveAsJson() const override
        {
            imgui_json::value json;
            json["id"] = imgui_json::number(m_id);
            json["start"] = imgui_json::number(m_start);
            json["end"] = imgui_json::number(m_end);
            json["z"] = imgui_json::number(m_z);
            json["bp"] = m_pBp->m_Document->Serialize();
            imgui_json::value kpJson;
            m_pKp->Save(kpJson);
            json["kp"] = kpJson;
            return json;
        }

    private:
        VideoEvent_Impl(VideoEventStackFilter_Impl* owner) : m_owner(owner) {}

    private:
        VideoEventStackFilter_Impl* m_owner;
        int64_t m_id{-1};
        BluePrint::BluePrintUI* m_pBp{nullptr};
        ImGui::KeyPointEditor* m_pKp{nullptr};
        int64_t m_start;
        int64_t m_end;
        int32_t m_z{-1};
        uint32_t m_status{0};
    };

    static const function<void(Event*)> VIDEO_EVENT_DELETER;

    bool EnrollEvent(Event::Holder hEvt)
    {
        for (auto& e : m_eventList)
        {
            if (e->Id() == hEvt->Id())
            {
                ostringstream oss; oss << "Duplicated id! Already contained an event with id '" << hEvt->Id() << "'.";
                m_errMsg = oss.str();
                return false;
            }
            bool hasOverlap = Event::CheckEventOverlapped(*e, hEvt->Start(), hEvt->End(), hEvt->Z());
            if (hasOverlap)
            {
                ostringstream oss; oss << "Can not enroll this event! It has overlap with the existing ones.";
                m_errMsg = oss.str();
                return false;
            }
        }
        m_eventList.push_back(hEvt);
        m_eventList.sort(EVENTLIST_COMPARATOR);
        return true;
    }

private:
    static function<bool(const Event::Holder&,const Event::Holder&)> EVENTLIST_COMPARATOR;

private:
    ALogger* m_logger;
    VideoClip* m_pClip{nullptr};
    list<Event::Holder> m_eventList;
    int64_t m_editingEventId{-1};
    BluePrint::BluePrintCallbackFunctions m_bpCallbacks;
    void* m_tlHandle{nullptr};
    string m_errMsg;
};

function<bool(const Event::Holder&,const Event::Holder&)> VideoEventStackFilter_Impl::EVENTLIST_COMPARATOR = [] (const Event::Holder& a, const Event::Holder& b)
{ return Event::EVENT_ORDER_COMPARATOR(*a, *b); };

Event::Holder VideoEventStackFilter_Impl::VideoEvent_Impl::LoadFromJson(
        VideoEventStackFilter_Impl* owner, const imgui_json::value& eventJson, const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    owner->m_logger->Log(DEBUG) << "Load EventJson : " << eventJson.dump() << endl;
    auto pEvtImpl = new VideoEventStackFilter_Impl::VideoEvent_Impl(owner);
    Event::Holder hEvt(pEvtImpl, VIDEO_EVENT_DELETER);
    string itemName = "id";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvtImpl->m_id = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    itemName = "start";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvtImpl->m_start = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    itemName = "end";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvtImpl->m_end = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    itemName = "z";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvtImpl->m_z = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    itemName = "bp";
    if (eventJson.contains(itemName))
    {
        auto pBp = pEvtImpl->m_pBp = new BluePrint::BluePrintUI();
        pBp->Initialize();
        pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(static_cast<MediaCore::VideoFilter*>(owner)));
        auto bpJson = eventJson[itemName];
        pBp->File_New_Filter(bpJson, "VideoFilter", "Video");
        if (!pBp->Blueprint_IsValid())
        {
            owner->m_errMsg = "BAD event json! Invalid blueprint json.";
            return nullptr;
        }
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    itemName = "kp";
    if (eventJson.contains(itemName))
    {
        auto pKp = pEvtImpl->m_pKp = new ImGui::KeyPointEditor();
        pKp->Load(eventJson[itemName]);
        pKp->SetRangeX(0, pEvtImpl->Length(), true);
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    return hEvt;
}

const function<void(Event*)> VideoEventStackFilter_Impl::VIDEO_EVENT_DELETER = [] (Event* p) {
    VideoEventStackFilter_Impl::VideoEvent_Impl* ptr = dynamic_cast<VideoEventStackFilter_Impl::VideoEvent_Impl*>(p);
    delete ptr;
};

static const auto VIDEO_EVENT_STACK_FILTER_DELETER = [] (VideoFilter* p) {
    VideoEventStackFilter_Impl* ptr = dynamic_cast<VideoEventStackFilter_Impl*>(p);
    delete ptr;
};

VideoFilter::Holder VideoEventStackFilter::CreateInstance(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    return VideoFilter::Holder(new VideoEventStackFilter_Impl(bpCallbacks), VIDEO_EVENT_STACK_FILTER_DELETER);
}

VideoFilter::Holder VideoEventStackFilter::LoadFromJson(const imgui_json::value& json, const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    if (!json.contains("name") || !json["name"].is_string())
        return nullptr;
    string filterName = json["name"].get<imgui_json::string>();
    if (filterName != "EventStackFilter")
        return nullptr;
    auto pFilter = new VideoEventStackFilter_Impl(bpCallbacks);
    if (json.contains("events") && json["events"].is_array())
    {
        auto& evtAry = json["events"].get<imgui_json::array>();
        for (auto& evtJson : evtAry)
        {
            auto pEvent = VideoEventStackFilter_Impl::VideoEvent_Impl::LoadFromJson(pFilter, evtJson, bpCallbacks);
            if (!pEvent)
            {
                Log(Error) << "FAILED to create EventStackFilter::Event isntance from Json! Error is '" << pFilter->GetError() << "'." << endl;
                delete pFilter; pFilter = nullptr;
                break;
            }
            if (!pFilter->EnrollEvent(pEvent))
            {
                Log(Error) << "FAILED to enroll event loaded from json! Error is '" << pFilter->GetError() << "'." << endl;
                delete pFilter; pFilter = nullptr;
                break;
            }
        }
    }
    if (!pFilter)
        return nullptr;
    return VideoFilter::Holder(pFilter, VIDEO_EVENT_STACK_FILTER_DELETER);
}

ostream& operator<<(ostream& os, const EventStack& estk)
{
    auto eventList = estk.GetEventList();
    if (eventList.empty())
    {
        os << "[(empty)]";
        return os;
    }
    os << "[";
    for (auto& e : eventList)
    {
        os << *e << ", ";
    }
    os << "]";
    return os;
}
}