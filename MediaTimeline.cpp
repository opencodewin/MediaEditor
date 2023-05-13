/*
    Copyright (c) 2023 CodeWin

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
#include <implot.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "Logger.h"
#include "DebugHelper.h"

#define AUDIO_WAVEFORM_IMPLOT   0

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

static void alignTime(int64_t& time, MediaCore::Ratio rate)
{
    if (rate.den && rate.num)
    {
        int64_t frame_index = (int64_t)floor((double)time * (double)rate.num / (double)rate.den / 1000.0);
        time = frame_index * 1000 * rate.den / rate.num;
    }
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

static void waveFrameResample(float * wave, int samples, int size, int start_offset, int size_max, int zoom, ImGui::ImMat& plot_frame_max, ImGui::ImMat& plot_frame_min)
{
    plot_frame_max.create_type(size, 1, 1, IM_DT_FLOAT32);
    plot_frame_min.create_type(size, 1, 1, IM_DT_FLOAT32);
    float * out_channel_data_max = (float *)plot_frame_max.data;
    float * out_channel_data_min = (float *)plot_frame_min.data;
    for (int i = 0; i < size; i++)
    {
        float max_val = -FLT_MAX;
        float min_val = FLT_MAX;
        if (samples <= 32)
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
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    mMediaType = type;
    UpdateItem(name, path, handle);
}

MediaItem::~MediaItem()
{
    mMediaOverview = nullptr;
    for (auto thumb : mMediaThumbnail)
    {
        ImGui::ImDestroyTexture(thumb); 
        thumb = nullptr;
    }
}

void MediaItem::UpdateItem(const std::string& name, const std::string& path, void* handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    ReleaseItem();
    auto old_name = mName;
    auto old_path = mPath;
    auto old_start = mStart;
    auto old_end = mEnd;
    mName = name;
    mPath = path;
    mMediaOverview = MediaCore::Overview::CreateInstance();
    if (timeline)
    {
        mMediaOverview->EnableHwAccel(timeline->mHardwareCodec);
    }
    if (!path.empty() && mMediaOverview)
    {
        mMediaOverview->SetSnapshotResizeFactor(0.05, 0.05);
        mMediaOverview->Open(path, 50);
    }
    if (mMediaOverview && mMediaOverview->IsOpened())
    {
        mStart = 0;
        if (mMediaOverview->HasVideo())
        {
            if (IS_IMAGE(mMediaType))
            {
                mEnd = 5000;
            }
            else
            {
                mMediaOverview->GetMediaParser()->EnableParseInfo(MediaCore::MediaParser::VIDEO_SEEK_POINTS);
                mEnd = mMediaOverview->GetVideoDuration();
            }
        }
        else if (mMediaOverview->HasAudio())
        {
            mEnd = mMediaOverview->GetMediaInfo()->duration * 1000;
        }
        else
        {
            mEnd = 5000;
        }
        if ((old_start !=0 && mStart != old_start) || (old_end != 0 && mEnd != old_end))
        {
            ReleaseItem();
            return;
        }
        mValid = true;
    }
    else if (mMediaOverview && IS_TEXT(mMediaType))
    {
        if (ImGuiHelper::file_exists(mPath))
            mValid = true;
    }
}

void MediaItem::ReleaseItem()
{
    mMediaOverview = nullptr;
    for (auto thumb : mMediaThumbnail)
    {
        ImGui::ImDestroyTexture(thumb); 
        thumb = nullptr;
    }
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
                    ImTextureID thumb = nullptr;
                    ImMatToTexture(snapshots[i], thumb);
                    mMediaThumbnail.push_back(thumb);
                }
            }
        }
    }
}
} //namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * Clip Struct Member Functions
 ***********************************************************************************************************/
Clip::Clip(int64_t start, int64_t end, int64_t id, MediaCore::MediaParser::Holder mediaParser, void * handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    mMediaID = id;
    mStart = start; 
    mEnd = end;
    mLength = end - start;
    mHandle = handle;
    mMediaParser = mediaParser;
    mFilterKeyPoints.SetMin({0.f, 0.f});
    mFilterKeyPoints.SetMax(ImVec2(mLength, 1.f), true);
    mAttributeKeyPoints.SetMin({0.f, 0.f});
    mAttributeKeyPoints.SetMax(ImVec2(mLength, 1.f), true);
}

Clip::~Clip()
{
}

void Clip::Load(Clip * clip, const imgui_json::value& value)
{
    if (value.contains("ID"))
    {
        auto& val = value["ID"];
        if (val.is_number()) clip->mID = val.get<imgui_json::number>();
    }
    if (value.contains("MediaID"))
    {
        auto& val = value["MediaID"];
        if (val.is_number()) clip->mMediaID = val.get<imgui_json::number>();
    }
    if (value.contains("GroupID"))
    {
        auto& val = value["GroupID"];
        if (val.is_number()) clip->mGroupID = val.get<imgui_json::number>();
    }
    if (value.contains("Start"))
    {
        auto& val = value["Start"];
        if (val.is_number()) clip->mStart = val.get<imgui_json::number>();
    }
    if (value.contains("End"))
    {
        auto& val = value["End"];
        if (val.is_number()) clip->mEnd = val.get<imgui_json::number>();
    }
    if (value.contains("StartOffset"))
    {
        auto& val = value["StartOffset"];
        if (val.is_number()) clip->mStartOffset = val.get<imgui_json::number>();
    }
    if (value.contains("EndOffset"))
    {
        auto& val = value["EndOffset"];
        if (val.is_number()) clip->mEndOffset = val.get<imgui_json::number>();
    }
    if (value.contains("Selected"))
    {
        auto& val = value["Selected"];
        if (val.is_boolean()) clip->bSelected = val.get<imgui_json::boolean>();
    }
    if (value.contains("Editing"))
    {
        auto& val = value["Editing"];
        if (val.is_boolean()) clip->bEditing = val.get<imgui_json::boolean>();
    }
    if (value.contains("Path"))
    {
        auto& val = value["Path"];
        if (val.is_string()) clip->mPath = val.get<imgui_json::string>();
    }
    if (value.contains("Name"))
    {
        auto& val = value["Name"];
        if (val.is_string()) clip->mName = val.get<imgui_json::string>();
    }
    clip->mLength = clip->mEnd-clip->mStart;
    // load filter bp
    if (value.contains("FilterBP"))
    {
        auto& val = value["FilterBP"];
        if (val.is_object()) clip->mFilterBP = val;
    }

    // load filter curve
    if (value.contains("FilterKeyPoint"))
    {
        auto& keypoint = value["FilterKeyPoint"];
        clip->mFilterKeyPoints.Load(keypoint);
    }

    // load attribute curve
    if (value.contains("AttributeKeyPoint"))
    {
        auto& keypoint = value["AttributeKeyPoint"];
        clip->mAttributeKeyPoints.Load(keypoint);
    }
}

void Clip::Save(imgui_json::value& value)
{
    // save clip global info
    value["ID"] = imgui_json::number(mID);
    value["MediaID"] = imgui_json::number(mMediaID);
    value["GroupID"] = imgui_json::number(mGroupID);
    value["Type"] = imgui_json::number(mType);
    value["Path"] = mPath;
    value["Name"] = mName;
    value["Start"] = imgui_json::number(mStart);
    value["End"] = imgui_json::number(mEnd);
    value["StartOffset"] = imgui_json::number(mStartOffset);
    value["EndOffset"] = imgui_json::number(mEndOffset);
    value["Selected"] = imgui_json::boolean(bSelected);
    value["Editing"] = imgui_json::boolean(bEditing);

    // save clip filter bp
    if (mFilterBP.is_object())
    {
        value["FilterBP"] = mFilterBP;
    }

    // save Filter curve setting
    imgui_json::value filter_keypoint;
    mFilterKeyPoints.Save(filter_keypoint);
    value["FilterKeyPoint"] = filter_keypoint;

    // save Attribute curve setting
    imgui_json::value attribute_keypoint;
    mAttributeKeyPoints.Save(attribute_keypoint);
    value["AttributeKeyPoint"] = attribute_keypoint;
}

int64_t Clip::Cropping(int64_t diff, int type)
{
    int64_t new_diff = 0;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return new_diff;
    auto track = timeline->FindTrackByClipID(mID);
    if (!track || track->mLocked)
        return new_diff;
    if (IS_DUMMY(mType))
        return new_diff;
    timeline->AlignTime(diff);
    float frame_duration = (timeline->mFrameRate.den > 0 && timeline->mFrameRate.num > 0) ? timeline->mFrameRate.den * 1000.0 / timeline->mFrameRate.num : 40;
    if (type == 0)
    {
        // cropping start
        if ((IS_VIDEO(mType) && !IS_IMAGE(mType)) || IS_AUDIO(mType))
        {
            // audio video stream have length limit
            if (mStart + diff < mEnd - ceil(frame_duration) && mStart + diff >= timeline->mStart)
            {
                if (mStartOffset + diff >= 0)
                {
                    new_diff = diff;
                    mStart += diff;
                    mStartOffset += diff;
                }
                else if (abs(mStartOffset + diff) <= abs(diff))
                {
                    new_diff = diff + abs(mStartOffset + diff);
                    mStart += new_diff;
                    mStartOffset += new_diff;
                }
            }
            else if (mEnd - mStart - ceil(frame_duration) < diff)
            {
                new_diff = mEnd - mStart - ceil(frame_duration);
                mStart += new_diff;
                mStartOffset += new_diff;
            }
        }
        else
        {
            // others have not length limit
            if (mStart + diff < mEnd - ceil(frame_duration) && mStart + diff >= timeline->mStart)
            {
                new_diff = diff;
                mStart += diff;
            }
            else if (mEnd - mStart - ceil(frame_duration) < diff)
            {
                new_diff = mEnd - mStart - ceil(frame_duration);
                mStart += new_diff;
            }
        }
    }
    else
    {
        // cropping end
        if ((IS_VIDEO(mType) && !IS_IMAGE(mType)) || IS_AUDIO(mType))
        {
            // audio video stream have length limit
            if (mEnd + diff > mStart + ceil(frame_duration))
            {
                if (mEndOffset - diff >= 0)
                {
                    new_diff = diff;
                    mEnd += new_diff;
                    mEndOffset -= new_diff;
                }
                else if (abs(mEndOffset - diff) <= abs(diff))
                {
                    new_diff = diff - abs(mEndOffset - diff);
                    mEnd += new_diff;
                    mEndOffset -= new_diff;
                }
            }
            else if (mEnd - mStart - ceil(frame_duration) < abs(diff))
            {
                new_diff = mStart - mEnd + ceil(frame_duration);
                mEnd += new_diff;
                mEndOffset -= new_diff;
            }
        }
        else
        {
            // others have not length limit
            if (mEnd + diff > mStart + ceil(frame_duration))
            {
                new_diff = diff;
                mEnd += new_diff;
            }
            else if (mEnd - mStart - ceil(frame_duration) < abs(diff))
            {
                new_diff = mStart - mEnd + ceil(frame_duration);
                mEnd += new_diff;
            }
        }
    }
    
    if (timeline->mVidFilterClip && timeline->mVidFilterClip->mID == mID)
    {
        timeline->mVidFilterClip->mStart = mStart;
        timeline->mVidFilterClip->mEnd = mEnd;
        if (timeline->mVidFilterClip->mFilter)
        {
            timeline->mVidFilterClip->mFilter->mKeyPoints.SetRangeX(0, mEnd - mStart, true);
            mFilterKeyPoints = timeline->mVidFilterClip->mFilter->mKeyPoints;
        }
        if (timeline->mVidFilterClip->mAttribute)
        {
            timeline->mVidFilterClip->mAttribute->GetKeyPoint()->SetRangeX(0, mEnd - mStart, true);
            mAttributeKeyPoints = *timeline->mVidFilterClip->mAttribute->GetKeyPoint();
        }
    }
    else if (timeline->mAudFilterClip && timeline->mAudFilterClip->mID == mID)
    {
        timeline->mAudFilterClip->mStart = mStart;
        timeline->mAudFilterClip->mEnd = mEnd;
        if (timeline->mAudFilterClip->mFilter)
        {
            timeline->mAudFilterClip->mFilter->mKeyPoints.SetRangeX(0, mEnd - mStart, true);
            mFilterKeyPoints = timeline->mAudFilterClip->mFilter->mKeyPoints;
        }
    }
    else
    {
        mFilterKeyPoints.SetRangeX(0, mEnd - mStart, true);
        mAttributeKeyPoints.SetRangeX(0, mEnd - mStart, true);
    }
    track->Update();
    timeline->UpdateRange();
    return new_diff;
}

void Clip::Cutting(int64_t pos, std::list<imgui_json::value>* pActionList)
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
    timeline->AlignTime(pos);
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
    if (pActionList)
    {
        imgui_json::value action;
        action["action"] = "CROP_CLIP";
        action["media_type"] = imgui_json::number(mType);
        action["clip_id"] = imgui_json::number(mID);
        action["from_track_id"] = imgui_json::number(track->mID);
        action["new_start"] = action["org_start"] = imgui_json::number(mStart);
        action["new_start_offset"] = action["org_start_offset"] = imgui_json::number(mStartOffset);
        action["org_end"] = imgui_json::number(org_end);
        action["org_end_offset"] = imgui_json::number(org_end_offset);
        action["new_end"] = imgui_json::number(mEnd);
        action["new_end_offset"] = imgui_json::number(mEndOffset);
        pActionList->push_back(std::move(action));
    }
    // and add a new clip start at this clip's end
    int64_t newClipId = -1;
    if ((newClipId = timeline->AddNewClip(mMediaID, mType, track->mID,
            new_start, new_start_offset, org_end, org_end_offset,
            mGroupID, -1, pActionList)) >= 0)
    {
        // update curve
        if (timeline->mVidFilterClip && timeline->mVidFilterClip->mID == mID)
        {
            timeline->mVidFilterClip->mStart = mStart;
            timeline->mVidFilterClip->mEnd = mEnd;
            if (timeline->mVidFilterClip->mFilter)
            {
                timeline->mVidFilterClip->mFilter->mKeyPoints.SetRangeX(0, mEnd - mStart, true);
                mFilterKeyPoints = timeline->mVidFilterClip->mFilter->mKeyPoints;
            }
            if (timeline->mVidFilterClip->mAttribute)
            {
                timeline->mVidFilterClip->mAttribute->GetKeyPoint()->SetRangeX(0, mEnd - mStart, true);
                mAttributeKeyPoints = *timeline->mVidFilterClip->mAttribute->GetKeyPoint();
            }
        }
        if (timeline->mAudFilterClip && timeline->mAudFilterClip->mID == mID)
        {
            timeline->mAudFilterClip->mStart = mStart;
            timeline->mAudFilterClip->mEnd = mEnd;
            if (timeline->mAudFilterClip->mFilter)
            {
                timeline->mAudFilterClip->mFilter->mKeyPoints.SetRangeX(0, mEnd - mStart, true);
                mFilterKeyPoints = timeline->mAudFilterClip->mFilter->mKeyPoints;
            }
        }
        else
        {
            mFilterKeyPoints.SetRangeX(0, mEnd - mStart, true);
            mAttributeKeyPoints.SetRangeX(0, mEnd - mStart, true);
        }

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
    
    ImGuiIO &io = ImGui::GetIO();
    bool single = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && (io.KeyMods == ImGuiMod_Ctrl);

    int track_index = timeline->FindTrackIndexByClipID(mID);
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
    for (auto clip : timeline->m_Clips)
    {
        if ((single && clip->mID == mID) || (!single && clip->bSelected && clip->bMoving))
        {
            selected_start_points.push_back(clip->mStart);
            selected_end_points.push_back(clip->mEnd);
        }
        else
        {
            unselected_start_points.push_back(clip->mStart);
            unselected_end_points.push_back(clip->mEnd);
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
        timeline->AlignTime(mStart);
        mEnd = mStart + length;
    }
    else
    {
        if (diff < 0 && group_start + diff < start)
        {
            new_diff = start - group_start;
            mStart += new_diff;
            timeline->AlignTime(mStart);
            mEnd = mStart + length;
        }
        if (diff > 0 && end != -1 && group_end + diff > end)
        {
            new_diff = end - group_end;
            mStart = end - length;
            timeline->AlignTime(mStart);
            mEnd = mStart + length;
        }
    }
    
    auto moving_clip_keypoint = [&](Clip * clip)
    {
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
                timeline->AlignTime(clip->mStart);
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
} // namespace MediaTimeline

namespace MediaTimeline
{
// VideoClip Struct Member Functions
VideoClip::VideoClip(int64_t start, int64_t end, int64_t id, std::string name, MediaCore::MediaParser::Holder hParser, MediaCore::Snapshot::Viewer::Holder hViewer, void* handle)
    : Clip(start, end, id, hParser, handle)
{
    if (handle && hViewer)
    {
        mSsViewer = hViewer;
        if (!mSsViewer)
            return;
        mType = MEDIA_VIDEO;
        mName = name;
        MediaCore::MediaInfo::Holder info = hParser->GetMediaInfo();
        const MediaCore::VideoStream* video_stream = hParser->GetBestVideoStream();
        if (!info || !video_stream)
        {
            hViewer->Release();
            mType |= MEDIA_DUMMY;
            return;
        }
        mWidth = video_stream->width;
        mHeight = video_stream->height;
        mPath = info->url;
    }
}

VideoClip::VideoClip(int64_t start, int64_t end, int64_t id, std::string name, MediaCore::Overview::Holder overview, void* handle)
    : Clip(start, end, id, overview->GetMediaParser(), handle), mOverview(overview)
{
    if (overview)
    {
        mType = MEDIA_SUBTYPE_VIDEO_IMAGE;
        mName = name;
        MediaCore::MediaInfo::Holder info = overview->GetMediaParser()->GetMediaInfo();
        const MediaCore::VideoStream* video_stream = mOverview->GetVideoStream();
        if (!info || !video_stream)
        {
            mOverview = nullptr;
            mType |= MEDIA_DUMMY;
            return;
        }
        TimeLine * timeline = (TimeLine *)handle;
        mWidth = video_stream->width;
        mHeight = video_stream->height;
        int _width = 0, _height = 0;
        std::vector<ImGui::ImMat> snap_images;
        if (mOverview->GetSnapshots(snap_images))
        {
            if (!snap_images.empty() && !snap_images[0].empty() && !mImgTexture && timeline && !timeline->m_in_threads)
            {
                ImMatToTexture(snap_images[0], mImgTexture);
                _width = snap_images[0].w;
                _height = snap_images[0].h;
            }
            if (mTrackHeight > 0 && _width > 0 && _height > 0)
            {
                mSnapHeight = mTrackHeight;
                mSnapWidth = mTrackHeight * _width / _height;
            }
        }

        mPath = info->url;
    }
}

VideoClip::VideoClip(int64_t start, int64_t end, int64_t id, std::string name, void* handle)
    : Clip(start, end, id, nullptr, handle)
{
    mType = MEDIA_VIDEO | MEDIA_DUMMY;
    mName = name;
}

VideoClip::~VideoClip()
{
    if (mSsViewer) mSsViewer->Release();
    for (auto& snap : mVideoSnapshots)
    {
        if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; }
    }
    mVideoSnapshots.clear();
    if (mImgTexture) { ImGui::ImDestroyTexture(mImgTexture); mImgTexture = nullptr; }
}

void VideoClip::UpdateClip(MediaCore::MediaParser::Holder hParser, MediaCore::Snapshot::Viewer::Holder viewer, int64_t duration)
{
    if (viewer)
    {
        mType = MEDIA_VIDEO;
        mMediaParser = hParser;
        mSsViewer = viewer;
        MediaCore::MediaInfo::Holder info = hParser->GetMediaInfo();
        const MediaCore::VideoStream* video_stream = hParser->GetBestVideoStream();
        if (!info || !video_stream)
        {
            viewer->Release();
            mType |= MEDIA_DUMMY;
            return;
        }
        if (mWidth != video_stream->width || mHeight != video_stream->height)
        {
            viewer->Release();
            mType |= MEDIA_DUMMY;
            return;
        }
        if (mEnd - mStart > duration)
        {
            viewer->Release();
            mType |= MEDIA_DUMMY;
            return;
        }

        mPath = info->url;
    }
}

void VideoClip::UpdateClip(MediaCore::Overview::Holder overview)
{
    if (overview)
    {
        mType = MEDIA_SUBTYPE_VIDEO_IMAGE;
        mOverview = overview;
        mMediaParser = overview->GetMediaParser();
        MediaCore::MediaInfo::Holder info = overview->GetMediaParser()->GetMediaInfo();
        const MediaCore::VideoStream* video_stream = mOverview->GetVideoStream();
        if (!info || !video_stream)
        {
            mOverview = nullptr;
            mType |= MEDIA_DUMMY;
            return;
        }
        if (mWidth != video_stream->width || mHeight != video_stream->height)
        {
            mOverview = nullptr;
            mType |= MEDIA_DUMMY;
            return;
        }
        int _width = 0, _height = 0;
        std::vector<ImGui::ImMat> snap_images;
        if (mOverview->GetSnapshots(snap_images))
        {
            TimeLine * timeline = (TimeLine *)mHandle;
            if (!snap_images.empty() && !snap_images[0].empty() && !mImgTexture && timeline && !timeline->m_in_threads)
            {
                ImMatToTexture(snap_images[0], mImgTexture);
                _width = snap_images[0].w;
                _height = snap_images[0].h;
            }
            if (mTrackHeight > 0 && _width > 0 && _height > 0)
            {
                mSnapHeight = mTrackHeight;
                mSnapWidth = mTrackHeight * _width / _height;
            }
        }

        mPath = info->url;
    }
}

Clip* VideoClip::Load(const imgui_json::value& value, void * handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    if (!timeline || timeline->media_items.size() <= 0)
        return nullptr;

    int64_t id = -1;
    int width = 0, height = 0;
    MediaItem * item = nullptr;
    if (value.contains("MediaID"))
    {
        auto& val = value["MediaID"];
        if (val.is_number()) id = val.get<imgui_json::number>();
    }
    if (id != -1)
    {
        item = timeline->FindMediaItemByID(id);
    }
    if (value.contains("width"))
    {
        auto& val = value["width"];
        if (val.is_number()) width = val.get<imgui_json::number>();
    }
    if (value.contains("height"))
    {
        auto& val = value["height"];
        if (val.is_number()) height = val.get<imgui_json::number>();
    }

    if (item)
    {
        // media is in bank
        VideoClip * new_clip = nullptr;
        if (!item->mValid)
        {
            new_clip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName, handle);
            if (IS_IMAGE(item->mMediaType))
                new_clip->mType |= MEDIA_SUBTYPE_VIDEO_IMAGE;
            else
                new_clip->mType |= MEDIA_VIDEO;
        }
        else if (IS_IMAGE(item->mMediaType))
        {
            new_clip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, handle);
        }
        else
        {
            MediaCore::Snapshot::Viewer::Holder hViewer;
            MediaCore::Snapshot::Generator::Holder hSsGen = timeline->GetSnapshotGenerator(item->mID);
            if (!hSsGen)
            {
                // TODO::Dicky create media error, need show dummy clip
                return nullptr;
            }

            hViewer = hSsGen->CreateViewer();
            new_clip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview->GetMediaParser(), hViewer, handle);
        }
        if (new_clip)
        {
            if ((width != 0 && new_clip->mWidth != width) || 
                (height != 0 && new_clip->mHeight != height))
            {
                new_clip->mType |= MEDIA_DUMMY;
                new_clip->mWidth = width;
                new_clip->mHeight = height;
            }
            Clip::Load(new_clip, value);
            // load video info
            if (value.contains("FrameRateDen"))
            {
                auto& val = value["FrameRateDen"];
                if (val.is_number()) new_clip->mClipFrameRate.den = val.get<imgui_json::number>();
            }
            if (value.contains("FrameRateNum"))
            {
                auto& val = value["FrameRateNum"];
                if (val.is_number()) new_clip->mClipFrameRate.num = val.get<imgui_json::number>();
            }
            if (value.contains("FrameRateDen"))
            {
                auto& val = value["FrameRateDen"];
                if (val.is_number()) new_clip->mClipFrameRate.den = val.get<imgui_json::number>();
            }
            if (value.contains("ScaleType"))
            {
                auto& val = value["ScaleType"];
                if (val.is_number()) new_clip->mScaleType = (MediaCore::ScaleType)val.get<imgui_json::number>();
            }
            if (value.contains("ScaleH"))
            {
                auto& val = value["ScaleH"];
                if (val.is_number()) new_clip->mScaleH = val.get<imgui_json::number>();
            }
            if (value.contains("ScaleV"))
            {
                auto& val = value["ScaleV"];
                if (val.is_number()) new_clip->mScaleV = val.get<imgui_json::number>();
            }
            if (value.contains("KeepAspectRatio"))
            {
                auto& val = value["KeepAspectRatio"];
                if (val.is_boolean()) new_clip->mKeepAspectRatio = val.get<imgui_json::boolean>();
            }
            if (value.contains("RotationAngle"))
            {
                auto& val = value["RotationAngle"];
                if (val.is_number()) new_clip->mRotationAngle = val.get<imgui_json::number>();
            }
            if (value.contains("PositionOffsetH"))
            {
                auto& val = value["PositionOffsetH"];
                if (val.is_number()) new_clip->mPositionOffsetH = val.get<imgui_json::number>();
            }
            if (value.contains("PositionOffsetV"))
            {
                auto& val = value["PositionOffsetV"];
                if (val.is_number()) new_clip->mPositionOffsetV = val.get<imgui_json::number>();
            }
            if (value.contains("CropMarginL"))
            {
                auto& val = value["CropMarginL"];
                if (val.is_number()) new_clip->mCropMarginL = val.get<imgui_json::number>();
            }
            if (value.contains("CropMarginT"))
            {
                auto& val = value["CropMarginT"];
                if (val.is_number()) new_clip->mCropMarginT = val.get<imgui_json::number>();
            }
            if (value.contains("CropMarginR"))
            {
                auto& val = value["CropMarginR"];
                if (val.is_number()) new_clip->mCropMarginR = val.get<imgui_json::number>();
            }
            if (value.contains("CropMarginB"))
            {
                auto& val = value["CropMarginB"];
                if (val.is_number()) new_clip->mCropMarginB = val.get<imgui_json::number>();
            }
            return new_clip;
        }
    }
    else
    {
        // media isn't in bank we need create new media item first ?
    }
    return nullptr;
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
    mClipViewStartPos = mStartOffset;
    if (millisec > mStart)
        mClipViewStartPos += millisec-mStart;
    if (!IS_DUMMY(mType) && !IS_IMAGE(mType))
    {
        if (!mSsViewer->GetSnapshots((double)mClipViewStartPos/1000, mSnapImages))
            throw std::runtime_error(mSsViewer->GetError());
        mSsViewer->UpdateSnapshotTexture(mSnapImages);
    }
}

void VideoClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect)
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
        return;
    }
    if (mImgTexture)
    {
        int _width = ImGui::ImGetTextureWidth(mImgTexture);
        int _height = ImGui::ImGetTextureHeight(mImgTexture);
        if (_width > 0 && _height > 0)
        {
            int trackHeight = rightBottom.y - leftTop.y;
            int snapHeight = trackHeight;
            int snapWidth = trackHeight * _width / _height;
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
                drawList->AddImage(mImgTexture, imgLeftTop, {imgLeftTop.x + snapDispWidth, rightBottom.y}, uvMin, uvMax);
                imgLeftTop.x += snapDispWidth;
                snapDispWidth = snapWidth;
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
            ImVec2 uvMin(0, 0), uvMax(1, 1);
            float snapDispWidth = img->mTimestampMs >= mClipViewStartPos ? mSnapWidth : mSnapWidth - (mClipViewStartPos - img->mTimestampMs) * mPixPerMs;
            if (img->mTimestampMs < mClipViewStartPos)
            {
                snapDispWidth = mSnapWidth - (mClipViewStartPos - img->mTimestampMs) * mPixPerMs;
                uvMin.x = 1 - snapDispWidth / mSnapWidth;
            }
            if (snapDispWidth <= 0)
                continue;
            if (snapLeftTop.x+snapDispWidth >= rightBottom.x)
            {
                snapDispWidth = rightBottom.x - snapLeftTop.x;
                uvMax.x = snapDispWidth / mSnapWidth;
            }
            if (img->mTextureReady)
            {
                ImTextureID tid = *(img->mTextureHolder);
                MediaCore::Snapshot::GetLogger()->Log(Logger::VERBOSE) << "[1]\t\t display tid=" << tid << std::endl;
                drawList->AddImage(tid, snapLeftTop, {snapLeftTop.x + snapDispWidth, rightBottom.y}, uvMin, uvMax);
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
    else if (mOverview)
    {
        int _width = 0, _height = 0;
        std::vector<ImGui::ImMat> snap_images;
        if (mOverview->GetSnapshots(snap_images))
        {
            if (!snap_images.empty() && !snap_images[0].empty())
            {
                ImMatToTexture(snap_images[0], mImgTexture);
                _width = snap_images[0].w;
                _height = snap_images[0].h;
            }
            if (mTrackHeight > 0 && _width > 0 && _height > 0)
            {
                mSnapHeight = mTrackHeight;
                mSnapWidth = mTrackHeight * _width / _height;
            }
        }
    }
}

void VideoClip::CalcDisplayParams()
{
    const MediaCore::VideoStream* video_stream = mMediaParser->GetBestVideoStream();
    mSnapHeight = mTrackHeight;
    MediaCore::Ratio displayAspectRatio = {
        (int32_t)(video_stream->width * video_stream->sampleAspectRatio.num), (int32_t)(video_stream->height * video_stream->sampleAspectRatio.den) };
    mSnapWidth = (float)mTrackHeight * displayAspectRatio.num / displayAspectRatio.den;
    // double windowSize = (double)mViewWndDur / 1000;
    // if (windowSize > video_stream->duration)
    //     windowSize = video_stream->duration;
    // mSnapsInViewWindow = std::max((float)((double)mPixPerMs*windowSize * 1000 / mSnapWidth), 1.f);
    // if (!mSnapshot->ConfigSnapWindow(windowSize, mSnapsInViewWindow))
    //     throw std::runtime_error(mSnapshot->GetError());
}

void VideoClip::Save(imgui_json::value& value)
{
    Clip::Save(value);
    // save video clip info
    value["width"] = imgui_json::number(mWidth);
    value["height"] = imgui_json::number(mHeight);
    value["FrameRateNum"] = imgui_json::number(mClipFrameRate.num);
    value["FrameRateDen"] = imgui_json::number(mClipFrameRate.den);

    value["CropMarginL"] = imgui_json::number(mCropMarginL);
    value["CropMarginT"] = imgui_json::number(mCropMarginT);
    value["CropMarginR"] = imgui_json::number(mCropMarginR);
    value["CropMarginB"] = imgui_json::number(mCropMarginB);

    value["PositionOffsetH"] = imgui_json::number(mPositionOffsetH);
    value["PositionOffsetV"] = imgui_json::number(mPositionOffsetV);

    value["ScaleH"] = imgui_json::number(mScaleH);
    value["ScaleV"] = imgui_json::number(mScaleV);
    value["ScaleType"] = imgui_json::number(mScaleType);
    value["KeepAspectRatio"] = imgui_json::boolean(mKeepAspectRatio);

    value["RotationAngle"] = imgui_json::number(mRotationAngle);
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// AudioClip Struct Member Functions
AudioClip::AudioClip(int64_t start, int64_t end, int64_t id, std::string name, MediaCore::Overview::Holder overview, void* handle)
    : Clip(start, end, id, overview->GetMediaParser(), handle), mOverview(overview)
{
    if (handle && mMediaParser)
    {
        mType = MEDIA_AUDIO;
        mName = name;
        MediaCore::MediaInfo::Holder info = mMediaParser->GetMediaInfo();
        const MediaCore::AudioStream* audio_stream = mMediaParser->GetBestAudioStream();
        if (!info || !audio_stream)
        {
            mType |= MEDIA_DUMMY;
            return;
        }
        mAudioSampleRate = audio_stream->sampleRate;
        mAudioChannels = audio_stream->channels;
        mPath = info->url;
        mWaveform = overview->GetWaveform();
    }
}

AudioClip::AudioClip(int64_t start, int64_t end, int64_t id, std::string name, void* handle)
    : Clip(start, end, id, nullptr, handle)
{
    mType = MEDIA_DUMMY | MEDIA_AUDIO;
    mName = name;
}

AudioClip::~AudioClip()
{
}

void AudioClip::UpdateClip(MediaCore::Overview::Holder overview, int64_t duration)
{
    if (overview)
    {
        mType = MEDIA_AUDIO;
        mOverview = overview;
        mMediaParser = overview->GetMediaParser();
        MediaCore::MediaInfo::Holder info = mMediaParser->GetMediaInfo();
        const MediaCore::AudioStream* audio_stream = mMediaParser->GetBestAudioStream();
        if (!info || !audio_stream)
        {
            mType |= MEDIA_DUMMY;
            return;
        }
        if (mAudioSampleRate != audio_stream->sampleRate ||
            mAudioChannels != audio_stream->channels)
        {
            mType |= MEDIA_DUMMY;
            return;
        }
        if (mEnd - mStart > duration)
        {
            mType |= MEDIA_DUMMY;
            return;
        }
        mPath = info->url;
        mWaveform = overview->GetWaveform();
    }
}

void AudioClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect)
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
    // TODO::Dicky opt more for audio waveform draw
    if (mWaveform->pcm.size() > 0)
    {
        std::string id_string = "##Waveform@" + std::to_string(mID);
        drawList->AddRectFilled(leftTop, rightBottom, IM_COL32(16, 16, 16, 255));
        drawList->AddRect(leftTop, rightBottom, IM_COL32_BLACK);
        float wave_range = fmax(fabs(mWaveform->minSample), fabs(mWaveform->maxSample));
        int sampleSize = mWaveform->pcm[0].size();
        int64_t start_time = std::max(mStart, timeline->firstTime);
        int64_t end_time = std::min(mEnd, timeline->lastTime);
        int start_offset = (int)((double)(mStartOffset) / 1000.f / mWaveform->aggregateDuration);
        if (mStart < timeline->firstTime)
            start_offset = (int)((double)(timeline->firstTime - mStart + mStartOffset) / 1000.f / mWaveform->aggregateDuration);
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
            ImGui::PushClipRect(leftTop, rightBottom, true);
#if AUDIO_WAVEFORM_IMPLOT
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
#else
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
#endif
            drawList->PopClipRect();
        }        
    }
}

Clip * AudioClip::Load(const imgui_json::value& value, void * handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    if (!timeline || timeline->media_items.size() <= 0)
        return nullptr;
    
    int64_t id = -1;
    MediaItem * item = nullptr;
    if (value.contains("MediaID"))
    {
        auto& val = value["MediaID"];
        if (val.is_number()) id = val.get<imgui_json::number>();
    }
    if (id != -1)
    {
        item = timeline->FindMediaItemByID(id);
    }
    if (item)
    {
        AudioClip * new_clip = nullptr;
        if (item->mValid)
            new_clip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, handle);
        else
            new_clip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName, handle);
        if (new_clip)
        {
            Clip::Load(new_clip, value);
            // load audio info
            if (value.contains("Channels"))
            {
                auto& val = value["Channels"];
                if (val.is_number()) new_clip->mAudioChannels = val.get<imgui_json::number>();
            }
            if (value.contains("SampleRate"))
            {
                auto& val = value["SampleRate"];
                if (val.is_number()) new_clip->mAudioSampleRate = val.get<imgui_json::number>();
            }
            if (value.contains("Format"))
            {
                auto& val = value["Format"];
                if (val.is_number()) new_clip->mAudioFormat = (MediaCore::AudioRender::PcmFormat)val.get<imgui_json::number>();
            }
            return new_clip;
        }
    }
    else
    {
        // media isn't in bank we need create new media item first ?
    }
    return nullptr;
}

void AudioClip::Save(imgui_json::value& value)
{
    Clip::Save(value);
    // save audio clip info
    value["Channels"] = imgui_json::number(mAudioChannels);
    value["SampleRate"] = imgui_json::number(mAudioSampleRate);
    value["Format"] = imgui_json::number(mAudioFormat);
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// TextClip Struct Member Functions
TextClip::TextClip(int64_t start, int64_t end, int64_t id, std::string name, std::string text, void* handle)
    : Clip(start, end, id, nullptr, handle)
{
    if (handle)
    {
        mType = MEDIA_TEXT;
        mName = name;
        mText = text;
        mTrackStyle = true;
    }
}

TextClip::TextClip(int64_t start, int64_t end, int64_t id, std::string name, void* handle)
    : Clip(start, end, id, nullptr, handle)
{
    mType = MEDIA_DUMMY | MEDIA_TEXT;
    mName = name;
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
    if (mClipHolder)
    {
        mClipHolder->SetFont(mFontName);
        mClipHolder->SetOffsetH(mFontOffsetH);
        mClipHolder->SetOffsetV(mFontOffsetV);
        mClipHolder->SetScaleX(mFontScaleX);
        mClipHolder->SetScaleY(mFontScaleY);
        mClipHolder->SetSpacing(mFontSpacing);
        mClipHolder->SetRotationX(mFontAngleX);
        mClipHolder->SetRotationY(mFontAngleY);
        mClipHolder->SetRotationZ(mFontAngleZ);
        mClipHolder->SetBorderWidth(mFontOutlineWidth);
        mClipHolder->SetBold(mFontBold);
        mClipHolder->SetItalic(mFontItalic);
        mClipHolder->SetUnderLine(mFontUnderLine);
        mClipHolder->SetStrikeOut(mFontStrikeOut);
        mClipHolder->SetAlignment(mFontAlignment);
        mClipHolder->SetShadowDepth(mFontShadowDepth);
        mClipHolder->SetPrimaryColor(mFontPrimaryColor);
        mClipHolder->SetOutlineColor(mFontOutlineColor);
        mClipHolder->SetBackColor(mFontBackColor);
    }
}

void TextClip::EnableUsingTrackStyle(bool enable)
{
    if (mTrackStyle != enable)
    {
        mTrackStyle = enable;
        if (mClipHolder)
        {
            if (!enable)
                SyncClipAttributes();
            mClipHolder->EnableUsingTrackStyle(enable);
        }
    }
}

void TextClip::CreateClipHold(void * _track)
{
    MediaTrack * track = (MediaTrack *)_track;
    if (!track || !track->mMttReader)
        return;
    mClipHolder = track->mMttReader->NewClip(mStart, mEnd - mStart);
    mTrack = track;
    mMediaID = track->mID;
    mName = track->mName;
    mClipHolder->SetText(mText);
    mClipHolder->EnableUsingTrackStyle(mTrackStyle);
    if (!mIsInited)
    {
        mFontName = mClipHolder->Font();
        mFontOffsetH = mClipHolder->OffsetHScale();
        mFontOffsetV = mClipHolder->OffsetVScale();
        mFontScaleX = mClipHolder->ScaleX();
        mFontScaleY = mClipHolder->ScaleY();
        mFontSpacing = mClipHolder->Spacing();
        mFontAngleX = mClipHolder->RotationX();
        mFontAngleY = mClipHolder->RotationY();
        mFontAngleZ = mClipHolder->RotationZ();
        mFontOutlineWidth = mClipHolder->BorderWidth();
        mFontBold = mClipHolder->Bold();
        mFontItalic = mClipHolder->Italic();
        mFontUnderLine = mClipHolder->UnderLine();
        mFontStrikeOut = mClipHolder->StrikeOut();
        mFontAlignment = mClipHolder->Alignment();
        mFontShadowDepth = mClipHolder->ShadowDepth();
        mFontPrimaryColor = mClipHolder->PrimaryColor().ToImVec4();
        mFontOutlineColor = mClipHolder->OutlineColor().ToImVec4();
        mFontBackColor = mClipHolder->BackColor().ToImVec4();
        mIsInited = true;
    }
    else if (!mTrackStyle)
    {
        SyncClipAttributes();
    }
    mClipHolder->SetKeyPoints(mAttributeKeyPoints);
}

int64_t TextClip::Moving(int64_t diff, int mouse_track)
{
    auto ret = Clip::Moving(diff, mouse_track);
    MediaTrack * track = (MediaTrack*)mTrack;
    if (track && track->mMttReader)
        track->mMttReader->ChangeClipTime(mClipHolder, mStart, mEnd - mStart);
    return ret;
}

int64_t TextClip::Cropping(int64_t diff, int type)
{
    auto ret = Clip::Cropping(diff, type);
    MediaTrack * track = (MediaTrack*)mTrack;
    if (track && track->mMttReader)
        track->mMttReader->ChangeClipTime(mClipHolder, mStart, mEnd - mStart);
    return ret;
}

void TextClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect)
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

Clip * TextClip::Load(const imgui_json::value& value, void * handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    if (!timeline)
        return nullptr;
    TextClip * new_clip = new TextClip(0, 0, 0, std::string(""), std::string(""), handle);
    if (!new_clip)
        return nullptr;

    Clip::Load(new_clip, value);
    // load text info
    if (value.contains("Text"))
    {
        auto& val = value["Text"];
        if (val.is_string()) new_clip->mText = val.get<imgui_json::string>();
    }
    if (value.contains("TrackStyle"))
    {
        auto& val = value["TrackStyle"];
        if (val.is_boolean()) new_clip->mTrackStyle = val.get<imgui_json::boolean>();
    }
    if (value.contains("Name"))
    {
        auto& val = value["Name"];
        if (val.is_string()) new_clip->mFontName = val.get<imgui_json::string>();
    }
    if (value.contains("ScaleLink"))
    {
        auto& val = value["ScaleLink"];
        if (val.is_boolean()) new_clip->mScaleSettingLink = val.get<imgui_json::boolean>();
    }
    if (value.contains("ScaleX"))
    {
        auto& val = value["ScaleX"];
        if (val.is_number()) new_clip->mFontScaleX = val.get<imgui_json::number>();
    }
    if (value.contains("ScaleY"))
    {
        auto& val = value["ScaleY"];
        if (val.is_number()) new_clip->mFontScaleY = val.get<imgui_json::number>();
    }
    if (value.contains("Spacing"))
    {
        auto& val = value["Spacing"];
        if (val.is_number()) new_clip->mFontSpacing = val.get<imgui_json::number>();
    }
    if (value.contains("AngleX"))
    {
        auto& val = value["AngleX"];
        if (val.is_number()) new_clip->mFontAngleX = val.get<imgui_json::number>();
    }
    if (value.contains("AngleY"))
    {
        auto& val = value["AngleY"];
        if (val.is_number()) new_clip->mFontAngleY = val.get<imgui_json::number>();
    }
    if (value.contains("AngleZ"))
    {
        auto& val = value["AngleZ"];
        if (val.is_number()) new_clip->mFontAngleZ = val.get<imgui_json::number>();
    }
    if (value.contains("OutlineWidth"))
    {
        auto& val = value["OutlineWidth"];
        if (val.is_number()) new_clip->mFontOutlineWidth = val.get<imgui_json::number>();
    }
    if (value.contains("Alignment"))
    {
        auto& val = value["Alignment"];
        if (val.is_number()) new_clip->mFontAlignment = val.get<imgui_json::number>();
    }
    if (value.contains("Bold"))
    {
        auto& val = value["Bold"];
        if (val.is_boolean()) new_clip->mFontBold = val.get<imgui_json::boolean>();
    }
    if (value.contains("Italic"))
    {
        auto& val = value["Italic"];
        if (val.is_boolean()) new_clip->mFontItalic = val.get<imgui_json::boolean>();
    }
    if (value.contains("UnderLine"))
    {
        auto& val = value["UnderLine"];
        if (val.is_boolean()) new_clip->mFontUnderLine = val.get<imgui_json::boolean>();
    }
    if (value.contains("StrikeOut"))
    {
        auto& val = value["StrikeOut"];
        if (val.is_boolean()) new_clip->mFontStrikeOut = val.get<imgui_json::boolean>();
    }
    if (value.contains("OffsetX"))
    {
        auto& val = value["OffsetX"];
        if (val.is_number()) new_clip->mFontOffsetH = val.get<imgui_json::number>();
    }
    if (value.contains("OffsetY"))
    {
        auto& val = value["OffsetY"];
        if (val.is_number()) new_clip->mFontOffsetV = val.get<imgui_json::number>();
    }
    if (value.contains("ShadowDepth"))
    {
        auto& val = value["ShadowDepth"];
        if (val.is_number()) new_clip->mFontShadowDepth = val.get<imgui_json::number>();
    }
    if (value.contains("PrimaryColor"))
    {
        auto& val = value["PrimaryColor"];
        if (val.is_vec4()) new_clip->mFontPrimaryColor = val.get<imgui_json::vec4>();
    }
    if (value.contains("OutlineColor"))
    {
        auto& val = value["OutlineColor"];
        if (val.is_vec4()) new_clip->mFontOutlineColor = val.get<imgui_json::vec4>();
    }
    if (value.contains("BackColor"))
    {
        auto& val = value["BackColor"];
        if (val.is_vec4()) new_clip->mFontBackColor = val.get<imgui_json::vec4>();
    }

    new_clip->mIsInited = true;
    return new_clip;
}

void TextClip::Save(imgui_json::value& value)
{
    Clip::Save(value);
    // save Text clip info
    value["Text"] = mText;
    value["TrackStyle"] = imgui_json::boolean(mTrackStyle);
    value["Name"] = mFontName;
    value["ScaleLink"] = imgui_json::boolean(mScaleSettingLink);
    value["ScaleX"] = imgui_json::number(mFontScaleX);
    value["ScaleY"] = imgui_json::number(mFontScaleY);
    value["Spacing"] = imgui_json::number(mFontSpacing);
    value["AngleX"] = imgui_json::number(mFontAngleX);
    value["AngleY"] = imgui_json::number(mFontAngleY);
    value["AngleZ"] = imgui_json::number(mFontAngleZ);
    value["OutlineWidth"] = imgui_json::number(mFontOutlineWidth);
    value["Alignment"] = imgui_json::number(mFontAlignment);
    value["Bold"] = imgui_json::boolean(mFontBold);
    value["Italic"] = imgui_json::boolean(mFontItalic);
    value["UnderLine"] = imgui_json::boolean(mFontUnderLine);
    value["StrikeOut"] = imgui_json::boolean(mFontStrikeOut);
    value["OffsetX"] = imgui_json::number(mFontOffsetH);
    value["OffsetY"] = imgui_json::number(mFontOffsetV);
    value["ShadowDepth"] = imgui_json::number(mFontShadowDepth);
    value["PrimaryColor"] = imgui_json::vec4(mFontPrimaryColor);
    value["OutlineColor"] = imgui_json::vec4(mFontOutlineColor);
    value["BackColor"] = imgui_json::vec4(mFontBackColor);
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// BluePrintVideoFilter class
BluePrintVideoFilter::BluePrintVideoFilter(void * handle)
    : mHandle(handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    imgui_json::value filter_BP; 
    mBp = new BluePrint::BluePrintUI();
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    mBp->Initialize(nullptr, timeline ? timeline->mPluginPath.c_str() : nullptr);
    mBp->SetCallbacks(callbacks, this);
    mBp->File_New_Filter(filter_BP, "VideoFilter", "Video");
}

BluePrintVideoFilter::~BluePrintVideoFilter()
{
    if (mBp) 
    {
        mBp->Finalize(); 
        delete mBp;
    }
}

int BluePrintVideoFilter::OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (!handle)
        return BluePrint::BP_CBR_Unknown;
    BluePrintVideoFilter * filter = (BluePrintVideoFilter *)handle;
    if (!filter) return ret;
    TimeLine * timeline = (TimeLine *)filter->mHandle;
    if (name.compare("VideoFilter") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            if (timeline) timeline->UpdatePreview();
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            if (timeline) timeline->UpdatePreview();
        }
    }
    return ret;
}

MediaCore::VideoFilter::Holder BluePrintVideoFilter::Clone()
{
    BluePrintVideoFilter* bpFilter = new BluePrintVideoFilter(mHandle);
    auto bpJson = mBp->m_Document->Serialize();
    bpFilter->SetBluePrintFromJson(bpJson);
    bpFilter->SetKeyPoint(mKeyPoints);
    return MediaCore::VideoFilter::Holder(bpFilter);
}

ImGui::ImMat BluePrintVideoFilter::FilterImage(const ImGui::ImMat& vmat, int64_t pos)
{
    std::lock_guard<std::mutex> lk(mBpLock);
    ImGui::ImMat outMat(vmat);
    if (mBp)
    {
        // setup bp input curve
        for (int i = 0; i < mKeyPoints.GetCurveCount(); i++)
        {
            auto name = mKeyPoints.GetCurveName(i);
            auto value = mKeyPoints.GetValue(i, pos);
            mBp->Blueprint_SetFilter(name, value);
        }
        ImGui::ImMat inMat(vmat);
        mBp->Blueprint_RunFilter(inMat, outMat);
    }
    return outMat;
}

