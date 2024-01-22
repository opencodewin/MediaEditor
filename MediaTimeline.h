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

#pragma once
#include <imgui.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImMaskCreator.h>
#include "Overview.h"
#include "Snapshot.h"
#include "MediaReader.h"
#include "MultiTrackVideoReader.h"
#include "MultiTrackAudioReader.h"
#include "VideoTransformFilter.h"
#include "MediaEncoder.h"
#include "AudioRender.h"
#include "SubtitleTrack.h"
#include "UI.h"
#include "MecProject.h"
#include "Event.h"
#include "EventStackFilter.h"
#include "MediaPlayer.h"
#include <thread>
#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <chrono>

#define PLOT_IMPLOT   0
#define PLOT_TEXTURE  1

#define ICON_SORT_ID        u8"\uf887"
#define ICON_SORT_TYPE      u8"\uf885"
#define ICON_SORT_NAME      u8"\uf882"
#define ICON_FILTER_NONE    u8"\uf031"
#define ICON_MEDIA_FINDER   u8"\uf07b"
#define ICON_MEDIA_TIMELINE u8"\uf538"
#define ICON_MEDIA_BANK     u8"\ue907"
#define ICON_MEDIA_TRANS    u8"\ue927"
#define ICON_MEDIA_FILTERS  u8"\ue663"
#define ICON_MEDIA_OUTPUT   u8"\uf197"
#define ICON_MEDIA_PREVIEW  u8"\ue04a"
#define ICON_MEDIA_VIDEO    u8"\ue04b"
#define ICON_MEDIA_AUDIO    u8"\ue050"
#define ICON_MEDIA_WAVE     u8"\ue4e8"        //u8"\ue495"
#define ICON_MEDIA_MIDI     u8"\ue48f"
#define ICON_MEDIA_IMAGE    u8"\ue3f4"
#define ICON_MEDIA_TEXT     u8"\ue8e2"
#define ICON_MEDIA_DIAGNOSIS u8"\uf551"
#define ICON_MEDIA_DELETE   u8"\ue92b"
#define ICON_MEDIA_DELETE_CLIP   u8"\ue92e"
#define ICON_MEDIA_GROUP    u8"\ue595"
#define ICON_MEDIA_UNGROUP  u8"\ue593"
#define ICON_SLIDER_MINIMUM u8"\uf424"
#define ICON_SLIDER_MAXIMUM u8"\uf422"
#define ICON_SLIDER_FRAME   u8"\ue8eb"
#define ICON_SLIDER_CLIP    u8"\ue8ed"
#define ICON_CURRENT_TIME   u8"\ue3e8"
#define ICON_VIEW           u8"\ue8f4"
#define ICON_VIEW_DISABLE   u8"\ue8f5"
#define ICON_ENABLE         u8"\uf205"
#define ICON_DISABLE        u8"\uf204"
#define ICON_ZOOM_IN        u8"\ue94f"
#define ICON_ZOOM_OUT       u8"\ue94d"
#define ICON_ZOOM           u8"\uf3ee"
#define ICON_ITEM_CUT       u8"\ue14e"
#define ICON_SPEAKER        u8"\ue050"
#define ICON_SPEAKER_MUTE   u8"\ue04f"
#define ICON_FILTER         u8"\uf331"
#define ICON_MUSIC          u8"\ue3a1"
#define ICON_MUSIC_DISABLE  u8"\ue440"
#define ICON_AUDIO_MIXING   u8"\ue429"
#define ICON_MUSIC_RECT     u8"\ue030"
#define ICON_MAGIC_3        u8"\ue663"
#define ICON_MAGIC_1        u8"\ue664"
#define ICON_MAGIC_DISABlE  u8"\ue665"
#define ICON_HDR            u8"\ue3ee"
#define ICON_HDR_DISABLE    u8"\ue3ed"
#define ICON_PALETTE        u8"\uf53f"
#define ICON_CROP           u8"\uf125"
#define ICON_ROTATE         u8"\ue437"
#define ICON_LOCKED         u8"\ue897"
#define ICON_UNLOCK         u8"\ue898"
#define ICON_TRASH          u8"\uf014"
#define ICON_CLONE          u8"\uf2d2"
#define ICON_ADD            u8"\uf067"
#define ICON_ALIGN_START    u8"\ue419"
#define ICON_CROPPING_LEFT  u8"\ue045"
#define ICON_CROPPING_RIGHT u8"\ue044"
#define ICON_REMOVE_CUT     u8"\ue011"
#define ICON_CUTTING        u8"\uf0c4"
#define ICON_MOVING         u8"\ue41f"
#define ICON_TRANS          u8"\ue882"
#define ICON_BANK           u8"\uf1b3"
#define ICON_BLUE_PRINT     u8"\ueabe"
#define ICON_BRAIN          u8"\uf5dc"
#define ICON_NEW_PROJECT    u8"\uf271"
#define ICON_OPEN_PROJECT   u8"\uf115"
#define ICON_SAVE           u8"\uf0c7"
#define ICON_CLIP_START     u8"\uf090"
#define ICON_CLIP_END       u8"\uf08b"
#define ICON_RETURN_DEFAULT u8"\ue4e0"
#define ICON_RETURN_ALL     u8"\uec20"
#define ICON_NODE           u8"\uf542"
#define ICON_WATCH          u8"\ue8f4"
#define ICON_UNWATCH        u8"\ue8f5"
#define ICON_DELETE         u8"\ue92b"
#define ICON_CURVE          u8"\ue463"
#define ICON_MAKE_VIDEO     u8"\ue52a"
#define ICON_BP_VALID       u8"\ueb5b"
#define ICON_BP_EDITING     u8"\uf044"
#define ICON_TRACK_ZIP      u8"\ueacf"
#define ICON_TRACK_UNZIP    u8"\uead0"
#define ICON_FILTER_EDITOR  u8"\ueb03"
#define ICON_DELETE_CLIPS   u8"\ue16f"
#define ICON_MASK           u8"\ueb63"
#define ICON_CLIP_ATTRIBUTE ICON_MD_FORMAT_SHAPES

#define ICON_FONT_BOLD      u8"\ue238"
#define ICON_FONT_ITALIC    u8"\ue23f"
#define ICON_FONT_UNDERLINE u8"\ue249"
#define ICON_FONT_STRIKEOUT u8"\ue257"

#define ICON_PLAY_FORWARD   u8"\uf04b"
#define ICON_PLAY_BACKWARD  u8"\uf04b" // need mirror
#define ICON_PAUSE          u8"\uf04c"
#define ICON_STOP           u8"\uf04d"
#define ICON_FAST_BACKWARD  u8"\uf04a"
#define ICON_FAST_FORWARD   u8"\uf04e"
#define ICON_FAST_TO_START  u8"\uf049"
#define ICON_TO_START       u8"\uf048"   
#define ICON_FAST_TO_END    u8"\uf050"
#define ICON_TO_END         u8"\uf051"
#define ICON_EJECT          u8"\uf052"
#define ICON_STEP_BACKWARD  u8"\uf053"
#define ICON_STEP_FORWARD   u8"\uf054"
#define ICON_LOOP           u8"\ue9d6"
#define ICON_LOOP_ONE       u8"\ue9d7"
#define ICON_COMPARE        u8"\uf0db"

#define ICON_CROPED         u8"\ue3e8"
#define ICON_SCALED         u8"\ue433"
#define ICON_UI_DEBUG       u8"\ue868"

#define ICON_1K             u8"\ue95c"
#define ICON_1K_PLUS        u8"\ue95d"
#define ICON_2K             u8"\ue963"
#define ICON_2K_PLUS        u8"\ue964"
#define ICON_3K             u8"\ue966"
#define ICON_3K_PLUS        u8"\ue967"
#define ICON_4K_PLUS        u8"\ue969"
#define ICON_5K             u8"\ue96b"
#define ICON_5K_PLUS        u8"\ue96c"
#define ICON_6K             u8"\ue96e"
#define ICON_6K_PLUS        u8"\ue96f"
#define ICON_7K             u8"\ue971"
#define ICON_7K_PLUS        u8"\ue972"
#define ICON_8K             u8"\ue974"
#define ICON_8K_PLUS        u8"\ue975"
#define ICON_9K             u8"\ue977"
#define ICON_9K_PLUS        u8"\ue978"
#define ICON_10K            u8"\ue951"

#define ICON_ONE            u8"\ue3d0"
#define ICON_TWO            u8"\ue3d1"
#define ICON_THREE          u8"\ue3d2"
#define ICON_FOUR           u8"\ue3d3"
#define ICON_FIVE           u8"\ue3d4"
#define ICON_SIX            u8"\ue3d5"
#define ICON_SEVEN          u8"\ue3d6"
#define ICON_EIGHT          u8"\ue3d7"
#define ICON_NINE           u8"\ue3d8"

#define ICON_STEREO         u8"\uf58f"
#define ICON_MONO           u8"\uf590"

