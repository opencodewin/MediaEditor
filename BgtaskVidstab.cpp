#include <cstdint>
#include <sstream>
#include <ios>
#include <iomanip>
#include <cassert>
#include <TimeUtils.h>
#include <FileSystemUtils.h>
#include <MediaParser.h>
#include <VideoClip.h>
#include <MediaEncoder.h>
#include <FFUtils.h>
#include <imgui.h>
#include "BackgroundTask.h"
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
class BgtaskVidstab : public BackgroundTask
{
public:
    BgtaskVidstab(const string& name) : m_name(name)
    {
        m_pLogger = GetLogger(name);
    }

    ~BgtaskVidstab()
    {
        ReleaseFilterGraph();
    }

    bool Initialize(const json::value& jnTask, MediaCore::SharedSettings::Holder hSettings)
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
                SysUtils::CreateDirectory(m_strTaskDir, true);
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
        // read 'clip_id'
        strAttrName = "clip_id";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_number())
            m_i64ClipId = jnTask[strAttrName].get<json::number>();
        else
            m_i64ClipId = -1;
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
        m_pVidstm = hParser->GetBestVideoStream();
        if (!m_pVidstm)
        {
            ostringstream oss; oss << "FAILED to find video stream in '" << m_strSrcUrl << "'!";
            m_errMsg = oss.str();
            return false;
        }
        // create SharedSettings
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
        // create VideoClip instance
        strAttrName = "clip_start_offset";
        if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_number())
        {
            ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'number' type!";
            m_errMsg = oss.str();
            return false;
        }
        const int64_t i64ClipStartOffset = jnTask[strAttrName].get<json::number>();
        strAttrName = "clip_length";
        if (!jnTask.contains(strAttrName) || !jnTask[strAttrName].is_number())
        {
            ostringstream oss; oss << "Task json must has a '" << strAttrName << "' attribute of 'number' type!";
            m_errMsg = oss.str();
            return false;
        }
        const int64_t i64ClipLength = jnTask[strAttrName].get<json::number>();
        const int64_t i64SrcDuration = static_cast<int64_t>(m_pVidstm->duration*1000);
        const int64_t i64ClipEndOffset = i64SrcDuration-i64ClipStartOffset-i64ClipLength;
        MediaCore::VideoClip::Holder hVclip = MediaCore::VideoClip::CreateVideoInstance(m_i64ClipId, hParser, m_hSettings,
                0, i64ClipLength, i64ClipStartOffset, i64ClipEndOffset, 0, true);
        if (!hVclip)
        {
            ostringstream oss; oss << "FAILED to create VideoClip instance for '" << m_strSrcUrl << "' with (start, end, startOffset, endOffset) = ("
                    << 0 << ", " << i64ClipLength << ", " << i64ClipStartOffset << ", " << i64ClipEndOffset << ").";
            m_errMsg = oss.str();
            return false;
        }
        m_hVclip = hVclip;

        strAttrName = "is_vidstab_detect_done";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
            m_bVidstabDetectFinished = jnTask[strAttrName].get<json::boolean>();
        else
            m_bVidstabDetectFinished = false;
        strAttrName = "is_vidstab_transform_done";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
            m_bVidstabTransformFinished = jnTask[strAttrName].get<json::boolean>();
        else
            m_bVidstabTransformFinished = false;
        bool bFailed = false;
        strAttrName = "is_task_failed";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
            bFailed = jnTask[strAttrName].get<json::boolean>();
        if (bFailed)
        {
            strAttrName = "error_message";
            if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_string())
                m_errMsg = jnTask[strAttrName].get<json::string>();
            SetState(DONE);
        }

        m_strOutputPath = SysUtils::JoinPath(m_strTaskDir, "TaskOutput.mp4");
        m_bInited = true;
        return true;
    }

    void DrawContent() override
    {
        ImGui::TextUnformatted("State: "); ImGui::SameLine(0, 10);
        switch (m_eState)
        {
        case WAITING:
            ImGui::TextColored(ImColor(0.8f, 0.8f, 0.1f), "Waiting");
            break;
        case PROCESSING:
            ImGui::TextColored(ImColor(0.3f, 0.3f, 0.85f), "Processing");
            break;
        case DONE:
            ImGui::TextColored(ImColor(0.3f, 0.85f, 0.3f), "Done");
            break;
        case FAILED:
            ImGui::TextColored(ImColor(0.85f, 0.3f, 0.3f), "FAILED");
            break;
        case CANCELLED:
            ImGui::TextColored(ImColor(0.8f, 0.8f, 0.8f), "Cancelled");
            break;
        default:
            ImGui::TextColored(ImColor(0.7f, 0.3f, 0.3f), "Unknown");
        }
        ImGui::TextUnformatted("Progress: "); ImGui::SameLine(0, 10);
        ImGui::Text("%.02f%%", m_fProgress*100);
    }

    void DrawContentCompact() override
    {

    }

    bool SaveAsJson(json::value& jnTask) override
    {
        jnTask = json::value();
        jnTask["type"] = "Vidstab";
        jnTask["name"] = m_name;
        jnTask["task_hash"] = json::number(m_szHash);
        jnTask["task_dir"] = m_strTaskDir;
        jnTask["source_url"] = m_strSrcUrl;
        jnTask["is_image_seq"] = m_bIsImageSeq;
        jnTask["clip_id"] = json::number(m_i64ClipId);
        json::value jnSettings;
        if (!m_hSettings->SaveAsJson(jnSettings))
        {
            m_errMsg = "FAILED to save 'SharedSettings' as json!";
            return false;
        }
        jnTask["media_settings"] = jnSettings;
        auto hParser = m_hVclip->GetMediaParser();
        if (m_bIsImageSeq)
        {
            const auto& tFrameRate = m_pVidstm->realFrameRate;
            auto hFileIter = hParser->GetImageSequenceIterator();
            jnTask["frame_rate_num"] = json::number(tFrameRate.num);
            jnTask["frame_rate_den"] = json::number(tFrameRate.den);
            bool bIsRegexPattern;
            const auto strFileFilterRegex = hFileIter->GetFilterPattern(bIsRegexPattern);
            jnTask["file_filter_regex"] = strFileFilterRegex;
            jnTask["case_sensitive"] = hFileIter->IsCaseSensitive();
            jnTask["include_sub_dir"] = hFileIter->IsRecursive();
        }
        jnTask["clip_start_offset"] = json::number(m_hVclip->StartOffset());
        jnTask["clip_length"] = json::number(m_hVclip->Duration());
        jnTask["is_vidstab_detect_done"] = m_bVidstabDetectFinished;
        jnTask["is_vidstab_transform_done"] = m_bVidstabTransformFinished;
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

protected:
    bool _TaskProc () override
    {
        m_pLogger->Log(INFO) << "Start background task 'Vidstab' for '" << m_strSrcUrl << "'." << endl;
        if (!m_bInited)
        {
            ostringstream oss; oss << "Background task 'Vidstab' with name '" << m_name << "' is NOT initialized!";
            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
            return false;
        }

        m_fProgress = 0.f;
        float fStageProgress = 0.f, fStageShare = 0.5f, fAccumShares = 0.f;
        bool bFilterGraphInited = false;
        int64_t i64FrmIdx = 0;
        const int64_t i64ClipDur = m_hVclip->Duration();
        ImMatToAVFrameConverter tMat2AvfrmCvter;
        tMat2AvfrmCvter.SetOutPixelFormat(m_eFgInputPixfmt);
        const auto tFrameRate = m_hSettings->VideoOutFrameRate();
        if (!m_bVidstabDetectFinished)
        {
            SelfFreeAVFramePtr hFgOutfrmPtr = AllocSelfFreeAVFramePtr();
            while (!IsCancelled())
            {
                int fferr;
                SelfFreeAVFramePtr hFgInfrmPtr;
                const int64_t i64ReadPos = round((double)i64FrmIdx*1000*tFrameRate.den/tFrameRate.num);
                bool bEof = false;
                auto hVfrm = m_hVclip->ReadSourceFrame(i64ReadPos, bEof, true);
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
                        if (!SetupVidstabDetectFilterGraph(hFgInfrmPtr.get()))
                        {
                            m_pLogger->Log(Error) << "'SetupVidstabDetectFilterGraph()' FAILED!" << endl;
                            return false;
                        }
                        bFilterGraphInited = true;
                    }

                    fferr = av_buffersrc_add_frame(m_pBufsrcCtx, hFgInfrmPtr.get());
                    if (fferr < 0)
                    {
                        ostringstream oss; oss << "Background task 'Vidstab-detect' FAILED when invoking 'av_buffersrc_add_frame()' at frame #" << (i64FrmIdx-1)
                                << ". fferr=" << fferr << ".";
                        m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                        return false;
                    }
                    av_frame_unref(hFgOutfrmPtr.get());
                    fferr = av_buffersink_get_frame(m_pBufsinkCtx, hFgOutfrmPtr.get());
                    if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr < 0)
                        {
                            ostringstream oss; oss << "Background task 'Vidstab-detect' FAILED when invoking 'av_buff5ersink_get_frame()' at frame #" << (i64FrmIdx-1)
                                    << ". fferr=" << fferr << ".";
                            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                            return false;
                        }
                    }
                    i64FrmIdx++;
                }
                float fStageProgress = (float)((double)i64ReadPos/i64ClipDur);
                m_fProgress = fAccumShares+(fStageProgress*fStageShare);
                if (bEof)
                    break;
            }
            m_bVidstabDetectFinished = true;
            ReleaseFilterGraph();
        }
        fAccumShares += fStageShare;
        m_fProgress = fAccumShares;

        fStageProgress = 0.f; fStageShare = 0.5f;
        i64FrmIdx = 0;
        m_hVclip->SeekTo(0);
        bFilterGraphInited = false;
        bool bEncoderInited = false;
        if (!m_bVidstabTransformFinished)
        {
            SelfFreeAVFramePtr hFgOutfrmPtr = AllocSelfFreeAVFramePtr();
            while (!IsCancelled())
            {
                int fferr;
                SelfFreeAVFramePtr hFgInfrmPtr;
                const int64_t i64ReadPos = round((double)i64FrmIdx*1000*tFrameRate.den/tFrameRate.num);
                bool bEof = false;
                auto hVfrm = m_hVclip->ReadSourceFrame(i64ReadPos, bEof, true);
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
                    hFgInfrmPtr->pict_type = AV_PICTURE_TYPE_NONE;
                    if (!bFilterGraphInited)
                    {
                        if (!SetupVidstabTransformFilterGraph(hFgInfrmPtr.get()))
                        {
                            m_pLogger->Log(Error) << "'SetupVidstabTransformFilterGraph()' FAILED! Error is '" << m_errMsg << "'." << endl;
                            return false;
                        }
                        bFilterGraphInited = true;
                    }

                    fferr = av_buffersrc_add_frame(m_pBufsrcCtx, hFgInfrmPtr.get());
                    if (fferr < 0)
                    {
                        ostringstream oss; oss << "Background task 'Vidstab-transform' FAILED when invoking 'av_buffersrc_add_frame()' at frame #" << (i64FrmIdx-1)
                                << ". fferr=" << fferr << ".";
                        m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                        return false;
                    }
                    av_frame_unref(hFgOutfrmPtr.get());
                    fferr = av_buffersink_get_frame(m_pBufsinkCtx, hFgOutfrmPtr.get());
                    if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr < 0)
                        {
                            ostringstream oss; oss << "Background task 'Vidstab-transform' FAILED when invoking 'av_buff5ersink_get_frame()' at frame #" << (i64FrmIdx-1)
                                    << ". fferr=" << fferr << ".";
                            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                            return false;
                        }
                        if (!bEncoderInited)
                        {
                            // setup MediaEncoder
                            if (!SetupEncoder(hFgOutfrmPtr.get()))
                            {
                                m_pLogger->Log(Error) << "'SetupEncoder()' FAILED!" << endl;
                                return false;
                            }
                            bEncoderInited = true;
                        }
                        auto hVfrm = FFUtils::CreateVideoFrameFromAVFrame(CloneSelfFreeAVFramePtr(hFgOutfrmPtr.get()), i64ReadPos);
                        if (!m_hEncoder->EncodeVideoFrame(hVfrm))
                        {
                            ostringstream oss; oss << "Background task 'Vidstab-transform' FAILED to encode video frame! pos=" << i64ReadPos;
                            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                            return false;
                        }
                    }
                    i64FrmIdx++;
                }
                float fStageProgress = (float)((double)i64ReadPos/i64ClipDur);
                m_fProgress = fAccumShares+(fStageProgress*fStageShare);
                if (bEof)
                    break;
            }
            if (bEncoderInited)
            {
                if (!m_hEncoder->FinishEncoding())
                {
                    ostringstream oss; oss << "FAILED to 'Finish' MediaEncoder! Error is '" << m_hEncoder->GetError() << "'.";
                    m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                    return false;
                }
                m_hEncoder->Close();
            }
            m_bVidstabTransformFinished = true;
        }
        fAccumShares += fStageShare;
        m_fProgress = fAccumShares;

        m_pLogger->Log(INFO) << "Quit background task 'Vidstab' for '" << m_strSrcUrl << "'." << endl;
        return true;
    }

    bool _AfterTaskProc() override
    {
        ReleaseFilterGraph();
        ReleaseEncoder();
        return true;
    }

