#pragma once
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_json.h>
#include "MediaOverview.h"
#include "MediaSnapshot.h"
#include "MediaReader.h"
#include "AudioRender.hpp"
#include "UI.h"
#include <thread>
#include <string>
#include <vector>
#include <list>

#define ICON_MEDIA_BANK     u8"\ue907"
#define ICON_MEDIA_TRANS    u8"\ue927"
#define ICON_MEDIA_FILTERS  u8"\ue663"
#define ICON_MEDIA_OUTPUT   u8"\uf197"
#define ICON_MEDIA_PREVIEW  u8"\ue04a"
#define ICON_MEDIA_VIDEO    u8"\ue04b"
#define ICON_MEDIA_AUDIO    u8"\ue050"
#define ICON_MEDIA_DIAGNOSIS u8"\uf0f0"
#define ICON_SLIDER_MINIMUM u8"\uf424"
#define ICON_SLIDER_MAXIMUM u8"\uf422"
#define ICON_VIEW           u8"\uf06e"
#define ICON_VIEW_DISABLE   u8"\uf070"
#define ICON_ENABLE         u8"\uf205"
#define ICON_DISABLE        u8"\uf204"
#define ICON_ZOOM_IN        u8"\uf00e"
#define ICON_ZOOM_OUT       u8"\uf010"
#define ICON_ITEM_CUT       u8"\ue14e"
#define ICON_SPEAKER        u8"\ue050"
#define ICON_SPEAKER_MUTE   u8"\ue04f"
#define ICON_FILTER         u8"\uf331"
#define ICON_MUSIC          u8"\ue3a1"
#define ICON_MUSIC_DISABLE  u8"\ue440"
#define ICON_MUSIC_RECT     u8"\ue030"
#define ICON_MAGIC_3        u8"\ue663"
#define ICON_MAGIC_1        u8"\ue664"
#define ICON_MAGIC_DISABlE  u8"\ue665"
#define ICON_HDR            u8"\ue3ee"
#define ICON_HDR_DISABLE    u8"\ue3ed"
#define ICON_PALETTE        u8"\uf53f"
#define ICON_STRAW          u8"\ue3b8"
#define ICON_CROP           u8"\uf125"
#define ICON_ROTATE         u8"\ue437"
#define ICON_LOCKED         u8"\uf023"
#define ICON_UNLOCK         u8"\uf09c"
#define ICON_TRASH          u8"\uf014"
#define ICON_CLONE          u8"\uf2d2"
#define ICON_ADD            u8"\uf067"
#define ICON_ALIGN_START    u8"\ue419"
#define ICON_CUT            u8"\ue00d"
#define ICON_REMOVE_CUT     u8"\ue011"
#define ICON_CUTTING        u8"\uf0c4"
#define ICON_MOVING         u8"\ue41f"
#define ICON_TRANS          u8"\ue882"
#define ICON_BANK           u8"\uf1b3"
#define ICON_BLUE_PRINT     u8"\uf55B"
#define ICON_BRAIN          u8"\uf5dc"
#define ICON_NEW_PROJECT    u8"\uf271"
#define ICON_OPEN_PROJECT   u8"\uf115"
#define ICON_SAVE_PROJECT   u8"\uf0c7"

#define ICON_PLAY           u8"\uf04b"
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

#define ICON_CROPED         u8"\ue3e8"
#define ICON_SCALED         u8"\ue433"

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

#define ICON_STEREO         u8"\uf58f"
#define ICON_MONO           u8"\uf590"