#define ICON_MORE           u8"\ue945"
#define ICON_SETTING        u8"\ue8b8"
#define ICON_HISTOGRAM      u8"\ue4a9"
#define ICON_WAVEFORM       u8"\ue495"
#define ICON_CIE            u8"\ue49e"
#define ICON_VECTOR         u8"\ue49f"
#define ICON_AUDIOVECTOR    u8"\uf20e"
#define ICON_WAVE           u8"\ue4ad"
#define ICON_FFT            u8"\ue4e8"
#define ICON_DB             u8"\ue451"
#define ICON_DB_LEVEL       u8"\ue4a9"
#define ICON_SPECTROGRAM    u8"\ue4a0"
#define ICON_DRAWING_PIN    u8"\uf08d"
#define ICON_EXPAND         u8"\uf0b2"
#define ICON_EXPAND_EVENT   u8"\ue8be"
#define ICON_EVENT_ZOOM     u8"\uf247"
#define ICON_SUBMENU_INDICATOR u8"\ue315"

#define ICON_SETTING_LINK   u8"\uf0c1"
#define ICON_SETTING_UNLINK u8"\uf127"
#define ICON_LINK_UP        u8"\u2510"
#define ICON_LINK_DOWN      u8"\u2518"

#define ICON_EVENT_VIEW     u8"\ue8d7"
#define ICON_EVENT_HIDE     u8"\ue89d"
#define ICON_ENEVT_CHANNEL_ADD u8"\ue97b"

#define ICON_ERROR_MEDIA    u8"\ue160"
#define ICON_ERROR_FRAME    u8"\uf410"
#define ICON_ERROR_AUDIO    u8"\ue440"

#define ICON_TOOLBAR_START  u8"\uf142"
#define ICON_MARK_IN        u8"\ueaf2"
#define ICON_MARK_OUT       u8"\ueaea"
#define ICON_MARK_NONE      u8"\ueaf4"
#define ICON_EMPTY_TRACK    u8"\ue3c0"

#define COL_FRAME_RECT      IM_COL32( 16,  16,  96, 255)
#define COL_LIGHT_BLUR      IM_COL32( 16, 128, 255, 255)
#define COL_CANVAS_BG       IM_COL32( 36,  36,  36, 255)
#define COL_LEGEND_BG       IM_COL32( 33,  33,  38, 255)
#define COL_PANEL_BG        IM_COL32( 36,  36,  40, 255)
#define COL_MARK            IM_COL32(255, 255, 255, 128)
#define COL_MARK_HALF       IM_COL32(128, 128, 128, 128)
#define COL_RULE_TEXT       IM_COL32(224, 224, 224, 255)
#define COL_SLOT_DEFAULT    IM_COL32( 80,  80, 100, 255)
#define COL_SLOT_ODD        IM_COL32( 58,  58,  58, 255)
#define COL_SLOT_EVEN       IM_COL32( 64,  64,  64, 255)
#define COL_EVENT_ODD       IM_COL32( 24,  24,  24, 255)
#define COL_EVENT_EVEN      IM_COL32( 32,  32,  32, 255)
#define COL_EVENT_ODD_DARK  IM_COL32( 10,  10,  10, 255)
#define COL_EVENT_EVEN_DARK IM_COL32( 18,  18,  18, 255)
#define COL_EVENT_HOVERED   IM_COL32( 48,  48,  48, 255)
#define COL_SLOT_SELECTED   IM_COL32(255,  64,  64, 255)
#define COL_SLOT_V_LINE     IM_COL32( 32,  32,  32,  96)
#define COL_SLIDER_BG       IM_COL32( 32,  32,  48, 255)
#define COL_SLIDER_THUMB_IN IM_COL32(224, 224, 224, 255)
#define COL_SLIDER_IN       IM_COL32(192, 192, 192, 255)
#define COL_SLIDER_MOVING   IM_COL32(144, 144, 144, 255)
#define COL_SLIDER_HANDLE   IM_COL32(112, 112, 112, 255)
#define COL_SLIDER_SIZING   IM_COL32(170, 170, 170, 255)
#define COL_CURSOR_ARROW    IM_COL32(  0, 255,   0, 192)
#define COL_CURSOR_LINE     IM_COL32(  0, 255,   0, 128)
#define COL_CURSOR_TEXT_BG  IM_COL32(  0, 128,   0, 144)
#define COL_CURSOR_TEXT     IM_COL32(  0, 255,   0, 255)
#define COL_CURSOR_TEXT_BG2 IM_COL32(128, 128,   0, 128)
#define COL_CURSOR_TEXT2    IM_COL32(255, 255,   0, 255)
#define COL_CURSOR_ARROW_R  IM_COL32(255,   0,   0, 192)
#define COL_CURSOR_LINE_R   IM_COL32(255,   0,   0, 128)
#define COL_CURSOR_TEXT_BR  IM_COL32(128,   0,   0, 144)
#define COL_CURSOR_TEXT_R   IM_COL32(255, 160, 160, 255)
#define COL_DARK_ONE        IM_COL32( 33,  33,  38, 255)
#define COL_DARK_TWO        IM_COL32( 40,  40,  46, 255)
#define COL_DARK_PANEL      IM_COL32( 48,  48,  54, 255)
#define COL_DEEP_DARK       IM_COL32( 23,  24,  26, 255)
#define COL_BLACK_DARK      IM_COL32( 16,  16,  16, 255)
#define COL_GRATICULE_DARK  IM_COL32(128,  96,   0, 128)
#define COL_GRATICULE       IM_COL32(255, 196,   0, 128)
#define COL_GRATICULE_HALF  IM_COL32(255, 196,   0,  64)
#define COL_GRAY_GRATICULE  IM_COL32( 96,  96,  96, 128)
#define COL_GRAY_TEXT       IM_COL32(128, 128, 128, 128)
#define COL_MARK_BAR        IM_COL32(128, 128, 128, 170)
#define COL_MARK_DOT        IM_COL32(170, 170, 170, 224)
#define COL_MARK_DOT_LIGHT  IM_COL32(255, 255, 255, 224)
#define COL_ERROR_MEDIA     IM_COL32(160,   0,   0, 224)
#define COL_TITLE_COLOR     IM_COL32(192, 192, 192, 255)
#define COL_TITLE_OUTLINE   IM_COL32( 32,  32, 192, 128)
//#define COL_MIXING_BG       IM_COL32( 10,  20,  17, 255)
//#define COL_MIXING_BG_MID   IM_COL32( 17,  28,  19, 255)
//#define COL_MIXING_BG_HIGH  IM_COL32( 25,  35,  20, 255)
#define COL_MIXING_BG       IM_COL32( 10,  10,  10, 255)
#define COL_MIXING_BG_MID   IM_COL32( 20,  20,  20, 255)
#define COL_MIXING_BG_HIGH  IM_COL32( 30,  30,  30, 255)
#define COL_MIXING_BORDER_LOW   IM_COL32( 30,  50,  60, 255)
#define COL_MIXING_BORDER IM_COL32( 60, 100, 120, 255)

#define HALF_COLOR(c)       (c & 0xFFFFFF) | 0x40000000;
#define TIMELINE_OVER_LENGTH    5000        // add 5 seconds end of timeline

