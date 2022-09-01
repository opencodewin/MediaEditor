#include "MediaTimeline.h"
#include "MediaInfo.h"
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <implot.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "Logger.h"

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
    if (overButton && !tooltips.empty())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tooltips.c_str());
        ImGui::EndTooltip();
    }
    return overButton;
}

static void RenderMouseCursor(const char* mouse_cursor, ImVec2 offset = ImVec2(0 ,0), float base_scale = 1.0, int rotate = 0, ImU32 col_fill = IM_COL32_WHITE, ImU32 col_border = IM_COL32_BLACK, ImU32 col_shadow = IM_COL32(0, 0, 0, 48))
{
    ImGuiViewportP* viewport = (ImGuiViewportP*)ImGui::GetWindowViewport();
    ImDrawList* draw_list = ImGui::GetForegroundDrawList(viewport);
    ImGuiIO& io = ImGui::GetIO();
    const float FontSize = draw_list->_Data->FontSize;
    ImVec2 size(FontSize, FontSize);
    const ImVec2 pos = io.MousePos - offset;
    const float scale = base_scale * viewport->DpiScale;
    if (!viewport->GetMainRect().Overlaps(ImRect(pos, pos + ImVec2(size.x + 2, size.y + 2) * scale)))
        return;

    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    int rotation_start_index = draw_list->VtxBuffer.Size;
    draw_list->AddText(pos + ImVec2(-1, -1), col_border, mouse_cursor);
    draw_list->AddText(pos + ImVec2(1, 1), col_shadow, mouse_cursor);
    draw_list->AddText(pos, col_fill, mouse_cursor);
    if (rotate != 0)
    {
        float rad = M_PI / 180 * (90 - rotate);
        ImVec2 l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX); // bounds
        auto& buf = draw_list->VtxBuffer;
        float s = sin(rad), c = cos(rad);
        for (int i = rotation_start_index; i < buf.Size; i++)
            l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);
        ImVec2 center = ImVec2((l.x + u.x) / 2, (l.y + u.y) / 2);
        center = ImRotate(center, s, c) - center;
        
        for (int i = rotation_start_index; i < buf.Size; i++)
            buf[i].pos = ImRotate(buf[i].pos, s, c) - center;
    }
}

static void alignTime(int64_t& time, MediaInfo::Ratio rate)
{
    if (rate.den && rate.num)
    {
        int64_t frame_index = (int64_t)floor((double)time * (double)rate.num / (double)rate.den / 1000.0);
        time = frame_index * 1000 * rate.den / rate.num;
    }
}

static void frameStepTime(int64_t& time, int32_t offset, MediaInfo::Ratio rate)
{
    if (rate.den && rate.num)
    {
        int64_t frame_index = (int64_t)floor((double)time * (double)rate.num / (double)rate.den / 1000.0 + 0.5);
        frame_index += offset;
        time = frame_index * 1000 * rate.den / rate.num;
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
MediaItem::MediaItem(const std::string& name, const std::string& path, MEDIA_TYPE type, void* handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    mName = name;
    mPath = path;
    mMediaType = type;
    mMediaOverview = CreateMediaOverview();
    if (timeline)
    {
        mMediaOverview->EnableHwAccel(timeline->mHardwareCodec);
    }
    if (!path.empty() && mMediaOverview)
    {
        mMediaOverview->SetSnapshotResizeFactor(0.1, 0.1);
        mMediaOverview->Open(path, 50);
    }
    if (mMediaOverview && mMediaOverview->IsOpened())
    {
        mStart = 0;
        if (mMediaOverview->HasVideo() && type != MEDIA_PICTURE)
        {
            mMediaOverview->GetMediaParser()->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            mEnd = mMediaOverview->GetVideoDuration();
        }
        else if (mMediaOverview->HasAudio())
        {
            mEnd = mMediaOverview->GetMediaInfo()->duration * 1000;
        }
        else
        {
            mEnd = 5000;
        }
    }
}

MediaItem::~MediaItem()
{
    ReleaseMediaOverview(&mMediaOverview);
    mMediaOverview = nullptr;
    for (auto thumb : mMediaThumbnail)
    {
        ImGui::ImDestroyTexture(thumb); 
        thumb = nullptr;
    }
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
Clip::Clip(int64_t start, int64_t end, int64_t id, MediaParserHolder mediaParser, void * handle)
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
    float frame_duration = (timeline->mFrameRate.den > 0 && timeline->mFrameRate.num > 0) ? timeline->mFrameRate.den * 1000.0 / timeline->mFrameRate.num : 40;
    if (type == 0)
    {
        // cropping start
        if (mType == MEDIA_VIDEO || mType == MEDIA_AUDIO)
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
        if (mType == MEDIA_VIDEO || mType == MEDIA_AUDIO)
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
    mFilterKeyPoints.SetMin(ImVec2(mStartOffset, 0.f), true);
    mFilterKeyPoints.SetMax(ImVec2(mEnd - mStart + mStartOffset, 1.f), true);
    if (timeline->mVidFilterClip && timeline->mVidFilterClip->mID == mID)
    {
        timeline->mVidFilterClip->mStart = mStart;
        timeline->mVidFilterClip->mEnd = mEnd;
    }
    mAttributeKeyPoints.SetMin(ImVec2(mStartOffset, 0.f), true);
    mAttributeKeyPoints.SetMax(ImVec2(mEnd - mStart + mStartOffset, 1.f), true);
    track->Update();
    return new_diff;
}

void Clip::Cutting(int64_t pos)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    auto track = timeline->FindTrackByClipID(mID);
    if (!track || track->mLocked)
        return;
    
    // calculate new pos
    int64_t adj_end = pos;
    int64_t adj_end_offset = mEndOffset + (mEnd - pos);
    int64_t new_start = pos;
    int64_t new_start_offset = mStartOffset + (pos - mStart);
    // create new clip base on current clip
    Clip * new_clip = nullptr;
    switch (mType)
    {
        case MEDIA_VIDEO:
        {
            VideoClip* vidclip = dynamic_cast<VideoClip*>(this);
            SnapshotGenerator::ViewerHolder hViewer = vidclip->mSsViewer->CreateViewer();
            auto new_video_clip = new VideoClip(mStart, mEnd, mMediaID, mName, mMediaParser, hViewer, timeline);
            new_clip = new_video_clip;
        }
        break;
        case MEDIA_AUDIO:
        {
            AudioClip* audclip = dynamic_cast<AudioClip*>(this);
            auto new_audio_clip = new AudioClip(mStart, mEnd, mMediaID, mName, audclip->mOverview, timeline);
            new_clip = new_audio_clip;
        }
        break;
        case MEDIA_PICTURE:
        {
            ImageClip* imgclip = dynamic_cast<ImageClip*>(this);
            auto new_image_clip = new ImageClip(mStart, mEnd, mMediaID, mName, imgclip->mOverview, timeline);
            new_clip = new_image_clip;
            new_start_offset = 0;
            adj_end_offset = 0;
        }
        break;
        case MEDIA_TEXT:
        {
            TextClip* textClip = dynamic_cast<TextClip*>(this);
            auto new_text_clip = new TextClip(new_start, mEnd, mMediaID, mName, textClip->mText, timeline);
            new_text_clip->SetClipDefault(textClip);
            new_text_clip->CreateClipHold(track);
            new_clip = new_text_clip;
            // adj current text clip time
            track->mMttReader->ChangeClipTime(textClip->mClipHolder, mStart, adj_end - mStart);
        }
        break;
        default:
        break;
    }

    // insert new clip into track and timeline
    if (new_clip)
    {
        new_clip->mStart = new_start;
        new_clip->mStartOffset = new_start_offset;
        new_clip->mEnd = mEnd;
        new_clip->mEndOffset = mEndOffset;
        new_clip->mFilterKeyPoints.SetMin(ImVec2(new_clip->mStartOffset, 0.f), true);
        new_clip->mFilterKeyPoints.SetMax(ImVec2(new_clip->mEnd - new_clip->mStart + new_clip->mStartOffset, 1.f), true);
        new_clip->mAttributeKeyPoints.SetMin(ImVec2(new_clip->mStartOffset, 1.f), true);
        new_clip->mAttributeKeyPoints.SetMax(ImVec2(new_clip->mEnd - new_clip->mStart + new_clip->mStartOffset, 1.f), true);
        mEnd = adj_end;
        mEndOffset = adj_end_offset;
        mFilterKeyPoints.SetMin(ImVec2(mStartOffset, 0.f), true);
        mFilterKeyPoints.SetMax(ImVec2(mEnd - mStart + mStartOffset, 1.f), true);
        mAttributeKeyPoints.SetMin(ImVec2(mStartOffset, 0.f), true);
        mAttributeKeyPoints.SetMax(ImVec2(mEnd - mStart + mStartOffset, 1.f), true);
        timeline->m_Clips.push_back(new_clip);

        // update curve
        if (timeline->mVidFilterClip && timeline->mVidFilterClip->mID == mID)
        {
            timeline->mVidFilterClip->mStart = mStart;
            timeline->mVidFilterClip->mEnd = mEnd;
        }

        // need check overlap status and update overlap info on data layer(UI info will update on track update)
        for (auto overlap : timeline->m_Overlaps)
        {
            if (overlap->m_Clip.first == mID || overlap->m_Clip.second == mID)
            {
                if (overlap->mStart >= new_clip->mStart && overlap->mEnd <= new_clip->mEnd)
                {
                    if (overlap->m_Clip.first == mID) overlap->m_Clip.first = new_clip->mID;
                    if (overlap->m_Clip.second == mID) overlap->m_Clip.second = new_clip->mID;
                }
            }
        }

        // update timeline
        track->InsertClip(new_clip, pos);
        timeline->AddClipIntoGroup(new_clip, mGroupID);
        timeline->Updata();

        // sync this action to data layer
        switch (mType)
        {
            case MEDIA_VIDEO:
            {
                DataLayer::VideoTrackHolder vidTrack = timeline->mMtvReader->GetTrackById(track->mID);
                vidTrack->ChangeClipRange(mID, mStartOffset, mEndOffset);
                DataLayer::VideoClipHolder thisVidClip = vidTrack->GetClipById(mID);
                DataLayer::VideoClipHolder newVidClip(new DataLayer::VideoClip(
                    new_clip->mID, thisVidClip->GetMediaParser(),
                    vidTrack->OutWidth(), vidTrack->OutHeight(), vidTrack->FrameRate(),
                    new_clip->mStart, new_clip->mStartOffset, new_clip->mEndOffset, new_clip->mStartOffset));
                vidTrack->InsertClip(newVidClip);
                timeline->mMtvReader->Refresh();
                break;
            }
            case MEDIA_AUDIO:
            {
                DataLayer::AudioTrackHolder audTrack = timeline->mMtaReader->GetTrackById(track->mID);
                audTrack->ChangeClipRange(mID, mStartOffset, mEndOffset);
                DataLayer::AudioClipHolder thisAudClip = audTrack->GetClipById(mID);
                DataLayer::AudioClipHolder newAudClip(new DataLayer::AudioClip(
                    new_clip->mID, thisAudClip->GetMediaParser(),
                    audTrack->OutChannels(), audTrack->OutSampleRate(),
                    new_clip->mStart, new_clip->mStartOffset, new_clip->mEndOffset, new_clip->mStartOffset));
                audTrack->InsertClip(newAudClip);
                timeline->mMtaReader->Refresh();
                break;
            }
            case MEDIA_TEXT:
            {
                DataLayer::SubtitleTrackHolder subTrack = timeline->mMtvReader->GetSubtitleTrackById(track->mID);
                DataLayer::SubtitleClipHolder thisSubClip = subTrack->GetClipByTime(mStart);
                subTrack->ChangeClipTime(thisSubClip, mStart, mEnd-mStart);
                DataLayer::SubtitleClipHolder newSubClip = subTrack->NewClip(new_clip->mStart, new_clip->mEnd-new_clip->mStart);
                newSubClip->SetText(thisSubClip->Text());
                newSubClip->CloneStyle(thisSubClip);
                break;
            }
            default:
                Logger::Log(Logger::WARN) << "Unhandled 'CUTTING' action!" << std::endl;
                break;
        }

        timeline->SyncDataLayer();
    }
}

int64_t Clip::Moving(int64_t diff, int mouse_track)
{
    int64_t index = -1;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return index;
    auto track = timeline->FindTrackByClipID(mID);
    if (!track)
        return index;
    
    ImGuiIO &io = ImGui::GetIO();
    const bool is_super_key_only = (io.KeyMods == ImGuiKeyModFlags_Super);
    bool single = (ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper)) && is_super_key_only;

    int track_index = timeline->FindTrackIndexByClipID(mID);
    int64_t length = mEnd - mStart;
    int64_t start = timeline->mStart;
    int64_t end = -1;
    int64_t new_diff = -1;

    int64_t group_start = mStart;
    int64_t group_end = mEnd;
    if (!single)
    {
        // check all select clip
        for (auto &clip : timeline->m_Clips)
        {
            if (clip->bSelected && clip->mID != mID)
            {
                if (clip->mStart < group_start)
                {
                    group_start = clip->mStart;
                }
                if (clip->mEnd > group_end)
                {
                    group_end = clip->mEnd;
                }
            }
        }
    }

    // check all selected clip's lock status
    for (auto clip : timeline->m_Clips)
    {
        if (clip->bSelected)
        {
            auto track = timeline->FindTrackByClipID(clip->mID);
            if (track && track->mLocked)
            {
                return index;
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
        if ((single && clip->mID == mID) || (!single && clip->bSelected))
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
        for (auto clip : timeline->m_Clips)
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
            bool is_moving = single ? clip->mID == mID : clip->bSelected;
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
    
    // check clip is cross track
    if (mouse_track == -2 && track->m_Clips.size() > 1)
    {
        index = timeline->NewTrack("", mType, track->mExpanded);
        if (mType == MEDIA_TEXT)
        {
            MediaTrack * newTrack = timeline->m_Tracks[index];
            newTrack->mMttReader = timeline->mMtvReader->NewEmptySubtitleTrack(newTrack->mID);
            newTrack->mMttReader->SetFont(timeline->mFontName);
            newTrack->mMttReader->SetFrameSize(timeline->mWidth, timeline->mHeight);
            newTrack->mMttReader->EnableFullSizeOutput(false);
        }
        timeline->MovingClip(mID, track_index, index);
    }
    else if (mouse_track >= 0 && mouse_track != track_index)
    {
        MediaTrack * track = timeline->m_Tracks[mouse_track];
        auto media_type = track->mType;
        if (mType == media_type)
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
        for (auto &clip : timeline->m_Clips)
        {
            if (clip->bSelected && clip->mID != mID)
            {
                int64_t clip_length = clip->mEnd - clip->mStart;
                clip->mStart += new_diff;
                clip->mEnd = clip->mStart + clip_length;
            }
        }
    }
    timeline->Updata();
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

// VideoClip Struct Member Functions
VideoClip::VideoClip(int64_t start, int64_t end, int64_t id, std::string name, MediaParserHolder hParser, SnapshotGenerator::ViewerHolder hViewer, void* handle)
    : Clip(start, end, id, hParser, handle)
{
    if (handle && hViewer)
    {
        mSsViewer = hViewer;
        if (!mSsViewer)
            return;
        mType = MEDIA_VIDEO;
        mName = name;
        MediaInfo::InfoHolder info = hParser->GetMediaInfo();
        const MediaInfo::VideoStream* video_stream = hParser->GetBestVideoStream();
        if (!info || !video_stream)
        {
            hViewer->Release();
            return;
        }
        mPath = info->url;
    }
}

VideoClip::~VideoClip()
{
    mSsViewer->Release();
    for (auto& snap : mVideoSnapshots)
    {
        if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; }
    }
    mVideoSnapshots.clear();
}

Clip* VideoClip::Load(const imgui_json::value& value, void * handle)
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
        // media is in bank
        SnapshotGenerator::ViewerHolder hViewer;
        SnapshotGeneratorHolder hSsGen = timeline->GetSnapshotGenerator(item->mID);
        hViewer = hSsGen->CreateViewer();
        VideoClip * new_clip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview->GetMediaParser(), hViewer, handle);
        if (new_clip)
        {
            Clip::Load(new_clip, value);
            // load video info
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
    if (!mSsViewer->GetSnapshots((double)mClipViewStartPos/1000, mSnapImages))
        throw std::runtime_error(mSsViewer->GetError());
    mSsViewer->UpdateSnapshotTexture(mSnapImages);
}

void VideoClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect)
{
    ImVec2 snapLeftTop = leftTop;
    float snapDispWidth;
    GetSnapshotGeneratorLogger()->Log(Logger::DEBUG) << "[1]>>>>> Begin display snapshot" << std::endl;
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
            GetSnapshotGeneratorLogger()->Log(Logger::DEBUG) << "[1]\t\t display tid=" << tid << std::endl;
            drawList->AddImage(tid, snapLeftTop, {snapLeftTop.x + snapDispWidth, rightBottom.y}, uvMin, uvMax);
        }
        else
        {
            drawList->AddRectFilled(snapLeftTop, {snapLeftTop.x + snapDispWidth, rightBottom.y}, IM_COL32_BLACK);
            auto center_pos = snapLeftTop + ImVec2(snapDispWidth, mSnapHeight) / 2;
            ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
            ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
            ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
            ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
            drawList->AddRect(snapLeftTop, {snapLeftTop.x + snapDispWidth, rightBottom.y}, COL_FRAME_RECT);
        }
        snapLeftTop.x += snapDispWidth;
        if (snapLeftTop.x >= rightBottom.x)
            break;
    }
    GetSnapshotGeneratorLogger()->Log(Logger::DEBUG) << "[1]<<<<< End display snapshot" << std::endl;
}

void VideoClip::CalcDisplayParams()
{
    const MediaInfo::VideoStream* video_stream = mMediaParser->GetBestVideoStream();
    mSnapHeight = mTrackHeight;
    MediaInfo::Ratio displayAspectRatio = {
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
    value["FrameRateNum"] = imgui_json::number(mClipFrameRate.num);
    value["FrameRateDen"] = imgui_json::number(mClipFrameRate.den);
}

// AudioClip Struct Member Functions
AudioClip::AudioClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle)
    : Clip(start, end, id, overview->GetMediaParser(), handle), mOverview(overview)
{
    if (handle && mMediaParser)
    {
        mType = MEDIA_AUDIO;
        mName = name;
        MediaInfo::InfoHolder info = mMediaParser->GetMediaInfo();
        const MediaInfo::AudioStream* audio_stream = mMediaParser->GetBestAudioStream();
        if (!info || !audio_stream)
        {
            return;
        }
        mPath = info->url;
        mWaveform = overview->GetWaveform();
    }
}

AudioClip::~AudioClip()
{
}

void AudioClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect)
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline || timeline->media_items.size() <= 0 || !mWaveform)
        return;

    ImVec2 draw_size = rightBottom - leftTop;
    // TODO::Dicky opt 
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
            start_offset = start_offset / sample_stride * sample_stride;
            ImGui::SetCursorScreenPos(customViewStart);
            int zoom = ImMin(sample_stride, 32);
            start_offset = start_offset / zoom * zoom; // align start_offset
