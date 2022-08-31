#include <sstream>
#include <mutex>
#include <algorithm>
#include <cmath>
#include "VideoTransformFilter.h"
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
    const std::string VideoTransformFilter::FILTER_NAME = "VideoTransformFilter";

    class VideoTransformFilter_FFImpl : public VideoTransformFilter
    {
    public:
        VideoTransformFilter_FFImpl() {}

        virtual ~VideoTransformFilter_FFImpl()
        {
            if (m_scaleFg)
                avfilter_graph_free(&m_scaleFg);
            if (m_rotateFg)
                avfilter_graph_free(&m_rotateFg);
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
            m_diagonalLen = (uint32_t)ceil(sqrt(outWidth*outWidth+outHeight*outHeight));
            if (m_diagonalLen%2 == 1) m_diagonalLen++;
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
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetPositionOffsetH(int32_t value) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_posOffsetH = value;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetPositionOffsetV(int32_t value) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_posOffsetV = value;
            m_needUpdateScaleParam = true;
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

        bool SetCropMarginL(uint32_t value) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_cropL = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginT(uint32_t value) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_cropT = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginR(uint32_t value) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_cropR = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginB(uint32_t value) override
        {
            lock_guard<mutex> lk(m_processLock);
            m_cropB = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetRotationAngle(double angle) override
        {
            lock_guard<mutex> lk(m_processLock);
            int32_t n = (int32_t)trunc(angle/360);
            m_rotateAngle = angle-n*360;
            m_needUpdateRotateParam = true;
            m_needUpdateScaleParam = true;
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

        bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) override
        {
            m_keyPoints = keypoint;
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
            int fferr;
            // if ((fferr = av_opt_set(avfg, "threads", "16", 0)) < 0)
            // {
            //     ostringstream oss;
            //     oss << "FAILED to invoke 'av_opt_set()' to set the 'threads' option of the filter graph! fferr=" << fferr << ".";
            //     m_errMsg = oss.str();
            //     return nullptr;
            // }

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

        bool ConvertInMatToAVFrame(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr)
        {
            if (!m_mat2frmCvt.ConvertImage(inMat, avfrmPtr.get(), avfrmPtr->pts))
            {
                ostringstream oss;
                oss << "FAILED to convert 'ImMat' to 'AVFrame'! Error message is '" << m_mat2frmCvt.GetError() << "'.";
                m_errMsg = oss.str();
                return false;
            }
            return true;
        }

        bool PerformCropStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr)
        {
            if (m_needUpdateCropParam)
            {
                uint32_t rectX = m_cropL<m_inWidth ? m_cropL : m_inWidth-1;
                uint32_t rectX1 = m_cropR<m_inWidth ? m_inWidth-m_cropR : 0;
                uint32_t rectW;
                if (rectX < rectX1)
                    rectW = rectX1-rectX;
                else
                {
                    rectW = rectX-rectX1;
                    rectX = rectX1;
                }
                uint32_t rectY = m_cropT<m_inHeight ? m_cropT : m_inHeight-1;
                uint32_t rectY1 = m_cropB<m_inHeight ? m_inHeight-m_cropB : 0;
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
                if (!avfrmPtr->data[0])
                {
                    if (!ConvertInMatToAVFrame(inMat, avfrmPtr))
                        return false;
                }

                int fferr;
                SelfFreeAVFramePtr cropfrmPtr = AllocSelfFreeAVFramePtr();
                cropfrmPtr->width = m_inWidth;
                cropfrmPtr->height = m_inHeight;
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
            }
            return true;
        }

        double CalcRadiansByXY(int32_t x, int32_t y)
        {
            double arc;
            if (x == 0 && y == 0)
            {
                return 0;
            }
            else if (y == 0)
            {
                if (x > 0)
                    arc = 0;
                else
                    arc = M_PI;
            }
            else if (x == 0)
            {
                if (y > 0)
                    arc = M_PI_2;
                else
                    arc = M_PI+M_PI_2;
            }
            else
            {
                arc = atan2(y, x);
            }
            return arc;
        }

        bool PerformScaleStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr)
        {
            if (m_needUpdateScaleParam)
            {
                uint32_t fitScaleWidth{m_inWidth}, fitScaleHeight{m_inHeight};
                switch (m_scaleType)
                {
                    case SCALE_TYPE__FIT:
                    if (m_inWidth*m_outHeight > m_inHeight*m_outWidth)
                    {
                        fitScaleWidth = m_outWidth;
                        fitScaleHeight = (uint32_t)round((double)m_inHeight*m_outWidth/m_inWidth);
                    }
                    else
                    {
                        fitScaleHeight = m_outHeight;
                        fitScaleWidth = (uint32_t)round((double)m_inWidth*m_outHeight/m_inHeight);
                    }
                    break;
                    case SCALE_TYPE__CROP:
                    fitScaleWidth = m_inWidth;
                    fitScaleHeight = m_inHeight;
                    break;
                    case SCALE_TYPE__FILL:
                    if (m_inWidth*m_outHeight > m_inHeight*m_outWidth)
                    {
                        fitScaleHeight = m_outHeight;
                        fitScaleWidth = (uint32_t)round((double)m_inWidth*m_outHeight/m_inHeight);
                    }
                    else
                    {
                        fitScaleWidth = m_outWidth;
                        fitScaleHeight = (uint32_t)round((double)m_inHeight*m_outWidth/m_inWidth);
                    }
                    break;
                    case SCALE_TYPE__STRETCH:
                    fitScaleWidth = m_outWidth;
                    fitScaleHeight = m_outHeight;
                    break;
                }
                m_scaledWidthWithoutCrop = (uint32_t)round(fitScaleWidth*m_scaleRatioH);
                m_scaledHeightWithoutCrop = (uint32_t)round(fitScaleHeight*m_scaleRatioV);
                m_realScaleRatioH = (double)fitScaleWidth/m_inWidth*m_scaleRatioH;
                m_realScaleRatioV = (double)fitScaleHeight/m_inHeight*m_scaleRatioV;

                double posOffBrH{0}, posOffBrV{0};
                int32_t scaleInputPosOffH{0}, scaleInputPosOffV{0};
                if (m_posOffsetH != 0 || m_posOffsetV != 0)
                {
                    int32_t viewOffsetH = -m_posOffsetH, viewOffsetV = -m_posOffsetV;
                    double offsetArc = CalcRadiansByXY(viewOffsetH, viewOffsetV)-m_rotateAngle*M_PI/180;
                    double r = sqrt(viewOffsetH*viewOffsetH+viewOffsetV*viewOffsetV);
                    posOffBrH = r*cos(offsetArc);
                    posOffBrV = r*sin(offsetArc);
                    scaleInputPosOffH = (int32_t)round(posOffBrH/m_realScaleRatioH);
                    scaleInputPosOffV = (int32_t)round(posOffBrV/m_realScaleRatioV);
                }
                uint32_t maxEdgeLen = m_diagonalLen+m_scaleSafePadding;
                uint32_t outW = (uint32_t)round(m_realScaleRatioH*inMat.w);
                m_scaleOutputRoiW = outW > maxEdgeLen ? maxEdgeLen : outW;
                m_scaleInputW = (uint32_t)round(m_scaleOutputRoiW/m_realScaleRatioH);
                if (m_scaleInputW > inMat.w) m_scaleInputW = inMat.w;
                uint32_t outH = (uint32_t)round(m_realScaleRatioV*inMat.h);
                m_scaleOutputRoiH = outH > maxEdgeLen ? maxEdgeLen : outH;
                m_scaleInputH = (uint32_t)round(m_scaleOutputRoiH/m_realScaleRatioV);
                if (m_scaleInputH > inMat.h) m_scaleInputH = inMat.h;

                if (m_realScaleRatioH <= 0 || m_realScaleRatioV <= 0 ||
                    (m_realScaleRatioH == 1 && m_realScaleRatioV == 1))
                {
                    m_posOffCompH = 0;
                    m_posOffCompV = 0;
                }
                else
                {
                    m_scaleInputOffX = (inMat.w-(int)m_scaleInputW)/2+scaleInputPosOffH;
                    m_scaleInputOffY = (inMat.h-(int)m_scaleInputH)/2+scaleInputPosOffV;
                    if (scaleInputPosOffH != 0 || scaleInputPosOffV != 0)
                    {
                        int32_t centerOffH = scaleInputPosOffH, centerOffV = scaleInputPosOffV;
                        if (m_scaleInputOffX < 0)
                        {
                            centerOffH -= m_scaleInputOffX;
                            m_scaleInputOffX = 0;
                        }
                        else if (m_scaleInputOffX+m_scaleInputW > inMat.w)
                        {
                            centerOffH -= m_scaleInputOffX+m_scaleInputW-inMat.w;
                            m_scaleInputOffX = inMat.w-m_scaleInputW;
                        }
                        if (m_scaleInputOffY < 0)
                        {
                            centerOffV -= m_scaleInputOffY;
                            m_scaleInputOffY = 0;
                        }
                        else if (m_scaleInputOffY+m_scaleInputH > inMat.h)
                        {
                            centerOffV -= m_scaleInputOffY+m_scaleInputH-inMat.h;
                            m_scaleInputOffY = inMat.h-m_scaleInputH;
                        }
                        double scaledCenterOffH = m_realScaleRatioH*centerOffH;
                        double scaledCenterOffV = m_realScaleRatioV*centerOffV;
                        double scaledCenterDistanceR = sqrt(scaledCenterOffH*scaledCenterOffH+scaledCenterOffV*scaledCenterOffV);
                        double arc = (double)m_rotateAngle*M_PI/180+CalcRadiansByXY(centerOffH, centerOffV);
                        int32_t scaledAndRotatedCenterOffH = (int32_t)round(scaledCenterDistanceR*cos(arc));
                        int32_t scaledAndRotatedCenterOffV = (int32_t)round(scaledCenterDistanceR*sin(arc));
                        m_posOffCompH = scaledAndRotatedCenterOffH;
                        m_posOffCompV = scaledAndRotatedCenterOffV;
                    }
                    else
                    {
                        m_posOffCompH = m_posOffCompV = 0;
                    }
                    if (m_scaleInputOffX%2 == 1)
                        m_scaleInputOffX--;
                }
                // Log(VERBOSE) << "Scale stage params: InputOffset=" << m_scaleInputOffX << "," << m_scaleInputOffY << "; InputSize=" << m_scaleInputW << "x" << m_scaleInputH
                //         << "; OutputRoiSize=" << m_scaleOutputRoiW << "x" << m_scaleOutputRoiH << "; CompensateOff=" << m_posOffCompH << "," << m_posOffCompV << endl;
            }
            int fferr;
            if (m_realScaleRatioH <= 0 || m_realScaleRatioV <= 0)
            {
                int64_t pts = avfrmPtr->pts;
                av_frame_unref(avfrmPtr.get());
                avfrmPtr->width = 2;
                avfrmPtr->height = 2;
                avfrmPtr->format = (int)m_unifiedInputPixfmt;
                fferr = av_frame_get_buffer(avfrmPtr.get(), 0);
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_frame_get_buffer()'! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                avfrmPtr->pts = pts;
                memset(avfrmPtr->buf[0]->data, 0, avfrmPtr->buf[0]->size);
            }
            else if (m_realScaleRatioH != 1 || m_realScaleRatioV != 1)
            {
                if (!avfrmPtr->data[0])
                {
                    if (!ConvertInMatToAVFrame(inMat, avfrmPtr))
                        return false;
                }
                if (!m_scaleFg)
                {
                    ostringstream argsOss;
                    argsOss << "scale=w=" << m_scaleOutputRoiW << ":h=" << m_scaleOutputRoiH << ":eval=frame:flags=bicubic";
                    string filterArgs = argsOss.str();
                    m_scaleFg = CreateFilterGraph(filterArgs, avfrmPtr->width, avfrmPtr->height, (AVPixelFormat)avfrmPtr->format, &m_scaleInputCtx, &m_scaleOutputCtx);
                    if (!m_scaleFg)
                        return false;
                    m_needUpdateScaleParam = false;
                }
                else if (m_needUpdateScaleParam)
                {
                    char cmdArgs[32] = {0}, cmdRes[128] = {0};
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", m_scaleOutputRoiW);
                    fferr = avfilter_graph_send_command(m_scaleFg, "scale", "w", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'scale' on argument 'w' = " << m_scaleOutputRoiW
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", m_scaleOutputRoiH);
                    fferr = avfilter_graph_send_command(m_scaleFg, "scale", "h", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command()' to 'scale' on argument 'h' = " << m_scaleOutputRoiH
                            << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
                        m_errMsg = oss.str();
                        return false;
                    }
                    m_needUpdateScaleParam = false;
                }
                SelfFreeAVFramePtr inputfrmPtr = avfrmPtr;
                if (m_scaleInputW != avfrmPtr->width || m_scaleInputH != avfrmPtr->height)
                {
                    inputfrmPtr = AllocSelfFreeAVFramePtr();
                    inputfrmPtr->width = m_scaleInputW;
                    inputfrmPtr->height = m_scaleInputH;
                    inputfrmPtr->format = avfrmPtr->format;
                    memset(inputfrmPtr->data, 0, sizeof(inputfrmPtr->data));
                    memset(inputfrmPtr->linesize, 0, sizeof(inputfrmPtr->linesize));
                    memset(inputfrmPtr->buf, 0, sizeof(inputfrmPtr->buf));
                    inputfrmPtr->data[0] = avfrmPtr->data[0]+m_scaleInputOffY*avfrmPtr->linesize[0]+m_scaleInputOffX*4;
                    AVBufferRef* extBufRef = av_buffer_create(
                        inputfrmPtr->data[0], m_scaleInputH*avfrmPtr->linesize[0]-m_scaleInputOffX*4,
                        [](void*, uint8_t*){}, nullptr, 0);
                    inputfrmPtr->linesize[0] = avfrmPtr->linesize[0];
                    inputfrmPtr->buf[0] = extBufRef;
                    av_frame_copy_props(inputfrmPtr.get(), avfrmPtr.get());
                }
                else
                {
                    m_posOffCompH = m_posOffCompV = 0;
                }
                fferr = av_buffersrc_add_frame_flags(m_scaleInputCtx, inputfrmPtr.get(), AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT);
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersrc_add_frame_flags()' at 'scale' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                SelfFreeAVFramePtr outfrmPtr = AllocSelfFreeAVFramePtr();
                fferr = av_buffersink_get_frame(m_scaleOutputCtx, outfrmPtr.get());
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_buffersink_get_frame()' at 'scale' stage! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                avfrmPtr = outfrmPtr;
            }
            return true;
        }

        bool PerformRotateStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr)
        {
            if (m_rotateAngle != 0)
            {
                if (!avfrmPtr->data[0])
                {
                    if (!ConvertInMatToAVFrame(inMat, avfrmPtr))
                        return false;
                }
                if (m_rotateFg && (avfrmPtr->width != m_rotInW || avfrmPtr->height != m_rotInH))
                    avfilter_graph_free(&m_rotateFg);

                int fferr;
                if (!m_rotateFg)
                {
                    uint32_t rotw = (uint32_t)ceil(sqrt(avfrmPtr->width*avfrmPtr->width+avfrmPtr->height*avfrmPtr->height));
                    if (rotw%2 == 1) rotw++;
                    uint32_t roth = rotw;
                    ostringstream argsOss;
                    argsOss << "rotate=a=" << m_rotateAngle*M_PI/180 << ":ow=" << rotw << ":oh=" << roth << ":c=0x00000000";
                    string filterArgs = argsOss.str();
                    m_rotateFg = CreateFilterGraph(filterArgs, avfrmPtr->width, avfrmPtr->height, (AVPixelFormat)avfrmPtr->format, &m_rotateInputCtx, &m_rotateOutputCtx);
                    if (!m_rotateFg)
                        return false;
                    m_rotInW = avfrmPtr->width;
                    m_rotInH = avfrmPtr->height;
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
            return true;
        }

        bool PerformOverlayStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr)
        {
            const int32_t posOffH = m_posOffsetH+m_posOffCompH;
            const int32_t posOffV = m_posOffsetV+m_posOffCompV;
            if (!avfrmPtr->data[0] && (inMat.w != m_outWidth || inMat.h != m_outHeight || posOffH != 0 || posOffV != 0))
            {
                if (!ConvertInMatToAVFrame(inMat, avfrmPtr))
                    return false;
            }
            if (avfrmPtr->data[0] && (avfrmPtr->width != m_outWidth || avfrmPtr->height != m_outHeight || posOffH != 0 || posOffV != 0))
            {
                int fferr;
                const int32_t ovlyX = ((int32_t)m_outWidth-avfrmPtr->width)/2+posOffH;
                const int32_t ovlyY = ((int32_t)m_outHeight-avfrmPtr->height)/2+posOffV;
                SelfFreeAVFramePtr ovlyBaseImg = AllocSelfFreeAVFramePtr();
                ovlyBaseImg->width = m_outWidth;
                ovlyBaseImg->height = m_outHeight;
                ovlyBaseImg->format = (int)m_unifiedOutputPixfmt;
                fferr = av_frame_get_buffer(ovlyBaseImg.get(), 0);
                if (fferr < 0)
                {
                    ostringstream oss;
                    oss << "FAILED to invoke 'av_frame_get_buffer()' for overlay base image! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    return false;
                }
                int maxBufCnt = sizeof(ovlyBaseImg->buf)/sizeof(ovlyBaseImg->buf[0]);
                for (int i = 0; i < maxBufCnt; i++)
                {
                    AVBufferRef* bufref = ovlyBaseImg->buf[i];
                    if (!bufref || bufref->size <= 0)
                        continue;
                    memset(bufref->data, 0, bufref->size);
                }
                av_frame_copy_props(ovlyBaseImg.get(), avfrmPtr.get());

                const int32_t srcX = ovlyX >= 0 ? 0 : -ovlyX;
                const int32_t srcY = ovlyY >= 0 ? 0 : -ovlyY;
                const int32_t dstX = ovlyX <= 0 ? 0 : ovlyX;
                const int32_t dstY = ovlyY <= 0 ? 0 : ovlyY;
                int32_t copyW = srcX > 0 ? avfrmPtr->width-srcX : avfrmPtr->width;
                if (dstX+copyW > ovlyBaseImg->width)
                    copyW = ovlyBaseImg->width-dstX;
                int32_t copyH = srcY > 0 ? avfrmPtr->height-srcY : avfrmPtr->height;
                if (dstY+copyH > ovlyBaseImg->height)
                    copyH = ovlyBaseImg->height-dstY;
                if (copyW > 0 && copyH > 0)
                {
                    const uint8_t* srcptr = avfrmPtr->data[0]+srcY*avfrmPtr->linesize[0]+srcX*4;
                    uint8_t* dstptr = ovlyBaseImg->data[0]+dstY*ovlyBaseImg->linesize[0]+dstX*4;
                    for (int32_t i = 0; i < copyH; i++)
                    {
                        memcpy(dstptr, srcptr, copyW*4);
                        srcptr += avfrmPtr->linesize[0];
                        dstptr += ovlyBaseImg->linesize[0];
                    }
                }

                avfrmPtr = ovlyBaseImg;
            }
            return true;
        }

        bool FilterImage_Internal(const ImGui::ImMat& inMat, ImGui::ImMat& outMat, int64_t pos)
        {
            lock_guard<mutex> lk(m_processLock);
            m_inWidth = inMat.w;
            m_inHeight = inMat.h;

            // check and update key points values
            for (int i = 0; i < m_keyPoints.GetCurveCount(); i++)
            {
                auto name = m_keyPoints.GetCurveName(i);
                auto value = m_keyPoints.GetValue(i, pos);
            }


            // allocate intermediate AVFrame
            SelfFreeAVFramePtr avfrmPtr = AllocSelfFreeAVFramePtr();
            avfrmPtr->pts = (int64_t)(m_inputCount++)*AV_TIME_BASE*m_inputFrameRate.den/m_inputFrameRate.num;

            if (!PerformCropStage(inMat, avfrmPtr))
                return false;
            if (!PerformScaleStage(inMat, avfrmPtr))
                return false;
            if (!PerformRotateStage(inMat, avfrmPtr))
                return false;
            if (!PerformOverlayStage(inMat, avfrmPtr))
                return false;

            if (avfrmPtr->data[0])
            {
                // AVFrame => ImMat
                if (!m_frm2matCvt.ConvertImage(avfrmPtr.get(), outMat, inMat.time_stamp))
                {
                    ostringstream oss;
                    oss << "FAILED to convert 'AVFrame' to 'ImMat'! Error message is '" << m_frm2matCvt.GetError() << "'.";
                    m_errMsg = oss.str();
                    return false;
                }
            }
            else
            {
                outMat = inMat;
            }
            return true;
        }

    private:
        uint32_t m_inWidth{0}, m_inHeight{0};
        uint32_t m_outWidth{0}, m_outHeight{0};
        uint32_t m_diagonalLen{0}, m_scaleSafePadding{2};
        AVPixelFormat m_unifiedInputPixfmt{AV_PIX_FMT_RGBA};
        AVPixelFormat m_unifiedOutputPixfmt{AV_PIX_FMT_NONE};
        ScaleType m_scaleType{SCALE_TYPE__FIT};
        AVRational m_inputFrameRate{25, 1};
        int32_t m_inputCount{0};

        ImMatToAVFrameConverter m_mat2frmCvt;
        AVFrameToImMatConverter m_frm2matCvt;

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
        uint32_t m_scaleOutputRoiW{0}, m_scaleOutputRoiH{0};
        uint32_t m_scaleInputW{0}, m_scaleInputH{0};
        int32_t m_scaleInputOffX{0}, m_scaleInputOffY{0};
        int32_t m_posOffCompH{0}, m_posOffCompV{0};

        AVFilterGraph* m_rotateFg{nullptr};
        AVFilterContext* m_rotateInputCtx{nullptr};
        AVFilterContext* m_rotateOutputCtx{nullptr};
        bool m_needUpdateRotateParam{false};
        uint32_t m_rotInW{0}, m_rotInH{0};
        double m_rotateAngle{0};

        int32_t m_posOffsetH{0}, m_posOffsetV{0};

        ImGui::KeyPointEditor m_keyPoints;

        mutex m_processLock;
        string m_errMsg;
    };

    VideoTransformFilter* NewVideoTransformFilter()
    {
        return new VideoTransformFilter_FFImpl();
    }
}