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
    SEQUENCER_CHANGE_FRAME = 1 << 3,
    SEQUENCER_ADD = 1 << 4,
    SEQUENCER_DEL = 1 << 5,
    SEQUENCER_COPYPASTE = 1 << 6,
    SEQUENCER_EDIT_ALL = SEQUENCER_EDIT_STARTEND | SEQUENCER_CHANGE_FRAME
};

struct SequenceInterface
{
    bool focused = false;
    virtual int GetFrameMin() const = 0;
    virtual int GetFrameMax() const = 0;
    virtual void SetFrameMin(int64_t pos) = 0;
    virtual void SetFrameMax(int64_t pos) = 0;
    virtual int GetItemCount() const = 0;
    virtual void BeginEdit(int /*index*/) {}
    virtual void EndEdit() {}
    virtual const char *GetItemLabel(int /*index*/) const { return ""; }
    virtual const char *GetCollapseFmt() const { return "%d Frames / %d entries"; }
    virtual void Get(int index, int64_t& start, int64_t& end, std::string& name, unsigned int& color) = 0;
    virtual void Set(int index, int64_t   start, int64_t end, std::string  name, unsigned int  color) = 0;
    virtual void Add(std::string& /*type*/) {}
    virtual void Del(int /*index*/) {}
    virtual void Duplicate(int /*index*/) {}
    virtual void Copy() {}
    virtual void Paste() {}
    virtual size_t GetCustomHeight(int /*index*/) { return 0; }
    virtual void DoubleClick(int /*index*/) {}
    virtual void CustomDraw(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*legendRect*/, const ImRect & /*clippingRect*/, const ImRect & /*legendClippingRect*/) {}
    virtual void CustomDrawCompact(int /*index*/, ImDrawList * /*draw_list*/, const ImRect & /*rc*/, const ImRect & /*clippingRect*/) {}
};
bool Sequencer(SequenceInterface *sequence, int *currentFrame, bool *expanded, int *selectedEntry, int *firstFrame, int *lastFrame, int sequenceOptions);

struct SequenceItem
{
    std::string mName;
    std::string mPath;
    unsigned int mColor {0};
    int64_t mFrameStart {0};
    int64_t mFrameEnd   {0};
    bool mExpanded  {false};
    int mMediaType {SEQUENCER_ITEM_UNKNOWN};
    MediaSnapshot* mMedia   {nullptr};
    ImTextureID mMediaSnapshot  {nullptr};
    SequenceItem(const std::string& name, const std::string& path, int64_t start, int64_t end, bool expand, int type);
    ~SequenceItem();
    void SequenceItemUpdateSnapShot();
};

struct MediaSequence : public SequenceInterface
{
    MediaSequence() : mStart(0), mEnd(0) {}
    ~MediaSequence();
    // interface with sequencer
    int GetFrameMin() const { return mStart; }
    int GetFrameMax() const { return mEnd; }
    void SetFrameMin(int64_t pos) { mStart = pos; }
    void SetFrameMax(int64_t pos) { mEnd = pos; }
    int GetItemCount() const { return (int)m_Items.size(); }
    const char *GetItemLabel(int index) const  { return m_Items[index]->mName.c_str(); }
    void Get(int index, int64_t& start, int64_t& end, std::string& name, unsigned int& color);
    void Set(int index, int64_t  start, int64_t  end, std::string  name, unsigned int  color);
    void Add(std::string& name);
    void Del(int index);
    void Duplicate(int index);
    size_t GetCustomHeight(int index) { return m_Items[index]->mExpanded ? 40 : 0; }
    void DoubleClick(int index) { m_Items[index]->mExpanded = !m_Items[index]->mExpanded; }
    void CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect);
    void CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &clippingRect);

    int64_t mStart   {0}; 
    int64_t mEnd   {0};
    std::vector<SequenceItem *> m_Items;
};

} // namespace ImSequencer

#endif /* __IM_SEQUENCER_H__ */