#if 1
            ImGui::PushClipRect(leftTop, rightBottom, true);
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
            ImGui::PopClipRect();
#else
            drawList->PushClipRect(leftTop, rightBottom, true);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 0.f, 1.0f));
            ImGui::PlotLines(id_string.c_str(), &mWaveform->pcm[0][start_offset], window_length / zoom, 0, nullptr, -wave_range / 2, wave_range / 2, window_size, sizeof(float) * zoom, false);
            ImGui::PopStyleColor();
            drawList->AddLine(ImVec2(leftTop.x, leftTop.y + draw_size.y / 2), ImVec2(rightBottom.x, leftTop.y + draw_size.y / 2), IM_COL32(255, 255, 255, 128));
            drawList->PopClipRect();
#endif
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
        AudioClip * new_clip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, handle);
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
                if (val.is_number()) new_clip->mAudioFormat = (AudioRender::PcmFormat)val.get<imgui_json::number>();
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

// ImageClip Struct Member Functions
ImageClip::ImageClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle)
    : Clip(start, end, id, overview->GetMediaParser(), handle), mOverview(overview)
{
    if (overview)
    {
        mType = MEDIA_PICTURE;
        mName = name;
        PrepareSnapImage();
    }
}

ImageClip::~ImageClip()
{
    if (mImgTexture)
        ImGui::ImDestroyTexture(mImgTexture);
}

void ImageClip::PrepareSnapImage()
{
    if (!mOverview->GetSnapshots(mSnapImages))
    {
        Logger::Log(Logger::Error) << mOverview->GetError() << std::endl;
        return;
    }
    if (!mSnapImages.empty() && !mSnapImages[0].empty() && !mImgTexture)
    {
        ImMatToTexture(mSnapImages[0], mImgTexture);
        mWidth = mSnapImages[0].w;
        mHeight = mSnapImages[0].h;
    }
    if (mTrackHeight > 0 && mWidth > 0 && mHeight > 0)
    {
        mSnapHeight = mTrackHeight;
        mSnapWidth = mTrackHeight*mWidth/mHeight;
    }
}

void ImageClip::SetTrackHeight(int trackHeight)
{
    Clip::SetTrackHeight(trackHeight);

    if (mWidth > 0 && mHeight > 0)
    {
        mSnapHeight = trackHeight;
        mSnapWidth = trackHeight*mWidth/mHeight;
    }
}

void ImageClip::SetViewWindowStart(int64_t millisec)
{
    mClipViewStartPos = millisec;
    if (millisec < mStart)
        mClipViewStartPos = mStart;
}

void ImageClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect)
{
    if (mSnapWidth == 0)
    {
        PrepareSnapImage();
        if (mSnapWidth == 0)
            return;
    }

    ImVec2 imgLeftTop = leftTop;
    float snapDispWidth = mSnapWidth;
    int img1stIndex = (int)floor((double)mClipViewStartPos*mPixPerMs/mSnapWidth);
    int64_t img1stPos = (int64_t)((double)img1stIndex*mSnapWidth/mPixPerMs);
    if (img1stPos < mClipViewStartPos)
        snapDispWidth -= (mClipViewStartPos-img1stPos)*mPixPerMs;
    while (imgLeftTop.x < rightBottom.x)
    {
        ImVec2 uvMin{0, 0}, uvMax{1, 1};
        if (snapDispWidth < mSnapWidth)
            uvMin.x = 1-snapDispWidth/mSnapWidth;
        if (imgLeftTop.x+snapDispWidth > rightBottom.x)
        {
            uvMax.x = 1-(imgLeftTop.x+snapDispWidth-rightBottom.x)/mSnapWidth;
            snapDispWidth = rightBottom.x-imgLeftTop.x;
        }
        if (mImgTexture)
            drawList->AddImage(mImgTexture, imgLeftTop, {imgLeftTop.x+snapDispWidth, rightBottom.y}, uvMin, uvMax);
        else
            drawList->AddRect(imgLeftTop, {imgLeftTop.x+snapDispWidth, rightBottom.y}, IM_COL32_BLACK);
        imgLeftTop.x += snapDispWidth;
        snapDispWidth = mSnapWidth;
    }
}

Clip * ImageClip::Load(const imgui_json::value& value, void * handle)
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
        ImageClip * new_clip = new ImageClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, handle);
        if (new_clip)
        {
            Clip::Load(new_clip, value);
            // load image info
            return new_clip;
        }
    }
    else
    {
        // media isn't in bank we need create new media item first ?
    }
    return nullptr;
}

void ImageClip::Save(imgui_json::value& value)
{
    Clip::Save(value);
    // save image clip info
}

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

TextClip::~TextClip()
{
}

void TextClip::SetClipDefault(const DataLayer::SubtitleStyle & style)
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
    mFontOffsetH = style.OffsetH();
    mFontOffsetV = style.OffsetV();
    mFontShadowDepth = fabs(style.ShadowDepth());
    mFontPrimaryColor = style.PrimaryColor().ToImVec4();
    mFontOutlineColor = style.OutlineColor().ToImVec4();
    mFontBackColor = style.BackColor().ToImVec4();

    // pos value need load later 
    mFontPosX = - FLT_MAX;
    mFontPosY = - FLT_MAX;
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
}

void TextClip::CreateClipHold(void * _track)
{
    MediaTrack * track = (MediaTrack *)_track;
    if (!track || !track->mMttReader)
        return;
    mClipHolder = track->mMttReader->NewClip(mStart, mEnd - mStart);
    mTrack = track;
    mClipHolder->SetText(mText);
    mClipHolder->EnableUsingTrackStyle(mTrackStyle);
    if (!mTrackStyle)
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
    drawList->AddText(leftTop + ImVec2(2, 2), IM_COL32_WHITE, mText.c_str());
    ImGui::SetWindowFontScale(1.0);
    drawList->PopClipRect();
    drawList->AddRect(leftTop, rightBottom, IM_COL32_BLACK);
}

Clip * TextClip::Load(const imgui_json::value& value, void * handle)
{
    TimeLine * timeline = (TimeLine *)handle;
    if (!timeline || timeline->media_items.size() <= 0)
        return nullptr;
    
    // text clip don't band with media item
    {
        TextClip * new_clip = new TextClip(0, 0, 0, std::string(""), std::string(""), handle);
        if (new_clip)
        {
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
            return new_clip;
        }
    }
    return nullptr;
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

BluePrintVideoFilter::~BluePrintVideoFilter()
{
    if (mBp)
    {
        mBp->Finalize();
        delete mBp;
        mBp = nullptr;
    }
}

ImGui::ImMat BluePrintVideoFilter::FilterImage(const ImGui::ImMat& vmat, int64_t pos)
{
    std::lock_guard<std::mutex> lk(mBpLock);
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
        ImGui::ImMat outMat;
        mBp->Blueprint_RunFilter(inMat, outMat);
        return outMat;
    }
    return vmat;
}

void BluePrintVideoFilter::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    BluePrint::BluePrintUI* bp = new BluePrint::BluePrintUI();
    bp->Initialize();
    // Logger::Log(Logger::DEBUG) << "Create bp filter from json " << bpJson.dump() << std::endl;
    bp->File_New_Filter(bpJson, "VideoFilter", "Video");
    if (!bp->Blueprint_IsValid())
    {
        bp->Finalize();
        delete bp;
        return;
    }
    BluePrint::BluePrintUI* oldbp = nullptr;
    {
        std::lock_guard<std::mutex> lk(mBpLock);
        oldbp = mBp;
        mBp = bp;
    }
    if (oldbp)
    {
        oldbp->Finalize();
        delete oldbp;
    }
}

BluePrintVideoTransition::~BluePrintVideoTransition()
{
    if (mBp)
    {
        mBp->Finalize();
        delete mBp;
        mBp = nullptr;
    }
}

DataLayer::VideoTransitionHolder BluePrintVideoTransition::Clone()
{
    BluePrintVideoTransition* bpTrans = new BluePrintVideoTransition;
    auto bpJson = mBp->m_Document->Serialize();
    bpTrans->SetBluePrintFromJson(bpJson);
    return DataLayer::VideoTransitionHolder(bpTrans);
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
            auto value = mKeyPoints.GetValue(i, pos);
            mBp->Blueprint_SetFusion(name, value);
        }
        ImGui::ImMat inMat1(vmat1), inMat2(vmat2);
        ImGui::ImMat outMat;
        mBp->Blueprint_RunFusion(inMat1, inMat2, outMat, pos, dur);
        return outMat;
    }
    return vmat1;
}

void BluePrintVideoTransition::SetBluePrintFromJson(imgui_json::value& bpJson)
{
    BluePrint::BluePrintUI* bp = new BluePrint::BluePrintUI();
    bp->Initialize();
    // Logger::Log(Logger::DEBUG) << "Create bp transition from json " << bpJson.dump() << std::endl;
    bp->File_New_Fusion(bpJson, "VideoFusion", "Video");
    if (!bp->Blueprint_IsValid())
    {
        bp->Finalize();
        delete bp;
        return;
    }
    BluePrint::BluePrintUI* oldbp = nullptr;
    {
        std::lock_guard<std::mutex> lk(mBpLock);
        oldbp = mBp;
        mBp = bp;
    }
    if (oldbp)
    {
        oldbp->Finalize();
        delete oldbp;
    }
}

EditingVideoClip::EditingVideoClip(VideoClip* vidclip)
    : BaseEditingClip(vidclip->mID, vidclip->mType, vidclip->mStart, vidclip->mEnd, vidclip->mStartOffset, vidclip->mEndOffset, vidclip->mHandle)
{
    TimeLine * timeline = (TimeLine *)vidclip->mHandle;
    mDuration = mEnd-mStart;
    if (mDuration < 0)
        throw std::invalid_argument("Clip duration is negative!");
    mCurrent = mStartOffset;

    mSsGen = CreateSnapshotGenerator();
    mMediaReader = CreateMediaReader();
    if (!mSsGen || !mMediaReader)
    {
        Logger::Log(Logger::Error) << "Create Editing Video Clip FAILED!" << std::endl;
        return;
    }
    if (timeline) mSsGen->EnableHwAccel(timeline->mHardwareCodec);
    if (timeline) mMediaReader->EnableHwAccel(timeline->mHardwareCodec);
    if (!mSsGen->Open(vidclip->mSsViewer->GetMediaParser()))
    {
        Logger::Log(Logger::Error) << mSsGen->GetError() << std::endl;
        return;
    }

    mSsGen->SetCacheFactor(1);
    auto video_info = vidclip->mMediaParser->GetBestVideoStream();
    float snapshot_scale = video_info->height > 0 ? 50.f / (float)video_info->height : 0.1;
    mSsGen->SetSnapshotResizeFactor(snapshot_scale, snapshot_scale);
    mSsViewer = mSsGen->CreateViewer((double)mStartOffset / 1000);

    // open video reader
    if (mMediaReader->Open(vidclip->mMediaParser))
    {
        if (mMediaReader->ConfigVideoReader(1.f, 1.f))
        {
            mMediaReader->Start();
        }
        else
        {
            ReleaseMediaReader(&mMediaReader);
        }
    }
    mClipFrameRate = vidclip->mClipFrameRate;
}

EditingVideoClip::~EditingVideoClip()
{
    if (mMediaReader) { ReleaseMediaReader(&mMediaReader); mMediaReader = nullptr; }
    mSsViewer = nullptr;
    mSsGen = nullptr;
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();
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
        if (mCurrent < mStartOffset)
            mCurrent = mStartOffset;
        if (mCurrent > mStartOffset + mDuration)
            mCurrent = mStartOffset + mDuration;
        CalcDisplayParams();
    }
}

void EditingVideoClip::Seek(int64_t pos)
{
    static int64_t last_seek_pos = -1;
    if (last_seek_pos != pos)
    {
        last_seek_pos = pos;
    }
    else
    {
        return;
    }
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();
    mLastTime = -1;
    mCurrent = pos;
    alignTime(mCurrent, mClipFrameRate);
    if (mMediaReader && mMediaReader->IsOpened())
    {
        mMediaReader->SeekTo((double)mCurrent / 1000.f);
    }
}

void EditingVideoClip::Step(bool forward, int64_t step)
{
    if (forward)
    {
        bForward = true;
        if (step > 0) mCurrent += step;
        else frameStepTime(mCurrent, 1, mClipFrameRate);
        if (mCurrent >= mEnd - mStart + mStartOffset)
        {
            mCurrent = mEnd - mStart + mStartOffset;
            mLastTime = -1;
            bPlay = false;
        }
    }
    else
    {
        bForward = false;
        if (step > 0) mCurrent -= step;
        else frameStepTime(mCurrent, -1, mClipFrameRate);
        if (mCurrent <= mStartOffset)
        {
            mCurrent = mStartOffset;
            mLastTime = -1;
            bPlay = false;
        }
    }
}

void EditingVideoClip::Save()
{
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline)
        return;
    auto clip = timeline->FindClipByID(mID);
    if (!clip)
        return;
    timeline->mVideoFilterBluePrintLock.lock();
    if (timeline->mVideoFilterBluePrint && timeline->mVideoFilterBluePrint->Blueprint_IsValid())
    {
        clip->mFilterBP = timeline->mVideoFilterBluePrint->m_Document->Serialize();
    }
    timeline->mVideoFilterBluePrintLock.unlock();

    // update video filter in datalayer
    DataLayer::VideoClipHolder hClip = timeline->mMtvReader->GetClipById(clip->mID);
    BluePrintVideoFilter* bpvf = new BluePrintVideoFilter();
    bpvf->SetBluePrintFromJson(clip->mFilterBP);
    bpvf->SetKeyPoint(clip->mFilterKeyPoints);
    DataLayer::VideoFilterHolder hFilter(bpvf);
    hClip->SetFilter(hFilter);
    timeline->mMtvReader->Refresh();
}

bool EditingVideoClip::GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame)
{
    int ret = false;
    TimeLine * timeline = (TimeLine *)mHandle;
    if (!timeline || mFrame.empty())
        return ret;

    auto frame_delay = mClipFrameRate.den * 1000 / mClipFrameRate.num;
    int64_t buffer_start = mFrame.begin()->first.time_stamp * 1000;
    int64_t buffer_end = buffer_start;
    frameStepTime(buffer_end, bForward ? timeline->mMaxCachedVideoFrame : -timeline->mMaxCachedVideoFrame, mClipFrameRate);
    if (buffer_start > buffer_end)
        std::swap(buffer_start, buffer_end);

    bool out_of_range = false;
    if (mCurrent < buffer_start - frame_delay || mCurrent > buffer_end + frame_delay)
        out_of_range = true;

    for (auto pair = mFrame.begin(); pair != mFrame.end();)
    {
        bool need_erase = false;
        int64_t time_diff = fabs(pair->first.time_stamp * 1000 - mCurrent);
        if (time_diff > frame_delay)
            need_erase = true;

        if (need_erase || out_of_range)
        {
            // if we on seek stage, may output last frame for smooth preview
            if (bSeeking && pair != mFrame.end())
            {
                in_out_frame = *pair;
                ret = true;
            }
            mFrameLock.lock();
            pair = mFrame.erase(pair);
            mFrameLock.unlock();
            if (ret) break;
        }
        else
        {
            in_out_frame = *pair;
            ret = true;
            // handle clip play event
            if (bPlay)
            {
                bool need_step_time = false;
                int64_t step_time = 0;
                int64_t current_system_time = ImGui::get_current_time_usec() / 1000;
                if (mLastTime != -1)
                {
                    step_time = current_system_time - mLastTime;
                    if (step_time >= frame_delay)
                        need_step_time = true;
                }
                else
                {
                    mLastTime = current_system_time;
                    need_step_time = true;
                }
                if (need_step_time)
                {
                    Step(bForward, step_time);
                    mLastTime = current_system_time;
                }
            }
            else
            {
                mLastTime = -1;
            }
            break;
        }
    }
    return ret;
}

void EditingVideoClip::DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom)
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
        CalcDisplayParams();
    }

    std::vector<SnapshotGenerator::ImageHolder> snapImages;
    if (!mSsViewer->GetSnapshots((double)mStartOffset / 1000, snapImages))
    {
        Logger::Log(Logger::Error) << mSsViewer->GetError() << std::endl;
        return;
    }
    mSsViewer->UpdateSnapshotTexture(snapImages);

    ImVec2 imgLeftTop = leftTop;
    for (int i = 0; i < snapImages.size(); i++)
    {
        ImVec2 snapDispSize = mSnapSize;
        ImVec2 uvMin{0, 0}, uvMax{1, 1};
        if (imgLeftTop.x+mSnapSize.x > rightBottom.x)
        {
            snapDispSize.x = rightBottom.x - imgLeftTop.x;
            uvMax.x = snapDispSize.x / mSnapSize.x;
        }
        auto& img = snapImages[i];
        if (img->mTextureReady)
            drawList->AddImage(*(img->mTextureHolder), imgLeftTop, imgLeftTop + snapDispSize, uvMin, uvMax);
        else
        {
            drawList->AddRectFilled(imgLeftTop, imgLeftTop + snapDispSize, IM_COL32_BLACK);
            auto center_pos = imgLeftTop + snapDispSize / 2;
            ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
            ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
            ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
            ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
            drawList->AddRect(imgLeftTop, imgLeftTop + snapDispSize, COL_FRAME_RECT);
        }

        imgLeftTop.x += snapDispSize.x;
        if (imgLeftTop.x >= rightBottom.x)
            break;
    }
}

void EditingVideoClip::CalcDisplayParams()
{
    double snapWndSize = (double)mDuration / 1000;
    double snapCntInView = (double)mViewWndSize.x / mSnapSize.x;
    mSsGen->ConfigSnapWindow(snapWndSize, snapCntInView);
}

EditingAudioClip::EditingAudioClip(AudioClip* audclip)
    : BaseEditingClip(audclip->mID, audclip->mType, audclip->mStart, audclip->mEnd, audclip->mStartOffset, audclip->mEndOffset, audclip->mHandle)
{}

EditingAudioClip::~EditingAudioClip()
{}

void EditingAudioClip::UpdateClipRange(Clip* clip)
{}

void EditingAudioClip::Seek(int64_t pos)
{
    mCurrent = pos;
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
    timeline->mAudioFilterBluePrintLock.lock();
    if (timeline->mAudioFilterBluePrint && timeline->mAudioFilterBluePrint->Blueprint_IsValid())
    {
        clip->mFilterBP = timeline->mAudioFilterBluePrint->m_Document->Serialize();
    }
    timeline->mAudioFilterBluePrintLock.unlock();

    // TODO::Dicky update audio filter in datalayer

}