namespace MediaTimeline
{
#define DEFAULT_TRACK_HEIGHT        0
#define DEFAULT_VIDEO_TRACK_HEIGHT  32
#define DEFAULT_AUDIO_TRACK_HEIGHT  20
#define DEFAULT_TEXT_TRACK_HEIGHT   20
#define DEFAULT_EVENT_TRACK_HEIGHT  20

#define PREVIEW_TEXTURE_POOL_NAME                           "PreviewTexturePool"
#define ARBITRARY_SIZE_TEXTURE_POOL_NAME                    "ArbitrarySizeTexturePool"
#define VIDEOITEM_OVERVIEW_GRID_TEXTURE_POOL_NAME           "VideoItemOverviewGridTexturePool"
#define VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME           "VideoClipSnapshotGridTexturePool"
#define EDITING_VIDEOCLIP_SNAPSHOT_GRID_TEXTURE_POOL_NAME   "EditingVideoClipSnapshotGridTexturePool"

#define MEDIA_UNKNOWN                       0
#define MEDIA_DUMMY                         0x80000000
#define MEDIA_VIDEO                         0x00000100
#define MEDIA_SUBTYPE_VIDEO_IMAGE           (MEDIA_VIDEO+1)
#define MEDIA_SUBTYPE_VIDEO_IMAGE_SEQUENCE  (MEDIA_VIDEO+2)
#define MEDIA_AUDIO                         0x00000200
#define MEDIA_SUBTYPE_AUDIO_MIDI            (MEDIA_AUDIO+1)
#define MEDIA_TEXT                          0x00000400
#define MEDIA_SUBTYPE_TEXT_SUBTITLE         (MEDIA_TEXT+1)
#define MEDIA_EVENT                         0x00000800
#define MEDIA_CUSTOM                        0x40000000

#define IS_DUMMY(t)     (((t) & MEDIA_DUMMY) != 0)
#define IS_VIDEO(t)     (((t) & MEDIA_VIDEO) != 0)
#define IS_IMAGE(t)     ((t) == MEDIA_SUBTYPE_VIDEO_IMAGE)
#define IS_IMAGESEQ(t)  ((t) == MEDIA_SUBTYPE_VIDEO_IMAGE_SEQUENCE)
#define IS_AUDIO(t)     (((t) & MEDIA_AUDIO) != 0)
#define IS_MIDI(t)      ((t) == MEDIA_SUBTYPE_AUDIO_MIDI)
#define IS_TEXT(t)      (((t) & MEDIA_TEXT) != 0)
#define IS_SUBTITLE(t)  ((t) == MEDIA_SUBTYPE_TEXT_SUBTITLE)
#define IS_EVENT(t)     (((t) & MEDIA_EVENT) != 0)
#define IS_SAME_TYPE(t1, t2) ((t1) & (t2) & 0xFFFFFF00)

static inline uint32_t EstimateMediaType(std::string file_suffix)
{
    uint32_t type = MEDIA_UNKNOWN;
    std::transform(file_suffix.begin(), file_suffix.end(), file_suffix.begin(), [](auto c) { return std::tolower(c); });
    if (!file_suffix.empty())
    {
        if ((file_suffix.compare(".mp4") == 0) ||
            (file_suffix.compare(".mov") == 0) ||
            (file_suffix.compare(".mkv") == 0) ||
            (file_suffix.compare(".mxf") == 0) ||
            (file_suffix.compare(".avi") == 0) ||
            (file_suffix.compare(".webm") == 0) ||
            (file_suffix.compare(".ts") == 0))
            type = MEDIA_VIDEO;
        else 
            if ((file_suffix.compare(".wav") == 0) ||
                (file_suffix.compare(".mp3") == 0) ||
                (file_suffix.compare(".aac") == 0) ||
                (file_suffix.compare(".ac3") == 0) ||
                (file_suffix.compare(".dts") == 0) ||
                (file_suffix.compare(".ogg") == 0))
            type = MEDIA_AUDIO;
        else
            if ((file_suffix.compare(".mid") == 0) ||
                (file_suffix.compare(".midi") == 0))
            type = MEDIA_SUBTYPE_AUDIO_MIDI;
        else 
            if ((file_suffix.compare(".jpg") == 0) ||
                (file_suffix.compare(".jpeg") == 0) ||
                (file_suffix.compare(".png") == 0) ||
                (file_suffix.compare(".gif") == 0) ||
                (file_suffix.compare(".tiff") == 0) ||
                (file_suffix.compare(".webp") == 0))
            type = MEDIA_SUBTYPE_VIDEO_IMAGE;
        else
            if ((file_suffix.compare(".txt") == 0) ||
                (file_suffix.compare(".srt") == 0) ||
                (file_suffix.compare(".ass") == 0) ||
                (file_suffix.compare(".stl") == 0) ||
                (file_suffix.compare(".lrc") == 0) ||
                (file_suffix.compare(".xml") == 0))
            type = MEDIA_SUBTYPE_TEXT_SUBTITLE;
    }
    else
    {
        type = MEDIA_SUBTYPE_VIDEO_IMAGE_SEQUENCE;
    }
    return type;
}

enum AudioVectorScopeMode  : int
{
    LISSAJOUS,
    LISSAJOUS_XY,
    POLAR,
    MODE_NB,
};

struct IDGenerator
{
    int64_t GenerateID();

    void SetState(int64_t state);
    int64_t State() const;

private:
    int64_t m_State = ImGui::get_current_time_usec();
};

struct MediaItem
{
    int64_t mID;                            // media ID
    std::string mName;
    std::string mPath;
    void* mHandle;
    bool mValid {false};                    // Media source is valid
    bool mSelected {false};                 // Media source is selected by double clicked
    int64_t mSrcLength  {0};                // whole Media end in ms
    uint32_t mMediaType {MEDIA_UNKNOWN};
    MediaCore::MediaParser::Holder mhParser;
    MediaCore::Overview::Holder mMediaOverview;
    RenderUtils::TextureManager::Holder mTxMgr;
    std::vector<RenderUtils::ManagedTexture::Holder> mMediaThumbnail;
    std::vector<ImTextureID> mWaveformTextures;
    MediaItem(const std::string& name, const std::string& path, uint32_t type, void* handle);
    MediaItem(MediaCore::MediaParser::Holder hParser, void* handle);
    ~MediaItem();
    bool Initialize();
    bool ChangeSource(const std::string& name, const std::string& path);
    void ReleaseItem();
    void UpdateThumbnail();
};

struct VideoSnapshotInfo
{
    ImRect rc;
    int64_t time_stamp;
    int64_t duration;
    float frame_width;
};

struct Snapshot
{
    ImTextureID texture {nullptr};
    int64_t     time_stamp {0};
    int64_t     estimate_time {0};
    bool        available{false};
};

struct Overlap
{
    int64_t mID                     {-1};       // overlap ID, project saved
    uint32_t mType                  {MEDIA_UNKNOWN};
    int64_t mStart                  {0};        // overlap start time at timeline, project saved
    int64_t mEnd                    {0};        // overlap end time at timeline, project saved
    int64_t mCurrent                {0};        // overlap current time, project saved
    bool bPlay                      {false};    // overlap play status
    bool bForward                   {true};     // overlap play direction
    bool bSeeking                   {false};    // overlap is seeking
    bool bEditing                   {false};    // overlap is editing
    std::pair<int64_t, int64_t>     m_Clip;     // overlaped clip's pair, project saved
    imgui_json::value mTransitionBP;            // overlap transion blueprint, project saved
    ImGui::KeyPointEditor mTransitionKeyPoints; // overlap key points, project saved
    void * mHandle                  {nullptr};  // overlap belong to timeline 
    Overlap(int64_t start, int64_t end, int64_t clip_first, int64_t clip_second, uint32_t type, void* handle);
    ~Overlap();

    bool IsOverlapValid(bool fixRange = false);
    bool IsOverlapEmpty();
    void Update(int64_t start, int64_t start_clip_id, int64_t end, int64_t end_clip_id);
    void Seek();
    static Overlap * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);
};

#define EVENT_SELECTED_BIT  0
#define EVENT_HOVERED_BIT   1
#define EVENT_NEED_SCROLL   2
#define EVENT_SELECTED      (1UL << EVENT_SELECTED_BIT)
#define EVENT_HOVERED       (1UL << EVENT_HOVERED_BIT)
#define EVENT_SCROLLING     (1UL << EVENT_NEED_SCROLL)

struct EventTrack
{
    EventTrack(int64_t id, void* handle);
    int64_t mID             {-1};               // event track ID, project saved
    int64_t mClipID         {-1};               // event track belong to clip ID, project saved
    bool    mExpanded       {false};            // event track is expanded for curve, project saved
    std::vector<int64_t> m_Events;              // track clips, project saved(id only)
    void * mHandle          {nullptr};          // Event track belong to timeline
    
    static EventTrack* Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);

    bool DrawContent(ImDrawList *draw_list, ImRect rect, int event_height, int curve_height, int64_t view_start, int64_t view_end, float pixelWidthMS, bool editable, bool& changed);
    void SelectEvent(MEC::Event::Holder event, bool appand);
    MEC::Event::Holder FindPreviousEvent(int64_t id);
    MEC::Event::Holder FindNextEvent(int64_t id);
    int64_t FindEventSpace(int64_t time);
    void Update();
};

struct TimeLine;
struct MediaTrack;

struct Clip
{
    int64_t mID                 {-1};               // clip ID, project saved
    int64_t mMediaID            {-1};               // MediaItem ID, project saved
    MediaItem* mpMediaItem      {nullptr};
    int64_t mGroupID            {-1};               // Group ID clip belong, project saved
    uint32_t mType              {MEDIA_UNKNOWN};    // clip type, project saved
    std::string mName;                              // clip name, project saved
    std::string mPath;                              // clip media path, project saved
    bool bSelected              {false};            // clip is selected, project saved
    std::mutex mLock;                               // clip mutex, not using yet
    void* mHandle               {nullptr};          // clip belong to timeline 
    MediaCore::MediaParser::Holder mMediaParser;
    MediaCore::Overview::Holder mhOverview;
    int64_t mViewWndDur         {0};
    float mPixPerMs             {0};
    int mTrackHeight            {0};
    bool bMoving                {false};            // clip is moving
    bool bHovered               {false};            // clip is under mouse

    imgui_json::value           mClipJson;
    bool                        bAttributeScrolling {false}; // need scrolling UI to attribute setting

