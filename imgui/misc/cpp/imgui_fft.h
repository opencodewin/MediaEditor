#pragma once
#include <imgui.h>

namespace ImGui
{
// FFT 1D
IMGUI_API void ImFFT (float* data, int N, bool forward);
IMGUI_API void ImRFFT (float* data, int N, bool forward);
IMGUI_API void ImRFFT (float* in, float* out, int N, bool forward);
IMGUI_API int ImReComposeDB(float * in, float * out, int samples, bool inverse = true);
IMGUI_API int ImReComposeAmplitude(float * in, float * out, int samples);
IMGUI_API int ImReComposePhase(float * in, float * out, int samples);
IMGUI_API int ImReComposeDBShort(float * in, float * out, int samples, bool inverse = true);
IMGUI_API int ImReComposeDBLong(float * in, float * out, int samples, bool inverse = true);
IMGUI_API float ImDoDecibel(float * in, int samples, bool inverse = true);

// STFT 1D
struct IMGUI_API ImSTFT
{
    ImSTFT(int _window, int _hope);
    ~ImSTFT();
    void stft(float* in, float* out);
    void istft(float* in, float* out);

private:
    void *hannwin {nullptr};
    void *overlap {nullptr};

    int frame_size;
    int shift_size;
    int overlap_size;
    float* buf  {nullptr};
};
} // namespace ImGui
