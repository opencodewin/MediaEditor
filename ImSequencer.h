#ifndef __IM_SEQUENCER_H__
#define __IM_SEQUENCER_H__
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
#define COL_SLOT_DEFAULT    IM_COL32(128, 128, 170, 255)
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

#define MAX_SEQUENCER_FRAME_NUMBER  8

namespace ImSequencer
{
enum SEQUENCER_ITEM_TYPE : int
{
    SEQUENCER_ITEM_UNKNOWN = -1,
    SEQUENCER_ITEM_VIDEO = 0,
    SEQUENCER_ITEM_AUDIO = 1,
    SEQUENCER_ITEM_PICTURE = 2,
    SEQUENCER_ITEM_TEXT = 3,
    // ...
};

enum SEQUENCER_OPTIONS
{
    SEQUENCER_EDIT_NONE = 0,
    SEQUENCER_EDIT_STARTEND = 1 << 1,
    SEQUENCER_CHANGE_TIME = 1 << 3,
    SEQUENCER_ADD = 1 << 4,
    SEQUENCER_DEL = 1 << 5,
    SEQUENCER_COPYPASTE = 1 << 6,
    SEQUENCER_LOCK = 1 << 7,
    SEQUENCER_VIEW = 1 << 8,
    SEQUENCER_MUTE = 1 << 9,
    SEQUENCER_RESTORE = 1 << 10,
    SEQUENCER_EDIT_ALL = SEQUENCER_EDIT_STARTEND | SEQUENCER_CHANGE_TIME
};

struct SequencerCustomDraw
{
    int index;
    ImRect customRect;
    ImRect titleRect;
    ImRect clippingTitleRect;
    ImRect legendRect;
    ImRect clippingRect;
    ImRect legendClippingRect;
};

struct SequencerInterface
{
    bool focused = false;
    int options = 0;
    int selectedEntry = -1;
    int64_t currentTime = 0;
    int64_t firstTime = 0;
    int64_t lastTime = 0;
    int64_t visibleTime = 0;
    int64_t timeStep = 0;
    bool bPlay = false;
    bool bForward = true;
    bool bLoop = false;
    bool bSeeking = false;
    std::mutex mSequencerLock;
    virtual int64_t GetStart() const = 0;
    virtual int64_t GetEnd() const = 0;
    virtual void SetStart(int64_t pos) = 0;
    virtual void SetEnd(int64_t pos) = 0;
    virtual void SetCurrent(int64_t pos, bool rev) = 0;
    virtual int GetItemCount() const = 0;
    virtual void BeginEdit(int /*index*/) {}
    virtual void EndEdit() {}
    virtual const char *GetItemLabel(int /*index*/) const { return ""; }
    virtual void Get(int /*index*/, int64_t& /*start*/, int64_t& /*end*/, int64_t& /*length*/, int64_t& /*start_offset*/, int64_t& /*end_offset*/, std::string& /*name*/, unsigned int& /*color*/) = 0;
    virtual void Get(int /*index*/, float& /*frame_duration*/, float& /*snapshot_width*/) = 0;
    virtual void Get(int /*index*/, bool& /*expanded*/, bool& /*view*/, bool& /*locked*/, bool& /*muted*/, bool& /*cutting*/) = 0;
    virtual void Set(int /*index*/, int64_t /*start*/, int64_t /*end*/, int64_t /*start_offset*/, int64_t /*end_offset*/, std::string  /*name*/, unsigned int /*color*/) = 0;
    virtual void Set(int /*index*/, bool /*expanded*/, bool /*view*/, bool /*locked*/, bool /*muted*/) = 0;
    virtual void Set(int /*index*/, int64_t /*cutting_pos*/, bool /*add*/) = 0;
    virtual int Check(int /*index*/, int64_t& /*cutting_pos*/) = 0;
    virtual void Add(std::string& /*type*/) {}
    virtual void Del(int /*index*/) {}
    virtual void Duplicate(int /*index*/) {}
    virtual void Copy() {}
    virtual void Paste() {}
    virtual void Seek() = 0;
    virtual size_t GetCustomHeight(int /*index*/) { return 0; }
    virtual bool GetItemSelected(int /*index*/) const = 0;
    virtual void SetItemSelected(int /*index*/) {};
    virtual void DoubleClick(int /*index*/) {}
    virtual void CustomDraw(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*titleRect*/, const ImRect & /*clippingTitleRect*/, const ImRect & /*legendRect*/, const ImRect & /*clippingRect*/, const ImRect & /*legendClippingRect*/, int64_t /* viewStartTime */, int64_t /* visibleTime */, float /*pixelWidth*/, bool /* need_update */) {}
    virtual void CustomDrawCompact(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*legendRect*/, const ImRect & /*clippingRect*/, int64_t /*viewStartTime*/, int64_t /*visibleTime*/, float /*pixelWidth*/) {}
};

bool Sequencer(SequencerInterface *sequencer, bool *expanded, int sequenceOptions);

struct MediaItem
{
    std::string mName;
    std::string mPath;
    int64_t mStart   {0};                   // whole Media start in ms
    int64_t mEnd   {0};                     // whole Media end in ms
    MediaOverview * mMediaOverview;
    int mMediaType {SEQUENCER_ITEM_UNKNOWN};
    std::vector<ImTextureID> mMediaThumbnail;
    MediaItem(const std::string& name, const std::string& path, int type);
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

struct ClipInfo
{
    int64_t mID     {-1};
    int64_t mStart  {0};
    int64_t mEnd    {0};
    int64_t mCurrent{0};
    int64_t mFrameInterval {40};
    int64_t mLastTime {-1};
    int64_t mCurrentFilterTime {-1};
    bool bPlay      {false};
    bool bForward   {true};
    bool bSeeking   {false};
    bool mDragOut   {false};
    bool mSelected  {false};
    void * mItem    {nullptr};
    MediaSnapshot* mSnapshot {nullptr};     // clip snapshot handle
    std::vector<Snapshot> mVideoSnapshots;  // clip snapshots, including texture and timestamp info
    std::mutex mFrameLock;                  // clip frame mutex
    std::list<std::pair<ImGui::ImMat, ImGui::ImMat>> mFrame;         // clip timeline input/output frame pair
    int mFrameCount    {0};                 // total snapshot number in clip range
    float mSnapshotWidth {0};
    ClipInfo(int64_t start, int64_t end, bool drag_out, void* handle);
    ~ClipInfo();
    void UpdateSnapshot();
    void Seek();
    bool GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame);
    ImTextureID mFilterInputTexture {nullptr};  // clip filter input texture
    ImTextureID mFilterOutputTexture {nullptr};  // clip filter output texture
    imgui_json::value mFilterBP;
};

struct SequencerItem
{
    std::string mName;                      // item name
    std::string mPath;                      // item media path
    unsigned int mColor {0};                // item view color
    int64_t mStart      {0};                // item Start time in sequencer
    int64_t mEnd        {0};                // item End time in sequencer
    int64_t mStartOffset {0};               // item start time in media
    int64_t mEndOffset   {0};               // item end time in media
    int64_t mLength     {0};                // item total length in ms, not effect by cropping
    int64_t mFrameInterval {40};            // timeline Media Frame Interval in ms
    int mAudioChannels  {2};                // item audio channels(could be setting?)
    int mAudioSampleRate {44100};           // item audio sample rate(could be setting?)
    bool mExpanded  {false};                // item is compact view or not
    bool mView      {true};                 // item is viewable or not
    bool mMuted     {false};                // item is muted or not
    bool mLocked    {false};                // item is locked or not(can't moving or cropping by locked)
    bool mCutting   {false};                // item is cutting or moving stage
    bool mSelected  {false};                // item is selected
    int mMediaType {SEQUENCER_ITEM_UNKNOWN}; // item media type, could be video, audio, image, text, and so on...
    int64_t mValidViewTime {0};             // current view area time is ms, only contented media part
    int mValidViewSnapshot {0};             // current view area contented snapshot number
    int mLastValidSnapshot {0};             // last current view area snapshot number
    float mSnapshotWidth {0};               // snapshot with in pixels
    float mSnapshotDuration {0};            // single snapshot duration in ms
    float mFrameDuration {0};               // single media frame duration in ms
    float mFrameCount    {0};               // total snapshot number in cropped range
    int64_t mSnapshotPos {-1};              // current snapshot position in ms(start of view area)
    int64_t mSnapshotLendth {0};            // crop range total length in ms
    MediaSnapshot* mSnapshot {nullptr};     // item snapshot handle
    MediaOverview::WaveformHolder mWaveform {nullptr};  // item audio snapshot
    MediaReader* mMediaReaderVideo {nullptr};           // item media reader for video
    MediaReader* mMediaReaderAudio {nullptr};           // item media reader for audio
    std::vector<VideoSnapshotInfo> mVideoSnapshotInfos; // item snapshots info, with all croped range
    std::vector<Snapshot> mVideoSnapshots;  // item snapshots, including texture and timestamp info
    std::vector<int64_t> mCutPoint;         // item cut points info
    std::vector<ClipInfo *> mClips;           // item clips info
    void Initialize(const std::string& name, MediaParserHolder parser_holder, MediaOverview::WaveformHolder wave_holder, int64_t start, int64_t end, bool expand, int type);
    SequencerItem(const std::string& name, MediaItem * media_item, int64_t start, int64_t end, bool expand, int type);
    SequencerItem(const std::string& name, SequencerItem * sequencer_item, int64_t start, int64_t end, bool expand, int type);
    ~SequencerItem();
    void SequencerItemUpdateThumbnail();
    void SequencerItemUpdateSnapshots();
    void SetClipSelected(ClipInfo* clip);
    void CalculateVideoSnapshotInfo(const ImRect &customRect, int64_t viewStartTime, int64_t visibleTime);
    bool DrawItemControlBar(ImDrawList *draw_list, ImRect rc, int sequenceOptions);
};

class SequencerPcmStream;
struct MediaSequencer : public SequencerInterface
{
    MediaSequencer();
    ~MediaSequencer();
    // interface with sequencer
    int64_t GetStart() const { return mStart; }
    int64_t GetEnd() const { return mEnd; }
    void SetStart(int64_t pos) { mStart = pos; }
    void SetEnd(int64_t pos) { mEnd = pos; }
    void SetCurrent(int64_t pos, bool rev);
    int GetItemCount() const { return (int)m_Items.size(); }
    bool GetItemSelected(int index) const { return m_Items[index]->mSelected; }
    void SetItemSelected(int index);
    const char *GetItemLabel(int index) const  { return m_Items[index]->mName.c_str(); }
    void Get(int index, int64_t& start, int64_t& end, int64_t& start_offset, int64_t& end_offset, int64_t& length, std::string& name, unsigned int& color);
    void Get(int index, float& frame_duration, float& snapshot_width);
    void Get(int index, bool& expanded, bool& view, bool& locked, bool& muted, bool& cutting);
    void Set(int index, int64_t start, int64_t end, int64_t start_offset, int64_t end_offset, std::string  name, unsigned int  color);
    void Set(int index, bool expanded, bool view, bool locked, bool muted);
    void Set(int index, int64_t cutting_pos, bool add);
    int Check(int index, int64_t& cutting_pos);
    void Add(std::string& name);
    void Del(int index);
    void Duplicate(int index);
    void Seek();
    size_t GetCustomHeight(int index) { return m_Items[index]->mExpanded ? mItemHeight : 0; }
    void DoubleClick(int index) { m_Items[index]->mExpanded = !m_Items[index]->mExpanded; }
    void CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &titleRect, const ImRect &clippingTitleRect, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, int64_t viewStartTime, int64_t visibleTime, float pixelWidth, bool need_update);
    void CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, int64_t viewStartTime, int64_t visibleTime, float pixelWidth);
    ImGui::ImMat GetPreviewFrame();
    int GetAudioLevel(int channel);

    void Play(bool play, bool forward = true);
    void Step(bool forward = true);
    void Loop(bool loop);
    void ToStart();
    void ToEnd();

    std::vector<SequencerItem *> m_Items;   // timeline items
    const int mItemHeight {60};             // item custom view height
    int64_t mStart   {0};                   // whole timeline start in ms
    int64_t mEnd   {0};                     // whole timeline end in ms
    int mWidth  {1920};                     // timeline Media Width
    int mHeight {1080};                     // timeline Media Height
    int64_t mFrameInterval {40};            // timeline Media Frame Duration in ms
    int mAudioChannels {2};                 // timeline audio channels
    int mAudioSampleRate {44100};           // timeline audio sample rate
    AudioRender::PcmFormat mAudioFormat {AudioRender::PcmFormat::FLOAT32};
                                            // timeline audio format
    std::vector<int> mAudioLevel;           // timeline audio levels
    
    std::thread * mPreviewThread {nullptr}; // Preview Thread, which is read whole time line and mixer all filter/transition
    bool mPreviewDone {false};              // Preview Thread should finished
    bool mPreviewRunning {false};           // Preview Thread is running
    std::thread * mVideoFilterThread {nullptr}; // Video Filter Thread, which is only one item/clip read from media
    bool mVideoFilterDone {false};          // Video Filter Thread should finished
    bool mVideoFilterRunning {false};       // Video Filter Thread is running
    std::thread * mVideoFusionThread {nullptr}; // Video Fusion Thread, which is two item/clip read from media and fusion with video transition
    bool mVideoFusionDone {false};          // Video Fusion Thread should finished
    bool mVideoFusionRunning {false};       // Video Fusion Thread is running
    std::mutex mFrameLock;                  // frame mutex
    std::list<ImGui::ImMat> mFrame;         // timeline output frame
    ImTextureID mMainPreviewTexture {nullptr};  // main preview texture
    int64_t mCurrentPreviewTime {-1};
    BluePrint::BluePrintUI * video_filter_bp {nullptr};

    AudioRender* mAudioRender {nullptr};        // audio render(SDL)
    SequencerPcmStream * mPCMStream {nullptr};  // audio pcm stream
};

class SequencerPcmStream : public AudioRender::ByteStream
{
public:
    SequencerPcmStream(MediaSequencer* sequencer) : m_sequencer(sequencer) {}
    uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override;
private:
    MediaSequencer* m_sequencer {nullptr};
};

bool ClipTimeLine(ClipInfo* clip);

} // namespace ImSequencer

#endif /* __IM_SEQUENCER_H__ */