    MEC::EventStack*            mEventStack {nullptr};// clip event stack,
    std::vector<EventTrack*>    mEventTracks;       // clip event tracks, contain event IDs only, project saved

    int64_t mDragAnchorTime{0};
    int64_t firstTime = 0;
    int64_t lastTime = 0;
    int64_t visibleTime = 0;
    float msPixelWidthTarget = -1.f;

    virtual ~Clip();

    virtual int64_t Moving(int64_t diff, int mouse_track);
    virtual int64_t Cropping(int64_t diff, int type);
    void Cutting(int64_t pos, int64_t gid, int64_t newClipId, std::list<imgui_json::value>* pActionList = nullptr);
    bool isLinkedWith(Clip * clip);

    virtual void ConfigViewWindow(int64_t wndDur, float pixPerMs) { mViewWndDur = wndDur; mPixPerMs = pixPerMs; }
    virtual void SetTrackHeight(int trackHeight) { mTrackHeight = trackHeight; }
    virtual void SetViewWindowStart(int64_t millisec) {}
    virtual void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect, bool updated = false) { drawList->AddRect(leftTop, rightBottom, IM_COL32_BLACK); }
    virtual void DrawTooltips() {};
    virtual bool ReloadSource(MediaItem* pMediaItem) = 0;
    virtual bool LoadFromJson(const imgui_json::value& j);
    virtual imgui_json::value SaveAsJson();

    int64_t Start() const { return mStart; }
    int64_t End() const { return mEnd; }
    int64_t Length() const { return mEnd-mStart; }
    int64_t StartOffset() const { return mStartOffset; }
    int64_t EndOffset() const { return mEndOffset; }
    void SetPositionAndRange(int64_t start, int64_t end, int64_t startOffset, int64_t endOffset)
    { mStart = start; mEnd = end; mStartOffset = startOffset; mEndOffset = endOffset; }
    bool IsInClipRange(int64_t pos) const { return pos >= mStart && pos < mEnd; }

    int AddEventTrack();
    MEC::Event::Holder FindEventByID(int64_t event_id);
    MEC::Event::Holder FindSelectedEvent();
    bool hasSelectedEvent();
    void EventMoving(int64_t event_id, int64_t diff, int64_t mouse, std::list<imgui_json::value>* pActionList);
    int64_t EventCropping(int64_t event_id, int64_t diff, int type, std::list<imgui_json::value>* pActionList);
    bool AddEvent(int64_t id, int evtTrackIndex, int64_t start, int64_t duration, const BluePrint::Node* node, std::list<imgui_json::value>* pActionList);
    bool AddEvent(int64_t id, int evtTrackIndex, int64_t start, int64_t duration, ID_TYPE nodeTypeId, const std::string& nodeName, std::list<imgui_json::value>* pActionList);
    bool AppendEvent(MEC::Event::Holder event, void* data);
    bool DeleteEvent(int64_t evtId, std::list<imgui_json::value>* pActionList);
    bool DeleteEvent(MEC::Event::Holder event, std::list<imgui_json::value>* pActionList);
    void SelectEvent(MEC::Event::Holder event, bool appand = false);

    void ChangeStart(int64_t pos);
    void ChangeStartOffset(int64_t newOffset);
    void ChangeEndOffset(int64_t newOffset);

protected:
    Clip(TimeLine* pOwner, uint32_t u32Type);
    Clip(TimeLine* pOwner, uint32_t u32Type, const std::string& strName, int64_t i64Start, int64_t i64End, int64_t i64StartOffset = 0, int64_t i64EndOffset = 0);

protected:
    int64_t mStart              {0};                // clip start time in timeline, project saved
    int64_t mEnd                {0};                // clip end time in timeline, project saved
    int64_t mStartOffset        {0};                // clip start time in media, project saved
    int64_t mEndOffset          {0};                // clip end time in media, project saved
};

struct VideoClip : Clip
{
    // video info
    MediaCore::Snapshot::Viewer::Holder mhSsViewer;
    std::vector<VideoSnapshotInfo> mVideoSnapshotInfos; // clip snapshots info, with all croped range
    // image info
    int mWidth          {0};        // image width, project saved
    int mHeight         {0};        // image height, project saved

    static VideoClip* CreateInstance(TimeLine* pOwner, const std::string& strName, MediaItem* pMediaItem, int64_t i64Start, int64_t i64End, int64_t i64StartOffset = 0, int64_t i64EndOffset = 0);
    static VideoClip* CreateInstance(TimeLine* pOwner, MediaItem* pMediaItem, int64_t i64Start);
    static VideoClip* CreateDummyInstance(TimeLine* pOwner, const std::string& strName, int64_t i64Start, int64_t i64End);
    virtual ~VideoClip();

    void CalcDisplayParams();

    void ConfigViewWindow(int64_t wndDur, float pixPerMs) override;
    void SetTrackHeight(int trackHeight) override;
    void SetViewWindowStart(int64_t millisec) override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect, bool updated = false) override;
    bool ReloadSource(MediaItem* pMediaItem) override;

    static VideoClip* CreateInstanceFromJson(const imgui_json::value& j, TimeLine* pOwner);
    imgui_json::value SaveAsJson() override;

    void SetDataLayer(MediaCore::VideoClip::Holder hVClip, bool bSyncStateToDataLayer);
    MediaCore::VideoClip::Holder GetDataLayer() const { return mhDataLayerClip; }

    RenderUtils::ManagedTexture::Holder GetImageTexture() const { return mhImageTx; }

private:
    VideoClip(TimeLine* pOwner) : Clip(pOwner, MEDIA_VIDEO) {}
    VideoClip(TimeLine* pOwner, const std::string& strName, int64_t i64Start, int64_t i64End, int64_t i64StartOffset = 0, int64_t i64EndOffset = 0)
        : Clip(pOwner, MEDIA_VIDEO, strName, i64Start, i64End, i64StartOffset, i64EndOffset)
    {}
    bool UpdateClip(MediaItem* pMediaItem);
    void SyncStateToDataLayer();
    void SyncStateFromDataLayer();

private:
    float mSnapWidth                {0};
    float mSnapHeight               {0};
    int64_t mClipViewStartPos;
    MediaCore::VideoClip::Holder mhDataLayerClip;
    std::vector<MediaCore::Snapshot::Image> mSnapImages;
    RenderUtils::ManagedTexture::Holder mhImageTx;
};

struct AudioClip : Clip
{
    int mAudioChannels {0};             // clip audio channels, project saved
    int mAudioSampleRate {0};           // clip audio sample rate, project saved
    MediaCore::Overview::Waveform::Holder mWaveform {nullptr};  // clip audio snapshot
    MediaCore::Overview::Holder mOverview;
    ImTextureID mWaveformTexture {nullptr}; // clip waveform texture

    static AudioClip* CreateInstance(TimeLine* pOwner, const std::string& strName, MediaItem* pMediaItem, int64_t i64Start, int64_t i64End, int64_t i64StartOffset = 0, int64_t i64EndOffset = 0);
    static AudioClip* CreateInstance(TimeLine* pOwner, MediaItem* pMediaItem, int64_t i64Start);
    static AudioClip* CreateDummyInstance(TimeLine* pOwner, const std::string& strName, int64_t i64Start, int64_t i64End);
    virtual ~AudioClip();

    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect, bool updated = false) override;
    bool ReloadSource(MediaItem* pMediaItem) override;

    static AudioClip* CreateInstanceFromJson(const imgui_json::value& j, TimeLine* pOwner);
    imgui_json::value SaveAsJson() override;

    MediaCore::AudioClip::Holder mhDataLayerClip;
    void SetDataLayer(MediaCore::AudioClip::Holder hVClip, bool bSyncStateToDataLayer);

private:
    AudioClip(TimeLine* pOwner) : Clip(pOwner, MEDIA_AUDIO) {}
    AudioClip(TimeLine* pOwner, const std::string& strName, int64_t i64Start, int64_t i64End, int64_t i64StartOffset = 0, int64_t i64EndOffset = 0)
        : Clip(pOwner, MEDIA_AUDIO, strName, i64Start, i64End, i64StartOffset, i64EndOffset)
    {}
    bool UpdateClip(MediaItem* pMediaItem);
    void SyncStateToDataLayer();
    void SyncStateFromDataLayer();
};