private:
    bool SetupVidstabDetectFilterGraph(const AVFrame* pInAvfrm)
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
        m_filterOutputs = filtInOutPtr;

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
        m_filterInputs = filtInOutPtr;

        const int iOutW = (int)m_hSettings->VideoOutWidth();
        const int iOutH = (int)m_hSettings->VideoOutHeight();
        oss.str("");
        if (pInAvfrm->width != iOutW || pInAvfrm->height != iOutH)
        {
            string strInterpAlgo = iOutW*iOutH >= pInAvfrm->width*pInAvfrm->height ? "bicubic" : "area";
            oss << "scale=w=" << iOutW << ":h=" << iOutH << ":flags=" << strInterpAlgo << ",";
        }
        if ((AVPixelFormat)pInAvfrm->format != AV_PIX_FMT_YUV420P)
        {
            oss << "format=yuv420p,";
        }
        oss << "vidstabdetect=result=" << m_strTrfPath << ":shakiness=" << (int)m_u8Shakiness << ":accuracy=" << (int)m_u8Accuracy << ":stepsize=" << (int)m_u16StepSize
                << ":mincontrast=" << m_fMinContrast;
        string filterArgs = oss.str();
        fferr = avfilter_graph_parse_ptr(m_pFilterGraph, filterArgs.c_str(), &m_filterInputs, &m_filterOutputs, nullptr);
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

        if (m_filterOutputs)
            avfilter_inout_free(&m_filterOutputs);
        if (m_filterInputs)
            avfilter_inout_free(&m_filterInputs);
        return true;
    }

    bool SetupVidstabTransformFilterGraph(const AVFrame* pInAvfrm)
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
        m_filterOutputs = filtInOutPtr;

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
        m_filterInputs = filtInOutPtr;

        const int iOutW = (int)m_hSettings->VideoOutWidth();
        const int iOutH = (int)m_hSettings->VideoOutHeight();
        oss.str("");
        if (pInAvfrm->width != iOutW || pInAvfrm->height != iOutH)
        {
            string strInterpAlgo = iOutW*iOutH >= pInAvfrm->width*pInAvfrm->height ? "bicubic" : "area";
            oss << "scale=w=" << iOutW << ":h=" << iOutH << ":flags=" << strInterpAlgo << ",";
        }
        // avoid pixel format change
        // if ((AVPixelFormat)pInAvfrm->format != AV_PIX_FMT_YUV420P)
        // {
        //     oss << "format=yuv420p,";
        // }
        string strOptAlgo = "gauss";
        if (m_u8OptAlgo == 1) strOptAlgo = "avg";
        string strCropMode = "keep";
        if (m_u8CropMode == 1) strCropMode = "black";
        string strInterpMode = "bilinear";
        if (m_u8InterpMode == 0) strInterpMode = "no";
        else if (m_u8InterpMode == 1) strInterpMode = "linear";
        else if (m_u8InterpMode == 3) strInterpMode = "bicubic";
        oss << "vidstabtransform=input=" << m_strTrfPath << ":smoothing=" << m_u32Smoothing << ":optalgo=" << strOptAlgo << ":maxshift=" << m_i32MaxShift
                << ":maxangle=" << m_fMaxAngle << ":crop=" << strCropMode << ":invert=" << (m_bInvertTrans?1:0) << ":relative=" << (m_bRelative?1:0)
                << ":zoom=" << m_fPresetZoom << ":optzoom=" << (int)m_u8OptZoom << ":zoomspeed=" << m_fZoomSpeed << ":interpol=" << strInterpMode;
        string filterArgs = oss.str();
        fferr = avfilter_graph_parse_ptr(m_pFilterGraph, filterArgs.c_str(), &m_filterInputs, &m_filterOutputs, nullptr);
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

        if (m_filterOutputs)
            avfilter_inout_free(&m_filterOutputs);
        if (m_filterInputs)
            avfilter_inout_free(&m_filterInputs);
        return true;
    }

    void ReleaseFilterGraph()
    {
        if (m_filterOutputs)
        {
            avfilter_inout_free(&m_filterOutputs);
            m_filterOutputs = nullptr;
        }
        if (m_filterInputs)
        {
            avfilter_inout_free(&m_filterInputs);
            m_filterInputs = nullptr;
        }
        m_pBufsrcCtx = nullptr;
        m_pBufsinkCtx = nullptr;
        if (m_pFilterGraph)
        {
            avfilter_graph_free(&m_pFilterGraph);
            m_pFilterGraph = nullptr;
        }
    }

    bool SetupEncoder(const AVFrame* pInAvfrm)
    {
        auto hEncoder = MediaCore::MediaEncoder::CreateInstance();
        if (!hEncoder->Open(m_strOutputPath))
        {
            ostringstream oss; oss << "FAILED to open MediaEncoder at location '" << m_strOutputPath << "'! Error is '" << hEncoder->GetError() << "'.";
            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
            return false;
        }
        auto strInputPixfmt = string(av_get_pix_fmt_name((AVPixelFormat)pInAvfrm->format));
        vector<MediaCore::MediaEncoder::Option> aExtraOpts = {
            { "profile",                MediaCore::Value("high") },
            { "aspect",                 MediaCore::Value(MediaCore::Ratio(1,1)) },
            { "colorspace",             MediaCore::Value(1) },
            { "color_trc",              MediaCore::Value(1) },
            { "color_primaries",        MediaCore::Value(1) },
        };
        if (!hEncoder->ConfigureVideoStream("h264", strInputPixfmt, pInAvfrm->width, pInAvfrm->height, m_hSettings->VideoOutFrameRate(), 20*1000*1000, &aExtraOpts))
        {
            ostringstream oss; oss << "FAILED to configure MediaEncoder VIDEO stream! Error is '" << hEncoder->GetError() << "'.";
            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
            return false;
        }
        if (!hEncoder->Start())
        {
            ostringstream oss; oss << "FAILED to 'Start' MediaEncoder! Error is '" << hEncoder->GetError() << "'.";
            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
            return false;
        }
        m_hEncoder = hEncoder;
        return true;
    }

    void ReleaseEncoder()
    {
        if (m_hEncoder)
        {
            if (!m_hEncoder->Close())
                m_pLogger->Log(Error) << "In bg-task '" << m_name << "', FAILED to close the encoder! Error is '" << m_hEncoder->GetError() << "'." << endl;
            m_hEncoder = nullptr;
        }
    }