#define COL_FRAME_RECT      IM_COL32( 16,  16,  96, 255)
#define COL_LIGHT_BLUR      IM_COL32( 16, 128, 255, 255)
#define COL_CANVAS_BG       IM_COL32( 36,  36,  36, 255)
#define COL_LEGEND_BG       IM_COL32( 33,  33,  38, 255)
#define COL_MARK            IM_COL32(255, 255, 255, 255)
#define COL_MARK_HALF       IM_COL32(128, 128, 128, 255)
#define COL_RULE_TEXT       IM_COL32(224, 224, 224, 255)
#define COL_SLOT_DEFAULT    IM_COL32( 80,  80, 100, 255)
#define COL_SLOT_ODD        IM_COL32( 58,  58,  58, 255)
#define COL_SLOT_EVEN       IM_COL32( 64,  64,  64, 255)
#define COL_SLOT_SELECTED   IM_COL32(255,  64,  64, 255)
#define COL_SLOT_V_LINE     IM_COL32( 96,  96,  96,  48)
#define COL_SLIDER_BG       IM_COL32( 32,  32,  64, 255)
#define COL_SLIDER_IN       IM_COL32( 96,  96,  96, 255)
#define COL_SLIDER_MOVING   IM_COL32( 80,  80,  80, 255)
#define COL_SLIDER_HANDLE   IM_COL32(112, 112, 112, 255)
#define COL_SLIDER_SIZING   IM_COL32(170, 170, 170, 255)
#define COL_CURSOR_ARROW    IM_COL32(  0, 255,   0, 255)
#define COL_CURSOR_TEXT_BG  IM_COL32(  0, 128,   0, 144)
#define COL_CURSOR_TEXT     IM_COL32(  0, 255,   0, 255)
#define COL_DARK_ONE        IM_COL32( 33,  33,  38, 255)
#define COL_DARK_TWO        IM_COL32( 40,  40,  46, 255)
#define COL_DARK_PANEL      IM_COL32( 48,  48,  54, 255)
#define COL_DEEP_DARK       IM_COL32( 23,  24,  26, 255)

#define HALF_COLOR(c)       (c & 0xFFFFFF) | 0x40000000;
#define TIMELINE_OVER_LENGTH    5000        // add 5 seconds end of timeline
namespace MediaTimeline
{
enum MEDIA_TYPE : int
{
    MEDIA_UNKNOWN = -1,
    MEDIA_VIDEO = 0,
    MEDIA_AUDIO = 1,
    MEDIA_PICTURE = 2,
    MEDIA_TEXT = 3,
    // ...
};

struct MediaItem
{
    int64_t mID;                            // media ID
    std::string mName;
    std::string mPath;
    int64_t mStart   {0};                   // whole Media start in ms
    int64_t mEnd   {0};                     // whole Media end in ms
    MediaOverview * mMediaOverview;
    MEDIA_TYPE mMediaType {MEDIA_UNKNOWN};
    std::vector<ImTextureID> mMediaThumbnail;
    MediaItem(const std::string& name, const std::string& path, MEDIA_TYPE type);
    ~MediaItem();
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

struct Clip
{
    int64_t mID                 {-1};               // clip ID
    int64_t mMediaID            {-1};               // clip media ID in media bank
    int64_t mGroupID            {-1};               // Group ID clip belong
    MEDIA_TYPE mType            {MEDIA_UNKNOWN};    // clip type
    std::string mName;                              // clip name ?
    std::string mPath;                              // clip media path
    int64_t mStart              {0};                // clip start time in timeline
    int64_t mEnd                {0};                // clip end time in timeline
    int64_t mStartOffset        {0};                // clip start time in media
    int64_t mEndOffset          {0};                // clip end time in media
    bool bSeeking               {false};
    bool bSelected              {false};
    std::mutex mLock;                               // clip mutex
    void * mHandle              {nullptr};          // clip belong to timeline 
    MediaOverview * mOverview   {nullptr};          // clip media overview
    MediaReader* mMediaReader   {nullptr};          // clip media reader
    imgui_json::value mFilterBP;

    Clip(int64_t start, int64_t end, int64_t id, MediaOverview * overview, void * handle);
    virtual ~Clip();

    int64_t Moving(int64_t diff);
    int64_t Cropping(int64_t diff, int type);
    bool isLinkedWith(Clip * clip);
    