void BluePrintVideoFilter::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    // Logger::Log(Logger::DEBUG) << "Create bp filter from json " << bpJson.dump() << std::endl;
    mBp->File_New_Filter(bpJson, "VideoFilter", "Video");
    if (!mBp->Blueprint_IsValid())
    {
        mBp->Finalize();
        return;
    }
}
} // namespace MediaTimeline

namespace MediaTimeline
{
// BluePrintVideoTransition class
BluePrintVideoTransition::BluePrintVideoTransition(void * handle)
    : mHandle(handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    imgui_json::value fusion_BP; 
    mBp = new BluePrint::BluePrintUI();
    mBp->Initialize(nullptr, timeline ? timeline->mPluginPath.c_str() : nullptr);
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    mBp->SetCallbacks(callbacks, this);
    mBp->File_New_Fusion(fusion_BP, "VideoFusion", "Video");
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
    BluePrintVideoTransition * fusion = (BluePrintVideoTransition *)handle;
    if (!fusion) return ret;
    TimeLine * timeline = (TimeLine *)fusion->mHandle;
    if (name.compare("VideoFusion") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            // need update
            if (timeline) timeline->UpdatePreview();
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            // need update
            if (timeline) timeline->UpdatePreview();
        }
    }
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

ImGui::ImMat BluePrintVideoTransition::MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur)
{
    std::lock_guard<std::mutex> lk(mBpLock);
    if (mBp)
    {
        // setup bp input curve
        for (int i = 0; i < mKeyPoints.GetCurveCount(); i++)
        {
            auto name = mKeyPoints.GetCurveName(i);
            auto value = mKeyPoints.GetValue(i, pos - mOverlap->Start());
            mBp->Blueprint_SetFusion(name, value);
        }
        ImGui::ImMat inMat1(vmat1), inMat2(vmat2);
        ImGui::ImMat outMat;
        mBp->Blueprint_RunFusion(inMat1, inMat2, outMat, pos - mOverlap->Start(), dur);
        return outMat;
    }
    return vmat1;
}

void BluePrintVideoTransition::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    // Logger::Log(Logger::DEBUG) << "Create bp transition from json " << bpJson.dump() << std::endl;
    mBp->File_New_Fusion(bpJson, "VideoFusion", "Video");
    if (!mBp->Blueprint_IsValid())
    {
        mBp->Finalize();
        return;
    }
}
} // namespace MediaTimeline

namespace MediaTimeline
{
BluePrintAudioFilter::BluePrintAudioFilter(void * handle)
    : mHandle(handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    imgui_json::value filter_BP; 
    mBp = new BluePrint::BluePrintUI();
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    mBp->Initialize(nullptr, timeline ? timeline->mPluginPath.c_str() : nullptr);
    mBp->SetCallbacks(callbacks, this);
    mBp->File_New_Filter(filter_BP, "AudioFilter", "Audio");
}

BluePrintAudioFilter::~BluePrintAudioFilter()
{
    if (mBp) { mBp->Finalize();  delete mBp; mBp = nullptr; }
}

int BluePrintAudioFilter::OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (!handle)
        return BluePrint::BP_CBR_Unknown;
    BluePrintAudioFilter * filter = (BluePrintAudioFilter *)handle;
    if (!filter) return ret;
    TimeLine * timeline = (TimeLine *)filter->mHandle;
    if (name.compare("AudioFilter") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            if (timeline) timeline->UpdatePreview();
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            if (timeline) timeline->UpdatePreview();
        }
    }
    return ret;
}

ImGui::ImMat BluePrintAudioFilter::FilterPcm(const ImGui::ImMat& amat, int64_t pos)
{
    std::lock_guard<std::mutex> lk(mBpLock);
    ImGui::ImMat outMat(amat);
    if (mBp)
    {
        // setup bp input curve
        for (int i = 0; i < mKeyPoints.GetCurveCount(); i++)
        {
            auto name = mKeyPoints.GetCurveName(i);
            auto value = mKeyPoints.GetValue(i, pos);
            mBp->Blueprint_SetFilter(name, value);
        }
        ImGui::ImMat inMat(amat);
        mBp->Blueprint_RunFilter(inMat, outMat);
    }
    return outMat;
}

void BluePrintAudioFilter::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    // Logger::Log(Logger::DEBUG) << "Create bp filter from json " << bpJson.dump() << std::endl;
    mBp->File_New_Filter(bpJson, "AudioFilter", "Audio");
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
    imgui_json::value fusion_BP; 
    mBp = new BluePrint::BluePrintUI();
    mBp->Initialize(nullptr, timeline ? timeline->mPluginPath.c_str() : nullptr);
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    mBp->SetCallbacks(callbacks, this);
    mBp->File_New_Fusion(fusion_BP, "AudioFusion", "Audio");
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
    BluePrintAudioTransition * fusion = (BluePrintAudioTransition *)handle;
    if (!fusion) return ret;
    TimeLine * timeline = (TimeLine *)fusion->mHandle;
    if (name.compare("AudioFusion") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            // need update
            if (timeline) timeline->UpdatePreview();
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            // need update
            //if (timeline) timeline->UpdatePreview();
        }
    }
    return ret;
}

ImGui::ImMat BluePrintAudioTransition::MixTwoAudioMats(const ImGui::ImMat& amat1, const ImGui::ImMat& amat2, int64_t pos)
{
    std::lock_guard<std::mutex> lk(mBpLock);
    if (mBp)
    {
        // setup bp input curve
        for (int i = 0; i < mKeyPoints.GetCurveCount(); i++)
        {
            auto name = mKeyPoints.GetCurveName(i);
            auto value = mKeyPoints.GetValue(i, pos - mOverlap->Start());
            mBp->Blueprint_SetFusion(name, value);
        }
        ImGui::ImMat inMat1(amat1), inMat2(amat2);
        ImGui::ImMat outMat;
        mBp->Blueprint_RunFusion(inMat1, inMat2, outMat, pos - mOverlap->Start(), mOverlap->End() - mOverlap->Start());
        return outMat;
    }
    return amat1;
}

void BluePrintAudioTransition::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    // Logger::Log(Logger::DEBUG) << "Create bp transition from json " << bpJson.dump() << std::endl;
    mBp->File_New_Fusion(bpJson, "AudioFusion", "Audio");
    if (!mBp->Blueprint_IsValid())
    {
        mBp->Finalize();
        return;
    }
}

} // namespace MediaTimeline

namespace MediaTimeline
{
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
        if (mEnd - currentTime < visibleTime / 2)
        {
            firstTime = mEnd - visibleTime;
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
    : BaseEditingClip(vidclip->mID, vidclip->mType, vidclip->mStart, vidclip->mEnd, vidclip->mStartOffset, vidclip->mEndOffset, vidclip->mHandle)
{
    TimeLine * timeline = (TimeLine *)vidclip->mHandle;
    mDuration = mEnd-mStart;
    mClipFrameRate = vidclip->mClipFrameRate;
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
        mImgTexture = vidclip->mImgTexture;
    }
    else
    {
        mSsGen = MediaCore::Snapshot::Generator::CreateInstance();
        if (!mSsGen)
        {
            Logger::Log(Logger::Error) << "Create Editing Video Clip FAILED!" << std::endl;
            return;
        }
        if (timeline) mSsGen->EnableHwAccel(timeline->mHardwareCodec);
        if (!mSsGen->Open(vidclip->mSsViewer->GetMediaParser()))
        {
            Logger::Log(Logger::Error) << mSsGen->GetError() << std::endl;
            return;
        }

        mSsGen->SetCacheFactor(1);
        float snapshot_scale = mHeight > 0 ? 50.f / (float)mHeight : 0.05;
        mSsGen->SetSnapshotResizeFactor(snapshot_scale, snapshot_scale);
        mSsViewer = mSsGen->CreateViewer((double)mStartOffset / 1000);
    }

    auto hClip = timeline->mMtvReader->GetClipById(vidclip->mID);
    IM_ASSERT(hClip);
    mFilter = dynamic_cast<BluePrintVideoFilter *>(hClip->GetFilter().get());
    if (!mFilter)
    {
        mFilter = new BluePrintVideoFilter(timeline);
        mFilter->SetKeyPoint(vidclip->mFilterKeyPoints);
        MediaCore::VideoFilter::Holder hFilter(mFilter);
        hClip->SetFilter(hFilter);
    }
    mAttribute = hClip->GetTransformFilter();
    if (mAttribute)
    {
        mAttribute->SetScaleType(vidclip->mScaleType);
        mAttribute->SetScaleH(vidclip->mScaleH);
        mAttribute->SetScaleV(vidclip->mScaleV);
        mAttribute->SetPositionOffsetH(vidclip->mPositionOffsetH);
        mAttribute->SetPositionOffsetV(vidclip->mPositionOffsetV);
        mAttribute->SetRotationAngle(vidclip->mRotationAngle);
        mAttribute->SetCropMarginL(vidclip->mCropMarginL);
        mAttribute->SetCropMarginT(vidclip->mCropMarginT);
        mAttribute->SetCropMarginR(vidclip->mCropMarginR);
        mAttribute->SetCropMarginB(vidclip->mCropMarginB);
        mAttribute->SetKeyPoint(vidclip->mAttributeKeyPoints);
    }
}

EditingVideoClip::~EditingVideoClip()
{
    mSsViewer = nullptr;
    mSsGen = nullptr;
    mFilter = nullptr;
}

void EditingVideoClip::UpdateClipRange(Clip* clip)
{
    if (mStart != clip->mStart)
        mStart = clip->mStart;
    if (mEnd != clip->mEnd)
        mEnd = clip->mEnd;
    if (mStartOffset != clip->mStartOffset || mEndOffset != clip->mEndOffset)
    {
        mStartOffset = clip->mStartOffset;
        mEndOffset = clip->mEndOffset;
        mDuration = mEnd - mStart;
    }
    
}

void EditingVideoClip::Seek(int64_t pos)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    timeline->Seek(pos);
}

void EditingVideoClip::Step(bool forward, int64_t step)
{
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
    if (mFilter && mFilter->mBp && mFilter->mBp->Blueprint_IsValid())
    {
        clip->mFilterBP = mFilter->mBp->m_Document->Serialize();
        clip->mFilterKeyPoints = mFilter->mKeyPoints;
    }
    if (mAttribute)
    {
        clip->mAttributeKeyPoints = *mAttribute->GetKeyPoint();
        clip->mScaleType = mAttribute->GetScaleType();
        clip->mScaleH = mAttribute->GetScaleH();
        clip->mScaleV = mAttribute->GetScaleV();
        clip->mRotationAngle = mAttribute->GetRotationAngle();
        clip->mPositionOffsetH = mAttribute->GetPositionOffsetHScale();
        clip->mPositionOffsetV = mAttribute->GetPositionOffsetVScale();
        clip->mCropMarginL = mAttribute->GetCropMarginLScale();
        clip->mCropMarginT = mAttribute->GetCropMarginTScale();
        clip->mCropMarginR = mAttribute->GetCropMarginRScale();
        clip->mCropMarginB = mAttribute->GetCropMarginBScale();
    }
    timeline->UpdatePreview();
}

bool EditingVideoClip::GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame, bool preview_frame, bool attribute)
{
    int ret = true;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return false;
    auto frames = timeline->GetPreviewFrame();
    ImGui::ImMat frame_org;
    auto iter = std::find_if(frames.begin(), frames.end(), [this] (auto& cf) {
        return cf.clipId == mID && cf.phase == MediaCore::CorrelativeFrame::PHASE_SOURCE_FRAME;
    });
    if (iter != frames.end())
        frame_org = iter->frame;
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
        auto iter_out = std::find_if(frames.begin(), frames.end(), [&] (auto& cf) {
            return cf.clipId == mID && cf.phase == (attribute ? MediaCore::CorrelativeFrame::PHASE_AFTER_TRANSFORM : MediaCore::CorrelativeFrame::PHASE_AFTER_FILTER);
        });
        if (iter_out != frames.end())
            in_out_frame.second = iter_out->frame;
        else
            ret = false;
    }
    in_out_frame.first = frame_org;
    return ret;
}

void EditingVideoClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom)
{
    if (mImgTexture)
    {
        int _width = ImGui::ImGetTextureWidth(mImgTexture);
        int _height = ImGui::ImGetTextureHeight(mImgTexture);
        if (_width > 0 && _height > 0)
        {
            int trackHeight = rightBottom.y - leftTop.y;
            int snapHeight = trackHeight;
            int snapWidth = trackHeight * _width / _height;
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
                drawList->AddImage(mImgTexture, imgLeftTop, {imgLeftTop.x + snapDispWidth, rightBottom.y}, uvMin, uvMax);
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

        std::vector<MediaCore::Snapshot::Image::Holder> snapImages;
        if (!mSsViewer->GetSnapshots((double)(mStartOffset + firstTime) / 1000, snapImages))
        {
            Logger::Log(Logger::Error) << mSsViewer->GetError() << std::endl;
            return;
        }
        mSsViewer->UpdateSnapshotTexture(snapImages);

        ImVec2 imgLeftTop = leftTop;
        for (int i = 0; i < snapImages.size(); i++)
        {
            auto& img = snapImages[i];
            ImVec2 uvMin{0, 0}, uvMax{1, 1};
            float snapDispWidth = img->mTimestampMs >= mStartOffset + firstTime ? mSnapSize.x : mSnapSize.x - (mStartOffset + firstTime - img->mTimestampMs) * msPixelWidthTarget;
            if (img->mTimestampMs < mStartOffset + firstTime)
            {
                snapDispWidth = mSnapSize.x - (mStartOffset + firstTime - img->mTimestampMs) * msPixelWidthTarget;
                uvMin.x = 1 - snapDispWidth / mSnapSize.x;
            }
            if (snapDispWidth <= 0)
                continue;
            if (imgLeftTop.x + snapDispWidth >= rightBottom.x)
            {
                snapDispWidth = rightBottom.x - imgLeftTop.x;
                uvMax.x = snapDispWidth / mSnapSize.x;
            }

            if (img->mTextureReady)
                drawList->AddImage(*(img->mTextureHolder), imgLeftTop, {imgLeftTop.x + snapDispWidth, rightBottom.y}, uvMin, uvMax);
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
} // namespace MediaTimeline

namespace MediaTimeline
{
EditingAudioClip::EditingAudioClip(AudioClip* audclip)
    : BaseEditingClip(audclip->mID, audclip->mType, audclip->mStart, audclip->mEnd, audclip->mStartOffset, audclip->mEndOffset, audclip->mHandle)
{
    TimeLine * timeline = (TimeLine *)audclip->mHandle;
    mDuration = mEnd-mStart;
    mAudioChannels = audclip->mAudioChannels;
    mAudioSampleRate = audclip->mAudioSampleRate;
    mAudioFormat = audclip->mAudioFormat;
    mWaveform = audclip->mWaveform;
    firstTime = audclip->firstTime;
    lastTime = audclip->lastTime;
    visibleTime = audclip->visibleTime;
    msPixelWidthTarget = audclip->msPixelWidthTarget;
    if (mDuration < 0)
        throw std::invalid_argument("Clip duration is negative!");
    auto hClip = timeline->mMtaReader->GetClipById(audclip->mID);
    IM_ASSERT(hClip);
    mFilter = dynamic_cast<BluePrintAudioFilter *>(hClip->GetFilter().get());
    if (!mFilter)
    {
        mFilter = new BluePrintAudioFilter(timeline);
        mFilter->SetKeyPoint(audclip->mFilterKeyPoints);
        MediaCore::AudioFilter::Holder hFilter(mFilter);
        hClip->SetFilter(hFilter);
    }
}

EditingAudioClip::~EditingAudioClip()
{}

void EditingAudioClip::UpdateClipRange(Clip* clip)
{}

void EditingAudioClip::Seek(int64_t pos)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    timeline->Seek(pos);
}

void EditingAudioClip::Step(bool forward, int64_t step)
{}

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
    if (mFilter && mFilter->mBp && mFilter->mBp->Blueprint_IsValid())
    {
        clip->mFilterBP = mFilter->mBp->m_Document->Serialize();
        clip->mFilterKeyPoints = mFilter->mKeyPoints;
    }
}

bool EditingAudioClip::GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame, bool preview_frame, bool attribute)
{
    return false;
}

void EditingAudioClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom)
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
    int start_offset = (int)((double)(clip->mStartOffset + firstTime) / 1000.f / waveform->aggregateDuration);
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
#if AUDIO_WAVEFORM_IMPLOT
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
#else
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
    mFusionKeyPoints.SetMin({0.f, 0.f});
    mFusionKeyPoints.SetMax(ImVec2(end - start, 1.f), true);
}

Overlap::~Overlap()
{
}

bool Overlap::IsOverlapValid()
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
    
    if (clip_start->mEnd <= clip_end->mStart ||
        clip_end->mEnd <= clip_start->mStart)
        return false;
    
    return true;
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
            auto fusion = dynamic_cast<BluePrintVideoTransition *>(hOvlp->GetTransition().get());
            if (fusion)
                fusion->mKeyPoints.SetMax(ImVec2(mEnd - mStart, 1.f), true);
        }
    }
    else if (IS_AUDIO(mType))
    {
        auto hOvlp = timeline->mMtaReader->GetOverlapById(mID);
        if (hOvlp)
        {
            auto fusion = dynamic_cast<BluePrintAudioTransition *>(hOvlp->GetTransition().get());
            if (fusion)
                fusion->mKeyPoints.SetMax(ImVec2(mEnd - mStart, 1.f), true);
        }
    }
    mFusionKeyPoints.SetMax(ImVec2(mEnd - mStart, 1.f), true);
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
        if (value.contains("Editing"))
        {
            auto& val = value["Editing"];
            if (val.is_boolean()) new_overlap->bEditing = val.get<imgui_json::boolean>();
        }
        // load fusion bp
        if (value.contains("FusionBP"))
        {
            auto& val = value["FusionBP"];
            if (val.is_object()) new_overlap->mFusionBP = val;
        }
        // load curve
        if (value.contains("KeyPoint"))
        {
            auto& keypoint = value["KeyPoint"];
            new_overlap->mFusionKeyPoints.Load(keypoint);
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
    value["Editing"] = imgui_json::boolean(bEditing);

    // save overlap fusion bp
    if (mFusionBP.is_object())
    {
        value["FusionBP"] = mFusionBP;
    }

    // save curve setting
    imgui_json::value keypoint;
    mFusionKeyPoints.Save(keypoint);
    value["KeyPoint"] = keypoint;
}
} // namespace MediaTimeline