bool EditingAudioClip::GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame)
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
    MediaOverview::WaveformHolder waveform = aclip->mWaveform;
    drawList->AddRectFilled(leftTop, rightBottom, IM_COL32(16, 16, 16, 255));
    drawList->AddRect(leftTop, rightBottom, IM_COL32(128, 128, 128, 255));
    float wave_range = fmax(fabs(waveform->minSample), fabs(waveform->maxSample));
    auto window_size = rightBottom - leftTop;
    window_size.y /= waveform->pcm.size();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    for (int i = 0; i < waveform->pcm.size(); i++)
    {
        std::string id_string = "##Waveform_editing@" + std::to_string(mID) + "@" +std::to_string(i);
        int sampleSize = waveform->pcm[i].size();
        if (sampleSize <= 0) continue;
        int sample_stride = sampleSize / window_size.x;
        int zoom = ImMin(sample_stride, 32);
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, {0, 0});
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.f);
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
        if (ImPlot::BeginPlot(id_string.c_str(), window_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
        {
            std::string plot_id = id_string + "_line";
            ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            ImPlot::SetupAxesLimits(0, sampleSize / zoom, -wave_range, wave_range, ImGuiCond_Always);
            ImPlot::PlotLine(plot_id.c_str(), waveform->pcm[i].data(), sampleSize / zoom, 1.0, 0.0, 0, 0, sizeof(float) * zoom);
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor();
        ImPlot::PopStyleVar(2);
    }
    ImGui::PopStyleVar();
}
} //namespace MediaTimeline/Clip

namespace MediaTimeline
{
Overlap::Overlap(int64_t start, int64_t end, int64_t clip_first, int64_t clip_second, MEDIA_TYPE type, void* handle)
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
    m_Clip.first = start_clip_id;
    m_Clip.second = end_clip_id;
    mStart = start;
    mEnd = end;
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

EditingVideoOverlap::EditingVideoOverlap(Overlap* ovlp)
    : BaseEditingOverlap(ovlp)
{
    TimeLine* timeline = (TimeLine*)(ovlp->mHandle);
    VideoClip* vidclip1 = (VideoClip*)timeline->FindClipByID(ovlp->m_Clip.first);
    VideoClip* vidclip2 = (VideoClip*)timeline->FindClipByID(ovlp->m_Clip.second);
    if (vidclip1 && vidclip2)
    {
        mClip1 = vidclip1; mClip2 = vidclip2;
        mSsGen1 = CreateSnapshotGenerator();
        if (timeline) mSsGen1->EnableHwAccel(timeline->mHardwareCodec);
        if (!mSsGen1->Open(vidclip1->mSsViewer->GetMediaParser()))
            throw std::runtime_error("FAILED to open the snapshot generator for the 1st video clip!");
        auto video1_info = vidclip1->mSsViewer->GetMediaParser()->GetBestVideoStream();
        float snapshot_scale1 = video1_info->height > 0 ? 50.f / (float)video1_info->height : 0.1;
        mSsGen1->SetCacheFactor(1.0);
        mSsGen1->SetSnapshotResizeFactor(snapshot_scale1, snapshot_scale1);
        m_StartOffset.first = vidclip1->mStartOffset + ovlp->mStart - vidclip1->mStart;
        mViewer1 = mSsGen1->CreateViewer(m_StartOffset.first);
        mSsGen2 = CreateSnapshotGenerator();
        if (timeline) mSsGen2->EnableHwAccel(timeline->mHardwareCodec);
        if (!mSsGen2->Open(vidclip2->mSsViewer->GetMediaParser()))
            throw std::runtime_error("FAILED to open the snapshot generator for the 2nd video clip!");
        auto video2_info = vidclip2->mSsViewer->GetMediaParser()->GetBestVideoStream();
        float snapshot_scale2 = video2_info->height > 0 ? 50.f / (float)video2_info->height : 0.1;
        mSsGen2->SetCacheFactor(1.0);
        mSsGen2->SetSnapshotResizeFactor(snapshot_scale2, snapshot_scale2);
        m_StartOffset.second = vidclip2->mStartOffset + ovlp->mStart - vidclip2->mStart;
        mViewer2 = mSsGen2->CreateViewer(m_StartOffset.second);
        mStart = ovlp->mStart;
        mEnd = ovlp->mEnd;
        mDuration = mEnd - mStart;
        
        mMediaReader.first = CreateMediaReader();
        mMediaReader.second = CreateMediaReader();

        if (!mMediaReader.first || !mMediaReader.second)
        {
            Logger::Log(Logger::Error) << "Create Fusion Video Clip" << std::endl;
            return;
        }
        if (timeline) mMediaReader.first->EnableHwAccel(timeline->mHardwareCodec);
        if (timeline) mMediaReader.second->EnableHwAccel(timeline->mHardwareCodec);
        // open first video reader
        if (mMediaReader.first->Open(mClip1->mSsViewer->GetMediaParser()))
        {
            if (mMediaReader.first->ConfigVideoReader(1.f, 1.f))
            {
                mMediaReader.first->Start();
            }
            else
            {
                ReleaseMediaReader(&mMediaReader.first);
            }
        }
        // open second video reader
        if (mMediaReader.second->Open(vidclip2->mSsViewer->GetMediaParser()))
        {
            if (mMediaReader.second->ConfigVideoReader(1.f, 1.f))
            {
                mMediaReader.second->Start();
            }
            else
            {
                ReleaseMediaReader(&mMediaReader.second);
            }
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
    if (mMediaReader.first) { ReleaseMediaReader(&mMediaReader.first); mMediaReader.first = nullptr; }
    if (mMediaReader.second) { ReleaseMediaReader(&mMediaReader.second); mMediaReader.second = nullptr; }
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();
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
        const MediaInfo::VideoStream* vidStream = mSsGen1->GetVideoStream();
        if (vidStream->width == 0 || vidStream->height == 0)
        {
            Logger::Log(Logger::Error) << "Snapshot video size is INVALID! Width or height is ZERO." << std::endl;
            return;
        }
        mSnapSize.y = viewWndSize.y/2;
        mSnapSize.x = mSnapSize.y * vidStream->width / vidStream->height;
    }
    if (mViewWndSize.x == 0 || mViewWndSize.y == 0)
        return;
    if (ovlpRngChanged || vwndChanged)
    {
        CalcDisplayParams();
    }

    // get snapshot images
    m_StartOffset.first = mClip1->mStartOffset + mOvlp->mStart-mClip1->mStart;
    std::vector<SnapshotGenerator::ImageHolder> snapImages1;
    if (!mViewer1->GetSnapshots((double)m_StartOffset.first/1000, snapImages1))
    {
        Logger::Log(Logger::Error) << mViewer1->GetError() << std::endl;
        return;
    }
    mViewer1->UpdateSnapshotTexture(snapImages1);
    m_StartOffset.second = mClip2->mStartOffset + mOvlp->mStart-mClip2->mStart;
    std::vector<SnapshotGenerator::ImageHolder> snapImages2;
    if (!mViewer2->GetSnapshots((double)m_StartOffset.second/1000, snapImages2))
    {
        Logger::Log(Logger::Error) << mViewer2->GetError() << std::endl;
        return;
    }
    mViewer2->UpdateSnapshotTexture(snapImages2);

    // draw snapshot images
    ImVec2 imgLeftTop = leftTop;
    auto imgIter1 = snapImages1.begin();
    auto imgIter2 = snapImages2.begin();
    while (imgIter1 != snapImages1.end() || imgIter2 != snapImages2.end())
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
            ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
            ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
            ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
            ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
            drawList->AddRect(imgLeftTop, imgLeftTop + snapDispSize, COL_FRAME_RECT);
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
            ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
            ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
            ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
            ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
            drawList->AddRect(img2LeftTop, img2LeftTop + snapDispSize, COL_FRAME_RECT);
        }

        imgLeftTop.x += snapDispSize.x;
        if (imgLeftTop.x >= rightBottom.x)
            break;
    }
}

void EditingVideoOverlap::CalcDisplayParams()
{
    double snapWndSize = (double)mDuration / 1000;
    double snapCntInView = (double)mViewWndSize.x / mSnapSize.x;
    mSsGen1->ConfigSnapWindow(snapWndSize, snapCntInView);
    mSsGen2->ConfigSnapWindow(snapWndSize, snapCntInView);
}

void EditingVideoOverlap::Seek(int64_t pos)
{
    TimeLine* timeline = (TimeLine*)(mOvlp->mHandle);
    if (!timeline)
        return;
    static int64_t last_seek_pos = -1;
    if (last_seek_pos != pos)
    {
        last_seek_pos = pos;
    }
    else
    {
        return;
    }
    
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();
    mLastTime = -1;
    mCurrent = pos;
    alignTime(mCurrent, timeline->mFrameRate);
    if (mMediaReader.first && mMediaReader.first->IsOpened())
    {
        int64_t pos = mCurrent + m_StartOffset.first;
        alignTime(pos, mClipFirstFrameRate);
        mMediaReader.first->SeekTo((double)pos / 1000.f);
    }
    if (mMediaReader.second && mMediaReader.second->IsOpened())
    {
        int64_t pos = mCurrent + m_StartOffset.second;
        alignTime(pos, mClipSecondFrameRate);
        mMediaReader.second->SeekTo((double)pos / 1000.f);
    }
}

void EditingVideoOverlap::Step(bool forward, int64_t step)
{
    TimeLine* timeline = (TimeLine*)(mOvlp->mHandle);
    if (!timeline)
        return;
    if (forward)
    {
        bForward = true;
        if (step > 0) mCurrent += step;
        else
        {
            frameStepTime(mCurrent, 1, timeline->mFrameRate);
        }
        if (mCurrent > mEnd - mStart)
        {
            mCurrent = mEnd - mStart;
            mLastTime = -1;
            bPlay = false;
            timeline->mVideoFusionNeedUpdate = true;
        }
    }
    else
    {
        bForward = false;
        if (step > 0) mCurrent -= step;
        else
        {
            frameStepTime(mCurrent, -1, timeline->mFrameRate);
        }
        if (mCurrent < 0)
        {
            mCurrent = 0;
            mLastTime = -1;
            bPlay = false;
            timeline->mVideoFusionNeedUpdate = true;
        }
    }
}

bool EditingVideoOverlap::GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame)
{
    int ret = false;
    TimeLine* timeline = (TimeLine*)(mOvlp->mHandle);
    if (!timeline || mFrame.empty())
        return ret;

    auto frame_delay_first = mClipFirstFrameRate.den * 1000 / mClipFirstFrameRate.num;
    auto frame_delay_second = mClipSecondFrameRate.den * 1000 / mClipSecondFrameRate.num;

    int64_t buffer_start_first = mFrame.begin()->first.first.time_stamp * 1000;
    int64_t buffer_end_first = buffer_start_first;
    frameStepTime(buffer_end_first, bForward ? timeline->mMaxCachedVideoFrame : -timeline->mMaxCachedVideoFrame, mClipFirstFrameRate);
    if (buffer_start_first > buffer_end_first)
        std::swap(buffer_start_first, buffer_end_first);

    int64_t buffer_start_second = mFrame.begin()->first.second.time_stamp * 1000;
    int64_t buffer_end_second = buffer_start_second;
    frameStepTime(buffer_end_second, bForward ? timeline->mMaxCachedVideoFrame : -timeline->mMaxCachedVideoFrame, mClipSecondFrameRate);
    if (buffer_start_second > buffer_end_second)
        std::swap(buffer_start_second, buffer_end_second);

    bool out_of_range = false;
    if (mCurrent + m_StartOffset.first < buffer_start_first - frame_delay_first || mCurrent + m_StartOffset.first > buffer_end_first + frame_delay_first)
        out_of_range = true;
    if (mCurrent + m_StartOffset.second < buffer_start_second - frame_delay_second || mCurrent + m_StartOffset.second > buffer_end_second + frame_delay_second)
        out_of_range = true;

    for (auto pair = mFrame.begin(); pair != mFrame.end();)
    {
        bool need_erase = false;
        int64_t time_diff_first = fabs(pair->first.first.time_stamp * 1000 - (mCurrent + m_StartOffset.first));
        if (time_diff_first > frame_delay_first)
            need_erase = true;
        int64_t time_diff_second = fabs(pair->first.second.time_stamp * 1000 - (mCurrent + m_StartOffset.second));
        if (time_diff_second > frame_delay_second)
            need_erase = true;

        if (need_erase || out_of_range)
        {
            // TODO::Dicky check seek will jitter
            // if we on seek stage, may output last frame for smooth preview
            //if (bSeeking && pair != mFrame.end())
            //{
            //    in_out_frame = *pair;
            //    ret = true;
            //}
            mFrameLock.lock();
            pair = mFrame.erase(pair);
            mFrameLock.unlock();
            if (ret) break;
        }
        else
        {
            in_out_frame = *pair;
            ret = true;
            // handle clip play event
            if (bPlay)
            {
                bool need_step_time = false;
                int64_t step_time = 0;
                int64_t current_system_time = ImGui::get_current_time_usec() / 1000;
                if (mLastTime != -1)
                {
                    step_time = current_system_time - mLastTime;
                    if (step_time >= frame_delay_first || step_time >= frame_delay_second)
                        need_step_time = true;
                }
                else
                {
                    mLastTime = current_system_time;
                    need_step_time = true;
                }
                if (need_step_time)
                {
                    mLastTime = current_system_time;
                    Step(bForward, step_time);
                }
            }
            else
            {
                mLastTime = -1;
            }
            break;
        }
    }
    return ret;
}

void EditingVideoOverlap::Save()
{
    TimeLine * timeline = (TimeLine *)(mOvlp->mHandle);
    if (!timeline)
        return;
    timeline->mVideoFusionBluePrintLock.lock();
    if (timeline->mVideoFusionBluePrint && timeline->mVideoFusionBluePrint->Blueprint_IsValid())
    {
        mOvlp->mFusionBP = timeline->mVideoFusionBluePrint->m_Document->Serialize();
    }
    timeline->mVideoFusionBluePrintLock.unlock();

    // update video filter in datalayer
    DataLayer::VideoOverlapHolder hOvlp = timeline->mMtvReader->GetOverlapById(mOvlp->mID);
    BluePrintVideoTransition* bpvt = new BluePrintVideoTransition();
    bpvt->SetBluePrintFromJson(mOvlp->mFusionBP);
    bpvt->SetKeyPoint(mOvlp->mFusionKeyPoints);
    DataLayer::VideoTransitionHolder hTrans(bpvt);
    hOvlp->SetTransition(hTrans);
    timeline->mMtvReader->Refresh();
}

}// namespace MediaTimeline

namespace MediaTimeline
{
/***********************************************************************************************************
 * MediaTrack Struct Member Functions
 ***********************************************************************************************************/
MediaTrack::MediaTrack(std::string name, MEDIA_TYPE type, void * handle)
    : m_Handle(handle),
      mType(type)
{
    TimeLine * timeline = (TimeLine *)handle;
    mID = timeline ? timeline->m_IDGenerator.GenerateID() : ImGui::get_current_time_usec();
    if (name.empty())
    {
        TimeLine * timeline = (TimeLine *)m_Handle;
        if (timeline)
        {
            auto media_count = timeline->GetTrackCount(type);
            media_count ++;
            switch (type)
            {
                case MEDIA_VIDEO:
                    mName = "V:";
                    mTrackHeight = DEFAULT_VIDEO_TRACK_HEIGHT;
                break;
                case MEDIA_AUDIO:
                    mName = "A:";
                    mTrackHeight = DEFAULT_AUDIO_TRACK_HEIGHT;
                break;
                case MEDIA_PICTURE:
                    mName = "P:";
                    mTrackHeight = DEFAULT_IMAGE_TRACK_HEIGHT;
                break;
                case MEDIA_TEXT:
                    mName = "T:";
                    mTrackHeight = DEFAULT_TEXT_TRACK_HEIGHT;
                break;
                default:
                    mName = "U:";
                    mTrackHeight = DEFAULT_TRACK_HEIGHT;
                break;
            }
            mName += std::to_string(media_count);
        }
    }
    else
    {
        mName = name;
    }
}

MediaTrack::~MediaTrack()
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline)
        return;

    // remove overlaps from timeline
    for (auto overlap : m_Overlaps)
    {
        timeline->DeleteOverlap(overlap->mID);
    }

    // remove clips from timeline
    for (auto clip : m_Clips)
    {
        timeline->DeleteClip(clip->mID);
    }

    // remove linked track info
    auto linked_track = timeline->FindTrackByID(mLinkedTrack);
    if (linked_track)
    {
        linked_track->mLinkedTrack = -1;
    }

    if (mMttReader)
    {
        timeline->mMtvReader->RemoveSubtitleTrackById(mID);
    }
}

