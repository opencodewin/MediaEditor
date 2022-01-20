#include "ImSequencer.h"
#include <imgui_helper.h>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ImSequencer
{
static void alignTime(int64_t& time, int64_t time_step)
{
    // has moving smooth issue?
    auto align_time = floor(time / time_step) * time_step;
    time = align_time;
}

static std::string MillisecToString(int64_t millisec, int show_millisec = 0)
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

static bool SequencerExpendButton(ImDrawList *draw_list, ImVec2 pos, bool expand = true)
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect delRect(pos, ImVec2(pos.x + 16, pos.y + 16));
    bool overDel = delRect.Contains(io.MousePos);
    int delColor = IM_COL32_WHITE;
    float midy = pos.y + 16 / 2 - 0.5f;
    float midx = pos.x + 16 / 2 - 0.5f;
    draw_list->AddRect(delRect.Min, delRect.Max, delColor, 4);
    draw_list->AddLine(ImVec2(delRect.Min.x + 3, midy), ImVec2(delRect.Max.x - 4, midy), delColor, 2);
    if (expand) draw_list->AddLine(ImVec2(midx, delRect.Min.y + 3), ImVec2(midx, delRect.Max.y - 4), delColor, 2);
    return overDel;
}

static bool SequencerButton(ImDrawList *draw_list, const char * label, ImVec2 pos, ImVec2 size, std::string tooltips = "", ImVec4 hover_color = ImVec4(0.5f, 0.5f, 0.75f, 1.0f))
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

static void RenderMouseCursor(ImDrawList* draw_list,/* ImVec2 pos, float scale, */const char* mouse_cursor, ImU32 col_fill/*, ImU32 col_border, ImU32 col_shadow*/)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    draw_list->AddText(io.MousePos, col_fill, mouse_cursor);
}

/***********************************************************************************************************
 * Draw Sequencer Timeline
 ***********************************************************************************************************/
bool Sequencer(SequencerInterface *sequencer, bool *expanded, int sequenceOptions)
{

    /*************************************************************************************************************
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
    static float msPixelWidth = 0.1f;
    static float msPixelWidthTarget = 0.1f;
    int legendWidth = 200;
    static int movingEntry = -1;
    static int movingPos = -1;
    static int movingPart = -1;
    int delEntry = -1;
    int dupEntry = -1;
    int ItemHeight = 20;
    int HeadHeight = 20;
    int scrollBarHeight = 16;
    bool popupOpened = false;
    int itemCount = sequencer->GetItemCount();
    sequencer->options = sequenceOptions;
    static int64_t start_time = -1;
    static int64_t last_time = -1;

    ImGui::BeginGroup();
    
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DARK_TWO);
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();     // ImDrawList API uses screen coordinates!
    ImVec2 canvas_size = ImGui::GetContentRegionAvail() - ImVec2(8, 0); // Resize canvas to what's available
    int64_t firstTimeUsed = sequencer->firstTime;
    int controlHeight = itemCount * ItemHeight;
    for (int i = 0; i < itemCount; i++)
        controlHeight += int(sequencer->GetCustomHeight(i));
    int64_t duration = ImMax(sequencer->GetEnd() - sequencer->GetStart(), (int64_t)1);
    static bool MovingScrollBar = false;
    static bool MovingCurrentTime = false;
    ImVector<SequencerCustomDraw> customDraws;
    ImVector<SequencerCustomDraw> compactCustomDraws;
    // zoom in/out
    sequencer->visibleTime = (int64_t)floorf((canvas_size.x - legendWidth) / msPixelWidth);
    const float barWidthRatio = ImMin(sequencer->visibleTime / (float)duration, 1.f);
    const float barWidthInPixels = barWidthRatio * (canvas_size.x - legendWidth);
    ImRect regionRect(canvas_pos + ImVec2(0, HeadHeight), canvas_pos + canvas_size);
    static bool panningView = false;
    static ImVec2 panningViewSource;
    static int64_t panningViewTime;
    ImRect scrollBarRect;
    ImRect scrollHandleBarRect;
    if (ImGui::IsWindowFocused() && io.KeyAlt && io.MouseDown[2])
    {
        if (!panningView)
        {
            panningViewSource = io.MousePos;
            panningView = true;
            panningViewTime = sequencer->firstTime;
        }
        sequencer->firstTime = panningViewTime - int((io.MousePos.x - panningViewSource.x) / msPixelWidth);
        sequencer->firstTime = ImClamp(sequencer->firstTime, sequencer->GetStart(), sequencer->GetEnd() - sequencer->visibleTime);
    }
    if (panningView && !io.MouseDown[2])
    {
        panningView = false;
    }

    float minPixelWidthTarget = ImMin(msPixelWidthTarget, (float)(canvas_size.x - legendWidth) / (float)duration);
    float maxPixelWidthTarget = 20.f;
    float min_frame_duration = FLT_MAX;
    for (int i = 0; i < itemCount; i++)
    {
        float frame_duration;
        float snapshot_width;
        sequencer->Get(i, frame_duration, snapshot_width);
        if (frame_duration > 0 && snapshot_width > 0)
        {
            if (min_frame_duration > frame_duration)
            {
                min_frame_duration = frame_duration;
                maxPixelWidthTarget = floor(snapshot_width) / frame_duration;
            }
        }
    }
    msPixelWidthTarget = ImClamp(msPixelWidthTarget, minPixelWidthTarget, maxPixelWidthTarget);
    msPixelWidth = ImLerp(msPixelWidth, msPixelWidthTarget, 0.5f);

    if (sequencer->visibleTime >= duration)
        sequencer->firstTime = sequencer->GetStart();

    sequencer->lastTime = firstTimeUsed + sequencer->visibleTime;

    ImGui::SetCursorScreenPos(canvas_pos + ImVec2(4, ItemHeight + 4));
    ImGui::InvisibleButton("canvas", canvas_size - ImVec2(8, ItemHeight + scrollBarHeight + 8));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Media_drag_drop"))
        {
            ImSequencer::MediaItem * item = (ImSequencer::MediaItem*)payload->Data;
            ImSequencer::MediaSequencer * seq = (ImSequencer::MediaSequencer *)sequencer;
            SequencerItem * new_item = new SequencerItem(item->mName, item, item->mStart, item->mEnd, true, item->mMediaType);
            auto length = new_item->mEnd - new_item->mStart;
            if (sequencer->currentTime >= sequencer->firstTime && sequencer->currentTime <= sequencer->GetEnd())
                new_item->mStart = sequencer->currentTime;
            else
                new_item->mStart = sequencer->firstTime;
            new_item->mEnd = new_item->mStart + length;
            if (new_item->mEnd > sequencer->GetEnd())
                sequencer->SetEnd(new_item->mEnd);
            seq->m_Items.push_back(new_item);
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Clip_drag_drop"))
        {
            ImSequencer::ClipInfo * clip = (ImSequencer::ClipInfo *)payload->Data;
            if (clip)
            {
                ImSequencer::SequencerItem * item = (ImSequencer::SequencerItem*)clip->mItem;
                ImSequencer::MediaSequencer * seq = (ImSequencer::MediaSequencer *)sequencer;
                auto start = clip->mStart + item->mStart;
                auto end = clip->mEnd + item->mStart;
                SequencerItem * new_item = new SequencerItem(item->mName, item, start, end, true, item->mMediaType);
                new_item->mStartOffset = clip->mStart - item->mStartOffset;
                new_item->mEndOffset = item->mLength - clip->mEnd;
                seq->m_Items.push_back(new_item);
                if (!item->mLocked)
                {
                    for (auto c : item->mClips)
                    {
                        if (c->mStart == clip->mStart)
                        {
                            c->bDragOut = true;
                            break;
                        }
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SetCursorScreenPos(canvas_pos);
    if ((expanded && !*expanded) || !itemCount)
    {
        // minimum view
        ImGui::InvisibleButton("canvas_minimum", ImVec2(canvas_size.x - canvas_pos.x - 8.f, (float)ItemHeight));
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x - 8.f, canvas_pos.y + ItemHeight), COL_DARK_ONE, 0);
        auto info_str = MillisecToString(duration, 3);
        info_str += " / ";
        info_str += std::to_string(itemCount) + " entries";
        draw_list->AddText(ImVec2(canvas_pos.x + 40, canvas_pos.y + 2), IM_COL32_WHITE, info_str.c_str());
    }
    else
    {
        // normal view
        bool hasScrollBar(true);
        // test scroll area
        ImVec2 headerSize(canvas_size.x - 4.f, (float)HeadHeight);
        ImVec2 scrollBarSize(canvas_size.x, scrollBarHeight);
        ImGui::InvisibleButton("topBar", headerSize);
        draw_list->AddRectFilled(canvas_pos, canvas_pos + headerSize, COL_DARK_ONE, 0);
        if (!itemCount) 
        {
            ImGui::EndGroup();
            return false;
        }
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImVec2 childFramePos = ImGui::GetCursorScreenPos();
        ImVec2 childFrameSize(canvas_size.x, canvas_size.y - 8.f - headerSize.y - (hasScrollBar ? scrollBarSize.y : 0));
        ImGui::BeginChildFrame(ImGui::GetID("sequencer_items"), childFrameSize, ImGuiWindowFlags_NoScrollbar);
        ImGui::InvisibleButton("contentBar", ImVec2(canvas_size.x - 8.f, float(controlHeight)));
        sequencer->focused = ImGui::IsWindowFocused();
        const ImVec2 contentMin = ImGui::GetItemRectMin();
        const ImVec2 contentMax = ImGui::GetItemRectMax();
        const ImRect contentRect(contentMin, contentMax);
        const ImRect legendRect(contentMin, ImVec2(contentMin.x + legendWidth, contentMax.y));
        const float contentHeight = contentMax.y - contentMin.y;
        
        // full canvas background
        draw_list->AddRectFilled(canvas_pos + ImVec2(4, ItemHeight + 4), canvas_pos + ImVec2(4, ItemHeight + 4) + canvas_size - ImVec2(8, ItemHeight + scrollBarHeight + 8), COL_CANVAS_BG, 0);
        // full legend background
        draw_list->AddRectFilled(legendRect.Min, legendRect.Max, COL_LEGEND_BG, 0);

        // current time top
        ImRect topRect(ImVec2(canvas_pos.x + legendWidth, canvas_pos.y), ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + ItemHeight));
        if (!MovingCurrentTime && !MovingScrollBar && movingEntry == -1 && sequenceOptions & SEQUENCER_CHANGE_TIME && sequencer->currentTime >= 0 && topRect.Contains(io.MousePos) && io.MouseDown[0])
        {
            MovingCurrentTime = true;
            sequencer->bSeeking = true;
        }
        if (MovingCurrentTime && duration)
        {
            sequencer->currentTime = (int64_t)((io.MousePos.x - topRect.Min.x) / msPixelWidth) + firstTimeUsed;
            alignTime(sequencer->currentTime, sequencer->timeStep);
            if (sequencer->currentTime < sequencer->GetStart())
                sequencer->currentTime = sequencer->GetStart();
            if (sequencer->currentTime >= sequencer->GetEnd())
                sequencer->currentTime = sequencer->GetEnd();
            sequencer->Seek(); // call seek event
        }
        if (sequencer->bSeeking && !io.MouseDown[0])
        {
            MovingCurrentTime = false;
            sequencer->bSeeking = false;
        }

        //header
        //header time and lines
        int64_t modTimeCount = 10;
        int timeStep = 1;
        while ((modTimeCount * msPixelWidth) < 75)
        {
            modTimeCount *= 10;
            timeStep *= 10;
        };
        int halfModTime = modTimeCount / 2;
        auto drawLine = [&](int64_t i, int regionHeight)
        {
            bool baseIndex = ((i % modTimeCount) == 0) || (i == sequencer->GetEnd() || i == sequencer->GetStart());
            bool halfIndex = (i % halfModTime) == 0;
            int px = (int)canvas_pos.x + int(i * msPixelWidth) + legendWidth - int(firstTimeUsed * msPixelWidth);
            int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int tiretEnd = baseIndex ? regionHeight : HeadHeight;
            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
            }
            if (baseIndex && px > (canvas_pos.x + legendWidth))
            {
                auto time_str = MillisecToString(i, 2);
                ImGui::SetWindowFontScale(0.8);
                draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y), COL_RULE_TEXT, time_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
        };
        auto drawLineContent = [&](int64_t i, int /*regionHeight*/)
        {
            int px = (int)canvas_pos.x + int(i * msPixelWidth) + legendWidth - int(firstTimeUsed * msPixelWidth);
            int tiretStart = int(contentMin.y);
            int tiretEnd = int(contentMax.y);
            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
                draw_list->AddLine(ImVec2(float(px), float(tiretStart)), ImVec2(float(px), float(tiretEnd)), COL_SLOT_V_LINE, 1);
            }
        };
        for (auto i = sequencer->GetStart(); i <= sequencer->GetEnd(); i += timeStep)
        {
            drawLine(i, HeadHeight);
        }
        drawLine(sequencer->GetStart(), HeadHeight);
        drawLine(sequencer->GetEnd(), HeadHeight);

        // cursor Arrow
        if (sequencer->currentTime >= sequencer->firstTime && sequencer->currentTime <= sequencer->GetEnd())
        {
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + legendWidth + (sequencer->currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - arrowWidth * 0.5f - 3;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = MillisecToString(sequencer->currentTime, 2);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + legendWidth + (sequencer->currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - str_size.x * 0.5f - 3;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }

        // clip content
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize);

        // slots background
        size_t customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
            unsigned int col = (i & 1) ? COL_SLOT_ODD : COL_SLOT_EVEN;
            size_t localCustomHeight = sequencer->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 sz = ImVec2(canvas_size.x + canvas_pos.x - 4.f, pos.y + ItemHeight - 1 + localCustomHeight);
            if (!popupOpened && cy >= pos.y && cy < pos.y + (ItemHeight + localCustomHeight) && movingEntry == -1 && cx > contentMin.x && cx < contentMin.x + canvas_size.x)
            {
                col += IM_COL32(8, 16, 32, 128);
                pos.x -= legendWidth;
            }
            draw_list->AddRectFilled(pos, sz, col, 0);
            customHeight += localCustomHeight;
        }
        // draw item title bar
        customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
            auto itemCustomHeight = sequencer->GetCustomHeight(i);
            ImVec2 button_size = ImVec2(14, 14);
            ImVec2 tpos(contentMin.x, contentMin.y + i * ItemHeight + customHeight);
            if (sequenceOptions & SEQUENCER_ADD)
            {
                bool ret = SequencerButton(draw_list, ICON_CLONE, ImVec2(contentMin.x + legendWidth - button_size.x * 2.5 - 4, tpos.y), button_size, "clone");
                if (ret && io.MouseClicked[0])
                    dupEntry = i;
            }
            if (sequenceOptions & SEQUENCER_DEL)
            {
                bool ret = SequencerButton(draw_list, ICON_TRASH, ImVec2(contentMin.x + legendWidth - button_size.x - 4, tpos.y), button_size, "delete");
                if (ret && io.MouseClicked[0])
                    delEntry = i;
            }
            customHeight += itemCustomHeight;
        }

        // clipping rect so items bars are not visible in the legend on the left when scrolled
        draw_list->PushClipRect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        // vertical time lines in content area
        for (auto i = sequencer->GetStart(); i <= sequencer->GetEnd(); i += timeStep)
        {
            drawLineContent(i, int(contentHeight));
        }
        drawLineContent(sequencer->GetStart(), int(contentHeight));
        drawLineContent(sequencer->GetEnd(), int(contentHeight));
        
        // slots
        customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
            bool selected = sequencer->GetItemSelected(i);
            int64_t start, end, length;
            int64_t start_offset, end_offset;
            std::string name;
            unsigned int color;
            sequencer->Get(i, start, end, length, start_offset, end_offset, name, color);
            size_t localCustomHeight = sequencer->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth - firstTimeUsed * msPixelWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 slotP1(pos.x + start * msPixelWidth, pos.y + 2);
            ImVec2 slotP2(pos.x + end * msPixelWidth + msPixelWidth - 4.f, pos.y + ItemHeight - 2);
            ImVec2 slotP3(pos.x + end * msPixelWidth + msPixelWidth - 4.f, pos.y + ItemHeight - 2 + localCustomHeight);
            unsigned int slotColor = color | IM_COL32_BLACK;
            unsigned int slotColorHalf = HALF_COLOR(color);
            if (slotP1.x <= (canvas_size.x + contentMin.x) && slotP2.x >= (contentMin.x + legendWidth))
            {
                draw_list->AddRectFilled(slotP1, slotP3, slotColorHalf, 0);
                draw_list->AddRectFilled(slotP1, slotP2, slotColor, 0);
            }
            if (ImRect(slotP1, slotP2).Contains(io.MousePos) && io.MouseDoubleClicked[0])
            {
                sequencer->DoubleClick(i);
            }

            // Ensure grabable handles
            const float max_handle_width = slotP2.x - slotP1.x / 3.0f;
            const float min_handle_width = ImMin(10.0f, max_handle_width);
            const float handle_width = ImClamp(msPixelWidth / 2.0f, min_handle_width, max_handle_width);
            ImRect rects[3] = {ImRect(slotP1, ImVec2(slotP1.x + handle_width, slotP2.y)), ImRect(ImVec2(slotP2.x - handle_width, slotP1.y), slotP2), ImRect(slotP1, slotP2)};
            const unsigned int quadColor[] = {IM_COL32_WHITE, IM_COL32_WHITE, slotColor + (selected ? 0 : 0x202020)};
            if (movingEntry == -1 && (sequenceOptions & SEQUENCER_EDIT_STARTEND))
            {
                for (int j = 2; j >= 0; j--)
                {
                    ImRect &rc = rects[j];
                    if (!rc.Contains(io.MousePos))
                        continue;
                    draw_list->AddRectFilled(rc.Min, rc.Max, quadColor[j], 0);
                }
                for (int j = 0; j < 3; j++)
                {
                    ImRect &rc = rects[j];
                    if (!rc.Contains(io.MousePos))
                        continue;
                    if (!ImRect(childFramePos, childFramePos + childFrameSize).Contains(io.MousePos))
                        continue;
                    if (ImGui::IsMouseClicked(0) && !MovingScrollBar && !MovingCurrentTime)
                    {
                        movingEntry = i;
                        movingPos = cx;
                        movingPart = j + 1;
                        sequencer->BeginEdit(movingEntry);
                        break;
                    }
                }
            }
            customHeight += localCustomHeight;
        }

        // slot moving
        if (movingEntry >= 0)
        {
            bool expanded, view, locked, muted, cutting;
            sequencer->Get(movingEntry, expanded, view, locked, muted, cutting);
            ImGui::CaptureMouseFromApp();
            int diffTime = int((cx - movingPos) / msPixelWidth);
            if (!locked && !cutting && std::abs(diffTime) > 0)
            {
                int64_t start, end, length;
                int64_t start_offset, end_offset;
                std::string name;
                unsigned int color;
                float frame_duration, snapshot_width;
                sequencer->Get(movingEntry, start, end, length, start_offset, end_offset, name, color);
                sequencer->Get(movingEntry, frame_duration, snapshot_width);
                sequencer->SetItemSelected(movingEntry);

                if (movingPart == 3)
                {
                    // whole slot moving
                    start += diffTime;
                    end += diffTime;
                    movingPos += int(diffTime * msPixelWidth);
                }
                else if (movingPart & 1)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                    // slot left moving
                    if (start + diffTime < end - ceil(frame_duration))
                    {
                        if (start_offset + diffTime >= 0)
                        {
                            start += diffTime;
                            start_offset += diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                        else if (abs(start_offset + diffTime) <= abs(diffTime))
                        {
                            diffTime += abs(start_offset + diffTime);
                            start += diffTime;
                            start_offset += diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                    }
                    else if (end - start - ceil(frame_duration) < diffTime)
                    {
                        diffTime = end - start - ceil(frame_duration);
                        start += diffTime;
                        start_offset += diffTime;
                        movingPos += int(diffTime * msPixelWidth);
                    }
                }
                else if (movingPart & 2)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                    // slot right moving
                    if (end + diffTime > start + ceil(frame_duration))
                    {
                        if (end_offset - diffTime >= 0)
                        {
                            end += diffTime;
                            end_offset -= diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                        else if (abs(end_offset - diffTime) <= abs(diffTime))
                        {
                            diffTime -= abs(end_offset - diffTime);
                            end += diffTime;
                            end_offset -= diffTime;
                            movingPos += int(diffTime * msPixelWidth);
                        }
                    }
                    else if (end - start - ceil(frame_duration) < abs(diffTime))
                    {
                        diffTime = - (end - start - ceil(frame_duration));
                        end += diffTime;
                        end_offset -= diffTime;
                        movingPos += int(diffTime * msPixelWidth);
                    }
                }
                sequencer->Set(movingEntry, start, end, start_offset, end_offset, name, color);
                if (end > sequencer->GetEnd())
                {
                    sequencer->SetEnd(end);
                }
            }
            if (!io.MouseDown[0])
            {
                // single select
                if (!diffTime && movingPart)
                {
                    sequencer->SetItemSelected(movingEntry);
                    ret = true;
                }
                movingEntry = -1;
                sequencer->EndEdit();
                ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            }
            sequencer->Seek();
        }

        draw_list->PopClipRect();
        draw_list->PopClipRect();
        
        // copy paste
        /*
        if (sequenceOptions & SEQUENCER_COPYPASTE)
        {
            ImRect rectCopy(ImVec2(contentMin.x + 100, canvas_pos.y + 2), ImVec2(contentMin.x + 100 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectCopy = rectCopy.Contains(io.MousePos);
            unsigned int copyColor = inRectCopy ? COL_LIGHT_BLUR : IM_COL32_BLACK;
            draw_list->AddText(rectCopy.Min, copyColor, "Copy");
            ImRect rectPaste(ImVec2(contentMin.x + 140, canvas_pos.y + 2), ImVec2(contentMin.x + 140 + 30, canvas_pos.y + ItemHeight - 2));
            bool inRectPaste = rectPaste.Contains(io.MousePos);
            unsigned int pasteColor = inRectPaste ? COL_LIGHT_BLUR : IM_COL32_BLACK;
            draw_list->AddText(rectPaste.Min, pasteColor, "Paste");
            if (inRectCopy && io.MouseReleased[0])
            {
                sequencer->Copy();
            }
            if (inRectPaste && io.MouseReleased[0])
            {
                sequencer->Paste();
            }
        }
        */

        ImGui::EndChildFrame();

        if (hasScrollBar)
        {
            auto scroll_pos = ImGui::GetCursorScreenPos();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::SetWindowFontScale(0.55);
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 16 - 4, 0));
            if (ImGui::Button(ICON_FAST_TO_END "##slider_to_end", ImVec2(16, 16)))
            {
                sequencer->firstTime = sequencer->GetEnd() - sequencer->visibleTime;
                alignTime(sequencer->firstTime, sequencer->timeStep);
            }
            ImGui::ShowTooltipOnHover("Slider to End");

            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 32 - 4, 0));
            if (ImGui::Button(ICON_TO_END "##slider_to_next_cut", ImVec2(16, 16)))
            {
                // TODO::Need check all items and get nearest cutting point
            }
            ImGui::ShowTooltipOnHover("Slider to next cutting point");

            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 48 - 4, 0));
            if (ImGui::Button(ICON_SLIDER_MAXIMUM "##slider_maximum", ImVec2(16, 16)))
            {
                msPixelWidthTarget = maxPixelWidthTarget;
            }
            ImGui::ShowTooltipOnHover("Maximum Slider");

            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 64 - 4, 0));
            if (ImGui::Button(ICON_ZOOM_IN "##slider_zoom_in", ImVec2(16, 16)))
            {
                msPixelWidthTarget *= 2.0f;
                if (msPixelWidthTarget > maxPixelWidthTarget)
                    msPixelWidthTarget = maxPixelWidthTarget;
            }
            ImGui::ShowTooltipOnHover("Slider Zoom In");

            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 80 - 4, 0));
            if (ImGui::Button(ICON_ZOOM_OUT "##slider_zoom_out", ImVec2(16, 16)))
            {
                msPixelWidthTarget *= 0.5f;
                if (msPixelWidthTarget < minPixelWidthTarget)
                    msPixelWidthTarget = minPixelWidthTarget;
            }
            ImGui::ShowTooltipOnHover("Slider Zoom Out");

            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 96 - 4, 0));
            if (ImGui::Button(ICON_SLIDER_MINIMUM "##slider_minimum", ImVec2(16, 16)))
            {
                msPixelWidthTarget = minPixelWidthTarget;
                sequencer->firstTime = sequencer->GetStart();
            }
            ImGui::ShowTooltipOnHover("Minimum Slider");

            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 112 - 4, 0));
            if (ImGui::Button(ICON_TO_START "##slider_to_prev_cut", ImVec2(16, 16)))
            {
                // TODO::Need check all items and get nearest cutting point
            }
            ImGui::ShowTooltipOnHover("Slider to previous Cutting Point");
            ImGui::SetCursorScreenPos(scroll_pos + ImVec2(legendWidth - 128 - 4, 0));
            if (ImGui::Button(ICON_FAST_TO_START "##slider_to_start", ImVec2(16, 16)))
            {
                sequencer->firstTime = sequencer->GetStart();
                alignTime(sequencer->firstTime, sequencer->timeStep);
            }
            ImGui::ShowTooltipOnHover("Slider to Start");
            ImGui::SetWindowFontScale(1.0);
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(scroll_pos);
            ImGui::InvisibleButton("scrollBar", scrollBarSize);
            ImVec2 scrollBarMin = ImGui::GetItemRectMin();
            ImVec2 scrollBarMax = ImGui::GetItemRectMax();

            // ratio = time visible in control / number to total time
            float startOffset = ((float)(firstTimeUsed - sequencer->GetStart()) / (float)duration) * (canvas_size.x - legendWidth);
            ImVec2 scrollBarA(scrollBarMin.x + legendWidth, scrollBarMin.y - 2);
            ImVec2 scrollBarB(scrollBarMin.x + canvas_size.x, scrollBarMax.y - 1);
            scrollBarRect = ImRect(scrollBarA, scrollBarB);
            bool inScrollBar = scrollBarRect.Contains(io.MousePos);
            draw_list->AddRectFilled(scrollBarA, scrollBarB, COL_SLIDER_BG, 8);
            ImVec2 scrollBarC(scrollBarMin.x + legendWidth + startOffset, scrollBarMin.y);
            ImVec2 scrollBarD(scrollBarMin.x + legendWidth + barWidthInPixels + startOffset, scrollBarMax.y - 2);
            scrollHandleBarRect = ImRect(scrollBarC, scrollBarD);
            bool inScrollHandle = scrollHandleBarRect.Contains(io.MousePos);
            draw_list->AddRectFilled(scrollBarC, scrollBarD, (inScrollBar || MovingScrollBar) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
            if (MovingScrollBar)
            {
                if (!io.MouseDown[0])
                {
                    MovingScrollBar = false;
                }
                else
                {
                    float msPerPixelInBar = barWidthInPixels / (float)sequencer->visibleTime;
                    sequencer->firstTime = int((io.MousePos.x - panningViewSource.x) / msPerPixelInBar) - panningViewTime;
                    //alignTime(sequencer->firstTime, sequencer->timeStep);
                    sequencer->firstTime = ImClamp(sequencer->firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - sequencer->visibleTime, sequencer->GetStart()));
                }
            }
            else if (inScrollHandle && ImGui::IsMouseClicked(0) && !MovingCurrentTime && movingEntry == -1)
            {
                MovingScrollBar = true;
                panningViewSource = io.MousePos;
                panningViewTime = - sequencer->firstTime;
            }
            else if (inScrollBar && ImGui::IsMouseReleased(0))
            {
                float msPerPixelInBar = barWidthInPixels / (float)sequencer->visibleTime;
                sequencer->firstTime = int((io.MousePos.x - legendWidth - scrollHandleBarRect.GetWidth() / 2)/ msPerPixelInBar);
                //alignTime(sequencer->firstTime, sequencer->timeStep);
                sequencer->firstTime = ImClamp(sequencer->firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - sequencer->visibleTime, sequencer->GetStart()));
            }
        }

        if (regionRect.Contains(io.MousePos))
        {
            bool overCustomDraw = false;
            bool overScrollBar = false;
            for (auto &custom : customDraws)
            {
                if (custom.customRect.Contains(io.MousePos))
                {
                    overCustomDraw = true;
                }
            }
            if (scrollBarRect.Contains(io.MousePos))
            {
                overScrollBar = true;
            }
            if (overScrollBar)
            {
                // up-down wheel over scrollbar, scale canvas view
                int64_t overCursor = sequencer->firstTime + (int64_t)(sequencer->visibleTime * ((io.MousePos.x - (float)legendWidth - canvas_pos.x) / (canvas_size.x - legendWidth)));
                if (io.MouseWheel < -FLT_EPSILON && sequencer->visibleTime <= sequencer->GetEnd())
                {
                    msPixelWidthTarget *= 0.9f;
                }
                if (io.MouseWheel > FLT_EPSILON)
                {
                    msPixelWidthTarget *= 1.1f;
                }
            }
            else
            {
                // left-right wheel over blank area, moving canvas view
                if (io.MouseWheelH < -FLT_EPSILON)
                {
                    sequencer->firstTime -= sequencer->visibleTime / 4;
                    //alignTime(sequencer->firstTime, sequencer->timeStep);
                    sequencer->firstTime = ImClamp(sequencer->firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - sequencer->visibleTime, sequencer->GetStart()));
                }
                if (io.MouseWheelH > FLT_EPSILON)
                {
                    sequencer->firstTime += sequencer->visibleTime / 4;
                    //alignTime(sequencer->firstTime, sequencer->timeStep);
                    sequencer->firstTime = ImClamp(sequencer->firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - sequencer->visibleTime, sequencer->GetStart()));
                }
            }
        }

        // calculate custom draw rect
        customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
            int64_t start, end, length;
            int64_t start_offset, end_offset;
            std::string name;
            unsigned int color;
            sequencer->Get(i, start, end, length, start_offset, end_offset, name, color);
            size_t localCustomHeight = sequencer->GetCustomHeight(i);
            if (localCustomHeight > 0)
            {
                // slot normal view (custom view)
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + 1 + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstTimeUsed - start - 0.5f) * msPixelWidth, float(ItemHeight)),
                                  rp + ImVec2(legendWidth + (end - firstTimeUsed - 0.5f + 2.f) * msPixelWidth, float(localCustomHeight + ItemHeight)));
                ImRect titleRect(rp + ImVec2(legendWidth - (firstTimeUsed - start - 0.5f) * msPixelWidth, 0),
                                  rp + ImVec2(legendWidth + (end - firstTimeUsed - 0.5f + 2.f) * msPixelWidth, float(ItemHeight)));
                ImRect clippingTitleRect(rp + ImVec2(float(legendWidth), 0), rp + ImVec2(canvas_size.x - 4.0f, float(ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(ItemHeight)), rp + ImVec2(canvas_size.x - 4.0f, float(localCustomHeight + ItemHeight)));
                ImRect legendRect(rp, rp + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
                ImRect legendClippingRect(rp + ImVec2(0.f, float(ItemHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
                customDraws.push_back({i, customRect, titleRect, clippingTitleRect, legendRect, clippingRect, legendClippingRect});
            }
            else
            {
                // slot compact view (item bar only) 
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstTimeUsed - start - 0.5f) * msPixelWidth, float(0.f)),
                                  rp + ImVec2(legendWidth + (end - firstTimeUsed - 0.5f + 2.f) * msPixelWidth, float(ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(0.f)), rp + ImVec2(canvas_size.x, float(ItemHeight)));
                ImRect legendRect(rp, rp + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
                compactCustomDraws.push_back({i, customRect, customRect, clippingRect, legendRect, clippingRect, ImRect()});
            }
            customHeight += localCustomHeight;
        }

        // draw custom
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize);
        for (auto &customDraw : customDraws)
            sequencer->CustomDraw(customDraw.index, draw_list, customDraw.customRect, customDraw.titleRect, customDraw.clippingTitleRect, customDraw.legendRect, customDraw.clippingRect, customDraw.legendClippingRect, sequencer->firstTime, sequencer->visibleTime, msPixelWidth, fabs(msPixelWidth / msPixelWidthTarget - 1.0) < 1e-6);
        for (auto &customDraw : compactCustomDraws)
            sequencer->CustomDrawCompact(customDraw.index, draw_list, customDraw.customRect, customDraw.legendRect, customDraw.clippingRect, sequencer->firstTime, sequencer->visibleTime, msPixelWidth);
        draw_list->PopClipRect();

        // cursor line
        ImRect custom_view_rect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
        if (itemCount > 0 && sequencer->currentTime >= sequencer->firstTime && sequencer->currentTime <= sequencer->GetEnd())
        {
            ImVec2 contentMin(canvas_pos.x + 4.f, canvas_pos.y + (float)HeadHeight + 8.f);
            ImVec2 contentMax(canvas_pos.x + canvas_size.x - 4.f, canvas_pos.y + (float)HeadHeight + float(controlHeight) + 8.f);
            static const float cursorWidth = 3.f;
            float cursorOffset = contentMin.x + legendWidth + (sequencer->currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - cursorWidth * 0.5f - 2;
            draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMax.y), IM_COL32(0, 255, 0, 128), cursorWidth);
        }
        draw_list->PopClipRect();

        // handle mouse cutting event
        if (custom_view_rect.Contains(io.MousePos))
        {
            auto mouseTime = (int64_t)((io.MousePos.x - topRect.Min.x) / msPixelWidth) + firstTimeUsed;
            for (auto &customDraw : customDraws)
            {
                int64_t start, end, length;
                int64_t start_offset, end_offset;
                std::string name;
                unsigned int color;
                sequencer->Get(customDraw.index, start, end, length, start_offset, end_offset, name, color);
                bool expanded, view, locked, muted, cutting;
                sequencer->Get(customDraw.index, expanded, view, locked, muted, cutting);
                auto view_rect = customDraw.titleRect;
                view_rect.ClipWithFull(customDraw.clippingTitleRect);
                if (view_rect.Contains(io.MousePos))
                {
                    mouseTime -= start;
                    mouseTime += start_offset;
                    if (cutting)
                    {
                        alignTime(mouseTime, sequencer->timeStep);
                        int alread_cut = sequencer->Check(customDraw.index, mouseTime);
                        auto time_stream = MillisecToString(mouseTime, 3);
                        if (alread_cut != -1)
                        {
                            RenderMouseCursor(draw_list, ICON_REMOVE_CUT, IM_COL32_WHITE);
                            ImGui::BeginTooltip();
                            ImGui::Text("Remove Cut point (%s)", time_stream.c_str());
                            ImGui::EndTooltip();
                            if (io.MouseClicked[0])
                            {
                                sequencer->Set(customDraw.index, alread_cut, false);
                            }
                        }
                        else
                        {
                            RenderMouseCursor(draw_list, ICON_CUT, IM_COL32_WHITE);
                            ImGui::BeginTooltip();
                            ImGui::Text("Cutting (%s)", time_stream.c_str());
                            ImGui::EndTooltip();
                            if (io.MouseClicked[0])
                            {
                                sequencer->Set(customDraw.index, mouseTime, true);
                            }
                        }
                    }
                }
            }
            for (auto &customDraw : compactCustomDraws)
            {
                int64_t start, end, length;
                int64_t start_offset, end_offset;
                std::string name;
                unsigned int color;
                sequencer->Get(customDraw.index, start, end, length, start_offset, end_offset, name, color);
                bool expanded, view, locked, muted, cutting;
                sequencer->Get(customDraw.index, expanded, view, locked, muted, cutting);
                auto view_rect = customDraw.customRect;
                view_rect.ClipWithFull(customDraw.clippingRect);
                if (view_rect.Contains(io.MousePos))
                {
                    mouseTime -= start;
                    mouseTime += start_offset;
                    if (cutting)
                    {
                        alignTime(mouseTime, sequencer->timeStep);
                        int alread_cut = sequencer->Check(customDraw.index, mouseTime);
                        auto time_stream = MillisecToString(mouseTime, 3);
                        if (alread_cut != -1)
                        {
                            RenderMouseCursor(draw_list, ICON_REMOVE_CUT, IM_COL32_WHITE);
                            ImGui::BeginTooltip();
                            ImGui::Text("Remove Cut point (%s)", time_stream.c_str());
                            ImGui::EndTooltip();
                            if (io.MouseClicked[0])
                            {
                                sequencer->Set(customDraw.index, alread_cut, false);
                            }
                        }
                        else
                        {
                            RenderMouseCursor(draw_list, ICON_CUT, IM_COL32_WHITE);
                            ImGui::BeginTooltip();
                            ImGui::Text("Cutting (%s)", time_stream.c_str());
                            ImGui::EndTooltip();
                            if (io.MouseClicked[0])
                            {
                                sequencer->Set(customDraw.index, mouseTime, true);
                            }
                        }
                    }
                }
            }
        }
        ImGui::PopStyleColor();
    }

    ImGui::EndGroup();

    if (expanded)
    {
        bool overExpanded = SequencerExpendButton(draw_list, ImVec2(canvas_pos.x + 2, canvas_pos.y + 2), !*expanded);
        if (overExpanded && io.MouseReleased[0])
            *expanded = !*expanded;
    }
    if (delEntry != -1)
    {
        sequencer->Del(delEntry);
        if (sequencer->GetItemSelected(delEntry))
        {
            sequencer->SetItemSelected(-1);
        }
    }
    if (dupEntry != -1)
    {
        sequencer->Duplicate(dupEntry);
    }

    // handle play event
    if (sequencer->bPlay)
    {
        if (start_time == -1)
        {
            start_time = ImGui::get_current_time_usec() / 1000;
            //alignTime(start_time, sequencer->timeStep);
            last_time = start_time;
        }
        else
        {
            int64_t current_time = ImGui::get_current_time_usec() / 1000;
            //alignTime(current_time, sequencer->timeStep);
            int64_t step_time = current_time - last_time;
            // Set TimeLine
            int64_t current_media_time = sequencer->currentTime;
            if (sequencer->bForward)
            {
                current_media_time += step_time;
                if (current_media_time >= sequencer->GetEnd())
                {
                    if (sequencer->bLoop)
                    {
                        last_time = current_media_time = 0;
                        start_time = current_time;
                    }
                    else 
                    {
                        sequencer->bPlay = false;
                        current_media_time = sequencer->GetEnd();
                    }
                }
            }
            else
            {
                current_media_time -= step_time;
                if (current_media_time <= sequencer->GetStart())
                {
                    if (sequencer->bLoop)
                    {
                        current_media_time = sequencer->GetEnd();
                        start_time = current_time;
                    }
                    else
                    {
                        sequencer->bPlay = false;
                        current_media_time = sequencer->GetStart();
                    }
                }
            }
            sequencer->SetCurrent(current_media_time, !sequencer->bForward);
            last_time = current_time;
        }
    }
    else
    {
        start_time = -1;
    }

    // selection
    sequencer->mSequencerLock.lock();
    sequencer->selectedEntry = -1;
    for (int i = 0; i < itemCount; i++)
    {
        if (sequencer->GetItemSelected(i))
        {
            sequencer->selectedEntry = i;
            break;
        }
    }
    sequencer->mSequencerLock.unlock();

    return ret;
}

