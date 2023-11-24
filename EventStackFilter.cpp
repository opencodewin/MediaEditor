#include <algorithm>
#include <functional>
#include <sstream>
#include <VideoBlender.h>
#include <MatMath.h>
#include "EventStackFilter.h"

using namespace std;
using namespace MediaCore;
using namespace Logger;

namespace MEC
{
class EventStack_Base;

class Event_Base : public virtual Event
{
public:
    virtual ~Event_Base()
    {
        if (m_pKp)
        {
            delete m_pKp;
            m_pKp = nullptr;
        }
        if (m_pBp) 
        {
            m_pBp->Finalize(); 
            delete m_pBp;
            m_pBp = nullptr;
        }
    }

    int64_t Id() const override { return m_id; }
    int64_t Start() const override { return m_start; }
    int64_t End() const override { return m_end; }
    int64_t Length() const override { return m_end-m_start; }
    int32_t Z() const override { return m_z; }
    uint32_t Status() const override { return m_status; }
    bool IsInRange(int64_t pos) const override { return pos >= m_start && pos < m_end; }
    BluePrint::BluePrintUI* GetBp() override { return m_pBp; }
    ImGui::KeyPointEditor* GetKeyPoint() override { return m_pKp; }
    void ChangeId(int64_t id) override { m_id = id; }
    bool ChangeRange(int64_t start, int64_t end) override;
    bool Move(int64_t start, int32_t z) override;
    EventStack* GetOwner() override;
    string GetError() const override;
    void SetStatus(uint32_t status) override { m_status = status; }
    void SetStatus(int bit, int val) override { m_status = (m_status & ~(1UL << bit)) | (val << bit); }

    void SetStart(int64_t start) { m_start = start; }
    void SetEnd(int64_t end) { m_end = end; }
    void SetZ(int32_t z) { m_z = z; }
    virtual void UpdateKeyPointRange() { m_pKp->SetTimeRange(0, Length(), true); }

    void SetBluePrintCallbacks(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
    {
        m_pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(&m_filterCtx));
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

protected:
    Event_Base(EventStack_Base* owner, int64_t id, int64_t start, int64_t end, int32_t z,
            const BluePrint::BluePrintCallbackFunctions& bpCallbacks);
    Event_Base(EventStack_Base* owner);

protected:
    EventStack_Base* m_owner;
    EventStackFilterContext m_filterCtx;
    int64_t m_id{-1};
    BluePrint::BluePrintUI* m_pBp{nullptr};
    ImGui::KeyPointEditor* m_pKp{nullptr};
    int64_t m_start;
    int64_t m_end;
    int32_t m_z{-1};
    uint32_t m_status{0};
};

class EventStack_Base : public virtual EventStack
{
public:
    EventStack_Base(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
        : m_bpCallbacks(bpCallbacks)
    {}

    virtual ~EventStack_Base()
    {
        m_eventList.clear();
    }

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

        Event::Holder hEvt = CreateNewEvent(id, start, end, z);
        m_eventList.push_back(hEvt);
        m_eventList.sort(EVENTLIST_COMPARATOR);
        return hEvt;
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
        Event_Base* pEvtBase = dynamic_cast<Event_Base*>(hEvt.get());
        auto z = pEvtBase->Z();
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
        pEvtBase->SetStart(start);
        pEvtBase->SetEnd(end);
        pEvtBase->UpdateKeyPointRange();
        m_eventList.sort(EVENTLIST_COMPARATOR);
        return true;
    }

    bool MoveEvent(int64_t id, int64_t start, int32_t z) override
    {
        auto hEvt = GetEvent(id);
        if (!hEvt)
            return false;
        Event_Base* pEvtBase = dynamic_cast<Event_Base*>(hEvt.get());
        auto end = pEvtBase->End()+(start-pEvtBase->Start());
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
        pEvtBase->SetStart(start);
        pEvtBase->SetEnd(end);
        pEvtBase->SetZ(z);
        m_eventList.sort(EVENTLIST_COMPARATOR);
        return true;
    }

