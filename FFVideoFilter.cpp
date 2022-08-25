#include <sstream>
#include <mutex>
#include <algorithm>
#include "FFVideoFilter.h"
#include "FFUtils.h"
#include "Logger.h"
extern "C"
{
    #include "libavformat/avformat.h"
    #include "libavutil/pixdesc.h"
    #include "libavutil/frame.h"
    #include "libavutil/opt.h"
    #include "libswscale/swscale.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
}

using namespace std;
using namespace Logger;

namespace DataLayer
{
    const std::string FFTransformVideoFilter::FILTER_NAME = "FFTransformVideoFilter";

    class FFTransformVideoFilter_Impl : public FFTransformVideoFilter
    {
    public:
        FFTransformVideoFilter_Impl() {}

        virtual ~FFTransformVideoFilter_Impl()
        {
            if (m_cropFg)
                avfilter_graph_free(&m_cropFg);
            if (m_scaleFg)
                avfilter_graph_free(&m_scaleFg);
            if (m_rotateFg)
                avfilter_graph_free(&m_rotateFg);
            if (m_overlayFg)
                avfilter_graph_free(&m_overlayFg);
            if (m_ovlyBaseImg)
                av_frame_free(&m_ovlyBaseImg);
        }

        const std::string GetFilterName() const override
        {
            return FILTER_NAME;
        }

        bool Initialize(uint32_t outWidth, uint32_t outHeight, const std::string& outputFormat) override
        {
            if (outWidth == 0 || outHeight == 0)
            {
                m_errMsg = "INVALID argument! 'outWidth' and 'outHeight' must be positive value.";
                return false;
            }
            m_outWidth = outWidth;
            m_outHeight = outHeight;
            string fmtLowerCase(outputFormat);
            transform(fmtLowerCase.begin(), fmtLowerCase.end(), fmtLowerCase.begin(), [] (char c) {
                if (c <= 'Z' && c >= 'A')
                    return (char)(c-('Z'-'z'));
                return c;
            });
            AVPixelFormat outputPixfmt = av_get_pix_fmt(fmtLowerCase.c_str());
            if (outputPixfmt == AV_PIX_FMT_NONE)
            {
                ostringstream oss;
                oss << "CANNOT find corresponding 'AVPixelFormat' for argument '" << outputFormat << "'!";
                m_errMsg = oss.str();
                return false;
            }
            ImColorFormat imclrfmt = ConvertPixelFormatToColorFormat(outputPixfmt);
            if ((int)imclrfmt < 0)
            {
                ostringstream oss;
                oss << "CANNOT find corresponding 'ImColorFormat' for argument '" << outputFormat << "'!";
                m_errMsg = oss.str();
                return false;
            }
            m_unifiedOutputPixfmt = outputPixfmt;

            m_mat2frmCvt.SetOutPixelFormat(m_unifiedInputPixfmt);
            m_frm2matCvt.SetOutColorFormat(imclrfmt);

            if (m_ovlyBaseImg)
                av_frame_unref(m_ovlyBaseImg);
            else
                m_ovlyBaseImg = av_frame_alloc();
            if (!m_ovlyBaseImg)
            {
                m_errMsg = "FAILED to allocate new 'AVFrame' instance!";
                return false;
            }
            m_ovlyBaseImg->width = outWidth;
            m_ovlyBaseImg->height = outHeight;
            m_ovlyBaseImg->format = (int)outputPixfmt;
            int fferr = av_frame_get_buffer(m_ovlyBaseImg, 0);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'av_frame_get_buffer()'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }
            int maxBufCnt = sizeof(m_ovlyBaseImg->buf)/sizeof(m_ovlyBaseImg->buf[0]);
            for (int i = 0; i < maxBufCnt; i++)
            {
                AVBufferRef* bufref = m_ovlyBaseImg->buf[i];
                if (!bufref || bufref->size <= 0)
                    continue;
                memset(bufref->data, 0, bufref->size);
            }
            return true;
        }

        bool SetScaleType(ScaleType type) override
        {
            lock_guard<mutex> lk(m_processLock);
            if (type < SCALE_TYPE__FIT || type > SCALE_TYPE__STRETCH)
            {
                m_errMsg = "INVALID argument 'type'!";
                return false;
            }
            m_scaleType = type;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetPositionOffset(int32_t offsetH, int32_t offsetV) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_posOffsetH = offsetH;
            m_posOffsetV = offsetV;
            return true;
        }

        bool SetCropMargin(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_cropL = left;
            m_cropT = top;
            m_cropR = right;
            m_cropB = bottom;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetRotation(double angle) override
        {
            lock_guard<mutex> lk(m_processLock);
            int32_t n = (int32_t)trunc(angle/360);
            m_rotateAngle = angle-n*360;
            m_needUpdateRotateParam = true;
            return true;
        }

        bool SetScaleH(double scale) override
        {
            lock_guard<mutex> lk(m_processLock);
            if (m_scaleRatioH != scale)
            {
                m_scaleRatioH = scale;
                m_needUpdateScaleParam = true;
            }
            return true;
        }

        bool SetScaleV(double scale) override
        {
            lock_guard<mutex> lk(m_processLock);
            if (m_scaleRatioV != scale)
            {
                m_scaleRatioV = scale;
                m_needUpdateScaleParam = true;
            }
            return true;
        }

        uint32_t GetInWidth() const override
        {
            return m_inWidth;
        }

        uint32_t GetInHeight() const override
        {
            return m_inHeight;
        }

        uint32_t GetOutWidth() const override
        {
            return m_outWidth;
        }

        uint32_t GetOutHeight() const override
        {
            return m_outHeight;
        }

        string GetOutputPixelFormat() const override
        {
            return string(av_get_pix_fmt_name(m_unifiedOutputPixfmt));
        }

        ScaleType GetScaleType() const override
        {
            return m_scaleType;
        }

        int32_t GetPositionOffsetH() const override
        {
            return m_posOffsetH;
        }

        int32_t GetPositionOffsetV() const override
        {
            return m_posOffsetV;
        }

        uint32_t GetCropMarginL() const override
        {
            return m_cropL;
        }

        uint32_t GetCropMarginT() const override
        {
            return m_cropT;
        }

        uint32_t GetCropMarginR() const override
        {
            return m_cropR;
        }

        uint32_t GetCropMarginB() const override
        {
            return m_cropB;
        }

        double GetRotationAngle() const override
        {
            return m_rotateAngle;
        }

        double GetScaleH() const override
        {
            return m_scaleRatioH;
        }

        double GetScaleV() const override
        {
            return m_scaleRatioV;
        }

        void ApplyTo(VideoClip* clip) override
        {}

        ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override
        {
            ImGui::ImMat res;
            if (!FilterImage_Internal(vmat, res, pos))
            {
                res.release();
                Log(Error) << "FilterImage() FAILED! " << m_errMsg << endl;
            }
            return res;
        }

        string GetError() const override
        {
            return m_errMsg;
        }

    private:
        AVFilterGraph* CreateFilterGraph(const string& filterArgs, uint32_t w, uint32_t h, AVPixelFormat inputPixfmt, AVFilterContext** inputCtx, AVFilterContext** outputCtx)
        {
            AVFilterGraph* avfg = avfilter_graph_alloc();
            if (!avfg)
            {
                m_errMsg = "FAILED to allocate new 'AVFilterGraph' instance!";
                return nullptr;
            }
            const AVFilter* avfilter;
            avfilter = avfilter_get_by_name("buffer");
            if (!avfilter)
            {
                m_errMsg = "FAILED to find filter 'buffer'!";
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            ostringstream bufsrcArgsOss;
            bufsrcArgsOss << w << ":" << h << ":pix_fmt=" << (int)inputPixfmt << ":time_base=1/" << AV_TIME_BASE
                << ":pixel_aspect=1/1:frame_rate=" << m_inputFrameRate.num << "/" << m_inputFrameRate.den;
            string bufsrcArgs = bufsrcArgsOss.str();
            int fferr;
            AVFilterContext* inFilterCtx = nullptr;
            fferr = avfilter_graph_create_filter(&inFilterCtx, avfilter, "inputBuffer", bufsrcArgs.c_str(), nullptr, avfg);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to create 'buffer' filter instance with arguments '" << bufsrcArgs << "'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            avfilter = avfilter_get_by_name("buffersink");
            if (!avfilter)
            {
                m_errMsg = "FAILED to find filter 'buffersink'!";
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            AVFilterContext* outFilterCtx = nullptr;
            fferr = avfilter_graph_create_filter(&outFilterCtx, avfilter, "outputBufferSink", nullptr, nullptr, avfg);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to create 'buffersink' filter instance! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            AVFilterInOut* outputs = avfilter_inout_alloc();
            if (!outputs)
            {
                m_errMsg = "FAILED to allocate new 'AVFilterInOut' instance for 'outputs'!";
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            outputs->name = av_strdup("in");
            outputs->filter_ctx = inFilterCtx;
            outputs->pad_idx = 0;
            outputs->next = nullptr;
            AVFilterInOut* inputs = avfilter_inout_alloc();
            if (!inputs)
            {
                m_errMsg = "FAILED to allocate new 'AVFilterInOut' instance for 'inputs'!";
                avfilter_inout_free(&outputs);
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            inputs->name = av_strdup("out");
            inputs->filter_ctx = outFilterCtx;
            inputs->pad_idx = 0;
            inputs->next = nullptr;

            fferr = avfilter_graph_parse_ptr(avfg, filterArgs.c_str(), &inputs, &outputs, nullptr);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_parse_ptr()' with arguments '" << filterArgs << "'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_inout_free(&inputs);
                avfilter_inout_free(&outputs);
                avfilter_graph_free(&avfg);
                return nullptr;
            }

            fferr = avfilter_graph_config(avfg, nullptr);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_config()' with arguments '" << filterArgs << "'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_inout_free(&inputs);
                avfilter_inout_free(&outputs);
                avfilter_graph_free(&avfg);
                return nullptr;
            }

            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            *inputCtx = inFilterCtx;
            *outputCtx = outFilterCtx;
            return avfg;
        }

        AVFilterGraph* CreateOverlayFilterGraph(uint32_t w, uint32_t h, AVPixelFormat inputPixfmt, int32_t x, int32_t y,
                                                AVFilterContext** input0Ctx, AVFilterContext** input1Ctx, AVFilterContext** outputCtx)
        {
            AVFilterGraph* avfg = avfilter_graph_alloc();
            if (!avfg)
            {
                m_errMsg = "FAILED to allocate new 'AVFilterGraph' instance!";
                return nullptr;
            }

            const AVFilter* avfilter;
            avfilter = avfilter_get_by_name("buffer");
            if (!avfilter)
            {
                m_errMsg = "FAILED to find filter 'buffer'!";
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            int fferr;
            ostringstream bufsrcArgsOss;
            bufsrcArgsOss << w << ":" << h << ":pix_fmt=" << (int)inputPixfmt << ":time_base=1/" << AV_TIME_BASE
                << ":pixel_aspect=1/1:frame_rate=" << m_inputFrameRate.num << "/" << m_inputFrameRate.den;
            string bufsrcArg = bufsrcArgsOss.str(); bufsrcArgsOss.str("");
            AVFilterContext* inFilterCtx0 = nullptr;
            string filterName = "base";
            fferr = avfilter_graph_create_filter(&inFilterCtx0, avfilter, filterName.c_str(), bufsrcArg.c_str(), nullptr, avfg);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED when invoking 'avfilter_graph_create_filter()' for INPUT0 '" << filterName << "'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            AVFilterInOut* outputs0 = avfilter_inout_alloc();
            if (!outputs0)
            {
                m_errMsg = "FAILED to allocate new 'AVFilterInOut' instance!";
                avfilter_graph_free(&avfg);
                return nullptr;
            }
            outputs0->name = av_strdup(filterName.c_str());
            outputs0->filter_ctx = inFilterCtx0;
            outputs0->pad_idx = 0;
            outputs0->next = nullptr;

            AVFilterContext* inFilterCtx1 = nullptr;
            filterName = "overlay";
            fferr = avfilter_graph_create_filter(&inFilterCtx1, avfilter, filterName.c_str(), bufsrcArg.c_str(), nullptr, avfg);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED when invoking 'avfilter_graph_create_filter()' for INPUT1 '" << filterName << "'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0);
                return nullptr;
            }
            AVFilterInOut* outputs1 = avfilter_inout_alloc();
            if (!outputs1)
            {
                m_errMsg = "FAILED to allocate new 'AVFilterInOut' instance!";
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0);
                return nullptr;
            }
            outputs1->name = av_strdup(filterName.c_str());
            outputs1->filter_ctx = inFilterCtx1;
            outputs1->pad_idx = 0;
            outputs1->next = nullptr;
            outputs0->next = outputs1;

            avfilter = avfilter_get_by_name("buffersink");
            if (!avfilter)
            {
                m_errMsg = "FAILED to find filter 'buffersink'!";
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0);
                return nullptr;
            }

            AVFilterContext* outFilterCtx = nullptr;
            filterName = "out";
            fferr = avfilter_graph_create_filter(&outFilterCtx, avfilter, filterName.c_str(), nullptr, nullptr, avfg);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED when invoking 'avfilter_graph_create_filter()' for OUTPUT 'out'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0);
                return nullptr;
            }
            const AVPixelFormat out_pix_fmts[] = { m_unifiedOutputPixfmt, (AVPixelFormat)-1 };
            fferr = av_opt_set_int_list(outFilterCtx, "pix_fmts", out_pix_fmts, -1, AV_OPT_SEARCH_CHILDREN);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED when invoking 'av_opt_set_int_list()' for OUTPUTS! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0);
                return nullptr;
            }
            AVFilterInOut* inputs = avfilter_inout_alloc();
            if (!inputs)
            {
                m_errMsg = "FAILED to allocate new 'AVFilterInOut' instance!";
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0);
                return nullptr;
            }
            inputs->name = av_strdup(filterName.c_str());
            inputs->filter_ctx = outFilterCtx;
            inputs->pad_idx = 0;
            inputs->next = nullptr;

            ostringstream argsOss;
            argsOss << "[base][overlay] overlay=x=" << x << ":y=" << y << ":format=auto:eof_action=pass:eval=frame";
            string filterArgs = argsOss.str();
            fferr = avfilter_graph_parse_ptr(avfg, filterArgs.c_str(), &inputs, &outputs0, nullptr);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_parse_ptr()' with arguments '" << filterArgs << "'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0); avfilter_inout_free(&inputs);
                return nullptr;
            }

            fferr = avfilter_graph_config(avfg, nullptr);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_config()'! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                avfilter_graph_free(&avfg); avfilter_inout_free(&outputs0); avfilter_inout_free(&inputs);
                // Log(Error) << m_errMsg << endl;
                return nullptr;
            }

            avfilter_inout_free(&outputs0);
            avfilter_inout_free(&inputs);
            *input0Ctx = inFilterCtx0;
            *input1Ctx = inFilterCtx1;
            *outputCtx = outFilterCtx;
            return avfg;
        }

        bool FilterImage_Internal(const ImGui::ImMat& inMat, ImGui::ImMat& outMat, int64_t pos)
        {
            lock_guard<mutex> lk(m_processLock);
            m_inWidth = inMat.w;
            m_inHeight = inMat.h;

            int64_t pts = (int64_t)(m_inputCount++)*AV_TIME_BASE*m_inputFrameRate.den/m_inputFrameRate.num;
            // ImMat => AVFrame
            SelfFreeAVFramePtr avfrmPtr = AllocSelfFreeAVFramePtr();
            if (!m_mat2frmCvt.ConvertImage(inMat, avfrmPtr.get(), pts))
            {
                ostringstream oss;
                oss << "FAILED to convert 'ImMat' to 'AVFrame'! Error message is '" << m_mat2frmCvt.GetError() << "'.";
                m_errMsg = oss.str();
                return false;
            }

            const uint32_t inputWidth = avfrmPtr->width;
            const uint32_t inputHeight = avfrmPtr->height;
            int fferr;
            // crop
            if (m_needUpdateCropParam)
            {
                uint32_t rectX = m_cropL<inputWidth ? m_cropL : inputWidth-1;
                uint32_t rectX1 = m_cropR<inputWidth ? inputWidth-m_cropR : 0;
                uint32_t rectW;
                if (rectX < rectX1)
                    rectW = rectX1-rectX;
                else
                {
                    rectW = rectX-rectX1;
                    rectX = rectX1;
                }
                uint32_t rectY = m_cropT<inputHeight ? m_cropT : inputHeight-1;
                uint32_t rectY1 = m_cropB<inputHeight ? inputHeight-m_cropB : 0;
                uint32_t rectH;
                if (rectY < rectY1)
                    rectH = rectY1-rectY;
                else
                {
                    rectH = rectY-rectY1;
                    rectY = rectY1;
                }
                m_cropRectX = rectX; m_cropRectY = rectY;
                m_cropRectW = rectW; m_cropRectH = rectH;
                m_needUpdateCropParam = false;
            }
            if (m_cropL != 0 || m_cropR != 0 || m_cropT != 0 || m_cropB != 0)
            {
#if 0
                if (!m_cropFg)
                {
                    ostringstream argsOss;
                    int32_t w = avfrmPtr->width-m_cropL-m_cropR;
                    int32_t h = avfrmPtr->height-m_cropT-m_cropB;
                    argsOss << "crop=w=" << w << ":h=" << h << ":x=" << m_cropL << ":y=" << m_cropT;
                    string filterArgs = argsOss.str();
                    m_cropFg = CreateFilterGraph(filterArgs, avfrmPtr->width, avfrmPtr->height, (AVPixelFormat)avfrmPtr->format, &m_cropInputCtx, &m_cropOutputCtx);
                    if (!m_cropFg)
                        return false;
                    m_needUpdateCropParam = false;
                }
                else if (m_needUpdateCropParam)
                {
                    char cmdArgs[32] = {0}, cmdRes[128] = {0};
                    int32_t w = avfrmPtr->width-m_cropL-m_cropR;
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", w);
                    fferr = avfilter_graph_send_command(m_scaleFg, "crop", "w", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'crop' on argument 'w' = " << w
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    int32_t h = avfrmPtr->height-m_cropT-m_cropB;
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", h);
                    fferr = avfilter_graph_send_command(m_scaleFg, "crop", "h", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'crop' on argument 'h' = " << h
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", m_cropL);
                    fferr = avfilter_graph_send_command(m_scaleFg, "crop", "x", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'crop' on argument 'x' = " << m_cropL
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", m_cropT);
                    fferr = avfilter_graph_send_command(m_scaleFg, "crop", "y", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'crop' on argument 'y' = " << m_cropT
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    m_needUpdateCropParam = false;
                }
                fferr = av_buffersrc_write_frame(m_cropInputCtx, avfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersrc_write_frame()' at 'crop' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                av_frame_unref(avfrmPtr.get());
                fferr = av_buffersink_get_frame(m_cropOutputCtx, avfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersink_get_frame()' at 'crop' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
#else
                SelfFreeAVFramePtr cropfrmPtr = AllocSelfFreeAVFramePtr();
                cropfrmPtr->width = inputWidth;
                cropfrmPtr->height = inputHeight;
                cropfrmPtr->format = avfrmPtr->format;
                fferr = av_frame_get_buffer(cropfrmPtr.get(), 0);
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to allocate buffer for crop output frame! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                int bufcnt = sizeof(cropfrmPtr->buf)/sizeof(cropfrmPtr->buf[0]);
                for (int i = 0; i < bufcnt; i++)
                {
                    AVBufferRef* bufref = cropfrmPtr->buf[i];
                    if (!bufref || bufref->size <= 0)
                        continue;
                    memset(bufref->data, 0, bufref->size);
                }
                if (m_cropRectW > 0 && m_cropRectH > 0)
                {
                    const uint32_t bytesPerPixel = 4;
                    const uint8_t* srcptr = avfrmPtr->data[0]+avfrmPtr->linesize[0]*m_cropRectY+m_cropRectX*bytesPerPixel;
                    uint8_t* dstptr = cropfrmPtr->data[0]+cropfrmPtr->linesize[0]*m_cropRectY+m_cropRectX*bytesPerPixel;
                    uint32_t copyBytesPerLine = m_cropRectW*bytesPerPixel;
                    for (int i = 0; i < (int)m_cropRectH; i++)
                    {
                        memcpy(dstptr, srcptr, copyBytesPerLine);
                        srcptr += avfrmPtr->linesize[0];
                        dstptr += cropfrmPtr->linesize[0];
                    }
                }
                av_frame_copy_props(cropfrmPtr.get(), avfrmPtr.get());
                avfrmPtr = cropfrmPtr;
#endif
            }

            // scale
            if (m_needUpdateScaleParam)
            {
                uint32_t scaleOutWidth{inputWidth}, scaleOutHeight{inputHeight};
                switch (m_scaleType)
                {
                    case SCALE_TYPE__FIT:
                    if (inputWidth*m_outHeight > inputHeight*m_outWidth)
                    {
                        scaleOutWidth = m_outWidth;
                        scaleOutHeight = (uint32_t)round((double)inputHeight*m_outWidth/inputWidth);
                    }
                    else
                    {
                        scaleOutHeight = m_outHeight;
                        scaleOutWidth = (uint32_t)round((double)inputWidth*m_outHeight/inputHeight);
                    }
                    break;
                    case SCALE_TYPE__CROP:
                    scaleOutWidth = inputWidth;
                    scaleOutHeight = inputHeight;
                    break;
                    case SCALE_TYPE__FILL:
                    if (inputWidth*m_outHeight > inputHeight*m_outWidth)
                    {
                        scaleOutHeight = m_outHeight;
                        scaleOutWidth = (uint32_t)round((double)inputWidth*m_outHeight/inputHeight);
                    }
                    else
                    {
                        scaleOutWidth = m_outWidth;
                        scaleOutHeight = (uint32_t)round((double)inputHeight*m_outWidth/inputWidth);
                    }
                    break;
                    case SCALE_TYPE__STRETCH:
                    scaleOutWidth = m_outWidth;
                    scaleOutHeight = m_outHeight;
                    break;
                }
                m_scaledWidthWithoutCrop = (uint32_t)round(scaleOutWidth*m_scaleRatioH);
                m_scaledHeightWithoutCrop = (uint32_t)round(scaleOutHeight*m_scaleRatioV);
                m_realScaleRatioH = (double)scaleOutWidth/inputWidth*m_scaleRatioH;
                m_realScaleRatioV = (double)scaleOutHeight/inputHeight*m_scaleRatioV;
            }
            if (m_realScaleRatioH != 1 || m_realScaleRatioV != 1)
            {
                if (!m_scaleFg)
                {
                    const uint32_t outW = (uint32_t)round(m_realScaleRatioH*avfrmPtr->width);
                    const uint32_t outH = (uint32_t)round(m_realScaleRatioV*avfrmPtr->height);
                    ostringstream argsOss;
                    argsOss << "scale=w=" << outW << ":h=" << outH << ":eval=frame:flags=bicubic";
                    string filterArgs = argsOss.str();
                    m_scaleFg = CreateFilterGraph(filterArgs, avfrmPtr->width, avfrmPtr->height, (AVPixelFormat)avfrmPtr->format, &m_scaleInputCtx, &m_scaleOutputCtx);
                    if (!m_scaleFg)
                        return false;
                    m_needUpdateScaleParam = false;
                }
                else if (m_needUpdateScaleParam)
                {
                    const uint32_t outW = (uint32_t)round(m_realScaleRatioH*avfrmPtr->width);
                    const uint32_t outH = (uint32_t)round(m_realScaleRatioV*avfrmPtr->height);
                    char cmdArgs[32] = {0}, cmdRes[128] = {0};
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", outW);
                    fferr = avfilter_graph_send_command(m_scaleFg, "scale", "w", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'scale' on argument 'w' = " << outW
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", outH);
                    fferr = avfilter_graph_send_command(m_scaleFg, "scale", "h", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'scale' on argument 'h' = " << outH
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    m_needUpdateScaleParam = false;
                }
                fferr = av_buffersrc_write_frame(m_scaleInputCtx, avfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersrc_write_frame()' at 'scale' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                av_frame_unref(avfrmPtr.get());
                fferr = av_buffersink_get_frame(m_scaleOutputCtx, avfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersink_get_frame()' at 'scale' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
            }

            // rotate
            if (m_rotateAngle != 0)
            {
                if (!m_rotateFg)
                {
                    const uint32_t rotw = max(m_scaledWidthWithoutCrop, m_scaledHeightWithoutCrop);
                    const uint32_t roth = rotw;
                    ostringstream argsOss;
                    argsOss << "rotate=a=" << m_rotateAngle*M_PI/180 << ":ow=" << rotw << ":oh=" << roth;
                    string filterArgs = argsOss.str();
                    m_rotateFg = CreateFilterGraph(filterArgs, avfrmPtr->width, avfrmPtr->height, (AVPixelFormat)avfrmPtr->format, &m_rotateInputCtx, &m_rotateOutputCtx);
                    if (!m_rotateFg)
                        return false;
                    m_needUpdateRotateParam = false;
                }
                else if (m_needUpdateRotateParam)
                {
                    double radians = m_rotateAngle*M_PI/180;
                    char cmdArgs[32] = {0}, cmdRes[128] = {0};
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%.4lf", radians);
                    fferr = avfilter_graph_send_command(m_rotateFg, "rotate", "a", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'rotate' on argument 'a' = " << radians
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    m_needUpdateRotateParam = false;
                }
                fferr = av_buffersrc_write_frame(m_rotateInputCtx, avfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersrc_write_frame()' at 'rotate' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                av_frame_unref(avfrmPtr.get());
                fferr = av_buffersink_get_frame(m_rotateOutputCtx, avfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersink_get_frame()' at 'rotate' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
            }

            // overlay
            const int ovlyX = ((int)m_outWidth-avfrmPtr->width)/2+m_posOffsetH;
            const int ovlyY = ((int)m_outHeight-avfrmPtr->height)/2+m_posOffsetV;
            if (m_ovlyX != ovlyX || m_ovlyY != ovlyY)
            {
                m_ovlyX = ovlyX;
                m_ovlyY = ovlyY;
                m_needUpdateOverlayParam = true;
            }
            if (!m_overlayFg)
            {
                m_overlayFg = CreateOverlayFilterGraph(m_outWidth, m_outHeight, (AVPixelFormat)avfrmPtr->format, m_ovlyX, m_ovlyY,
                                                        &m_ovlyInput0Ctx, &m_ovlyInput1Ctx, &m_ovlyOutputCtx);
                if (!m_overlayFg)
                    return false;
                m_needUpdateOverlayParam = false;
            }
            else if (m_needUpdateOverlayParam)
            {
                char cmdArgs[32] = {0}, cmdRes[128] = {0};
                snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", m_ovlyX);
                fferr = avfilter_graph_send_command(m_rotateFg, "overlay", "x", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'overlay' on argument 'x' = " << m_ovlyX
                        << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                    m_errMsg = oss.str();
                    return false;
                }
                snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", m_ovlyY);
                fferr = avfilter_graph_send_command(m_rotateFg, "overlay", "y", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'overlay' on argument 'y' = " << m_ovlyY
                        << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                    m_errMsg = oss.str();
                    return false;
                }
                m_needUpdateOverlayParam = false;
            }
            m_ovlyBaseImg->pts = pts;
            fferr = av_buffersrc_write_frame(m_ovlyInput0Ctx, m_ovlyBaseImg);
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'av_buffersrc_write_frame()' at 'overlay' stage for base image! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }
            fferr = av_buffersrc_write_frame(m_ovlyInput1Ctx, avfrmPtr.get());
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'av_buffersrc_write_frame()' at 'overlay' stage for overlay image! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }
            av_frame_unref(avfrmPtr.get());
            fferr = av_buffersink_get_frame(m_ovlyOutputCtx, avfrmPtr.get());
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke 'av_buffersink_get_frame()' at 'overlay' stage! fferr=" << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }

            // AVFrame => ImMat
            m_frm2matCvt.ConvertImage(avfrmPtr.get(), outMat, inMat.time_stamp);
            return true;
        }

    private:
        uint32_t m_inWidth{0}, m_inHeight{0};
        uint32_t m_outWidth{0}, m_outHeight{0};
        AVPixelFormat m_unifiedInputPixfmt{AV_PIX_FMT_RGBA};
        AVPixelFormat m_unifiedOutputPixfmt{AV_PIX_FMT_NONE};
        ScaleType m_scaleType{SCALE_TYPE__FIT};
        AVRational m_inputFrameRate{25, 1};
        int32_t m_inputCount{0};

        ImMatToAVFrameConverter m_mat2frmCvt;
        AVFrameToImMatConverter m_frm2matCvt;

        AVFilterGraph* m_cropFg{nullptr};
        AVFilterContext* m_cropInputCtx{nullptr};
        AVFilterContext* m_cropOutputCtx{nullptr};
        bool m_needUpdateCropParam{false};
        uint32_t m_cropL{0}, m_cropR{0}, m_cropT{0}, m_cropB{0};
        uint32_t m_cropRectX{0}, m_cropRectY{0}, m_cropRectW{0}, m_cropRectH{0};

        AVFilterGraph* m_scaleFg{nullptr};
        AVFilterContext* m_scaleInputCtx{nullptr};
        AVFilterContext* m_scaleOutputCtx{nullptr};
        bool m_needUpdateScaleParam{true};
        double m_scaleRatioH{1}, m_scaleRatioV{1};
        double m_realScaleRatioH{1}, m_realScaleRatioV{1};
        uint32_t m_scaledWidthWithoutCrop{0}, m_scaledHeightWithoutCrop{0};

        AVFilterGraph* m_rotateFg{nullptr};
        AVFilterContext* m_rotateInputCtx{nullptr};
        AVFilterContext* m_rotateOutputCtx{nullptr};
        bool m_needUpdateRotateParam{false};
        double m_rotateAngle{0};

        AVFilterGraph* m_overlayFg{nullptr};
        AVFilterContext* m_ovlyInput0Ctx{nullptr};
        AVFilterContext* m_ovlyInput1Ctx{nullptr};
        AVFilterContext* m_ovlyOutputCtx{nullptr};
        bool m_needUpdateOverlayParam{false};
        int32_t m_ovlyX{0}, m_ovlyY{0};
        int32_t m_posOffsetH{0}, m_posOffsetV{0};
        AVFrame* m_ovlyBaseImg{nullptr};

        mutex m_processLock;
        string m_errMsg;
    };

    FFTransformVideoFilter* NewFFTransformVideoFilter()
    {
        return new FFTransformVideoFilter_Impl();
    }
}