    virtual void UpdateSnapshot() = 0;
    virtual void Seek() = 0;
    virtual void Step(bool forward, int64_t step) = 0;
    static void Load(Clip * clip, const imgui_json::value& value);
    virtual void Save(imgui_json::value& value) = 0;
};

struct VideoClip : Clip
{
    MediaSnapshot* mSnapshot {nullptr};                 // clip snapshot handle
    std::vector<VideoSnapshotInfo> mVideoSnapshotInfos; // clip snapshots info, with all croped range
    std::vector<Snapshot> mVideoSnapshots;              // clip snapshots, including texture and timestamp info
    //int mFrameCount         {0};                        // total snapshot number in clip range
    //float mSnapshotWidth    {0};
    //ImTextureID mFilterInputTexture {nullptr};  // clip filter input texture
    //ImTextureID mFilterOutputTexture {nullptr};  // clip filter output texture
    MediaInfo::Ratio mClipFrameRate {25, 1};// clip Frame rate

    VideoClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle);
    ~VideoClip();

    void UpdateSnapshot();
    void Seek();
    void Step(bool forward, int64_t step);

    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);
};

struct AudioClip : Clip
{
    int mAudioChannels  {2};                // clip audio channels
    int mAudioSampleRate {44100};           // clip audio sample rate
    AudioRender::PcmFormat mAudioFormat {AudioRender::PcmFormat::FLOAT32}; // clip audio type
    MediaOverview::WaveformHolder mWaveform {nullptr};  // clip audio snapshot

    AudioClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle);
    ~AudioClip();

    void UpdateSnapshot();
    void Seek();
    void Step(bool forward, int64_t step);
    bool GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame);
    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);
};

struct ImageClip : Clip
{
    int mWidth  {0};
    int mHeight  {0};
    int mColorFormat    {0};

    ImageClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle);
    ~ImageClip();

    void UpdateSnapshot();
    void Seek();
    void Step(bool forward, int64_t step);
    bool GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame);
    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);
};

struct TextClip : Clip
{
    TextClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle);
    ~TextClip();

    void UpdateSnapshot();
    void Seek();
    void Step(bool forward, int64_t step);
    bool GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame);
    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);
};


struct MediaTrack
{
    int64_t mID             {-1};               // track ID
    MEDIA_TYPE mType        {MEDIA_UNKNOWN};    // track type
    std::string mName;                          // track name
    std::vector<Clip *> m_Clips;                // track clips
    void * m_Handle         {nullptr};          // user handle

    int mTrackHeight {60};                      // track custom view height
    int64_t mLinkedTrack    {-1};               // relative track ID
    bool mExpanded  {false};                    // track is compact view or not
    bool mView      {true};                     // track is viewable or not
    bool mLocked    {false};                    // track is locked or not(can't moving or cropping by locked)
    bool mSelected  {false};                    // track is selected

    MediaTrack(std::string name, MEDIA_TYPE type, void * handle);
    ~MediaTrack();

    bool DrawTrackControlBar(ImDrawList *draw_list, ImRect rc);
    void InsertClip(Clip * clip, int64_t pos = 0);
    void PushBackClip(Clip * clip);
    void SelectClip(Clip * clip, bool appand);
    static inline bool CompareClip(Clip* a, Clip* b) { return a->mStart < b->mStart; }
    Clip * FindPrevClip(int64_t id);            // find prev clip in track, if not found then return null
    Clip * FindNextClip(int64_t id);            // find next clip in track, if not found then return null
    void Update();                              // update track clip include clip order and overlap area
    static MediaTrack* Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);
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
    std::vector<int64_t> m_Grouped_Clips;
    ClipGroup() { mID = ImGui::get_current_time_usec(); }
    void Load(const imgui_json::value& value);
    void Save(imgui_json::value& value);
};

struct TimeLine
{
    TimeLine();
    ~TimeLine();

    std::vector<MediaTrack *> m_Tracks;     // timeline tracks
    std::vector<Clip *> m_Clips;            // timeline clips
    std::vector<ClipGroup> m_Groups;        // timeline clip groups
    int64_t mStart   {0};                   // whole timeline start in ms
    int64_t mEnd     {0};                   // whole timeline end in ms

    int mWidth  {1920};                     // timeline Media Width
    int mHeight {1080};                     // timeline Media Height
    MediaInfo::Ratio mFrameRate {25, 1};    // timeline Media Frame rate

    int mAudioChannels {2};                 // timeline audio channels
    int mAudioSampleRate {44100};           // timeline audio sample rate
    AudioRender::PcmFormat mAudioFormat {AudioRender::PcmFormat::FLOAT32}; // timeline audio format
    std::vector<int> mAudioLevel;           // timeline audio levels

    int64_t currentTime = 0;
    int64_t firstTime = 0;
    int64_t lastTime = 0;
    int64_t visibleTime = 0;
    float msPixelWidthTarget = 0.1f;

    bool bPlay = false;
    bool bSeeking = false;

    bool bForward = true;                   // save in project
    bool bLoop = false;                     // save in project
    bool bSelectLinked = true;              // save in project
    
    // BP CallBacks
    static int OnBluePrintChange(int type, std::string name, void* handle);

    BluePrint::BluePrintUI * mVideoFilterBluePrint {nullptr};
    std::mutex mVideoFilterBluePrintLock;   // Video Filter BluePrint mutex
    bool mVideoFilterNeedUpdate {false};

    BluePrint::BluePrintUI * mAudioFilterBluePrint {nullptr};
    std::mutex mAudioFilterBluePrintLock;   // Audio Filter BluePrint mutex
    bool mAudioFilterNeedUpdate {false};

    BluePrint::BluePrintUI * mVideoFusionBluePrint {nullptr};
    std::mutex mVideoFusionBluePrintLock;   // Video Fusion BluePrint mutex
    bool mVideoFusionNeedUpdate {false};

    std::mutex mFrameLock;                      // timeline frame mutex
    std::list<ImGui::ImMat> mFrame;             // timeline output frame
    ImTextureID mMainPreviewTexture {nullptr};  // main preview texture

    int64_t GetStart() const { return mStart; }
    int64_t GetEnd() const { return mEnd; }
    void SetStart(int64_t pos) { mStart = pos; }
    void SetEnd(int64_t pos) { mEnd = pos; }
    size_t GetCustomHeight(int index) { return (index < m_Tracks.size() && m_Tracks[index]->mExpanded) ? m_Tracks[index]->mTrackHeight : 0; }
    void Updata();
    void AlignTime(int64_t& time);

    int GetTrackCount() const { return (int)m_Tracks.size(); }
    int GetTrackCount(MEDIA_TYPE type) const;
    void DeleteTrack(int index);
    void SelectTrack(int index);
    void DeleteClip(int64_t id);
    void DoubleClick(int index) { m_Tracks[index]->mExpanded = !m_Tracks[index]->mExpanded; }
    void Click(int index);

    void CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &titleRect, const ImRect &clippingTitleRect, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, int64_t viewStartTime, int64_t visibleTime, float pixelWidth, bool need_update);
    
    ImGui::ImMat GetPreviewFrame();
    int GetAudioLevel(int channel);

    void Play(bool play, bool forward = true);
    void Step(bool forward = true);
    void Loop(bool loop);
    void ToStart();
    void ToEnd();

    AudioRender* mAudioRender {nullptr};                // audio render(SDL)

    std::vector<MediaItem *> media_items;               // Media Bank
    MediaItem* FindMediaItemByName(std::string name);   // Find media from bank by name
    MediaItem* FindMediaItemByID(int64_t id);           // Find media from bank by ID
    MediaTrack * FindTrackByID(int64_t id);             // Find track by ID
    MediaTrack * FindTrackByClipID(int64_t id);         // Find track by clip ID
    Clip * FindClipByID(int64_t id);                    // Find clip info with clip ID
    int64_t NextClipStart(Clip * clip);                 // Get next clip start pos by clip, if don't have next clip, then return -1
    int64_t NextClipStart(int64_t pos);                 // Get next clip start pos by time, if don't have next clip, then return -1
    int64_t NewGroup(Clip * clip);                      // Create a new group with clip ID
    void AddClipIntoGroup(Clip * clip, int64_t group_id); // Insert clip into group
    void DeleteClipFromGroup(Clip *clip, int64_t group_id); // Delete clip from group
    ClipGroup GetGroupByID(int64_t group_id);          // Get Group info by ID
    int Load(const imgui_json::value& value);
    void Save(imgui_json::value& value);
};

bool DrawTimeLine(TimeLine *timeline, bool *expanded);

} // namespace MediaTimeline
