#ifndef __IM_SEQUENCER_H__
#define __IM_SEQUENCER_H__
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "MediaSnapshot.h"
#include <string>
#include <vector>

#define ICON_MEDIA_BANK     u8"\ue907"
#define ICON_MEDIA_TRANSITIONS u8"\ue882"
#define ICON_MEDIA_FILTERS  u8"\ue663"
#define ICON_MEDIA_OUTPUT   u8"\uf197"
#define ICON_MEDIA_VIDEO    u8"\ue04b"
#define ICON_MEDIA_AUDIO    u8"\ue050"
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
#define ICON_CROP           u8"\uf5c8"
#define ICON_LOCKED         u8"\uf023"
#define ICON_UNLOCK         u8"\uf09c"
#define ICON_TRASH          u8"\uf014"
#define ICON_CLONE          u8"\uf2d2"
#define ICON_ADD            u8"\uf067"

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

#define COL_FRAME_RECT      IM_COL32( 16,  16,  96, 255)
#define COL_LIGHT_BLUR      IM_COL32( 16, 128, 255, 255)
#define COL_CANVAS_BG       IM_COL32( 36,  36,  36, 255)
#define COL_LEGEND_BG       IM_COL32( 18,  18,  18, 255)
#define COL_MARK            IM_COL32(255, 255, 255, 255)
#define COL_MARK_HALF       IM_COL32(128, 128, 128, 255)
#define COL_RULE_TEXT       IM_COL32(224, 224, 224, 255)
#define COL_SLOT_DEFAULT    IM_COL32(128, 128, 170, 255)
#define COL_SLOT_ODD        IM_COL32( 58,  58,  58, 255)
#define COL_SLOT_EVEN       IM_COL32( 64,  64,  64, 255)
#define COL_SLOT_SELECTED   IM_COL32( 64,  64, 255, 128)
#define COL_SLOT_V_LINE     IM_COL32( 96,  96,  96,  48)
#define COL_SLIDER_BG       IM_COL32( 32,  32,  64, 255)
#define COL_SLIDER_IN       IM_COL32( 96,  96,  96, 255)
#define COL_SLIDER_MOVING   IM_COL32( 80,  80,  80, 255)
#define COL_SLIDER_HANDLE   IM_COL32(112, 112, 112, 255)
#define COL_SLIDER_SIZING   IM_COL32(170, 170, 170, 255)
#define COL_CURSOR_ARROW    IM_COL32(  0, 255,   0, 255)
#define COL_CURSOR_TEXT_BG  IM_COL32(  0, 128,   0, 144)
#define COL_CURSOR_TEXT     IM_COL32(  0, 255,   0, 255)

#define HALF_COLOR(c)       (c & 0xFFFFFF) | 0x40000000;

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
    SEQUENCER_EDIT_ALL = SEQUENCER_EDIT_STARTEND | SEQUENCER_CHANGE_TIME
};

struct SequencerCustomDraw
{
    int index;
    ImRect customRect;
    ImRect legendRect;
    ImRect clippingRect;
    ImRect legendClippingRect;
};

struct VideoSnapshotInfo
{
    ImRect rc;
    int64_t time_stamp;
    int64_t duration;
    float frame_width;
};

struct SequencerInterface
{
    bool focused = false;
    int options = 0;
    virtual int64_t GetStart() const = 0;
    virtual int64_t GetEnd() const = 0;
    virtual void SetStart(int64_t pos) = 0;
    virtual void SetEnd(int64_t pos) = 0;
    virtual int GetItemCount() const = 0;
    virtual void BeginEdit(int /*index*/) {}
    virtual void EndEdit() {}
    virtual const char *GetItemLabel(int /*index*/) const { return ""; }
    virtual void Get(int /*index*/, int64_t& /*start*/, int64_t& /*end*/, int64_t& /*length*/, int64_t& /*start_offset*/, int64_t& /*end_offset*/, std::string& /*name*/, unsigned int& /*color*/) = 0;
    virtual void Get(int /*index*/, float& /*frame_duration*/, float& /*snapshot_width*/) = 0;
    virtual void Get(int /*index*/, bool& /*expanded*/, bool& /*view*/, bool& /*locked*/, bool& /*muted*/) = 0;
    virtual void Set(int /*index*/, int64_t /*start*/, int64_t /*end*/, int64_t /*start_offset*/, int64_t /*end_offset*/, std::string  /*name*/, unsigned int /*color*/) = 0;
    virtual void Set(int /*index*/, bool /*expanded*/, bool /*view*/, bool /*locked*/, bool /*muted*/) = 0;
    virtual void Add(std::string& /*type*/) {}
    virtual void Del(int /*index*/) {}
    virtual void Duplicate(int /*index*/) {}
    virtual void Copy() {}
    virtual void Paste() {}
    virtual size_t GetCustomHeight(int /*index*/) { return 0; }
    virtual void DoubleClick(int /*index*/) {}
    virtual void CustomDraw(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*legendRect*/, const ImRect & /*clippingRect*/, const ImRect & /*legendClippingRect*/, int64_t /* viewStartTime */, int64_t /* visibleTime */) {}
    virtual void CustomDrawCompact(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*legendRect*/, const ImRect & /*clippingRect*/, int64_t /*viewStartTime*/, int64_t /*visibleTime*/) {}
    virtual void GetVideoSnapshotInfo(int /*index*/, std::vector<VideoSnapshotInfo>&) {}
};

