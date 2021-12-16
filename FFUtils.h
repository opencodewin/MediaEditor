#pragma once
#include <cstdint>
#include <string>
#include "immat.h"
extern "C"
{
    #include "libavutil/frame.h"
}

std::string MillisecToString(int64_t millisec);
std::string TimestampToString(double timestamp);
bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp);