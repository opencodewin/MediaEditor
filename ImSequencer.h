#ifndef __IM_SEQUENCER_H__
#define __IM_SEQUENCER_H__
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "MediaSnapshot.h"
#include <string>
#include <vector>

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
    virtual int64_t GetStart() const = 0;
    virtual int64_t GetEnd() const = 0;
    virtual void SetStart(int64_t pos) = 0;
    virtual void SetEnd(int64_t pos) = 0;
    virtual int GetItemCount() const = 0;
    virtual void BeginEdit(int /*index*/) {}
    virtual void EndEdit() {}
    virtual const char *GetItemLabel(int /*index*/) const { return ""; }
    virtual void Get(int index, int64_t& start, int64_t& end, std::string& name, unsigned int& color) = 0;
    virtual void Get(int index, float& frame_duration, float& snapshot_width) = 0;
    virtual void Set(int index, int64_t   start, int64_t end, std::string  name, unsigned int  color) = 0;
    virtual void Add(std::string& /*type*/) {}
    virtual void Del(int /*index*/) {}
    virtual void Duplicate(int /*index*/) {}
    virtual void Copy() {}
    virtual void Paste() {}
    virtual size_t GetCustomHeight(int /*index*/) { return 0; }
    virtual void DoubleClick(int /*index*/) {}
    virtual void CustomDraw(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*legendRect*/, const ImRect & /*clippingRect*/, const ImRect & /*legendClippingRect*/, int64_t /* viewStartTime */, int64_t /* visibleTime */) {}
    virtual void CustomDrawCompact(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*clippingRect*/, int64_t /*viewStartTime*/, int64_t /*visibleTime*/) {}
    virtual void GetVideoSnapshotInfo(int /*index*/, std::vector<VideoSnapshotInfo>&) {}
};

bool Sequencer(SequencerInterface *sequencer, int64_t *currentTime, bool *expanded, int *selectedEntry, int64_t *firstTime, int64_t *lastTime, int sequenceOptions);

struct Snapshot
{
    ImTextureID texture {nullptr};
    int64_t     time_stamp {0};
    int64_t     estimate_time {0};
};

struct SequencerItem
{
    std::string mName;
    std::string mPath;
    unsigned int mColor {0};
    int64_t mStart {0};
    int64_t mEnd   {0};
    bool mExpanded  {false};
    int mMediaType {SEQUENCER_ITEM_UNKNOWN};
    int mMaxViewSnapshot;
    float mTotalFrame;
    float mSnapshotWidth {0};
    float mFrameDuration {0};
    int64_t mSnapshotPos {-1};
    MediaSnapshot* mMedia   {nullptr};
    ImTextureID mMediaThumbnail  {nullptr};
    std::vector<VideoSnapshotInfo> mVideoSnapshotInfos;
    std::vector<Snapshot> mVideoSnapshots;
    SequencerItem(const std::string& name, const std::string& path, int64_t start, int64_t end, bool expand, int type);
    ~SequencerItem();
    void SequencerItemUpdateThumbnail();
    void SequencerItemUpdateSnapshots();
    void CalculateVideoSnapshotInfo(const ImRect &customRect, int64_t viewStartTime, int64_t visibleTime);
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
    void Get(int index, int64_t& start, int64_t& end, std::string& name, unsigned int& color);
    void Get(int index, float& frame_duration, float& snapshot_width);
    void Set(int index, int64_t  start, int64_t  end, std::string  name, unsigned int  color);
    void Add(std::string& name);
    void Del(int index);
    void Duplicate(int index);
    size_t GetCustomHeight(int index) { return m_Items[index]->mExpanded ? mItemHeight : 0; }
    void DoubleClick(int index) { m_Items[index]->mExpanded = !m_Items[index]->mExpanded; }
    void CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, int64_t viewStartTime, int64_t visibleTime);
    void CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &clippingRect, int64_t viewStartTime, int64_t visibleTime);
    void GetVideoSnapshotInfo(int index, std::vector<VideoSnapshotInfo>& snapshots);

    const int mItemHeight {60};
    int64_t mStart   {0}; 
    int64_t mEnd   {0};
    std::vector<SequencerItem *> m_Items;
};

} // namespace ImSequencer

#endif /* __IM_SEQUENCER_H__ */