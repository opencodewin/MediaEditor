#ifndef __IM_SEQUENCER_H__
#define __IM_SEQUENCER_H__
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include <string>
#include <vector>

namespace ImSequencer
{
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
    virtual int GetItemCount() const = 0;
    virtual void BeginEdit(int /*index*/) {}
    virtual void EndEdit() {}
    virtual const char *GetItemLabel(int /*index*/) const { return ""; }
    virtual const char *GetCollapseFmt() const { return "%d Frames / %d entries"; }
    virtual void Get(int index, int **start, int **end, const char **name, unsigned int *color) = 0;
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
    int mFrameStart, mFrameEnd;
    bool mExpanded;
    SequenceItem(const std::string& name, const std::string& path, int start, int end, bool expand)
    {
        mName = name;
        mPath = path;
        mFrameStart = start;
        mFrameEnd = end;
        mExpanded = expand;
    }
};

struct MediaSequence : public SequenceInterface
{
    MediaSequence() : mFrameMin(0), mFrameMax(0) {}
    // interface with sequencer
    int GetFrameMin() const
    {
        return mFrameMin;
    }
    int GetFrameMax() const
    {
        return mFrameMax;
    }
    int GetItemCount() const { return (int)m_Items.size(); }
    //int GetItemTypeCount() const { return sizeof(SequencerItemTypeNames) / sizeof(char *); }
    //const char *GetItemTypeName(int typeIndex) const { return SequencerItemTypeNames[typeIndex]; }
    const char *GetItemLabel(int index) const  { return m_Items[index].mName.c_str(); }
    void Get(int index, int **start, int **end, const char **name, unsigned int *color)
    {
        SequenceItem &item = m_Items[index];
        if (color)
            *color = 0xFFAA8080; // same color for everyone, return color based on type
        if (start)
            *start = &item.mFrameStart;
        if (end)
            *end = &item.mFrameEnd;
        if (name)
            *name = item.mName.c_str();
    }
    void Add(std::string& name) { m_Items.push_back(SequenceItem{name, "", 0, 10, false}); };
    void Del(int index) { m_Items.erase(m_Items.begin() + index); }
    void Duplicate(int index) { m_Items.push_back(m_Items[index]); }
    size_t GetCustomHeight(int index) { return m_Items[index].mExpanded ? 40 : 0; }
    void DoubleClick(int index)
    {
        m_Items[index].mExpanded = !m_Items[index].mExpanded;
    }
    void CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect)
    {
        draw_list->PushClipRect(legendClippingRect.Min, legendClippingRect.Max, true);
        draw_list->PopClipRect();
        ImGui::SetCursorScreenPos(rc.Min);
    }
    void CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &clippingRect)
    {
        draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
        draw_list->PopClipRect();
    }

    int mFrameMin, mFrameMax;
    std::vector<SequenceItem> m_Items;
};

} // namespace ImSequencer

#endif /* __IM_SEQUENCER_H__ */