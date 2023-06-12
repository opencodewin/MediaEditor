#include <algorithm>
#include <functional>
#include <sstream>
#include "EventStackFilter.h"

using namespace std;
using namespace MediaCore;
using namespace Logger;

namespace MEC
{
class EventStackFilter_Impl : public EventStackFilter
{
public:
    EventStackFilter_Impl(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
        : m_bpCallbacks(bpCallbacks)
    {
        m_logger = GetLogger("EventStackFilter");
    }

    virtual ~EventStackFilter_Impl()
    {
        m_pClip = nullptr;
        for (auto e : m_eventList)
        {
            Event_Impl* ptr = dynamic_cast<Event_Impl*>(e);
            delete ptr;
        }
        m_eventList.clear();
    }

    const string GetFilterName() const override
    {
        return "EventStackFilter";
    }

    Holder Clone() override
    {
        return nullptr;
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
        list<Event*> effectiveEvents;
        for (auto e : m_eventList)
        {
            if (e->IsInRange(pos))
                effectiveEvents.push_back(e);
        }
        ImGui::ImMat outM = vmat;
        for (auto e : effectiveEvents)
        {
            outM = e->FilterImage(outM, pos-e->Start());
        }
        return outM;
    }

    const VideoClip* GetVideoClip() const override { return m_pClip; }

    Event* GetEvent(int64_t id) override
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

    Event* AddNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) override
    {
        if (start == end)
        {
            m_errMsg = "IVALID arguments! 'start' and 'end' CANNOT be IDENTICAL.";
            return nullptr;
        }
        if (end < start)
        {
            auto tmp = end; end = start; start = tmp;
        }
        bool hasOverlap = false;
        for (auto e : m_eventList)
        {
            if (Event::CheckEventOverlapped(e, start, end, z))
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

        Event* pNewEvent = new Event_Impl(this, id, start, end, z, m_bpCallbacks);
        m_eventList.push_back(pNewEvent);
        m_eventList.sort(EVENT_COMPARATOR);
        return pNewEvent;
    }

    void RemoveEvent(int64_t id) override
    {
        auto iter = find_if(m_eventList.begin(), m_eventList.end(), [id] (auto e) {
            return e->Id() == id;
        });
        if (iter != m_eventList.end())
        {
            auto e = *iter;
            Event_Impl* ptr = dynamic_cast<Event_Impl*>(e);
            delete ptr;
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
        Event_Impl* pEvt = dynamic_cast<Event_Impl*>(GetEvent(id));
        if (!pEvt)
            return false;
        auto z = pEvt->Z();
        bool hasOverlap = false;
        for (auto e : m_eventList)
        {
            if (e->Id() == id)
                continue;
            if (Event::CheckEventOverlapped(e, start, end, z))
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
        pEvt->SetStart(start);
        pEvt->SetEnd(end);
        pEvt->UpdateKeyPointRange();
        m_eventList.sort(EVENT_COMPARATOR);
        return true;
    }

    bool MoveEvent(int64_t id, int64_t start, int32_t z) override
    {
        Event_Impl* pEvt = dynamic_cast<Event_Impl*>(GetEvent(id));
        if (!pEvt)
            return false;
        auto end = pEvt->End()+(start-pEvt->Start());
        bool hasOverlap = false;
        for (auto e : m_eventList)
        {
            if (e->Id() == id)
                continue;
            if (Event::CheckEventOverlapped(e, start, end, z))
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
        pEvt->SetStart(start);
        pEvt->SetEnd(end);
        pEvt->SetZ(z);
        m_eventList.sort(EVENT_COMPARATOR);
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

    Event* GetEditingEvent() override
    {
        return GetEvent(m_editingEventId);
    }


    imgui_json::value SaveAsJson() const override
    {
        imgui_json::value json;
        json["name"] = imgui_json::string(GetFilterName());
        imgui_json::array eventJsonAry;
        for (auto e : m_eventList)
        {
            Event_Impl* pEvt = dynamic_cast<Event_Impl*>(e);
            eventJsonAry.push_back(pEvt->SaveAsJson());
        }
        json["events"] = eventJsonAry;
        m_logger->Log(DEBUG) << "Save filter-json : " << json.dump() << std::endl;
        return std::move(json);
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
    class Event_Impl : public EventStackFilter::Event
    {
    public:
        Event_Impl(EventStackFilter_Impl* owner, int64_t id, int64_t start, int64_t end, int32_t z,
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

        virtual ~Event_Impl()
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

        static Event_Impl* LoadFromJson(EventStackFilter_Impl* owner, const imgui_json::value& bpJson, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);

        int64_t Id() const override { return m_id; }
        int64_t Start() const override { return m_start; }
        int64_t End() const override { return m_end; }
        int64_t Length() const override { return m_end-m_start; }
        int32_t Z() const override { return m_z; }
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
                m_pBp->Blueprint_RunFilter(inMat, outMat);
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

        EventStackFilter* GetOwner() override
        {
            return static_cast<EventStackFilter*>(m_owner);
        }

        string GetError() const override
        {
            return m_owner->GetError();
        }        

        imgui_json::value SaveAsJson() const
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
        Event_Impl(EventStackFilter_Impl* owner) : m_owner(owner) {}

    private:
        EventStackFilter_Impl* m_owner;
        int64_t m_id{-1};
        BluePrint::BluePrintUI* m_pBp{nullptr};
        ImGui::KeyPointEditor* m_pKp{nullptr};
        int64_t m_start;
        int64_t m_end;
        int32_t m_z;
    };

    bool EnrollEvent(Event_Impl* pEvent)
    {
        for (auto e : m_eventList)
        {
            if (e->Id() == pEvent->Id())
            {
                ostringstream oss; oss << "Duplicated id! Already contained an event with id '" << pEvent->Id() << "'.";
                m_errMsg = oss.str();
                return false;
            }
            bool hasOverlap = Event::CheckEventOverlapped(e, pEvent->Start(), pEvent->End(), pEvent->Z());
            if (hasOverlap)
            {
                ostringstream oss; oss << "Can not enroll this event! It has overlap with the existing ones.";
                m_errMsg = oss.str();
                return false;
            }
        }
        m_eventList.push_back(pEvent);
        m_eventList.sort(EVENT_COMPARATOR);
        return true;
    }

private:
    static function<bool(Event*, Event*)> EVENT_COMPARATOR;

private:
    ALogger* m_logger;
    VideoClip* m_pClip{nullptr};
    list<Event*> m_eventList;
    int64_t m_editingEventId{-1};
    BluePrint::BluePrintCallbackFunctions m_bpCallbacks;
    void* m_tlHandle{nullptr};
    string m_errMsg;
};

bool EventStackFilter::Event::CheckEventOverlapped(const EventStackFilter::Event* e, int64_t start, int64_t end, int32_t z)
{
    if (z == e->Z() &&
       (start >= e->Start() && start < e->End() || end > e->Start() && end <= e->End() ||
        start < e->Start() && end > e->End()))
        return true;
    return false;
}

function<bool (EventStackFilter::Event*, EventStackFilter::Event*)> EventStackFilter_Impl::EVENT_COMPARATOR =
[] (EventStackFilter::Event* a, EventStackFilter::Event* b)
{ return a->Z() < b->Z() || (a->Z() == b->Z() && a->Start() < b->Start()); };

EventStackFilter_Impl::Event_Impl* EventStackFilter_Impl::Event_Impl::LoadFromJson(
        EventStackFilter_Impl* owner, const imgui_json::value& eventJson, const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    owner->m_logger->Log(DEBUG) << "Load EventJson : " << eventJson.dump() << endl;
    EventStackFilter_Impl::Event_Impl* pEvent = new EventStackFilter_Impl::Event_Impl(owner);
    string itemName = "id";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvent->m_id = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        delete pEvent;
        return nullptr;
    }
    itemName = "start";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvent->m_start = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        delete pEvent;
        return nullptr;
    }
    itemName = "end";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvent->m_end = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        delete pEvent;
        return nullptr;
    }
    itemName = "z";
    if (eventJson.contains(itemName) && eventJson[itemName].is_number())
    {
        pEvent->m_z = eventJson[itemName].get<imgui_json::number>();
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        delete pEvent;
        return nullptr;
    }
    itemName = "bp";
    if (eventJson.contains(itemName))
    {
        pEvent->m_pBp = new BluePrint::BluePrintUI();
        pEvent->m_pBp->Initialize();
        pEvent->m_pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(static_cast<MediaCore::VideoFilter*>(owner)));
        auto bpJson = eventJson[itemName];
        pEvent->m_pBp->File_New_Filter(bpJson, "VideoFilter", "Video");
        if (!pEvent->m_pBp->Blueprint_IsValid())
        {
            owner->m_errMsg = "BAD event json! Invalid blueprint json.";
            delete pEvent;
            return nullptr;
        }
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        delete pEvent;
        return nullptr;
    }
    itemName = "kp";
    if (eventJson.contains(itemName))
    {
        pEvent->m_pKp = new ImGui::KeyPointEditor();
        pEvent->m_pKp->Load(eventJson[itemName]);
        pEvent->m_pKp->SetRangeX(0, pEvent->Length(), true);
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        delete pEvent;
        return nullptr;
    }
    return pEvent;
}

static const auto EVENT_STACK_FILTER_DELETER = [] (VideoFilter* p) {
    EventStackFilter_Impl* ptr = dynamic_cast<EventStackFilter_Impl*>(p);
    delete ptr;
};

VideoFilter::Holder EventStackFilter::CreateInstance(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    return VideoFilter::Holder(new EventStackFilter_Impl(bpCallbacks), EVENT_STACK_FILTER_DELETER);
}

VideoFilter::Holder EventStackFilter::LoadFromJson(const imgui_json::value& json, const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    if (!json.contains("name") || !json["name"].is_string())
        return nullptr;
    string filterName = json["name"].get<imgui_json::string>();
    if (filterName != "EventStackFilter")
        return nullptr;
    auto pFilter = new EventStackFilter_Impl(bpCallbacks);
    if (json.contains("events") && json["events"].is_array())
    {
        auto& evtAry = json["events"].get<imgui_json::array>();
        for (auto& evtJson : evtAry)
        {
            auto pEvent = EventStackFilter_Impl::Event_Impl::LoadFromJson(pFilter, evtJson, bpCallbacks);
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
    return VideoFilter::Holder(pFilter, EVENT_STACK_FILTER_DELETER);
}
}