struct TextClip : Clip
{
    static TextClip* CreateInstance(TimeLine* pOwner, const std::string& strText, int64_t i64Start, int64_t i64Length = 0);
    virtual ~TextClip();
    void SetClipDefault(const MediaCore::SubtitleStyle & style);
    void SetClipDefault(const TextClip* clip);
    void SyncClipAttributes();
    void EnableUsingTrackStyle(bool enable);

    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect, bool updated = false) override;
    void DrawTooltips() override;
    int64_t Moving(int64_t diff, int mouse_track) override;
    int64_t Cropping(int64_t diff, int type) override;
    bool ReloadSource(MediaItem* pMediaItem) override;

    static TextClip* CreateInstanceFromJson(const imgui_json::value& j, TimeLine* pOwner);
    imgui_json::value SaveAsJson() override;

    void CreateDataLayer(MediaTrack* pTrack);
    
    std::string mText;
    std::string mFontName;
    ImGui::KeyPointEditor mAttributeKeyPoints;
    bool mTrackStyle {true};
    bool mScaleSettingLink {true};
    float mFontScaleX {1.0f};
    float mFontScaleY {1.0f};
    float mFontSpacing {1.0f};
    float mFontAngleX {0.0f};
    float mFontAngleY {0.0f};
    float mFontAngleZ {0.0f};
    float mFontOutlineWidth {1.0f};
    int mFontAlignment {2};
    bool mFontBold {false};
    bool mFontItalic {false};
    bool mFontUnderLine {false};
    bool mFontStrikeOut {false};
    float mFontPosX {0.0f};
    float mFontPosY {0.0f};
    float mFontOffsetH {0.f};
    float mFontOffsetV {0.f};
    float mFontShadowDepth {0.f};
    ImVec4 mFontPrimaryColor {0, 0, 0, 0};
    ImVec4 mFontOutlineColor {0, 0, 0, 0};
    ImVec4 mFontBackColor {0, 0, 0, 0};
    bool mIsInited {false};
    MediaCore::SubtitleClipHolder mhDataLayerClip;
    void* mTrack {nullptr};

private:
    TextClip(TimeLine* pOwner) : Clip(pOwner, MEDIA_TEXT) {}
    TextClip(TimeLine* pOwner, const std::string& strText, int64_t i64Start, int64_t i64End)
        : Clip(pOwner, MEDIA_TEXT, "", i64Start, i64End, 0, 0), mText(strText)
    {
        mAttributeKeyPoints.SetMin({0, 0, 0, 0});
        mAttributeKeyPoints.SetMax(ImVec4(1, 1, 1, Length()), true);
    }
};

class BluePrintVideoTransition : public MediaCore::VideoTransition
{
public:
    BluePrintVideoTransition(void * handle);
    ~BluePrintVideoTransition();

    MediaCore::VideoTransition::Holder Clone() override;
    void ApplyTo(MediaCore::VideoOverlap* overlap) override { mOverlap = overlap; }
    ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur) override;

    void SetBluePrintFromJson(imgui_json::value& bpJson);
    void SetKeyPoint(ImGui::KeyPointEditor &keypoint) { mKeyPoints = keypoint; };

    imgui_json::value SaveAsJson() const override;

public:
    BluePrint::BluePrintUI* mBp{nullptr};
    ImGui::KeyPointEditor mKeyPoints;

private:
    static int OnBluePrintChange(int type, std::string name, void* handle);
    MediaCore::VideoOverlap* mOverlap;
    std::mutex mBpLock;
    void * mHandle {nullptr};
};

class BluePrintAudioTransition : public MediaCore::AudioTransition
{
public:
    BluePrintAudioTransition(void * handle);
    ~BluePrintAudioTransition();
    void ApplyTo(MediaCore::AudioOverlap* overlap) override { mOverlap = overlap; }
    ImGui::ImMat MixTwoAudioMats(const ImGui::ImMat& amat1, const ImGui::ImMat& amat2, int64_t pos) override;

    void SetBluePrintFromJson(imgui_json::value& bpJson);
    void SetKeyPoint(ImGui::KeyPointEditor &keypoint) { mKeyPoints = keypoint; };

public:
    BluePrint::BluePrintUI* mBp{nullptr};
    ImGui::KeyPointEditor mKeyPoints;

private:
    static int OnBluePrintChange(int type, std::string name, void* handle);
    MediaCore::AudioOverlap* mOverlap;
    std::mutex mBpLock;
    void * mHandle {nullptr};
};

struct BaseEditingClip
{
    void* mHandle               {nullptr};              // main timeline handle
    int64_t mID                 {-1};                   // editing clip ID
    int64_t mMediaID            {-1};                   // editing meida(item) ID
    uint32_t mType              {MEDIA_UNKNOWN};
    int64_t mStart              {0};
    int64_t mEnd                {0};
    int64_t mStartOffset        {0};                    // editing clip start time in media
    int64_t mEndOffset          {0};                    // editing clip end time in media
    int64_t mDuration           {0};
    int64_t mCurrentTime        {-1};
    ImVec2 mViewWndSize         {0, 0};
    bool bSeeking               {false};
    bool bEditingAttribute      {false};                // editing clip attribute mode, unique UI layout

    int64_t firstTime = 0;
    int64_t lastTime = 0;
    int64_t visibleTime = 0;
    float msPixelWidthTarget = -1.f;

    BaseEditingClip(int64_t id, int64_t mid, uint32_t type, int64_t start, int64_t end, int64_t startOffset, int64_t endOffset, void* handle)
        : mID(id), mMediaID(mid), mType(type), mStart(start), mEnd(end), mStartOffset(startOffset), mEndOffset(endOffset), mHandle(handle)
    {}
    virtual ~BaseEditingClip() {};

    Clip * GetClip();
    void UpdateCurrent(bool forward, int64_t currentTime);

    virtual void CalcDisplayParams(int64_t viewWndDur) = 0;
    virtual void UpdateClipRange(Clip* clip) = 0;
    virtual void Save() = 0;
    virtual void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated = false) = 0;
};

struct EditingVideoClip : BaseEditingClip
{
    MediaCore::Snapshot::Generator::Holder mSsGen;
    MediaCore::Snapshot::Viewer::Holder mSsViewer;
    ImVec2 mSnapSize            {0, 0};
    uint32_t mWidth             {0};
    uint32_t mHeight            {0};

    // for image clip
    RenderUtils::ManagedTexture::Holder mhImgTx;
    // for attribute editor
    RenderUtils::ManagedTexture::Holder mhTransformOutputTx;
    RenderUtils::ManagedTexture::Holder mhFilterInputTx;
    RenderUtils::ManagedTexture::Holder mhFilterOutputTx;
    MediaCore::CorrelativeFrame::Phase meOutputFramePhase {MediaCore::CorrelativeFrame::PHASE_AFTER_MIXING};
    MediaCore::CorrelativeFrame::Phase meAttrOutFramePhase {MediaCore::CorrelativeFrame::PHASE_AFTER_MIXING};
    ImGui::ImMat mFilterOutputMat;

    MediaCore::VideoFilter::Holder mhVideoFilter;
    MediaCore::VideoTransformFilter::Holder mhTransformFilter;
    BluePrint::BluePrintUI* mFilterBp {nullptr};
    ImGui::KeyPointEditor* mFilterKp {nullptr};
    ImGui::MaskCreator::Holder mhMaskCreator;
    int64_t mMaskEventId {-1}, mMaskNodeId {-1};
    int mMaskIndex {-1};
    int64_t mMaskEventStart, mMaskEventEnd;

public:
    EditingVideoClip(VideoClip* vidclip);
    virtual ~EditingVideoClip();

    void CalcDisplayParams(int64_t viewWndDur) override;
    void UpdateClipRange(Clip* clip) override;
    void Save() override;
    bool UpdatePreviewTexture(bool blocking = false);
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated = false) override;
    void SelectEditingMask(MEC::Event::Holder hEvent, int64_t nodeId, int maskIndex, ImGui::MaskCreator::Holder hMaskCreator = nullptr);
    void UnselectEditingMask();
};

struct EditingAudioClip : BaseEditingClip
{
    int mAudioChannels  {2}; 
    int mAudioSampleRate {44100};
    MediaCore::Overview::Waveform::Holder mWaveform {nullptr};
    std::vector<ImTextureID> mWaveformTextures;

    BluePrint::BluePrintUI* mFilterBp {nullptr};
    ImGui::KeyPointEditor* mFilterKp {nullptr};

    MediaCore::AudioFilter::Holder mhAudioFilter;

public:
    EditingAudioClip(AudioClip* vidclip);
    virtual ~EditingAudioClip();

    void CalcDisplayParams(int64_t viewWndDur) override;
    void UpdateClipRange(Clip* clip) override;
    void Save() override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated = false) override;
};

struct EditingTextClip : BaseEditingClip
{
    std::string mText;
public:
    EditingTextClip(TextClip* clip);
    virtual ~EditingTextClip();

    void CalcDisplayParams(int64_t viewWndDur) override;
    void UpdateClipRange(Clip* clip) override;
    void Save() override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated = false) override;
public:
    void UpdateClip(Clip* clip);
};

struct BaseEditingOverlap
{
    void* mHandle                   {nullptr};  // main timeline handle
    int64_t mID                     {-1};       // overlap ID
    int64_t mStart                  {0};
    int64_t mEnd                    {0};
    int64_t mDuration               {0};
    int64_t mCurrentTime            {-1};
    ImVec2 mViewWndSize             {0, 0};
    float msPixelWidth              {0};
    bool bSeeking                   {false};
    BaseEditingOverlap(int64_t id, void* handle) { mID = id; mHandle = handle; }
    virtual ~BaseEditingOverlap() {};
    std::pair<int64_t, int64_t> m_StartOffset;