/***********************************************************************************************************
 * Draw Clip Timeline
 ***********************************************************************************************************/
bool ClipTimeLine(ClipInfo* clip)
{
    /*************************************************************************************************************
     |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    c
     |_______________|_____________________________________________________________________ a
     |               |        custom area                                                   n 
     |               |                                                                      v                                            
     |_______________|_____________________________________________________________________ a
     ************************************************************************************************************/
    bool ret = false;
    if (!clip) return ret;
    ImGuiIO &io = ImGui::GetIO();
    int cx = (int)(io.MousePos.x);
    int cy = (int)(io.MousePos.y);
    int headHeight = 30;
    int customHeight = 70;
    static bool MovingCurrentTime = false;
    //static int64_t start_time = -1;
    //static int64_t last_time = -1;

    ImGui::BeginGroup();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImRect regionRect(window_pos + ImVec2(0, headHeight), window_pos + window_size);
    int64_t duration = ImMax(clip->mEnd - clip->mStart, (int64_t)1);
    float msPixelWidth = (float)(window_size.x) / (float)duration;
    ImRect custom_view_rect(window_pos + ImVec2(0, headHeight), window_pos + window_size);

    //header
    //header time and lines
    ImVec2 headerSize(window_size.x, (float)headHeight);
    ImGui::InvisibleButton("ClipTopBar", headerSize);
    draw_list->AddRectFilled(window_pos, window_pos + headerSize, COL_DARK_ONE, 0);

    ImRect topRect(window_pos, window_pos + headerSize);
    if (!MovingCurrentTime && clip->mCurrent >= clip->mStart && topRect.Contains(io.MousePos) && io.MouseDown[0])
    {
        MovingCurrentTime = true;
        clip->bSeeking = true;
    }
    if (MovingCurrentTime && duration)
    {
        clip->mCurrent = (int64_t)((io.MousePos.x - topRect.Min.x) / msPixelWidth) + clip->mStart;
        alignTime(clip->mCurrent, clip->mFrameInterval);
        if (clip->mCurrent < clip->mStart)
            clip->mCurrent = clip->mStart;
        if (clip->mCurrent >= clip->mEnd)
            clip->mCurrent = clip->mEnd;
        clip->Seek(); // call seek event
    }
    if (clip->bSeeking && !io.MouseDown[0])
    {
        MovingCurrentTime = false;
        clip->bSeeking = false;
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
        int tiretEnd = baseIndex ? regionHeight : headHeight;
        if (px <= (window_size.x + window_pos.x) && px >= window_pos.x)
        {
            draw_list->AddLine(ImVec2((float)px, window_pos.y + (float)tiretStart), ImVec2((float)px, window_pos.y + (float)tiretEnd - 1), halfIndex ? COL_MARK : COL_MARK_HALF, halfIndex ? 2 : 1);
        }
        if (baseIndex && px >= window_pos.x)
        {
            auto time_str = MillisecToString(i + clip->mStart, 2);
            ImGui::SetWindowFontScale(0.8);
            draw_list->AddText(ImVec2((float)px + 3.f, window_pos.y), COL_RULE_TEXT, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }
    };
    for (auto i = 0; i < duration; i+= timeStep)
    {
        drawLine(i, headHeight);
    }
    drawLine(0, headHeight);
    drawLine(duration, headHeight);
    // cursor Arrow
    const float arrowWidth = draw_list->_Data->FontSize;
    float arrowOffset = window_pos.x + (clip->mCurrent - clip->mStart) * msPixelWidth + msPixelWidth / 2 - arrowWidth * 0.5f - 3;
    ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, window_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
    ImGui::SetWindowFontScale(0.8);
    auto time_str = MillisecToString(clip->mCurrent, 2);
    ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
    float strOffset = window_pos.x + (clip->mCurrent - clip->mStart) * msPixelWidth + msPixelWidth / 2 - str_size.x * 0.5f - 3;
    ImVec2 str_pos = ImVec2(strOffset, window_pos.y + 10);
    draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
    draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);

    // snapshot
    if (clip->mSnapshot && clip->mSnapshot->IsOpened() && clip->mSnapshot->HasVideo())
    {
        auto width = clip->mSnapshot->GetVideoStream()->width;
        auto height = clip->mSnapshot->GetVideoStream()->height;
        if (width && height && duration)
        {
            float aspio = (float)width / (float)height;
            float snapshot_width = customHeight * aspio;
            float frame_count = window_size.x / snapshot_width;
            int i_frame_count = ceil(frame_count);
            clip->mSnapshotWidth = snapshot_width;
            if (i_frame_count != clip->mFrameCount)
            {
                double snapshot_window_size = (double)(clip->mEnd - clip->mStart) / 1000.0;
                clip->mSnapshot->ConfigSnapWindow(snapshot_window_size, i_frame_count);
                clip->mFrameCount = i_frame_count;
                for (auto& snap : clip->mVideoSnapshots)
                {
                    if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; }
                }
                clip->mVideoSnapshots.clear();
            }
            if (i_frame_count > clip->mVideoSnapshots.size())
            {
                clip->UpdateSnapshot();
            }
        }
        // draw snapshot
        draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
        ImVec2 snap_pos = custom_view_rect.Min;
        ImVec2 size = ImVec2(clip->mSnapshotWidth, customHeight);
        for (auto snap : clip->mVideoSnapshots)
        {
            if (snap_pos.x + size.x > window_pos.x + window_size.x)
            {
                // last frame
                size.x = window_pos.x + window_size.x - snap_pos.x;
            }
            ImGui::SetCursorScreenPos(snap_pos);
            ImGui::Image(snap.texture, size);
            snap_pos += ImVec2(size.x, 0);
        }
        draw_list->PopClipRect();
    }

    // cursor line
    draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
    ImVec2 contentMin(window_pos.x, window_pos.y + (float)headHeight);
    ImVec2 contentMax(window_pos.x + window_size.x, window_pos.y + (float)headHeight + float(customHeight));
    static const float cursorWidth = 3.f;
    float cursorOffset = contentMin.x + (clip->mCurrent - clip->mStart) * msPixelWidth + msPixelWidth / 2 - cursorWidth * 0.5f - 2;
    draw_list->AddLine(ImVec2(cursorOffset, contentMin.y), ImVec2(cursorOffset, contentMax.y), IM_COL32(0, 255, 0, 128), cursorWidth);
    draw_list->PopClipRect();
    ImGui::EndGroup();

    return ret;
}

/***********************************************************************************************************
 * MediaItem Struct Member Functions
 ***********************************************************************************************************/
MediaItem::MediaItem(const std::string& name, const std::string& path, int type)
{
    mName = name;
    mPath = path;
    mMediaType = type;
    mMediaOverview = CreateMediaOverview();
    if (!path.empty() && mMediaOverview)
    {
        mMediaOverview->SetSnapshotResizeFactor(0.1, 0.1);
        mMediaOverview->Open(path, 50);
    }
    if (mMediaOverview && mMediaOverview->IsOpened())
    {
        mStart = 0;
        mEnd = mMediaOverview->GetVideoDuration();
        if (mMediaOverview->HasVideo())
            mMediaOverview->GetMediaParser()->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
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

/***********************************************************************************************************
 * SequencerItem Struct Member Functions
 ***********************************************************************************************************/
void SequencerItem::Initialize(const std::string& name, MediaParserHolder parser_holder, MediaOverview::WaveformHolder wave_holder, int64_t start, int64_t end, bool expand, int type)
{
    mName = name;
    mPath = parser_holder->GetUrl();
    mStart = start;
    mEnd = end;
    mExpanded = expand;
    mMediaType = type;
    mSnapshot = CreateMediaSnapshot();
    mMediaReaderVideo = CreateMediaReader();
    mMediaReaderAudio = CreateMediaReader();
    if (!mSnapshot || !mMediaReaderVideo || !mMediaReaderAudio)
        return;
    mColor = COL_SLOT_DEFAULT;

    // open snapshot
    mSnapshot->Open(parser_holder);

    // open video reader
    mMediaReaderVideo->Open(parser_holder);
    if (mMediaReaderVideo->ConfigVideoReader(1.f, 1.f))
    {
        mMediaReaderVideo->Start();
    }
    else
    {
        ReleaseMediaReader(&mMediaReaderVideo);
    }

    // open audio reader
    mMediaReaderAudio->Open(parser_holder);
    if (mMediaReaderAudio->ConfigAudioReader(mAudioChannels, mAudioSampleRate))
    {
        mMediaReaderAudio->Start();
    }
    else
    {
        ReleaseMediaReader(&mMediaReaderAudio);
    }

    if (mSnapshot->IsOpened())
    {
        mLength = mSnapshot->GetVideoDuration();

        if (mSnapshot->HasVideo())
        {
            auto rate = mSnapshot->GetVideoStream()->avgFrameRate;
            if (rate.num > 0) mFrameInterval = rate.den * 1000 / rate.num;
            double window_size = 1.0f;
            mSnapshot->SetCacheFactor(8.0);
            mSnapshot->SetSnapshotResizeFactor(0.1, 0.1);
            mSnapshot->ConfigSnapWindow(window_size, 1);
        }
        if (mSnapshot->HasAudio())
        {
            mWaveform = wave_holder;
        }
        if (mEnd > mStart + mLength)
            mEnd = mLength - mStart;
    }
    ClipInfo *first_clip = new ClipInfo(mStart, mEnd, false, this);
    mClips.push_back(first_clip);
}

SequencerItem::SequencerItem(const std::string& name, MediaItem * media_item, int64_t start, int64_t end, bool expand, int type)
{
    auto parser_holder = media_item->mMediaOverview->GetMediaParser();
    auto wave_holder = media_item->mMediaOverview->GetWaveform();
    Initialize(name, parser_holder, wave_holder, start, end, expand, type);
}

SequencerItem::SequencerItem(const std::string& name, SequencerItem * sequencer_item, int64_t start, int64_t end, bool expand, int type)
{
    auto parser_holder = sequencer_item->mSnapshot->GetMediaParser();
    auto wave_holder = sequencer_item->mWaveform;
    Initialize(name, parser_holder, wave_holder, start, end, expand, type);
}

SequencerItem::~SequencerItem()
{
    ReleaseMediaSnapshot(&mSnapshot);
    ReleaseMediaReader(&mMediaReaderVideo);
    ReleaseMediaReader(&mMediaReaderAudio);
    mSnapshot = nullptr;
    mMediaReaderVideo = nullptr;
    for (auto& snap : mVideoSnapshots)
    {
        if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; } 
    }
    for (auto clip : mClips)
    {
        delete clip;
    }
    mClips.clear();
}

void SequencerItem::SequencerItemUpdateSnapshots()
{
    if (mSnapshot && mSnapshot->IsOpened() && mSnapshot->HasVideo())
    {
        std::vector<ImGui::ImMat> snapshots;
        double pos = (double)(mSnapshotPos) / 1000.f;
        int media_snapshot_index = 0;
        if (mSnapshot->GetSnapshots(snapshots, pos))
        {
            for (int i = 0; i < snapshots.size(); i++)
            {
                if (!snapshots[i].empty())
                {
                    if (i == 0 && fabs(snapshots[i].time_stamp - pos) > (mSnapshotDuration / 1000.0 / 2))
                    {
                        continue;
                    }
                    // if i <= mVideoSnapshots.size() then update text else create a new text and push back into mVideoSnapshots
                    if (media_snapshot_index < mVideoSnapshots.size())
                    {
                        if (mVideoSnapshots[media_snapshot_index].time_stamp != (int64_t)(snapshots[i].time_stamp * 1000) || !mVideoSnapshots[media_snapshot_index].available)
                        {
                            ImMatToTexture(snapshots[i], mVideoSnapshots[media_snapshot_index].texture);
                            mVideoSnapshots[media_snapshot_index].time_stamp = (int64_t)(snapshots[i].time_stamp * 1000);
                            mVideoSnapshots[media_snapshot_index].available = true;
                        }
                    }
                    else
                    {
                        Snapshot snap;
                        ImMatToTexture(snapshots[i], snap.texture);
                        snap.time_stamp = (int64_t)(snapshots[i].time_stamp * 1000);
                        snap.available = true;
                        mVideoSnapshots.push_back(snap);
                    }
                    media_snapshot_index ++;
                }
                else
                {
                    // do we need clean texture cache?
                    if (i < mVideoSnapshots.size())
                    {
                        auto snap = mVideoSnapshots.begin() + i;
                        snap->available = false;
                        snap->time_stamp = 0;
                        snap->estimate_time = 0;
                    }
                }
            }
        }
        else
        {
            for (auto& snap : mVideoSnapshots)
            {
                snap.available = false;
            }
        }
    }
}

void SequencerItem::CalculateVideoSnapshotInfo(const ImRect &customRect, int64_t viewStartTime, int64_t visibleTime)
{
    if (mSnapshot && mSnapshot->IsOpened() && mSnapshot->HasVideo())
    {
        auto width = mSnapshot->GetVideoStream()->width;
        auto height = mSnapshot->GetVideoStream()->height;
        auto duration = mSnapshot->GetVideoDuration();
        auto total_frames = mSnapshot->GetVideoFrameCount();
        auto clip_duration = mEnd - mStart;
        if (!width || !height || !duration || !total_frames)
            return;
        if (customRect.GetHeight() <= 0 || customRect.GetWidth() <= 0)
            return;
        float aspio = (float)width / (float)height;
        float snapshot_height = customRect.GetHeight();
        float snapshot_width = snapshot_height * aspio;
        mSnapshotWidth = snapshot_width;
        mFrameDuration = (float)duration / (float)total_frames;
        float frame_count = customRect.GetWidth() / snapshot_width;
        float snapshot_duration = (float)clip_duration / (float)(frame_count);
        mSnapshotDuration = snapshot_duration;

        int64_t start_time = 0;
        int64_t end_time = 0;
        if (mStart < viewStartTime)
            start_time = viewStartTime;
        else if (mStart >= viewStartTime && mStart <= viewStartTime + visibleTime)
        {
            if (mEnd <= viewStartTime + visibleTime)
                start_time = mStart;
            else if (mEnd - mStart < visibleTime)
                start_time = mStart;
            else
                start_time = viewStartTime;
        }
        else
            start_time = viewStartTime + visibleTime;
        
        if (mEnd < viewStartTime)
            end_time = viewStartTime;
        else if (mEnd >= viewStartTime && mEnd <= viewStartTime + visibleTime)
            end_time = mEnd;
        else if (mEnd - mStart < visibleTime)
            end_time = mEnd;
        else
            end_time = viewStartTime + visibleTime;
        mValidViewTime = end_time - start_time;
        mValidViewSnapshot = (int)((mValidViewTime + snapshot_duration / 2) / snapshot_duration) + 1;
        if (mValidViewSnapshot > frame_count) mValidViewSnapshot = frame_count;
        frame_count++; // one more frame for end
        if (mSnapshotLendth != clip_duration || mLastValidSnapshot < mValidViewSnapshot || (int)frame_count != mVideoSnapshotInfos.size() || fabs(frame_count - mFrameCount) > 1e-2)
        {
            //fprintf(stderr, "[Dicky Debug] Update snapinfo\n");
            double window_size = mValidViewSnapshot * snapshot_duration / 1000.0;
            mSnapshot->ConfigSnapWindow(window_size, mValidViewSnapshot);
            mLastValidSnapshot = mValidViewSnapshot;
            mVideoSnapshotInfos.clear();
            for (auto& snap : mVideoSnapshots)
            {
                if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; }
            }
            mVideoSnapshots.clear();
            mSnapshotPos = -1;
            mSnapshotLendth = clip_duration;
            mFrameCount = frame_count;
            for (int i = 0; i < (int)frame_count; i++)
            {
                VideoSnapshotInfo snapshot;
                snapshot.rc.Min = ImVec2(i * snapshot_width, 0);
                snapshot.rc.Max = ImVec2((i + 1) * snapshot_width, snapshot_height);
                if (snapshot.rc.Max.x > customRect.GetWidth() + 2)
                    snapshot.rc.Max.x = customRect.GetWidth() + 2;
                snapshot.time_stamp = i * snapshot_duration + mStartOffset;
                snapshot.duration = snapshot_duration;
                snapshot.frame_width = snapshot.rc.Max.x - snapshot.rc.Min.x;
                mVideoSnapshotInfos.push_back(snapshot);
            }
        }
    }
}

bool SequencerItem::DrawItemControlBar(ImDrawList *draw_list, ImRect rc, int sequenceOptions)
{
    bool need_update = false;
    ImGuiIO &io = ImGui::GetIO();
    if (mExpanded) draw_list->AddText(rc.Min + ImVec2(4, 0), IM_COL32_WHITE, mName.c_str());
    ImVec2 button_size = ImVec2(12, 12);
    int button_count = 0;
    if (sequenceOptions & SEQUENCER_LOCK)
    {
        bool ret = SequencerButton(draw_list, mLocked ? ICON_LOCKED : ICON_UNLOCK, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mLocked ? "unlock" : "lock");
        if (ret && io.MouseReleased[0])
            mLocked = !mLocked;
        button_count ++;
    }
    if (sequenceOptions & SEQUENCER_VIEW)
    {
        bool ret = SequencerButton(draw_list, mView ? ICON_VIEW : ICON_VIEW_DISABLE, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mView ? "hidden" : "view");
        if (ret && io.MouseReleased[0])
        {
            mView = !mView;
            need_update = true;
        }
        button_count ++;
    }
    if (sequenceOptions & SEQUENCER_MUTE)
    {
        bool ret = SequencerButton(draw_list, mMuted ? ICON_SPEAKER_MUTE : ICON_SPEAKER, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mMuted ? "voice" : "mute");
        if (ret && io.MouseReleased[0])
            mMuted = !mMuted;
        button_count ++;
    }
    if (sequenceOptions & SEQUENCER_EDIT_STARTEND)
    {
        bool ret = SequencerButton(draw_list, mCutting ? ICON_CUTTING : ICON_MOVING, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, mCutting ? "cutting" : "moving");
        if (ret && io.MouseReleased[0])
            mCutting = !mCutting;
        button_count ++;
    }
    if (sequenceOptions & SEQUENCER_RESTORE)
    {
        bool ret = SequencerButton(draw_list, ICON_ALIGN_START, ImVec2(rc.Min.x + button_size.x * button_count * 1.5 + 6, rc.Max.y - button_size.y - 2), button_size, "Align to start");
        if (ret && io.MouseReleased[0])
        {
            auto length = mEnd - mStart;
            mStart = 0;
            mEnd = mStart + length;
            need_update = true;
        }
        button_count ++;
    }
    return need_update;
}

void SequencerItem::SetClipSelected(ClipInfo* clip)
{
    for (auto it : mClips)
    {
        if (it->mStart == clip->mStart)
            it->bSelected = true;
        else
        {
            if (it->bSelected)
            {
                for (auto& snap : it->mVideoSnapshots)
                {
                    if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; }
                }
                it->mVideoSnapshots.clear();
                it->mFrameCount = 0;
            }
            it->bSelected = false;
            it->bPlay = false;
            it->mLastTime = -1;
            it->mFrameLock.lock();
            it->mFrame.clear();
            it->mFrameLock.unlock();
        }
    }
}

SequencerItem * SequencerItem::Load(const imgui_json::value& value, void * handle)
{
    // first get name and path to create new item
    SequencerItem * new_item = nullptr;
    MediaSequencer * sequencer = (MediaSequencer *)handle;
    if (!sequencer)
        return new_item;
    std::string name;
    std::string path;
    if (value.contains("Name"))
    {
        auto& val = value["Name"];
        if (val.is_string()) name = val.get<imgui_json::string>();
    }
    if (value.contains("Path"))
    {
        auto& val = value["Path"];
        if (val.is_string()) path = val.get<imgui_json::string>();
    }
    MediaItem * item = sequencer->FindMediaItemByName(name);
    if (item)
    {
        new_item = new SequencerItem(item->mName, item, item->mStart, item->mEnd, true, item->mMediaType);
    }

    // load item
    if (new_item)
    {
        // load global item info
        if (value.contains("Start"))
        {
            auto& val = value["Start"];
            if (val.is_number()) new_item->mStart = val.get<imgui_json::number>();
        }
        if (value.contains("End"))
        {
            auto& val = value["End"];
            if (val.is_number()) new_item->mEnd = val.get<imgui_json::number>();
        }
        if (value.contains("StartOffset"))
        {
            auto& val = value["StartOffset"];
            if (val.is_number()) new_item->mStartOffset = val.get<imgui_json::number>();
        }
        if (value.contains("EndOffset"))
        {
            auto& val = value["EndOffset"];
            if (val.is_number()) new_item->mEndOffset = val.get<imgui_json::number>();
        }
        if (value.contains("FrameInterval"))
        {
            auto& val = value["FrameInterval"];
            if (val.is_number()) new_item->mFrameInterval = val.get<imgui_json::number>();
        }
        if (value.contains("MediaType"))
        {
            auto& val = value["MediaType"];
            if (val.is_number()) new_item->mMediaType = val.get<imgui_json::number>();
        }
        if (value.contains("AudioChannels"))
        {
            auto& val = value["AudioChannels"];
            if (val.is_number()) new_item->mAudioChannels = val.get<imgui_json::number>();
        }
        if (value.contains("AudioSampleRate"))
        {
            auto& val = value["AudioSampleRate"];
            if (val.is_number()) new_item->mAudioSampleRate = val.get<imgui_json::number>();
        }
        if (value.contains("Expanded"))
        {
            auto& val = value["Expanded"];
            if (val.is_boolean()) new_item->mExpanded = val.get<imgui_json::boolean>();
        }
        if (value.contains("View"))
        {
            auto& val = value["View"];
            if (val.is_boolean()) new_item->mView = val.get<imgui_json::boolean>();
        }
        if (value.contains("Muted"))
        {
            auto& val = value["Muted"];
            if (val.is_boolean()) new_item->mMuted = val.get<imgui_json::boolean>();
        }
        if (value.contains("Cutting"))
        {
            auto& val = value["Cutting"];
            if (val.is_boolean()) new_item->mCutting = val.get<imgui_json::boolean>();
        }
        // load cut points
        const imgui_json::array* cutPointArray = nullptr;
        if (BluePrint::GetPtrTo(value, "CutPoint", cutPointArray))
        {
            for (auto& val : *cutPointArray)
            {
                if (val.is_number())
                {
                    int64_t point = val.get<imgui_json::number>();
                    new_item->mCutPoint.push_back(point);
                }
            }
        }
        // load clip points
        const imgui_json::array* clipArray = nullptr;
        if (BluePrint::GetPtrTo(value, "Clip", clipArray))
        {
            new_item->mClips.clear();
            for (auto& clip : *clipArray)
            {
                ClipInfo * new_clip = ClipInfo::Load(clip, new_item);
                if (new_clip)
                {
                    new_item->mClips.push_back(new_clip);
                }
            }
        }
    }

    return new_item;
}

void SequencerItem::Save(imgui_json::value& value)
{
    // first save clip info
    imgui_json::value clips;
    for (auto clip : mClips)
    {
        imgui_json::value clip_value;
        clip->Save(clip_value);
        clips.push_back(clip_value);
    }
    value["Clip"] = clips;

    // second save cut points
    imgui_json::value cuts;
    for (auto cut : mCutPoint)
    {
        imgui_json::value cut_value = imgui_json::number(cut);
        cuts.push_back(cut_value);
    }
    value["CutPoint"] = cuts;

    // third save item info
    value["Name"] = mName;
    value["Path"] = mPath;
    value["Start"] = imgui_json::number(mStart);
    value["End"] = imgui_json::number(mEnd);
    value["StartOffset"] = imgui_json::number(mStartOffset);
    value["EndOffset"] = imgui_json::number(mEndOffset);
    value["FrameInterval"] = imgui_json::number(mFrameInterval);
    value["MediaType"] = imgui_json::number(mMediaType);
    value["AudioChannels"] = imgui_json::number(mAudioChannels);
    value["AudioSampleRate"] = imgui_json::number(mAudioSampleRate);
    value["Expanded"] = imgui_json::boolean(mExpanded);
    value["View"] = imgui_json::boolean(mView);
    value["Muted"] = imgui_json::boolean(mMuted);
    value["Cutting"] = imgui_json::boolean(mCutting);
}


/***********************************************************************************************************
 * MediaSequencer Struct Member Functions
 ***********************************************************************************************************/
static inline bool CompareClip(ClipInfo* a, ClipInfo* b)
{
    return a->mStart < b->mStart;
}

static int thread_preview(MediaSequencer * sequencer)
{
    if (!sequencer)
        return -1;
    sequencer->mPreviewRunning = true;
    while (!sequencer->mPreviewDone)
    {
        if (sequencer->m_Items.empty() || sequencer->mFrame.size() >= MAX_SEQUENCER_FRAME_NUMBER)
        {
            ImGui::sleep((int)5);
            continue;
        }
        int64_t current_time = 0;
        sequencer->mFrameLock.lock();
        if (sequencer->mFrame.empty() || sequencer->bSeeking)
            current_time = sequencer->currentTime;
        else
        {
            auto it = sequencer->mFrame.end(); it--;
            current_time = it->time_stamp * 1000;
        }
        alignTime(current_time, sequencer->mFrameInterval);
        sequencer->mFrameLock.unlock();
        while (sequencer->mFrame.size() < MAX_SEQUENCER_FRAME_NUMBER)
        {
            if (sequencer->mPreviewDone)
                break;
            if (!sequencer->mFrame.empty())
            {
                int64_t buffer_start = sequencer->mFrame.begin()->time_stamp * 1000;
                int64_t buffer_end = sequencer->bForward ? buffer_start + sequencer->mFrameInterval * MAX_SEQUENCER_FRAME_NUMBER : 
                                                           buffer_start - sequencer->mFrameInterval * MAX_SEQUENCER_FRAME_NUMBER ;
                if (buffer_start > buffer_end)
                    std::swap(buffer_start, buffer_end);
                if (sequencer->currentTime < buffer_start - sequencer->mFrameInterval || sequencer->currentTime > buffer_end + sequencer->mFrameInterval)
                {
                    ImGui::sleep((int)5);
                    break;
                }
            }
            ImGui::ImMat mat;
            for (auto &item : sequencer->m_Items)
            {
                int64_t item_time = current_time - item->mStart + item->mStartOffset;
                alignTime(item_time, sequencer->mFrameInterval);
                if (item_time >= item->mStartOffset && item_time <= item->mLength - item->mEndOffset)
                {
                    bool valid_time = item->mView;
                    for (auto clip : item->mClips)
                    {
                        if (clip->bDragOut && item_time >= clip->mStart && item_time <= clip->mEnd)
                        {
                            valid_time = false;
                            break;
                        }
                    }
                    if (valid_time && item->mMediaReaderAudio && item->mMediaReaderAudio->IsOpened())
                    {
                        if ((item->mMediaReaderAudio->IsDirectionForward() && !sequencer->bForward) || 
                            (!item->mMediaReaderAudio->IsDirectionForward() && sequencer->bForward))
                            item->mMediaReaderAudio->SetDirection(sequencer->bForward);
                    }
                    if (valid_time && item->mMediaReaderVideo && item->mMediaReaderVideo->IsOpened())
                    {
                        if ((item->mMediaReaderVideo->IsDirectionForward() && !sequencer->bForward) || 
                            (!item->mMediaReaderVideo->IsDirectionForward() && sequencer->bForward))
                            item->mMediaReaderVideo->SetDirection(sequencer->bForward);
                        bool eof;
                        if (item->mMediaReaderVideo->ReadVideoFrame((float)item_time / 1000.0, mat, eof))
                            break;
                        else
                            mat.release();
                    }
                }
            }
            if (mat.empty())
            {
                mat = ImGui::ImMat(sequencer->mWidth, sequencer->mHeight, 4, 1u);
            }
            mat.time_stamp = (double)current_time / 1000.f;
            sequencer->mFrameLock.lock();
            sequencer->mFrame.push_back(mat);
            sequencer->mFrameLock.unlock();
            if (sequencer->bForward)
            {
                current_time += sequencer->mFrameInterval;
                if (current_time > sequencer->mEnd)
                {
                    if (sequencer->bLoop)
                    {
                        sequencer->mFrameLock.lock();
                        sequencer->mFrame.clear();
                        sequencer->mFrameLock.unlock();
                        current_time = sequencer->mStart;
                    }
                    else 
                        current_time = sequencer->mEnd;
                }
            }
            else
            {
                current_time -= sequencer->mFrameInterval;
                if (current_time < sequencer->mStart)
                {
                    if (sequencer->bLoop)
                    {
                        sequencer->mFrameLock.lock();
                        sequencer->mFrame.clear();
                        sequencer->mFrameLock.unlock();
                        current_time = sequencer->mEnd;
                    }
                    else
                        current_time = sequencer->mStart;
                }
            }
        }
    }
    sequencer->mPreviewRunning = false;
    return 0;
}

static int thread_video_filter(MediaSequencer * sequencer)
{
    if (!sequencer)
        return -1;

    sequencer->mVideoFilterRunning = true;
    while (!sequencer->mVideoFilterDone)
    {
        if (!sequencer->mVideoFilterBluePrint || !sequencer->mVideoFilterBluePrint->Blueprint_IsValid())
        {
            ImGui::sleep((int)5);
            continue;
        }
        sequencer->mSequencerLock.lock();
        int select_item = sequencer->selectedEntry;
        sequencer->mSequencerLock.unlock();
        if (sequencer->m_Items.empty() || select_item == -1 || select_item >= sequencer->m_Items.size())
        {
            ImGui::sleep((int)5);
            continue;
        }
        SequencerItem * item = sequencer->m_Items[select_item];
        
        ClipInfo * selected_clip = item->mClips[0];
        for (auto clip : item->mClips)
        {
            if (clip->bSelected)
            {
                selected_clip = clip;
                break;
            }
        }
        if (sequencer->mVideoFilterNeedUpdate)
        {
            selected_clip->mFrameLock.lock();
            selected_clip->mFrame.clear();
            selected_clip->mFrameLock.unlock();
            sequencer->mVideoFilterNeedUpdate = false;
        }
        if (selected_clip->mFrame.size() >= MAX_SEQUENCER_FRAME_NUMBER)
        {
            ImGui::sleep((int)5);
            continue;
        }
        int64_t current_time = 0;
        selected_clip->mFrameLock.lock();
        if (selected_clip->mFrame.empty() || selected_clip->bSeeking)
            current_time = selected_clip->mCurrent;
        else
        {
            auto it = selected_clip->mFrame.end(); it--;
            current_time = it->first.time_stamp * 1000;
        }
        alignTime(current_time, selected_clip->mFrameInterval);
        selected_clip->mFrameLock.unlock();
        while (selected_clip->mFrame.size() < MAX_SEQUENCER_FRAME_NUMBER)
        {
            if (sequencer->mVideoFilterDone)
                break;
            if (!selected_clip->mFrame.empty())
            {
                int64_t buffer_start = selected_clip->mFrame.begin()->first.time_stamp * 1000;
                int64_t buffer_end = selected_clip->bForward ? buffer_start + selected_clip->mFrameInterval * MAX_SEQUENCER_FRAME_NUMBER : 
                                                           buffer_start - selected_clip->mFrameInterval * MAX_SEQUENCER_FRAME_NUMBER ;
                if (buffer_start > buffer_end)
                    std::swap(buffer_start, buffer_end);
                if (selected_clip->mCurrent < buffer_start - selected_clip->mFrameInterval || selected_clip->mCurrent > buffer_end + selected_clip->mFrameInterval)
                {
                    ImGui::sleep((int)5);
                    break;
                }
            }
            
            std::pair<ImGui::ImMat, ImGui::ImMat> result;
            if ((item->mMediaReaderVideo->IsDirectionForward() && !selected_clip->bForward) ||
                (!item->mMediaReaderVideo->IsDirectionForward() && selected_clip->bForward))
                item->mMediaReaderVideo->SetDirection(selected_clip->bForward);
            bool eof;
            if (item->mMediaReaderVideo->ReadVideoFrame((float)current_time / 1000.0, result.first, eof))
            {
                result.first.time_stamp = (double)current_time / 1000.f;
                sequencer->mBluePrintLock.lock();
                if (sequencer->mVideoFilterBluePrint && 
                    sequencer->mVideoFilterBluePrint->Blueprint_Run(result.first, result.second))
                {
                    selected_clip->mFrameLock.lock();
                    selected_clip->mFrame.push_back(result);
                    selected_clip->mFrameLock.unlock();
                    if (selected_clip->bForward)
                    {
                        current_time += selected_clip->mFrameInterval;
                        if (current_time > selected_clip->mEnd)
                            current_time = selected_clip->mEnd;
                    }
                    else
                    {
                        current_time -= selected_clip->mFrameInterval;
                        if (current_time < selected_clip->mStart)
                        {
                            current_time = selected_clip->mStart;
                        }
                    }
                }
                sequencer->mBluePrintLock.unlock();
            }
        }
    }
    sequencer->mVideoFilterRunning = false;
    return 0;
}

MediaSequencer::MediaSequencer()
    : mStart(0), mEnd(0)
{
    timeStep = mFrameInterval;
    mPCMStream = new SequencerPcmStream(this);
    mAudioRender = CreateAudioRender();
    if (mAudioRender)
    {
        mAudioRender->OpenDevice(mAudioSampleRate, mAudioChannels, mAudioFormat, mPCMStream);
    }

    mVideoFilterBluePrint = new BluePrint::BluePrintUI();
    if (mVideoFilterBluePrint)
    {
        BluePrint::BluePrintCallbackFunctions callbacks;
        callbacks.BluePrintOnChanged = OnBluePrintChange;
        mVideoFilterBluePrint->Initialize();
        mVideoFilterBluePrint->SetCallbacks(callbacks, this);
    }

    mPreviewThread = new std::thread(thread_preview, this);
    mVideoFilterThread = new std::thread(thread_video_filter, this);
    for (int i = 0; i < mAudioChannels; i++)
        mAudioLevel.push_back(0);
}

MediaSequencer::~MediaSequencer()
{
    if (mPreviewThread && mPreviewThread->joinable())
    {
        mPreviewDone = true;
        mPreviewThread->join();
        delete mPreviewThread;
        mPreviewThread = nullptr;
        mPreviewDone = false;
    }
    if (mVideoFilterThread && mVideoFilterThread->joinable())
    {
        mVideoFilterDone = true;
        mVideoFilterThread->join();
        delete mVideoFilterThread;
        mVideoFilterThread = nullptr;
        mVideoFilterDone = false;
    }
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();

    for (auto item : m_Items) delete item;

    if (mAudioRender)
    {
        mAudioRender->CloseDevice();
        ReleaseAudioRender(&mAudioRender);
    }
    if (mPCMStream) { delete mPCMStream; mPCMStream = nullptr; }
    if (mMainPreviewTexture) { ImGui::ImDestroyTexture(mMainPreviewTexture); mMainPreviewTexture = nullptr; }
    mAudioLevel.clear();
    if (mVideoFilterBluePrint)
    {
        mVideoFilterBluePrint->Finalize();
        delete mVideoFilterBluePrint;
    }
    for (auto item : media_items) delete item;
}

int MediaSequencer::OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (!handle)
        return BluePrint::BP_CBR_Unknown;
    MediaSequencer * sequencer = (MediaSequencer *)handle;
    if (name.compare("VideoFilter") == 0)
    {
        if (type == BluePrint::BP_CB_Link ||
            type == BluePrint::BP_CB_Unlink ||
            type == BluePrint::BP_CB_NODE_DELETED)
        {
            sequencer->mVideoFilterNeedUpdate = true;
            ret = BluePrint::BP_CBR_AutoLink;
        }
        else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
                type == BluePrint::BP_CB_SETTING_CHANGED)
        {
            sequencer->mVideoFilterNeedUpdate = true;
        }
    }

    return ret;
}

ImGui::ImMat MediaSequencer::GetPreviewFrame()
{
    ImGui::ImMat frame;
    if (mCurrentPreviewTime == currentTime || mFrame.empty())
        return frame;
    double current_time = (double)currentTime / 1000.f;
    double buffer_start = mFrame.begin()->time_stamp;
    auto end_frame = mFrame.end(); end_frame--;
    double buffer_end = end_frame->time_stamp;
    if (buffer_start > buffer_end)
        std::swap(buffer_start, buffer_end);
    bool out_of_range = false;
    if (current_time < buffer_start || current_time > buffer_end)
        out_of_range = true;

    for (auto mat = mFrame.begin(); mat != mFrame.end();)
    {
        bool need_erase = false;
        if (bForward && mat->time_stamp < current_time)
        {
            need_erase = true;
        }
        else if (!bForward && mat->time_stamp > current_time)
        {
            need_erase = true;
        }

        if (need_erase || out_of_range)
        {
            // if we on seek stage, may output last frame for smooth preview
            if (bSeeking && mFrame.size() == 1)
            {
                frame = *mat;
                //mCurrentPreviewTime = currentTime; // Do we need update mCurrentPreviewTime?
            }
            mFrameLock.lock();
            mat = mFrame.erase(mat);
            mFrameLock.unlock();
        }
        else
        {
            frame = *mat;
            mCurrentPreviewTime = currentTime;
            break;
        }
    }

    return frame;
}

void MediaSequencer::Get(int index, int64_t& start, int64_t& end, int64_t& length, int64_t& start_offset, int64_t& end_offset, std::string& name, unsigned int& color)
{
    SequencerItem *item = m_Items[index];
    color = item->mColor;
    start = item->mStart;
    end = item->mEnd;
    length = item->mLength;
    start_offset = item->mStartOffset;
    end_offset = item->mEndOffset;
    name = item->mName;
}

void MediaSequencer::Get(int index, float& frame_duration, float& snapshot_width)
{
    SequencerItem *item = m_Items[index];
    frame_duration = item->mFrameDuration;
    snapshot_width = item->mSnapshotWidth;
}

void MediaSequencer::Get(int index, bool& expanded, bool& view, bool& locked, bool& muted, bool& cutting)
{
    SequencerItem *item = m_Items[index];
    expanded = item->mExpanded;
    view = item->mView;
    locked = item->mLocked;
    muted = item->mMuted;
    cutting = item->mCutting;
}

void MediaSequencer::Set(int index, int64_t start, int64_t end, int64_t start_offset, int64_t end_offset, std::string name, unsigned int color)
{
    SequencerItem *item = m_Items[index];
    item->mColor = color;
    //alignTime(start, mFrameInterval);
    item->mStart = start;
    //alignTime(end, mFrameInterval);
    item->mEnd = end;
    item->mName = name;
    //alignTime(start_offset, mFrameInterval);
    item->mStartOffset = start_offset;
    //alignTime(end_offset, mFrameInterval);
    item->mEndOffset = end_offset;
}

void MediaSequencer::Set(int index, bool expanded, bool view, bool locked, bool muted)
{
    SequencerItem *item = m_Items[index];
    item->mExpanded = expanded;
    item->mView = view;
    item->mLocked = locked;
    item->mMuted = muted;
}

void MediaSequencer::Set(int index, int64_t cutting_pos, bool add)
{
    SequencerItem *item = m_Items[index];
    if (!add)
    {
        // cutting_pos means mCutPoint index
        auto cutting_time = item->mCutPoint[cutting_pos];
        item->mCutPoint.erase(item->mCutPoint.begin() + cutting_pos);
        for (auto clip = item->mClips.begin(); clip != item->mClips.end();)
        {
            if ((*clip)->mEnd == cutting_time)
            {
                auto start_time = (*clip)->mStart;
                auto clip_delete = *clip;
                clip = item->mClips.erase(clip);
                delete clip_delete;
                (*clip)->mStart = start_time;
            }
            else 
                clip++;
        }
    }
    else
    {
        // cutting_pos means cutting point time
        bool found = false;
        for (auto point : item->mCutPoint)
        {
            if (cutting_pos == point)
                found = true;
        }
        if (!found)
        {
            item->mCutPoint.push_back(cutting_pos);
            sort(item->mCutPoint.begin(), item->mCutPoint.end());
            // found point in clips
            ClipInfo *new_clip = new ClipInfo(cutting_pos, item->mEnd, false, item);
            for (auto clip : item->mClips)
            {
                if (cutting_pos > clip->mStart && cutting_pos < clip->mEnd)
                {
                    new_clip->mEnd = clip->mEnd;
                    clip->mEnd = cutting_pos;
                    break;
                }
            }
            item->mClips.push_back(new_clip);
            std::sort(item->mClips.begin(), item->mClips.end(), CompareClip);
        }
    }
}

int MediaSequencer::Check(int index, int64_t& cutting_pos)
{
    int found = -1;
    SequencerItem *item = m_Items[index];
    for (int i = 0; i < item->mCutPoint.size(); i++)
    {
        auto point = item->mCutPoint[i];
        if (abs(cutting_pos - point) < 20)
        {
            found = i;
            cutting_pos = point;
            break;
        }
    }
    return found;
}

void MediaSequencer::Add(std::string& name)
{ 
    /*m_Items.push_back(SequencerItem{name, "", 0, 10, false});*/ 
}
    
void MediaSequencer::Del(int index)
{
    auto item = m_Items[index];
    m_Items.erase(m_Items.begin() + index);
    delete item;
    if (m_Items.size() == 0)
    {
        mStart = mEnd = 0;
        currentTime = firstTime = lastTime = visibleTime = 0;
    }
}
    
void MediaSequencer::Duplicate(int index)
{
    /*m_Items.push_back(m_Items[index]);*/
}

void MediaSequencer::SetItemSelected(int index)
{
    for (int i = 0; i < m_Items.size(); i++)
    {
        if (i == index)
            m_Items[i]->mSelected = true;
        else
        {
            m_Items[i]->mSelected = false;
            for (auto clip : m_Items[i]->mClips)
            {
                clip->bSelected = false;
                clip->bPlay = false;
                clip->mLastTime = -1;
                clip->mFrameLock.lock();
                clip->mFrame.clear();
                clip->mFrameLock.unlock();
            }
        }
    }
}

