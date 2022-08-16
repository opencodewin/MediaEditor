#pragma once
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include "MediaOverview.h"
#include "SnapshotGenerator.h"
#include "MediaReader.h"
#include "MultiTrackVideoReader.h"
#include "MultiTrackAudioReader.h"
#include "MediaEncoder.h"
#include "AudioRender.hpp"
#include "SubtitleTrack.h"
#include "UI.h"
#include <thread>
#include <string>
#include <vector>
#include <list>
#include <chrono>

#define ICON_MEDIA_TIMELINE u8"\uf538"
#define ICON_MEDIA_BANK     u8"\ue907"
#define ICON_MEDIA_TRANS    u8"\ue927"
#define ICON_MEDIA_FILTERS  u8"\ue663"
#define ICON_MEDIA_OUTPUT   u8"\uf197"
#define ICON_MEDIA_PREVIEW  u8"\ue04a"
#define ICON_MEDIA_VIDEO    u8"\ue04b"
#define ICON_MEDIA_AUDIO    u8"\ue050"
#define ICON_MEDIA_WAVE     u8"\ue495"
#define ICON_MEDIA_IMAGE    u8"\ue3f4"
#define ICON_MEDIA_TEXT     u8"\ue8e2"
#define ICON_MEDIA_DIAGNOSIS u8"\uf551"
#define ICON_MEDIA_DELETE   u8"\ue92b"
#define ICON_MEDIA_DELETE_CLIP   u8"\ue92e"
#define ICON_MEDIA_GROUP    u8"\ue533"
#define ICON_MEDIA_UNGROUP  u8"\ue552"
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
#define ICON_CROPPING_LEFT  u8"\ue045"
#define ICON_CROPPING_RIGHT u8"\ue044"
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
#define ICON_CLIP_START     u8"\uf090"
#define ICON_CLIP_END       u8"\uf08b"
#define ICON_RETURN_DEFAULT u8"\ue042"

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
#define ICON_UI_DEBUG       u8"\uf085"

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
#define ICON_VETCTOR        u8"\ue49f"
#define ICON_AUDIOVECTOR    u8"\uf20e"
#define ICON_WAVE           u8"\ue4ad"
#define ICON_FFT            u8"\ue4e8"
#define ICON_DB             u8"\ue451"
#define ICON_DB_LEVEL       u8"\ue4a9"
#define ICON_SPECTROGRAM    u8"\ue4a0"
#define ICON_DRAWING_PIN    u8"\uf08d"
#define ICON_EXPANMD        u8"\uf0b2"

#define ICON_SETTING_LINK   u8"\uf0c1"
#define ICON_SETTING_UNLINK u8"\uf127"

#define COL_FRAME_RECT      IM_COL32( 16,  16,  96, 255)
#define COL_LIGHT_BLUR      IM_COL32( 16, 128, 255, 255)
#define COL_CANVAS_BG       IM_COL32( 36,  36,  36, 255)
#define COL_LEGEND_BG       IM_COL32( 33,  33,  38, 255)
#define COL_PANEL_BG        IM_COL32( 36,  36,  40, 255)
#define COL_MARK            IM_COL32(255, 255, 255, 255)
#define COL_MARK_HALF       IM_COL32(128, 128, 128, 255)
#define COL_RULE_TEXT       IM_COL32(224, 224, 224, 255)
#define COL_SLOT_DEFAULT    IM_COL32( 80,  80, 100, 255)
#define COL_SLOT_ODD        IM_COL32( 58,  58,  58, 255)
#define COL_SLOT_EVEN       IM_COL32( 64,  64,  64, 255)
#define COL_SLOT_SELECTED   IM_COL32(255,  64,  64, 255)
#define COL_SLOT_V_LINE     IM_COL32( 32,  32,  32,  96)
#define COL_SLIDER_BG       IM_COL32( 32,  32,  48, 255)
#define COL_SLIDER_IN       IM_COL32(192, 192, 192, 255)
#define COL_SLIDER_MOVING   IM_COL32(144, 144, 144, 255)
#define COL_SLIDER_HANDLE   IM_COL32(112, 112, 112, 255)
#define COL_SLIDER_SIZING   IM_COL32(170, 170, 170, 255)
#define COL_CURSOR_ARROW    IM_COL32(  0, 255,   0, 255)
#define COL_CURSOR_TEXT_BG  IM_COL32(  0, 128,   0, 144)
#define COL_CURSOR_TEXT     IM_COL32(  0, 255,   0, 255)
#define COL_DARK_ONE        IM_COL32( 33,  33,  38, 255)
#define COL_DARK_TWO        IM_COL32( 40,  40,  46, 255)
#define COL_DARK_PANEL      IM_COL32( 48,  48,  54, 255)
#define COL_DEEP_DARK       IM_COL32( 23,  24,  26, 255)
#define COL_BLACK_DARK      IM_COL32( 16,  16,  16, 255)
#define COL_GRATICULE_DARK  IM_COL32(128,  96,   0, 128)
#define COL_GRATICULE       IM_COL32(255, 196,   0, 128)
#define COL_GRATICULE_HALF  IM_COL32(255, 196,   0,  64)
#define COL_GRAY_GRATICULE  IM_COL32( 96,  96,  96, 128)
#define COL_MARK_BAR        IM_COL32(128, 128, 128, 170)
#define COL_MARK_DOT        IM_COL32(170, 170, 170, 224)
#define COL_MARK_DOT_LIGHT  IM_COL32(255, 255, 255, 224)

#define HALF_COLOR(c)       (c & 0xFFFFFF) | 0x40000000;
#define TIMELINE_OVER_LENGTH    5000        // add 5 seconds end of timeline

namespace MediaTimeline
{
#define DEFAULT_TRACK_HEIGHT        0
#define DEFAULT_VIDEO_TRACK_HEIGHT  40
#define DEFAULT_AUDIO_TRACK_HEIGHT  20
#define DEFAULT_IMAGE_TRACK_HEIGHT  30
#define DEFAULT_TEXT_TRACK_HEIGHT   20

enum MEDIA_TYPE : int
{
    MEDIA_UNKNOWN = -1,
    MEDIA_VIDEO = 0,
    MEDIA_AUDIO = 1,
    MEDIA_PICTURE = 2,
    MEDIA_TEXT = 3,
    // ...
};

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
    int64_t mStart  {0};                    // whole Media start in ms
    int64_t mEnd    {0};                    // whole Media end in ms
    MediaOverview * mMediaOverview;
    MEDIA_TYPE mMediaType {MEDIA_UNKNOWN};
    std::vector<ImTextureID> mMediaThumbnail;
    MediaItem(const std::string& name, const std::string& path, MEDIA_TYPE type, void* handle);
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

struct Overlap
{
    int64_t mID                     {-1};       // overlap ID, project saved
    MEDIA_TYPE mType                {MEDIA_UNKNOWN};
    int64_t mStart                  {0};        // overlap start time at timeline, project saved
    int64_t mEnd                    {0};        // overlap end time at timeline, project saved
    int64_t mCurrent                {0};        // overlap current time, project saved
    bool bPlay                      {false};    // overlap play status
    bool bForward                   {true};     // overlap play direction
    bool bSeeking                   {false};    // overlap is seeking
    bool bEditing                   {false};    // overlap is editing, project saved
    std::pair<int64_t, int64_t>     m_Clip;     // overlaped clip's pair, project saved
    imgui_json::value mFusionBP;                // overlap transion blueprint, project saved
    void * mHandle                  {nullptr};  // overlap belong to timeline 
    Overlap(int64_t start, int64_t end, int64_t clip_first, int64_t clip_second, MEDIA_TYPE type, void* handle);
    ~Overlap();

    bool IsOverlapValid();
    void Update(int64_t start, int64_t start_clip_id, int64_t end, int64_t end_clip_id);
    void Seek();
    static Overlap * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value);
};

struct Clip
{
    int64_t mID                 {-1};               // clip ID, project saved
    int64_t mMediaID            {-1};               // clip media ID in media bank, project saved
    int64_t mGroupID            {-1};               // Group ID clip belong, project saved
    MEDIA_TYPE mType            {MEDIA_UNKNOWN};    // clip type, project saved
    std::string mName;                              // clip name, project saved
    std::string mPath;                              // clip media path, project saved
    int64_t mStart              {0};                // clip start time in timeline, project saved
    int64_t mEnd                {0};                // clip end time in timeline, project saved
    int64_t mStartOffset        {0};                // clip start time in media, project saved
    int64_t mEndOffset          {0};                // clip end time in media, project saved
    int64_t mLength             {0};                // clip length, = mEnd - mStart
    bool bSelected              {false};            // clip is selected, project saved
    bool bEditing               {false};            // clip is Editing by double click selected, project saved
    std::mutex mLock;                               // clip mutex, not using yet
    void * mHandle              {nullptr};          // clip belong to timeline 
    MediaParserHolder mMediaParser;
    int64_t mViewWndDur         {0};
    float mPixPerMs             {0};
    int mTrackHeight            {0};

    imgui_json::value mFilterBP;                    // clip filter blue print, project saved

    Clip(int64_t start, int64_t end, int64_t id, MediaParserHolder mediaParser, void * handle);
    virtual ~Clip();

    virtual int64_t Moving(int64_t diff, int mouse_track);
    virtual int64_t Cropping(int64_t diff, int type);
    void Cutting(int64_t pos);
    bool isLinkedWith(Clip * clip);
    
    virtual void ConfigViewWindow(int64_t wndDur, float pixPerMs) { mViewWndDur = wndDur; mPixPerMs = pixPerMs; }
    virtual void SetTrackHeight(int trackHeight) { mTrackHeight = trackHeight; }
    virtual void SetViewWindowStart(int64_t millisec) {}
    virtual void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect) { drawList->AddRect(leftTop, rightBottom, IM_COL32_BLACK); }
    static void Load(Clip * clip, const imgui_json::value& value);
    virtual void Save(imgui_json::value& value) = 0;
};

struct VideoClip : Clip
{
    SnapshotGenerator::ViewerHolder mSsViewer;
    std::vector<VideoSnapshotInfo> mVideoSnapshotInfos; // clip snapshots info, with all croped range
    std::list<Snapshot> mVideoSnapshots;                // clip snapshots, including texture and timestamp info
    MediaInfo::Ratio mClipFrameRate {25, 1};            // clip Frame rate, project saved

    VideoClip(int64_t start, int64_t end, int64_t id, std::string name, MediaParserHolder hParser, SnapshotGenerator::ViewerHolder viewer, void* handle);
    ~VideoClip();

    void ConfigViewWindow(int64_t wndDur, float pixPerMs) override;
    void SetTrackHeight(int trackHeight) override;
    void SetViewWindowStart(int64_t millisec) override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect) override;

    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value) override;

private:
    void CalcDisplayParams();

private:
    float mSnapWidth                {0};
    float mSnapHeight               {0};
    int64_t mClipViewStartPos;
    std::vector<SnapshotGenerator::ImageHolder> mSnapImages;
};

struct AudioClip : Clip
{
    int mAudioChannels  {2};                // clip audio channels, project saved
    int mAudioSampleRate {44100};           // clip audio sample rate, project saved
    AudioRender::PcmFormat mAudioFormat {AudioRender::PcmFormat::FLOAT32}; // clip audio type, project saved
    MediaOverview::WaveformHolder mWaveform {nullptr};  // clip audio snapshot
    MediaOverview * mOverview {nullptr};

    AudioClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle);
    ~AudioClip();

    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect) override;
    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value) override;
};

struct ImageClip : Clip
{
    int mWidth          {0};        // image width, project saved
    int mHeight         {0};        // image height, project saved
    int mColorFormat    {0};        // image color format, project saved
    MediaOverview * mOverview   {nullptr};

    ImageClip(int64_t start, int64_t end, int64_t id, std::string name, MediaOverview * overview, void* handle);
    ~ImageClip();

    void SetTrackHeight(int trackHeight) override;
    void SetViewWindowStart(int64_t millisec) override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect) override;

    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value) override;

private:
    void PrepareSnapImage();

private:
    float mSnapWidth            {0};
    float mSnapHeight           {0};
    std::vector<ImGui::ImMat> mSnapImages;
    ImTextureID mImgTexture     {0};
    int64_t mClipViewStartPos;
};

struct TextClip : Clip
{
    TextClip(int64_t start, int64_t end, int64_t id, std::string name, std::string text, void* handle);
    ~TextClip();
    void SetClipDefault(const DataLayer::SubtitleStyle & style);
    void SetClipDefault(const TextClip* clip);

    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom, const ImRect& clipRect) override;
    int64_t Moving(int64_t diff, int mouse_track) override;
    int64_t Cropping(int64_t diff, int type) override;

    static Clip * Load(const imgui_json::value& value, void * handle);
    void Save(imgui_json::value& value) override;

    void CreateClipHold(void * track);
    
    std::string mText;
    std::string mFontName;
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
    ImVec4 mFontPrimaryColor {0, 0, 0, 0};
    ImVec4 mFontOutlineColor {0, 0, 0, 0};
    DataLayer::SubtitleClipHolder mClipHolder {nullptr};
    void* mTrack {nullptr};
};

class BluePrintVideoFilter : public DataLayer::VideoFilter
{
public:
    virtual ~BluePrintVideoFilter();

    void ApplyTo(DataLayer::VideoClip* clip) override {}
    ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override;

    void SetBluePrintFromJson(imgui_json::value& bpJson);

private:
    BluePrint::BluePrintUI* mBp{nullptr};
    std::mutex mBpLock;
};

class BluePrintVideoTransition : public DataLayer::VideoTransition
{
public:
    virtual ~BluePrintVideoTransition();

    DataLayer::VideoTransitionHolder Clone() override;
    void ApplyTo(DataLayer::VideoOverlap* overlap) override { mOverlap = overlap; }
    ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur) override;

    void SetBluePrintFromJson(imgui_json::value& bpJson);

private:
    DataLayer::VideoOverlap* mOverlap;
    BluePrint::BluePrintUI* mBp{nullptr};
    std::mutex mBpLock;
};

struct BaseEditingClip
{
    int64_t mID                 {-1};                   // editing clip ID
    MEDIA_TYPE mType            {MEDIA_UNKNOWN};
    int64_t mStart              {0};
    int64_t mEnd                {0};
    int64_t mStartOffset        {0};                    // editing clip start time in media
    int64_t mEndOffset          {0};                    // editing clip end time in media
    int64_t mDuration           {0};
    int64_t mCurrPos            {0};
    bool bPlay                  {false};                // editing clip play status
    bool bForward               {true};                 // editing clip play direction
    bool bSeeking               {false};
    int64_t mLastTime           {-1};
    ImVec2 mViewWndSize         {0, 0};

    void* mHandle               {nullptr};              // main timeline handle
    MediaReader* mMediaReader   {nullptr};              // editing clip media reader

    BaseEditingClip(int64_t id, MEDIA_TYPE type, int64_t start, int64_t end, int64_t startOffset, int64_t endOffset, void* handle)
        : mID(id), mType(type), mStart(start), mEnd(end), mStartOffset(startOffset), mEndOffset(endOffset), mHandle(handle)
    {}

    virtual void UpdateClipRange(Clip* clip) = 0;
    virtual void Seek(int64_t pos) = 0;
    virtual void Step(bool forward, int64_t step = 0) = 0;
    virtual void Save() = 0;
    virtual bool GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame) = 0;
    virtual void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom) = 0;
};

struct EditingVideoClip : BaseEditingClip
{
    SnapshotGeneratorHolder mSsGen;
    SnapshotGenerator::ViewerHolder mSsViewer;
    ImVec2 mSnapSize            {0, 0};
    MediaInfo::Ratio mClipFrameRate {25, 1};                    // clip Frame rate
    int mMaxCachedVideoFrame    {10};                           // clip Media Video Frame cache size

    std::mutex mFrameLock;                                      // clip frame mutex
    std::list<std::pair<ImGui::ImMat, ImGui::ImMat>> mFrame;    // clip timeline input/output frame pair
    int64_t mLastFrameTime {-1};

    EditingVideoClip(VideoClip* vidclip);
    virtual ~EditingVideoClip();

    void UpdateClipRange(Clip* clip) override;
    void Seek(int64_t pos) override;
    void Step(bool forward, int64_t step = 0) override;
    void Save() override;
    bool GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame) override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom) override;

    void CalcDisplayParams();
};

struct EditingAudioClip : BaseEditingClip
{
    EditingAudioClip(AudioClip* vidclip);
    virtual ~EditingAudioClip();

    void UpdateClipRange(Clip* clip) override;
    void Seek(int64_t pos) override;
    void Step(bool forward, int64_t step = 0) override;
    void Save() override;
    bool GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame) override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom) override;
};

struct BaseEditingOverlap
{
    Overlap* mOvlp;
    int64_t mStart;
    int64_t mEnd;
    int64_t mDuration;
    int64_t mCurrent        {0};
    int64_t mLastTime       {-1};
    ImVec2 mViewWndSize     {0, 0};

    bool bPlay                  {false};                // editing overlap play status
    bool bForward               {true};                 // editing overlap play direction
    bool bSeeking{false};

    BaseEditingOverlap(Overlap* ovlp) : mOvlp(ovlp) {}
    std::pair<int64_t, int64_t> m_StartOffset;
    std::pair<MediaReader*, MediaReader*> mMediaReader;

    virtual void Seek(int64_t pos) = 0;
    virtual void Step(bool forward, int64_t step = 0) = 0;
    virtual bool GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame) = 0;
    virtual void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom) = 0;
    virtual void Save() = 0;
};

struct EditingVideoOverlap : BaseEditingOverlap
{
    VideoClip *mClip1, *mClip2;
    SnapshotGeneratorHolder mSsGen1, mSsGen2;
    SnapshotGenerator::ViewerHolder mViewer1, mViewer2;
    ImVec2 mSnapSize{0, 0};
    int mMaxCachedVideoFrame    {10};                           // clip Media Video Frame cache size
    int64_t mLastFrameTime  {-1};

    MediaInfo::Ratio mClipFirstFrameRate {25, 1};     // overlap clip first Frame rate
    MediaInfo::Ratio mClipSecondFrameRate {25, 1};     // overlap clip second Frame rate

    std::mutex mFrameLock;
    std::list<std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>> mFrame;    // overlap timeline input pair/output frame pair

    EditingVideoOverlap(Overlap* ovlp);
    virtual ~EditingVideoOverlap();

    void Seek(int64_t pos) override;
    void Step(bool forward, int64_t step = 0) override;
    bool GetFrame(std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat>& in_out_frame) override;
    void DrawContent(ImDrawList* drawList, const ImVec2& leftTop, const ImVec2& rightBottom) override;
    void Save() override;

    void CalcDisplayParams();
};

struct MediaTrack
{
    int64_t mID             {-1};               // track ID, project saved
    MEDIA_TYPE mType        {MEDIA_UNKNOWN};    // track type, project saved
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
    int64_t mViewWndDur     {0};
    float mPixPerMs         {0};
    DataLayer::SubtitleTrackHolder mMttReader {nullptr};
    bool mTextTrackScaleLink {true};
    MediaTrack(std::string name, MEDIA_TYPE type, void * handle);
    ~MediaTrack();

    bool DrawTrackControlBar(ImDrawList *draw_list, ImRect rc);
    bool CanInsertClip(Clip * clip, int64_t pos);
    void InsertClip(Clip * clip, int64_t pos = 0, bool update = true);
    void PushBackClip(Clip * clip);
    void SelectClip(Clip * clip, bool appand);
    void SelectEditingClip(Clip * clip);
    void SelectEditingOverlap(Overlap * overlap);
    void DeleteClip(int64_t id);
    Clip * FindPrevClip(int64_t id);                // find prev clip in track, if not found then return null
    Clip * FindNextClip(int64_t id);                // find next clip in track, if not found then return null
    Clip * FindClips(int64_t time, int& count);     // find clips at time, count means clip number at time
    void CreateOverlap(int64_t start, int64_t start_clip_id, int64_t end, int64_t end_clip_id, MEDIA_TYPE type);
    Overlap * FindExistOverlap(int64_t start_clip_id, int64_t end_clip_id);
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
            clip->ConfigViewWindow(wndDur, pixPerMs);
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
    TimeLineCallback  EditingClip       {nullptr};
    TimeLineCallback  EditingOverlap    {nullptr};
} TimeLineCallbackFunctions;

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

struct TimeLine
{
    TimeLine();
    ~TimeLine();
    IDGenerator m_IDGenerator;              // Timeline ID generator
    std::vector<MediaItem *> media_items;   // Media Bank, project saved
    std::vector<MediaTrack *> m_Tracks;     // timeline tracks, project saved
    std::vector<Clip *> m_Clips;            // timeline clips, project saved
    std::vector<ClipGroup> m_Groups;        // timeline clip groups, project saved
    std::vector<Overlap *> m_Overlaps;      // timeline clip overlap, project saved
    std::unordered_map<int64_t, SnapshotGeneratorHolder> m_VidSsGenTable;  // Snapshot generator for video media item, provide snapshots for VideoClip
    int64_t mStart   {0};                   // whole timeline start in ms, project saved
    int64_t mEnd     {0};                   // whole timeline end in ms, project saved

    bool mShowHelpTooltips      {true};     // timeline show help tooltips, project saved, configured
    bool mHardwareCodec         {true};     // timeline Video/Audio decode/encode try to enable HW if available;
    int mWidth  {1920};                     // timeline Media Width, project saved, configured
    int mHeight {1080};                     // timeline Media Height, project saved, configured
    MediaInfo::Ratio mFrameRate {25, 1};    // timeline Media Frame rate, project saved, configured
    int mMaxCachedVideoFrame    {10};       // timeline Media Video Frame cache size, project saved, configured

    int mAudioChannels {2};                 // timeline audio channels, project saved, configured
    int mAudioSampleRate {44100};           // timeline audio sample rate, project saved, configured
    AudioRender::PcmFormat mAudioFormat {AudioRender::PcmFormat::FLOAT32}; // timeline audio format, project saved, configured

    // sutitle Setting
    std::string mFontName;
    // Output Setting
    std::string mOutputName {"Untitled"};
    std::string mOutputPath {""};
    std::string mVideoCodec {"h264"};
    std::string mAudioCodec {"aac"};
    bool bExportVideo {true};
    bool bExportAudio {true};
    MediaEncoder* mEncoder {nullptr};

    struct VideoEncoderParams
    {
        std::string codecName;
        std::string imageFormat;
        uint32_t width;
        uint32_t height;
        MediaInfo::Ratio frameRate;
        uint64_t bitRate;
        std::vector<MediaEncoder::Option> extraOpts;
    };

    struct AudioEncoderParams
    {
        std::string codecName;
        std::string sampleFormat;
        uint32_t channels;
        uint32_t sampleRate;
        uint64_t bitRate;
        uint32_t samplesPerFrame {1024};
        std::vector<MediaEncoder::Option> extraOpts;
    };

    MultiTrackVideoReader* mEncMtvReader {nullptr};
    MultiTrackAudioReader* mEncMtaReader {nullptr};

    bool ConfigEncoder(const std::string& outputPath, VideoEncoderParams& vidEncParams, AudioEncoderParams& audEncParams, std::string& errMsg);
    void StartEncoding();
    void StopEncoding();
    void _EncodeProc();
    std::thread mEncodingThread;
    bool mIsEncoding {false};
    bool mQuitEncoding {false};
    std::string mEncodeProcErrMsg;
    float mEncodingProgress;

    std::vector<audio_channel_data> m_audio_channel_data;   // timeline audio data replace audio levels
    ImGui::ImMat m_audio_vector;
    ImTextureID m_audio_vector_texture {nullptr};
    float mAudioVectorScale  {1};
    int mAudioVectorMode {LISSAJOUS};
    void CalculateAudioScopeData(ImGui::ImMat& mat);
    float mAudioSpectrogramOffset {0.0};
    float mAudioSpectrogramLight {1.0};

    int64_t attract_docking_pixels {10};    // clip attract docking sucking in pixels range, pulling range is 1/5
    int64_t mConnectedPoints = -1;

    int64_t currentTime = 0;
    int64_t firstTime = 0;
    int64_t lastTime = 0;
    int64_t visibleTime = 0;
    int64_t mark_in = -1;                   // mark in point, -1 means no mark in point or mark in point is start of timeline if mark out isn't -1
    int64_t mark_out = -1;                  // mark out point, -1 means no mark out point or mark out point is end of timeline if mark in isn't -1
    float msPixelWidthTarget = 0.1f;

    bool bSeeking = false;
    bool bLoop = false;                     // project saved
    bool bCompare = false;                  // project saved
    bool bSelectLinked = true;              // project saved

    std::mutex mVidFilterClipLock;          // timeline clip mutex
    EditingVideoClip* mVidFilterClip    {nullptr};
    std::mutex mAudFilterClipLock;          // timeline clip mutex
    EditingAudioClip* mAudFilterClip    {nullptr};
    std::mutex mVidFusionLock;              // timeline overlap mutex
    EditingVideoOverlap* mVidOverlap    {nullptr};
    std::mutex mAudFusionLock;              // timeline overlap mutex
    EditingVideoOverlap* mAudOverlap    {nullptr};

    MultiTrackVideoReader* mMtvReader   {nullptr};
    MultiTrackAudioReader* mMtaReader   {nullptr};
    int64_t mPreviewResumePos               {0};
    bool mIsPreviewPlaying                  {false};
    bool mIsPreviewForward                  {true};
    bool mIsStepMode                        {false};
    int64_t mLastFrameTime                  {-1};
    using PlayerClock = std::chrono::steady_clock;
    PlayerClock::time_point mPlayTriggerTp;

    imgui_json::value mOngoingAction;
    std::list<imgui_json::value> mUiActions;
    void PerformUiActions();
    void PerformVideoAction(imgui_json::value& action);
    void PerformAudioAction(imgui_json::value& action);

    class SimplePcmStream : public AudioRender::ByteStream
    {
    public:
        SimplePcmStream(TimeLine* owner) : m_owner(owner) {}
        void SetAudioReader(MultiTrackAudioReader* areader) { m_areader = areader; }
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
        MultiTrackAudioReader* m_areader;
        ImGui::ImMat m_amat;
        uint32_t m_readPosInAmat{0};
        bool m_tsValid{false};
        int64_t m_timestampMs{0};
        std::mutex m_amatLock;
    };
    SimplePcmStream mPcmStream;

    std::mutex mTrackLock;                  // timeline track mutex
    
    // BP CallBacks
    static int OnBluePrintChange(int type, std::string name, void* handle);

    BluePrint::BluePrintUI * mVideoFilterBluePrint {nullptr};
    std::mutex mVideoFilterBluePrintLock;   // Video Filter BluePrint mutex
    bool mVideoFilterNeedUpdate {false};

    ImTextureID mVideoFilterInputTexture {nullptr};  // clip video filter input texture
    ImTextureID mVideoFilterOutputTexture {nullptr};  // clip video filter output texture

    BluePrint::BluePrintUI * mAudioFilterBluePrint {nullptr};
    std::mutex mAudioFilterBluePrintLock;   // Audio Filter BluePrint mutex
    bool mAudioFilterNeedUpdate {false};

    BluePrint::BluePrintUI * mVideoFusionBluePrint {nullptr};
    std::mutex mVideoFusionBluePrintLock;   // Video Fusion BluePrint mutex
    bool mVideoFusionNeedUpdate {false};

    BluePrint::BluePrintUI * mAudioFusionBluePrint {nullptr};
    std::mutex mAudioFusionBluePrintLock;   // Video Fusion BluePrint mutex
    bool mAudioFusionNeedUpdate {false};

    std::mutex mFrameLock;                      // timeline frame mutex
    std::list<ImGui::ImMat> mFrame;             // timeline output frame
    ImTextureID mMainPreviewTexture {nullptr};  // main preview texture

    std::thread * mVideoFilterThread {nullptr}; // Video Filter Thread, which is only one item/clip read from media
    bool mVideoFilterDone {false};              // Video Filter Thread should finished
    bool mVideoFilterRunning {false};           // Video Filter Thread is running

    std::thread * mVideoFusionThread {nullptr}; // Video Fusion Thread, which is only two item/clip read from media
    bool mVideoFusionDone {false};              // Video Fusion Thread should finished
    bool mVideoFusionRunning {false};           // Video Fusion Thread is running