    virtual void Seek(int64_t pos, bool enterSeekingState) = 0;
    virtual void Step(bool forward, int64_t step = 0) = 0;
    virtual bool GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame, bool preview_frame = true) = 0;
    virtual void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated = false) = 0;
    virtual void Save() = 0;
};

struct EditingVideoOverlap : BaseEditingOverlap
{
    VideoClip *mClip1, *mClip2;
    MediaCore::Snapshot::Generator::Holder mSsGen1, mSsGen2;
    MediaCore::Snapshot::Viewer::Holder mViewer1, mViewer2;
    RenderUtils::ManagedTexture::Holder mhImgTx1;
    RenderUtils::ManagedTexture::Holder mhImgTx2;
    ImVec2 mSnapSize{0, 0};

    BluePrintVideoTransition* mTransition{nullptr};

public:
    EditingVideoOverlap(int64_t id, void* handle);
    virtual ~EditingVideoOverlap();

    void Seek(int64_t pos, bool enterSeekingState) override;
    void Step(bool forward, int64_t step = 0) override;
    bool GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame, bool preview_frame = true) override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated = false) override;
    void Save() override;

    void CalcDisplayParams();
};

struct EditingAudioOverlap : BaseEditingOverlap
{
    AudioClip *mClip1, *mClip2;
    BluePrintAudioTransition* mTransition{nullptr};

    std::vector<ImTextureID> mClip1WaveformTextures;
    std::vector<ImTextureID> mClip2WaveformTextures;

public:
    EditingAudioOverlap(int64_t id, void* handle);
    virtual ~EditingAudioOverlap();

    void Seek(int64_t pos, bool enterSeekingState) override;
    void Step(bool forward, int64_t step = 0) override;
    bool GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame, bool preview_frame = true) override { return false; }
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, bool updated = false) override;
    void Save() override;
};

enum EditingType : int
{
    EDITING_UNKNOWN = -1,
    EDITING_CLIP = 0,
    EDITING_TRANSITION,
};

struct EditingItem
{
    uint32_t mMediaType                  {MEDIA_UNKNOWN};
    int mEditorType                      {EDITING_UNKNOWN};
    int mIndex                           {-1};
    bool mIsSelected                     {false};
    BaseEditingClip * mEditingClip       {nullptr};
    BaseEditingOverlap * mEditingOverlap {nullptr};
    ImTextureID mTexture                 {nullptr};
    std::string mName                    {""};
    std::string mTooltip                 {""};
public:
    EditingItem(uint32_t media_type, BaseEditingClip * clip);
    EditingItem(uint32_t media_type, BaseEditingOverlap * overlap);
    virtual ~EditingItem();
};

struct audio_channel_data
{
    ImGui::ImMat m_wave;
    ImGui::ImMat m_fft;
    ImGui::ImMat m_db;
    ImGui::ImMat m_DBShort;
    ImGui::ImMat m_DBLong;
    ImGui::ImMat m_Spectrogram;
    ImTextureID texture_spectrogram {nullptr};
    float m_decibel {0};
    int m_DBMaxIndex {-1};
    ~audio_channel_data() { if (texture_spectrogram) ImGui::ImDestroyTexture(texture_spectrogram); }
};

#define MAX_GAIN 12
#define MIN_GAIN -12
struct audio_band_config
{
    uint32_t centerFreq;  // in Hz
    uint32_t bandWidth;  // in Hz
    int32_t gain;  // in db
};

struct AudioAttribute
{
    std::mutex audio_mutex;
    // meters
    int left_stack {0};                         // audio left meter stack
    int left_count {0};                         // audio left meter count
    int right_stack {0};                        // audio right meter stack
    int right_count {0};                        // audio right meter count

    std::vector<audio_channel_data> channel_data; // audio channel data
    ImGui::ImMat m_audio_vector;
    ImTextureID m_audio_vector_texture {nullptr};
    float mAudioVectorScale  {1};
    int mAudioVectorMode {LISSAJOUS};
    float mAudioSpectrogramOffset {0.0};
    float mAudioSpectrogramLight {1.0};


    // gain setting
    float mAudioGain    {1.0};                  // audio gain, project saved

    // equalizer setting
    bool bEqualizer     {false};                // enable audio equalizer, project saved
    audio_band_config mBandCfg[10];             // timeline audio band equalizer, project saved

    // pan setting
    bool bPan       {false};                    // enable audio pan, project saved
    ImVec2 audio_pan    {0.5, 0.5};             // audio pan, project saved

    // limiter
    bool bLimiter   {false};                    // enable audio limiter, project saved
    float limit     {1};                        // audio limiter, project saved(0.0625-1)
    float limiter_attack    {5};                // audio limiter attack, project saved(0.1-80ms)
    float limiter_release   {50};               // audio limiter release, project saved(1-8000ms)

    // compressor
    bool bCompressor        {false};             // enable audio compressor, project saved
    float compressor_thd    {0.125};             // audio compressor threshold, project saved(0.001-1)
    float compressor_ratio  {2.0};               // audio compressor ratio, project saved(1-20)
    float compressor_knee   {2.82843};           // audio compressor knee, project saved(1-8)
    float compressor_mix    {1};                 // audio compressor mix, project saved(0-1)
    float compressor_attack {20};                // audio compressor attack, project saved(0.01-2000)
    float compressor_release{250};               // audio compressor release, project saved(0.01-9000)
    float compressor_makeup {1};                 // audio compressor makeup, project saved(1-64)
    float compressor_level_sc {1};               // audio compressor sidechain gain, project saved(0.015-64)

    // gate
    bool bGate            {false};               // enable audio gate, project saved
    float gate_thd        {0.125};               // audio gate threshold, project saved(0-1)
    float gate_range      {0.06125};             // audio gate range, project saved(0-1)
    float gate_ratio      {2.0};                 // audio gate ratio, project saved(1-9000)
    float gate_attack     {20};                  // audio gate attack, project saved(0.01-9000)
    float gate_release    {250};                 // audio gate release, project saved(0.01-9000)
    float gate_makeup     {1.0};                 // audio gate makeup, project saved(1-64)
    float gate_knee       {2.82843};             // audio gate knee, project saved(1-8)
};

struct MediaTrack
{
    int64_t mID             {-1};               // track ID, project saved
    uint32_t mType          {MEDIA_UNKNOWN};    // track type, project saved
    std::string mName;                          // track name, project saved
    std::vector<Clip *> m_Clips;                // track clips, project saved(id only)
    std::vector<Overlap *> m_Overlaps;          // track overlaps, project saved(id only)
    void * m_Handle         {nullptr};          // user handle, so far we using it contant timeline struct

    int mTrackHeight {DEFAULT_TRACK_HEIGHT};    // track custom view height, project saved
    int64_t mLinkedTrack    {-1};               // relative track ID, project saved
    bool mExpanded  {false};                    // track is compact view, project saved
    bool mView      {true};                     // track is viewable, project saved
    bool mLocked    {false};                    // track is locked(can't moving or cropping by locked), project saved
    bool mSelected  {false};                    // track is selected, project saved
    
    int mAudioChannels {2};                     // track audio channels, project saved, configured
    int mAudioSampleRate {44100};               // track audio sample rate, project saved, configured
    MediaCore::AudioRender::PcmFormat mAudioFormat {MediaCore::AudioRender::PcmFormat::FLOAT32}; // timeline audio format, project saved, configured
    AudioAttribute mAudioTrackAttribute;        // audio track attribute, project saved

    int64_t mViewWndDur     {0};
    float mPixPerMs         {0};
    MediaCore::SubtitleTrackHolder mMttReader {nullptr};
    bool mTextTrackScaleLink {true};
    MediaTrack(std::string name, uint32_t type, void * handle);
    ~MediaTrack();

    bool DrawTrackControlBar(ImDrawList *draw_list, ImRect rc, bool editable, std::list<imgui_json::value>* pActionList);
    bool CanInsertClip(Clip * clip, int64_t pos);
    void InsertClip(Clip * clip, int64_t pos = 0, bool update = true, std::list<imgui_json::value>* pActionList = nullptr);
    void SelectClip(Clip * clip, bool appand);
    void SelectEditingClip(Clip * clip);
    void SelectEditingOverlap(Overlap * overlap);
    void DeleteClip(int64_t id);
    Clip * FindPrevClip(int64_t id);                // find prev clip in track, if not found then return null
    Clip * FindNextClip(int64_t id);                // find next clip in track, if not found then return null
    Clip * FindClips(int64_t time, int& count);     // find clips at time, count means clip number at time
    void CreateOverlap(int64_t start, int64_t start_clip_id, int64_t end, int64_t end_clip_id, uint32_t type);
    Overlap * FindExistOverlap(int64_t start_clip_id, int64_t end_clip_id);
    