    bool MoveAllEvents(int64_t offset) override
    {
        for (auto e : m_eventList)
        {
        Event_Base* pEvtBase = dynamic_cast<Event_Base*>(e.get());
            auto newStart = pEvtBase->Start()+offset;
            auto newEnd = pEvtBase->End()+offset;
            pEvtBase->SetStart(newStart);
            pEvtBase->SetEnd(newEnd);
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

protected:
    static function<bool(const Event::Holder&,const Event::Holder&)> EVENTLIST_COMPARATOR;

    virtual Event::Holder CreateNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) = 0;

public:
    ALogger* m_logger;
    list<Event::Holder> m_eventList;
    int64_t m_editingEventId{-1};
    BluePrint::BluePrintCallbackFunctions m_bpCallbacks;
    void* m_tlHandle{nullptr};
    string m_errMsg;
};

function<bool(const Event::Holder&,const Event::Holder&)> EventStack_Base::EVENTLIST_COMPARATOR = [] (const Event::Holder& a, const Event::Holder& b)
{ return Event::EVENT_ORDER_COMPARATOR(*a, *b); };

Event_Base::Event_Base(EventStack_Base* owner, int64_t id, int64_t start, int64_t end, int32_t z,
        const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
    : m_owner(owner), m_id(id), m_start(start), m_end(end), m_z(z)
{
    m_pKp = new ImGui::KeyPointEditor();
    m_pKp->SetTimeRange(0, end-start, true);

    m_pBp = new BluePrint::BluePrintUI();
    m_pBp->Initialize();
    m_filterCtx = {reinterpret_cast<void*>(static_cast<EventStack*>(owner)), reinterpret_cast<void*>(static_cast<Event*>(this))};
    m_pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(&m_filterCtx));
}

Event_Base::Event_Base(EventStack_Base* owner) : m_owner(owner)
{
    m_filterCtx = {reinterpret_cast<void*>(static_cast<EventStack*>(owner)), reinterpret_cast<void*>(static_cast<Event*>(this))};
}

bool Event_Base::ChangeRange(int64_t start, int64_t end)
{
    return m_owner->ChangeEventRange(m_id, start, end);
}

bool Event_Base::Move(int64_t start, int32_t z)
{
    return m_owner->MoveEvent(m_id, start, z);
}

EventStack* Event_Base::GetOwner()
{
    return static_cast<EventStack*>(m_owner);
}

string Event_Base::GetError() const
{
    return m_owner->GetError();
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

class VideoEventStackFilter_Impl final : public VideoEventStackFilter, public EventStack_Base
{
public:
    VideoEventStackFilter_Impl(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
        : EventStack_Base(bpCallbacks)
    {
        m_logger = GetLogger("VideoEventStackFilter");
    }

    ~VideoEventStackFilter_Impl()
    {
        m_pClip = nullptr;
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

    const VideoClip* GetVideoClip() const override
    {
        return m_pClip;
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

    Event::Holder RestoreEventFromJson(const imgui_json::value& eventJson) override
    {
        auto hEvent = VideoEvent_Impl::LoadFromJson(this, eventJson, m_bpCallbacks);
        if (!hEvent)
            return nullptr;
        if (!EnrollEvent(hEvent))
            return nullptr;
        return hEvent;
    }

public:
    class VideoEvent_Impl final : public Event_Base, public VideoEvent
    {
    public:
        VideoEvent_Impl(VideoEventStackFilter_Impl* owner, int64_t id, int64_t start, int64_t end, int32_t z,
                const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
            : Event_Base(owner, id, start, end, z, bpCallbacks)
        {
            imgui_json::value emptyJson;
            m_pBp->File_New_Filter(emptyJson, "VideoEventBp", "Video");
        }

        ~VideoEvent_Impl()
        {}

        static Event::Holder LoadFromJson(VideoEventStackFilter_Impl* owner, const imgui_json::value& bpJson, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);

        ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override
        {
            ImGui::ImMat outMat(vmat);
            if (m_pBp->Blueprint_IsExecutable())
            {
                // setup bp input curve
                const int iCurveCnt = m_pKp->GetCurveCount();
                for (int i = 0; i < iCurveCnt; i++)
                {
                    auto name = m_pKp->GetCurveName(i);
                    auto value = m_pKp->GetValueByDim(i, pos, ImGui::ImCurveEdit::DIM_X);
                    m_pBp->Blueprint_SetFilter(name, value);
                }
                ImGui::ImMat inMat(vmat);
                m_pBp->Blueprint_RunFilter(inMat, outMat, pos, Length());

                if (!m_ahMaskCreators.empty())
                {
                    const int64_t i64Tick = pos;
                    ImGui::ImMat mCombinedMask;
                    const auto szMaskCnt = m_ahMaskCreators.size();
                    for (auto i = 0; i < szMaskCnt; i++)
                    {
                        auto mMask = m_ahMaskCreators[i]->GetMask(ImGui::MaskCreator::AA, true, IM_DT_FLOAT32, 1, 0, i64Tick);
                        if (mCombinedMask.empty())
                            mCombinedMask = mMask;
                        else
                            MatUtils::Max(mCombinedMask, mMask);
                    }
                    if (!mCombinedMask.empty())
                    {
                        if (!m_hBlender) m_hBlender = MediaCore::VideoBlender::CreateInstance();
                        outMat = m_hBlender->Blend(outMat, inMat, mCombinedMask);
                    }
                }
            }
            return outMat;
        }

        void UpdateKeyPointRange() override
        {
            Event_Base::UpdateKeyPointRange();
            for (auto& hMaskCreator : m_ahMaskCreators)
                hMaskCreator->SetTickRange(0, Length());
        }

        ImGui::MaskCreator::Holder CreateNewMask(const string& name, const MatUtils::Size2i& szMaskSize) override
        {
            auto hMaskCreator = ImGui::MaskCreator::CreateInstance(szMaskSize, name);
            hMaskCreator->SetTickRange(0, Length());
            m_ahMaskCreators.push_back(hMaskCreator);
            return hMaskCreator;
        }

        int GetMaskCount() const override
        {
            return m_ahMaskCreators.size();
        }

        int GetMaskCount(int64_t nodeId) const override
        {
            auto iter = m_mapEffectMaskTable.find(nodeId);
            if (iter == m_mapEffectMaskTable.end())
                return 0;
            return iter->second.size();
        }

        const ImGui::MaskCreator::Holder GetMaskCreator(size_t index) const override
        {
            const auto szMaskCnt = m_ahMaskCreators.size();
            if (index >= szMaskCnt)
            {
                ostringstream oss; oss << "FAILED to get mask json! Event with id (" << m_id << ") has only " << szMaskCnt << " masks, cannot get mask at index " << index << ".";
                m_owner->m_errMsg = oss.str();
                return nullptr;
            }
            return m_ahMaskCreators[index];
        }

        const ImGui::MaskCreator::Holder GetMaskCreator(int64_t nodeId, size_t index) const
        {
            auto iter = m_mapEffectMaskTable.find(nodeId);
            if (iter == m_mapEffectMaskTable.end())
            {
                ostringstream oss; oss << "FAILED to get mask json! No mask is found for node id (" <<  nodeId << ").";
                m_owner->m_errMsg = oss.str();
                return nullptr;
            }
            const auto& ahMaskCreators = iter->second;
            if (index >= ahMaskCreators.size())
            {
                ostringstream oss; oss << "FAILED to get mask json! Node with id (" << nodeId << ") has only " << ahMaskCreators.size() << " masks, cannot get mask at index " << index << ".";
                m_owner->m_errMsg = oss.str();
                return nullptr;
            }
            return ahMaskCreators[index];
        }

        bool RemoveMask(size_t index) override
        {
            const auto szMaskCnt = m_ahMaskCreators.size();
            if (index >= szMaskCnt)
            {
                ostringstream oss; oss << "FAILED to remove mask json! Event with id (" << m_id << ") has only " << szMaskCnt << " masks, cannot remove mask at index " << index << ".";
                m_owner->m_errMsg = oss.str();
                return false;
            }
            auto itDel = m_ahMaskCreators.begin()+index;
            m_ahMaskCreators.erase(itDel);
            return true;
        }

        bool RemoveMask(int64_t nodeId, size_t index) override
        {
            auto iter = m_mapEffectMaskTable.find(nodeId);
            if (iter == m_mapEffectMaskTable.end())
            {
                ostringstream oss; oss << "FAILED to remove mask json! No mask is found for node id (" <<  nodeId << ").";
                m_owner->m_errMsg = oss.str();
                return false;
            }
            auto& ahMaskCreators = iter->second;
            if (index >= ahMaskCreators.size())
            {
                ostringstream oss; oss << "FAILED to remove mask json! Node with id (" << nodeId << ") has only " << ahMaskCreators.size() << " masks, cannot remove mask at index " << index << ".";
                m_owner->m_errMsg = oss.str();
                return false;
            }
            auto itDel = ahMaskCreators.begin()+index;
            ahMaskCreators.erase(itDel);
            return true;
        }

        imgui_json::value SaveAsJson() const override
        {
            auto j = Event_Base::SaveAsJson();
            imgui_json::array ajnMaskCreators;
            for (const auto& hMaskCreator : m_ahMaskCreators)
            {
                imgui_json::value jnMask;
                if (!hMaskCreator->SaveAsJson(jnMask))
                {
                    ostringstream oss; oss << "FAILED to save 'ImGui::MaskCreator' instance as json! Error is '" << hMaskCreator->GetError() << "'.";
                    throw runtime_error(oss.str());
                }
                ajnMaskCreators.push_back(jnMask);
            }
            j["event_masks"] = ajnMaskCreators;
            imgui_json::array maskTableJson;
            for (auto& elem : m_mapEffectMaskTable)
            {
                imgui_json::value subj;
                subj["node_id"] = imgui_json::number(elem.first);
                ajnMaskCreators.clear();
                for (const auto& hMaskCreator : elem.second)
                {
                    imgui_json::value jnMask;
                    if (!hMaskCreator->SaveAsJson(jnMask))
                    {
                        ostringstream oss; oss << "FAILED to save 'ImGui::MaskCreator' instance as json! Error is '" << hMaskCreator->GetError() << "'.";
                        throw runtime_error(oss.str());
                    }
                    ajnMaskCreators.push_back(jnMask);
                }
                subj["masks"] = ajnMaskCreators;
                maskTableJson.push_back(subj);
            }
            j["effect_mask_table"] = maskTableJson;
            return j;
        }

    private:
        MediaCore::VideoBlender::Holder m_hBlender;

    private:
        VideoEvent_Impl(VideoEventStackFilter_Impl* owner) : Event_Base(owner) {}

        vector<ImGui::MaskCreator::Holder> m_ahMaskCreators;
        unordered_map<int64_t, vector<ImGui::MaskCreator::Holder>> m_mapEffectMaskTable;
    };

    static const function<void(Event*)> VIDEO_EVENT_DELETER;

protected:
    Event::Holder CreateNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) override
    {
        auto pEvtImpl = new VideoEvent_Impl(this, id, start, end, z, m_bpCallbacks);
        return Event::Holder(pEvtImpl, VIDEO_EVENT_DELETER);
    }

private:
    VideoClip* m_pClip{nullptr};
};

Event::Holder
VideoEventStackFilter_Impl::VideoEvent_Impl::LoadFromJson(
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
        pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(&pEvtImpl->m_filterCtx));
        auto bpJson = eventJson[itemName];
        pBp->File_New_Filter(bpJson, "VideoEventBp", "Video");
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
        pKp->SetTimeRange(0, pEvtImpl->Length(), true);
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    itemName = "event_masks";
    if (eventJson.contains(itemName))
    {
        const auto& ajnMasks = eventJson[itemName].get<imgui_json::array>();
        for (const auto& jnMask : ajnMasks)
        {
            auto hMaskCreator = ImGui::MaskCreator::LoadFromJson(jnMask);
            hMaskCreator->SetTickRange(0, pEvtImpl->Length());
            pEvtImpl->m_ahMaskCreators.push_back(hMaskCreator);
        }
    }
    itemName = "effect_mask_table";
    if (eventJson.contains(itemName))
    {
        const auto& maskTableJn = eventJson[itemName].get<imgui_json::array>();
        for (const auto& elemJn : maskTableJn)
        {
            int64_t nodeId = elemJn["node_id"].get<imgui_json::number>();
            const auto& ajnMasks = elemJn["masks"].get<imgui_json::array>();
            vector<ImGui::MaskCreator::Holder> ahMaskCreators;
            for (const auto& jnMask : ajnMasks)
            {
                auto hMaskCreator = ImGui::MaskCreator::LoadFromJson(jnMask);
                hMaskCreator->SetTickRange(0, pEvtImpl->Length());
                ahMaskCreators.push_back(hMaskCreator);
            }
            pEvtImpl->m_mapEffectMaskTable.emplace(nodeId, ahMaskCreators);
        }
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
                Log(Error) << "FAILED to create VideoEventStackFilter::Event isntance from Json! Error is '" << pFilter->GetError() << "'." << endl;
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

class AudioEventStackFilter_Impl final : public AudioEventStackFilter, public EventStack_Base
{
public:
    AudioEventStackFilter_Impl(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
        : EventStack_Base(bpCallbacks)
    {
        m_logger = GetLogger("AudioEventStackFilter");
    }

    ~AudioEventStackFilter_Impl()
    {
        m_pClip = nullptr;
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

    void ApplyTo(AudioClip* clip) override
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

    ImGui::ImMat FilterPcm(const ImGui::ImMat& amat, int64_t pos, int64_t dur) override
    {
        list<Event::Holder> effectiveEvents;
        for (auto e : m_eventList)
        {
            if (e->IsInRange(pos))
                effectiveEvents.push_back(e);
        }
        ImGui::ImMat outM = amat;
        for (auto& e : effectiveEvents)
        {
            AudioEvent_Impl* pEvtImpl = dynamic_cast<AudioEvent_Impl*>(e.get());
            outM = pEvtImpl->FilterPcm(outM, pos-pEvtImpl->Start(), dur);
        }
        return outM;
    }

    const AudioClip* GetAudioClip() const override
    {
        return m_pClip;
    }

    imgui_json::value SaveAsJson() const override
    {
        imgui_json::value json;
        json["name"] = imgui_json::string(GetFilterName());
        imgui_json::array eventJsonAry;
        for (auto& e : m_eventList)
        {
            AudioEvent_Impl* pEvtImpl = dynamic_cast<AudioEvent_Impl*>(e.get());
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
            auto pEvt = dynamic_cast<AudioEvent_Impl*>(hEvent.get());
            pEvt->SetBluePrintCallbacks(bpCallbacks);
        }
        m_bpCallbacks = bpCallbacks;
    }

    Event::Holder RestoreEventFromJson(const imgui_json::value& eventJson) override
    {
        auto hEvent = AudioEvent_Impl::LoadFromJson(this, eventJson, m_bpCallbacks);
        if (!hEvent)
            return nullptr;
        if (!EnrollEvent(hEvent))
            return nullptr;
        return hEvent;
    }

public:
    class AudioEvent_Impl final : public Event_Base, public AudioEvent
    {
    public:
        AudioEvent_Impl(AudioEventStackFilter_Impl* owner, int64_t id, int64_t start, int64_t end, int32_t z,
                const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
            : Event_Base(owner, id, start, end, z, bpCallbacks)
        {
            imgui_json::value emptyJson;
            m_pBp->File_New_Filter(emptyJson, "AudioEventBp", "Audio");
        }

        ~AudioEvent_Impl()
        {}

        static Event::Holder LoadFromJson(AudioEventStackFilter_Impl* owner, const imgui_json::value& bpJson, const BluePrint::BluePrintCallbackFunctions& bpCallbacks);

        ImGui::ImMat FilterPcm(const ImGui::ImMat& amat, int64_t pos, int64_t dur) override
        {
            ImGui::ImMat outMat(amat);
            if (m_pBp->Blueprint_IsExecutable())
            {
                // setup bp input curve
                for (int i = 0; i < m_pKp->GetCurveCount(); i++)
                {
                    auto name = m_pKp->GetCurveName(i);
                    auto value = m_pKp->GetValueByDim(i, pos, ImGui::ImCurveEdit::DIM_X);
                    m_pBp->Blueprint_SetFilter(name, value);
                }
                ImGui::ImMat inMat(amat);
                m_pBp->Blueprint_RunFilter(inMat, outMat, pos, Length());
            }
            return outMat;
        }

    private:
        AudioEvent_Impl(AudioEventStackFilter_Impl* owner) : Event_Base(owner)
        {}
    };

    static const function<void(Event*)> AUDIO_EVENT_DELETER;

protected:
    Event::Holder CreateNewEvent(int64_t id, int64_t start, int64_t end, int32_t z) override
    {
        auto pEvtImpl = new AudioEvent_Impl(this, id, start, end, z, m_bpCallbacks);
        return Event::Holder(pEvtImpl, AUDIO_EVENT_DELETER);
    }

private:
    AudioClip* m_pClip{nullptr};
};

Event::Holder
AudioEventStackFilter_Impl::AudioEvent_Impl::LoadFromJson(
        AudioEventStackFilter_Impl* owner, const imgui_json::value& eventJson, const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    owner->m_logger->Log(DEBUG) << "Load EventJson : " << eventJson.dump() << endl;
    auto pEvtImpl = new AudioEventStackFilter_Impl::AudioEvent_Impl(owner);
    Event::Holder hEvt(pEvtImpl, AUDIO_EVENT_DELETER);
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
        pBp->SetCallbacks(bpCallbacks, reinterpret_cast<void*>(&pEvtImpl->m_filterCtx));
        auto bpJson = eventJson[itemName];
        pBp->File_New_Filter(bpJson, "AudioEventBp", "Audio");
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
        pKp->SetTimeRange(0, pEvtImpl->Length(), true);
    }
    else
    {
        owner->m_errMsg = "BAD event json! Missing '"+itemName+"'.";
        return nullptr;
    }
    return hEvt;
}

const function<void(Event*)> AudioEventStackFilter_Impl::AUDIO_EVENT_DELETER = [] (Event* p) {
    AudioEventStackFilter_Impl::AudioEvent_Impl* ptr = dynamic_cast<AudioEventStackFilter_Impl::AudioEvent_Impl*>(p);
    delete ptr;
};

static const auto AUDIO_EVENT_STACK_FILTER_DELETER = [] (AudioFilter* p) {
    AudioEventStackFilter_Impl* ptr = dynamic_cast<AudioEventStackFilter_Impl*>(p);
    delete ptr;
};

AudioFilter::Holder AudioEventStackFilter::CreateInstance(const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    return AudioFilter::Holder(new AudioEventStackFilter_Impl(bpCallbacks), AUDIO_EVENT_STACK_FILTER_DELETER);
}

AudioFilter::Holder AudioEventStackFilter::LoadFromJson(const imgui_json::value& json, const BluePrint::BluePrintCallbackFunctions& bpCallbacks)
{
    if (!json.contains("name") || !json["name"].is_string())
        return nullptr;
    string filterName = json["name"].get<imgui_json::string>();
    if (filterName != "EventStackFilter")
        return nullptr;
    auto pFilter = new AudioEventStackFilter_Impl(bpCallbacks);
    if (json.contains("events") && json["events"].is_array())
    {
        auto& evtAry = json["events"].get<imgui_json::array>();
        for (auto& evtJson : evtAry)
        {
            auto pEvent = AudioEventStackFilter_Impl::AudioEvent_Impl::LoadFromJson(pFilter, evtJson, bpCallbacks);
            if (!pEvent)
            {
                Log(Error) << "FAILED to create AudioEventStackFilter::Event isntance from Json! Error is '" << pFilter->GetError() << "'." << endl;
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
    return AudioFilter::Holder(pFilter, AUDIO_EVENT_STACK_FILTER_DELETER);
}
}