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

#include "MediaTimeline.h"
#include "MediaInfo.h"
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <imgui_fft.h>
#include <implot.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <vector>
#include <utility>
#include <ThreadUtils.h>
#include <MatUtilsImVecHelper.h>
#include "EventStackFilter.h"
#include "TextureManager.h"
#include "MatUtils.h"
#include "Logger.h"
#include "DebugHelper.h"

const MediaTimeline::audio_band_config DEFAULT_BAND_CFG[10] = {
    { 32,       32,         0 },        { 64,       64,         0 },
    { 125,      125,        0 },        { 250,      250,        0 },
    { 500,      500,        0 },        { 1000,     1000,       0 },
    { 2000,     2000,       0 },        { 4000,     4000,       0 },
    { 8000,     8000,       0 },        { 16000,    16000,      0 },
};

static bool TimelineButton(ImDrawList *draw_list, const char * label, ImVec2 pos, ImVec2 size, std::string tooltips = "", ImVec4 hover_color = ImVec4(0.5f, 0.5f, 0.75f, 1.0f))
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect rect(pos, pos + size);
    bool overButton = rect.Contains(io.MousePos);
    if (overButton)
        draw_list->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(hover_color), 2);
    ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
    ImGui::SetWindowFontScale(0.75);
    draw_list->AddText(pos, ImGui::GetColorU32(color), label);
    ImGui::SetWindowFontScale(1.0);
    if (overButton && !tooltips.empty() && ImGui::BeginTooltip())
    {
        ImGui::TextUnformatted(tooltips.c_str());
        ImGui::EndTooltip();
    }
    return overButton;
}

static int64_t alignTime(int64_t time, const MediaCore::Ratio& rate, bool useCeil = false)
{
    if (rate.den && rate.num)
    {
        const float frame_index_f = (double)time * rate.num / ((double)rate.den * 1000.0);
        const int64_t frame_index = useCeil ? (int64_t)ceil(frame_index_f) : (int64_t)floor(frame_index_f);
        time = round((double)frame_index * 1000 * rate.den / rate.num);
    }
    return time;
}

static int64_t frameTime(MediaCore::Ratio rate)
{
    if (rate.den && rate.num)
        return rate.den * 1000 / rate.num;
    else
        return 0;
}

static void frameStepTime(int64_t& time, int32_t offset, MediaCore::Ratio rate)
{
    if (rate.den && rate.num)
    {
        int64_t frame_index = (int64_t)floor((double)time * (double)rate.num / (double)rate.den / 1000.0 + 0.5);
        frame_index += offset;
        time = frame_index * 1000 * rate.den / rate.num;
    }
}

static bool waveFrameResample(float * wave, int samples, int size, int start_offset, int size_max, int zoom, ImGui::ImMat& plot_frame_max, ImGui::ImMat& plot_frame_min)
{
    bool min_max = samples > 16;
    plot_frame_max.create_type(size, 1, 1, IM_DT_FLOAT32);
    plot_frame_min.create_type(size, 1, 1, IM_DT_FLOAT32);
    float * out_channel_data_max = (float *)plot_frame_max.data;
    float * out_channel_data_min = (float *)plot_frame_min.data;
    for (int i = 0; i < size; i++)
    {
        float max_val = -FLT_MAX;
        float min_val = FLT_MAX;
        if (!min_max)
        {
            if (i * samples + start_offset < size_max)
                min_val = max_val = wave[i * samples + start_offset];
        }
        else
        {
            for (int n = 0; n < samples; n += zoom)
            {
                if (i * samples + n + start_offset >= size_max)
                    break;
                float val = wave[i * samples + n + start_offset];
                if (max_val < val) max_val = val;
                if (min_val > val) min_val = val;
            }
            if (max_val < 0 && min_val < 0)
            {
                max_val = min_val;
            }
            else if (max_val > 0 && min_val > 0)
            {
                min_val = max_val;
            }
        }
        out_channel_data_max[i] = ImMin(max_val, 1.f);
        out_channel_data_min[i] = ImMax(min_val, -1.f);
    }
    return min_max;
}

static void waveformToMat(const MediaCore::Overview::Waveform::Holder wavefrom, ImGui::ImMat& mat, ImVec2 wave_size)
{
    int channels = wavefrom->pcm.size();
    if (channels > 2) channels = 2;
    int channel_height = wave_size.y / channels;
    ImVec2 channel_size(wave_size.x, channel_height);
    float wave_range = fmax(fabs(wavefrom->minSample), fabs(wavefrom->maxSample));
    mat.create(wave_size.x, wave_size.y, 4, (size_t)1, 4);
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 1.f, 0.f, 0.75f));
    for (int i = 0; i < channels; i++)
    {
        ImGui::PlotMat(mat, ImVec2(0, channel_height * i), &wavefrom->pcm[i][0], wavefrom->pcm[i].size(), 0, -wave_range / 2, wave_range / 2, channel_size, sizeof(float), true, true);
    }
    ImGui::PopStyleColor(2);
}

namespace MediaTimeline
{
/***********************************************************************************************************
 * ID Generator Member Functions
 ***********************************************************************************************************/
int64_t IDGenerator::GenerateID()
{
    return m_State ++;
}

void IDGenerator::SetState(int64_t state)
{
    m_State = state;
}

int64_t IDGenerator::State() const
{
    return m_State;
}
} // namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * MediaItem Struct Member Functions
 ***********************************************************************************************************/
MediaItem::MediaItem(const std::string& name, const std::string& path, uint32_t type, void* handle)
    : mName(name), mPath(path), mMediaType(type), mHandle(handle)
{
    TimeLine* timeline = (TimeLine*)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    mTxMgr = timeline->mTxMgr;
}

MediaItem::MediaItem(MediaCore::MediaParser::Holder hParser, void* handle)
    : mHandle(handle), mhParser(hParser)
{
    TimeLine* timeline = (TimeLine*)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    auto pVidstm = hParser->HasVideo() ? hParser->GetBestVideoStream() : nullptr;
    mMediaType = hParser->HasVideo() ? (hParser->IsImageSequence() ? MEDIA_SUBTYPE_VIDEO_IMAGE_SEQUENCE
            : (pVidstm->isImage ? MEDIA_SUBTYPE_VIDEO_IMAGE : MEDIA_VIDEO))
            : (hParser->HasAudio() ? MEDIA_AUDIO : MEDIA_UNKNOWN);
    mTxMgr = timeline->mTxMgr;
    mPath = hParser->GetUrl();
    mName = ImGuiHelper::path_filename(mPath);
}

MediaItem::~MediaItem()
{
    for (auto texture : mWaveformTextures) ImGui::ImDestroyTexture(texture);
    mWaveformTextures.clear();
    mMediaOverview = nullptr;
    mMediaThumbnail.clear();
}

bool MediaItem::ChangeSource(const std::string& name, const std::string& path)
{
    ReleaseItem();
    mName = name;
    mPath = path;
    return Initialize();
}

bool MediaItem::Initialize()
{
    mValid = false;
    if (mPath.empty() || !ImGuiHelper::file_exists(mPath))
        return false;

    TimeLine* timeline = (TimeLine*)mHandle;

    if (IS_TEXT(mMediaType))
    {
        mValid = true;
    }
    else
    {
        if (!mhParser)
        {
            mhParser = MediaCore::MediaParser::CreateInstance();
            if (IS_IMAGESEQ(mMediaType))
                mhParser->OpenImageSequence({25, 1}, mPath, ".+_([[:digit:]]{1,})\\.png", false, true);
            else
                mhParser->Open(mPath);
            if (!mhParser->IsOpened())
                return false;
        }

        mMediaOverview = MediaCore::Overview::CreateInstance();
        mMediaOverview->EnableHwAccel(timeline->mHardwareCodec);
        RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
        if (mTxMgr->GetTexturePoolAttributes(VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME, tTxPoolAttrs))
            mMediaOverview->SetSnapshotSize(tTxPoolAttrs.tTxSize.x, tTxPoolAttrs.tTxSize.y);
        else
            mMediaOverview->SetSnapshotResizeFactor(0.05, 0.05);
        if (!mMediaOverview->Open(mhParser, 64))
            return false;
        mSrcLength = mMediaOverview->GetMediaInfo()->duration * 1000;
        mValid = true;
    }
    return true;
}

void MediaItem::ReleaseItem()
{
    mMediaOverview = nullptr;
    mMediaThumbnail.clear();
    mSrcLength = 0;
    mValid = false;
}

void MediaItem::UpdateThumbnail()
{
    if (mMediaOverview && mMediaOverview->IsOpened())
    {
        auto count = mMediaOverview->GetSnapshotCount();
        if (mMediaThumbnail.size() >= count)
            return;
        std::vector<ImGui::ImMat> snapshots;
        if (mMediaOverview->GetSnapshots(snapshots))
        {
            for (int i = 0; i < snapshots.size(); i++)
            {
                if (i >= mMediaThumbnail.size() && !snapshots[i].empty())
                {
                    auto hTx = mTxMgr->GetGridTextureFromPool(VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME);
                    if (hTx)
                    {
                        hTx->RenderMatToTexture(snapshots[i]);
                        mMediaThumbnail.push_back(hTx);
                    }
                }
            }
        }
    }
}
} //namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * Event track Struct Member Functions
 ***********************************************************************************************************/
EventTrack::EventTrack(int64_t id, void* handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    mClipID = id;
    mHandle = handle;
}

EventTrack* EventTrack::Load(const imgui_json::value& value, void * handle)
{
    Clip * clip = (Clip *)handle;
    int64_t clip_id = -1;
    if (value.contains("ClipID"))
    {
        auto& val = value["ClipID"];
        if (val.is_number()) clip_id = val.get<imgui_json::number>();
    }
    EventTrack * new_track = new EventTrack(clip_id, clip->mHandle);
    if (new_track)
    {
        if (value.contains("ID"))
        {
            auto& val = value["ID"];
            if (val.is_number()) new_track->mID = val.get<imgui_json::number>();
        }
        if (value.contains("Expanded"))
        {
            auto& val = value["Expanded"];
            if (val.is_boolean()) new_track->mExpanded = val.get<imgui_json::boolean>();
        }
        // load and check event into track
        const imgui_json::array* eventIDArray = nullptr;
        if (imgui_json::GetPtrTo(value, "EventIDS", eventIDArray))
        {
            for (auto& id_val : *eventIDArray)
            {
                int64_t event_id = id_val.get<imgui_json::number>();
                new_track->m_Events.push_back(event_id);
            }
        }
    }
    return new_track;
}

void EventTrack::Save(imgui_json::value& value)
{
    value["ID"] = imgui_json::number(mID);
    value["ClipID"] = imgui_json::number(mClipID);
    value["Expanded"] = imgui_json::boolean(mExpanded);
    // save event ids
    imgui_json::value events;
    for (auto event : m_Events)
    {
        imgui_json::value event_id_value = imgui_json::number(event);
        events.push_back(event_id_value);
    }
    if (m_Events.size() > 0) value["EventIDS"] = events;
}

bool EventTrack::DrawContent(ImDrawList *draw_list, ImRect rect, int event_height, int curve_height, int64_t view_start, int64_t view_end, float pixelWidthMS, bool editable, bool& changed)
{
    bool mouse_hold = false;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return mouse_hold;
    auto clip = timeline->FindClipByID(mClipID);
    if (!clip || !clip->mEventStack)
        return mouse_hold;

    ImGui::PushClipRect(rect.Min, rect.Max, true);
    ImGui::SetCursorScreenPos(rect.Min);
    bool mouse_clicked = false;
    bool curve_hovered = false;

    // draw events
    for (auto event_id : m_Events)
    {
        bool draw_event = false;
        int64_t curve_start = 0;
        int64_t curve_end = 0;
        float cursor_start = 0;
        float cursor_end  = 0;
        ImDrawFlags flag = ImDrawFlags_RoundCornersNone;
        auto event = clip->mEventStack->GetEvent(event_id);
        if (!event) continue;
        if (event->IsInRange(view_start) && event->End() <= view_end)
        {
            /***********************************************************
             *         ----------------------------------------
             * XXXXXXXX|XXXXXXXXXXXXXXXXXXXXXX|
             *         ----------------------------------------
            ************************************************************/
            cursor_start = rect.Min.x;
            cursor_end = rect.Min.x + (event->End() - view_start) * pixelWidthMS;
            curve_start = view_start - event->Start();
            curve_end = event->End() - event->Start();
            draw_event = true;
            flag |= ImDrawFlags_RoundCornersRight;
        }
        else if (event->Start() >= view_start && event->End() <= view_end)
        {
            /***********************************************************
             *         ----------------------------------------
             *                  |XXXXXXXXXXXXXXXXXXXXXX|
             *         ----------------------------------------
            ************************************************************/
            cursor_start = rect.Min.x + (event->Start() - view_start) * pixelWidthMS;
            cursor_end = rect.Min.x + (event->End() - view_start) * pixelWidthMS;
            curve_start = 0;
            curve_end = event->End() - event->Start();
            draw_event = true;
            flag |= ImDrawFlags_RoundCornersAll;
        }
        else if (event->Start() >= view_start && event->IsInRange(view_end))
        {
            /***********************************************************
             *         ----------------------------------------
             *                         |XXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXX
             *         ----------------------------------------
            ************************************************************/
            cursor_start = rect.Min.x + (event->Start() - view_start) * pixelWidthMS;
            cursor_end = rect.Max.x;
            curve_start = 0;
            curve_end = view_end - event->Start();
            draw_event = true;
            flag |= ImDrawFlags_RoundCornersLeft;
        }
        else if (event->Start() <= view_start && event->End() >= view_end)
        {
            /***********************************************************
             *         ----------------------------------------
             *  XXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXX
             *         ----------------------------------------
            ************************************************************/
            cursor_start = rect.Min.x;
            cursor_end  = rect.Max.x;
            curve_start = view_start - event->Start();
            curve_end = view_end - event->Start();
            draw_event = true;
        }
        if (event->Start() == view_start)
            flag |= ImDrawFlags_RoundCornersLeft;
        if (event->End() == view_end)
            flag |= ImDrawFlags_RoundCornersRight;
        ImVec2 event_pos_min = ImVec2(cursor_start, rect.Min.y);
        ImVec2 event_pos_max = ImVec2(cursor_end, rect.Min.y + event_height);
        ImRect event_rect(event_pos_min, event_pos_max);
        if (!event_rect.Overlaps(rect))
        {
            draw_event = false;
        }
        if (event_rect.GetWidth() < 1)
        {
            draw_event = false;
        }

        if (draw_event && cursor_end > cursor_start)
        {
            auto pBP = event->GetBp();
            bool is_event_valid = pBP && pBP->Blueprint_IsExecutable();
            bool is_select = event->Status() & EVENT_SELECTED;
            bool is_hovered = event->Status() & EVENT_HOVERED;
            if (is_select)
                draw_list->AddRect(event_pos_min, event_pos_max, IM_COL32(0,255,0,224), 4, flag, 2.0f);
            else
                draw_list->AddRect(event_pos_min, event_pos_max, IM_COL32(128,128,255,224), 4, flag, 2.0f);
            auto event_color = is_event_valid ? (is_hovered ? IM_COL32(64, 64, 192, 128) : IM_COL32(32, 32, 192, 128)) : IM_COL32(192, 32, 32, 128);
            draw_list->AddRectFilled(event_pos_min, event_pos_max, event_color, 4, flag);
            if (pBP)
            {
                auto nodes = pBP->m_Document->m_Blueprint.GetNodes();
                ImGui::SetCursorScreenPos(event_pos_min);
                draw_list->PushClipRect(event_pos_min, event_pos_max, true);
                if (!nodes.empty() && event_pos_max.x - event_pos_min.x < 24)
                {
                    auto center_point = ImVec2(event_pos_min.x + (event_pos_max.x - event_pos_min.x) / 2, event_pos_min.y + (event_pos_max.y - event_pos_min.y) / 2);
                    draw_list->AddCircleFilled(center_point, 4, IM_COL32_WHITE, 16);
                }
                else
                {
                    int count = 0;
                    for (auto node : nodes)
                    {
                        if (!IS_ENTRY_EXIT_NODE(node->GetType()))
                        {
                            ImGui::SetCursorScreenPos(event_pos_min + ImVec2(16 * count, 0));
                            node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(26, 24));
                            count ++;
                        }
                    }
                }
                draw_list->PopClipRect();
            }
            if (is_hovered && editable)
            {
                if (!mouse_clicked && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    SelectEvent(event, false);
                    mouse_clicked = true;
                }
                // TODO::Dicky event draw tooltips
                //event->DrawTooltips();
            }
        }
        if (mExpanded && draw_event)
        {
            ImVec2 curve_pos_min = event_pos_min + ImVec2(0, event_height);
            ImVec2 curve_pos_max = event_pos_max + ImVec2(0, curve_height);
            ImRect curve_rect(curve_pos_min, curve_pos_max);
            curve_hovered |= curve_rect.Contains(ImGui::GetMousePos());
            ImGui::SetCursorScreenPos(curve_pos_min);
            ImGui::PushID(event_id);
            if (ImGui::BeginChild("##event_curve", curve_rect.GetSize(), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
            {
                ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
                ImVec2 sub_window_size = ImGui::GetWindowSize();
                draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_BLACK_DARK);
                bool _changed = false;
                auto pKP = event->GetKeyPoint();
                if (pKP)
                {
                    float current_time = timeline->mCurrentTime - clip->Start();
                    const auto frameRate = timeline->mhMediaSettings->VideoOutFrameRate();
                    ImVec2 alignT = { (float)frameRate.num, (float)frameRate.den*1000 };
                    pKP->SetCurveAlign(alignT, ImGui::ImCurveEdit::DIM_T);
                    mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        pKP,
                                                        sub_window_size, 
                                                        ImGui::GetID("##video_filter_event_keypoint_editor"),
                                                        editable,
                                                        current_time,
                                                        curve_start,
                                                        curve_end,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END, 
                                                        nullptr, // clippingRect
                                                        &_changed
                                                        );
                    if (_changed) { timeline->RefreshPreview(); changed |= _changed; }
                }
            }
            ImGui::EndChild();
            ImGui::PopID();
        }
    }

    if (m_Events.empty())
        mExpanded = false;
    else if (rect.Contains(ImGui::GetMousePos()) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !curve_hovered && editable)
        mExpanded = !mExpanded;

    ImGui::PopClipRect();
    return mouse_hold;
}

void EventTrack::SelectEvent(MEC::Event::Holder event, bool appand)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline || !event)
        return;
    auto clip = timeline->FindClipByID(mClipID);
    if (!clip)
        return;

    bool selected = true;
    bool is_selected = event->Status() & EVENT_SELECTED;
    if (appand && is_selected)
    {
        selected = false;
    }

    if (clip->mEventStack)
    {
        auto event_list = clip->mEventStack->GetEventList();
        for (auto _event : event_list)
        {
            if (_event->Id() != event->Id())
            {
                if (!appand)
                {
                    _event->SetStatus(EVENT_SELECTED_BIT, selected ? 0 : 1);
                    if (selected) _event->SetStatus(EVENT_NEED_SCROLL, 0);
                }
            }
        }
    }
    event->SetStatus(EVENT_SELECTED_BIT, selected ? 1 : 0);
    if (selected)
    {
        clip->bAttributeScrolling = false;
        event->SetStatus(EVENT_NEED_SCROLL, 1);
    }
    if (clip->mEventStack)
    {
        clip->mEventStack->SetEditingEvent(selected ? event->Id() : -1);
    }
}

MEC::Event::Holder EventTrack::FindPreviousEvent(int64_t id)
{
    MEC::Event::Holder found_event = nullptr;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline) return found_event;
    auto clip = timeline->FindClipByID(mClipID);
    if (!clip) return found_event;
    auto event = clip->FindEventByID(id);
    if (!event) return found_event;

    auto iter = std::find_if(m_Events.begin(), m_Events.end(), [id](const int64_t e) {
        return e == id;
    });
    if (iter == m_Events.begin() || iter == m_Events.end())
        return found_event;
    found_event = clip->FindEventByID(*(iter - 1));
    return found_event;
}

MEC::Event::Holder EventTrack::FindNextEvent(int64_t id)
{
    MEC::Event::Holder found_event = nullptr;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline) return found_event;
    auto clip = timeline->FindClipByID(mClipID);
    if (!clip) return found_event;
    auto event = clip->FindEventByID(id);
    if (!event) return found_event;

    auto iter = std::find_if(m_Events.begin(), m_Events.end(), [id](const int64_t e) {
        return e == id;
    });
    if (iter == m_Events.end() || iter == m_Events.end() - 1)
        return found_event;
    found_event = clip->FindEventByID(*(iter + 1));
    return found_event;
}

int64_t EventTrack::FindEventSpace(int64_t time)
{
    int64_t space = -1;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline) return space;
    auto clip = timeline->FindClipByID(mClipID);
    if (!clip) return space;
    for (auto event_id : m_Events)
    {
        auto event = clip->FindEventByID(event_id);
        if (event && event->Start() >= time)
        {
            space = event->Start() - time;
            break;
        }
    }
    return space;
}

void EventTrack::Update()
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline) return;
    auto clip = timeline->FindClipByID(mClipID);
    if (!clip) return;
    auto clip_track = timeline->FindTrackByClipID(clip->mID);
    if (clip_track) timeline->RefreshTrackView({clip_track->mID});
    // sort m_Events by start time
    std::sort(m_Events.begin(), m_Events.end(), [clip](const int64_t a, const int64_t b) {
        auto a_event = clip->FindEventByID(a);
        auto b_event = clip->FindEventByID(b);
        if (a_event && b_event)
            return a_event->Start() < b_event->Start();
        else
            return false;
    });
}
} //namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * Clip Struct Member Functions
 ***********************************************************************************************************/
Clip::Clip(TimeLine* pOwner, uint32_t u32Type) : mHandle(pOwner), mType(u32Type)
{
    mID = pOwner ? pOwner->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
}

Clip::Clip(TimeLine* pOwner, uint32_t u32Type, const std::string& strName, int64_t i64Start, int64_t i64End, int64_t i64StartOffset, int64_t i64EndOffset)
    : mHandle(pOwner), mType(u32Type), mName(strName), mStart(i64Start), mEnd(i64End), mStartOffset(i64StartOffset), mEndOffset(i64EndOffset)
{
    mID = pOwner ? pOwner->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
}

Clip::~Clip()
{
}

bool Clip::LoadFromJson(const imgui_json::value& j)
{
    std::string strAttrName;
    strAttrName = "ID";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mID = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }
    strAttrName = "Type";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mType = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }
    strAttrName = "MediaID";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mMediaID = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }
    strAttrName = "GroupID";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mGroupID = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::WARN) << "ABNORMAL json in 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found." << std::endl;
    }
    strAttrName = "Start";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mStart = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }
    strAttrName = "End";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mEnd = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }
    strAttrName = "StartOffset";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mStartOffset = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }
    strAttrName = "EndOffset";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        mEndOffset = j[strAttrName].get<imgui_json::number>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }
    strAttrName = "Path";
    if (j.contains(strAttrName) && j[strAttrName].is_string())
        mPath = j[strAttrName].get<imgui_json::string>();
    else
    {
        Logger::Log(Logger::WARN) << "ABNORMAL json in 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found." << std::endl;
    }
    strAttrName = "Name";
    if (j.contains(strAttrName) && j[strAttrName].is_string())
        mName = j[strAttrName].get<imgui_json::string>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'Clip::LoadFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        return false;
    }

    // load event tracks
    strAttrName = "EventTracks";
    if (j.contains(strAttrName) && j[strAttrName].is_array())
    {
        const auto& jnEventTracks = j[strAttrName].get<imgui_json::array>();
        for (const auto& jnEvtTrack : jnEventTracks)
        {
            EventTrack* pEvtTrack = EventTrack::Load(jnEvtTrack, this);
            if (pEvtTrack)
                mEventTracks.push_back(pEvtTrack);
            else
            {
                Logger::Log(Logger::WARN) << "FAILED to restore 'EventTrack' instance for 'Clip (id=" << mID << ", path=" << mPath <<", type=" << (int)mType
                        << ")'! Event track json is:" << std::endl;
                Logger::Log(Logger::WARN) << jnEvtTrack.dump() << std::endl << std::endl;
            }
        }
    }

    // save a copy of the json for this clip
    mClipJson = j;
    return true;
}

imgui_json::value Clip::SaveAsJson()
{
    imgui_json::value j;
    // save clip global info
    j["ID"] = imgui_json::number(mID);
    j["MediaID"] = imgui_json::number(mMediaID);
    j["GroupID"] = imgui_json::number(mGroupID);
    j["Type"] = imgui_json::number(mType);
    j["Path"] = mPath;
    j["Name"] = mName;
    j["Start"] = imgui_json::number(mStart);
    j["End"] = imgui_json::number(mEnd);
    j["StartOffset"] = imgui_json::number(mStartOffset);
    j["EndOffset"] = imgui_json::number(mEndOffset);
    // jnClip["Selected"] = imgui_json::boolean(bSelected);

    // save event track
    imgui_json::array jnEvtTracks;
    for (const auto pEvtTrack : mEventTracks)
    {
        imgui_json::value jnEvtTrack;
        pEvtTrack->Save(jnEvtTrack);
        jnEvtTracks.push_back(jnEvtTrack);
    }
    if (!jnEvtTracks.empty())
        j["EventTracks"] = jnEvtTracks;

    mClipJson = j;
    return std::move(j);
}

int64_t Clip::Cropping(int64_t diff, int type)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return 0;
    auto track = timeline->FindTrackByClipID(mID);
    if (!track || track->mLocked)
        return 0;
    if (IS_DUMMY(mType))
        return 0;
    diff = timeline->AlignTime(diff);
    if (diff == 0)
        return 0;

    int64_t length = Length();
    int64_t old_offset = mStartOffset;

    // cropping start
    if (type == 0)
    {
        // for clips that have length limitation
        if ((IS_VIDEO(mType) && !IS_IMAGE(mType)) || IS_AUDIO(mType))
        {
            if (diff + mStartOffset < 0)
                diff = -mStartOffset;  // mStartOffset can NOT be NEGATIVE
            int64_t newStart = mStart + diff;
            if (newStart >= mEnd)
            {
                newStart = timeline->AlignTimeToPrevFrame(mEnd);
                diff = newStart - mStart;
            }
            int64_t newStartOffset = mStartOffset + diff;
            assert(newStartOffset >= 0);
            int64_t newLength = length-diff;
            assert(newLength > 0);

            mStart = newStart;
            mStartOffset = newStartOffset;
        }
        // for clips that have no length limitation
        else
        {
            int64_t newStart = mStart + diff;
            if (newStart >= mEnd)
            {
                newStart = timeline->AlignTimeToPrevFrame(mEnd);
                diff = newStart - mStart;
            }
            int64_t newLength = length-diff;
            assert(newLength > 0);

            mStart = newStart;
        }
    }
    // cropping end
    else
    {
        // for clips that have length limitation
        if ((IS_VIDEO(mType) && !IS_IMAGE(mType)) || IS_AUDIO(mType))
        {
            if (mEndOffset < diff)
                diff = mEndOffset;  // mEndOffset can NOT be NEGATIVE
            int64_t newEnd = mStart + length + diff;
            if (newEnd <= mStart)
            {
                newEnd = timeline->AlignTimeToNextFrame(mStart);
                diff = newEnd - mEnd;
            }
            int64_t newEndOffset = mEndOffset-diff;
            assert(newEndOffset >= 0);
            int64_t newLength = length+diff;
            assert(newLength > 0);

            mEnd = newEnd;
            mEndOffset = newEndOffset;
        }
        // for clips that have no length limitation
        else
        {
            int64_t newEnd = mStart + length + diff;
            if (newEnd <= mStart)
            {
                newEnd = timeline->AlignTimeToNextFrame(mStart);
                diff = newEnd - mEnd;
            }
            int64_t newLength = length+diff;
            assert(newLength > 0);

            mEnd = newEnd;
        }
    }

#if 0 // TODO::Dicky editing item support cropping
    auto found = timeline->FindEditingItem(EDITING_CLIP, mID);
    if (found != -1)
    {
        auto item = timeline->mEditingItems[found];
        if (item && item->mEditingClip)
        {
            item->mEditingClip->mStart = Start();
            item->mEditingClip->mEnd = End();
            if (IS_VIDEO(item->mMediaType))
            {
                auto editing_clip = (EditingVideoClip *)item->mEditingClip;
                editing_clip->mhTransformFilter->GetKeyPoint()->SetRangeX(0, Length(), true);
            }
        }
    }
#endif

    track->Update();
    timeline->UpdateRange();
    // update clip's event time range
    if (mEventStack && type == 0)
    {
        mEventStack->MoveAllEvents(-diff);
    }
    return diff;
}

void Clip::Cutting(int64_t pos, int64_t gid, int64_t newClipId, std::list<imgui_json::value>* pActionList)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    auto track = timeline->FindTrackByClipID(mID);
    if (!track || track->mLocked)
        return;
    if (IS_DUMMY(mType))
        return;
    // check if the cut position is inside the clip
    auto cut_pos = pos;
    pos = timeline->AlignTime(pos);
    if (pos <= mStart || pos >= mEnd)
        return;

    // calculate new pos
    int64_t org_end = mEnd;
    int64_t org_end_offset = mEndOffset;
    int64_t adj_end = pos;
    int64_t adj_end_offset = mEndOffset + (mEnd - pos);
    int64_t new_start = pos;
    int64_t new_start_offset = mStartOffset + (pos - mStart);

    // crop this clip's end
    mEnd = adj_end;
    mEndOffset = adj_end_offset;
    // and add a new clip start at this clip's end
    if ((newClipId = timeline->AddNewClip(mMediaID, mType, track->mID,
            new_start, new_start_offset, org_end, org_end_offset,
            gid, newClipId, nullptr)) >= 0)
    {
        // need check overlap status and update overlap info on data layer(UI info will update on track update)
        for (auto overlap : timeline->m_Overlaps)
        {
            if (overlap->m_Clip.first == mID || overlap->m_Clip.second == mID)
            {
                if (overlap->mStart >= new_start && overlap->mEnd <= org_end)
                {
                    if (overlap->m_Clip.first == mID) overlap->m_Clip.first = newClipId;
                    if (overlap->m_Clip.second == mID) overlap->m_Clip.second = newClipId;
                }
            }
        }
    }
    timeline->Update();

    if (pActionList)
    {
        imgui_json::value action;
        action["action"] = "CUT_CLIP";
        action["media_type"] = imgui_json::number(mType);
        action["clip_id"] = imgui_json::number(mID);
        action["track_id"] = imgui_json::number(track->mID);
        action["cut_pos"] = imgui_json::number(cut_pos);
        action["org_end"] = imgui_json::number(org_end);
        action["new_clip_id"] = imgui_json::number(newClipId);
        action["new_clip_start"] = imgui_json::number(pos);
        action["group_id"] = imgui_json::number(gid);
        pActionList->push_back(std::move(action));
    }
}

int64_t Clip::Moving(int64_t diff, int mouse_track)
{
    int64_t index = -1;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return index;
    auto track = timeline->FindTrackByClipID(mID);
    if (!track || track->mLocked)
        return index;
    if (IS_DUMMY(mType))
        return index;
    int track_index = timeline->FindTrackIndexByClipID(mID);
    diff = timeline->AlignTime(diff);
    if (diff == 0 && track_index == mouse_track)
        return index;

    ImGuiIO &io = ImGui::GetIO();
    // [shortcut]: left ctrl into single moving status
    bool single = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);

    int64_t length = mEnd - mStart;
    int64_t start = timeline->mStart;
    int64_t end = -1;
    int64_t new_diff = -1;

    int64_t group_start = mStart;
    int64_t group_end = mEnd;
    std::vector<Clip *> moving_clips;
    bMoving = true;
    moving_clips.push_back(this);
    if (!single)
    {
        // check all select clip
        for (auto &clip : timeline->m_Clips)
        {
            if (clip->bSelected && clip->mID != mID)
            {
                auto clip_track = timeline->FindTrackByClipID(clip->mID);
                if (clip_track && clip_track->mLocked)
                    continue;
                if (IS_DUMMY(clip->mType))
                    continue;
                if (clip->mStart < group_start)
                {
                    group_start = clip->mStart;
                }
                if (clip->mEnd > group_end)
                {
                    group_end = clip->mEnd;
                }
                clip->bMoving = true;
                moving_clips.push_back(clip);
            }
        }
    }
    
    // get all clip time march point
    std::vector<int64_t> selected_start_points;
    std::vector<int64_t> selected_end_points;
    std::vector<int64_t> unselected_start_points;
    std::vector<int64_t> unselected_end_points;
    if (timeline->bMovingAttract)
    {
        for (auto clip : timeline->m_Clips)
        {
            // align clip end to timeline frame rate
            int64_t _start = clip->mStart;
            int64_t _end = clip->mEnd;
            if ((single && clip->mID == mID) || (!single && clip->bSelected && clip->bMoving))
            {
                selected_start_points.push_back(_start);
                selected_end_points.push_back(_end);
            }
            else
            {
                unselected_start_points.push_back(_start);
                unselected_end_points.push_back(_end);
            }
        }
    }

    // find all connected point between selected clip and unselected clip
    int64_t attract_docking_gap = timeline->attract_docking_pixels / timeline->msPixelWidthTarget;
    int64_t min_gap = INT64_MAX;
    int64_t connected_point = -1;
    for (auto point_start : selected_start_points)
    {
        for (auto _point_start : unselected_start_points)
        {
            if (abs(point_start - _point_start) < attract_docking_gap)
            {
                if (abs(min_gap) > abs(point_start - _point_start))
                {
                    min_gap = point_start - _point_start;
                    connected_point = _point_start;
                }
            }
        }
        for (auto _point_end : unselected_end_points)
        {
            if (abs(point_start - _point_end) < attract_docking_gap)
            {
                if (abs(min_gap) > abs(point_start - _point_end))
                {
                    min_gap = point_start - _point_end;
                    connected_point = _point_end;
                }
            }
        }
    }
    for (auto point_end : selected_end_points)
    {
        for (auto _point_start : unselected_start_points)
        {
            if (abs(point_end - _point_start) < attract_docking_gap)
            {
                if (abs(min_gap) > abs(point_end - _point_start))
                {
                    min_gap = point_end - _point_start;
                    connected_point = _point_start;
                }
            }
        }
        for (auto _point_end : unselected_end_points)
        {
            if (abs(point_end - _point_end) < attract_docking_gap)
            {
                if (abs(min_gap) > abs(point_end - _point_end))
                {
                    min_gap = point_end - _point_end;
                    connected_point = _point_end;
                }
            }
        }
    }

    if (min_gap != INT64_MAX)
    {
        if (diff < 0 && min_gap > 0)
        {
            // attracting point when clip move left
            // [AAAAAAA]<-[BBBBBB] or [AAAAAAAA[X<-]BBBBBBB]
            timeline->mConnectedPoints = connected_point;
            diff = -min_gap;
        }
        else if (diff > 0 && min_gap < 0)
        {
            // attracting point when clip move right
            // [AAAAAAA]->[BBBBBB] or [AAAAAAAA[->X]BBBBBBB]
            timeline->mConnectedPoints = connected_point;
            diff = -min_gap;
        }
        else if (diff < 0 && min_gap < 0)
        {
            // leaving attracted point when clip move left
            // [AAAAAAAA<-] [BBBBBBBB] or [AAAAAAA[X<-]BBBBBB]
            if (timeline->mConnectedPoints != -1 && abs(diff) < attract_docking_gap / 5)
            {
                diff = -min_gap;
            }
        }
        else if (diff > 0 && min_gap > 0)
        {
            // leaving attracted point when clip move right
            // [AAAAAAAA] [->BBBBBBBB] || [AAAAAAA[->X]BBBBBB]
            if (timeline->mConnectedPoints != -1 && abs(diff) < attract_docking_gap / 5)
            {
                diff = -min_gap;
            }
        }
        // need alignment mStart to accurate connected_point and calculate new_diff
    }
    else
    {
        timeline->mConnectedPoints = -1;
    }

    // get moving tracks
    std::vector<MediaTrack*> tracks;
    if (single)
    {
        auto track = timeline->FindTrackByClipID(mID);
        if (track) tracks.push_back(track);
    }
    else
    {
        //for (auto clip : timeline->m_Clips)
        for (auto clip : moving_clips)
        {
            if (clip->bSelected)
            {
                auto track = timeline->FindTrackByClipID(clip->mID);
                if (track)
                {
                    auto iter = std::find_if(tracks.begin(), tracks.end(), [track](const MediaTrack* _track)
                    {
                        return track->mID == _track->mID;
                    });
                    if (iter == tracks.end())
                    {
                        tracks.push_back(track);
                    }
                }
            }
        }
    }

    // check overlap connected point pre track
    int64_t overlap_max = 0;
    for (auto track : tracks)
    {
        // simulate clip moving
        std::vector<std::pair<int64_t, int64_t>> clips;
        for (auto clip : track->m_Clips)
        {
            bool is_moving = single ? clip->mID == mID : clip->bMoving; //clip->bSelected;
            if (is_moving)
                clips.push_back({clip->mStart + diff ,clip->mEnd + diff});
            else
                clips.push_back({clip->mStart ,clip->mEnd});
        }

        // sort simulate clips by start time
        std::sort(clips.begin(), clips.end(), [](const std::pair<int64_t, int64_t> & a, const std::pair<int64_t, int64_t> & b) {
            return a.first < b.first;
        });

        // calculate overlap after simulate moving
        std::vector<std::pair<int64_t, int64_t>> overlaps;
        for (auto iter = clips.begin(); iter != clips.end(); iter++)
        {
            for (auto next = iter + 1; next != clips.end(); next++)
            {
                if ((*iter).second >= (*next).first)
                {
                    int64_t _start = std::max((*next).first, (*iter).first);
                    int64_t _end = std::min((*iter).second, (*next).second);
                    if (_end > _start)
                    {
                        overlaps.push_back({_start, _end});
                    }
                }
            }
        }

        // find overlap overlaped after simulate moving
        for (auto iter = overlaps.begin(); iter != overlaps.end(); iter++)
        {
            for (auto next = iter + 1; next != overlaps.end(); next++)
            {
                if ((*iter).second > (*next).first)
                {
                    int overlap_length = (*iter).second - (*next).first;
                    if (overlap_max < overlap_length)
                        overlap_max = overlap_length;
                }
            }
        }
    }
    if (overlap_max > 0)
    {
        diff = 0;
    }
    if (group_start + diff >= start && (end == -1 || (end != -1 && group_end + diff <= end)))
    {
        new_diff = diff;
        mStart += new_diff;
        mEnd = mStart + length;
    }
    else
    {
        if (diff < 0 && group_start + diff < start)
        {
            new_diff = start - group_start;
            mStart += new_diff;
            mEnd = mStart + length;
        }
        if (diff > 0 && end != -1 && group_end + diff > end)
        {
            new_diff = end - group_end;
            mStart = end - length;
            mEnd = mStart + length;
        }
    }
    
    auto moving_clip_keypoint = [&](Clip * clip)
    {
#if 0 // TODO::Dicky editing item support moving
        if (timeline->mVidFilterClip && timeline->mVidFilterClip->mID == clip->mID)
        {
            timeline->mVidFilterClip->mStart = clip->mStart;
            timeline->mVidFilterClip->mEnd = clip->mEnd;
        }
        if (timeline->mAudFilterClip && timeline->mAudFilterClip->mID == clip->mID)
        {
            timeline->mAudFilterClip->mStart = clip->mStart;
            timeline->mAudFilterClip->mEnd = clip->mEnd;
        }
#endif
    };

    moving_clip_keypoint(this);

    // check clip is cross track
    if (mouse_track == -2 && track->m_Clips.size() > 1)
    {
        index = timeline->NewTrack("", mType, track->mExpanded, -1, -1, &timeline->mUiActions);
        if (IS_TEXT(mType))
        {
            MediaTrack * newTrack = timeline->m_Tracks[index];
            newTrack->mMttReader = timeline->mMtvReader->NewEmptySubtitleTrack(newTrack->mID);
            newTrack->mMttReader->SetFont(timeline->mFontName);
            newTrack->mMttReader->SetFrameSize(timeline->GetPreviewWidth(), timeline->GetPreviewHeight());
            newTrack->mMttReader->EnableFullSizeOutput(false);
        }
        timeline->MovingClip(mID, track_index, index);
    }
    else if (mouse_track >= 0 && mouse_track != track_index)
    {
        MediaTrack * track = timeline->m_Tracks[mouse_track];
        auto media_type = track->mType;
        //if (mType == media_type)
        if (IS_SAME_TYPE(mType, media_type) && !track->mLocked)
        {
            // check clip is suitable for moving cross track base on overlap status
            bool can_moving = true;
            for (auto overlap : track->m_Overlaps)
            {
                if ((overlap->mStart >= mStart && overlap->mStart <= mEnd) || 
                    (overlap->mEnd >= mStart && overlap->mEnd <= mEnd))
                {
                    can_moving = false;
                    break;
                }
            }
            // clip move into other same type track
            if (can_moving)
            {
                timeline->MovingClip(mID, track_index, mouse_track);
                index = mouse_track;
            }
        }
    }

    if (!single)
    {
        for (auto &clip : moving_clips)
        {
            if (clip->bSelected && clip->mID != mID)
            {
                int64_t clip_length = clip->mEnd - clip->mStart;
                clip->mStart += new_diff;
                clip->mEnd = clip->mStart + clip_length;
                moving_clip_keypoint(clip);
            }
        }
    }
    timeline->Update();
    // clean clip moving flags
    for (auto& clip : moving_clips)
        clip->bMoving = false;
    io.MouseDelta.x = new_diff;
    return index;
}

bool Clip::isLinkedWith(Clip * clip)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return false;
    if (mGroupID != -1 && mGroupID == clip->mGroupID)
        return true;

    return false;
}

int Clip::AddEventTrack()
{
    EventTrack* track = new EventTrack(mID, mHandle);
    if (track)
    {
        mEventTracks.push_back(track);
    }
    return mEventTracks.size() - 1;;
}

bool Clip::AddEvent(int64_t id, int evtTrackIndex, int64_t start, int64_t duration, const BluePrint::Node* node, std::list<imgui_json::value>* pActionList)
{
    if (!node)
        return false;
    return AddEvent(id, evtTrackIndex, start, duration, node->GetTypeID(), node->GetName(), pActionList);
}

bool Clip::AddEvent(int64_t id, int evtTrackIndex, int64_t start, int64_t duration, ID_TYPE nodeTypeId, const std::string& nodeName, std::list<imgui_json::value>* pActionList)
{
    if (!mEventStack || evtTrackIndex >= mEventTracks.size())
        return false;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (id == -1)
        id = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    start = timeline->AlignTime(start);
    auto end = start + duration;
    end = timeline->AlignTime(end);
    auto event = mEventStack->AddNewEvent(id, start, end, evtTrackIndex);
    if (!event)
    {
        auto err_str = mEventStack->GetError();
        return false;
    }
    // Create BP and set node into BP
    auto event_bp = event->GetBp();
    if (event_bp)
    {
        if (event_bp->Blueprint_AppendNode(nodeTypeId))
        {
            mEventTracks[evtTrackIndex]->m_Events.push_back(event->Id());
            auto pTrack = timeline->FindTrackByClipID(mID);
            timeline->RefreshTrackView({ pTrack->mID });
        }
        else
        {
            mEventStack->RemoveEvent(event->Id());
            Logger::Log(Logger::Error) << "FAILED to invoke 'Blueprint_AppendNode()' to add new node of type '" << nodeName << "'!" << std::endl;
            return false;
        }
    }

    if (pActionList)
    {
        imgui_json::value action;
        action["action"] = "ADD_EVENT";
        action["media_type"] = imgui_json::number(mType);
        action["clip_id"] = imgui_json::number(mID);
        action["event_id"] = imgui_json::number(id);
        action["event_z"] = imgui_json::number(evtTrackIndex);
        action["event_start"] = imgui_json::number(start);
        action["event_end"] = imgui_json::number(end);
        action["node_type_id"] = imgui_json::number(nodeTypeId);
        action["node_name"] = imgui_json::string(nodeName);
        pActionList->push_back(std::move(action));
    }
    mEventTracks[evtTrackIndex]->Update();
    return true;
}

bool Clip::DeleteEvent(int64_t evtId, std::list<imgui_json::value>* pActionList)
{
    if (!mEventStack)
        return false;
    auto hEvent = mEventStack->GetEvent(evtId);
    if (!hEvent)
        return false;
    return DeleteEvent(hEvent, pActionList);
}


bool Clip::DeleteEvent(MEC::Event::Holder event, std::list<imgui_json::value>* pActionList)
{
    if (!mEventStack || !event)
        return false;
    int track_index = event->Z();
    if (track_index < 0 || track_index >= mEventTracks.size())
        return false;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return false;
    // remove event from track
    auto track = mEventTracks[track_index];
    if (!track)
        return false;
    for (auto event_iter = track->m_Events.begin(); event_iter != track->m_Events.end();)
    {
        if (*event_iter == event->Id())
        {
            event_iter = track->m_Events.erase(event_iter);
        }
        else
            event_iter++;
    }

    if (pActionList)
    {
        imgui_json::value action;
        action["action"] = "DELETE_EVENT";
        action["media_type"] = imgui_json::number(mType);
        action["clip_id"] = imgui_json::number(mID);
        action["event_id"] = imgui_json::number(event->Id());
        action["event_json"] = event->SaveAsJson();
        pActionList->push_back(std::move(action));
    }

    mEventStack->RemoveEvent(event->Id());
    auto clip_track = timeline->FindTrackByClipID(mID);
    if (clip_track) timeline->RefreshTrackView({clip_track->mID});
    track->Update();
    return true;
}

bool Clip::AppendEvent(MEC::Event::Holder event, void* data)
{
	if (!mEventStack || !event || !data)
        return false;

    BluePrint::Node* node = static_cast<BluePrint::Node*>(data);
    auto event_bp = event->GetBp();
    ID_TYPE newNodeId = 0;
    if (event_bp)
    {
        if (!event_bp->Blueprint_AppendNode(node->GetTypeID(), &newNodeId))
        {
            return false;
        }
    }
    return false;
}

void Clip::SelectEvent(MEC::Event::Holder event, bool appand)
{
    if (!mEventStack || !event)
        return;
    bool selected = true;
    bool is_selected = event->Status() & EVENT_SELECTED;
    if (appand && is_selected)
    {
        selected = false;
    }

    auto event_list = mEventStack->GetEventList();
    for (auto _event : event_list)
    {
        if (_event->Id() != event->Id())
        {
            if (!appand)
            {
                _event->SetStatus(EVENT_SELECTED_BIT, selected ? 0 : 1);
            }
        }
    }
    event->SetStatus(EVENT_SELECTED_BIT, selected ? 1 : 0);
    mEventStack->SetEditingEvent(selected ? event->Id() : -1);
}

void Clip::ChangeStart(int64_t pos)
{
    if (pos == mStart)
        return;
    const int64_t length = Length();
    mStart = pos;
    mEnd = pos+length;
}

void Clip::ChangeStartOffset(int64_t newOffset)
{
    if (newOffset == mStartOffset)
        return;
    assert(newOffset >= 0 && newOffset-mStartOffset < Length());
    mStart += newOffset-mStartOffset;
    mStartOffset = newOffset;
}

void Clip::ChangeEndOffset(int64_t newOffset)
{
    if (newOffset == mEndOffset)
        return;
    assert(newOffset >= 0 && newOffset-mEndOffset < Length());
    mEnd -= newOffset-mEndOffset;
    mEndOffset = newOffset;
}

// clip event editing
MEC::Event::Holder Clip::FindEventByID(int64_t id)
{
    if (!mEventStack)
        return nullptr;
    return mEventStack->GetEvent(id);
}

MEC::Event::Holder Clip::FindSelectedEvent()
{
    if (!mEventStack)
        return nullptr;
    for (auto event : mEventStack->GetEventList())
    {
        if (event->Status() & EVENT_SELECTED)
            return event;
    }
    return nullptr;
}

bool Clip::hasSelectedEvent()
{
    if (!mEventStack)
        return false;
    for (auto event : mEventStack->GetEventList())
    {
        if (event->Status() & EVENT_SELECTED)
            return true;
    }
    return false;
}

void Clip::EventMoving(int64_t event_id, int64_t diff, int64_t mouse, std::list<imgui_json::value>* pActionList)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline) return;
    diff = timeline->AlignTime(diff);
    if (diff == 0) return;
    auto event = FindEventByID(event_id);
    if (!event || !(event->Status() & EVENT_SELECTED)) return;
    auto index = event->Z();
    if (index < 0 || index >= mEventTracks.size()) return;
    auto track = mEventTracks[index];
    int64_t length = event->Length();
    auto new_diff = diff;
    auto prev_event = track->FindPreviousEvent(event->Id());
    auto next_event = track->FindNextEvent(event->Id());

    if (diff < 0)
    {
        // moving backward
        if (prev_event && event->Start() + diff < prev_event->End())
        {
            if (mouse < prev_event->Start())
            {
                int64_t space = 0;
                auto pprev_event = track->FindPreviousEvent(prev_event->Id());
                if (pprev_event)
                    space = prev_event->Start() - pprev_event->End();
                else
                    space = prev_event->Start();
                if (space >= length)
                    new_diff = prev_event->Start() - length - event->Start();
                else
                    new_diff = prev_event->End() - event->Start();
            }
            else
                new_diff = prev_event->End() - event->Start();
        }
    }
    else
    {
        // moving forward
        if (next_event && event->End() + diff > next_event->Start())
        {
            if (mouse > next_event->End())
            {
                int64_t space = 0;
                auto nnext_event = track->FindNextEvent(next_event->Id());
                if (nnext_event)
                    space = nnext_event->Start() - next_event->End();
                else
                    space = Length() - next_event->End();
                if (space >= length)
                    new_diff = next_event->End() - event->Start();
                else
                    new_diff = next_event->Start() - event->End();
            }
            else
                new_diff = next_event->Start() - event->End();
        }
    }
    auto old_start = event->Start();
    auto new_start = old_start + new_diff;
    if (new_start < 0) new_start = 0;
    if (new_start + length > Length())
        new_start = Length() - length;
    event->Move(new_start, index);
    track->Update();

    if (pActionList)
    {
        imgui_json::value action;
        action["action"] = "MOVE_EVENT";
        action["media_type"] = imgui_json::number(mType);
        action["clip_id"] = imgui_json::number(mID);
        action["event_id"] = imgui_json::number(event_id);
        action["event_start_old"] = imgui_json::number(old_start);
        action["event_z_old"] = imgui_json::number(index);
        pActionList->push_back(std::move(action));
    }
}

int64_t Clip::EventCropping(int64_t event_id, int64_t diff, int type, std::list<imgui_json::value>* pActionList)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline) return 0;
    diff = timeline->AlignTime(diff);
    if (diff == 0) return 0;
    auto event = FindEventByID(event_id);
    if (!event || !(event->Status() & EVENT_SELECTED)) return 0;
    auto index = event->Z();
    if (index < 0 || index >= mEventTracks.size()) return 0;
    auto track = mEventTracks[index];
    int64_t new_diff = diff;
    auto prev_event = track->FindPreviousEvent(event->Id());
    auto next_event = track->FindNextEvent(event->Id());
    const auto oldStart = event->Start();
    const auto oldEnd = event->End();

    if (type == 0)
    {
        // cropping start
        if (diff < 0)
        {
            // crop backward
            if (prev_event && event->Start() + diff < prev_event->End())
                new_diff = prev_event->End() - event->Start();
        }
        auto new_start = event->Start() + new_diff;
        if (new_start < 0) new_start = 0;
        if (new_start >= event->End()) new_start = timeline->AlignTimeToPrevFrame(event->End());
        new_diff = new_start - new_diff;
        event->ChangeRange(new_start, event->End());
    }
    else
    {
        // cropping end
        if (diff > 0)
        {
            // crop forward
            if (next_event && event->End() + diff > next_event->Start())
                new_diff = next_event->Start() - event->End();
        }
        auto new_end = event->End() + new_diff;
        if (new_end > End()) new_end = End();
        if (new_end <= event->Start()) new_end = timeline->AlignTimeToNextFrame(event->Start());
        new_diff = new_end - new_diff;
        event->ChangeRange(event->Start(), new_end);
    }
    // TODO::   need update event curve
    track->Update();

    if (pActionList)
    {
        imgui_json::value action;
        action["action"] = "CROP_EVENT";
        action["media_type"] = imgui_json::number(mType);
        action["clip_id"] = imgui_json::number(mID);
        action["event_id"] = imgui_json::number(event_id);
        action["event_start_old"] = imgui_json::number(oldStart);
        action["event_end_old"] = imgui_json::number(oldEnd);
        pActionList->push_back(std::move(action));
    }
    return new_diff;
}

} // namespace MediaTimeline

namespace MediaTimeline
{
// VideoClip Struct Member Functions
VideoClip::~VideoClip()
{
    if (mhSsViewer) mhSsViewer->Release();
}

VideoClip* VideoClip::CreateInstance(TimeLine* pOwner, const std::string& strName, MediaItem* pMediaItem, int64_t i64Start, int64_t i64End, int64_t i64StartOffset, int64_t i64EndOffset)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    if (!pMediaItem)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pMediaItem'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    auto hParser = pMediaItem->mhParser;
    IM_ASSERT(hParser);
    const auto pVidstm = pMediaItem->mhParser->GetBestVideoStream();
    if (!pVidstm)
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'VideoClip::CreateInstance()'! CANNOT find any video stream from '" << hParser->GetUrl() << "'." << std::endl;
        return nullptr;
    }
    auto pNewClip = new VideoClip(pOwner, strName, i64Start, i64End, i64StartOffset, i64EndOffset);
    IM_ASSERT(pNewClip);
    if (pVidstm->isImage)
        pNewClip->mType = MEDIA_SUBTYPE_VIDEO_IMAGE;
    else if (hParser->IsImageSequence())
        pNewClip->mType = MEDIA_SUBTYPE_VIDEO_IMAGE_SEQUENCE;
    if (!pNewClip->UpdateClip(pMediaItem))
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'VideoClip::CreateInstance()'! 'VideoClip::UpdateClip()' failed." << std::endl;
        delete pNewClip;
        return nullptr;
    }
    return pNewClip;
}

VideoClip* VideoClip::CreateInstance(TimeLine* pOwner, MediaItem* pMediaItem, int64_t i64Start)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    if (!pMediaItem)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pMediaItem'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    auto hParser = pMediaItem->mhParser;
    IM_ASSERT(hParser);
    const auto pVidstm = pMediaItem->mhParser->GetBestVideoStream();
    if (!pVidstm)
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'VideoClip::CreateInstance()'! CANNOT find any video stream from '" << hParser->GetUrl() << "'." << std::endl;
        return nullptr;
    }
    const auto tClipRange = pOwner->AlignClipRange(pVidstm->isImage ? std::pair<int64_t, int64_t>(i64Start, 10000) : std::pair<int64_t, int64_t>(i64Start, (int64_t)(pVidstm->duration*1000)));
    const std::string strClipName = pVidstm->isImage ? pMediaItem->mName : (pMediaItem->mName+":Video");
    return CreateInstance(pOwner, strClipName, pMediaItem, tClipRange.first, tClipRange.first+tClipRange.second, 0, 0);
}

VideoClip* VideoClip::CreateDummyInstance(TimeLine* pOwner, const std::string& strName, int64_t i64Start, int64_t i64End)
{
    auto pNewClip = new VideoClip(pOwner);
    pNewClip->mName = strName;
    pNewClip->mType |= MEDIA_DUMMY;
    pNewClip->mStart = i64Start;
    pNewClip->mEnd = i64End;
    return pNewClip;
}

imgui_json::value VideoClip::SaveAsJson()
{
    imgui_json::value j = Clip::SaveAsJson();
    // save video clip info
    j["width"] = imgui_json::number(mWidth);
    j["height"] = imgui_json::number(mHeight);

    if (mhDataLayerClip)
    {
        j["TransformFilter"] = mhDataLayerClip->GetTransformFilter()->SaveAsJson();
        const auto hVFilter = mhDataLayerClip->GetFilter();
        if (hVFilter)
            j["VideoFilter"] = hVFilter->SaveAsJson();
    }
    mClipJson = j;
    return std::move(j);
}

VideoClip* VideoClip::CreateInstanceFromJson(const imgui_json::value& j, TimeLine* pOwner)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    auto pNewClip = new VideoClip(pOwner);
    if (!pNewClip->LoadFromJson(j))
    {
        delete pNewClip;
        return nullptr;
    }
    if (!IS_VIDEO(pNewClip->mType))
    {
        Logger::Log(Logger::Error) << "FAILED to create 'VideoClip' instance from json! Invalid clip type (" << pNewClip->mType << "). Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        delete pNewClip;
        return nullptr;
    }
    if (IS_DUMMY(pNewClip->mType))
        return pNewClip;
    auto pMediaItem = pOwner->FindMediaItemByID(pNewClip->mMediaID);
    if (!pMediaItem)
    {
        Logger::Log(Logger::Error) << "FAILED to create 'VideoClip' instance from json! Invalid 'MediaItem' id (" << pNewClip->mMediaID << "). Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        delete pNewClip;
        return nullptr;
    }
    if (!pNewClip->UpdateClip(pMediaItem))
    {
        delete pNewClip;
        return nullptr;
    }
    if (pNewClip->mhDataLayerClip)
        pNewClip->SyncStateToDataLayer();
    return pNewClip;
}

bool VideoClip::UpdateClip(MediaItem* pMediaItem)
{
    auto pVidstm = pMediaItem->mhParser->GetBestVideoStream();
    if (!pVidstm)
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'VideoClip::UpdateClip()'! CANNOT find any video stream from '" << mPath << "'." << std::endl;
        return false;
    }
    const bool bIsImage = IS_IMAGE(mType);
    const bool bIsImgseq = IS_IMAGESEQ(mType);
    MediaCore::Snapshot::Viewer::Holder hSsViewer;
    if (bIsImage)
    {
        if (!pVidstm->isImage)
        {
            Logger::Log(Logger::Error) << "WRONG media type! Try to use a NON-IMAGE source to create an IMAGE 'Videoclip'. Url is '" << mPath << "'." << std::endl;
            return false;
        }
    }
    else
    {
        if (pVidstm->isImage)
        {
            Logger::Log(Logger::Error) << "WRONG media type! Try to use an IMAGE source to create a NON-IMAGE 'VideoClip'. Url is '" << mPath << "'." << std::endl;
            return false;
        }
        TimeLine* pOwner = (TimeLine*)mHandle;
        auto hSsGen = pOwner->GetSnapshotGenerator(pMediaItem->mID);
        if (hSsGen)
            hSsViewer = hSsGen->CreateViewer();
        else
        {
            Logger::Log(Logger::WARN) << "FAILED to retrieve 'Snapshot::Generator' for 'VideoClip' built on '" << mPath << "'! Then no 'Snapshot::Viewer' is available." << std::endl;
            return false;
        }
    }
    if (!bIsImage && !bIsImgseq && StartOffset()+Length() > (int64_t)(pVidstm->duration*1000))
        Logger::Log(Logger::WARN) << "The underline range of 'VideoClip' exceeds the duration of the source video stream! start-offset(" << StartOffset() << ")+length("
                << Length() << ")=" << StartOffset()+Length() << " > video-stream-duration(" << (int64_t)(pVidstm->duration*1000) << ")." << std::endl;

    mpMediaItem = pMediaItem;
    mMediaID = pMediaItem->mID;
    mMediaParser = pMediaItem->mhParser;
    mhOverview = pMediaItem->mMediaOverview;
    mhSsViewer = hSsViewer;
    mPath = mMediaParser->GetUrl();
    mWidth = pVidstm->width;
    mHeight = pVidstm->height;
    return true;
}

bool VideoClip::ReloadSource(MediaItem* pMediaItem)
{
    return UpdateClip(pMediaItem);
}

void VideoClip::ConfigViewWindow(int64_t wndDur, float pixPerMs)
{
    Clip::ConfigViewWindow(wndDur, pixPerMs);

    if (mTrackHeight > 0)
        CalcDisplayParams();
}

void VideoClip::SetTrackHeight(int trackHeight)
{
    Clip::SetTrackHeight(trackHeight);

    if (mViewWndDur > 0 && mPixPerMs > 0)
        CalcDisplayParams();
}

void VideoClip::SetViewWindowStart(int64_t millisec)
{
    mClipViewStartPos = StartOffset();
    if (millisec > Start())
        mClipViewStartPos += millisec-Start();
    if (!IS_DUMMY(mType) && !IS_IMAGE(mType))
    {
        if (!mhSsViewer->GetSnapshots((double)mClipViewStartPos/1000, mSnapImages))
            throw std::runtime_error(mhSsViewer->GetError());
        auto txmgr = ((TimeLine*)mHandle)->mTxMgr;
        mhSsViewer->UpdateSnapshotTexture(mSnapImages, txmgr, VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME);
    }
}

void VideoClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect, bool updated)
{
    if (IS_DUMMY(mType))
    {
        int trackHeight = rightBottom.y - leftTop.y;
        int snapHeight = trackHeight;
        int snapWidth = trackHeight * 4 / 3;
        ImVec2 imgLeftTop = leftTop;
        float snapDispWidth = snapWidth;
        while (imgLeftTop.x < rightBottom.x)
        {
            ImVec2 uvMin{0, 0}, uvMax{1, 1};
            if (snapDispWidth < snapWidth)
                uvMin.x = 1 - snapDispWidth / snapWidth;
            if (imgLeftTop.x + snapDispWidth > rightBottom.x)
            {
                uvMax.x = 1 - (imgLeftTop.x + snapDispWidth - rightBottom.x) / snapWidth;
                snapDispWidth = rightBottom.x - imgLeftTop.x;
            }
            auto frame_min = imgLeftTop;
            auto frame_max = ImVec2(imgLeftTop.x + snapDispWidth, rightBottom.y);
            drawList->AddRectFilled(frame_min, frame_max, COL_ERROR_MEDIA, 8, ImDrawFlags_RoundCornersAll);
            drawList->AddRect(frame_min, frame_max, IM_COL32(0, 0, 0, 255));
            drawList->AddLine(frame_min, frame_max, IM_COL32_BLACK);
            drawList->AddLine(frame_min + ImVec2(0, trackHeight), frame_max - ImVec2(0, trackHeight), IM_COL32_BLACK);
            std::string error_mark = std::string(ICON_ERROR_FRAME);
            ImGui::SetWindowFontScale(1.2);
            auto error_mark_size = ImGui::CalcTextSize(error_mark.c_str());
            float _xoft = (frame_max.x - frame_min.x - error_mark_size.x) / 2;
            float _yoft = (frame_max.y - frame_min.y - error_mark_size.y) / 2;
            if (_xoft >= 0 && _yoft >= 0)
                drawList->AddText(frame_min + ImVec2(_xoft, _yoft), IM_COL32_WHITE, ICON_ERROR_FRAME);
            ImGui::SetWindowFontScale(1.0);
            imgLeftTop.x += snapDispWidth;
            snapDispWidth = snapWidth;
        }
    }
    else if (IS_IMAGE(mType))
    {
        if (!mhImageTx)
        {
            TimeLine* pOwner = (TimeLine*)mHandle;
            std::vector<ImGui::ImMat> aOvwSsAry;
            mhOverview->GetSnapshots(aOvwSsAry);
            if (!aOvwSsAry.empty() && !aOvwSsAry[0].empty())
            {
                mhImageTx = pOwner->mTxMgr->GetGridTextureFromPool(VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME);
                mhImageTx->RenderMatToTexture(aOvwSsAry[0]);
            }
        }
        auto pImgTid = mhImageTx ? mhImageTx->TextureID() : nullptr;
        if (pImgTid && mSnapWidth > 0 && mSnapHeight > 0)
        {
            const auto roiRect = mhImageTx->GetDisplayRoi();
            const auto& roiSize = roiRect.size;
            ImVec2 imgLeftTop = leftTop;
            while (imgLeftTop.x < rightBottom.x)
            {
                auto fSsW = mSnapWidth;
                const auto fSsH = mSnapHeight;
                ImVec2 uvMin{0, 0}, uvMax{1, 1};
                if (imgLeftTop.x+fSsW > rightBottom.x)
                {
                    uvMax.x = 1 - (imgLeftTop.x+fSsW-rightBottom.x) / fSsW;
                    fSsW = rightBottom.x - imgLeftTop.x;
                }
                MatUtils::Point2f uvMin2(uvMin.x, uvMin.y), uvMax2(uvMax.x, uvMax.y);
                uvMin2 = roiRect.leftTop+roiSize*uvMin2; uvMax2 = roiRect.leftTop+roiSize*uvMax2;
                drawList->AddImage(pImgTid, imgLeftTop, {imgLeftTop.x + fSsW, rightBottom.y}, MatUtils::ToImVec2(uvMin2), MatUtils::ToImVec2(uvMax2));
                imgLeftTop.x += fSsW;
            }
        }
    }
    else if (!mSnapImages.empty())
    {
        ImVec2 snapLeftTop = leftTop;
        float snapDispWidth;
        MediaCore::Snapshot::GetLogger()->Log(Logger::VERBOSE) << "[1]>>>>> Begin display snapshot" << std::endl;
        for (int i = 0; i < mSnapImages.size(); i++)
        {
            auto& img = mSnapImages[i];
            if (!img.hDispData) continue;
            ImVec2 uvMin(0, 0), uvMax(1, 1);
            float snapDispWidth = img.ssTimestampMs >= mClipViewStartPos ? mSnapWidth : mSnapWidth - (mClipViewStartPos - img.ssTimestampMs) * mPixPerMs;
            if (img.ssTimestampMs < mClipViewStartPos)
            {
                snapDispWidth = mSnapWidth - (mClipViewStartPos - img.ssTimestampMs) * mPixPerMs;
                uvMin.x = 1 - snapDispWidth / mSnapWidth;
            }
            if (snapDispWidth <= 0) continue;
            if (snapLeftTop.x+snapDispWidth >= rightBottom.x)
            {
                snapDispWidth = rightBottom.x - snapLeftTop.x;
                uvMax.x = snapDispWidth / mSnapWidth;
            }

            auto hTx = img.hDispData->mTextureReady ? img.hDispData->mhTx : nullptr;
            auto tid = hTx ? hTx->TextureID() : nullptr;
            if (tid)
            {
                const auto roiRect = hTx->GetDisplayRoi();
                const auto& roiSize = roiRect.size;
                MatUtils::Point2f uvMin2(uvMin.x, uvMin.y), uvMax2(uvMax.x, uvMax.y);
                uvMin2 = roiRect.leftTop+roiSize*uvMin2; uvMax2 = roiRect.leftTop+roiSize*uvMax2;
                drawList->AddImage(tid, snapLeftTop, {snapLeftTop.x + snapDispWidth, rightBottom.y}, MatUtils::ToImVec2(uvMin2), MatUtils::ToImVec2(uvMax2));
            }
            else
            {
                drawList->AddRectFilled(snapLeftTop, {snapLeftTop.x + snapDispWidth, rightBottom.y}, IM_COL32_BLACK);
                auto center_pos = snapLeftTop + ImVec2(snapDispWidth, mSnapHeight) / 2;
                ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
                //ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
                //ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
                //ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
                ImGui::SpinnerBarsRotateFade("Running", 3, 6, 2, ImColor(128, 128, 128), 7.6f, 6);
                drawList->AddRect(snapLeftTop, {snapLeftTop.x + snapDispWidth, rightBottom.y}, COL_FRAME_RECT);
            }
            snapLeftTop.x += snapDispWidth;
            if (snapLeftTop.x >= rightBottom.x)
                break;
        }
        MediaCore::Snapshot::GetLogger()->Log(Logger::VERBOSE) << "[1]<<<<< End display snapshot" << std::endl;
    }
    else
    {
        Logger::Log(Logger::WARN) << "ABNORMAL state for 'VideoClip' based on '" << mPath << "': No snapshots to show." << std::endl;
    }
}

void VideoClip::CalcDisplayParams()
{
    const MediaCore::VideoStream* video_stream = mMediaParser->GetBestVideoStream();
    mSnapHeight = mTrackHeight;
    MediaCore::Ratio displayAspectRatio = {
        (int32_t)(video_stream->width * video_stream->sampleAspectRatio.num), (int32_t)(video_stream->height * video_stream->sampleAspectRatio.den) };
    mSnapWidth = (float)mTrackHeight * displayAspectRatio.num / displayAspectRatio.den;
}

void VideoClip::SetDataLayer(MediaCore::VideoClip::Holder hVClip, bool bSyncStateToDataLayer)
{
    mhDataLayerClip = hVClip;
    if (bSyncStateToDataLayer)
        SyncStateToDataLayer();
    else
        SyncStateFromDataLayer();
}

void VideoClip::SyncStateToDataLayer()
{
    // sync 'VideoFilter' from clip json to data-layer
    MediaCore::VideoFilter::Holder hVFilter;
    BluePrint::BluePrintCallbackFunctions tBpCallbacks;
    tBpCallbacks.BluePrintOnChanged = TimeLine::OnVideoEventStackFilterBpChanged;
    std::string strAttrName;
    strAttrName = "VideoFilter";
    if (mClipJson.contains(strAttrName) && mClipJson[strAttrName].is_object())
    {
        const auto& jnFilterJson = mClipJson[strAttrName];
        std::string strFilterName;
        strAttrName = "name";
        if (jnFilterJson.contains(strAttrName) && jnFilterJson[strAttrName].is_string())
            strFilterName = jnFilterJson[strAttrName].get<imgui_json::string>();
        if (strFilterName == "EventStackFilter")
            hVFilter = MEC::VideoEventStackFilter::LoadFromJson(jnFilterJson, tBpCallbacks);
        else if (!strFilterName.empty())
            Logger::Log(Logger::WARN) << "Unrecognized 'VideoFilter' type '" << strFilterName << "'! Ignore the 'VideoFilter' json for 'VideoClip' on '" << mPath << "'." << std::endl;
    }
    if (!hVFilter)
        hVFilter = MEC::VideoEventStackFilter::CreateInstance(tBpCallbacks);
    if (hVFilter->GetFilterName() == "EventStackFilter")
    {
        MEC::VideoEventStackFilter* pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(hVFilter.get());
        pEsf->SetTimelineHandle(mHandle);
        mEventStack = static_cast<MEC::EventStack*>(pEsf);
    }
    mhDataLayerClip->SetFilter(hVFilter);
    // sync 'TransformFilter' from clip json to data-layer
    strAttrName = "TransformFilter";
    if (mClipJson.contains(strAttrName) && mClipJson[strAttrName].is_object())
    {
        const auto& jnTransformFilterJson = mClipJson[strAttrName];
        if (!mhDataLayerClip->GetTransformFilter()->LoadFromJson(jnTransformFilterJson))
        {
            Logger::Log(Logger::WARN) << "FAILED to sync 'TransformFilter' json to 'VideoClip' on '" << mPath << "'. Transform filter json is:" << std::endl;
            Logger::Log(Logger::WARN) << jnTransformFilterJson.dump() << std::endl << std::endl;
        }
    }
}

void VideoClip::SyncStateFromDataLayer()
{
    // sync 'VideoFilter' from data-layer to clip json
    mEventTracks.clear();
    auto hVFilter = mhDataLayerClip->GetFilter();
    if (!hVFilter)
    {
        BluePrint::BluePrintCallbackFunctions tBpCallbacks;
        tBpCallbacks.BluePrintOnChanged = TimeLine::OnVideoEventStackFilterBpChanged;
        hVFilter = MEC::VideoEventStackFilter::CreateInstance(tBpCallbacks);
        MEC::VideoEventStackFilter* pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(hVFilter.get());
        pEsf->SetTimelineHandle(mHandle);
        mEventStack = static_cast<MEC::EventStack*>(pEsf);
        mhDataLayerClip->SetFilter(hVFilter);
    }

    const auto strFilterName = hVFilter->GetFilterName();
    if (strFilterName == "EventStackFilter")
    {
        MEC::VideoEventStackFilter* pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(hVFilter.get());
        mEventStack = static_cast<MEC::EventStack*>(pEsf);
        auto aEventList = mEventStack->GetEventList();
        for (auto& hEvt : aEventList)
        {
            const auto z = hEvt->Z();
            while (z >= mEventTracks.size())
                AddEventTrack();
            mEventTracks[z]->m_Events.push_back(hEvt->Id());
        }
    }
    else
        Logger::Log(Logger::WARN) << "Unrecognized 'VideoFilter' type '" << strFilterName << "'! Ignore syncing state from this instance for 'VideoClip' on '" << mPath << "'." << std::endl;
    mClipJson["VideoFilter"] = hVFilter->SaveAsJson();

    // sync 'TransformFilter' from data-layer to clip json
    mClipJson["TransformFilter"] = mhDataLayerClip->GetTransformFilter()->SaveAsJson();
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// AudioClip Struct Member Functions
AudioClip::~AudioClip()
{
    if (mWaveformTexture) { ImGui::ImDestroyTexture(mWaveformTexture); mWaveformTexture = nullptr; }
}

AudioClip* AudioClip::CreateInstance(TimeLine* pOwner, const std::string& strName, MediaItem* pMediaItem, int64_t i64Start, int64_t i64End, int64_t i64StartOffset, int64_t i64EndOffset)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    if (!pMediaItem)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pMediaItem'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    auto hParser = pMediaItem->mhParser;
    IM_ASSERT(hParser);
    const auto pAudstm = pMediaItem->mhParser->GetBestAudioStream();
    if (!pAudstm)
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'AudioClip::CreateInstance()'! CANNOT find any video stream from '" << hParser->GetUrl() << "'." << std::endl;
        return nullptr;
    }
    auto pNewClip = new AudioClip(pOwner, strName, i64Start, i64End, i64StartOffset, i64EndOffset);
    IM_ASSERT(pNewClip);
    if (!pNewClip->UpdateClip(pMediaItem))
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'AudioClip::CreateInstance()'! 'AudioClip::UpdateClip()' failed." << std::endl;
        delete pNewClip;
        return nullptr;
    }
    return pNewClip;
}

AudioClip* AudioClip::CreateInstance(TimeLine* pOwner, MediaItem* pMediaItem, int64_t i64Start)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    if (!pMediaItem)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pMediaItem'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    auto hParser = pMediaItem->mhParser;
    IM_ASSERT(hParser);
    const auto pAudstm = pMediaItem->mhParser->GetBestAudioStream();
    if (!pAudstm)
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'AudioClip::CreateInstance()'! CANNOT find any video stream from '" << hParser->GetUrl() << "'." << std::endl;
        return nullptr;
    }
    const auto tClipRange = pOwner->AlignClipRange(std::pair<int64_t, int64_t>(i64Start, (int64_t)(pAudstm->duration*1000)));
    const std::string strClipName = pMediaItem->mName+":Audio";
    return CreateInstance(pOwner, strClipName, pMediaItem, tClipRange.first, tClipRange.first+tClipRange.second, 0, 0);
}

AudioClip* AudioClip::CreateDummyInstance(TimeLine* pOwner, const std::string& strName, int64_t i64Start, int64_t i64End)
{
    auto pNewClip = new AudioClip(pOwner);
    pNewClip->mName = strName;
    pNewClip->mType |= MEDIA_DUMMY;
    pNewClip->mStart = i64Start;
    pNewClip->mEnd = i64End;
    return pNewClip;
}

imgui_json::value AudioClip::SaveAsJson()
{
    imgui_json::value j = Clip::SaveAsJson();
    // save audio clip info
    j["Channels"] = imgui_json::number(mAudioChannels);
    j["SampleRate"] = imgui_json::number(mAudioSampleRate);
    mClipJson = j;
    return std::move(j);
}

AudioClip* AudioClip::CreateInstanceFromJson(const imgui_json::value& j, TimeLine* pOwner)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    auto pNewClip = new AudioClip(pOwner);
    if (!pNewClip->LoadFromJson(j))
    {
        delete pNewClip;
        return nullptr;
    }
    if (!IS_AUDIO(pNewClip->mType))
    {
        Logger::Log(Logger::Error) << "FAILED to create 'AudioClip' instance from json! Invalid clip type (" << pNewClip->mType << "). Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        delete pNewClip;
        return nullptr;
    }
    if (IS_DUMMY(pNewClip->mType))
        return pNewClip;
    auto pMediaItem = pOwner->FindMediaItemByID(pNewClip->mMediaID);
    if (!pMediaItem)
    {
        Logger::Log(Logger::Error) << "FAILED to create 'AudioClip' instance from json! Invalid 'MediaItem' id (" << pNewClip->mMediaID << "). Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        delete pNewClip;
        return nullptr;
    }
    if (!pNewClip->UpdateClip(pMediaItem))
    {
        delete pNewClip;
        return nullptr;
    }
    if (pNewClip->mhDataLayerClip)
        pNewClip->SyncStateToDataLayer();
    return pNewClip;
}

bool AudioClip::UpdateClip(MediaItem* pMediaItem)
{
    auto pAudstm = pMediaItem->mhParser->GetBestAudioStream();
    if (!pAudstm)
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'AudioClip::UpdateClip()'! CANNOT find any audio stream from '" << mPath << "'." << std::endl;
        return false;
    }
    mpMediaItem = pMediaItem;
    mMediaID = pMediaItem->mID;
    mMediaParser = pMediaItem->mhParser;
    mhOverview = pMediaItem->mMediaOverview;
    mPath = mMediaParser->GetUrl();
    mWaveform = mhOverview->GetWaveform();
    mAudioChannels = pAudstm->channels;
    mAudioChannels = pAudstm->sampleRate;
    return true;
}

bool AudioClip::ReloadSource(MediaItem* pMediaItem)
{
    return UpdateClip(pMediaItem);
}

void AudioClip::SetDataLayer(MediaCore::AudioClip::Holder hAClip, bool bSyncStateToDataLayer)
{
    mhDataLayerClip = hAClip;
    if (bSyncStateToDataLayer)
        SyncStateToDataLayer();
    else
        SyncStateFromDataLayer();
}

void AudioClip::SyncStateToDataLayer()
{
    // sync 'AudioFilter' from clip json to data-layer
    MediaCore::AudioFilter::Holder hAFilter;
    BluePrint::BluePrintCallbackFunctions tBpCallbacks;
    tBpCallbacks.BluePrintOnChanged = TimeLine::OnAudioEventStackFilterBpChanged;
    std::string strAttrName;
    strAttrName = "AudioFilter";
    if (mClipJson.contains(strAttrName) && mClipJson[strAttrName].is_object())
    {
        const auto& jnFilterJson = mClipJson[strAttrName];
        std::string strFilterName;
        strAttrName = "name";
        if (jnFilterJson.contains(strAttrName) && jnFilterJson[strAttrName].is_string())
            strFilterName = jnFilterJson[strAttrName].get<imgui_json::string>();
        if (strFilterName == "EventStackFilter")
            hAFilter = MEC::AudioEventStackFilter::LoadFromJson(jnFilterJson, tBpCallbacks);
        else if (!strFilterName.empty())
            Logger::Log(Logger::WARN) << "Unrecognized 'AudioFilter' type '" << strFilterName << "'! Ignore the 'AudioFilter' json for 'AudioClip' on '" << mPath << "'." << std::endl;
    }
    if (!hAFilter)
        hAFilter = MEC::AudioEventStackFilter::CreateInstance(tBpCallbacks);
    if (hAFilter->GetFilterName() == "EventStackFilter")
    {
        MEC::AudioEventStackFilter* pEsf = dynamic_cast<MEC::AudioEventStackFilter*>(hAFilter.get());
        pEsf->SetTimelineHandle(mHandle);
        mEventStack = static_cast<MEC::EventStack*>(pEsf);
    }
    mhDataLayerClip->SetFilter(hAFilter);
}

void AudioClip::SyncStateFromDataLayer()
{
    // sync 'AudioFilter' from data-layer to clip json
    mEventTracks.clear();
    auto hAFilter = mhDataLayerClip->GetFilter();
    if (!hAFilter)
    {
        BluePrint::BluePrintCallbackFunctions tBpCallbacks;
        tBpCallbacks.BluePrintOnChanged = TimeLine::OnAudioEventStackFilterBpChanged;
        hAFilter = MEC::AudioEventStackFilter::CreateInstance(tBpCallbacks);
        mhDataLayerClip->SetFilter(hAFilter);
    }

    const auto strFilterName = hAFilter->GetFilterName();
    if (strFilterName == "EventStackFilter")
    {
        MEC::AudioEventStackFilter* pEsf = dynamic_cast<MEC::AudioEventStackFilter*>(hAFilter.get());
        mEventStack = static_cast<MEC::EventStack*>(pEsf);
        auto aEventList = mEventStack->GetEventList();
        for (auto& hEvt : aEventList)
        {
            const auto z = hEvt->Z();
            while (z >= mEventTracks.size())
                AddEventTrack();
            mEventTracks[z]->m_Events.push_back(hEvt->Id());
        }
    }
    else
        Logger::Log(Logger::WARN) << "Unrecognized 'AudioFilter' type '" << strFilterName << "'! Ignore syncing state from this instance for 'AudioClip' on '" << mPath << "'." << std::endl;
    mClipJson["AudioFilter"] = hAFilter->SaveAsJson();
}

void AudioClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect, bool updated)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    if (IS_DUMMY(mType))
    {
        int trackHeight = rightBottom.y - leftTop.y;
        drawList->AddRectFilled(leftTop, rightBottom, COL_ERROR_MEDIA, 8, ImDrawFlags_RoundCornersAll);
        drawList->AddLine(leftTop, rightBottom, IM_COL32_BLACK);
        drawList->AddLine(leftTop + ImVec2(0, trackHeight), rightBottom - ImVec2(0, trackHeight), IM_COL32_BLACK);
        ImGui::SetWindowFontScale(1.2);
        std::string error_mark = std::string(ICON_ERROR_AUDIO);
        auto error_mark_size = ImGui::CalcTextSize(error_mark.c_str());
        float _xoft = (rightBottom.x - leftTop.x - error_mark_size.x) / 2;
        float _yoft = (rightBottom.y - leftTop.y - error_mark_size.y) / 2;
        drawList->AddText(leftTop + ImVec2(_xoft, _yoft), IM_COL32_WHITE, ICON_ERROR_AUDIO);
        ImGui::SetWindowFontScale(1.0);
        return;
    }
    if (timeline->media_items.size() <= 0 || !mWaveform)
        return;

    ImVec2 draw_size = rightBottom - leftTop;

    if (mWaveform->pcm.size() > 0)
    {
        std::string id_string = "##Waveform@" + std::to_string(mID);
        drawList->AddRectFilled(leftTop, rightBottom, IM_COL32(16, 16, 16, 255));
        drawList->AddRect(leftTop, rightBottom, IM_COL32_BLACK);
        float wave_range = fmax(fabs(mWaveform->minSample), fabs(mWaveform->maxSample));
        int sampleSize = mWaveform->pcm[0].size();
        int64_t start_time = std::max(Start(), timeline->firstTime);
        int64_t end_time = std::min(End(), timeline->lastTime);
        int start_offset = (int)((double)StartOffset() / 1000.f / mWaveform->aggregateDuration);
        if (Start() < timeline->firstTime)
            start_offset = (int)((double)(timeline->firstTime - Start() + StartOffset()) / 1000.f / mWaveform->aggregateDuration);
        start_offset = std::max(start_offset, 0);
        int window_length = (int)((double)(end_time - start_time) / 1000.f / mWaveform->aggregateDuration);
        window_length = std::min(window_length, sampleSize);
        ImVec2 customViewStart = ImVec2((start_time - timeline->firstTime) * timeline->msPixelWidthTarget + clipRect.Min.x, clipRect.Min.y);
        ImVec2 customViewEnd = ImVec2((end_time - timeline->firstTime)  * timeline->msPixelWidthTarget + clipRect.Min.x, clipRect.Max.y);
        auto window_size = customViewEnd - customViewStart;
        if (window_size.x > 0 && mWaveform->pcm[0].size() > 0)
        {
            int sample_stride = window_length / window_size.x;
            if (sample_stride <= 0) sample_stride = 1;
            int min_zoom = ImMax(window_length >> 13, 16);
            int zoom = ImMin(sample_stride, min_zoom);
            drawList->PushClipRect(leftTop, rightBottom, true);
#if PLOT_IMPLOT
            start_offset = start_offset / zoom * zoom; // align start_offset
            ImGui::SetCursorScreenPos(customViewStart);
            ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, {0, 0});
            ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
            ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
            if (ImPlot::BeginPlot(id_string.c_str(), window_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
            {
                std::string plot_id = id_string + "_line";
                ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, window_length / zoom, -wave_range / 2, wave_range / 2, ImGuiCond_Always);
                ImPlot::PlotLine(plot_id.c_str(), &mWaveform->pcm[0][start_offset], window_length / zoom, 1.0, 0.0, 0, 0, sizeof(float) * zoom);
                ImPlot::EndPlot();
            }
            ImPlot::PopStyleColor();
            ImPlot::PopStyleVar(2);
#elif PLOT_TEXTURE
            if (!mWaveformTexture || updated || !mWaveform->parseDone)
            {
                ImGui::ImMat plot_mat;
                start_offset = start_offset / sample_stride * sample_stride; // align start_offset
                ImGui::ImMat plot_frame_max, plot_frame_min;
                auto filled = waveFrameResample(&mWaveform->pcm[0][0], sample_stride, draw_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.4f, 0.4f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.3f, 0.8f, 0.5f));
                if (filled)
                {
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_max.data, plot_frame_max.w, 0, -wave_range, wave_range, draw_size, sizeof(float), filled);
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, draw_size, sizeof(float), filled);
                }
                else
                {
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, draw_size, sizeof(float), filled);
                }
                ImGui::PopStyleColor(2);
                ImMatToTexture(plot_mat, mWaveformTexture);
                ImGui::UpdateData();
            }
            if (mWaveformTexture) drawList->AddImage(mWaveformTexture, customViewStart, customViewStart + window_size, ImVec2(0, 0), ImVec2(1, 1));
#else
            ImGui::SetCursorScreenPos(customViewStart);
            if (ImGui::BeginChild(id_string.c_str(), window_size, false, ImGuiWindowFlags_NoScrollbar))
            {
                start_offset = start_offset / sample_stride * sample_stride; // align start_offset
                ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.4f, 0.4f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.3f, 0.8f, 0.5f));
                std::string plot_max_id = id_string + "_line_max";
                std::string plot_min_id = id_string + "_line_min";
                ImGui::ImMat plot_frame_max, plot_frame_min;
                waveFrameResample(&mWaveform->pcm[0][0], sample_stride, draw_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
                ImGui::SetCursorScreenPos(customViewStart);
                ImGui::PlotLinesEx(plot_max_id.c_str(), (float *)plot_frame_max.data, plot_frame_max.w, 0, nullptr, -wave_range, wave_range, draw_size, sizeof(float), false, true);
                ImGui::SetCursorScreenPos(customViewStart);
                ImGui::PlotLinesEx(plot_min_id.c_str(), (float *)plot_frame_min.data, plot_frame_min.w, 0, nullptr, -wave_range, wave_range, draw_size, sizeof(float), false, true);
                ImGui::PopStyleColor(3);
            }
            ImGui::EndChild();
#endif
            drawList->PopClipRect();
        }        
    }
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// TextClip Struct Member Functions
TextClip* TextClip::CreateInstance(TimeLine* pOwner, const std::string& strText, int64_t i64Start, int64_t i64Length)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    if (i64Length <= 0)
        i64Length = 5000;
    const auto tClipRange = pOwner->AlignClipRange({i64Start, i64Length});
    auto pNewClip = new TextClip(pOwner, strText, tClipRange.first, tClipRange.first+tClipRange.second);
    IM_ASSERT(pNewClip);
    return pNewClip;
}

TextClip::~TextClip()
{
}

void TextClip::SetClipDefault(const MediaCore::SubtitleStyle & style)
{
    mTrackStyle = true;
    mFontScaleX = style.ScaleX();
    mFontScaleY = style.ScaleY();
    mFontItalic = style.Italic() > 0;
    mFontBold = style.Bold() > 0;
    mFontAngleX = style.Angle();
    mFontAngleY = style.Angle();
    mFontAngleZ = style.Angle();
    mFontOutlineWidth = style.OutlineWidth();
    mFontSpacing = style.Spacing();
    mFontAlignment = style.Alignment();
    mFontUnderLine = style.UnderLine();
    mFontStrikeOut = style.StrikeOut();
    mFontName = style.Font();
    mFontOffsetH = style.OffsetHScale();
    mFontOffsetV = style.OffsetVScale();
    mFontShadowDepth = fabs(style.ShadowDepth());
    mFontPrimaryColor = style.PrimaryColor().ToImVec4();
    mFontOutlineColor = style.OutlineColor().ToImVec4();
    mFontBackColor = style.BackColor().ToImVec4();
    // pos value need load later 
    mFontPosX = - FLT_MAX;
    mFontPosY = - FLT_MAX;

    if (!mTrackStyle)
        SyncClipAttributes();
}

void TextClip::SetClipDefault(const TextClip* clip)
{
    mTrackStyle = clip->mTrackStyle;
    mFontScaleX = clip->mFontScaleX;
    mFontScaleY = clip->mFontScaleY;
    mFontItalic = clip->mFontItalic;
    mFontBold = clip->mFontBold;
    mFontAngleX = clip->mFontAngleX;
    mFontAngleY = clip->mFontAngleY;
    mFontAngleZ = clip->mFontAngleZ;
    mFontOutlineWidth = clip->mFontOutlineWidth;
    mFontSpacing = clip->mFontSpacing;
    mFontAlignment = clip->mFontAlignment;
    mFontUnderLine = clip->mFontUnderLine;
    mFontStrikeOut = clip->mFontStrikeOut;
    mFontName = clip->mFontName;
    mFontOffsetH = clip->mFontOffsetH;
    mFontOffsetV = clip->mFontOffsetV;
    mFontShadowDepth = clip->mFontShadowDepth;
    mFontPrimaryColor = clip->mFontPrimaryColor;
    mFontOutlineColor = clip->mFontOutlineColor;
    mFontBackColor = clip->mFontBackColor;
    // pos value need load later 
    mFontPosX = - FLT_MAX;
    mFontPosY = - FLT_MAX;

    if (!mTrackStyle)
        SyncClipAttributes();
}

void TextClip::SyncClipAttributes()
{
    if (mhDataLayerClip)
    {
        mhDataLayerClip->SetFont(mFontName);
        mhDataLayerClip->SetOffsetH(mFontOffsetH);
        mhDataLayerClip->SetOffsetV(mFontOffsetV);
        mhDataLayerClip->SetScaleX(mFontScaleX);
        mhDataLayerClip->SetScaleY(mFontScaleY);
        mhDataLayerClip->SetSpacing(mFontSpacing);
        mhDataLayerClip->SetRotationX(mFontAngleX);
        mhDataLayerClip->SetRotationY(mFontAngleY);
        mhDataLayerClip->SetRotationZ(mFontAngleZ);
        mhDataLayerClip->SetBorderWidth(mFontOutlineWidth);
        mhDataLayerClip->SetBold(mFontBold);
        mhDataLayerClip->SetItalic(mFontItalic);
        mhDataLayerClip->SetUnderLine(mFontUnderLine);
        mhDataLayerClip->SetStrikeOut(mFontStrikeOut);
        mhDataLayerClip->SetAlignment(mFontAlignment);
        mhDataLayerClip->SetShadowDepth(mFontShadowDepth);
        mhDataLayerClip->SetPrimaryColor(mFontPrimaryColor);
        mhDataLayerClip->SetOutlineColor(mFontOutlineColor);
        mhDataLayerClip->SetBackColor(mFontBackColor);
    }
}

void TextClip::EnableUsingTrackStyle(bool enable)
{
    if (mTrackStyle != enable)
    {
        mTrackStyle = enable;
        if (mhDataLayerClip)
        {
            if (!enable)
                SyncClipAttributes();
            mhDataLayerClip->EnableUsingTrackStyle(enable);
        }
    }
}

void TextClip::CreateDataLayer(MediaTrack* pTrack)
{
    IM_ASSERT(pTrack && pTrack->mMttReader);
    mhDataLayerClip = pTrack->mMttReader->NewClip(Start(), Length());
    mTrack = pTrack;
    // mMediaID = pTrack->mID;
    // mName = track->mName;
    mhDataLayerClip->SetText(mText);
    mhDataLayerClip->EnableUsingTrackStyle(mTrackStyle);
    if (!mIsInited)
    {
        mFontName = mhDataLayerClip->Font();
        mFontOffsetH = mhDataLayerClip->OffsetHScale();
        mFontOffsetV = mhDataLayerClip->OffsetVScale();
        mFontScaleX = mhDataLayerClip->ScaleX();
        mFontScaleY = mhDataLayerClip->ScaleY();
        mFontSpacing = mhDataLayerClip->Spacing();
        mFontAngleX = mhDataLayerClip->RotationX();
        mFontAngleY = mhDataLayerClip->RotationY();
        mFontAngleZ = mhDataLayerClip->RotationZ();
        mFontOutlineWidth = mhDataLayerClip->BorderWidth();
        mFontBold = mhDataLayerClip->Bold();
        mFontItalic = mhDataLayerClip->Italic();
        mFontUnderLine = mhDataLayerClip->UnderLine();
        mFontStrikeOut = mhDataLayerClip->StrikeOut();
        mFontAlignment = mhDataLayerClip->Alignment();
        mFontShadowDepth = mhDataLayerClip->ShadowDepth();
        mFontPrimaryColor = mhDataLayerClip->PrimaryColor().ToImVec4();
        mFontOutlineColor = mhDataLayerClip->OutlineColor().ToImVec4();
        mFontBackColor = mhDataLayerClip->BackColor().ToImVec4();
        mIsInited = true;
    }
    else if (!mTrackStyle)
    {
        SyncClipAttributes();
    }
    mhDataLayerClip->SetKeyPoints(mAttributeKeyPoints);
}

bool TextClip::ReloadSource(MediaItem* pMediaItem)
{
    Logger::Log(Logger::Error) << "INVALID CODE BRANCH! TextClip does NOT SUPPORT reload source." << std::endl;
    return false;
}

int64_t TextClip::Moving(int64_t diff, int mouse_track)
{
    auto ret = Clip::Moving(diff, mouse_track);
    MediaTrack * track = (MediaTrack*)mTrack;
    if (track && track->mMttReader)
        track->mMttReader->ChangeClipTime(mhDataLayerClip, Start(), Length());
    return ret;
}

int64_t TextClip::Cropping(int64_t diff, int type)
{
    auto ret = Clip::Cropping(diff, type);
    MediaTrack * track = (MediaTrack*)mTrack;
    if (track && track->mMttReader)
        track->mMttReader->ChangeClipTime(mhDataLayerClip, Start(), Length());
    return ret;
}

void TextClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect, bool updated)
{
    drawList->AddRectFilled(leftTop, rightBottom, IM_COL32(16, 16, 16, 255));
    drawList->PushClipRect(leftTop, rightBottom, true);
    ImGui::SetWindowFontScale(0.75);
    ImGui::PushStyleVar(ImGuiStyleVar_TextInternationalize, 0);
    drawList->AddText(leftTop + ImVec2(2, 2), IM_COL32_WHITE, mText.c_str());
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
    drawList->PopClipRect();
    drawList->AddRect(leftTop, rightBottom, IM_COL32_BLACK);
}

void TextClip::DrawTooltips()
{
    if (ImGui::BeginTooltip())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_TextInternationalize, 0);
        ImGui::Text("%s", mText.c_str());
        ImGui::PopStyleVar();
        ImGui::EndTooltip();
    }
}

TextClip* TextClip::CreateInstanceFromJson(const imgui_json::value& j, TimeLine* pOwner)
{
    if (!pOwner)
    {
        Logger::Log(Logger::Error) << "INVALID argument 'pOwner'! Can NOT be NULL." << std::endl;
        return nullptr;
    }
    auto pNewClip = new TextClip(pOwner);
    if (!pNewClip->LoadFromJson(j))
    {
        delete pNewClip;
        return nullptr;
    }
    if (!IS_TEXT(pNewClip->mType))
    {
        Logger::Log(Logger::Error) << "FAILED to create 'TextClip' instance from json! Invalid clip type (" << pNewClip->mType << "). Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        delete pNewClip;
        return nullptr;
    }
    std::string strAttrName;
    strAttrName = "Text";
    if (j.contains(strAttrName) && j[strAttrName].is_string())
        pNewClip->mText = j[strAttrName].get<imgui_json::string>();
    else
    {
        Logger::Log(Logger::Error) << "FAILED to perform 'TextClip::CreateInstanceFromJson()'! No attribute '" << strAttrName << "' can be found. Clip json is:" << std::endl;
        Logger::Log(Logger::Error) << j.dump() << std::endl << std::endl;
        delete pNewClip;
        return nullptr;
    }
    strAttrName = "TrackStyle";
    if (j.contains(strAttrName) && j[strAttrName].is_boolean())
        pNewClip->mTrackStyle = j[strAttrName].get<imgui_json::boolean>();
    strAttrName = "FontName";
    if (j.contains(strAttrName) && j[strAttrName].is_string())
        pNewClip->mFontName = j[strAttrName].get<imgui_json::string>();
    strAttrName = "ScaleLink";
    if (j.contains(strAttrName) && j[strAttrName].is_boolean())
        pNewClip->mScaleSettingLink = j[strAttrName].get<imgui_json::boolean>();
    strAttrName = "ScaleX";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontScaleX = j[strAttrName].get<imgui_json::number>();
    strAttrName = "ScaleY";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontScaleY = j[strAttrName].get<imgui_json::number>();
    strAttrName = "Spacing";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontSpacing = j[strAttrName].get<imgui_json::number>();
    strAttrName = "AngleX";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontAngleX = j[strAttrName].get<imgui_json::number>();
    strAttrName = "AngleY";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontAngleY = j[strAttrName].get<imgui_json::number>();
    strAttrName = "AngleZ";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontAngleZ = j[strAttrName].get<imgui_json::number>();
    strAttrName = "OutlineWidth";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontOutlineWidth = j[strAttrName].get<imgui_json::number>();
    strAttrName = "Alignment";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontAlignment = j[strAttrName].get<imgui_json::number>();
    strAttrName = "Bold";
    if (j.contains(strAttrName) && j[strAttrName].is_boolean())
        pNewClip->mFontBold = j[strAttrName].get<imgui_json::boolean>();
    strAttrName = "Italic";
    if (j.contains(strAttrName) && j[strAttrName].is_boolean())
        pNewClip->mFontItalic = j[strAttrName].get<imgui_json::boolean>();
    strAttrName = "UnderLine";
    if (j.contains(strAttrName) && j[strAttrName].is_boolean())
        pNewClip->mFontUnderLine = j[strAttrName].get<imgui_json::boolean>();
    strAttrName = "StrikeOut";
    if (j.contains(strAttrName) && j[strAttrName].is_boolean())
        pNewClip->mFontStrikeOut = j[strAttrName].get<imgui_json::boolean>();
    strAttrName = "OffsetX";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontOffsetH = j[strAttrName].get<imgui_json::number>();
    strAttrName = "OffsetY";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontOffsetV = j[strAttrName].get<imgui_json::number>();
    strAttrName = "ShadowDepth";
    if (j.contains(strAttrName) && j[strAttrName].is_number())
        pNewClip->mFontShadowDepth = j[strAttrName].get<imgui_json::number>();
    strAttrName = "PrimaryColor";
    if (j.contains(strAttrName) && j[strAttrName].is_vec4())
        pNewClip->mFontPrimaryColor = j[strAttrName].get<imgui_json::vec4>();
    strAttrName = "OutlineColor";
    if (j.contains(strAttrName) && j[strAttrName].is_vec4())
        pNewClip->mFontOutlineColor = j[strAttrName].get<imgui_json::vec4>();
    strAttrName = "BackColor";
    if (j.contains(strAttrName) && j[strAttrName].is_vec4())
        pNewClip->mFontBackColor = j[strAttrName].get<imgui_json::vec4>();

    // load attribute curve
    strAttrName = "Atrributes";
    if (j.contains(strAttrName) && j[strAttrName].is_object())
        pNewClip->mAttributeKeyPoints.Load(j[strAttrName]);

    pNewClip->mIsInited = true;
    return pNewClip;
}

imgui_json::value TextClip::SaveAsJson()
{
    auto j = Clip::SaveAsJson();

    j["Text"] = mText;
    j["TrackStyle"] = imgui_json::boolean(mTrackStyle);
    j["FontName"] = imgui_json::string(mFontName);
    j["ScaleLink"] = imgui_json::boolean(mScaleSettingLink);
    j["ScaleX"] = imgui_json::number(mFontScaleX);
    j["ScaleY"] = imgui_json::number(mFontScaleY);
    j["Spacing"] = imgui_json::number(mFontSpacing);
    j["AngleX"] = imgui_json::number(mFontAngleX);
    j["AngleY"] = imgui_json::number(mFontAngleY);
    j["AngleZ"] = imgui_json::number(mFontAngleZ);
    j["OutlineWidth"] = imgui_json::number(mFontOutlineWidth);
    j["Alignment"] = imgui_json::number(mFontAlignment);
    j["Bold"] = imgui_json::boolean(mFontBold);
    j["Italic"] = imgui_json::boolean(mFontItalic);
    j["UnderLine"] = imgui_json::boolean(mFontUnderLine);
    j["StrikeOut"] = imgui_json::boolean(mFontStrikeOut);
    j["OffsetX"] = imgui_json::number(mFontOffsetH);
    j["OffsetY"] = imgui_json::number(mFontOffsetV);
    j["ShadowDepth"] = imgui_json::number(mFontShadowDepth);
    j["PrimaryColor"] = imgui_json::vec4(mFontPrimaryColor);
    j["OutlineColor"] = imgui_json::vec4(mFontOutlineColor);
    j["BackColor"] = imgui_json::vec4(mFontBackColor);

    imgui_json::value jnAttributes;
    mAttributeKeyPoints.Save(jnAttributes);
    j["Attributes"] = jnAttributes;

    mClipJson = j;
    return std::move(j);
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// BluePrintVideoTransition class
BluePrintVideoTransition::BluePrintVideoTransition(void * handle)
    : mHandle(handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    imgui_json::value transition_BP; 
    mBp = new BluePrint::BluePrintUI();
    mBp->Initialize();
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    mBp->SetCallbacks(callbacks, this);
    mBp->File_New_Transition(transition_BP, "VideoTransition", "Video");
}

BluePrintVideoTransition::~BluePrintVideoTransition()
{
    if (mBp)
    {
        mBp->Finalize();
        delete mBp;
    }
}

int BluePrintVideoTransition::OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (!handle)
        return BluePrint::BP_CBR_Unknown;
    BluePrintVideoTransition * transition = (BluePrintVideoTransition *)handle;
    if (!transition) return ret;
    TimeLine * timeline = (TimeLine *)transition->mHandle;
    if (name.compare("VideoTransition") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED ||
            type == BluePrint::BP_CB_NODE_APPEND ||
            type == BluePrint::BP_CB_NODE_INSERT)
        {
            // need update
            if (timeline) timeline->RefreshPreview();
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            // need update
            if (timeline) timeline->RefreshPreview();
        }
    }
    if (timeline) timeline->mIsBluePrintChanged = true;
    return ret;
}

MediaCore::VideoTransition::Holder BluePrintVideoTransition::Clone()
{
    BluePrintVideoTransition* bpTrans = new BluePrintVideoTransition(mHandle);
    auto bpJson = mBp->m_Document->Serialize();
    bpTrans->SetBluePrintFromJson(bpJson);
    bpTrans->SetKeyPoint(mKeyPoints);
    return MediaCore::VideoTransition::Holder(bpTrans);
}

imgui_json::value BluePrintVideoTransition::SaveAsJson() const
{
    imgui_json::value j;
    j["BluePrint"] = mBp->m_Document->Serialize();
    // TODO: KeyPointEditor::Save() CANNOT be invoked here because it needs to be changed as 'const' member function!
    // imgui_json::value jnCurve;
    // mKeyPoints.Save(jnCurve);
    // j["KeyFrameCurve"] = jnCurve;
    return std::move(j);
}

ImGui::ImMat BluePrintVideoTransition::MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur)
{
    std::lock_guard<std::mutex> lk(mBpLock);
    if (mBp && mBp->Blueprint_IsExecutable())
    {
        // setup bp input curve
        for (int i = 0; i < mKeyPoints.GetCurveCount(); i++)
        {
            auto name = mKeyPoints.GetCurveName(i);
            auto value = mKeyPoints.GetValue(i, pos - mOverlap->Start());
            mBp->Blueprint_SetTransition(name, value);
        }
        ImGui::ImMat inMat1(vmat1), inMat2(vmat2);
        ImGui::ImMat outMat;
        mBp->Blueprint_RunTransition(inMat1, inMat2, outMat, pos - mOverlap->Start(), dur);
        return outMat;
    }
    return vmat1;
}

void BluePrintVideoTransition::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    // Logger::Log(Logger::DEBUG) << "Create bp transition from json " << bpJson.dump() << std::endl;
    mBp->File_New_Transition(bpJson, "VideoTransition", "Video");
    if (!mBp->Blueprint_IsValid())
    {
        mBp->Finalize();
        return;
    }
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// BluePrintAudioTransition class
BluePrintAudioTransition::BluePrintAudioTransition(void * handle)
    : mHandle(handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    imgui_json::value transition_BP; 
    mBp = new BluePrint::BluePrintUI();
    mBp->Initialize();
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    mBp->SetCallbacks(callbacks, this);
    mBp->File_New_Transition(transition_BP, "AudioTransition", "Audio");
}

BluePrintAudioTransition::~BluePrintAudioTransition()
{
    if (mBp)
    {
        mBp->Finalize();
        delete mBp;
    }
}

int BluePrintAudioTransition::OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (!handle)
        return BluePrint::BP_CBR_Unknown;
    BluePrintAudioTransition * transition = (BluePrintAudioTransition *)handle;
    if (!transition) return ret;
    TimeLine * timeline = (TimeLine *)transition->mHandle;
    if (name.compare("AudioTransition") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED ||
            type == BluePrint::BP_CB_NODE_APPEND ||
            type == BluePrint::BP_CB_NODE_INSERT)
        {
            // need update
            if (timeline) timeline->RefreshPreview();
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            // need update
            //if (timeline) timeline->UpdatePreview();
        }
    }
    if (timeline) timeline->mIsBluePrintChanged = true;
    return ret;
}

ImGui::ImMat BluePrintAudioTransition::MixTwoAudioMats(const ImGui::ImMat& amat1, const ImGui::ImMat& amat2, int64_t pos)
{
    std::lock_guard<std::mutex> lk(mBpLock);
    if (mBp && mBp->Blueprint_IsExecutable())
    {
        // setup bp input curve
        for (int i = 0; i < mKeyPoints.GetCurveCount(); i++)
        {
            auto name = mKeyPoints.GetCurveName(i);
            auto value = mKeyPoints.GetValue(i, pos - mOverlap->Start());
            mBp->Blueprint_SetTransition(name, value);
        }
        ImGui::ImMat inMat1(amat1), inMat2(amat2);
        ImGui::ImMat outMat;
        mBp->Blueprint_RunTransition(inMat1, inMat2, outMat, pos - mOverlap->Start(), mOverlap->End() - mOverlap->Start());
        return outMat;
    }
    return amat1;
}

void BluePrintAudioTransition::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    // Logger::Log(Logger::DEBUG) << "Create bp transition from json " << bpJson.dump() << std::endl;
    mBp->File_New_Transition(bpJson, "AudioTransition", "Audio");
    if (!mBp->Blueprint_IsValid())
    {
        mBp->Finalize();
        return;
    }
}

} // namespace MediaTimeline

namespace MediaTimeline
{
Clip * BaseEditingClip::GetClip()
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline) return nullptr;
    return timeline->FindClipByID(mID);
}

void BaseEditingClip::UpdateCurrent(bool forward, int64_t currentTime)
{
    if (!forward)
    {
        if (currentTime < firstTime + visibleTime / 2)
        {
            firstTime = currentTime - visibleTime / 2;
        }
        else if (currentTime > firstTime + visibleTime)
        {
            firstTime = currentTime - visibleTime;
        }
    }
    else
    {
        if (mEnd - mStart - currentTime < visibleTime / 2)
        {
            firstTime = mEnd - mStart - visibleTime;
        }
        else if (currentTime > firstTime + visibleTime / 2)
        {
            firstTime = currentTime - visibleTime / 2;
        }
        else if (currentTime < firstTime)
        {
            firstTime = currentTime;
        }
    }
    if (firstTime < 0) firstTime = 0;
}
} // namespace MediaTimeline

namespace MediaTimeline
{
EditingVideoClip::EditingVideoClip(VideoClip* vidclip)
    : BaseEditingClip(vidclip->mID, vidclip->mMediaID, vidclip->mType, vidclip->Start(), vidclip->End(), vidclip->StartOffset(), vidclip->EndOffset(), vidclip->mHandle)
{
    TimeLine * timeline = (TimeLine *)vidclip->mHandle;
    mDuration = mEnd-mStart;
    mWidth = vidclip->mWidth;
    mHeight = vidclip->mHeight;
    firstTime = vidclip->firstTime;
    lastTime = vidclip->lastTime;
    visibleTime = vidclip->visibleTime;
    msPixelWidthTarget = vidclip->msPixelWidthTarget;
    if (mDuration < 0)
        throw std::invalid_argument("Clip duration is negative!");
    
    if (IS_IMAGE(vidclip->mType))
    {
        mhImgTx = vidclip->GetImageTexture();
    }
    else
    {
        mSsGen = MediaCore::Snapshot::Generator::CreateInstance();
        MediaItem* mi = timeline->FindMediaItemByID(vidclip->mMediaID);
        if (mi && mi->mMediaOverview)
            mSsGen->SetOverview(mi->mMediaOverview);
        if (!mSsGen)
        {
            Logger::Log(Logger::Error) << "Create Editing Video Clip FAILED!" << std::endl;
            return;
        }
        if (timeline) mSsGen->EnableHwAccel(timeline->mHardwareCodec);
        if (!mSsGen->Open(vidclip->mhSsViewer->GetMediaParser(), timeline->mhMediaSettings->VideoOutFrameRate()))
        {
            Logger::Log(Logger::Error) << mSsGen->GetError() << std::endl;
            return;
        }

        mSsGen->SetCacheFactor(1);
        RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
        if (timeline->mTxMgr->GetTexturePoolAttributes(EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME, tTxPoolAttrs))
        {
            mSsGen->SetSnapshotSize(tTxPoolAttrs.tTxSize.x, tTxPoolAttrs.tTxSize.y);
        }
        else
        {
            float snapshot_scale = mHeight > 0 ? 50.f / (float)mHeight : 0.05;
            mSsGen->SetSnapshotResizeFactor(snapshot_scale, snapshot_scale);
        }
        mSsViewer = mSsGen->CreateViewer((double)mStartOffset / 1000);
    }

    // create ManagedTexture instances
    mhTransformOutputTx = timeline->mTxMgr->GetTextureFromPool(PREVIEW_TEXTURE_POOL_NAME);
    mhFilterInputTx = timeline->mTxMgr->GetTextureFromPool(ARBITRARY_SIZE_TEXTURE_POOL_NAME);
    mhFilterOutputTx = timeline->mTxMgr->GetTextureFromPool(PREVIEW_TEXTURE_POOL_NAME);

    auto hClip = vidclip->GetDataLayer();
    IM_ASSERT(hClip);
    auto hFilter = hClip->GetFilter();      
    IM_ASSERT(hFilter);
    mhVideoFilter = hFilter;
    auto filterName = hFilter->GetFilterName();
    if (filterName == "EventStackFilter")
    {
        auto pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(mhVideoFilter.get());
        auto editingEvent = pEsf->GetEditingEvent();
        if (editingEvent)
        {
            mFilterBp = editingEvent->GetBp();
            auto filterJson = mFilterBp->m_Document->Serialize();
            mFilterKp = editingEvent->GetKeyPoint();
        }
    }
    mhTransformFilter = hClip->GetTransformFilter();
    m_pTransFilterUiCtrl = new MEC::VideoTransformFilterUiCtrl(mhTransformFilter);
}

EditingVideoClip::~EditingVideoClip()
{
    if (m_pTransFilterUiCtrl)
    {
        delete m_pTransFilterUiCtrl;
        m_pTransFilterUiCtrl = nullptr;
    }
    if (mSsViewer) mSsViewer->Release();
    mSsViewer = nullptr;
    mSsGen = nullptr;
    mhVideoFilter = nullptr;
    mhTransformFilter = nullptr;
    mFilterBp = nullptr;
    mFilterKp = nullptr;
    // if (mImgTexture) { ImGui::ImDestroyTexture(mImgTexture); mImgTexture = nullptr; }
}

void EditingVideoClip::UpdateClipRange(Clip* clip)
{
    if (mStart != clip->Start())
        mStart = clip->Start();
    if (mEnd != clip->End())
        mEnd = clip->End();
    if (mStartOffset != clip->StartOffset() || mEndOffset != clip->EndOffset())
    {
        mStartOffset = clip->StartOffset();
        mEndOffset = clip->StartOffset();
        mDuration = mEnd - mStart;
    }
}

void EditingVideoClip::Save()
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    VideoClip * clip = (VideoClip *)timeline->FindClipByID(mID);
    if (!clip || !IS_VIDEO(clip->mType))
        return;
    clip->firstTime = firstTime;
    clip->lastTime = lastTime;
    clip->visibleTime = visibleTime;
    clip->msPixelWidthTarget = msPixelWidthTarget;
    // TODO::Dicky save clip event track?
    timeline->RefreshPreview();
}

bool EditingVideoClip::UpdatePreviewTexture(bool blocking)
{
    if (!mHandle)
        return false;
    TimeLine* pTimeLine = (TimeLine*)mHandle;
    bool bTxUpdated = pTimeLine->UpdatePreviewTexture(blocking);
    const auto& aCurrFrames = pTimeLine->maCurrFrames;
    if (bTxUpdated || !mhFilterInputTx->IsValid())
    {
        auto iter = std::find_if(aCurrFrames.begin(), aCurrFrames.end(), [this] (const auto& cf) {
            return cf.clipId == mID && cf.phase == MediaCore::CorrelativeFrame::PHASE_SOURCE_FRAME;
        });
        if (iter != aCurrFrames.end() && !iter->frame.empty())
            mhFilterInputTx->RenderMatToTexture(iter->frame);
    }
    if (bTxUpdated || !mhFilterOutputTx->IsValid())
    {
        auto iter = std::find_if(aCurrFrames.begin(), aCurrFrames.end(), [this] (const auto& cf) {
            return cf.clipId == mID && cf.phase == MediaCore::CorrelativeFrame::PHASE_AFTER_FILTER;
        });
        if (iter != aCurrFrames.end() && !iter->frame.empty())
        {
            mFilterOutputMat = iter->frame;
            mhFilterOutputTx->RenderMatToTexture(iter->frame);
        }
    }
    if (bTxUpdated || !mhTransformOutputTx->IsValid())
    {
        auto iter = std::find_if(aCurrFrames.begin(), aCurrFrames.end(), [this] (const auto& cf) {
            return cf.clipId == mID && cf.phase == MediaCore::CorrelativeFrame::PHASE_AFTER_TRANSFORM;
        });
        if (iter != aCurrFrames.end() && !iter->frame.empty())
            mhTransformOutputTx->RenderMatToTexture(iter->frame);
    }
    return bTxUpdated;
}

void EditingVideoClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated)
{
    if (mhImgTx)
    {
        const auto tImgTid = mhImgTx->TextureID();
        const auto roiRect = mhImgTx->GetDisplayRoi();
        const auto& roiSize = roiRect.size;
        if (roiSize.x > 0 && roiSize.y > 0)
        {
            int trackHeight = rightBottom.y - leftTop.y;
            int snapHeight = trackHeight;
            int snapWidth = trackHeight * roiSize.x / roiSize.y;
            ImVec2 imgLeftTop = leftTop;
            float snapDispWidth = snapWidth;
            while (imgLeftTop.x < rightBottom.x)
            {
                ImVec2 uvMin{0, 0}, uvMax{1, 1};
                if (snapDispWidth < snapWidth)
                    uvMin.x = 1 - snapDispWidth / snapWidth;
                if (imgLeftTop.x + snapDispWidth > rightBottom.x)
                {
                    uvMax.x = 1 - (imgLeftTop.x + snapDispWidth - rightBottom.x) / snapWidth;
                    snapDispWidth = rightBottom.x - imgLeftTop.x;
                }
                MatUtils::Point2f uvMin2(uvMin.x, uvMin.y), uvMax2(uvMax.x, uvMax.y);
                uvMin2 = roiRect.leftTop+roiSize*uvMin2; uvMax2 = roiRect.leftTop+roiSize*uvMax2;
                drawList->AddImage(tImgTid, imgLeftTop, {imgLeftTop.x + snapDispWidth, rightBottom.y}, MatUtils::ToImVec2(uvMin2), MatUtils::ToImVec2(uvMax2));
                imgLeftTop.x += snapDispWidth;
                snapDispWidth = snapWidth;
            }
            // if media type is image, Image texture is already generate, but mSnapSize and mViewWndSize is empty
            if (mSnapSize.x == 0 || mSnapSize.y == 0)
            {
                mSnapSize.x = snapWidth;
                mSnapSize.y = snapHeight;
            }
            if (mViewWndSize.x == 0 || mViewWndSize.y == 0)
            {
                mViewWndSize = ImVec2(rightBottom.x - leftTop.x, rightBottom.y - leftTop.y);
            }
        }
    }
    else
    {
        ImVec2 viewWndSize = { rightBottom.x - leftTop.x, rightBottom.y - leftTop.y };
        if (mViewWndSize.x != viewWndSize.x || mViewWndSize.y != viewWndSize.y)
        {
            mViewWndSize = viewWndSize;
            if (mViewWndSize.x == 0 || mViewWndSize.y == 0)
                return;
            auto vidStream = mSsViewer->GetMediaParser()->GetBestVideoStream();
            if (vidStream->width == 0 || vidStream->height == 0)
            {
                Logger::Log(Logger::Error) << "Snapshot video size is INVALID! Width or height is ZERO." << std::endl;
                return;
            }
            mSnapSize.y = viewWndSize.y;
            mSnapSize.x = mSnapSize.y * vidStream->width / vidStream->height;
        }

        std::vector<MediaCore::Snapshot::Image> snapImages;
        if (!mSsViewer->GetSnapshots((double)(mStartOffset + firstTime) / 1000, snapImages))
        {
            Logger::Log(Logger::Error) << mSsViewer->GetError() << std::endl;
            return;
        }
        auto txmgr = ((TimeLine*)mHandle)->mTxMgr;
        mSsViewer->UpdateSnapshotTexture(snapImages, txmgr, EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME);

        ImVec2 imgLeftTop = leftTop;
        for (int i = 0; i < snapImages.size(); i++)
        {
            auto& img = snapImages[i];
            ImVec2 uvMin{0, 0}, uvMax{1, 1};
            float snapDispWidth = img.ssTimestampMs >= mStartOffset + firstTime ? mSnapSize.x : mSnapSize.x - (mStartOffset + firstTime - img.ssTimestampMs) * msPixelWidthTarget;
            if (img.ssTimestampMs < mStartOffset + firstTime)
            {
                snapDispWidth = mSnapSize.x - (mStartOffset + firstTime - img.ssTimestampMs) * msPixelWidthTarget;
                uvMin.x = 1 - snapDispWidth / mSnapSize.x;
            }
            if (snapDispWidth <= 0)
                continue;
            if (imgLeftTop.x + snapDispWidth >= rightBottom.x)
            {
                snapDispWidth = rightBottom.x - imgLeftTop.x;
                uvMax.x = snapDispWidth / mSnapSize.x;
            }

            auto hTx = img.hDispData->mTextureReady ? img.hDispData->mhTx : nullptr;
            auto tid = hTx ? hTx->TextureID() : nullptr;
            if (tid)
            {
                const auto roiRect = hTx->GetDisplayRoi();
                const auto& roiSize = roiRect.size;
                MatUtils::Point2f uvMin2(uvMin.x, uvMin.y), uvMax2(uvMax.x, uvMax.y);
                uvMin2 = roiRect.leftTop+roiSize*uvMin2; uvMax2 = roiRect.leftTop+roiSize*uvMax2;
                drawList->AddImage(tid, imgLeftTop, {imgLeftTop.x + snapDispWidth, rightBottom.y}, MatUtils::ToImVec2(uvMin2), MatUtils::ToImVec2(uvMax2));
            }
            else
            {
                drawList->AddRectFilled(imgLeftTop, {imgLeftTop.x + snapDispWidth, rightBottom.y}, IM_COL32_BLACK);
                auto center_pos = imgLeftTop + mSnapSize / 2;
                ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
                ImGui::SpinnerBarsRotateFade("Running", 3, 6, 2, ImColor(128, 128, 128), 7.6f, 6);
                drawList->AddRect(imgLeftTop, {imgLeftTop.x + snapDispWidth, rightBottom.y}, COL_FRAME_RECT);
            }

            imgLeftTop.x += snapDispWidth;
            if (imgLeftTop.x >= rightBottom.x)
                break;
        }
    }
}

void EditingVideoClip::CalcDisplayParams(int64_t viewWndDur)
{
    if (!mViewWndSize.x || !mSnapSize.x)
        return;
    //if (visibleTime == viewWndDur)
    //    return;
    visibleTime = viewWndDur;
    if (!IS_IMAGE(mType))
    {
        double snapWndSize = (double)viewWndDur / 1000;
        double snapCntInView = (double)mViewWndSize.x / mSnapSize.x;
        mSsGen->ConfigSnapWindow(snapWndSize, snapCntInView);
    }
}

void EditingVideoClip::SelectEditingMask(MEC::Event::Holder hEvent, int64_t nodeId, int maskIndex, ImGui::MaskCreator::Holder hMaskCreator)
{
    mhMaskCreator = hMaskCreator;
    mMaskEventId = hEvent->Id();
    mMaskNodeId = nodeId;
    mMaskIndex = maskIndex;
    mMaskEventStart = hEvent->Start();
    mMaskEventEnd = hEvent->End();
}

void EditingVideoClip::UnselectEditingMask()
{
    mhMaskCreator = nullptr;
    mMaskEventId = mMaskNodeId = -1;
    mMaskIndex = -1;
    mMaskEventStart = mMaskEventEnd = 0;
}
} // namespace MediaTimeline

namespace MediaTimeline
{
EditingAudioClip::EditingAudioClip(AudioClip* audclip)
    : BaseEditingClip(audclip->mID, audclip->mMediaID, audclip->mType, audclip->Start(), audclip->End(), audclip->StartOffset(), audclip->EndOffset(), audclip->mHandle)
{
    TimeLine * timeline = (TimeLine *)audclip->mHandle;
    mDuration = mEnd-mStart;
    mAudioChannels = audclip->mAudioChannels;
    mAudioSampleRate = audclip->mAudioSampleRate;
    mWaveform = audclip->mWaveform;
    firstTime = audclip->firstTime;
    lastTime = audclip->lastTime;
    visibleTime = audclip->visibleTime;
    msPixelWidthTarget = audclip->msPixelWidthTarget;
    if (mDuration < 0)
        throw std::invalid_argument("Clip duration is negative!");
    auto hClip = timeline->mMtaReader->GetClipById(audclip->mID);
    IM_ASSERT(hClip);
    auto hFilter = hClip->GetFilter();      
    IM_ASSERT(hFilter);
    mhAudioFilter = hFilter;
    const auto strFilterName = hFilter->GetFilterName();
    if (strFilterName == "EventStackFilter")
    {
        auto pEsf = dynamic_cast<MEC::AudioEventStackFilter*>(hFilter.get());
        auto editingEvent = pEsf->GetEditingEvent();
        if (editingEvent)
        {
            mFilterBp = editingEvent->GetBp();
            auto filterJson = mFilterBp->m_Document->Serialize();
            mFilterKp = editingEvent->GetKeyPoint();
        }
    }
}

EditingAudioClip::~EditingAudioClip()
{
    for (auto texture : mWaveformTextures) ImGui::ImDestroyTexture(texture);
    mWaveformTextures.clear();
    mhAudioFilter = nullptr;
    mFilterBp = nullptr;
    mFilterKp = nullptr;
}

void EditingAudioClip::UpdateClipRange(Clip* clip)
{
    if (mStart != clip->Start())
        mStart = clip->Start();
    if (mEnd != clip->End())
        mEnd = clip->End();
    if (mStartOffset != clip->StartOffset() || mEndOffset != clip->EndOffset())
    {
        mStartOffset = clip->StartOffset();
        mEndOffset = clip->StartOffset();
        mDuration = mEnd - mStart;
    }
}

void EditingAudioClip::Save()
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    auto clip = timeline->FindClipByID(mID);
    if (!clip)
        return;
    clip->firstTime = firstTime;
    clip->lastTime = lastTime;
    clip->visibleTime = visibleTime;
    clip->msPixelWidthTarget = msPixelWidthTarget;
}

void EditingAudioClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    auto clip = timeline->FindClipByID(mID);
    if (!clip)
        return;
    AudioClip * aclip = (AudioClip *)clip;
    if (!aclip->mWaveform || aclip->mWaveform->pcm.size() <= 0)
        return;
    MediaCore::Overview::Waveform::Holder waveform = aclip->mWaveform;
    drawList->AddRectFilled(leftTop, rightBottom, IM_COL32(16, 16, 16, 255));
    drawList->AddRect(leftTop, rightBottom, IM_COL32(128, 128, 128, 255));
    float wave_range = fmax(fabs(waveform->minSample), fabs(waveform->maxSample));
    int64_t start_time = firstTime; //clip->mStart;
    int64_t end_time = firstTime + visibleTime; //clip->mEnd;
    int start_offset = (int)((double)(clip->StartOffset() + firstTime) / 1000.f / waveform->aggregateDuration);
    auto window_size = rightBottom - leftTop;
    window_size.y /= waveform->pcm.size();
    int window_length = (int)((double)(end_time - start_time) / 1000.f / waveform->aggregateDuration);
    ImGui::SetCursorScreenPos(leftTop);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    for (int i = 0; i < waveform->pcm.size(); i++)
    {
        std::string id_string = "##Waveform_editing@" + std::to_string(mID) + "@" +std::to_string(i);
        int sampleSize = waveform->pcm[i].size();
        if (sampleSize <= 0) continue;
        int sample_stride = window_length / window_size.x;
        if (sample_stride <= 0) sample_stride = 1;
        int min_zoom = ImMax(window_length >> 13, 16);
        int zoom = ImMin(sample_stride, min_zoom);
#if PLOT_IMPLOT
        start_offset = start_offset / zoom * zoom; // align start_offset
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, {0, 0});
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
        if (ImPlot::BeginPlot(id_string.c_str(), window_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
        {
            std::string plot_id = id_string + "_line";
            ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            ImPlot::SetupAxesLimits(0, window_length / zoom, -wave_range, wave_range, ImGuiCond_Always);
            ImPlot::PlotLine(plot_id.c_str(), &waveform->pcm[i][start_offset], window_length / zoom, 1.0, 0.0, 0, 0, sizeof(float) * zoom);
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor();
        ImPlot::PopStyleVar(2);
#elif PLOT_TEXTURE
        ImTextureID texture = mWaveformTextures.size() > i ? mWaveformTextures[i] : nullptr;
        if (!texture || updated || !waveform->parseDone)
        {
            ImGui::ImMat plot_mat;
            start_offset = start_offset / sample_stride * sample_stride; // align start_offset
            ImGui::ImMat plot_frame_max, plot_frame_min;
            auto filled = waveFrameResample(&mWaveform->pcm[i][0], sample_stride, window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.8f, 0.3f, 0.5f));
            if (filled)
            {
                ImGui::PlotMat(plot_mat, (float *)plot_frame_max.data, plot_frame_max.w, 0, -wave_range, wave_range, window_size, sizeof(float), filled, true);
                ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, window_size, sizeof(float), filled, true);
            }
            else
            {
                ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, window_size, sizeof(float), filled);
            }
            ImGui::PopStyleColor(2);
            ImMatToTexture(plot_mat, texture);
            if (texture)
            {
                if (mWaveformTextures.size() <= i) mWaveformTextures.push_back(texture);
                else mWaveformTextures[i] = texture;
            }
        }
        if (texture) drawList->AddImage(texture, leftTop + ImVec2(0, i * window_size.y), leftTop + ImVec2(0, i * window_size.y) + window_size, ImVec2(0, 0), ImVec2(1, 1));
#else
        if (ImGui::BeginChild(id_string.c_str(), window_size, false, ImGuiWindowFlags_NoScrollbar))
        {
            start_offset = start_offset / sample_stride * sample_stride; // align start_offset
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.8f, 0.3f, 0.5f));
            std::string plot_max_id = id_string + "_line_max";
            std::string plot_min_id = id_string + "_line_min";
            ImGui::ImMat plot_frame_max, plot_frame_min;
            waveFrameResample(&mWaveform->pcm[i][0], sample_stride, window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
            ImGui::SetCursorScreenPos(leftTop + ImVec2(0, i * window_size.y));
            ImGui::PlotLinesEx(plot_max_id.c_str(), (float *)plot_frame_max.data, plot_frame_max.w, 0, nullptr, -wave_range, wave_range, window_size, sizeof(float), false, true);
            ImGui::SetCursorScreenPos(leftTop + ImVec2(0, i * window_size.y));
            ImGui::PlotLinesEx(plot_min_id.c_str(), (float *)plot_frame_min.data, plot_frame_min.w, 0, nullptr, -wave_range, wave_range, window_size, sizeof(float), false, true);
            ImGui::PopStyleColor(3);
        }
        ImGui::EndChild();
#endif
    }
    ImGui::PopStyleVar();
}

void EditingAudioClip::CalcDisplayParams(int64_t viewWndDur)
{
    if (visibleTime == viewWndDur)
        return;
    visibleTime = viewWndDur;
}
} //namespace MediaTimeline/Clip

namespace MediaTimeline
{
EditingTextClip::EditingTextClip(TextClip* clip)
    : BaseEditingClip(clip->mID, clip->mMediaID, clip->mType, clip->Start(), clip->End(), clip->StartOffset(), clip->EndOffset(), clip->mHandle)
{
    mText = clip->mText;
}

EditingTextClip::~EditingTextClip()
{

}

void EditingTextClip::CalcDisplayParams(int64_t viewWndDur)
{

}

void EditingTextClip::UpdateClipRange(Clip* clip)
{
    if (mStart != clip->Start())
        mStart = clip->Start();
    if (mEnd != clip->End())
        mEnd = clip->End();
    if (mStartOffset != clip->StartOffset() || mEndOffset != clip->EndOffset())
    {
        mStartOffset = clip->StartOffset();
        mEndOffset = clip->StartOffset();
        mDuration = mEnd - mStart;
    }
}

void EditingTextClip::UpdateClip(Clip* clip)
{
    TextClip* tclip = (TextClip*)clip;
    mID = tclip->mID;
    mMediaID = tclip->mMediaID;
    mType = tclip->mType;
    mStart = tclip->Start();
    mEnd = tclip->End();
    mStartOffset = tclip->StartOffset();
    mEndOffset = tclip->EndOffset();
    mHandle = tclip->mHandle;
    mText = tclip->mText;
}

void EditingTextClip::Save()
{

}

void EditingTextClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated)
{
}
} // namespace MediaTimeline/Text

namespace MediaTimeline
{
Overlap::Overlap(int64_t start, int64_t end, int64_t clip_first, int64_t clip_second, uint32_t type, void* handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    mType = type;
    mStart = start;
    mEnd = end;
    m_Clip.first = clip_first;
    m_Clip.second = clip_second;
    mHandle = handle;
    mTransitionKeyPoints.SetMin({0, 0, 0, 0});
    mTransitionKeyPoints.SetMax(ImVec4(1, 1, 1, end-start), true);
}

Overlap::~Overlap()
{
}

bool Overlap::IsOverlapValid(bool fixRange)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return false;
    auto clip_start = timeline->FindClipByID(m_Clip.first);
    auto clip_end = timeline->FindClipByID(m_Clip.second);
    if (!clip_start || !clip_end)
        return false;
    auto track_start = timeline->FindTrackByClipID(clip_start->mID);
    auto track_end = timeline->FindTrackByClipID(clip_end->mID);
    if (!track_start || !track_end || track_start->mID != track_end->mID)
        return false;
    if (clip_start->End() <= clip_end->Start() ||
        clip_end->End() <= clip_start->Start())
        return false;

    if (fixRange)
    {
        mStart = clip_end->Start() > clip_start->Start() ? clip_end->Start() : clip_start->Start();
        mEnd = clip_start->End() < clip_end->End() ? clip_start->End() : clip_end->End();
    }
    return true;
}

bool Overlap::IsOverlapEmpty()
{
    bool empty = true;
    if (mTransitionBP.is_object() && mTransitionBP.contains("document"))
    {
        auto& doc = mTransitionBP["document"];
        if (doc.is_object() && doc.contains("blueprint"))
        {
            auto& bp = doc["blueprint"];
            if (bp.is_object())
            {
                const imgui_json::array* nodeArray = nullptr;
                if (imgui_json::GetPtrTo(bp, "nodes", nodeArray))
                {
                    for (auto& nodeValue : *nodeArray)
                    {
                        std::string type;
                        if (imgui_json::GetTo<imgui_json::string>(nodeValue, "type", type))
                        {
                            if (type != "Entry" && type !="Exit")
                            {
                                empty = false;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return empty;
}

void Overlap::Update(int64_t start, int64_t start_clip_id, int64_t end, int64_t end_clip_id)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    m_Clip.first = start_clip_id;
    m_Clip.second = end_clip_id;
    mStart = start;
    mEnd = end;
    if (IS_VIDEO(mType))
    {
        auto hOvlp = timeline->mMtvReader->GetOverlapById(mID);
        if (hOvlp)
        {
            auto transition = dynamic_cast<BluePrintVideoTransition *>(hOvlp->GetTransition().get());
            if (transition)
                transition->mKeyPoints.SetMax(ImVec4(1, 1, 1, mEnd-mStart), true);
        }
    }
    else if (IS_AUDIO(mType))
    {
        auto hOvlp = timeline->mMtaReader->GetOverlapById(mID);
        if (hOvlp)
        {
            auto transition = dynamic_cast<BluePrintAudioTransition *>(hOvlp->GetTransition().get());
            if (transition)
                transition->mKeyPoints.SetMax(ImVec4(1, 1, 1, mEnd-mStart), true);
        }
    }
    mTransitionKeyPoints.SetMax(ImVec4(1, 1, 1, mEnd-mStart), true);
}

void Overlap::Seek()
{

}

Overlap* Overlap::Load(const imgui_json::value& value, void * handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    if (!timeline)
        return nullptr;
    int64_t start = 0;
    int64_t end = 0;
    int64_t first = -1;
    int64_t second = -1;
    if (value.contains("Start"))
    {
        auto& val = value["Start"];
        if (val.is_number()) start = val.get<imgui_json::number>();
    }
    if (value.contains("End"))
    {
        auto& val = value["End"];
        if (val.is_number()) end = val.get<imgui_json::number>();
    }
    if (value.contains("Clip_First"))
    {
        auto& val = value["Clip_First"];
        if (val.is_number()) first = val.get<imgui_json::number>();
    }
    if (value.contains("Clip_Second"))
    {
        auto& val = value["Clip_Second"];
        if (val.is_number()) second = val.get<imgui_json::number>();
    }

    Clip* firstClip = timeline->FindClipByID(first);
    if (!firstClip)
        return nullptr;
    Overlap * new_overlap = new Overlap(start, end, first, second, firstClip->mType, handle);
    if (new_overlap)
    {
        if (value.contains("ID"))
        {
            auto& val = value["ID"];
            if (val.is_number()) new_overlap->mID = val.get<imgui_json::number>();
        }
        if (value.contains("Current"))
        {
            auto& val = value["Current"];
            if (val.is_number()) new_overlap->mCurrent = val.get<imgui_json::number>();
        }
        // load transition bp
        if (value.contains("TransitionBP"))
        {
            auto& val = value["TransitionBP"];
            if (val.is_object()) new_overlap->mTransitionBP = val;
        }
        // load curve
        if (value.contains("KeyPoint"))
        {
            auto& keypoint = value["KeyPoint"];
            new_overlap->mTransitionKeyPoints.Load(keypoint);
        }
    }
    return new_overlap;
}

void Overlap::Save(imgui_json::value& value)
{
    // save overlap global info
    value["ID"] = imgui_json::number(mID);
    value["Start"] = imgui_json::number(mStart);
    value["End"] = imgui_json::number(mEnd);
    value["Clip_First"] = imgui_json::number(m_Clip.first);
    value["Clip_Second"] = imgui_json::number(m_Clip.second);
    value["Current"] = imgui_json::number(mCurrent);

    // save overlap transition bp
    if (mTransitionBP.is_object())
    {
        value["TransitionBP"] = mTransitionBP;
    }

    // save curve setting
    imgui_json::value keypoint;
    mTransitionKeyPoints.Save(keypoint);
    value["KeyPoint"] = keypoint;
}
} // namespace MediaTimeline

namespace MediaTimeline
{
EditingVideoOverlap::EditingVideoOverlap(int64_t id, void* handle)
    : BaseEditingOverlap(id, handle)
{

    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    auto ovlp = timeline->FindOverlapByID(mID);
    if (!ovlp)
        return;
    VideoClip* vidclip1 = (VideoClip*)timeline->FindClipByID(ovlp->m_Clip.first);
    VideoClip* vidclip2 = (VideoClip*)timeline->FindClipByID(ovlp->m_Clip.second);
    if (vidclip1 && vidclip2)
    {
        mClip1 = vidclip1; mClip2 = vidclip2;
        if (IS_IMAGE(mClip1->mType))
        {
            m_StartOffset.first = ovlp->mStart - vidclip1->Start();
            mhImgTx1 = vidclip1->GetImageTexture();
        }
        else
        {
            mSsGen1 = MediaCore::Snapshot::Generator::CreateInstance();
            MediaItem* mi = timeline->FindMediaItemByID(vidclip1->mMediaID);
            if (mi && mi->mMediaOverview)
                mSsGen1->SetOverview(mi->mMediaOverview);
            if (timeline) mSsGen1->EnableHwAccel(timeline->mHardwareCodec);
            if (!mSsGen1->Open(vidclip1->mhSsViewer->GetMediaParser(), timeline->mhMediaSettings->VideoOutFrameRate()))
                throw std::runtime_error("FAILED to open the snapshot generator for the 1st video clip!");
            mSsGen1->SetCacheFactor(1.0);
            RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
            if (timeline->mTxMgr->GetTexturePoolAttributes(EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME, tTxPoolAttrs))
            {
                mSsGen1->SetSnapshotSize(tTxPoolAttrs.tTxSize.x, tTxPoolAttrs.tTxSize.y);
            }
            else
            {
                auto video1_info = vidclip1->mhSsViewer->GetMediaParser()->GetBestVideoStream();
                float snapshot_scale1 = video1_info->height > 0 ? 50.f / (float)video1_info->height : 0.05;
                mSsGen1->SetSnapshotResizeFactor(snapshot_scale1, snapshot_scale1);
            }
            m_StartOffset.first = vidclip1->StartOffset() + ovlp->mStart - vidclip1->Start();
            mViewer1 = mSsGen1->CreateViewer(m_StartOffset.first);
        }

        if (IS_IMAGE(mClip2->mType))
        {
            m_StartOffset.second = ovlp->mStart - vidclip2->Start();
            mhImgTx2 = vidclip2->GetImageTexture();
        }
        else
        {
            mSsGen2 = MediaCore::Snapshot::Generator::CreateInstance();
            MediaItem* mi = timeline->FindMediaItemByID(vidclip2->mMediaID);
            if (mi && mi->mMediaOverview)
                mSsGen2->SetOverview(mi->mMediaOverview);
            if (timeline) mSsGen2->EnableHwAccel(timeline->mHardwareCodec);
            if (!mSsGen2->Open(vidclip2->mhSsViewer->GetMediaParser(), timeline->mhMediaSettings->VideoOutFrameRate()))
                throw std::runtime_error("FAILED to open the snapshot generator for the 2nd video clip!");
            mSsGen2->SetCacheFactor(1.0);
            RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
            if (timeline->mTxMgr->GetTexturePoolAttributes(EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME, tTxPoolAttrs))
            {
                mSsGen2->SetSnapshotSize(tTxPoolAttrs.tTxSize.x, tTxPoolAttrs.tTxSize.y);
            }
            else
            {
                auto video2_info = vidclip2->mhSsViewer->GetMediaParser()->GetBestVideoStream();
                float snapshot_scale2 = video2_info->height > 0 ? 50.f / (float)video2_info->height : 0.05;
                mSsGen2->SetSnapshotResizeFactor(snapshot_scale2, snapshot_scale2);
            }
            m_StartOffset.second = vidclip2->StartOffset() + ovlp->mStart - vidclip2->Start();
            mViewer2 = mSsGen2->CreateViewer(m_StartOffset.second);
        }
        mStart = ovlp->mStart;
        mEnd = ovlp->mEnd;
        mDuration = mEnd - mStart;
        
        auto hOvlp = timeline->mMtvReader->GetOverlapById(ovlp->mID);
        IM_ASSERT(hOvlp);
        mTransition = dynamic_cast<BluePrintVideoTransition *>(hOvlp->GetTransition().get());
        if (!mTransition)
        {
            mTransition = new BluePrintVideoTransition(timeline);
            mTransition->SetKeyPoint(ovlp->mTransitionKeyPoints);
            MediaCore::VideoTransition::Holder hTrans(mTransition);
            hOvlp->SetTransition(hTrans);
        }
    }
    else
    {
        Logger::Log(Logger::Error) << "FAILED to initialize 'EditingVideoOverlap' instance! One or both of the source video clip can not be found." << std::endl;
    }
}

EditingVideoOverlap::~EditingVideoOverlap()
{
    mViewer1 = nullptr;
    mViewer2 = nullptr;
    mSsGen1 = nullptr;
    mSsGen2 = nullptr;
    mTransition = nullptr;
}

void EditingVideoOverlap::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated)
{
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    auto ovlp = timeline->FindOverlapByID(mID);
    if (!ovlp)
        return;
    // update display params
    bool ovlpRngChanged = ovlp->mStart != mStart || ovlp->mEnd != mEnd;
    if (ovlpRngChanged)
    {
        mStart = ovlp->mStart;
        mEnd = ovlp->mEnd;
        mDuration = mEnd - mStart;
    }
    ImVec2 viewWndSize = { rightBottom.x - leftTop.x, rightBottom.y - leftTop.y };
    bool vwndChanged = mViewWndSize.x != viewWndSize.x || mViewWndSize.y != viewWndSize.y;
    if (vwndChanged && viewWndSize.x != 0 && viewWndSize.y != 0)
    {
        mViewWndSize = viewWndSize;
        // TODO: need to consider the situation that 1st and 2nd video doesn't have the same size. (wyvern)
        if (mSsGen1 || mSsGen2)
        {
            const MediaCore::VideoStream* vidStream = mSsGen1 ? mSsGen1->GetVideoStream() : mSsGen2->GetVideoStream();
            if (vidStream->width == 0 || vidStream->height == 0)
            {
                Logger::Log(Logger::Error) << "Snapshot video size is INVALID! Width or height is ZERO." << std::endl;
                return;
            }
            mSnapSize.y = viewWndSize.y / 2;
            mSnapSize.x = mSnapSize.y * vidStream->width / vidStream->height;
        }
        else if (mhImgTx1 || mhImgTx2)
        {
            const auto tImgTid1 = mhImgTx1 ? mhImgTx1->TextureID() : nullptr;
            const auto tImgTid2 = mhImgTx2 ? mhImgTx2->TextureID() : nullptr;
            int width = ImGui::ImGetTextureWidth(tImgTid1 ? tImgTid1 : tImgTid2);
            int height = ImGui::ImGetTextureHeight(tImgTid1 ? tImgTid1 : tImgTid2);
            if (width == 0 || height == 0)
            {
                Logger::Log(Logger::Error) << "Snapshot video size is INVALID! Width or height is ZERO." << std::endl;
                return;
            }
            mSnapSize.y = viewWndSize.y / 2;
            mSnapSize.x = mSnapSize.y * width / height;
        }
        else
        {
            Logger::Log(Logger::Error) << "Snapshot video size is INVALID! Width or height is ZERO." << std::endl;
            return;
        }
    }
    if (mViewWndSize.x == 0 || mViewWndSize.y == 0)
        return;

    CalcDisplayParams();

    // get snapshot images
    auto txmgr = ((TimeLine*)(ovlp->mHandle))->mTxMgr;
    std::vector<MediaCore::Snapshot::Image> snapImages1;
    if (mViewer1)
    {
        m_StartOffset.first = mClip1->StartOffset() + ovlp->mStart - mClip1->Start();
        if (!mViewer1->GetSnapshots((double)m_StartOffset.first / 1000, snapImages1))
        {
            Logger::Log(Logger::Error) << mViewer1->GetError() << std::endl;
            return;
        }
        mViewer1->UpdateSnapshotTexture(snapImages1, txmgr, EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME);
    }
    std::vector<MediaCore::Snapshot::Image> snapImages2;
    if (mViewer2)
    {
        m_StartOffset.second = mClip2->StartOffset() + ovlp->mStart - mClip2->Start();
        if (!mViewer2->GetSnapshots((double)m_StartOffset.second / 1000, snapImages2))
        {
            Logger::Log(Logger::Error) << mViewer2->GetError() << std::endl;
            return;
        }
        mViewer2->UpdateSnapshotTexture(snapImages2, txmgr, EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME);
    }

    // draw snapshot images
    if (snapImages1.size() > 0)
    {
        ImVec2 imgLeftTop = leftTop;
        auto imgIter1 = snapImages1.begin();
        while (imgIter1 != snapImages1.end())
        {
            ImVec2 snapDispSize = mSnapSize;
            ImVec2 uvMin{0, 0}, uvMax{1, 1};
            if (imgLeftTop.x+mSnapSize.x > rightBottom.x)
            {
                snapDispSize.x = rightBottom.x - imgLeftTop.x;
                uvMax.x = snapDispSize.x / mSnapSize.x;
            }

            auto snapImg = *imgIter1++;
            auto hTx = snapImg.hDispData&&snapImg.hDispData->mTextureReady ? snapImg.hDispData->mhTx : nullptr;
            auto tid = hTx ? hTx->TextureID() : nullptr;
            if (tid)
            {
                const auto roiRect = hTx->GetDisplayRoi();
                const auto& roiSize = roiRect.size;
                MatUtils::Point2f uvMin2(uvMin.x, uvMin.y), uvMax2(uvMax.x, uvMax.y);
                uvMin2 = roiRect.leftTop+roiSize*uvMin2; uvMax2 = roiRect.leftTop+roiSize*uvMax2;
                drawList->AddImage(tid, imgLeftTop, imgLeftTop + snapDispSize, MatUtils::ToImVec2(uvMin2), MatUtils::ToImVec2(uvMax2));
            }
            else
            {
                drawList->AddRectFilled(imgLeftTop, imgLeftTop + snapDispSize, IM_COL32_BLACK);
                auto center_pos = imgLeftTop + snapDispSize / 2;
                ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
                //ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
                //ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
                //ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
                ImGui::SpinnerBarsRotateFade("Running", 3, 6, 2, ImColor(128, 128, 128), 7.6f, 6);
                drawList->AddRect(imgLeftTop, imgLeftTop + snapDispSize, COL_FRAME_RECT);
            }
            imgLeftTop.x += snapDispSize.x;
            if (imgLeftTop.x >= rightBottom.x)
                break;
        }
    }
    else if (mhImgTx1)
    {
        const auto tImgTid1 = mhImgTx1->TextureID();
        ImVec2 imgLeftTop = leftTop;
        while (1)
        {
            ImVec2 snapDispSize = mSnapSize;
            ImVec2 uvMin{0, 0}, uvMax{1, 1};
            if (imgLeftTop.x+mSnapSize.x > rightBottom.x)
            {
                snapDispSize.x = rightBottom.x - imgLeftTop.x;
                uvMax.x = snapDispSize.x / mSnapSize.x;
            }
            drawList->AddImage(tImgTid1, imgLeftTop, imgLeftTop + snapDispSize, uvMin, uvMax);
            imgLeftTop.x += snapDispSize.x;
            if (imgLeftTop.x >= rightBottom.x)
                break;
        }
    }
    if (snapImages2.size() > 0)
    {
        ImVec2 imgLeftTop = leftTop;
        auto imgIter2 = snapImages2.begin();
        while (imgIter2 != snapImages2.end())
        {
            ImVec2 snapDispSize = mSnapSize;
            ImVec2 uvMin{0, 0}, uvMax{1, 1};
            if (imgLeftTop.x+mSnapSize.x > rightBottom.x)
            {
                snapDispSize.x = rightBottom.x - imgLeftTop.x;
                uvMax.x = snapDispSize.x / mSnapSize.x;
            }
            ImVec2 img2LeftTop = {imgLeftTop.x, imgLeftTop.y+mSnapSize.y};
            auto snapImg = *imgIter2++;
            auto hTx = snapImg.hDispData&&snapImg.hDispData->mTextureReady ? snapImg.hDispData->mhTx : nullptr;
            auto tid = hTx ? hTx->TextureID() : nullptr;
            if (tid)
            {
                const auto roiRect = hTx->GetDisplayRoi();
                const auto& roiSize = roiRect.size;
                MatUtils::Point2f uvMin2(uvMin.x, uvMin.y), uvMax2(uvMax.x, uvMax.y);
                uvMin2 = roiRect.leftTop+roiSize*uvMin2; uvMax2 = roiRect.leftTop+roiSize*uvMax2;
                drawList->AddImage(tid, img2LeftTop, img2LeftTop + snapDispSize, MatUtils::ToImVec2(uvMin2), MatUtils::ToImVec2(uvMax2));
            }
            else
            {
                drawList->AddRectFilled(img2LeftTop, img2LeftTop + snapDispSize, IM_COL32_BLACK);
                auto center_pos = img2LeftTop + snapDispSize / 2;
                ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
                //ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
                //ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
                //ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
                ImGui::SpinnerBarsRotateFade("Running", 3, 6, 2, ImColor(128, 128, 128), 7.6f, 6);
                drawList->AddRect(img2LeftTop, img2LeftTop + snapDispSize, COL_FRAME_RECT);
            }
            imgLeftTop.x += snapDispSize.x;
            if (imgLeftTop.x >= rightBottom.x)
                break;
        }
    }
    else if (mhImgTx2)
    {
        const auto tImgTid2 = mhImgTx2->TextureID();
        ImVec2 imgLeftTop = leftTop;
        while (1)
        {
            ImVec2 snapDispSize = mSnapSize;
            ImVec2 uvMin{0, 0}, uvMax{1, 1};
            if (imgLeftTop.x+mSnapSize.x > rightBottom.x)
            {
                snapDispSize.x = rightBottom.x - imgLeftTop.x;
                uvMax.x = snapDispSize.x / mSnapSize.x;
            }
            ImVec2 img2LeftTop = {imgLeftTop.x, imgLeftTop.y+mSnapSize.y};
            drawList->AddImage(tImgTid2, img2LeftTop, img2LeftTop + snapDispSize, uvMin, uvMax);
            imgLeftTop.x += snapDispSize.x;
            if (imgLeftTop.x >= rightBottom.x)
                break;
        }
    }
}

void EditingVideoOverlap::CalcDisplayParams()
{
    double snapWndSize = (double)mDuration / 1000;
    double snapCntInView = (double)mViewWndSize.x / mSnapSize.x;
    if (mSsGen1) mSsGen1->ConfigSnapWindow(snapWndSize, snapCntInView);
    if (mSsGen2) mSsGen2->ConfigSnapWindow(snapWndSize, snapCntInView);
}

void EditingVideoOverlap::Seek(int64_t pos, bool enterSeekingState)
{
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    timeline->Seek(pos, enterSeekingState);
}

void EditingVideoOverlap::Step(bool forward, int64_t step)
{
}

bool EditingVideoOverlap::GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame, bool preview_frame)
{
    int ret = true;
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return false;
    auto ovlp = timeline->FindOverlapByID(mID);
    if (!ovlp)
        return false;

    auto frames = timeline->GetPreviewFrame();
    ImGui::ImMat frame_org_first;
    auto iter_first = std::find_if(frames.begin(), frames.end(), [ovlp] (auto& cf) {
        return cf.clipId == ovlp->m_Clip.first && cf.phase == MediaCore::CorrelativeFrame::PHASE_SOURCE_FRAME;
    });
    if (iter_first != frames.end())
        frame_org_first = iter_first->frame;
    else
        ret = false;
    ImGui::ImMat frame_org_second;
    auto iter_second = std::find_if(frames.begin(), frames.end(), [ovlp] (auto& cf) {
        return cf.clipId == ovlp->m_Clip.second && cf.phase == MediaCore::CorrelativeFrame::PHASE_SOURCE_FRAME;
    });
    if (iter_second != frames.end())
        frame_org_second = iter_second->frame;
    else
        ret = false;
    
    if (preview_frame)
    {
        if (!frames.empty())
            in_out_frame.second = frames[0].frame;
        else
            ret = false;
    }
    else
    {
        auto iter_out = std::find_if(frames.begin(), frames.end(), [this] (auto& cf) {
            return cf.phase == MediaCore::CorrelativeFrame::PHASE_AFTER_TRANSITION;
        });
        if (iter_out != frames.end())
            in_out_frame.second = iter_out->frame;
        else
            ret = false;
    }
    in_out_frame.first.first = frame_org_first;
    in_out_frame.first.second = frame_org_second;
    return ret;
}

void EditingVideoOverlap::Save()
{
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    auto ovlp = timeline->FindOverlapByID(mID);
    if (!ovlp)
        return;
    ovlp->bEditing = false;
    if (mTransition && mTransition->mBp && mTransition->mBp->Blueprint_IsValid())
    {
        ovlp->mTransitionBP = mTransition->mBp->m_Document->Serialize();
        ovlp->mTransitionKeyPoints = mTransition->mKeyPoints;
    }
    timeline->RefreshPreview();
}
}// namespace MediaTimeline

namespace MediaTimeline
{
EditingAudioOverlap::EditingAudioOverlap(int64_t id, void* handle)
    : BaseEditingOverlap(id, handle)
{
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    auto ovlp = timeline->FindOverlapByID(mID);
    if (!ovlp)
        return;
    AudioClip* audclip1 = (AudioClip*)timeline->FindClipByID(ovlp->m_Clip.first);
    AudioClip* audclip2 = (AudioClip*)timeline->FindClipByID(ovlp->m_Clip.second);
    if (audclip1 && audclip2)
    {
        mClip1 = audclip1; mClip2 = audclip2;
        m_StartOffset.first = audclip1->StartOffset() + ovlp->mStart - audclip1->Start();
        m_StartOffset.second = audclip2->StartOffset() + ovlp->mStart - audclip2->Start();
        mStart = ovlp->mStart;
        mEnd = ovlp->mEnd;
        mDuration = mEnd - mStart;
        auto hOvlp = timeline->mMtaReader->GetOverlapById(ovlp->mID);
        IM_ASSERT(hOvlp);
        mTransition = dynamic_cast<BluePrintAudioTransition *>(hOvlp->GetTransition().get());
        if (!mTransition)
        {
            mTransition = new BluePrintAudioTransition(timeline);
            mTransition->SetKeyPoint(ovlp->mTransitionKeyPoints);
            MediaCore::AudioTransition::Holder hTrans(mTransition);
            hOvlp->SetTransition(hTrans);
        }
    }
    else
    {
        Logger::Log(Logger::Error) << "FAILED to initialize 'EditingAudioOverlap' instance! One or both of the source audio clip can not be found." << std::endl;
    }
}

EditingAudioOverlap::~EditingAudioOverlap()
{
    for (auto texture : mClip1WaveformTextures) ImGui::ImDestroyTexture(texture);
    for (auto texture : mClip2WaveformTextures) ImGui::ImDestroyTexture(texture);
    mClip1WaveformTextures.clear();
    mClip2WaveformTextures.clear();
    mTransition = nullptr;
}

void EditingAudioOverlap::Seek(int64_t pos, bool enterSeekingState)
{
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    timeline->Seek(pos, enterSeekingState);
}

void EditingAudioOverlap::Step(bool forward, int64_t step)
{
}

void EditingAudioOverlap::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated)
{
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    auto ovlp = timeline->FindOverlapByID(mID);
    if (!ovlp)
        return;
    // update display params
    bool ovlpRngChanged = ovlp->mStart != mStart || ovlp->mEnd != mEnd;
    if (ovlpRngChanged)
    {
        mStart = ovlp->mStart;
        mEnd = ovlp->mEnd;
        mDuration = mEnd - mStart;
    }
    ImVec2 viewWndSize = { rightBottom.x - leftTop.x, rightBottom.y - leftTop.y };
    bool vwndChanged = mViewWndSize.x != viewWndSize.x || mViewWndSize.y != viewWndSize.y;
    if (vwndChanged && viewWndSize.x != 0 && viewWndSize.y != 0)
    {
        mViewWndSize = viewWndSize;
    }

    auto window_size = rightBottom - leftTop;
    window_size.y /= 2;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, {0, 0});
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
    if (mClip1 && mClip1->mWaveform)
    {
        ImGui::SetCursorScreenPos(leftTop);
        drawList->AddRectFilled(leftTop, leftTop + window_size, IM_COL32(16, 16, 16, 255));
        drawList->AddRect(leftTop, leftTop + window_size, IM_COL32(128, 128, 128, 255));

        MediaCore::Overview::Waveform::Holder waveform = mClip1->mWaveform;
        float wave_range = fmax(fabs(waveform->minSample), fabs(waveform->maxSample));
        int64_t start_time = std::max(mClip1->Start(), mStart);
        int64_t end_time = std::min(mClip1->End(), mEnd);
        IM_ASSERT(start_time <= end_time);
        int start_offset = (int)((double)((mStart - mClip1->Start())) / 1000.f / waveform->aggregateDuration);
        start_offset = std::max(start_offset, 0);
        auto clip_window_size = window_size;
        clip_window_size.y /= waveform->pcm.size();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.8f, 0.3f, 0.5f));
        for (int i = 0; i < waveform->pcm.size(); i++)
        {
            std::string id_string = "##Waveform_overlap@" + std::to_string(mClip1->mID) + "@" +std::to_string(i);
            int sampleSize = waveform->pcm[i].size();
            if (sampleSize <= 0) continue;
            int window_length = (int)((double)(end_time - start_time) / 1000.f / waveform->aggregateDuration);
            window_length = std::min(window_length, sampleSize);
            int sample_stride = window_length / clip_window_size.x;
            if (sample_stride <= 0) sample_stride = 1;
            int min_zoom = ImMax(window_length >> 13, 16);
            int zoom = ImMin(sample_stride, min_zoom);
#if PLOT_IMPLOT
            start_offset = start_offset / zoom * zoom; // align start_offset
            if (ImPlot::BeginPlot(id_string.c_str(), clip_window_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
            {
                std::string plot_id = id_string + "_line";
                ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, window_length / zoom, -wave_range, wave_range, ImGuiCond_Always);
                ImPlot::PlotLine(plot_id.c_str(), &waveform->pcm[i][start_offset], window_length / zoom, 1.0, 0.0, 0, 0, sizeof(float) * zoom);
                ImPlot::EndPlot();
            }
#elif PLOT_TEXTURE
            ImTextureID texture = mClip1WaveformTextures.size() > i ? mClip1WaveformTextures[i] : nullptr;
            if (!texture || updated || !waveform->parseDone)
            {
                ImGui::ImMat plot_mat;
                start_offset = start_offset / sample_stride * sample_stride; // align start_offset
                ImGui::ImMat plot_frame_max, plot_frame_min;
                auto filled = waveFrameResample(&waveform->pcm[i][0], sample_stride, clip_window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
                if (filled)
                {
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_max.data, plot_frame_max.w, 0, -wave_range, wave_range, clip_window_size, sizeof(float), filled, true);
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, clip_window_size, sizeof(float), filled, true);
                }
                else
                {
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, clip_window_size, sizeof(float), filled);
                }
                ImMatToTexture(plot_mat, texture);
                if (texture)
                {
                    if(mClip1WaveformTextures.size() <= i) mClip1WaveformTextures.push_back(texture);
                    else mClip1WaveformTextures[i] = texture;
                }
            }
            if (texture) drawList->AddImage(texture, leftTop + ImVec2(0, i * clip_window_size.y), leftTop + ImVec2(0, i * clip_window_size.y) + clip_window_size, ImVec2(0, 0), ImVec2(1, 1));
#else
            if (ImGui::BeginChild(id_string.c_str(), clip_window_size, false, ImGuiWindowFlags_NoScrollbar))
            {
                start_offset = start_offset / sample_stride * sample_stride; // align start_offset
                std::string plot_max_id = id_string + "_line_max";
                std::string plot_min_id = id_string + "_line_min";
                ImGui::ImMat plot_frame_max, plot_frame_min;
                waveFrameResample(&waveform->pcm[i][0], sample_stride, clip_window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
                ImGui::SetCursorScreenPos(leftTop + ImVec2(0, i * clip_window_size.y));
                ImGui::PlotLinesEx(plot_max_id.c_str(), (float *)plot_frame_max.data, plot_frame_max.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
                ImGui::SetCursorScreenPos(leftTop + ImVec2(0, i * clip_window_size.y));
                ImGui::PlotLinesEx(plot_min_id.c_str(), (float *)plot_frame_min.data, plot_frame_min.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
            }
            ImGui::EndChild();
#endif
            if (i > 0)
                drawList->AddLine(leftTop + ImVec2(0, clip_window_size.y * i), leftTop + ImVec2(clip_window_size.x, clip_window_size.y * i), IM_COL32(64, 64, 64, 255));
        }
        ImGui::PopStyleColor(3);
    }
    if (mClip2)
    {
        auto clip2_pos = leftTop + ImVec2(0, window_size.y);
        ImGui::SetCursorScreenPos(clip2_pos);
        drawList->AddRectFilled(clip2_pos, clip2_pos + window_size, IM_COL32(32, 32, 32, 255));
        drawList->AddRect(clip2_pos, clip2_pos + window_size, IM_COL32(128, 128, 128, 255));

        MediaCore::Overview::Waveform::Holder waveform = mClip2->mWaveform;
        float wave_range = fmax(fabs(waveform->minSample), fabs(waveform->maxSample));
        int64_t start_time = std::max(mClip2->Start(), mStart);
        int64_t end_time = std::min(mClip2->End(), mEnd);
        IM_ASSERT(start_time <= end_time);
        int start_offset = (int)((double)((mStart - mClip2->Start())) / 1000.f / waveform->aggregateDuration);
        start_offset = std::max(start_offset, 0);
        auto clip_window_size = window_size;
        clip_window_size.y /= waveform->pcm.size();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.8f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.8f, 0.3f, 0.5f));
        for (int i = 0; i < waveform->pcm.size(); i++)
        {
            std::string id_string = "##Waveform_overlap@" + std::to_string(mClip2->mID) + "@" +std::to_string(i);
            int sampleSize = waveform->pcm[i].size();
            if (sampleSize <= 0) continue;
            int window_length = (int)((double)(end_time - start_time) / 1000.f / waveform->aggregateDuration);
            window_length = std::min(window_length, sampleSize);
            int sample_stride = window_length / clip_window_size.x;
            if (sample_stride <= 0) sample_stride = 1;
            int min_zoom = ImMax(window_length >> 13, 16);
            int zoom = ImMin(sample_stride, min_zoom);
#if PLOT_IMPLOT
            start_offset = start_offset / zoom * zoom; // align start_offset
            if (ImPlot::BeginPlot(id_string.c_str(), clip_window_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
            {
                std::string plot_id = id_string + "_line";
                ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, window_length / zoom, -wave_range, wave_range, ImGuiCond_Always);
                ImPlot::PlotLine(plot_id.c_str(), &waveform->pcm[i][start_offset], window_length / zoom, 1.0, 0.0, 0, 0, sizeof(float) * zoom);
                ImPlot::EndPlot();
            }
#elif PLOT_TEXTURE
            ImTextureID texture = mClip2WaveformTextures.size() > i ? mClip2WaveformTextures[i] : nullptr;
            if (!texture || updated || !waveform->parseDone)
            {
                ImGui::ImMat plot_mat;
                start_offset = start_offset / sample_stride * sample_stride; // align start_offset
                ImGui::ImMat plot_frame_max, plot_frame_min;
                auto filled = waveFrameResample(&waveform->pcm[i][0], sample_stride, clip_window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
                if (filled)
                {
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_max.data, plot_frame_max.w, 0, -wave_range, wave_range, clip_window_size, sizeof(float), filled, true);
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, clip_window_size, sizeof(float), filled, true);
                }
                else
                {
                    ImGui::PlotMat(plot_mat, (float *)plot_frame_min.data, plot_frame_min.w, 0, -wave_range, wave_range, clip_window_size, sizeof(float), filled);
                }
                ImMatToTexture(plot_mat, texture);
                if (texture)
                {
                    if(mClip2WaveformTextures.size() <= i) mClip2WaveformTextures.push_back(texture);
                    else mClip2WaveformTextures[i] = texture;
                }
            }
            if (texture) drawList->AddImage(texture, clip2_pos + ImVec2(0, i * clip_window_size.y), clip2_pos + ImVec2(0, i * clip_window_size.y) + clip_window_size, ImVec2(0, 0), ImVec2(1, 1));
#else
            if (ImGui::BeginChild(id_string.c_str(), clip_window_size, false, ImGuiWindowFlags_NoScrollbar))
            {
                start_offset = start_offset / sample_stride * sample_stride; // align start_offset
                std::string plot_max_id = id_string + "_line_max";
                std::string plot_min_id = id_string + "_line_min";
                ImGui::ImMat plot_frame_max, plot_frame_min;
                waveFrameResample(&waveform->pcm[i][0], sample_stride, clip_window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
                ImGui::SetCursorScreenPos(clip2_pos + ImVec2(0, i * clip_window_size.y));
                ImGui::PlotLinesEx(plot_max_id.c_str(), (float *)plot_frame_max.data, plot_frame_max.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
                ImGui::SetCursorScreenPos(clip2_pos + ImVec2(0, i * clip_window_size.y));
                ImGui::PlotLinesEx(plot_min_id.c_str(), (float *)plot_frame_min.data, plot_frame_min.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
            }
            ImGui::EndChild();
#endif
            if (i > 0)
                drawList->AddLine(clip2_pos + ImVec2(0, clip_window_size.y * i), clip2_pos + ImVec2(clip_window_size.x, clip_window_size.y * i), IM_COL32(64, 64, 64, 255));
        }
        ImGui::PopStyleColor(3);
    }
    ImPlot::PopStyleColor();
    ImPlot::PopStyleVar(2);
    ImGui::PopStyleVar();
}

void EditingAudioOverlap::Save()
{
    TimeLine* timeline = (TimeLine*)(mHandle);
    if (!timeline)
        return;
    auto ovlp = timeline->FindOverlapByID(mID);
    if (!ovlp)
        return;
    ovlp->bEditing = false;
    if (mTransition && mTransition->mBp && mTransition->mBp->Blueprint_IsValid())
    {
        ovlp->mTransitionBP = mTransition->mBp->m_Document->Serialize();
        ovlp->mTransitionKeyPoints = mTransition->mKeyPoints;
    }
}

} // namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * MediaTrack Struct Member Functions
 ***********************************************************************************************************/
MediaTrack::MediaTrack(std::string name, uint32_t type, void * handle) :
    m_Handle(handle),
    mType(type)
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    if (timeline)
    {
        if (IS_VIDEO(type))
        {
            if (name.empty()) mName = "V:"; else mName = name;
            mTrackHeight = DEFAULT_VIDEO_TRACK_HEIGHT;
        }
        else if (IS_AUDIO(type))
        {
            if (name.empty()) mName = "A:"; else mName = name;
            mTrackHeight = DEFAULT_AUDIO_TRACK_HEIGHT;
        }
        else if (IS_TEXT(type))
        {
            if (name.empty()) mName = "T:"; else mName = name;
            mTrackHeight = DEFAULT_TEXT_TRACK_HEIGHT;
        }
        else
        {
            if (name.empty()) mName = "U:"; else mName = name;
            mTrackHeight = DEFAULT_TRACK_HEIGHT;
        }
        if (name.empty())
        {
            auto media_count = timeline->GetTrackCount(type);
            media_count ++;
            auto new_name = mName + std::to_string(media_count);
            while (timeline->FindTrackByName(new_name))
            {
                media_count ++;
                new_name = mName + std::to_string(media_count);
            }
            mName = new_name;
        }
        else if (timeline->FindTrackByName(name))
        {
            int name_count = 1;
            auto new_name = name + std::to_string(name_count);
            while (timeline->FindTrackByName(new_name))
            {
                name_count ++;
                new_name = name + std::to_string(name_count);
            }
            mName = new_name;
        }
        else
            mName = name;
    }
    mAudioTrackAttribute.channel_data.clear();
    mAudioTrackAttribute.channel_data.resize(mAudioChannels);
    memcpy(&mAudioTrackAttribute.mBandCfg, &DEFAULT_BAND_CFG, sizeof(mAudioTrackAttribute.mBandCfg));
}

MediaTrack::~MediaTrack()
{
}

bool MediaTrack::DrawTrackControlBar(ImDrawList *draw_list, ImRect rc, bool editable, std::list<imgui_json::value>* pActionList)
{
    bool is_Hovered = false;
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 button_size = ImVec2(12, 12);
    if (mExpanded) ImGui::AddTextRolling(draw_list, NULL, 0, rc.Min + ImVec2(4, 0), ImVec2(rc.GetSize().x - 16, 16), IM_COL32_WHITE, 5, mName.c_str());
    int button_count = 0;
    {
        bool ret = TimelineButton(draw_list, mLocked ? ICON_LOCKED : ICON_UNLOCK, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mLocked ? "unlock" : "lock");
        if (ret && editable && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            mLocked = !mLocked;
        is_Hovered |= ret;
        button_count ++;
    }

    if (IS_AUDIO(mType))
    {
        bool ret = TimelineButton(draw_list, mView ? ICON_SPEAKER : ICON_SPEAKER_MUTE, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mView ? "mute" : "voice");
        if (ret && editable && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            mView = !mView;
            if (pActionList)
            {
                imgui_json::value action;
                action["action"] = "MUTE_TRACK";
                action["media_type"] = imgui_json::number(MEDIA_AUDIO);
                action["track_id"] = imgui_json::number(mID);
                action["muted"] = imgui_json::boolean(!mView);
                pActionList->push_back(std::move(action));
            }
        }
        is_Hovered |= ret;
        button_count ++;
    }
    else
    {
        bool ret = TimelineButton(draw_list, mView ? ICON_VIEW : ICON_VIEW_DISABLE, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mView ? "hidden" : "view");
        if (ret && editable && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            mView = !mView;
            if (pActionList)
            {
                imgui_json::value action;
                action["action"] = "HIDE_TRACK";
                action["media_type"] = imgui_json::number(MEDIA_VIDEO);
                action["track_id"] = imgui_json::number(mID);
                action["visible"] = imgui_json::boolean(mView);
                pActionList->push_back(std::move(action));
            }
        }
        button_count ++;
        is_Hovered |= ret;
    }
    // draw zip button
    bool ret = TimelineButton(draw_list, mExpanded ? ICON_TRACK_ZIP : ICON_TRACK_UNZIP, ImVec2(rc.Max.x - 14, rc.Min.y + 2), ImVec2(14, 14), mExpanded ? "zip" : "unzip");
    if (ret && editable && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        mExpanded = !mExpanded;
    }
    if (!mExpanded) ImGui::AddTextRolling(draw_list, NULL, button_size.y, rc.Min + ImVec2(8 + button_count * 1.5 * button_size.x, 0), ImVec2(rc.GetSize().x - 24 - button_count * 1.5 * button_size.x, 16), IM_COL32_WHITE, 5, mName.c_str());
    is_Hovered |= ret;

    return is_Hovered;
}

void MediaTrack::Update()
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline)
        return;
    // sort m_Clips by clip start time
    std::sort(m_Clips.begin(), m_Clips.end(), [](const Clip *a, const Clip* b){
        return a->Start() < b->Start();
    });
    
    // check all overlaps
    for (auto iter = m_Overlaps.begin(); iter != m_Overlaps.end();)
    {
        if (!(*iter)->IsOverlapValid(true))
        {
            int64_t id = (*iter)->mID;
            iter = m_Overlaps.erase(iter);
            timeline->DeleteOverlap(id);
        }
        else
            ++iter;
    }

    // check is there have new overlap area
    for (auto iter = m_Clips.begin(); iter != m_Clips.end(); iter++)
    {
        for (auto next = iter + 1; next != m_Clips.end(); next++)
        {
            if ((*iter)->End() >= (*next)->Start())
            {
                // it is a overlap area
                int64_t start = std::max((*next)->Start(), (*iter)->Start());
                int64_t end = std::min((*iter)->End(), (*next)->End());
                if (end > start)
                {
                    // check it is in exist overlaps
                    auto overlap = FindExistOverlap((*iter)->mID, (*next)->mID);
                    if (overlap)
                        overlap->Update(start, (*iter)->mID, end, (*next)->mID);
                    else
                        CreateOverlap(start, (*iter)->mID, end, (*next)->mID, (*iter)->mType);
                }
            }
        }
    }
    // update curve range
    if (mMttReader)
        mMttReader->GetKeyPoints()->SetTimeRange(0, timeline->mEnd - timeline->mStart, true);
}

void MediaTrack::CreateOverlap(int64_t start, int64_t start_clip_id, int64_t end, int64_t end_clip_id, uint32_t type)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline)
        return;

    Overlap * new_overlap = new Overlap(start, end, start_clip_id, end_clip_id, type, timeline);
    timeline->m_Overlaps.push_back(new_overlap);
    m_Overlaps.push_back(new_overlap);
    // sort track overlap by overlap start time
    std::sort(m_Overlaps.begin(), m_Overlaps.end(), [](const Overlap *a, const Overlap *b){
        return a->mStart < b->mStart;
    });
}

Overlap * MediaTrack::FindExistOverlap(int64_t start_clip_id, int64_t end_clip_id)
{
    Overlap * found_overlap = nullptr;
    for (auto overlap : m_Overlaps)
    {
        if ((overlap->m_Clip.first == start_clip_id && overlap->m_Clip.second == end_clip_id) ||
            (overlap->m_Clip.first == end_clip_id && overlap->m_Clip.second == start_clip_id))
        {
            found_overlap = overlap;
            break;
        }

    }
    return found_overlap;
}

void MediaTrack::DeleteClip(int64_t id)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline)
        return;
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [id](const Clip* clip)
    {
        return clip->mID == id;
    });
    if (iter != m_Clips.end())
    {
        if (IS_TEXT((*iter)->mType))
        {
            TextClip * tclip = dynamic_cast<TextClip *>(*iter);
            // if clip is text clip, need delete from track holder
            if (mMttReader && tclip->mhDataLayerClip)
            {
                mMttReader->DeleteClip(tclip->mhDataLayerClip);
            }
        }
        m_Clips.erase(iter);
    }
}

bool MediaTrack::CanInsertClip(Clip * clip, int64_t pos)
{
    bool can_insert_clip = true;
    if (!clip || !IS_SAME_TYPE(mType, clip->mType))
    {
        can_insert_clip = false;
    }
    else
    {
        for (auto overlap : m_Overlaps)
        {
            if ((overlap->mStart >= pos && overlap->mStart <= pos + clip->Length()) || 
                (overlap->mEnd >= pos && overlap->mEnd <= pos + clip->Length()))
            {
                can_insert_clip = false;
                break;
            }
        }
    }
    return can_insert_clip;
}

void MediaTrack::InsertClip(Clip* clip, int64_t pos, bool update, std::list<imgui_json::value>* pActionList)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !clip)
        return;
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [clip](const Clip * _clip)
    {
        return _clip->mID == clip->mID;
    });
    if (pos < 0) pos = 0;
    pos = timeline->AlignTime(pos);
    clip->ChangeStart(pos);
    if (iter == m_Clips.end())
    {
        clip->ConfigViewWindow(mViewWndDur, mPixPerMs);
        clip->SetTrackHeight(mTrackHeight);
        m_Clips.push_back(clip);
        if (pActionList)
        {
            imgui_json::value action;
            action["action"] = "ADD_CLIP";
            action["media_type"] = imgui_json::number(clip->mType);
            action["to_track_id"] = imgui_json::number(mID);
            action["clip_json"] = clip->SaveAsJson();
            pActionList->push_back(std::move(action));
        }
    }
    // also insert this clip into TimeLine::m_Clips array
    auto& aTlClips = timeline->m_Clips;
    auto iter2 = std::find_if(aTlClips.begin(), aTlClips.end(), [clip] (const auto& elem) {
        return elem->mID == clip->mID;
    });
    if (iter2 == aTlClips.end())
        aTlClips.push_back(clip);
    if (update) Update();
}

Clip * MediaTrack::FindPrevClip(int64_t id)
{
    Clip * found_clip = nullptr;
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline)
        return found_clip;
    auto current_clip = timeline->FindClipByID(id);
    if (!current_clip)
        return found_clip;
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [id](const Clip* ss) {
        return ss->mID == id;
    });
    if (iter == m_Clips.begin() || iter == m_Clips.end())
        return found_clip;
    
    found_clip = *(iter - 1);
    return found_clip;
}

Clip * MediaTrack::FindNextClip(int64_t id)
{
    Clip * found_clip = nullptr;
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline)
        return found_clip;
    auto current_clip = timeline->FindClipByID(id);
    if (!current_clip)
        return found_clip;
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [id](const Clip* ss) {
        return ss->mID == id;
    });

    if (iter == m_Clips.end() || iter == m_Clips.end() - 1)
        return found_clip;

    found_clip = *(iter + 1);
    return found_clip;
}

Clip * MediaTrack::FindClips(int64_t time, int& count)
{
    Clip * ret_clip = nullptr;
    int selected_count = 0;
    std::vector<Clip *> clips;
    std::vector<Clip *> select_clips;
    for (auto clip : m_Clips)
    {
        if (clip->Start() <= time && clip->End() >= time)
        {
            clips.push_back(clip);
            if (clip->bSelected)
                select_clips.push_back(clip);
        }
    }
    for (auto clip : select_clips)
    {
        if (!ret_clip || ret_clip->Length() > clip->Length())
            ret_clip = clip;
    }
    if (!ret_clip)
    {
        for (auto clip : clips)
        {
            if (!ret_clip || ret_clip->Length() > clip->Length())
                ret_clip = clip;
        }
    }
    count = clips.size();
    return ret_clip;
}

void MediaTrack::SelectClip(Clip * clip, bool appand)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !clip)
        return;

    bool selected = true;
    if (appand && clip->bSelected)
    {
        selected = false;
    }
    
    for (auto _clip : timeline->m_Clips)
    {
        if (_clip->mID != clip->mID)
        {
            if (timeline->bSelectLinked && _clip->isLinkedWith(clip))
            {
                _clip->bSelected = selected;
            }
            else if (!appand)
            {
                _clip->bSelected = !selected;
            }
        }
    }
    clip->bSelected = selected;
}

void MediaTrack::SelectEditingClip(Clip * clip)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !clip)
        return;
    if (IS_DUMMY(clip->mType))
        return;
    
    int updated = 0;

    auto found = timeline->FindEditingItem(EDITING_CLIP, clip->mID);
    if (found == -1)
    {
        uint32_t type = MEDIA_UNKNOWN;
        BaseEditingClip *eclip = nullptr;
        if (IS_VIDEO(clip->mType))
        {
            type = MEDIA_VIDEO;
            eclip = new EditingVideoClip((VideoClip*)clip);
        }
        else if (IS_AUDIO(clip->mType))
        {
            type = MEDIA_AUDIO;
            eclip = new EditingAudioClip((AudioClip*)clip);
        }
        else if (IS_TEXT(clip->mType))
        {
            type = MEDIA_TEXT;
            eclip = new EditingTextClip((TextClip*)clip);
        }
        if (eclip)
        {
            EditingItem *item = new EditingItem(type, eclip);
            item->mIndex = timeline->mEditingItems.size();
            timeline->mEditingItems.push_back(item);
            timeline->mSelectedItem = item->mIndex;
            if (timeline->mCurrentTime < clip->Start() || timeline->mCurrentTime >= clip->End())
                timeline->Seek(clip->Start());
        }
    }
    else
    {
        timeline->mSelectedItem = found;
        auto item = timeline->mEditingItems[found];
        if (item)
        {
            if (item->mEditorType == EDITING_CLIP && item->mEditingClip && item->mEditingClip->mCurrentTime != -1)
            {
                timeline->Seek(clip->Start() + item->mEditingClip->mCurrentTime);
            }
            else
            {
                timeline->Seek(clip->Start());
            }
            if (IS_TEXT(item->mMediaType))
            {
                TextClip* tclip = (TextClip*)clip;
                EditingTextClip * eclip = (EditingTextClip *)item->mEditingClip;
                if (eclip->mText != tclip->mText || eclip->mStart != tclip->Start() || eclip->mDuration != tclip->Length())
                {
                    // text editing clip need to update if we choose diff clip(text clip editing shared UI with same track)
                    eclip->UpdateClip(clip);
                    item->mTooltip = tclip->mText;
                }
            }
        }
    }
    if (timeline->m_CallBacks.EditingClip)
    {
        updated = timeline->m_CallBacks.EditingClip(0, clip);
    }
}

void MediaTrack::SelectEditingOverlap(Overlap * overlap)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !overlap)
        return;

    auto first = timeline->FindClipByID(overlap->m_Clip.first);
    auto second = timeline->FindClipByID(overlap->m_Clip.second);
    if (!first || !second)
        return;
    if (IS_DUMMY(first->mType) || IS_DUMMY(second->mType))
        return;

    overlap->bEditing = true;

    auto found = timeline->FindEditingItem(EDITING_TRANSITION, overlap->mID);
    if (found == -1)
    {
        uint32_t type = MEDIA_UNKNOWN;
        BaseEditingOverlap *eoverlap = nullptr;
        if (IS_VIDEO(first->mType) && IS_VIDEO(second->mType))
        {
            type = MEDIA_VIDEO;
            eoverlap = new EditingVideoOverlap(overlap->mID, timeline);
        }
        else if (IS_AUDIO(first->mType) && IS_AUDIO(second->mType))
        {
            type = MEDIA_AUDIO;
            eoverlap = new EditingAudioOverlap(overlap->mID, timeline);
        }
        if (eoverlap)
        {
            EditingItem * item = new EditingItem(type, eoverlap);
            item->mIndex = timeline->mEditingItems.size();
            timeline->mEditingItems.push_back(item);
            timeline->mSelectedItem = item->mIndex;
            timeline->Seek(overlap->mStart);
        }
    }
    else
    {
        timeline->mSelectedItem = found;
        auto item = timeline->mEditingItems[found];
        if (item)
        {
            if (item->mEditorType == EDITING_TRANSITION && item->mEditingOverlap && item->mEditingOverlap->mCurrentTime != -1)
            {
                timeline->Seek(overlap->mStart + item->mEditingOverlap->mCurrentTime);
            }
            else
            {
                timeline->Seek(overlap->mStart);
            }
        }
    }

    if (timeline->m_CallBacks.EditingOverlap)
    {
        timeline->m_CallBacks.EditingOverlap(first->mType, overlap);
    }
}

void MediaTrack::CalculateAudioScopeData(ImGui::ImMat& mat_in)
{
    ImGui::ImMat mat;
    if (mat_in.empty() || mat_in.w < 64)
        return;
    int fft_size = mat_in.w  > 256 ? 256 : mat_in.w > 128 ? 128 : 64;
    if (mat_in.elempack > 1)
    {
        mat.create_type(fft_size, 1, mat_in.c, mat_in.type);
        float * data = (float *)mat_in.data;
        for (int x = 0; x < mat.w; x++)
        {
            for (int i = 0; i < mat.c; i++)
            {
                mat.at<float>(x, 0, i) = data[x * mat.c + i];
            }
        }
    }
    else
    {
        //mat = mat_in;
        mat.create_type(fft_size, 1, mat_in.c, mat_in.data, mat_in.type);
    }
    for (int i = 0; i < mat.c; i++)
    {
        if (i < mAudioChannels)
        {
            // we only calculate decibel for now
            auto & channel_data = mAudioTrackAttribute.channel_data[i];
            channel_data.m_wave.clone_from(mat.channel(i));
            channel_data.m_fft.clone_from(mat.channel(i));
            ImGui::ImRFFT((float *)channel_data.m_fft.data, channel_data.m_fft.w, true);
            channel_data.m_decibel = ImGui::ImDoDecibel((float*)channel_data.m_fft.data, mat.w);
        }
    }
}

float MediaTrack::GetAudioLevel(int channel)
{
    if (IS_AUDIO(mType))
    {
        if (channel < mAudioTrackAttribute.channel_data.size())
            return mAudioTrackAttribute.channel_data[channel].m_decibel;
    }
    return 0;
}

void MediaTrack::SetAudioLevel(int channel, float level)
{
    if (IS_AUDIO(mType))
    {
        if (channel < mAudioTrackAttribute.channel_data.size())
            mAudioTrackAttribute.channel_data[channel].m_decibel = level;
    }
}

MediaTrack* MediaTrack::Load(const imgui_json::value& value, void * handle)
{
    uint32_t type = MEDIA_UNKNOWN;
    std::string name;
    TimeLine * timeline = (TimeLine *)handle;
    if (!timeline)
        return nullptr;
    
    if (value.contains("Type"))
    {
        auto& val = value["Type"];
        if (val.is_number()) type = val.get<imgui_json::number>();
    }
    if (value.contains("Name"))
    {
        auto& val = value["Name"];
        if (val.is_string()) name = val.get<imgui_json::string>();
    }
    MediaTrack * new_track = new MediaTrack(name, type, handle);
    if (new_track)
    {
        new_track->mPixPerMs = timeline->msPixelWidthTarget;
        new_track->mViewWndDur = timeline->visibleTime;
        if (value.contains("ID"))
        {
            auto& val = value["ID"];
            if (val.is_number()) new_track->mID = val.get<imgui_json::number>();
        }
        if (value.contains("Expanded"))
        {
            auto& val = value["Expanded"];
            if (val.is_boolean()) new_track->mExpanded = val.get<imgui_json::boolean>();
        }
        if (value.contains("View"))
        {
            auto& val = value["View"];
            if (val.is_boolean()) new_track->mView = val.get<imgui_json::boolean>();
        }
        if (value.contains("Locked"))
        {
            auto& val = value["Locked"];
            if (val.is_boolean()) new_track->mLocked = val.get<imgui_json::boolean>();
        }
        if (value.contains("Selected"))
        {
            auto& val = value["Selected"];
            if (val.is_boolean()) new_track->mSelected = val.get<imgui_json::boolean>();
        }
        if (value.contains("Linked"))
        {
            auto& val = value["Linked"];
            if (val.is_number()) new_track->mLinkedTrack = val.get<imgui_json::number>();
        }
        if (value.contains("ViewHeight"))
        {
            auto& val = value["ViewHeight"];
            if (val.is_number()) new_track->mTrackHeight = val.get<imgui_json::number>();
        }

        // load and check clip into track
        const imgui_json::array* clipIDArray = nullptr;
        if (imgui_json::GetPtrTo(value, "ClipIDS", clipIDArray))
        {
            for (auto& id_val : *clipIDArray)
            {
                int64_t clip_id = id_val.get<imgui_json::number>();
                Clip * clip = timeline->FindClipByID(clip_id);
                if (clip)
                {
                    new_track->m_Clips.push_back(clip);
                    clip->ConfigViewWindow(new_track->mViewWndDur, new_track->mPixPerMs);
                    clip->SetTrackHeight(new_track->mTrackHeight);
                }
            }
        }

        // load and check overlap into track
        const imgui_json::array* overlapIDArray = nullptr;
        if (imgui_json::GetPtrTo(value, "OverlapIDS", overlapIDArray))
        {
            for (auto& id_val : *overlapIDArray)
            {
                int64_t overlap_id = id_val.get<imgui_json::number>();
                Overlap * overlap = timeline->FindOverlapByID(overlap_id);
                if (overlap)
                    new_track->m_Overlaps.push_back(overlap);
            }
        }

        // load audio attribute
        if (value.contains("AudioAttribute"))
        {
            auto& audio_attr = value["AudioAttribute"];
            if (audio_attr.contains("AudioGain"))
            {
                auto& val = audio_attr["AudioGain"];
                if (val.is_number()) new_track->mAudioTrackAttribute.mAudioGain = val.get<imgui_json::number>();
            }
        }

        // load subtitle track
        if (value.contains("SubTrack"))
        {
            auto& track = value["SubTrack"];
            int64_t track_id = 0;
            if (track.contains("ID"))
            {
                auto& val = track["ID"];
                if (val.is_number()) track_id = val.get<imgui_json::number>();
            }
            new_track->mMttReader = timeline->mMtvReader->NewEmptySubtitleTrack(track_id); //MediaCore::SubtitleTrack::NewEmptyTrack(track_id);
            if (track.contains("Font"))
            {
                auto& val = track["Font"];
                if (val.is_string()) new_track->mMttReader->SetFont(val.get<imgui_json::string>());
            }
            if (track.contains("OffsetX"))
            {
                auto& val = track["OffsetX"];
                if (val.is_number()) new_track->mMttReader->SetOffsetH((float)val.get<imgui_json::number>());
            }
            if (track.contains("OffsetY"))
            {
                auto& val = track["OffsetY"];
                if (val.is_number()) new_track->mMttReader->SetOffsetV((float)val.get<imgui_json::number>());
            }
            if (track.contains("ScaleX"))
            {
                auto& val = track["ScaleX"];
                if (val.is_number()) new_track->mMttReader->SetScaleX(val.get<imgui_json::number>());
            }
            if (track.contains("ScaleY"))
            {
                auto& val = track["ScaleY"];
                if (val.is_number()) new_track->mMttReader->SetScaleY(val.get<imgui_json::number>());
            }
            if (track.contains("Spacing"))
            {
                auto& val = track["Spacing"];
                if (val.is_number()) new_track->mMttReader->SetSpacing(val.get<imgui_json::number>());
            }
            if (track.contains("Angle"))
            {
                auto& val = track["Angle"];
                if (val.is_number()) new_track->mMttReader->SetAngle(val.get<imgui_json::number>());
            }
            if (track.contains("OutlineWidth"))
            {
                auto& val = track["OutlineWidth"];
                if (val.is_number()) new_track->mMttReader->SetOutlineWidth(val.get<imgui_json::number>());
            }
            if (track.contains("Alignment"))
            {
                auto& val = track["Alignment"];
                if (val.is_number()) new_track->mMttReader->SetAlignment(val.get<imgui_json::number>());
            }
            if (track.contains("Italic"))
            {
                auto& val = track["Italic"];
                if (val.is_number()) new_track->mMttReader->SetItalic(val.get<imgui_json::number>());
            }
            if (track.contains("Bold"))
            {
                auto& val = track["Bold"];
                if (val.is_number()) new_track->mMttReader->SetBold(val.get<imgui_json::number>());
            }
            if (track.contains("UnderLine"))
            {
                auto& val = track["UnderLine"];
                if (val.is_boolean()) new_track->mMttReader->SetUnderLine(val.get<imgui_json::boolean>());
            }
            if (track.contains("StrikeOut"))
            {
                auto& val = track["StrikeOut"];
                if (val.is_boolean()) new_track->mMttReader->SetStrikeOut(val.get<imgui_json::boolean>());
            }
            if (track.contains("PrimaryColor"))
            {
                auto& val = track["PrimaryColor"];
                if (val.is_vec4()) new_track->mMttReader->SetPrimaryColor(val.get<imgui_json::vec4>());
            }
            if (track.contains("OutlineColor"))
            {
                auto& val = track["OutlineColor"];
                if (val.is_vec4()) new_track->mMttReader->SetOutlineColor(val.get<imgui_json::vec4>());
            }
            if (track.contains("ScaleLink"))
            {
                auto& val = track["ScaleLink"];
                if (val.is_boolean()) new_track->mTextTrackScaleLink = val.get<imgui_json::boolean>();
            }
            if (track.contains("KeyPoint"))
            {
                auto& keypoint = track["KeyPoint"];
                new_track->mMttReader->GetKeyPoints()->Load(keypoint);
            }
            new_track->mMttReader->SetFrameSize(timeline->GetPreviewWidth(), timeline->GetPreviewHeight());
            new_track->mMttReader->EnableFullSizeOutput(false);
            for (auto clip : new_track->m_Clips)
            {
                if (IS_TEXT(clip->mType))
                {
                    TextClip * tclip = dynamic_cast<TextClip *>(clip);
                    tclip->CreateDataLayer(new_track);
                }
            }
        }
    }
    return new_track;
}

void MediaTrack::Save(imgui_json::value& value)
{
    // save track info
    value["ID"] = imgui_json::number(mID);
    value["Type"] = imgui_json::number(mType);
    value["Name"] = mName;
    value["Expanded"] = imgui_json::boolean(mExpanded);
    value["View"] = imgui_json::boolean(mView);
    value["Locked"] = imgui_json::boolean(mLocked);
    value["Selected"] = imgui_json::boolean(mSelected);
    value["Linked"] = imgui_json::number(mLinkedTrack);
    value["ViewHeight"] = imgui_json::number(mTrackHeight);

    // save clip ids
    imgui_json::value clips;
    for (auto clip : m_Clips)
    {
        imgui_json::value clip_id_value = imgui_json::number(clip->mID);
        clips.push_back(clip_id_value);
    }
    if (m_Clips.size() > 0) value["ClipIDS"] = clips;

    // save overlap ids
    imgui_json::value overlaps;
    for (auto overlap : m_Overlaps)
    {
        imgui_json::value overlap_id_value = imgui_json::number(overlap->mID);
        overlaps.push_back(overlap_id_value);
    }
    if (m_Overlaps.size() > 0) value["OverlapIDS"] = overlaps;

    // save audio attribute
    imgui_json::value audio_attr;
    {
        audio_attr["AudioGain"] = imgui_json::number(mAudioTrackAttribute.mAudioGain);
    }
    value["AudioAttribute"] = audio_attr;

    // save subtitle track info
    if (mMttReader)
    {
        imgui_json::value subtrack;
        auto& style = mMttReader->DefaultStyle();
        subtrack["ID"] = imgui_json::number(mMttReader->Id());
        subtrack["Font"] = style.Font();
        subtrack["OffsetX"] = imgui_json::number(style.OffsetHScale());
        subtrack["OffsetY"] = imgui_json::number(style.OffsetVScale());
        subtrack["ScaleX"] = imgui_json::number(style.ScaleX());
        subtrack["ScaleY"] = imgui_json::number(style.ScaleY());
        subtrack["Spacing"] = imgui_json::number(style.Spacing());
        subtrack["Angle"] = imgui_json::number(style.Angle());
        subtrack["OutlineWidth"] = imgui_json::number(style.OutlineWidth());
        subtrack["Alignment"] = imgui_json::number(style.Alignment());
        subtrack["Italic"] = imgui_json::number(style.Italic());
        subtrack["Bold"] = imgui_json::number(style.Bold());
        subtrack["UnderLine"] = imgui_json::boolean(style.UnderLine());
        subtrack["StrikeOut"] = imgui_json::boolean(style.StrikeOut());
        subtrack["PrimaryColor"] = imgui_json::vec4(style.PrimaryColor().ToImVec4());
        subtrack["OutlineColor"] = imgui_json::vec4(style.OutlineColor().ToImVec4());
        subtrack["ScaleLink"] = imgui_json::boolean(mTextTrackScaleLink);
        imgui_json::value keypoint;
        mMttReader->GetKeyPoints()->Save(keypoint);
        subtrack["KeyPoint"] = keypoint;
        value["SubTrack"] = subtrack;
    }
}

} // namespace MediaTimeline/MediaTrack

namespace MediaTimeline
{
/***********************************************************************************************************
 * ClipGroup Struct Member Functions
 ***********************************************************************************************************/
ClipGroup::ClipGroup(void * handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    ImGui::RandomColor(mColor, 0.5f);
}

void ClipGroup::Load(const imgui_json::value& value)
{
    if (value.contains("ID"))
    {
        auto& val = value["ID"];
        if (val.is_number()) mID = val.get<imgui_json::number>();
    }
    if (value.contains("Color"))
    {
        auto& val = value["Color"];
        if (val.is_number()) mColor = val.get<imgui_json::number>();
    }
    const imgui_json::array* clipIDArray = nullptr;
    if (imgui_json::GetPtrTo(value, "ClipIDS", clipIDArray))
    {
        for (auto& id_val : *clipIDArray)
        {
            int64_t clip_id = id_val.get<imgui_json::number>();
            m_Grouped_Clips.push_back(clip_id);
        }
    }
}

void ClipGroup::Save(imgui_json::value& value)
{
    value["ID"] = imgui_json::number(mID);
    value["Color"] = imgui_json::number(mColor);
    imgui_json::array clipIdsJson;
    for (auto cid : m_Grouped_Clips)
        clipIdsJson.push_back(imgui_json::number(cid));
    value["ClipIDS"] = clipIdsJson;
}
} // namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * EditingItem Struct Member Functions
 ***********************************************************************************************************/
EditingItem::EditingItem(uint32_t media_type, BaseEditingClip * clip)
{
    if (!clip)
        return;
    TimeLine * timeline = (TimeLine *)clip->mHandle;
    if (!timeline)
        return;
    auto item = timeline->FindMediaItemByID(clip->mMediaID);
    if (!IS_TEXT(clip->mType) && (!item || !item->mValid))
        return;
    mMediaType = media_type; 
    mEditingClip = clip; 
    mEditorType = EDITING_CLIP;
    mName = IS_VIDEO(media_type) ? std::string(ICON_MEDIA_VIDEO) : IS_AUDIO(media_type) ? std::string(ICON_MEDIA_AUDIO) : IS_TEXT(media_type) ? std::string(ICON_MEDIA_TEXT) : "?";
    if (!IS_TEXT(clip->mType))
    {
        mName += " " + item->mName;
        mTooltip = "Clip Editing " + mName;
    }
    else
    {
        auto track = timeline->FindTrackByClipID(clip->mID);
        auto tclip = (EditingTextClip*)clip;
        if (track) mName += " " + track->mName;
        mTooltip = tclip->mText;
    }
    
    if (IS_VIDEO(media_type))
    {
        if (!item->mMediaThumbnail.empty() && item->mMediaThumbnail[0])
        {
            ImGui::ImMat thumbnail_mat;
            auto hTx = item->mMediaThumbnail[0];
            auto roi = hTx->GetDisplayRoi();
            auto texture = hTx->TextureID();
            ImVec2 size(ImGui::ImGetTextureWidth(texture), ImGui::ImGetTextureHeight(texture));
            ImVec2 offset = MatUtils::ToImVec2(roi.leftTop) * size;
            ImVec2 texture_size = MatUtils::ToImVec2(roi.size) * size;
            ImGui::ImTextureToMat(texture, thumbnail_mat, offset, texture_size);
            if (!thumbnail_mat.empty()) ImMatToTexture(thumbnail_mat, mTexture);
        }
    }
    else if (IS_AUDIO(media_type))
    {
        auto wavefrom = item->mMediaOverview->GetWaveform();
        if (wavefrom && wavefrom->pcm.size() > 0)
        {
            ImGui::ImMat plot_mat;
            ImVec2 wave_size(128, 64);
            waveformToMat(wavefrom, plot_mat, wave_size);
            if (!plot_mat.empty())
            {
                plot_mat.draw_rectangle(ImPoint(0, 0), ImPoint(plot_mat.w - 1, plot_mat.h - 1), ImPixel(1, 1, 1, 1));
                ImMatToTexture(plot_mat, mTexture);
            }
        }
    }
    else if (IS_TEXT(media_type))
    {
        mTexture = nullptr;
    }
}

EditingItem::EditingItem(uint32_t media_type, BaseEditingOverlap * overlap)
{
    if (!overlap)
        return;
    TimeLine * timeline = (TimeLine *)overlap->mHandle;
    if (!timeline)
        return;
    auto ovlp = timeline->FindOverlapByID(overlap->mID);
    if (!ovlp)
        return;
    auto first_clip = timeline->FindClipByID(ovlp->m_Clip.first);
    auto second_clip = timeline->FindClipByID(ovlp->m_Clip.second);
    if (!first_clip || !second_clip)
        return;
    auto first_item = timeline->FindMediaItemByID(first_clip->mMediaID);
    auto second_item = timeline->FindMediaItemByID(second_clip->mMediaID);
    if (!first_item || !second_item)
        return;
    mMediaType = media_type; 
    mEditingOverlap = overlap; 
    mEditorType = EDITING_TRANSITION;
    auto type_char = IS_VIDEO(media_type) ? std::string(ICON_MEDIA_VIDEO) : IS_AUDIO(media_type) ? std::string(ICON_MEDIA_AUDIO) : IS_TEXT(media_type) ? std::string(ICON_MEDIA_TEXT) : "?";
    mName = type_char + " " + first_item->mName + "->" + type_char + " " + second_item->mName;
    mTooltip = "Transition Editing " + mName;
    if (IS_VIDEO(media_type))
    {
        ImGui::ImMat first_mat, second_mat;
        if (!first_item->mMediaThumbnail.empty() && first_item->mMediaThumbnail[0])
        {
            auto hTx = first_item->mMediaThumbnail[0];
            auto roi = hTx->GetDisplayRoi();
            auto texture = hTx->TextureID();
            ImVec2 size(ImGui::ImGetTextureWidth(texture), ImGui::ImGetTextureHeight(texture));
            ImVec2 offset = MatUtils::ToImVec2(roi.leftTop) * size;
            ImVec2 texture_size = MatUtils::ToImVec2(roi.size) * size;
            ImGui::ImTextureToMat(texture, first_mat, offset, texture_size);
            first_mat.draw_rectangle(ImPoint(0, 0), ImPoint(first_mat.w - 1, first_mat.h - 1), ImPixel(1, 1, 1, 1));
        }
        if (!second_item->mMediaThumbnail.empty() && second_item->mMediaThumbnail[0])
        {
            auto hTx = second_item->mMediaThumbnail[0];
            auto roi = hTx->GetDisplayRoi();
            auto texture = hTx->TextureID();
            ImVec2 size(ImGui::ImGetTextureWidth(texture), ImGui::ImGetTextureHeight(texture));
            ImVec2 offset = MatUtils::ToImVec2(roi.leftTop) * size;
            ImVec2 texture_size = MatUtils::ToImVec2(roi.size) * size;
            ImGui::ImTextureToMat(texture, second_mat, offset, texture_size);
            second_mat.draw_rectangle(ImPoint(0, 0), ImPoint(second_mat.w - 1, second_mat.h - 1), ImPixel(1, 1, 1, 1));
        }
        if (!first_mat.empty() && !second_mat.empty())
        {
            auto width = first_mat.w + first_mat.w / 3;
            auto height = first_mat.h + first_mat.h / 3;
            ImGui::ImMat overlap_mat(width, height, 4, (size_t)1, 4);
            first_mat.copy_to(overlap_mat);
            second_mat.copy_to(overlap_mat, ImPoint(first_mat.w / 3, first_mat.h / 3));
            ImMatToTexture(overlap_mat, mTexture);
        }
        else
        {
            if (!first_mat.empty()) ImMatToTexture(first_mat, mTexture);
            else if (!second_mat.empty()) ImMatToTexture(second_mat, mTexture);
        }
    }
    else if (IS_AUDIO(media_type))
    {
        ImGui::ImMat first_mat, second_mat;
        auto first_wavefrom = first_item->mMediaOverview->GetWaveform();
        auto second_wavefrom = second_item->mMediaOverview->GetWaveform();
        ImVec2 wave_size(96, 48);
        if (first_wavefrom && first_wavefrom->pcm.size() > 0)
        {
            waveformToMat(first_wavefrom, first_mat, wave_size);
            first_mat.draw_rectangle(ImPoint(0, 0), ImPoint(first_mat.w - 1, first_mat.h - 1), ImPixel(1, 1, 1, 1));
        }
        if (second_wavefrom && second_wavefrom->pcm.size() > 0)
        {
            waveformToMat(second_wavefrom, second_mat, wave_size);
            second_mat.draw_rectangle(ImPoint(0, 0), ImPoint(second_mat.w - 1, second_mat.h - 1), ImPixel(1, 1, 1, 1));
        }
        if (!first_mat.empty() && !second_mat.empty())
        {
            auto width = first_mat.w + first_mat.w / 3;
            auto height = first_mat.h + first_mat.h / 3;
            ImGui::ImMat overlap_mat(width, height, 4, (size_t)1, 4);
            first_mat.copy_to(overlap_mat);
            second_mat.copy_to(overlap_mat, ImPoint(first_mat.w / 3, first_mat.h / 3));
            ImMatToTexture(overlap_mat, mTexture);
        }
        else
        {
            if (!first_mat.empty()) ImMatToTexture(first_mat, mTexture);
            else if (!second_mat.empty()) ImMatToTexture(second_mat, mTexture);
        }
    }
    else if (IS_TEXT(media_type))
    {
        // TODO::Dicky
    }
}

EditingItem::~EditingItem()
{
    if (mTexture)
    {
        ImGui::ImDestroyTexture(mTexture);
        mTexture = nullptr;
    }
    if (mEditingClip)
    {
        mEditingClip->Save();
        delete mEditingClip;
    }
    if (mEditingOverlap)
    {
        mEditingOverlap->Save();
        delete mEditingOverlap;
    }
}
} // namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * TimeLine Struct Member Functions
 ***********************************************************************************************************/
int TimeLine::OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (!handle)
        return BluePrint::BP_CBR_Unknown;
    TimeLine * timeline = (TimeLine *)handle;
    if (name.compare("VideoFilter") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED ||
            type == BluePrint::BP_CB_NODE_APPEND ||
            type == BluePrint::BP_CB_NODE_INSERT)
        {
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
        }
    }
    if (name.compare("VideoTransition") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED ||
            type == BluePrint::BP_CB_NODE_APPEND ||
            type == BluePrint::BP_CB_NODE_INSERT)
        {
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
        }
    }
    if (name.compare("AudioFilter") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED ||
            type == BluePrint::BP_CB_NODE_APPEND ||
            type == BluePrint::BP_CB_NODE_INSERT)
        {
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
        }
    }
    if (timeline) timeline->mIsBluePrintChanged = true;
    return ret;
}

TimeLine::TimeLine(std::string plugin_path)
    : mStart(0), mEnd(0), mPcmStream(this)
{
    std::srand(std::time(0)); // init std::rand

    mTxMgr = RenderUtils::TextureManager::GetDefaultInstance();
    if (!mTxMgr->CreateTexturePool(PREVIEW_TEXTURE_POOL_NAME, {1920, 1080}, IM_DT_INT8, 0))
        Logger::Log(Logger::Error) << "FAILED to create texture pool '" << PREVIEW_TEXTURE_POOL_NAME << "'! Error is '" << mTxMgr->GetError() << "'." << std::endl;
    if (!mTxMgr->CreateTexturePool(ARBITRARY_SIZE_TEXTURE_POOL_NAME, {0, 0}, IM_DT_INT8, 0))
        Logger::Log(Logger::Error) << "FAILED to create texture pool '" << ARBITRARY_SIZE_TEXTURE_POOL_NAME << "'! Error is '" << mTxMgr->GetError() << "'." << std::endl;
    MatUtils::Size2i snapshotGridTextureSize;
    snapshotGridTextureSize = {64*16/9, 64};
    if (!mTxMgr->CreateGridTexturePool(VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME, snapshotGridTextureSize, IM_DT_INT8, {8, 8}, 1))
        Logger::Log(Logger::Error) << "FAILED to create grid texture pool '" << VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME << "'! Error is '" << mTxMgr->GetError() << "'." << std::endl;
    else
    {
        RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
        mTxMgr->GetTexturePoolAttributes(VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME, tTxPoolAttrs);
        tTxPoolAttrs.bKeepAspectRatio = true;
        mTxMgr->SetTexturePoolAttributes(VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME, tTxPoolAttrs);
    }
    snapshotGridTextureSize = {DEFAULT_VIDEO_TRACK_HEIGHT*16/9, DEFAULT_VIDEO_TRACK_HEIGHT};
    if (!mTxMgr->CreateGridTexturePool(VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME, snapshotGridTextureSize, IM_DT_INT8, {8, 8}, 1))
        Logger::Log(Logger::Error) << "FAILED to create grid texture pool '" << VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME << "'! Error is '" << mTxMgr->GetError() << "'." << std::endl;
    snapshotGridTextureSize = {50*16/9, 50};
    if (!mTxMgr->CreateGridTexturePool(EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME, snapshotGridTextureSize, IM_DT_INT8, {8, 8}, 1))
        Logger::Log(Logger::Error) << "FAILED to create grid texture pool '" << EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME << "'! Error is '" << mTxMgr->GetError() << "'." << std::endl;

    mhMediaSettings = MediaCore::SharedSettings::CreateInstance();
    mhMediaSettings->SetHwaccelManager(MediaCore::HwaccelManager::GetDefaultInstance());
    // set default video settings
    mhMediaSettings->SetVideoOutWidth(1920);
    mhMediaSettings->SetVideoOutHeight(1080);
    mhMediaSettings->SetVideoOutFrameRate({25, 1});
    mhMediaSettings->SetVideoOutColorFormat(IM_CF_RGBA);
    mhMediaSettings->SetVideoOutDataType(IM_DT_INT8);
    // set default audio settings
    mhMediaSettings->SetAudioOutChannels(2);
    mhMediaSettings->SetAudioOutSampleRate(48000);
    auto pcmDataType = MatUtils::PcmFormat2ImDataType(mAudioRenderFormat);
    if (pcmDataType == IM_DT_UNDEFINED)
        throw std::runtime_error("UNSUPPORTED audio render format!");
    mhMediaSettings->SetAudioOutDataType(pcmDataType);
    mhMediaSettings->SetAudioOutIsPlanar(false);

    // preview use the same settings of timeline as default
    mhPreviewSettings = mhMediaSettings->Clone();

    mAudioRender = MediaCore::AudioRender::CreateInstance();
    if (!mAudioRender)
        throw std::runtime_error("FAILED to create AudioRender instance!");
    if (!mAudioRender->OpenDevice(mhPreviewSettings->AudioOutSampleRate(), mhPreviewSettings->AudioOutChannels(), mAudioRenderFormat, &mPcmStream))
        throw std::runtime_error("FAILED to open audio render device!");

    auto exec_path = ImGuiHelper::exec_path();
    m_BP_UI.Initialize();

    ConfigureDataLayer();

    mAudioAttribute.channel_data.clear();
    mAudioAttribute.channel_data.resize(mhMediaSettings->AudioOutChannels());
    memcpy(&mAudioAttribute.mBandCfg, &DEFAULT_BAND_CFG, sizeof(mAudioAttribute.mBandCfg));

    mhPreviewTx = mTxMgr->GetTextureFromPool(PREVIEW_TEXTURE_POOL_NAME);
    mRecordIter = mHistoryRecords.begin();
    mMediaPlayer = new MEC::MediaPlayer(mTxMgr);
}

TimeLine::~TimeLine()
{    
    if (mEncodingPreviewTexture) { ImGui::ImDestroyTexture(mEncodingPreviewTexture); mEncodingPreviewTexture = nullptr; }
    mAudioAttribute.channel_data.clear();

    if (mAudioAttribute.m_audio_vector_texture) { ImGui::ImDestroyTexture(mAudioAttribute.m_audio_vector_texture); mAudioAttribute.m_audio_vector_texture = nullptr; }
    
    m_BP_UI.Finalize();

    for (auto item : mEditingItems) delete item;
    for (auto track : m_Tracks) delete track;
    for (auto clip : m_Clips) delete clip;
    for (auto overlap : m_Overlaps)  delete overlap;
    for (auto item : media_items) delete item;

    if (mVideoTransitionInputFirstTexture) { ImGui::ImDestroyTexture(mVideoTransitionInputFirstTexture); mVideoTransitionInputFirstTexture = nullptr; }
    if (mVideoTransitionInputSecondTexture) { ImGui::ImDestroyTexture(mVideoTransitionInputSecondTexture); mVideoTransitionInputSecondTexture = nullptr; }
    if (mVideoTransitionOutputTexture) { ImGui::ImDestroyTexture(mVideoTransitionOutputTexture); mVideoTransitionOutputTexture = nullptr;  }

    if (mAudioRender)
    {
        MediaCore::AudioRender::ReleaseInstance(&mAudioRender);
        mAudioRender = nullptr;
    }

    if (mEncodingThread.joinable())
    {
        StopEncoding();
    }
    mEncoder = nullptr;

    mTxMgr->ReleaseTexturePool(VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME);
    mTxMgr->ReleaseTexturePool(VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME);
    mTxMgr->ReleaseTexturePool(EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME);
    mMtvReader = nullptr;
    mMtaReader = nullptr;

    if (mMediaPlayer) { delete mMediaPlayer;  mMediaPlayer = nullptr; }
}

bool TimeLine::AddMediaItem(MediaCore::MediaParser::Holder hParser)
{
    MediaItem* pNewMitem = new MediaItem(hParser, this);
    // check media is already in bank
    auto iter = std::find_if(media_items.begin(), media_items.end(), [pNewMitem] (const MediaItem* pExistMitem)
    {
        return  pNewMitem->mName == pExistMitem->mName &&
                pNewMitem->mPath == pExistMitem->mPath &&
                pNewMitem->mMediaType == pExistMitem->mMediaType;
    });
    if (iter != media_items.end())
    {
        // ignore duplicated MediaItem
        delete pNewMitem;
        return true;
    }
    if (!pNewMitem->Initialize())
    {
        delete pNewMitem;
        return false;
    }
    media_items.push_back(pNewMitem);
    return true;
}

bool TimeLine::CheckMediaItemImported(const std::string& strPath)
{
    // check media is already in bank
    auto iter = std::find_if(media_items.begin(), media_items.end(), [strPath] (const MediaItem* pExistMitem)
    {
        return  strPath == pExistMitem->mPath;
    });
    return iter != media_items.end();
}

int64_t TimeLine::AlignTime(int64_t time, int mode)
{
    const auto frameRate = mhMediaSettings->VideoOutFrameRate();
    const float frame_index_f = (double)time * frameRate.num / ((double)frameRate.den * 1000.0);
    const int64_t frame_index = mode==0 ? (int64_t)floor(frame_index_f) : mode==1 ? (int64_t)round(frame_index_f) : (int64_t)ceil(frame_index_f);
    time = round((double)frame_index * 1000 * frameRate.den / frameRate.num);
    return time;
}

int64_t TimeLine::AlignTimeToPrevFrame(int64_t time)
{
    const auto frameRate = mhMediaSettings->VideoOutFrameRate();
    int64_t frame_index = round((double)time * frameRate.num / ((double)frameRate.den * 1000.0));
    frame_index--;
    time = round((double)frame_index * 1000 * frameRate.den / frameRate.num);
    return time;
}

int64_t TimeLine::AlignTimeToNextFrame(int64_t time)
{
    const auto frameRate = mhMediaSettings->VideoOutFrameRate();
    int64_t frame_index = round((double)time * frameRate.num / ((double)frameRate.den * 1000.0));
    frame_index++;
    time = round((double)frame_index * 1000 * frameRate.den / frameRate.num);
    return time;
}

std::pair<int64_t, int64_t> TimeLine::AlignClipRange(const std::pair<int64_t, int64_t>& startAndLength)
{
    int64_t start = AlignTime(startAndLength.first);
    int64_t length = AlignTime(startAndLength.second);
    if (length == 0) length = AlignTimeToNextFrame(0);
    return {start, start+length};
}

void TimeLine::UpdateRange()
{
    int64_t start_min = INT64_MAX;
    int64_t end_max = INT64_MIN;
    for (auto clip : m_Clips)
    {
        if (clip->Start() < start_min)
            start_min = clip->Start();
        if (clip->End() > end_max)
            end_max = clip->End();
    }
    if (start_min < mStart)
        mStart = ImMax(start_min, (int64_t)0);
    if (end_max > mEnd)
    {
        mEnd = end_max + TIMELINE_OVER_LENGTH;
    }
}

void TimeLine::Update()
{
    UpdateRange();

    // update track
    for (auto track : m_Tracks)
    {
        track->Update();
    }
}

void TimeLine::Click(int index, int64_t time)
{
    bool click_empty_space = true;
    if (index >= 0 && index < m_Tracks.size())
    {
        auto current_track = m_Tracks[index];
        for (auto clip : current_track->m_Clips)
        {
            if (clip->IsInClipRange(time))
            {
                click_empty_space = false;
                break;
            }
        }
    }
    if (click_empty_space)
    {
        // clear selected
        for (auto clip : m_Clips)
        {
            clip->bSelected = false;
        }
    }
}

void TimeLine::DoubleClick(int index, int64_t time)
{
}

void TimeLine::SelectTrack(int index)
{
    if (index >= 0 && index < m_Tracks.size())
    {
        auto current_track = m_Tracks[index];
        for (auto track : m_Tracks)
        {
            if (track->mID != current_track->mID)
                track->mSelected = false;
            else
                track->mSelected = true;
        }
    }
}

int64_t TimeLine::DeleteTrack(int index, std::list<imgui_json::value>* pActionList)
{
    if (index < 0 || index >= m_Tracks.size())
        return -1;

    auto pTrack = m_Tracks[index];
    auto trackId = pTrack->mID;
    // save action json for UNDO operation
    if (pActionList)
    {
        // find the same media type track which the deleted track is after, return its ID for UNDO operation.
        int64_t afterTrackId, afterUiTrkId;
        if (index == 0)
        {
            afterTrackId = -2;
            afterUiTrkId = -2;
        }
        else
        {
            auto iter = m_Tracks.begin()+index-1;
            while ((*iter)->mType != pTrack->mType && iter != m_Tracks.begin())
                iter--;
            if ((*iter)->mType == pTrack->mType)
                afterTrackId = (*iter)->mID;
            else
                afterTrackId = -1;
            afterUiTrkId = m_Tracks[index-1]->mID;
        }
        imgui_json::value action;
        action["action"] = "REMOVE_TRACK";
        action["media_type"] = imgui_json::number(pTrack->mType);
        action["after_track_id"] = imgui_json::number(afterTrackId);
        action["after_ui_track_id"] = imgui_json::number(afterUiTrkId);
        imgui_json::value trackJson;
        pTrack->Save(trackJson);
        action["track_json"] = trackJson;
        std::vector<int64_t> relatedGroupIds;
        auto& containedClips = pTrack->m_Clips;
        if (!containedClips.empty())
        {
            imgui_json::value containedClipsJson;
            for (auto clip : containedClips)
            {
                containedClipsJson.push_back(clip->SaveAsJson());
                if (clip->mGroupID != -1)
                {
                    auto iter = std::find(relatedGroupIds.begin(), relatedGroupIds.end(), clip->mGroupID);
                    if (iter == relatedGroupIds.end())
                        relatedGroupIds.push_back(clip->mGroupID);
                }
            }
            action["contained_clips"] = containedClipsJson;
        }
        if (!relatedGroupIds.empty())
        {
            imgui_json::value relatedGroups;
            for (auto& group : m_Groups)
            {
                auto iter = std::find(relatedGroupIds.begin(), relatedGroupIds.end(), group.mID);
                if (iter != relatedGroupIds.end())
                {
                    imgui_json::value groupJson;
                    group.Save(groupJson);
                    relatedGroups.push_back(groupJson);
                }
            }
            action["related_groups"] = relatedGroups;
        }
        auto& containedOverlaps = pTrack->m_Overlaps;
        if (!containedOverlaps.empty())
        {
            imgui_json::value containedOverlapsJson;
            for (auto overlap : containedOverlaps)
            {
                imgui_json::value overlapJson;
                overlap->Save(overlapJson);
                containedOverlapsJson.push_back(overlapJson);
            }
            action["contained_overlaps"] = containedOverlapsJson;
        }
        pActionList->push_back(std::move(action));
    }

    // remove overlaps in this track
    std::vector<Overlap*> delOverlaps(pTrack->m_Overlaps);
    for (auto overlap : delOverlaps)
    {
        DeleteOverlap(overlap->mID);
    }
    // remove clips in this track
    std::vector<Clip*> delClips(pTrack->m_Clips);
    for (auto clip : delClips)
    {
        DeleteClip(clip->mID, nullptr);
    }
    // remove linked track info
    auto linked_track = FindTrackByID(pTrack->mLinkedTrack);
    if (linked_track)
    {
        linked_track->mLinkedTrack = -1;
    }
    // remove this track from array
    m_Tracks.erase(m_Tracks.begin() + index);
    delete pTrack;
    if (m_Tracks.size() == 0)
    {
        mStart = mEnd = 0;
        mCurrentTime = firstTime = lastTime = visibleTime = 0;
        mark_in = mark_out = -1;
    }

    RefreshPreview();
    return trackId;
}

int TimeLine::NewTrack(const std::string& name, uint32_t type, bool expand, int64_t id, int64_t afterUiTrkId, std::list<imgui_json::value>* pActionList)
{
    auto new_track = new MediaTrack(name, type, this);
    if (id != -1)
        new_track->mID = id;
    new_track->mPixPerMs = msPixelWidthTarget;
    new_track->mViewWndDur = visibleTime;
    new_track->mExpanded = expand;

    // find 'after_track_id' and 'after_ui_track_id' for UI action
    auto searchIter = m_Tracks.begin();
    if (afterUiTrkId == -1) // add the new track at the tail
    {
        if (m_Tracks.empty())
            afterUiTrkId = -1;
        else
            afterUiTrkId = m_Tracks.back()->mID;
        m_Tracks.push_back(new_track);
        searchIter = m_Tracks.end()-1;
    }
    else  // insert the new track after the specifed track with id = afterUiTrkId
    {
        auto iter = std::find_if(m_Tracks.begin(), m_Tracks.end(), [afterUiTrkId] (auto& t) {
            return t->mID == afterUiTrkId;
        });
        if (iter != m_Tracks.end())
        {
            iter++;
            searchIter = m_Tracks.insert(iter, new_track);
        }
        else
        {
            Logger::Log(Logger::WARN) << "CANNOT find the specifed track with id eqaul to 'afterUiTrkId'(" << afterUiTrkId << ")!" << std::endl;
            if (m_Tracks.empty())
                afterUiTrkId = -1;
            else
                afterUiTrkId = m_Tracks.back()->mID;
            m_Tracks.push_back(new_track);
            searchIter = m_Tracks.end()-1;
        }
    }
    Update();

    if (pActionList)
    {
        int64_t afterId = -1;
        if (searchIter != m_Tracks.begin())
        {
            searchIter--;
            while ((*searchIter)->mType != type && searchIter != m_Tracks.begin())
                searchIter--;
            if ((*searchIter)->mType == type)
                afterId = (*searchIter)->mID;
        }
        imgui_json::value action;
        action["action"] = "ADD_TRACK";
        action["media_type"] = imgui_json::number(type);
        imgui_json::value trackJson;
        new_track->Save(trackJson);
        action["track_json"] = trackJson;
        // 'after_track_id' is the argument required by DataLayer for API MultiTrackVideoReader::AddTrack()
        action["after_track_id"] = imgui_json::number(afterId);
        // 'after_ui_track_id' is saved for UNDO/REDO operation to retore the deleted track to the original position
        action["after_ui_track_id"] = imgui_json::number(afterUiTrkId);
        pActionList->push_back(std::move(action));
    }
    return m_Tracks.size() - 1;
}

bool TimeLine::RestoreTrack(imgui_json::value& action)
{
    if (!action.contains("action") || action["action"].get<imgui_json::string>() != "REMOVE_TRACK")
    {
        Logger::Log(Logger::WARN) << "WRONG ARGUMENT! Restore track must take a 'REMOVE_TRACK' action as input." << std::endl;
        return false;
    }
    std::vector<int64_t> restoredGroupIds;
    int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
    // restore auto deleted groups
    if (action.contains("related_groups"))
    {
        auto& relatedGroups = action["related_groups"].get<imgui_json::array>();
        for (auto& groupJson : relatedGroups)
        {
            int64_t gid = groupJson["ID"].get<imgui_json::number>();
            auto iter = std::find_if(m_Groups.begin(), m_Groups.end(), [gid] (auto& g) {
                return g.mID == gid;
            });
            if (iter == m_Groups.end())
            {
                ClipGroup grp(this);
                grp.Load(groupJson);
                restoredGroupIds.push_back(grp.mID);
                m_Groups.push_back(grp);
            }
        }
    }
    // restore the contained clips
    if (action.contains("contained_clips"))
    {
        auto& containedClips = action["contained_clips"].get<imgui_json::array>();
        for (auto& clipJson : containedClips)
        {
            Clip* c = nullptr;
            uint32_t type = clipJson["Type"].get<imgui_json::number>();
            if (IS_VIDEO(type))
                c = VideoClip::CreateInstanceFromJson(clipJson, this);
            else if (IS_AUDIO(type))
                c = AudioClip::CreateInstanceFromJson(clipJson, this);
            else if (IS_TEXT(type))
                c = TextClip::CreateInstanceFromJson(clipJson, this);
            else
            {
                Logger::Log(Logger::WARN) << "FAILED to restore clip (id=";
                if (clipJson.contains("ID"))
                    Logger::Log(Logger::WARN) << (int64_t)(clipJson["ID"].get<imgui_json::number>());
                else
                    Logger::Log(Logger::WARN) << "unknown";
                Logger::Log(Logger::WARN) << ")! Unsupported media type value " << type << "." << std::endl;
            }
            if (c)
            {
                m_Clips.push_back(c);
                // restore group
                if (c->mGroupID != -1)
                {
                    int64_t gid = c->mGroupID;
                    auto iter = std::find_if(m_Groups.begin(), m_Groups.end(), [gid] (auto& g) {
                        return g.mID == gid;
                    });
                    if (iter != m_Groups.end())
                    {
                        auto iter2 = std::find(iter->m_Grouped_Clips.begin(), iter->m_Grouped_Clips.end(), c->mID);
                        if (iter2 == iter->m_Grouped_Clips.end())
                            iter->m_Grouped_Clips.push_back(c->mID);
                    }
                    else
                    {
                        Logger::Log(Logger::WARN) << "Warning during restoring track(id=" << trackId << "): Cannot add clip(id="
                            << c->mID << ") into ClipGroup(id=" << gid << "), the group does NOT EXIST!" << std::endl;
                    }
                }
            }
        }
        // restore group id for clips
        for (auto gid : restoredGroupIds)
        {
            auto iter = std::find_if(m_Groups.begin(), m_Groups.end(), [gid] (auto& g) {
                return g.mID == gid;
            });
            if (iter != m_Groups.end())
            {
                for (auto cid : iter->m_Grouped_Clips)
                {
                    auto pClip = FindClipByID(cid);
                    if (pClip)
                        pClip->mGroupID = gid;
                }
            }
        }
    }
    // restore the contained overlaps
    if (action.contains("contained_overlaps"))
    {
        auto& containedOverlaps = action["contained_overlaps"].get<imgui_json::array>();
        for (auto& overlapJson : containedOverlaps)
        {
            Overlap* o = Overlap::Load(overlapJson, this);
            if (o)
                m_Overlaps.push_back(o);
        }
    }
    // restore the removed track
    MediaTrack* t = MediaTrack::Load(action["track_json"], this);
    int64_t afterUiTrkId = -1;
    if (action.contains("after_ui_track_id"))
        afterUiTrkId = action["after_ui_track_id"].get<imgui_json::number>();
    // insert the track into the specified position in the track list
    auto searchIter = m_Tracks.begin();
    if (afterUiTrkId == -1) // add the new track at the tail
    {
        m_Tracks.push_back(t);
        searchIter = m_Tracks.end()-1;
    }
    else  // insert the new track after the specifed track with id = afterUiTrkId
    {
        auto iter = m_Tracks.begin();
        if (afterUiTrkId != -2)
        {
            iter = std::find_if(m_Tracks.begin(), m_Tracks.end(), [afterUiTrkId] (auto& t) {
                    return t->mID == afterUiTrkId;
                });
            if (iter != m_Tracks.end())
                iter++;
            else
                Logger::Log(Logger::WARN) << "Warning during restoring track(id=" << trackId
                    << "): CANNOT find the specifed track with id eqaul to 'afterUiTrkId'(" << afterUiTrkId << ")!" << std::endl;
        }
        searchIter = m_Tracks.insert(iter, t);
    }
    int64_t afterTrackId = -2;
    if (searchIter != m_Tracks.begin())
    {
        searchIter--;
        while ((*searchIter)->mType != t->mType && searchIter != m_Tracks.begin())
            searchIter--;
        if ((*searchIter)->mType == t->mType)
            afterTrackId = (*searchIter)->mID;
    }
    int64_t afterTrackId2 = -1;
    if (action.contains("after_track_id"))
        afterTrackId2 = action["after_track_id"].get<imgui_json::number>();
    if (afterTrackId != afterTrackId2)
        Logger::Log(Logger::WARN) << "Warning during restoring track(id=" << trackId
            << "): Actual 'afterTrackId'(" << afterTrackId << ") is NOT EQUAL to the value stored in json ("
            << afterTrackId2 << ")!" << std::endl;

    // restore DataLayer stat
    if (IS_VIDEO(t->mType))
    {
        MediaCore::VideoTrack::Holder hVidTrk = mMtvReader->AddTrack(t->mID, afterTrackId);
        for (auto c : t->m_Clips)
        {
            if (IS_DUMMY(c->mType))
                continue;
            MediaCore::VideoClip::Holder hVidClip;
            if (IS_IMAGE(c->mType))
                hVidClip = hVidTrk->AddImageClip(c->mID, c->mMediaParser, c->Start(), c->Length());
            else
                hVidClip = hVidTrk->AddVideoClip(c->mID, c->mMediaParser, c->Start(), c->End(), c->StartOffset(), c->EndOffset(), mCurrentTime-c->Start());
            VideoClip* pUiVClip = dynamic_cast<VideoClip*>(c);
            pUiVClip->SetDataLayer(hVidClip, true);
        }
        RefreshPreview();
    }
    else if (IS_AUDIO(t->mType))
    {
        MediaCore::AudioTrack::Holder hAudTrk = mMtaReader->AddTrack(t->mID);
        for (auto c : t->m_Clips)
        {
            if (IS_DUMMY(c->mType))
                continue;
            MediaCore::AudioClip::Holder hAudClip = hAudTrk->AddNewClip(
                c->mID, c->mMediaParser,
                c->Start(), c->End(), c->StartOffset(), c->EndOffset());
            AudioClip* pUiAClip = dynamic_cast<AudioClip*>(c);
            pUiAClip->SetDataLayer(hAudClip, true);
        }
        // audio attribute
        auto aeFilter = hAudTrk->GetAudioEffectFilter();
        // gain
        auto volParams = aeFilter->GetVolumeParams();
        volParams.volume = t->mAudioTrackAttribute.mAudioGain;
        aeFilter->SetVolumeParams(&volParams);
        mMtaReader->UpdateDuration();
        mMtaReader->SeekTo(mCurrentTime);
    }

    SyncDataLayer(true);
    return true;
}

void TimeLine::MovingTrack(int index, int dst_index, std::list<imgui_json::value>* pActionList)
{
    if (m_Tracks.size() < 2 || index < 0 || index >= m_Tracks.size())
        return;
    auto iter = m_Tracks.begin() + index;
    if (dst_index == -2)
        dst_index = m_Tracks.size()-1;
    if (dst_index < 0 || dst_index >= m_Tracks.size())
        return;
    auto iter_dst = m_Tracks.begin() + dst_index;
    if (iter == iter_dst)
        return;

    if (pActionList && IS_VIDEO((*iter)->mType))
    {
        imgui_json::value action;
        action["action"] = "MOVE_TRACK";
        action["media_type"] = imgui_json::number((*iter)->mType);
        action["org_index"] = imgui_json::number(index);
        action["dst_index"] = imgui_json::number(dst_index);
        action["track_id1"] = imgui_json::number((*iter)->mID);

        bool isInsertAfterDst = iter < iter_dst ? true : false;
        int64_t insertAfterId = -1;
        auto iter_insert = iter_dst;
        if (isInsertAfterDst)
        {
            while (iter_insert != m_Tracks.begin() && !IS_VIDEO((*iter_insert)->mType))
                iter_insert--;
            if (IS_VIDEO((*iter_insert)->mType))
                insertAfterId = (*iter_insert)->mID;
            else
                insertAfterId = -2;
        }
        else
        {
            if (iter_insert == m_Tracks.begin())
                insertAfterId = -2;
            else
            {
                iter_insert--;
                while (iter_insert != m_Tracks.begin() && !IS_VIDEO((*iter_insert)->mType))
                    iter_insert--;
                if (IS_VIDEO((*iter_insert)->mType))
                    insertAfterId = (*iter_insert)->mID;
                else
                    insertAfterId = -2;
            }
        }
        if (iter_insert != iter)
        {
            auto iter_next = iter_insert;
            if (insertAfterId >= 0)
                iter_next++;
            while (iter_next != m_Tracks.end() && !IS_VIDEO((*iter_next)->mType))
                iter_next++;
            if (iter_next != iter)
                action["track_id2"] = imgui_json::number(insertAfterId);
        }
        pActionList->push_back(std::move(action));
    }

    // do we need change other type of media?
    auto temp = *iter;
    if (iter > iter_dst)
    {
        auto iter_prev = iter-1;
        do {
            *iter = *iter_prev;
            if (iter_prev != iter_dst)
            {
                iter = iter_prev;
                iter_prev--;
            }
            else
                break;
        } while (true);
        *iter_dst = temp;
    }
    else
    {
        auto iter_next = iter+1;
        do {
            *iter = *iter_next;
            if (iter_next != iter_dst)
            {
                iter = iter_next;
                iter_next++;
            }
            else
                break;
        } while (true);
        *iter_dst = temp;
    }
}

void TimeLine::MovingClip(int64_t id, int from_track_index, int to_track_index)
{
    if (from_track_index < 0 || to_track_index < 0 ||
        from_track_index >= m_Tracks.size() || to_track_index >= m_Tracks.size())
        return;
    auto track = m_Tracks[from_track_index];
    auto dst_track = m_Tracks[to_track_index];
    if (!track || !dst_track)
    {
        return;
    }
    // remove clip from source track
    for (auto iter = track->m_Clips.begin(); iter != track->m_Clips.end();)
    {
        if ((*iter)->mID == id)
        {
            iter = track->m_Clips.erase(iter);
        }
        else
            ++iter;
    }
    // find clip in timeline
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [id](const Clip* clip) {
        return clip->mID == id;
    });
    if (iter != m_Clips.end())
    {
        auto clip = *iter;
        if (IS_TEXT(clip->mType))
        {
            TextClip * tclip = dynamic_cast<TextClip *>(clip);
            // need remove from source track holder
            if (track->mMttReader && tclip->mhDataLayerClip)
            {
                track->mMttReader->DeleteClip(tclip->mhDataLayerClip);
            }
            // and add into dst track holder
            tclip->CreateDataLayer(dst_track);
        }

        dst_track->InsertClip(clip, clip->Start());
    }
}

bool TimeLine::DeleteClip(int64_t id, std::list<imgui_json::value>* pActionList)
{
    auto track = FindTrackByClipID(id);
    if (!track || track->mLocked)
        return false;
    track->DeleteClip(id);

    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [id](const Clip* clip) {
        return clip->mID == id;
    });
    if (iter != m_Clips.end())
    {
        auto clip = *iter;
        m_Clips.erase(iter);

        auto found = FindEditingItem(EDITING_CLIP, clip->mID);
        if (found != -1)
        {
            auto iter = mEditingItems.begin() + found;
            auto item = *iter;
            mEditingItems.erase(iter);
            delete item;
            if (mSelectedItem == found || mSelectedItem >= mEditingItems.size())
            {
                mSelectedItem = -1;
            }
            if (m_CallBacks.EditingClip)
            {
                m_CallBacks.EditingClip(0, clip);
            }
        }

        DeleteClipFromGroup(clip, clip->mGroupID, pActionList);

        if (pActionList)
        {
            imgui_json::value action;
            action["action"] = "REMOVE_CLIP";
            action["media_type"] = imgui_json::number(clip->mType);
            action["from_track_id"] = imgui_json::number(track->mID);
            action["clip_json"] = clip->SaveAsJson();
            pActionList->push_back(std::move(action));
        }
        delete clip;
    }
    return true;
}

void TimeLine::DeleteOverlap(int64_t id)
{
    for (auto iter = m_Overlaps.begin(); iter != m_Overlaps.end();)
    {
        if ((*iter)->mID == id)
        {
            Overlap * overlap = *iter;
            iter = m_Overlaps.erase(iter);
            auto found = FindEditingItem(EDITING_TRANSITION, overlap->mID);
            if (found != -1)
            {
                auto iter = mEditingItems.begin() + found;
                auto item = *iter;
                mEditingItems.erase(iter);
                delete item;
                if (mSelectedItem == found || mSelectedItem >= mEditingItems.size())
                {
                    mSelectedItem = -1;
                }
                if (m_CallBacks.EditingOverlap)
                {
                    m_CallBacks.EditingOverlap(overlap->mType, overlap);
                }
            }
            delete overlap;
        }
        else
            ++ iter;
    }
}

void TimeLine::UpdateCurrent()
{
    if (bSeeking)
        return;
    if (!mIsPreviewForward)
    {
        if (mCurrentTime < firstTime + visibleTime / 2)
        {
            //firstTime = currentTime - visibleTime / 2;
            auto step = (firstTime + visibleTime / 2 - mCurrentTime) / 4;
            if (step <= visibleTime / 32)
                firstTime = mCurrentTime - visibleTime / 2;
            else
                firstTime -= step;
        }
        else if (mCurrentTime > firstTime + visibleTime)
        {
            firstTime = mCurrentTime - visibleTime;
        }
    }
    else
    {
        if (mEnd - mCurrentTime < visibleTime / 2)
        {
            firstTime = mEnd - visibleTime;
        }
        else if (mCurrentTime > firstTime + visibleTime / 2)
        {
            //firstTime = currentTime - visibleTime / 2;
            auto step = (mCurrentTime - firstTime - visibleTime / 2) / 4;
            if (step <= visibleTime / 32)
                firstTime = mCurrentTime - visibleTime / 2;
            else
                firstTime += step;
        }
        else if (mCurrentTime < firstTime)
        {
            firstTime = mCurrentTime;
        }
    }
    if (firstTime < 0) firstTime = 0;
}

void TimeLine::RefreshPreview(bool updateDuration)
{
    mMtvReader->Refresh(updateDuration);
    mIsPreviewNeedUpdate = true;
}

void TimeLine::RefreshTrackView(const std::unordered_set<int64_t>& trackIds)
{
    mMtvReader->RefreshTrackView(trackIds);
    mIsPreviewNeedUpdate = true;
}

std::vector<MediaCore::CorrelativeFrame> TimeLine::GetPreviewFrame(bool blocking)
{
    int64_t auddataPos, previewPos;
    if (!bSeeking)
    {
        if (mPcmStream.GetTimestampMs(auddataPos))
        {
            int64_t bufferedDur = mMtaReader->SizeToDuration(mAudioRender->GetBufferedDataSize());
            previewPos = mIsPreviewForward ? auddataPos-bufferedDur : auddataPos+bufferedDur;
            if (previewPos < 0) previewPos = 0;
        }
        else
        {
            int64_t elapsedTime = (int64_t)(std::chrono::duration_cast<std::chrono::duration<double>>((PlayerClock::now()-mPlayTriggerTp)).count()*1000);
            previewPos = mIsPreviewPlaying ? (mIsPreviewForward ? mPreviewResumePos+elapsedTime : mPreviewResumePos-elapsedTime) : mPreviewResumePos;
            if (previewPos < 0) previewPos = 0;
        }
    }
    else
    {
        previewPos = mCurrentTime;
    }

    if (mIsPreviewPlaying)
    {
        bool playEof = false;
        bool needSeek = false;
        int64_t dur = ValidDuration();
        if (!mIsPreviewForward && previewPos <= 0)
        {
            if (bLoop)
            {
                previewPos = dur;
                needSeek = true;
            }
            else
            {
                previewPos = 0;
                playEof = true;
            }
        }
        else if (mIsPreviewForward && previewPos >= dur)
        {
            if (bLoop)
            {
                previewPos = 0;
                needSeek = true;
            }
            else
            {
                previewPos = dur;
                playEof = true;
            }
        }
        if (needSeek)
            Seek(previewPos);
        else
        {
            mFrameIndex = mMtvReader->MillsecToFrameIndex(previewPos);
            mCurrentTime = mMtvReader->FrameIndexToMillsec(mFrameIndex);
        }
        if (playEof)
        {
            mIsPreviewPlaying = false;
            mPreviewResumePos = mCurrentTime;
            if (mAudioRender)
                mAudioRender->Pause();
            for (auto& audio : mAudioAttribute.channel_data) audio.m_decibel = 0;
            for (auto track : m_Tracks)
            {
                if (IS_AUDIO(track->mType))
                {
                    for (auto& track_audio : track->mAudioTrackAttribute.channel_data)
                        track_audio.m_decibel = 0;
                }
            }
        }
    }

    std::vector<MediaCore::CorrelativeFrame> frames;
    const bool needPreciseFrame = !(bSeeking || mIsPreviewPlaying);
    mMtvReader->ReadVideoFrameByIdxEx(mFrameIndex, frames, !blocking, needPreciseFrame);
    mCurrentTime = mMtvReader->FrameIndexToMillsec(mFrameIndex);
    if (mIsPreviewPlaying && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) UpdateCurrent();
    return frames;
}

bool TimeLine::UpdatePreviewTexture(bool blocking)
{
    bool bTxUpdated = false;
    maCurrFrames = GetPreviewFrame(blocking);
    if (maCurrFrames.empty() || maCurrFrames[0].frame.empty())
        return bTxUpdated;
    const auto& mainPreviewMat = maCurrFrames[0].frame;
    const auto i64Timestamp = (int64_t)(mainPreviewMat.time_stamp*1000);
    if (mIsPreviewNeedUpdate || mLastFrameTime == -1 || mLastFrameTime != i64Timestamp || !mhPreviewTx->IsValid())
    {
        mPreviewMat = mainPreviewMat;
        mhPreviewTx->RenderMatToTexture(mainPreviewMat);
        mLastFrameTime = i64Timestamp;
        mIsPreviewNeedUpdate = false;
        bTxUpdated = true;
    }
    return bTxUpdated;
}

float TimeLine::GetAudioLevel(int channel)
{
    if (channel < mAudioAttribute.channel_data.size())
        return mAudioAttribute.channel_data[channel].m_decibel;
    return 0;
}

void TimeLine::SetAudioLevel(int channel, float level)
{
    if (channel < mAudioAttribute.channel_data.size())
        mAudioAttribute.channel_data[channel].m_decibel = level;
}

int TimeLine::GetSelectedClipCount()
{
    int count = 0;
    for (auto clip : m_Clips)
    {
        if (clip->bSelected) count++;
    }
    return count;
}

void TimeLine::Play(bool play, bool forward)
{
    bool needSeekAudio = false;
    if (mIsStepMode)
    {
        mIsStepMode = false;
        if (mAudioRender)
            needSeekAudio = true;
    }
    if (forward != mIsPreviewForward)
    {
        if (mAudioRender)
        {
            mAudioRender->Pause();
            mAudioRender->Flush();
        }
        mMtvReader->SetDirection(forward, mCurrentTime);
        mMtaReader->SetDirection(forward, mCurrentTime);
        mIsPreviewForward = forward;
        mPlayTriggerTp = PlayerClock::now();
        mPreviewResumePos = mCurrentTime;
        if (mAudioRender)
        {
            mAudioRender->Resume();
        }
    }
    if (needSeekAudio && mAudioRender)
    {
        mAudioRender->Flush();
        mMtaReader->SeekTo(mCurrentTime);
    }
    if (play != mIsPreviewPlaying)
    {
        mIsPreviewPlaying = play;
        if (play)
        {
            mPlayTriggerTp = PlayerClock::now();
            if (mAudioRender)
                mAudioRender->Resume();
        }
        else
        {
            mLastFrameTime = -1;
            mPreviewResumePos = mCurrentTime;
            if (mAudioRender)
                mAudioRender->Pause();
            for (int i = 0; i < mAudioAttribute.channel_data.size(); i++) SetAudioLevel(i, 0);
            for (auto track : m_Tracks)
            {
                for (int i = 0; i < track->mAudioTrackAttribute.channel_data.size(); i++) track->SetAudioLevel(i, 0);
            }
        }
    }
}

void TimeLine::Seek(int64_t msPos, bool enterSeekingState)
{
    msPos = AlignTime(msPos);
    if (enterSeekingState && !bSeeking)
    {
        // begin to seek
        bSeeking = true;
        if (mAudioRender && !mIsPreviewPlaying)
            mAudioRender->Resume();
    }

    auto targetFrameIndex = mMtvReader->MillsecToFrameIndex(msPos);
    if (targetFrameIndex == mFrameIndex)
        return;
    mFrameIndex = targetFrameIndex;
    mCurrentTime = mMtvReader->FrameIndexToMillsec(mFrameIndex);

    if (bSeeking)
    {
        mPlayTriggerTp = PlayerClock::now();
        mMtaReader->SeekTo(msPos, true);
        mMtvReader->ConsecutiveSeek(msPos);
    }
    else
    {
        mPlayTriggerTp = PlayerClock::now();
        mMtaReader->SeekTo(msPos, false);
        mMtvReader->SeekTo(msPos);
        mAudioRender->Flush();
        mPreviewResumePos = mCurrentTime;
    }
}

void TimeLine::StopSeek()
{
    if (bSeeking)
    {
        bSeeking = false;
        if (mMtaReader)
            mMtaReader->SeekTo(mCurrentTime, false);
        if (mAudioRender)
        {
            if (!mIsPreviewPlaying)
                mAudioRender->Pause();
            mAudioRender->Flush();
        }
        if (mMtvReader)
            mMtvReader->StopConsecutiveSeek();
        mPlayTriggerTp = PlayerClock::now();
        mPreviewResumePos = mCurrentTime;
    }
    if (!mIsPreviewPlaying)
    {
        for (int i = 0; i < mAudioAttribute.channel_data.size(); i++) SetAudioLevel(i, 0);
        for (auto track : m_Tracks)
        {
            for (int i = 0; i < track->mAudioTrackAttribute.channel_data.size(); i++) track->SetAudioLevel(i, 0);
        }
    }
}

void TimeLine::Step(bool forward)
{
    if (mIsPreviewPlaying)
    {
        mIsPreviewPlaying = false;
        if (mAudioRender)
            mAudioRender->Pause();
    }
    if (!mIsStepMode)
    {
        mIsStepMode = true;
        if (mAudioRender)
            mAudioRender->Flush();
    }
    if (forward != mIsPreviewForward)
    {
        mMtvReader->SetDirection(forward);
        mMtaReader->SetDirection(forward);
        mIsPreviewForward = forward;
    }
    ImGui::ImMat vmat;
    mMtvReader->ReadNextVideoFrame(vmat);
    mFrameIndex = vmat.index_count;
    mCurrentTime = mMtvReader->FrameIndexToMillsec(mFrameIndex);
    mPreviewResumePos = mCurrentTime;

    UpdateCurrent();
}

void TimeLine::Loop(bool loop)
{
    bLoop = loop;
}

void TimeLine::ToStart()
{
    Seek(0);
}

void TimeLine::ToEnd()
{
    int64_t dur = ValidDuration();
    Seek(dur);
}

int64_t TimeLine::ValidDuration()
{
    int64_t vdur = mMtvReader->Duration();
    int64_t adur = mMtaReader->Duration();
    int64_t media_range = vdur > adur ? vdur : adur;
    if (!mEncodingInRange)
    {
        mEncodingStart = 0;
        mEncodingEnd = media_range;
        return media_range;
    }
    else if (mark_out == -1 || mark_in == -1 || mark_out <= mark_in)
    {
        mEncodingStart = 0;
        mEncodingEnd = media_range;
        return media_range;
    }
    else
    {
        auto _mark_in = mark_in;
        auto _mark_out = mark_out;
        if (mark_out > media_range) _mark_out = media_range;
        if (_mark_out <= _mark_in)
        {
            mEncodingStart = 0;
            mEncodingEnd = 0;
            return 0;
        }
        mEncodingStart = _mark_in;
        mEncodingEnd = _mark_out;
        return _mark_out - _mark_in;
    }
}

MediaItem* TimeLine::FindMediaItemByName(std::string name)
{
    auto iter = std::find_if(media_items.begin(), media_items.end(), [name](const MediaItem* item)
    {
        return item->mName.compare(name) == 0;
    });
    if (iter != media_items.end())
        return *iter;
    return nullptr;
}

MediaItem* TimeLine::FindMediaItemByID(int64_t id)
{
    auto iter = std::find_if(media_items.begin(), media_items.end(), [id](const MediaItem* item)
    {
        return item->mID == id;
    });
    if (iter != media_items.end())
        return *iter;
    return nullptr;
}

void TimeLine::SortMediaItemByID()
{
    std::sort(media_items.begin(), media_items.end(), [](const MediaItem* lit, const MediaItem* rit)
    {
        return lit->mID < rit->mID;
    });
}

void TimeLine::SortMediaItemByName()
{
    std::sort(media_items.begin(), media_items.end(), [](const MediaItem* lit, const MediaItem* rit)
    {
        return lit->mName < rit->mName;
    });
}

void TimeLine::SortMediaItemByType()
{
    std::sort(media_items.begin(), media_items.end(), [](const MediaItem* lit, const MediaItem* rit)
    {
        return lit->mMediaType < rit->mMediaType;
    });
}

void TimeLine::FilterMediaItemByType(uint32_t mediaType)
{
    for (auto media_item : media_items)
    {
        if (media_item->mMediaType == mediaType)
            filter_media_items.push_back(media_item);
    }
}

MediaTrack * TimeLine::FindTrackByID(int64_t id)
{
    auto iter = std::find_if(m_Tracks.begin(), m_Tracks.end(), [id](const MediaTrack* track)
    {
        return track->mID == id;
    });
    if (iter != m_Tracks.end())
        return *iter;
    return nullptr;
}

MediaTrack * TimeLine::FindTrackByClipID(int64_t id)
{
    auto iter = std::find_if(m_Tracks.begin(), m_Tracks.end(), [id](const MediaTrack* track)
    {
        auto iter_clip = std::find_if(track->m_Clips.begin(), track->m_Clips.end(), [id](const Clip* clip)
        {
            return clip->mID == id;
        });
        return iter_clip != track->m_Clips.end();
    });
    if (iter != m_Tracks.end())
        return *iter;
    return nullptr;
}

MediaTrack * TimeLine::FindTrackByName(std::string name)
{
    auto iter = std::find_if(m_Tracks.begin(), m_Tracks.end(), [name](const MediaTrack* track)
    {
        return track->mName == name;
    });
    if (iter != m_Tracks.end())
        return *iter;
    return nullptr;
}

MediaTrack * TimeLine::FindEmptyTrackByType(uint32_t type)
{
    auto iter = std::find_if(m_Tracks.begin(), m_Tracks.end(), [type](const MediaTrack* track)
    {
        return track->m_Clips.size() == 0 && IS_SAME_TYPE(track->mType, type);
    });
    if (iter != m_Tracks.end())
        return *iter;
    return nullptr;
}

int TimeLine::FindTrackIndexByClipID(int64_t id)
{
    int track_found = -1;
    auto track = FindTrackByClipID(id);
    for (int i = 0; i < m_Tracks.size(); i++)
    {
        if (track && m_Tracks[i]->mID == track->mID)
        {
            track_found = i;
            break;
        }
    }
    return track_found;
}

Clip * TimeLine::FindClipByID(int64_t id)
{
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [id](const Clip* clip)
    {
        return clip->mID == id;
    });
    if (iter != m_Clips.end())
        return *iter;
    return nullptr;
}

Overlap * TimeLine::FindOverlapByID(int64_t id)
{
    auto iter = std::find_if(m_Overlaps.begin(), m_Overlaps.end(), [id](const Overlap* overlap)
    {
        return overlap->mID == id;
    });
    if (iter != m_Overlaps.end())
        return *iter;
    return nullptr;
}

Overlap * TimeLine::FindEditingOverlap()
{
    auto iter = std::find_if(m_Overlaps.begin(), m_Overlaps.end(), [](const Overlap* overlap)
    {
        return overlap->bEditing;
    });
    if (iter != m_Overlaps.end())
        return *iter;
    return nullptr;
}

int64_t TimeLine::NextClipStart(Clip * clip)
{
    int64_t next_start = -1;
    if (!clip) return next_start;
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [clip](const Clip* _clip)
    {
        return _clip->Start() >= clip->End();
    });
    if (iter != m_Clips.end())
        next_start = (*iter)->Start();
    return next_start;
}

int64_t TimeLine::NextClipStart(int64_t pos)
{
    int64_t next_start = -1;
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [pos](const Clip* _clip)
    {
        return _clip->Start() > pos;
    });
    if (iter != m_Clips.end())
        next_start = (*iter)->Start();
    return next_start;
}

int TimeLine::GetTrackCount(uint32_t type)
{
    return std::count_if(m_Tracks.begin(), m_Tracks.end(), [type](const MediaTrack * track){
        return track->mType == type;
    });
}

int TimeLine::GetEmptyTrackCount()
{
    return std::count_if(m_Tracks.begin(), m_Tracks.end(), [&](const MediaTrack * track){
        return track->m_Clips.size() == 0;
    });
}

int64_t TimeLine::NewGroup(Clip * clip, int64_t id, ImU32 color, std::list<imgui_json::value>* pActionList)
{
    ClipGroup new_group(this);
    if (id != -1)
        new_group.mID = id;
    else
        id = new_group.mID;
    if (color != 0) new_group.mColor = color;
    auto iter = std::find_if(m_Groups.begin(), m_Groups.end(), [id] (auto& g) {
        return g.mID == id;
    });
    if (iter == m_Groups.end())
    {
        m_Groups.push_back(new_group);
        iter = m_Groups.end()-1;
    }
    else
        iter->mColor = new_group.mColor;

    if (clip)
    {
        DeleteClipFromGroup(clip, clip->mGroupID);
        iter->m_Grouped_Clips.push_back(clip->mID);
        clip->mGroupID = id;
    }

    if (pActionList)
    {
        imgui_json::value action1;
        action1["action"] = "ADD_GROUP";
        imgui_json::value groupJson;
        new_group.Save(groupJson);
        action1["group_json"] = groupJson;
        pActionList->push_back(std::move(action1));
        if (clip)
        {
            imgui_json::value action2;
            action2["action"] = "ADD_CLIP_INTO_GROUP";
            action2["clip_id"] = imgui_json::number(clip->mID);
            action2["group_id"] = imgui_json::number(id);
            pActionList->push_back(std::move(action2));
        }
    }
    return id;
}

int64_t TimeLine::RestoreGroup(const imgui_json::value& groupJson)
{
    int64_t gid = NewGroup(nullptr, groupJson["ID"].get<imgui_json::number>(), groupJson["Color"].get<imgui_json::number>());
    if (gid != -1)
    {
        auto giter = std::find_if(m_Groups.begin(), m_Groups.end(), [gid] (auto& g) {
            return g.mID == gid;
        });
        assert(giter != m_Groups.end());
        auto citer = giter->m_Grouped_Clips.begin();
        while (citer != giter->m_Grouped_Clips.end())
        {
            auto cid = *citer;
            auto c = FindClipByID(cid);
            if (c)
            {
                c->mGroupID = gid;
                citer++;
            }
            else
            {
                citer = giter->m_Grouped_Clips.erase(citer);
            }
        }
    }
    else
    {
        Logger::Log(Logger::WARN) << "FAILED to restore clip group from JSON: " << groupJson.dump() << std::endl;
    }
    return gid;
}

void TimeLine::AddClipIntoGroup(Clip * clip, int64_t group_id, std::list<imgui_json::value>* pActionList)
{
    if (!clip || group_id == -1 || clip->mGroupID == group_id)
        return;
    // remove clip if clip is already in some group
    DeleteClipFromGroup(clip, clip->mGroupID, pActionList);
    for (auto & group : m_Groups)
    {
        if (group_id == group.mID)
        {
            group.m_Grouped_Clips.push_back(clip->mID);
            clip->mGroupID = group_id;
            if (pActionList)
            {
                imgui_json::value action;
                action["action"] = "ADD_CLIP_INTO_GROUP";
                action["clip_id"] = imgui_json::number(clip->mID);
                action["group_id"] = imgui_json::number(group_id);
                pActionList->push_back(std::move(action));
            }
        }
    }
}

void TimeLine::DeleteClipFromGroup(Clip *clip, int64_t group_id, std::list<imgui_json::value>* pActionList)
{
    if (group_id == -1 || !clip)
        return;
    for (auto iter = m_Groups.begin(); iter != m_Groups.end();)
    {
        bool need_erase = false;
        if ((*iter).mID == group_id)
        {
            auto clip_iter = std::find_if((*iter).m_Grouped_Clips.begin(), (*iter).m_Grouped_Clips.end(), [clip](const int64_t& id) 
            {
                return clip->mID == id;
            });
            if (clip_iter != (*iter).m_Grouped_Clips.end())
            {
                (*iter).m_Grouped_Clips.erase(clip_iter);
                if ((*iter).m_Grouped_Clips.size() <= 0)
                    need_erase = true;
                if (pActionList)
                {
                    imgui_json::value action;
                    action["action"] = "DELETE_CLIP_FROM_GROUP";
                    action["clip_id"] = imgui_json::number(clip->mID);
                    action["group_id"] = imgui_json::number(iter->mID);
                    pActionList->push_back(std::move(action));
                }
            }
        }
        if (need_erase)
        {
            if (pActionList)
            {
                imgui_json::value action;
                action["action"] = "REMOVE_GROUP";
                imgui_json::value groupJson;
                iter->Save(groupJson);
                action["group_json"] = groupJson;
                pActionList->push_back(std::move(action));
            }
            iter = m_Groups.erase(iter);
        }
        else
            ++iter;
    }
    clip->mGroupID = -1;
}

ImU32 TimeLine::GetGroupColor(int64_t group_id)
{
    ImU32 color = IM_COL32(32,128,32,128);
    for (auto group : m_Groups)
    {
        if (group.mID == group_id)
            return group.mColor;
    }
    return color;
}

void TimeLine::CustomDraw(
        int index, ImDrawList *draw_list, const ImRect &view_rc, const ImRect &rc,
        const ImRect &titleRect, const ImRect &clippingTitleRect, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRec,
        int64_t mouse_time, bool is_moving, bool enable_select, bool is_updated, std::list<imgui_json::value>* pActionList)
{
    // view_rc: track view rect
    // rc: full track length rect
    // titleRect: full track length title rect(same as Compact view rc)
    // clippingTitleRect: current view title area
    // clippingRect: current view window area
    // legendRect: legend area
    // legendClippingRect: legend area
    bool mouse_clicked = false;
    int64_t viewEndTime = firstTime + visibleTime;
    ImGuiIO &io = ImGui::GetIO();
    if (index >= m_Tracks.size())
        return;
    MediaTrack *track = m_Tracks[index];
    if (!track)
        return;

    // draw legend
    draw_list->PushClipRect(legendRect.Min, legendRect.Max, true);
    draw_list->AddRect(legendRect.Min, legendRect.Max, COL_DEEP_DARK, 0, 0, 2);
    // TODO::Dicky need indicate track type and track status such as linked
    std::string back_icon = IS_VIDEO(track->mType) ? std::string(ICON_MEDIA_VIDEO) : 
                            IS_AUDIO(track->mType) ? std::string(ICON_MEDIA_WAVE) : 
                            IS_TEXT(track->mType) ? std::string(ICON_MEDIA_TEXT) :
                            IS_MIDI(track->mType) ? std::string(ICON_MEDIA_AUDIO) : std::string();
    ImU32 back_icon_color = !track->mView ? IM_COL32(64, 64, 64, 128) :
                            track->mLocked ? IM_COL32(128, 64, 64, 128) : 
                            IM_COL32(128, 128, 255, 128);

    if (!back_icon.empty())
    {
        float back_icon_scale = std::min(legendRect.GetSize().x, legendRect.GetSize().y) / ImGui::GetFontSize();
        ImGui::SetWindowFontScale(back_icon_scale * 0.75);
        auto back_icon_size = ImGui::CalcTextSize(back_icon.c_str());
        float start_offset_x = std::max((legendRect.GetSize().x - back_icon_size.x) / 2, 0.f);
        float start_offset_y = std::max((legendRect.GetSize().y - back_icon_size.y) / 2, 0.f);
        ImGui::SetWindowFontScale(1.0f);
        draw_list->AddTextComplex(legendRect.Min + ImVec2(start_offset_x, start_offset_y), back_icon.c_str(), back_icon_scale * 0.75, back_icon_color, 1.0f, IM_COL32(0, 0, 0, 255));
    }

    auto is_control_hovered = track->DrawTrackControlBar(draw_list, legendRect, enable_select, pActionList);
    draw_list->PopClipRect();

    bool bOverlapHovered = false;
    if (clippingRect.Contains(io.MousePos))
    {
        for (auto overlap : track->m_Overlaps)
        {
            if (overlap->mStart <= mouse_time && overlap->mEnd >= mouse_time)
            {
                bOverlapHovered = true;
                break;
            }
        }
    }

    // draw clips
    for (auto clip : track->m_Clips)
    {
        clip->SetViewWindowStart(firstTime);
        bool draw_clip = false;
        float cursor_start = 0;
        float cursor_end  = 0;
        ImDrawFlags flag = ImDrawFlags_RoundCornersNone;
        if (clip->IsInClipRange(firstTime) && clip->End() <= viewEndTime)
        {
            /***********************************************************
             *         ----------------------------------------
             * XXXXXXXX|XXXXXXXXXXXXXXXXXXXXXX|
             *         ----------------------------------------
            ************************************************************/
            cursor_start = clippingRect.Min.x;
            cursor_end = clippingRect.Min.x + (clip->End() - firstTime) * msPixelWidthTarget;
            draw_clip = true;
            flag |= ImDrawFlags_RoundCornersRight;
        }
        else if (clip->Start() >= firstTime && clip->End() <= viewEndTime)
        {
            /***********************************************************
             *         ----------------------------------------
             *                  |XXXXXXXXXXXXXXXXXXXXXX|
             *         ----------------------------------------
            ************************************************************/
            cursor_start = clippingRect.Min.x + (clip->Start() - firstTime) * msPixelWidthTarget;
            cursor_end = clippingRect.Min.x + (clip->End() - firstTime) * msPixelWidthTarget;
            draw_clip = true;
            flag |= ImDrawFlags_RoundCornersAll;
        }
        else if (clip->Start() >= firstTime && clip->IsInClipRange(viewEndTime))
        {
            /***********************************************************
             *         ----------------------------------------
             *                         |XXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXX
             *         ----------------------------------------
            ************************************************************/
            cursor_start = clippingRect.Min.x + (clip->Start() - firstTime) * msPixelWidthTarget;
            cursor_end = clippingRect.Max.x;
            draw_clip = true;
            flag |= ImDrawFlags_RoundCornersLeft;
        }
        else if (clip->Start() <= firstTime && clip->End() >= viewEndTime)
        {
            /***********************************************************
             *         ----------------------------------------
             *  XXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXX
             *         ----------------------------------------
            ************************************************************/
            cursor_start = clippingRect.Min.x;
            cursor_end  = clippingRect.Max.x;
            draw_clip = true;
        }
        if (clip->Start() == firstTime)
            flag |= ImDrawFlags_RoundCornersLeft;
        if (clip->End() == viewEndTime)
            flag |= ImDrawFlags_RoundCornersRight;

        ImVec2 clip_title_pos_min = ImVec2(cursor_start, clippingTitleRect.Min.y);
        ImVec2 clip_title_pos_max = ImVec2(cursor_end, clippingTitleRect.Max.y);
        ImVec2 clip_pos_min = clip_title_pos_min;
        ImVec2 clip_pos_max = clip_title_pos_max;
        if (track->mExpanded)
        {
            ImVec2 custom_pos_min = ImVec2(cursor_start, clippingRect.Min.y);
            ImVec2 custom_pos_max = ImVec2(cursor_end, clippingRect.Max.y);
            clip_pos_min = custom_pos_min;
            clip_pos_max = custom_pos_max;
        }
        // Check if clip is outof view rect then don't draw
        ImRect clip_rect(clip_title_pos_min, clip_pos_max);
        ImRect clip_area_rect(clip_pos_min, clip_pos_max);
        if (!clip_rect.Overlaps(view_rc))
        {
            draw_clip = false;
        }

        if (draw_clip && cursor_end > cursor_start)
        {
            // draw title bar
            auto color = clip->mGroupID != -1 ? GetGroupColor(clip->mGroupID) : IM_COL32(64,128,64,128);
            draw_list->AddRectFilled(clip_title_pos_min, clip_title_pos_max, color, 4, flag);
            
            // draw clip status
            draw_list->PushClipRect(clip_title_pos_min, clip_title_pos_max, true);           
            
            // add clip event on title bar
            if (clip && clip->mEventStack)
            {
                auto events = clip->mEventStack->GetEventList();
                auto start_time = firstTime - clip->Start();
                auto end_time = viewEndTime - clip->Start();
                for (auto event : events)
                {
                    bool draw_event = false;
                    float event_cursor_start = 0;
                    float event_cursor_end  = 0;
                    if (event->IsInRange(start_time) && event->End() <= end_time)
                    {
                        event_cursor_start = clippingRect.Min.x;
                        event_cursor_end = clippingRect.Min.x + (event->End() - start_time) * msPixelWidthTarget;
                        draw_event = true;
                    }
                    else if (event->Start() >= start_time && event->End() <= end_time)
                    {
                        event_cursor_start = clippingRect.Min.x + (event->Start() - start_time) * msPixelWidthTarget;
                        event_cursor_end = clippingRect.Min.x + (event->End() - start_time) * msPixelWidthTarget;
                        draw_event = true;
                    }
                    else if (event->Start() >= start_time && event->IsInRange(end_time))
                    {
                        event_cursor_start = clippingRect.Min.x + (event->Start() - start_time) * msPixelWidthTarget;
                        event_cursor_end = clippingRect.Max.x;
                        draw_event = true;
                    }
                    else if (event->Start() <= start_time && event->End() >= end_time)
                    {
                        event_cursor_start = clippingRect.Min.x;
                        event_cursor_end  = clippingRect.Max.x;
                        draw_event = true;
                    }
                    if (draw_event)
                    {
                        ImVec2 event_pos_min = ImVec2(event_cursor_start, clippingTitleRect.Min.y + titleRect.GetHeight() / 4);
                        ImVec2 event_pos_max = ImVec2(event_cursor_end, clippingTitleRect.Max.y - titleRect.GetHeight() / 4);
                        draw_list->AddRectFilled(event_pos_min, event_pos_max, IM_COL32_INVERSE(color));
                    }
                }
            }
            draw_list->AddText(clip_title_pos_min + ImVec2(4, 0), IM_COL32_WHITE, IS_TEXT(clip->mType) ? "T" : clip->mName.c_str());
            draw_list->PopClipRect();

            // draw custom view
            if (track->mExpanded)
            {
                // can't using draw_list->PushClipRect, maybe all PushClipRect with screen pos/size need change to ImGui::PushClipRect
                ImGui::PushClipRect(clippingRect.Min, clippingRect.Max, true);
                clip->DrawContent(draw_list, clip_pos_min, clip_pos_max, clippingRect, is_updated);
                ImGui::PopClipRect();
            }

            if (clip->bSelected)
            {
                draw_list->AddRect(clip_rect.Min, clip_rect.Max, IM_COL32(255,0,0,224), 4, flag, 2.0f);
            }

            // Clip select
            if (enable_select)
            {
                //if (clip_rect.Contains(io.MousePos) )
                if (clip->bHovered)
                {
                    draw_list->AddRect(clip_rect.Min, clip_rect.Max, IM_COL32(255,255,255,255), 4, flag, 2.0f);
                    // [shortcut]: shift+a for appand select
                    //const bool is_shift_key_only = (io.KeyMods == ImGuiModFlags_Shift);
                    bool appand = (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) && ImGui::IsKeyDown(ImGuiKey_A);
                    bool can_be_select = false;
                    if (is_moving && !clip->bSelected)
                        can_be_select = true;
                    else if (!is_moving && !clip->bSelected)
                        can_be_select = true;
                    else if (appand)
                        can_be_select = true;
                    if (can_be_select && !mouse_clicked && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        track->SelectClip(clip, appand);
                        SelectTrack(index);
                        mouse_clicked = true;
                    }
                    else if (track->mExpanded && clip_rect.Contains(io.MousePos) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        // [shortcut]: shift + double left click to attribute page
                        //bool b_attr_editing = ImGui::IsKeyDown(ImGuiKey_LeftShift) && (io.KeyMods == ImGuiModFlags_Shift);
                        if (!IS_DUMMY(clip->mType) && !bOverlapHovered)
                            track->SelectEditingClip(clip);
                    }
                    clip->DrawTooltips();
                }
            }
        }
    }

    // draw overlap
    for (auto overlap : track->m_Overlaps)
    {
        bool draw_overlap = false;
        float cursor_start = 0;
        float cursor_end  = 0;
        if (overlap->mStart >= firstTime && overlap->mEnd < viewEndTime)
        {
            cursor_start = clippingTitleRect.Min.x + (overlap->mStart - firstTime) * msPixelWidthTarget;
            cursor_end = clippingTitleRect.Min.x + (overlap->mEnd - firstTime) * msPixelWidthTarget;
            draw_overlap = true;
        }
        else if (overlap->mStart >= firstTime && overlap->mStart <= viewEndTime && overlap->mEnd >= viewEndTime)
        {
            cursor_start = clippingTitleRect.Min.x + (overlap->mStart - firstTime) * msPixelWidthTarget;
            cursor_end = clippingTitleRect.Max.x;
            draw_overlap = true;
        }
        else if (overlap->mStart <= firstTime && overlap->mEnd <= viewEndTime)
        {
            cursor_start = clippingTitleRect.Min.x;
            cursor_end = clippingTitleRect.Min.x + (overlap->mEnd - firstTime) * msPixelWidthTarget;
            draw_overlap = true;
        }
        else if (overlap->mStart <= firstTime && overlap->mEnd >= viewEndTime)
        {
            cursor_start = clippingTitleRect.Min.x;
            cursor_end  = clippingTitleRect.Max.x;
            draw_overlap = true;
        }

        ImVec2 overlap_pos_min = ImVec2(cursor_start, clippingTitleRect.Min.y);
        ImVec2 overlap_pos_max = ImVec2(cursor_end, clippingTitleRect.Max.y);
        // Check if overlap is outof view rect then don't draw
        ImRect overlap_rect(overlap_pos_min, overlap_pos_max);
        ImRect overlap_clip_rect = overlap_rect;
        if (track->mExpanded)
        {
            overlap_clip_rect.Min.y = clippingRect.Min.y;
            overlap_clip_rect.Max.y = clippingRect.Max.y;
        }

        if (!overlap_rect.Overlaps(view_rc))
        {
            draw_overlap = false;
        }

        if (draw_overlap && cursor_end > cursor_start)
        {
            bool isOverlapEmpty = overlap->IsOverlapEmpty();
            draw_list->PushClipRect(clippingTitleRect.Min, clippingTitleRect.Max, true);
            if (overlap->bEditing)
            {
                draw_list->AddRect(overlap_pos_min, overlap_pos_max, IM_COL32(255, 255, 255, 224), 4, ImDrawFlags_RoundCornersAll, 3.f);
            }
            if (overlap_rect.Contains(io.MousePos) || overlap_clip_rect.Contains(io.MousePos))
            {
                draw_list->AddRectFilled(overlap_pos_min, overlap_pos_max, IM_COL32(255,32,32,128), 4, ImDrawFlags_RoundCornersAll);
                if (enable_select && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    track->SelectEditingOverlap(overlap);
                }
            }
            else
                draw_list->AddRectFilled(overlap_pos_min, overlap_pos_max, IM_COL32(128,32,32,128), 4, ImDrawFlags_RoundCornersAll);
            draw_list->AddLine(overlap_pos_min, overlap_pos_max, IM_COL32(0, 0, 0, 255));
            draw_list->AddLine(ImVec2(overlap_pos_max.x, overlap_pos_min.y), ImVec2(overlap_pos_min.x, overlap_pos_max.y), IM_COL32(0, 0, 0, 255));
            draw_list->PopClipRect();
            
            draw_list->AddRect(overlap_clip_rect.Min, overlap_clip_rect.Max, IM_COL32(255,255,32,255), 4, ImDrawFlags_RoundCornersAll);
            std::string Overlap_icon = overlap->bEditing ? std::string(ICON_BP_EDITING) : !isOverlapEmpty ? std::string(ICON_BP_VALID) : std::string();
            if (!Overlap_icon.empty())
            {
                float icon_scale = std::min(overlap_clip_rect.GetSize().x, overlap_clip_rect.GetSize().y) / ImGui::GetFontSize();
                auto icon_size = ImGui::CalcTextSize(Overlap_icon.c_str());
                float start_offset_x = std::max((overlap_clip_rect.GetSize().x - icon_size.x) / 2, 0.f);
                float start_offset_y = std::max((overlap_clip_rect.GetSize().y - icon_size.y) / 2, 0.f);
                draw_list->AddText(overlap_clip_rect.Min + ImVec2(start_offset_x, start_offset_y), isOverlapEmpty ? IM_COL32(255, 0, 0, 255) : IM_COL32_WHITE, Overlap_icon.c_str());
            }
        }
    }

    if (!is_control_hovered && enable_select && legendRect.Contains(io.MousePos) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        track->mExpanded = !track->mExpanded;
    }
}

int TimeLine::Load(const imgui_json::value& value)
{
    // load ID Generator state
    if (value.contains("IDGenerateState"))
    {
        int64_t state = ImGui::get_current_time_usec();
        auto& val = value["IDGenerateState"];
        if (val.is_number()) state = val.get<imgui_json::number>();
        m_IDGenerator.SetState(state);
    }
    
    // load global info
    if (value.contains("Start"))
    {
        auto& val = value["Start"];
        if (val.is_number()) mStart = val.get<imgui_json::number>();
    }
    if (value.contains("End"))
    {
        auto& val = value["End"];
        if (val.is_number()) mEnd = val.get<imgui_json::number>();
    }
    if (value.contains("VideoWidth"))
    {
        auto& val = value["VideoWidth"];
        if (val.is_number()) mhMediaSettings->SetVideoOutWidth(val.get<imgui_json::number>());
    }
    if (value.contains("VideoHeight"))
    {
        auto& val = value["VideoHeight"];
        if (val.is_number()) mhMediaSettings->SetVideoOutHeight(val.get<imgui_json::number>());
    }
    if (value.contains("PreviewScale"))
    {
        auto& val = value["PreviewScale"];
        if (val.is_number()) mPreviewScale = val.get<imgui_json::number>();
    }
    MediaCore::Ratio frameRate;
    if (value.contains("FrameRateNum"))
    {
        auto& val = value["FrameRateNum"];
        if (val.is_number()) frameRate.num = val.get<imgui_json::number>();
    }
    if (value.contains("FrameRateDen"))
    {
        auto& val = value["FrameRateDen"];
        if (val.is_number()) frameRate.den = val.get<imgui_json::number>();
    }
    mhMediaSettings->SetVideoOutFrameRate(frameRate);
    if (value.contains("AudioChannels"))
    {
        auto& val = value["AudioChannels"];
        if (val.is_number()) mhMediaSettings->SetAudioOutChannels(val.get<imgui_json::number>());
    }
    if (value.contains("AudioSampleRate"))
    {
        auto& val = value["AudioSampleRate"];
        if (val.is_number()) mhMediaSettings->SetAudioOutSampleRate(val.get<imgui_json::number>());
    }
    if (value.contains("AudioRenderFormat"))
    {
        auto& val = value["AudioRenderFormat"];
        if (val.is_number()) mAudioRenderFormat = (MediaCore::AudioRender::PcmFormat)val.get<imgui_json::number>();
    }
    if (value.contains("msPixelWidth"))
    {
        auto& val = value["msPixelWidth"];
        if (val.is_number()) msPixelWidthTarget = val.get<imgui_json::number>();
    }
    if (value.contains("FirstTime"))
    {
        auto& val = value["FirstTime"];
        if (val.is_number()) firstTime = val.get<imgui_json::number>();
    }
    if (value.contains("CurrentTime"))
    {
        auto& val = value["CurrentTime"];
        if (val.is_number()) mCurrentTime = val.get<imgui_json::number>();
    }
    if (value.contains("MarkIn"))
    {
        auto& val = value["MarkIn"];
        if (val.is_number()) mark_in = val.get<imgui_json::number>();
    }
    if (value.contains("MarkOut"))
    {
        auto& val = value["MarkOut"];
        if (val.is_number()) mark_out = val.get<imgui_json::number>();
    }
    if (value.contains("PreviewForward"))
    {
        auto& val = value["PreviewForward"];
        if (val.is_boolean()) mIsPreviewForward = val.get<imgui_json::boolean>();
    }
    if (value.contains("Loop"))
    {
        auto& val = value["Loop"];
        if (val.is_boolean()) bLoop = val.get<imgui_json::boolean>();
    }
    if (value.contains("Compare"))
    {
        auto& val = value["Compare"];
        if (val.is_boolean()) bCompare = val.get<imgui_json::boolean>();
    }
    if (value.contains("TransitionOutPreview"))
    {
        auto& val = value["TransitionOutPreview"];
        if (val.is_boolean()) bTransitionOutputPreview = val.get<imgui_json::boolean>();
    }
    if (value.contains("SelectLinked"))
    {
        auto& val = value["SelectLinked"];
        if (val.is_boolean()) bSelectLinked = val.get<imgui_json::boolean>();
    }
    if (value.contains("MovingAttract"))
    {
        auto& val = value["MovingAttract"];
        if (val.is_boolean()) bMovingAttract = val.get<imgui_json::boolean>();
    }

    if (value.contains("FontName"))
    {
        auto& val = value["FontName"];
        if (val.is_string()) mFontName = val.get<imgui_json::string>();
    }

    if (value.contains("OutputName"))
    {
        auto& val = value["OutputName"];
        if (val.is_string()) mOutputName = val.get<imgui_json::string>();
    }

    if (value.contains("OutputPath"))
    {
        auto& val = value["OutputPath"];
        if (val.is_string()) mOutputPath = val.get<imgui_json::string>();
    }
    
    if (value.contains("OutputVideoCode"))
    {
        auto& val = value["OutputVideoCode"];
        if (val.is_string()) mVideoCodec = val.get<imgui_json::string>();
    }

    if (value.contains("OutputAudioCode"))
    {
        auto& val = value["OutputAudioCode"];
        if (val.is_string()) mAudioCodec = val.get<imgui_json::string>();
    }

    if (value.contains("OutputVideo"))
    {
        auto& val = value["OutputVideo"];
        if (val.is_boolean()) bExportVideo = val.get<imgui_json::boolean>();
    }

    if (value.contains("OutputAudio"))
    {
        auto& val = value["OutputAudio"];
        if (val.is_boolean()) bExportAudio = val.get<imgui_json::boolean>();
    }

    if (value.contains("SortMethod"))
    {
        auto& val = value["SortMethod"];
        if (val.is_boolean()) mSortMethod = val.get<imgui_json::number>();
    }

    mPreviewResumePos = mCurrentTime = AlignTime(mCurrentTime);

    auto pcmDataType = MatUtils::PcmFormat2ImDataType(mAudioRenderFormat);
    if (pcmDataType == IM_DT_UNDEFINED)
        throw std::runtime_error("UNSUPPORTED audio render format!");
    mhMediaSettings->SetAudioOutDataType(pcmDataType);
    mhPreviewSettings->SyncVideoSettingsFrom(mhMediaSettings.get());
    auto previewSize = CalcPreviewSize({(int32_t)mhMediaSettings->VideoOutWidth(), (int32_t)mhMediaSettings->VideoOutHeight()}, mPreviewScale);
    mhPreviewSettings->SetVideoOutWidth(previewSize.x);
    mhPreviewSettings->SetVideoOutHeight(previewSize.y);
    mhPreviewSettings->SyncAudioSettingsFrom(mhMediaSettings.get());
    RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
    mTxMgr->GetTexturePoolAttributes(PREVIEW_TEXTURE_POOL_NAME, tTxPoolAttrs);
    tTxPoolAttrs.tTxSize = previewSize;
    mTxMgr->SetTexturePoolAttributes(PREVIEW_TEXTURE_POOL_NAME, tTxPoolAttrs);
    mhPreviewTx = mTxMgr->GetTextureFromPool(PREVIEW_TEXTURE_POOL_NAME);
    mAudioRender->CloseDevice();
    mPcmStream.Flush();
    if (!mAudioRender->OpenDevice(mhPreviewSettings->AudioOutSampleRate(), mhPreviewSettings->AudioOutChannels(), mAudioRenderFormat, &mPcmStream))
        throw std::runtime_error("FAILED to open audio render device!");
    mAudioAttribute.channel_data.clear();
    mAudioAttribute.channel_data.resize(mhMediaSettings->AudioOutChannels());

    // load data layer
    ConfigureDataLayer();

    // load media clip
    const imgui_json::array* mediaClipArray = nullptr;
    if (imgui_json::GetPtrTo(value, "MediaClip", mediaClipArray))
    {
        for (const auto& jnClipJson : *mediaClipArray)
        {
            uint32_t type = MEDIA_UNKNOWN;
            if (jnClipJson.contains("Type"))
            {
                auto& val = jnClipJson["Type"];
                if (val.is_number()) type = val.get<imgui_json::number>();
            }
            Clip* pClip = nullptr;
            if (IS_VIDEO(type))
                pClip = VideoClip::CreateInstanceFromJson(jnClipJson, this);
            else if (IS_AUDIO(type))
                pClip = AudioClip::CreateInstanceFromJson(jnClipJson, this);
            else if (IS_TEXT(type))
                pClip = TextClip::CreateInstanceFromJson(jnClipJson, this);
            if (pClip)
                m_Clips.push_back(pClip);
        }
    }

    // load media group
    const imgui_json::array* mediaGroupArray = nullptr;
    if (imgui_json::GetPtrTo(value, "MediaGroup", mediaGroupArray))
    {
        for (auto& group : *mediaGroupArray)
        {
            ClipGroup new_group(this);
            new_group.Load(group);
            m_Groups.push_back(new_group);
        }
    }

    // load media overlap
    const imgui_json::array* mediaOverlapArray = nullptr;
    if (imgui_json::GetPtrTo(value, "MediaOverlap", mediaOverlapArray))
    {
        for (auto& overlap : *mediaOverlapArray)
        {
            Overlap * new_overlap = Overlap::Load(overlap, this);
            if (new_overlap)
                m_Overlaps.push_back(new_overlap);
        }
    }

    // load media track
    const imgui_json::array* mediaTrackArray = nullptr;
    if (imgui_json::GetPtrTo(value, "MediaTrack", mediaTrackArray))
    {
        for (auto& track : *mediaTrackArray)
        {
            MediaTrack * media_track = MediaTrack::Load(track, this);
            if (media_track)
            {
                m_Tracks.push_back(media_track);
            }
        }
    }

    // load audio attribute
    if (value.contains("AudioAttribute"))
    {
        auto& audio_attr = value["AudioAttribute"];
        if (audio_attr.contains("AudioGain"))
        {
            auto& val = audio_attr["AudioGain"];
            if (val.is_number()) mAudioAttribute.mAudioGain = val.get<imgui_json::number>();
        }
        if (audio_attr.contains("AudioPanEnabled"))
        {
            auto& val = audio_attr["AudioPanEnabled"];
            if (val.is_boolean()) mAudioAttribute.bPan = val.get<imgui_json::boolean>();
        }
        if (audio_attr.contains("AudioPan"))
        {
            auto& val = audio_attr["AudioPan"];
            if (val.is_vec2()) mAudioAttribute.audio_pan = val.get<imgui_json::vec2>();
        }
        if (audio_attr.contains("AudioLimiterEnabled"))
        {
            auto& val = audio_attr["AudioLimiterEnabled"];
            if (val.is_boolean()) mAudioAttribute.bLimiter = val.get<imgui_json::boolean>();
        }
        if (audio_attr.contains("AudioLimiter"))
        {
            auto& val = audio_attr["AudioLimiter"];
            if (val.is_object())
            {
                if (val.contains("limit"))
                {
                    auto& _val = val["limit"];
                    if (_val.is_number()) mAudioAttribute.limit = _val.get<imgui_json::number>();
                }
                if (val.contains("attack"))
                {
                    auto& _val = val["attack"];
                    if (_val.is_number()) mAudioAttribute.limiter_attack = _val.get<imgui_json::number>();
                }
                if (val.contains("release"))
                {
                    auto& _val = val["release"];
                    if (_val.is_number()) mAudioAttribute.limiter_release = _val.get<imgui_json::number>();
                }
            }
        }
        if (audio_attr.contains("AudioGateEnabled"))
        {
            auto& val = audio_attr["AudioGateEnabled"];
            if (val.is_boolean()) mAudioAttribute.bGate = val.get<imgui_json::boolean>();
        }
        if (audio_attr.contains("AudioGate"))
        {
            auto& val = audio_attr["AudioGate"];
            if (val.is_object())
            {
                if (val.contains("threshold"))
                {
                    auto& _val = val["threshold"];
                    if (_val.is_number()) mAudioAttribute.gate_thd = _val.get<imgui_json::number>();
                }
                if (val.contains("range"))
                {
                    auto& _val = val["range"];
                    if (_val.is_number()) mAudioAttribute.gate_range = _val.get<imgui_json::number>();
                }
                if (val.contains("ratio"))
                {
                    auto& _val = val["ratio"];
                    if (_val.is_number()) mAudioAttribute.gate_ratio = _val.get<imgui_json::number>();
                }
                if (val.contains("attack"))
                {
                    auto& _val = val["attack"];
                    if (_val.is_number()) mAudioAttribute.gate_attack = _val.get<imgui_json::number>();
                }
                if (val.contains("release"))
                {
                    auto& _val = val["release"];
                    if (_val.is_number()) mAudioAttribute.gate_release = _val.get<imgui_json::number>();
                }
                if (val.contains("makeup"))
                {
                    auto& _val = val["makeup"];
                    if (_val.is_number()) mAudioAttribute.gate_makeup = _val.get<imgui_json::number>();
                }
                if (val.contains("knee"))
                {
                    auto& _val = val["knee"];
                    if (_val.is_number()) mAudioAttribute.gate_knee = _val.get<imgui_json::number>();
                }
            }
        }
        if (audio_attr.contains("AudioCompressorEnabled"))
        {
            auto& val = audio_attr["AudioCompressorEnabled"];
            if (val.is_boolean()) mAudioAttribute.bCompressor = val.get<imgui_json::boolean>();
        }
        if (audio_attr.contains("AudioCompressor"))
        {
            auto& val = audio_attr["AudioCompressor"];
            if (val.is_object())
            {
                if (val.contains("threshold"))
                {
                    auto& _val = val["threshold"];
                    if (_val.is_number()) mAudioAttribute.compressor_thd = _val.get<imgui_json::number>();
                }
                if (val.contains("ratio"))
                {
                    auto& _val = val["ratio"];
                    if (_val.is_number()) mAudioAttribute.compressor_ratio = _val.get<imgui_json::number>();
                }
                if (val.contains("knee"))
                {
                    auto& _val = val["knee"];
                    if (_val.is_number()) mAudioAttribute.compressor_knee = _val.get<imgui_json::number>();
                }
                if (val.contains("mix"))
                {
                    auto& _val = val["mix"];
                    if (_val.is_number()) mAudioAttribute.compressor_mix = _val.get<imgui_json::number>();
                }
                if (val.contains("attack"))
                {
                    auto& _val = val["attack"];
                    if (_val.is_number()) mAudioAttribute.compressor_attack = _val.get<imgui_json::number>();
                }
                if (val.contains("release"))
                {
                    auto& _val = val["release"];
                    if (_val.is_number()) mAudioAttribute.compressor_release = _val.get<imgui_json::number>();
                }
                if (val.contains("makeup"))
                {
                    auto& _val = val["makeup"];
                    if (_val.is_number()) mAudioAttribute.compressor_makeup = _val.get<imgui_json::number>();
                }
                if (val.contains("levelIn"))
                {
                    auto& _val = val["levelIn"];
                    if (_val.is_number()) mAudioAttribute.compressor_level_sc = _val.get<imgui_json::number>();
                }
            }
        }
        if (audio_attr.contains("AudioEqualizerEnabled"))
        {
            auto& val = audio_attr["AudioEqualizerEnabled"];
            if (val.is_boolean()) mAudioAttribute.bEqualizer = val.get<imgui_json::boolean>();
        }
        if (audio_attr.contains("AudioEqualizer"))
        {
            auto& val = audio_attr["AudioEqualizer"];
            if (val.is_array())
            {
                auto& gainsAry = val.get<imgui_json::array>();
                int idx = 0;
                for (auto& jval : gainsAry)
                {
                    mAudioAttribute.mBandCfg[idx].gain = (int32_t)jval.get<imgui_json::number>();
                    if (idx >= 10)
                    break;
                    idx++;
                }
            }
        }
    }

    // Adjust the position and range of clips and overlaps according to timeline's frame rate
    for (auto clip : m_Clips)
    {
        const auto alignedStart = AlignTime(clip->Start());
        auto alignedEnd = AlignTime(clip->End());
        if (alignedEnd <= alignedStart)
            alignedEnd = AlignTimeToNextFrame(alignedStart);
        const auto alignedStartOffset = AlignTime(clip->StartOffset());
        const auto alignedEndOffset = AlignTime(clip->EndOffset());
        if (IS_IMAGE(clip->mType))
        {
            if (alignedStart != clip->Start() || alignedEnd != clip->End())
                clip->SetPositionAndRange(alignedStart, alignedEnd, 0, 0);
        }
        else
        {
            const auto alignedSourceDuration = clip->mMediaParser ? AlignTime(clip->mMediaParser->GetMediaInfo()->duration*1000) : AlignTime(clip->Length());
            if (alignedEnd-alignedStart+alignedStartOffset+alignedEndOffset != alignedSourceDuration)
            {
                auto newStart = alignedStart;
                auto newEnd = alignedEnd;
                auto newStartOffset = alignedStartOffset;
                auto newEndOffset = alignedSourceDuration-newEnd+newStart-newStartOffset;
                if (newEndOffset < 0)
                {
                    newStartOffset += newEndOffset;
                    newEndOffset = 0;
                }
                if (newStartOffset < 0)
                {
                    newEnd += newStartOffset;
                    newStartOffset = 0;
                }
                clip->SetPositionAndRange(newStart, newEnd, newStartOffset, newEndOffset);
            }
            else if (alignedStart != clip->Start() || alignedEnd != clip->End() || alignedStartOffset != clip->StartOffset() || alignedEndOffset != clip->EndOffset())
            {
                clip->SetPositionAndRange(alignedStart, alignedEnd, alignedStartOffset, alignedEndOffset);
            }
        }
    }

    // build data layer multi-track media reader
    for (auto track : m_Tracks)
    {
        if (IS_VIDEO(track->mType))
        {
            MediaCore::VideoTrack::Holder vidTrack = mMtvReader->AddTrack(track->mID);
            vidTrack->SetVisible(track->mView);
            for (auto pUiClip : track->m_Clips)
            {
                if (IS_DUMMY(pUiClip->mType))
                    continue;
                MediaCore::VideoClip::Holder hVidClip;
                if (IS_IMAGE(pUiClip->mType))
                    hVidClip = vidTrack->AddImageClip(pUiClip->mID, pUiClip->mMediaParser, pUiClip->Start(), pUiClip->Length());
                else
                    hVidClip = vidTrack->AddVideoClip(pUiClip->mID, pUiClip->mMediaParser, pUiClip->Start(), pUiClip->End(), pUiClip->StartOffset(), pUiClip->EndOffset(), mCurrentTime-pUiClip->Start());
                VideoClip* pUiVClip = dynamic_cast<VideoClip*>(pUiClip);
                pUiVClip->SetDataLayer(hVidClip, true);
            }
        }
        else if (IS_AUDIO(track->mType))
        {
            MediaCore::AudioTrack::Holder audTrack = mMtaReader->AddTrack(track->mID);
            audTrack->SetMuted(!track->mView);
            for (auto pUiClip : track->m_Clips)
            {
                if (IS_DUMMY(pUiClip->mType) || !pUiClip->mMediaParser)
                    continue;
                MediaCore::AudioClip::Holder hAudClip = audTrack->AddNewClip(
                    pUiClip->mID, pUiClip->mMediaParser,
                    pUiClip->Start(), pUiClip->End(), pUiClip->StartOffset(), pUiClip->EndOffset());
                AudioClip* pUiAClip = dynamic_cast<AudioClip*>(pUiClip);
                pUiAClip->SetDataLayer(hAudClip, true);
            }
            // audio attribute
            auto aeFilter = audTrack->GetAudioEffectFilter();
            // gain
            auto volParams = aeFilter->GetVolumeParams();
            volParams.volume = track->mAudioTrackAttribute.mAudioGain;
            aeFilter->SetVolumeParams(&volParams);
        }
    }

    // build data layer audio attribute
    auto amFilter = mMtaReader->GetAudioEffectFilter();
    // gain
    auto volMaster = amFilter->GetVolumeParams();
    volMaster.volume = mAudioAttribute.mAudioGain;
    amFilter->SetVolumeParams(&volMaster);
    // pan
    auto panParams = amFilter->GetPanParams();
    panParams.x = mAudioAttribute.bPan ? mAudioAttribute.audio_pan.x : 0.5f;
    panParams.y = mAudioAttribute.bPan ? mAudioAttribute.audio_pan.y : 0.5f;
    amFilter->SetPanParams(&panParams);
    // limiter
    auto limiterParams = amFilter->GetLimiterParams();
    limiterParams.limit = mAudioAttribute.bLimiter ? mAudioAttribute.limit : 1.0;
    limiterParams.attack = mAudioAttribute.bLimiter ? mAudioAttribute.limiter_attack : 5;
    limiterParams.release = mAudioAttribute.bLimiter ? mAudioAttribute.limiter_release : 50;
    amFilter->SetLimiterParams(&limiterParams);
    // gate
    auto gateParams = amFilter->GetGateParams();
    gateParams.threshold = mAudioAttribute.bGate ? mAudioAttribute.gate_thd : 0.f;
    gateParams.range = mAudioAttribute.bGate ? mAudioAttribute.gate_range : 0.f;
    gateParams.ratio = mAudioAttribute.bGate ? mAudioAttribute.gate_ratio : 2.f;
    gateParams.attack = mAudioAttribute.bGate ? mAudioAttribute.gate_attack : 20.f;
    gateParams.release = mAudioAttribute.bGate ? mAudioAttribute.gate_release : 250.f;
    gateParams.makeup = mAudioAttribute.bGate ? mAudioAttribute.gate_makeup : 1.f;
    gateParams.knee = mAudioAttribute.bGate ? mAudioAttribute.gate_knee : 2.82843f;
    amFilter->SetGateParams(&gateParams);
    // compressor
    auto compressorParams = amFilter->GetCompressorParams();
    compressorParams.threshold = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_thd : 1.f;
    compressorParams.ratio = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_ratio : 2.f;
    compressorParams.knee = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_knee : 2.82843f;
    compressorParams.mix = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_mix : 1.f;
    compressorParams.attack = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_attack : 20.f;
    compressorParams.release = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_release : 250.f;
    compressorParams.makeup = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_makeup : 1.f;
    compressorParams.levelIn = mAudioAttribute.bCompressor ? mAudioAttribute.compressor_level_sc : 1.f;
    amFilter->SetCompressorParams(&compressorParams);
    // equalizer
    for (int i = 0; i < 10; i++)
    {
        auto equalizerParams = amFilter->GetEqualizerParamsByIndex(i);
        equalizerParams.gain = mAudioAttribute.bEqualizer ? mAudioAttribute.mBandCfg[i].gain : 0;
        amFilter->SetEqualizerParamsByIndex(&equalizerParams, i);
    }

    Update();
    mMtvReader->SeekTo(mCurrentTime);
    mFrameIndex = mMtvReader->MillsecToFrameIndex(mCurrentTime);
    mMtaReader->UpdateDuration();
    mMtaReader->SeekTo(mCurrentTime, false);
    SyncDataLayer(true);
    return 0;
}

void TimeLine::Save(imgui_json::value& value)
{
    // save editing item
    for (auto item : mEditingItems)
    {
        if (item->mEditingClip) item->mEditingClip->Save();
        if (item->mEditingOverlap) item->mEditingOverlap->Save();
    }
    // TODO::Dicky editing item save editing item into json?

    // save media clip
    imgui_json::value media_clips;
    for (auto clip : m_Clips)
    {
        media_clips.push_back(clip->SaveAsJson());
    }
    if (m_Clips.size() > 0) value["MediaClip"] = media_clips;

    // save clip group
    imgui_json::value clip_groups;
    for (auto group : m_Groups)
    {
        imgui_json::value media_group;
        group.Save(media_group);
        clip_groups.push_back(media_group);
    }
    if (m_Groups.size() > 0) value["MediaGroup"] = clip_groups;

    // save media overlap
    imgui_json::value overlaps;
    for (auto overlap : m_Overlaps)
    {
        imgui_json::value media_overlap;
        overlap->Save(media_overlap);
        overlaps.push_back(media_overlap);
    }
    if (m_Overlaps.size() > 0) value["MediaOverlap"] = overlaps;

    // save media track
    imgui_json::value media_tracks;
    for (auto track : m_Tracks)
    {
        imgui_json::value media_track;
        track->Save(media_track);
        media_tracks.push_back(media_track);
    }
    if (m_Tracks.size() > 0) value["MediaTrack"] = media_tracks;

    // save audio attribute
    imgui_json::value audio_attr;
    {
        // gain
        audio_attr["AudioGain"] = imgui_json::number(mAudioAttribute.mAudioGain);
        // pan
        audio_attr["AudioPanEnabled"] = imgui_json::boolean(mAudioAttribute.bPan);
        audio_attr["AudioPan"] = imgui_json::vec2(mAudioAttribute.audio_pan);
        // limiter
        audio_attr["AudioLimiterEnabled"] = imgui_json::boolean(mAudioAttribute.bLimiter);
        imgui_json::value audio_attr_limiter;
        {
            audio_attr_limiter["limit"] = imgui_json::number(mAudioAttribute.limit);
            audio_attr_limiter["attack"] = imgui_json::number(mAudioAttribute.limiter_attack);
            audio_attr_limiter["release"] = imgui_json::number(mAudioAttribute.limiter_release);
        }
        audio_attr["AudioLimiter"] = audio_attr_limiter;
        // gate
        audio_attr["AudioGateEnabled"] = imgui_json::boolean(mAudioAttribute.bGate);
        imgui_json::value audio_attr_gate;
        {
            audio_attr_gate["threshold"] = imgui_json::number(mAudioAttribute.gate_thd);
            audio_attr_gate["range"] = imgui_json::number(mAudioAttribute.gate_range);
            audio_attr_gate["ratio"] = imgui_json::number(mAudioAttribute.gate_ratio);
            audio_attr_gate["attack"] = imgui_json::number(mAudioAttribute.gate_attack);
            audio_attr_gate["release"] = imgui_json::number(mAudioAttribute.gate_release);
            audio_attr_gate["makeup"] = imgui_json::number(mAudioAttribute.gate_makeup);
            audio_attr_gate["knee"] = imgui_json::number(mAudioAttribute.gate_knee);
        }
        audio_attr["AudioGate"] = audio_attr_gate;
        // compressor
        audio_attr["AudioCompressorEnabled"] = imgui_json::boolean(mAudioAttribute.bCompressor);
        imgui_json::value audio_attr_compressor;
        {
            audio_attr_compressor["threshold"] = imgui_json::number(mAudioAttribute.compressor_thd);
            audio_attr_compressor["ratio"] = imgui_json::number(mAudioAttribute.compressor_ratio);
            audio_attr_compressor["knee"] = imgui_json::number(mAudioAttribute.compressor_knee);
            audio_attr_compressor["mix"] = imgui_json::number(mAudioAttribute.compressor_mix);
            audio_attr_compressor["attack"] = imgui_json::number(mAudioAttribute.compressor_attack);
            audio_attr_compressor["release"] = imgui_json::number(mAudioAttribute.compressor_release);
            audio_attr_compressor["makeup"] = imgui_json::number(mAudioAttribute.compressor_makeup);
            audio_attr_compressor["levelIn"] = imgui_json::number(mAudioAttribute.compressor_level_sc);
        }
        audio_attr["AudioCompressor"] = audio_attr_compressor;
        // equalizer
        audio_attr["AudioEqualizerEnabled"] = imgui_json::boolean(mAudioAttribute.bEqualizer);
        imgui_json::array bandGains;
        for (auto& bandCfg : mAudioAttribute.mBandCfg)
            bandGains.push_back(imgui_json::number(bandCfg.gain));
        audio_attr["AudioEqualizer"] = bandGains;
    }
    value["AudioAttribute"] = audio_attr;

    // save global timeline info
    value["Start"] = imgui_json::number(mStart);
    value["End"] = imgui_json::number(mEnd);
    value["VideoWidth"] = imgui_json::number(mhMediaSettings->VideoOutWidth());
    value["VideoHeight"] = imgui_json::number(mhMediaSettings->VideoOutHeight());
    value["PreviewScale"] = imgui_json::number(mPreviewScale);
    const auto frameRate = mhMediaSettings->VideoOutFrameRate();
    value["FrameRateNum"] = imgui_json::number(frameRate.num);
    value["FrameRateDen"] = imgui_json::number(frameRate.den);
    value["AudioChannels"] = imgui_json::number(mhMediaSettings->AudioOutChannels());
    value["AudioSampleRate"] = imgui_json::number(mhMediaSettings->AudioOutSampleRate());
    value["AudioRenderFormat"] = imgui_json::number(mAudioRenderFormat);
    value["msPixelWidth"] = imgui_json::number(msPixelWidthTarget);
    value["FirstTime"] = imgui_json::number(firstTime);
    value["CurrentTime"] = imgui_json::number(mCurrentTime);
    value["MarkIn"] = imgui_json::number(mark_in);
    value["MarkOut"] = imgui_json::number(mark_out);
    value["PreviewForward"] = imgui_json::boolean(mIsPreviewForward);
    value["Loop"] = imgui_json::boolean(bLoop);
    value["Compare"] = imgui_json::boolean(bCompare);
    value["TransitionOutPreview"] = imgui_json::boolean(bTransitionOutputPreview);
    value["SelectLinked"] = imgui_json::boolean(bSelectLinked);
    value["MovingAttract"] = imgui_json::boolean(bMovingAttract);
    value["IDGenerateState"] = imgui_json::number(m_IDGenerator.State());
    value["FontName"] = mFontName;
    value["OutputName"] = mOutputName;
    value["OutputPath"] = mOutputPath;
    value["OutputVideoCode"] = mVideoCodec;
    value["OutputAudioCode"] = mAudioCodec;
    value["OutputVideo"] = imgui_json::boolean(bExportVideo);
    value["OutputAudio"] = imgui_json::boolean(bExportAudio);
    value["SortMethod"] = imgui_json::number(mSortMethod);
}

void TimeLine::PrintActionList(const std::string& title, const std::list<imgui_json::value>& actionList)
{
    Logger::Log(Logger::VERBOSE) << std::endl << title << " : [" << std::endl;
    if (actionList.empty())
    {
        Logger::Log(Logger::VERBOSE) << "(EMPTY)" << std::endl;
    }
    else
    {
        for (auto& action : actionList)
            Logger::Log(Logger::VERBOSE) << "\t" << action.dump() << "," << std::endl;
    }
    Logger::Log(Logger::VERBOSE) << "] #" << title << std::endl << std::endl;
}

void TimeLine::PrintActionList(const std::string& title, const imgui_json::array& actionList)
{
    Logger::Log(Logger::VERBOSE) << std::endl << title << " : [" << std::endl;
    if (actionList.empty())
    {
        Logger::Log(Logger::VERBOSE) << "(EMPTY)" << std::endl;
    }
    else
    {
        for (auto& action : actionList)
            Logger::Log(Logger::VERBOSE) << "\t" << action.dump() << "," << std::endl;
    }
    Logger::Log(Logger::VERBOSE) << "] #" << title << std::endl << std::endl;
}

void TimeLine::PerformUiActions()
{
#if UI_PERFORMANCE_ANALYSIS
    MediaCore::AutoSection _as("PerfUiActs");
#endif
    if (mUiActions.empty())
        return;

    PrintActionList("UiActions", mUiActions);
    for (auto& action : mUiActions)
    {
        if (action["action"].get<imgui_json::string>() == "BP_OPERATION")
            continue;

        uint32_t mediaType = MEDIA_UNKNOWN;
        if (action.contains("media_type"))
            mediaType = action["media_type"].get<imgui_json::number>();
        if (IS_VIDEO(mediaType))
        {
            if (IS_IMAGE(mediaType))
                PerformImageAction(action);
            else
                PerformVideoAction(action);
        }
        else if (IS_AUDIO(mediaType))
            PerformAudioAction(action);
        else if (IS_TEXT(mediaType))
            PerformTextAction(action);
        else if (mediaType != MEDIA_UNKNOWN)
        {
            Logger::Log(Logger::DEBUG) << "Skip action due to unsupported MEDIA_TYPE: " << action.dump() << "." << std::endl;
            continue;
        }
    }
    if (!mUiActions.empty())
    {
        SyncDataLayer();
    }

    mUiActions.clear();
}

void TimeLine::PerformVideoAction(imgui_json::value& action)
{
    std::string actionName = action["action"].get<imgui_json::string>();
    if (actionName == "ADD_CLIP")
    {
#if UI_PERFORMANCE_ANALYSIS
        MediaCore::AutoSection _as("UiAct_AddVidClip");
        auto hPa = MediaCore::PerformanceAnalyzer::GetThreadLocalInstance();
#endif
        int64_t trackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId, true);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        auto pUiVClip = dynamic_cast<VideoClip*>(FindClipByID(clipId));
        IM_ASSERT(pUiVClip);
        MediaCore::VideoClip::Holder hVidClip = MediaCore::VideoClip::CreateVideoInstance(
            pUiVClip->mID, pUiVClip->mMediaParser, mMtvReader->GetSharedSettings(),
            pUiVClip->Start(), pUiVClip->End(), pUiVClip->StartOffset(), pUiVClip->EndOffset(), mCurrentTime-pUiVClip->Start(), vidTrack->Direction());
        pUiVClip->SetDataLayer(hVidClip, false);
        vidTrack->InsertClip(hVidClip);
        bool updateDuration = true;
        if (action.contains("update_duration"))
            updateDuration = action["update_duration"].get<imgui_json::boolean>();
        RefreshPreview(updateDuration);
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        vidTrack->RemoveClipById(clipId);
        bool updateDuration = true;
        if (action.contains("update_duration"))
            updateDuration = action["update_duration"].get<imgui_json::boolean>();
        RefreshPreview(updateDuration);
    }
    else if (actionName == "MOVE_CLIP")
    {
        int64_t srcTrackId = action["from_track_id"].get<imgui_json::number>();
        int64_t dstTrackId = srcTrackId;
        if (action.contains("to_track_id"))
            dstTrackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder dstVidTrack = mMtvReader->GetTrackById(dstTrackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStart = action["new_start"].get<imgui_json::number>();
        if (srcTrackId != dstTrackId)
        {
            MediaCore::VideoTrack::Holder srcVidTrack = mMtvReader->GetTrackById(srcTrackId);
            MediaCore::VideoClip::Holder vidClip = srcVidTrack->RemoveClipById(clipId);
            vidClip->SetStart(newStart);
            dstVidTrack->InsertClip(vidClip);
        }
        else
        {
            dstVidTrack->MoveClip(clipId, newStart);
        }
        RefreshPreview();
    }
    else if (actionName == "CROP_CLIP")
    {
#if UI_PERFORMANCE_ANALYSIS
        MediaCore::AutoSection _as("UiAct_CropVidClip");
        auto hPa = MediaCore::PerformanceAnalyzer::GetThreadLocalInstance();
#endif
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStartOffset = action["new_start_offset"].get<imgui_json::number>();
        int64_t newEndOffset = action["new_end_offset"].get<imgui_json::number>();
        vidTrack->ChangeClipRange(clipId, newStartOffset, newEndOffset);
        bool updateDuration = true;
        if (action.contains("update_duration"))
            updateDuration = action["update_duration"].get<imgui_json::boolean>();
        RefreshPreview(updateDuration);
    }
    else if (actionName == "CUT_CLIP")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        auto hVidTrk = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        auto hClip = hVidTrk->GetClipById(clipId);
        int64_t newClipStart = action["new_clip_start"].get<imgui_json::number>();
        int64_t newClipEnd = hClip->End();
        int64_t newClipStartOffset = hClip->StartOffset()+newClipStart-hClip->Start();
        int64_t newClipEndOffset = hClip->EndOffset();
        hVidTrk->ChangeClipRange(clipId, hClip->StartOffset(), hClip->EndOffset()+hClip->End()-newClipStart);
        int64_t newClipId = action["new_clip_id"].get<imgui_json::number>();
        MediaCore::VideoClip::Holder hNewClip = MediaCore::VideoClip::CreateVideoInstance(
            newClipId, hClip->GetMediaParser(), mMtvReader->GetSharedSettings(),
            newClipStart, newClipEnd, newClipStartOffset, newClipEndOffset, mCurrentTime-newClipStart, hVidTrk->Direction());
        MediaCore::VideoFilter::Holder hNewFilter;
        if (hClip->GetFilter())
            hNewFilter = hClip->GetFilter()->Clone(mhPreviewSettings);
        if (hNewFilter)
        {
            const auto filterName = hNewFilter->GetFilterName();
            if (filterName == "EventStackFilter")
            {
                MEC::VideoEventStackFilter* pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(hNewFilter.get());
                BluePrint::BluePrintCallbackFunctions bpCallbacks;
                bpCallbacks.BluePrintOnChanged = TimeLine::OnVideoEventStackFilterBpChanged;
                pEsf->SetBluePrintCallbacks(bpCallbacks);
                pEsf->MoveAllEvents(hClip->Start()-newClipStart);
                // generate new ids for cloned events
                auto eventList = pEsf->GetEventList();
                for (auto& e : eventList)
                    e->ChangeId(m_IDGenerator.GenerateID());
            }
            else
                Logger::Log(Logger::WARN) << "UNHANDLED video filter type '" << filterName << "'." << std::endl;
            hNewClip->SetFilter(hNewFilter);
        }
        auto pUiClip = dynamic_cast<VideoClip*>(FindClipByID(newClipId));
        pUiClip->SetDataLayer(hNewClip, true);
        hVidTrk->InsertClip(hNewClip);
        RefreshPreview(false);
    }
    else if (actionName == "ADD_TRACK")
    {
        int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
        int64_t afterId = action["after_track_id"].get<imgui_json::number>();
        mMtvReader->AddTrack(trackId, afterId);
    }
    else if (actionName == "REMOVE_TRACK")
    {
        int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
        mMtvReader->RemoveTrackById(trackId);
    }
    else if (actionName == "MOVE_TRACK")
    {
        if (action.contains("track_id2"))
        {
            int64_t trackId1 = action["track_id1"].get<imgui_json::number>();
            int64_t trackId2 = action["track_id2"].get<imgui_json::number>();
            mMtvReader->ChangeTrackViewOrder(trackId1, trackId2);
            RefreshPreview();
        }
    }
    else if (actionName == "HIDE_TRACK")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        bool visible = action["visible"].get<imgui_json::boolean>();
        mMtvReader->SetTrackVisible(trackId, visible);
        RefreshPreview();
    }
    else if (actionName == "ADD_EVENT" || actionName == "DELETE_EVENT" || actionName == "MOVE_EVENT" || actionName == "CROP_EVENT")
    {
        // skip handle these actions
    }
    else
    {
        Logger::Log(Logger::WARN) << "UNHANDLED UI ACTION(Video): '" << actionName << "'." << std::endl;
    }
}

void TimeLine::PerformAudioAction(imgui_json::value& action)
{
    std::string actionName = action["action"].get<imgui_json::string>();
    if (actionName == "ADD_CLIP")
    {
#if UI_PERFORMANCE_ANALYSIS
        MediaCore::AutoSection _as("UiAct_AddAudClip");
        auto hPa = MediaCore::PerformanceAnalyzer::GetThreadLocalInstance();
#endif
        int64_t trackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::AudioTrack::Holder audTrack = mMtaReader->GetTrackById(trackId, true);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        auto pUiAClip = dynamic_cast<AudioClip*>(FindClipByID(clipId));
        MediaCore::AudioClip::Holder hAudClip = MediaCore::AudioClip::CreateInstance(
            pUiAClip->mID, pUiAClip->mMediaParser, mMtaReader->GetTrackSharedSettings(),
            pUiAClip->Start(), pUiAClip->End(), pUiAClip->StartOffset(), pUiAClip->EndOffset());
        pUiAClip->SetDataLayer(hAudClip, true);
        audTrack->InsertClip(hAudClip);
        bool updateDuration = true;
        if (action.contains("update_duration"))
            updateDuration = action["update_duration"].get<imgui_json::boolean>();
        mMtaReader->Refresh(updateDuration);
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::AudioTrack::Holder audTrack = mMtaReader->GetTrackById(trackId);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        audTrack->RemoveClipById(clipId);
        bool updateDuration = true;
        if (action.contains("update_duration"))
            updateDuration = action["update_duration"].get<imgui_json::boolean>();
        mMtaReader->Refresh(updateDuration);
    }
    else if (actionName == "MOVE_CLIP")
    {
        int64_t srcTrackId = action["from_track_id"].get<imgui_json::number>();
        int64_t dstTrackId = srcTrackId;
        if (action.contains("to_track_id"))
            dstTrackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::AudioTrack::Holder dstAudTrack = mMtaReader->GetTrackById(dstTrackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStart = action["new_start"].get<imgui_json::number>();
        if (srcTrackId != dstTrackId)
        {
            MediaCore::AudioTrack::Holder srcAudTrack = mMtaReader->GetTrackById(srcTrackId);
            MediaCore::AudioClip::Holder audClip = srcAudTrack->RemoveClipById(clipId);
            audClip->SetStart(newStart);
            dstAudTrack->InsertClip(audClip);
        }
        else
        {
            dstAudTrack->MoveClip(clipId, newStart);
        }
        mMtaReader->Refresh();
    }
    else if (actionName == "CROP_CLIP")
    {
#if UI_PERFORMANCE_ANALYSIS
        MediaCore::AutoSection _as("UiAct_CropAudClip");
        auto hPa = MediaCore::PerformanceAnalyzer::GetThreadLocalInstance();
#endif
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::AudioTrack::Holder audTrack = mMtaReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStartOffset = action["new_start_offset"].get<imgui_json::number>();
        int64_t newEndOffset = action["new_end_offset"].get<imgui_json::number>();
        audTrack->ChangeClipRange(clipId, newStartOffset, newEndOffset);
        bool updateDuration = true;
        if (action.contains("update_duration"))
            updateDuration = action["update_duration"].get<imgui_json::boolean>();
        mMtaReader->Refresh(updateDuration);
    }
    else if (actionName == "CUT_CLIP")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        auto hAudTrk = mMtaReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        auto hClip = hAudTrk->GetClipById(clipId);
        int64_t newClipStart = action["new_clip_start"].get<imgui_json::number>();
        int64_t newClipEnd = hClip->End();
        int64_t newClipStartOffset = hClip->StartOffset()+newClipStart-hClip->Start();
        int64_t newClipEndOffset = hClip->EndOffset();
        hAudTrk->ChangeClipRange(clipId, hClip->StartOffset(), hClip->EndOffset()+hClip->End()-newClipStart);
        int64_t newClipId = action["new_clip_id"].get<imgui_json::number>();
        MediaCore::AudioClip::Holder hNewClip = MediaCore::AudioClip::CreateInstance(
            newClipId, hClip->GetMediaParser(), mMtaReader->GetTrackSharedSettings(),
            newClipStart, newClipEnd, newClipStartOffset, newClipEndOffset);
        MediaCore::AudioFilter::Holder hNewFilter;
        if (hClip->GetFilter())
            hNewFilter = hClip->GetFilter()->Clone();
        if (hNewFilter)
        {
            const auto filterName = hNewFilter->GetFilterName();
            if (filterName == "EventStackFilter")
            {
                MEC::AudioEventStackFilter* pEsf = dynamic_cast<MEC::AudioEventStackFilter*>(hNewFilter.get());
                BluePrint::BluePrintCallbackFunctions bpCallbacks;
                bpCallbacks.BluePrintOnChanged = TimeLine::OnAudioEventStackFilterBpChanged;
                pEsf->SetBluePrintCallbacks(bpCallbacks);
                pEsf->MoveAllEvents(hClip->Start()-newClipStart);
                // generate new ids for cloned events
                auto eventList = pEsf->GetEventList();
                for (auto& e : eventList)
                    e->ChangeId(m_IDGenerator.GenerateID());
            }
            else
                Logger::Log(Logger::WARN) << "UNHANDLED audio filter type '" << filterName << "'." << std::endl;
            hNewClip->SetFilter(hNewFilter);
        }
        auto pUiClip = dynamic_cast<AudioClip*>(FindClipByID(newClipId));
        pUiClip->SetDataLayer(hNewClip, true);
        hAudTrk->InsertClip(hNewClip);
        mMtaReader->Refresh(false);
    }
    else if (actionName == "ADD_TRACK")
    {
        int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
        mMtaReader->AddTrack(trackId);
    }
    else if (actionName == "REMOVE_TRACK")
    {
        int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
        mMtaReader->RemoveTrackById(trackId);
    }
    else if (actionName == "MOVE_TRACK")
    {
        // currently need to do nothing
    }
    else if (actionName == "MUTE_TRACK")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        bool muted = action["muted"].get<imgui_json::boolean>();
        mMtaReader->SetTrackMuted(trackId, muted);
    }
    else
    {
        Logger::Log(Logger::WARN) << "UNHANDLED UI ACTION(Audio): '" << actionName << "'." << std::endl;
    }
}

void TimeLine::PerformImageAction(imgui_json::value& action)
{
    std::string actionName = action["action"].get<imgui_json::string>();
    if (actionName == "ADD_CLIP")
    {
        int64_t trackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId, true);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        auto pUiVClip = dynamic_cast<VideoClip*>(FindClipByID(clipId));
        IM_ASSERT(pUiVClip);
        MediaCore::VideoClip::Holder hImgClip = MediaCore::VideoClip::CreateImageInstance(
            pUiVClip->mID, pUiVClip->mMediaParser, mMtvReader->GetSharedSettings(),
            pUiVClip->Start(), pUiVClip->Length());
        pUiVClip->SetDataLayer(hImgClip, false);
        vidTrack->InsertClip(hImgClip);
        RefreshPreview();
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        vidTrack->RemoveClipById(clipId);
        RefreshPreview();
    }
    else if (actionName == "MOVE_CLIP")
    {
        int64_t srcTrackId = action["from_track_id"].get<imgui_json::number>();
        int64_t dstTrackId = srcTrackId;
        if (action.contains("to_track_id"))
            dstTrackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder dstVidTrack = mMtvReader->GetTrackById(dstTrackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStart = action["new_start"].get<imgui_json::number>();
        if (srcTrackId != dstTrackId)
        {
            MediaCore::VideoTrack::Holder srcVidTrack = mMtvReader->GetTrackById(srcTrackId);
            MediaCore::VideoClip::Holder imgClip = srcVidTrack->RemoveClipById(clipId);
            imgClip->SetStart(newStart);
            dstVidTrack->InsertClip(imgClip);
        }
        else
        {
            dstVidTrack->MoveClip(clipId, newStart);
        }
        RefreshPreview();
    }
    else if (actionName == "CROP_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStart = action["new_start"].get<imgui_json::number>();
        int64_t newEnd = action["new_end"].get<imgui_json::number>();
        vidTrack->ChangeClipRange(clipId, newStart, newEnd);
        RefreshPreview();
    }
    else if (actionName == "CUT_CLIP")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        auto hVidTrk = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        auto hClip = hVidTrk->GetClipById(clipId);
        int64_t newClipStart = action["new_clip_start"].get<imgui_json::number>();
        int64_t newClipLength = hClip->End()-newClipStart;
        hVidTrk->ChangeClipRange(clipId, hClip->Start(), newClipStart);
        int64_t newClipId = action["new_clip_id"].get<imgui_json::number>();
        MediaCore::VideoClip::Holder hNewClip = MediaCore::VideoClip::CreateImageInstance(
            newClipId, hClip->GetMediaParser(), mMtvReader->GetSharedSettings(),
            newClipStart, newClipLength);
        auto hNewFilter = hClip->GetFilter()->Clone(mhPreviewSettings);
        if (hNewFilter)
        {
            const auto filterName = hNewFilter->GetFilterName();
            if (filterName == "EventStackFilter")
            {
                MEC::VideoEventStackFilter* pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(hNewFilter.get());
                BluePrint::BluePrintCallbackFunctions bpCallbacks;
                bpCallbacks.BluePrintOnChanged = TimeLine::OnVideoEventStackFilterBpChanged;
                pEsf->SetBluePrintCallbacks(bpCallbacks);
                pEsf->MoveAllEvents(hClip->Start()-newClipStart);
                // generate new ids for cloned events
                auto eventList = pEsf->GetEventList();
                for (auto& e : eventList)
                    e->ChangeId(m_IDGenerator.GenerateID());
            }
            else
                Logger::Log(Logger::WARN) << "UNHANDLED video filter type '" << filterName << "'." << std::endl;
            hNewClip->SetFilter(hNewFilter);
        }
        auto pUiClip = dynamic_cast<VideoClip*>(FindClipByID(newClipId));
        pUiClip->SetDataLayer(hNewClip, true);
        hVidTrk->InsertClip(hNewClip);
        RefreshPreview(false);
    }
    else if (actionName == "ADD_TRACK")
    {
        int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
        int64_t afterId = action["after_track_id"].get<imgui_json::number>();
        mMtvReader->AddTrack(trackId);
    }
    else if (actionName == "REMOVE_TRACK")
    {
        int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
        mMtvReader->RemoveTrackById(trackId);
    }
    else if (actionName == "MOVE_TRACK")
    {
        throw std::runtime_error("'MOVE_TRACK' operation shouldn't happen as an image action!");
    }
    else
    {
        Logger::Log(Logger::WARN) << "UNHANDLED UI ACTION(Image): '" << actionName << "'." << std::endl;
    }
}

void TimeLine::PerformTextAction(imgui_json::value& action)
{
    std::string actionName = action["action"].get<imgui_json::string>();
    if (actionName == "ADD_TRACK")
    {
        Logger::Log(Logger::INFO) << "Adding TEXT track is handled else where.." << std::endl;
    }
    else if (actionName == "REMOVE_TRACK")
    {
        int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
        mMtvReader->RemoveSubtitleTrackById(trackId);
    }
    else if (actionName == "MOVE_TRACK")
    {
        // currently need to do nothing
    }
    else
    {
        Logger::Log(Logger::WARN) << "UNHANDLED UI ACTION(Text): '" << actionName << "'." << std::endl;
    }
}

int TimeLine::OnVideoEventStackFilterBpChanged(int type, std::string name, void* handle)
{
    auto pFilterCtx = reinterpret_cast<MEC::EventStackFilterContext*>(handle);
    if (!pFilterCtx)
        return BluePrint::BP_CBR_Unknown;

    int ret = BluePrint::BP_CBR_Nothing;
    MEC::VideoEventStackFilter* pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(reinterpret_cast<MEC::EventStack*>(pFilterCtx->pFilterPtr));
    TimeLine* timeline = (TimeLine*)pEsf->GetTimelineHandle();
    MEC::Event* pEvt = reinterpret_cast<MEC::Event*>(pFilterCtx->pEventPtr);
    bool needUpdateView = false;
    if (type == BluePrint::BP_CB_Link ||
        type == BluePrint::BP_CB_Unlink ||
        type == BluePrint::BP_CB_NODE_DELETED ||
        type == BluePrint::BP_CB_NODE_APPEND ||
        type == BluePrint::BP_CB_NODE_INSERT)
    {
        needUpdateView = true;
        ret = BluePrint::BP_CBR_AutoLink;
    }
    else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
            type == BluePrint::BP_CB_SETTING_CHANGED)
    {
        needUpdateView = true;
    }
    else if (type == BluePrint::BP_CB_OPERATION_DONE)
    {
        auto pBp = pEvt->GetBp();
        imgui_json::value opRecord = pBp->Blueprint_GetOpRecord();
        if (opRecord.contains("operation") && opRecord.contains("before_op_state") && opRecord.contains("after_op_state"))
        {
            imgui_json::value action;
            action["action"] = "BP_OPERATION";
            action["media_type"] = imgui_json::number(MEDIA_VIDEO);
            action["clip_id"] = imgui_json::number(pEsf->GetVideoClip()->Id());
            action["event_id"] = imgui_json::number(pEvt->Id());
            action["bp_operation"] = opRecord["operation"];
            action["before_op_state"] = opRecord["before_op_state"];
            action["after_op_state"] = opRecord["after_op_state"];
            timeline->mUiActions.push_back(std::move(action));
        }
    }
    else
    {
        Logger::Log(Logger::WARN) << "---> Ignore 'OnVideoEventStackFilterBpChanged' change type " << type << "." << std::endl;
    }
    if (needUpdateView)
    {
        auto pClip = pEsf->GetVideoClip();
        auto trackId = pClip->TrackId();
        timeline->mNeedUpdateTrackIds.insert(trackId);
    }
    if (timeline) timeline->mIsBluePrintChanged = true;
    return ret;
}

int TimeLine::OnAudioEventStackFilterBpChanged(int type, std::string name, void* handle)
{
    auto pFilterCtx = reinterpret_cast<MEC::EventStackFilterContext*>(handle);
    if (!pFilterCtx)
        return BluePrint::BP_CBR_Unknown;

    int ret = BluePrint::BP_CBR_Nothing;
    MEC::AudioEventStackFilter* pEsf = dynamic_cast<MEC::AudioEventStackFilter*>(reinterpret_cast<MEC::EventStack*>(pFilterCtx->pFilterPtr));
    TimeLine* timeline = (TimeLine*)pEsf->GetTimelineHandle();
    MEC::Event* pEvt = reinterpret_cast<MEC::Event*>(pFilterCtx->pEventPtr);
    if (type == BluePrint::BP_CB_OPERATION_DONE)
    {
        auto pBp = pEvt->GetBp();
        imgui_json::value opRecord = pBp->Blueprint_GetOpRecord();
        if (opRecord.contains("operation") && opRecord.contains("before_op_state") && opRecord.contains("after_op_state"))
        {
            imgui_json::value action;
            action["action"] = "BP_OPERATION";
            action["media_type"] = imgui_json::number(MEDIA_AUDIO);
            action["clip_id"] = imgui_json::number(pEsf->GetAudioClip()->Id());
            action["event_id"] = imgui_json::number(pEvt->Id());
            action["bp_operation"] = opRecord["operation"];
            action["before_op_state"] = opRecord["before_op_state"];
            action["after_op_state"] = opRecord["after_op_state"];
            timeline->mUiActions.push_back(std::move(action));
        }
    }
    else
    {
        Logger::Log(Logger::WARN) << "---> Ignore 'OnAudioEventStackFilterBpChanged' change type " << type << "." << std::endl;
    }
    if (timeline) timeline->mIsBluePrintChanged = true;
    return ret;
}

void TimeLine::ConfigureDataLayer()
{
    mMtvReader = MediaCore::MultiTrackVideoReader::CreateInstance();
    mMtvReader->Configure(mhPreviewSettings);
    mMtvReader->Start();
    mMtaReader = MediaCore::MultiTrackAudioReader::CreateInstance();
    mMtaReader->Configure(mhPreviewSettings);
    mMtaReader->Start();
    mPcmStream.SetAudioReader(mMtaReader);
}

void TimeLine::SyncDataLayer(bool forceRefresh)
{
    // video overlap
    int syncedOverlapCount = 0;
    bool needUpdatePreview = false;
    bool needRefreshAudio = false;
    auto vidTrackIter = mMtvReader->TrackListBegin();
    while (vidTrackIter != mMtvReader->TrackListEnd())
    {
        auto& vidTrack = *vidTrackIter++;
        vidTrack->UpdateClipState();
        auto ovlpList = vidTrack->GetOverlapList();
        auto ovlpIter = ovlpList.begin();
        while (ovlpIter != ovlpList.end())
        {
            auto& vidOvlp = *ovlpIter++;
            const int64_t frontClipId = vidOvlp->FrontClip()->Id();
            const int64_t rearClipId = vidOvlp->RearClip()->Id();
            bool found = false;
            for (auto ovlp : m_Overlaps)
            {
                if ((ovlp->m_Clip.first == frontClipId && ovlp->m_Clip.second == rearClipId) ||
                    (ovlp->m_Clip.first == rearClipId && ovlp->m_Clip.second == frontClipId))
                {
                    if (vidOvlp->Id() != ovlp->mID)
                    {
                        vidOvlp->SetId(ovlp->mID);
                        BluePrintVideoTransition* bpvt = new BluePrintVideoTransition(this);
                        bpvt->SetBluePrintFromJson(ovlp->mTransitionBP);
                        bpvt->SetKeyPoint(ovlp->mTransitionKeyPoints);
                        MediaCore::VideoTransition::Holder hTrans(bpvt);
                        vidOvlp->SetTransition(hTrans);
                        needUpdatePreview = true;
                    }
                    found = true;
                    break;
                }
            }
            if (!found)
                Logger::Log(Logger::Error) << "CANNOT find matching video OVERLAP! Front clip id is " << frontClipId
                    << ", rear clip id is " << rearClipId << "." << std::endl;
            else
                syncedOverlapCount++;
        }
    }
    // audio overlap
    auto audTrackIter = mMtaReader->TrackListBegin();
    while (audTrackIter != mMtaReader->TrackListEnd())
    {
        auto& audTrack = *audTrackIter++;
        auto ovlpIter = audTrack->OverlapListBegin();
        while (ovlpIter != audTrack->OverlapListEnd())
        {
            auto& audOvlp = *ovlpIter++;
            const int64_t frontClipId = audOvlp->FrontClip()->Id();
            const int64_t rearClipId = audOvlp->RearClip()->Id();
            bool found = false;
            for (auto ovlp : m_Overlaps)
            {
                if ((ovlp->m_Clip.first == frontClipId && ovlp->m_Clip.second == rearClipId) ||
                    (ovlp->m_Clip.first == rearClipId && ovlp->m_Clip.second == frontClipId))
                {
                    if (audOvlp->Id() != ovlp->mID)
                    {
                        audOvlp->SetId(ovlp->mID);
                        BluePrintAudioTransition* bpat = new BluePrintAudioTransition(this);
                        bpat->SetBluePrintFromJson(ovlp->mTransitionBP);
                        bpat->SetKeyPoint(ovlp->mTransitionKeyPoints);
                        MediaCore::AudioTransition::Holder hTrans(bpat);
                        audOvlp->SetTransition(hTrans);
                        needRefreshAudio = true;
                    }
                    found = true;
                    break;
                }
            }
            if (!found)
                Logger::Log(Logger::Error) << "CANNOT find matching audio OVERLAP! Front clip id is " << frontClipId
                    << ", rear clip id is " << rearClipId << "." << std::endl;
            else
                syncedOverlapCount++;
        }
    }
    if (needUpdatePreview || forceRefresh)
        RefreshPreview();
    if (needRefreshAudio || forceRefresh)
        mMtaReader->Refresh();

    int OvlpCnt = 0;
    for (auto ovlp : m_Overlaps)
    {
        if (IS_VIDEO(ovlp->mType))
            OvlpCnt++;
        if (IS_AUDIO(ovlp->mType))
            OvlpCnt ++;
    }
    Logger::Log(Logger::VERBOSE) << std::endl << mMtvReader << std::endl;
    Logger::Log(Logger::VERBOSE) << mMtaReader << std::endl << std::endl;
    if (syncedOverlapCount != OvlpCnt)
        Logger::Log(Logger::Error) << "Overlap SYNC FAILED! Synced count is " << syncedOverlapCount
            << ", while the count of video overlap array is " << OvlpCnt << "." << std::endl;
}

MediaCore::Snapshot::Generator::Holder TimeLine::GetSnapshotGenerator(int64_t mediaItemId)
{
    auto iter = m_VidSsGenTable.find(mediaItemId);
    if (iter != m_VidSsGenTable.end())
        return iter->second;
    MediaItem* mi = FindMediaItemByID(mediaItemId);
    if (!mi)
        return nullptr;
    if (!IS_VIDEO(mi->mMediaType) || IS_IMAGE(mi->mMediaType))
        return nullptr;
    MediaCore::Snapshot::Generator::Holder hSsGen = MediaCore::Snapshot::Generator::CreateInstance();
    hSsGen->SetOverview(mi->mMediaOverview);
    hSsGen->EnableHwAccel(mHardwareCodec);
    if (!hSsGen->Open(mi->mMediaOverview->GetMediaParser(), mhMediaSettings->VideoOutFrameRate()))
    {
        Logger::Log(Logger::Error) << hSsGen->GetError() << std::endl;
        return nullptr;
    }
    RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
    if (mTxMgr->GetTexturePoolAttributes(VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME, tTxPoolAttrs))
    {
        hSsGen->SetSnapshotSize(tTxPoolAttrs.tTxSize.x, tTxPoolAttrs.tTxSize.y);
    }
    else
    {
        auto video_info = mi->mMediaOverview->GetMediaParser()->GetBestVideoStream();
        float snapshot_scale = video_info->height > 0 ? DEFAULT_VIDEO_TRACK_HEIGHT / (float)video_info->height : 0.05;
        hSsGen->SetSnapshotResizeFactor(snapshot_scale, snapshot_scale);
    }
    hSsGen->SetCacheFactor(3);
    if (visibleTime > 0 && msPixelWidthTarget > 0)
    {
        const MediaCore::VideoStream* video_stream = hSsGen->GetVideoStream();
        float snapHeight = DEFAULT_VIDEO_TRACK_HEIGHT;  // TODO: video clip UI height is hard coded here, should be fixed later (wyvern)
        MediaCore::Ratio displayAspectRatio = {
            (int32_t)(video_stream->width * video_stream->sampleAspectRatio.num), (int32_t)(video_stream->height * video_stream->sampleAspectRatio.den) };
        float snapWidth = snapHeight * displayAspectRatio.num / displayAspectRatio.den;
        double windowSize = (double)visibleTime / 1000;
        if (windowSize > video_stream->duration)
            windowSize = video_stream->duration;
        double snapsInViewWindow = std::max((float)((double)msPixelWidthTarget*windowSize * 1000 / snapWidth), 1.f);
        if (!hSsGen->ConfigSnapWindow(windowSize, snapsInViewWindow))
            throw std::runtime_error(hSsGen->GetError());
    }
    m_VidSsGenTable[mediaItemId] = hSsGen;
    return hSsGen;
}

void TimeLine::ConfigSnapshotWindow(int64_t viewWndDur)
{
    if (visibleTime == viewWndDur)
        return;

    visibleTime = viewWndDur;
    for (auto& elem : m_VidSsGenTable)
    {
        auto& ssGen = elem.second;
        const MediaCore::VideoStream* video_stream = ssGen->GetVideoStream();
        float snapHeight = DEFAULT_VIDEO_TRACK_HEIGHT;  // TODO: video clip UI height is hard coded here, should be fixed later (wyvern)
        MediaCore::Ratio displayAspectRatio = {
            (int32_t)(video_stream->width * video_stream->sampleAspectRatio.num), (int32_t)(video_stream->height * video_stream->sampleAspectRatio.den) };
        float snapWidth = snapHeight * displayAspectRatio.num / displayAspectRatio.den;
        double windowSize = (double)visibleTime / 1000;
        if (windowSize > video_stream->duration)
            windowSize = video_stream->duration;
        double snapsInViewWindow = std::max((float)((double)msPixelWidthTarget*windowSize * 1000 / snapWidth), 1.f);
        if (!ssGen->ConfigSnapWindow(windowSize, snapsInViewWindow))
            throw std::runtime_error(ssGen->GetError());
    }
    for (auto& track : m_Tracks)
        track->ConfigViewWindow(visibleTime, msPixelWidthTarget);
    MediaCore::Ratio timelineAspectRatio = { (int32_t)mhMediaSettings->VideoOutWidth(), (int32_t)mhMediaSettings->VideoOutHeight() };
    mSnapShotWidth = DEFAULT_VIDEO_TRACK_HEIGHT * (float)timelineAspectRatio.num / (float)timelineAspectRatio.den;
}

MatUtils::Size2i TimeLine::CalcPreviewSize(const MatUtils::Size2i& videoSize, float previewScale)
{
    if (previewScale <= 0.01 || previewScale >= 4)
    {
        Logger::Log(Logger::WARN) << "INVALID preview scale " << previewScale << " !" << std::endl;
        return videoSize;
    }
    auto previewWidth = (int32_t)round((float)videoSize.x*previewScale);
    previewWidth += previewWidth&0x1;
    auto previewHeight = (int32_t)round((float)videoSize.y*previewScale);
    previewHeight += previewHeight&0x1;
    return {previewWidth, previewHeight};
}

void TimeLine::UpdateVideoSettings(MediaCore::SharedSettings::Holder hSettings, float previewScale)
{
    auto hNewPreviewSettings = hSettings->Clone();
    auto previewSize = CalcPreviewSize({(int32_t)hSettings->VideoOutWidth(), (int32_t)hSettings->VideoOutHeight()}, previewScale);
    hNewPreviewSettings->SetVideoOutWidth(previewSize.x);
    hNewPreviewSettings->SetVideoOutHeight(previewSize.y);
    if (!mMtvReader->UpdateSettings(hNewPreviewSettings))
    {
        std::ostringstream oss; oss << "Update video settings FAILED! Error is '" << mMtvReader->GetError() << "'.";
        throw std::runtime_error(oss.str());
    }
    mhMediaSettings->SyncVideoSettingsFrom(hSettings.get());
    mhPreviewSettings = hNewPreviewSettings;
    mPreviewScale = previewScale;
    RenderUtils::TextureManager::TexturePoolAttributes tTxPoolAttrs;
    mTxMgr->GetTexturePoolAttributes(PREVIEW_TEXTURE_POOL_NAME, tTxPoolAttrs);
    tTxPoolAttrs.tTxSize = previewSize;
    mTxMgr->SetTexturePoolAttributes(PREVIEW_TEXTURE_POOL_NAME, tTxPoolAttrs);
    mhPreviewTx = mTxMgr->GetTextureFromPool(PREVIEW_TEXTURE_POOL_NAME);
    RefreshPreview(false);
}

void TimeLine::UpdateAudioSettings(MediaCore::SharedSettings::Holder hSettings, MediaCore::AudioRender::PcmFormat pcmFormat)
{
    mAudioRender->CloseDevice();
    mPcmStream.Flush();
    if (!mAudioRender->OpenDevice(hSettings->AudioOutSampleRate(), hSettings->AudioOutChannels(), pcmFormat, &mPcmStream))
        throw std::runtime_error("FAILED to open audio render device!");
    mAudioRenderFormat = pcmFormat;
    if (!mMtaReader->UpdateSettings(hSettings))
        Logger::Log(Logger::Error) << "FAILED to update audio settings!" << std::endl;
    mhMediaSettings->SyncAudioSettingsFrom(mhPreviewSettings.get());
    mAudioAttribute.channel_data.clear();
    mAudioAttribute.channel_data.resize(hSettings->AudioOutChannels());
    if (mIsPreviewPlaying)
        mAudioRender->Resume();
}

uint32_t TimeLine::SimplePcmStream::Read(uint8_t* buff, uint32_t buffSize, bool blocking)
{
    if (!m_areader)
        return 0;
    std::lock_guard<std::mutex> lk(m_amatLock);
    uint32_t readSize = 0;
    while (readSize < buffSize)
    {
        uint32_t amatTotalDataSize = m_amat.total()*m_amat.elemsize;
        if (m_readPosInAmat < amatTotalDataSize)
        {
            uint32_t copySize = buffSize-readSize;
            if (copySize > amatTotalDataSize)
                copySize = amatTotalDataSize;
            memcpy(buff+readSize, (uint8_t*)m_amat.data+m_readPosInAmat, copySize);
            readSize += copySize;
            m_readPosInAmat += copySize;
        }
        if (m_readPosInAmat >= amatTotalDataSize)
        {
            std::vector<MediaCore::CorrelativeFrame> amats;
            bool eof;
            if (!m_areader->ReadAudioSamplesEx(amats, eof))
                return 0;
            // main audio out
            m_amat = amats[0].frame;
            // if (!m_amat.empty())
            //     Logger::Log(Logger::INFO) << "=======> m_amat.timestamp=" << m_amat.time_stamp << std::endl;
            if (m_owner->mAudioAttribute.audio_mutex.try_lock())
            {
                m_owner->CalculateAudioScopeData(m_amat);
                m_owner->mAudioAttribute.audio_mutex.unlock();
            }
            // channel audio
            for (auto amat : amats)
            {
                if (amat.phase == MediaCore::CorrelativeFrame::PHASE_AFTER_TRANSITION)
                {
                    auto track = m_owner->FindTrackByID(amat.trackId);
                    if (track && IS_AUDIO(track->mType))
                    {
                        if (track->mAudioTrackAttribute.audio_mutex.try_lock())
                        {
                            track->CalculateAudioScopeData(amat.frame);
                            track->mAudioTrackAttribute.audio_mutex.unlock();
                        }
                    }
                }
            }
            m_readPosInAmat = 0;
        }
    }
    if (!m_amat.empty())
    {
        m_timestampMs = (int64_t)(m_amat.time_stamp*1000)+m_areader->SizeToDuration(m_readPosInAmat);
        m_tsValid = true;
    }
    return buffSize;
}

void TimeLine::SimplePcmStream::Flush()
{
    std::lock_guard<std::mutex> lk(m_amatLock);
    m_amat.release();
    m_readPosInAmat = 0;
    m_tsValid = false;
}

void TimeLine::CalculateAudioScopeData(ImGui::ImMat& mat_in)
{
    if (mat_in.empty() || mat_in.w < 64)
        return;
    const int fft_size = mat_in.w  > 256 ? 256 : mat_in.w > 128 ? 128 : 64;
    const int ch = mat_in.c;
    ImGui::ImMat mat;
    mat.create_type(fft_size, 1, ch, IM_DT_FLOAT32);
    // copy fft_size samples from input mat, and convert them into float type
    {
        float** ppDstPtrs = new float*[ch];
        for (int i = 0; i < ch; i++)
            ppDstPtrs[i] = (float*)mat.data+mat.w*i;
        if (mat_in.elempack > 1)
        {
            if (mat_in.type == IM_DT_FLOAT32)
            {
                const float* pSrcPtr = (const float*)mat_in.data;
                for (int i = 0; i < fft_size; i++)
                {
                    for (int j = 0; j < ch; j++)
                    {
                        auto& pDstPtr = ppDstPtrs[j];
                        *pDstPtr++ = *pSrcPtr++;
                    }
                }
            }
            else if (mat_in.type == IM_DT_INT16)
            {
                const int16_t* pSrcPtr = (const int16_t*)mat_in.data;
                for (int i = 0; i < fft_size; i++)
                {
                    for (int j = 0; j < ch; j++)
                    {
                        auto& pDstPtr = ppDstPtrs[j];
                        *pDstPtr++ = (float)(*pSrcPtr++)/INT16_MAX;
                    }
                }
            }
            else
                throw std::runtime_error("This PCM format is NOT SUPPORTED yet!");
        }
        else
        {
            if (mat_in.type == IM_DT_FLOAT32)
            {
                const float** ppSrcPtrs = new const float*[ch];
                for (int i = 0; i < ch; i++)
                    ppSrcPtrs[i] = (const float*)mat_in.data+mat_in.w*i;
                for (int i = 0; i < ch; i++)
                    memcpy(ppDstPtrs[i], ppSrcPtrs[i], fft_size*sizeof(float));
                delete [] ppSrcPtrs;
            }
            else if (mat_in.type == IM_DT_INT16)
            {
                const int16_t** ppSrcPtrs = new const int16_t*[ch];
                for (int i = 0; i < ch; i++)
                    ppSrcPtrs[i] = (const int16_t*)mat_in.data+mat_in.w*i;
                for (int i = 0; i < ch; i++)
                {
                    auto& pDstPtr = ppDstPtrs[i];
                    auto& pSrcPtr = ppSrcPtrs[i];
                    for (int j = 0; j < fft_size; j++)
                        *pDstPtr++ = (float)(*pSrcPtr++)/INT16_MAX;
                }
                delete [] ppSrcPtrs;
            }
            else
                throw std::runtime_error("This PCM format is NOT SUPPORTED yet!");
        }
        delete [] ppDstPtrs;
    }

    for (int i = 0; i < mat.c; i++)
    {
        if (i >= (int)mhMediaSettings->AudioOutChannels())
            break;
        auto & channel_data = mAudioAttribute.channel_data[i];
        channel_data.m_wave.clone_from(mat.channel(i));
        channel_data.m_fft.clone_from(mat.channel(i));
        ImGui::ImRFFT((float *)channel_data.m_fft.data, channel_data.m_fft.w, true);
        channel_data.m_db.create_type((mat.w >> 1) + 1, IM_DT_FLOAT32);
        channel_data.m_DBMaxIndex = ImGui::ImReComposeDB((float*)channel_data.m_fft.data, (float *)channel_data.m_db.data, mat.w, false);
        channel_data.m_DBShort.create_type(20, IM_DT_FLOAT32);
        ImGui::ImReComposeDBShort((float*)channel_data.m_fft.data, (float*)channel_data.m_DBShort.data, mat.w);
        channel_data.m_DBLong.create_type(76, IM_DT_FLOAT32);
        ImGui::ImReComposeDBLong((float*)channel_data.m_fft.data, (float*)channel_data.m_DBLong.data, mat.w);
        channel_data.m_decibel = ImGui::ImDoDecibel((float*)channel_data.m_fft.data, mat.w);
        if (channel_data.m_Spectrogram.w != (mat.w >> 1) + 1)
        {
            channel_data.m_Spectrogram.create_type((mat.w >> 1) + 1, 256, 4, IM_DT_INT8);
        }
        if (!channel_data.m_Spectrogram.empty())
        {
            auto w = channel_data.m_Spectrogram.w;
            auto c = channel_data.m_Spectrogram.c;
            memmove(channel_data.m_Spectrogram.data, (char *)channel_data.m_Spectrogram.data + w * c, channel_data.m_Spectrogram.total() - w * c);
            uint32_t * last_line = (uint32_t *)channel_data.m_Spectrogram.row_c<uint8_t>(255);
            for (int n = 0; n < w; n++)
            {
                float value = channel_data.m_db.at<float>(n) * M_SQRT2 + 64 + mAudioAttribute.mAudioSpectrogramOffset;
                value = ImClamp(value, -64.f, 63.f);
                float light = (value + 64) / 127.f;
                value = (int)((value + 64) + 170) % 255; 
                auto hue = value / 255.f;
                auto color = ImColor::HSV(hue, 1.0, light * mAudioAttribute.mAudioSpectrogramLight);
                last_line[n] = color;
            }
            channel_data.m_Spectrogram.flags |= IM_MAT_FLAGS_CUSTOM_UPDATED;
        }
    }
    if (mat.c >= 2)
    {
        if (mAudioAttribute.m_audio_vector.empty())
        {
            mAudioAttribute.m_audio_vector.create_type(256, 256, 4, IM_DT_INT8);
            mAudioAttribute.m_audio_vector.fill((int8_t)0);
            mAudioAttribute.m_audio_vector.elempack = 4;
        }
        if (!mAudioAttribute.m_audio_vector.empty())
        {
            float zoom = mAudioAttribute.mAudioVectorScale;
            float hw = mAudioAttribute.m_audio_vector.w / 2;
            float hh = mAudioAttribute.m_audio_vector.h / 2;
            int samples = mat.w;
            mAudioAttribute.m_audio_vector -= 64;
            for (int n = 0; n < samples; n++)
            {
                float s1 = mAudioAttribute.channel_data[0].m_wave.at<float>(n, 0);
                float s2 = mAudioAttribute.channel_data[1].m_wave.at<float>(n, 0);
                int x = hw;
                int y = hh;

                if (mAudioAttribute.mAudioVectorMode == LISSAJOUS)
                {
                    x = ((s2 - s1) * zoom / 2 + 1) * hw;
                    y = (1.0 - (s1 + s2) * zoom / 2) * hh;
                }
                else if (mAudioAttribute.mAudioVectorMode == LISSAJOUS_XY)
                {
                    x = (s2 * zoom + 1) * hw;
                    y = (s1 * zoom + 1) * hh;
                }
                else
                {
                    float sx, sy, cx, cy;
                    sx = s2 * zoom;
                    sy = s1 * zoom;
                    cx = sx * sqrtf(1 - 0.5 * sy * sy);
                    cy = sy * sqrtf(1 - 0.5 * sx * sx);
                    x = hw + hw * ImSign(cx + cy) * (cx - cy) * .7;
                    y = mAudioAttribute.m_audio_vector.h - mAudioAttribute.m_audio_vector.h * fabsf(cx + cy) * .7;
                }
                x = ImClamp(x, 0, mAudioAttribute.m_audio_vector.w - 1);
                y = ImClamp(y, 0, mAudioAttribute.m_audio_vector.h - 1);
                uint8_t r = ImClamp(mAudioAttribute.m_audio_vector.at<uint8_t>(x, y, 0) + 30, 0, 255);
                uint8_t g = ImClamp(mAudioAttribute.m_audio_vector.at<uint8_t>(x, y, 1) + 50, 0, 255);
                uint8_t b = ImClamp(mAudioAttribute.m_audio_vector.at<uint8_t>(x, y, 2) + 30, 0, 255);
                mAudioAttribute.m_audio_vector.set_pixel(x, y, ImPixel(r / 255.0, g / 255.0, b / 255.0, 1.f));
            }
            mAudioAttribute.m_audio_vector.flags |= IM_MAT_FLAGS_CUSTOM_UPDATED;
        }
    }
}

bool TimeLine::ConfigEncoder(const std::string& outputPath, VideoEncoderParams& vidEncParams, AudioEncoderParams& audEncParams, std::string& errMsg)
{
    mEncoder = MediaCore::MediaEncoder::CreateInstance();
    if (!mEncoder->Open(outputPath))
    {
        errMsg = mEncoder->GetError();
        return false;
    }
    // Video
    if (!mEncoder->ConfigureVideoStream(
        vidEncParams.codecName, vidEncParams.imageFormat, vidEncParams.width, vidEncParams.height,
        vidEncParams.frameRate, vidEncParams.bitRate, &vidEncParams.extraOpts))
    {
        errMsg = mEncoder->GetError();
        return false;
    }
    mEncMtvReader = mMtvReader->CloneAndConfigure(vidEncParams.width, vidEncParams.height, vidEncParams.frameRate);

    // Audio
    if (!mEncoder->ConfigureAudioStream(
        audEncParams.codecName, audEncParams.sampleFormat, audEncParams.channels,
        audEncParams.sampleRate, audEncParams.bitRate))
    {
        errMsg = mEncoder->GetError();
        return false;
    }
    mEncMtaReader = mMtaReader->CloneAndConfigure(audEncParams.channels, audEncParams.sampleRate, audEncParams.samplesPerFrame);
    return true;
}

void TimeLine::StartEncoding()
{
    if (mEncodingThread.joinable())
    {
        mQuitEncoding = true;
        mEncodingThread.join();
        //return;
    }
    mEncodeProcErrMsg.clear();
    mEncodingProgress = 0;
    mEncodingDuration = (double)ValidDuration()/1000.f;
    mQuitEncoding = false;
    mIsEncoding = true;
    mEncodingThread = std::thread(&TimeLine::_EncodeProc, this);
    SysUtils::SetThreadName(mEncodingThread, "TL-EncProc");
}

void TimeLine::StopEncoding()
{
    mQuitEncoding = true;
    if (mEncodingThread.joinable())
    {
        mEncodingThread.join();
        mEncodingThread = std::thread();
    }
    mIsEncoding = false;
    mEncMtvReader = nullptr;
    mEncMtaReader = nullptr;
}

void TimeLine::_EncodeProc()
{
    Logger::Log(Logger::DEBUG) << ">>>>>>>>>>> Enter encoding proc >>>>>>>>>>>>" << std::endl;
    mEncoder->Start();
    bool vidInputEof = false;
    bool audInputEof = false;
    int64_t audpos = 0, vidpos = 0;
    double maxEncodeDuration = 0;
    MediaCore::Ratio outFrameRate = mEncoder->GetVideoFrameRate();
    ImGui::ImMat vmat, amat;
    uint32_t pcmbufSize = 8192;
    uint8_t* pcmbuf = new uint8_t[pcmbufSize];
    auto dur = ValidDuration();
    int64_t encpos = 0;
    int64_t vidFrameCount = mEncMtvReader->MillsecToFrameIndex(mEncodingStart);
    int64_t startTimeOffset = mEncMtvReader->FrameIndexToMillsec(vidFrameCount);
    if (mEncMtvReader) mEncMtvReader->SeekTo(mEncodingStart);
    if (mEncMtaReader) mEncMtaReader->SeekTo(mEncodingStart);
    while (!mQuitEncoding && (!vidInputEof || !audInputEof))
    {
        bool idleLoop = true;
        if ((!vidInputEof && vidpos <= audpos) || audInputEof)
        {
            vidpos = mEncMtvReader->FrameIndexToMillsec(vidFrameCount);
            bool eof = vidpos >= mEncodingEnd;
            if (!eof)
            {
                if (!mEncMtvReader->ReadVideoFrameByIdx(vidFrameCount, vmat))
                {
                    std::ostringstream oss;
                    oss << "[video] '" << mEncMtvReader->GetError() << "'.";
                    mEncodeProcErrMsg = oss.str();
                    break;
                }
                if (!vmat.empty())
                {
                    vidFrameCount++;
                    vmat.time_stamp = (double)(vidpos-startTimeOffset)/1000.;
                    {
                        std::lock_guard<std::mutex> lk(mEncodingMutex);
                        mEncodingVFrame = vmat;
                    }
                    if (!mEncoder->EncodeVideoFrame(vmat))
                    {
                        std::ostringstream oss;
                        oss << "[video] '" << mEncoder->GetError() << "'.";
                        mEncodeProcErrMsg = oss.str();
                        break;
                    }
                    if (vidpos > encpos)
                    {
                        encpos = vidpos;
                        mEncodingProgress = (float)((double)(encpos - startTimeOffset) / dur);
                    }
                }
            }
            else
            {
                vmat.release();
                if (!mEncoder->EncodeVideoFrame(vmat))
                {
                    std::ostringstream oss;
                    oss << "[video] '" << mEncoder->GetError() << "'.";
                    mEncodeProcErrMsg = oss.str();
                    break;
                }
                vidInputEof = true;
            }
        }
        else
        {
            bool eof;
            uint32_t readSize = pcmbufSize;
            if (!mEncMtaReader->ReadAudioSamples(amat, eof) && !eof)
            {
                std::ostringstream oss;
                oss << "[audio] '" << mEncMtaReader->GetError() << "'.";
                mEncodeProcErrMsg = oss.str();
                break;
            }
            if (audpos > mEncodingEnd) eof = true;
            if (!eof && !amat.empty())
            {
                audpos = amat.time_stamp * 1000;
                amat.time_stamp = (double)(audpos-startTimeOffset)/1000.;
                if (!mEncoder->EncodeAudioSamples(amat))
                {
                    std::ostringstream oss;
                    oss << "[audio] '" << mEncoder->GetError() << "'.";
                    mEncodeProcErrMsg = oss.str();
                    break;
                }
                if (audpos > encpos)
                {
                    encpos = audpos;
                    mEncodingProgress = (float)((double)(encpos - startTimeOffset) / dur);
                }
            }
            else
            {
                amat.release();
                // {
                //     std::lock_guard<std::mutex> lk(mEncodingMutex);
                //     mEncodingAFrame = amat;
                // }
                if (!mEncoder->EncodeAudioSamples(amat))
                {
                    std::ostringstream oss;
                    oss << "[audio] '" << mEncoder->GetError() << "'.";
                    mEncodeProcErrMsg = oss.str();
                    break;
                }
                audInputEof = true;
            }
        }
    }
    if (!mQuitEncoding && mEncodeProcErrMsg.empty())
    {
        mEncodingProgress = 1;
    }
    mEncoder->FinishEncoding();
    mEncoder->Close();
    mIsEncoding = false;
    Logger::Log(Logger::DEBUG) << "<<<<<<<<<<<<< Quit encoding proc <<<<<<<<<<<<<<<<" << std::endl;
}

void TimeLine::AddNewRecord(imgui_json::value& record)
{
    // truncate the history record list if needed
    if (mRecordIter != mHistoryRecords.end())
        mHistoryRecords.erase(mRecordIter, mHistoryRecords.end());
    mHistoryRecords.push_back(std::move(record));
    mRecordIter = mHistoryRecords.end();
}

bool TimeLine::UndoOneRecord()
{
    if (mRecordIter == mHistoryRecords.begin())
        return false;

    mRecordIter--;
    auto& record = *mRecordIter;
    auto& actions = record["actions"].get<imgui_json::array>();
    PrintActionList("UNDO record", actions);
    auto iter = actions.end();
    while (iter != actions.begin())
    {
        iter--;
        auto& action = *iter;
        std::string& actionName = action["action"].get<imgui_json::string>();
        if (actionName == "ADD_TRACK")
        {
            int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
            int index = 0;
            for (; index < m_Tracks.size(); index++)
            {
                if (m_Tracks[index]->mID == trackId)
                    break;
            }
            if (index < m_Tracks.size())
            {
                DeleteTrack(index, nullptr);
                imgui_json::value undoAction = action;
                undoAction["action"] = "REMOVE_TRACK";
                mUiActions.push_back(std::move(undoAction));
            }
        }
        else if (actionName == "REMOVE_TRACK")
        {
            RestoreTrack(action);
        }
        else if (actionName == "MOVE_TRACK")
        {
            int64_t orgIndex = action["org_index"].get<imgui_json::number>();
            int64_t dstIndex = action["dst_index"].get<imgui_json::number>();
            MovingTrack(dstIndex, orgIndex, &mUiActions);
        }
        else if (actionName == "ADD_CLIP")
        {
            DeleteClip(action["clip_json"]["ID"].get<imgui_json::number>(), nullptr);
            Update();
            imgui_json::value undoAction;
            undoAction["action"] = "REMOVE_CLIP";
            undoAction["media_type"] = action["media_type"];
            undoAction["from_track_id"] = action["to_track_id"];
            undoAction["clip_json"] = action["clip_json"];
            mUiActions.push_back(std::move(undoAction));
        }
        else if (actionName == "REMOVE_CLIP")
        {
            AddNewClip(action["clip_json"], action["from_track_id"].get<imgui_json::number>());
            Update();
            imgui_json::value undoAction;
            undoAction["action"] = "ADD_CLIP";
            undoAction["media_type"] = action["media_type"];
            undoAction["to_track_id"] = action["from_track_id"];
            undoAction["clip_json"] = action["clip_json"];
            mUiActions.push_back(std::move(undoAction));
        }
        else if (actionName == "MOVE_CLIP")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            int64_t orgStart = action["org_start"].get<imgui_json::number>();
            int64_t fromTrackId = action["from_track_id"].get<imgui_json::number>();
            int64_t toTrackId = action["to_track_id"].get<imgui_json::number>();
            int fromTrackIndex = -1;
            for (int i = 0; i < m_Tracks.size(); i++)
            {
                if (m_Tracks[i]->mID == fromTrackId)
                {
                    fromTrackIndex = i;
                    break;
                }
            }
            int toTrackIndex = -1;
            for (int i = 0; i < m_Tracks.size(); i++)
            {
                if (m_Tracks[i]->mID == toTrackId)
                {
                    toTrackIndex = i;
                    break;
                }
            }
            Clip* clip = FindClipByID(clipId);
            clip->ChangeStart(orgStart);
            MovingClip(clipId, toTrackIndex, fromTrackIndex);
            Update();

            imgui_json::value undoAction;
            undoAction["action"] = "MOVE_CLIP";
            undoAction["clip_id"] = action["clip_id"];
            undoAction["media_type"] = action["media_type"];
            undoAction["org_start"] = action["new_start"];
            undoAction["new_start"] = action["org_start"];
            undoAction["from_track_id"] = action["to_track_id"];
            undoAction["to_track_id"] = action["from_track_id"];
            mUiActions.push_back(std::move(undoAction));
        }
        else if (actionName == "CROP_CLIP")
        {
            auto clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
            auto clipType = clip->mType;
            int64_t startDiff{0}, endDiff{0};
            if ((IS_VIDEO(clipType) && !IS_IMAGE(clipType)) || IS_AUDIO(clipType))
            {
                int64_t orgStartOffset = action["org_start_offset"].get<imgui_json::number>();
                int64_t orgEndOffset = action["org_end_offset"].get<imgui_json::number>();
                int64_t newStartOffset = action["new_start_offset"].get<imgui_json::number>();
                int64_t newEndOffset = action["new_end_offset"].get<imgui_json::number>();
                startDiff = orgStartOffset-newStartOffset;
                endDiff = orgEndOffset-newEndOffset;
            }
            else
            {
                int64_t orgStart = action["org_start"].get<imgui_json::number>();
                int64_t orgEnd = action["org_end"].get<imgui_json::number>();
                int64_t newStart = action["new_start"].get<imgui_json::number>();
                int64_t newEnd = action["new_end"].get<imgui_json::number>();
                startDiff = orgStart-newStart;
                endDiff = orgEnd-newEnd;
            }
            if (startDiff != 0)
                clip->Cropping(startDiff, 0);
            if (endDiff != 0)
                clip->Cropping(-endDiff, 1);

            imgui_json::value undoAction;
            undoAction["action"] = "CROP_CLIP";
            undoAction["clip_id"] = action["clip_id"];
            undoAction["media_type"] = action["media_type"];
            undoAction["from_track_id"] = action["from_track_id"];
            undoAction["org_start_offset"] = action["new_start_offset"];
            undoAction["new_start_offset"] = action["org_start_offset"];
            undoAction["org_end_offset"] = action["new_end_offset"];
            undoAction["new_end_offset"] = action["org_end_offset"];
            undoAction["org_start"] = action["new_start"];
            undoAction["new_start"] = action["org_start"];
            undoAction["org_end"] = action["new_end"];
            undoAction["new_end"] = action["org_end"];
            mUiActions.push_back(std::move(undoAction));
        }
        else if (actionName == "CUT_CLIP")
        {
            int64_t newClipId = action["new_clip_id"].get<imgui_json::number>();
            auto pUiClip = FindClipByID(newClipId);
            imgui_json::value undoAction;
            undoAction["action"] = "REMOVE_CLIP";
            undoAction["media_type"] = action["media_type"];
            undoAction["from_track_id"] = action["track_id"];
            undoAction["clip_json"] = pUiClip->SaveAsJson();
            undoAction["update_duration"] = imgui_json::boolean(false);
            mUiActions.push_back(std::move(undoAction));
            DeleteClip(newClipId, nullptr);

            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            int64_t newClipStart = action["new_clip_start"].get<imgui_json::number>();
            int64_t orgEnd = action["org_end"].get<imgui_json::number>();
            int64_t endDiff = orgEnd-newClipStart;
            pUiClip = FindClipByID(clipId);
            pUiClip->Cropping(endDiff, 1);
            undoAction = imgui_json::value();
            undoAction["action"] = "CROP_CLIP";
            undoAction["clip_id"] = action["clip_id"];
            undoAction["media_type"] = action["media_type"];
            undoAction["from_track_id"] = action["track_id"];
            undoAction["new_start"] = imgui_json::number(pUiClip->Start());
            undoAction["new_end"] = imgui_json::number(pUiClip->End());
            undoAction["new_start_offset"] = imgui_json::number(pUiClip->StartOffset());
            undoAction["new_end_offset"] = imgui_json::number(pUiClip->EndOffset());
            undoAction["update_duration"] = imgui_json::boolean(false);
            mUiActions.push_back(std::move(undoAction));
        }
        else if (actionName == "ADD_GROUP")
        {
            int64_t gid = action["group_json"]["ID"].get<imgui_json::number>();
            auto giter = std::find_if(m_Groups.begin(), m_Groups.end(), [gid] (auto& g) {
                return g.mID == gid;
            });
            if (giter != m_Groups.end())
            {
                std::vector<int64_t> containedClipIds(giter->m_Grouped_Clips);
                for (auto cid : containedClipIds)
                {
                    auto c = FindClipByID(cid);
                    DeleteClipFromGroup(c, gid);
                }
            }
        }
        else if (actionName == "REMOVE_GROUP")
        {
            RestoreGroup(action["group_json"]);
        }
        else if (actionName == "ADD_CLIP_INTO_GROUP")
        {
            auto pClip = FindClipByID(action["clip_id"].get<imgui_json::number>());
            DeleteClipFromGroup(pClip, action["group_id"].get<imgui_json::number>());
        }
        else if (actionName == "DELETE_CLIP_FROM_GROUP")
        {
            auto pClip = FindClipByID(action["clip_id"].get<imgui_json::number>());
            AddClipIntoGroup(pClip, action["group_id"].get<imgui_json::number>());
        }
        else if (actionName == "LINK_TRACK")
        {
            auto pTrack1 = FindTrackByID(action["track_id1"].get<imgui_json::number>());
            pTrack1->mLinkedTrack = -1;
            auto pTrack2 = FindTrackByID(action["track_id2"].get<imgui_json::number>());
            pTrack2->mLinkedTrack = -1;
        }
        else if (actionName == "HIDE_TRACK")
        {
            int64_t trackId = action["track_id"].get<imgui_json::number>();
            bool visible = !action["visible"].get<imgui_json::boolean>();
            auto pTrack = FindTrackByID(trackId);
            pTrack->mView = visible;
            imgui_json::value undoAction = action;
            undoAction["visible"] = visible;
            mUiActions.push_back(std::move(undoAction));
        }
        else if (actionName == "MUTE_TRACK")
        {
            int64_t trackId = action["track_id"].get<imgui_json::number>();
            bool muted = !action["muted"].get<imgui_json::boolean>();
            auto pTrack = FindTrackByID(trackId);
            pTrack->mView = !muted;
            imgui_json::value undoAction = action;
            undoAction["muted"] = muted;
            mUiActions.push_back(std::move(undoAction));
        }
        else if (actionName == "ADD_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            pClip->DeleteEvent(evtId, nullptr);
        }
        else if (actionName == "DELETE_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            auto hEvent = pClip->mEventStack->RestoreEventFromJson(action["event_json"]);
            if (hEvent)
            {
                pClip->mEventTracks[hEvent->Z()]->m_Events.push_back(hEvent->Id());
                auto pTrack = FindTrackByClipID(pClip->mID);
                RefreshTrackView({ pTrack->mID });
            }
            else
            {
                Logger::Log(Logger::WARN) << "FAILED to restore Event from json " << action["event_json"].dump() << "!" << std::endl;
            }
        }
        else if (actionName == "MOVE_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            int64_t oldStart = action["event_start_old"].get<imgui_json::number>();
            int32_t oldZ = action["event_z_old"].get<imgui_json::number>();
            pClip->mEventStack->MoveEvent(evtId, oldStart, oldZ);
            auto pTrack = FindTrackByClipID(clipId);
            RefreshTrackView({ pTrack->mID });
        }
        else if (actionName == "CROP_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            int64_t oldStart = action["event_start_old"].get<imgui_json::number>();
            int32_t oldEnd = action["event_end_old"].get<imgui_json::number>();
            pClip->mEventStack->ChangeEventRange(evtId, oldStart, oldEnd);
            auto pTrack = FindTrackByClipID(clipId);
            RefreshTrackView({ pTrack->mID });
        }
        else if (actionName == "BP_OPERATION")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            auto pUiClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            auto hEvent = pUiClip->mEventStack->GetEvent(evtId);
            auto pBp = hEvent->GetBp();
            uint32_t mediaType = action["media_type"].get<imgui_json::number>();
            pBp->File_New_Filter(action["before_op_state"], "EventBp",
                    IS_VIDEO(mediaType) ? "Video" : IS_AUDIO(mediaType) ? "Audio" : IS_TEXT(mediaType) ? "Text" : "");
            auto pUiTrack = FindTrackByClipID(clipId);
            RefreshTrackView({ pUiTrack->mID });
        }
        else
        {
            Logger::Log(Logger::WARN) << "Unhandled UNDO action '" << actionName << "'!" << std::endl;
        }
    }
    return true;
}

bool TimeLine::RedoOneRecord()
{
    if (mRecordIter == mHistoryRecords.end())
        return false;

    auto& record = *mRecordIter;
    auto& actions = record["actions"].get<imgui_json::array>();
    mRecordIter++;
    ImU32 groupColor = 0;
    PrintActionList("REDO record", actions);
    for (auto& action : actions)
    {
        std::string& actionName = action["action"].get<imgui_json::string>();
        if (actionName == "ADD_TRACK")
        {
            uint32_t type = action["media_type"].get<imgui_json::number>();
            int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
            int64_t afterUiTrkId = action["after_ui_track_id"].get<imgui_json::number>();
            NewTrack("", type, true, trackId, afterUiTrkId);
            mUiActions.push_back(action);
        }
        else if (actionName == "REMOVE_TRACK")
        {
            int64_t trackId = action["track_json"]["ID"].get<imgui_json::number>();
            int i = 0;
            for (; i < m_Tracks.size(); i++)
            {
                if (m_Tracks[i]->mID == trackId)
                    break;
            }
            if (i < m_Tracks.size())
            {
                DeleteTrack(i, nullptr);
                mUiActions.push_back(action);
            }
        }
        else if (actionName == "MOVE_TRACK")
        {
            int64_t orgIndex = action["org_index"].get<imgui_json::number>();
            int64_t dstIndex = action["dst_index"].get<imgui_json::number>();
            MovingTrack(orgIndex, dstIndex, &mUiActions);
        }
        else if (actionName == "ADD_CLIP")
        {
            AddNewClip(action["clip_json"], action["to_track_id"].get<imgui_json::number>());
            Update();
            mUiActions.push_back(action);
        }
        else if (actionName == "REMOVE_CLIP")
        {
            int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
            DeleteClip(clipId, nullptr);
            Update();
            mUiActions.push_back(action);
        }
        else if (actionName == "MOVE_CLIP")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            int64_t newStart = action["new_start"].get<imgui_json::number>();
            int64_t fromTrackId = action["from_track_id"].get<imgui_json::number>();
            int64_t toTrackId = action["to_track_id"].get<imgui_json::number>();
            int fromTrackIndex = -1;
            for (int i = 0; i < m_Tracks.size(); i++)
            {
                if (m_Tracks[i]->mID == fromTrackId)
                {
                    fromTrackIndex = i;
                    break;
                }
            }
            int toTrackIndex = -1;
            for (int i = 0; i < m_Tracks.size(); i++)
            {
                if (m_Tracks[i]->mID == toTrackId)
                {
                    toTrackIndex = i;
                    break;
                }
            }
            Clip* clip = FindClipByID(clipId);
            clip->ChangeStart(newStart);
            MovingClip(clipId, fromTrackIndex, toTrackIndex);
            Update();
            mUiActions.push_back(action);
        }
        else if (actionName == "CROP_CLIP")
        {
            auto clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
            auto clipType = clip->mType;
            int64_t startDiff{0}, endDiff{0};
            if ((IS_VIDEO(clipType) && !IS_IMAGE(clipType)) || IS_AUDIO(clipType))
            {
                int64_t orgStartOffset = action["org_start_offset"].get<imgui_json::number>();
                int64_t orgEndOffset = action["org_end_offset"].get<imgui_json::number>();
                int64_t newStartOffset = action["new_start_offset"].get<imgui_json::number>();
                int64_t newEndOffset = action["new_end_offset"].get<imgui_json::number>();
                startDiff = orgStartOffset-newStartOffset;
                endDiff = orgEndOffset-newEndOffset;
            }
            else
            {
                int64_t orgStart = action["org_start"].get<imgui_json::number>();
                int64_t orgEnd = action["org_end"].get<imgui_json::number>();
                int64_t newStart = action["new_start"].get<imgui_json::number>();
                int64_t newEnd = action["new_end"].get<imgui_json::number>();
                startDiff = newStart-orgStart;
                endDiff = newEnd-orgEnd;
            }
            if (startDiff != 0)
                clip->Cropping(-startDiff, 0);
            if (endDiff)
                clip->Cropping(endDiff, 1);
            mUiActions.push_back(action);
        }
        else if (actionName == "CUT_CLIP")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            auto pUiClip = FindClipByID(clipId);
            int64_t cutPos = action["cut_pos"].get<imgui_json::number>();
            int64_t gid = action["group_id"].get<imgui_json::number>();
            int64_t newClipId = action["new_clip_id"].get<imgui_json::number>();
            pUiClip->Cutting(cutPos, gid, newClipId);
            mUiActions.push_back(action);
        }
        else if (actionName == "ADD_GROUP")
        {
            RestoreGroup(action["group_json"]);
        }
        else if (actionName == "REMOVE_GROUP")
        {
            int64_t gid = action["group_json"]["ID"].get<imgui_json::number>();
            auto giter = std::find_if(m_Groups.begin(), m_Groups.end(), [gid] (auto& g) {
                return g.mID == gid;
            });
            if (giter != m_Groups.end())
            {
                std::vector<int64_t> containedClipIds(giter->m_Grouped_Clips);
                for (auto cid : containedClipIds)
                {
                    auto c = FindClipByID(cid);
                    DeleteClipFromGroup(c, gid);
                }
            }
        }
        else if (actionName == "ADD_CLIP_INTO_GROUP")
        {
            auto pClip = FindClipByID(action["clip_id"].get<imgui_json::number>());
            AddClipIntoGroup(pClip, action["group_id"].get<imgui_json::number>());
        }
        else if (actionName == "DELETE_CLIP_FROM_GROUP")
        {
            auto pClip = FindClipByID(action["clip_id"].get<imgui_json::number>());
            DeleteClipFromGroup(pClip, action["group_id"].get<imgui_json::number>());
        }
        else if (actionName == "LINK_TRACK")
        {
            auto pTrack1 = FindTrackByID(action["track_id1"].get<imgui_json::number>());
            auto pTrack2 = FindTrackByID(action["track_id2"].get<imgui_json::number>());
            pTrack1->mLinkedTrack = pTrack2->mID;
            pTrack2->mLinkedTrack = pTrack1->mID;
        }
        else if (actionName == "HIDE_TRACK")
        {
            int64_t trackId = action["track_id"].get<imgui_json::number>();
            bool visible = action["visible"].get<imgui_json::boolean>();
            auto pTrack = FindTrackByID(trackId);
            pTrack->mView = visible;
            mUiActions.push_back(action);
        }
        else if (actionName == "MUTE_TRACK")
        {
            int64_t trackId = action["track_id"].get<imgui_json::number>();
            bool muted = action["muted"].get<imgui_json::boolean>();
            auto pTrack = FindTrackByID(trackId);
            pTrack->mView = !muted;
            mUiActions.push_back(action);
        }
        else if (actionName == "ADD_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            int64_t evtStart = action["event_start"].get<imgui_json::number>();
            int64_t evtEnd = action["event_end"].get<imgui_json::number>();
            int32_t evtZ = action["event_z"].get<imgui_json::number>();
            ID_TYPE nodeTypeId = action["node_type_id"].get<imgui_json::number>();
            std::string nodeName = action["node_name"].get<imgui_json::string>();
            pClip->AddEvent(evtId, evtZ, evtStart, evtEnd-evtStart, nodeTypeId, nodeName, nullptr);
        }
        else if (actionName == "DELETE_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            pClip->DeleteEvent(evtId, nullptr);
        }
        else if (actionName == "MOVE_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            int64_t newStart = action["event_start_new"].get<imgui_json::number>();
            int32_t newZ = action["event_z_new"].get<imgui_json::number>();
            pClip->mEventStack->MoveEvent(evtId, newStart, newZ);
            auto pTrack = FindTrackByClipID(clipId);
            RefreshTrackView({ pTrack->mID });
        }
        else if (actionName == "CROP_EVENT")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            Clip* pClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            int64_t newStart = action["event_start_new"].get<imgui_json::number>();
            int32_t newEnd = action["event_end_new"].get<imgui_json::number>();
            pClip->mEventStack->ChangeEventRange(evtId, newStart, newEnd);
            auto pTrack = FindTrackByClipID(clipId);
            RefreshTrackView({ pTrack->mID });
        }
        else if (actionName == "BP_OPERATION")
        {
            int64_t clipId = action["clip_id"].get<imgui_json::number>();
            auto pUiClip = FindClipByID(clipId);
            int64_t evtId = action["event_id"].get<imgui_json::number>();
            auto hEvent = pUiClip->mEventStack->GetEvent(evtId);
            auto pBp = hEvent->GetBp();
            uint32_t mediaType = action["media_type"].get<imgui_json::number>();
            pBp->File_New_Filter(action["after_op_state"], "EventBp",
                    IS_VIDEO(mediaType) ? "Video" : IS_AUDIO(mediaType) ? "Audio" : IS_TEXT(mediaType) ? "Text" : "");
            auto pUiTrack = FindTrackByClipID(clipId);
            RefreshTrackView({ pUiTrack->mID });
        }
        else
        {
            Logger::Log(Logger::WARN) << "Unhandled REDO action '" << actionName << "'!" << std::endl;
        }
    }
    return true;
}

int64_t TimeLine::AddNewClip(const imgui_json::value& jnClipJson, int64_t track_id, std::list<imgui_json::value>* pActionList)
{
    MediaTrack* track = FindTrackByID(track_id);
    if (!track)
    {
        Logger::Log(Logger::Error) << "FAILED to invoke 'TimeLine::AddNewClip()'! Target 'MediaTrack' does NOT exist." << std::endl;
        return -1;
    }
    const uint32_t mediaType = jnClipJson["Type"].get<imgui_json::number>();
    Clip* pUiNewClip = nullptr;
    switch (mediaType)
    {
    case MEDIA_VIDEO:
        pUiNewClip = VideoClip::CreateInstanceFromJson(jnClipJson, this);
        break;
    case MEDIA_AUDIO:
        pUiNewClip = AudioClip::CreateInstanceFromJson(jnClipJson, this);
        break;
    case MEDIA_TEXT:
        pUiNewClip = TextClip::CreateInstanceFromJson(jnClipJson, this);
        break;
    }
    m_Clips.push_back(pUiNewClip);
    track->InsertClip(pUiNewClip, pUiNewClip->Start(), true, pActionList);

    int64_t groupId = jnClipJson["GroupID"].get<imgui_json::number>();
    if (groupId != -1)
    {
        auto iter = std::find_if(m_Groups.begin(), m_Groups.end(), [groupId] (auto& group) {
            return group.mID == groupId;
        });
        if (iter == m_Groups.end())
            NewGroup(pUiNewClip, groupId, 0, pActionList);
        else
            AddClipIntoGroup(pUiNewClip, groupId, pActionList);
    }
    return pUiNewClip->mID;
}

int64_t TimeLine::AddNewClip(
        int64_t media_id, uint32_t media_type, int64_t track_id,
        int64_t start, int64_t start_offset, int64_t end, int64_t end_offset,
        int64_t group_id, int64_t clip_id, std::list<imgui_json::value>* pActionList)
{
    MediaItem* item = FindMediaItemByID(media_id);
    if (!item)
    {
        Logger::Log(Logger::Error) << "FAILED to invoke 'TimeLine::AddNewClip()'! Target 'MediaItem' does NOT exist." << std::endl;
        return -1;
    }
    MediaTrack* track = FindTrackByID(track_id);
    if (!track)
    {
        Logger::Log(Logger::Error) << "FAILED to invoke 'TimeLine::AddNewClip()'! Target 'MediaTrack' does NOT exist." << std::endl;
        return -1;
    }
    Clip* newClip = nullptr;
    if (IS_VIDEO(media_type))
        newClip = VideoClip::CreateInstance(this, item, start);
    else if (IS_AUDIO(media_type))
        newClip = AudioClip::CreateInstance(this, item, start);
    else
    {
        Logger::Log(Logger::Error) << "Unsupported media type " << media_type << " for TimeLine::AddNewClip()!" << std::endl;
        return -1;
    }

    if (clip_id != -1)
        newClip->mID = clip_id;
    if (!IS_IMAGE(media_type) && !IS_TEXT(media_type))
    {
        newClip->ChangeStartOffset(start_offset);
        newClip->ChangeEndOffset(end_offset);
    }
    // newClip->ChangeStart(start);
    m_Clips.push_back(newClip);
    track->InsertClip(newClip, start, true, pActionList);
    if (group_id != -1)
    {
        auto iter = std::find_if(m_Groups.begin(), m_Groups.end(), [group_id] (auto& group) {
            return group.mID == group_id;
        });
        if (iter == m_Groups.end())
            NewGroup(newClip, group_id, 0, pActionList);
        else
            AddClipIntoGroup(newClip, group_id, pActionList);
    }
    return newClip->mID;
}

int TimeLine::FindEditingItem(int type, int64_t id)
{
    auto iter = std::find_if(mEditingItems.begin(), mEditingItems.end(), [id, type, this] (auto& item) {
        return item->mEditorType == type && 
                    ((type == EDITING_CLIP && item->mEditingClip && item->mEditingClip->mID == id) ||
                    (type == EDITING_TRANSITION && item->mEditingOverlap && item->mEditingOverlap->mID == id));
    });
    if (iter != mEditingItems.end())
        return iter - mEditingItems.begin();
    return -1;
}

MediaItem* TimeLine::isURLInMediaBank(std::string url)
{
    auto name = ImGuiHelper::path_filename(url);
    auto file_suffix = ImGuiHelper::path_filename_suffix(url);
    auto type = EstimateMediaType(file_suffix);
    auto iter = std::find_if(media_items.begin(), media_items.end(), [name, url, type](const MediaItem* item)
    {
        return  name.compare(item->mName) == 0 &&
                url.compare(item->mPath) == 0 &&
                type == item->mMediaType;
    });
    if (iter != media_items.end())
    {
        return *iter;
    }
    return nullptr;
}

bool TimeLine::isURLInTimeline(std::string url)
{
    auto file_suffix = ImGuiHelper::path_filename_suffix(url);
    auto type = EstimateMediaType(file_suffix);
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [url, type](const Clip* clip)
    {
        return  url.compare(clip->mPath) == 0 &&
                type == clip->mType;
    });
    if (iter != m_Clips.end())
    {
        return true;
    }
    return false;
}
} // namespace MediaTimeline/TimeLine

namespace MediaTimeline
{
struct BgtaskMenuItem
{
    std::string label;
    std::string taskType;
    std::function<bool(Clip*)> checkUsable;
    std::function<MEC::BackgroundTask::Holder(Clip*,bool&)> drawCreateTaskDialog;
};

/***********************************************************************************************************
 * Draw Main Timeline
 ***********************************************************************************************************/
bool DrawTimeLine(TimeLine *timeline, bool *expanded, bool& need_save, bool editable)
{
    /************************************************************************************************************
    * [v]------------------------------------ header area ----------------------------------------------------- +
    *                                                [] [] [] | [] [] [] [] | []  [] | [] [] [] [] [] [] [] []  +
    * --------------------------------------------------------------------------------------------------------- +
    *                    |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    c
    * ___________________|_______________|_____________________________________________________________________ a
    s | title     [+][-] |               |          Item bar                                                    n
    l |     legend       |---------------|--------------------------------------------------------------------- v
    o |                  |               |        custom area                                                   a 
    t |                  |               |                                                                      s                                            
    s |__________________|_______________|_____________________________________________________________________ |
    * | title     [+][-] |               |          Item bar                                                    h
    * |     legend       |---------------|--------------------------------------------------------------------- e
    * |                  |               |                                                                      i
    * |                  |               |        custom area                                                   g
    * |________________________________________________________________________________________________________ h
    * | title     [+][-] |               |          Item bar  <compact view>                                    t
    * |__________________|_______________|_____________________________________________________________________ +
    *                                                                                                           +
    *                     [     <==slider==>                                                                  ] +
    ++++++++++++++++++++++++++++++++++++++++ canvas width +++++++++++++++++++++++++++++++++++++++++++++++++++++++
    *************************************************************************************************************/
    bool changed = false;
    ImGuiIO &io = ImGui::GetIO();
    const int toolbar_height = 32;
    const int scrollSize = 16;
    const int trackHeadHeight = 16;
    const int HeadHeight = 28;
    const int legendWidth = 200;
    int trackCount = timeline->GetTrackCount();
    int64_t duration = ImMax(timeline->GetEnd() - timeline->GetStart(), (int64_t)1);
    ImVector<TimelineCustomDraw> customDraws;
    static int MovingHorizonScrollBar = -1;
    static bool MovingVerticalScrollBar = false;
    static bool MovingCurrentTime = false;
    static int trackMovingEntry = -1;
    static bool bTrackMoving = false;
    static int trackEntry = -1;
    static int trackMenuEntry = -1;
    static int64_t clipMenuEntry = -1;
    static int64_t clipMovingEntry = -1;
    static int clipMovingPart = -1;
    static float diffTime = 0;
    int delTrackEntry = -1;
    int mouseEntry = -1;
    int legendEntry = -1;
    std::vector<int64_t> mouseClip; // may have 2 clips(overlap area)
    int64_t mouseTime = -1;
    int alignedMousePosX = -1;
    static int64_t menuMouseTime = -1;
    std::vector<int64_t> delClipEntry;
    std::vector<int64_t> groupClipEntry;
    std::vector<int64_t> unGroupClipEntry;
    bool removeEmptyTrack = false;
    int insertEmptyTrackType = MEDIA_UNKNOWN;
    static int64_t lastFirstTime = -1;
    static int64_t lastVisiableTime = -1;
    static bool menuIsOpened = false;
    static bool bCropping = false;
    static bool bClipMoving = false;
    static bool bInsertNewTrack = false;
    static int InsertHeight = 0;
    // [shortcut]: left alt only for cutting clip
    timeline->mIsCutting = ImGui::IsKeyDown(ImGuiKey_LeftAlt) && (io.KeyMods == ImGuiMod_Alt);
    int64_t doCutPos = -1;
    bool overTrackView = false;
    bool overHorizonScrollBar = false;
    bool overCustomDraw = false;
    bool overLegend = false;
    bool overTopBar = false;
    bool clipClickedTriggered = false;
    bool bAnyPopup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (bAnyPopup) editable = false;

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();                    // ImDrawList API uses screen coordinates!
    ImVec2 canvas_size = ImGui::GetContentRegionAvail() - ImVec2(8, 0); // Resize canvas to what's available
    ImVec2 timline_size = canvas_size - ImVec2(scrollSize / 2, 0);     // add Vertical Scroll
    if (timline_size.y - trackHeadHeight - scrollSize - 8 <= 0)
        return changed;
    // zoom in/out    
    int64_t newVisibleTime = (int64_t)floorf((timline_size.x - legendWidth) / timeline->msPixelWidthTarget);
    timeline->ConfigSnapshotWindow(newVisibleTime);
    const float HorizonBarWidthRatio = ImMin(timeline->visibleTime / (float)duration, 1.f);
    const float HorizonBarWidthInPixels = std::max(HorizonBarWidthRatio * (timline_size.x - legendWidth), (float)scrollSize);
    const float HorizonBarPos = HorizonBarWidthRatio * (timline_size.x - legendWidth);
    ImRect regionRect(canvas_pos, canvas_pos + timline_size);
    ImRect HorizonScrollBarRect;
    ImRect HorizonScrollHandleBarRect;
    static ImVec2 panningViewHorizonSource;
    static int64_t panningViewHorizonTime;

    static ImVec2 panningViewVerticalSource;
    static float panningViewVerticalPos;

    static float headerMarkPos = -1;
    static int markMovingEntry = -1;
    static int64_t markMovingShift = 0;

    static const std::vector<BgtaskMenuItem> s_aBgtaskMenuItems = {
        {
            "Video Stabilization", "Vidstab",
            [timeline] (Clip* pClip) {
                if (!(timeline && timeline->IsProjectDirReady()))
                    return false;
                const auto clipType = pClip->mType;
                return IS_VIDEO(clipType)&&!IS_IMAGE(clipType);
            },
            [timeline] (Clip* pClip, bool& bCloseDlg) {
                auto hParser = pClip->mMediaParser;
                ImColor tTagColor(KNOWNIMGUICOLOR_LIGHTGRAY);
                ImColor tTextColor(KNOWNIMGUICOLOR_LIGHTGREEN);
                ImGui::TextColored(tTagColor, "Source File: ");
                ImGui::SameLine(); ImGui::TextColored(tTextColor, "%s", SysUtils::ExtractFileName(hParser->GetUrl()).c_str());
                ImGui::ShowTooltipOnHover("Path: '%s'", hParser->GetUrl().c_str());
                ImGui::TextColored(tTagColor, "Duration: ");
                ImGui::SameLine(); ImGui::TextColored(tTextColor, "%s", ImGuiHelper::MillisecToString(pClip->Length()).c_str());
                ImGui::SameLine(0, 25); ImGui::TextColored(tTagColor, "Start Offset: ");
                ImGui::SameLine(); ImGui::TextColored(tTextColor, "%s", ImGuiHelper::MillisecToString(pClip->StartOffset()).c_str());
                ImGui::SameLine(0, 25); ImGui::TextColored(tTagColor, "End Offset: ");
                ImGui::SameLine(); ImGui::TextColored(tTextColor, "%s", ImGuiHelper::MillisecToString(pClip->EndOffset()).c_str());
                ImGui::TextColored(tTagColor, "Work Dir: ");
                ImGui::SameLine(); ImGui::TextColored(tTextColor, "%s", timeline->mhProject->GetProjectDir().c_str());

                static int m_vidstabParam_iShakiness = 5;
                static int m_vidstabParam_iAccuracy = 15;
                static int m_vidstabParam_iStepsize = 6;
                static float m_vidstabParam_fMincontrast = 0.3f;
                static int m_vidstabParam_iSmoothing = 10;
                static int m_vidstabParam_iOptalgo = 0;
                static bool m_vidstabParam_bMaxshiftNolimit = true;
                static int m_vidstabParam_iMaxshift = 0;
                static bool m_vidstabParam_bMaxangleNolimit = true;
                static float m_vidstabParam_fMaxangle = 0;
                static int m_vidstabParam_iCropmode = 0;
                static bool m_vidstabParam_bRelative = true;
                static float m_vidstabParam_fFixedZoom = 0;
                static int m_vidstabParam_iAutoZoomMode = 0;
                static const char* s_vidstabParam_aAutoZoomModes[] = { "Disable", "Static", "Adaptive" };
                static float m_vidstabParam_fAutoZoomSpeed = 0.25f;
                static int m_vidstabParam_iInterpolationMode = 2;
                static const char* s_vidstabParam_aInterpolations[] = { "Nearest", "Linear", "Bilinear", "Bicubic" };
                tTagColor = ImColor(KNOWNIMGUICOLOR_LIGHTBLUE);
                const auto v2TagTextPadding = ImGui::GetStyle().FramePadding;
                const auto fTextHeight = ImGui::CalcTextSize("A").y;
                ImGui::Spacing(); ImGui::Spacing();
                ImGui::BeginGroup();
                if (ImGui::Button("Reset params##VidstabParamResetParams"))
                {
                    m_vidstabParam_iShakiness = 5;
                    m_vidstabParam_iAccuracy = 15;
                    m_vidstabParam_iStepsize = 6;
                    m_vidstabParam_fMincontrast = 0.3f;
                    m_vidstabParam_iSmoothing = 10;
                    m_vidstabParam_iOptalgo = 0;
                    m_vidstabParam_bMaxshiftNolimit = true;
                    m_vidstabParam_iMaxshift = 0;
                    m_vidstabParam_bMaxangleNolimit = true;
                    m_vidstabParam_fMaxangle = 0;
                    m_vidstabParam_iCropmode = 0;
                    m_vidstabParam_bRelative = true;
                    m_vidstabParam_fFixedZoom = 0;
                    m_vidstabParam_iAutoZoomMode = 0;
                    m_vidstabParam_fAutoZoomSpeed = 0.25f;
                    m_vidstabParam_iInterpolationMode = 2;
                }
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Shakiness:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Accuracy:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Step size:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Min contrast:");
                ImGui::EndGroup(); ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::Dummy({0, fTextHeight+v2TagTextPadding.y*2.f});
                ImGui::PushItemWidth(60);
                ImGui::SliderInt("##VidstabParamShakiness", &m_vidstabParam_iShakiness, 0, 10, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick);
                ImGui::SliderInt("##VidstabParamAccuracy", &m_vidstabParam_iAccuracy, 1, 15, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick);
                ImGui::SliderInt("##VidstabParamStepsize", &m_vidstabParam_iStepsize, 1, 32, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick);
                ImGui::SliderFloat("##VidstabParamMincontrast", &m_vidstabParam_fMincontrast, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick);
                ImGui::PopItemWidth();
                ImGui::EndGroup(); ImGui::SameLine(0, 40);
                ImGui::BeginGroup();
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Smoothing:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Opt algo:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Max shift:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Max angle:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Crop mode:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Frame relative:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Fixed zoom:");
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Auto zoom:");
                ImGui::BeginDisabled(m_vidstabParam_iAutoZoomMode != 2);
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Auto zoom speed:");
                ImGui::EndDisabled();
                ImGui::TextColoredWithPadding(tTagColor, v2TagTextPadding, "Interpolation:");
                ImGui::EndGroup(); ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::PushItemWidth(80);
                ImGui::PushItemWidth(140);
                ImGui::SliderInt("##VidstabParamSmoothing", &m_vidstabParam_iSmoothing, 0, 300, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick);
                ImGui::PopItemWidth();
                ImGui::RadioButton("Gauss##VidstabParamOptalgo", &m_vidstabParam_iOptalgo, 0); ImGui::SameLine();
                ImGui::RadioButton("Avg##VidstabParamOptalgo", &m_vidstabParam_iOptalgo, 1);
                ImGui::BeginDisabled(m_vidstabParam_bMaxshiftNolimit);
                ImGui::SliderInt("##VidstabParamMaxshift", &m_vidstabParam_iMaxshift, 0, 500, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick); ImGui::SameLine();
                ImGui::EndDisabled();
                ImGui::Checkbox("No limit##VidstabParamMaxshift", &m_vidstabParam_bMaxshiftNolimit);
                ImGui::BeginDisabled(m_vidstabParam_bMaxangleNolimit);
                ImGui::SliderFloat("##VidstabParamMaxangle", &m_vidstabParam_fMaxangle, 0, 180, "%.2f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick); ImGui::SameLine();
                ImGui::EndDisabled();
                ImGui::Checkbox("No limit##VidstabParamMaxangle", &m_vidstabParam_bMaxangleNolimit);
                ImGui::RadioButton("Keep##VidstabParamCropmode", &m_vidstabParam_iCropmode, 0); ImGui::SameLine();
                ImGui::RadioButton("Black##VidstabParamCropmode", &m_vidstabParam_iCropmode, 1);
                ImGui::Checkbox("##VidstabParamRelative", &m_vidstabParam_bRelative);
                ImGui::PushItemWidth(140);
                ImGui::InputFloat("##VidstabParamFixedZoom", &m_vidstabParam_fFixedZoom, 0.1f, 5.f, "%.1f");
                if (ImGui::BeginCombo("##VidstabParamAutoZoomMode", s_vidstabParam_aAutoZoomModes[m_vidstabParam_iAutoZoomMode]))
                {
                    const int iOptCnt = sizeof(s_vidstabParam_aAutoZoomModes)/sizeof(s_vidstabParam_aAutoZoomModes[0]);
                    for (auto i = 0; i < iOptCnt; i++)
                    {
                        const bool bIsSelected = i == m_vidstabParam_iAutoZoomMode;
                        if (ImGui::Selectable(s_vidstabParam_aAutoZoomModes[i], bIsSelected))
                            m_vidstabParam_iAutoZoomMode = i;
                        if (bIsSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::BeginDisabled(m_vidstabParam_iAutoZoomMode != 2);
                ImGui::SliderFloat("##VidstabParamAutoZoomSpeed", &m_vidstabParam_fAutoZoomSpeed, 0.f, 5.f, "%.2f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick);
                ImGui::EndDisabled();
                if (ImGui::BeginCombo("##VidstabParamInterpolation", s_vidstabParam_aInterpolations[m_vidstabParam_iInterpolationMode]))
                {
                    const int iOptCnt = sizeof(s_vidstabParam_aInterpolations)/sizeof(s_vidstabParam_aInterpolations[0]);
                    for (auto i = 0; i < iOptCnt; i++)
                    {
                        const bool bIsSelected = i == m_vidstabParam_iInterpolationMode;
                        if (ImGui::Selectable(s_vidstabParam_aInterpolations[i], bIsSelected))
                            m_vidstabParam_iInterpolationMode = i;
                        if (bIsSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::PopItemWidth();
                ImGui::EndGroup(); ImGui::SameLine();
                ImGui::Dummy({0, 0});

                bCloseDlg = false;
                MEC::BackgroundTask::Holder hTask;
                if (ImGui::Button("   OK   "))
                {
                    imgui_json::value jnTask;
                    jnTask["type"] = "Vidstab";
                    jnTask["project_dir"] = timeline->mhProject->GetProjectDir();
                    jnTask["source_url"] = hParser->GetUrl();
                    jnTask["is_image_seq"] = IS_IMAGESEQ(pClip->mType);
                    jnTask["clip_id"] = imgui_json::number(pClip->mID);
                    jnTask["clip_start_offset"] = imgui_json::number(pClip->StartOffset());
                    jnTask["clip_length"] = imgui_json::number(pClip->Length());
                    jnTask["use_src_attr"] = true;
                    // send vidstab params
                    jnTask["vidstab_arg_shakiness"] = imgui_json::number(m_vidstabParam_iShakiness);
                    jnTask["vidstab_arg_accuracy"] = imgui_json::number(m_vidstabParam_iAccuracy);
                    jnTask["vidstab_arg_stepsize"] = imgui_json::number(m_vidstabParam_iStepsize);
                    jnTask["vidstab_arg_mincontrast"] = imgui_json::number(m_vidstabParam_fMincontrast);
                    jnTask["vidstab_arg_smoothing"] = imgui_json::number(m_vidstabParam_iSmoothing);
                    jnTask["vidstab_arg_optalgo"] = imgui_json::number(m_vidstabParam_iOptalgo);
                    jnTask["vidstab_arg_maxshift"] = m_vidstabParam_bMaxshiftNolimit ? imgui_json::number(-1) : imgui_json::number(m_vidstabParam_iMaxshift);
                    jnTask["vidstab_arg_maxangle"] = m_vidstabParam_bMaxangleNolimit ? imgui_json::number(-1) : imgui_json::number(m_vidstabParam_fMaxangle);
                    jnTask["vidstab_arg_crop"] = imgui_json::number(m_vidstabParam_iCropmode);
                    jnTask["vidstab_arg_relative"] = m_vidstabParam_bRelative;
                    jnTask["vidstab_arg_zoom"] = m_vidstabParam_fFixedZoom;
                    jnTask["vidstab_arg_optzoom"] = imgui_json::number(m_vidstabParam_iAutoZoomMode);
                    jnTask["vidstab_arg_zoomspeed"] = imgui_json::number(m_vidstabParam_fAutoZoomSpeed);
                    jnTask["vidstab_arg_interp"] = imgui_json::number(m_vidstabParam_iInterpolationMode);
                    auto hSettings = timeline->mhMediaSettings->Clone();
                    hTask = MEC::BackgroundTask::CreateBackgroundTask(jnTask, hSettings);
                    bCloseDlg = true;
                } ImGui::SameLine(0, 10);
                if (ImGui::Button(" Cancel "))
                    bCloseDlg = true;
                return hTask;
            },
        },
    };
    static size_t s_szBgtaskSelIdx;
    static string s_strBgtaskCreateDlgLabel;
    static int64_t s_i64BgtaskSrcClipId;
    bool bOpenCreateBgtaskDialog = false;

    float minPixelWidthTarget = ImMin(timeline->msPixelWidthTarget, (float)(timline_size.x - legendWidth) / (float)duration);
    const auto frameRate = timeline->mhMediaSettings->VideoOutFrameRate();
    float frame_duration = (frameRate.den > 0 && frameRate.num > 0) ? frameRate.den * 1000.0 / frameRate.num : 40;
    float maxPixelWidthTarget = (timeline->mSnapShotWidth > 0.0 ? timeline->mSnapShotWidth : 60.f) / frame_duration;
    timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
    int view_frames = timeline->mSnapShotWidth > 0 ? (float)(timline_size.x - legendWidth) / timeline->mSnapShotWidth : 16;

    if (timeline->visibleTime >= duration)
        timeline->firstTime = timeline->GetStart();
    timeline->lastTime = timeline->firstTime + timeline->visibleTime;
    
    int controlHeight = trackCount * trackHeadHeight;
    for (int i = 0; i < trackCount; i++)
        controlHeight += int(timeline->GetCustomHeight(i));

    std::list<imgui_json::value> actionList; // wyvern: add this 'actionList' to save the operation records, for UNDO/REDO.

    if (lastFirstTime != -1 && lastFirstTime != timeline->firstTime) need_save = true;
    if (lastVisiableTime != -1 && lastVisiableTime != newVisibleTime) need_save = true;
    lastFirstTime = timeline->firstTime;
    lastVisiableTime = newVisibleTime;

    ImGui::BeginGroup();
    bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    ImGui::SetCursorScreenPos(canvas_pos);
    if ((expanded && !*expanded) || !trackCount)
    {
        // minimum view
        draw_list->AddRectFilled(canvas_pos + ImVec2(legendWidth, 0), ImVec2(timline_size.x + canvas_pos.x, canvas_pos.y + HeadHeight + 4), COL_DARK_ONE, 0);
        auto info_str = ImGuiHelper::MillisecToString(duration, 3);
        info_str += " / ";
        info_str += std::to_string(trackCount) + " tracks";
        draw_list->AddText(ImVec2(canvas_pos.x + legendWidth + 2, canvas_pos.y + 4), IM_COL32_WHITE, info_str.c_str());
        if (!trackCount && *expanded)
        {
            ImGui::SetWindowFontScale(4);
            auto pos_center = canvas_pos + canvas_size / 2;
            std::string tips_string = "Please Drag Media Here";
            auto string_width = ImGui::CalcTextSize(tips_string.c_str());
            auto tips_pos = pos_center - string_width / 2;
            ImGui::SetWindowFontScale(1.0);
            draw_list->AddTextComplex(tips_pos, tips_string.c_str(), 4.f, IM_COL32(255, 255, 255, 128), 0.5f, IM_COL32(56, 56, 56, 192));

            if (regionRect.Contains(io.MousePos) && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
            {
                ImGui::OpenPopup("##empty-timeline-context-menu");
            }
            if (ImGui::BeginPopup("##empty-timeline-context-menu"))
            {
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0,1.0,1.0,1.0));
            
                if (ImGui::MenuItem(ICON_MEDIA_VIDEO  " Insert Empty Video Track", nullptr, nullptr))
                {
                    insertEmptyTrackType = MEDIA_VIDEO;
                }
                if (ImGui::MenuItem(ICON_MEDIA_AUDIO " Insert Empty Audio Track", nullptr, nullptr))
                {
                    insertEmptyTrackType = MEDIA_AUDIO;
                }
                if (ImGui::MenuItem(ICON_MEDIA_TEXT " Insert Empty Text Track", nullptr, nullptr))
                {
                    insertEmptyTrackType = MEDIA_TEXT;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
        }
    }
    else
    {
        // normal view
        // ToolBar view
        ImVec2 toolBarSize(timline_size.x, (float)toolbar_height);
        ImRect ToolBarAreaRect(canvas_pos, canvas_pos + toolBarSize);
        ImVec2 HeaderPos = ImGui::GetCursorScreenPos() + ImVec2(0, toolbar_height);
        // draw ToolBar bg
        draw_list->AddRectFilled(ToolBarAreaRect.Min, ToolBarAreaRect.Max, COL_DARK_PANEL, 0);
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7, 0.7, 0.7, 1.0));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0, 1.0, 1.0, 0.5));

        ImGui::SetCursorScreenPos(ToolBarAreaRect.Min + ImVec2(legendWidth, 4));
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 1.0, 0.5));
        ImGui::TextUnformatted(ICON_TOOLBAR_START);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("+" ICON_MEDIA_VIDEO "##main_timeline_insert_empty_video_track"))
        {
            insertEmptyTrackType = MEDIA_VIDEO;
        }
        ImGui::ShowTooltipOnHover("Insert Empty Video Track");
        ImGui::SameLine();
        if (ImGui::Button("+" ICON_MEDIA_AUDIO "##main_timeline_insert_empty_audio_track"))
        {
            insertEmptyTrackType = MEDIA_AUDIO;
        }
        ImGui::ShowTooltipOnHover("Insert Empty Audio Track");

        ImGui::SameLine();
        if (ImGui::Button("+" ICON_MEDIA_TEXT "##main_timeline_insert_empty_text_track"))
        {
            insertEmptyTrackType = MEDIA_TEXT;
        }
        ImGui::ShowTooltipOnHover("Insert Empty Text Track");

        ImGui::SameLine();
        ImGui::BeginDisabled(timeline->GetEmptyTrackCount() <= 0);
        if (ImGui::Button("-" ICON_EMPTY_TRACK "##main_timeline_detele_empty_track"))
        {
            removeEmptyTrack = true;
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Delete Empty Track");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();
        if (ImGui::Button(ICON_MARK_IN "##main_timeline_add_mark_in"))
        {
            if (timeline->mark_out != -1 && timeline->mCurrentTime > timeline->mark_out)
                timeline->mark_out = -1;
            timeline->mark_in = timeline->mCurrentTime;
            headerMarkPos = -1;
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Add mark in");

        ImGui::SameLine();
        if (ImGui::Button(ICON_MARK_OUT "##main_timeline_add_mark_out"))
        {
            if (timeline->mark_in != -1 && timeline->mCurrentTime < timeline->mark_in)
                timeline->mark_in = -1;
            timeline->mark_out = timeline->mCurrentTime;
            headerMarkPos = -1;
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Add mark out");

        ImGui::BeginDisabled(timeline->mark_out == -1 && timeline->mark_in == -1);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MARK_NONE "##main_timeline_add_mark_out"))
        {
            timeline->mark_in = timeline->mark_out = -1;
            headerMarkPos = -1;
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Delete mark point");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

#if 0   // TODO::Dicky need add more editing function
        auto _clip = timeline->FindClipByID(clipMenuEntry);
        auto _track = _clip ? timeline->FindTrackByClipID(_clip->mID) : nullptr;
        bool _disable_editing = _clip ? IS_DUMMY(_clip->mType) : true;
        ImGui::BeginDisabled(_disable_editing);
        ImGui::SameLine();
        if (ImGui::Button(ICON_CROP "##main_timeline_edit_clip_attribute"))
        {
            _track->SelectEditingClip(_clip, false);
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Edit Clip Attribute");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FILTER_EDITOR "##main_timeline_edit_clip_filter"))
        {
            _track->SelectEditingClip(_clip, true);
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Edit Clip Filter");

        ImGui::SameLine();
        if (ImGui::Button(ICON_MEDIA_DELETE_CLIP "##main_timeline_delete_clip") && clipMenuEntry != -1)
        {
            delClipEntry.push_back(clipMenuEntry);
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Delete Clip");

        ImGui::BeginDisabled(!_clip || _clip->mGroupID == -1);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MEDIA_UNGROUP "##main_timeline_ungroup_clip") && clipMenuEntry != -1)
        {
            unGroupClipEntry.push_back(clipMenuEntry);
            changed = true;
        }
        ImGui::ShowTooltipOnHover("Ungroup Clip");
        ImGui::EndDisabled();
        ImGui::EndDisabled();
#endif
        ImGui::BeginDisabled(timeline->GetSelectedClipCount() <= 0);
        ImGui::SameLine();
        if (ImGui::Button(ICON_DELETE_CLIPS "##main_timeline_delete_selected"))
        {
            for (auto clip : timeline->m_Clips)
            {
                if (clip->bSelected)
                {
                    delClipEntry.push_back(clip->mID);
                    changed = true;
                }
            }
        }
        ImGui::ShowTooltipOnHover("Delete Selected");

        ImGui::BeginDisabled(timeline->GetSelectedClipCount() <= 1);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MEDIA_GROUP "##main_timeline_group_selected"))
        {
            for (auto clip : timeline->m_Clips)
            {
                if (clip->bSelected)
                {
                    groupClipEntry.push_back(clip->mID);
                    changed = true;
                }
            }
        }
        ImGui::ShowTooltipOnHover("Group Selected");
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(ICON_MEDIA_UNGROUP "##main_timeline_ungroup_selected"))
        {
            for (auto clip : timeline->m_Clips)
            {
                if (clip->bSelected)
                {
                    unGroupClipEntry.push_back(clip->mID);
                    changed = true;
                }
            }
        }
        ImGui::ShowTooltipOnHover("Ungroup Selected");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();
        if (ImGui::RotateButton(ICON_CLIP_START "##slider_to_start", ImVec2(0, 0), -180))
        {
            timeline->firstTime = timeline->GetStart();
            need_save = true;
        }
        ImGui::ShowTooltipOnHover("Slider to Start");

        ImGui::SameLine();
        if (ImGui::Button(ICON_SLIDER_FRAME "##slider_maximum"))
        {
            timeline->msPixelWidthTarget = maxPixelWidthTarget;
            need_save = true;
        }
        ImGui::ShowTooltipOnHover("Frame accuracy");

        ImGui::SameLine();
        if (ImGui::RotateButton(ICON_ZOOM_IN "##slider_zoom_in", ImVec2(0, 0), -90))
        {
            timeline->msPixelWidthTarget *= 2.0f;
            if (timeline->msPixelWidthTarget > maxPixelWidthTarget)
                timeline->msPixelWidthTarget = maxPixelWidthTarget;
            need_save = true;
        }
        ImGui::ShowTooltipOnHover("Accuracy Zoom In");

        ImGui::SameLine();
        if (ImGui::Button(ICON_CURRENT_TIME "##timeline_current_time"))
        {
            timeline->firstTime = timeline->mCurrentTime - timeline->visibleTime / 2;
            timeline->firstTime = ImClamp(timeline->firstTime, (int64_t)0, ImMax(duration - timeline->visibleTime, (int64_t)0));
            need_save = true;
        }
        ImGui::ShowTooltipOnHover("Current time");

        ImGui::SameLine();
        if (ImGui::RotateButton(ICON_ZOOM_OUT "##slider_zoom_out", ImVec2(0, 0), -90))
        {
            timeline->msPixelWidthTarget *= 0.5f;
            if (timeline->msPixelWidthTarget < minPixelWidthTarget)
                timeline->msPixelWidthTarget = minPixelWidthTarget;
            need_save = true;
        }
        ImGui::ShowTooltipOnHover("Accuracy Zoom Out");

        ImGui::SameLine();
        if (ImGui::Button(ICON_SLIDER_CLIP "##slider_minimum"))
        {
            timeline->msPixelWidthTarget = minPixelWidthTarget;
            timeline->firstTime = timeline->GetStart();
            need_save = true;
        }
        ImGui::ShowTooltipOnHover("Timeline accuracy");

        ImGui::SameLine();
        if (ImGui::Button(ICON_CLIP_START "##slider_to_end"))
        {
            timeline->firstTime = timeline->GetEnd() - timeline->visibleTime;
            need_save = true;
        }
        ImGui::ShowTooltipOnHover("Slider to End");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        //TODO::Dicky Add more toolbar items

        ImGui::PopStyleColor(4);
        draw_list->AddLine(ToolBarAreaRect.Min + ImVec2(0, toolbar_height - 1), ToolBarAreaRect.Max + ImVec2(0, -1), IM_COL32(255, 255, 255, 224));

        ImGui::SetCursorScreenPos(HeaderPos);
        ImVec2 headerSize(timline_size.x - 4.f, (float)HeadHeight);
        ImVec2 HorizonScrollBarSize(timline_size.x, scrollSize);
        ImVec2 VerticalScrollBarSize(scrollSize / 2, canvas_size.y - scrollSize - HeadHeight);
        ImRect HeaderAreaRect(canvas_pos + ImVec2(legendWidth, toolbar_height), canvas_pos + headerSize + ImVec2(0, toolbar_height));
        ImGui::InvisibleButton("topBar", headerSize);

        // draw Header bg
        draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Max, COL_DARK_ONE, 0);
        
        if (!trackCount) 
        {
            ImGui::EndGroup();
            return changed;
        }
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImVec2 childFramePos = ImGui::GetCursorScreenPos();
        ImVec2 childFrameSize(timline_size.x, timline_size.y - 8.0f - toolBarSize.y - headerSize.y - HorizonScrollBarSize.y);
        ImGui::BeginChildFrame(ImGui::GetID("timeline_Tracks"), childFrameSize, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        auto VerticalScrollPos = ImGui::GetScrollY();
        auto VerticalScrollMax = ImGui::GetScrollMaxY();
        auto VerticalWindow = ImGui::GetCurrentWindow();
        ImGui::InvisibleButton("contentBar", ImVec2(timline_size.x - 8.f, float(controlHeight)));
        const ImVec2 contentMin = ImGui::GetItemRectMin();
        const ImVec2 contentMax = ImGui::GetItemRectMax();
        const ImRect contentRect(contentMin, contentMax);
        const ImRect legendRect(contentMin, ImVec2(contentMin.x + legendWidth, contentMax.y));
        const ImRect legendAreaRect(contentMin, ImVec2(contentMin.x + legendWidth, contentMin.y + timline_size.y - (HeadHeight + 8)));
        const ImRect trackRect(ImVec2(contentMin.x + legendWidth, contentMin.y), contentMax);
        const ImRect trackAreaRect(ImVec2(contentMin.x + legendWidth, contentMin.y), ImVec2(contentMax.x, contentMin.y + timline_size.y - (HeadHeight + scrollSize + 8)));
        const ImRect timeMeterRect(ImVec2(contentMin.x + legendWidth, HeaderAreaRect.Min.y), ImVec2(contentMin.x + timline_size.x, HeaderAreaRect.Min.y + HeadHeight + 8));

        const float contentHeight = contentMax.y - contentMin.y;
        // full canvas background
        draw_list->AddRectFilled(canvas_pos + ImVec2(4, HeadHeight + 4), canvas_pos + ImVec2(4, HeadHeight + 4) + timline_size - ImVec2(8, HeadHeight + scrollSize + 8), COL_CANVAS_BG, 0);

        // for debug
        //draw_list->AddRect(trackRect.Min, trackRect.Max, IM_COL32(255, 0, 0, 255), 0, 0, 2);
        //draw_list->AddRect(trackAreaRect.Min, trackAreaRect.Max, IM_COL32(0, 0, 255, 255), 0, 0, 2);
        //draw_list->AddRect(legendRect.Min, legendRect.Max, IM_COL32(0, 255, 0, 255), 0, 0, 2);
        //draw_list->AddRect(legendAreaRect.Min, legendAreaRect.Max, IM_COL32(255, 255, 0, 255), 0, 0, 2);
        // for debug end

        // calculate mouse pos to time
        mouseTime = (int64_t)((io.MousePos.x - contentMin.x - legendWidth) / timeline->msPixelWidthTarget) + timeline->firstTime;
        auto alignedTime = timeline->AlignTime(mouseTime);
        if (alignedTime >= timeline->firstTime)
            alignedMousePosX = ((alignedTime - timeline->firstTime) * timeline->msPixelWidthTarget) + contentMin.x + legendWidth;
        menuIsOpened = ImGui::IsPopupOpen("##timeline-context-menu") || ImGui::IsPopupOpen("##timeline-header-context-menu");

        //header
        //header time and lines
        int64_t modTimeCount = 10;
        int timeStep = 1;
        while ((modTimeCount * timeline->msPixelWidthTarget) < 75)
        {
            modTimeCount *= 10;
            timeStep *= 10;
        };
        int halfModTime = modTimeCount / 2;
        auto drawLine = [&](int64_t i, int regionHeight)
        {
            bool baseIndex = ((i % modTimeCount) == 0) || (i == timeline->GetEnd() || i == timeline->GetStart());
            bool halfIndex = (i % halfModTime) == 0;
            int px = (int)contentMin.x + int(i * timeline->msPixelWidthTarget) + legendWidth - int(timeline->firstTime * timeline->msPixelWidthTarget);
            int timeStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int timeEnd = baseIndex ? regionHeight : HeadHeight - 8;
            if (px <= (timline_size.x + contentMin.x) && px >= (contentMin.x + legendWidth))
            {
                draw_list->AddLine(ImVec2((float)px, HeaderAreaRect.Min.y + (float)timeStart), ImVec2((float)px, HeaderAreaRect.Min.y + (float)timeEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
            }
            if (baseIndex && px > (contentMin.x + legendWidth))
            {
                auto time_str = ImGuiHelper::MillisecToString(i, 2);
                ImGui::SetWindowFontScale(0.8);
                draw_list->AddText(ImVec2((float)px + 3.f, HeaderAreaRect.Min.y + 8), COL_RULE_TEXT, time_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
        };
        auto drawLineContent = [&](int64_t i, int)
        {
            int px = (int)contentMin.x + int(i * timeline->msPixelWidthTarget) + legendWidth - int(timeline->firstTime * timeline->msPixelWidthTarget);
            int timeStart = int(contentMin.y);
            int timeEnd = int(contentMax.y);
            if (px <= (timline_size.x + contentMin.x) && px >= (contentMin.x + legendWidth))
            {
                draw_list->AddLine(ImVec2(float(px), float(timeStart)), ImVec2(float(px), float(timeEnd)), COL_SLOT_V_LINE, 1);
            }
        };
        auto _mark_start = (timeline->firstTime / timeStep) * timeStep;
        auto _mark_end = (timeline->lastTime / timeStep) * timeStep;
        for (auto i = _mark_start; i <= _mark_end; i += timeStep)
        {
            drawLine(i, HeadHeight);
        }

        // cursor Arrow
        if (timeline->mCurrentTime >= timeline->firstTime && timeline->mCurrentTime <= timeline->GetEnd())
        {
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + legendWidth + (timeline->mCurrentTime - timeline->firstTime) * timeline->msPixelWidthTarget - arrowWidth * 0.5f + 1;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, HeaderAreaRect.Min.y), COL_CURSOR_ARROW, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = ImGuiHelper::MillisecToString(timeline->mCurrentTime, 2);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + legendWidth + (timeline->mCurrentTime - timeline->firstTime) * timeline->msPixelWidthTarget - str_size.x * 0.5f + 1;
            if (strOffset + str_size.x > contentMax.x) strOffset = contentMax.x - str_size.x;
            ImVec2 str_pos = ImVec2(strOffset, HeaderAreaRect.Min.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }

        // crop content
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize, true);

        // track background
        size_t customHeight = 0;
        for (int i = 0; i < trackCount; i++)
        {
            unsigned int col = (i & 1) ? COL_SLOT_ODD : COL_SLOT_EVEN;
            size_t localCustomHeight = timeline->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth, contentMin.y + trackHeadHeight * i + 1 + customHeight);
            ImVec2 sz = ImVec2(timline_size.x + contentMin.x, pos.y + trackHeadHeight - 1 + localCustomHeight);
            if (io.MousePos.y >= pos.y && io.MousePos.y < pos.y + (trackHeadHeight + localCustomHeight) && clipMovingEntry == -1 && io.MousePos.x > contentMin.x && io.MousePos.x < contentMin.x + timline_size.x)
            {
                col += IM_COL32(8, 16, 32, 128);
                pos.x -= legendWidth;
            }
            draw_list->AddRectFilled(pos, sz, col, 0);
            customHeight += localCustomHeight;
        }

        // draw track legend area title bar
        customHeight = 0;
        for (int i = 0; i < trackCount; i++)
        {
            auto itemCustomHeight = timeline->GetCustomHeight(i);
            ImVec2 button_size = ImVec2(14, 14);
            ImVec2 tpos(contentMin.x, contentMin.y + i * trackHeadHeight + customHeight);
            bool is_delete = TimelineButton(draw_list, ICON_TRASH, ImVec2(contentMin.x + legendWidth - button_size.x - 12 - 4, tpos.y + 2), button_size, "delete");
            if (is_delete && editable && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                delTrackEntry = i;
            customHeight += itemCustomHeight;
        }

        draw_list->PushClipRect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize, true);
        // vertical time lines in content area
        for (auto i = _mark_start; i <= _mark_end; i += timeStep)
        {
            drawLineContent(i, int(contentHeight));
        }

        // track
        customHeight = 0;
        for (int i = 0; i < trackCount; i++)
        {
            size_t localCustomHeight = timeline->GetCustomHeight(i);
            unsigned int color = COL_SLOT_DEFAULT;
            ImVec2 pos = ImVec2(contentMin.x + legendWidth - timeline->firstTime * timeline->msPixelWidthTarget, contentMin.y + trackHeadHeight * i + 1 + customHeight);
            ImVec2 slotP1(pos.x + timeline->firstTime * timeline->msPixelWidthTarget, pos.y + 2);
            ImVec2 slotP2(pos.x + timeline->lastTime * timeline->msPixelWidthTarget, pos.y + trackHeadHeight - 2);
            ImVec2 slotP3(pos.x + timeline->lastTime * timeline->msPixelWidthTarget, pos.y + trackHeadHeight - 2 + localCustomHeight);
            unsigned int slotColor = color | IM_COL32_BLACK;
            unsigned int slotColorHalf = HALF_COLOR(color);
            if (slotP1.x <= (timline_size.x + contentMin.x) && slotP2.x >= (contentMin.x + legendWidth))
            {
                draw_list->AddRectFilled(slotP1, slotP3, slotColorHalf, 0);
                draw_list->AddRectFilled(slotP1, slotP2, slotColor, 0);
            }
            if (ImRect(slotP1, slotP3).Contains(io.MousePos))
            {
                mouseEntry = i;
            }
            ImVec2 slotLegendP1(contentMin.x, contentMin.y + trackHeadHeight * i + customHeight);
            ImVec2 slotLegendP2(slotLegendP1.x + legendWidth, slotLegendP1.y + trackHeadHeight + localCustomHeight);
            if (ImRect(slotLegendP1, slotLegendP2).Contains(io.MousePos))
            {
                legendEntry = i;
            }

            // Ensure grabable handles for track
            if (legendEntry != -1 && legendEntry < timeline->m_Tracks.size())
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left) && MovingHorizonScrollBar == -1 && !MovingCurrentTime && !menuIsOpened && editable)
                {
                    trackMovingEntry = legendEntry;
                }
            }

            // Ensure grabable handles and find selected clip
            for (auto clip : timeline->m_Clips)  clip->bHovered = false; // clear clip hovered status
            if (mouseTime != -1 && mouseEntry >= 0 && mouseEntry < timeline->m_Tracks.size())
            {
                MediaTrack * track = timeline->m_Tracks[mouseEntry];
                if (track)
                {
                    Clip * mouse_clip = nullptr;
                    // [shortcut]: left shift swap top/bottom clip if overlaped
                    bool swap_clip = ImGui::IsKeyDown(ImGuiKey_LeftShift);
                    mouseClip.clear();
                    // it should be at most 2 clips under mouse
                    for (auto clip : track->m_Clips)
                    {
                        if (clip->IsInClipRange(mouseTime))
                        {
                            mouseClip.push_back(clip->mID);
                        }
                    }
                    if (!mouseClip.empty())
                    {
                        if (mouseClip.size() == 1 || !swap_clip) { mouse_clip = timeline->FindClipByID(mouseClip[0]); mouse_clip->bHovered = true; }
                        else if (mouseClip.size() == 2 && swap_clip) { mouse_clip = timeline->FindClipByID(mouseClip[1]); mouse_clip->bHovered = true; }
                    }
                    if (mouse_clip && clipMovingEntry == -1)
                    {
                        auto clip_view_width = mouse_clip->Length() * timeline->msPixelWidthTarget;
                        if (clip_view_width <= 20)
                        {
                            // clip is too small, don't dropping
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && MovingHorizonScrollBar == -1 && !MovingCurrentTime && !menuIsOpened && editable)
                            {
                                clipMovingEntry = mouse_clip->mID;
                                clipMovingPart = 3;
                            }
                        }
                        else
                        {
                            // check clip moving part
                            ImVec2 clipP1(pos.x + mouse_clip->Start() * timeline->msPixelWidthTarget, pos.y + 2);
                            ImVec2 clipP2(pos.x + mouse_clip->End() * timeline->msPixelWidthTarget, pos.y + trackHeadHeight - 2 + localCustomHeight);
                            const float max_handle_width = clipP2.x - clipP1.x / 3.0f;
                            const float min_handle_width = ImMin(10.0f, max_handle_width);
                            const float handle_width = ImClamp(timeline->msPixelWidthTarget / 2.0f, min_handle_width, max_handle_width);
                            ImRect rects[3] = {ImRect(clipP1, ImVec2(clipP1.x + handle_width, clipP2.y)), ImRect(ImVec2(clipP2.x - handle_width, clipP1.y), clipP2), ImRect(clipP1 + ImVec2(handle_width, 0), clipP2 - ImVec2(handle_width, 0))};

                            for (int j = 1; j >= 0; j--)
                            {
                                ImRect &rc = rects[j];
                                if (!rc.Contains(io.MousePos))
                                    continue;
                                if (j == 0)
                                    ImGui::RenderMouseCursor(ICON_CROPPING_LEFT, ImVec2(4, 0));
                                else
                                    ImGui::RenderMouseCursor(ICON_CROPPING_RIGHT, ImVec2(12, 0));
                                draw_list->AddRectFilled(rc.Min, rc.Max, IM_COL32(255,0,0,255), 0);
                            }
                            for (int j = 0; j < 3; j++)
                            {
                                // j == 0 : left crop rect
                                // j == 1 : right crop rect
                                // j == 2 : cutting rect
                                ImRect &rc = rects[j];
                                if (!rc.Contains(io.MousePos))
                                    continue;
                                if (!ImRect(childFramePos, childFramePos + childFrameSize).Contains(io.MousePos))
                                    continue;
                                if (j == 2 && timeline->mIsCutting && mouseClip.size() <= 1 && editable)
                                {
                                    ImGui::RenderMouseCursor(ICON_CUTTING, ImVec2(7, 0), 1.0, -90);
                                }
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && MovingHorizonScrollBar == -1 && !MovingCurrentTime && !menuIsOpened && editable)
                                {
                                    if (j == 2 && timeline->mIsCutting && mouseClip.size() <= 1)
                                    {
                                        doCutPos = mouseTime;
                                    }
                                    else
                                    {
                                        clipMovingEntry = mouse_clip->mID;
                                        clipMovingPart = j + 1;
                                        if (j <= 1)
                                        {
                                            bCropping = true;
                                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                                        }
                                        clipClickedTriggered = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            customHeight += localCustomHeight;
        }

        // check mouseEntry is below track area
        if (mouseEntry < 0 && trackAreaRect.Contains(io.MousePos) && io.MousePos.y >= trackRect.Max.y)
        {
            mouseEntry = -2;
        }

        // track moving
        if (trackMovingEntry != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImGui::CaptureMouseFromApp();
            bTrackMoving = true;
            trackEntry = -1;
            ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
            ImGui::SetNextWindowPos(ImVec2(legendRect.Min.x + 8, io.MousePos.y - trackHeadHeight));
            ImGui::SetNextWindowBgAlpha(0.25);
            if (ImGui::BeginTooltip())
            {
                ImVec2 button_size = ImVec2(12, 12);
                MediaTrack *track = timeline->m_Tracks[trackMovingEntry];
                size_t localCustomHeight = track->mExpanded ? track->mTrackHeight : 12;
                ImRect rc(ImVec2(0, 0), ImVec2(float(legendWidth), float(localCustomHeight)));
                int button_count = 0;
                ImGui::SetWindowFontScale(0.75);
                ImGui::TextUnformatted(track->mLocked ? ICON_LOCKED : ICON_UNLOCK);
                button_count ++;
                ImGui::SameLine();
                if (IS_AUDIO(track->mType)) { ImGui::TextUnformatted(track->mView ? ICON_SPEAKER : ICON_SPEAKER_MUTE); }
                else { ImGui::TextUnformatted(track->mView ? ICON_VIEW : ICON_VIEW_DISABLE); }
                button_count ++;
                ImGui::SameLine();
                ImGui::AddTextRolling(track->mName.c_str(), ImVec2(rc.GetSize().x - 24 - button_count * 1.5 * button_size.x, 16), 5);
                ImGui::SetWindowFontScale(1.0);
                ImGui::EndTooltip();
            }
        }
        if (bTrackMoving && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (io.MousePos.y > legendAreaRect.Min.y && io.MousePos.y < legendAreaRect.Max.y)
            {
                trackEntry = legendEntry;
                if (trackEntry == -1 && io.MousePos.y >= legendRect.Max.y)
                {
                    trackEntry = -2; // end of tracks
                }
            }
            bTrackMoving = false;
            changed = true;
        }

        // clip cropping or moving
        if (clipMovingEntry != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            Clip * clip = timeline->FindClipByID(clipMovingEntry);
            ImGui::CaptureMouseFromApp();
            if (diffTime == 0)
            {
                if (clipMovingPart == 3 || clipMovingPart == 1)
                    clip->mDragAnchorTime = clip->Start();
                else if (clipMovingPart == 2)
                    clip->mDragAnchorTime = clip->End();
            }
            diffTime += io.MouseDelta.x / timeline->msPixelWidthTarget;

            if (clipMovingPart == 3)
            {
                bClipMoving = true;
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                // whole slot moving
                auto diff = diffTime-clip->Start()+clip->mDragAnchorTime;
                int dst_entry = clip->Moving(diff, mouseEntry);
                if (dst_entry >= 0)
                    mouseEntry = dst_entry;
            }
            else if (clipMovingPart & 1)
            {
                // clip left cropping
                auto diff = diffTime-clip->Start()+clip->mDragAnchorTime;
                clip->Cropping(diff, 0);
            }
            else if (clipMovingPart & 2)
            {
                // clip right cropping
                auto diff = diffTime-clip->End()+clip->mDragAnchorTime;
                clip->Cropping(diff, 1);
            }
            changed = true;
        }

        // mark moving
        if (markMovingEntry != -1 && !MovingCurrentTime && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImGui::CaptureMouseFromApp();
            int64_t mouse_time = (int64_t)((io.MousePos.x - timeMeterRect.Min.x) / timeline->msPixelWidthTarget) + timeline->firstTime;
            if (markMovingEntry == 0)
            {
                timeline->mark_in = mouse_time;
                if (timeline->mark_in < timeline->GetStart()) timeline->mark_in = timeline->GetStart();
                if (timeline->mark_in >= timeline->mark_out) timeline->mark_out = -1;
                if (timeline->mark_in < timeline->firstTime)
                    timeline->firstTime = timeline->mark_in;
                if (timeline->mark_in > timeline->lastTime)
                    timeline->firstTime = timeline->mark_in - timeline->visibleTime;
                timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
            }
            else if (markMovingEntry == 1)
            {
                timeline->mark_out = mouse_time;
                if (timeline->mark_out > timeline->GetEnd()) timeline->mark_out = timeline->GetEnd();
                if (timeline->mark_out <= timeline->mark_in) timeline->mark_in = -1;
                if (timeline->mark_out < timeline->firstTime)
                    timeline->firstTime = timeline->mark_out;
                if (timeline->mark_out > timeline->lastTime)
                    timeline->firstTime = timeline->mark_out - timeline->visibleTime;
                timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
            }
            else
            {
                int64_t mark_duration = timeline->mark_out - timeline->mark_in;
                timeline->mark_in = mouseTime - markMovingShift;
                timeline->mark_out = timeline->mark_in + mark_duration;
                if (timeline->mark_in < timeline->GetStart())
                {
                    timeline->mark_in = timeline->GetStart();
                    timeline->mark_out = timeline->mark_in + mark_duration;
                }
                if (timeline->mark_out > timeline->mEnd)
                {
                    timeline->mark_out = timeline->GetEnd();
                    timeline->mark_in = timeline->mark_out - mark_duration;
                }
                if (timeline->mark_in < timeline->firstTime)
                    timeline->firstTime = timeline->mark_in;
                if (timeline->mark_out > timeline->lastTime)
                    timeline->firstTime = timeline->mark_out - timeline->visibleTime;
                timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
            }
            if (ImGui::BeginTooltip())
            {
                ImGui::Text(" In:%s", ImGuiHelper::MillisecToString(timeline->mark_in, 2).c_str());
                ImGui::Text("Out:%s", ImGuiHelper::MillisecToString(timeline->mark_out, 2).c_str());
                ImGui::Text("Dur:%s", ImGuiHelper::MillisecToString(timeline->mark_out - timeline->mark_in, 2).c_str());
                ImGui::EndTooltip();
            }
            changed = true;
        }

        if (trackAreaRect.Contains(io.MousePos) && editable && !menuIsOpened && !bCropping && !timeline->mIsCutting && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            timeline->Click(mouseEntry, mouseTime);

        if (trackAreaRect.Contains(io.MousePos) && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
        {
            if (!mouseClip.empty())
                clipMenuEntry = mouseClip[0];
            if (mouseEntry >= 0)
                trackMenuEntry = mouseEntry;
            if (mouseTime != -1)
                menuMouseTime = mouseTime;
            ImGui::OpenPopup("##timeline-context-menu");
            menuIsOpened = true;
        }
        if (HeaderAreaRect.Contains(io.MousePos) && !menuIsOpened && !bCropping && !timeline->mIsCutting && ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            headerMarkPos = io.MousePos.x;
            ImGui::OpenPopup("##timeline-header-context-menu");
            menuIsOpened = true;
        }

        draw_list->PopClipRect();
        draw_list->PopClipRect();

        // handle menu
        if (!menuIsOpened)
        {
            headerMarkPos = -1;
            trackMenuEntry = -1;
            clipMenuEntry = -1;
            menuMouseTime = -1;
        }

        if (ImGui::BeginPopup("##timeline-header-context-menu"))
        {
            if (headerMarkPos >= 0)
            {
                int64_t mouse_time = (int64_t)((headerMarkPos - timeMeterRect.Min.x) / timeline->msPixelWidthTarget) + timeline->firstTime;
                if (ImGui::MenuItem( ICON_MARK_IN " Add mark in"))
                {
                    if (timeline->mark_out != -1 && mouse_time > timeline->mark_out)
                        timeline->mark_out = -1;
                    timeline->mark_in = mouse_time;
                    headerMarkPos = -1;
                    changed = true;
                }
                if (ImGui::MenuItem(ICON_MARK_OUT " Add mark out"))
                {
                    if (timeline->mark_in != -1 && mouse_time < timeline->mark_in)
                        timeline->mark_in = -1;
                    timeline->mark_out = mouse_time;
                    headerMarkPos = -1;
                    changed = true;
                }
                if (ImGui::MenuItem(ICON_MARK_NONE " Delete mark point"))
                {
                    timeline->mark_in = timeline->mark_out = -1;
                    headerMarkPos = -1;
                    changed = true;
                }
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("##timeline-context-menu"))
        {
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0,1.0,1.0,1.0));
            auto selected_clip_count = timeline->GetSelectedClipCount();
            auto empty_track_count = timeline->GetEmptyTrackCount();
            
            if (ImGui::MenuItem(ICON_MEDIA_VIDEO  " Insert Empty Video Track"))
            {
                insertEmptyTrackType = MEDIA_VIDEO;
            }
            if (ImGui::MenuItem(ICON_MEDIA_AUDIO " Insert Empty Audio Track"))
            {
                insertEmptyTrackType = MEDIA_AUDIO;
            }
            if (ImGui::MenuItem(ICON_MEDIA_TEXT " Insert Empty Text Track"))
            {
                insertEmptyTrackType = MEDIA_TEXT;
            }

            if (empty_track_count > 0)
            {
                if (ImGui::MenuItem(ICON_MEDIA_DELETE " Delete Empty Track"))
                {
                    removeEmptyTrack = true;
                    changed = true;
                }
            }

            if (clipMenuEntry != -1)
            {
                ImGui::Separator();
                auto clip = timeline->FindClipByID(clipMenuEntry);
                auto track = timeline->FindTrackByClipID(clip->mID);
                bool disable_editing = IS_DUMMY(clip->mType);
                ImGui::BeginDisabled(disable_editing);
                //if (ImGui::MenuItem(ICON_CROP " Edit Clip Attribute", nullptr, nullptr))
                //{
                //    track->SelectEditingClip(clip, false);
                //}
                if (ImGui::MenuItem(ICON_FILTER_EDITOR " Edit Clip Filter"))
                {
                    track->SelectEditingClip(clip);
                }
                ImGui::EndDisabled();
#ifdef ENABLE_BACKGROUND_TASK
                if (ImGui::BeginMenu(ICON_NODE " Background Task"))
                {
                    const auto szSubItemCnt = s_aBgtaskMenuItems.size();
                    for (auto i = 0; i < szSubItemCnt; i++)
                    {
                        const auto& menuItem = s_aBgtaskMenuItems[i];
                        bool bDisableMenuItem = !menuItem.checkUsable(clip);
                        ImGui::BeginDisabled(bDisableMenuItem);
                        if (ImGui::MenuItem(menuItem.label.c_str()))
                        {
                            bOpenCreateBgtaskDialog = true;
                            s_szBgtaskSelIdx = i;
                            std::ostringstream oss; oss << "Create " << menuItem.label << " Task";
                            s_strBgtaskCreateDlgLabel = oss.str();
                            s_i64BgtaskSrcClipId = clipMenuEntry;
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::EndMenu();
                }
#endif
                if (ImGui::MenuItem(ICON_MEDIA_DELETE_CLIP " Delete Clip"))
                {
                    delClipEntry.push_back(clipMenuEntry);
                    changed = true;
                }
                if (clip->mGroupID != -1 && ImGui::MenuItem(ICON_MEDIA_UNGROUP " Ungroup Clip"))
                {
                    unGroupClipEntry.push_back(clipMenuEntry);
                    changed = true;
                }
            }

            if (selected_clip_count > 0)
            {
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MEDIA_DELETE_CLIP " Delete Selected"))
                {
                    for (auto clip : timeline->m_Clips)
                    {
                        if (clip->bSelected)
                        {
                            delClipEntry.push_back(clip->mID);
                            changed = true;
                        }
                    }
                }
                if (selected_clip_count > 1 && ImGui::MenuItem(ICON_MEDIA_GROUP " Group Selected"))
                {
                    for (auto clip : timeline->m_Clips)
                    {
                        if (clip->bSelected)
                        {
                            groupClipEntry.push_back(clip->mID);
                            changed = true;
                        }
                    }
                }
                if (ImGui::MenuItem(ICON_MEDIA_UNGROUP " Ungroup Selected"))
                {
                    for (auto clip : timeline->m_Clips)
                    {
                        if (clip->bSelected)
                        {
                            unGroupClipEntry.push_back(clip->mID);
                            changed = true;
                        }
                    }
                }
            }

            if (trackMenuEntry >= 0 && clipMenuEntry < 0)
            {
                auto track = timeline->m_Tracks[trackMenuEntry];
                if (IS_TEXT(track->mType))
                {
                    ImGui::Separator();
                    if (ImGui::MenuItem(ICON_MEDIA_TEXT  " Add Text"))
                    {
                        if (track->mMttReader && menuMouseTime != -1)
                        {
                            auto pTextClip = TextClip::CreateInstance(timeline, "", menuMouseTime);
                            pTextClip->CreateDataLayer(track);
                            pTextClip->SetClipDefault(track->mMttReader->DefaultStyle());
                            track->InsertClip(pTextClip, menuMouseTime);
                            track->SelectEditingClip(pTextClip);
                            if (timeline->m_CallBacks.EditingClip)
                            {
                                timeline->m_CallBacks.EditingClip(0, pTextClip);
                            }
                            changed = true;
                        }
                    }
                }
            }

            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }

        // add a gap to bottom
        ImGui::Dummy(ImVec2(0, trackHeadHeight * 2));

        ImGui::EndChildFrame(); // finished track view
        auto horizon_scroll_pos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(horizon_scroll_pos + ImVec2(16, 0));
        ImGui::SetWindowFontScale(0.7);
        auto info_str = ImGuiHelper::MillisecToString(duration, 3);
        info_str += " / ";
        info_str += std::to_string(trackCount) + " tracks";
        ImGui::Text("%s", info_str.c_str());
        ImGui::SetWindowFontScale(1.0);

        // Horizon Scroll bar
        ImGui::SetCursorScreenPos(horizon_scroll_pos);
        ImGui::InvisibleButton("HorizonScrollBar", HorizonScrollBarSize);
        ImVec2 HorizonScrollAreaMin = ImGui::GetItemRectMin();
        ImVec2 HorizonScrollAreaMax = ImGui::GetItemRectMax();
        float HorizonStartOffset = ((float)(timeline->firstTime - timeline->GetStart()) / (float)duration) * (timline_size.x - legendWidth);
        ImVec2 HorizonScrollBarMin(HorizonScrollAreaMin.x + legendWidth, HorizonScrollAreaMin.y - 2);        // whole bar area
        ImVec2 HorizonScrollBarMax(HorizonScrollAreaMin.x + timline_size.x, HorizonScrollAreaMax.y - 1);      // whole bar area
        HorizonScrollBarRect = ImRect(HorizonScrollBarMin, HorizonScrollBarMax);
        bool inHorizonScrollBar = HorizonScrollBarRect.Contains(io.MousePos);
        draw_list->AddRectFilled(HorizonScrollBarMin, HorizonScrollBarMax, COL_SLIDER_BG, 8);
        ImVec2 HorizonScrollHandleBarMin(HorizonScrollAreaMin.x + legendWidth + HorizonStartOffset, HorizonScrollAreaMin.y);  // current bar area
        ImVec2 HorizonScrollHandleBarMax(HorizonScrollAreaMin.x + legendWidth + HorizonBarWidthInPixels + HorizonStartOffset, HorizonScrollAreaMax.y - 2);
        ImRect HorizonScrollThumbLeft(HorizonScrollHandleBarMin + ImVec2(2, 2), HorizonScrollHandleBarMin + ImVec2(scrollSize - 4, scrollSize - 4));
        ImRect HorizonScrollThumbRight(HorizonScrollHandleBarMax - ImVec2(scrollSize - 4, scrollSize - 4), HorizonScrollHandleBarMax - ImVec2(2, 2));
        HorizonScrollHandleBarRect = ImRect(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax);
        bool inHorizonScrollHandle = HorizonScrollHandleBarRect.Contains(io.MousePos);
        bool inHorizonScrollThumbLeft = HorizonScrollThumbLeft.Contains(io.MousePos);
        bool inHorizonScrollThumbRight = HorizonScrollThumbRight.Contains(io.MousePos);
        draw_list->AddRectFilled(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax, (inHorizonScrollBar || MovingHorizonScrollBar == 0) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
        draw_list->AddRectFilled(HorizonScrollThumbLeft.Min, HorizonScrollThumbLeft.Max, (inHorizonScrollThumbLeft || MovingHorizonScrollBar == 1) ? COL_SLIDER_THUMB_IN : COL_SLIDER_HANDLE, 6);
        draw_list->AddRectFilled(HorizonScrollThumbRight.Min, HorizonScrollThumbRight.Max, (inHorizonScrollThumbRight || MovingHorizonScrollBar == 2) ? COL_SLIDER_THUMB_IN : COL_SLIDER_HANDLE, 6);
        if (MovingHorizonScrollBar == 1)
        {
            // Scroll Thumb Left
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = -1;
            }
            else
            {
                auto new_start_offset = ImMax(io.MousePos.x - HorizonScrollBarMin.x, 0.f);
                auto current_scroll_width = ImMax(HorizonScrollHandleBarMax.x - new_start_offset - HorizonScrollBarMin.x, (float)scrollSize);
                timeline->msPixelWidthTarget = (HorizonScrollBarRect.GetWidth() * HorizonScrollBarRect.GetWidth()) / (current_scroll_width * duration);
                timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                timeline->firstTime = new_start_offset / HorizonScrollBarRect.GetWidth() * (float)duration + timeline->GetStart();
                int64_t new_visible_time = (int64_t)floorf((timline_size.x - legendWidth) / timeline->msPixelWidthTarget);
                timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - new_visible_time, timeline->GetStart()));
                need_save = true;
            }
        }
        else if (MovingHorizonScrollBar == 2)
        {
            // Scroll Thumb Right
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = -1;
            }
            else
            {
                auto new_end_offset = ImClamp(io.MousePos.x - HorizonScrollBarMin.x, HorizonScrollHandleBarMin.x - HorizonScrollBarMin.x + scrollSize, HorizonScrollBarMax.x - HorizonScrollBarMin.x);
                auto current_scroll_width = ImMax(new_end_offset + HorizonScrollBarMin.x - HorizonScrollHandleBarMin.x, (float)scrollSize);
                timeline->msPixelWidthTarget = (HorizonScrollBarRect.GetWidth() * HorizonScrollBarRect.GetWidth()) / (current_scroll_width * duration);
                timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                need_save = true;
            }
        }
        else if (MovingHorizonScrollBar == 0)
        {
            // Scroll
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = -1;
            }
            else
            {
                float msPerPixelInBar = HorizonBarPos / (float)timeline->visibleTime;
                timeline->firstTime = int((io.MousePos.x - panningViewHorizonSource.x) / msPerPixelInBar) - panningViewHorizonTime;
                timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                need_save = true;
            }
        }
        else if (inHorizonScrollThumbLeft && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && clipMovingEntry == -1 && !menuIsOpened && editable)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = 1;
        }
        else if (inHorizonScrollThumbRight && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && clipMovingEntry == -1 && !menuIsOpened && editable)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = 2;
        }
        else if (inHorizonScrollHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && clipMovingEntry == -1 && !menuIsOpened && editable)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = 0;
            panningViewHorizonSource = io.MousePos;
            panningViewHorizonTime = - timeline->firstTime;
        }
        //else if (inHorizonScrollBar && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !menuIsOpened && editable)
        //{
        //    float msPerPixelInBar = HorizonBarPos / (float)timeline->visibleTime;
        //    timeline->firstTime = int((io.MousePos.x - legendWidth - contentMin.x) / msPerPixelInBar);
        //    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
        //    need_save = true;
        //}

        // Vertical Scroll bar
        auto vertical_scroll_pos = ImVec2(canvas_pos.x - scrollSize * 2, canvas_pos.y + HeadHeight);
        const float VerticalBarHeightRatio = ImMin(VerticalScrollBarSize.y / (VerticalScrollBarSize.y + VerticalScrollMax), 1.f);
        const float VerticalBarHeightInPixels = std::max(VerticalBarHeightRatio * VerticalScrollBarSize.y, (float)scrollSize / 2);
        ImGui::SetCursorScreenPos(vertical_scroll_pos);
        ImGui::InvisibleButton("VerticalScrollBar", VerticalScrollBarSize);
        float VerticalStartOffset = VerticalScrollPos * VerticalBarHeightRatio;
        ImVec2 VerticalScrollBarMin = ImGui::GetItemRectMin();
        ImVec2 VerticalScrollBarMax = ImGui::GetItemRectMax();
        auto VerticalScrollBarRect = ImRect(VerticalScrollBarMin, VerticalScrollBarMax);
        bool inVerticalScrollBar = VerticalScrollBarRect.Contains(io.MousePos);
        draw_list->AddRectFilled(VerticalScrollBarMin, VerticalScrollBarMax, COL_SLIDER_BG, 8);
        ImVec2 VerticalScrollHandleBarMin(VerticalScrollBarMin.x, VerticalScrollBarMin.y + VerticalStartOffset);  // current bar area
        ImVec2 VerticalScrollHandleBarMax(VerticalScrollBarMin.x + (float)scrollSize / 2, VerticalScrollBarMin.y + VerticalBarHeightInPixels + VerticalStartOffset);
        auto VerticalScrollHandleBarRect = ImRect(VerticalScrollHandleBarMin, VerticalScrollHandleBarMax);
        bool inVerticalScrollHandle = VerticalScrollHandleBarRect.Contains(io.MousePos);
        draw_list->AddRectFilled(VerticalScrollHandleBarMin, VerticalScrollHandleBarMax, (inVerticalScrollBar || MovingVerticalScrollBar) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
        if (MovingVerticalScrollBar)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingVerticalScrollBar = false;
            }
            else
            {
                float offset = (io.MousePos.y - panningViewVerticalSource.y) / VerticalBarHeightRatio + panningViewVerticalPos;
                offset = ImClamp(offset, 0.f, VerticalScrollMax);
                ImGui::SetScrollY(VerticalWindow, offset);
            }
        }
        else if (inVerticalScrollHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && clipMovingEntry == -1 && !menuIsOpened && editable)
        {
            ImGui::CaptureMouseFromApp();
            MovingVerticalScrollBar = true;
            panningViewVerticalSource = io.MousePos;
            panningViewVerticalPos = VerticalScrollPos;
        }
        else if (inVerticalScrollBar && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !menuIsOpened && editable)
        {
            float offset = (io.MousePos.y - vertical_scroll_pos.y) / VerticalBarHeightRatio;
            offset = ImClamp(offset, 0.f, VerticalScrollMax);
            ImGui::SetScrollY(VerticalWindow, offset);
        }
        if (bInsertNewTrack)
        {
            ImGui::SetScrollY(VerticalWindow, VerticalScrollMax + InsertHeight);
            bInsertNewTrack = false;
            InsertHeight = 0;
        }

        // handle mouse wheel event
        if (regionRect.Contains(io.MousePos) && !menuIsOpened && editable)
        {
            if (timeMeterRect.Contains(io.MousePos))
            {
                overTopBar = true;
            }
            if (trackRect.Contains(io.MousePos))
            {
                overCustomDraw = true;
            }
            if (trackAreaRect.Contains(io.MousePos))
            {
                overTrackView = true;
            } 
            if (HorizonScrollBarRect.Contains(io.MousePos))
            {
                overHorizonScrollBar = true;
            }
            if (legendRect.Contains(io.MousePos))
            {
                overLegend = true;
            }
            if (overLegend)
            {
                if (io.MouseWheel < -FLT_EPSILON || io.MouseWheel > FLT_EPSILON)
                {
                    auto scroll_y = VerticalScrollPos;
                    float offset = -io.MouseWheel * 5 / VerticalBarHeightRatio + scroll_y;
                    offset = ImClamp(offset, 0.f, VerticalScrollMax);
                    ImGui::SetScrollY(VerticalWindow, offset);
                    panningViewVerticalPos = offset;
                }
            }
            if (overCustomDraw || overTrackView || overHorizonScrollBar || overTopBar)
            {
                // up-down wheel to scroll vertical
                if (io.MouseWheel < -FLT_EPSILON || io.MouseWheel > FLT_EPSILON)
                {
                    auto scroll_y = VerticalScrollPos;
                    float offset = -io.MouseWheel * 5 / VerticalBarHeightRatio + scroll_y;
                    offset = ImClamp(offset, 0.f, VerticalScrollMax);
                    ImGui::SetScrollY(VerticalWindow, offset);
                    panningViewVerticalPos = offset;
                }
                // left-right wheel over blank area, moving canvas view
                else if (io.MouseWheelH < -FLT_EPSILON)
                {
                    timeline->firstTime -= timeline->visibleTime / view_frames;
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                    need_save = true;
                }
                else if (io.MouseWheelH > FLT_EPSILON)
                {
                    timeline->firstTime += timeline->visibleTime / view_frames;
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                    need_save = true;
                }
            }
            if (overHorizonScrollBar && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // up-down wheel over scrollbar, scale canvas view
                if (io.MouseWheel < -FLT_EPSILON && timeline->visibleTime <= timeline->GetEnd())
                {
                    timeline->msPixelWidthTarget *= 0.9f;
                    timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                    int64_t new_mouse_time = (int64_t)((io.MousePos.x - contentMin.x - legendWidth) / timeline->msPixelWidthTarget) + timeline->firstTime;
                    int64_t offset = new_mouse_time - mouseTime;
                    timeline->firstTime -= offset;
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                    need_save = true;
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    timeline->msPixelWidthTarget *= 1.1f;
                    timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                    int64_t new_mouse_time = (int64_t)((io.MousePos.x - contentMin.x - legendWidth) / timeline->msPixelWidthTarget) + timeline->firstTime;
                    int64_t offset = new_mouse_time - mouseTime;
                    timeline->firstTime -= offset;
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                    need_save = true;
                }
            }
        }

        // calculate custom draw rect
        customHeight = 0;
        for (int i = 0; i < trackCount; i++)
        {
            int64_t start = 0, end = 0, length = 0;
            size_t localCustomHeight = timeline->GetCustomHeight(i);

            ImVec2 rp(contentMin.x, contentMin.y + trackHeadHeight * i + 1 + customHeight);
            ImRect customRect(rp + ImVec2(legendWidth - (timeline->firstTime - start - 0.5f) * timeline->msPixelWidthTarget, float(trackHeadHeight)),
                              rp + ImVec2(legendWidth + (end - timeline->firstTime - 0.5f + 2.f) * timeline->msPixelWidthTarget, float(localCustomHeight + trackHeadHeight)));
            ImRect titleRect(rp + ImVec2(legendWidth - (timeline->firstTime - start - 0.5f) * timeline->msPixelWidthTarget, 0),
                              rp + ImVec2(legendWidth + (end - timeline->firstTime - 0.5f + 2.f) * timeline->msPixelWidthTarget, float(trackHeadHeight)));
            ImRect clippingTitleRect(rp + ImVec2(float(legendWidth), 0), rp + ImVec2(timline_size.x - 4.0f, float(trackHeadHeight)));
            ImRect clippingRect(rp + ImVec2(float(legendWidth), float(trackHeadHeight)), rp + ImVec2(timline_size.x - 4.0f, float(localCustomHeight + trackHeadHeight)));
            ImRect legendRect(rp, rp + ImVec2(float(legendWidth), float(localCustomHeight + trackHeadHeight)));
            ImRect legendClippingRect(rp + ImVec2(0.f, float(trackHeadHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight + trackHeadHeight)));
            customDraws.push_back({i, customRect, titleRect, clippingTitleRect, legendRect, clippingRect, legendClippingRect});

            customHeight += localCustomHeight;
        }

        // draw custom
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize);
        for (auto &customDraw : customDraws)
            timeline->CustomDraw(
                    customDraw.index, draw_list, ImRect(childFramePos, childFramePos + childFrameSize), customDraw.customRect,
                    customDraw.titleRect, customDraw.clippingTitleRect, customDraw.legendRect, customDraw.clippingRect, customDraw.legendClippingRect,
                    mouseTime, bClipMoving, !menuIsOpened && !timeline->mIsCutting && editable, changed | need_save, &actionList);
        draw_list->PopClipRect();

        // show cutting line
        if (timeline->mIsCutting && editable)
        {
            std::vector<Clip*> cuttingClips;
            // 1. find track on which the mouse is hovering
            int cutTrkIdx = -1;
            ImRect trkRect;
            for (auto& customDraw : customDraws)
            {
                if (io.MousePos.y >= customDraw.clippingTitleRect.Min.y && io.MousePos.y <= customDraw.clippingRect.Max.y)
                {
                    cutTrkIdx = customDraw.index;
                    trkRect = ImRect(customDraw.clippingTitleRect.Min, customDraw.clippingRect.Max);
                    break;
                }
            }
            // 2. find clip on which the mouse is hovering
            MediaTrack* cutTrk = nullptr;
            Clip* hoveringClip = nullptr;
            if (cutTrkIdx >= 0 && cutTrkIdx < timeline->m_Tracks.size())
            {
                cutTrk = timeline->m_Tracks[cutTrkIdx];
                for (auto clip : cutTrk->m_Clips)
                {
                    if (clip->IsInClipRange(alignedTime))
                    {
                        hoveringClip = clip;
                        break;
                    }
                }
            }
            // 3. check if the mouse is reside in an overlap
            bool inOverlap = false;
            if (cutTrk && hoveringClip)
            {
                for (auto ovlp : cutTrk->m_Overlaps)
                {
                    if (alignedTime >= ovlp->mStart && alignedTime <= ovlp->mEnd)
                    {
                        inOverlap = true;
                        break;
                    }
                }
            }
            // 4. show cutting line in the hovering clip and clips in the same group
            if (hoveringClip && !inOverlap)
            {
                cuttingClips.push_back(hoveringClip);
                // 4.1 draw cutting line in the hovering clip
                draw_list->AddLine({(float)alignedMousePosX, trkRect.Min.y}, {(float)alignedMousePosX, trkRect.Max.y}, IM_COL32_WHITE, 2);
                // 4.2 draw cutting line in the clips in the same group
                if (hoveringClip->mGroupID != -1)
                {
                    const auto targetGid = hoveringClip->mGroupID;
                    auto grpIter = std::find_if(timeline->m_Groups.begin(), timeline->m_Groups.end(), [targetGid] (auto& grp) {
                        return grp.mID == targetGid;
                    });
                    if (grpIter != timeline->m_Groups.end())
                    {
                        for (auto clipId : grpIter->m_Grouped_Clips)
                        {
                            if (clipId == hoveringClip->mID)
                                continue;
                            auto clip = timeline->FindClipByID(clipId);
                            if (!clip) continue;
                            if (clip->IsInClipRange(alignedTime))
                            {
                                int trkIdx = timeline->FindTrackIndexByClipID(clip->mID);
                                if (trkIdx >= 0)
                                {
                                    auto cdIter = std::find_if(customDraws.begin(), customDraws.end(), [trkIdx] (auto& cd) {
                                        return cd.index == trkIdx;
                                    });
                                    if (cdIter != customDraws.end())
                                    {
                                        cuttingClips.push_back(clip);
                                        trkRect = ImRect(cdIter->clippingTitleRect.Min, cdIter->clippingRect.Max);
                                        draw_list->AddLine({(float)alignedMousePosX, trkRect.Min.y}, {(float)alignedMousePosX, trkRect.Max.y}, IM_COL32_WHITE, 2);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // handle cut operation
            if (doCutPos > 0 && !cuttingClips.empty())
            {
                ClipGroup newGroup(timeline);
                timeline->m_Groups.push_back(newGroup);
                for (auto clip : cuttingClips)
                {
                    clip->Cutting(doCutPos, newGroup.mID, -1, &timeline->mUiActions);
                }
                changed = true;
            }
        }

        // record moving or cropping action
        if (clipClickedTriggered && clipMovingEntry != -1)
        {
            auto clip = timeline->FindClipByID(clipMovingEntry);
            auto track = timeline->FindTrackByClipID(clipMovingEntry);
            imgui_json::value ongoingAction;
            ongoingAction["action"] = bCropping ? "CROP_CLIP" : "MOVE_CLIP";
            ongoingAction["clip_id"] = imgui_json::number(clip->mID);
            ongoingAction["media_type"] = imgui_json::number(clip->mType);
            ongoingAction["from_track_id"] = imgui_json::number(track->mID);
            ongoingAction["org_start"] = imgui_json::number(clip->Start());
            ongoingAction["org_end"] = imgui_json::number(clip->End());
            ongoingAction["org_start_offset"] = imgui_json::number(clip->StartOffset());
            ongoingAction["org_end_offset"] = imgui_json::number(clip->EndOffset());
            timeline->mOngoingActions.push_back(std::move(ongoingAction));
            int selectCnt = 0;
            for (auto pclip : timeline->m_Clips)
            {
                if (pclip->bSelected)
                    selectCnt++;
            }
            if (!bCropping)
            {
                // record other selected clips for move action
                for (auto pClip : timeline->m_Clips)
                {
                    if (pClip->mID == clip->mID || !pClip->bSelected)
                        continue;
                    ongoingAction["action"] = "MOVE_CLIP";
                    ongoingAction["clip_id"] = imgui_json::number(pClip->mID);
                    ongoingAction["media_type"] = imgui_json::number(pClip->mType);
                    MediaTrack* track = timeline->FindTrackByClipID(pClip->mID);
                    ongoingAction["from_track_id"] = imgui_json::number(track->mID);
                    ongoingAction["org_start"] = imgui_json::number(pClip->Start());
                    timeline->mOngoingActions.push_back(std::move(ongoingAction));
                }
            }
        }

        // time metric
        bool movable = true;

        if (menuIsOpened || !editable ||
            ImGui::IsDragDropActive())
        {
            movable = false;
        }

        ImGui::SetCursorScreenPos(timeMeterRect.Min);
        ImGui::BeginChildFrame(ImGui::GetCurrentWindow()->GetID("#timeline metric"), timeMeterRect.GetSize(), ImGuiWindowFlags_NoScrollbar);

        // draw mark range for timeline header bar and draw shadow out of mark range 
        ImGui::PushClipRect(HeaderAreaRect.Min, HeaderAreaRect.Min + ImVec2(trackAreaRect.GetWidth() + 8, contentMin.y + timline_size.y - scrollSize), false);
        ImRect mark_rect = HeaderAreaRect;
        bool mark_in_view = false;
        if (timeline->mark_in >= timeline->firstTime && timeline->mark_in <= timeline->lastTime)
        {
            float mark_in_offset = (timeline->mark_in - timeline->firstTime) * timeline->msPixelWidthTarget;
            if (timeline->mark_out >= timeline->lastTime)
            {
                draw_list->AddRectFilled(HeaderAreaRect.Min + ImVec2(mark_in_offset, 0), HeaderAreaRect.Max - ImVec2(0, HeadHeight - 8), COL_MARK_BAR, 0);
            }
            else if (timeline->mark_out > timeline->firstTime)
            {
                float mark_out_offset = (timeline->mark_out - timeline->firstTime) * timeline->msPixelWidthTarget;
                draw_list->AddRectFilled(HeaderAreaRect.Min + ImVec2(mark_in_offset, 0), HeaderAreaRect.Min + ImVec2(mark_out_offset, 8), COL_MARK_BAR, 0);
            }
            
            ImRect handle_rect(HeaderAreaRect.Min + ImVec2(mark_in_offset, 0), HeaderAreaRect.Min + ImVec2(mark_in_offset + 8, 8));
            if (movable && handle_rect.Contains(io.MousePos))
            {
                draw_list->AddCircleFilled(HeaderAreaRect.Min + ImVec2(mark_in_offset + 2, 4), 4, COL_MARK_DOT_LIGHT);
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && markMovingEntry == -1)
                {
                    markMovingEntry = 0;
                    markMovingShift = 0;
                }
            }
            else
            {
                draw_list->AddCircleFilled(HeaderAreaRect.Min + ImVec2(mark_in_offset + 2, 4), 4, COL_MARK_DOT);
            }
            // add left area shadow
            draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Min + ImVec2(mark_in_offset, timline_size.y - scrollSize), IM_COL32(0,0,0,128));
            mark_rect.Min = HeaderAreaRect.Min + ImVec2(mark_in_offset, 0);
            mark_in_view = true;
        }
        if (timeline->mark_out >= timeline->firstTime && timeline->mark_out <= timeline->lastTime)
        {
            float mark_out_offset = (timeline->mark_out - timeline->firstTime) * timeline->msPixelWidthTarget;
            if (timeline->mark_in != -1 && timeline->mark_in < timeline->firstTime)
            {
                draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Min + ImVec2(mark_out_offset, 8), COL_MARK_BAR, 0);
            }
            else if (timeline->mark_in != -1 && timeline->mark_in < timeline->lastTime)
            {
                float mark_in_offset = (timeline->mark_in - timeline->firstTime) * timeline->msPixelWidthTarget;
                draw_list->AddRectFilled(HeaderAreaRect.Min + ImVec2(mark_in_offset, 0), HeaderAreaRect.Min + ImVec2(mark_out_offset, 8), COL_MARK_BAR, 0);
            }
            
            ImRect handle_rect(HeaderAreaRect.Min + ImVec2(mark_out_offset - 4, 0), HeaderAreaRect.Min + ImVec2(mark_out_offset + 4, 8));
            if (movable && handle_rect.Contains(io.MousePos))
            {
                draw_list->AddCircleFilled(HeaderAreaRect.Min + ImVec2(mark_out_offset + 2, 4), 4, COL_MARK_DOT_LIGHT);
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && markMovingEntry == -1)
                {
                    markMovingEntry = 1;
                    markMovingShift = 0;
                }
            }
            else
            {
                draw_list->AddCircleFilled(HeaderAreaRect.Min + ImVec2(mark_out_offset + 2, 4), 4, COL_MARK_DOT);
            }
            // add right area shadow
            draw_list->AddRectFilled(HeaderAreaRect.Min + ImVec2(mark_out_offset + 4, 0), HeaderAreaRect.Min + ImVec2(timline_size.x + 8, timline_size.y - scrollSize), IM_COL32(0,0,0,128));
            mark_rect.Max = HeaderAreaRect.Min + ImVec2(mark_out_offset + 4, 8);
            mark_in_view = true;
        }
        if (timeline->mark_in != -1 && timeline->mark_in < timeline->firstTime && timeline->mark_out >= timeline->lastTime)
        {
            draw_list->AddRectFilled(HeaderAreaRect.Min , HeaderAreaRect.Max - ImVec2(0, HeadHeight - 8), COL_MARK_BAR, 0);
            mark_in_view = true;
        }

        if ((timeline->mark_in != -1 && timeline->mark_in >= timeline->lastTime) || (timeline->mark_out != -1 && timeline->mark_out <= timeline->firstTime))
        {
            // add shadow on whole timeline
            draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Min + ImVec2(timline_size.x + 8, timline_size.y - scrollSize), IM_COL32(0,0,0,128));
        }
        if (timeline->mark_in == -1 || timeline->mark_out == -1)
            mark_in_view = false;
        
        if (movable && mark_in_view && mark_rect.Contains(io.MousePos))
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && markMovingEntry == -1)
            {
                markMovingEntry = 2;
                markMovingShift = mouseTime - timeline->mark_in;
            }
        }
        ImGui::PopClipRect();

        // check current time moving
        if (isFocused && movable && !MovingCurrentTime && markMovingEntry == -1 && MovingHorizonScrollBar == -1 && clipMovingEntry == -1 && timeline->mCurrentTime >= 0 && timeMeterRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = true;
        }
        if (MovingCurrentTime && duration)
        {
            if (!timeline->mIsPreviewPlaying || !timeline->bSeeking || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                auto mouseTime = (int64_t)((io.MousePos.x - timeMeterRect.Min.x) / timeline->msPixelWidthTarget) + timeline->firstTime;
                if (mouseTime < timeline->GetStart())
                    mouseTime = timeline->GetStart();
                if (mouseTime >= timeline->GetEnd())
                    mouseTime = timeline->GetEnd();
                if (mouseTime < timeline->firstTime)
                    timeline->firstTime = mouseTime;
                if (mouseTime > timeline->lastTime)
                    timeline->firstTime = mouseTime - timeline->visibleTime;
                timeline->mCurrentTime = timeline->AlignTime(mouseTime, 1);
                timeline->Seek(timeline->mCurrentTime, true);
                need_save = true;
            }
        }
        if (timeline->bSeeking && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = false;
            timeline->StopSeek();
        }
        ImGui::EndChildFrame();

        // cursor line
        ImRect custom_view_rect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
        if (trackCount > 0 && timeline->mCurrentTime >= timeline->firstTime && timeline->mCurrentTime <= timeline->GetEnd())
        {
            static const float cursorWidth = 2.f;
            float cursorOffset = contentMin.x + legendWidth + (timeline->mCurrentTime - timeline->firstTime) * timeline->msPixelWidthTarget + 1;
            draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMin.y + trackRect.Max.y - scrollSize), COL_CURSOR_LINE, cursorWidth);
        }
        draw_list->PopClipRect();

        // alignment line
        draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
        if (timeline->mConnectedPoints >= timeline->firstTime && timeline->mConnectedPoints <= timeline->firstTime + timeline->visibleTime)
        {
            static const float cursorWidth = 2.f;
            float lineOffset = contentMin.x + legendWidth + (timeline->mConnectedPoints - timeline->firstTime) * timeline->msPixelWidthTarget + 1;
            draw_list->AddLine(ImVec2(lineOffset, contentMin.y), ImVec2(lineOffset, contentMax.y), IM_COL32(255, 255, 255, 255), cursorWidth);
        }
        draw_list->PopClipRect();

        // drag drop line and time indicate
        if (ImGui::IsDragDropActive() && custom_view_rect.Contains(io.MousePos))
        {
            auto _payload = ImGui::GetDragDropPayload();
            if (_payload && (_payload->IsDataType("Media_drag_drop") || _payload->IsDataType("ImGuiFileDialog")))
            {
                draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
                static const float cursorWidth = 2.f;
                float lineOffset = contentMin.x + legendWidth + (mouseTime - timeline->firstTime) * timeline->msPixelWidthTarget + 1;
                draw_list->AddLine(ImVec2(lineOffset, contentMin.y), ImVec2(lineOffset, contentMax.y+ trackRect.Max.y - scrollSize), IM_COL32(255, 255, 0, 255), cursorWidth);
                draw_list->PopClipRect();
                ImGui::SetWindowFontScale(0.8);
                auto time_str = ImGuiHelper::MillisecToString(mouseTime, 2);
                ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
                float strOffset = contentMin.x + legendWidth + (mouseTime - timeline->firstTime) * timeline->msPixelWidthTarget - str_size.x * 0.5f + 1;
                ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y);
                draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG2, 2.0, ImDrawFlags_RoundCornersAll);
                draw_list->AddText(str_pos, COL_CURSOR_TEXT2, time_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
        }

        ImGui::PopStyleColor();
    }
    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        auto& ongoingActions = timeline->mOngoingActions;
        if (!ongoingActions.empty())
        {
            for (auto& action : ongoingActions)
            {
                int64_t clipId = action["clip_id"].get<imgui_json::number>();
                Clip* clip = timeline->FindClipByID(clipId);
                std::string actionName = action["action"].get<imgui_json::string>();
                if (actionName == "MOVE_CLIP")
                {
                    int64_t orgStart = action["org_start"].get<imgui_json::number>();
                    int64_t fromTrackId = action["from_track_id"].get<imgui_json::number>();
                    auto toTrack = timeline->FindTrackByClipID(clipId);
                    if (fromTrackId != toTrack->mID || orgStart != clip->Start())
                    {
                        action["to_track_id"] = imgui_json::number(toTrack->mID);
                        action["new_start"] = imgui_json::number(clip->Start());
                        actionList.push_back(std::move(action));
                    }
                    else
                    {
                        Logger::Log(Logger::VERBOSE) << "-- Unchanged MOVE action DISCARDED --" << std::endl;
                    }
                }
                else if (actionName == "CROP_CLIP")
                {
                    int64_t orgStartOffset = action["org_start_offset"].get<imgui_json::number>();
                    int64_t orgEndOffset = action["org_end_offset"].get<imgui_json::number>();
                    int64_t orgStart = action["org_start"].get<imgui_json::number>();
                    int64_t orgEnd = action["org_end"].get<imgui_json::number>();
                    if (orgStartOffset != clip->StartOffset() || orgEndOffset != clip->EndOffset() ||
                        orgStart != clip->Start() || orgEnd != clip->End())
                    {
                        action["new_start_offset"] = imgui_json::number(clip->StartOffset());
                        action["new_end_offset"] = imgui_json::number(clip->EndOffset());
                        action["new_start"] = imgui_json::number(clip->Start());
                        action["new_end"] = imgui_json::number(clip->End());
                        actionList.push_back(std::move(action));
                    }
                    else
                        Logger::Log(Logger::VERBOSE) << "-- Unchanged CROP action DISCARDED --" << std::endl;
                }
                else if (actionName == "MOVE_EVENT")
                {
                    int64_t clipId = action["clip_id"].get<imgui_json::number>();
                    int64_t evtId = action["event_id"].get<imgui_json::number>();
                    int64_t oldStart = action["event_start_old"].get<imgui_json::number>();
                    int32_t oldZ = action["event_z_old"].get<imgui_json::number>();
                    auto pClip = timeline->FindClipByID(clipId);
                    auto hEvent = pClip->mEventStack->GetEvent(evtId);
                    int64_t newStart = hEvent->Start();
                    int32_t newZ = hEvent->Z();
                    if (newStart != oldStart || newZ != oldZ)
                    {
                        action["event_start_new"] = imgui_json::number(newStart);
                        action["event_z_new"] = imgui_json::number(newZ);
                        actionList.push_back(std::move(action));
                    }
                    else
                        Logger::Log(Logger::VERBOSE) << "-- Unchanged MOVE_EVENT action DISCARDED --" << std::endl;
                }
                else if (actionName == "CROP_EVENT")
                {
                    int64_t clipId = action["clip_id"].get<imgui_json::number>();
                    int64_t evtId = action["event_id"].get<imgui_json::number>();
                    int64_t oldStart = action["event_start_old"].get<imgui_json::number>();
                    int32_t oldEnd = action["event_end_old"].get<imgui_json::number>();
                    auto pClip = timeline->FindClipByID(clipId);
                    auto hEvent = pClip->mEventStack->GetEvent(evtId);
                    int64_t newStart = hEvent->Start();
                    int32_t newEnd = hEvent->End();
                    if (newStart != oldStart || newEnd != oldEnd)
                    {
                        action["event_start_new"] = imgui_json::number(newStart);
                        action["event_end_new"] = imgui_json::number(newEnd);
                        actionList.push_back(std::move(action));
                    }
                    else
                        Logger::Log(Logger::VERBOSE) << "-- Unchanged MOVE_EVENT action DISCARDED --" << std::endl;
                }
                else
                {
                    Logger::Log(Logger::WARN) << "Unhandled 'ongoingAction' action '" << actionName << "'!" << std::endl;
                }
            }
            timeline->mOngoingActions.clear();
        }

        clipMovingEntry = -1;
        clipMovingPart = -1;
        markMovingEntry = -1;
        markMovingShift = 0;
        //trackEntry = -1;
        //trackMovingEntry = -1;
        bCropping = false;
        bClipMoving = false;
        bTrackMoving = false;
        diffTime = 0;
        timeline->mConnectedPoints = -1;
        ImGui::CaptureMouseFromApp(false);
    }

    // Show help tips
    if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        if (mouseTime != -1 && !mouseClip.empty() && mouseEntry >= 0 && mouseEntry < timeline->m_Tracks.size())
        {
            if (!mouseClip.empty() && !bClipMoving && ImGui::BeginTooltip())
            {
                ImGui::TextUnformatted("Help:");
                ImGui::TextUnformatted("    Left button click to select clip");
                ImGui::TextUnformatted("    Left button double click to editing clip");
                ImGui::TextUnformatted("    Left button double click title bar zip/unzip clip");
                ImGui::TextUnformatted("    Hold left button and drag left/right to move clip position");
                ImGui::TextUnformatted("    Hold left button and drag up/down to move clip cross track");
                ImGui::TextUnformatted("    Hold left Shift key to appand select");
                ImGui::TextUnformatted("    Hold left Alt/Option key to cut clip");
                ImGui::EndTooltip();
            }
            else if (bClipMoving && ImGui::BeginTooltip())
            {
                ImGui::TextUnformatted("Help:");
                ImGui::TextUnformatted("    Hold left Command/Win key to single select");
                ImGui::EndTooltip();
            }
        }
        if ((overTrackView || overHorizonScrollBar) && !bClipMoving && ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted("Help:");
            ImGui::TextUnformatted("    Mouse wheel up/down zooming timeline");
            ImGui::TextUnformatted("    Mouse wheel left/right moving timeline");
            ImGui::EndTooltip();
        }
        if (overLegend && ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted("Help:");
            ImGui::TextUnformatted("    Drag track up/down to re-order");
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar();
    }
    // Show help tips end
    ImGui::EndGroup();

    // handle drag drop
    auto insert_item_into_timeline = [&](MediaItem * item, MediaTrack * track)
    {
        if (IS_VIDEO(item->mMediaType))
        {
            auto pNewVideoClip = VideoClip::CreateInstance(timeline, item, 0);
            auto pVidInsTrack = track;
            if (!pVidInsTrack || !pVidInsTrack->CanInsertClip(pNewVideoClip, mouseTime))
            {
                const auto iNewTrackIndex = timeline->NewTrack("", MEDIA_VIDEO, true, -1, -1, &actionList);
                pVidInsTrack = timeline->m_Tracks[iNewTrackIndex];
                bInsertNewTrack = true;
                InsertHeight += pVidInsTrack->mTrackHeight + trackHeadHeight;
            }
            pVidInsTrack->InsertClip(pNewVideoClip, mouseTime, true, &actionList);

            if (item->mhParser->HasAudio())
            {
                // create the 'AudioClip' based on the audio stream in the same 'MediaItem'
                auto pNewAudioClip = AudioClip::CreateInstance(timeline, item, 0);
                // first try to insert the bundled AudioClip into the 'linked' audio track for the video track
                auto pAudInsTrack = timeline->FindTrackByID(pVidInsTrack->mLinkedTrack);
                if (!pAudInsTrack || !pAudInsTrack->CanInsertClip(pNewAudioClip, mouseTime))
                {
                    // then try to insert the AudioClip into an empty audio track
                    pAudInsTrack = timeline->FindEmptyTrackByType(MEDIA_AUDIO);
                    if (!pAudInsTrack || !pAudInsTrack->CanInsertClip(pNewAudioClip, mouseTime))
                    {
                        const auto iNewTrackIndex = timeline->NewTrack("", MEDIA_AUDIO, true, -1, -1, &actionList);
                        pAudInsTrack = timeline->m_Tracks[iNewTrackIndex];
                        bInsertNewTrack = true;
                        InsertHeight += pAudInsTrack->mTrackHeight + trackHeadHeight;
                    }
                }
                pAudInsTrack->InsertClip(pNewAudioClip, mouseTime, true, &actionList);

                // group new added Video&Audio Clip in the same group
                if (pNewVideoClip->mGroupID == -1)
                    timeline->NewGroup(pNewVideoClip, -1L, 0U, &actionList);
                timeline->AddClipIntoGroup(pNewAudioClip, pNewVideoClip->mGroupID, &actionList);

                // link two insert tracks
                // wyvern: The logic is bad, video track may already has a linked audio track, and the following code breaks the old link!
                // TODO: need to figure out a better way of keeping 'linked' relationship between clips generated from the same source
                if (pVidInsTrack->mLinkedTrack != pAudInsTrack->mID || pAudInsTrack->mLinkedTrack != pVidInsTrack->mID)
                {
                    pVidInsTrack->mLinkedTrack = pAudInsTrack->mID;
                    pAudInsTrack->mLinkedTrack = pVidInsTrack->mID;
                    imgui_json::value jnUiAction;
                    jnUiAction["action"] = "LINK_TRACK";
                    jnUiAction["track_id1"] = imgui_json::number(pVidInsTrack->mID);
                    jnUiAction["track_id2"] = imgui_json::number(pAudInsTrack->mID);
                    actionList.push_back(std::move(jnUiAction));
                }
            }
        }
        else if (IS_AUDIO(item->mMediaType))
        {
            auto pNewAudioClip = AudioClip::CreateInstance(timeline, item, 0);
            auto pAudInsTrack = track;
            if (!pAudInsTrack || !pAudInsTrack->CanInsertClip(pNewAudioClip, mouseTime))
            {
                const auto iNewTrackIndex = timeline->NewTrack("", MEDIA_VIDEO, true, -1, -1, &actionList);
                pAudInsTrack = timeline->m_Tracks[iNewTrackIndex];
                bInsertNewTrack = true;
                InsertHeight += pAudInsTrack->mTrackHeight + trackHeadHeight;
            }
            pAudInsTrack->InsertClip(pNewAudioClip, mouseTime, true, &actionList);
        } 
        else if (IS_SUBTITLE(item->mMediaType))
        {
            // subtitle track isn't like other media tracks, it need load clips after insert a empty track
            // text clip don't band with media item
            int newTrackIndex = timeline->NewTrack("", MEDIA_TEXT, true, -1, -1, &actionList);
            MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
            newTrack->mMttReader = timeline->mMtvReader->BuildSubtitleTrackFromFile(newTrack->mID, item->mPath);//MediaCore::SubtitleTrack::BuildFromFile(newTrack->mID, item->mPath);
            if (newTrack->mMttReader)
            {
                auto& style = newTrack->mMttReader->DefaultStyle();
                newTrack->mMttReader->SetFrameSize(timeline->GetPreviewWidth(), timeline->GetPreviewHeight());
                newTrack->mMttReader->SeekToIndex(0);
                newTrack->mMttReader->EnableFullSizeOutput(false);
                MediaCore::SubtitleClipHolder hSubClip = newTrack->mMttReader->GetCurrClip();
                while (hSubClip)
                {
                    TextClip* pNewTextClip = TextClip::CreateInstance(timeline, hSubClip->Text(), hSubClip->StartTime(), hSubClip->Duration());
                    pNewTextClip->SetClipDefault(style);
                    pNewTextClip->mhDataLayerClip = hSubClip;
                    pNewTextClip->mTrack = newTrack;
                    timeline->m_Clips.push_back(pNewTextClip);
                    newTrack->InsertClip(pNewTextClip, hSubClip->StartTime(), false);
                    hSubClip = newTrack->mMttReader->GetNextClip();
                }
                if (newTrack->mMttReader->Duration() > timeline->mEnd)
                {
                    timeline->mEnd = newTrack->mMttReader->Duration() + 1000;
                }
            }
            bInsertNewTrack = true;
            InsertHeight += newTrack->mTrackHeight + trackHeadHeight;
        }
        else
        {
            throw std::runtime_error("Unsupported media type!");
        }
        timeline->Update();
        changed = true;
    };
    ImGui::SetCursorScreenPos(canvas_pos + ImVec2(4, trackHeadHeight + 4));
    ImGui::InvisibleButton("canvas", timline_size - ImVec2(8, trackHeadHeight + scrollSize + 8));
    if (ImGui::BeginDragDropTarget())
    {
        // find current mouse pos track
        MediaTrack * track = nullptr;
        if (mouseEntry >= 0 && mouseEntry < timeline->m_Tracks.size())
        {
            track = timeline->m_Tracks[mouseEntry];
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Media_drag_drop"))
        {
            MediaItem * item = (MediaItem*)payload->Data;
            if (item)
            {
                insert_item_into_timeline(item, track);
            }
        }
        else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ImGuiFileDialog"))
        {
            IGFD::DropInfos* dinfo = (IGFD::DropInfos*)payload->Data;
            auto item = timeline->isURLInMediaBank(std::string(dinfo->filePath));
            if (item)
            {
                insert_item_into_timeline(item, track);
            }
            else
            {
                auto name = ImGuiHelper::path_filename(dinfo->filePath);
                auto path = std::string(dinfo->filePath);
                auto file_suffix = ImGuiHelper::path_filename_suffix(path);
                auto type = EstimateMediaType(file_suffix);
                MediaItem * item = new MediaItem(name, path, type, timeline);
                if (item)
                {
                    item->Initialize();
                    timeline->media_items.push_back(item);
                    insert_item_into_timeline(item, track);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // handle delete event
    for (auto clipId : delClipEntry)
    {
        if (timeline->DeleteClip(clipId, &timeline->mUiActions))
        {
            timeline->Update();
            changed = true;
        }
    }
    if (delTrackEntry != -1)
    {
        MediaTrack* track = timeline->m_Tracks[delTrackEntry];
        if (track && !track->mLocked)
        {
            uint32_t trackMediaType = track->mType;
            int64_t afterId = 0, afterUiTrkId = 0;
            imgui_json::value action;
            int64_t delTrackId = timeline->DeleteTrack(delTrackEntry, &timeline->mUiActions);
            if (delTrackId != -1)
                changed = true;
        }
        if (changed)
            timeline->Update();
    }
    if (removeEmptyTrack)
    {
        int index = 0, trackNum = timeline->m_Tracks.size();
        while (index < trackNum)
        {
            auto pTrack = timeline->m_Tracks[index];
            if (!pTrack || pTrack->mLocked)
            {
                index++;
                continue;
            }
            if (pTrack->m_Clips.size() <= 0)
            {
                uint32_t trackMediaType = pTrack->mType;
                int64_t afterId = 0, afterUiTrkId = 0;
                int64_t delTrackId = timeline->DeleteTrack(index, &timeline->mUiActions);
                if (delTrackId != -1)
                {
                    changed = true;
                    trackNum = timeline->m_Tracks.size();
                }
            }
            else
            {
                index++;
            }
        }
    }

    // handle insert event
    if (IS_VIDEO(insertEmptyTrackType))
    {
        int newTrackIndex = timeline->NewTrack("", MEDIA_VIDEO, true, -1, -1, &timeline->mUiActions);
        MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
        changed = true;
        bInsertNewTrack = true;
        InsertHeight += newTrack->mTrackHeight + trackHeadHeight;
    }
    else if (IS_AUDIO(insertEmptyTrackType))
    {
        int newTrackIndex = timeline->NewTrack("", MEDIA_AUDIO, true, -1, -1, &timeline->mUiActions);
        MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
        changed = true;
        bInsertNewTrack = true;
        InsertHeight += newTrack->mTrackHeight + trackHeadHeight;
    }
    else if (IS_TEXT(insertEmptyTrackType))
    {
        int newTrackIndex = timeline->NewTrack("", MEDIA_TEXT, true);
        MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
        newTrack->mMttReader = timeline->mMtvReader->NewEmptySubtitleTrack(newTrack->mID);
        newTrack->mMttReader->SetFont(timeline->mFontName);
        newTrack->mMttReader->SetFrameSize(timeline->GetPreviewWidth(), timeline->GetPreviewHeight());
        newTrack->mMttReader->EnableFullSizeOutput(false);
        changed = true;
        bInsertNewTrack = true;
        InsertHeight += newTrack->mTrackHeight + trackHeadHeight;
    }
    
    // handle group event
    if (groupClipEntry.size() > 0)
    {
        auto new_gourp_id = timeline->NewGroup(nullptr, -1L, 0U, &timeline->mUiActions);
        for (auto clip_id : groupClipEntry)
        {
            auto clip = timeline->FindClipByID(clip_id);
            timeline->AddClipIntoGroup(clip, new_gourp_id, &timeline->mUiActions);
        }
        changed = true;
    }

    for (auto clip_id : unGroupClipEntry)
    {
        auto clip = timeline->FindClipByID(clip_id);
        timeline->DeleteClipFromGroup(clip, clip->mGroupID, &timeline->mUiActions);
        changed = true;
    }

    // handle track moving
    if (trackMovingEntry != -1)
    {
        if (trackEntry != -1 && trackMovingEntry != trackEntry)
        {
            timeline->MovingTrack(trackMovingEntry, trackEntry, &timeline->mUiActions);
            changed = true;
            trackMovingEntry = -1;
            trackEntry = -1;
        }
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            trackMovingEntry = -1;
        }
    }

    // handle popup dialog
    if (bOpenCreateBgtaskDialog)
        ImGui::OpenPopup(s_strBgtaskCreateDlgLabel.c_str(), ImGuiPopupFlags_AnyPopup);
    if (ImGui::BeginPopupModal(s_strBgtaskCreateDlgLabel.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        const auto& bgtaskSubMenuItem = s_aBgtaskMenuItems[s_szBgtaskSelIdx];
        auto pClip = timeline->FindClipByID(s_i64BgtaskSrcClipId);
        bool bCloseDlg;
        auto hTask = bgtaskSubMenuItem.drawCreateTaskDialog(pClip, bCloseDlg);
        if (hTask)
            timeline->mhProject->EnqueueBackgroundTask(hTask);
        if (bCloseDlg)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // for debug
    //if (ImGui::BeginTooltip())
    //{
    //    ImGui::Text("%s", std::to_string(trackEntry).c_str());
    //    ImGui::Text("%s", ImGuiHelper::MillisecToString(mouseTime).c_str());
    //    ImGui::EndTooltip();
    //    ImGui::ShowMetricsWindow();
    //}
    // for debug end

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
#ifdef __APPLE__
        if (io.KeyMods == ImGuiModFlags_Super)
#else
        if (io.KeyMods == ImGuiModFlags_Ctrl)
#endif
        {
            if (!actionList.empty())
            {
                Logger::Log(Logger::WARN) << "TimeLine::mUiActions is NOT EMPTY when UNDO is triggered!" << std::endl;
                timeline->PrintActionList("UiActions", actionList);
            }
            timeline->UndoOneRecord();
        }
#ifdef __APPLE__
        else if (io.KeyMods == (ImGuiModFlags_Super|ImGuiModFlags_Shift))
#else
        else if (io.KeyMods == (ImGuiModFlags_Ctrl|ImGuiModFlags_Shift))
#endif
        {
            if (!actionList.empty())
            {
                Logger::Log(Logger::WARN) << "TimeLine::mUiActions is NOT EMPTY when REDO is triggered!" << std::endl;
                timeline->PrintActionList("UiActions", actionList);
            }
            timeline->RedoOneRecord();
        }
        timeline->Update();
        timeline->PerformUiActions();
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        timeline->mUiActions.splice(timeline->mUiActions.end(), actionList);
    
    if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        diffTime = 0;

    if (!timeline->mUiActions.empty())
    {
        // preprocess
        {
            // 1. remove BP_OPERATION actions if ADD_EVENT is present
            bool acAddEventPresent = false;
            for (auto& action : timeline->mUiActions)
            {
                if (action["action"].get<imgui_json::string>() == "ADD_EVENT")
                {
                    acAddEventPresent = true;
                    break;
                }
            }
            if (acAddEventPresent)
            {
                auto iter = timeline->mUiActions.begin();
                while (iter != timeline->mUiActions.end())
                {
                    auto& action = *iter;
                    if (action["action"].get<imgui_json::string>() == "BP_OPERATION")
                        iter = timeline->mUiActions.erase(iter);
                    else
                        iter++;
                }
            }
        }

        // add to history record list
        imgui_json::value historyRecord;
        historyRecord["time"] = ImGui::get_current_time();
        auto& actions = timeline->mUiActions;
        historyRecord["actions"] = imgui_json::array(actions.begin(), actions.end());
        timeline->AddNewRecord(historyRecord);

        // perform actions
        timeline->PerformUiActions();
        changed = true;
    }
    return changed;
}

bool DrawClipTimeLine(TimeLine* main_timeline, BaseEditingClip * editingClip, int64_t CurrentTime, int header_height, int custom_height, bool& show_BP, bool& changed)
{
    /***************************************************************************************
    |------------------------------------------------------------------------------------- 
    |_____________________________________________________________________________________ 
    |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    c
    |_______________|_____________________________________________________________________ a
    |               |        custom area                                                   n 
    |_______________|______________________________________________________________________v
    |               |   [event]                                                            a
    |               |            [event]                                                                                                   
    |_______________|_____________________________________________________________________                                                                                                            +
    [                           <==slider==>                                               ]
    ****************************************************************************************/
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    static const char* buttons[] = { "Delete", "Cancel", NULL };
    static ImGui::MsgBox msgbox;
    msgbox.Init("Delete Event?", ICON_MD_WARNING, "Are you really sure you want to delete event?", buttons, false);
    static int64_t lastFirstTime = -1;
    static int64_t lastVisiableTime = -1;
    if (!editingClip)
    {
        ImGui::SetWindowFontScale(2);
        auto pos_center = window_pos + window_size / 2;
        std::string tips_string = "Please Select Clip by Double Click From Main Timeline";
        auto string_width = ImGui::CalcTextSize(tips_string.c_str());
        auto tips_pos = pos_center - string_width / 2;
        ImGui::SetWindowFontScale(1);
        draw_list->AddTextComplex(tips_pos, tips_string.c_str(), 2.f, IM_COL32(255, 255, 255, 128), 0.5f, IM_COL32(56, 56, 56, 192));
        return false;
    }
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    const int scrollSize = 12;
    const int trackHeight = 24;
    const int curveHeight = 64;
    const int toolbarHeight = 24;
    int64_t start = editingClip->mStart;
    int64_t end = editingClip->mEnd;
    bool is_audio_clip = IS_AUDIO(editingClip->mType);
    bool is_video_clip = IS_VIDEO(editingClip->mType);
    //int64_t currentTime = CurrentTime - start;
    if (editingClip->mCurrentTime == -1) 
    {
        editingClip->mCurrentTime = CurrentTime - start;
        if (editingClip->mCurrentTime < 0) editingClip->mCurrentTime = 0;
        if (editingClip->mCurrentTime > editingClip->mDuration) editingClip->mCurrentTime = editingClip->mDuration;
    }
    else
    {
        auto clip_time = CurrentTime - start;
        if (clip_time >= 0 && clip_time <= editingClip->mDuration)
        {
            editingClip->mCurrentTime = clip_time;
        }
        else
            editingClip->mCurrentTime = -1;
    }
    int64_t duration = ImMax(end - start, (int64_t)1);
    static int MovingHorizonScrollBar = -1;
    static bool MovingVerticalScrollBar = false;
    static bool MovingCurrentTime = false;
    static bool menuIsOpened = false;
    int64_t mouseTime = -1;
    static int64_t menuMouseTime = -1;
    int mouseEntry = -1;
    MEC::Event::Holder mouseEvent = nullptr;
    static ImVec2 menuMousePos = ImVec2(-1, -1);
    static bool mouse_hold = false;
    bool overHorizonScrollBar = false;
    bool overCustomDraw = false;
    bool overTopBar = false;
    bool overTrackView = false;
    bool popupDialog = false;
    bool need_update = false;

    static int64_t eventMovingEntry = -1;
    static int eventMovingPart = -1;
    static float diffTime = 0;
    static bool bCropping = false;
    static bool bEventMoving = false;
    static bool bNewDragOp = false;
    auto clip = editingClip->GetClip();

    bool isAttribute = editingClip->bEditingAttribute;
    bool has_selected_event = clip->hasSelectedEvent();
    ImVec2 toolbar_pos = window_pos;
    ImVec2 toolbar_size = ImVec2(window_size.x, toolbarHeight);
    
    ImVec2 canvas_pos = window_pos + ImVec2(0, toolbarHeight + 1); //ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    canvas_size.y -= toolbarHeight + 1;
    ImVec2 timline_size = canvas_size - ImVec2(scrollSize / 2, 0);
    ImVec2 event_track_size = ImVec2(timline_size.x, trackHeight);
    float minPixelWidthTarget = (float)(timline_size.x) / (float)duration;
    float maxPixelWidthTarget = 20.f;
    int view_frames = 16;
    MediaCore::VideoTransformFilter::Holder hTransformFilter = nullptr;
    if (is_video_clip)
    {
        EditingVideoClip * video_clip = (EditingVideoClip *)editingClip;
        const auto frameRate = main_timeline->mhMediaSettings->VideoOutFrameRate();
        maxPixelWidthTarget = (video_clip->mSnapSize.x > 0 ? video_clip->mSnapSize.x : 60.f) * frameRate.num / (frameRate.den * 1000);
        view_frames = video_clip->mSnapSize.x > 0 ? (window_size.x / video_clip->mSnapSize.x) : 16;
        hTransformFilter = video_clip->mhTransformFilter;
    }
    else if (is_audio_clip)
    {
        maxPixelWidthTarget = 1;
        view_frames = window_size.x / 40.f;
    }
    if (editingClip->msPixelWidthTarget < 0) editingClip->msPixelWidthTarget = minPixelWidthTarget;

    int64_t newVisibleTime = (int64_t)floorf((timline_size.x) / editingClip->msPixelWidthTarget);
    editingClip->CalcDisplayParams(newVisibleTime);
    const float HorizonBarWidthRatio = ImMin(editingClip->visibleTime / (float)duration, 1.f);
    const float HorizonBarWidthInPixels = std::max(HorizonBarWidthRatio * (timline_size.x), (float)scrollSize);
    const float HorizonBarPos = HorizonBarWidthRatio * (timline_size.x);
    ImRect regionRect(canvas_pos, canvas_pos + timline_size);
    ImRect HorizonScrollBarRect;
    ImRect HorizonScrollHandleBarRect;
    static ImVec2 panningViewHorizonSource;
    static int64_t panningViewHorizonTime;

    ImVec2 HorizonScrollBarSize(window_size.x, scrollSize);
    ImVec2 HorizonScrollPos = window_pos + ImVec2(0, window_size.y - scrollSize);
    ImGui::SetCursorScreenPos(HorizonScrollPos);
    ImGui::InvisibleButton("clip_HorizonScrollBar", HorizonScrollBarSize);
    ImVec2 HorizonScrollAreaMin = HorizonScrollPos;
    ImVec2 HorizonScrollAreaMax = HorizonScrollAreaMin + HorizonScrollBarSize;
    ImVec2 HorizonScrollBarMin(HorizonScrollAreaMin.x, HorizonScrollAreaMin.y - 2);                     // whole bar area
    ImVec2 HorizonScrollBarMax(HorizonScrollAreaMin.x + timline_size.x, HorizonScrollAreaMax.y - 1);    // whole bar area
    HorizonScrollBarRect = ImRect(HorizonScrollBarMin, HorizonScrollBarMax);

    ImVec2 VerticalScrollBarSize(scrollSize / 2, window_size.y - scrollSize - toolbar_size.y);
    ImVec2 VerticalScrollBarPos = window_pos + ImVec2(timline_size.x, toolbar_size.y);
    static ImVec2 panningViewVerticalSource;
    static float panningViewVerticalPos;

    editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
    //if (lastFirstTime != -1 && lastFirstTime != editingClip->firstTime) changed = true;
    //if (lastVisiableTime != -1 && lastVisiableTime != newVisibleTime) changed = true;
    lastFirstTime = editingClip->firstTime;
    lastVisiableTime = newVisibleTime;

    if (editingClip->visibleTime >= duration)
    {
        editingClip->firstTime = 0;
        need_update = true;
    }
    else if (editingClip->firstTime + editingClip->visibleTime > duration)
    {
        editingClip->firstTime = duration - editingClip->visibleTime;
        editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
        need_update = true;
    }
    editingClip->lastTime = editingClip->firstTime + editingClip->visibleTime;

    // draw toolbar
    draw_list->AddRectFilled(toolbar_pos, toolbar_pos + toolbar_size, COL_DARK_ONE, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5, 0.5, 0.5, 1.0));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0, 1.0, 1.0, 0.5));

    ImGui::SetCursorScreenPos(toolbar_pos + ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 1.0, 0.5));
    ImGui::TextUnformatted(ICON_TOOLBAR_START);
    ImGui::PopStyleColor();

    if (!isAttribute)
    {
        ImGui::SameLine();
        ImGui::CheckButton(ICON_BLUE_PRINT "##clip_timeline_show_bp", &show_BP, ImVec4(0.5, 0.5, 0.0, 1.0), false);
        ImGui::ShowTooltipOnHover("Show BluePrint");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::BeginDisabled(!has_selected_event);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MEDIA_DELETE_CLIP "##clip_timeline_delete_event"))
        {
            ImGui::OpenPopup("Delete Event?");
        }
        ImGui::ShowTooltipOnHover("Delete Event");
#if 0
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_CONTENT_CUT "##clip_timeline_cut_event"))
        {
            // TODO::Dicky cut event
        }
        ImGui::ShowTooltipOnHover("Cut Event");
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_CONTENT_PASTE "##clip_timeline_paste_event"))
        {
            // TODO::Dicky paste event
        }
        ImGui::ShowTooltipOnHover("Paste Event");
        ImGui::SameLine();
        if (ImGui::Button(ICON_NODE_COPY "##clip_timeline_copy_event"))
        {
            // TODO::Dicky copy event
        }
        ImGui::ShowTooltipOnHover("Copy Event");
#endif
        ImGui::SameLine();
        if (ImGui::Button(ICON_EXPAND_EVENT "##clip_timeline_expand_event"))
        {
            auto select_event = clip->FindSelectedEvent();
            if (select_event)
            {
                int track_index = select_event->Z();
                if (track_index >= 0 && track_index < clip->mEventTracks.size())
                {
                    auto track = clip->mEventTracks[track_index];
                    if (track)
                    {
                        int64_t new_start = 0;
                        int64_t new_end = clip->Length();
                        auto prev_event = track->FindPreviousEvent(select_event->Id());
                        auto next_event = track->FindNextEvent(select_event->Id());
                        if (prev_event) new_start = prev_event->End();
                        if (next_event) new_end = next_event->Start();
                        select_event->ChangeRange(new_start, new_end);
                        track->Update();
                        //changed = true; // ?
                    }
                }
            }
        }
        ImGui::ShowTooltipOnHover("Expand Event");
        ImGui::EndDisabled();
    }
#if 0
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_UNDO "##clip_timeline_undo"))
    {
        // TODO::Dicky undo
    }
    ImGui::ShowTooltipOnHover("Undo");

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_REDO "##clip_timeline_redo"))
    {
        // TODO::Dicky redo
    }
    ImGui::ShowTooltipOnHover("Redo");
#endif
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

    ImGui::SameLine();
    if (ImGui::Button(ICON_SLIDER_FRAME "##clip_timeline_frame_accuracy"))
    {
        editingClip->msPixelWidthTarget = maxPixelWidthTarget;
        int64_t new_visible_time = (int64_t)floorf((timline_size.x) / editingClip->msPixelWidthTarget);
        editingClip->firstTime = editingClip->mCurrentTime - new_visible_time / 2;
        editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - new_visible_time, (int64_t)0));
        need_update = true;
    }
    ImGui::ShowTooltipOnHover("Frame accuracy");

    ImGui::SameLine();
    if (ImGui::RotateButton(ICON_ZOOM_IN "##slider_zoom_in", ImVec2(0, 0), -90))
    {
        editingClip->msPixelWidthTarget *= 2.0f;
        if (editingClip->msPixelWidthTarget > maxPixelWidthTarget)
            editingClip->msPixelWidthTarget = maxPixelWidthTarget;
        need_update = true;
    }
    ImGui::ShowTooltipOnHover("Accuracy Zoom In");

    ImGui::SameLine();
    if (ImGui::Button(ICON_CURRENT_TIME "##clip_timeline_current_time"))
    {
        editingClip->firstTime = editingClip->mCurrentTime - editingClip->visibleTime / 2;
        editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
        need_update = true;
    }
    ImGui::ShowTooltipOnHover("Current time");

    ImGui::SameLine();
    if (ImGui::RotateButton(ICON_ZOOM_OUT "##slider_zoom_out", ImVec2(0, 0), -90))
    {
        editingClip->msPixelWidthTarget *= 0.5f;
        if (editingClip->msPixelWidthTarget < minPixelWidthTarget)
            editingClip->msPixelWidthTarget = minPixelWidthTarget;
        need_update = true;
    }
    ImGui::ShowTooltipOnHover("Accuracy Zoom Out");

    ImGui::SameLine();
    if (ImGui::Button(ICON_SLIDER_CLIP "##clip_timeline_clip_accuracy"))
    {
        editingClip->msPixelWidthTarget = minPixelWidthTarget;
        editingClip->firstTime = 0;
        need_update = true;
    }
    ImGui::ShowTooltipOnHover("Clip accuracy");

    if (!isAttribute)
    {
        ImGui::BeginDisabled(!has_selected_event);
        ImGui::SameLine();
        if (ImGui::Button(ICON_EVENT_ZOOM "##clip_timeline_event_accuracy"))
        {
            auto select_event = clip->FindSelectedEvent();
            if (select_event)
            {
                editingClip->firstTime = select_event->Start();
                int64_t new_visible_time = select_event->Length();
                if (new_visible_time > 0)
                {
                    editingClip->msPixelWidthTarget = HorizonScrollBarRect.GetWidth() / (float)new_visible_time;
                    editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                }
                need_update = true;
            }
        }
        ImGui::ShowTooltipOnHover("Event accuracy");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

    if (!isAttribute)
    {
        ImGui::BeginDisabled(!has_selected_event);
        ImGui::SameLine();
        if (ImGui::RotateButton(ICON_MD_EXIT_TO_APP "##clip_timeline_prev_event", ImVec2(0, 0), 180))
        {
            auto select_event = clip->FindSelectedEvent();
            if (select_event)
            {
                int track_index = select_event->Z();
                if (track_index >= 0 && track_index < clip->mEventTracks.size())
                {
                    auto track = clip->mEventTracks[track_index];
                    if (track)
                    {
                        auto prev_event = track->FindPreviousEvent(select_event->Id());
                        if (prev_event)
                        {
                            if (prev_event->Start() < editingClip->firstTime || prev_event->Start() > editingClip->lastTime)
                            {
                                editingClip->firstTime = prev_event->Start();
                                need_update = true;
                            }
                            clip->SelectEvent(prev_event);
                        }
                    }
                }
            }
        }
        ImGui::ShowTooltipOnHover("Previous event");
    
        ImGui::SameLine();
        if (ImGui::Button(ICON_EVENT_HIDE "##clip_timeline_current_event"))
        {
            auto select_event = clip->FindSelectedEvent();
            if (select_event)
            {
                editingClip->firstTime = select_event->Start();
                need_update = true;
            }
        }
        ImGui::ShowTooltipOnHover("Current event");
    
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_EXIT_TO_APP "##clip_timeline_next_event"))
        {
            auto select_event = clip->FindSelectedEvent();
            if (select_event)
            {
                int track_index = select_event->Z();
                if (track_index >= 0 && track_index < clip->mEventTracks.size())
                {
                    auto track = clip->mEventTracks[track_index];
                    if (track)
                    {
                        auto next_event = track->FindNextEvent(select_event->Id());
                        if (next_event)
                        {
                            if (next_event->Start() < editingClip->firstTime || next_event->Start() > editingClip->lastTime)
                            {
                                editingClip->firstTime = next_event->Start();
                                need_update = true;
                            }
                            clip->SelectEvent(next_event);
                        }
                    }
                }
            }
        }
        ImGui::ShowTooltipOnHover("Next event");
        ImGui::EndDisabled();
    }
    else if (is_video_clip)
    {
        EditingVideoClip* pVidEditingClip = dynamic_cast<EditingVideoClip*>(editingClip);
        ImGui::SameLine(); ImGui::BeginDisabled(!pVidEditingClip);
        bool bShowPreviewFrame = pVidEditingClip ? pVidEditingClip->meAttrOutFramePhase == MediaCore::CorrelativeFrame::PHASE_AFTER_MIXING : true;
        if (ImGui::RotateCheckButton(bShowPreviewFrame ? ICON_MEDIA_PREVIEW : ICON_FILTER "##video_attribute_output_preview", &bShowPreviewFrame, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)))
            pVidEditingClip->meAttrOutFramePhase = bShowPreviewFrame ? MediaCore::CorrelativeFrame::PHASE_AFTER_MIXING : MediaCore::CorrelativeFrame::PHASE_AFTER_TRANSFORM;
        ImGui::EndDisabled();
        ImGui::ShowTooltipOnHover(bShowPreviewFrame ? "Attribute Output" : "Preview Output");
    }

    ImGui::PopStyleColor(4);
    // tool bar end

    draw_list->AddLine(canvas_pos + ImVec2(2, 1), canvas_pos + ImVec2(canvas_size.x - 4, 1), IM_COL32(255, 255, 255, 224));
    
    // Handle event delete
    if (msgbox.Draw() == 1)
    {
        clip->DeleteEvent(clip->FindSelectedEvent(), &main_timeline->mUiActions);
        has_selected_event = false;
    }
    popupDialog = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

    // draw clip timeline
    ImGui::SetCursorScreenPos(toolbar_pos + ImVec2(0, toolbarHeight + 1));
    ImGui::BeginGroup();
    bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    {
        ImVec2 HeaderPos = canvas_pos + ImVec2(0, 1);
        ImGui::SetCursorScreenPos(HeaderPos);
        ImVec2 headerSize(timline_size.x, (float)header_height);
        ImRect HeaderAreaRect(HeaderPos, HeaderPos + headerSize);
        ImGui::InvisibleButton("clip_topBar", headerSize);

        // draw Header bg
        draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Max, COL_DARK_ONE, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

        ImVec2 childFramePos = window_pos + ImVec2(0, header_height + toolbarHeight + 1); // add tool bar height
        ImVec2 childFrameSize(timline_size.x, custom_height); // clip snapshot
        ImGui::BeginChildFrame(ImGui::GetID("clip_timeline"), childFrameSize, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::InvisibleButton("clip_contentBar", ImVec2(timline_size.x, float(header_height + custom_height)));
        const ImVec2 contentMin = childFramePos;
        const ImVec2 contentMax = childFramePos + childFrameSize;
        const ImRect contentRect(contentMin, contentMax);
        const ImRect trackAreaRect(contentMin + ImVec2(0, custom_height), window_pos + window_size - ImVec2(0, scrollSize));
        const ImRect topRect(ImVec2(contentMin.x, canvas_pos.y), ImVec2(contentMin.x + timline_size.x, canvas_pos.y + header_height));
        const float contentHeight = contentMax.y - contentMin.y;
        // full canvas background
        draw_list->AddRectFilled(HeaderPos + ImVec2(0, header_height), HeaderPos + ImVec2(0, header_height) + timline_size - ImVec2(0, header_height + scrollSize), COL_CANVAS_BG, 0);

        // calculate mouse pos to time
        mouseTime = (int64_t)((cx - contentMin.x) / editingClip->msPixelWidthTarget) + editingClip->firstTime;
        //alignTime(mouseTime, frame_duration);
        menuIsOpened = ImGui::IsPopupOpen("##clip_timeline-context-menu");

        //header
        //header time and lines
        int64_t modTimeCount = 10;
        int timeStep = 1;
        while ((modTimeCount * editingClip->msPixelWidthTarget) < 75)
        {
            modTimeCount *= 10;
            timeStep *= 10;
        };
        int halfModTime = modTimeCount / 2;
        auto drawLine = [&](int64_t i, int regionHeight)
        {
            bool baseIndex = ((i % modTimeCount) == 0) || (i == end - start || i == 0);
            bool halfIndex = (i % halfModTime) == 0;
            int px = (int)contentMin.x + int(i * editingClip->msPixelWidthTarget) - int(editingClip->firstTime * editingClip->msPixelWidthTarget);
            int timeStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int timeEnd = baseIndex ? regionHeight : header_height - 8;
            if (px <= (timline_size.x + contentMin.x) && px >= contentMin.x)
            {
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)timeStart), ImVec2((float)px, canvas_pos.y + (float)timeEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
            }
            if (baseIndex && px > contentMin.x)
            {
                auto time_str = ImGuiHelper::MillisecToString(i, 2);
                ImGui::SetWindowFontScale(0.8);
                draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y + 8), COL_RULE_TEXT, time_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
        };
        auto drawLineContent = [&](int64_t i, int)
        {
            int px = (int)contentMin.x + int(i * editingClip->msPixelWidthTarget) - int(editingClip->firstTime * editingClip->msPixelWidthTarget);
            int timeStart = int(contentMin.y);
            int timeEnd = int(contentMax.y);
            if (px <= (timline_size.x + contentMin.x) && px >= contentMin.x)
            {
                draw_list->AddLine(ImVec2(float(px), float(timeStart)), ImVec2(float(px), float(timeEnd)), COL_SLOT_V_LINE, 1);
            }
        };
        auto _mark_start = (editingClip->firstTime / timeStep) * timeStep;
        auto _mark_end = (editingClip->lastTime / timeStep) * timeStep;
        for (auto i = _mark_start; i <= _mark_end; i += timeStep)
        {
            drawLine(i, header_height);
        }

        // handle menu
        if ((HeaderAreaRect.Contains(io.MousePos) || trackAreaRect.Contains(io.MousePos)) && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right) && !menuIsOpened)
        {
            if (mouseTime != -1) 
            {
                menuMouseTime = mouseTime;
                menuMousePos = io.MousePos;
            }
            ImGui::OpenPopup("##clip_timeline-context-menu");
            menuIsOpened = true;
        }

        if (!menuIsOpened)
        {
            menuMouseTime = -1;
            menuMousePos = ImVec2(-1, -1);
        }
        if (ImGui::BeginPopup("##clip_timeline-context-menu"))
        {
            if (ImGui::MenuItem(ICON_SLIDER_FRAME " Frame accuracy", nullptr, nullptr))
            {
                editingClip->msPixelWidthTarget = maxPixelWidthTarget;
                if (menuMouseTime != -1)
                {
                    int64_t new_visible_time = (int64_t)floorf((timline_size.x) / editingClip->msPixelWidthTarget);
                    editingClip->firstTime = menuMouseTime - new_visible_time / 2;
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - new_visible_time, (int64_t)0));
                    need_update = true;
                }
            }
            if (ImGui::MenuItem(ICON_SLIDER_CLIP " Clip accuracy", nullptr, nullptr))
            {
                editingClip->msPixelWidthTarget = minPixelWidthTarget;
                editingClip->firstTime = 0;
                need_update = true;
            }
            if (ImGui::MenuItem(ICON_CURRENT_TIME " Current Time", nullptr, nullptr))
            {
                editingClip->firstTime = editingClip->mCurrentTime - editingClip->visibleTime / 2;
                editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                need_update = true;
            }

            if (HeaderAreaRect.Contains(menuMousePos))
            {
                // TODO::Dicky Clip timeline Add Header menu items?
            }
            if (trackAreaRect.Contains(menuMousePos))
            {
                // TODO::Dicky Clip timeline Add Clip menu items?
            }
            ImGui::EndPopup();
        }
        ImGui::EndChildFrame();

        // Horizon Scroll bar
        float HorizonStartOffset = ((float)(editingClip->firstTime) / (float)duration) * timline_size.x;
        bool inHorizonScrollBar = HorizonScrollBarRect.Contains(io.MousePos);
        draw_list->AddRectFilled(HorizonScrollBarMin, HorizonScrollBarMax, COL_SLIDER_BG, 8);
        ImVec2 HorizonScrollHandleBarMin(HorizonScrollAreaMin.x + HorizonStartOffset, HorizonScrollAreaMin.y);  // current bar area
        ImVec2 HorizonScrollHandleBarMax(HorizonScrollAreaMin.x + HorizonBarWidthInPixels + HorizonStartOffset, HorizonScrollAreaMax.y - 2);
        ImRect HorizonScrollThumbLeft(HorizonScrollHandleBarMin + ImVec2(2, 2), HorizonScrollHandleBarMin + ImVec2(scrollSize - 4, scrollSize - 4));
        ImRect HorizonScrollThumbRight(HorizonScrollHandleBarMax - ImVec2(scrollSize - 4, scrollSize - 4), HorizonScrollHandleBarMax - ImVec2(2, 2));
        HorizonScrollHandleBarRect = ImRect(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax);
        bool inHorizonScrollHandle = HorizonScrollHandleBarRect.Contains(io.MousePos);
        bool inHorizonScrollThumbLeft = HorizonScrollThumbLeft.Contains(io.MousePos);
        bool inHorizonScrollThumbRight = HorizonScrollThumbRight.Contains(io.MousePos);
        draw_list->AddRectFilled(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax, (inHorizonScrollBar || MovingHorizonScrollBar == 0) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
        draw_list->AddRectFilled(HorizonScrollThumbLeft.Min, HorizonScrollThumbLeft.Max, (inHorizonScrollThumbLeft || MovingHorizonScrollBar == 1) ? COL_SLIDER_THUMB_IN : COL_SLIDER_HANDLE, 6);
        draw_list->AddRectFilled(HorizonScrollThumbRight.Min, HorizonScrollThumbRight.Max, (inHorizonScrollThumbRight || MovingHorizonScrollBar == 2) ? COL_SLIDER_THUMB_IN : COL_SLIDER_HANDLE, 6);

        if (MovingHorizonScrollBar == 1)
        {
            // Scroll Thumb Left
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = -1;
            }
            else
            {
                auto new_start_offset = ImMax(io.MousePos.x - HorizonScrollBarMin.x, 0.f);
                auto current_scroll_width = ImMax(HorizonScrollHandleBarMax.x - new_start_offset - HorizonScrollBarMin.x, (float)scrollSize);
                editingClip->msPixelWidthTarget = (HorizonScrollBarRect.GetWidth() * HorizonScrollBarRect.GetWidth()) / (current_scroll_width * duration);
                editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                editingClip->firstTime = new_start_offset / HorizonScrollBarRect.GetWidth() * (float)duration;
                int64_t new_visible_time = (int64_t)floorf(timline_size.x / editingClip->msPixelWidthTarget);
                editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - new_visible_time, (int64_t)0));
                need_update = true;
            }
        }
        else if (MovingHorizonScrollBar == 2)
        {
            // Scroll Thumb Right
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = -1;
            }
            else
            {
                auto new_end_offset = ImClamp(io.MousePos.x - HorizonScrollBarMin.x, HorizonScrollHandleBarMin.x - HorizonScrollBarMin.x + scrollSize, HorizonScrollBarMax.x - HorizonScrollBarMin.x);
                auto current_scroll_width = ImMax(new_end_offset + HorizonScrollBarMin.x - HorizonScrollHandleBarMin.x, (float)scrollSize);
                editingClip->msPixelWidthTarget = (HorizonScrollBarRect.GetWidth() * HorizonScrollBarRect.GetWidth()) / (current_scroll_width * duration);
                editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                need_update = true;
            }
        }
        else if (MovingHorizonScrollBar == 0)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = -1;
            }
            else
            {
                float msPerPixelInBar = HorizonBarPos / (float)editingClip->visibleTime;
                editingClip->firstTime = int((io.MousePos.x - panningViewHorizonSource.x) / msPerPixelInBar) - panningViewHorizonTime;
                editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                need_update = true;
            }
        }
        else if (inHorizonScrollThumbLeft && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && !menuIsOpened && !mouse_hold)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = 1;
        }
        else if (inHorizonScrollThumbRight && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && !menuIsOpened && !mouse_hold)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = 2;
        }
        else if (inHorizonScrollHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && !menuIsOpened && !mouse_hold)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = 0;
            panningViewHorizonSource = io.MousePos;
            panningViewHorizonTime = - editingClip->firstTime;
        }
        //else if (inHorizonScrollBar && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !menuIsOpened && !mouse_hold)
        //{
        //    float msPerPixelInBar = HorizonBarPos / (float)editingClip->visibleTime;
        //    editingClip->firstTime = int((io.MousePos.x - contentMin.x) / msPerPixelInBar);
        //    alignTime(editingClip->firstTime, frame_duration);
        //    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
        //}

        if (topRect.Contains(io.MousePos))
        {
            overTopBar = true;
        }
        if (contentRect.Contains(io.MousePos))
        {
            overCustomDraw = true;
        }
        if (HorizonScrollBarRect.Contains(io.MousePos))
        {
            overHorizonScrollBar = true;
        }
        if (trackAreaRect.Contains(io.MousePos))
        {
            overTrackView = true;
        }

        // handle mouse wheel event
        if (regionRect.Contains(io.MousePos) && !menuIsOpened && !popupDialog)
        {
            if (overCustomDraw || overHorizonScrollBar || overTopBar || overTrackView)
            {
                // left-right wheel over blank area, moving canvas view
                if (io.MouseWheelH < -FLT_EPSILON)
                {
                    editingClip->firstTime -= editingClip->visibleTime / view_frames;
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                    need_update = true;
                }
                else if (io.MouseWheelH > FLT_EPSILON)
                {
                    editingClip->firstTime += editingClip->visibleTime / view_frames;
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                    need_update = true;
                }
            }
            if (overHorizonScrollBar && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // up-down wheel over scrollbar, scale canvas view
                if (io.MouseWheel < -FLT_EPSILON && editingClip->visibleTime <= duration)
                {
                    editingClip->msPixelWidthTarget *= 0.9f;
                    editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                    int64_t new_mouse_time = (int64_t)((cx - contentMin.x) / editingClip->msPixelWidthTarget) + editingClip->firstTime;
                    int64_t offset = new_mouse_time - mouseTime;
                    editingClip->firstTime -= offset;
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                    need_update = true;
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    editingClip->msPixelWidthTarget *= 1.1f;
                    editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                    int64_t new_mouse_time = (int64_t)((cx - contentMin.x) / editingClip->msPixelWidthTarget) + editingClip->firstTime;
                    int64_t offset = new_mouse_time - mouseTime;
                    editingClip->firstTime -= offset;
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                    need_update = true;
                }
            }
        }

        // handle playing cursor move
        if (main_timeline->mIsPreviewPlaying && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            editingClip->UpdateCurrent(main_timeline->mIsPreviewForward, editingClip->mCurrentTime);
            need_update = true;
        }
        
        // draw clip content
        ImGui::PushClipRect(contentMin, contentMax, true);
        editingClip->DrawContent(draw_list, contentMin, contentMax, changed | need_update);
        ImGui::PopClipRect();

        // time cursor
        if (!MovingCurrentTime && MovingHorizonScrollBar == -1 && editingClip->mCurrentTime >= 0 && topRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !menuIsOpened && !popupDialog && !mouse_hold && isFocused)
        {
            MovingCurrentTime = true;
            editingClip->bSeeking = true;
        }
        if (MovingCurrentTime && duration)
        {
            editingClip->mCurrentTime = (int64_t)((io.MousePos.x - topRect.Min.x) / editingClip->msPixelWidthTarget) + editingClip->firstTime;
            editingClip->mCurrentTime = main_timeline->AlignTime(editingClip->mCurrentTime, 1);
            if (editingClip->mCurrentTime < 0)
                editingClip->mCurrentTime = 0;
            if (editingClip->mCurrentTime >= duration)
                editingClip->mCurrentTime = duration;
            if (editingClip->mCurrentTime < editingClip->firstTime)
                editingClip->firstTime = editingClip->mCurrentTime;
            if (editingClip->mCurrentTime > editingClip->lastTime)
                editingClip->firstTime = editingClip->mCurrentTime - editingClip->visibleTime;
            main_timeline->Seek(editingClip->mCurrentTime + start, true);
        }
        if (editingClip->bSeeking && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = false;
            editingClip->bSeeking = false;
        }

        // draw cursor
        static const float cursorWidth = 2.f;
        ImRect custom_view_rect(canvas_pos, canvas_pos + ImVec2(canvas_size.x, header_height + custom_height));
        draw_list->PushClipRect(custom_view_rect.Min - ImVec2(32, 0), custom_view_rect.Max + ImVec2(32, 0));
        if (editingClip->mCurrentTime >= editingClip->firstTime && editingClip->mCurrentTime <= editingClip->lastTime)
        {
            // cursor arrow
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + (editingClip->mCurrentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - arrowWidth * 0.5f + 1;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), COL_CURSOR_ARROW_R, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = ImGuiHelper::MillisecToString(editingClip->mCurrentTime, 2);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + (editingClip->mCurrentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - str_size.x * 0.5f + 1;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BR, 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, COL_CURSOR_TEXT_R, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
            // cursor line
            float cursorOffset = contentMin.x + (editingClip->mCurrentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - 0.5f;
            draw_list->AddLine(ImVec2(cursorOffset, canvas_pos.y + header_height), ImVec2(cursorOffset, canvas_pos.y + header_height + custom_height), COL_CURSOR_LINE_R, cursorWidth);
        }
        draw_list->PopClipRect();
        ImGui::PopStyleColor();

        // event track
        bool event_editable = !MovingCurrentTime && !ImGui::IsDragDropActive() && !menuIsOpened && !popupDialog;
        auto prevEventMovingPart = eventMovingPart;
        ImGui::SetCursorScreenPos(trackAreaRect.Min);
        if (ImGui::BeginChild("##clip_event_tracks", trackAreaRect.GetSize(), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            auto VerticalScrollPos = ImGui::GetScrollY();
            auto VerticalScrollMax = ImGui::GetScrollMaxY();
            auto VerticalWindow = ImGui::GetCurrentWindow();
            const float VerticalBarHeightRatio = ImMin(VerticalScrollBarSize.y / (VerticalScrollBarSize.y + VerticalScrollMax), 1.f);
            const float VerticalBarHeightInPixels = std::max(VerticalBarHeightRatio * VerticalScrollBarSize.y, (float)scrollSize / 2);

            auto trackview_pos = ImGui::GetCursorPos();
            ImDrawList * drawList = ImGui::GetWindowDrawList();
            ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

            // up-down wheel to scroll vertical
            if (trackAreaRect.Contains(io.MousePos))
            {
                if (io.MouseWheel < -FLT_EPSILON || io.MouseWheel > FLT_EPSILON)
                {
                    float offset = -io.MouseWheel * 5 + VerticalScrollPos;
                    offset = ImClamp(offset, 0.f, VerticalScrollMax);
                    ImGui::SetScrollY(VerticalWindow, offset);
                }
            }

            if (isAttribute)
            {
                // TODO: Draw attribute curves using 'ImGui::ImNewCurve'
            }
            else
            {
                // show tracks
                int mouse_track_index = -1;
                auto tracks = clip->mEventTracks;
                auto track_pos = ImGui::GetCursorScreenPos();
                auto track_current = track_pos;
                int current_index = 0;
                bool mouse_hold = false;
                for (auto event : clip->mEventStack->GetEventList())  event->SetStatus(EVENT_HOVERED_BIT, 0); // clear clip hovered status
                for ( auto track : tracks)
                {
                    ImVec2 track_size = ImVec2(event_track_size.x, trackHeight + (track->mExpanded ? curveHeight : 0));
                    ImRect track_area = ImRect(track_current, track_current + track_size);
                    ImVec2 track_title_size = ImVec2(event_track_size.x, trackHeight);
                    ImRect track_title_area = ImRect(track_current, track_current + track_title_size);
                    ImRect track_curve_area = track_area; track_curve_area.Min.y += trackHeight;
                    ImVec2 pos = ImVec2(track_current.x - editingClip->firstTime * editingClip->msPixelWidthTarget, track_current.y);
                    bool is_mouse_hovered = track_area.Contains(io.MousePos);
                    unsigned int col = is_mouse_hovered && event_editable ? COL_EVENT_HOVERED : (current_index & 1) ? COL_EVENT_ODD : COL_EVENT_EVEN;
                    drawList->AddRectFilled(track_title_area.Min, track_title_area.Max, col);
                    if (track->mExpanded) drawList->AddRectFilled(track_curve_area.Min, track_curve_area.Max, IM_COL32_BLACK);
                    ImGui::SetCursorScreenPos(track_current);
                    ImGui::InvisibleButton("##event_track", track_size);
                    if (is_mouse_hovered)
                    {
                        for (auto event_id : track->m_Events)
                        {
                            auto event = clip->mEventStack->GetEvent(event_id);
                            if (!event) continue;
                            if (event->IsInRange(mouseTime))
                            {
                                event->SetStatus(EVENT_HOVERED_BIT, 1);
                                mouseEvent = event;
                                auto event_view_width = event->Length() * editingClip->msPixelWidthTarget;
                                if (eventMovingEntry == -1)
                                {
                                    if (event_view_width <= 20)
                                    {
                                        // event is too small, don't dropping
                                        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                                        {
                                            eventMovingEntry = event->Id();
                                            eventMovingPart = 3;
                                            bCropping = false;
                                        }
                                    }
                                    else if (event_editable)
                                    {
                                        // check event moving part
                                        ImVec2 eventP1(pos.x + event->Start() * editingClip->msPixelWidthTarget, pos.y);
                                        ImVec2 eventP2(pos.x + event->End() * editingClip->msPixelWidthTarget, pos.y + trackHeight);
                                        const float max_handle_width = eventP2.x - eventP1.x / 3.0f;
                                        const float min_handle_width = ImMin(10.0f, max_handle_width);
                                        const float handle_width = ImClamp(editingClip->msPixelWidthTarget / 2.0f, min_handle_width, max_handle_width);
                                        ImRect rects[3] = {ImRect(eventP1, ImVec2(eventP1.x + handle_width, eventP2.y)), ImRect(ImVec2(eventP2.x - handle_width, eventP1.y), eventP2), ImRect(eventP1 + ImVec2(handle_width, 0), eventP2 - ImVec2(handle_width, 0))};
                                        for (int j = 1; j >= 0; j--)
                                        {
                                            int flags = j == 0 ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
                                            ImRect &rc = rects[j];
                                            if (!rc.Contains(io.MousePos))
                                                continue;
                                            if (j == 0)
                                                ImGui::RenderMouseCursor(ICON_CROPPING_LEFT, ImVec2(4, 0));
                                            else
                                                ImGui::RenderMouseCursor(ICON_CROPPING_RIGHT, ImVec2(12, 0));
                                            drawList->AddRectFilled(rc.Min, rc.Max, IM_COL32(255,255,0,255), 4, flags);
                                        }
                                        for (int j = 0; j < 3; j++)
                                        {
                                            // j == 0 : left crop rect
                                            // j == 1 : right crop rect
                                            // j == 2 : moving rect
                                            ImRect &rc = rects[j];
                                            if (!rc.Contains(io.MousePos))
                                                continue;
                                            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                                            {
                                                eventMovingEntry = event->Id();
                                                eventMovingPart = j + 1;
                                                if (j <= 1)
                                                {
                                                    bCropping = true;
                                                    if (j == 0)
                                                        ImGui::RenderMouseCursor(ICON_CROPPING_LEFT, ImVec2(4, 0));
                                                    else
                                                        ImGui::RenderMouseCursor(ICON_CROPPING_RIGHT, ImVec2(12, 0));
                                                    //ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        mouse_track_index = current_index;
                    }
                    mouse_hold |= track->DrawContent(drawList, ImRect(track_current, track_current + track_size), trackHeight, curveHeight, editingClip->firstTime, editingClip->lastTime, editingClip->msPixelWidthTarget, event_editable, changed);

                    track_current += ImVec2(0, track_size.y);
                    current_index ++;
                }
                if (prevEventMovingPart != eventMovingPart)
                    bNewDragOp = true;

                // draw empty track if has space
                while (track_current.y + event_track_size.y < track_pos.y + trackAreaRect.GetSize().y)
                {
                    ImRect track_area = ImRect(track_current, track_current + event_track_size);
                    bool is_mouse_hovered = track_area.Contains(io.MousePos);
                    unsigned int col = /*is_mouse_hovered ? COL_EVENT_HOVERED :*/ (current_index & 1) ? COL_EVENT_ODD : COL_EVENT_EVEN;
                    drawList->AddRectFilled(track_area.Min, track_area.Max, col);
                    ImGui::SetCursorScreenPos(track_current);
                    ImGui::InvisibleButton("##empty_event_track", event_track_size);
                    if (is_mouse_hovered) mouse_track_index = current_index;
                    track_current += ImVec2(0, event_track_size.y);
                    current_index ++;
                }

                // event cropping or moving
                if (!mouse_hold && eventMovingEntry != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    ImGui::CaptureMouseFromApp();
                    diffTime += io.MouseDelta.x / editingClip->msPixelWidthTarget;
                    const auto frameRate = main_timeline->mhMediaSettings->VideoOutFrameRate();
                    if (diffTime > frameTime(frameRate) || diffTime < -frameTime(frameRate))
                    {
                        std::list<imgui_json::value>* pActionList = nullptr;
                        if (bNewDragOp)
                        {
                            pActionList = &main_timeline->mOngoingActions;
                            bNewDragOp = false;
                        }
                        if (eventMovingPart == 3)
                        {
                            clip->EventMoving(eventMovingEntry, diffTime, mouseTime, pActionList);
                            changed = true;
                        }
                        else if (eventMovingPart & 1)
                        {
                            clip->EventCropping(eventMovingEntry, diffTime, 0, pActionList);
                            changed = true;
                        }
                        else if (eventMovingPart & 2)
                        {
                            clip->EventCropping(eventMovingEntry, diffTime, 1, pActionList);
                            changed = true;
                        }
                        diffTime = 0;
                    }
                }

                // handle drag a filter here
                ImGui::SetCursorScreenPos(trackAreaRect.Min + ImVec2(3, 3));
                ImGui::InvisibleButton("clip_event_tracks_view", trackAreaRect.GetSize() - ImVec2(6, 6));
                if (ImGui::BeginDragDropTarget())
                {
                    // drag drop line and time indicate
                    auto _payload = ImGui::GetDragDropPayload();
                    if (_payload && 
                        (IS_VIDEO(editingClip->mType) && _payload->IsDataType("Filter_drag_drop_Video")) ||
                        (IS_AUDIO(editingClip->mType) && _payload->IsDataType("Filter_drag_drop_Audio")) )
                    {
                        drawList->PushClipRect(trackAreaRect.Min, trackAreaRect.Max);
                        static const float cursorWidth = 2.f;
                        float lineOffset = track_pos.x + (mouseTime - editingClip->firstTime) * editingClip->msPixelWidthTarget + 1;
                        drawList->AddLine(ImVec2(lineOffset, contentMin.y), ImVec2(lineOffset, track_current.y), mouseEvent ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255), cursorWidth);
                        drawList->PopClipRect();
                        ImGui::SetWindowFontScale(0.8);
                        auto time_str = ImGuiHelper::MillisecToString(mouseTime, 2);
                        ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
                        float strOffset = track_pos.x + (mouseTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - str_size.x * 0.5f + 1;
                        ImVec2 str_pos = ImVec2(strOffset, track_pos.y);
                        drawList->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG2, 2.0, ImDrawFlags_RoundCornersAll);
                        drawList->AddText(str_pos, COL_CURSOR_TEXT2, time_str.c_str());
                        ImGui::SetWindowFontScale(1.0);
                    }

                    if (const ImGuiPayload* payload =   IS_VIDEO(editingClip->mType) ? ImGui::AcceptDragDropPayload("Filter_drag_drop_Video") :
                                                        IS_AUDIO(editingClip->mType) ? ImGui::AcceptDragDropPayload("Filter_drag_drop_Audio") : nullptr)
                    {
                        const BluePrint::Node * node = (const BluePrint::Node *)payload->Data;
                        if (node)
                        {
                            int64_t min_duration = 30 / editingClip->msPixelWidthTarget;
                            if (mouse_track_index == -1 || mouse_track_index >= tracks.size() || tracks.empty())
                            {
                                // need add new event track
                                auto new_track = clip->AddEventTrack();
                                if (new_track >= 0)
                                {
                                    int64_t _duration = ImMin(min_duration, clip->Length() - mouseTime);
                                    auto event = clip->AddEvent(-1, new_track, mouseTime, _duration, node, &main_timeline->mUiActions);
                                    if (event) changed = true;
                                }
                            }
                            else if (!mouseEvent)
                            {
                                // insert event on exist track
                                // check next event start time to decide duration
                                auto track = clip->mEventTracks[mouse_track_index];
                                auto _max_duration = track->FindEventSpace(mouseTime);
                                int64_t _duration = _max_duration == -1 ? ImMin(min_duration, clip->Length() - mouseTime) : 
                                                    ImMin(min_duration, _max_duration);
                                auto event = clip->AddEvent(-1, mouse_track_index, mouseTime, _duration, node, &main_timeline->mUiActions);
                                if (event) changed = true;
                            }
                            else if (mouseEvent)
                            {
                                // append event on exist event
                                auto appended = clip->AppendEvent(mouseEvent, (void *)node);
                                if (appended) { clip->SelectEvent(mouseEvent); changed = true; }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            if (editingClip->mCurrentTime >= editingClip->firstTime && editingClip->mCurrentTime <= editingClip->lastTime)
            {
                // draw cursor line
                float cursorOffset = window_pos.x + (editingClip->mCurrentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - 0.5f;
                drawList->AddLine(ImVec2(cursorOffset, cursor_pos.y), ImVec2(cursorOffset, window_pos.y + window_size.y), COL_CURSOR_LINE_R, cursorWidth);
            }

            // draw vertical scroll bar
            if (!isAttribute)
            {
                ImGui::SetCursorScreenPos(VerticalScrollBarPos);
                ImGui::InvisibleButton("VerticalScrollBar##event_track_view", VerticalScrollBarSize);
                float VerticalStartOffset = VerticalScrollPos * VerticalBarHeightRatio;
                ImVec2 VerticalScrollBarMin = ImGui::GetItemRectMin();
                ImVec2 VerticalScrollBarMax = ImGui::GetItemRectMax();
                auto VerticalScrollBarRect = ImRect(VerticalScrollBarMin, VerticalScrollBarMax);
                bool inVerticalScrollBar = VerticalScrollBarRect.Contains(io.MousePos);
                draw_list->AddRectFilled(VerticalScrollBarMin, VerticalScrollBarMax, COL_SLIDER_BG, 8);
                ImVec2 VerticalScrollHandleBarMin(VerticalScrollBarMin.x, VerticalScrollBarMin.y + VerticalStartOffset);  // current bar area
                ImVec2 VerticalScrollHandleBarMax(VerticalScrollBarMin.x + (float)scrollSize / 2, VerticalScrollBarMin.y + VerticalBarHeightInPixels + VerticalStartOffset);
                auto VerticalScrollHandleBarRect = ImRect(VerticalScrollHandleBarMin, VerticalScrollHandleBarMax);
                bool inVerticalScrollHandle = VerticalScrollHandleBarRect.Contains(io.MousePos);
                draw_list->AddRectFilled(VerticalScrollHandleBarMin, VerticalScrollHandleBarMax, (inVerticalScrollBar || MovingVerticalScrollBar) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
                if (MovingVerticalScrollBar)
                {
                    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        MovingVerticalScrollBar = false;
                    }
                    else
                    {
                        float offset = (io.MousePos.y - panningViewVerticalSource.y) / VerticalBarHeightRatio + panningViewVerticalPos;
                        offset = ImClamp(offset, 0.f, VerticalScrollMax);
                        ImGui::SetScrollY(VerticalWindow, offset);
                    }
                }
                else if (inVerticalScrollHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && !bEventMoving && !menuIsOpened && !mouse_hold)
                {
                    ImGui::CaptureMouseFromApp();
                    MovingVerticalScrollBar = true;
                    panningViewVerticalSource = io.MousePos;
                    panningViewVerticalPos = VerticalScrollPos;
                }
                else if (inVerticalScrollBar && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !bEventMoving && !menuIsOpened && mouse_hold)
                {
                    float offset = (io.MousePos.y - VerticalScrollBarPos.y) / VerticalBarHeightRatio;
                    offset = ImClamp(offset, 0.f, VerticalScrollMax);
                    ImGui::SetScrollY(VerticalWindow, offset);
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        mouse_hold = false;
        bCropping = false;
        bEventMoving = false;
        eventMovingEntry = -1;
        eventMovingPart = -1;
        bNewDragOp = false;
        ImGui::CaptureMouseFromApp(false);
    }
    return mouse_hold;
}

/***********************************************************************************************************
 * Draw Transition Timeline
 ***********************************************************************************************************/
bool DrawOverlapTimeLine(BaseEditingOverlap * overlap, int64_t CurrentTime, int header_height, int custom_height)
{
    /***************************************************************************************
    |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    |
    |_______________|_____________________________________________________________________ c
    |               |        clip 1 custom area                                            a 
    |               |                                                                      n                                            
    |_______________|_____________________________________________________________________ v
    |               |        clip 2 custom area                                            a 
    |               |                                                                      s                                           
    |_______________|_____________________________________________________________________ |    
    ***************************************************************************************/
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    static int64_t lastDuration = -1;
    static int lastWindowWidth = -1;
    bool changed = false;
    if (!overlap)
    {
        ImGui::SetWindowFontScale(2);
        auto pos_center = window_pos + window_size / 2;
        std::string tips_string = "Please Select Overlap by Double Click From Main Timeline";
        auto string_width = ImGui::CalcTextSize(tips_string.c_str());
        auto tips_pos = pos_center - string_width / 2;
        ImGui::SetWindowFontScale(1.0);
        draw_list->AddTextComplex(tips_pos, tips_string.c_str(), 2.f, IM_COL32(255, 255, 255, 128), 0.5f, IM_COL32(56, 56, 56, 192));
        return changed;
    }
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    static bool MovingCurrentTime = false;
    bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    int64_t duration = ImMax(overlap->mEnd - overlap->mStart, (int64_t)1);
    int64_t start = 0;
    int64_t end = start + duration;
    if (overlap->mCurrentTime == -1)
    {
        overlap->mCurrentTime = CurrentTime;
        if (overlap->mCurrentTime < 0) overlap->mCurrentTime = 0;
        if (overlap->mCurrentTime > duration) overlap->mCurrentTime = duration;
    }
    else
    {
    }

    if (lastDuration != -1 && lastDuration != duration) changed = true;
    if (lastWindowWidth != -1 && lastWindowWidth != (int)window_size.x) changed = true;
    lastDuration = duration;
    lastWindowWidth = (int)window_size.x;

    ImGui::BeginGroup();
    ImRect regionRect(window_pos + ImVec2(0, header_height), window_pos + window_size);
    
    overlap->msPixelWidth = (float)(window_size.x) / (float)duration;
    ImRect custom_view_rect(window_pos + ImVec2(0, header_height), window_pos + window_size);

    //header
    //header time and lines
    ImVec2 headerSize(window_size.x, (float)header_height);
    ImGui::InvisibleButton("ClipTimelineBar#overlap", headerSize);
    draw_list->AddRectFilled(window_pos, window_pos + headerSize, COL_DARK_ONE, 0);

    ImRect movRect(window_pos, window_pos + window_size);
    if (!MovingCurrentTime && /*overlap->mCurrent >= start &&*/ movRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left) && isFocused)
    {
        MovingCurrentTime = true;
        overlap->bSeeking = true;
    }
    if (MovingCurrentTime && duration)
    {
        auto oldPos = overlap->mCurrentTime;
        auto newPos = (int64_t)((io.MousePos.x - movRect.Min.x) / overlap->msPixelWidth) + start;
        if (newPos < start)
            newPos = start;
        if (newPos >= end)
            newPos = end;
        if (oldPos != newPos)
        {
            overlap->mCurrentTime = newPos;
            overlap->Seek(newPos + overlap->mStart, true); // call seek event
        }
    }
    if (overlap->bSeeking && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        MovingCurrentTime = false;
        overlap->bSeeking = false;
    }

    int64_t modTimeCount = 10;
    int timeStep = 1;
    while ((modTimeCount * overlap->msPixelWidth) < 100)
    {
        modTimeCount *= 10;
        timeStep *= 10;
    };
    int halfModTime = modTimeCount / 2;
    auto drawLine = [&](int64_t i, int regionHeight)
    {
        bool baseIndex = ((i % modTimeCount) == 0) || (i == 0 || i == duration);
        bool halfIndex = (i % halfModTime) == 0;
        int px = (int)window_pos.x + int(i * overlap->msPixelWidth);
        int timeStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
        int timeEnd = baseIndex ? regionHeight : header_height;
        if (px <= (window_size.x + window_pos.x) && px >= window_pos.x)
        {
            draw_list->AddLine(ImVec2((float)px, window_pos.y + (float)timeStart), ImVec2((float)px, window_pos.y + (float)timeEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
        }
        if (baseIndex && px >= window_pos.x)
        {
            auto time_str = ImGuiHelper::MillisecToString(i + start, 2);
            ImGui::SetWindowFontScale(0.8);
            draw_list->AddText(ImVec2((float)px + 3.f, window_pos.y), COL_RULE_TEXT, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }
    };
    for (auto i = 0; i < duration; i+= timeStep)
    {
        drawLine(i, header_height);
    }
    drawLine(0, header_height);
    drawLine(duration, header_height);
    // cursor Arrow
    ImVec2 headerMin = window_pos;
    ImVec2 headerMax = headerMin + headerSize;
    draw_list->PushClipRect(headerMin - ImVec2(32, 0), headerMax + ImVec2(32, 0));
    const float arrowWidth = draw_list->_Data->FontSize;
    float arrowOffset = window_pos.x + (CurrentTime - start) * overlap->msPixelWidth - arrowWidth * 0.5f;
    ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, window_pos.y), COL_CURSOR_ARROW_R, ImGuiDir_Down);
    ImGui::SetWindowFontScale(0.8);
    auto time_str = ImGuiHelper::MillisecToString(CurrentTime, 2);
    ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
    float strOffset = window_pos.x + (CurrentTime - start) * overlap->msPixelWidth - str_size.x * 0.5f;
    ImVec2 str_pos = ImVec2(strOffset, window_pos.y + 10);
    draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BR, 2.0, ImDrawFlags_RoundCornersAll);
    draw_list->AddText(str_pos, COL_CURSOR_TEXT_R, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);
    draw_list->PopClipRect();

    // snapshot
    ImVec2 contentMin(window_pos.x, window_pos.y + (float)header_height);
    ImVec2 contentMax(window_pos.x + window_size.x, window_pos.y + (float)header_height + float(custom_height) * 2);
    overlap->DrawContent(draw_list, contentMin, contentMax, changed);

    // cursor line
    draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
    static const float cursorWidth = 2.f;
    float cursorOffset = contentMin.x + (CurrentTime - start) * overlap->msPixelWidth - cursorWidth * 0.5f + 1;
    draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMax.y), COL_CURSOR_LINE_R, cursorWidth);
    draw_list->PopClipRect();
    ImGui::EndGroup();
    return changed;
}
} // namespace MediaTimeline/Main Timeline