void MediaSequencer::CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &titleRect, const ImRect &clippingTitleRect, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, int64_t viewStartTime, int64_t visibleTime, float pixelWidth, bool need_update)
{
    // rc: full item length rect
    // titleRect: full item length title rect(same as Compact view rc)
    // clippingTitleRect: current view title area
    // clippingRect: current view window area
    // legendRect: legend area
    // legendClippingRect: legend area

    SequencerItem *item = m_Items[index];
    if (need_update) item->CalculateVideoSnapshotInfo(rc, viewStartTime, visibleTime);
    if (item->mVideoSnapshotInfos.size() == 0) return;
    float frame_width = item->mVideoSnapshotInfos[0].frame_width;
    int64_t lendth = item->mEnd - item->mStart;

    int64_t startTime = 0;
    if (item->mStart >= viewStartTime && item->mStart <= viewStartTime + visibleTime)
        startTime = 0;
    else if (item->mStart < viewStartTime && item->mEnd >= viewStartTime)
        startTime = viewStartTime - item->mStart;
    else
        startTime = viewStartTime + visibleTime;

    int64_t endTime = 0;
    if (item->mEnd >= viewStartTime && item->mEnd <= viewStartTime + visibleTime)
        endTime = item->mEnd;
    else if (item->mStart <= viewStartTime + visibleTime && item->mEnd > viewStartTime + visibleTime)
        endTime = viewStartTime + visibleTime;

    if (item->mSnapshot->HasVideo())
    {
        int total_snapshot = item->mVideoSnapshotInfos.size();
        int snapshot_index = floor((float)startTime / (float)lendth * (float)total_snapshot);
        int64_t snapshot_time = item->mVideoSnapshotInfos[snapshot_index].time_stamp;

        int max_snapshot = (clippingRect.GetWidth() + frame_width / 2) / frame_width + 1; // two more frame ?
        int snapshot_count = (snapshot_index + max_snapshot < total_snapshot) ? max_snapshot : total_snapshot - snapshot_index;

        if (need_update)
        {
            if (item->mSnapshotPos != snapshot_time)
            {
                item->mSnapshotPos = snapshot_time;
                item->SequencerItemUpdateSnapshots();
            }
            else
                item->SequencerItemUpdateSnapshots();
        }

        // draw video snapshot
        draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
        for (int i = 0; i < snapshot_count; i++)
        {
            if (i + snapshot_index > total_snapshot - 1) break;
            if (item->mVideoSnapshots.size() == 0) break;
            int64_t time_stamp = item->mVideoSnapshotInfos[snapshot_index + i].time_stamp;
            ImRect frame_rc = item->mVideoSnapshotInfos[snapshot_index + i].rc;
            ImVec2 pos = frame_rc.Min + rc.Min;
            ImVec2 size = frame_rc.Max - frame_rc.Min;
            if (i < item->mVideoSnapshots.size() && item->mVideoSnapshots[i].texture && item->mVideoSnapshots[i].available)
            {
                item->mVideoSnapshots[i].estimate_time = time_stamp;
                // already got snapshot
                ImGui::SetCursorScreenPos(pos);

                if (i == 0 && frame_rc.Min.x > rc.Min.x && snapshot_index > 0)
                {
                    // first snap pos over rc.Min
                    ImRect _frame_rc = item->mVideoSnapshotInfos[snapshot_index - 1].rc;
                    ImVec2 _pos = _frame_rc.Min + rc.Min;
                    ImVec2 _size = _frame_rc.Max - _frame_rc.Min;
                    draw_list->AddRectFilled(_pos, _pos + _size, IM_COL32_BLACK);
                }
                if (i + snapshot_index == total_snapshot - 1)
                {
                    // last frame of media, we need clip frame
                    float width_clip = size.x / frame_width;
                    ImGui::Image(item->mVideoSnapshots[i].texture, ImVec2(size.x, size.y), ImVec2(0, 0), ImVec2(width_clip, 1));
                }
                else if (pos.x + size.x < clippingRect.Max.x)
                {
                    ImGui::Image(item->mVideoSnapshots[i].texture, size);
                }
                else if (pos.x < clippingRect.Max.x)
                {
                    // last frame of view range, we need clip frame
                    float width_clip = (clippingRect.Max.x - pos.x) / size.x;
                    ImGui::Image(item->mVideoSnapshots[i].texture, ImVec2(clippingRect.Max.x - pos.x, size.y), ImVec2(0, 0), ImVec2(width_clip, 1));
                }
                time_stamp = item->mVideoSnapshots[i].time_stamp;
            }
            else if (i > 0 && snapshot_index + i == item->mVideoSnapshotInfos.size() - 1 && i >= item->mVideoSnapshots.size() - 1 && item->mVideoSnapshots[i - 1].available)
            {
                ImGui::SetCursorScreenPos(pos);
                float width_clip = size.x / frame_width;
                if (item->mVideoSnapshots[i - 1].texture)
                    ImGui::Image(item->mVideoSnapshots[i - 1].texture, ImVec2(size.x, size.y), ImVec2(0, 0), ImVec2(width_clip, 1));
            }
            else
            {
                // not got snapshot, we show circle indicalor
                draw_list->AddRectFilled(pos, pos + size, IM_COL32_BLACK);
                auto center_pos = pos + size / 2;
                ImVec4 color_main(1.0, 1.0, 1.0, 1.0);
                ImVec4 color_back(0.5, 0.5, 0.5, 1.0);
                ImGui::SetCursorScreenPos(center_pos - ImVec2(8, 8));
                ImGui::LoadingIndicatorCircle("Running", 1.0f, &color_main, &color_back);
                draw_list->AddRect(pos, pos + size, COL_FRAME_RECT);
            }
#ifdef DEBUG_SNAPSHOT
            auto time_string = MillisecToString(time_stamp, 3);
            ImGui::SetWindowFontScale(0.7);
            ImGui::PushStyleColor(ImGuiCol_TexGlyphShadow, ImVec4(0.1, 0.1, 0.1, 1.0));
            ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1,1));
            ImVec2 str_size = ImGui::CalcTextSize(time_string.c_str(), nullptr, true);
            if (str_size.x <= size.x)
            {
                draw_list->AddText(frame_rc.Min + rc.Min + ImVec2(2, 48), IM_COL32_WHITE, time_string.c_str());
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0);
#endif
        }
    }
    if (item->mSnapshot->HasAudio() && item->mWaveform)
    {
        int64_t start_pos = 0;
        if (item->mStart >= viewStartTime && item->mStart <= viewStartTime + visibleTime)
            start_pos = item->mStart - viewStartTime;
        else if (item->mStart < viewStartTime && item->mEnd >= viewStartTime)
            start_pos = 0;
        else
            start_pos = viewStartTime + visibleTime;
    
        int64_t end_pos = 0;
        if (item->mEnd >= viewStartTime && item->mEnd <= viewStartTime + visibleTime)
        {
            if (item->mStart >= viewStartTime)
                end_pos = item->mEnd - viewStartTime;
            else
                end_pos = item->mEnd - startTime - item->mStart;
        }
        else if (item->mEnd > viewStartTime + visibleTime)
            end_pos = viewStartTime + visibleTime;
        else
            end_pos = viewStartTime;

        int channels = item->mWaveform->pcm.size();
        int sampleSize = item->mWaveform->pcm[0].size();
        int startOff = (int)((double)(startTime)/1000.f/item->mWaveform->aggregateDuration);
        int windowLen = (int)((double)(end_pos - start_pos)/1000.f/item->mWaveform->aggregateDuration);
        if (startOff + windowLen > sampleSize)
            windowLen = sampleSize - startOff;
        float cursorStartOffset = clippingRect.Min.x + start_pos * pixelWidth;
        float cursorEndOffset = clippingRect.Min.x + end_pos * pixelWidth;
        // draw audio wave snapshot
        if (item->mSnapshot->HasVideo())
        {
            float wave_range = fmax(fabs(item->mWaveform->minSample), fabs(item->mWaveform->maxSample));
            // draw audio wave snapshot at bottom of video snapshot
            draw_list->AddRectFilled(ImVec2(cursorStartOffset, clippingRect.Max.y - 16), ImVec2(cursorEndOffset, clippingRect.Max.y), IM_COL32(128, 128, 128, 128));
            draw_list->AddRect(ImVec2(cursorStartOffset, clippingRect.Max.y - 16), ImVec2(cursorEndOffset, clippingRect.Max.y), IM_COL32(0, 0, 0, 128));
            ImGui::SetCursorScreenPos(ImVec2(cursorStartOffset, clippingRect.Max.y - 16));
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f,0.f, 1.0f));
            ImGui::PlotLines("##Waveform", item->mWaveform->pcm[0].data() + startOff, windowLen, 0, nullptr, -wave_range, wave_range, ImVec2(cursorEndOffset - cursorStartOffset, 16), sizeof(float), false);
            ImGui::PopStyleColor();
        }
        else
        {
            // draw audio wave snapshot at whole custom view
        }
    }

    // for Debug: print some info here 
    //int64_t current_item_time = currentTime - item->mStart + item->mStartOffset;
    //draw_list->AddText(clippingRect.Min + ImVec2(2,  8), IM_COL32_WHITE, std::to_string(current_item_time).c_str());
    //draw_list->AddText(clippingRect.Min + ImVec2(2, 24), IM_COL32_WHITE, std::to_string(item->mStartOffset).c_str());
    draw_list->PopClipRect();

    // draw legend
    draw_list->PushClipRect(legendRect.Min, legendRect.Max, true);
    draw_list->AddRect(legendRect.Min, legendRect.Max, COL_DEEP_DARK, 0, 0, 2);
    // draw media control
    auto need_seek = item->DrawItemControlBar(draw_list, legendRect, options);
    if (need_seek) Seek();
    
    draw_list->PopClipRect();

    // draw title bar
    draw_list->PushClipRect(clippingTitleRect.Min, clippingTitleRect.Max, true);
    if (item->mSelected)
        draw_list->AddRect(clippingTitleRect.Min, clippingTitleRect.Max, COL_SLOT_SELECTED);

    for (auto point : item->mCutPoint)
    {
        if (point > startTime && point < ImMin(viewStartTime + visibleTime, item->mEnd))
        {
            float cursorOffset = clippingTitleRect.Min.x + (point + item->mStart - viewStartTime) * pixelWidth;
            draw_list->AddLine(ImVec2(cursorOffset, clippingTitleRect.Min.y), ImVec2(cursorOffset, clippingTitleRect.Max.y), IM_COL32(0, 0, 0, 128), 2);
            ImGui::RenderArrowPointingAt(draw_list, ImVec2(cursorOffset, clippingTitleRect.Min.y + clippingTitleRect.GetHeight() / 2), ImVec2(4, 4), ImGuiDir_Left, IM_COL32(0, 0, 0, 255));
            ImGui::RenderArrowPointingAt(draw_list, ImVec2(cursorOffset, clippingTitleRect.Min.y + clippingTitleRect.GetHeight() / 2), ImVec2(4, 4), ImGuiDir_Right, IM_COL32(0, 0, 0, 255));
        }
    }
    draw_list->PopClipRect();

    // draw clip
    if (item->mClips.size() > 1)
    {
        ImGuiIO &io = ImGui::GetIO();
        draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
        bool mouse_clicked = false;
        for (auto clip : item->mClips)
        {
            bool draw_clip = false;
            float cursor_start = 0;
            float cursor_end  = 0;
            if (clip->mStart >= viewStartTime && clip->mEnd < viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x + (clip->mStart + item->mStart - viewStartTime) * pixelWidth;
                cursor_end = clippingRect.Min.x + (clip->mEnd + item->mStart - viewStartTime) * pixelWidth;
                draw_clip = true;
            }
            else if (clip->mStart >= viewStartTime && clip->mStart <= viewStartTime + visibleTime && clip->mEnd >= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x + (clip->mStart + item->mStart - viewStartTime) * pixelWidth;
                cursor_end = clippingRect.Max.x;
                draw_clip = true;
            }
            else if (clip->mStart <= viewStartTime && clip->mEnd <= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x;
                cursor_end = clippingRect.Min.x + (clip->mEnd + item->mStart - viewStartTime) * pixelWidth;
                draw_clip = true;
            }
            else if (clip->mStart <= viewStartTime && clip->mEnd >= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x;
                cursor_end  = clippingRect.Max.x;
                draw_clip = true;
            }

            if (draw_clip && cursor_end > cursor_start)
            {
                ImVec2 clip_pos_min = ImVec2(cursor_start, clippingRect.Min.y);
                ImVec2 clip_pos_max = ImVec2(cursor_end, clippingRect.Max.y);
                ImRect clip_rect(clip_pos_min, clip_pos_max);
                if (!clip->bDragOut)
                {
                    ImGui::SetCursorScreenPos(clip_pos_min);
                    auto frame_id_string = item->mPath + "@" + std::to_string(clip->mStart);
                    ImGui::BeginChildFrame(ImGui::GetID(("items_clips::" + frame_id_string).c_str()), clip_pos_max - clip_pos_min, ImGuiWindowFlags_NoScrollbar);
                    ImGui::InvisibleButton(frame_id_string.c_str(), clip_pos_max - clip_pos_min);
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                    {
                        ImGui::SetDragDropPayload("Clip_drag_drop", clip, sizeof(ClipInfo));
                        auto start_time_string = MillisecToString(clip->mStart, 3);
                        auto end_time_string = MillisecToString(clip->mEnd, 3);
                        auto length_time_string = MillisecToString(clip->mEnd - clip->mStart, 3);
                        ImGui::TextUnformatted((item)->mName.c_str());
                        ImGui::Text(" Start: %s", start_time_string.c_str());
                        ImGui::Text("   End: %s", end_time_string.c_str());
                        ImGui::Text("Length: %s", length_time_string.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (clip->bSelected)
                    {
                        draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(64,64,32,128));
                    }
                    if (ImGui::IsItemHovered())
                    {
                        draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(32,64,32,128));
                        if (!mouse_clicked && io.MouseClicked[0])
                        {
                            item->SetClipSelected(clip);
                            SetItemSelected(index);
                            mouse_clicked = true;
                        }
                    }
                    ImGui::EndChildFrame();
                }
                else
                {
                    draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(64,32,32,192));
                }
            }
        }
        draw_list->PopClipRect();
    }
}

void MediaSequencer::CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, int64_t viewStartTime, int64_t visibleTime, float pixelWidth)
{
    // rc: full item length rect
    // legendRect: legend area
    // clippingRect: current view window area

    SequencerItem *item = m_Items[index];
    int64_t startTime = 0;
    if (item->mStart >= viewStartTime && item->mStart <= viewStartTime + visibleTime)
        startTime = 0;
    else if (item->mStart < viewStartTime && item->mEnd >= viewStartTime)
        startTime = viewStartTime - item->mStart;
    else
        startTime = viewStartTime + visibleTime;

    // draw title bar
    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
    if (item->mSelected)
        draw_list->AddRect(clippingRect.Min, clippingRect.Max, COL_SLOT_SELECTED);
    for (auto point : item->mCutPoint)
    {
        if (point > startTime && point < ImMin(viewStartTime + visibleTime, item->mEnd))
        {
            float cursorOffset = clippingRect.Min.x + (point + item->mStart - viewStartTime) * pixelWidth;
            draw_list->AddLine(ImVec2(cursorOffset, clippingRect.Min.y), ImVec2(cursorOffset, clippingRect.Max.y), IM_COL32(0, 0, 0, 128), 2);
            ImGui::RenderArrowPointingAt(draw_list, ImVec2(cursorOffset, clippingRect.Min.y + clippingRect.GetHeight() / 2), ImVec2(4, 4), ImGuiDir_Left, IM_COL32(0, 0, 0, 255));
            ImGui::RenderArrowPointingAt(draw_list, ImVec2(cursorOffset, clippingRect.Min.y + clippingRect.GetHeight() / 2), ImVec2(4, 4), ImGuiDir_Right, IM_COL32(0, 0, 0, 255));
        }
    }
    // draw clip
    if (item->mClips.size() > 1)
    {
        ImGuiIO &io = ImGui::GetIO();
        bool mouse_clicked = false;
        for (auto clip : item->mClips)
        {
            bool draw_clip = false;
            float cursor_start = 0;
            float cursor_end  = 0;
            if (clip->mStart >= viewStartTime && clip->mEnd < viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x + (clip->mStart + item->mStart - viewStartTime) * pixelWidth;
                cursor_end = clippingRect.Min.x + (clip->mEnd + item->mStart - viewStartTime) * pixelWidth;
                draw_clip = true;
            }
            else if (clip->mStart >= viewStartTime && clip->mStart <= viewStartTime + visibleTime && clip->mEnd >= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x + (clip->mStart + item->mStart - viewStartTime) * pixelWidth;
                cursor_end = clippingRect.Max.x;
                draw_clip = true;
            }
            else if (clip->mStart <= viewStartTime && clip->mEnd <= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x;
                cursor_end = clippingRect.Min.x + (clip->mEnd + item->mStart - viewStartTime) * pixelWidth;
                draw_clip = true;
            }
            else if (clip->mStart <= viewStartTime && clip->mEnd >= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x;
                cursor_end  = clippingRect.Max.x;
                draw_clip = true;
            }
            if (draw_clip && cursor_end > cursor_start)
            {
                ImVec2 clip_pos_min = ImVec2(cursor_start, clippingRect.Min.y);
                ImVec2 clip_pos_max = ImVec2(cursor_end, clippingRect.Max.y);
                ImRect clip_rect(clip_pos_min, clip_pos_max);
                if (clip->bSelected)
                {
                    draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(64,64,32,128));
                }
                if (clip->bDragOut)
                {
                    draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(64,32,32,192));
                }
                else if (clip_rect.Contains(io.MousePos))
                {
                    draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(32,64,32,128));
                    if (!mouse_clicked && io.MouseClicked[0])
                    {
                        item->SetClipSelected(clip);
                        SetItemSelected(index);
                        mouse_clicked = true;
                    }
                }
            }
        }
    }

    draw_list->PopClipRect();
    //draw_list->AddRectFilled(clippingRect.Min, clippingRect.Max, IM_COL32(255,0, 0, 128), 0);

    // draw legend
    draw_list->PushClipRect(legendRect.Min, legendRect.Max, true);

    // draw media control
    auto need_seek = item->DrawItemControlBar(draw_list, legendRect, options);
    if (need_seek) Seek();
    
    draw_list->PopClipRect();
}