    void CalculateAudioScopeData(ImGui::ImMat& mat_in);
    float GetAudioLevel(int channel);
    void SetAudioLevel(int channel, float level);

    void Update();                                  // update track clip include clip order and overlap area
    static MediaTrack* Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);

    void ConfigViewWindow(int64_t wndDur, float pixPerMs)
    {
        if (mViewWndDur == wndDur && mPixPerMs == pixPerMs)
            return;
        mViewWndDur = wndDur;
        mPixPerMs = pixPerMs;
        for (auto& clip : m_Clips)
        {
            if (!IS_DUMMY(clip->mType)) clip->ConfigViewWindow(wndDur, pixPerMs);
        }
    }
};

struct TimelineCustomDraw
{
    int index;
    ImRect customRect;
    ImRect titleRect;
    ImRect clippingTitleRect;
    ImRect legendRect;
    ImRect clippingRect;
    ImRect legendClippingRect;
};

struct ClipGroup
{
    int64_t mID;
    ImU32 mColor;
    std::vector<int64_t> m_Grouped_Clips;
    ClipGroup(void * handle);
    void Load(const imgui_json::value& value);
    void Save(imgui_json::value& value);
};

typedef int (*TimeLineCallback)(int type, void* handle);
typedef struct TimeLineCallbackFunctions
{
    TimeLineCallback  EditingClip           {nullptr};
    TimeLineCallback  EditingOverlap        {nullptr};
} TimeLineCallbackFunctions;

struct TimeLine
{
#define MAX_VIDEO_CACHE_FRAMES  3
    TimeLine(std::string plugin_path = {});
    ~TimeLine();
    IDGenerator m_IDGenerator;              // Timeline ID generator
    std::vector<MediaItem *> media_items;   // Media Bank, project saved
    std::vector<MediaTrack *> m_Tracks;     // timeline tracks, project saved
    std::vector<Clip *> m_Clips;            // timeline clips, project saved
    std::vector<ClipGroup> m_Groups;        // timeline clip groups, project saved
    std::vector<Overlap *> m_Overlaps;      // timeline clip overlap, project saved
    std::unordered_map<int64_t, MediaCore::Snapshot::Generator::Holder> m_VidSsGenTable;  // Snapshot generator for video media item, provide snapshots for VideoClip
    int64_t mStart   {0};                   // whole timeline start in ms, project saved
    int64_t mEnd     {0};                   // whole timeline end in ms, project saved
    bool m_in_threads {false};

    bool mShowHelpTooltips      {true};     // timeline show help tooltips, project saved, configured
    bool mHardwareCodec         {true};     // timeline Video/Audio decode/encode try to enable HW if available;
    float mPreviewScale {0.5};              // timeline preview video size scale, usually < 1.0, default is 0.5
    int mMaxCachedVideoFrame {MAX_VIDEO_CACHE_FRAMES};  // timeline Media Video Frame cache size, project saved, configured
    float mSnapShotWidth        {60.0};
    RenderUtils::TextureManager::Holder mTxMgr;

    MediaCore::SharedSettings::Holder mhMediaSettings;
    MediaCore::SharedSettings::Holder mhPreviewSettings;
    MediaCore::AudioRender::PcmFormat mAudioRenderFormat {MediaCore::AudioRender::PcmFormat::FLOAT32}; // timeline audio format, project saved, configured
    AudioAttribute mAudioAttribute;         // timeline audio attribute, need save

    BluePrint::BluePrintUI m_BP_UI;         // for node catalog

    // MEC project handling
    MEC::Project::Holder mhProject;
    bool IsProjectDirReady() const
    {
        if (!mhProject || !mhProject->IsOpened())
            return false;
        return SysUtils::IsDirectory(mhProject->GetProjectDir());
    }

    bool AddMediaItem(MediaCore::MediaParser::Holder hParser);
    bool CheckMediaItemImported(const std::string& strPath);

    // sutitle Setting
    std::string mFontName;
    // Output Setting
    std::string mOutputName {"Untitled"};
    std::string mOutputPath {""};
    std::string mVideoCodec {"h264"};
    std::string mAudioCodec {"aac"};
    bool bExportVideo {true};
    bool bExportAudio {true};
    MediaCore::MediaEncoder::Holder mEncoder;

    struct VideoEncoderParams
    {
        std::string codecName;
        std::string imageFormat;
        uint32_t width;
        uint32_t height;
        MediaCore::Ratio frameRate;
        uint64_t bitRate;
        std::vector<MediaCore::MediaEncoder::Option> extraOpts;
    };

    struct AudioEncoderParams
    {
        std::string codecName;
        std::string sampleFormat;
        uint32_t channels;
        uint32_t sampleRate;
        uint64_t bitRate;
        uint32_t samplesPerFrame {1024};
        std::vector<MediaCore::MediaEncoder::Option> extraOpts;
    };

    MediaCore::MultiTrackVideoReader::Holder mEncMtvReader;
    MediaCore::MultiTrackAudioReader::Holder mEncMtaReader;

    bool ConfigEncoder(const std::string& outputPath, VideoEncoderParams& vidEncParams, AudioEncoderParams& audEncParams, std::string& errMsg);
    void StartEncoding();
    void StopEncoding();
    void _EncodeProc();
    // encoding 
    std::thread mEncodingThread;
    bool mIsEncoding {false};
    bool mQuitEncoding {false};
    bool mEncodingInRange {false};
    int64_t mEncodingStart {0};
    int64_t mEncodingEnd {0};
    std::string mEncodeProcErrMsg;
    float mEncodingProgress {0};
    float mEncodingDuration {0};
    std::mutex mEncodingMutex;
    ImGui::ImMat mEncodingVFrame;
    ImGui::ImMat mEncodingAFrame;
    ImTextureID mEncodingPreviewTexture {nullptr};  // encoding preview texture

    void CalculateAudioScopeData(ImGui::ImMat& mat);

    int64_t attract_docking_pixels {10};    // clip attract docking sucking in pixels range, pulling range is 1/5
    int64_t mConnectedPoints = -1;

    int64_t mCurrentTime = 0;
    int64_t mFrameIndex = 0;
    int64_t firstTime = 0;
    int64_t lastTime = 0;
    int64_t visibleTime = 0;
    int64_t mark_in = -1;                   // mark in point, -1 means no mark in point or mark in point is start of timeline if mark out isn't -1
    int64_t mark_out = -1;                  // mark out point, -1 means no mark out point or mark out point is end of timeline if mark in isn't -1
    float msPixelWidthTarget = 0.1f;

    bool bSeeking = false;
    bool bPreviewZoom = false;
    bool bEditingText = false;              // indicate UI at Text editing page
    bool bPreviewing = false;               // indicate UI at Preview page 
    bool bLoop = false;                     // project saved
    bool bCompare = false;                  // project saved
    bool bTransitionOutputPreview = true;   // project saved
    bool bSelectLinked = true;              // project saved
    bool bMovingAttract = true;             // project saved

    // Add By Jimmy: Start
    uint32_t mSortMethod {0};
    uint32_t mFilterMethod {0};
    std::vector<MediaItem *> filter_media_items;
    std::vector<MediaItem *> search_media_items;
    // Add By Jimmy: End

    std::vector<EditingItem*> mEditingItems;
    int mSelectedItem                   {-1};
    int FindEditingItem(int type, int64_t id);

    MediaCore::MultiTrackVideoReader::Holder mMtvReader;
    MediaCore::MultiTrackAudioReader::Holder mMtaReader;
    int64_t mPreviewResumePos               {0};
    bool mIsPreviewNeedUpdate               {false};
    bool mIsPreviewPlaying                  {false};
    bool mIsPreviewForward                  {true};
    bool mIsStepMode                        {false};
    int64_t mLastFrameTime                  {-1};
    using PlayerClock = std::chrono::steady_clock;
    PlayerClock::time_point mPlayTriggerTp;
    std::unordered_set<int64_t> mNeedUpdateTrackIds;

    bool mIsCutting {false};
    std::list<imgui_json::value> mOngoingActions;
    std::list<imgui_json::value> mUiActions;
    void PrintActionList(const std::string& title, const std::list<imgui_json::value>& actionList);
    void PrintActionList(const std::string& title, const imgui_json::array& actionList);
    void PerformUiActions();
    void PerformVideoAction(imgui_json::value& action);
    void PerformAudioAction(imgui_json::value& action);
    void PerformImageAction(imgui_json::value& action);
    void PerformTextAction(imgui_json::value& action);

    class SimplePcmStream : public MediaCore::AudioRender::ByteStream
    {
    public:
        SimplePcmStream(TimeLine* owner) : m_owner(owner) {}
        void SetAudioReader(MediaCore::MultiTrackAudioReader::Holder areader) { m_areader = areader; }
        uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override;
        void Flush() override;
        bool GetTimestampMs(int64_t& ts) override
        {
            if (m_tsValid)
            {
                ts = m_timestampMs;
                return true;
            }
            else
                return false;
        }