    ImTextureID mVideoFusionInputFirstTexture {nullptr};    // clip video fusion first input texture
    ImTextureID mVideoFusionInputSecondTexture {nullptr};   // clip video fusion second input texture
    ImTextureID mVideoFusionOutputTexture {nullptr};        // clip video fusion output texture

    TimeLineCallbackFunctions  m_CallBacks;

    int64_t GetStart() const { return mStart; }
    int64_t GetEnd() const { return mEnd; }
    void SetStart(int64_t pos) { mStart = pos; }
    void SetEnd(int64_t pos) { mEnd = pos; }
    size_t GetCustomHeight(int index) { return (index < m_Tracks.size() && m_Tracks[index]->mExpanded) ? m_Tracks[index]->mTrackHeight : 0; }
    void Updata();
    void AlignTime(int64_t& time);

    int GetTrackCount() const { return (int)m_Tracks.size(); }
    int GetTrackCount(MEDIA_TYPE type);
    int GetEmptyTrackCount();
    int NewTrack(const std::string& name, MEDIA_TYPE type, bool expand);
    int64_t DeleteTrack(int index);
    void SelectTrack(int index);
    void MovingTrack(int& index, int& dst_index);

    void MovingClip(int64_t id, int from_track_index, int to_track_index);
    void DeleteClip(int64_t id);

    void DeleteOverlap(int64_t id);

    void DoubleClick(int index, int64_t time);
    void Click(int index, int64_t time);

    void CustomDraw(int index, ImDrawList *draw_list, const ImRect &view_rc, const ImRect &rc, const ImRect &titleRect, const ImRect &clippingTitleRect, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, bool is_moving, bool enable_select);
    
    ImGui::ImMat GetPreviewFrame();
    float GetAudioLevel(int channel);

    void Play(bool play, bool forward = true);
    void Seek(int64_t msPos);
    void Step(bool forward = true);
    void Loop(bool loop);
    void ToStart();
    void ToEnd();
    void UpdateCurrent();
    int64_t ValidDuration();

    AudioRender* mAudioRender {nullptr};                // audio render(SDL)

    MediaItem* FindMediaItemByName(std::string name);   // Find media from bank by name
    MediaItem* FindMediaItemByID(int64_t id);           // Find media from bank by ID
    MediaTrack * FindTrackByID(int64_t id);             // Find track by ID
    MediaTrack * FindTrackByClipID(int64_t id);         // Find track by clip ID
    int FindTrackIndexByClipID(int64_t id);             // Find track by clip ID
    Clip * FindClipByID(int64_t id);                    // Find clip with clip ID
    Clip * FindEditingClip();                           // Find clip which is editing
    Overlap * FindOverlapByID(int64_t id);              // Find overlap with overlap ID
    Overlap * FindEditingOverlap();                     // Find overlap which is editing
    int GetSelectedClipCount();                         // Get current selected clip count
    int64_t NextClipStart(Clip * clip);                 // Get next clip start pos by clip, if don't have next clip, then return -1
    int64_t NextClipStart(int64_t pos);                 // Get next clip start pos by time, if don't have next clip, then return -1
    int64_t NewGroup(Clip * clip);                      // Create a new group with clip ID
    void AddClipIntoGroup(Clip * clip, int64_t group_id); // Insert clip into group
    void DeleteClipFromGroup(Clip *clip, int64_t group_id); // Delete clip from group
    ImU32 GetGroupColor(int64_t group_id);              // Get Group color by id
    int Load(const imgui_json::value& value);
    void Save(imgui_json::value& value);

    void ConfigureDataLayer();
    void SyncDataLayer();
    SnapshotGeneratorHolder GetSnapshotGenerator(int64_t mediaItemId);
    void ConfigSnapshotWindow(int64_t viewWndDur);
};

bool DrawTimeLine(TimeLine *timeline, bool *expanded, bool editable = true);
bool DrawClipTimeLine(BaseEditingClip * editingClip, int header_height, int custom_height);
bool DrawOverlapTimeLine(BaseEditingOverlap * overlap, int header_height, int custom_height);
std::string TimelineMillisecToString(int64_t millisec, int show_millisec = 0);
} // namespace MediaTimeline
