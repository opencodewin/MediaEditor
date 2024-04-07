#include <iomanip>
#include <limits>
#include <list>
#include <TimeUtils.h>
#include <MediaParser.h>
#include <VideoClip.h>
#include <FFUtils.h>
#include "BackgroundTask.h"
#include "MediaTimeline.h"
extern "C"
{
#include "libavutil/avutil.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}


namespace json = imgui_json;
using namespace std;
using namespace Logger;

namespace MEC
{

class BgtaskSceneDetect : public BackgroundTask
{
public:
    BgtaskSceneDetect(const string& name) : m_name(name)
    {
        m_pLogger = GetLogger(name);
    }

    ~BgtaskSceneDetect()
    {
        ReleaseFilterGraph();
    }

    bool Initialize(const json::value& jnTask, MediaCore::SharedSettings::Holder hSettings, RenderUtils::TextureManager::Holder hTxMgr)
    {
        string strAttrName;
        // read 'task_dir'
        strAttrName = "task_dir";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_string())
        {
            m_strTaskDir = jnTask[strAttrName].get<json::string>();
            if (!SysUtils::IsDirectory(m_strTaskDir))
            {
                ostringstream oss; oss << "INVALID task json attribute '" << strAttrName << "'! '" << m_strTaskDir << "' is NOT a DIRECTORY.";
                m_errMsg = oss.str();
                return false;
            }
            strAttrName = "task_hash";
            if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_number())
            {
                ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'string' type!";
                m_errMsg = oss.str();
                return false;
            }
            m_szHash = (size_t)jnTask[strAttrName].get<json::number>();
        }
        else
        {
            strAttrName = "project_dir";
            if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_string())
            {
                ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'string' type!";
                m_errMsg = oss.str();
                return false;
            }
            string strAttrValue = jnTask[strAttrName].get<json::string>();
            if (!SysUtils::IsDirectory(strAttrValue))
            {
                ostringstream oss; oss << "INVALID task json attribute '" << strAttrName << "'! '" << strAttrValue << "' is NOT a DIRECTORY.";
                m_errMsg = oss.str();
                return false;
            }
            m_szHash = SysUtils::GetTickHash();
            ostringstream oss; oss << m_name << "-" << setw(16) << setfill('0') << hex << m_szHash;
            const auto strWorkDirName = oss.str();
            m_strTaskDir = SysUtils::JoinPath(strAttrValue, strWorkDirName);
            if (!SysUtils::IsDirectory(m_strTaskDir))
                SysUtils::CreateDirectoryAt(m_strTaskDir, true);
        }
        m_strTrfPath = SysUtils::JoinPath(m_strTaskDir, "transforms.trf");
        // read 'source_url'
        strAttrName = "source_url";
        if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_string())
        {
            ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'string' type!";
            m_errMsg = oss.str();
            return false;
        }
        m_strSrcUrl = jnTask[strAttrName].get<json::string>();
        if (!SysUtils::IsFile(m_strSrcUrl))
        {
            ostringstream oss; oss << "INVALID task json attribute '" << strAttrName << "'! '" << m_strSrcUrl << "' is NOT a FILE.";
            m_errMsg = oss.str();
            return false;
        }
        // read 'is_image_seq'
        strAttrName = "is_image_seq";
        if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_boolean())
        {
            ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'boolean' type!";
            m_errMsg = oss.str();
            return false;
        }
        m_bIsImageSeq = jnTask[strAttrName].get<json::boolean>();
        // read 'media_item_id'
        strAttrName = "media_item_id";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_number())
            m_i64MediaItemId = jnTask[strAttrName].get<json::number>();
        else
            m_i64MediaItemId = -1;
        // check source url validity
        if (m_bIsImageSeq && !SysUtils::IsDirectory(m_strSrcUrl))
        {
            ostringstream oss; oss << "INVALID task json attribute 'source_url'! '" << m_strSrcUrl << "' is NOT a DIRECTORY.";
            m_errMsg = oss.str();
            return false;
        }
        else if (!m_bIsImageSeq && !SysUtils::IsFile(m_strSrcUrl))
        {
            ostringstream oss; oss << "INVALID task json attribute 'source_url'! '" << m_strSrcUrl << "' is NOT a FILE.";
            m_errMsg = oss.str();
            return false;
        }
        // create MediaParser instance
        MediaCore::MediaParser::Holder hParser;
        if (m_bIsImageSeq)
        {
            // check source url validity
            if (!SysUtils::IsDirectory(m_strSrcUrl))
            {
                ostringstream oss; oss << "INVALID task json attribute 'source_url'! '" << m_strSrcUrl << "' is NOT a DIRECTORY.";
                m_errMsg = oss.str();
                return false;
            }
            // read additional attributes for image-sequence
            // read 'frame_rate'
            MediaCore::Ratio tFrameRate;
            strAttrName = "frame_rate_num";
            if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_number())
            {
                ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'number' type!";
                m_errMsg = oss.str();
                return false;
            }
            tFrameRate.num = jnTask[strAttrName].get<json::number>();
            strAttrName = "frame_rate_den";
            if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_number())
            {
                ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'number' type!";
                m_errMsg = oss.str();
                return false;
            }
            tFrameRate.den = jnTask[strAttrName].get<json::number>();
            if (tFrameRate.num <= 0 || tFrameRate.den <= 0)
            {
                ostringstream oss; oss << "INVALID task json attribute '" << strAttrName << "'! '" << tFrameRate.num << "/" << tFrameRate.den << "' is NOT a valid rational.";
                m_errMsg = oss.str();
                return false;
            }
            // read "file_filter_regex"
            strAttrName = "file_filter_regex";
            if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_string())
            {
                ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'string' type!";
                m_errMsg = oss.str();
                return false;
            }
            const string strFileFilterRegex = jnTask[strAttrName].get<json::string>();
            // read "case_sensitive"
            bool bCaseSensitive = false;
            strAttrName = "case_sensitive";
            if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
                bCaseSensitive = jnTask[strAttrName].get<json::boolean>();
            // read "include_sub_dir"
            bool bIncludeSubDir = false;
            strAttrName = "include_sub_dir";
            if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
                bIncludeSubDir = jnTask[strAttrName].get<json::boolean>();
            // create MediaParser instance
            hParser = MediaCore::MediaParser::CreateInstance();
            if (!hParser)
            {
                m_errMsg = "FAILED to create MediaParser instance!";
                return false;
            }
            if (!hParser->OpenImageSequence(tFrameRate, m_strSrcUrl, strFileFilterRegex, bCaseSensitive, bIncludeSubDir))
            {
                ostringstream oss; oss << "FAILED to open image-sequence parser at '" << m_strSrcUrl << "'! Error is '" << hParser->GetError() << "'.";
                m_errMsg = oss.str();
                return false;
            }
        }
        else
        {
            if (!SysUtils::IsFile(m_strSrcUrl))
            {
                ostringstream oss; oss << "INVALID task json attribute 'source_url'! '" << m_strSrcUrl << "' is NOT a FILE.";
                m_errMsg = oss.str();
                return false;
            }
            // create MediaParser instance
            hParser = MediaCore::MediaParser::CreateInstance();
            if (!hParser)
            {
                m_errMsg = "FAILED to create MediaParser instance!";
                return false;
            }
            if (!hParser->Open(m_strSrcUrl))
            {
                ostringstream oss; oss << "FAILED to open media parser for '" << m_strSrcUrl << "'! Error is '" << hParser->GetError() << "'.";
                m_errMsg = oss.str();
                return false;
            }
        }
        m_hParser = hParser;
        m_pVidstm = hParser->GetBestVideoStream();
        if (!m_pVidstm)
        {
            ostringstream oss; oss << "FAILED to find video stream in '" << m_strSrcUrl << "'!";
            m_errMsg = oss.str();
            return false;
        }
        // create SharedSettings
        strAttrName = "use_src_attr";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
            m_bUseSrcAttr = jnTask[strAttrName].get<json::boolean>();
        else
            m_bUseSrcAttr = false;
        if (m_bUseSrcAttr)
        {
            m_hSettings = MediaCore::SharedSettings::CreateInstance();
            m_hSettings->SetVideoOutWidth(m_pVidstm->width);
            m_hSettings->SetVideoOutHeight(m_pVidstm->height);
            m_hSettings->SetVideoOutFrameRate(m_pVidstm->realFrameRate);
            MediaCore::HwaccelManager::Holder hHwMgr;
            if (hSettings)
                hHwMgr = hSettings->GetHwaccelManager();
            m_hSettings->SetHwaccelManager(hHwMgr ? hHwMgr : MediaCore::HwaccelManager::GetDefaultInstance());
        }
        else
        {
            strAttrName = "media_settings";
            if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_object())
            {
                m_hSettings = MediaCore::SharedSettings::CreateInstanceFromJson(jnTask[strAttrName]);
                if (!m_hSettings)
                {
                    m_errMsg = "FAILED to create SharedSettings instance from json!";
                    return false;
                }
                MediaCore::HwaccelManager::Holder hHwMgr;
                if (hSettings)
                    hHwMgr = hSettings->GetHwaccelManager();
                m_hSettings->SetHwaccelManager(hHwMgr ? hHwMgr : MediaCore::HwaccelManager::GetDefaultInstance());
            }
            else
            {
                if (hSettings)
                    m_hSettings = hSettings;
                else
                {
                    m_errMsg = "Argument 'SharedSettings' is null!";
                    return false;
                }
            }
        }
        // create VideoClip instance
        strAttrName = "parse_start_offset";
        if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_number())
        {
            ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'number' type!";
            m_errMsg = oss.str();
            return false;
        }
        m_i64ParseStartOffset = jnTask[strAttrName].get<json::number>();
        strAttrName = "parse_length";
        if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_number())
        {
            ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'number' type!";
            m_errMsg = oss.str();
            return false;
        }
        m_i64ParseLength = jnTask[strAttrName].get<json::number>();
        const int64_t i64SrcDuration = static_cast<int64_t>(m_pVidstm->duration*1000);
        const int64_t i64ClipEndOffset = i64SrcDuration-m_i64ParseStartOffset-m_i64ParseLength;
        MediaCore::VideoClip::Holder hVclip = MediaCore::VideoClip::CreateVideoInstance(m_i64MediaItemId, hParser, m_hSettings,
                0, m_i64ParseLength, m_i64ParseStartOffset, i64ClipEndOffset, 0, true);
        if (!hVclip)
        {
            ostringstream oss; oss << "FAILED to create VideoClip instance for '" << m_strSrcUrl << "' with (start, end, startOffset, endOffset) = ("
                    << 0 << ", " << m_i64ParseLength << ", " << m_i64ParseStartOffset << ", " << i64ClipEndOffset << ").";
            m_errMsg = oss.str();
            return false;
        }
        m_hParseVclip = hVclip;
        auto hPreviewSettings = m_hSettings->Clone();
        hPreviewSettings->SetVideoOutWidth(m_u32PreviewWidth); hPreviewSettings->SetVideoOutHeight(m_u32PreviewHeight);
        m_hPreviewVclip = hVclip->Clone(hPreviewSettings);
        const auto tOutputFrameRate = m_hSettings->VideoOutFrameRate();
        m_tVidTimeBase = { tOutputFrameRate.den, tOutputFrameRate.num };
        m_hTxMgr = hTxMgr ? hTxMgr : RenderUtils::TextureManager::GetDefaultInstance();
        // read scene detect arguments
        strAttrName = "scene_detect_thresh";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_number())
        {
            const auto numValue = jnTask[strAttrName].get<json::number>();
            if (numValue >= 0 || numValue <= 1)
                m_fSceneDetectThresh = (float)numValue;
            else
            {
                ostringstream oss; oss << "INVALID argument '" << strAttrName << "'! The valid value should be an integer in the range of [1, 10], while the provided value is "
                        << numValue << ".";
                m_errMsg = oss.str();
                return false;
            }
        }
        // read task status
        strAttrName = "parsed_frame_idx";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_number())
            m_i64ParsedFrameIdx = (int64_t)jnTask[strAttrName].get<json::number>();
        strAttrName = "scene_cut_points";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_array())
        {
            const auto& jnSceneCutPoints = jnTask[strAttrName].get<json::array>();
            for (const auto& jnElem : jnSceneCutPoints)
                m_aSceneCutPoints.push_back(_SceneCutPoint::FromJson(jnElem));
        }
        strAttrName = "diff_scores";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_array())
        {
            const auto& jnDiffScores = jnTask[strAttrName].get<json::array>();
            for (const auto& jnElem : jnDiffScores)
                m_aDiffScores.push_back((float)jnElem.get<json::number>());
        }

        bool bFailed = false;
        bool bDone = false;
        strAttrName = "is_task_failed";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
            bFailed = jnTask[strAttrName].get<json::boolean>();
        if (bFailed)
        {
            strAttrName = "error_message";
            if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_string())
                m_errMsg = jnTask[strAttrName].get<json::string>();
            SetState(FAILED, true);
        }
        else
        {
            strAttrName = "is_task_done";
            if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
                bDone = jnTask[strAttrName].get<json::boolean>();
            if (bDone)
                SetState(DONE, true);
        }

        // initialize ui vars
        if (bDone)
        {
            m_fProgress = 1.f;
        }
        else
        {
            if (m_i64ParsedFrameIdx > 0)
                m_i64ParsedFrameIdx--;
            const auto i64ParsedTimeMts = av_rescale_q(m_i64ParsedFrameIdx, m_tVidTimeBase, MILLISEC_TIMEBASE);
            m_fProgress = (double)i64ParsedTimeMts/m_hParseVclip->Duration();
        }
        ostringstream oss; oss << "##" << m_name << "-" << setw(16) << setfill('0') << hex << m_szHash;
        m_strTaskNameWithHash = oss.str();
        oss.str(""); oss << "##ShowResultPopupDlg" << m_strTaskNameWithHash;
        m_strShowResultPopupLabel = oss.str();

        m_bInited = true;
        return true;
    }

    void SetCallbacks(Callbacks* pCb) override
    {
        m_pCb = pCb;
    }

    bool CanPause()
    {
        return m_eState == PROCESSING;
    }

    bool Pause() override
    {
        if (m_bPause)
            return true;
        m_bPauseCheckPointHit = false;
        m_bPause = true;
        return true;
    }

    bool IsPaused() const override
    {
        return m_bPause && m_bPauseCheckPointHit;
    }

    bool Resume() override
    {
        m_bPause = false;
        return true;
    }

    bool DrawContent(const ImVec2& v2ViewSize) override
    {
        bool bRemoveThisTask = false;
        ostringstream oss;
        auto strLabel = m_strTaskNameWithHash;
        ImGui::BeginChild(strLabel.c_str(), v2ViewSize, ImGuiChildFlags_Border|ImGuiChildFlags_AutoResizeY);
        const auto v2AreaPos = ImGui::GetCursorPos();
        const auto v2AreaAvailSize = ImGui::GetContentRegionAvail();
        const ImColor tTaskTitleClr(KNOWNIMGUICOLOR_WHITESMOKE);
        const auto v2TextPadding = ImGui::GetStyle().FramePadding;
        const auto orgFontScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale = 1.2f;
        ImGui::PushFont(ImGui::GetFont());
        ImGui::TextColoredWithPadding(tTaskTitleClr, v2TextPadding, "%s", TASK_TYPE_NAME.c_str()); ImGui::SameLine();
        ImGui::GetFont()->Scale = orgFontScale;
        ImGui::PopFont();
        auto v2CurrPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos({v2AreaPos.x+v2AreaAvailSize.x-60, v2CurrPos.y});
        bool bDisableThisWidget;
        oss.str(""); oss << (IsPaused() ? ICON_PLAY_FORWARD : ICON_PAUSE) << m_strTaskNameWithHash;
        strLabel = oss.str();
        bDisableThisWidget = !CanPause();
        ImGui::BeginDisabled(bDisableThisWidget);
        if (ImGui::Button(strLabel.c_str()))
        {
            if (m_bPause)
                Resume();
            else
                Pause();
        } ImGui::SameLine();
        ImGui::ShowTooltipOnHover(bDisableThisWidget
                ? (IsWaiting() ? "Task hasn't started yet." : "Task is already stopped.")
                : (m_bPause ? "Resume task" : "Pause task"));
        ImGui::EndDisabled();
        oss.str(""); oss << ICON_DELETE << m_strTaskNameWithHash;
        strLabel = oss.str();
        oss.str(""); oss << ICON_TRASH << " Task Deletion" << m_strTaskNameWithHash;
        const auto strDelLabel = oss.str();
        bDisableThisWidget = false;
        ImGui::BeginDisabled(bDisableThisWidget);
        if (ImGui::Button(strLabel.c_str()))
        {
            ImGui::OpenPopup(strDelLabel.c_str());
        }
        ImGui::ShowTooltipOnHover(bDisableThisWidget ? "Can not delete this task." : "Delete this task.");
        ImGui::EndDisabled();
        const ImColor tTagClr(KNOWNIMGUICOLOR_LIGHTGRAY);
        ImGui::TextColoredWithPadding(tTagClr, v2TextPadding, "State: "); ImGui::SameLine(0, 10);
        switch (m_eState)
        {
        case WAITING:
            ImGui::TextColoredWithPadding(ImColor(0.8f, 0.8f, 0.1f), v2TextPadding, "Waiting");
            break;
        case PROCESSING:
            if (m_bPause)
                ImGui::TextColoredWithPadding(ImColor(0.8f, 0.8f, 0.1f), v2TextPadding, "Paused");
            else
                ImGui::TextColoredWithPadding(ImColor(0.3f, 0.3f, 0.85f), v2TextPadding, "Processing");
            break;
        case DONE:
            ImGui::TextColoredWithPadding(ImColor(0.3f, 0.85f, 0.3f), v2TextPadding, "Done");
            break;
        case FAILED:
            ImGui::TextColoredWithPadding(ImColor(0.85f, 0.3f, 0.3f), v2TextPadding, "FAILED");
            break;
        case CANCELLED:
            ImGui::TextColoredWithPadding(ImColor(0.8f, 0.8f, 0.8f), v2TextPadding, "Cancelled");
            break;
        default:
            ImGui::TextColoredWithPadding(ImColor(0.7f, 0.3f, 0.3f), v2TextPadding, "Unknown");
        }
        ImGui::TextColoredWithPadding(tTagClr, v2TextPadding, "Progress: "); ImGui::SameLine(0, 10);
        ImGui::TextColoredWithPadding(ImColor(0.3f, 0.85f, 0.3f), v2TextPadding, "%.02f%%", m_fProgress*100);
        ImGui::SameLine(); v2CurrPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos({v2AreaPos.x+v2AreaAvailSize.x-126, v2CurrPos.y});
        oss.str(""); oss << ICON_WATCH << " Show Result" << m_strTaskNameWithHash;
        strLabel = oss.str();
        if (ImGui::Button(strLabel.c_str()))
        {
            const auto bIsPopupOpen = ImGui::IsPopupOpen(m_strShowResultPopupLabel.c_str());
            if (!bIsPopupOpen)
                ImGui::OpenPopup(m_strShowResultPopupLabel.c_str());
        }

        if (ImGui::BeginPopupModal(strDelLabel.c_str(), nullptr, ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings))
        {
            bool bClosePopup = false;
            const ImColor tWarnMsgClr(KNOWNIMGUICOLOR_PALEVIOLETRED);
            ImGui::TextColoredWithPadding(tWarnMsgClr, {10, 6}, "This task and all of its intermediat result will be removed!");
            if (ImGui::Button("  OK  "))
            {
                Cancel(); WaitDone();
                bRemoveThisTask = true;
                bClosePopup = true;
            } ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                bClosePopup = true;
            if (bClosePopup)
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::SetNextWindowSize({1200, 0});
        if (ImGui::BeginPopupModal(m_strShowResultPopupLabel.c_str(), nullptr, ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings))
        {
            bool bClosePopup = false;

            // >>>> Show scene detect cut point frame pair
            const auto aSceneCutPoints = m_aSceneCutPoints;
            int64_t i64PreviewFrmIdx = -1;
            if (!aSceneCutPoints.empty())
            {
                if (m_iSelPrevwSceneCutIdx >= aSceneCutPoints.size()) m_iSelPrevwSceneCutIdx = aSceneCutPoints.size()-1;
                i64PreviewFrmIdx = aSceneCutPoints[m_iSelPrevwSceneCutIdx].i64FrameIdx;
            }
            const int64_t i64ReadPos0 = av_rescale_q(i64PreviewFrmIdx-1, m_tVidTimeBase, MILLISEC_TIMEBASE);
            const int64_t i64ReadPos1 = av_rescale_q(i64PreviewFrmIdx, m_tVidTimeBase, MILLISEC_TIMEBASE);
            if (i64PreviewFrmIdx > 0 && m_i64PrevwFrameIdx != i64PreviewFrmIdx)
            {
                m_hPreviewVclip->SeekTo(i64ReadPos0);
                vector<MediaCore::CorrelativeFrame> frames;
                bool eof;
                auto hVfrm1 = m_hPreviewVclip->ReadVideoFrame(i64ReadPos0, frames, eof);
                ImGui::ImMat vmat;
                if (hVfrm1->GetMat(vmat))
                {
                    if (m_hPrevwTx1)
                        m_hPrevwTx1->RenderMatToTexture(vmat);
                    else
                    {
                        MatUtils::Size2i prevwSize;
                        m_hPrevwTx1 = m_hTxMgr->CreateManagedTextureFromMat(vmat, prevwSize);
                    }
                }
                auto hVfrm2 = m_hPreviewVclip->ReadVideoFrame(i64ReadPos1, frames, eof);
                if (hVfrm2->GetMat(vmat))
                {
                    if (m_hPrevwTx2)
                        m_hPrevwTx2->RenderMatToTexture(vmat);
                    else
                    {
                        MatUtils::Size2i prevwSize;
                        m_hPrevwTx2 = m_hTxMgr->CreateManagedTextureFromMat(vmat, prevwSize);
                    }
                }
                m_i64PrevwFrameIdx = i64PreviewFrmIdx;
            }
            const auto tid1 = m_hPrevwTx1 ? m_hPrevwTx1->TextureID() : 0;
            const auto tid2 = m_hPrevwTx2 ? m_hPrevwTx2->TextureID() : 0;
            const ImVec2 v2PrevwImgSize(m_u32PreviewWidth, m_u32PreviewHeight);
            if (tid1)
                ImGui::Image(tid1, v2PrevwImgSize);
            else
                ImGui::Dummy(v2PrevwImgSize);
            ImGui::SameLine(0, 20);
            if (tid2)
                ImGui::Image(tid2, v2PrevwImgSize);
            else
                ImGui::Dummy(v2PrevwImgSize);
            // <<<<

            // >>>> show scene detect cut points
            ImGui::SameLine(0, 20);
            ImGuiTableFlags uiTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
                | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
                | ImGuiTableFlags_SizingFixedFit;
            if (ImGui::BeginTable("Scene cut points", 6, uiTableFlags, ImVec2(0, m_u32SceneCutTableHeight)))
            {
                ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Time",  ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                if (!aSceneCutPoints.empty())
                {
                    ImGuiListClipper clipper;
                    clipper.Begin(aSceneCutPoints.size());
                    while (clipper.Step())
                    {
                        for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                        {
                            ImGui::PushID(rowIdx);
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            oss.str(""); oss << rowIdx;
                            strLabel = oss.str();
                            auto bIsSelected = rowIdx == m_iSelPrevwSceneCutIdx;
                            if (ImGui::Selectable(strLabel.c_str(), bIsSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                                m_iSelPrevwSceneCutIdx = rowIdx;
                            ImGui::TableSetColumnIndex(1);
                            const int64_t i64Pos = av_rescale_q(aSceneCutPoints[rowIdx].i64FrameIdx, m_tVidTimeBase, MILLISEC_TIMEBASE);
                            strLabel = MillisecToString(i64Pos);
                            ImGui::TextUnformatted(strLabel.c_str());
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndTable();
            }

            // <<<<

            ImGui::Spacing();
            if (ImGui::Button("  OK  "))
            {
                bClosePopup = true;
            }
            if (bClosePopup)
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        return bRemoveThisTask;
    }

    void DrawContentCompact() override
    {

    }

    bool SaveAsJson(json::value& jnTask) override
    {
        jnTask = json::value();
        // save basic info
        jnTask["type"] = "SceneDetect";
        jnTask["name"] = m_name;
        jnTask["task_hash"] = json::number(m_szHash);
        jnTask["task_dir"] = m_strTaskDir;
        jnTask["source_url"] = m_strSrcUrl;
        jnTask["is_image_seq"] = m_bIsImageSeq;
        jnTask["media_item_id"] = json::number(m_i64MediaItemId);
        json::value jnSettings;
        if (!m_hSettings->SaveAsJson(jnSettings))
        {
            m_errMsg = "FAILED to save 'SharedSettings' as json!";
            return false;
        }
        jnTask["media_settings"] = jnSettings;
        if (m_bIsImageSeq)
        {
            const auto& tFrameRate = m_pVidstm->realFrameRate;
            auto hFileIter = m_hParser->GetImageSequenceIterator();
            jnTask["frame_rate_num"] = json::number(tFrameRate.num);
            jnTask["frame_rate_den"] = json::number(tFrameRate.den);
            bool bIsRegexPattern;
            const auto strFileFilterRegex = hFileIter->GetFilterPattern(bIsRegexPattern);
            jnTask["file_filter_regex"] = strFileFilterRegex;
            jnTask["case_sensitive"] = hFileIter->IsCaseSensitive();
            jnTask["include_sub_dir"] = hFileIter->IsRecursive();
        }
        jnTask["parse_start_offset"] = json::number(m_i64ParseStartOffset);
        jnTask["parse_length"] = json::number(m_i64ParseLength);
        // save scene detect parameters
        jnTask["scene_detect_thresh"] = json::number(m_fSceneDetectThresh);
        // save task status
        jnTask["parsed_frame_idx"] = json::number(m_i64ParsedFrameIdx);
        json::array jnSceneCutPoints;
        for (const auto& elem : m_aSceneCutPoints)
            jnSceneCutPoints.push_back(elem.SaveAsJson());
        jnTask["scene_cut_points"] = jnSceneCutPoints;
        json::array jnDiffScores;
        for (const auto& elem : m_aDiffScores)
            jnDiffScores.push_back(json::number(elem));
        jnTask["diff_scores"] = jnDiffScores;
        jnTask["is_task_done"] = IsDone();
        jnTask["is_task_failed"] = IsFailed();
        jnTask["error_message"] = m_errMsg;
        return true;
    }

    string Save(const string& _strSavePath) override
    {
        json::value jnTask;
        if (!SaveAsJson(jnTask))
        {
            m_pLogger->Log(Error) << "FAILED to save '" << m_name << "' as json!" << endl;
            return "";
        }
        const auto strSavePath = _strSavePath.empty() ? SysUtils::JoinPath(m_strTaskDir, "task.json") : _strSavePath;
        if (!jnTask.save(strSavePath))
        {
            m_pLogger->Log(Error) << "FAILED to save task json of '" << m_name << "' at location '" << strSavePath << "'!" << endl;
            return "";
        }
        return strSavePath;
    }

    string GetTaskDir() const override
    {
        return m_strTaskDir;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

    void SetLogLevel(Logger::Level l) override
    {
        m_pLogger->SetShowLevels(l);
    }

public:
    static const string TASK_TYPE_NAME;

protected:
    struct _SceneCutPoint
    {
        int64_t i64FrameIdx;
        float fScore;

        json::value SaveAsJson() const
        {
            json::value j;
            j["frame_index"] = json::number(i64FrameIdx);
            j["score"] = json::number(fScore);
            return std::move(j);
        }

        static _SceneCutPoint FromJson(const json::value& j)
        {
            _SceneCutPoint newinst;
            string strAttrName;
            strAttrName = "frame_index";
            if (j.contains(strAttrName) && j[strAttrName].is_number())
                newinst.i64FrameIdx = (int64_t)j[strAttrName].get<json::number>();
            strAttrName = "score";
            if (j.contains(strAttrName) && j[strAttrName].is_number())
                newinst.fScore = (float)j[strAttrName].get<json::number>();
            return std::move(newinst);
        }
    };

    bool _TaskProc () override
    {
        m_pLogger->Log(INFO) << "Start background task 'SceneDetect' for '" << m_strSrcUrl << "'." << endl;
        if (!m_bInited)
        {
            ostringstream oss; oss << "Background task 'SceneDetect' with name '" << m_name << "' is NOT initialized!";
            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
            return false;
        }

        float fStageProgress = 0.f, fStageShare = 0.5f, fAccumShares = 0.f;
        bool bFilterGraphInited = false;
        const int64_t i64ClipDur = m_hParseVclip->Duration();
        ImMatToAVFrameConverter tMat2AvfrmCvter;
        tMat2AvfrmCvter.SetOutPixelFormat(m_eFgInputPixfmt);
        const auto tFrameRate = m_hSettings->VideoOutFrameRate();
        const AVRational tb = { tFrameRate.den, tFrameRate.num };
        int64_t i64FrmIdx = m_i64ParsedFrameIdx;
        if (i64FrmIdx > 0)
        {
            const int64_t i64ReadPos = round((double)i64FrmIdx*1000*tFrameRate.den/tFrameRate.num);
            m_hParseVclip->SeekTo(i64ReadPos);
        }

        SelfFreeAVFramePtr hFgOutfrmPtr = AllocSelfFreeAVFramePtr();
        while (!IsCancelled())
        {
            if (m_bPause)
            {
                m_bPauseCheckPointHit = true;
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
                continue;
            }

            int fferr;
            SelfFreeAVFramePtr hFgInfrmPtr;
            const int64_t i64ReadPos = round((double)i64FrmIdx*1000*tFrameRate.den/tFrameRate.num);
            bool bEof = false;
            auto hVfrm = m_hParseVclip->ReadSourceFrame(i64ReadPos, bEof, true);
            ImMatWrapper_AVFrame tAvfrmWrapper;
            if (hVfrm)
            {
                auto tNativeData = hVfrm->GetNativeData();
                if (tNativeData.eType == MediaCore::VideoFrame::NativeData::AVFRAME)
                    hFgInfrmPtr = CloneSelfFreeAVFramePtr((AVFrame*)tNativeData.pData);
                else if (tNativeData.eType == MediaCore::VideoFrame::NativeData::AVFRAME_HOLDER)
                    hFgInfrmPtr = *((SelfFreeAVFramePtr*)tNativeData.pData);
                else if (tNativeData.eType == MediaCore::VideoFrame::NativeData::MAT)
                {
                    const auto& vmat = *((ImGui::ImMat*)tNativeData.pData);
                    if (vmat.device != IM_DD_CPU)
                    {
                        hFgInfrmPtr = AllocSelfFreeAVFramePtr();
                        tMat2AvfrmCvter.ConvertImage(vmat, hFgInfrmPtr.get(), i64FrmIdx);
                    }
                    else
                    {
                        tAvfrmWrapper.SetMat(vmat);
                        hFgInfrmPtr = tAvfrmWrapper.GetWrapper(i64FrmIdx);
                    }
                }
            }
            if (hFgInfrmPtr)
            {
                hFgInfrmPtr->pts = i64FrmIdx;
                if (!bFilterGraphInited)
                {
                    if (!SetupSceneDetectFilterGraph(hFgInfrmPtr.get()))
                    {
                        m_pLogger->Log(Error) << "'SetupSceneDetectFilterGraph()' FAILED!" << endl;
                        return false;
                    }
                    bFilterGraphInited = true;
                }

                fferr = av_buffersrc_add_frame(m_pBufsrcCtx, hFgInfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss; oss << "Background task 'SceneDetect' FAILED when invoking 'av_buffersrc_add_frame()' at frame #" << (i64FrmIdx-1)
                            << ". fferr=" << fferr << ".";
                    m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                    return false;
                }
                m_i64ParsedFrameIdx = i64FrmIdx;
                i64FrmIdx++;

                av_frame_unref(hFgOutfrmPtr.get());
                fferr = av_buffersink_get_frame(m_pBufsinkCtx, hFgOutfrmPtr.get());
                if (fferr != AVERROR(EAGAIN))
                {
                    if (fferr == 0)
                    {
                        float fScore = 0;
                        auto dictEntry = av_dict_get(hFgOutfrmPtr->metadata, "lavfi.scene_score", nullptr, 0);
                        if (dictEntry)
                            fScore = stof(dictEntry->value);
                        m_aDiffScores.push_back(fScore);
                        if (fScore >= m_fSceneDetectThresh)
                        {
                            m_aSceneCutPoints.push_back({hFgOutfrmPtr->pts, fScore});
                            int64_t mts = av_rescale_q(hFgOutfrmPtr->pts, tb, MILLISEC_TIMEBASE);
                            m_pLogger->Log(INFO) << "Scene detect output: frame#" << hFgOutfrmPtr->pts << ", time=" << MillisecToString(mts) << ", score=" << fScore << endl;
                        }
                    }
                    else
                    {
                        ostringstream oss; oss << "Background task 'SceneDetect' FAILED when invoking 'av_buffersink_get_frame()' at frame #" << (i64FrmIdx-1)
                                << ". fferr=" << fferr << ".";
                        m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                        return false;
                    }
                }
            }
            float fStageProgress = (float)((double)i64ReadPos/i64ClipDur);
            m_fProgress = fAccumShares+(fStageProgress*fStageShare);
            if (bEof)
                break;
        }
        ReleaseFilterGraph();
        m_hParseVclip = nullptr;

        m_fProgress = 1.f;
        m_pLogger->Log(INFO) << "Quit background task 'SceneDetect' for '" << m_strSrcUrl << "'." << endl;
        return true;
    }

    bool _AfterTaskProc() override
    {
        ReleaseFilterGraph();
        return true;
    }

private:
    bool SetupSceneDetectFilterGraph(const AVFrame* pInAvfrm)
    {
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");
        const AVFilter *buffersrc  = avfilter_get_by_name("buffer");

        m_pFilterGraph = avfilter_graph_alloc();
        if (!m_pFilterGraph)
        {
            m_errMsg = "FAILED to allocate new 'AVFilterGraph'!";
            return false;
        }

        int fferr;
        ostringstream oss;
        m_eFgInputPixfmt = (AVPixelFormat)pInAvfrm->format;
        const auto tFrameRate = m_hSettings->VideoOutFrameRate();
        oss << pInAvfrm->width << ":" << pInAvfrm->height << ":pix_fmt=" << (int)m_eFgInputPixfmt << ":sar=1"
                << ":time_base=" << tFrameRate.den << "/" << tFrameRate.num << ":frame_rate=" << tFrameRate.num << "/" << tFrameRate.den;
        string bufsrcArg = oss.str();
        m_pBufsrcCtx = nullptr;
        fferr = avfilter_graph_create_filter(&m_pBufsrcCtx, buffersrc, "buffer_source", bufsrcArg.c_str(), nullptr, m_pFilterGraph);
        if (fferr < 0)
        {
            oss << "FAILED when invoking 'avfilter_graph_create_filter' for INPUT 'buffer_source'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }
        AVFilterInOut* filtInOutPtr = avfilter_inout_alloc();
        if (!filtInOutPtr)
        {
            m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
            return false;
        }
        filtInOutPtr->name       = av_strdup("in");
        filtInOutPtr->filter_ctx = m_pBufsrcCtx;
        filtInOutPtr->pad_idx    = 0;
        filtInOutPtr->next       = nullptr;
        m_pFilterOutputs = filtInOutPtr;

        m_pBufsinkCtx = nullptr;
        fferr = avfilter_graph_create_filter(&m_pBufsinkCtx, buffersink, "buffer_sink", nullptr, nullptr, m_pFilterGraph);
        if (fferr < 0)
        {
            oss << "FAILED when invoking 'avfilter_graph_create_filter' for OUTPUT 'out'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }
        filtInOutPtr = avfilter_inout_alloc();
        if (!filtInOutPtr)
        {
            m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
            return false;
        }
        filtInOutPtr->name        = av_strdup("out");
        filtInOutPtr->filter_ctx  = m_pBufsinkCtx;
        filtInOutPtr->pad_idx     = 0;
        filtInOutPtr->next        = nullptr;
        m_pFilterInputs = filtInOutPtr;

        const int iOutW = (int)m_hSettings->VideoOutWidth();
        const int iOutH = (int)m_hSettings->VideoOutHeight();
        oss.str("");
        if (pInAvfrm->width != iOutW || pInAvfrm->height != iOutH)
        {
            string strInterpAlgo = iOutW*iOutH >= pInAvfrm->width*pInAvfrm->height ? "bicubic" : "area";
            oss << "scale=w=" << iOutW << ":h=" << iOutH << ":flags=" << strInterpAlgo << ",";
        }
        if ((AVPixelFormat)pInAvfrm->format != AV_PIX_FMT_RGB24)
        {
            oss << "format=rgb24,";
        }
        oss << "select='gte(scene\\,0)'";
        string filterArgs = oss.str();
        fferr = avfilter_graph_parse_ptr(m_pFilterGraph, filterArgs.c_str(), &m_pFilterInputs, &m_pFilterOutputs, nullptr);
        if (fferr < 0)
        {
            oss.str(""); oss << "FAILED to invoke 'avfilter_graph_parse_ptr'! fferr=" << fferr << ". Arguments are \"" << filterArgs << "\".";
            m_errMsg = oss.str();
            return false;
        }
        m_pLogger->Log(INFO) << "Setup filter-graph with arguments: '" << filterArgs << "'." << endl;

        fferr = avfilter_graph_config(m_pFilterGraph, nullptr);
        if (fferr < 0)
        {
            oss << "FAILED to invoke 'avfilter_graph_config'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

        if (m_pFilterOutputs)
            avfilter_inout_free(&m_pFilterOutputs);
        if (m_pFilterInputs)
            avfilter_inout_free(&m_pFilterInputs);
        return true;
    }

    void ReleaseFilterGraph()
    {
        if (m_pFilterOutputs)
        {
            avfilter_inout_free(&m_pFilterOutputs);
            m_pFilterOutputs = nullptr;
        }
        if (m_pFilterInputs)
        {
            avfilter_inout_free(&m_pFilterInputs);
            m_pFilterInputs = nullptr;
        }
        m_pBufsrcCtx = nullptr;
        m_pBufsinkCtx = nullptr;
        if (m_pFilterGraph)
        {
            avfilter_graph_free(&m_pFilterGraph);
            m_pFilterGraph = nullptr;
        }
    }

private:
    string m_name;
    size_t m_szHash;
    string m_errMsg;
    ALogger* m_pLogger;
    Callbacks* m_pCb{nullptr};
    bool m_bInited{false};
    string m_strTaskDir;
    AVFilterGraph* m_pFilterGraph{nullptr};
    AVFilterContext* m_pBufsrcCtx;
    AVFilterContext* m_pBufsinkCtx;
    AVFilterInOut* m_pFilterOutputs{nullptr};
    AVFilterInOut* m_pFilterInputs{nullptr};
    AVPixelFormat m_eFgInputPixfmt;
    string m_strTrfPath;
    string m_strSrcUrl;
    int64_t m_i64MediaItemId;
    int64_t m_i64ParseStartOffset, m_i64ParseLength;
    MediaCore::MediaParser::Holder m_hParser;
    MediaCore::VideoClip::Holder m_hParseVclip;
    MediaCore::VideoClip::Holder m_hPreviewVclip;
    RenderUtils::TextureManager::Holder m_hTxMgr;
    RenderUtils::ManagedTexture::Holder m_hPrevwTx1, m_hPrevwTx2;
    const MediaCore::VideoStream* m_pVidstm{nullptr};
    MediaCore::SharedSettings::Holder m_hSettings;
    AVRational m_tVidTimeBase;
    bool m_bIsImageSeq{false};
    bool m_bUseSrcAttr;
    // scene detect parameters
    float m_fSceneDetectThresh{0.4f};
    // output
    string m_strOutputPath;
    vector<_SceneCutPoint> m_aSceneCutPoints;
    list<float> m_aDiffScores;
    // task control
    int64_t m_i64ParsedFrameIdx{0};
    float m_fProgress{0.f};
    bool m_bPause{false};
    bool m_bPauseCheckPointHit{false};
    // ui vars
    string m_strTaskNameWithHash;
    uint32_t m_u32PreviewWidth{480}, m_u32PreviewHeight{270};
    string m_strShowResultPopupLabel;
    int m_iSelPrevwSceneCutIdx{0};
    int64_t m_i64PrevwFrameIdx{-1};
    uint32_t m_u32SceneCutTableHeight{350};
};

const string BgtaskSceneDetect::TASK_TYPE_NAME = "Scene Detect";

static const auto _BGTASK_SCENEDETECT_DELETER = [] (BackgroundTask* p) {
    BgtaskSceneDetect* ptr = dynamic_cast<BgtaskSceneDetect*>(p);
    delete ptr;
};

BackgroundTask::Holder CreateBgtask_SceneDetect(const json::value& jnTask, MediaCore::SharedSettings::Holder hSettings, RenderUtils::TextureManager::Holder hTxMgr)
{
    string strTaskName;
    string strAttrName = "name";
    if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_string())
        strTaskName = jnTask["name"].get<json::string>();
    else
        strTaskName = "BgtskSceneDetect";
    auto p = new BgtaskSceneDetect(strTaskName);
    if (!p->Initialize(jnTask, hSettings, hTxMgr))
    {
        Log(Error) << "FAILED to create new 'SceneDetect' background task! Error is '" << p->GetError() << "'." << endl;
        delete p; 
        return nullptr;
    }
    p->Save("");
    return BackgroundTask::Holder(p, _BGTASK_SCENEDETECT_DELETER);
}
}