bool MediaTrack::DrawTrackControlBar(ImDrawList *draw_list, ImRect rc)
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
    if (mType == MEDIA_AUDIO)
    {
        bool ret = TimelineButton(draw_list, mView ? ICON_SPEAKER_MUTE : ICON_SPEAKER, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mView ? "voice" : "mute");
        if (ret && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            mView = !mView;
        button_count ++;
    }
    else
    {
        bool ret = TimelineButton(draw_list, mView ? ICON_VIEW : ICON_VIEW_DISABLE, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mView ? "hidden" : "view");
        if (ret && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            mView = !mView;
            need_update = true;
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
}

void MediaTrack::CreateOverlap(int64_t start, int64_t start_clip_id, int64_t end, int64_t end_clip_id, MEDIA_TYPE type)
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
        if ((*iter)->mType == MEDIA_TEXT)
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
    Update();
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
    if (!clip || mType != clip->mType)
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

void MediaTrack::InsertClip(Clip * clip, int64_t pos, bool update)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !clip)
        return;
        
    auto iter = std::find_if(m_Clips.begin(), m_Clips.end(), [clip](const Clip * _clip)
    {
        return _clip->mID == clip->mID;
    });
    if (iter == m_Clips.end())
    {
        int64_t length = clip->mEnd - clip->mStart;
        clip->mStart = pos == -1 ? 0 : pos;
        clip->mEnd = clip->mStart + length;
        clip->ConfigViewWindow(mViewWndDur, mPixPerMs);
        clip->SetTrackHeight(mTrackHeight);
        m_Clips.push_back(clip);
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
    
    int updated = 0;
    if (filter_editing && timeline->m_CallBacks.EditingClipFilter)
    {
        if (clip->mType == MEDIA_TEXT) timeline->Seek(clip->mStart);
        updated = timeline->m_CallBacks.EditingClipFilter(clip->mType, clip);
    }
    else if (timeline->m_CallBacks.EditingClipAttribute)
        updated = timeline->m_CallBacks.EditingClipAttribute(clip->mType, clip);

    // find old editing clip and reset BP
    auto editing_clip = timeline->FindEditingClip();
    if (editing_clip && editing_clip->mID == clip->mID)
    {
        if (editing_clip->mType == MEDIA_VIDEO)
        {
            if (filter_editing && timeline->mVidFilterClip && timeline->mVideoFilterBluePrint && timeline->mVideoFilterBluePrint->Blueprint_IsValid())
                return;
        }
        if (editing_clip->mType == MEDIA_AUDIO)
        {
            if (filter_editing && timeline->mAudFilterClip && timeline->mAudioFilterBluePrint && timeline->mAudioFilterBluePrint->Blueprint_IsValid())
                return;
        }
    }
    else if (editing_clip && editing_clip->mID != clip->mID)
    {
        if (editing_clip->mType == MEDIA_VIDEO)
        {
            if (!updated && timeline->mVidFilterClip)
            {
                timeline->mVidFilterClip->bPlay = false;
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
        else if (editing_clip->mType == MEDIA_AUDIO)
        {
            if (!updated && timeline->mAudFilterClip)
            {
                timeline->mAudFilterClip->bPlay = false;
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
    if (filter_editing)
    {
        if (clip->mType == MEDIA_VIDEO)
        {
            if (!timeline->mVidFilterClip)
                timeline->mVidFilterClip = new EditingVideoClip((VideoClip*)clip);
            if (timeline->mVideoFilterBluePrint && timeline->mVideoFilterBluePrint->m_Document)
            {                
                timeline->mVideoFilterBluePrintLock.lock();
                timeline->mVideoFilterBluePrint->File_New_Filter(clip->mFilterBP, "VideoFilter", "Video");
                timeline->mVideoFilterNeedUpdate = true;
                timeline->mVideoFilterBluePrintLock.unlock();
            }
        }
        else if (clip->mType == MEDIA_AUDIO)
        {
            if (!timeline->mAudFilterClip)
                timeline->mAudFilterClip = new EditingAudioClip((AudioClip*)clip);
            if (timeline->mAudioFilterBluePrint && timeline->mAudioFilterBluePrint->m_Document)
            {                
                timeline->mAudioFilterBluePrintLock.lock();
                timeline->mAudioFilterBluePrint->File_New_Filter(clip->mFilterBP, "AudioFilter", "Audio");
                timeline->mAudioFilterNeedUpdate = true;
                timeline->mAudioFilterBluePrintLock.unlock();
            }
        }
    }
}

void MediaTrack::SelectEditingOverlap(Overlap * overlap)
{
    TimeLine * timeline = (TimeLine *)m_Handle;
    if (!timeline || !overlap)
        return;
    
    // find old editing overlap and reset BP
    Overlap * editing_overlap = timeline->FindEditingOverlap();

    if (editing_overlap && editing_overlap->mID != overlap->mID)
    {
        auto clip_first = timeline->FindClipByID(editing_overlap->m_Clip.first);
        auto clip_second = timeline->FindClipByID(editing_overlap->m_Clip.second);
        if (clip_first && clip_second)
        {
            if (clip_first->mType == MEDIA_VIDEO && 
                clip_second->mType == MEDIA_VIDEO &&
                timeline->mVideoFusionBluePrint &&
                timeline->mVideoFusionBluePrint->Blueprint_IsValid())
            {
                timeline->mVideoFusionBluePrintLock.lock();
                editing_overlap->mFusionBP = timeline->mVideoFusionBluePrint->m_Document->Serialize();
                timeline->mVideoFusionBluePrintLock.unlock();
            }
            if (clip_first->mType == MEDIA_AUDIO && 
                clip_second->mType == MEDIA_AUDIO &&
                timeline->mAudioFusionBluePrint &&
                timeline->mAudioFusionBluePrint->Blueprint_IsValid())
            {
                timeline->mAudioFusionBluePrintLock.lock();
                editing_overlap->mFusionBP = timeline->mAudioFusionBluePrint->m_Document->Serialize();
                timeline->mAudioFusionBluePrintLock.unlock();
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
    if (first->mType == MEDIA_VIDEO && second->mType == MEDIA_VIDEO &&
        timeline->mVideoFusionBluePrint && timeline->mVideoFusionBluePrint->m_Document)
    {                
        timeline->mVideoFusionBluePrintLock.lock();
        timeline->mVideoFusionBluePrint->File_New_Fusion(overlap->mFusionBP, "VideoFusion", "Video");
        timeline->mVideoFusionNeedUpdate = true;
        timeline->mVideoFusionBluePrintLock.unlock();
        if (!timeline->mVidOverlap)
            timeline->mVidOverlap = new EditingVideoOverlap(overlap);
    }
    if (first->mType == MEDIA_AUDIO && second->mType == MEDIA_AUDIO &&
        timeline->mAudioFusionBluePrint && timeline->mAudioFusionBluePrint->m_Document)
    {                
        timeline->mAudioFusionBluePrintLock.lock();
        timeline->mAudioFusionBluePrint->File_New_Fusion(overlap->mFusionBP, "AudioFusion", "Audio");
        timeline->mAudioFusionNeedUpdate = true;
        timeline->mAudioFusionBluePrintLock.unlock();
    }
    if (timeline->m_CallBacks.EditingOverlap)
    {
        timeline->m_CallBacks.EditingOverlap(first->mType, overlap);
    }
}

MediaTrack* MediaTrack::Load(const imgui_json::value& value, void * handle)
{
    MEDIA_TYPE type = MEDIA_UNKNOWN;
    std::string name;
    TimeLine * timeline = (TimeLine *)handle;
    if (!timeline)
        return nullptr;
    
    if (value.contains("Type"))
    {
        auto& val = value["Type"];
        if (val.is_number()) type = (MEDIA_TYPE)val.get<imgui_json::number>();
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
            new_track->mMttReader = timeline->mMtvReader->NewEmptySubtitleTrack(track_id); //DataLayer::SubtitleTrack::NewEmptyTrack(track_id);
            if (track.contains("Font"))
            {
                auto& val = track["Font"];
                if (val.is_string()) new_track->mMttReader->SetFont(val.get<imgui_json::string>());
            }
            if (track.contains("OffsetX"))
            {
                auto& val = track["OffsetX"];
                if (val.is_number()) new_track->mMttReader->SetOffsetH(val.get<imgui_json::number>());
            }
            if (track.contains("OffsetY"))
            {
                auto& val = track["OffsetY"];
                if (val.is_number()) new_track->mMttReader->SetOffsetV(val.get<imgui_json::number>());
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
            new_track->mMttReader->SetFrameSize(timeline->mWidth, timeline->mHeight);
            new_track->mMttReader->EnableFullSizeOutput(false);
            for (auto clip : new_track->m_Clips)
            {
                if (clip->mType == MEDIA_TEXT)
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

    // save subtitle track info
    if (mMttReader)
    {
        imgui_json::value subtrack;
        auto& style = mMttReader->DefaultStyle();
        subtrack["ID"] = imgui_json::number(mMttReader->Id());
        subtrack["Font"] = style.Font();
        subtrack["OffsetX"] = imgui_json::number(style.OffsetH());
        subtrack["OffsetY"] = imgui_json::number(style.OffsetV());
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
    if (m_Grouped_Clips.size() > 0)
    {
        value["ID"] = imgui_json::number(mID);
        value["Color"] = imgui_json::number(mColor);
        imgui_json::value clips;
        for (auto clip : m_Grouped_Clips)
        {
            imgui_json::value clip_id_value = imgui_json::number(clip);
            clips.push_back(clip_id_value);
        }
        value["ClipIDS"] = clips;
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
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            timeline->mVideoFilterNeedUpdate = true;
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            timeline->mVideoFilterNeedUpdate = true;
        }
    }
    if (name.compare("VideoFusion") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            timeline->mVideoFusionNeedUpdate = true;
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            timeline->mVideoFusionNeedUpdate = true;
        }
    }

    return ret;
}

static int thread_video_filter(TimeLine * timeline)
{
    if (!timeline)
        return -1;
    timeline->mVideoFilterRunning = true;
    while (!timeline->mVideoFilterDone)
    {
        if (timeline->mVideoFilterNeedUpdate && timeline->mVidFilterClip)
        {
            timeline->mVidFilterClip->mFrameLock.lock();
            timeline->mVidFilterClip->mFrame.clear();
            timeline->mVidFilterClip->mLastFrameTime = -1;
            timeline->mVidFilterClip->mLastTime = -1;
            timeline->mVidFilterClip->mFrameLock.unlock();
            timeline->mVideoFilterNeedUpdate = false;
        }
        if (!timeline->mVidFilterClip || !timeline->mVidFilterClip->mMediaReader || !timeline->mVidFilterClip->mMediaReader->IsOpened() ||
            !timeline->mVideoFilterBluePrint || !timeline->mVideoFilterBluePrint->Blueprint_IsValid())
        {
            ImGui::sleep((int)5);
            continue;
        }
        Clip * editing_clip = timeline->FindEditingClip();
        if (!editing_clip)
        {
            ImGui::sleep((int)5);
            continue;
        }
        timeline->mVidFilterClipLock.lock();
        {
            if (!timeline->mVidFilterClip || timeline->mVidFilterClip->mFrame.size() >= timeline->mMaxCachedVideoFrame)
            {
                timeline->mVidFilterClipLock.unlock();
                ImGui::sleep((int)5);
                continue;
            }
            int64_t current_time = 0;
            timeline->mVidFilterClip->mFrameLock.lock();
            if (timeline->mVidFilterClip->mFrame.empty() || timeline->mVidFilterClip->bSeeking)
                current_time = timeline->mVidFilterClip->mCurrent;
            else
            {
                auto it = timeline->mVidFilterClip->mFrame.end(); it--;
                current_time = it->first.time_stamp * 1000;
            }
            const int64_t frame_delay = timeline->mVidFilterClip->mClipFrameRate.den * 1000 / timeline->mVidFilterClip->mClipFrameRate.num;
            alignTime(current_time, timeline->mVidFilterClip->mClipFrameRate);
            timeline->mVidFilterClip->mFrameLock.unlock();
            while (timeline->mVidFilterClip->mFrame.size() < timeline->mMaxCachedVideoFrame)
            {
                bool eof = false;
                if (timeline->mVideoFilterDone)
                    break;
                if (!timeline->mVidFilterClip->mFrame.empty())
                {
                    int64_t buffer_start = timeline->mVidFilterClip->mFrame.begin()->first.time_stamp * 1000;
                    int64_t buffer_end = buffer_start;
                    frameStepTime(buffer_end, timeline->mVidFilterClip->bForward ? timeline->mMaxCachedVideoFrame : -timeline->mMaxCachedVideoFrame, timeline->mVidFilterClip->mClipFrameRate);
                    if (buffer_start > buffer_end)
                        std::swap(buffer_start, buffer_end);
                    if (timeline->mVidFilterClip->mCurrent < buffer_start - frame_delay || timeline->mVidFilterClip->mCurrent > buffer_end + frame_delay)
                    {
                        ImGui::sleep((int)5);
                        break;
                    }
                }
                std::pair<ImGui::ImMat, ImGui::ImMat> result;
                if ((timeline->mVidFilterClip->mMediaReader->IsDirectionForward() && !timeline->mVidFilterClip->bForward) ||
                    (!timeline->mVidFilterClip->mMediaReader->IsDirectionForward() && timeline->mVidFilterClip->bForward))
                    timeline->mVidFilterClip->mMediaReader->SetDirection(timeline->mVidFilterClip->bForward);
                if (timeline->mVidFilterClip->mMediaReader->ReadVideoFrame((float)current_time / 1000.0, result.first, eof))
                {
                    result.first.time_stamp = (double)current_time / 1000.f;
                    timeline->mVideoFilterBluePrintLock.lock();
                    // setup bp input curve
                    for (int i = 0; i < editing_clip->mFilterKeyPoints.GetCurveCount(); i++)
                    {
                        auto name = editing_clip->mFilterKeyPoints.GetCurveName(i);
                        auto value = editing_clip->mFilterKeyPoints.GetValue(i, current_time);
                        timeline->mVideoFilterBluePrint->Blueprint_SetFilter(name, value);
                    }
                    if (timeline->mVideoFilterBluePrint->Blueprint_RunFilter(result.first, result.second))
                    {
                        timeline->mVidFilterClip->mFrameLock.lock();
                        timeline->mVidFilterClip->mFrame.push_back(result);
                        timeline->mVidFilterClip->mFrameLock.unlock();
                        if (timeline->mVidFilterClip->bForward)
                        {
                            frameStepTime(current_time, 1, timeline->mVidFilterClip->mClipFrameRate);
                            if (current_time > timeline->mVidFilterClip->mEnd - timeline->mVidFilterClip->mStart + timeline->mVidFilterClip->mStartOffset)
                                current_time = timeline->mVidFilterClip->mEnd - timeline->mVidFilterClip->mStart + timeline->mVidFilterClip->mStartOffset;
                        }
                        else
                        {
                            frameStepTime(current_time, -1, timeline->mVidFilterClip->mClipFrameRate);
                            if (current_time < timeline->mVidFilterClip->mStartOffset)
                            {
                                current_time = timeline->mVidFilterClip->mStartOffset;
                            }
                        }
                    }
                    timeline->mVideoFilterBluePrintLock.unlock();
                }
            }
        }
        timeline->mVidFilterClipLock.unlock();
    }
    timeline->mVideoFilterRunning = false;
    return 0;
}

static int thread_video_fusion(TimeLine * timeline)
{
    if (!timeline)
        return -1;
    timeline->mVideoFusionRunning = true;
    while (!timeline->mVideoFusionDone)
    {
        if (timeline->mVideoFusionNeedUpdate && timeline->mVidOverlap)
        {
            timeline->mVidOverlap->mFrameLock.lock();
            timeline->mVidOverlap->mFrame.clear();
            timeline->mVidOverlap->mLastFrameTime = -1;
            timeline->mVidOverlap->mFrameLock.unlock();
            timeline->mVideoFusionNeedUpdate = false;
        }
        if (!timeline->mVidOverlap || !timeline->mVidOverlap->mMediaReader.first || !timeline->mVidOverlap->mMediaReader.second ||
            !timeline->mVidOverlap->mMediaReader.first->IsOpened() || !timeline->mVidOverlap->mMediaReader.second->IsOpened() ||
            !timeline->mVideoFusionBluePrint || !timeline->mVideoFusionBluePrint->Blueprint_IsValid())
        {
            ImGui::sleep((int)5);
            continue;
        }
        Overlap * editing_overlap = timeline->FindEditingOverlap();
        if (!editing_overlap)
        {
            ImGui::sleep((int)5);
            continue;
        }
        timeline->mVidFusionLock.lock();
        {
            if (!timeline->mVidOverlap || timeline->mVidOverlap->mFrame.size() >= timeline->mMaxCachedVideoFrame)
            {
                timeline->mVidFusionLock.unlock();
                ImGui::sleep((int)5);
                continue;
            }
            int64_t current_time_first = 0;
            int64_t current_time_second = 0;
            int64_t current_time = 0;
            timeline->mVidOverlap->mFrameLock.lock();
            if (timeline->mVidOverlap->mFrame.empty() || timeline->mVidOverlap->bSeeking)
            {
                current_time_first = timeline->mVidOverlap->mCurrent + timeline->mVidOverlap->m_StartOffset.first;
                current_time_second = timeline->mVidOverlap->mCurrent + timeline->mVidOverlap->m_StartOffset.second;
            }
            else
            {
                auto it = timeline->mVidOverlap->mFrame.end(); it--;
                current_time_first = it->first.first.time_stamp * 1000;
                current_time_second = it->first.second.time_stamp * 1000;
            }
            const int64_t frame_delay_first = timeline->mVidOverlap->mClipFirstFrameRate.den * 1000 / timeline->mVidOverlap->mClipFirstFrameRate.num;
            const int64_t frame_delay_second = timeline->mVidOverlap->mClipSecondFrameRate.den * 1000 / timeline->mVidOverlap->mClipSecondFrameRate.num;
            alignTime(current_time_first, timeline->mVidOverlap->mClipFirstFrameRate);
            alignTime(current_time_second, timeline->mVidOverlap->mClipSecondFrameRate);
            timeline->mVidOverlap->mFrameLock.unlock();
            while (timeline->mVidOverlap->mFrame.size() < timeline->mMaxCachedVideoFrame)
            {
                bool eof_first = false;
                bool eof_second = false;
                if (timeline->mVideoFusionDone)
                    break;
                if (!timeline->mVidOverlap->mFrame.empty())
                {
                    int64_t buffer_start_first = timeline->mVidOverlap->mFrame.begin()->first.first.time_stamp * 1000;
                    int64_t buffer_end_first = buffer_start_first;
                    frameStepTime(buffer_end_first, timeline->mVidOverlap->bForward ? timeline->mMaxCachedVideoFrame : -timeline->mMaxCachedVideoFrame, timeline->mVidOverlap->mClipFirstFrameRate);
                    if (buffer_start_first > buffer_end_first)
                        std::swap(buffer_start_first, buffer_end_first);
                    if (timeline->mVidOverlap->mCurrent + timeline->mVidOverlap->m_StartOffset.first < buffer_start_first - frame_delay_first ||
                        timeline->mVidOverlap->mCurrent + timeline->mVidOverlap->m_StartOffset.first > buffer_end_first + frame_delay_first)
                    {
                        ImGui::sleep((int)5);
                        break;
                    }
                    int64_t buffer_start_second = timeline->mVidOverlap->mFrame.begin()->first.second.time_stamp * 1000;
                    int64_t buffer_end_second = buffer_start_second;
                    frameStepTime(buffer_end_second, timeline->mVidOverlap->bForward ? timeline->mMaxCachedVideoFrame : -timeline->mMaxCachedVideoFrame, timeline->mVidOverlap->mClipSecondFrameRate);
                    if (buffer_start_second > buffer_end_second)
                        std::swap(buffer_start_second, buffer_end_second);
                    if (timeline->mVidOverlap->mCurrent + timeline->mVidOverlap->m_StartOffset.second < buffer_start_second - frame_delay_second ||
                        timeline->mVidOverlap->mCurrent + timeline->mVidOverlap->m_StartOffset.second > buffer_end_second + frame_delay_second)
                    {
                        ImGui::sleep((int)5);
                        break;
                    }
                }
                std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat> result;
                if ((timeline->mVidOverlap->mMediaReader.first->IsDirectionForward() && !timeline->mVidOverlap->bForward) ||
                    (!timeline->mVidOverlap->mMediaReader.first->IsDirectionForward() && timeline->mVidOverlap->bForward))
                    timeline->mVidOverlap->mMediaReader.first->SetDirection(timeline->mVidOverlap->bForward);
                if ((timeline->mVidOverlap->mMediaReader.second->IsDirectionForward() && !timeline->mVidOverlap->bForward) ||
                    (!timeline->mVidOverlap->mMediaReader.second->IsDirectionForward() && timeline->mVidOverlap->bForward))
                    timeline->mVidOverlap->mMediaReader.second->SetDirection(timeline->mVidOverlap->bForward);
                if (timeline->mVidOverlap->mMediaReader.first->ReadVideoFrame((float)current_time_first / 1000.0, result.first.first, eof_first) &&
                    timeline->mVidOverlap->mMediaReader.second->ReadVideoFrame((float)current_time_second / 1000.0, result.first.second, eof_second))
                {
                    result.first.first.time_stamp = (double)current_time_first / 1000.f;
                    result.first.second.time_stamp = (double)current_time_second / 1000.f;
                    current_time = current_time_first - timeline->mVidOverlap->m_StartOffset.first;
                    timeline->mVideoFusionBluePrintLock.lock();
                    // setup bp input curve
                    for (int i = 0; i < editing_overlap->mFusionKeyPoints.GetCurveCount(); i++)
                    {
                        auto name = editing_overlap->mFusionKeyPoints.GetCurveName(i);
                        auto value = editing_overlap->mFusionKeyPoints.GetValue(i, current_time);
                        timeline->mVideoFusionBluePrint->Blueprint_SetFusion(name, value);
                    }
                    if (timeline->mVideoFusionBluePrint->Blueprint_RunFusion(result.first.first, result.first.second, result.second, current_time, timeline->mVidOverlap->mDuration))
                    {
                        timeline->mVidOverlap->mFrameLock.lock();
                        timeline->mVidOverlap->mFrame.push_back(result);
                        timeline->mVidOverlap->mFrameLock.unlock();
                        if (timeline->mVidOverlap->bForward)
                        {
                            frameStepTime(current_time_first, 1, timeline->mVidOverlap->mClipFirstFrameRate);
                            if (current_time_first > timeline->mVidOverlap->mClip1->mEnd - timeline->mVidOverlap->mClip1->mStart + timeline->mVidOverlap->mClip1->mStartOffset)
                                current_time_first = timeline->mVidOverlap->mClip1->mEnd - timeline->mVidOverlap->mClip1->mStart + timeline->mVidOverlap->mClip1->mStartOffset;
                            frameStepTime(current_time_second, 1, timeline->mVidOverlap->mClipSecondFrameRate);
                            if (current_time_second > timeline->mVidOverlap->mClip2->mEnd - timeline->mVidOverlap->mClip2->mStart + timeline->mVidOverlap->mClip2->mStartOffset)
                                current_time_second = timeline->mVidOverlap->mClip2->mEnd - timeline->mVidOverlap->mClip2->mStart + timeline->mVidOverlap->mClip2->mStartOffset;
                        }
                        else
                        {
                            frameStepTime(current_time_first, -1, timeline->mVidOverlap->mClipFirstFrameRate);
                            if (current_time_first < timeline->mVidOverlap->mClip1->mStartOffset)
                                current_time_first = timeline->mVidOverlap->mClip1->mStartOffset;
                            frameStepTime(current_time_second, -1, timeline->mVidOverlap->mClipSecondFrameRate);
                            if (current_time_second < timeline->mVidOverlap->mClip2->mStartOffset)
                                current_time_second = timeline->mVidOverlap->mClip2->mStartOffset;
                        }
                        
                    }
                    timeline->mVideoFusionBluePrintLock.unlock();
                }
            }
        }
        timeline->mVidFusionLock.unlock();
    }
    timeline->mVideoFusionRunning = false;
    return 0;
}

TimeLine::TimeLine()
    : mStart(0), mEnd(0), mPcmStream(this)
{
    std::srand(std::time(0)); // init std::rand

    mAudioRender = CreateAudioRender();
    if (mAudioRender)
    {
        mAudioRender->OpenDevice(mAudioSampleRate, mAudioChannels, mAudioFormat, &mPcmStream);
    }

    mVideoFilterBluePrint = new BluePrint::BluePrintUI();
    if (mVideoFilterBluePrint)
    {
        BluePrint::BluePrintCallbackFunctions callbacks;
        callbacks.BluePrintOnChanged = OnBluePrintChange;
        mVideoFilterBluePrint->Initialize();
        mVideoFilterBluePrint->SetCallbacks(callbacks, this);
    }

    mAudioFilterBluePrint = new BluePrint::BluePrintUI();
    if (mAudioFilterBluePrint)
    {
        BluePrint::BluePrintCallbackFunctions callbacks;
        callbacks.BluePrintOnChanged = OnBluePrintChange;
        mAudioFilterBluePrint->Initialize();
        mAudioFilterBluePrint->SetCallbacks(callbacks, this);
    }

    mVideoFusionBluePrint = new BluePrint::BluePrintUI();
    if (mVideoFusionBluePrint)
    {
        BluePrint::BluePrintCallbackFunctions callbacks;
        callbacks.BluePrintOnChanged = OnBluePrintChange;
        mVideoFusionBluePrint->Initialize();
        mVideoFusionBluePrint->SetCallbacks(callbacks, this);
    }

    mAudioFusionBluePrint = new BluePrint::BluePrintUI();
    if (mAudioFusionBluePrint)
    {
        BluePrint::BluePrintCallbackFunctions callbacks;
        callbacks.BluePrintOnChanged = OnBluePrintChange;
        mAudioFusionBluePrint->Initialize();
        mAudioFusionBluePrint->SetCallbacks(callbacks, this);
    }

    ConfigureDataLayer();

    m_audio_channel_data.clear();
    m_audio_channel_data.resize(mAudioChannels);

    mVideoFilterThread = new std::thread(thread_video_filter, this);
    mVideoFusionThread = new std::thread(thread_video_fusion, this);
}

TimeLine::~TimeLine()
{

    if (mVideoFilterThread && mVideoFilterThread->joinable())
    {
        mVideoFilterDone = true;
        mVideoFilterThread->join();
        delete mVideoFilterThread;
        mVideoFilterThread = nullptr;
        mVideoFilterDone = false;
    }
    if (mVideoFusionThread && mVideoFusionThread->joinable())
    {
        mVideoFusionDone = true;
        mVideoFusionThread->join();
        delete mVideoFusionThread;
        mVideoFusionThread = nullptr;
        mVideoFusionDone = false;
    }

    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();

    if (mMainPreviewTexture) { ImGui::ImDestroyTexture(mMainPreviewTexture); mMainPreviewTexture = nullptr; }
    
    m_audio_channel_data.clear();

    if (m_audio_vector_texture) { ImGui::ImDestroyTexture(m_audio_vector_texture); m_audio_vector_texture = nullptr; }
    
    if (mVideoFilterBluePrint)
    {
        mVideoFilterBluePrint->Finalize();
        delete mVideoFilterBluePrint;
    }
    if (mAudioFilterBluePrint)
    {
        mAudioFilterBluePrint->Finalize();
        delete mAudioFilterBluePrint;
    }
    if (mVideoFusionBluePrint)
    {
        mVideoFusionBluePrint->Finalize();
        delete mVideoFusionBluePrint;
    }
    if (mAudioFusionBluePrint)
    {
        mAudioFusionBluePrint->Finalize();
        delete mAudioFusionBluePrint;
    }
    for (auto track : m_Tracks) delete track;
    for (auto clip : m_Clips) delete clip;
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
        ReleaseAudioRender(&mAudioRender);
        mAudioRender = nullptr;
    }

    if (mMtvReader)
    {
        ReleaseMultiTrackVideoReader(&mMtvReader);
        mMtvReader = nullptr;
    }
    if (mMtaReader)
    {
        ReleaseMultiTrackAudioReader(&mMtaReader);
        mMtaReader = nullptr;
    }
    if (mEncoder)
    {
        ReleaseMediaEncoder(&mEncoder);
        mEncoder = nullptr;
    }
    if (mEncodingThread.joinable())
    {
        StopEncoding();
    }
}

void TimeLine::AlignTime(int64_t& time)
{
    alignTime(time, mFrameRate);
}

void TimeLine::Updata()
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

int64_t TimeLine::DeleteTrack(int index)
{
    int64_t trackId = -1;
    if (index >= 0 && index < m_Tracks.size())
    {
        auto track = m_Tracks[index];
        trackId = track->mID;
        m_Tracks.erase(m_Tracks.begin() + index);
        delete track;
        if (m_Tracks.size() == 0)
        {
            mStart = mEnd = 0;
            currentTime = firstTime = lastTime = visibleTime = 0;
        }
    }
    return trackId;
}

int TimeLine::NewTrack(const std::string& name, MEDIA_TYPE type, bool expand)
{
    auto new_track = new MediaTrack(name, type, this);
    new_track->mPixPerMs = msPixelWidthTarget;
    new_track->mViewWndDur = visibleTime;
    new_track->mExpanded = expand;
    m_Tracks.push_back(new_track);
    Updata();

    imgui_json::value action;
    action["action"] = "ADD_TRACK";
    action["media_type"] = imgui_json::number(type);
    action["track_id"] = imgui_json::number(new_track->mID);
    mUiActions.push_back(std::move(action));
    return m_Tracks.size() - 1;
}

void TimeLine::MovingTrack(int& index, int& dst_index)
{
    auto iter = m_Tracks.begin() + index;
    auto iter_dst = dst_index == -2 ? m_Tracks.end() - 1 : m_Tracks.begin() + dst_index;
    if (dst_index == -2 && iter == m_Tracks.end() - 1)
    {
        return;
    }

    // sync to datalayer
    if ((*iter)->mType == (*iter_dst)->mType)
    {
        if ((*iter)->mType == MEDIA_VIDEO)
        {
            if (dst_index > index)
                mMtvReader->ChangeTrackViewOrder((*iter_dst)->mID, (*iter)->mID);
            else
                mMtvReader->ChangeTrackViewOrder((*iter)->mID, (*iter_dst)->mID);
            mMtvReader->Refresh();
            mIsPreviewNeedUpdate = true;
        }
        // do we need change other type of media?
    }
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
        if (clip->mType == MEDIA_TEXT)
        {
            TextClip * tclip = dynamic_cast<TextClip *>(clip);
            // need remove from source track holder
            if (track->mMttReader && tclip->mClipHolder)
            {
                track->mMttReader->DeleteClip(tclip->mClipHolder);
            }
            // and add into dst track holder
            tclip->CreateClipHold(dst_track);
            tclip->mMediaID = dst_track->mID;
            tclip->mName = dst_track->mName;
        }

        dst_track->InsertClip(clip, clip->mStart);
        mOngoingAction["to_track_id"] = imgui_json::number(dst_track->mID);
    }
}

void TimeLine::DeleteClip(int64_t id)
{
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
        DeleteClipFromGroup(clip, clip->mGroupID);
        delete clip;
    }
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

ImGui::ImMat TimeLine::GetPreviewFrame()
{
    int64_t auddataPos, previewPos;
    if (mPcmStream.GetTimestampMs(auddataPos))
    {
        int64_t bufferedDur = mMtaReader->SizeToDuration(mAudioRender->GetBufferedDataSize());
        previewPos = mIsPreviewForward ? auddataPos-bufferedDur : auddataPos+bufferedDur;
    }
    else
    {
        int64_t elapsedTime = (int64_t)(std::chrono::duration_cast<std::chrono::duration<double>>((PlayerClock::now() - mPlayTriggerTp)).count()*1000);
        previewPos = mIsPreviewPlaying ? (mIsPreviewForward ? mPreviewResumePos+elapsedTime : mPreviewResumePos-elapsedTime) : mPreviewResumePos;
    }
    ImGui::ImMat frame;
    currentTime = previewPos;
    if (mIsPreviewPlaying)
    {
        bool playEof = false;
        int64_t dur = ValidDuration();
        if (!mIsPreviewForward && currentTime <= 0)
        {
            currentTime = 0;
            playEof = true;
        }
        else if (mIsPreviewForward && currentTime >= dur)
        {
            currentTime = dur;
            playEof = true;
        }
        if (playEof)
        {
            mIsPreviewPlaying = false;
            mPreviewResumePos = currentTime;
            if (mAudioRender)
                mAudioRender->Pause();
            for (auto& audio : m_audio_channel_data)
            {
                audio.m_decibel = 0;
            }
        }
    }
    mMtvReader->ReadVideoFrame(currentTime, frame, bSeeking);
    if (mIsPreviewPlaying) UpdateCurrent();
    return frame;
}

float TimeLine::GetAudioLevel(int channel)
{
    if (channel < m_audio_channel_data.size())
        return m_audio_channel_data[channel].m_decibel;
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
            for (auto& audio : m_audio_channel_data)
            {
                audio.m_decibel = 0;
            }
        }
    }
}

void TimeLine::Seek(int64_t msPos)
{
    mPlayTriggerTp = PlayerClock::now();
    mPreviewResumePos = msPos;
    if (mAudioRender)
    {
        if (mIsPreviewPlaying)
            mAudioRender->Pause();
        mAudioRender->Flush();
        mMtaReader->SeekTo(msPos);
        if (mIsPreviewPlaying)
            mAudioRender->Resume();
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
    return vdur > adur ? vdur : adur;
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

int TimeLine::GetTrackCount(MEDIA_TYPE type)
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

int64_t TimeLine::NewGroup(Clip * clip)
{
    if (!clip)
        return -1;
    DeleteClipFromGroup(clip, clip->mGroupID);
    ClipGroup new_group(this);
    new_group.m_Grouped_Clips.push_back(clip->mID);
    m_Groups.push_back(new_group);
    clip->mGroupID = new_group.mID;
    return clip->mGroupID;
}

void TimeLine::AddClipIntoGroup(Clip * clip, int64_t group_id)
{
    if (!clip || group_id == -1 || clip->mGroupID == group_id)
        return;
    // remove clip if clip is already in some group
    DeleteClipFromGroup(clip, clip->mGroupID);
    for (auto & group : m_Groups)
    {
        if (group_id == group.mID)
        {
            group.m_Grouped_Clips.push_back(clip->mID);
            clip->mGroupID = group_id;
        }
    }
}

void TimeLine::DeleteClipFromGroup(Clip *clip, int64_t group_id)
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
            }
        }
        if (need_erase)
        {
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

void TimeLine::CustomDraw(int index, ImDrawList *draw_list, const ImRect &view_rc, const ImRect &rc, const ImRect &titleRect, const ImRect &clippingTitleRect, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRec, bool is_moving, bool enable_select)
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
    auto need_seek = track->DrawTrackControlBar(draw_list, legendRect);
    //if (need_seek) Seek();
    draw_list->PopClipRect();
    
    // draw clips
    for (auto clip : track->m_Clips)
    {
        clip->SetViewWindowStart(firstTime);
        bool draw_clip = false;
        float cursor_start = 0;
        float cursor_end  = 0;
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
                draw_list->AddRectFilled(clip_title_pos_min, clip_title_pos_max, color, 4, ImDrawFlags_RoundCornersAll);
            }
            else
                draw_list->AddRectFilled(clip_title_pos_min, clip_title_pos_max, IM_COL32(64,128,64,128), 4, ImDrawFlags_RoundCornersAll);
            
            // draw clip status
            draw_list->PushClipRect(clip_title_pos_min, clip_title_pos_max, true);
            draw_list->AddText(clip_title_pos_min + ImVec2(4, 0), IM_COL32_WHITE, clip->mType == MEDIA_TEXT ? "T" : clip->mName.c_str());
            // add clip filter curve point
            for (int i = 0; i < clip->mFilterKeyPoints.GetCurveCount(); i++)
            {
                auto curve_color = clip->mFilterKeyPoints.GetCurveColor(i);
                for (int p = 0; p < clip->mFilterKeyPoints.GetCurvePointCount(i); p++)
                {
                    auto point = clip->mFilterKeyPoints.GetPoint(i, p);
                    if (point.point.x - clip->mStartOffset >= firstTime && point.point.x - clip->mStartOffset <= viewEndTime)
                    {
                        ImVec2 center = ImVec2(clip_title_pos_min.x + (point.point.x - firstTime - clip->mStartOffset) * msPixelWidthTarget, clip_title_pos_min.y + (clip_title_pos_max.y - clip_title_pos_min.y) / 2);
                        draw_list->AddCircle(center, 3, curve_color, 0, 2);
                    }
                }
            }
            // TODO::Dicky add clip attribute curve point
            draw_list->PopClipRect();

            // draw custom view
            if (track->mExpanded)
            {
                draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
                clip->DrawContent(draw_list, clip_pos_min, clip_pos_max, clippingRect);
                draw_list->PopClipRect();
            }

            if (clip->bSelected)
            {
                if (clip->bEditing)
                    draw_list->AddRect(clip_pos_min, clip_pos_max, IM_COL32(255,0,255,224), 4, ImDrawFlags_RoundCornersAll, 2.0f);
                else
                    draw_list->AddRect(clip_pos_min, clip_pos_max, IM_COL32(255,0,0,224), 4, ImDrawFlags_RoundCornersAll, 2.0f);
            }
            else if (clip->bEditing)
            {
                draw_list->AddRect(clip_pos_min, clip_pos_max, IM_COL32(0,0,255,224), 4, ImDrawFlags_RoundCornersAll, 2.0f);
            }

            // Clip select
            if (enable_select)
            {
                if (clip_rect.Contains(io.MousePos) )
                {
                    draw_list->AddRect(clip_rect.Min, clip_rect.Max, IM_COL32(255,255,255,255), 4, ImDrawFlags_RoundCornersAll, 2.0f);
                    const bool is_shift_key_only = (io.KeyMods == ImGuiKeyModFlags_Shift);
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
                        const bool is_ctrl_key_only = (io.KeyMods == ImGuiKeyModFlags_Ctrl);
                        bool b_attr_editing = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && is_ctrl_key_only;
                        track->SelectEditingClip(clip, !b_attr_editing);
                    }
                }
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
        if (val.is_number()) mAudioFormat = (AudioRender::PcmFormat)val.get<imgui_json::number>();
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
    Updata();
    //Seek();

    // load data layer
    ConfigureDataLayer();

    // load media clip
    const imgui_json::array* mediaClipArray = nullptr;
    if (imgui_json::GetPtrTo(value, "MediaClip", mediaClipArray))
    {
        for (auto& clip : *mediaClipArray)
        {
            MEDIA_TYPE type = MEDIA_UNKNOWN;
            if (clip.contains("Type"))
            {
                auto& val = clip["Type"];
                if (val.is_number()) type = (MEDIA_TYPE)val.get<imgui_json::number>();
            }
            Clip * media_clip = nullptr;
            switch (type)
            {
                case MEDIA_VIDEO: media_clip = VideoClip::Load(clip, this); break;
                case MEDIA_AUDIO: media_clip = AudioClip::Load(clip, this); break;
                case MEDIA_PICTURE: media_clip = ImageClip::Load(clip, this); break;
                case MEDIA_TEXT: media_clip = TextClip::Load(clip, this); break;
                default:
                break;
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

    // build data layer multi-track video reader
    for (auto track : m_Tracks)
    {
        if (track->mType == MEDIA_VIDEO)
        {
            DataLayer::VideoTrackHolder vidTrack = mMtvReader->AddTrack(track->mID);
            for (auto clip : track->m_Clips)
            {
                DataLayer::VideoClipHolder vidClip = vidTrack->AddNewClip(
                    clip->mID, clip->mMediaParser,
                    clip->mStart, clip->mStartOffset, clip->mEndOffset, currentTime-clip->mStart);

                BluePrintVideoFilter* bpvf = new BluePrintVideoFilter();
                bpvf->SetBluePrintFromJson(clip->mFilterBP);
                bpvf->SetKeyPoint(clip->mFilterKeyPoints);
                DataLayer::VideoFilterHolder hFilter(bpvf);
                vidClip->SetFilter(hFilter);
            }
        }
        else if (track->mType == MEDIA_AUDIO)
        {
            DataLayer::AudioTrackHolder audTrack = mMtaReader->AddTrack(track->mID);
            for (auto clip : track->m_Clips)
            {
                if (!clip->mMediaParser)
                    continue;
                DataLayer::AudioClipHolder audClip = audTrack->AddNewClip(
                    clip->mID, clip->mMediaParser,
                    clip->mStart, clip->mStartOffset, clip->mEndOffset);
            }
        }
    }
    SyncDataLayer();
    mMtvReader->Refresh();
    mMtaReader->SeekTo(currentTime);
    mMtaReader->Refresh();
    Logger::Log(Logger::VERBOSE) << *mMtvReader << std::endl;
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

    // save global timeline info
    value["Start"] = imgui_json::number(mStart);
    value["End"] = imgui_json::number(mEnd);
    value["VideoWidth"] = imgui_json::number(mWidth);
    value["VideoHeight"] = imgui_json::number(mHeight);
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

void TimeLine::PerformUiActions()
{
    if (mUiActions.empty())
        return;

    if (!mUiActions.empty())
    {
        Logger::Log(Logger::VERBOSE) << std::endl << "UiActions : [" << std::endl;
    }
    for (auto& action : mUiActions)
    {
        Logger::Log(Logger::VERBOSE) << "\t" << action.dump() << std::endl;
        MEDIA_TYPE mediaType = MEDIA_UNKNOWN;
        if (action.contains("media_type"))
            mediaType = (MEDIA_TYPE)action["media_type"].get<imgui_json::number>();
        if (mediaType == MEDIA_VIDEO)
            PerformVideoAction(action);
        else if (mediaType == MEDIA_AUDIO)
            PerformAudioAction(action);
        else
        {
            Logger::Log(Logger::DEBUG) << "Skip action due to unsupported MEDIA_TYPE: " << action.dump() << "." << std::endl;
            continue;
        }
    }
    if (!mUiActions.empty())
    {
        Logger::Log(Logger::VERBOSE) << "] #UiActions" << std::endl << std::endl;
        SyncDataLayer();
        Logger::Log(Logger::VERBOSE) << *mMtvReader << std::endl << std::endl;
    }

    mUiActions.clear();
}

void TimeLine::PerformVideoAction(imgui_json::value& action)
{
    std::string actionName = action["action"].get<imgui_json::string>();
    if (actionName == "ADD_CLIP")
    {
        int64_t trackId = action["to_track_id"].get<imgui_json::number>();
        DataLayer::VideoTrackHolder vidTrack = mMtvReader->GetTrackById(trackId, true);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        Clip* clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
        DataLayer::VideoClipHolder vidClip(new DataLayer::VideoClip(
            clip->mID, clip->mMediaParser,
            vidTrack->OutWidth(), vidTrack->OutHeight(), vidTrack->FrameRate(),
            clip->mStart, clip->mStartOffset, clip->mEndOffset, currentTime-clip->mStart));
        vidTrack->InsertClip(vidClip);
        mMtvReader->Refresh();
    }
    else if (actionName == "MOVE_CLIP")
    {
        int64_t srcTrackId = action["from_track_id"].get<imgui_json::number>();
        int64_t dstTrackId = srcTrackId;
        if (action.contains("to_track_id"))
            dstTrackId = action["to_track_id"].get<imgui_json::number>();
        DataLayer::VideoTrackHolder dstVidTrack = mMtvReader->GetTrackById(dstTrackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        Clip* clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
        if (srcTrackId != dstTrackId)
        {
            DataLayer::VideoTrackHolder srcVidTrack = mMtvReader->GetTrackById(srcTrackId);
            DataLayer::VideoClipHolder vidClip = srcVidTrack->RemoveClipById(clip->mID);
            vidClip->SetStart(clip->mStart);
            dstVidTrack->InsertClip(vidClip);
        }
        else
        {
            dstVidTrack->MoveClip(clip->mID, clip->mStart);
        }
        mMtvReader->Refresh();
    }
    else if (actionName == "CROP_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        DataLayer::VideoTrackHolder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        Clip* clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
        vidTrack->ChangeClipRange(clip->mID, clip->mStartOffset, clip->mEndOffset);
        mMtvReader->Refresh();
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        DataLayer::VideoTrackHolder vidTrack = mMtvReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        vidTrack->RemoveClipById(clipId);
        mMtvReader->Refresh();
    }
    else if (actionName == "ADD_TRACK")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        mMtvReader->AddTrack(trackId);
    }
    else if (actionName == "REMOVE_TRACK")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        mMtvReader->RemoveTrackById(trackId);
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
        DataLayer::AudioTrackHolder audTrack = mMtaReader->GetTrackById(trackId, true);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        Clip* clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
        DataLayer::AudioClipHolder audClip(new DataLayer::AudioClip(
            clip->mID, clip->mMediaParser,
            audTrack->OutChannels(), audTrack->OutSampleRate(),
            clip->mStart, clip->mStartOffset, clip->mEndOffset, currentTime-clip->mStart));
        audTrack->InsertClip(audClip);
        mMtaReader->Refresh();
    }
    else if (actionName == "MOVE_CLIP")
    {
        int64_t srcTrackId = action["from_track_id"].get<imgui_json::number>();
        int64_t dstTrackId = srcTrackId;
        if (action.contains("to_track_id"))
            dstTrackId = action["to_track_id"].get<imgui_json::number>();
        DataLayer::AudioTrackHolder dstAudTrack = mMtaReader->GetTrackById(dstTrackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        Clip* clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
        if (srcTrackId != dstTrackId)
        {
            DataLayer::AudioTrackHolder srcAudTrack = mMtaReader->GetTrackById(srcTrackId);
            DataLayer::AudioClipHolder audClip = srcAudTrack->RemoveClipById(clip->mID);
            audClip->SetStart(clip->mStart);
            dstAudTrack->InsertClip(audClip);
        }
        else
        {
            dstAudTrack->MoveClip(clip->mID, clip->mStart);
        }
        mMtaReader->Refresh();
    }
    else if (actionName == "CROP_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        DataLayer::AudioTrackHolder audTrack = mMtaReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        Clip* clip = FindClipByID(action["clip_id"].get<imgui_json::number>());
        audTrack->ChangeClipRange(clip->mID, clip->mStartOffset, clip->mEndOffset);
        mMtaReader->Refresh();
    }
    else if (actionName == "REMOVE_CLIP")
    {
        int64_t trackId = action["from_track_id"].get<imgui_json::number>();
        DataLayer::AudioTrackHolder audTrack = mMtaReader->GetTrackById(trackId);
        int64_t clipId = action["clip_id"].get<imgui_json::number>();
        audTrack->RemoveClipById(clipId);
        mMtaReader->Refresh();
    }
    else if (actionName == "ADD_TRACK")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        mMtaReader->AddTrack(trackId);
    }
    else if (actionName == "REMOVE_TRACK")
    {
        int64_t trackId = action["track_id"].get<imgui_json::number>();
        mMtaReader->RemoveTrackById(trackId);
    }
    else
    {
        Logger::Log(Logger::WARN) << "UNHANDLED UI ACTION(Video): '" << actionName << "'." << std::endl;
    }
}

void TimeLine::ConfigureDataLayer()
{
    if (mMtvReader)
        ReleaseMultiTrackVideoReader(&mMtvReader);
    mMtvReader = CreateMultiTrackVideoReader();
    mMtvReader->Configure(mWidth, mHeight, mFrameRate);
    mMtvReader->Start();
    if (mMtaReader)
        ReleaseMultiTrackAudioReader(&mMtaReader);
    mMtaReader = CreateMultiTrackAudioReader();
    mMtaReader->Configure(mAudioChannels, mAudioSampleRate);
    mMtaReader->Start();
    mPcmStream.SetAudioReader(mMtaReader);
}

void TimeLine::SyncDataLayer()
{
    // video
    int syncedOverlapCount = 0;
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
                    vidOvlp->SetId(ovlp->mID);
                    BluePrintVideoTransition* bpvt = new BluePrintVideoTransition();
                    bpvt->SetBluePrintFromJson(ovlp->mFusionBP);
                    bpvt->SetKeyPoint(ovlp->mFusionKeyPoints);
                    DataLayer::VideoTransitionHolder hTrans(bpvt);
                    vidOvlp->SetTransition(hTrans);
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
    int vidOvlpCnt = 0;
    for (auto ovlp : m_Overlaps)
    {
        if (ovlp->mType == MEDIA_VIDEO)
            vidOvlpCnt++;
    }
    if (syncedOverlapCount != vidOvlpCnt)
        Logger::Log(Logger::Error) << "Overlap SYNC FAILED! Synced count is " << syncedOverlapCount
            << ", while the count of video overlap array is " << vidOvlpCnt << "." << std::endl;
    // TODO: sync audio and other types of data with data-layer
}

SnapshotGeneratorHolder TimeLine::GetSnapshotGenerator(int64_t mediaItemId)
{
    auto iter = m_VidSsGenTable.find(mediaItemId);
    if (iter != m_VidSsGenTable.end())
        return iter->second;
    MediaItem* mi = FindMediaItemByID(mediaItemId);
    if (!mi)
        return nullptr;
    if (mi->mMediaType != MEDIA_VIDEO)
        return nullptr;
    SnapshotGeneratorHolder hSsGen = CreateSnapshotGenerator();
    hSsGen->EnableHwAccel(mHardwareCodec);
    if (!hSsGen->Open(mi->mMediaOverview->GetMediaParser()))
    {
        Logger::Log(Logger::Error) << hSsGen->GetError() << std::endl;
        return nullptr;
    }
    auto video_info = mi->mMediaOverview->GetMediaParser()->GetBestVideoStream();
    float snapshot_scale = video_info->height > 0 ? DEFAULT_VIDEO_TRACK_HEIGHT / (float)video_info->height : 0.1;
    hSsGen->SetSnapshotResizeFactor(snapshot_scale, snapshot_scale);
    hSsGen->SetCacheFactor(9);
    if (visibleTime > 0 && msPixelWidthTarget > 0)
    {
        const MediaInfo::VideoStream* video_stream = hSsGen->GetVideoStream();
        float snapHeight = DEFAULT_VIDEO_TRACK_HEIGHT;  // TODO: video clip UI height is hard coded here, should be fixed later (wyvern)
        MediaInfo::Ratio displayAspectRatio = {
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
        const MediaInfo::VideoStream* video_stream = ssGen->GetVideoStream();
        float snapHeight = DEFAULT_VIDEO_TRACK_HEIGHT;  // TODO: video clip UI height is hard coded here, should be fixed later (wyvern)
        MediaInfo::Ratio displayAspectRatio = {
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
            ImGui::ImMat amat;
            bool eof;
            if (!m_areader->ReadAudioSamples(amat, eof))
                return 0;
            m_amat = amat;
            m_owner->CalculateAudioScopeData(m_amat);
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
    mat.create_type(mat_in.w, 1, mat_in.c, mat_in.type);
    float * data = (float *)mat_in.data;
    for (int x = 0; x < mat.w; x++)
    {
        for (int i = 0; i < mat.c; i++)
        {
            mat.at<float>(x, 0, i) = data[x * mat.c + i];
        }
    }
    for (int i = 0; i < mat.c; i++)
    {
        if (i < mAudioChannels)
        {
            m_audio_channel_data[i].m_wave.clone_from(mat.channel(i));
            m_audio_channel_data[i].m_fft.clone_from(mat.channel(i));
            ImGui::ImRFFT((float *)m_audio_channel_data[i].m_fft.data, m_audio_channel_data[i].m_fft.w, true);
            m_audio_channel_data[i].m_db.create_type((mat.w >> 1) + 1, IM_DT_FLOAT32);
            m_audio_channel_data[i].m_DBMaxIndex = ImGui::ImReComposeDB((float*)m_audio_channel_data[i].m_fft.data, (float *)m_audio_channel_data[i].m_db.data, mat.w, false);
            m_audio_channel_data[i].m_DBShort.create_type(20, IM_DT_FLOAT32);
            ImGui::ImReComposeDBShort((float*)m_audio_channel_data[i].m_fft.data, (float*)m_audio_channel_data[i].m_DBShort.data, mat.w);
            m_audio_channel_data[i].m_DBLong.create_type(76, IM_DT_FLOAT32);
            ImGui::ImReComposeDBLong((float*)m_audio_channel_data[i].m_fft.data, (float*)m_audio_channel_data[i].m_DBLong.data, mat.w);
            m_audio_channel_data[i].m_decibel = ImGui::ImDoDecibel((float*)m_audio_channel_data[i].m_fft.data, mat.w);
            if (m_audio_channel_data[i].m_Spectrogram.w != (mat.w >> 1) + 1)
            {
                m_audio_channel_data[i].m_Spectrogram.create_type((mat.w >> 1) + 1, 256, 4, IM_DT_INT8);
            }
            if (!m_audio_channel_data[i].m_Spectrogram.empty())
            {
                auto w = m_audio_channel_data[i].m_Spectrogram.w;
                auto c = m_audio_channel_data[i].m_Spectrogram.c;
                memmove(m_audio_channel_data[i].m_Spectrogram.data, (char *)m_audio_channel_data[i].m_Spectrogram.data + w * c, m_audio_channel_data[i].m_Spectrogram.total() - w * c);
                uint32_t * last_line = (uint32_t *)m_audio_channel_data[i].m_Spectrogram.row_c<uint8_t>(255);
                for (int n = 0; n < w; n++)
                {
                    float value = m_audio_channel_data[i].m_db.at<float>(n) * M_SQRT2 + 64 + mAudioSpectrogramOffset;
                    value = ImClamp(value, -64.f, 63.f);
                    float light = (value + 64) / 127.f;
                    value = (int)((value + 64) + 170) % 255; 
                    auto hue = value / 255.f;
                    auto color = ImColor::HSV(hue, 1.0, light * mAudioSpectrogramLight);
                    last_line[n] = color;
                }
            }
        }
    }
    if (mat.c >= 2)
    {
        if (m_audio_vector.empty())
        {
            m_audio_vector.create_type(256, 256, 4, IM_DT_INT8);
            m_audio_vector.fill((int8_t)0);
            m_audio_vector.elempack = 4;
        }
        if (!m_audio_vector.empty())
        {
            float zoom = mAudioVectorScale;
            float hw = m_audio_vector.w / 2;
            float hh = m_audio_vector.h / 2;
            int samples = mat_in.w;
            m_audio_vector *= 0.99;
            for (int n = 0; n < samples; n++)
            {
                float s1 = m_audio_channel_data[0].m_wave.at<float>(n, 0);
                float s2 = m_audio_channel_data[1].m_wave.at<float>(n, 0);
                int x = hw;
                int y = hh;

                if (mAudioVectorMode == LISSAJOUS)
                {
                    x = ((s2 - s1) * zoom / 2 + 1) * hw;
                    y = (1.0 - (s1 + s2) * zoom / 2) * hh;
                }
                else if (mAudioVectorMode == LISSAJOUS_XY)
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
                    y = m_audio_vector.h - m_audio_vector.h * fabsf(cx + cy) * .7;
                }
                x = ImClamp(x, 0, m_audio_vector.w - 1);
                y = ImClamp(y, 0, m_audio_vector.h - 1);
                uint8_t r = ImClamp(m_audio_vector.at<uint8_t>(x, y, 0) + 30, 0, 255);
                uint8_t g = ImClamp(m_audio_vector.at<uint8_t>(x, y, 1) + 50, 0, 255);
                uint8_t b = ImClamp(m_audio_vector.at<uint8_t>(x, y, 2) + 30, 0, 255);
                m_audio_vector.draw_dot(x, y, ImPixel(r / 255.0, g / 255.0, b / 255.0, 1.f));
            }
        }
    }
}

bool TimeLine::ConfigEncoder(const std::string& outputPath, VideoEncoderParams& vidEncParams, AudioEncoderParams& audEncParams, std::string& errMsg)
{
    if (mEncoder)
    {
        ReleaseMediaEncoder(&mEncoder);
        mEncoder = nullptr;
    }
    mEncoder = CreateMediaEncoder();
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
    if (mEncMtvReader)
    {
        ReleaseMultiTrackVideoReader(&mEncMtvReader);
        mEncMtvReader = nullptr;
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
    if (mEncMtaReader)
    {
        ReleaseMultiTrackAudioReader(&mEncMtaReader);
        mEncMtaReader = nullptr;
    }
    mEncMtaReader = mMtaReader->CloneAndConfigure(audEncParams.channels, audEncParams.sampleRate, audEncParams.samplesPerFrame);
    return true;
}

void TimeLine::StartEncoding()
{
    if (mEncodingThread.joinable())
        return;
    mEncodeProcErrMsg.clear();
    mEncodingProgress = 0;
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

    if (mEncMtvReader)
    {
        ReleaseMultiTrackVideoReader(&mEncMtvReader);
        mEncMtvReader = nullptr;
    }
    if (mEncMtaReader)
    {
        ReleaseMultiTrackAudioReader(&mEncMtaReader);
        mEncMtaReader = nullptr;
    }
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
    MediaInfo::Ratio outFrameRate = mEncoder->GetVideoFrameRate();
    ImGui::ImMat vmat, amat;
    uint32_t pcmbufSize = 8192;
    uint8_t* pcmbuf = new uint8_t[pcmbufSize];
    double viddur = (double)mEncMtvReader->Duration()/1000;
    double dur = (double)ValidDuration()/1000;
    double encpos = 0;
    while (!mQuitEncoding && (!vidInputEof || !audInputEof))
    {
        if ((!vidInputEof && vidpos <= audpos) || audInputEof)
        {
            vidpos = (double)vidFrameCount*outFrameRate.den/outFrameRate.num;
            vidFrameCount++;
            bool eof = vidpos >= viddur;
            if (!eof)
            {
                if (!mEncMtvReader->ReadVideoFrame((int64_t)(vidpos*1000), vmat))
                {
                    std::ostringstream oss;
                    oss << "[video] '" << mEncMtvReader->GetError() << "'.";
                    mEncodeProcErrMsg = oss.str();
                    break;
                }
                vmat.time_stamp = vidpos;
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
                    mEncodingProgress = encpos/dur;
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
            audpos = amat.time_stamp;
            if (!eof)
            {
                if (!mEncoder->EncodeAudioSamples(amat))
                {
                    std::ostringstream oss;
                    oss << "[audio] '" << mEncoder->GetError() << "'.";
                    mEncodeProcErrMsg = oss.str();
                    break;
                }
                if (amat.time_stamp > encpos)
                {
                    encpos = amat.time_stamp;
                    mEncodingProgress = encpos/dur;
                }
            }
            else
            {
                amat.release();
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
} // namespace MediaTimeline/TimeLine

namespace MediaTimeline
{
std::string TimelineMillisecToString(int64_t millisec, int show_millisec)
{
    std::ostringstream oss;
    if (millisec < 0)
    {
        oss << "-";
        millisec = -millisec;
    }
    uint64_t t = (uint64_t) millisec;
    uint32_t milli = (uint32_t)(t%1000); t /= 1000;
    uint32_t sec = (uint32_t)(t%60); t /= 60;
    uint32_t min = (uint32_t)(t%60); t /= 60;
    uint32_t hour = (uint32_t)t;
    if (hour > 0)
    {
        oss << std::setfill('0') << std::setw(2) << hour << ':'
            << std::setw(2) << min << ':'
            << std::setw(2) << sec;
        if (show_millisec == 3)
            oss << '.' << std::setw(3) << milli;
        else if (show_millisec == 2)
            oss << '.' << std::setw(2) << milli / 10;
        else if (show_millisec == 1)
            oss << '.' << std::setw(1) << milli / 100;
    }
    else
    {
        oss << std::setfill('0') << std::setw(2) << min << ':'
            << std::setw(2) << sec;
        if (show_millisec == 3)
            oss << '.' << std::setw(3) << milli;
        else if (show_millisec == 2)
            oss << '.' << std::setw(2) << milli / 10;
        else if (show_millisec == 1)
            oss << '.' << std::setw(1) << milli / 100;
    }
    return oss.str();
}
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
    const bool is_alt_key_only = (io.KeyMods == ImGuiKeyModFlags_Alt);
    bCutting = ImGui::IsKeyDown(ImGuiKey_LeftAlt) && is_alt_key_only;
    bool overTrackView = false;
    bool overHorizonScrollBar = false;
    bool overCustomDraw = false;
    bool overLegend = false;
    bool overTopBar = false;

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
    float maxPixelWidthTarget = frame_duration > 0.0 ? 60.f / frame_duration : 20.f;
    timeline->msPixelWidthTarget = ImClamp(timeline->msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);

    if (timeline->visibleTime >= duration)
        timeline->firstTime = timeline->GetStart();
    timeline->lastTime = timeline->firstTime + timeline->visibleTime;
    
    int controlHeight = trackCount * trackHeadHeight;
    for (int i = 0; i < trackCount; i++)
        controlHeight += int(timeline->GetCustomHeight(i));

    // draw backbround
    //draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DARK_TWO);

    ImGui::BeginGroup();
    bool isFocused = ImGui::IsWindowFocused();

    ImGui::SetCursorScreenPos(canvas_pos);
    if ((expanded && !*expanded) || !trackCount)
    {
        // minimum view
        draw_list->AddRectFilled(canvas_pos + ImVec2(legendWidth, 0), ImVec2(timline_size.x + canvas_pos.x, canvas_pos.y + HeadHeight + 4), COL_DARK_ONE, 0);
        auto info_str = TimelineMillisecToString(duration, 3);
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
            int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int tiretEnd = baseIndex ? regionHeight : HeadHeight - 8;
            if (px <= (timline_size.x + contentMin.x) && px >= (contentMin.x + legendWidth))
            {
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
            }
            if (baseIndex && px > (contentMin.x + legendWidth))
            {
                auto time_str = TimelineMillisecToString(i, 2);
                ImGui::SetWindowFontScale(0.8);
                draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y + 8), COL_RULE_TEXT, time_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
        };
        auto drawLineContent = [&](int64_t i, int)
        {
            int px = (int)contentMin.x + int(i * timeline->msPixelWidthTarget) + legendWidth - int(timeline->firstTime * timeline->msPixelWidthTarget);
            int tiretStart = int(contentMin.y);
            int tiretEnd = int(contentMax.y);
            if (px <= (timline_size.x + contentMin.x) && px >= (contentMin.x + legendWidth))
            {
                draw_list->AddLine(ImVec2(float(px), float(tiretStart)), ImVec2(float(px), float(tiretEnd)), COL_SLOT_V_LINE, 1);
            }
        };
        for (auto i = timeline->GetStart(); i <= timeline->GetEnd(); i += timeStep)
        {
            drawLine(i, HeadHeight);
        }
        drawLine(timeline->GetStart(), HeadHeight);
        drawLine(timeline->GetEnd(), HeadHeight);

        // cursor Arrow
        if (timeline->currentTime >= timeline->firstTime && timeline->currentTime <= timeline->GetEnd())
        {
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + legendWidth + (timeline->currentTime - timeline->firstTime) * timeline->msPixelWidthTarget - arrowWidth * 0.5f + 1;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = TimelineMillisecToString(timeline->currentTime, 2);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + legendWidth + (timeline->currentTime - timeline->firstTime) * timeline->msPixelWidthTarget - str_size.x * 0.5f + 1;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }
        // crop content
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize);

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

        // cropping rect so track bars are not visible in the legend on the left when scrolled
        draw_list->PushClipRect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        // vertical time lines in content area
        for (auto i = timeline->GetStart(); i <= timeline->GetEnd(); i += timeStep)
        {
            drawLineContent(i, int(contentHeight));
        }
        drawLineContent(timeline->GetStart(), int(contentHeight));
        drawLineContent(timeline->GetEnd(), int(contentHeight));

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
                        ImRect rects[3] = {ImRect(clipP1, ImVec2(clipP1.x + handle_width, clipP2.y)), ImRect(ImVec2(clipP2.x - handle_width, clipP1.y), clipP2), ImRect(clipP1, clipP3)};

                        for (int j = 1; j >= 0; j--)
                        {
                            ImRect &rc = rects[j];
                            if (!rc.Contains(io.MousePos))
                                continue;
                            if (j == 0)
                                RenderMouseCursor(ICON_CROPPING_LEFT, ImVec2(4, 0));
                            else
                                RenderMouseCursor(ICON_CROPPING_RIGHT, ImVec2(12, 0));
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
                                // draw dotted line at mouse pos
                                ImVec2 P1(cx, canvas_pos.y + (float)HeadHeight + 8.f);
                                ImVec2 P2(cx, canvas_pos.y + (float)HeadHeight + float(controlHeight) + 8.f);
                                draw_list->AddLine(P1, P2, IM_COL32(0, 0, 255, 255), 2);
                                RenderMouseCursor(ICON_CUTTING, ImVec2(7, 0), 1.0, -90);
                            }
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingHorizonScrollBar && !MovingCurrentTime && !menuIsOpened && editable)
                            {
                                if (j == 2 && bCutting && count <= 1)
                                {
                                    clip->Cutting(mouseTime);
                                }
                                else
                                {
                                    clipMovingEntry = clip->mID;
                                    clipMovingPart = j + 1;
                                    timeline->mOngoingAction["action"] = "MOVE_CLIP";
                                    timeline->mOngoingAction["clip_id"] = imgui_json::number(clip->mID);
                                    timeline->mOngoingAction["media_type"] = imgui_json::number(clip->mType);
                                    timeline->mOngoingAction["from_track_id"] = imgui_json::number(track->mID);
                                    timeline->mOngoingAction["start"] = imgui_json::number(clip->mStart);
                                    if (j <= 1)
                                    {
                                        timeline->mOngoingAction["action"] = "CROP_CLIP";
                                        bCropping = true;
                                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                                    }
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
            //ImGui::BeginTooltip();
            //ImGui::Text("Draging track:%s", std::to_string(trackMovingEntry).c_str());
            //ImGui::Text("currrent track:%s", std::to_string(trackEntry).c_str());
            //ImGui::EndTooltip();
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
            else
            {
                timeline->mark_out = mouse_time;
                if (timeline->mark_out < 0) timeline->mark_out = 0;
                if (timeline->mark_out <= timeline->mark_in) timeline->mark_in = -1;
            }
            ImGui::BeginTooltip();
            ImGui::Text(" In:%s", TimelineMillisecToString(timeline->mark_in, 2).c_str());
            ImGui::Text("Out:%s", TimelineMillisecToString(timeline->mark_out, 2).c_str());
            ImGui::Text("Dur:%s", TimelineMillisecToString(timeline->mark_out - timeline->mark_in, 2).c_str());
            ImGui::EndTooltip();
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
        if (HeaderAreaRect.Contains(io.MousePos) && !menuIsOpened && !bCropping && !bCutting&& ImGui::IsMouseDown(ImGuiMouseButton_Right))
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
            
            if (ImGui::MenuItem("+" ICON_MEDIA_VIDEO  " Insert Empty Video Track", nullptr, nullptr))
            {
                insertEmptyTrackType = MEDIA_VIDEO;
            }
            if (ImGui::MenuItem("+" ICON_MEDIA_AUDIO " Insert Empty Audio Track", nullptr, nullptr))
            {
                insertEmptyTrackType = MEDIA_AUDIO;
            }
            if (ImGui::MenuItem( "+" ICON_MEDIA_IMAGE " Insert Empty Image Track", nullptr, nullptr))
            {
                insertEmptyTrackType = MEDIA_PICTURE;
            }
            if (ImGui::MenuItem( "+" ICON_MEDIA_TEXT " Insert Empty Text Track", nullptr, nullptr))
            {
                insertEmptyTrackType = MEDIA_TEXT;
            }

            if (empty_track_count > 0)
            {
                if (ImGui::MenuItem(" " ICON_MEDIA_DELETE " Delete Empty Track", nullptr, nullptr))
                {
                    removeEmptyTrack = true;
                }
            }

            if (clipMenuEntry != -1)
            {
                ImGui::Separator();
                auto clip = timeline->FindClipByID(clipMenuEntry);
                auto track = timeline->FindTrackByClipID(clip->mID);
                if (ImGui::MenuItem(ICON_CROP " Edit Clip Attribute", nullptr, nullptr))
                {
                    track->SelectEditingClip(clip, false);
                }
                if (ImGui::MenuItem(ICON_BLUE_PRINT " Edit Clip Filter", nullptr, nullptr))
                {
                    track->SelectEditingClip(clip, true);
                }
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
                if (track->mType == MEDIA_TEXT)
                {
                    ImGui::Separator();
                    if (ImGui::MenuItem(ICON_MEDIA_TEXT  " Add Text", nullptr, nullptr))
                    {
                        if (track->mMttReader && menuMouseTime != -1)
                        {
                            auto& style = track->mMttReader->DefaultStyle();
                            TextClip * clip = new TextClip(menuMouseTime, menuMouseTime + 5000, track->mID, track->mName, std::string(""), timeline);
                            auto holder = track->mMttReader->NewClip(clip->mStart, clip->mEnd - clip->mStart);
                            clip->SetClipDefault(style);
                            clip->mClipHolder = holder;
                            clip->mTrack = track;
                            holder->EnableUsingTrackStyle(clip->mTrackStyle);
                            timeline->m_Clips.push_back(clip);
                            track->InsertClip(clip, holder->StartTime());
                            track->SelectEditingClip(clip, false);
                            if (timeline->m_CallBacks.EditingClipFilter)
                            {
                                timeline->m_CallBacks.EditingClipFilter(clip->mType, clip);
                            }
                            //action["clip_id"] = imgui_json::number(clip->mID);
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
        if (regionRect.Contains(io.MousePos))
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
                    timeline->firstTime -= timeline->visibleTime / 16;
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                }
                else if (io.MouseWheelH > FLT_EPSILON)
                {
                    timeline->firstTime += timeline->visibleTime / 16;
                    timeline->firstTime = ImClamp(timeline->firstTime, timeline->GetStart(), ImMax(timeline->GetEnd() - timeline->visibleTime, timeline->GetStart()));
                }
                // up-down wheel over scrollbar, scale canvas view
                else if (io.MouseWheel < -FLT_EPSILON && timeline->visibleTime <= timeline->GetEnd())
                {
                    timeline->msPixelWidthTarget *= 0.9f;
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    timeline->msPixelWidthTarget *= 1.1f;
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
            timeline->CustomDraw(customDraw.index, draw_list, ImRect(childFramePos, childFramePos + childFrameSize), customDraw.customRect, customDraw.titleRect, customDraw.clippingTitleRect, customDraw.legendRect, customDraw.clippingRect, customDraw.legendClippingRect, bMoving, !menuIsOpened && !bCutting);
        draw_list->PopClipRect();

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
        if (movable && !MovingCurrentTime && !MovingHorizonScrollBar && clipMovingEntry == -1 && timeline->currentTime >= 0 && topRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = true;
            timeline->bSeeking = true;
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
            timeline->bSeeking = false;
        }
        ImGui::EndChildFrame();

        // cursor line
        ImRect custom_view_rect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
        if (trackCount > 0 && timeline->currentTime >= timeline->firstTime && timeline->currentTime <= timeline->GetEnd())
        {
            static const float cursorWidth = 2.f;
            float cursorOffset = contentMin.x + legendWidth + (timeline->currentTime - timeline->firstTime) * timeline->msPixelWidthTarget + 1;
            draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMin.y + trackRect.Max.y - scrollSize), IM_COL32(0, 255, 0, 224), cursorWidth);
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

        // draw mark range for timeline header bar and draw shadow out of mark range 
        ImGui::PushClipRect(HeaderAreaRect.Min, HeaderAreaRect.Min + ImVec2(trackAreaRect.GetWidth() + 8, contentMin.y + timline_size.y - scrollSize), false);
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
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
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
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
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
        }
        if (timeline->mark_in != -1 && timeline->mark_in < timeline->firstTime && timeline->mark_out >= timeline->lastTime)
        {
            draw_list->AddRectFilled(HeaderAreaRect.Min , HeaderAreaRect.Max - ImVec2(0, HeadHeight - 8), COL_MARK_BAR, 0);
        }

        if ((timeline->mark_in != -1 && timeline->mark_in >= timeline->lastTime) || (timeline->mark_out != -1 && timeline->mark_out <= timeline->firstTime))
        {
            // add shadow on whole timeline
            draw_list->AddRectFilled(HeaderAreaRect.Min, HeaderAreaRect.Min + ImVec2(timline_size.x + 8, timline_size.y - scrollSize), IM_COL32(0,0,0,128));
        }
        ImGui::PopClipRect();
    }
    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        auto& ongoingAction = timeline->mOngoingAction;
        if (clipMovingEntry != -1 && ongoingAction.contains("action"))
        {
            int64_t clipId = ongoingAction["clip_id"].get<imgui_json::number>();
            Clip* clip = timeline->FindClipByID(clipId);
            std::string actionName = ongoingAction["action"].get<imgui_json::string>();
            if (actionName == "MOVE_CLIP")
            {
                int64_t orgStart = ongoingAction["start"].get<imgui_json::number>();
                bool acrossTrack = ongoingAction.contains("to_track_id") ?
                    ongoingAction["to_track_id"].get<imgui_json::number>() != ongoingAction["from_track_id"].get<imgui_json::number>() : false;
                if (!acrossTrack && orgStart == clip->mStart)
                {
                    // clip is not actually moved, so discard this move action
                    imgui_json::value emptyJson;
                    ongoingAction.swap(emptyJson);
                    Logger::Log(Logger::VERBOSE) << "!!!!!!!! action DISCARDED !!!!!!!!!!" << std::endl;
                }
                else if (orgStart != clip->mStart)
                {
                    ongoingAction["start"] = imgui_json::number(clip->mStart);
                    // add actions of other selected clips
                    for (auto clip : timeline->m_Clips)
                    {
                        if (clip->mID == clipId || !clip->bSelected)
                            continue;
                        imgui_json::value action;
                        action["action"] = "MOVE_CLIP";
                        action["clip_id"] = imgui_json::number(clip->mID);
                        action["media_type"] = imgui_json::number(clip->mType);
                        MediaTrack* track = timeline->FindTrackByClipID(clip->mID);
                        action["from_track_id"] = imgui_json::number(track->mID);
                        action["start"] = imgui_json::number(clip->mStart);
                        timeline->mUiActions.push_back(std::move(action));
                    }
                }
            }
            else if (actionName == "CROP_CLIP")
            {
                ongoingAction["startOffset"] = imgui_json::number(clip->mStartOffset);
                ongoingAction["endOffset"] = imgui_json::number(clip->mEndOffset);
            }

            if (ongoingAction.contains("action"))
            {
                timeline->mUiActions.push_back(std::move(ongoingAction));
            }
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
    if (timeline->mShowHelpTooltips)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
        if (mouseTime != -1 && mouseClip != -1 && mouseEntry >= 0 && mouseEntry < timeline->m_Tracks.size())
        {
            if (mouseClip != -1 && !bMoving)
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("Help:");
                ImGui::TextUnformatted("    Left botton click to select clip");
                ImGui::TextUnformatted("    Left botton double click to editing clip");
                ImGui::TextUnformatted("    Left botton double click title bar zip/unzip clip");
                ImGui::TextUnformatted("    Hold left Shift key to appand select");
                ImGui::TextUnformatted("    Hold left Alt/Option key to cut clip");
                ImGui::EndTooltip();
            }
            else if (bMoving)
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("Help:");
                ImGui::TextUnformatted("    Hold left Command/Win key to single select");
                ImGui::EndTooltip();
            }
        }
        if (overHorizonScrollBar)
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("Help:");
            ImGui::TextUnformatted("    Mouse wheel up/down zooming timeline");
            ImGui::TextUnformatted("    Mouse wheel left/right moving timeline");
            ImGui::EndTooltip();
        }
        if (overTrackView)
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("Help:");
            ImGui::TextUnformatted("    Mouse wheel left/right moving timeline");
            ImGui::EndTooltip();
        }
        if (overLegend)
        {
            ImGui::BeginTooltip();
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
                imgui_json::value action;
                action["action"] = "ADD_CLIP";
                action["media_type"] = imgui_json::number(item->mMediaType);
                if (track)
                    action["to_track_id"] = imgui_json::number(track->mID);
                action["start"] = imgui_json::number(item->mStart);
                action["end"] = imgui_json::number(item->mEnd);
                if (item->mMediaType == MEDIA_PICTURE)
                {
                    ImageClip * new_image_clip = new ImageClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, timeline);
                    action["clip_id"] = imgui_json::number(new_image_clip->mID);
                    timeline->m_Clips.push_back(new_image_clip);
                    bool can_insert_clip = track ? track->CanInsertClip(new_image_clip, mouseTime) : false;
                    if (can_insert_clip)
                    {
                        // update clip info and push into track
                        track->InsertClip(new_image_clip, mouseTime);
                        timeline->Updata();
                    }
                    else
                    {
                        int newTrackIndex = timeline->NewTrack("", MEDIA_PICTURE, true);
                        MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
                        newTrack->InsertClip(new_image_clip, mouseTime);
                        action["to_track_id"] = imgui_json::number(newTrack->mID);
                    }
                }
                else if (item->mMediaType == MEDIA_AUDIO)
                {
                    AudioClip * new_audio_clip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName, item->mMediaOverview, timeline);
                    action["clip_id"] = imgui_json::number(new_audio_clip->mID);
                    timeline->m_Clips.push_back(new_audio_clip);
                    bool can_insert_clip = track ? track->CanInsertClip(new_audio_clip, mouseTime) : false;
                    if (can_insert_clip)
                    {
                        // update clip info and push into track
                        track->InsertClip(new_audio_clip, mouseTime);
                        timeline->Updata();
                    }
                    else
                    {
                        int newTrackIndex = timeline->NewTrack("", MEDIA_AUDIO, true);
                        MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
                        newTrack->InsertClip(new_audio_clip, mouseTime);
                        action["to_track_id"] = imgui_json::number(newTrack->mID);
                    }
                } 
                else if (item->mMediaType == MEDIA_TEXT)
                {
                    // subtitle track isn't like other media tracks, it need load clips after insert a empty track
                    // text clip don't band with media item
                    int newTrackIndex = timeline->NewTrack("", MEDIA_TEXT, true);
                    MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
                    newTrack->mMttReader = timeline->mMtvReader->BuildSubtitleTrackFromFile(newTrack->mID, item->mPath);//DataLayer::SubtitleTrack::BuildFromFile(newTrack->mID, item->mPath);
                    if (newTrack->mMttReader)
                    {
                        auto& style = newTrack->mMttReader->DefaultStyle();
                        newTrack->mMttReader->SetFrameSize(timeline->mWidth, timeline->mHeight);
                        newTrack->mMttReader->SeekToIndex(0);
                        newTrack->mMttReader->EnableFullSizeOutput(false);
                        DataLayer::SubtitleClipHolder hSubClip = newTrack->mMttReader->GetCurrClip();
                        while (hSubClip)
                        {
                            TextClip * new_text_clip = new TextClip(hSubClip->StartTime(), hSubClip->EndTime(), newTrack->mID, newTrack->mName, hSubClip->Text(), timeline);
                            new_text_clip->SetClipDefault(style);
                            new_text_clip->mClipHolder = hSubClip;
                            new_text_clip->mTrack = newTrack;
                            timeline->m_Clips.push_back(new_text_clip);
                            newTrack->InsertClip(new_text_clip, hSubClip->StartTime(), false);
                            action["clip_id"] = imgui_json::number(new_text_clip->mID);
                            hSubClip = newTrack->mMttReader->GetNextClip();
                        }
                        if (newTrack->mMttReader->Duration() > timeline->mEnd)
                        {
                            timeline->mEnd = newTrack->mMttReader->Duration() + 1000;
                        }
                    }
                }
                else
                {
                    bool create_new_track = false;
                    MediaTrack * videoTrack = nullptr;
                    VideoClip * new_video_clip = nullptr;
                    AudioClip * new_audio_clip = nullptr;
                    const MediaInfo::VideoStream* video_stream = item->mMediaOverview->GetVideoStream();
                    const MediaInfo::AudioStream* audio_stream = item->mMediaOverview->GetAudioStream();
                    const MediaInfo::SubtitleStream * subtitle_stream = nullptr;
                    if (video_stream)
                    {
                        SnapshotGenerator::ViewerHolder hViewer;
                        SnapshotGeneratorHolder hSsGen = timeline->GetSnapshotGenerator(item->mID);
                        if (hSsGen) hViewer = hSsGen->CreateViewer();
                        new_video_clip = new VideoClip(item->mStart, item->mEnd, item->mID, item->mName + ":Video", item->mMediaOverview->GetMediaParser(), hViewer, timeline);
                        action["clip_id"] = imgui_json::number(new_video_clip->mID);
                        timeline->m_Clips.push_back(new_video_clip);
                        bool can_insert_clip = track ? track->CanInsertClip(new_video_clip, mouseTime) : false;
                        if (can_insert_clip)
                        {
                            // update clip info and push into track
                            track->InsertClip(new_video_clip, mouseTime);
                            timeline->Updata();
                            videoTrack = track;
                        }
                        else
                        {
                            int newTrackIndex = timeline->NewTrack("", MEDIA_VIDEO, true);
                            videoTrack = timeline->m_Tracks[newTrackIndex];
                            videoTrack->InsertClip(new_video_clip, mouseTime);
                            create_new_track = true;
                            action["to_track_id"] = imgui_json::number(videoTrack->mID);
                        }
                    }
                    if (audio_stream)
                    {
                        new_audio_clip = new AudioClip(item->mStart, item->mEnd, item->mID, item->mName + ":Audio", item->mMediaOverview, timeline);
                        timeline->m_Clips.push_back(new_audio_clip);
                        imgui_json::value action2;
                        action2["action"] = "ADD_CLIP";
                        action2["media_type"] = imgui_json::number(new_audio_clip->mType);
                        action2["clip_id"] = imgui_json::number(new_audio_clip->mID);
                        action2["start"] = (double)new_audio_clip->mStart/1000;
                        if (!create_new_track)
                        {
                            if (new_video_clip)
                            {
                                // video clip is insert into track, we need check if this track has linked track
                                if (track && track->mLinkedTrack != -1)
                                {
                                    MediaTrack * relative_track = timeline->FindTrackByID(track->mLinkedTrack);
                                    if (relative_track && relative_track->mType == MEDIA_AUDIO)
                                    {
                                        bool can_insert_clip = relative_track->CanInsertClip(new_audio_clip, mouseTime);
                                        if (can_insert_clip)
                                        {
                                            if (new_video_clip->mGroupID == -1)
                                            {
                                                timeline->NewGroup(new_video_clip);
                                            }
                                            timeline->AddClipIntoGroup(new_audio_clip, new_video_clip->mGroupID);
                                            relative_track->InsertClip(new_audio_clip, mouseTime);
                                            action2["to_track_id"] = imgui_json::number(relative_track->mID);
                                            timeline->Updata();
                                        }
                                        else
                                            create_new_track = true;
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
                                    track->InsertClip(new_audio_clip, mouseTime);
                                    action2["to_track_id"] = imgui_json::number(track->mID);
                                    timeline->Updata();
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
                                    timeline->NewGroup(new_video_clip);
                                }
                                timeline->AddClipIntoGroup(new_audio_clip, new_video_clip->mGroupID);
                            }

                            int newTrackIndex = timeline->NewTrack("", MEDIA_AUDIO, true);
                            MediaTrack * audioTrack = timeline->m_Tracks[newTrackIndex];
                            audioTrack->InsertClip(new_audio_clip, mouseTime);
                            action2["to_track_id"] = imgui_json::number(audioTrack->mID);
                            if (videoTrack)
                            {
                                videoTrack->mLinkedTrack = audioTrack->mID;
                                audioTrack->mLinkedTrack = videoTrack->mID;
                            }

                        }
                        timeline->mUiActions.push_back(std::move(action2));
                    }
                    // TODO::Dicky add subtitle stream here?
                }
                timeline->mUiActions.push_back(std::move(action));
                ret = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // handle delete event
    for (auto clipId : delClipEntry)
    {
        imgui_json::value action;
        action["action"] = "REMOVE_CLIP";
        Clip* clip = timeline->FindClipByID(clipId);
        action["media_type"] = imgui_json::number(clip->mType);
        action["clip_id"] = imgui_json::number(clipId);
        auto track = timeline->FindTrackByClipID(clipId);
        if (track && !track->mLocked)
        {
            track->DeleteClip(clipId);
            action["from_track_id"] = imgui_json::number(track->mID);
            timeline->DeleteClip(clipId);
            timeline->mUiActions.push_back(std::move(action));
            ret = true;
        }
    }
    if (delTrackEntry != -1)
    {
        MediaTrack* track = timeline->m_Tracks[delTrackEntry];
        if (track && !track->mLocked)
        {
            MEDIA_TYPE trackMediaType = track->mType;
            int64_t delTrackId = timeline->DeleteTrack(delTrackEntry);
            if (delTrackId != -1)
            {
                imgui_json::value action;
                action["action"] = "REMOVE_TRACK";
                action["media_type"] = imgui_json::number(trackMediaType);
                action["track_id"] = imgui_json::number(delTrackId);
                timeline->mUiActions.push_back(std::move(action));
                ret = true;
            }
        }
    }
    if (removeEmptyTrack)
    {
        for (auto iter = timeline->m_Tracks.begin(); iter != timeline->m_Tracks.end();)
        {
            if ((*iter)->m_Clips.size() <= 0)
            {
                auto track = *iter;
                imgui_json::value action;
                action["action"] = "REMOVE_TRACK";
                action["media_type"] = imgui_json::number(track->mType);
                action["track_id"] = imgui_json::number(track->mID);
                timeline->mUiActions.push_back(std::move(action));
                iter = timeline->m_Tracks.erase(iter);
                delete track;
                ret = true;
            }
            else
                ++iter;
        }
    }

    // handle insert event
    switch(insertEmptyTrackType)
    {
        case MEDIA_UNKNOWN: break;
        case MEDIA_VIDEO:
            timeline->NewTrack("", MEDIA_VIDEO, true);
            ret = true;
        break;
        case MEDIA_AUDIO:
            timeline->NewTrack("", MEDIA_AUDIO, true);
            ret = true;
        break;
        case MEDIA_PICTURE:
            timeline->NewTrack("", MEDIA_PICTURE, true);
            ret = true;
        break;
        case MEDIA_TEXT:
        {
            int newTrackIndex = timeline->NewTrack("", MEDIA_TEXT, true);
            MediaTrack * newTrack = timeline->m_Tracks[newTrackIndex];
            newTrack->mMttReader = timeline->mMtvReader->NewEmptySubtitleTrack(newTrack->mID); //DataLayer::SubtitleTrack::NewEmptyTrack(newTrack->mID);
            newTrack->mMttReader->SetFont(timeline->mFontName);
            newTrack->mMttReader->SetFrameSize(timeline->mWidth, timeline->mHeight);
            newTrack->mMttReader->EnableFullSizeOutput(false);
            ret = true;
        }
        break;
        default:
        break;
    }
    
    // handle group event
    if (groupClipEntry.size() > 0)
    {
        ClipGroup new_group(timeline);
        timeline->m_Groups.push_back(new_group);
        for (auto clip_id : groupClipEntry)
        {
            auto clip = timeline->FindClipByID(clip_id);
            timeline->AddClipIntoGroup(clip, new_group.mID);
        }
        ret = true;
    }

    for (auto clip_id : unGroupClipEntry)
    {
        auto clip = timeline->FindClipByID(clip_id);
        timeline->DeleteClipFromGroup(clip, clip->mGroupID);
        ret = true;
    }

    // handle track moving
    if (trackMovingEntry != -1 && trackEntry != -1 && trackMovingEntry != trackEntry)
    {
        timeline->MovingTrack(trackMovingEntry, trackEntry);
        ret = true;
    }

    // for debug
    //ImGui::BeginTooltip();
    //ImGui::Text("%s", std::to_string(trackEntry).c_str());
    //ImGui::Text("%s", TimelineMillisecToString(mouseTime).c_str());
    //ImGui::EndTooltip();
    //ImGui::ShowMetricsWindow();
    // for debug end

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (!timeline->mUiActions.empty())
        {
            timeline->PerformUiActions();
            ret = true;
        }
    }
    return ret;
}

/***********************************************************************************************************
 * Draw Clip Timeline
 ***********************************************************************************************************/
bool DrawClipTimeLine(BaseEditingClip * editingClip, int header_height, int custom_height)
{
    /***************************************************************************************
    |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    c
    |_______________|_____________________________________________________________________ a
    |               |        custom area                                                   n 
    |               |                                                                      v                                            
    |_______________|_____________________________________________________________________ a
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
        return ret;
    }

    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    static bool MovingCurrentTime = false;
    bool isFocused = ImGui::IsWindowFocused();
    // modify start/end/offset range
    int64_t duration = ImMax(editingClip->mEnd - editingClip->mStart, (int64_t)1);
    int64_t start = editingClip->mStartOffset;
    int64_t end = start + duration;

    ImGui::BeginGroup();
    ImRect regionRect(window_pos + ImVec2(0, header_height), window_pos + window_size);
    float msPixelWidth = (float)(window_size.x) / (float)duration;
    ImRect custom_view_rect(window_pos + ImVec2(0, header_height), window_pos + window_size);

    //header
    //header time and lines
    ImVec2 headerSize(window_size.x, (float)header_height);
    ImGui::InvisibleButton("ClipTimelineBar#filter", headerSize);
    draw_list->AddRectFilled(window_pos, window_pos + headerSize, COL_DARK_ONE, 0);

    ImRect movRect(window_pos, window_pos + window_size);
    if ( !MovingCurrentTime && editingClip->mCurrent >= start && movRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left) && isFocused)
    {
        MovingCurrentTime = true;
        editingClip->bSeeking = true;
    }
    if (MovingCurrentTime && duration)
    {
        auto oldPos = editingClip->mCurrent;
        auto newPos = (int64_t)((io.MousePos.x - movRect.Min.x) / msPixelWidth) + start;
        if (newPos < start)
            newPos = start;
        if (newPos >= end)
            newPos = end;
        editingClip->Seek(newPos);
    }
    if (editingClip->bSeeking && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        MovingCurrentTime = false;
        editingClip->bSeeking = false;
    }

    int64_t modTimeCount = 10;
    int timeStep = 1;
    while ((modTimeCount * msPixelWidth) < 100)
    {
        modTimeCount *= 10;
        timeStep *= 10;
    };
    int halfModTime = modTimeCount / 2;
    auto drawLine = [&](int64_t i, int regionHeight)
    {
        bool baseIndex = ((i % modTimeCount) == 0) || (i == 0 || i == duration);
        bool halfIndex = (i % halfModTime) == 0;
        int px = (int)window_pos.x + int(i * msPixelWidth);
        int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
        int tiretEnd = baseIndex ? regionHeight : header_height;
        if (px <= (window_size.x + window_pos.x) && px >= window_pos.x)
        {
            draw_list->AddLine(ImVec2((float)px, window_pos.y + (float)tiretStart), ImVec2((float)px, window_pos.y + (float)tiretEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
        }
        if (baseIndex && px >= window_pos.x)
        {
            auto time_str = TimelineMillisecToString(i + start, 2);
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
    const float arrowWidth = draw_list->_Data->FontSize;
    float arrowOffset = window_pos.x + (editingClip->mCurrent - start) * msPixelWidth - arrowWidth * 0.5f;
    ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, window_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
    ImGui::SetWindowFontScale(0.8);
    auto time_str = TimelineMillisecToString(editingClip->mCurrent, 2);
    ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
    float strOffset = window_pos.x + (editingClip->mCurrent - start) * msPixelWidth - str_size.x * 0.5f;
    ImVec2 str_pos = ImVec2(strOffset, window_pos.y + 10);
    draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
    draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);

    draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
    ImVec2 contentMin(window_pos.x, window_pos.y + (float)header_height);
    ImVec2 contentMax(window_pos.x + window_size.x, window_pos.y + (float)header_height + float(custom_height));

    // snapshot
    editingClip->DrawContent(draw_list, contentMin, contentMax);
    // draw_list->AddRect(contentMin+ImVec2(3, 3), contentMax+ImVec2(-3, -3), IM_COL32_WHITE);

    // cursor line
    static const float cursorWidth = 2.f;
    float cursorOffset = contentMin.x + (editingClip ->mCurrent - start) * msPixelWidth - cursorWidth * 0.5f + 1;
    draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMax.y), IM_COL32(0, 255, 0, 224), cursorWidth);
    draw_list->PopClipRect();
    ImGui::EndGroup();

    return ret;
}

/***********************************************************************************************************
 * Draw Fusion Timeline
 ***********************************************************************************************************/
bool DrawOverlapTimeLine(BaseEditingOverlap * overlap, int header_height, int custom_height)
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
    if (overlap->mCurrent < start)
        overlap->mCurrent = start;
    if (overlap->mCurrent >= end)
        overlap->mCurrent = end;

    ImGui::BeginGroup();
    ImRect regionRect(window_pos + ImVec2(0, header_height), window_pos + window_size);
    
    float msPixelWidth = (float)(window_size.x) / (float)duration;
    ImRect custom_view_rect(window_pos + ImVec2(0, header_height), window_pos + window_size);

    //header
    //header time and lines
    ImVec2 headerSize(window_size.x, (float)header_height);
    ImGui::InvisibleButton("ClipTimelineBar#overlap", headerSize);
    draw_list->AddRectFilled(window_pos, window_pos + headerSize, COL_DARK_ONE, 0);

    ImRect movRect(window_pos, window_pos + window_size);
    if (!MovingCurrentTime && overlap->mCurrent >= start && movRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left) && isFocused)
    {
        MovingCurrentTime = true;
        overlap->bSeeking = true;
    }
    if (MovingCurrentTime && duration)
    {
        auto oldPos = overlap->mCurrent;
        auto newPos = (int64_t)((io.MousePos.x - movRect.Min.x) / msPixelWidth) + start;
        if (newPos < start)
            newPos = start;
        if (newPos >= end)
            newPos = end;
        if (oldPos != newPos)
            overlap->Seek(newPos); // call seek event
    }
    if (overlap->bSeeking && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        MovingCurrentTime = false;
        overlap->bSeeking = false;
    }

    int64_t modTimeCount = 10;
    int timeStep = 1;
    while ((modTimeCount * msPixelWidth) < 100)
    {
        modTimeCount *= 10;
        timeStep *= 10;
    };
    int halfModTime = modTimeCount / 2;
    auto drawLine = [&](int64_t i, int regionHeight)
    {
        bool baseIndex = ((i % modTimeCount) == 0) || (i == 0 || i == duration);
        bool halfIndex = (i % halfModTime) == 0;
        int px = (int)window_pos.x + int(i * msPixelWidth);
        int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
        int tiretEnd = baseIndex ? regionHeight : header_height;
        if (px <= (window_size.x + window_pos.x) && px >= window_pos.x)
        {
            draw_list->AddLine(ImVec2((float)px, window_pos.y + (float)tiretStart), ImVec2((float)px, window_pos.y + (float)tiretEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
        }
        if (baseIndex && px >= window_pos.x)
        {
            auto time_str = TimelineMillisecToString(i + start, 2);
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
    const float arrowWidth = draw_list->_Data->FontSize;
    float arrowOffset = window_pos.x + (overlap->mCurrent - start) * msPixelWidth - arrowWidth * 0.5f;
    ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, window_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
    ImGui::SetWindowFontScale(0.8);
    auto time_str = TimelineMillisecToString(overlap->mCurrent, 2);
    ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
    float strOffset = window_pos.x + (overlap->mCurrent - start) * msPixelWidth - str_size.x * 0.5f;
    ImVec2 str_pos = ImVec2(strOffset, window_pos.y + 10);
    draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
    draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);

    // snapshot
    ImVec2 contentMin(window_pos.x, window_pos.y + (float)header_height);
    ImVec2 contentMax(window_pos.x + window_size.x, window_pos.y + (float)header_height + float(custom_height) * 2);
    overlap->DrawContent(draw_list, contentMin, contentMax);

    // cursor line
    draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
    static const float cursorWidth = 2.f;
    float cursorOffset = contentMin.x + (overlap->mCurrent - start) * msPixelWidth - cursorWidth * 0.5f + 1;
    draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMax.y), IM_COL32(0, 255, 0, 224), cursorWidth);
    draw_list->PopClipRect();
    ImGui::EndGroup();
    return ret;
}
} // namespace MediaTimeline/Main Timeline
