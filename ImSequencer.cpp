#include "ImSequencer.h"
#include <imgui_helper.h>
#include <cmath>
#include <sstream>
#include <iomanip>

#define COL_LIGHT_BLUR      IM_COL32( 16, 128, 255, 255)
#define COL_CANVAS_BG       IM_COL32( 36,  36,  36, 255)
#define COL_LEGEND_BG       IM_COL32( 18,  18,  18, 255)
#define COL_MARK            IM_COL32( 96,  96,  96, 255)
#define COL_RULE_TEXT       IM_COL32(188, 188, 188, 255)
#define COL_SLOT_DEFAULT    IM_COL32(128, 128, 170, 255)
#define COL_SLOT_ODD        IM_COL32( 58,  58,  58, 255)
#define COL_SLOT_EVEN       IM_COL32( 64,  64,  64, 255)
#define COL_SLOT_SELECTED   IM_COL32( 64,  64, 255, 128)
#define COL_SLOT_V_LINE     IM_COL32( 96,  96,  96,  48)
#define COL_SLIDER_BG       IM_COL32( 16,  16,  16, 255)
#define COL_SLIDER_IN       IM_COL32( 96,  96,  96, 255)
#define COL_SLIDER_MOVING   IM_COL32( 80,  80,  80, 255)
#define COL_SLIDER_HANDLE   IM_COL32(112, 112, 112, 255)
#define COL_SLIDER_SIZING   IM_COL32(170, 170, 170, 255)
#define COL_CURSOR_ARROW    IM_COL32(  0, 255,   0, 255)
#define COL_CURSOR_TEXT_BG  IM_COL32(  0, 128,   0, 144)
#define COL_CURSOR_TEXT     IM_COL32(  0, 255,   0, 255)

#define HALF_COLOR(c)       (c & 0xffffff) | 0x40000000;

namespace ImSequencer
{
std::string MillisecToString(int64_t millisec, bool show_millisec = false)
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
        if (show_millisec)
            oss << '.' << std::setw(3) << milli;
    }
    else
    {
        oss << std::setfill('0') << std::setw(2) << min << ':'
            << std::setw(2) << sec;
        if (show_millisec)
            oss << '.' << std::setw(3) << milli;
    }
    return oss.str();
}

static bool SequencerAddDelButton(ImDrawList *draw_list, ImVec2 pos, bool add = true)
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect delRect(pos, ImVec2(pos.x + 16, pos.y + 16));
    bool overDel = delRect.Contains(io.MousePos);
    int delColor = IM_COL32_WHITE;
    float midy = pos.y + 16 / 2 - 0.5f;
    float midx = pos.x + 16 / 2 - 0.5f;
    draw_list->AddRect(delRect.Min, delRect.Max, delColor, 4);
    draw_list->AddLine(ImVec2(delRect.Min.x + 3, midy), ImVec2(delRect.Max.x - 4, midy), delColor, 2);
    if (add) draw_list->AddLine(ImVec2(midx, delRect.Min.y + 3), ImVec2(midx, delRect.Max.y - 4), delColor, 2);
    return overDel;
}

bool Sequencer(SequenceInterface *sequence, int64_t *currentTime, bool *expanded, int *selectedEntry, int64_t *firstTime, int64_t *lastTime, int sequenceOptions)
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
    int scrollBarHeight = 14;
    bool popupOpened = false;
    int sequenceCount = sequence->GetItemCount();

    ImGui::BeginGroup();

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();     // ImDrawList API uses screen coordinates!
    ImVec2 canvas_size = ImGui::GetContentRegionAvail() - ImVec2(16, 0); // Resize canvas to what's available
    int64_t firstTimeUsed = firstTime ? *firstTime : 0;
    int controlHeight = sequenceCount * ItemHeight;
    for (int i = 0; i < sequenceCount; i++)
        controlHeight += int(sequence->GetCustomHeight(i));
    int64_t duration = ImMax(sequence->GetEnd() - sequence->GetStart(), (int64_t)1);
    static bool MovingScrollBar = false;
    static bool MovingCurrentTime = false;
    ImVector<SequencerCustomDraw> customDraws;
    ImVector<SequencerCustomDraw> compactCustomDraws;
    // zoom in/out
    const int64_t visibleTime = (int64_t)floorf((canvas_size.x - legendWidth) / msPixelWidth);
    const float barWidthRatio = ImMin(visibleTime / (float)duration, 1.f);
    const float barWidthInPixels = barWidthRatio * (canvas_size.x - legendWidth);
    ImRect regionRect(canvas_pos, canvas_pos + canvas_size);
    static bool panningView = false;
    static ImVec2 panningViewSource;
    static int64_t panningViewTime;
    ImRect scrollBarRect;
    if (ImGui::IsWindowFocused() && io.KeyAlt && io.MouseDown[2])
    {
        if (!panningView)
        {
            panningViewSource = io.MousePos;
            panningView = true;
            panningViewTime = *firstTime;
        }
        *firstTime = panningViewTime - int((io.MousePos.x - panningViewSource.x) / msPixelWidth);
        *firstTime = ImClamp(*firstTime, sequence->GetStart(), sequence->GetEnd() - visibleTime);
    }
    if (panningView && !io.MouseDown[2])
    {
        panningView = false;
    }

    msPixelWidthTarget = ImClamp(msPixelWidthTarget, 0.001f, 50.f);
    msPixelWidth = ImLerp(msPixelWidth, msPixelWidthTarget, 0.33f);
    duration = sequence->GetEnd() - sequence->GetStart();

    if (visibleTime >= duration && firstTime)
        *firstTime = sequence->GetStart();
    if (lastTime)
        *lastTime = firstTimeUsed + visibleTime;

    ImGui::SetCursorScreenPos(canvas_pos + ImVec2(3, ItemHeight + 3));
    ImGui::InvisibleButton("canvas", canvas_size - ImVec2(6, ItemHeight + scrollBarHeight + 8));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Media_drag_drop"))
        {
            ImSequencer::SequenceItem * item = (ImSequencer::SequenceItem*)payload->Data;
            ImSequencer::MediaSequence * seq = (ImSequencer::MediaSequence *)sequence;
            SequenceItem * new_item = new SequenceItem(item->mName, item->mPath, item->mStart, item->mEnd, true, item->mMediaType);
            auto length = item->mEnd - item->mStart;
            if (currentTime && firstTime && *currentTime >= *firstTime && *currentTime <= sequence->GetEnd())
                new_item->mStart = *currentTime;
            else
                new_item->mStart = *firstTime;
            new_item->mEnd = new_item->mStart + length;
            if (new_item->mEnd > sequence->GetEnd())
                sequence->SetEnd(new_item->mEnd + 60 * 1000);
            seq->m_Items.push_back(new_item);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SetCursorScreenPos(canvas_pos);
    if ((expanded && !*expanded) || !sequenceCount)
    {
        // minimum view
        ImGui::InvisibleButton("canvas_minimum", ImVec2(canvas_size.x - canvas_pos.x, (float)ItemHeight));
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x, canvas_pos.y + ItemHeight), COL_CANVAS_BG, 0);
        auto info_str = MillisecToString(duration, true);
        info_str += " / ";
        info_str += std::to_string(sequenceCount) + " entries";
        draw_list->AddText(ImVec2(canvas_pos.x + 40, canvas_pos.y + 2), IM_COL32_WHITE, info_str.c_str());
    }
    else
    {
        // normal view
        bool hasScrollBar(true);
        // test scroll area
        ImVec2 headerSize(canvas_size.x, (float)HeadHeight);
        ImVec2 scrollBarSize(canvas_size.x, scrollBarHeight);
        ImGui::InvisibleButton("topBar", headerSize);
        draw_list->AddRectFilled(canvas_pos, canvas_pos + headerSize, IM_COL32_BLACK, 0);
        if (!sequenceCount) 
        {
            ImGui::EndGroup();
            return false;
        }
        ImVec2 childFramePos = ImGui::GetCursorScreenPos();
        ImVec2 childFrameSize(canvas_size.x, canvas_size.y - 8.f - headerSize.y - (hasScrollBar ? scrollBarSize.y : 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::BeginChildFrame(889, childFrameSize, ImGuiWindowFlags_NoScrollbar); // id = 889 why?
        sequence->focused = ImGui::IsWindowFocused();
        ImGui::InvisibleButton("contentBar", ImVec2(canvas_size.x - 14, float(controlHeight)));
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
        if (!MovingCurrentTime && !MovingScrollBar && movingEntry == -1 && sequenceOptions & SEQUENCER_CHANGE_TIME && currentTime && *currentTime >= 0 && topRect.Contains(io.MousePos) && io.MouseDown[0])
        {
            MovingCurrentTime = true;
        }
        if (MovingCurrentTime)
        {
            if (duration)
            {
                *currentTime = (int)((io.MousePos.x - topRect.Min.x) / msPixelWidth) + firstTimeUsed;
                if (*currentTime < sequence->GetStart())
                    *currentTime = sequence->GetStart();
                if (*currentTime >= sequence->GetEnd())
                    *currentTime = sequence->GetEnd();
            }
            if (!io.MouseDown[0])
                MovingCurrentTime = false;
        }

        //header
        //header time and lines
        int64_t modTimeCount = 10;
        int timeStep = 1;
        while ((modTimeCount * msPixelWidth) < 150)
        {
            modTimeCount *= 2;
            timeStep *= 2;
        };
        int halfModTime = modTimeCount / 2;
        auto drawLine = [&](int64_t i, int regionHeight)
        {
            bool baseIndex = ((i % modTimeCount) == 0) || (i == sequence->GetEnd() || i == sequence->GetStart());
            bool halfIndex = (i % halfModTime) == 0;
            int px = (int)canvas_pos.x + int(i * msPixelWidth) + legendWidth - int(firstTimeUsed * msPixelWidth);
            int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int tiretEnd = baseIndex ? regionHeight : HeadHeight;
            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legendWidth))
            {
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), COL_MARK, 1);
                draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)HeadHeight), ImVec2((float)px, canvas_pos.y + (float)regionHeight - 1), COL_MARK, 1);
            }
            if (baseIndex && px > (canvas_pos.x + legendWidth))
            {
                auto time_str = MillisecToString(i, true);
                draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y), COL_RULE_TEXT, time_str.c_str());
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
        for (auto i = sequence->GetStart(); i <= sequence->GetEnd(); i += timeStep)
        {
            drawLine(i, HeadHeight);
        }
        drawLine(sequence->GetStart(), HeadHeight);
        drawLine(sequence->GetEnd(), HeadHeight);

        // cursor Arrow
        if (currentTime && firstTime && *currentTime >= *firstTime && *currentTime <= sequence->GetEnd())
        {
            const float arrowWidth = draw_list->_Data->FontSize;
            float arrowOffset = contentMin.x + legendWidth + (*currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - arrowWidth * 0.5f - 2;
            ImGui::RenderArrow(draw_list, ImVec2(arrowOffset, canvas_pos.y), COL_CURSOR_ARROW, ImGuiDir_Down);
            ImGui::SetWindowFontScale(0.8);
            auto time_str = MillisecToString(*currentTime, true);
            ImVec2 str_size = ImGui::CalcTextSize(time_str.c_str(), nullptr, true);
            float strOffset = contentMin.x + legendWidth + (*currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - str_size.x * 0.5f - 2;
            ImVec2 str_pos = ImVec2(strOffset, canvas_pos.y + 10);
            draw_list->AddRectFilled(str_pos + ImVec2(-2, 0), str_pos + str_size + ImVec2(4, 4), COL_CURSOR_TEXT_BG, 2.0, ImDrawFlags_RoundCornersAll);
            draw_list->AddText(str_pos, COL_CURSOR_TEXT, time_str.c_str());
            ImGui::SetWindowFontScale(1.0);
        }

        // clip content
        draw_list->PushClipRect(childFramePos, childFramePos + childFrameSize);
        // draw item names in the legend rect on the left
        size_t customHeight = 0;
        for (int i = 0; i < sequenceCount; i++)
        {
            ImVec2 tpos(contentMin.x + 3, contentMin.y + i * ItemHeight + 2 + customHeight);
            draw_list->AddText(tpos, IM_COL32_WHITE, sequence->GetItemLabel(i));
            if (sequenceOptions & SEQUENCER_DEL)
            {
                bool overDel = SequencerAddDelButton(draw_list, ImVec2(contentMin.x + legendWidth - ItemHeight + 2 - 10, tpos.y + 2), false);
                if (overDel && io.MouseReleased[0])
                    delEntry = i;
            }
            if (sequenceOptions & SEQUENCER_ADD)
            {
                bool overDup = SequencerAddDelButton(draw_list, ImVec2(contentMin.x + legendWidth - ItemHeight - ItemHeight + 2 - 10, tpos.y + 2), true);
                if (overDup && io.MouseReleased[0])
                    dupEntry = i;
            }
            customHeight += sequence->GetCustomHeight(i);
        }
        // clipping rect so items bars are not visible in the legend on the left when scrolled
    
        // slots background
        customHeight = 0;
        for (int i = 0; i < sequenceCount; i++)
        {
            unsigned int col = (i & 1) ? COL_SLOT_ODD : COL_SLOT_EVEN;
            size_t localCustomHeight = sequence->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 sz = ImVec2(canvas_size.x + canvas_pos.x, pos.y + ItemHeight - 1 + localCustomHeight);
            if (!popupOpened && cy >= pos.y && cy < pos.y + (ItemHeight + localCustomHeight) && movingEntry == -1 && cx > contentMin.x && cx < contentMin.x + canvas_size.x)
            {
                col += IM_COL32(8, 16, 32, 128);
                pos.x -= legendWidth;
            }
            draw_list->AddRectFilled(pos, sz, col, 0);
            customHeight += localCustomHeight;
        }
        draw_list->PushClipRect(childFramePos + ImVec2(float(legendWidth), 0.f), childFramePos + childFrameSize);
        // vertical time lines in content area
        for (auto i = sequence->GetStart(); i <= sequence->GetEnd(); i += timeStep)
        {
            drawLineContent(i, int(contentHeight));
        }
        drawLineContent(sequence->GetStart(), int(contentHeight));
        drawLineContent(sequence->GetEnd(), int(contentHeight));
        // selection
        bool selected = selectedEntry && (*selectedEntry >= 0);
        if (selected)
        {
            customHeight = 0;
            for (int i = 0; i < *selectedEntry; i++)
                customHeight += sequence->GetCustomHeight(i);
            draw_list->AddRectFilled(ImVec2(contentMin.x, contentMin.y + ItemHeight * *selectedEntry + customHeight), ImVec2(contentMin.x + canvas_size.x, contentMin.y + ItemHeight * (*selectedEntry + 1) + customHeight), COL_SLOT_SELECTED, 1.f);
        }
        
        // slots
        customHeight = 0;
        for (int i = 0; i < sequenceCount; i++)
        {
            int64_t start, end;
            std::string name;
            unsigned int color;
            std::vector<VideoSnapshotInfo> snapshots;
            sequence->Get(i, start, end, name, color);
            size_t localCustomHeight = sequence->GetCustomHeight(i);
            ImVec2 pos = ImVec2(contentMin.x + legendWidth - firstTimeUsed * msPixelWidth, contentMin.y + ItemHeight * i + 1 + customHeight);
            ImVec2 slotP1(pos.x + start * msPixelWidth, pos.y + 2);
            ImVec2 slotP2(pos.x + end * msPixelWidth + msPixelWidth, pos.y + ItemHeight - 2);
            ImVec2 slotP3(pos.x + end * msPixelWidth + msPixelWidth, pos.y + ItemHeight - 2 + localCustomHeight);
            unsigned int slotColor = color | IM_COL32_BLACK;
            unsigned int slotColorHalf = HALF_COLOR(color);
            if (slotP1.x <= (canvas_size.x + contentMin.x) && slotP2.x >= (contentMin.x + legendWidth))
            {
                draw_list->AddRectFilled(slotP1, slotP3, slotColorHalf, 2);
                draw_list->AddRectFilled(slotP1, slotP2, slotColor, 2);
            }
            if (ImRect(slotP1, slotP2).Contains(io.MousePos) && io.MouseDoubleClicked[0])
            {
                sequence->DoubleClick(i);
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
                    draw_list->AddRectFilled(rc.Min, rc.Max, quadColor[j], 2);
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
                        sequence->BeginEdit(movingEntry);
                        break;
                    }
                }
            }

            // calculate custom draw rect
            if (localCustomHeight > 0)
            {
                // slot normal view (custom view)
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + 1 + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstTimeUsed - start - 0.5f) * msPixelWidth, float(ItemHeight)),
                                  rp + ImVec2(legendWidth + (end - firstTimeUsed - 0.5f + 2.f) * msPixelWidth, float(localCustomHeight + ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(ItemHeight)), rp + ImVec2(canvas_size.x, float(localCustomHeight + ItemHeight)));
                ImRect legendRect(rp + ImVec2(0.f, float(ItemHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
                ImRect legendClippingRect(rp + ImVec2(0.f, float(ItemHeight)), rp + ImVec2(float(legendWidth), float(localCustomHeight + ItemHeight)));
                customDraws.push_back({i, customRect, legendRect, clippingRect, legendClippingRect});
            }
            else
            {
                // slot compact view (item bar only) 
                ImVec2 rp(canvas_pos.x, contentMin.y + ItemHeight * i + customHeight);
                ImRect customRect(rp + ImVec2(legendWidth - (firstTimeUsed - sequence->GetStart() - 0.5f) * msPixelWidth, float(0.f)),
                                  rp + ImVec2(legendWidth + (sequence->GetEnd() - firstTimeUsed - 0.5f + 2.f) * msPixelWidth, float(ItemHeight)));
                ImRect clippingRect(rp + ImVec2(float(legendWidth), float(0.f)), rp + ImVec2(canvas_size.x, float(ItemHeight)));
                compactCustomDraws.push_back({i, customRect, ImRect(), clippingRect, ImRect()});
            }
            customHeight += localCustomHeight;
        }

        // slot moving
        if (movingEntry >= 0)
        {
            ImGui::CaptureMouseFromApp();
            int diffTime = int((cx - movingPos) / msPixelWidth);
            if (std::abs(diffTime) > 0)
            {
                int64_t start, end;
                std::string name;
                unsigned int color;
                sequence->Get(movingEntry, start, end, name, color);
                if (selectedEntry)
                    *selectedEntry = movingEntry;
                int64_t l = start;
                int64_t r = end;
                if (movingPart & 1)
                    l += diffTime;
                if (movingPart & 2)
                    r += diffTime;
                if (l < 0)
                {
                    if (movingPart & 2)
                        r -= l;
                    l = 0;
                }
                if (movingPart & 1 && l > r)
                    l = r;
                if (movingPart & 2 && r < l)
                    r = l;
                movingPos += int(diffTime * msPixelWidth);
                if (r > sequence->GetEnd())
                    sequence->SetEnd(r + 60 * 1000);
                sequence->Set(movingEntry, l, r, name, color);
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
                sequence->EndEdit();
            }
        }

        // cursor line
        if (currentTime && firstTime && *currentTime >= *firstTime && *currentTime <= sequence->GetEnd())
        {
            static const float cursorWidth = 3.f;
            float cursorOffset = contentMin.x + legendWidth + (*currentTime - firstTimeUsed) * msPixelWidth + msPixelWidth / 2 - cursorWidth * 0.5f - 1;
            draw_list->AddLine(ImVec2(cursorOffset, canvas_pos.y), ImVec2(cursorOffset, contentMax.y), IM_COL32(0, 255, 0, 128), cursorWidth);
        }
        draw_list->PopClipRect();
        draw_list->PopClipRect();
        
        // copy paste
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
                sequence->Copy();
            }
            if (inRectPaste && io.MouseReleased[0])
            {
                sequence->Paste();
            }
        }
        ImGui::EndChildFrame();

        ImGui::PopStyleColor();
        if (hasScrollBar)
        {
            ImGui::InvisibleButton("scrollBar", scrollBarSize);
            ImVec2 scrollBarMin = ImGui::GetItemRectMin();
            ImVec2 scrollBarMax = ImGui::GetItemRectMax();
            // ratio = time visible in control / number to total time
            float startOffset = ((float)(firstTimeUsed - sequence->GetStart()) / (float)duration) * (canvas_size.x - legendWidth);
            ImVec2 scrollBarA(scrollBarMin.x + legendWidth, scrollBarMin.y - 2);
            ImVec2 scrollBarB(scrollBarMin.x + canvas_size.x, scrollBarMax.y - 1);
            scrollBarRect = ImRect(scrollBarA, scrollBarB);
            bool inScrollBar = scrollBarRect.Contains(io.MousePos);
            draw_list->AddRectFilled(scrollBarA, scrollBarB, COL_SLIDER_BG, 8);
            ImVec2 scrollBarC(scrollBarMin.x + legendWidth + startOffset, scrollBarMin.y);
            ImVec2 scrollBarD(scrollBarMin.x + legendWidth + barWidthInPixels + startOffset, scrollBarMax.y - 2);
            draw_list->AddRectFilled(scrollBarC, scrollBarD, (inScrollBar || MovingScrollBar) ? COL_SLIDER_IN : COL_SLIDER_MOVING, 6);
            ImRect barHandleLeft(scrollBarC, ImVec2(scrollBarC.x + 14, scrollBarD.y));
            ImRect barHandleRight(ImVec2(scrollBarD.x - 14, scrollBarC.y), scrollBarD);
            bool onLeft = barHandleLeft.Contains(io.MousePos);
            bool onRight = barHandleRight.Contains(io.MousePos);
            static bool sizingRBar = false;
            static bool sizingLBar = false;
            draw_list->AddRectFilled(barHandleLeft.Min, barHandleLeft.Max, (onLeft || sizingLBar) ? COL_SLIDER_SIZING : COL_SLIDER_HANDLE, 6);
            draw_list->AddRectFilled(barHandleRight.Min, barHandleRight.Max, (onRight || sizingRBar) ? COL_SLIDER_SIZING : COL_SLIDER_HANDLE, 6);
            ImRect scrollBarThumb(scrollBarC, scrollBarD);
            static const float MinBarWidth = 44.f;
            if (sizingRBar)
            {
                if (!io.MouseDown[0])
                {
                    sizingRBar = false;
                }
                else
                {
                    float barNewWidth = ImMax(barWidthInPixels + io.MouseDelta.x, MinBarWidth);
                    float barRatio = barNewWidth / barWidthInPixels;
                    msPixelWidthTarget = msPixelWidth = msPixelWidth / barRatio;
                    int newVisibleTimeCount = int((canvas_size.x - legendWidth) / msPixelWidthTarget);
                    int lastTime = *firstTime + newVisibleTimeCount;
                    if (lastTime > sequence->GetEnd())
                    {
                        msPixelWidthTarget = msPixelWidth = (canvas_size.x - legendWidth) / float(sequence->GetEnd() - *firstTime);
                    }
                }
            }
            else if (sizingLBar)
            {
                if (!io.MouseDown[0])
                {
                    sizingLBar = false;
                }
                else
                {
                    if (fabsf(io.MouseDelta.x) > FLT_EPSILON)
                    {
                        float barNewWidth = ImMax(barWidthInPixels - io.MouseDelta.x, MinBarWidth);
                        float barRatio = barNewWidth / barWidthInPixels;
                        float previousMsPixelWidthTarget = msPixelWidthTarget;
                        msPixelWidthTarget = msPixelWidth = msPixelWidth / barRatio;
                        int newVisibleTimeCount = int(visibleTime / barRatio);
                        int64_t newFirst = *firstTime + newVisibleTimeCount - visibleTime;
                        newFirst = ImClamp(newFirst, sequence->GetStart(), ImMax(sequence->GetEnd() - visibleTime, sequence->GetStart()));
                        if (newFirst == *firstTime)
                        {
                            msPixelWidth = msPixelWidthTarget = previousMsPixelWidthTarget;
                        }
                        else
                        {
                            *firstTime = newFirst;
                        }
                    }
                }
            }
            else
            {
                if (MovingScrollBar)
                {
                    if (!io.MouseDown[0])
                    {
                        MovingScrollBar = false;
                    }
                    else
                    {
                        float msPerPixelInBar = barWidthInPixels / (float)visibleTime;
                        *firstTime = int((io.MousePos.x - panningViewSource.x) / msPerPixelInBar) - panningViewTime;
                        *firstTime = ImClamp(*firstTime, sequence->GetStart(), ImMax(sequence->GetEnd() - visibleTime, sequence->GetStart()));
                    }
                }
                else
                {
                    if (scrollBarThumb.Contains(io.MousePos) && ImGui::IsMouseClicked(0) && firstTime && !MovingCurrentTime && movingEntry == -1)
                    {
                        MovingScrollBar = true;
                        panningViewSource = io.MousePos;
                        panningViewTime = -*firstTime;
                    }
                    if (!sizingRBar && onRight && ImGui::IsMouseClicked(0))
                        sizingRBar = true;
                    if (!sizingLBar && onLeft && ImGui::IsMouseClicked(0))
                        sizingLBar = true;
                }
            }
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
            int64_t overCursor = *firstTime + (int64_t)(visibleTime * ((io.MousePos.x - (float)legendWidth - canvas_pos.x) / (canvas_size.x - legendWidth)));
            if (io.MouseWheel < -FLT_EPSILON && visibleTime <= sequence->GetEnd())
            {
                *firstTime -= overCursor;
                *firstTime = int64_t(*firstTime * 1.1f);
                msPixelWidthTarget *= 0.9f;
                *firstTime += overCursor;
            }
            if (io.MouseWheel > FLT_EPSILON)
            {
                *firstTime -= overCursor;
                *firstTime = int64_t(*firstTime * 0.9f);
                msPixelWidthTarget *= 1.1f;
                *firstTime += overCursor;
            }
        }
        else
        {
            // left-right wheel over blank area, moving canvas view
            if (io.MouseWheelH < -FLT_EPSILON)
            {
                *firstTime -= visibleTime / 4;
                *firstTime = ImClamp(*firstTime, sequence->GetStart(), ImMax(sequence->GetEnd() - visibleTime, sequence->GetStart()));
            }
            if (io.MouseWheelH > FLT_EPSILON)
            {
                *firstTime += visibleTime / 4;
                *firstTime = ImClamp(*firstTime, sequence->GetStart(), ImMax(sequence->GetEnd() - visibleTime, sequence->GetStart()));
            }
        }
    }

    for (auto &customDraw : customDraws)
        sequence->CustomDraw(customDraw.index, draw_list, customDraw.customRect, customDraw.legendRect, customDraw.clippingRect, customDraw.legendClippingRect, *firstTime, visibleTime);
    for (auto &customDraw : compactCustomDraws)
        sequence->CustomDrawCompact(customDraw.index, draw_list, customDraw.customRect, customDraw.clippingRect);

    ImGui::EndGroup();

    if (expanded)
    {
        bool overExpanded = SequencerAddDelButton(draw_list, ImVec2(canvas_pos.x + 2, canvas_pos.y + 2), !*expanded);
        if (overExpanded && io.MouseReleased[0])
            *expanded = !*expanded;
    }
    if (delEntry != -1)
    {
        sequence->Del(delEntry);
        if (selectedEntry && (*selectedEntry == delEntry || *selectedEntry >= sequence->GetItemCount()))
            *selectedEntry = -1;
    }
    if (dupEntry != -1)
    {
        sequence->Duplicate(dupEntry);
    }
    return ret;
}

/***********************************************************************************************************
 * SequenceItem Struct Member Functions
 ***********************************************************************************************************/

SequenceItem::SequenceItem(const std::string& name, const std::string& path, int64_t start, int64_t end, bool expand, int type)
{
    mName = name;
    mPath = path;
    mStart = start;
    mEnd = end;
    mExpanded = expand;
    mMediaType = type;
    mMedia = CreateMediaSnapshot();
    mColor = COL_SLOT_DEFAULT;
    if (!path.empty() && mMedia)
    {
        mMedia->Open(path);
    }
    if (mMedia && mMedia->IsOpened())
    {
        double window_size = 1.0f;
        mEnd = mMedia->GetVidoeDuration();
        mMedia->SetSnapshotResizeFactor(0.25, 0.25);
        mMedia->ConfigSnapWindow(window_size, 10);
    }
}

SequenceItem::~SequenceItem()
{
    ReleaseMediaSnapshot(&mMedia);
    mMedia = nullptr;
    if (mMediaSnapshot)
    {
        ImGui::ImDestroyTexture(mMediaSnapshot); 
        mMediaSnapshot = nullptr;
    }
}

void SequenceItem::SequenceItemUpdateSnapShot()
{
    if (mMediaSnapshot)
        return;
    if (mMedia && mMedia->IsOpened())
    {
        auto pos = (float)mMedia->GetVidoeMinPos()/1000.f;
        std::vector<ImGui::ImMat> snapshots;
        if (mMedia->GetSnapshots(snapshots, pos))
        {
            auto snap = snapshots[snapshots.size() / 2];
            if (!snap.empty())
            {
                if (snap.device == ImDataDevice::IM_DD_CPU)
                {
                    ImGui::ImGenerateOrUpdateTexture(mMediaSnapshot, snap.w, snap.h, snap.c, (const unsigned char *)snap.data);
                }
                if (snap.device == ImDataDevice::IM_DD_VULKAN)
                {
                    ImGui::VkMat vkmat = snap;
                    ImGui::ImGenerateOrUpdateTexture(mMediaSnapshot, vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                }
            }
        }
    }
}

void SequenceItem::CalculateVideoSnapshotInfo(const ImRect &customRect)
{
    if (mMedia && mMedia->IsOpened() && mMedia->HasVideo())
    {
        auto width = mMedia->GetVideoWidth();
        auto height = mMedia->GetVideoHeight();
        auto duration = mMedia->GetVidoeDuration();
        if (!width || !height)
            return;
        if (customRect.GetHeight() <= 0 || customRect.GetWidth() <= 0)
            return;
        float aspio = (float)width / (float)height;
        float snapshot_height = customRect.GetHeight();
        float snapshot_width = snapshot_height * aspio;
        float frame_count = (customRect.GetWidth() + snapshot_width) / snapshot_width;
        float frame_duration = (float)duration / (float)frame_count;
        if (frame_count != mVideoSnapshots.size())
        {
            mVideoSnapshots.clear();
            for (int i = 0; i < (int)frame_count; i++)
            {
                VideoSnapshotInfo snapshot;
                snapshot.rc.Min = ImVec2(i * snapshot_width, 0);
                snapshot.rc.Max = ImVec2((i + 1) * snapshot_width, snapshot_height);
                if (snapshot.rc.Max.x > customRect.GetWidth() + 2)
                    snapshot.rc.Max.x = customRect.GetWidth() + 2;
                snapshot.time_stamp = i * frame_duration;
                snapshot.duration = frame_duration;
                snapshot.frame_width = snapshot_width;
                mVideoSnapshots.push_back(snapshot);
            }
        }
    }
}

/***********************************************************************************************************
 * MediaSequence Struct Member Functions
 ***********************************************************************************************************/

MediaSequence::~MediaSequence()
{
    for (auto item : m_Items)
    {
        delete item;
    }
}

void MediaSequence::Get(int index, int64_t& start, int64_t& end, std::string& name, unsigned int& color)
{
    SequenceItem *item = m_Items[index];
    color = item->mColor;
    start = item->mStart;
    end = item->mEnd;
    name = item->mName;
}

void MediaSequence::Set(int index, int64_t start, int64_t end, std::string name, unsigned int color)
{
    SequenceItem *item = m_Items[index];
    item->mColor = color;
    item->mStart = start;
    item->mEnd = end;
    item->mName = name;
}

void MediaSequence::Add(std::string& name)
{ 
    /*m_Items.push_back(SequenceItem{name, "", 0, 10, false});*/ 
}
    
void MediaSequence::Del(int index)
{
    auto item = m_Items.erase(m_Items.begin() + index);
    delete *item;
}
    
void MediaSequence::Duplicate(int index)
{
    /*m_Items.push_back(m_Items[index]);*/
}

void MediaSequence::CustomDraw(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &legendRect, const ImRect &clippingRect, const ImRect &legendClippingRect, int64_t viewStartTime, int64_t visibleTime)
{
    // rc: full item length rect
    // clippingRect: current view window area
    // legendRect: legend area
    // legendClippingRect: legend area

    SequenceItem *item = m_Items[index];
    item->CalculateVideoSnapshotInfo(rc);
    if (item->mVideoSnapshots.size()  == 0) return;
    float frame_width = item->mVideoSnapshots[0].frame_width;
    int64_t lendth = item->mEnd - item->mStart;
    int64_t startTime = viewStartTime - item->mStart;
    if (startTime < 0) startTime = 0;
    if (startTime > lendth) startTime = lendth;
    int total_snapshot = item->mVideoSnapshots.size();
    int snapshot_index = floor((float)startTime / (float)lendth * (float)total_snapshot);
    
    int max_snapshot = (clippingRect.GetWidth() + frame_width / 2) / frame_width + 1; 
    int snapshot_count = max_snapshot;

    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
    for (int i = 0; i < snapshot_count; i++)
    {
        if (i + snapshot_index > total_snapshot - 1) break;
        ImRect frame_rc = item->mVideoSnapshots[snapshot_index + i].rc;
        int64_t time_stamp = item->mVideoSnapshots[snapshot_index + i].time_stamp;
        draw_list->AddRect(frame_rc.Min + rc.Min, frame_rc.Max + rc.Min, IM_COL32_BLACK);
        auto time_string = MillisecToString(time_stamp, true);
        draw_list->AddText(frame_rc.Min + rc.Min + ImVec2(2, 40), IM_COL32_WHITE, time_string.c_str());
    }
    draw_list->PopClipRect();
    
    //ImGui::SetCursorScreenPos(rc.Min);
}

void MediaSequence::CustomDrawCompact(int index, ImDrawList *draw_list, const ImRect &rc, const ImRect &clippingRect)
{
    // rc: full item length rect
    // clippingRect: current view window area

    //draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
    //draw_list->PopClipRect();
    //draw_list->AddRectFilled(clippingRect.Min, clippingRect.Max, IM_COL32(255,0, 0, 128), 0);
}

void MediaSequence::GetVideoSnapshotInfo(int index, std::vector<VideoSnapshotInfo>& snapshots)
{
    SequenceItem *item = m_Items[index];
    snapshots = item->mVideoSnapshots;
}

} // namespace ImSequencer