namespace MediaTimeline
{
EditingVideoOverlap::EditingVideoOverlap(Overlap* ovlp)
    : BaseEditingOverlap(ovlp)
{
    TimeLine* timeline = (TimeLine*)(ovlp->mHandle);
    VideoClip* vidclip1 = (VideoClip*)timeline->FindClipByID(ovlp->m_Clip.first);
    VideoClip* vidclip2 = (VideoClip*)timeline->FindClipByID(ovlp->m_Clip.second);
    if (vidclip1 && vidclip2)
    {
        mClip1 = vidclip1; mClip2 = vidclip2;
        if (IS_IMAGE(mClip1->mType))
        {
            m_StartOffset.first = ovlp->mStart - vidclip1->mStart;
            if (vidclip1->mImgTexture) mImgTexture1 = vidclip1->mImgTexture;
        }
        else
        {
            mSsGen1 = MediaCore::Snapshot::Generator::CreateInstance();
            if (timeline) mSsGen1->EnableHwAccel(timeline->mHardwareCodec);
            if (!mSsGen1->Open(vidclip1->mSsViewer->GetMediaParser()))
                throw std::runtime_error("FAILED to open the snapshot generator for the 1st video clip!");
            auto video1_info = vidclip1->mSsViewer->GetMediaParser()->GetBestVideoStream();
            float snapshot_scale1 = video1_info->height > 0 ? 50.f / (float)video1_info->height : 0.05;
            mSsGen1->SetCacheFactor(1.0);
            mSsGen1->SetSnapshotResizeFactor(snapshot_scale1, snapshot_scale1);
            m_StartOffset.first = vidclip1->mStartOffset + ovlp->mStart - vidclip1->mStart;
            mViewer1 = mSsGen1->CreateViewer(m_StartOffset.first);
        }

        if (IS_IMAGE(mClip2->mType))
        {
            m_StartOffset.second = ovlp->mStart - vidclip2->mStart;
            if (vidclip2->mImgTexture) mImgTexture2 = vidclip2->mImgTexture;
        }
        else
        {
            mSsGen2 = MediaCore::Snapshot::Generator::CreateInstance();
            if (timeline) mSsGen2->EnableHwAccel(timeline->mHardwareCodec);
            if (!mSsGen2->Open(vidclip2->mSsViewer->GetMediaParser()))
                throw std::runtime_error("FAILED to open the snapshot generator for the 2nd video clip!");
            auto video2_info = vidclip2->mSsViewer->GetMediaParser()->GetBestVideoStream();
            float snapshot_scale2 = video2_info->height > 0 ? 50.f / (float)video2_info->height : 0.05;
            mSsGen2->SetCacheFactor(1.0);
            mSsGen2->SetSnapshotResizeFactor(snapshot_scale2, snapshot_scale2);
            m_StartOffset.second = vidclip2->mStartOffset + ovlp->mStart - vidclip2->mStart;
            mViewer2 = mSsGen2->CreateViewer(m_StartOffset.second);
        }
        mStart = ovlp->mStart;
        mEnd = ovlp->mEnd;
        mDuration = mEnd - mStart;
        
        auto hOvlp = timeline->mMtvReader->GetOverlapById(mOvlp->mID);
        IM_ASSERT(hOvlp);
        mFusion = dynamic_cast<BluePrintVideoTransition *>(hOvlp->GetTransition().get());
        if (!mFusion)
        {
            mFusion = new BluePrintVideoTransition(timeline);
            mFusion->SetKeyPoint(mOvlp->mFusionKeyPoints);
            MediaCore::VideoTransition::Holder hTrans(mFusion);
            hOvlp->SetTransition(hTrans);
        }
        mClipFirstFrameRate = mClip1->mClipFrameRate;
        mClipSecondFrameRate = mClip2->mClipFrameRate;
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
    mFusion = nullptr;
}

void EditingVideoOverlap::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom)
{
    // update display params
    bool ovlpRngChanged = mOvlp->mStart != mStart || mOvlp->mEnd != mEnd;
    if (ovlpRngChanged)
    {
        mStart = mOvlp->mStart;
        mEnd = mOvlp->mEnd;
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
        else if (mImgTexture1 || mImgTexture2)
        {
            int width = ImGui::ImGetTextureWidth(mImgTexture1 ? mImgTexture1 : mImgTexture2);
            int height = ImGui::ImGetTextureHeight(mImgTexture1 ? mImgTexture1 : mImgTexture2);
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
    std::vector<MediaCore::Snapshot::Image::Holder> snapImages1;
    if (mViewer1)
    {
        m_StartOffset.first = mClip1->mStartOffset + mOvlp->mStart - mClip1->mStart;
        if (!mViewer1->GetSnapshots((double)m_StartOffset.first / 1000, snapImages1))
        {
            Logger::Log(Logger::Error) << mViewer1->GetError() << std::endl;
            return;
        }
        mViewer1->UpdateSnapshotTexture(snapImages1);
    }
    std::vector<MediaCore::Snapshot::Image::Holder> snapImages2;
    if (mViewer2)
    {
        m_StartOffset.second = mClip2->mStartOffset + mOvlp->mStart - mClip2->mStart;
        if (!mViewer2->GetSnapshots((double)m_StartOffset.second / 1000, snapImages2))
        {
            Logger::Log(Logger::Error) << mViewer2->GetError() << std::endl;
            return;
        }
        mViewer2->UpdateSnapshotTexture(snapImages2);
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
            if (imgIter1 != snapImages1.end() && (*imgIter1)->mTextureReady)
            {
                auto& img = *imgIter1++;
                drawList->AddImage(*(img->mTextureHolder), imgLeftTop, imgLeftTop + snapDispSize, uvMin, uvMax);
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
    else if (mImgTexture1)
    {
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
            drawList->AddImage(mImgTexture1, imgLeftTop, imgLeftTop + snapDispSize, uvMin, uvMax);
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
            if (imgIter2 != snapImages2.end() && (*imgIter2)->mTextureReady)
            {
                auto& img = *imgIter2++;
                drawList->AddImage(*(img->mTextureHolder), img2LeftTop, img2LeftTop + snapDispSize, uvMin, uvMax);
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
    else if (mImgTexture2)
    {
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
            drawList->AddImage(mImgTexture2, img2LeftTop, img2LeftTop + snapDispSize, uvMin, uvMax);
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

void EditingVideoOverlap::Seek(int64_t pos)
{
    TimeLine* timeline = (TimeLine*)(mOvlp->mHandle);
    if (!timeline)
        return;
    timeline->Seek(pos);
}

void EditingVideoOverlap::Step(bool forward, int64_t step)
{
}

bool EditingVideoOverlap::GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame, bool preview_frame)
{
    int ret = true;
    TimeLine* timeline = (TimeLine*)(mOvlp->mHandle);
    if (!timeline)
        return false;

    auto frames = timeline->GetPreviewFrame();
    ImGui::ImMat frame_org_first;
    auto iter_first = std::find_if(frames.begin(), frames.end(), [this] (auto& cf) {
        return cf.clipId == mOvlp->m_Clip.first && cf.phase == MediaCore::CorrelativeFrame::PHASE_SOURCE_FRAME;
    });
    if (iter_first != frames.end())
        frame_org_first = iter_first->frame;
    else
        ret = false;
    ImGui::ImMat frame_org_second;
    auto iter_second = std::find_if(frames.begin(), frames.end(), [this] (auto& cf) {
        return cf.clipId == mOvlp->m_Clip.second && cf.phase == MediaCore::CorrelativeFrame::PHASE_SOURCE_FRAME;
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
    TimeLine * timeline = (TimeLine *)(mOvlp->mHandle);
    if (!timeline)
        return;
    auto overlap = timeline->FindOverlapByID(mOvlp->mID);
    if (!overlap)
        return;
    if (mFusion && mFusion->mBp && mFusion->mBp->Blueprint_IsValid())
    {
        overlap->mFusionBP = mFusion->mBp->m_Document->Serialize();
        overlap->mFusionKeyPoints = mFusion->mKeyPoints;
    }
    timeline->UpdatePreview();
}
}// namespace MediaTimeline

namespace MediaTimeline
{
EditingAudioOverlap::EditingAudioOverlap(Overlap* ovlp)
    : BaseEditingOverlap(ovlp)
{
    TimeLine* timeline = (TimeLine*)(ovlp->mHandle);
    AudioClip* audclip1 = (AudioClip*)timeline->FindClipByID(ovlp->m_Clip.first);
    AudioClip* audclip2 = (AudioClip*)timeline->FindClipByID(ovlp->m_Clip.second);
    if (audclip1 && audclip2)
    {
        mClip1 = audclip1; mClip2 = audclip2;
        m_StartOffset.first = audclip1->mStartOffset + ovlp->mStart - audclip1->mStart;
        m_StartOffset.second = audclip2->mStartOffset + ovlp->mStart - audclip2->mStart;
        mStart = ovlp->mStart;
        mEnd = ovlp->mEnd;
        mDuration = mEnd - mStart;
        auto hOvlp = timeline->mMtaReader->GetOverlapById(mOvlp->mID);
        IM_ASSERT(hOvlp);
        mFusion = dynamic_cast<BluePrintAudioTransition *>(hOvlp->GetTransition().get());
        if (!mFusion)
        {
            mFusion = new BluePrintAudioTransition(timeline);
            mFusion->SetKeyPoint(mOvlp->mFusionKeyPoints);
            MediaCore::AudioTransition::Holder hTrans(mFusion);
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
    mFusion = nullptr;
}

void EditingAudioOverlap::Seek(int64_t pos)
{
    TimeLine* timeline = (TimeLine*)(mOvlp->mHandle);
    if (!timeline)
        return;
    timeline->Seek(pos);
}

void EditingAudioOverlap::Step(bool forward, int64_t step)
{
}

void EditingAudioOverlap::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom)
{
    // update display params
    bool ovlpRngChanged = mOvlp->mStart != mStart || mOvlp->mEnd != mEnd;
    if (ovlpRngChanged)
    {
        mStart = mOvlp->mStart;
        mEnd = mOvlp->mEnd;
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
        int64_t start_time = std::max(mClip1->mStart, mStart);
        int64_t end_time = std::min(mClip1->mEnd, mEnd);
        IM_ASSERT(start_time <= end_time);
        int start_offset = (int)((double)((mStart - mClip1->mStart)) / 1000.f / waveform->aggregateDuration);
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
#if AUDIO_WAVEFORM_IMPLOT
            start_offset = start_offset / zoom * zoom; // align start_offset
            if (ImPlot::BeginPlot(id_string.c_str(), clip_window_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
            {
                std::string plot_id = id_string + "_line";
                ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, window_length / zoom, -wave_range, wave_range, ImGuiCond_Always);
                ImPlot::PlotLine(plot_id.c_str(), &waveform->pcm[i][start_offset], window_length / zoom, 1.0, 0.0, 0, 0, sizeof(float) * zoom);
                ImPlot::EndPlot();
            }
#else
            start_offset = start_offset / sample_stride * sample_stride; // align start_offset
            std::string plot_max_id = id_string + "_line_max";
            std::string plot_min_id = id_string + "_line_min";
            ImGui::ImMat plot_frame_max, plot_frame_min;
            waveFrameResample(&waveform->pcm[i][0], sample_stride, clip_window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
            ImGui::SetCursorScreenPos(leftTop + ImVec2(0, i * clip_window_size.y));
            ImGui::PlotLinesEx(plot_max_id.c_str(), (float *)plot_frame_max.data, plot_frame_max.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
            ImGui::SetCursorScreenPos(leftTop + ImVec2(0, i * clip_window_size.y));
            ImGui::PlotLinesEx(plot_min_id.c_str(), (float *)plot_frame_min.data, plot_frame_min.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
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
        int64_t start_time = std::max(mClip2->mStart, mStart);
        int64_t end_time = std::min(mClip2->mEnd, mEnd);
        IM_ASSERT(start_time <= end_time);
        int start_offset = (int)((double)((mStart - mClip2->mStart)) / 1000.f / waveform->aggregateDuration);
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
#if AUDIO_WAVEFORM_IMPLOT
            start_offset = start_offset / zoom * zoom; // align start_offset
            if (ImPlot::BeginPlot(id_string.c_str(), clip_window_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
            {
                std::string plot_id = id_string + "_line";
                ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, window_length / zoom, -wave_range, wave_range, ImGuiCond_Always);
                ImPlot::PlotLine(plot_id.c_str(), &waveform->pcm[i][start_offset], window_length / zoom, 1.0, 0.0, 0, 0, sizeof(float) * zoom);
                ImPlot::EndPlot();
            }
#else
            start_offset = start_offset / sample_stride * sample_stride; // align start_offset
            std::string plot_max_id = id_string + "_line_max";
            std::string plot_min_id = id_string + "_line_min";
            ImGui::ImMat plot_frame_max, plot_frame_min;
            waveFrameResample(&waveform->pcm[i][0], sample_stride, clip_window_size.x, start_offset, sampleSize, zoom, plot_frame_max, plot_frame_min);
            ImGui::SetCursorScreenPos(clip2_pos + ImVec2(0, i * clip_window_size.y));
            ImGui::PlotLinesEx(plot_max_id.c_str(), (float *)plot_frame_max.data, plot_frame_max.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
            ImGui::SetCursorScreenPos(clip2_pos + ImVec2(0, i * clip_window_size.y));
            ImGui::PlotLinesEx(plot_min_id.c_str(), (float *)plot_frame_min.data, plot_frame_min.w, 0, nullptr, -wave_range, wave_range, clip_window_size, sizeof(float), false, true);
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
    TimeLine * timeline = (TimeLine *)(mOvlp->mHandle);
    if (!timeline)
        return;
    auto overlap = timeline->FindOverlapByID(mOvlp->mID);
    if (!overlap)
        return;
    if (mFusion && mFusion->mBp && mFusion->mBp->Blueprint_IsValid())
    {
        overlap->mFusionBP = mFusion->mBp->m_Document->Serialize();
        overlap->mFusionKeyPoints = mFusion->mKeyPoints;
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

bool MediaTrack::DrawTrackControlBar(ImDrawList *draw_list, ImRect rc, std::list<imgui_json::value>* pActionList)
{
    bool need_update = false;
    ImGuiIO &io = ImGui::GetIO();
    if (mExpanded) draw_list->AddText(rc.Min + ImVec2(4, 0), IM_COL32_WHITE, mName.c_str());
    ImVec2 button_size = ImVec2(12, 12);
    int button_count = 0;
    {
        bool ret = TimelineButton(draw_list, mLocked ? ICON_LOCKED : ICON_UNLOCK, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mLocked ? "unlock" : "lock");
        if (ret && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            mLocked = !mLocked;
        button_count ++;
    }
    if (IS_AUDIO(mType))
    {
        bool ret = TimelineButton(draw_list, mView ? ICON_SPEAKER : ICON_SPEAKER_MUTE, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mView ? "mute" : "voice");
        if (ret && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
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
        button_count ++;
    }
    else
    {
        bool ret = TimelineButton(draw_list, mView ? ICON_VIEW : ICON_VIEW_DISABLE, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mView ? "hidden" : "view");
        if (ret && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            mView = !mView;
            need_update = true;
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
    }
    return need_update;
}

void MediaTrack::Update()
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline)
        return;
    // sort m_Clips by clip start time
    std::sort(m_Clips.begin(), m_Clips.end(), [](const Clip *a, const Clip* b){
        return a->mStart < b->mStart;
    });
    
    // check all overlaps
    for (auto iter = m_Overlaps.begin(); iter != m_Overlaps.end();)
    {
        if (!(*iter)->IsOverlapValid())
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
            if ((*iter)->mEnd >= (*next)->mStart)
            {
                // it is a overlap area
                int64_t start = std::max((*next)->mStart, (*iter)->mStart);
                int64_t end = std::min((*iter)->mEnd, (*next)->mEnd);
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
        mMttReader->GetKeyPoints()->SetRangeX(0, timeline->mEnd - timeline->mStart, true);
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
            if (mMttReader && tclip->mClipHolder)
            {
                mMttReader->DeleteClip(tclip->mClipHolder);
            }
        }
        m_Clips.erase(iter);
    }
}

void MediaTrack::PushBackClip(Clip * clip)
{
    if (m_Clips.size() > 0)
    {
        int64_t length = clip->mEnd - clip->mStart;
        auto last_clip = m_Clips.end() - 1;
        clip->mStart = (*last_clip)->mEnd;
        clip->mEnd = clip->mStart + length;
    }
    clip->ConfigViewWindow(mViewWndDur, mPixPerMs);
    clip->SetTrackHeight(mTrackHeight);
    m_Clips.push_back(clip);
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
            if ((overlap->mStart >= pos && overlap->mStart <= pos + clip->mLength) || 
                (overlap->mEnd >= pos && overlap->mEnd <= pos + clip->mLength))
            {
                can_insert_clip = false;
                break;
            }
        }
    }
    return can_insert_clip;
}

void MediaTrack::InsertClip(Clip * clip, int64_t pos, bool update, std::list<imgui_json::value>* pActionList)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !clip)
        return;
        
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [clip](const Clip * _clip)
    {
        return _clip->mID == clip->mID;
    });
    if (pos > 0) timeline->AlignTime(pos);
    if (iter == m_Clips.end())
    {
        int64_t length = clip->mEnd - clip->mStart;
        clip->mStart = pos <= 0 ? 0 : pos;
        clip->mEnd = clip->mStart + length;
        clip->ConfigViewWindow(mViewWndDur, mPixPerMs);
        clip->SetTrackHeight(mTrackHeight);
        // Set keypoint
        clip->mFilterKeyPoints.SetRangeX(0, clip->mEnd - clip->mStart, true);
        clip->mAttributeKeyPoints.SetRangeX(0, clip->mEnd - clip->mStart, true);
        m_Clips.push_back(clip);
        if (pActionList)
        {
            imgui_json::value action;
            action["action"] = "ADD_CLIP";
            action["media_type"] = imgui_json::number(clip->mType);
            action["to_track_id"] = imgui_json::number(mID);
            imgui_json::value clipJson;
            clip->Save(clipJson);
            action["clip_json"] = clipJson;
            pActionList->push_back(std::move(action));
        }
    }
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
        if (clip->mStart <= time && clip->mEnd >= time)
        {
            clips.push_back(clip);
            if (clip->bSelected)
                select_clips.push_back(clip);
        }
    }
    for (auto clip : select_clips)
    {
        if (!ret_clip || ret_clip->mEnd - ret_clip->mStart > clip->mEnd - clip->mStart)
            ret_clip = clip;
    }
    if (!ret_clip)
    {
        for (auto clip : clips)
        {
            if (!ret_clip || ret_clip->mEnd - ret_clip->mStart > clip->mEnd - clip->mStart)
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

void MediaTrack::SelectEditingClip(Clip * clip, bool filter_editing)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !clip)
        return;
    if (IS_DUMMY(clip->mType))
        return;
    
    int updated = 0;
    if (filter_editing && timeline->m_CallBacks.EditingClipFilter)
    {
        updated = timeline->m_CallBacks.EditingClipFilter(clip->mType, clip);
    }
    else if (timeline->m_CallBacks.EditingClipAttribute)
    {
        updated = timeline->m_CallBacks.EditingClipAttribute(clip->mType, clip);
    }
    if (timeline->currentTime < clip->mStart || timeline->currentTime > clip->mEnd || timeline->currentTime < timeline->firstTime || timeline->currentTime > timeline->lastTime)
        timeline->Seek(clip->mStart);

    // find old editing clip and reset BP
    auto editing_clip = timeline->FindEditingClip();
    if (editing_clip && editing_clip->mID == clip->mID)
    {
        if (filter_editing)
        {
            if (IS_VIDEO(editing_clip->mType) &&
                timeline->mVidFilterClip &&
                timeline->mVidFilterClip->mFilter &&
                timeline->mVidFilterClip->mFilter->mBp &&
                timeline->mVidFilterClip->mFilter->mBp->Blueprint_IsValid())
                return;
            if (IS_AUDIO(editing_clip->mType) &&
                timeline->mAudFilterClip &&
                timeline->mAudFilterClip->mFilter &&
                timeline->mAudFilterClip->mFilter->mBp &&
                timeline->mAudFilterClip->mFilter->mBp->Blueprint_IsValid())
                return;
        }
    }
    else if (editing_clip && editing_clip->mID != clip->mID)
    {
        if (IS_VIDEO(editing_clip->mType))
        {
            if (!updated && timeline->mVidFilterClip)
            {
                timeline->mVidFilterClip->Save();
            }
            // update timeline video filter clip
            timeline->mVidFilterClipLock.lock();
            if (timeline->mVidFilterClip)
            {
                delete timeline->mVidFilterClip;
                timeline->mVidFilterClip = nullptr;
                if (timeline->mVideoFilterInputTexture) {ImGui::ImDestroyTexture(timeline->mVideoFilterInputTexture); timeline->mVideoFilterInputTexture = nullptr;}
                if (timeline->mVideoFilterOutputTexture) { ImGui::ImDestroyTexture(timeline->mVideoFilterOutputTexture); timeline->mVideoFilterOutputTexture = nullptr;  }
            }
            timeline->mVidFilterClipLock.unlock();
        }
        else if (IS_AUDIO(editing_clip->mType))
        {
            if (!updated && timeline->mAudFilterClip)
            {
                timeline->mAudFilterClip->Save();
            }
            // update timeline Audio filter clip
            timeline->mAudFilterClipLock.lock();
            if (timeline->mAudFilterClip)
            {
                delete timeline->mAudFilterClip;
                timeline->mAudFilterClip = nullptr;
            }
            timeline->mAudFilterClipLock.unlock();
        }
        editing_clip->bEditing = false;
    }

    clip->bEditing = true;

    if (IS_VIDEO(clip->mType))
    {
        if (!timeline->mVidFilterClip)
            timeline->mVidFilterClip = new EditingVideoClip((VideoClip*)clip);
    }
    else if (IS_AUDIO(clip->mType))
    {
        if (!timeline->mAudFilterClip)
            timeline->mAudFilterClip = new EditingAudioClip((AudioClip*)clip);
    }
}

void MediaTrack::SelectEditingOverlap(Overlap * overlap)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !overlap)
        return;
    
    // find old editing overlap and reset BP
    Overlap * editing_overlap = timeline->FindEditingOverlap();
    timeline->Seek(overlap->mStart);

    if (editing_overlap && editing_overlap->mID != overlap->mID)
    {
        auto clip_first = timeline->FindClipByID(editing_overlap->m_Clip.first);
        auto clip_second = timeline->FindClipByID(editing_overlap->m_Clip.second);
        if (clip_first && clip_second)
        {
            if (IS_VIDEO(clip_first->mType) && 
                IS_VIDEO(clip_second->mType) &&
                timeline->mVidOverlap &&
                timeline->mVidOverlap->mFusion &&
                timeline->mVidOverlap->mFusion->mBp &&
                timeline->mVidOverlap->mFusion->mBp->Blueprint_IsValid())
            {
                editing_overlap->mFusionBP = timeline->mVidOverlap->mFusion->mBp->m_Document->Serialize();
            }
            if (IS_AUDIO(clip_first->mType) && 
                IS_AUDIO(clip_second->mType) &&
                timeline->mAudOverlap &&
                timeline->mAudOverlap->mFusion &&
                timeline->mAudOverlap->mFusion->mBp &&
                timeline->mAudOverlap->mFusion->mBp->Blueprint_IsValid())
            {
                editing_overlap->mFusionBP = timeline->mAudOverlap->mFusion->mBp->m_Document->Serialize();
            }
        }
        if (timeline->mVidOverlap)
            timeline->mVidOverlap->Save();
        editing_overlap->bEditing = false;

        if (timeline->mVidOverlap)
        {
            delete timeline->mVidOverlap;
            timeline->mVidOverlap = nullptr;
            if (timeline->mVideoFusionInputFirstTexture) { ImGui::ImDestroyTexture(timeline->mVideoFusionInputFirstTexture); timeline->mVideoFusionInputFirstTexture = nullptr; }
            if (timeline->mVideoFusionInputSecondTexture) { ImGui::ImDestroyTexture(timeline->mVideoFusionInputSecondTexture); timeline->mVideoFusionInputSecondTexture = nullptr; }
            if (timeline->mVideoFusionOutputTexture) { ImGui::ImDestroyTexture(timeline->mVideoFusionOutputTexture); timeline->mVideoFusionOutputTexture = nullptr;  }
        }
    }

    overlap->bEditing = true;
    auto first = timeline->FindClipByID(overlap->m_Clip.first);
    auto second = timeline->FindClipByID(overlap->m_Clip.second);
    if (!first || !second)
        return;
    if (IS_DUMMY(first->mType) || IS_DUMMY(second->mType))
        return;

    if (IS_VIDEO(first->mType) && IS_VIDEO(second->mType))
    {
        if (!timeline->mVidOverlap)
            timeline->mVidOverlap = new EditingVideoOverlap(overlap);
    }

    if (IS_AUDIO(first->mType) && IS_AUDIO(second->mType))
    {
        if (!timeline->mAudOverlap)
            timeline->mAudOverlap = new EditingAudioOverlap(overlap);
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
                    tclip->CreateClipHold(new_track);
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
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
        }
    }
    if (name.compare("VideoFusion") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED)
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
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
        }
    }

    return ret;
}

TimeLine::TimeLine(std::string plugin_path)
    : mStart(0), mEnd(0), mPcmStream(this)
{
    std::srand(std::time(0)); // init std::rand

    mAudioRender = MediaCore::AudioRender::CreateInstance();
    if (mAudioRender)
    {
        mAudioRender->OpenDevice(mAudioSampleRate, mAudioChannels, mAudioFormat, &mPcmStream);
    }

    auto exec_path = ImGuiHelper::exec_path();
    mPluginPath = plugin_path.empty() ? ImGuiHelper::path_parent(exec_path) + "plugins" : plugin_path;
    m_BP_UI.Initialize(nullptr, mPluginPath.c_str());

    ConfigureDataLayer();

    mAudioAttribute.channel_data.clear();
    mAudioAttribute.channel_data.resize(mAudioChannels);
    memcpy(&mAudioAttribute.mBandCfg, &DEFAULT_BAND_CFG, sizeof(mAudioAttribute.mBandCfg));

    mRecordIter = mHistoryRecords.begin();
}

TimeLine::~TimeLine()
{
    mMtvReader = nullptr;
    mMtaReader = nullptr;
    
    if (mMainPreviewTexture) { ImGui::ImDestroyTexture(mMainPreviewTexture); mMainPreviewTexture = nullptr; }
    if (mEncodingPreviewTexture) { ImGui::ImDestroyTexture(mEncodingPreviewTexture); mEncodingPreviewTexture = nullptr; }
    mAudioAttribute.channel_data.clear();

    if (mAudioAttribute.m_audio_vector_texture) { ImGui::ImDestroyTexture(mAudioAttribute.m_audio_vector_texture); mAudioAttribute.m_audio_vector_texture = nullptr; }
    
    m_BP_UI.Finalize();

    for (auto track : m_Tracks) delete track;
    for (auto clip : m_Clips) delete clip;
    for (auto overlap : m_Overlaps)  delete overlap;
    for (auto item : media_items) delete item;

    if (mVideoFilterInputTexture) { ImGui::ImDestroyTexture(mVideoFilterInputTexture); mVideoFilterInputTexture = nullptr; }
    if (mVideoFilterOutputTexture) { ImGui::ImDestroyTexture(mVideoFilterOutputTexture); mVideoFilterOutputTexture = nullptr;  }

    if (mVideoFusionInputFirstTexture) { ImGui::ImDestroyTexture(mVideoFusionInputFirstTexture); mVideoFusionInputFirstTexture = nullptr; }
    if (mVideoFusionInputSecondTexture) { ImGui::ImDestroyTexture(mVideoFusionInputSecondTexture); mVideoFusionInputSecondTexture = nullptr; }
    if (mVideoFusionOutputTexture) { ImGui::ImDestroyTexture(mVideoFusionOutputTexture); mVideoFusionOutputTexture = nullptr;  }

    if (mVidFilterClip)
    {
        delete mVidFilterClip;
        mVidFilterClip = nullptr;
    }
    if (mAudFilterClip)
    {
        delete mAudFilterClip;
        mAudFilterClip = nullptr;
    }
    if (mVidOverlap)
    {
        delete mVidOverlap;
        mVidOverlap = nullptr;
    }
    if (mAudOverlap)
    {
        delete mAudOverlap;
        mAudOverlap = nullptr;
    }

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
}

void TimeLine::AlignTime(int64_t& time)
{
    alignTime(time, mFrameRate);
}

void TimeLine::UpdateRange()
{
    int64_t start_min = INT64_MAX;
    int64_t end_max = INT64_MIN;
    for (auto clip : m_Clips)
    {
        if (clip->mStart < start_min)
            start_min = clip->mStart;
        if (clip->mEnd > end_max)
            end_max = clip->mEnd;
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
            if (clip->mStart <= time && clip->mEnd >= time)
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
    if (index >= 0 && index < m_Tracks.size())
    {
        bool click_empty_space = true;
        auto current_track = m_Tracks[index];
        for (auto overlap : m_Overlaps)
        {
            if (overlap->mStart <= time && overlap->mEnd >= time)
            {
                click_empty_space = false;
                break;
            }
        }
        if (click_empty_space)
        {
            current_track->mExpanded = !current_track->mExpanded;
        }
    }
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
                imgui_json::value clipJson;
                clip->Save(clipJson);
                containedClipsJson.push_back(clipJson);
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
        currentTime = firstTime = lastTime = visibleTime = 0;
        mark_in = mark_out = -1;
    }

    UpdatePreview();
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
                c = VideoClip::Load(clipJson, this);
            else if (IS_AUDIO(type))
                c = AudioClip::Load(clipJson, this);
            else if (IS_TEXT(type))
                c = TextClip::Load(clipJson, this);
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
                hVidClip = hVidTrk->AddNewClip(c->mID, c->mMediaParser, c->mStart, c->mEnd-c->mStart, 0, 0);
            else
                hVidClip = hVidTrk->AddNewClip(c->mID, c->mMediaParser, c->mStart, c->mStartOffset, c->mEndOffset, currentTime-c->mStart);
            BluePrintVideoFilter* bpvf = new BluePrintVideoFilter(this);
            bpvf->SetBluePrintFromJson(c->mFilterBP);
            bpvf->SetKeyPoint(c->mFilterKeyPoints);
            MediaCore::VideoFilter::Holder hFilter(bpvf);
            hVidClip->SetFilter(hFilter);
            auto attribute = hVidClip->GetTransformFilter();
            if (attribute)
            {
                VideoClip* vidclip = (VideoClip*)c;
                attribute->SetScaleType(vidclip->mScaleType);
                attribute->SetScaleH(vidclip->mScaleH);
                attribute->SetScaleV(vidclip->mScaleV);
                attribute->SetPositionOffsetH(vidclip->mPositionOffsetH);
                attribute->SetPositionOffsetV(vidclip->mPositionOffsetV);
                attribute->SetRotationAngle(vidclip->mRotationAngle);
                attribute->SetCropMarginL(vidclip->mCropMarginL);
                attribute->SetCropMarginT(vidclip->mCropMarginT);
                attribute->SetCropMarginR(vidclip->mCropMarginR);
                attribute->SetCropMarginB(vidclip->mCropMarginB);
                attribute->SetKeyPoint(vidclip->mAttributeKeyPoints);
            }
        }
        UpdatePreview();
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
                c->mStart, c->mStartOffset, c->mEndOffset);
            BluePrintAudioFilter* bpaf = new BluePrintAudioFilter(this);
            bpaf->SetBluePrintFromJson(c->mFilterBP);
            bpaf->SetKeyPoint(c->mFilterKeyPoints);
            MediaCore::AudioFilter::Holder hFilter(bpaf);
            hAudClip->SetFilter(hFilter);
        }
        // audio attribute
        auto aeFilter = hAudTrk->GetAudioEffectFilter();
        // gain
        auto volParams = aeFilter->GetVolumeParams();
        volParams.volume = t->mAudioTrackAttribute.mAudioGain;
        aeFilter->SetVolumeParams(&volParams);
        mMtaReader->UpdateDuration();
        mMtaReader->SeekTo(currentTime);
    }

    SyncDataLayer(true);
    return true;
}

void TimeLine::MovingTrack(int& index, int& dst_index, std::list<imgui_json::value>* pActionList)
{
    auto iter = m_Tracks.begin() + index;
    auto iter_dst = dst_index == -2 ? m_Tracks.end() - 1 : m_Tracks.begin() + dst_index;
    if (dst_index == -2 && iter == m_Tracks.end() - 1)
    {
        return;
    }

    if (pActionList)
    {
        imgui_json::value action;
        action["action"] = "MOVE_TRACK";
        if (dst_index < index)
        {
            action["media_type"] = imgui_json::number((*iter_dst)->mType);
            action["track_id1"] = imgui_json::number((*iter_dst)->mID);
            action["media_type2"] = imgui_json::number((*iter)->mType);
            action["track_id2"] = imgui_json::number((*iter)->mID);
        }
        else
        {
            action["media_type"] = imgui_json::number((*iter)->mType);
            action["track_id1"] = imgui_json::number((*iter)->mID);
            action["media_type2"] = imgui_json::number((*iter_dst)->mType);
            action["track_id2"] = imgui_json::number((*iter_dst)->mID);
        }
        pActionList->push_back(std::move(action));
    }

    // do we need change other type of media?
    MediaTrack * tmp = *iter;
    *iter = *iter_dst;
    *iter_dst = tmp;
    index = dst_index;
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
            if (track->mMttReader && tclip->mClipHolder)
            {
                track->mMttReader->DeleteClip(tclip->mClipHolder);
            }
            // and add into dst track holder
            tclip->CreateClipHold(dst_track);
        }

        dst_track->InsertClip(clip, clip->mStart);
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
        if (mVidFilterClip && clip->mID == mVidFilterClip->mID)
        {
            mVidFilterClipLock.lock();
            delete mVidFilterClip;
            mVidFilterClip = nullptr;
            mVidFilterClipLock.unlock();
        }
        else if (mAudFilterClip && clip->mID == mAudFilterClip->mID)
        {
            mAudFilterClipLock.lock();
            delete mAudFilterClip;
            mAudFilterClip = nullptr;
            mAudFilterClipLock.unlock();
        }
        DeleteClipFromGroup(clip, clip->mGroupID, pActionList);

        if (pActionList)
        {
            imgui_json::value action;
            action["action"] = "REMOVE_CLIP";
            action["media_type"] = imgui_json::number(clip->mType);
            action["from_track_id"] = imgui_json::number(track->mID);
            imgui_json::value clip_json;
            clip->Save(clip_json);
            action["clip_json"] = clip_json;
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
            if (mVidOverlap && mVidOverlap->mOvlp == overlap)
            {
                delete mVidOverlap;
                mVidOverlap = nullptr;
            }
            if (mAudOverlap && mAudOverlap->mOvlp == overlap)
            {
                delete mAudOverlap;
                mAudOverlap = nullptr;
            }
            delete overlap;
        }
        else
            ++ iter;
    }
}

void TimeLine::UpdateCurrent()
{
    if (!mIsPreviewForward)
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
        if (mEnd - currentTime < visibleTime / 2)
        {
            firstTime = mEnd - visibleTime;
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

void TimeLine::UpdatePreview()
{
    mMtvReader->Refresh(true);
    mIsPreviewNeedUpdate = true;
}

std::vector<MediaCore::CorrelativeFrame> TimeLine::GetPreviewFrame()
{
    int64_t auddataPos, previewPos;
    if (!bSeeking)
    {
        if (mPcmStream.GetTimestampMs(auddataPos))
        {
            int64_t bufferedDur = mMtaReader->SizeToDuration(mAudioRender->GetBufferedDataSize());
            previewPos = mIsPreviewForward ? auddataPos-bufferedDur : auddataPos+bufferedDur;
        }
        else
        {
            int64_t elapsedTime = (int64_t)(std::chrono::duration_cast<std::chrono::duration<double>>((PlayerClock::now()-mPlayTriggerTp)).count()*1000);
            previewPos = mIsPreviewPlaying ? (mIsPreviewForward ? mPreviewResumePos+elapsedTime : mPreviewResumePos-elapsedTime) : mPreviewResumePos;
        }
    }
    else
    {
        previewPos = mPreviewResumePos;
    }

    currentTime = previewPos;
    if (mIsPreviewPlaying)
    {
        bool playEof = false;
        int64_t dur = ValidDuration();
        if (!mIsPreviewForward && currentTime <= 0)
        {
            if (bLoop)
            {
                currentTime = dur;
                Seek(currentTime);
            }
            else
            {
                currentTime = 0;
                playEof = true;
            }
        }
        else if (mIsPreviewForward && currentTime >= dur)
        {
            if (bLoop)
            {
                currentTime = 0;
                Seek(currentTime);
            }
            else
            {
                currentTime = dur;
                playEof = true;
            }
        }
        if (playEof)
        {
            mIsPreviewPlaying = false;
            mPreviewResumePos = currentTime;
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
    mMtvReader->ReadVideoFrameEx(currentTime, frames, true, needPreciseFrame);
    if (mIsPreviewPlaying) UpdateCurrent();
    return frames;
}

float TimeLine::GetAudioLevel(int channel)
{
    if (channel < mAudioAttribute.channel_data.size())
        return mAudioAttribute.channel_data[channel].m_decibel;
    return 0;
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
        mMtvReader->SetDirection(forward);
        mMtaReader->SetDirection(forward);
        mIsPreviewForward = forward;
        mPlayTriggerTp = PlayerClock::now();
        mPreviewResumePos = currentTime;
        needSeekAudio = true;
    }
    if (needSeekAudio && mAudioRender)
    {
        mAudioRender->Flush();
        mMtaReader->SeekTo(currentTime);
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
            mPreviewResumePos = currentTime;
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
}

void TimeLine::Seek(int64_t msPos)
{
    bool beginSeek = false;
    if (!bSeeking)
    {
        // begin to seek
        bSeeking = true;
        beginSeek = true;
        if (mAudioRender && !mIsPreviewPlaying)
            mAudioRender->Resume();
    }
    if (beginSeek || msPos != mPreviewResumePos)
    {
        mPlayTriggerTp = PlayerClock::now();
        if (mMtaReader)
            mMtaReader->SeekTo(msPos, true);
        if (mMtvReader)
            mMtvReader->SeekTo(msPos, true);
        mPreviewResumePos = msPos;
    }
}

void TimeLine::StopSeek()
{
    if (bSeeking)
    {
        bSeeking = false;
        if (mAudioRender)
        {
            if (!mIsPreviewPlaying)
                mAudioRender->Pause();
            mAudioRender->Flush();
        }
        if (mMtaReader)
            mMtaReader->SeekTo(mPreviewResumePos, false);
        mPlayTriggerTp = PlayerClock::now();
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
    currentTime = std::round(vmat.time_stamp*1000);
    mPreviewResumePos = currentTime;

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
    if (dur > 0) dur -= 1;
    Seek(dur);
}

int64_t TimeLine::ValidDuration()
{
    int64_t vdur = mMtvReader->Duration();
    int64_t adur = mMtaReader->Duration();
    int64_t media_range = vdur > adur ? vdur : adur;
    if (!mEncodingInRange)
    {
        mEncoding_start = 0;
        mEncoding_end = media_range;
        return media_range;
    }
    else if (mark_out == -1 || mark_in == -1 || mark_out <= mark_in)
    {
        mEncoding_start = 0;
        mEncoding_end = media_range;
        return media_range;
    }
    else
    {
        auto _mark_in = mark_in;
        auto _mark_out = mark_out;
        if (mark_out > media_range) _mark_out = media_range;
        if (_mark_out <= _mark_in)
        {
            mEncoding_start = 0;
            mEncoding_end = 0;
            return 0;
        }
        mEncoding_start = _mark_in;
        mEncoding_end = _mark_out;
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

Clip * TimeLine::FindEditingClip()
{
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [](const Clip* clip)
    {
        return clip->bEditing;
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
        return _clip->mStart >= clip->mEnd;
    });
    if (iter != m_Clips.end())
        next_start = (*iter)->mStart;
    return next_start;
}

int64_t TimeLine::NextClipStart(int64_t pos)
{
    int64_t next_start = -1;
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [pos](const Clip* _clip)
    {
        return _clip->mStart > pos;
    });
    if (iter != m_Clips.end())
        next_start = (*iter)->mStart;
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
        bool is_moving, bool enable_select, bool is_cutting, int alignedMousePosX, std::list<imgui_json::value>* pActionList)
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
    auto need_seek = track->DrawTrackControlBar(draw_list, legendRect, pActionList);
    //if (need_seek) Seek();
    draw_list->PopClipRect();

    // draw clips
    for (auto clip : track->m_Clips)
    {
        clip->SetViewWindowStart(firstTime);
        bool draw_clip = false;
        float cursor_start = 0;
        float cursor_end  = 0;
        ImDrawFlags flag = ImDrawFlags_RoundCornersNone;
        if (clip->mStart <= firstTime && clip->mEnd > firstTime && clip->mEnd <= viewEndTime)
        {
            /***********************************************************
             *         ----------------------------------------
             * XXXXXXXX|XXXXXXXXXXXXXXXXXXXXXX|
             *         ----------------------------------------
            ************************************************************/
            cursor_start = clippingRect.Min.x;
            cursor_end = clippingRect.Min.x + (clip->mEnd - firstTime) * msPixelWidthTarget;
            draw_clip = true;
            flag |= ImDrawFlags_RoundCornersRight;
        }
        else if (clip->mStart >= firstTime && clip->mEnd <= viewEndTime)
        {
            /***********************************************************
             *         ----------------------------------------
             *                  |XXXXXXXXXXXXXXXXXXXXXX|
             *         ----------------------------------------
            ************************************************************/
            cursor_start = clippingRect.Min.x + (clip->mStart - firstTime) * msPixelWidthTarget;
            cursor_end = clippingRect.Min.x + (clip->mEnd - firstTime) * msPixelWidthTarget;
            draw_clip = true;
            flag |= ImDrawFlags_RoundCornersAll;
        }
        else if (clip->mStart >= firstTime && clip->mStart < viewEndTime && clip->mEnd >= viewEndTime)
        {
            /***********************************************************
             *         ----------------------------------------
             *                         |XXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXX
             *         ----------------------------------------
            ************************************************************/
            cursor_start = clippingRect.Min.x + (clip->mStart - firstTime) * msPixelWidthTarget;
            cursor_end = clippingRect.Max.x;
            draw_clip = true;
            flag |= ImDrawFlags_RoundCornersLeft;
        }
        else if (clip->mStart <= firstTime && clip->mEnd >= viewEndTime)
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
        if (clip->mStart == firstTime)
            flag |= ImDrawFlags_RoundCornersLeft;
        if (clip->mEnd == viewEndTime)
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
            if (clip->mGroupID != -1)
            {
                auto color = GetGroupColor(clip->mGroupID);
                draw_list->AddRectFilled(clip_title_pos_min, clip_title_pos_max, color, 4, flag);
            }
            else
                draw_list->AddRectFilled(clip_title_pos_min, clip_title_pos_max, IM_COL32(64,128,64,128), 4, flag);
            
            // draw clip status
            draw_list->PushClipRect(clip_title_pos_min, clip_title_pos_max, true);
            draw_list->AddText(clip_title_pos_min + ImVec2(4, 0), IM_COL32_WHITE, IS_TEXT(clip->mType) ? "T" : clip->mName.c_str());
            
            // add clip filter curve point
            ImGui::KeyPointEditor* keypoint_filter = &clip->mFilterKeyPoints;
            if (mVidFilterClip && mVidFilterClip->mID == clip->mID && mVidFilterClip->mFilter)
            {
                keypoint_filter = &mVidFilterClip->mFilter->mKeyPoints;
            }
            else if (mAudFilterClip && mAudFilterClip->mID == clip->mID && mAudFilterClip->mFilter)
            {
                keypoint_filter = &mAudFilterClip->mFilter->mKeyPoints;
            }
            for (int i = 0; i < keypoint_filter->GetCurveCount(); i++)
            {
                auto curve_color = keypoint_filter->GetCurveColor(i);
                for (int p = 0; p < keypoint_filter->GetCurvePointCount(i); p++)
                {
                    auto point = keypoint_filter->GetPoint(i, p);
                    auto pos = point.point.x + clip->mStart;
                    if (pos >= firstTime && pos <= viewEndTime)
                    {
                        ImVec2 center = ImVec2(clippingRect.Min.x + (pos - firstTime) * msPixelWidthTarget, clip_title_pos_min.y + (clip_title_pos_max.y - clip_title_pos_min.y) / 2);
                        draw_list->AddCircle(center, 3, curve_color, 0, 2);
                    }
                }
            }

            // add clip attribute curve point
            ImGui::KeyPointEditor* keypoint_attribute = &clip->mAttributeKeyPoints;
            if (mVidFilterClip && mVidFilterClip->mID == clip->mID && mVidFilterClip->mAttribute)
            {
                keypoint_attribute = mVidFilterClip->mAttribute->GetKeyPoint();
            }
            for (int i = 0; i < keypoint_attribute->GetCurveCount(); i++)
            {
                auto curve_color = keypoint_attribute->GetCurveColor(i);
                for (int p = 0; p < keypoint_attribute->GetCurvePointCount(i); p++)
                {
                    auto point = keypoint_attribute->GetPoint(i, p);
                    auto pos = point.point.x + clip->mStart;
                    if (pos >= firstTime && pos <= viewEndTime)
                    {
                        ImVec2 center = ImVec2(clippingRect.Min.x + (pos - firstTime) * msPixelWidthTarget, clip_title_pos_min.y + (clip_title_pos_max.y - clip_title_pos_min.y) / 2);
                        draw_list->AddRect(center - ImVec2(3, 3), center + ImVec2(3, 3), curve_color, 0, 2);
                    }
                }
            }

            draw_list->PopClipRect();

            // draw custom view
            if (track->mExpanded)
            {
                // can't using draw_list->PushClipRect, maybe all PushClipRect with screen pos/size need change to ImGui::PushClipRect
                ImGui::PushClipRect(clippingRect.Min, clippingRect.Max, true);
                clip->DrawContent(draw_list, clip_pos_min, clip_pos_max, clippingRect);
                ImGui::PopClipRect();
            }

            if (clip->bSelected)
            {
                if (clip->bEditing)
                    draw_list->AddRect(clip_rect.Min, clip_rect.Max, IM_COL32(255,0,255,224), 4, flag, 2.0f);
                else
                    draw_list->AddRect(clip_rect.Min, clip_rect.Max, IM_COL32(255,0,0,224), 4, flag, 2.0f);
            }
            else if (clip->bEditing)
            {
                draw_list->AddRect(clip_rect.Min, clip_rect.Max, IM_COL32(0,0,255,224), 4, flag, 2.0f);
            }

            // Clip select
            if (enable_select)
            {
                if (clip_rect.Contains(io.MousePos) )
                {
                    draw_list->AddRect(clip_rect.Min, clip_rect.Max, IM_COL32(255,255,255,255), 4, flag, 2.0f);
                    const bool is_shift_key_only = (io.KeyMods == ImGuiModFlags_Shift);
                    bool appand = (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) && is_shift_key_only;
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
                    else if (track->mExpanded && clip_area_rect.Contains(io.MousePos) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        bool b_attr_editing = ImGui::IsKeyDown(ImGuiKey_LeftShift) && (io.KeyMods == ImGuiModFlags_Shift);
                        if (!IS_DUMMY(clip->mType))
                            track->SelectEditingClip(clip, !b_attr_editing);
                    }
                    clip->DrawTooltips();
                }
            }

            if (is_cutting && clip->bSelected && alignedMousePosX >= 0 && clip_rect.Min.x < alignedMousePosX && clip_rect.Max.x > alignedMousePosX)
            {
                ImVec2 P1(alignedMousePosX, clip_rect.Min.y);
                ImVec2 P2(alignedMousePosX, clip_rect.Max.y);
                draw_list->AddLine(P1, P2, IM_COL32_WHITE, 2);
            }
        }
    }

    // draw overlap
    draw_list->PushClipRect(clippingTitleRect.Min, clippingTitleRect.Max, true);
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
        if (!overlap_rect.Overlaps(view_rc))
        {
            draw_overlap = false;
        }

        if (draw_overlap && cursor_end > cursor_start)
        {
            if (overlap->bEditing)
            {
                draw_list->AddRect(overlap_pos_min, overlap_pos_max, IM_COL32(255, 0, 255, 255), 4, ImDrawFlags_RoundCornersAll, 2.f);
            }
            if (overlap_rect.Contains(io.MousePos))
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
        }
    }
    draw_list->PopClipRect();
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
        if (val.is_number()) mWidth = val.get<imgui_json::number>();
    }
    if (value.contains("VideoHeight"))
    {
        auto& val = value["VideoHeight"];
        if (val.is_number()) mHeight = val.get<imgui_json::number>();
    }
    if (value.contains("PreviewScale"))
    {
        auto& val = value["PreviewScale"];
        if (val.is_number()) mPreviewScale = val.get<imgui_json::number>();
    }
    if (value.contains("FrameRateNum"))
    {
        auto& val = value["FrameRateNum"];
        if (val.is_number()) mFrameRate.num = val.get<imgui_json::number>();
    }
    if (value.contains("FrameRateDen"))
    {
        auto& val = value["FrameRateDen"];
        if (val.is_number()) mFrameRate.den = val.get<imgui_json::number>();
    }
    if (value.contains("AudioChannels"))
    {
        auto& val = value["AudioChannels"];
        if (val.is_number()) mAudioChannels = val.get<imgui_json::number>();
    }
    if (value.contains("AudioSampleRate"))
    {
        auto& val = value["AudioSampleRate"];
        if (val.is_number()) mAudioSampleRate = val.get<imgui_json::number>();
    }
    if (value.contains("AudioFormat"))
    {
        auto& val = value["AudioFormat"];
        if (val.is_number()) mAudioFormat = (MediaCore::AudioRender::PcmFormat)val.get<imgui_json::number>();
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
        if (val.is_number()) currentTime = val.get<imgui_json::number>();
        mPreviewResumePos = currentTime;
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
    if (value.contains("FilterOutPreview"))
    {
        auto& val = value["FilterOutPreview"];
        if (val.is_boolean()) bFilterOutputPreview = val.get<imgui_json::boolean>();
    }
    if (value.contains("FusionOutPreview"))
    {
        auto& val = value["FusionOutPreview"];
        if (val.is_boolean()) bFusionOutputPreview = val.get<imgui_json::boolean>();
    }
    if (value.contains("AttributeOutPreview"))
    {
        auto& val = value["AttributeOutPreview"];
        if (val.is_boolean()) bAttributeOutputPreview = val.get<imgui_json::boolean>();
    }
    if (value.contains("SelectLinked"))
    {
        auto& val = value["SelectLinked"];
        if (val.is_boolean()) bSelectLinked = val.get<imgui_json::boolean>();
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
    Update();
    //Seek();

    // load data layer
    ConfigureDataLayer();

    // load media clip
    const imgui_json::array* mediaClipArray = nullptr;
    if (imgui_json::GetPtrTo(value, "MediaClip", mediaClipArray))
    {
        for (auto& clip : *mediaClipArray)
        {
            uint32_t type = MEDIA_UNKNOWN;
            if (clip.contains("Type"))
            {
                auto& val = clip["Type"];
                if (val.is_number()) type = val.get<imgui_json::number>();
            }
            Clip * media_clip = nullptr;
            if (IS_VIDEO(type))
            {
                media_clip = VideoClip::Load(clip, this);
            }
            else if (IS_AUDIO(type))
            {
                media_clip = AudioClip::Load(clip, this);
            }
            else if (IS_TEXT(type))
            {
                media_clip = TextClip::Load(clip, this);
            }
            if (media_clip)
                m_Clips.push_back(media_clip);
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

    // build data layer multi-track media reader
    for (auto track : m_Tracks)
    {
        if (IS_VIDEO(track->mType))
        {
            MediaCore::VideoTrack::Holder vidTrack = mMtvReader->AddTrack(track->mID);
            vidTrack->SetVisible(track->mView);
            for (auto clip : track->m_Clips)
            {
                if (IS_DUMMY(clip->mType))
                    continue;
                MediaCore::VideoClip::Holder hVidClip;
                if (IS_IMAGE(clip->mType))
                    hVidClip = vidTrack->AddNewClip(clip->mID, clip->mMediaParser, clip->mStart, clip->mEnd-clip->mStart, 0, 0);
                else
                    hVidClip = vidTrack->AddNewClip(clip->mID, clip->mMediaParser, clip->mStart, clip->mStartOffset, clip->mEndOffset, currentTime-clip->mStart);

                BluePrintVideoFilter* bpvf = new BluePrintVideoFilter(this);
                bpvf->SetBluePrintFromJson(clip->mFilterBP);
                bpvf->SetKeyPoint(clip->mFilterKeyPoints);
                MediaCore::VideoFilter::Holder hFilter(bpvf);
                hVidClip->SetFilter(hFilter);
                auto attribute = hVidClip->GetTransformFilter();
                if (attribute)
                {
                    VideoClip * vidclip = (VideoClip *)clip;
                    attribute->SetScaleType(vidclip->mScaleType);
                    attribute->SetScaleH(vidclip->mScaleH);
                    attribute->SetScaleV(vidclip->mScaleV);
                    attribute->SetPositionOffsetH(vidclip->mPositionOffsetH);
                    attribute->SetPositionOffsetV(vidclip->mPositionOffsetV);
                    attribute->SetRotationAngle(vidclip->mRotationAngle);
                    attribute->SetCropMarginL(vidclip->mCropMarginL);
                    attribute->SetCropMarginT(vidclip->mCropMarginT);
                    attribute->SetCropMarginR(vidclip->mCropMarginR);
                    attribute->SetCropMarginB(vidclip->mCropMarginB);
                    attribute->SetKeyPoint(vidclip->mAttributeKeyPoints);
                }
            }
        }
        else if (IS_AUDIO(track->mType))
        {
            MediaCore::AudioTrack::Holder audTrack = mMtaReader->AddTrack(track->mID);
            audTrack->SetMuted(!track->mView);
            for (auto clip : track->m_Clips)
            {
                if (IS_DUMMY(clip->mType) || !clip->mMediaParser)
                    continue;
                MediaCore::AudioClip::Holder hAudClip = audTrack->AddNewClip(
                    clip->mID, clip->mMediaParser,
                    clip->mStart, clip->mStartOffset, clip->mEndOffset);
                BluePrintAudioFilter* bpaf = new BluePrintAudioFilter(this);
                bpaf->SetBluePrintFromJson(clip->mFilterBP);
                bpaf->SetKeyPoint(clip->mFilterKeyPoints);
                MediaCore::AudioFilter::Holder hFilter(bpaf);
                hAudClip->SetFilter(hFilter);
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

    mMtvReader->SeekTo(currentTime, true);
    mMtaReader->SeekTo(currentTime, false);
    SyncDataLayer(true);
    return 0;
}

void TimeLine::Save(imgui_json::value& value)
{
    // save media clip
    imgui_json::value media_clips;
    for (auto clip : m_Clips)
    {
        imgui_json::value media_clip;
        clip->Save(media_clip);
        media_clips.push_back(media_clip);
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
    value["VideoWidth"] = imgui_json::number(mWidth);
    value["VideoHeight"] = imgui_json::number(mHeight);
    value["PreviewScale"] = imgui_json::number(mPreviewScale);
    value["FrameRateNum"] = imgui_json::number(mFrameRate.num);
    value["FrameRateDen"] = imgui_json::number(mFrameRate.den);
    value["AudioChannels"] = imgui_json::number(mAudioChannels);
    value["AudioSampleRate"] = imgui_json::number(mAudioSampleRate);
    value["AudioFormat"] = imgui_json::number(mAudioFormat);
    value["msPixelWidth"] = imgui_json::number(msPixelWidthTarget);
    value["FirstTime"] = imgui_json::number(firstTime);
    value["CurrentTime"] = imgui_json::number(currentTime);
    value["MarkIn"] = imgui_json::number(mark_in);
    value["MarkOut"] = imgui_json::number(mark_out);
    value["PreviewForward"] = imgui_json::boolean(mIsPreviewForward);
    value["Loop"] = imgui_json::boolean(bLoop);
    value["Compare"] = imgui_json::boolean(bCompare);
    value["FilterOutPreview"] = imgui_json::boolean(bFilterOutputPreview);
    value["FusionOutPreview"] = imgui_json::boolean(bFusionOutputPreview);
    value["AttributeOutPreview"] = imgui_json::boolean(bAttributeOutputPreview);
    value["SelectLinked"] = imgui_json::boolean(bSelectLinked);
    value["IDGenerateState"] = imgui_json::number(m_IDGenerator.State());
    value["FontName"] = mFontName;
    value["OutputName"] = mOutputName;
    value["OutputPath"] = mOutputPath;
    value["OutputVideoCode"] = mVideoCodec;
    value["OutputAudioCode"] = mAudioCodec;
    value["OutputVideo"] = imgui_json::boolean(bExportVideo);
    value["OutputAudio"] = imgui_json::boolean(bExportAudio);
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
    if (mUiActions.empty())
        return;

    PrintActionList("UiActions", mUiActions);
    for (auto& action : mUiActions)
    {
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
        int64_t trackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId, true);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        Clip* clip = FindClipByID(clipId);
        MediaCore::VideoClip::Holder vidClip = MediaCore::VideoClip::CreateVideoInstance(
            clip->mID, clip->mMediaParser,
            vidTrack->OutWidth(), vidTrack->OutHeight(), vidTrack->FrameRate(),
            clip->mStart, clip->mStartOffset, clip->mEndOffset, currentTime-clip->mStart, vidTrack->Direction());
        BluePrintVideoFilter* bpvf = new BluePrintVideoFilter(this);
        bpvf->SetBluePrintFromJson(clip->mFilterBP);
        bpvf->SetKeyPoint(clip->mFilterKeyPoints);
        MediaCore::VideoFilter::Holder hFilter(bpvf);
        vidClip->SetFilter(hFilter);
        auto attribute = vidClip->GetTransformFilter();
        if (attribute)
        {
            VideoClip * vidclip = (VideoClip *)clip;
            attribute->SetScaleType(vidclip->mScaleType);
            attribute->SetScaleH(vidclip->mScaleH);
            attribute->SetScaleV(vidclip->mScaleV);
            attribute->SetPositionOffsetH(vidclip->mPositionOffsetH);
            attribute->SetPositionOffsetV(vidclip->mPositionOffsetV);
            attribute->SetRotationAngle(vidclip->mRotationAngle);
            attribute->SetCropMarginL(vidclip->mCropMarginL);
            attribute->SetCropMarginT(vidclip->mCropMarginT);
            attribute->SetCropMarginR(vidclip->mCropMarginR);
            attribute->SetCropMarginB(vidclip->mCropMarginB);
            attribute->SetKeyPoint(vidclip->mAttributeKeyPoints);
        }
        vidTrack->InsertClip(vidClip);
        UpdatePreview();
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        vidTrack->RemoveClipById(clipId);
        UpdatePreview();
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
        UpdatePreview();
    }
    else if (actionName == "CROP_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStartOffset = action["new_start_offset"].get<imgui_json::number>();
        int64_t newEndOffset = action["new_end_offset"].get<imgui_json::number>();
        vidTrack->ChangeClipRange(clipId, newStartOffset, newEndOffset);
        UpdatePreview();
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
        uint32_t track_type2 = action["media_type2"].get<imgui_json::number>();
        if (IS_VIDEO(track_type2))
        {
            int64_t trackId1 = action["track_id1"].get<imgui_json::number>();
            int64_t trackId2 = action["track_id2"].get<imgui_json::number>();
            mMtvReader->ChangeTrackViewOrder(trackId1, trackId2);
            UpdatePreview();
        }
    }
    else if (actionName == "HIDE_TRACK")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        bool visible = action["visible"].get<imgui_json::boolean>();
        mMtvReader->SetTrackVisible(trackId, visible);
        UpdatePreview();
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
        int64_t trackId = action["to_track_id"].get<imgui_json::number>();
        MediaCore::AudioTrack::Holder audTrack = mMtaReader->GetTrackById(trackId, true);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        Clip* clip = FindClipByID(clipId);
        MediaCore::AudioClip::Holder audClip = MediaCore::AudioClip::CreateInstance(
            clip->mID, clip->mMediaParser,
            audTrack->OutChannels(), audTrack->OutSampleRate(), audTrack->OutSampleFormat(),
            clip->mStart, clip->mStartOffset, clip->mEndOffset);
        BluePrintAudioFilter* bpaf = new BluePrintAudioFilter(this);
        bpaf->SetBluePrintFromJson(clip->mFilterBP);
        bpaf->SetKeyPoint(clip->mFilterKeyPoints);
        MediaCore::AudioFilter::Holder hFilter(bpaf);
        audClip->SetFilter(hFilter);
        audTrack->InsertClip(audClip);
        mMtaReader->Refresh();
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::AudioTrack::Holder audTrack = mMtaReader->GetTrackById(trackId);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        audTrack->RemoveClipById(clipId);
        mMtaReader->Refresh();
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
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::AudioTrack::Holder audTrack = mMtaReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStartOffset = action["new_start_offset"].get<imgui_json::number>();
        int64_t newEndOffset = action["new_end_offset"].get<imgui_json::number>();
        audTrack->ChangeClipRange(clipId, newStartOffset, newEndOffset);
        mMtaReader->Refresh();
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
        Clip* clip = FindClipByID(clipId);
        MediaCore::VideoClip::Holder imgClip = MediaCore::VideoClip::CreateImageInstance(
            clip->mID, clip->mMediaParser,
            vidTrack->OutWidth(), vidTrack->OutHeight(), clip->mStart, clip->mLength);
        vidTrack->InsertClip(imgClip);
        UpdatePreview();
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_json"]["ID"].get<imgui_json::number>();
        vidTrack->RemoveClipById(clipId);
        UpdatePreview();
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
        UpdatePreview();
    }
    else if (actionName == "CROP_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        MediaCore::VideoTrack::Holder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        int64_t newStart = action["new_start"].get<imgui_json::number>();
        int64_t newEnd = action["new_end"].get<imgui_json::number>();
        vidTrack->ChangeClipRange(clipId, newStart, newEnd);
        UpdatePreview();
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
        // currently need to do nothing
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

void TimeLine::ConfigureDataLayer()
{
    mMtvReader = MediaCore::MultiTrackVideoReader::CreateInstance();
    mMtvReader->Configure(GetPreviewWidth(), GetPreviewHeight(), mFrameRate);
    mMtvReader->Start();
    mMtaReader = MediaCore::MultiTrackAudioReader::CreateInstance();
    mMtaReader->Configure(mAudioChannels, mAudioSampleRate);
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
        auto ovlpIter = vidTrack->OverlapListBegin();
        while (ovlpIter != vidTrack->OverlapListEnd())
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
                        bpvt->SetBluePrintFromJson(ovlp->mFusionBP);
                        bpvt->SetKeyPoint(ovlp->mFusionKeyPoints);
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
                        bpat->SetBluePrintFromJson(ovlp->mFusionBP);
                        bpat->SetKeyPoint(ovlp->mFusionKeyPoints);
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
        UpdatePreview();
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
    if (!hSsGen->Open(mi->mMediaOverview->GetMediaParser()))
    {
        Logger::Log(Logger::Error) << hSsGen->GetError() << std::endl;
        return nullptr;
    }
    auto video_info = mi->mMediaOverview->GetMediaParser()->GetBestVideoStream();
    float snapshot_scale = video_info->height > 0 ? DEFAULT_VIDEO_TRACK_HEIGHT / (float)video_info->height : 0.05;
    hSsGen->SetSnapshotResizeFactor(snapshot_scale, snapshot_scale);
    hSsGen->SetCacheFactor(9);
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
    MediaCore::Ratio timelineAspectRatio = { (int32_t)(mWidth), (int32_t)(mHeight) };
    mSnapShotWidth = DEFAULT_VIDEO_TRACK_HEIGHT * (float)timelineAspectRatio.num / (float)timelineAspectRatio.den;
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
                mAudioAttribute.m_audio_vector.draw_dot(x, y, ImPixel(r / 255.0, g / 255.0, b / 255.0, 1.f));
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
    std::string vidEncImgFormat;
    if (!mEncoder->ConfigureVideoStream(
        vidEncParams.codecName, vidEncParams.imageFormat, vidEncParams.width, vidEncParams.height,
        vidEncParams.frameRate, vidEncParams.bitRate, &vidEncParams.extraOpts))
    {
        errMsg = mEncoder->GetError();
        return false;
    }
    mEncMtvReader = mMtvReader->CloneAndConfigure(vidEncParams.width, vidEncParams.height, vidEncParams.frameRate);

    // Audio
    std::string audEncSmpFormat;
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
}

void TimeLine::StopEncoding()
{
    if (!mEncodingThread.joinable())
        return;
    mQuitEncoding = true;
    mEncodingThread.join();
    mEncodingThread = std::thread();

    mEncMtvReader = nullptr;
    mEncMtaReader = nullptr;
}

void TimeLine::_EncodeProc()
{
    Logger::Log(Logger::DEBUG) << ">>>>>>>>>>> Enter encoding proc >>>>>>>>>>>>" << std::endl;
    mEncoder->Start();
    bool vidInputEof = false;
    bool audInputEof = false;
    double audpos = 0, vidpos = 0;
    double maxEncodeDuration = 0;
    uint32_t vidFrameCount = 0;
    MediaCore::Ratio outFrameRate = mEncoder->GetVideoFrameRate();
    ImGui::ImMat vmat, amat;
    uint32_t pcmbufSize = 8192;
    uint8_t* pcmbuf = new uint8_t[pcmbufSize];
    double viddur = (double)mEncMtvReader->Duration() / 1000;
    double dur = (double)ValidDuration() / 1000;
    double encpos = 0;
    double enc_start = (double)mEncoding_start / 1000;
    double enc_end = (double)mEncoding_end / 1000;
    if (mEncMtaReader) mEncMtaReader->SeekTo(mEncoding_start);
    while (!mQuitEncoding && (!vidInputEof || !audInputEof))
    {
        bool idleLoop = true;
        if ((!vidInputEof && vidpos <= audpos) || audInputEof)
        {
            vidpos = (double)vidFrameCount * outFrameRate.den / outFrameRate.num + enc_start;
            bool eof = vidpos >= enc_end; //viddur;
            if (!eof)
            {
                if (!mEncMtvReader->ReadVideoFrame((int64_t)(vidpos * 1000), vmat))
                {
                    std::ostringstream oss;
                    oss << "[video] '" << mEncMtvReader->GetError() << "'.";
                    mEncodeProcErrMsg = oss.str();
                    break;
                }
                if (!vmat.empty())
                {
                    vidFrameCount++;
                    vmat.time_stamp = vidpos - enc_start;
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
                        mEncodingProgress = (encpos - enc_start) / dur;
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
            if (audpos > enc_end) eof = true;
            if (!eof && !amat.empty())
            {
                audpos = amat.time_stamp;
                amat.time_stamp -= enc_start;
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
                    mEncodingProgress = (encpos - enc_start) / dur;
                }
            }
            else
            {
                amat.release();
                {
                    std::lock_guard<std::mutex> lk(mEncodingMutex);
                    mEncodingAFrame = amat;
                }
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
            int64_t trackId1 = action["track_id1"].get<imgui_json::number>();
            int64_t trackId2 = action["track_id2"].get<imgui_json::number>();
            int index = -1;
            for (int i = 0; i < m_Tracks.size(); i ++)
            {
                if (m_Tracks[i]->mID == trackId1)
                {
                    index = i;
                    break;
                }
            }
            int indexDst = -1;
            for (int i = 0; i < m_Tracks.size(); i ++)
            {
                if (m_Tracks[i]->mID == trackId2)
                {
                    indexDst = i;
                    break;
                }
            }
            MovingTrack(index, indexDst, nullptr);
            imgui_json::value undoAction;
            undoAction["action"] = "MOVE_TRACK";
            undoAction["media_type"] = action["media_type2"];
            undoAction["track_id1"] = action["track_id2"];
            undoAction["media_type2"] = action["media_type"];
            undoAction["track_id2"] = action["track_id1"];
            mUiActions.push_back(std::move(undoAction));
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
            clip->mStart = orgStart;
            clip->mEnd = clip->mStart+clip->mLength;
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
                startDiff = newStartOffset-orgStartOffset;
                endDiff = newEndOffset-orgEndOffset;
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
            if (endDiff)
                clip->Cropping(endDiff, 1);

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
            int64_t trackId1 = action["track_id1"].get<imgui_json::number>();
            int64_t trackId2 = action["track_id2"].get<imgui_json::number>();
            int index = -1;
            for (int i = 0; i < m_Tracks.size(); i ++)
            {
                if (m_Tracks[i]->mID == trackId1)
                {
                    index = i;
                    break;
                }
            }
            int indexDst = -1;
            for (int i = 0; i < m_Tracks.size(); i ++)
            {
                if (m_Tracks[i]->mID == trackId2)
                {
                    indexDst = i;
                    break;
                }
            }
            MovingTrack(index, indexDst, nullptr);
            mUiActions.push_back(action);
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
            clip->mStart = newStart;
            clip->mEnd = newStart+clip->mLength;
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
                clip->Cropping(startDiff, 0);
            if (endDiff)
                clip->Cropping(endDiff, 1);

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
        else
        {
            Logger::Log(Logger::WARN) << "Unhandled REDO action '" << actionName << "'!" << std::endl;
        }
    }
    return true;
}

int64_t TimeLine::AddNewClip(const imgui_json::value& clip_json, int64_t track_id, std::list<imgui_json::value>* pActionList)
{
    MediaTrack* track = FindTrackByID(track_id);
    if (!track)
    {
        Logger::Log(Logger::Error) << "FAILED to invoke 'TimeLine::AddNewClip()'! Target 'MediaTrack' does NOT exist." << std::endl;
        return -1;
    }
    const uint32_t mediaType = clip_json["Type"].get<imgui_json::number>();
    Clip* newClip = nullptr;
    switch (mediaType)
    {
    case MEDIA_VIDEO:
        newClip = VideoClip::Load(clip_json, this);
        break;
    case MEDIA_AUDIO:
        newClip = AudioClip::Load(clip_json, this);
        break;
    case MEDIA_TEXT:
        newClip = TextClip::Load(clip_json, this);
        break;
    }
    m_Clips.push_back(newClip);
    track->InsertClip(newClip, newClip->mStart, true, pActionList);

    int64_t groupId = clip_json["GroupID"].get<imgui_json::number>();
    if (groupId != -1)
    {
        auto iter = std::find_if(m_Groups.begin(), m_Groups.end(), [groupId] (auto& group) {
            return group.mID == groupId;
        });
        if (iter == m_Groups.end())
            NewGroup(newClip, groupId, 0, pActionList);
        else
            AddClipIntoGroup(newClip, groupId, pActionList);
    }
    return newClip->mID;
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
    if (IS_VIDEO(media_type) && !IS_IMAGE(media_type))
    {
        MediaCore::Snapshot::Viewer::Holder hViewer;
        MediaCore::Snapshot::Generator::Holder hSsGen = GetSnapshotGenerator(item->mID);
        if (hSsGen) hViewer = hSsGen->CreateViewer();
        newClip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName+":Video", item->mMediaOverview->GetMediaParser(), hViewer, this);
    }
    else if (IS_IMAGE(media_type))
    {
        newClip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, this);
    }
    else if (IS_AUDIO(media_type))
    {
        newClip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName+":Audio", item->mMediaOverview, this);
    }
    else if (IS_TEXT(media_type))
    {
        newClip = new TextClip(start, end, track->mID, track->mName, "", this);
    }
    else
    {
        Logger::Log(Logger::Error) << "Unsupported media type " << media_type << " for TimeLine::AddNewClip()!" << std::endl;
        return -1;
    }

    if (clip_id != -1)
        newClip->mID = clip_id;
    newClip->mStart = start;
    if (IS_IMAGE(media_type) || IS_TEXT(media_type))
    {
        newClip->mStartOffset = 0;
        newClip->mEndOffset = 0;
    }
    else
    {
        newClip->mStartOffset = start_offset;
        newClip->mEndOffset = end_offset;
    }
    newClip->mLength -= start_offset+end_offset;
    newClip->mEnd = start+newClip->mLength;
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
} // namespace MediaTimeline/TimeLine

namespace MediaTimeline
{
/***********************************************************************************************************
 * Draw Main Timeline
 ***********************************************************************************************************/
bool DrawTimeLine(TimeLine *timeline, bool *expanded, bool editable)
{
    /************************************************************************************************************
    * [-]------------------------------------ header area ----------------------------------------------------- +
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
    bool ret = false;
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    int scrollSize = 16;
    int trackHeadHeight = 16;
    int HeadHeight = 28;
    int legendWidth = 200;
    int trackCount = timeline->GetTrackCount();
    int64_t duration = ImMax(timeline->GetEnd() - timeline->GetStart(), (int64_t)1);
    ImVector<TimelineCustomDraw> customDraws;
    static bool MovingHorizonScrollBar = false;
    static bool MovingVerticalScrollBar = false;
    static bool MovingCurrentTime = false;
    static int trackMovingEntry = -1;
    static int trackEntry = -1;
    static int trackMenuEntry = -1;
    static int64_t clipMenuEntry = -1;
    static int64_t clipMovingEntry = -1;
    static int clipMovingPart = -1;
    int delTrackEntry = -1;
    int mouseEntry = -1;
    int legendEntry = -1;
    int64_t mouseClip = -1;
    int64_t mouseTime = -1;
    int alignedMousePosX = -1;
    static int64_t menuMouseTime = -1;
    std::vector<int64_t> delClipEntry;
    std::vector<int64_t> groupClipEntry;
    std::vector<int64_t> unGroupClipEntry;
    bool removeEmptyTrack = false;
    int insertEmptyTrackType = MEDIA_UNKNOWN;
    static bool menuIsOpened = false;
    static bool bCutting = false;
    static bool bCropping = false;
    static bool bMoving = false;
    bCutting = ImGui::IsKeyDown(ImGuiKey_LeftAlt) && (io.KeyMods == ImGuiMod_Alt);
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
        return ret;
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

    float minPixelWidthTarget = ImMin(timeline->msPixelWidthTarget, (float)(timline_size.x - legendWidth) / (float)duration);
    float frame_duration = (timeline->mFrameRate.den > 0 && timeline->mFrameRate.num > 0) ? timeline->mFrameRate.den * 1000.0 / timeline->mFrameRate.num : 40;
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

    // draw backbround
    //draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DARK_TWO);

    ImGui::BeginGroup();
    bool isFocused = ImGui::IsWindowFocused();

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
            ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
            ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
            draw_list->AddText(tips_pos, IM_COL32(255, 255, 255, 128), tips_string.c_str());
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.0);
            if (regionRect.Contains(io.MousePos) && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
            {
                ImGui::OpenPopup("##empty-timeline-context-menu");
            }
            if (ImGui::BeginPopup("##empty-timeline-context-menu"))
            {
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(255,255,255,255));
            
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
        ImVec2 headerSize(timline_size.x - 4.f, (float)HeadHeight);
        ImVec2 HorizonScrollBarSize(timline_size.x, scrollSize);
        ImVec2 VerticalScrollBarSize(scrollSize / 2, canvas_size.y - scrollSize - HeadHeight);
        ImRect HeaderAreaRect(canvas_pos + ImVec2(legendWidth, 0), canvas_pos + headerSize);
        ImGui::InvisibleButton("topBar", headerSize);

        // draw Header bg
        draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Max, COL_DARK_ONE, 0);
        
        if (!trackCount) 
        {
            ImGui::EndGroup();
            return ret;
        }
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImVec2 childFramePos = ImGui::GetCursorScreenPos();
        ImVec2 childFrameSize(timline_size.x, timline_size.y - 8.0f - headerSize.y - HorizonScrollBarSize.y);
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
        const ImRect topRect(ImVec2(contentMin.x + legendWidth, canvas_pos.y), ImVec2(contentMin.x + timline_size.x, canvas_pos.y + HeadHeight));

        const float contentHeight = contentMax.y - contentMin.y;
        // full canvas background
        draw_list->AddRectFilled(canvas_pos + ImVec2(4, HeadHeight + 4), canvas_pos + ImVec2(4, HeadHeight + 4) + timline_size - ImVec2(8, HeadHeight + scrollSize + 8), COL_CANVAS_BG, 0);
        // full legend background
        draw_list->AddRectFilled(legendRect.Min, legendRect.Max, COL_LEGEND_BG, 0);

        // for debug
        //draw_list->AddRect(trackRect.Min, trackRect.Max, IM_COL32(255, 0, 0, 255), 0, 0, 2);
        //draw_list->AddRect(trackAreaRect.Min, trackAreaRect.Max, IM_COL32(0, 0, 255, 255), 0, 0, 2);
        //draw_list->AddRect(legendRect.Min, legendRect.Max, IM_COL32(0, 255, 0, 255), 0, 0, 2);
        //draw_list->AddRect(legendAreaRect.Min, legendAreaRect.Max, IM_COL32(255, 255, 0, 255), 0, 0, 2);
        // for debug end

        // calculate mouse pos to time
        mouseTime = (int64_t)((cx - contentMin.x - legendWidth) / timeline->msPixelWidthTarget) + timeline->firstTime;
        //timeline->AlignTime(mouseTime);
        auto alignedTime = mouseTime; timeline->AlignTime(alignedTime);
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
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)timeStart), ImVec2((float)px, canvas_pos.y + (float)timeEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
            }
            if (baseIndex && px > (contentMin.x + legendWidth))
            {
                auto time_str = ImGuiHelper::MillisecToString(i, 2);
                ImGui::SetWindowFontScale(0.8);
                draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y + 8), COL_RULE_TEXT, time_str.c_str());
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
        if (timeline->currentTime >= timeline->firstTime && timeline->currentTime <= timeline->GetEnd())
        {
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + legendWidth + (timeline->currentTime - timeline->firstTime) * timeline->msPixelWidthTarget - arrowWidth * 0.5f + 1;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = ImGuiHelper::MillisecToString(timeline->currentTime, 2);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + legendWidth + (timeline->currentTime - timeline->firstTime) * timeline->msPixelWidthTarget - str_size.x * 0.5f + 1;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
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
            if (cy >= pos.y && cy < pos.y + (trackHeadHeight + localCustomHeight) && clipMovingEntry == -1 && cx > contentMin.x && cx < contentMin.x + timline_size.x)
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
            bool is_delete = TimelineButton(draw_list, ICON_TRASH, ImVec2(contentMin.x + legendWidth - button_size.x - 4, tpos.y + 2), button_size, "delete");
            if (is_delete && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
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
            if (ImRect(slotP1, slotP2).Contains(io.MousePos))
            {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    timeline->DoubleClick(i, mouseTime);
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
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingHorizonScrollBar && !MovingCurrentTime && !menuIsOpened && editable)
                {
                    trackMovingEntry = legendEntry;
                }
            }

            // Ensure grabable handles and find selected clip
            if (mouseTime != -1 && mouseEntry >= 0 && mouseEntry < timeline->m_Tracks.size())
            {
                MediaTrack * track = timeline->m_Tracks[mouseEntry];
                if (track && !track->mLocked)
                {
                    int count = 0;
                    auto clip = track->FindClips(mouseTime, count);
                    if (clip && clipMovingEntry == -1)
                    {
                        // check clip moving part
                        ImVec2 clipP1(pos.x + clip->mStart * timeline->msPixelWidthTarget, pos.y + 2);
                        ImVec2 clipP2(pos.x + clip->mEnd * timeline->msPixelWidthTarget, pos.y + trackHeadHeight - 2);
                        ImVec2 clipP3(pos.x + clip->mEnd * timeline->msPixelWidthTarget, pos.y + trackHeadHeight - 2 + localCustomHeight);
                        const float max_handle_width = clipP2.x - clipP1.x / 3.0f;
                        const float min_handle_width = ImMin(10.0f, max_handle_width);
                        const float handle_width = ImClamp(timeline->msPixelWidthTarget / 2.0f, min_handle_width, max_handle_width);
                        ImRect rects[3] = {ImRect(clipP1, ImVec2(clipP1.x + handle_width, clipP2.y)), ImRect(ImVec2(clipP2.x - handle_width, clipP1.y), clipP2), ImRect(clipP1 + ImVec2(handle_width, 0), clipP3 - ImVec2(handle_width, 0))};

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
                            ImRect &rc = rects[j];
                            if (!rc.Contains(io.MousePos))
                                continue;
                            if (!ImRect(childFramePos, childFramePos + childFrameSize).Contains(io.MousePos))
                                continue;
                            if (j == 2 && bCutting && count <= 1)
                            {
                                ImGui::RenderMouseCursor(ICON_CUTTING, ImVec2(7, 0), 1.0, -90);
                            }
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingHorizonScrollBar && !MovingCurrentTime && !menuIsOpened && editable)
                            {
                                if (j == 2 && bCutting && count <= 1)
                                {
                                    doCutPos = mouseTime;
                                }
                                else
                                {
                                    clipMovingEntry = clip->mID;
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
                if (track)
                {
                    for (auto clip : track->m_Clips)
                    {
                        if (clip->mStart <= mouseTime && clip->mEnd >= mouseTime)
                        {
                            mouseClip = clip->mID;
                            break;
                        }
                    }
                }
            }
            customHeight += localCustomHeight;
        }
        // handle cut operation
        if (doCutPos > 0)
        {
            vector<Clip*> clips(timeline->m_Clips);
            for (auto clip : clips)
            {
                if (clip->bSelected)
                    clip->Cutting(doCutPos, &timeline->mUiActions);
            }
        }

        // check mouseEntry is below track area
        if (mouseEntry < 0 && trackAreaRect.Contains(io.MousePos) && cy >= trackRect.Max.y)
        {
            mouseEntry = -2;
        }

        // track moving
        if (trackMovingEntry != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImGui::CaptureMouseFromApp();
            if (cy > legendAreaRect.Min.y && cy < legendAreaRect.Max.y)
            {
                trackEntry = legendEntry;
                if (trackEntry == -1 && cy >= legendRect.Max.y)
                {
                    trackEntry = -2; // end of tracks
                }
            }
            //if (ImGui::BeginTooltip())
            //{
            //    ImGui::Text("Draging track:%s", std::to_string(trackMovingEntry).c_str());
            //    ImGui::Text("currrent track:%s", std::to_string(trackEntry).c_str());
            //    ImGui::EndTooltip();
            //}
        }

        // clip cropping or moving
        if (clipMovingEntry != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImGui::CaptureMouseFromApp();
            int64_t diffTime = int64_t(io.MouseDelta.x / timeline->msPixelWidthTarget);
            if (std::abs(diffTime) > 0)
            {
                Clip * clip = timeline->FindClipByID(clipMovingEntry);
                if (clip && clip->bSelected)
                {
                    if (clipMovingPart == 3)
                    {
                        bMoving = true;
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                        // whole slot moving
                        int dst_entry = clip->Moving(diffTime, mouseEntry);
                        if (dst_entry >= 0)
                            mouseEntry = dst_entry;
                    }
                    else if (clipMovingPart & 1)
                    {
                        // clip left moving
                        clip->Cropping(diffTime, 0);
                    }
                    else if (clipMovingPart & 2)
                    {
                        // clip right moving
                        clip->Cropping(diffTime, 1);
                    }
                }
            }
        }

        // mark moving
        if (markMovingEntry != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImGui::CaptureMouseFromApp();
            int64_t mouse_time = (int64_t)((io.MousePos.x - topRect.Min.x) / timeline->msPixelWidthTarget) + timeline->firstTime;
            if (markMovingEntry == 0)
            {
                timeline->mark_in = mouse_time;
                if (timeline->mark_in < 0) timeline->mark_in = 0;
                if (timeline->mark_in >= timeline->mark_out) timeline->mark_out = -1;
            }
            else if (markMovingEntry == 1)
            {
                timeline->mark_out = mouse_time;
                if (timeline->mark_out < 0) timeline->mark_out = 0;
                if (timeline->mark_out <= timeline->mark_in) timeline->mark_in = -1;
            }
            else
            {
                int64_t diffTime = int64_t(io.MouseDelta.x / timeline->msPixelWidthTarget);
                if (timeline->mark_in + diffTime < timeline->mStart) diffTime = timeline->mark_in - timeline->mStart;
                if (timeline->mark_out + diffTime > timeline->mEnd) diffTime = timeline->mEnd - timeline->mark_out;
                timeline->mark_in += diffTime;
                timeline->mark_out += diffTime;
            }
            if (ImGui::BeginTooltip())
            {
                ImGui::Text(" In:%s", ImGuiHelper::MillisecToString(timeline->mark_in, 2).c_str());
                ImGui::Text("Out:%s", ImGuiHelper::MillisecToString(timeline->mark_out, 2).c_str());
                ImGui::Text("Dur:%s", ImGuiHelper::MillisecToString(timeline->mark_out - timeline->mark_in, 2).c_str());
                ImGui::EndTooltip();
            }
        }

        if (trackAreaRect.Contains(io.MousePos) && editable && !menuIsOpened && !bCropping && !bCutting && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            timeline->Click(mouseEntry, mouseTime);

        if (trackAreaRect.Contains(io.MousePos) && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
        {
            if (mouseClip != -1)
                clipMenuEntry = mouseClip;
            if (mouseEntry >= 0)
                trackMenuEntry = mouseEntry;
            if (mouseTime != -1)
                menuMouseTime = mouseTime;
            ImGui::OpenPopup("##timeline-context-menu");
            menuIsOpened = true;
        }
        if (HeaderAreaRect.Contains(io.MousePos) && !menuIsOpened && !bCropping && !bCutting && ImGui::IsMouseDown(ImGuiMouseButton_Right))
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
                int64_t mouse_time = (int64_t)((headerMarkPos - topRect.Min.x) / timeline->msPixelWidthTarget) + timeline->firstTime;
                if (ImGui::MenuItem("+ Add mark in", nullptr, nullptr))
                {
                    if (timeline->mark_out != -1 && mouse_time > timeline->mark_out)
                        timeline->mark_out = -1;
                    timeline->mark_in = mouse_time;
                    headerMarkPos = -1;
                }
                if (ImGui::MenuItem("+ Add mark out", nullptr, nullptr))
                {
                    if (timeline->mark_in != -1 && mouse_time < timeline->mark_in)
                        timeline->mark_in = -1;
                    timeline->mark_out = mouse_time;
                    headerMarkPos = -1;
                }
                if (ImGui::MenuItem("- Delete mark point", nullptr, nullptr))
                {
                    timeline->mark_in = timeline->mark_out = -1;
                    headerMarkPos = -1;
                }
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("##timeline-context-menu"))
        {
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(255,255,255,255));
            auto selected_clip_count = timeline->GetSelectedClipCount();
            auto empty_track_count = timeline->GetEmptyTrackCount();
            
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

            if (empty_track_count > 0)
            {
                if (ImGui::MenuItem(ICON_MEDIA_DELETE " Delete Empty Track", nullptr, nullptr))
                {
                    removeEmptyTrack = true;
                }
            }

            if (clipMenuEntry != -1)
            {
                ImGui::Separator();
                auto clip = timeline->FindClipByID(clipMenuEntry);
                auto track = timeline->FindTrackByClipID(clip->mID);
                bool disable_editing = IS_DUMMY(clip->mType);
                ImGui::BeginDisabled(disable_editing);
                if (ImGui::MenuItem(ICON_CROP " Edit Clip Attribute", nullptr, nullptr))
                {
                    track->SelectEditingClip(clip, false);
                }
                if (ImGui::MenuItem(ICON_BLUE_PRINT " Edit Clip Filter", nullptr, nullptr))
                {
                    track->SelectEditingClip(clip, true);
                }
                ImGui::EndDisabled();
                if (ImGui::MenuItem(ICON_MEDIA_DELETE_CLIP " Delete Clip", nullptr, nullptr))
                {
                    delClipEntry.push_back(clipMenuEntry);
                }
                if (clip->mGroupID != -1 && ImGui::MenuItem(ICON_MEDIA_UNGROUP " Ungroup Clip", nullptr, nullptr))
                {
                    unGroupClipEntry.push_back(clipMenuEntry);
                }
            }

            if (selected_clip_count > 0)
            {
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MEDIA_DELETE_CLIP " Delete Selected", nullptr, nullptr))
                {
                    for (auto clip : timeline->m_Clips)
                    {
                        if (clip->bSelected)
                            delClipEntry.push_back(clip->mID);
                    }
                }
                if (selected_clip_count > 1 && ImGui::MenuItem(ICON_MEDIA_GROUP " Group Selected", nullptr, nullptr))
                {
                    for (auto clip : timeline->m_Clips)
                    {
                        if (clip->bSelected)
                            groupClipEntry.push_back(clip->mID);
                    }
                }
                if (ImGui::MenuItem(ICON_MEDIA_UNGROUP " Ungroup Selected", nullptr, nullptr))
                {
                    for (auto clip : timeline->m_Clips)
                    {
                        if (clip->bSelected)
                            unGroupClipEntry.push_back(clip->mID);
                    }
                }
            }

            if (trackMenuEntry >= 0 && clipMenuEntry < 0)
            {
                auto track = timeline->m_Tracks[trackMenuEntry];
                if (IS_TEXT(track->mType))
                {
                    ImGui::Separator();
                    if (ImGui::MenuItem(ICON_MEDIA_TEXT  " Add Text", nullptr, nullptr))
                    {
                        if (track->mMttReader && menuMouseTime != -1)
                        {
                            int64_t text_time = menuMouseTime;
                            timeline->AlignTime(text_time);
                            TextClip * clip = new TextClip(text_time, text_time + 5000, track->mID, track->mName, std::string(""), timeline);
                            clip->CreateClipHold(track);
                            clip->SetClipDefault(track->mMttReader->DefaultStyle());
                            timeline->m_Clips.push_back(clip);
                            track->InsertClip(clip, text_time);
                            track->SelectEditingClip(clip, false);
                            if (timeline->m_CallBacks.EditingClipFilter)
                            {
                                timeline->m_CallBacks.EditingClipFilter(clip->mType, clip);
                            }
                        }
                    }
                }
            }

            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        ImGui::EndChildFrame();

        // Horizon Scroll bar control buttons
        auto horizon_scroll_pos = ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::SetWindowFontScale(0.7);
        int button_offset = 16;
        ImGui::SetCursorScreenPos(horizon_scroll_pos + ImVec2(legendWidth - button_offset - 4, 0));
        if (ImGui::Button(ICON_FAST_TO_END "##slider_to_end", ImVec2(16, 16)))
        {
            timeline->firstTime = timeline->GetEnd() - timeline->visibleTime;
            timeline->AlignTime(timeline->firstTime);
        }
        ImGui::ShowTooltipOnHover("Slider to End");

        button_offset += 16;
        ImGui::SetCursorScreenPos(horizon_scroll_pos + ImVec2(legendWidth - button_offset - 4, 0));
        if (ImGui::Button(ICON_SLIDER_MAXIMUM "##slider_maximum", ImVec2(16, 16)))
        {
            timeline->msPixelWidthTarget = maxPixelWidthTarget;
        }
        ImGui::ShowTooltipOnHover("Maximum Slider");

        button_offset += 16;
        ImGui::SetCursorScreenPos(horizon_scroll_pos + ImVec2(legendWidth - button_offset - 4, 0));
        if (ImGui::Button(ICON_ZOOM_IN "##slider_zoom_in", ImVec2(16, 16)))
        {
            timeline->msPixelWidthTarget *= 2.0f;
            if (timeline->msPixelWidthTarget > maxPixelWidthTarget)
                timeline->msPixelWidthTarget = maxPixelWidthTarget;
        }
        ImGui::ShowTooltipOnHover("Slider Zoom In");

        button_offset += 16;
        ImGui::SetCursorScreenPos(horizon_scroll_pos + ImVec2(legendWidth - button_offset - 4, 0));
        if (ImGui::Button(ICON_ZOOM_OUT "##slider_zoom_out", ImVec2(16, 16)))
        {
            timeline->msPixelWidthTarget *= 0.5f;
            if (timeline->msPixelWidthTarget < minPixelWidthTarget)
                timeline->msPixelWidthTarget = minPixelWidthTarget;
        }
        ImGui::ShowTooltipOnHover("Slider Zoom Out");

        button_offset += 16;
        ImGui::SetCursorScreenPos(horizon_scroll_pos + ImVec2(legendWidth - button_offset - 4, 0));
        if (ImGui::Button(ICON_SLIDER_MINIMUM "##slider_minimum", ImVec2(16, 16)))
        {
            timeline->msPixelWidthTarget = minPixelWidthTarget;
            timeline->firstTime = timeline->GetStart();
        }
        ImGui::ShowTooltipOnHover("Minimum Slider");

        button_offset += 16;
        ImGui::SetCursorScreenPos(horizon_scroll_pos + ImVec2(legendWidth - button_offset - 4, 0));
        if (ImGui::Button(ICON_FAST_TO_START "##slider_to_start", ImVec2(16, 16)))
        {
            timeline->firstTime = timeline->GetStart();
            timeline->AlignTime(timeline->firstTime);
        }
        ImGui::ShowTooltipOnHover("Slider to Start");
        ImGui::SetWindowFontScale(1.0);
        ImGui::PopStyleColor();

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
        HorizonScrollHandleBarRect = ImRect(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax);
        bool inHorizonScrollHandle = HorizonScrollHandleBarRect.Contains(io.MousePos);
        draw_list->AddRectFilled(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax, (inHorizonScrollBar || MovingHorizonScrollBar) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
        if (MovingHorizonScrollBar)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = false;
            }
            else
            {
                float msPerPixelInBar = HorizonBarPos / (float)timeline->visibleTime;
                timeline->firstTime = int((io.MousePos.x - panningViewHorizonSource.x) / msPerPixelInBar) - panningViewHorizonTime;
                timeline->AlignTime(timeline->firstTime);
                timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
            }
        }
        else if (inHorizonScrollHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && clipMovingEntry == -1 && !menuIsOpened && editable)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = true;
            panningViewHorizonSource = io.MousePos;
            panningViewHorizonTime = - timeline->firstTime;
        }
        else if (inHorizonScrollBar && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !menuIsOpened && editable)
        {
            float msPerPixelInBar = HorizonBarPos / (float)timeline->visibleTime;
            timeline->firstTime = int((io.MousePos.x - legendWidth - contentMin.x) / msPerPixelInBar);
            timeline->AlignTime(timeline->firstTime);
            timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
        }

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

        // handle mouse wheel event
        if (regionRect.Contains(io.MousePos) && !menuIsOpened && editable)
        {
            if (topRect.Contains(io.MousePos))
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
                    float offset = io.MouseWheel * 5 / VerticalBarHeightRatio + scroll_y;
                    offset = ImClamp(offset, 0.f, VerticalScrollMax);
                    ImGui::SetScrollY(VerticalWindow, offset);
                    panningViewVerticalPos = offset;
                }
            }
            if (overCustomDraw || overTrackView || overHorizonScrollBar || overTopBar)
            {
                // left-right wheel over blank area, moving canvas view
                if (io.MouseWheelH < -FLT_EPSILON)
                {
                    timeline->firstTime -= timeline->visibleTime / view_frames;
                    timeline->AlignTime(timeline->firstTime);
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                }
                else if (io.MouseWheelH > FLT_EPSILON)
                {
                    timeline->firstTime += timeline->visibleTime / view_frames;
                    timeline->AlignTime(timeline->firstTime);
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                }
                // up-down wheel over scrollbar, scale canvas view
                else if (io.MouseWheel < -FLT_EPSILON && timeline->visibleTime <= timeline->GetEnd())
                {
                    timeline->msPixelWidthTarget *= 0.9f;
                    timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    timeline->msPixelWidthTarget *= 1.1f;
                    timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
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
                    bMoving, !menuIsOpened && !bCutting && editable, bCutting, alignedMousePosX, &actionList);
        draw_list->PopClipRect();

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
            ongoingAction["org_start"] = imgui_json::number(clip->mStart);
            ongoingAction["org_end"] = imgui_json::number(clip->mEnd);
            ongoingAction["org_start_offset"] = imgui_json::number(clip->mStartOffset);
            ongoingAction["org_end_offset"] = imgui_json::number(clip->mEndOffset);
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
                    ongoingAction["org_start"] = imgui_json::number(pClip->mStart);
                    timeline->mOngoingActions.push_back(std::move(ongoingAction));
                }
            }
        }

        // time metric
        bool movable = true;
        if ((timeline->mVidFilterClip && timeline->mVidFilterClip->bSeeking) ||
            (timeline->mAudFilterClip && timeline->mAudFilterClip->bSeeking) ||
            menuIsOpened || !editable)
        {
            movable = false;
        }
        ImGui::SetCursorScreenPos(topRect.Min);
        ImGui::BeginChildFrame(ImGui::GetCurrentWindow()->GetID("#timeline metric"), topRect.GetSize(), ImGuiWindowFlags_NoScrollbar);

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
            if (handle_rect.Contains(io.MousePos))
            {
                draw_list->AddCircleFilled(HeaderAreaRect.Min + ImVec2(mark_in_offset + 2, 4), 4, COL_MARK_DOT_LIGHT);
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && markMovingEntry == -1)
                {
                    markMovingEntry = 0;
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
            if (handle_rect.Contains(io.MousePos))
            {
                draw_list->AddCircleFilled(HeaderAreaRect.Min + ImVec2(mark_out_offset + 2, 4), 4, COL_MARK_DOT_LIGHT);
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && markMovingEntry == -1)
                {
                    markMovingEntry = 1;
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
        
        if (mark_in_view && mark_rect.Contains(io.MousePos))
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && markMovingEntry == -1)
            {
                markMovingEntry = 2;
            }
        }
        ImGui::PopClipRect();

        // check current time moving
        if (movable && !MovingCurrentTime && markMovingEntry == -1 && !MovingHorizonScrollBar && clipMovingEntry == -1 && timeline->currentTime >= 0 && topRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = true;
        }
        if (MovingCurrentTime && duration)
        {
            auto old_time = timeline->currentTime;
            timeline->currentTime = (int64_t)((io.MousePos.x - topRect.Min.x) / timeline->msPixelWidthTarget) + timeline->firstTime;
            timeline->AlignTime(timeline->currentTime);
            if (timeline->currentTime < timeline->GetStart())
                timeline->currentTime = timeline->GetStart();
            if (timeline->currentTime >= timeline->GetEnd())
                timeline->currentTime = timeline->GetEnd();
            //if (old_time != timeline->currentTime)
            //    timeline->Seek(); // call seek event
            timeline->Seek(timeline->currentTime);
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
        if (trackCount > 0 && timeline->currentTime >= timeline->firstTime && timeline->currentTime <= timeline->GetEnd())
        {
            static const float cursorWidth = 2.f;
            float cursorOffset = contentMin.x + legendWidth + (timeline->currentTime - timeline->firstTime) * timeline->msPixelWidthTarget + 1;
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

        ImGui::PopStyleColor();
    }
    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        auto& ongoingActions = timeline->mOngoingActions;
        if (clipMovingEntry != -1 && !ongoingActions.empty())
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
                    if (fromTrackId != toTrack->mID || orgStart != clip->mStart)
                    {
                        action["to_track_id"] = imgui_json::number(toTrack->mID);
                        action["new_start"] = imgui_json::number(clip->mStart);
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
                    if (orgStartOffset != clip->mStartOffset || orgEndOffset != clip->mEndOffset ||
                        orgStart != clip->mStart || orgEnd != clip->mEnd)
                    {
                        action["new_start_offset"] = imgui_json::number(clip->mStartOffset);
                        action["new_end_offset"] = imgui_json::number(clip->mEndOffset);
                        action["new_start"] = imgui_json::number(clip->mStart);
                        action["new_end"] = imgui_json::number(clip->mEnd);
                        actionList.push_back(std::move(action));
                    }
                    else
                        Logger::Log(Logger::VERBOSE) << "-- Unchanged CROP action DISCARDED --" << std::endl;
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
        trackMovingEntry = -1;
        markMovingEntry = -1;
        trackEntry = -1;
        bCropping = false;
        bMoving = false;
        timeline->mConnectedPoints = -1;
        ImGui::CaptureMouseFromApp(false);
    }

    // Show help tips
    if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        if (mouseTime != -1 && mouseClip != -1 && mouseEntry >= 0 && mouseEntry < timeline->m_Tracks.size())
        {
            if (mouseClip != -1 && !bMoving && ImGui::BeginTooltip())
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
            else if (bMoving && ImGui::BeginTooltip())
            {
                ImGui::TextUnformatted("Help:");
                ImGui::TextUnformatted("    Hold left Command/Win key to single select");
                ImGui::EndTooltip();
            }
        }
        if ((overTrackView || overHorizonScrollBar) && !bMoving && ImGui::BeginTooltip())
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
                if (IS_IMAGE(item->mMediaType))
                {
                    VideoClip * new_image_clip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, timeline);
                    timeline->m_Clips.push_back(new_image_clip);
                    MediaTrack* insertTrack = track;
                    if (!track || !track->CanInsertClip(new_image_clip, mouseTime))
                    {
                        int newTrackIndex = timeline->NewTrack("", MEDIA_VIDEO, true, -1, -1, &actionList);
                        insertTrack = timeline->m_Tracks[newTrackIndex];
                    }
                    insertTrack->InsertClip(new_image_clip, mouseTime, true, &actionList);
                }
                else if (IS_AUDIO(item->mMediaType))
                {
                    AudioClip * new_audio_clip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, timeline);
                    timeline->m_Clips.push_back(new_audio_clip);
                    MediaTrack* insertTrack = track;
                    if (!track || !track->CanInsertClip(new_audio_clip, mouseTime))
                    {
                        int newTrackIndex = timeline->NewTrack("", MEDIA_AUDIO, true, -1, -1, &actionList);
                        insertTrack = timeline->m_Tracks[newTrackIndex];
                    }
                    insertTrack->InsertClip(new_audio_clip, mouseTime, true, &actionList);
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
                            TextClip * new_text_clip = new TextClip(hSubClip->StartTime(), hSubClip->EndTime(), newTrack->mID, newTrack->mName, hSubClip->Text(), timeline);
                            new_text_clip->SetClipDefault(style);
                            new_text_clip->mClipHolder = hSubClip;
                            new_text_clip->mTrack = newTrack;
                            timeline->m_Clips.push_back(new_text_clip);
                            newTrack->InsertClip(new_text_clip, hSubClip->StartTime(), false);
                            hSubClip = newTrack->mMttReader->GetNextClip();
                        }
                        if (newTrack->mMttReader->Duration() > timeline->mEnd)
                        {
                            timeline->mEnd = newTrack->mMttReader->Duration() + 1000;
                        }
                    }
                }
                else  // add a video clip
                {
                    bool create_new_track = false;
                    MediaTrack * videoTrack = nullptr;
                    VideoClip * new_video_clip = nullptr;
                    AudioClip * new_audio_clip = nullptr;
                    const MediaCore::VideoStream* video_stream = item->mMediaOverview->GetVideoStream();
                    const MediaCore::AudioStream* audio_stream = item->mMediaOverview->GetAudioStream();
                    const MediaCore::SubtitleStream * subtitle_stream = nullptr;
                    if (video_stream)
                    {
                        MediaCore::Snapshot::Viewer::Holder hViewer;
                        MediaCore::Snapshot::Generator::Holder hSsGen = timeline->GetSnapshotGenerator(item->mID);
                        if (hSsGen) hViewer = hSsGen->CreateViewer();
                        new_video_clip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName + ":Video", item->mMediaOverview->GetMediaParser(), hViewer, timeline);
                        timeline->m_Clips.push_back(new_video_clip);
                        videoTrack = track;
                        if (!track || !track->CanInsertClip(new_video_clip, mouseTime))
                        {
                            int newTrackIndex = timeline->NewTrack("", MEDIA_VIDEO, true, -1, -1, &actionList);
                            videoTrack = timeline->m_Tracks[newTrackIndex];
                        }
                        videoTrack->InsertClip(new_video_clip, mouseTime, true, &actionList);
                    }
                    if (audio_stream)
                    {
                        new_audio_clip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName + ":Audio", item->mMediaOverview, timeline);
                        timeline->m_Clips.push_back(new_audio_clip);
                        if (!create_new_track)
                        {
                            if (new_video_clip)
                            {
                                // video clip is insert into track, we need check if this track has linked track
                                if (track && track->mLinkedTrack != -1)
                                {
                                    MediaTrack * relative_track = timeline->FindTrackByID(track->mLinkedTrack);
                                    if (relative_track && IS_AUDIO(relative_track->mType))
                                    {
                                        bool can_insert_clip = relative_track->CanInsertClip(new_audio_clip, mouseTime);
                                        if (can_insert_clip)
                                        {
                                            if (new_video_clip->mGroupID == -1)
                                            {
                                                timeline->NewGroup(new_video_clip, -1L, 0U, &actionList);
                                            }
                                            relative_track->InsertClip(new_audio_clip, mouseTime, true, &actionList);
                                            timeline->AddClipIntoGroup(new_audio_clip, new_video_clip->mGroupID, &actionList);
                                        }
                                        else
                                            create_new_track = true;
                                    }
                                    else
                                        create_new_track = true;
                                }
                                else if (track)
                                {
                                    // no mLinkedTrack with track, we try to find empty audio track first
                                    MediaTrack * empty_track = timeline->FindEmptyTrackByType(MEDIA_AUDIO);
                                    if (empty_track)
                                    {
                                        if (new_video_clip->mGroupID == -1)
                                        {
                                            timeline->NewGroup(new_video_clip, -1L, 0U, &actionList);
                                        }
                                        timeline->AddClipIntoGroup(new_audio_clip, new_video_clip->mGroupID, &actionList);
                                        empty_track->InsertClip(new_audio_clip, mouseTime, true, &actionList);
                                    }
                                    else
                                        create_new_track = true;
                                }
                                else
                                    create_new_track = true;
                            }
                            else
                            {
                                // no video stream
                                bool can_insert_clip = track ? track->CanInsertClip(new_audio_clip, mouseTime) : false;
                                if (can_insert_clip)
                                {
                                    // update clip info and push into track
                                    track->InsertClip(new_audio_clip, mouseTime, true, &actionList);
                                }
                                else
                                {
                                    create_new_track = true;
                                }
                            }
                        }
                        if (create_new_track)
                        {
                            if (new_video_clip)
                            {
                                if (new_video_clip->mGroupID == -1)
                                {
                                    timeline->NewGroup(new_video_clip, -1L, 0U, &actionList);
                                }
                                timeline->AddClipIntoGroup(new_audio_clip, new_video_clip->mGroupID, &actionList);
                            }
                            //  we try to find empty audio track first
                            MediaTrack * audioTrack = timeline->FindEmptyTrackByType(MEDIA_AUDIO);
                            if (!audioTrack)
                            {
                                int newTrackIndex = timeline->NewTrack("", MEDIA_AUDIO, true, -1, -1, &actionList);
                                audioTrack = timeline->m_Tracks[newTrackIndex];
                            }
                            audioTrack->InsertClip(new_audio_clip, mouseTime, true, &actionList);
                            if (videoTrack)
                            {
                                videoTrack->mLinkedTrack = audioTrack->mID;
                                audioTrack->mLinkedTrack = videoTrack->mID;
                                imgui_json::value action;
                                action["action"] = "LINK_TRACK";
                                action["track_id1"] = imgui_json::number(videoTrack->mID);
                                action["track_id2"] = imgui_json::number(audioTrack->mID);
                                actionList.push_back(std::move(action));
                            }
                        }
                    }
                    // TODO::Dicky add subtitle stream here?
                }
                timeline->Update();
                ret = true;
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
            ret = true;
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
                ret = true;
        }
        if (ret)
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
                    ret = true;
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
        timeline->NewTrack("", MEDIA_VIDEO, true, -1, -1, &timeline->mUiActions);
        ret = true;
    }
    else if (IS_AUDIO(insertEmptyTrackType))
    {
        timeline->NewTrack("", MEDIA_AUDIO, true, -1, -1, &timeline->mUiActions);
        ret = true;
    }
    else if (IS_TEXT(insertEmptyTrackType))
    {
        int newTrackIndex = timeline->NewTrack("", MEDIA_TEXT, true);
        MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
        newTrack->mMttReader = timeline->mMtvReader->NewEmptySubtitleTrack(newTrack->mID);
        newTrack->mMttReader->SetFont(timeline->mFontName);
        newTrack->mMttReader->SetFrameSize(timeline->GetPreviewWidth(), timeline->GetPreviewHeight());
        newTrack->mMttReader->EnableFullSizeOutput(false);
        ret = true;
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
        ret = true;
    }

    for (auto clip_id : unGroupClipEntry)
    {
        auto clip = timeline->FindClipByID(clip_id);
        timeline->DeleteClipFromGroup(clip, clip->mGroupID, &timeline->mUiActions);
        ret = true;
    }

    // handle track moving
    if (trackMovingEntry != -1 && trackEntry != -1 && trackMovingEntry != trackEntry)
    {
        timeline->MovingTrack(trackMovingEntry, trackEntry, &timeline->mUiActions);
        ret = true;
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

    if (!timeline->mUiActions.empty())
    {
        // add to history record list
        imgui_json::value historyRecord;
        historyRecord["time"] = ImGui::get_current_time();
        auto& actions = timeline->mUiActions;
        historyRecord["actions"] = imgui_json::array(actions.begin(), actions.end());
        timeline->AddNewRecord(historyRecord);

        // perform actions
        timeline->PerformUiActions();
        ret = true;
    }
    return ret;
}

/***********************************************************************************************************
 * Draw Clip Timeline
 ***********************************************************************************************************/
bool DrawClipTimeLine(TimeLine* main_timeline, BaseEditingClip * editingClip, int64_t CurrentTime, int header_height, int custom_height, int curve_height, ImGui::KeyPointEditor* key_point)
{
    /***************************************************************************************
    |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    c
    |_______________|_____________________________________________________________________ a
    |               |        custom area                                                   n 
    |               |                                                                      v                                            
    |_______________|_____________________________________________________________________ a                                                                                                           +
    [                           <==slider==>                                               ]
    ****************************************************************************************/
    bool ret = false;
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    if (!editingClip)
    {
        ImGui::SetWindowFontScale(2);
        auto pos_center = window_pos + window_size / 2;
        std::string tips_string = "Please Select Clip by Double Click From Main Timeline";
        auto string_width = ImGui::CalcTextSize(tips_string.c_str());
        auto tips_pos = pos_center - string_width / 2;
        ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
        ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
        draw_list->AddText(tips_pos, IM_COL32(255, 255, 255, 128), tips_string.c_str());
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1);
        return ret;
    }
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    int scrollSize = 12;
    int64_t start = editingClip->mStart;
    int64_t end = editingClip->mEnd;
    int64_t currentTime = CurrentTime - start;
    int64_t duration = ImMax(end - start, (int64_t)1);
    static bool MovingHorizonScrollBar = false;
    static bool MovingVerticalScrollBar = false;
    static bool MovingCurrentTime = false;
    static bool menuIsOpened = false;
    int64_t mouseTime = -1;
    static int64_t menuMouseTime = -1;
    static ImVec2 menuMousePos = ImVec2(-1, -1);
    static bool mouse_hold = false;
    bool overHorizonScrollBar = false;
    bool overCustomDraw = false;
    bool overTopBar = false;
    bool overCurveDraw = false;
    float frame_duration = 40;

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();                    // ImDrawList API uses screen coordinates!
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();                // Resize canvas to what's available
    ImVec2 timline_size = canvas_size;     // add Vertical Scroll
    if (timline_size.y - header_height <= 0)
        return ret;

    float minPixelWidthTarget = (float)(timline_size.x) / (float)duration; //ImMin(0.1f, (float)(timline_size.x) / (float)duration);
    //float minPixelWidthTarget = ImMin(0.1f, (float)(timline_size.x) / (float)duration);
    float maxPixelWidthTarget = 20.f;
    int view_frames = 16;
    if (IS_VIDEO(editingClip->mType))
    {
        EditingVideoClip * video_clip = (EditingVideoClip *)editingClip;
        frame_duration = (video_clip->mClipFrameRate.den > 0 && video_clip->mClipFrameRate.num > 0) ? video_clip->mClipFrameRate.den * 1000.0 / video_clip->mClipFrameRate.num : 40;
        maxPixelWidthTarget = (video_clip->mSnapSize.x > 0 ? video_clip->mSnapSize.x : 60.f) / frame_duration;
        view_frames = video_clip->mSnapSize.x > 0 ? (window_size.x / video_clip->mSnapSize.x) : 16;
    }
    else if (IS_AUDIO(editingClip->mType))
    {
        frame_duration = (main_timeline->mFrameRate.den > 0 && main_timeline->mFrameRate.num > 0) ? main_timeline->mFrameRate.den * 1000.0 / main_timeline->mFrameRate.num : 40;
        maxPixelWidthTarget = 40.f / frame_duration;
        view_frames = window_size.x / 40.f;
    }

    // zoom in/out
    if (editingClip->msPixelWidthTarget < 0)
    {
        editingClip->msPixelWidthTarget = minPixelWidthTarget;
    }

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

    editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);

    if (editingClip->visibleTime >= duration)
        editingClip->firstTime = 0;
    else if (editingClip->firstTime + editingClip->visibleTime > duration)
    {
        editingClip->firstTime = duration - editingClip->visibleTime;
        main_timeline->AlignTime(editingClip->firstTime);
        editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
    }
    editingClip->lastTime = editingClip->firstTime + editingClip->visibleTime;

    ImGui::BeginGroup();
    bool isFocused = ImGui::IsWindowFocused();
    {
        ImGui::SetCursorScreenPos(canvas_pos);
        ImVec2 headerSize(timline_size.x, (float)header_height);
        ImVec2 HorizonScrollBarSize(timline_size.x, scrollSize);
        ImRect HeaderAreaRect(canvas_pos, canvas_pos + headerSize);
        ImGui::InvisibleButton("clip_topBar", headerSize);

        // draw Header bg
        draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Max, COL_DARK_ONE, 0);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

        ImVec2 childFramePos = window_pos + ImVec2(0, header_height);
        ImVec2 childFrameSize(timline_size.x, custom_height);
        ImGui::BeginChildFrame(ImGui::GetID("clip_timeline"), childFrameSize, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::InvisibleButton("clip_contentBar", ImVec2(timline_size.x, float(header_height + custom_height)));
        const ImVec2 contentMin = childFramePos;
        const ImVec2 contentMax = childFramePos + childFrameSize;
        const ImRect contentRect(contentMin, contentMax);
        const ImRect trackAreaRect(contentMin, ImVec2(contentMax.x, contentMin.y + timline_size.y - (header_height + scrollSize)));
        const ImRect trackRect(ImVec2(contentMin.x, contentMin.y), contentMax);
        const ImRect topRect(ImVec2(contentMin.x, canvas_pos.y), ImVec2(contentMin.x + timline_size.x, canvas_pos.y + header_height));
        const ImRect curveRect(ImVec2(contentMin.x, contentMin.y + custom_height), ImVec2(contentMax.x, contentMax.y + curve_height));
        const float contentHeight = contentMax.y - contentMin.y;
        // full canvas background
        draw_list->AddRectFilled(canvas_pos + ImVec2(0, header_height), canvas_pos + ImVec2(0, header_height) + timline_size - ImVec2(0, header_height + scrollSize), COL_CANVAS_BG, 0);

        // calculate mouse pos to time
        mouseTime = (int64_t)((cx - contentMin.x) / editingClip->msPixelWidthTarget) + editingClip->firstTime;
        main_timeline->AlignTime(mouseTime);
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

        // handle menu
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
                    main_timeline->AlignTime(editingClip->firstTime);
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - new_visible_time, (int64_t)0));
                }
            }
            if (ImGui::MenuItem(ICON_SLIDER_CLIP " Clip accuracy", nullptr, nullptr))
            {
                editingClip->msPixelWidthTarget = minPixelWidthTarget;
                editingClip->firstTime = 0;
            }
            if (ImGui::MenuItem(ICON_CURRENT_TIME " Current Time", nullptr, nullptr))
            {
                editingClip->firstTime = currentTime - editingClip->visibleTime / 2;
                main_timeline->AlignTime(editingClip->firstTime);
                editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
            }

            if (HeaderAreaRect.Contains(menuMousePos))
            {
                // TODO::Dicky Clip timeline Add Header menu items?
            }
            if (trackAreaRect.Contains(menuMousePos))
            {
                // TODO::Dicky Clip timeline Add Clip menu items?
            }
            if (curveRect.Contains(menuMousePos) && key_point)
            {
                // Add Curve items
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_CROPPING_RIGHT " Next key", nullptr, nullptr))
                {
                    auto point = key_point->GetNextPoint(menuMouseTime);
                    currentTime = point.x;
                    main_timeline->Seek(currentTime + start);
                    editingClip->firstTime = currentTime - editingClip->visibleTime / 2;
                    main_timeline->AlignTime(editingClip->firstTime);
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                }
                if (ImGui::MenuItem(ICON_CROPPING_LEFT " Prev key", nullptr, nullptr))
                {
                    auto point = key_point->GetPrevPoint(menuMouseTime);
                    currentTime = point.x;
                    main_timeline->Seek(currentTime + start);
                    editingClip->firstTime = currentTime - editingClip->visibleTime / 2;
                    main_timeline->AlignTime(editingClip->firstTime);
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                }
            }

            ImGui::EndPopup();
        }

        ImGui::EndChildFrame();

        // Horizon Scroll bar control buttons
        ImGui::SetCursorScreenPos(window_pos + ImVec2(0, header_height + custom_height + curve_height));
        ImGui::InvisibleButton("clip_HorizonScrollBar", HorizonScrollBarSize);
        ImVec2 HorizonScrollAreaMin = window_pos + ImVec2(0, header_height + custom_height + curve_height);
        ImVec2 HorizonScrollAreaMax = HorizonScrollAreaMin + HorizonScrollBarSize;
        float HorizonStartOffset = ((float)(editingClip->firstTime) / (float)duration) * (timline_size.x - scrollSize);
        ImVec2 HorizonScrollBarMin(HorizonScrollAreaMin.x, HorizonScrollAreaMin.y - 2);        // whole bar area
        ImVec2 HorizonScrollBarMax(HorizonScrollAreaMin.x + timline_size.x - scrollSize, HorizonScrollAreaMax.y - 1);      // whole bar area
        HorizonScrollBarRect = ImRect(HorizonScrollBarMin, HorizonScrollBarMax);
        bool inHorizonScrollBar = HorizonScrollBarRect.Contains(io.MousePos);
        draw_list->AddRectFilled(HorizonScrollBarMin, HorizonScrollBarMax, COL_SLIDER_BG, 8);
        ImVec2 HorizonScrollHandleBarMin(HorizonScrollAreaMin.x + HorizonStartOffset, HorizonScrollAreaMin.y);  // current bar area
        ImVec2 HorizonScrollHandleBarMax(HorizonScrollAreaMin.x + HorizonBarWidthInPixels + HorizonStartOffset, HorizonScrollAreaMax.y - 2);
        HorizonScrollHandleBarRect = ImRect(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax);
        bool inHorizonScrollHandle = HorizonScrollHandleBarRect.Contains(io.MousePos);
        draw_list->AddRectFilled(HorizonScrollHandleBarMin, HorizonScrollHandleBarMax, (inHorizonScrollBar || MovingHorizonScrollBar) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);

        if (MovingHorizonScrollBar)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                MovingHorizonScrollBar = false;
            }
            else
            {
                float msPerPixelInBar = HorizonBarPos / (float)editingClip->visibleTime;
                editingClip->firstTime = int((io.MousePos.x - panningViewHorizonSource.x) / msPerPixelInBar) - panningViewHorizonTime;
                main_timeline->AlignTime(editingClip->firstTime);
                editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
            }
        }
        else if (inHorizonScrollHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingCurrentTime && !menuIsOpened && !mouse_hold)
        {
            ImGui::CaptureMouseFromApp();
            MovingHorizonScrollBar = true;
            panningViewHorizonSource = io.MousePos;
            panningViewHorizonTime = - editingClip->firstTime;
        }
        else if (inHorizonScrollBar && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !menuIsOpened && !mouse_hold)
        {
            float msPerPixelInBar = HorizonBarPos / (float)editingClip->visibleTime;
            editingClip->firstTime = int((io.MousePos.x - contentMin.x) / msPerPixelInBar);
            main_timeline->AlignTime(editingClip->firstTime);
            editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
        }

        // handle mouse wheel event
        if (regionRect.Contains(io.MousePos) && !menuIsOpened)
        {
            if (topRect.Contains(io.MousePos))
            {
                overTopBar = true;
            }
            if (trackRect.Contains(io.MousePos))
            {
                overCustomDraw = true;
            }
            if (HorizonScrollBarRect.Contains(io.MousePos))
            {
                overHorizonScrollBar = true;
            }
            if (curveRect.Contains(io.MousePos))
            {
                overCurveDraw = true;
            }
            if (overCustomDraw || overHorizonScrollBar || overTopBar || overCurveDraw)
            {
                
                // left-right wheel over blank area, moving canvas view
                if (io.MouseWheelH < -FLT_EPSILON)
                {
                    editingClip->firstTime -= editingClip->visibleTime / view_frames;
                    main_timeline->AlignTime(editingClip->firstTime);
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                }
                else if (io.MouseWheelH > FLT_EPSILON)
                {
                    editingClip->firstTime += editingClip->visibleTime / view_frames;
                    main_timeline->AlignTime(editingClip->firstTime);
                    editingClip->firstTime = ImClamp(editingClip->firstTime, (int64_t)0, ImMax(duration - editingClip->visibleTime, (int64_t)0));
                }
                // up-down wheel over scrollbar, scale canvas view
                else if (io.MouseWheel < -FLT_EPSILON && editingClip->visibleTime <= duration)
                {
                    editingClip->msPixelWidthTarget *= 0.9f;
                    editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    editingClip->msPixelWidthTarget *= 1.1f;
                    editingClip->msPixelWidthTarget = ImClamp(editingClip->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
                }
            }
        }

        // draw clip content
        ImGui::PushClipRect(contentMin, contentMax, true);
        editingClip->DrawContent(draw_list, contentMin, contentMax);
        ImGui::PopClipRect();

        // time metric
        ImGui::SetCursorScreenPos(topRect.Min);
        ImGui::BeginChildFrame(ImGui::GetCurrentWindow()->GetID("#timeline metric"), topRect.GetSize(), ImGuiWindowFlags_NoScrollbar);
        if (!MovingCurrentTime && !MovingHorizonScrollBar && currentTime >= 0 && topRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !menuIsOpened && !mouse_hold)
        {
            MovingCurrentTime = true;
            editingClip->bSeeking = true;
        }
        if (MovingCurrentTime && duration)
        {
            auto old_time = currentTime;
            currentTime = (int64_t)((io.MousePos.x - topRect.Min.x) / editingClip->msPixelWidthTarget) + editingClip->firstTime;
            main_timeline->AlignTime(currentTime);
            if (currentTime < editingClip->firstTime)
                currentTime = editingClip->firstTime;
            if (currentTime > editingClip->lastTime)
                currentTime = editingClip->lastTime;
            main_timeline->Seek(currentTime + start);
        }
        if (editingClip->bSeeking && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = false;
            editingClip->bSeeking = false;
        }
        ImGui::EndChildFrame();
        
        // handle playing curses move
        if (main_timeline->mIsPreviewPlaying) editingClip->UpdateCurrent(main_timeline->mIsPreviewForward, currentTime);
        
        // draw cursor
        ImRect custom_view_rect(window_pos, window_pos + ImVec2(window_size.x, header_height + custom_height));
        draw_list->PushClipRect(custom_view_rect.Min - ImVec2(32, 0), custom_view_rect.Max + ImVec2(32, 0));
        if (currentTime >= editingClip->firstTime && currentTime <= editingClip->lastTime)
        {
            // cursor arrow
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + (currentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - arrowWidth * 0.5f + 1;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), COL_CURSOR_ARROW_R, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = ImGuiHelper::MillisecToString(currentTime, 2);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + (currentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - str_size.x * 0.5f + 1;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BR, 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, COL_CURSOR_TEXT_R, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
            // cursor line
            static const float cursorWidth = 2.f;
            float cursorOffset = contentMin.x + (currentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - 0.5f;
            draw_list->AddLine(ImVec2(cursorOffset, window_pos.y + header_height), ImVec2(cursorOffset, window_pos.y + header_height + custom_height), COL_CURSOR_LINE_R, cursorWidth);
        }
        draw_list->PopClipRect();

        ImGui::PopStyleColor();

        if (curve_height && key_point)
        {
            // Draw curve
            ImVec2 curveFramePos = window_pos + ImVec2(0, header_height + custom_height);
            ImVec2 curveFrameSize(timline_size.x, curve_height);
            ImGui::SetCursorScreenPos(curveFramePos);
            if (ImGui::BeginChild("##clip_curve", curveFrameSize, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
            {
                ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
                ImVec2 sub_window_size = ImGui::GetWindowSize();
                draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
                bool _changed = false;
                float current_time = currentTime;// + start;
                key_point->SetCurveAlign(ImVec2(frame_duration, -1.0));
                mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        key_point,
                                                        sub_window_size, 
                                                        ImGui::GetID("##clip_keypoint_editor"), 
                                                        !menuIsOpened,
                                                        current_time,
                                                        editingClip->firstTime,
                                                        editingClip->lastTime,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END, 
                                                        nullptr, // clippingRect
                                                        &_changed);
                if (_changed) main_timeline->UpdatePreview();
            }
            ImGui::EndChild();
            // Draw cursor line after curve draw
            static const float cursorWidth = 2.f;
            float cursorOffset = contentMin.x + (currentTime - editingClip->firstTime) * editingClip->msPixelWidthTarget - 0.5f;
            draw_list->AddLine(ImVec2(cursorOffset, window_pos.y + header_height + custom_height), ImVec2(cursorOffset, window_pos.y + header_height + custom_height + curve_height), COL_CURSOR_LINE_R, cursorWidth);
        }
    }

    ImGui::EndGroup();

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        mouse_hold = false;
    }
    return mouse_hold;
}

/***********************************************************************************************************
 * Draw Fusion Timeline
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
    bool ret = false;
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    if (!overlap)
    {
        ImGui::SetWindowFontScale(2);
        auto pos_center = window_pos + window_size / 2;
        std::string tips_string = "Please Select Overlap by Double Click From Main Timeline";
        auto string_width = ImGui::CalcTextSize(tips_string.c_str());
        auto tips_pos = pos_center - string_width / 2;
        ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
        ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
        draw_list->AddText(tips_pos, IM_COL32(255, 255, 255, 128), tips_string.c_str());
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        return ret;
    }
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    static bool MovingCurrentTime = false;
    bool isFocused = ImGui::IsWindowFocused();
    int64_t duration = ImMax(overlap->mOvlp->mEnd-overlap->mOvlp->mStart, (int64_t)1);
    int64_t start = 0;
    int64_t end = start + duration;

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
        auto oldPos = CurrentTime;
        auto newPos = (int64_t)((io.MousePos.x - movRect.Min.x) / overlap->msPixelWidth) + start;
        if (newPos < start)
            newPos = start;
        if (newPos >= end)
            newPos = end;
        if (oldPos != newPos)
            overlap->Seek(newPos + overlap->mStart); // call seek event
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
    overlap->DrawContent(draw_list, contentMin, contentMax);

    // cursor line
    draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
    static const float cursorWidth = 2.f;
    float cursorOffset = contentMin.x + (CurrentTime - start) * overlap->msPixelWidth - cursorWidth * 0.5f + 1;
    draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMax.y), COL_CURSOR_LINE_R, cursorWidth);
    draw_list->PopClipRect();
    ImGui::EndGroup();
    return ret;
}
} // namespace MediaTimeline/Main Timeline
