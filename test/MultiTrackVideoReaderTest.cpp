#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#include <string>
#include <iostream>
#include <sstream>
#include <atomic>
#include <vector>
#include <cmath>
#include <chrono>
#include "MultiTrackVideoReader.h"
#include "VideoTransformFilter.h"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
using namespace MediaCore;
using Clock = chrono::steady_clock;

static atomic_int64_t g_idIndex{1};

static MultiTrackVideoReader* g_mtVidReader = nullptr;
static SubtitleTrackHolder g_subtrk;
const int c_videoOutputWidth = 960;
const int c_videoOutputHeight = 660;
const MediaInfo::Ratio c_videoFrameRate = { 25, 1 };

static double g_playStartPos = 0.f;
static Clock::time_point g_playStartTp;
static bool g_isPlay = false;
static bool g_playForward = true;
static bool g_isSeeking = false;

static ImTextureID g_imageTid;
static ImVec2 g_imageDisplaySize = { 640, 440 };

const string c_imguiIniPath = "ms_test.ini";
const string c_bookmarkPath = "bookmark.ini";

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Multi-track Video Reader Test";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 840;
}

void Application_SetupContext(ImGuiContext* ctx)
{
}

static uint32_t s_addClipOptSelIdx = 0;
static double s_addClipStart = 0;
static double s_addClipStartOffset = 0;
static double s_addClipEndOffset = 0;
static uint32_t s_remTrackOptSelIdx = 0;
static uint32_t s_clipOpTrackSelIdx = 0;
static uint32_t s_clipOpClipSelIdx = 0;
static double s_changeClipStart = 0;
static double s_changeClipStartOffset = 0;
static double s_changeClipEndOffset = 0;
static bool s_keepAspectRatio = true;
static vector<string> s_fitScaleTypeSelections;
static int s_fitScaleTypeSelIdx = 0;
static bool s_showClipSourceFrame = false;

void Application_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    GetMultiTrackVideoReaderLogger()
        ->SetShowLevels(DEBUG);
    GetSubtitleTrackLogger()
        ->SetShowLevels(DEBUG);

#ifdef USE_BOOKMARK
	// load bookmarks
	ifstream docFile(c_bookmarkPath, ios::in);
	if (docFile.is_open())
	{
		stringstream strStream;
		strStream << docFile.rdbuf(); //read the file
		ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
		docFile.close();
	}
#endif

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = c_imguiIniPath.c_str();

    s_fitScaleTypeSelections = { "fit", "crop", "fill", "stretch" };

    InitializeSubtitleLibrary();

    g_mtVidReader = CreateMultiTrackVideoReader();
    g_mtVidReader->Configure(c_videoOutputWidth, c_videoOutputHeight, c_videoFrameRate);
    g_mtVidReader->Start();
}

void Application_Finalize(void** handle)
{
    ReleaseMultiTrackVideoReader(&g_mtVidReader);

    ReleaseSubtitleLibrary();

#ifdef USE_BOOKMARK
	// save bookmarks
	ofstream configFileWriter(c_bookmarkPath, ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
		configFileWriter.close();
	}
#endif
}

bool Application_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();

    float playPos = g_playStartPos;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        // control line #1
        vector<string> trackNames;
        for (uint32_t i = 0; i < g_mtVidReader->TrackCount(); i++)
        {
            ostringstream oss;
            oss << "track#" << i+1;
            trackNames.push_back(oss.str());
        }

        vector<string> addClipOpts(trackNames);
        addClipOpts.push_back("new track");
        if (s_addClipOptSelIdx >= addClipOpts.size())
            s_addClipOptSelIdx = addClipOpts.size()-1;
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("AddClipOptions");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##AddClipOptions", addClipOpts[s_addClipOptSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < addClipOpts.size(); i++)
            {
                string& item = addClipOpts[i];
                const bool isSelected = s_addClipOptSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_addClipOptSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("Start");
        ImGui::SameLine();
        ImGui::InputDouble("##Start", &s_addClipStart);
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("ClipStartOffset");
        ImGui::SameLine();
        ImGui::InputDouble("##ClipStartOffset", &s_addClipStartOffset);
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("ClipEndOffset");
        ImGui::SameLine();
        ImGui::InputDouble("##ClipEndOffset", &s_addClipEndOffset);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi){.mp4,.mov,.mkv,.webm,.avi,.MP4,.MOV,.MKV,WEBM,.AVI},.*";
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", 
                                                    filters, 
                                                    "/mnt/data2/video/hd/", 
                                                    1, 
                                                    nullptr, 
                                                    ImGuiFileDialogFlags_ShowBookmark |
                                                    ImGuiFileDialogFlags_Modal);
        }

        // control line #2
        ImGui::Spacing();
        vector<string> selectTrackOpts(trackNames);
        if (selectTrackOpts.empty())
            selectTrackOpts.push_back("<No track>");
        bool noTrack = trackNames.empty();
        ImGui::PushItemWidth(100);
        if (ImGui::BeginCombo("##MovClipSelTrackOptions", selectTrackOpts[s_clipOpTrackSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < selectTrackOpts.size(); i++)
            {
                string& item = selectTrackOpts[i];
                const bool isSelected = s_clipOpTrackSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_clipOpTrackSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, 10);
        vector<string> clipNames;
        if (!noTrack)
        {
            VideoTrackHolder hTrack = g_mtVidReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            auto clipIter = hTrack->ClipListBegin();
            while (clipIter != hTrack->ClipListEnd())
            {
                ostringstream oss;
                oss << "Clip#" << (*clipIter)->Id();
                clipNames.push_back(oss.str());
                clipIter++;
            }
        }
        bool noClip = false;
        vector<string> clipSelOpts(clipNames);
        if (clipSelOpts.empty())
        {
            clipSelOpts.push_back("<no clip>");
            noClip = true;
        }
        if (s_clipOpClipSelIdx >= clipSelOpts.size())
            s_clipOpClipSelIdx = clipSelOpts.size()-1;
        if (ImGui::BeginCombo("##MovClipSelClipOptions", clipSelOpts[s_clipOpClipSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < clipSelOpts.size(); i++)
            {
                string& item = clipSelOpts[i];
                const bool isSelected = s_clipOpClipSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_clipOpClipSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noClip);
        if (ImGui::Button("Remove Clip"))
        {
            VideoTrackHolder hTrack = g_mtVidReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            hTrack->RemoveClipByIndex(s_clipOpClipSelIdx);
            g_mtVidReader->Refresh();
            s_clipOpClipSelIdx = 0;
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 20);
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("tloff");
        ImGui::SameLine();
        ImGui::InputDouble("##tloff", &s_changeClipStart);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noClip);
        if (ImGui::Button("Move Clip"))
        {
            VideoTrackHolder hTrack = g_mtVidReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            VideoClipHolder hClip = hTrack->GetClipByIndex(s_clipOpClipSelIdx);
            hTrack->MoveClip(hClip->Id(), (int64_t)(s_changeClipStart*1000));
            g_mtVidReader->Refresh();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 20);
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("off0");
        ImGui::SameLine();
        ImGui::InputDouble("##off0", &s_changeClipStartOffset);
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("off1");
        ImGui::SameLine();
        ImGui::InputDouble("##off1", &s_changeClipEndOffset);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noClip);
        if (ImGui::Button("Change Clip Range"))
        {
            VideoTrackHolder hTrack = g_mtVidReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            VideoClipHolder hClip = hTrack->GetClipByIndex(s_clipOpClipSelIdx);
            hTrack->ChangeClipRange(hClip->Id(), (int64_t)(s_changeClipStartOffset*1000), (int64_t)(s_changeClipEndOffset*1000));
            g_mtVidReader->Refresh();
        }
        ImGui::EndDisabled();

        // control line #3
        ImGui::Spacing();
        ImGui::PushItemWidth(200);
        VideoClipHolder selectedClip;
        VideoTransformFilter* fftransFilter = nullptr;
        if (!noClip)
        {
            VideoTrackHolder hTrack = g_mtVidReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            selectedClip = hTrack->GetClipByIndex(s_clipOpClipSelIdx);
            fftransFilter = selectedClip->GetTransformFilterPtr();
        }
        ImGui::BeginDisabled(!fftransFilter);
        int sldintMaxValue = selectedClip ? selectedClip->SrcWidth() : 0;
        int sldintValue = fftransFilter ? fftransFilter->GetCropMarginL() : 0;
        if (ImGui::SliderInt("CropL", &sldintValue, 0, sldintMaxValue))
        {
            fftransFilter->SetCropMarginL(sldintValue);
            g_mtVidReader->Refresh();
        }
        ImGui::SameLine(0, 10);
        sldintMaxValue = selectedClip ? selectedClip->SrcHeight() : 0;
        sldintValue = fftransFilter ? fftransFilter->GetCropMarginT() : 0;
        if (ImGui::SliderInt("CropT", &sldintValue, 0, sldintMaxValue))
        {
            fftransFilter->SetCropMarginT(sldintValue);
            g_mtVidReader->Refresh();
        }
        ImGui::SameLine(0, 10);
        sldintMaxValue = selectedClip ? selectedClip->SrcWidth() : 0;
        sldintValue = fftransFilter ? fftransFilter->GetCropMarginR() : 0;
        if (ImGui::SliderInt("CropR", &sldintValue, 0, sldintMaxValue))
        {
            fftransFilter->SetCropMarginR(sldintValue);
            g_mtVidReader->Refresh();
        }
        ImGui::SameLine(0, 10);
        sldintMaxValue = selectedClip ? selectedClip->SrcHeight() : 0;
        sldintValue = fftransFilter ? fftransFilter->GetCropMarginB() : 0;
        if (ImGui::SliderInt("CropB", &sldintValue, 0, sldintMaxValue))
        {
            fftransFilter->SetCropMarginB(sldintValue);
            g_mtVidReader->Refresh();
        }
        ImGui::EndDisabled();

        // control line #4
        ImGui::BeginDisabled(!fftransFilter);
        float sldfltValue = fftransFilter ? fftransFilter->GetRotationAngle() : 0;
        if (ImGui::SliderFloat("Angle", &sldfltValue, -360, 360, "%.1f"))
        {
            fftransFilter->SetRotationAngle(sldfltValue);
            g_mtVidReader->Refresh();
        }
        ImGui::SameLine(0, 20);
        sldintMaxValue = selectedClip ? selectedClip->OutWidth() : 0;
        sldintValue = fftransFilter ? fftransFilter->GetPositionOffsetH() : 0;
        if (ImGui::SliderInt("OffsetH", &sldintValue, -sldintMaxValue, sldintMaxValue))
        {
            fftransFilter->SetPositionOffsetH(sldintValue);
            g_mtVidReader->Refresh();
        }
        ImGui::SameLine(0, 10);
        sldintMaxValue = selectedClip ? selectedClip->OutHeight() : 0;
        sldintValue = fftransFilter ? fftransFilter->GetPositionOffsetV() : 0;
        if (ImGui::SliderInt("OffsetV", &sldintValue, -sldintMaxValue, sldintMaxValue))
        {
            fftransFilter->SetPositionOffsetV(sldintValue);
            g_mtVidReader->Refresh();
        }
        ImGui::SameLine(0, 20);
        if (ImGui::BeginCombo("Fit scale type", s_fitScaleTypeSelections[s_fitScaleTypeSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < s_fitScaleTypeSelections.size(); i++)
            {
                string& item = s_fitScaleTypeSelections[i];
                const bool isSelected = s_fitScaleTypeSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                {
                    s_fitScaleTypeSelIdx = i;
                    MediaCore::ScaleType fitType;
                    switch (i)
                    {
                        case 1:
                        fitType = MediaCore::SCALE_TYPE__CROP;
                        break;
                        case 2:
                        fitType = MediaCore::SCALE_TYPE__FILL;
                        break;
                        case 3:
                        fitType = MediaCore::SCALE_TYPE__STRETCH;
                        break;
                        default:
                        fitType = MediaCore::SCALE_TYPE__FIT;
                    }
                    fftransFilter->SetScaleType(fitType);
                    g_mtVidReader->Refresh();
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        // control line #5
        ImGui::BeginDisabled(!fftransFilter);
        if (ImGui::Checkbox("Keep aspect-ratio", &s_keepAspectRatio) &&s_keepAspectRatio)
        {
            if (fftransFilter->GetScaleH() != fftransFilter->GetScaleV())
            {
                fftransFilter->SetScaleV(fftransFilter->GetScaleH());
                g_mtVidReader->Refresh();
            }
        }
        ImGui::SameLine(0, 10);
        sldfltValue = fftransFilter ? fftransFilter->GetScaleH() : 0;
        if (ImGui::SliderFloat("ScaleH", &sldfltValue, 0, 4, "%.2f"))
        {
            fftransFilter->SetScaleH(sldfltValue);
            if (s_keepAspectRatio)
                fftransFilter->SetScaleV(sldfltValue);
            g_mtVidReader->Refresh();
        }
        ImGui::SameLine(0, 10);
        sldfltValue = fftransFilter ? fftransFilter->GetScaleV() : 0;
        if (ImGui::SliderFloat("ScaleV", &sldfltValue, 0, 4, "%.2f"))
        {
            fftransFilter->SetScaleV(sldfltValue);
            if (s_keepAspectRatio)
                fftransFilter->SetScaleH(sldfltValue);
            g_mtVidReader->Refresh();
        }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();

        // control line #6
        ImGui::Spacing();
        ImGui::TextUnformatted("Track status:");
        uint32_t vidTrackIdx = 1;
        for (auto track = g_mtVidReader->TrackListBegin(); track != g_mtVidReader->TrackListEnd(); track++)
        {
            ostringstream oss;
            oss << "Track#" << vidTrackIdx++ << "{ 'clips': [";
            for (auto clIter = (*track)->ClipListBegin(); clIter != (*track)->ClipListEnd();)
            {
                oss << "Clip#" << (*clIter)->Id() << ":{'tlOff':" << (*clIter)->Start()
                    << ", 'off0':" << (*clIter)->StartOffset() << ", 'off1':" << (*clIter)->EndOffset()
                    << ", 'dur':" << (*clIter)->Duration() << "}";
                clIter++;
                if (clIter != (*track)->ClipListEnd())
                    oss << ", ";
            }
            oss << "], 'overlaps': [";
            for (auto ovIter = (*track)->OverlapListBegin(); ovIter != (*track)->OverlapListEnd();)
            {
                oss << "Overlap#" << (*ovIter)->Id() << ":{'start':" << (*ovIter)->Start()
                    << ", 'dur':" << (*ovIter)->Duration() << "}";
                ovIter++;
                if (ovIter != (*track)->OverlapListEnd())
                    oss << ", ";
            }
            oss << "].";
            ImGui::TextUnformatted(oss.str().c_str());
        }

        // control line #5
        ImGui::Spacing();
        ImGui::PushItemWidth(100);
        if (ImGui::BeginCombo("##RemTrackOptions", selectTrackOpts[s_remTrackOptSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < selectTrackOpts.size(); i++)
            {
                string& item = selectTrackOpts[i];
                const bool isSelected = s_remTrackOptSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_remTrackOptSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noTrack);
        if (ImGui::Button("Remove Track"))
        {
            g_mtVidReader->RemoveTrackByIndex(s_remTrackOptSelIdx);
            s_remTrackOptSelIdx = 0;
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 20);
        if (ImGui::Button("Open subtitle file"))
        {
            const char *filters = "字幕文件(*.srt *.ass *.ssa){.srt,.ass,.ssa,.SRT,.ASS,.SSA},.*";
            ImGuiFileDialog::Instance()->OpenDialog("ChooseSubtitleFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开字幕文件", 
                                                    filters, 
                                                    "/workspace/MediaFiles/", 
                                                    1, 
                                                    nullptr, 
                                                    ImGuiFileDialogFlags_ShowBookmark |
                                                    ImGuiFileDialogFlags_Modal);
        }

        // video
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        if (!selectedClip) s_showClipSourceFrame = false;
        ImGui::BeginDisabled(!selectedClip);
        ImGui::Checkbox("Show clip source frame", &s_showClipSourceFrame);
        ImGui::EndDisabled();
        float mediaDur = (float)g_mtVidReader->Duration();
        double elapsedTime = chrono::duration_cast<chrono::duration<double>>((Clock::now()-g_playStartTp)).count();
        playPos = g_isPlay ? (g_playForward ? g_playStartPos+elapsedTime : g_playStartPos-elapsedTime) : g_playStartPos;
        if (playPos < 0) playPos = 0;
        if (playPos > mediaDur) playPos = mediaDur;

        const int64_t readPos = (int64_t)(playPos*1000);
        vector<CorrelativeFrame> frames;
        bool readRes = g_mtVidReader->ReadVideoFrameEx(readPos, frames, g_isSeeking);
        ImGui::ImMat vmat;
        if (s_showClipSourceFrame)
        {
            auto iter = find_if(frames.begin(), frames.end(), [selectedClip] (auto& cf) {
                return cf.clipId == selectedClip->Id() && cf.phase == CorrelativeFrame::PHASE_SOURCE_FRAME;
            });
            if (iter != frames.end())
                vmat = iter->frame;
        }
        else if (!frames.empty())
            vmat = frames[0].frame;
        if (readRes)
        {
            string imgTag = TimestampToString(vmat.time_stamp);
            bool imgValid = true;
            if (vmat.empty())
            {
                imgValid = false;
                imgTag += "(loading)";
            }
            if (imgValid &&
                ((vmat.color_format != IM_CF_RGBA && vmat.color_format != IM_CF_ABGR) ||
                vmat.type != IM_DT_INT8 ||
                (vmat.device != IM_DD_CPU && vmat.device != IM_DD_VULKAN)))
            {
                Log(Error) << "WRONG snapshot format!" << endl;
                imgValid = false;
                imgTag += "(bad format)";
            }
            if (imgValid)
            {
                ImGui::ImMatToTexture(vmat, g_imageTid);
                if (g_imageTid) ImGui::Image(g_imageTid, g_imageDisplaySize);
            }
            else
            {
                ImGui::Dummy(g_imageDisplaySize);
            }
            ImGui::TextUnformatted(imgTag.c_str());
        }

        float currPos = playPos;
        int64_t dur = g_mtVidReader->Duration();
        if (ImGui::SliderFloat("Seek Pos", &currPos, 0, (float)dur/1000, "%.3f"))
        {
            g_isSeeking = true;
            g_playStartTp = Clock::now();
            g_playStartPos = currPos;
        }

        if (g_isSeeking && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            g_isSeeking = false;
        }

        string playBtnLabel = g_isPlay ? "Pause" : "Play ";
        if (ImGui::Button(playBtnLabel.c_str()))
        {
            g_isPlay = !g_isPlay;
            if (g_isPlay)
                g_playStartTp = Clock::now();
            else
                g_playStartPos = playPos;
        }

        ImGui::SameLine(0, 10);
        string dirBtnLabel = g_playForward ? "Backword" : "Forward";
        if (ImGui::Button(dirBtnLabel.c_str()))
        {
            bool notForward = !g_playForward;
            g_mtVidReader->SetDirection(notForward);
            g_playForward = notForward;
            g_playStartPos = playPos;
            g_playStartTp = Clock::now();
        }

        ImGui::SameLine(0, 10);
        if (ImGui::Button("Refresh"))
        {
            g_mtVidReader->Refresh();
        }

        ImGui::Spacing();

        ostringstream oss;
        oss << "Video pos: " << TimestampToString(playPos);
        string audTag = oss.str();
        ImGui::TextUnformatted(audTag.c_str());

        ImGui::End();
    }

    // open file dialog
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (s_addClipOptSelIdx == g_mtVidReader->TrackCount())
            {
                int64_t trackId = g_idIndex++;
                if (!g_mtVidReader->AddTrack(trackId))
                {
                    Log(Error) << "FAILED to 'AddTrack'! Message is '" << g_mtVidReader->GetError() << "'." << endl;
                }
            }
            VideoTrackHolder hTrack = g_mtVidReader->GetTrackByIndex(s_addClipOptSelIdx);
            MediaParserHolder hParser = CreateMediaParser();
            if (!hParser->Open(filePathName))
                throw std::runtime_error(hParser->GetError());
            int64_t clipId = g_idIndex++;
            auto vidstream = hParser->GetBestVideoStream();
            VideoClipHolder hClip;
            if (vidstream->isImage)
            {
                int64_t duration = (int64_t)(s_addClipStartOffset*1000);
                if (duration <= 0) duration = 20000;
                hClip = VideoClip::CreateImageInstance(
                    clipId, hParser,
                    hTrack->OutWidth(), hTrack->OutHeight(),
                    (int64_t)(s_addClipStart*1000), duration);
            }
            else
            {
                hClip = VideoClip::CreateVideoInstance(
                    clipId, hParser,
                    hTrack->OutWidth(), hTrack->OutHeight(), hTrack->FrameRate(),
                    (int64_t)(s_addClipStart*1000), (int64_t)(s_addClipStartOffset*1000), (int64_t)(s_addClipEndOffset*1000),
                    (int64_t)((playPos-s_addClipStart)*1000));
            }
            hTrack->InsertClip(hClip);
            g_mtVidReader->Refresh();

            s_addClipOptSelIdx = g_mtVidReader->TrackCount();
            s_addClipStart = 0;
            s_addClipStartOffset = 0;
            s_addClipEndOffset = 0;
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // open subtitle file dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseSubtitleFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_subtrk = g_mtVidReader->BuildSubtitleTrackFromFile(111, filePathName);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        app_done = true;
    }
    if (app_will_quit)
    {
        app_done = true;
    }

    return app_done;
}
