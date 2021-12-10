#pragma once
#include "immat.h"
extern "C"
{
    #include "libavutil/frame.h"
}

bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp);