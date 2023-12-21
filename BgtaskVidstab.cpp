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
        if (m_filterOutputs)
            avfilter_inout_free(&m_filterOutputs);
        if (m_filterInputs)
            avfilter_inout_free(&m_filterInputs);
        m_pBufsrcCtx = nullptr;
        m_pBufsinkCtx = nullptr;
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
        strAttrName = "is_task_failed";
        if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_boolean())
            m_bFailed = jnTask[strAttrName].get<json::boolean>();
        else
            m_bFailed = false;
        if (m_bFailed)
        {
            strAttrName = "error_message";
            if (jnTask.contains(strAttrName) && jnTask[strAttrName].is_string())
                m_errMsg = jnTask[strAttrName].get<json::string>();
            SetState(DONE);
        }

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
            if (m_bCancel)
                ImGui::TextColored(ImColor(0.8f, 0.8f, 0.8f), "Cancelled");
            else if (m_bFailed)
                ImGui::TextColored(ImColor(0.85f, 0.3f, 0.3f), "FAILED");
            else
                ImGui::TextColored(ImColor(0.3f, 0.85f, 0.3f), "Done");
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
        jnTask["is_task_failed"] = m_bFailed;
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
    void _TaskProc () override
    {
        m_pLogger->Log(INFO) << "Start background task 'Vidstab' for '" << m_strSrcUrl << "'." << endl;
        if (!m_bInited)
        {
            ostringstream oss; oss << "Background task 'Vidstab' with name '" << m_name << "' is NOT initialized!";
            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
            m_bFailed = true;
            return;
        }

        ImMatToAVFrameConverter tMat2AvfrmCvter;
        tMat2AvfrmCvter.SetOutPixelFormat(m_eFgInputPixfmt);
        const auto tFrameRate = m_hSettings->VideoOutFrameRate();
        if (!m_bVidstabDetectFinished)
        {
            int64_t i64FrmIdx = 0;
            const int64_t i64ClipDur = m_hVclip->Duration();
            SelfFreeAVFramePtr hFgOutfrmPtr = AllocSelfFreeAVFramePtr();
            float fStageProgress = 0.f, fStageShare = 0.5f, fAccumShares = 0.f;
            m_fProgress = 0.f;
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
                    if (tNativeData.eType == MediaCore::MediaReader::VideoFrame::NativeData::AVFRAME)
                        hFgInfrmPtr = CloneSelfFreeAVFramePtr((AVFrame*)tNativeData.pData);
                    else if (tNativeData.eType == MediaCore::MediaReader::VideoFrame::NativeData::AVFRAME_HOLDER)
                        hFgInfrmPtr = *((SelfFreeAVFramePtr*)tNativeData.pData);
                    else if (tNativeData.eType == MediaCore::MediaReader::VideoFrame::NativeData::MAT)
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
                    if (m_bFirstRun)
                    {
                        if (!SetupVidstabDetectFilterGraph(hFgInfrmPtr.get()))
                        {
                            m_pLogger->Log(Error) << "'SetupVidstabDetectFilterGraph()' FAILED!" << endl;
                            m_bFailed = true;
                            return;
                        }
                        m_bFirstRun = false;
                    }

                    fferr = av_buffersrc_add_frame(m_pBufsrcCtx, hFgInfrmPtr.get());
                    if (fferr < 0)
                    {
                        ostringstream oss; oss << "Background task 'Vidstab' FAILED when invoking 'av_buffersrc_add_frame()' at frame #" << (i64FrmIdx-1)
                                << ". fferr=" << fferr << ".";
                        m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                        m_bFailed = true;
                        return;
                    }
                    av_frame_unref(hFgOutfrmPtr.get());
                    fferr = av_buffersink_get_frame(m_pBufsinkCtx, hFgOutfrmPtr.get());
                    if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr < 0)
                        {
                            ostringstream oss; oss << "Background task 'Vidstab' FAILED when invoking 'av_buff5ersink_get_frame()' at frame #" << (i64FrmIdx-1)
                                    << ". fferr=" << fferr << ".";
                            m_errMsg = oss.str(); m_pLogger->Log(Error) << m_errMsg << endl;
                            m_bFailed = true;
                            return;
                        }
                    }
                }
                i64FrmIdx++;
                float fStageProgress = (float)((double)i64ReadPos/i64ClipDur);
                m_fProgress = fAccumShares+(fStageProgress*fStageShare);
                if (bEof)
                    break;
            }
        }
        m_pLogger->Log(INFO) << "Quit background task 'Vidstab' for '" << m_strSrcUrl << "'." << endl;
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
            oss << "FAILED to invoke 'avfilter_graph_parse_ptr'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

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
    bool m_bFirstRun{true};
    uint8_t m_u8Shakiness{5};       // 1-10, a value of 1 means little shakiness, a value of 10 means strong shakiness.
    uint8_t m_u8Accuracy{15};       // 1-15. A value of 1 means low accuracy, a value of 15 means high accuracy.
    uint16_t m_u16StepSize{6};      // The region around minimum is scanned with 1 pixel resolution.
    float m_fMinContrast{0.3f};     // 0-1, below this value a local measurement field is discarded.
    bool m_bVidstabDetectFinished{false};
    MediaCore::MediaEncoder::Holder m_hEncoder;
    float m_fProgress{0.f};
    bool m_bFailed{false};
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