private:
    string m_name;
    size_t m_szHash;
    string m_errMsg;
    ALogger* m_pLogger;
    bool m_bInited{false};
    string m_strTaskDir;
    AVFilterGraph* m_pFilterGraph{nullptr};
    AVFilterContext* m_pBufsrcCtx;
    AVFilterContext* m_pBufsinkCtx;
    AVFilterInOut* m_filterOutputs{nullptr};
    AVFilterInOut* m_filterInputs{nullptr};
    AVPixelFormat m_eFgInputPixfmt;
    string m_strTrfPath;
    string m_strSrcUrl;
    int64_t m_i64ClipId;
    MediaCore::VideoClip::Holder m_hVclip;
    const MediaCore::VideoStream* m_pVidstm{nullptr};
    MediaCore::SharedSettings::Holder m_hSettings;
    bool m_bIsImageSeq{false};
    bool m_bUseSrcAttr;
    // vidstab detect parameters
    uint8_t m_u8Shakiness{7};       // 1-10, a value of 1 means little shakiness, a value of 10 means strong shakiness.
    uint8_t m_u8Accuracy{10};       // 1-15. A value of 1 means low accuracy, a value of 15 means high accuracy.
    uint16_t m_u16StepSize{12};      // The region around minimum is scanned with 1 pixel resolution.
    float m_fMinContrast{0.1f};     // 0-1, below this value a local measurement field is discarded.
    bool m_bVidstabDetectFinished{false};
    // vidstab transform parameters
    uint32_t m_u32Smoothing{20};    // (value*2+1) frames are used for lowpass filtering the camera movements.
    uint8_t m_u8OptAlgo{0};         // 0: gauss, 1: avg. This is the camera path optimization algorithm.
    int32_t m_i32MaxShift{-1};      // Maximum limit for the pixel pan distance, -1 means no limit.
    float m_fMaxAngle{-1};          // Maximum limit for the camera rotation angle in radians(degree*PI/180), -1 means no limit.
    uint8_t m_u8CropMode{1};        // 0: keep, 1: black. Specify how to deal with borders that may be visible due to movement compensation.
    bool m_bInvertTrans{false};     // Invert transforms if set to 'true'.
    bool m_bRelative{true};        // Consider transforms as relative to previous frame if set to 'true'.
    float m_fPresetZoom{0.f};       // Set percentage to zoom. A positive value will result in a zoom-in effect, a negative value in a zoom-out effect.
    uint8_t m_u8OptZoom{1};         // Set optimal zooming to avoid borders. 0: disabled, 1: optimal static zoom value, 2: optimal adaptive zoom value.
    float m_fZoomSpeed{0.25f};      // Set percent to zoom maximally each frame (enabled when optzoom is set to 2). Range is from 0 to 5.
    uint8_t m_u8InterpMode{2};      // 0: nearest, 1: linear, 2: bilinear, 3: bicubic.
    bool m_bVidstabTransformFinished{false};
    // output settings
    MediaCore::MediaEncoder::Holder m_hEncoder;
    string m_strOutputPath;
    float m_fProgress{0.f};
};

static const auto _BGTASK_VIDSTAB_DELETER = [] (BackgroundTask* p) {
    BgtaskVidstab* ptr = dynamic_cast<BgtaskVidstab*>(p);
    delete ptr;
};

BackgroundTask::Holder CreateBgtask_Vidstab(const json::value& jnTask, MediaCore::SharedSettings::Holder hSettings)
{
    string strTaskName;
    string strAttrName = "name";
    if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_string())
        strTaskName = jnTask["name"].get<json::string>();
    else
        strTaskName = "BgtskVidstab";
    auto p = new BgtaskVidstab(strTaskName);
    if (!p->Initialize(jnTask, hSettings))
    {
        Log(Error) << "FAILED to create new 'Vidstab' background task! Error is '" << p->GetError() << "'." << endl;
        delete p; 
        return nullptr;
    }
    p->Save("");
    return BackgroundTask::Holder(p, _BGTASK_VIDSTAB_DELETER);
}
}