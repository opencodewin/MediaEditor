#include "ImSequencer.h"
#include <imgui_helper.h>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ImSequencer
{

std::string MillisecToString(int64_t millisec, int show_millisec = 0)
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

bool Sequencer(SequencerInterface *sequencer, bool *expanded, int *selectedEntry, int sequenceOptions)
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
            SequencerItem * new_item = new SequencerItem(item->mName, item->mMediaOverview->GetMediaParser(), item->mStart, item->mEnd, true, item->mMediaType);
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
                SequencerItem * new_item = new SequencerItem(item->mName, item->mMedia->GetMediaParser(), start, end, true, item->mMediaType);
                new_item->mStartOffset = clip->mStart - item->mStartOffset;
                new_item->mEndOffset = item->mLength - clip->mEnd;
                seq->m_Items.push_back(new_item);
                for (auto &c : item->mClips)
                {
                    if (c.mStart == clip->mStart)
                    {
                        c.mDragOut = true;
                        break;
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
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x - 8.f, canvas_pos.y + ItemHeight), COL_CANVAS_BG, 0);
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
        draw_list->AddRectFilled(canvas_pos, canvas_pos + headerSize, IM_COL32_BLACK, 0);
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
        }
        if (MovingCurrentTime)
        {
            if (duration)
            {
                sequencer->currentTime = (int64_t)((io.MousePos.x - topRect.Min.x) / msPixelWidth) + firstTimeUsed;
                if (sequencer->currentTime < sequencer->GetStart())
                    sequencer->currentTime = sequencer->GetStart();
                if (sequencer->currentTime >= sequencer->GetEnd())
                    sequencer->currentTime = sequencer->GetEnd();
                sequencer->Seek(); // call seek event
            }
            if (!io.MouseDown[0])
                MovingCurrentTime = false;
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
                //draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)HeadHeight), ImVec2((float)px, canvas_pos.y + (float)regionHeight - 1), COL_MARK_HALF, 1);
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
        // selection
        bool selected = selectedEntry && (*selectedEntry >= 0);
        if (selected)
        {
            customHeight = 0;
            for (int i = 0; i < *selectedEntry; i++)
                customHeight += sequencer->GetCustomHeight(i);
            draw_list->AddRectFilled(ImVec2(contentMin.x, contentMin.y + ItemHeight * *selectedEntry + customHeight), ImVec2(contentMin.x + canvas_size.x - 8.f, contentMin.y + ItemHeight * (*selectedEntry + 1) + customHeight), COL_SLOT_SELECTED, 1.f);
        }
        
        // slots
        customHeight = 0;
        for (int i = 0; i < itemCount; i++)
        {
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
                if (selectedEntry)
                    *selectedEntry = movingEntry;

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
                if (!diffTime && movingPart && selectedEntry)
                {
                    *selectedEntry = movingEntry;
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
                    sequencer->firstTime = ImClamp(sequencer->firstTime, sequencer->GetStart(), ImMax(sequencer->GetEnd() - sequencer->visibleTime, sequencer->GetStart()));
                }
                if (io.MouseWheelH > FLT_EPSILON)
                {
                    sequencer->firstTime += sequencer->visibleTime / 4;
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
        if (selectedEntry && (*selectedEntry == delEntry || *selectedEntry >= sequencer->GetItemCount()))
            *selectedEntry = -1;
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
            last_time = start_time;
        }
        else
        {
            int64_t current_time = ImGui::get_current_time_usec() / 1000;
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
                    if (snapshots[i].device == ImDataDevice::IM_DD_CPU)
                    {
                        ImGui::ImGenerateOrUpdateTexture(thumb, snapshots[i].w, snapshots[i].h, snapshots[i].c, (const unsigned char *)snapshots[i].data);
                    }
#if IMGUI_VULKAN_SHADER
                    if (snapshots[i].device == ImDataDevice::IM_DD_VULKAN)
                    {
                        ImGui::VkMat vkmat = snapshots[i];
                        ImGui::ImGenerateOrUpdateTexture(thumb, vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                    }
#endif
                    mMediaThumbnail.push_back(thumb);
                }
            }
        }
    }
}

/***********************************************************************************************************
 * SequencerItem Struct Member Functions
 ***********************************************************************************************************/