bool Sequencer(SequencerInterface *sequencer, int64_t *currentTime, bool *expanded, int *selectedEntry, int64_t *firstTime, int64_t *lastTime, int sequenceOptions);

struct Snapshot
{
    ImTextureID texture {nullptr};
    int64_t     time_stamp {0};
    int64_t     estimate_time {0};
    bool        available{false};
};

struct SequencerItem
{
    std::string mName;
    std::string mPath;
    unsigned int mColor {0};
    int64_t mStart      {0};        // item Start time in sequencer
    int64_t mEnd        {0};        // item End time in sequencer
    int64_t mStartOffset {0};       // item start time in media
    int64_t mEndOffset   {0};       // item end time in media
    int64_t mLength     {0};
    bool mExpanded  {false};
    bool mView      {true};
    bool mMuted     {false};
    bool mLocked    {false};
    int mMediaType {SEQUENCER_ITEM_UNKNOWN};
    int mMaxViewSnapshot;
    float mSnapshotWidth {0};
    float mFrameDuration {0};
    int64_t mSnapshotPos {-1};
    int64_t mSnapshotLendth {0};
    MediaSnapshot* mMedia   {nullptr};
    ImTextureID mMediaThumbnail  {nullptr};
    std::vector<VideoSnapshotInfo> mVideoSnapshotInfos;
    std::vector<Snapshot> mVideoSnapshots;
    SequencerItem(const std::string& name, const std::string& path, int64_t start, int64_t end, bool expand, int type);
    ~SequencerItem();
    void SequencerItemUpdateThumbnail();
    void SequencerItemUpdateSnapshots();
    void CalculateVideoSnapshotInfo(const ImRect &customRect, int64_t viewStartTime, int64_t visibleTime);
    void DrawItemControlBar(ImDrawList *draw_list, ImRect rc, int sequenceOptions);
};

struct MediaSequencer : public SequencerInterface
{
    MediaSequencer() : mStart(0), mEnd(0) {}
    ~MediaSequencer();
    // interface with sequencer
    int64_t GetStart() const { return mStart; }
    int64_t GetEnd() const { return mEnd; }
    void SetStart(int64_t pos) { mStart = pos; }
    void SetEnd(int64_t pos) { mEnd = pos; }
    int GetItemCount() const { return (int)m_Items.size(); }
    const char *GetItemLabel(int index) const  { return m_Items[index]->mName.c_str(); }
    void Get(int index, int64_t& start, int64_t& end, int64_t& start_offset, int64_t& end_offset, int64_t& length, std::string& name, unsigned int& color);
    void Get(int index, float& frame_duration, float& snapshot_width);
    void Get(int index, bool& expanded, bool& view, bool& locked, bool& muted);
    void Set(int index, int64_t start, int64_t end, int64_t start_offset, int64_t end_offset, std::string  name, unsigned int  color);
    void Set(int index, bool expanded, bool view, bool locked, bool muted);
    void Add(std::string& name);
    void Del(int index);
    void Duplicate(int index);
    size_t GetCustomHeight(int index) { return m_Items[index]->mExpanded ? mItemHeight : 0; }
    void DoubleClick(int index) { m_Items[index]->mExpanded = !m_Items[index]->mExpanded; }
    void CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, int64_t viewStartTime, int64_t visibleTime);
    void CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, int64_t viewStartTime, int64_t visibleTime);
    void GetVideoSnapshotInfo(int index, std::vector<VideoSnapshotInfo>& snapshots);
    const int mItemHeight {60};
    int64_t mStart   {0}; 
    int64_t mEnd   {0};
    std::vector<SequencerItem *> m_Items;
};

} // namespace ImSequencer

#endif /* __IM_SEQUENCER_H__ */