    private:
        TimeLine* m_owner;
        MediaCore::MultiTrackAudioReader::Holder m_areader;
        ImGui::ImMat m_amat;
        uint32_t m_readPosInAmat{0};
        bool m_tsValid{false};
        int64_t m_timestampMs{0};
        std::mutex m_amatLock;
    };
    SimplePcmStream mPcmStream;

    std::mutex mTrackLock;                  // timeline track mutex
    
    // BP CallBacks
    bool mIsBluePrintChanged {false};
    static int OnBluePrintChange(int type, std::string name, void* handle);
    // This callback can only be assigned to a EventStackFilter, since it will interpret the 'handle' as a 'MEC::EventStackFilterContext' pointer
    static int OnVideoEventStackFilterBpChanged(int type, std::string name, void* handle);
    static int OnAudioEventStackFilterBpChanged(int type, std::string name, void* handle);

    std::vector<MediaCore::CorrelativeFrame> maCurrFrames;
    ImGui::ImMat mPreviewMat;
    RenderUtils::ManagedTexture::Holder mhPreviewTx;

    ImTextureID mVideoTransitionInputFirstTexture {nullptr};    // clip video transition first input texture
    ImTextureID mVideoTransitionInputSecondTexture {nullptr};   // clip video transition second input texture
    ImTextureID mVideoTransitionOutputTexture {nullptr};        // clip video transition output texture

    TimeLineCallbackFunctions  m_CallBacks;

    uint32_t GetPreviewWidth() { return mhPreviewSettings->VideoOutWidth(); }
    uint32_t GetPreviewHeight() { return mhPreviewSettings->VideoOutHeight(); }
    int64_t GetStart() const { return mStart; }
    int64_t GetEnd() const { return mEnd; }
    void SetStart(int64_t pos) { mStart = pos; }
    void SetEnd(int64_t pos) { mEnd = pos; }
    size_t GetCustomHeight(int index) { return (index < m_Tracks.size() && m_Tracks[index]->mExpanded) ? m_Tracks[index]->mTrackHeight : 0; }
    void Update();
    void UpdateRange();
    int64_t AlignTime(int64_t time, int mode = 0);  // mode: 0=floor, 1=round, 2=ceil
    int64_t AlignTimeToPrevFrame(int64_t time);
    int64_t AlignTimeToNextFrame(int64_t time);
    std::pair<int64_t, int64_t> AlignClipRange(const std::pair<int64_t, int64_t>& startAndLength);  // (start, length) => aligned (start, end)

    int GetTrackCount() const { return (int)m_Tracks.size(); }
    int GetTrackCount(uint32_t type);
    int GetEmptyTrackCount();
    int NewTrack(const std::string& name, uint32_t type, bool expand, int64_t id = -1, int64_t afterUiTrkId = -1, std::list<imgui_json::value>* pActionList = nullptr);
    bool RestoreTrack(imgui_json::value& action);
    int64_t DeleteTrack(int index, std::list<imgui_json::value>* pActionList);
    void SelectTrack(int index);
    void MovingTrack(int index, int dst_index, std::list<imgui_json::value>* pActionList);

    void MovingClip(int64_t id, int from_track_index, int to_track_index);
    bool DeleteClip(int64_t id, std::list<imgui_json::value>* pActionList);
    void DeleteOverlap(int64_t id);

    void DoubleClick(int index, int64_t time);
    void Click(int index, int64_t time);

    void CustomDraw(
            int index, ImDrawList *draw_list, const ImRect &view_rc, const ImRect &rc,
            const ImRect &titleRect, const ImRect &clippingTitleRect, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect,
            int64_t mouse_time, bool is_moving, bool enable_select, bool is_updated, std::list<imgui_json::value>* pActionList);
    
    std::vector<MediaCore::CorrelativeFrame> GetPreviewFrame(bool blocking = false);
    bool UpdatePreviewTexture(bool blocking = false);
    float GetAudioLevel(int channel);
    void SetAudioLevel(int channel, float level);

    void Play(bool play, bool forward = true);
    void Seek(int64_t msPos, bool enterSeekingState = false);
    void StopSeek();
    void Step(bool forward = true);
    void Loop(bool loop);
    void ToStart();
    void ToEnd();
    void UpdateCurrent();
    void RefreshPreview(bool updateDuration = true);
    void RefreshTrackView(const std::unordered_set<int64_t>& trackIds);
    int64_t ValidDuration();

    MediaCore::AudioRender* mAudioRender {nullptr};                // audio render(SDL)

    MediaItem* FindMediaItemByName(std::string name);   // Find media from bank by name
    MediaItem* FindMediaItemByID(int64_t id);           // Find media from bank by ID

    MediaItem* isURLInMediaBank(std::string url);       // check media url is in media bank
    bool isURLInTimeline(std::string url);              // check media url is in timeline
    // Add By Jimmy: Start
    ImGuiTextFilter mTextSearchFilter;                  // Media Bank, search
    uint32_t mCurrViewCount = 0;
    void SortMediaItemByID();                           // Media Bank, sort
    void SortMediaItemByName();
    void SortMediaItemByType();
    void FilterMediaItemByType(uint32_t mediaType);     // Media Bank, filter
    MEC::MediaPlayer * mMediaPlayer;                    // Media Player
    // Add By Jimmy: End

    MediaTrack * FindTrackByID(int64_t id);             // Find track by ID
    MediaTrack * FindTrackByClipID(int64_t id);         // Find track by clip ID
    MediaTrack * FindTrackByName(std::string name);     // Find track by clip ID
    MediaTrack * FindEmptyTrackByType(uint32_t type);   // Find first empty track by type
    int FindTrackIndexByClipID(int64_t id);             // Find track by clip ID
    Clip * FindClipByID(int64_t id);                    // Find clip with clip ID
    Overlap * FindOverlapByID(int64_t id);              // Find overlap with overlap ID
    Overlap * FindEditingOverlap();                     // Find overlap which is editing
    int GetSelectedClipCount();                         // Get current selected clip count
    int64_t NextClipStart(Clip * clip);                 // Get next clip start pos by clip, if don't have next clip, then return -1
    int64_t NextClipStart(int64_t pos);                 // Get next clip start pos by time, if don't have next clip, then return -1
    int64_t NewGroup(Clip * clip, int64_t id = -1, ImU32 color = 0, std::list<imgui_json::value>* pActionList = nullptr); // Create a new group with clip ID
    int64_t RestoreGroup(const imgui_json::value& groupJson);
    void AddClipIntoGroup(Clip * clip, int64_t group_id, std::list<imgui_json::value>* pActionList = nullptr); // Insert clip into group
    void DeleteClipFromGroup(Clip *clip, int64_t group_id, std::list<imgui_json::value>* pActionList = nullptr); // Delete clip from group
    ImU32 GetGroupColor(int64_t group_id);              // Get Group color by id
    int Load(const imgui_json::value& value);
    void Save(imgui_json::value& value);

    void ConfigureDataLayer();
    void SyncDataLayer(bool forceRefresh = false);
    MediaCore::Snapshot::Generator::Holder GetSnapshotGenerator(int64_t mediaItemId);
    void ConfigSnapshotWindow(int64_t viewWndDur);
    MatUtils::Size2i CalcPreviewSize(const MatUtils::Size2i& videoSize, float previewScale);
    void UpdateVideoSettings(MediaCore::SharedSettings::Holder hSettings, float previewScale);
    void UpdateAudioSettings(MediaCore::SharedSettings::Holder hSettings, MediaCore::AudioRender::PcmFormat pcmFormat);

    std::list<imgui_json::value> mHistoryRecords;
    std::list<imgui_json::value>::iterator mRecordIter;
    void AddNewRecord(imgui_json::value& record);
    bool UndoOneRecord();
    bool RedoOneRecord();
    int64_t AddNewClip(const imgui_json::value& clip_json, int64_t track_id, std::list<imgui_json::value>* pActionList = nullptr);
    int64_t AddNewClip(int64_t media_id, uint32_t media_type, int64_t track_id, int64_t start, int64_t start_offset, int64_t end, int64_t end_offset, int64_t group_id, int64_t clip_id = -1, std::list<imgui_json::value>* pActionList = nullptr);
};

bool DrawTimeLine(TimeLine *timeline, bool *expanded, bool& need_save, bool editable = true);
bool DrawClipTimeLine(TimeLine* main_timeline, BaseEditingClip * editingClip, int64_t CurrentTime, int header_height, int custom_height, bool& show_BP, bool& changed);
bool DrawOverlapTimeLine(BaseEditingOverlap * overlap, int64_t CurrentTime, int header_height, int custom_height);
} // namespace MediaTimeline