void MediaSequencer::SetCurrent(int64_t pos, bool rev)
{
    currentTime = pos;
    if (rev)
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

void MediaSequencer::Seek()
{
    mFrameLock.lock();
    mFrame.clear();
    mCurrentPreviewTime = -1;
    mFrameLock.unlock();
    if (mAudioRender)
    {
        if (bPlay) mAudioRender->Pause();
        mAudioRender->Flush();
    }
    for (auto item : m_Items)
    {
        int64_t item_time = currentTime - item->mStart + item->mStartOffset;
        if (item->mMediaReaderVideo && item->mMediaReaderVideo->IsOpened())
        {
            int64_t video_time = item_time;
            alignTime(video_time, mFrameInterval);
            item->mMediaReaderVideo->SeekTo((double)video_time / 1000.f);
        }
        if (item->mMediaReaderAudio && item->mMediaReaderAudio->IsOpened())
        {
            int64_t audio_time = item_time;
            item->mMediaReaderAudio->SeekTo((double)audio_time / 1000.f);
        }
    }
    if (mAudioRender)
    {
        if (bPlay) mAudioRender->Resume();
    }
}

void MediaSequencer::Play(bool play, bool forward)
{
    if (play) bForward = forward;
    bPlay = play;
    if (mAudioRender)
    {
        if (bPlay) mAudioRender->Resume();
        else
        {
            mAudioRender->Pause();
            for (int i = 0; i < mAudioLevel.size(); i++)
            {
                mAudioLevel[i] = 0;
            }
        }
    }
}

void MediaSequencer::Step(bool forward)
{
    if (!bPlay && m_Items.size() > 0)
    {
        bForward = forward;
        if (forward)
        {
            currentTime += mFrameInterval;
            if (currentTime > mEnd)
                currentTime = mEnd;
        }
        else
        {
            currentTime -= mFrameInterval;
            if (currentTime < mStart)
                currentTime = mStart;
        }
    }
}

void MediaSequencer::Loop(bool loop)
{
    bLoop = loop;
}

void MediaSequencer::ToStart()
{
    if (!bPlay && m_Items.size() > 0)
    {
        firstTime = mStart;
        currentTime = mStart;
        Seek();
    }
}

void MediaSequencer::ToEnd()
{
    if (!bPlay && m_Items.size() > 0)
    {
        if (mEnd - mStart - visibleTime > 0)
            firstTime = mEnd - visibleTime;
        else
            firstTime = mStart;
        currentTime = mEnd;
        Seek();
    }
}

int MediaSequencer::GetAudioLevel(int channel)
{
    if (channel < mAudioLevel.size())
        return mAudioLevel[channel];
    return 0;
}

int MediaSequencer::Load(const imgui_json::value& value)
{
    // first load media item
    const imgui_json::array* mediaItemArray = nullptr;
    if (BluePrint::GetPtrTo(value, "MediaItem", mediaItemArray))
    {
        for (auto& item : *mediaItemArray)
        {
            SequencerItem * media_item = SequencerItem::Load(item, this);
            if (media_item)
            {
                m_Items.push_back(media_item);
            }
        }
    }

    // second load global info
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
    if (value.contains("ItemHeight"))
    {
        auto& val = value["ItemHeight"];
        if (val.is_number()) mItemHeight = val.get<imgui_json::number>();
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
    if (value.contains("FrameInterval"))
    {
        auto& val = value["FrameInterval"];
        if (val.is_number()) mFrameInterval = val.get<imgui_json::number>();
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
    if (value.contains("FirstTime"))
    {
        auto& val = value["FirstTime"];
        if (val.is_number()) firstTime = val.get<imgui_json::number>();
    }
    if (value.contains("CurrentTime"))
    {
        auto& val = value["CurrentTime"];
        if (val.is_number()) currentTime = val.get<imgui_json::number>();
    }
    if (value.contains("Forward"))
    {
        auto& val = value["Forward"];
        if (val.is_boolean()) bForward = val.get<imgui_json::boolean>();
    }
    if (value.contains("Loop"))
    {
        auto& val = value["Loop"];
        if (val.is_boolean()) bLoop = val.get<imgui_json::boolean>();
    }
    Seek();
    return 0;
}

void MediaSequencer::Save(imgui_json::value& value)
{
    // first save media item
    imgui_json::value media_items;
    for (auto item : m_Items)
    {
        imgui_json::value media_item;
        item->Save(media_item);
        media_items.push_back(media_item);
    }
    value["MediaItem"] = media_items;

    // second save global timeline info
    value["Start"] = imgui_json::number(mStart);
    value["End"] = imgui_json::number(mEnd);
    value["ItemHeight"] = imgui_json::number(mItemHeight);
    value["VideoWidth"] = imgui_json::number(mWidth);
    value["VideoHeight"] = imgui_json::number(mHeight);
    value["FrameInterval"] = imgui_json::number(mFrameInterval);
    value["AudioChannels"] = imgui_json::number(mAudioChannels);
    value["AudioSampleRate"] = imgui_json::number(mAudioSampleRate);
    value["AudioFormat"] = imgui_json::number(mAudioFormat);
    value["FirstTime"] = imgui_json::number(firstTime);
    value["CurrentTime"] = imgui_json::number(currentTime);
    value["Forward"] = imgui_json::boolean(bForward);
    value["Loop"] = imgui_json::boolean(bLoop);
}

MediaItem* MediaSequencer::FindMediaItemByName(std::string name)
{
    for (auto media: media_items)
    {
        if (media->mName.compare(name) == 0)
            return media;
    }
    return nullptr;
}

/***********************************************************************************************************
 * ClipInfo Struct Member Functions
 ***********************************************************************************************************/
ClipInfo::ClipInfo(int64_t start, int64_t end, bool drag_out, void* handle)
{
    mID = ImGui::get_current_time_usec(); // sample using system time stamp for Clip ID
    mStart = mCurrent = start; 
    mEnd = end; 
    bDragOut = drag_out; 
    mItem = handle;
    mSnapshot = CreateMediaSnapshot();
    if (!mSnapshot || !mItem)
        return;
    SequencerItem * item = (SequencerItem *)mItem;
    MediaParserHolder holder = item->mSnapshot->GetMediaParser();
    if (!holder)
        return;
    mSnapshot->Open(holder);
    if (mSnapshot->IsOpened())
    {
        mFrameInterval = item->mFrameInterval;
        double window_size = 1.0f;
        mSnapshot->SetCacheFactor(1.0);
        mSnapshot->SetSnapshotResizeFactor(0.1, 0.1);
        mSnapshot->ConfigSnapWindow(window_size, 1);
    }
};

ClipInfo::~ClipInfo()
{
    ReleaseMediaSnapshot(&mSnapshot);
    for (auto& snap : mVideoSnapshots)
    {
        if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; }
    }
    mVideoSnapshots.clear();
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();
    if (mFilterInputTexture) { ImGui::ImDestroyTexture(mFilterInputTexture); mFilterInputTexture = nullptr; }
    if (mFilterOutputTexture) { ImGui::ImDestroyTexture(mFilterOutputTexture); mFilterOutputTexture = nullptr;  }
}

void ClipInfo::UpdateSnapshot()
{
    if (mSnapshot && mSnapshot->IsOpened() && mSnapshot->HasVideo())
    {
        std::vector<ImGui::ImMat> snapshots;
        double pos = (double)(mStart) / 1000.f;
        if (mSnapshot->GetSnapshots(snapshots, pos))
        {
            for (int i = 0; i < snapshots.size(); i++)
            {
                if (!snapshots[i].empty() && i >= mVideoSnapshots.size())
                {
                    Snapshot snap;
                    ImMatToTexture(snapshots[i], snap.texture);
                    snap.time_stamp = (int64_t)(snapshots[i].time_stamp * 1000);
                    snap.available = true;
                    mVideoSnapshots.push_back(snap);
                }
            }
        }
    }
}

void ClipInfo::Seek()
{
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();
    mLastTime = -1;
    mCurrentFilterTime = -1;
    SequencerItem * item = (SequencerItem *)mItem;
    if (item && item->mMediaReaderVideo && item->mMediaReaderVideo->IsOpened())
    {
        int64_t item_time = mCurrent - item->mStart + item->mStartOffset;
        alignTime(item_time, item->mFrameInterval);
        item->mMediaReaderVideo->SeekTo((double)item_time / 1000.f);
    }
}

bool ClipInfo::GetFrame(std::pair<ImGui::ImMat, ImGui::ImMat>& in_out_frame)
{
    if (mCurrentFilterTime == mCurrent || mFrame.empty())
        return false;
    double current_time = (double)mCurrent / 1000.f;
        
    double buffer_start = mFrame.begin()->first.time_stamp;
    double buffer_end = bForward ? buffer_start + mFrameInterval * MAX_SEQUENCER_FRAME_NUMBER : 
                                    buffer_start - mFrameInterval * MAX_SEQUENCER_FRAME_NUMBER ;
    if (buffer_start > buffer_end)
        std::swap(buffer_start, buffer_end);

    bool out_of_range = false;
    if (current_time < buffer_start || current_time > buffer_end)
        out_of_range = true;

    for (auto pair = mFrame.begin(); pair != mFrame.end();)
    {
        bool need_erase = false;
        int64_t time_diff = fabs(pair->first.time_stamp - current_time) * 1000;
        if (time_diff > mFrameInterval)
            need_erase = true;

        if (need_erase || out_of_range)
        {
            // if we on seek stage, may output last frame for smooth preview
            if (bSeeking && mFrame.size() == 1)
            {
                in_out_frame = *pair;
            }
            mFrameLock.lock();
            pair = mFrame.erase(pair);
            mFrameLock.unlock();
        }
        else
        {
            in_out_frame = *pair;
            mCurrentFilterTime = current_time;
            if (bPlay)
            {
                bool need_step_time = false;
                int64_t current_system_time = ImGui::get_current_time_usec() / 1000;
                if (mLastTime != -1)
                {
                    int64_t step_time = current_system_time - mLastTime;
                    if (step_time >= mFrameInterval)
                        need_step_time = true;
                }
                if (need_step_time)
                {
                    if (bForward)
                    {
                        mCurrent += mFrameInterval;
                        if (mCurrent > mEnd)
                            mCurrent = mEnd;
                    }
                    else
                    {
                        mCurrent -= mFrameInterval;
                        if (mCurrent < mStart)
                            mCurrent = mStart;
                    }
                }
                mLastTime = current_system_time;
            }
            else
            {
                mLastTime = -1;
                mCurrentFilterTime = -1;
            }
            break;
        }
    }
    return out_of_range ? false : true;
}

ClipInfo * ClipInfo::Load(const imgui_json::value& value, void * handle)
{
    // first get id, start and end to create new clip
    ClipInfo * new_clip = nullptr;
    SequencerItem * item = (SequencerItem *)handle;
    if (!item)
        return new_clip;
    int64_t start = 0;
    int64_t end = 0;
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
    new_clip = new ClipInfo(start, end, false, item);

    // load item
    if (new_clip)
    {
        // load global clip info
        if (value.contains("ID"))
        {
            auto& val = value["ID"];
            if (val.is_number()) new_clip->mID = val.get<imgui_json::number>();
        }
        if (value.contains("Current"))
        {
            auto& val = value["Current"];
            if (val.is_number()) new_clip->mCurrent = val.get<imgui_json::number>();
        }
        if (value.contains("Forward"))
        {
            auto& val = value["Forward"];
            if (val.is_boolean()) new_clip->bForward = val.get<imgui_json::boolean>();
        }
        if (value.contains("DragOut"))
        {
            auto& val = value["DragOut"];
            if (val.is_boolean()) new_clip->bDragOut = val.get<imgui_json::boolean>();
        }
        // load video filter bp
        if (value.contains("VideoFilterBP"))
        {
            auto& val = value["VideoFilterBP"];
            if (val.is_object()) new_clip->mVideoFilterBP = val;
        }
        // load video transition bp
        if (value.contains("VideoTransitionBP"))
        {
            auto& val = value["VideoTransitionBP"];
            if (val.is_object()) new_clip->mFusionBP = val;
        }
        // load audio filter bp
        if (value.contains("AudioFilterBP"))
        {
            auto& val = value["AudioFilterBP"];
            if (val.is_object()) new_clip->mAudioFilterBP = val;
        }
        new_clip->Seek();
    }

    return new_clip;
}

void ClipInfo::Save(imgui_json::value& value)
{
    // save clip video filter bp
    if (mVideoFilterBP.is_object())
    {
        value["VideoFilterBP"] = mVideoFilterBP;
    }
    // save clip video transition bp
    if (mFusionBP.is_object())
    {
        value["VideoTransitionBP"] = mFusionBP;
    }
    // save clip audio filter bp
    if (mAudioFilterBP.is_object())
    {
        value["AudioFilterBP"] = mAudioFilterBP;
    }

    // save clip global info
    value["ID"] = imgui_json::number(mID);
    value["Start"] = imgui_json::number(mStart);
    value["End"] = imgui_json::number(mEnd);
    value["Current"] = imgui_json::number(mCurrent);
    value["Forward"] = imgui_json::boolean(bForward);
    value["DragOut"] = imgui_json::boolean(bDragOut);
}
/***********************************************************************************************************
 * SequencerPcmStream class Member Functions
 ***********************************************************************************************************/
template<typename T>
static int calculate_audio_db(const T* data, int channels, int channel_index, size_t length, const float max_level) 
{
    static const float kMaxSquaredLevel = max_level * max_level;
    constexpr float kMinLevel = -96.f;
    float sum_square_ = 0;
    size_t sample_count_ = 0;
    for (size_t i = 0; i < length; i += channels) 
    {
        T audio_data = data[i + channel_index];
        sum_square_ += audio_data * audio_data;
    }
    sample_count_ += length / channels;
    float rms = sum_square_ / (sample_count_ * kMaxSquaredLevel);
    rms = 10 * log10(rms);
    if (rms < kMinLevel)
        rms = kMinLevel;
    rms = -kMinLevel + rms;
    return static_cast<int>(rms + 0.5);
}

uint32_t SequencerPcmStream::Read(uint8_t* buff, uint32_t buffSize, bool blocking)
{
    if (!m_sequencer)
        return 0;
    uint32_t readSize = buffSize;
    int64_t current_time = m_sequencer->currentTime;
    bool out_audio = false;
    for (auto &item : m_sequencer->m_Items)
    {
        int64_t item_time = current_time - item->mStart + item->mStartOffset;
        if (item_time >= item->mStartOffset && item_time <= item->mLength - item->mEndOffset)
        {
            bool valid_time = true;
            for (auto clip : item->mClips)
            {
                if (clip->bDragOut && item_time >= clip->mStart && item_time <= clip->mEnd)
                {
                    valid_time = false;
                    break;
                }
            }
            if (valid_time && item->mMediaReaderAudio && item->mMediaReaderAudio->IsOpened())
            {
                double pos;
                if (item->mMediaReaderAudio->ReadAudioSamples(buff, readSize, pos, blocking))
                {
                    for (int i = 0; i < m_sequencer->mAudioLevel.size(); i++)
                    {
                        m_sequencer->mAudioLevel[i] = calculate_audio_db((float*)buff, m_sequencer->mAudioLevel.size(), i, readSize / sizeof(float), 1.0);
                    }
                    out_audio = item->mMuted ? false : true;
                    break;
                }
            }
        }
        // TODO::Mix all item's Audio
    }
    if (!out_audio)
        return 0;
    return readSize;
}
} // namespace ImSequencer