SequencerItem::SequencerItem(const std::string& name, const std::string& path, int64_t start, int64_t end, bool expand, int type)
{
    mName = name;
    mPath = path;
    mStart = start;
    mEnd = end;
    mExpanded = expand;
    mMediaType = type;
    mSnapshot = CreateMediaSnapshot();
    mMedia = CreateMediaReader();
    if (!mSnapshot || !mMedia)
        return;
    mColor = COL_SLOT_DEFAULT;
    if (!path.empty() && mSnapshot)
    {
        mSnapshot->Open(path);
    }
    if (mSnapshot && mSnapshot->IsOpened())
    {
        mMedia->Open(mSnapshot->GetMediaParser());
        double window_size = 1.0f;
        mLength = mSnapshot->GetVideoDuration();
        mSnapshot->SetCacheFactor(16.0);
        mSnapshot->SetSnapshotResizeFactor(0.1, 0.1);
        mSnapshot->ConfigSnapWindow(window_size, 5);
        if (mEnd > mStart + mLength)
            mEnd = mLength - mStart;
    }
    mClips.push_back(ClipInfo(mStart, mEnd, false, this));
}

SequencerItem::SequencerItem(const std::string& name, MediaParserHolder holder, int64_t start, int64_t end, bool expand, int type)
{
    mName = name;
    mPath = holder->GetUrl();
    mStart = start;
    mEnd = end;
    mExpanded = expand;
    mMediaType = type;
    mSnapshot = CreateMediaSnapshot();
    mMedia = CreateMediaReader();
    if (!mSnapshot || !mMedia)
        return;
    mColor = COL_SLOT_DEFAULT;
    mSnapshot->Open(holder);
    mMedia->Open(holder);
    if (mSnapshot && mSnapshot->IsOpened())
    {
        double window_size = 1.0f;
        mLength = mSnapshot->GetVideoDuration();
        mSnapshot->SetCacheFactor(16.0);
        mSnapshot->SetSnapshotResizeFactor(0.1, 0.1);
        mSnapshot->ConfigSnapWindow(window_size, 5);
        if (mEnd > mStart + mLength)
            mEnd = mLength - mStart;
    }
    mClips.push_back(ClipInfo(mStart, mEnd, false, this));
}

SequencerItem::~SequencerItem()
{
    ReleaseMediaSnapshot(&mSnapshot);
    ReleaseMediaReader(&mMedia);
    mSnapshot = nullptr;
    mMedia = nullptr;
    for (auto& snap : mVideoSnapshots)
    {
        if (snap.texture) { ImGui::ImDestroyTexture(snap.texture); snap.texture = nullptr; } 
    }
}

void SequencerItem::SequencerItemUpdateSnapshots()
{
    if (mSnapshot && mSnapshot->IsOpened())
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
                            if (snapshots[i].device == ImDataDevice::IM_DD_CPU)
                            {
                                ImGui::ImGenerateOrUpdateTexture(mVideoSnapshots[media_snapshot_index].texture, snapshots[i].w, snapshots[i].h, snapshots[i].c, (const unsigned char *)snapshots[i].data);
                            }
#if IMGUI_VULKAN_SHADER
                            if (snapshots[i].device == ImDataDevice::IM_DD_VULKAN)
                            {
                                ImGui::VkMat vkmat = snapshots[i];
                                ImGui::ImGenerateOrUpdateTexture(mVideoSnapshots[media_snapshot_index].texture, vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                            }
#endif
                            mVideoSnapshots[media_snapshot_index].time_stamp = (int64_t)(snapshots[i].time_stamp * 1000);
                            mVideoSnapshots[media_snapshot_index].available = true;
                        }
                    }
                    else
                    {
                        Snapshot snap;
                        if (snapshots[i].device == ImDataDevice::IM_DD_CPU)
                        {
                            ImGui::ImGenerateOrUpdateTexture(snap.texture, snapshots[i].w, snapshots[i].h, snapshots[i].c, (const unsigned char *)snapshots[i].data);
                        }
#if IMGUI_VULKAN_SHADER
                        if (snapshots[i].device == ImDataDevice::IM_DD_VULKAN)
                        {
                            ImGui::VkMat vkmat = snapshots[i];
                            ImGui::ImGenerateOrUpdateTexture(snap.texture, vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                        }
#endif
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
        auto width = mSnapshot->GetVideoWidth();
        auto height = mSnapshot->GetVideoHeight();
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

/***********************************************************************************************************
 * MediaSequencer Struct Member Functions
 ***********************************************************************************************************/
static inline bool CompareClip(ClipInfo& a, ClipInfo& b)
{
    return a.mStart < b.mStart;
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
        if (sequencer->mFrame.empty())
            current_time = floor(sequencer->currentTime / sequencer->mFrameDuration) * sequencer->mFrameDuration;
        else
        {
            auto it = sequencer->mFrame.end(); it--;
            current_time = it->time_stamp * 1000;
        }
        sequencer->mFrameLock.unlock();
        while (sequencer->mFrame.size() < MAX_SEQUENCER_FRAME_NUMBER)
        {
            ImGui::ImMat mat;
            for (auto &item : sequencer->m_Items)
            {
                int64_t item_time = current_time - item->mStart + item->mStartOffset;
                if (item_time >= item->mStart && item_time <= item->mEnd)
                {
                    bool valid_time = item->mView;
                    for (auto clip : item->mClips)
                    {
                        if (clip.mDragOut && item_time >= clip.mStart && item_time <= clip.mEnd)
                        {
                            valid_time = false;
                            break;
                        }
                    }
                    if (valid_time && item->mMedia->IsOpened())
                    {
                        item->mMedia->ReadFrame((float)item_time / 1000.0, mat);
                        if (!mat.empty())
                            break;
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
                current_time += sequencer->mFrameDuration;
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
                current_time -= sequencer->mFrameDuration;
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

MediaSequencer::MediaSequencer()
    : mStart(0), mEnd(0)
{
    mPreviewThread = new std::thread(thread_preview, this);
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
    mFrameLock.lock();
    mFrame.clear();
    mFrameLock.unlock();
    for (auto item : m_Items)
    {
        delete item;
    }
    if (mMainPreviewTexture) { ImGui::ImDestroyTexture(mMainPreviewTexture); mMainPreviewTexture = nullptr; }
}

ImGui::ImMat MediaSequencer::GetPreviewFrame()
{
    ImGui::ImMat frame;
    if (mCurrentPreviewTime == currentTime)
        return frame;
    double current_time = (double)currentTime / 1000.f;

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

        if (need_erase)
        {
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
    item->mStart = start;
    item->mEnd = end;
    item->mName = name;
    item->mStartOffset = start_offset;
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
            if (clip->mEnd == cutting_time)
            {
                auto start_time =  clip->mStart;
                clip = item->mClips.erase(clip);
                clip->mStart = start_time;
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
            ClipInfo new_clip(cutting_pos, item->mEnd, false, item);
            for (auto &clip : item->mClips)
            {
                if (cutting_pos > clip.mStart && cutting_pos < clip.mEnd)
                {
                    new_clip.mEnd = clip.mEnd;
                    clip.mEnd = cutting_pos;
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
    if (item->mVideoSnapshotInfos.size()  == 0) return;
    float frame_width = item->mVideoSnapshotInfos[0].frame_width;
    int64_t lendth = item->mEnd - item->mStart;

    int64_t startTime = 0;
    if (item->mStart >= viewStartTime && item->mStart <= viewStartTime + visibleTime)
        startTime = 0;
    else if (item->mStart < viewStartTime && item->mEnd >= viewStartTime)
        startTime = viewStartTime - item->mStart;
    else
        startTime = viewStartTime + visibleTime;

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

    // draw snapshot
    /*
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
                ImGui::Image(item->mVideoSnapshots[i].texture, size);
            else if (pos.x < clippingRect.Max.x)
            {
                // last frame of view range, we need clip frame
                float width_clip = (clippingRect.Max.x - pos.x) / size.x;
                ImGui::Image(item->mVideoSnapshots[i].texture, ImVec2(clippingRect.Max.x - pos.x, size.y), ImVec2(0, 0), ImVec2(width_clip, 1));
            }
            time_stamp = item->mVideoSnapshots[i].time_stamp;
        }
        else if (i > 0 && snapshot_index + i == item->mVideoSnapshotInfos.size() - 1 && i >= item->mVideoSnapshots.size() && item->mVideoSnapshots[i - 1].available)
        {
            ImGui::SetCursorScreenPos(pos);
            float width_clip = size.x / frame_width;
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
        
        auto time_string = MillisecToString(time_stamp, 3);
        //auto esttime_str = MillisecToString(item->mVideoSnapshotInfos[snapshot_index + i].time_stamp, 3);
        ImGui::SetWindowFontScale(0.7);
        ImGui::PushStyleColor(ImGuiCol_TexGlyphShadow, ImVec4(0.1, 0.1, 0.1, 1.0));
        ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1,1));
        ImVec2 str_size = ImGui::CalcTextSize(time_string.c_str(), nullptr, true);
        if (str_size.x <= size.x)
        {
            //draw_list->AddText(frame_rc.Min + rc.Min + ImVec2(2, 32), IM_COL32_WHITE, esttime_str.c_str());
            draw_list->AddText(frame_rc.Min + rc.Min + ImVec2(2, 48), IM_COL32_WHITE, time_string.c_str());
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0);
    }

    // for Debug: print some info here 
    //int64_t current_item_time = currentTime - item->mStart + item->mStartOffset;
    //draw_list->AddText(clippingRect.Min + ImVec2(2,  8), IM_COL32_WHITE, std::to_string(current_item_time).c_str());
    //draw_list->AddText(clippingRect.Min + ImVec2(2, 24), IM_COL32_WHITE, std::to_string(item->mStartOffset).c_str());
    draw_list->PopClipRect();
    */

    // draw legend
    draw_list->PushClipRect(legendRect.Min, legendRect.Max, true);

    // draw media control
    auto need_seek = item->DrawItemControlBar(draw_list, legendRect, options);
    if (need_seek) Seek();
    
    draw_list->PopClipRect();

    // draw title bar
    draw_list->PushClipRect(clippingTitleRect.Min, clippingTitleRect.Max, true);
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
        for (auto clip : item->mClips)
        {
            bool draw_clip = false;
            float cursor_start = 0;
            float cursor_end  = 0;
            if (clip.mStart >= viewStartTime && clip.mEnd < viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x + (clip.mStart + item->mStart - viewStartTime) * pixelWidth;
                cursor_end = clippingRect.Min.x + (clip.mEnd + item->mStart - viewStartTime) * pixelWidth;
                draw_clip = true;
            }
            else if (clip.mStart >= viewStartTime && clip.mStart <= viewStartTime + visibleTime && clip.mEnd >= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x + (clip.mStart + item->mStart - viewStartTime) * pixelWidth;
                cursor_end = clippingRect.Max.x;
                draw_clip = true;
            }
            else if (clip.mStart <= viewStartTime && clip.mEnd <= viewStartTime + visibleTime)
            {
                cursor_start = clippingRect.Min.x;
                cursor_end = clippingRect.Min.x + (clip.mEnd + item->mStart - viewStartTime) * pixelWidth;
                draw_clip = true;
            }
            else if (clip.mStart <= viewStartTime && clip.mEnd >= viewStartTime + visibleTime)
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
                if (!clip.mDragOut)
                {
                    ImGui::SetCursorScreenPos(clip_pos_min);
                    auto frame_id_string = item->mPath + "@" + std::to_string(clip.mStart);
                    ImGui::BeginChildFrame(ImGui::GetID(("items_clips::" + frame_id_string).c_str()), clip_pos_max - clip_pos_min, ImGuiWindowFlags_NoScrollbar);
                    ImGui::InvisibleButton(frame_id_string.c_str(), clip_pos_max - clip_pos_min);
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                    {
                        ImGui::SetDragDropPayload("Clip_drag_drop", &clip, sizeof(ClipInfo));
                        auto start_time_string = MillisecToString(clip.mStart, 3);
                        auto end_time_string = MillisecToString(clip.mEnd, 3);
                        auto length_time_string = MillisecToString(clip.mEnd - clip.mStart, 3);
                        ImGui::TextUnformatted((item)->mName.c_str());
                        ImGui::Text(" Start: %s", start_time_string.c_str());
                        ImGui::Text("   End: %s", end_time_string.c_str());
                        ImGui::Text("Length: %s", length_time_string.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::IsItemHovered())
                    {
                        draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(32,64,32,128));
                    }
                    ImGui::EndChildFrame();
                }
                else //if (clip_rect.Contains(io.MousePos))
                {
                    draw_list->AddRectFilled(clip_pos_min, clip_pos_max, IM_COL32(64,32,32,192));
                }
            }
        }
        draw_list->PopClipRect();
    }

    //ImGui::SetCursorScreenPos(rc.Min);
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

    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
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
    draw_list->PopClipRect();
    //draw_list->AddRectFilled(clippingRect.Min, clippingRect.Max, IM_COL32(255,0, 0, 128), 0);

    // draw legend
    draw_list->PushClipRect(legendRect.Min, legendRect.Max, true);

    // draw media control
    auto need_seek = item->DrawItemControlBar(draw_list, legendRect, options);
    if (need_seek) Seek();
    
    draw_list->PopClipRect();
}

void MediaSequencer::GetVideoSnapshotInfo(int index, std::vector<VideoSnapshotInfo>& snapshots)
{
    SequencerItem *item = m_Items[index];
    snapshots = item->mVideoSnapshotInfos;
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
    for (auto item : m_Items)
    {
        if (item->mMedia && item->mMedia->IsOpened())
        {
            int64_t item_time = currentTime - item->mStart + item->mStartOffset;
            item->mMedia->SeekTo((double)item_time / 1000.f);
        }
    }
}

} // namespace ImSequencer
